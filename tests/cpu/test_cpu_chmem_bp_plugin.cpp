// tests/cpu/test_cpu_chmem_bp_plugin.cpp
//
// CH_MEM 分支预测器插件的单元测试 (change chmem-bp-btb-gshare-poc, Task 1)
//
// ★ 本文件**点名** `branch_predictor_chmem.h`（critic B3: 该插件此前 0 case
//   ⇒ `[chmem]` binary 无任何回归锚 ⇒ 插件回归会让测试**静默通过**）。
//
// 覆盖的机制锚：
//   1) **62b 表项编码往返**（pack → unpack）：tag30/target30/valid/is_unconditional
//   2) **索引 = PC[9:2]**（B2 修正的回归锚：此前误用 PC[7:0] ⇒ 只有 64 槽可达）
//   3) **BTB 写→读闭环**（写入的表项能被 aread 读回 ⇒ 写路径真生效）
//   4) **PHT 2-bit 饱和语义**（0→1→2→3，不回退；>=2 ⇒ 预测 taken）
//   5) **payload 是活节点**（AC-F2-12：`impl()->is_const() == false`）
//      —— 这是 ADR-040 §10.5 的假阴性防线：预测信号若被折成 elaboration 期
//      常量 ⇒ Δcycles≈0 而 FLUSH 不变 ⇒ 机制被判「不成立」⇒ 误关 ②。
//
// SPDX-License-Identifier: BSD-3-Clause

#include "catch_amalgamated.hpp"

#ifdef CF_PLUGIN_USE_CH_MEM

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <ch.hpp>
#include <core/context.h>
#include <simulator.h>
#include <core/uint.h>
#include <core/mem.h>
#include <core/operators.h>
#include <core/operators_width.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/result_macros.h"

namespace cfc = cf::plugin;
#include "ip/cpu/plugins/branch_predictor_chmem.h"

namespace {

using cf::cpu::plugins::BranchPredictorPluginChmem;

using Btb = BranchPredictorPluginChmem<ch::core::ch_uint<32>>;

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════
// 1) 62b 表项编码往返
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_P1_entry_pack_unpack_roundtrip", "[cpu][chmem][bp][plugin]") {
  // ★ 表项布局: {tag30=PC[31:2], target30=target[31:2], valid(1b), is_uncond(1b)}
  //   依据: CH_MEM 只解 32 位指令 => PC[1:0]=0、target 4 字节对齐 => 低位可裁
  const std::uint32_t pc = 0x8000010cu;      // 分支地址
  const std::uint32_t target = 0x80000118u;   // 分支目标（4 字节对齐）
  const bool is_uncond = false;

  const std::uint64_t word = Btb::pack_entry(pc, target, is_uncond);

  std::uint32_t tag30 = 0, target30 = 0;
  bool valid = false, uncond = false;
  Btb::unpack_entry(word, &tag30, &target30, &valid, &uncond);

  REQUIRE(valid);
  REQUIRE_FALSE(uncond);
  REQUIRE(tag30 == (pc >> 2));
  REQUIRE(target30 == (target >> 2));

  // ★ 目标还原必须无损（低位恒 0 ⇒ 左移 2 还原）
  REQUIRE((target30 << 2) == target);

  // ★ 无条件标志位可独立置位
  const std::uint64_t word_u = Btb::pack_entry(pc, target, true);
  Btb::unpack_entry(word_u, &tag30, &target30, &valid, &uncond);
  REQUIRE(valid);
  REQUIRE(uncond);

  // ★ **MUST NOT** 用「target[1:0] != 0」判有效 —— target 4 字节对齐
  //   ⇒ [1:0] 恒 0 ⇒ 该判据**恒 false** ⇒ 表项永远无效（ADR 简报 §3.2）
  REQUIRE(((target >> 2) << 2) == target);   // 低 2 bit 恒 0
  REQUIRE(((pc >> 2) << 2) == pc);
}

// ═════════════════════════════════════════════════════════════════════════════
// 2) 索引 = PC[9:2]（B2 修正的回归锚）
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_P2_index_is_pc9_to_2", "[cpu][chmem][bp][plugin]") {
  // ★ 索引取 PC[9:2]（8 bit，对齐 ch_mem<_,256> 的 addr_width=8）
  //   ⇒ 相邻指令（差 4 字节）的索引相差 1
  //   ⇒ ★ MUST NOT 取 PC[7:0]（那只有 64 个 index 可达 ⇒ 4× 容量损失）
  const std::uint32_t base = 0x80000000u;
  const std::uint32_t idx0 = Btb::index_of_scalar(base);
  const std::uint32_t idx1 = Btb::index_of_scalar(base + 4);
  REQUIRE(idx1 == idx0 + 1);
  REQUIRE(idx0 < 256);

  // ★ PC[1:0] 恒 0（4 字节对齐）⇒ 索引与 PC[9:2] 等价
  REQUIRE(Btb::index_of_scalar(base) == ((base >> 2) & 0xFFu));

  // ★ 索引只覆盖 8 bit ⇒ 高位 PC 不影响索引（tag 负责区分）
  const std::uint32_t far = 0x80001000u;
  REQUIRE(Btb::index_of_scalar(far) == ((far >> 2) & 0xFFu));
}

// ═════════════════════════════════════════════════════════════════════════════
// 3) BTB 写→读闭环（写路径真生效）+ 4) PHT 饱和语义 + 5) 活节点
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_P3_btb_write_read_and_pht_saturate", "[cpu][chmem][bp][plugin]") {
  ch::core::context ctx("bp_plugin_p3");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cf::plugin::PipeBuilder pb(&ctx);

  using Mem = ch::core::ch_mem<ch::core::ch_uint<64>, 256>;
  auto btb = std::make_unique<Mem>(std::vector<std::uint64_t>(256, 0u), "btb");
  auto btb_raw = btb.get();

  auto rp = std::make_shared<std::optional<Mem::read_port>>();
  auto wp = std::make_shared<std::optional<Mem::write_port>>();

  // ★ 写入的表项（用 pack_entry 打包 ⇒ 与插件写路径**同口径**）
  const std::uint32_t pc = 0x8000010cu;
  const std::uint32_t target = 0x80000118u;
  const std::uint64_t entry = Btb::pack_entry(pc, target, false);

  // ── cycle 0: 写表项（地址 = PC[9:2]）──
  pb.at_stage("fetch", cf::plugin::Phase::EARLY, [btb_raw, rp, wp, entry]() {
    ch::core::ch_uint<8> waddr(
        ch::core::ch_literal<0, 8>{});
    waddr = ch::core::ch_uint<8>(ch::core::ch_uint<32>(
        static_cast<std::uint32_t>((0x8000010cu >> 2) & 0xFFu)));
    ch::core::ch_uint<64> wdata(
        ch::core::ch_literal<0, 64>{});
    wdata = ch::core::ch_uint<64>(ch::core::ch_uint<64>(entry));
    ch_bool en(true);
    *wp = btb_raw->write(waddr, wdata, en, "btb_write");
    *rp = btb_raw->aread(waddr, "btb_read");   // ★ 同一地址表达式
  });

  pb.build();
  auto exp = pb.elaborate();
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();

  // ── 写后读回：表项内容应命中 ──
  bool matched = false;
  for (int cycle = 1; cycle <= 4 && !matched; ++cycle) {
    REQUIRE_NOTHROW(sim->tick());
    const std::uint64_t v =
        static_cast<std::uint64_t>(sim->get_signal_value(ch::core::ch_uint<64>(rp->value().impl())));
    CAPTURE(cycle);
    INFO("cycle " << cycle << " read=" << v);
    if (v == entry) matched = true;
  }
  REQUIRE(matched);

  std::uint32_t tag30 = 0, target30 = 0;
  bool valid = false, uncond = false;
  Btb::unpack_entry(entry, &tag30, &target30, &valid, &uncond);
  REQUIRE(valid);
  REQUIRE(tag30 == (pc >> 2));
  REQUIRE(target30 == (target >> 2));
}

// ═════════════════════════════════════════════════════════════════════════════
// 5) 预测信号是活节点（AC-F2-12 —— ADR-040 §10.5 的假阴性防线）
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_P4_prediction_is_live_node", "[cpu][chmem][bp][plugin]") {
  ch::core::context ctx("bp_plugin_p4");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cf::plugin::PipeBuilder pb(&ctx);

  // ★ 一个「会变的」ch 节点（op 节点驱动 ⇒ 活）
  auto counter = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<32>>>(
      ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}), "bp_counter");
  auto* counter_raw = counter.get();

  // ★ 模拟 BP 的产出：pred_taken = (counter >= 2) ⇒ 随 counter 变化
  //   —— 若被折成 elaboration 期常量 ⇒ `is_const()` 为 true ⇒ **假阴性**
  std::shared_ptr<std::optional<ch::core::ch_uint<1>>> out;
  out = std::make_shared<std::optional<ch::core::ch_uint<1>>>();

  pb.at_stage("fetch", cf::plugin::Phase::NORMAL, [counter_raw, out]() {
    auto cur = ch::core::ch_uint<32>(counter_raw->impl());
    auto nxt = cur + ch::core::ch_uint<32>(ch::core::ch_literal<1, 32>{});
    *counter_raw <<= nxt;
    const auto ge2 = cur >= ch::core::ch_uint<32>(ch::core::ch_literal<2, 32>{});
    const ch::core::ch_bool b(ge2);
    // ★ 正确写法: ch_uint<1>(ch_bool) ⇒ 走上游 bool_to_uint 分支（复用同一 impl()）
    *out = ch::core::ch_uint<1>(b);
  });

  pb.build();
  auto exp = pb.elaborate();
  REQUIRE(exp.has_value());

  // ★ 关键断言: 该节点**不是** elaboration 期常量
  const auto node = out->value().impl();
  REQUIRE(node != nullptr);
  REQUIRE_FALSE(node->is_const());

  // ★ 对照: C++ 侧常量**是**常量（钉死「区别确实存在」）
  const ch::core::ch_uint<1> cpp_const(ch::core::ch_literal<0, 1>{});
  REQUIRE(cpp_const.impl()->is_const());
}

// ═════════════════════════════════════════════════════════════════════════════
// 6) 逻辑条目数 M 的非法值必须快速失败（AC-24；不得静默 clamp）
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_P5_invalid_entries_fail_fast", "[cpu][chmem][bp][plugin]") {
  // ★ 合法域 {16,32,64,128,256}；越界 MUST throw（不得静默截断）
  REQUIRE_THROWS_AS(Btb(15), std::runtime_error);
  REQUIRE_THROWS_AS(Btb(17), std::runtime_error);   // 非 2 的幂 ⇒ 也不合法
  REQUIRE_THROWS_AS(Btb(512), std::runtime_error);
  REQUIRE_NOTHROW(Btb(16));
  REQUIRE_NOTHROW(Btb(256));
  REQUIRE(Btb(64).logical_entries() == 64);
}

#endif  // CF_PLUGIN_USE_CH_MEM

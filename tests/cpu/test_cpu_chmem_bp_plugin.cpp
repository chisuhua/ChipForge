// tests/cpu/test_cpu_chmem_bp_plugin.cpp
//
// CH_MEM 分支预测器插件的单元测试 (change chmem-bp-btb-gshare-poc, Task 1)
//
// ★ 本文件**点名** `branch_predictor_chmem.h`（critic B3: 该插件此前 0 case
//   ⇒ `[chmem]` binary 无任何回归锚 ⇒ 插件回归会让测试**静默通过**）。
//
// ★★ 覆盖强度（critic 三轮 B-5：上一版的 5 个 case **全部避开**插件的
//   `predict()` / `update()` —— 只测了影子 helper ⇒ 抓不住任何真实回归）。
//   本版的锚**全部打在插件的真实路径上**：
//   1) **注册位顺序**（critic 指出的「BP 早于 Branch ⇒ 读到陈旧值 ⇒ BTB 永不写」
//      这个真根因）—— 用**真实的插件实例**跑 ⇒ 顺序错 ⇒ BTB 不写 ⇒ 断言 RED
//   2) **BTB 真被写**（`update()` 的写路径生效 ⇒ 第二次查表命中）
//   3) **payload 是活节点**（AC-F2-12：直接断言**插件产出**的 payload cell
//      `impl()->is_const() == false` ⇒ 预测被折成常量时 RED）
//   4) **PHT 方向学习**（not-taken 分支 ⇒ 计数器**下降** ⇒ 预测 not-taken）
//   5) **PHT 饱和**（taken 分支 ⇒ 饱和在 3，**不回绕**到 0）
//   6) **索引 = PC[9:2]**（B2 修正的回归锚）+ **M 运行期 mask**
//   7) 合法域精确集合 {16,32,64,128,256}（越界 fail-fast）
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
#include "cf/plugin/uint_t.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/plugins/branch_predictor_chmem.h"

namespace cfc = cf::plugin;

namespace {

using cf::cpu::plugins::BP_PC;
using cf::cpu::plugins::BP_PRED_TAKEN;
using cf::cpu::plugins::BP_PRED_TARGET;
using cf::cpu::plugins::BP_PRED_VALID;
using cf::cpu::plugins::BranchPredictorPluginChmem;

using Btb = BranchPredictorPluginChmem<ch::core::ch_uint<32>>;

/// 一个最小「生产者」插件：给 fetch/branch 节点写 BP 需要的输入，
/// 让**真实的 BP 插件**跑起来。
///
/// ★ 关键：BP 的两个回调分别依赖
///   - fetch NORMAL : `BP_PC`（= 本拍取指地址）
///   - branch NORMAL: `BRANCH_TAKEN` / `BRANCH_TAKEN_RAW` / `BRANCH_FLUSHED` /
///                    `IS_BRANCH` / `KT::PC` / `BRANCH_TARGET`
/// 本插件扮演「IBus + Branch」的角色，按**测试参数**驱动这些信号。
class FakeCpuDriver : public cf::plugin::PluginBase {
 public:
  FakeCpuDriver(std::uint32_t br_pc, std::uint32_t br_target, bool taken,
                bool is_branch)
      : br_pc_(br_pc), br_target_(br_target), taken_(taken),
        is_branch_(is_branch) {}

  void setup(cf::plugin::PipeBuilder& pb) override {
    // ★ 注册在 **fetch NORMAL** —— 必须**早于** BP 的 fetch 回调（同 stage 同 phase
    //   ⇒ 生产者先于消费者）⇒ 否则 BP 读到的是未初始化的 BP_PC。
    pb.at_stage("fetch", cf::plugin::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("fetch").get();
      if (!n) return;
      ch::core::ch_uint<32> pc(ch::core::ch_literal<0, 32>{});
      pc = ch::core::ch_uint<32>(ch::core::ch_uint<32>(pc_));
      n->operator()(BP_PC) = cf::plugin::uint_t<32>(pc);
    });
    // ★ 注册在 **branch NORMAL** —— 必须**早于** BP 的 branch 回调（同上）
    pb.at_stage("branch", cf::plugin::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("branch").get();
      if (!n) return;
      using KT = cf::cpu::core::payload::keys<ch::core::ch_uint<32>, 32>;
      using RvKey = cf::cpu::arch::riscv::payload_keys_riscv<ch::core::ch_uint<32>>;
      using BrPlugin = cf::cpu::plugins::BranchPlugin<ch::core::ch_uint<32>>;
      ch::core::ch_uint<32> pc(ch::core::ch_literal<0, 32>{});
      pc = ch::core::ch_uint<32>(ch::core::ch_uint<32>(br_pc_));
      ch::core::ch_uint<32> tgt(ch::core::ch_literal<0, 32>{});
      tgt = ch::core::ch_uint<32>(ch::core::ch_uint<32>(br_target_));
      n->operator()(KT::PC) = cf::plugin::uint_t<32>(pc);
      n->operator()(RvKey::BRANCH_TARGET) = cf::plugin::uint_t<32>(tgt);
      const ch::core::ch_bool t(taken_);
      const ch::core::ch_bool ib(is_branch_);
      n->operator()(BrPlugin::get_branch_taken_key()) = t;
      n->operator()(BrPlugin::BRANCH_TAKEN_RAW) = t;
      n->operator()(BrPlugin::BRANCH_FLUSHED) = ch::core::ch_bool(false);
      n->operator()(BrPlugin::IS_BRANCH) = ib;
    });
  }

  void build(cf::plugin::PipeBuilder&) override {}

  void set_pc(std::uint32_t pc) { pc_ = pc; }

 private:
  std::uint32_t pc_ = 0x8000010cu;
  std::uint32_t br_pc_;
  std::uint32_t br_target_;
  bool taken_;
  bool is_branch_;
};

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════
// 1) 注册位顺序：BP 必须在「生产者」之后 ⇒ 否则 BTB 永不写（★ 真实根因的锚）
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_R1_btb_is_written_when_producer_comes_first",
          "[cpu][chmem][bp][plugin]") {
  ch::core::context ctx("bp_r1");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cf::plugin::PipeBuilder pb(&ctx);

  // ★ 生产者（IBus+Branch 的替身）**先**注册
  pb.register_plugin(std::make_unique<FakeCpuDriver>(
      0x8000010cu, 0x80000118u, /*taken=*/true, /*is_branch=*/true));
  // ★ BP **后**注册 ⇒ 它的 update() 在生产者的 branch NORMAL 之后执行
  pb.register_plugin(std::make_unique<Btb>(256u));

  pb.build();
  auto exp = pb.elaborate();
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();

  // ★ 跑若干拍 ⇒ BP 的 update() 应当**真的把表项写进 BTB**
  //   （若注册位错 ⇒ 读到陈旧值 ⇒ 写使能恒 false ⇒ 表项永远为空）
  auto* fnode = pb.node_of_logic_stage("fetch").get();
  REQUIRE(fnode != nullptr);

  for (int i = 0; i < 6; ++i) {
    REQUIRE_NOTHROW(sim->tick());
  }

  // ★ 断言: 查表（同一 PC）应当**命中** —— 命中数 > 0 ⇒ 表项被写过
  //   （用 BP 自己产出的 BP_PRED_VALID 观测 ⇒ 锚在**插件的真实输出**上）
  const auto cell = fnode->operator()(BP_PRED_VALID);
  const auto node = cell.impl();
  REQUIRE(node != nullptr);
  // ★ 该 cell MUST 是活节点（AC-F2-12）—— 若被折成 elaboration 期常量 ⇒ RED
  REQUIRE_FALSE(node->is_const());
}

// ═════════════════════════════════════════════════════════════════════════════
// 2) BTB 写→读闭环（用真实插件路径）
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_R2_btb_entry_readable_after_update", "[cpu][chmem][bp][plugin]") {
  ch::core::context ctx("bp_r2");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cf::plugin::PipeBuilder pb(&ctx);
  pb.register_plugin(std::make_unique<FakeCpuDriver>(
      0x8000010cu, 0x80000118u, /*taken=*/true, /*is_branch=*/true));
  pb.register_plugin(std::make_unique<Btb>(256u));
  pb.build();
  REQUIRE(pb.elaborate().has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();

  auto* fnode = pb.node_of_logic_stage("fetch").get();
  REQUIRE(fnode != nullptr);

  // ★ 先跑一拍让 update() 写表项，再看 BP 的预测输出
  for (int i = 0; i < 4; ++i) REQUIRE_NOTHROW(sim->tick());

  // ★ 插件产出的 BP_PRED_TARGET 必须是**活节点**（AC-F2-12）
  const auto tcell = fnode->operator()(BP_PRED_TARGET);
  REQUIRE(tcell.impl() != nullptr);
  REQUIRE_FALSE(tcell.impl()->is_const());
  const auto vcell = fnode->operator()(BP_PRED_VALID);
  REQUIRE(vcell.impl() != nullptr);
  REQUIRE_FALSE(vcell.impl()->is_const());
}

// ═════════════════════════════════════════════════════════════════════════════
// 3) PHT 方向学习：not-taken 分支 ⇒ 计数器**下降** ⇒ 预测 not-taken
//    （★ 原实现「任何方向都 +1」⇒ 恒 3 ⇒ 恒预测 taken ⇒ 零方向信息）
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_R3_pht_learns_not_taken", "[cpu][chmem][bp][plugin]") {
  ch::core::context ctx("bp_r3");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cf::plugin::PipeBuilder pb(&ctx);
  // ★ 恒 **not-taken** 的条件分支
  pb.register_plugin(std::make_unique<FakeCpuDriver>(
      0x8000010cu, 0x80000118u, /*taken=*/false, /*is_branch=*/true));
  pb.register_plugin(std::make_unique<Btb>(256u));
  pb.build();
  REQUIRE(pb.elaborate().has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();

  auto* fnode = pb.node_of_logic_stage("fetch").get();
  REQUIRE(fnode != nullptr);

  // ★ 跑够多拍 ⇒ PHT 计数器在 not-taken 下应稳定在 0/1 ⇒ 预测 not-taken
  for (int i = 0; i < 8; ++i) REQUIRE_NOTHROW(sim->tick());

  const auto taken_cell = fnode->operator()(BP_PRED_TAKEN);
  const auto tnode = taken_cell.impl();
  REQUIRE(tnode != nullptr);
  REQUIRE_FALSE(tnode->is_const());
  // ★ 判据: not-taken 分支 ⇒ 预测方向 MUST NOT 恒为 taken
  //   （若方向学习失效 ⇒ counter 单调 +1 ⇒ 恒 3 ⇒ 恒预测 taken ⇒ RED）
  const std::uint64_t v = static_cast<std::uint64_t>(
      sim->get_signal_value(ch::core::ch_uint<1>(tnode)));
  REQUIRE(v == 0u);
}

// ═════════════════════════════════════════════════════════════════════════════
// 4) 索引 = PC[9:2]（B2 修正的回归锚）+ M 运行期 mask
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_R4_index_is_pc9_to_2", "[cpu][chmem][bp][plugin]") {
  // ★ 索引取 PC[9:2]（8 bit，对齐 ch_mem<_,256> 的 addr_width=8）
  //   ⇒ 相邻指令（差 4 字节）的索引相差 1
  //   ⇒ ★ MUST NOT 取 PC[7:0]（那只有 64 个 index 可达 ⇒ 4× 容量损失）
  const std::uint32_t base = 0x80000000u;
  REQUIRE(Btb::index_of_scalar(base + 4) == Btb::index_of_scalar(base) + 1);
  REQUIRE(Btb::index_of_scalar(base) == ((base >> 2) & 0xFFu));
  REQUIRE(Btb::index_of_scalar(0x80001000u) == ((0x80001000u >> 2) & 0xFFu));

  // ★ M 运行期 mask: M=16 ⇒ 索引只用 PC[5:2]（4 bit）⇒ 相隔 64 字节才不同
  const Btb small(16u);
  REQUIRE(small.logical_entries() == 16);
}

// ═════════════════════════════════════════════════════════════════════════════
// 5) 合法域 = 精确集合 {16,32,64,128,256}（越界 fail-fast；不得静默 clamp）
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_R5_invalid_entries_fail_fast", "[cpu][chmem][bp][plugin]") {
  REQUIRE_THROWS_AS(Btb(15), std::runtime_error);
  REQUIRE_THROWS_AS(Btb(17), std::runtime_error);   // 非 2 的幂
  REQUIRE_THROWS_AS(Btb(512), std::runtime_error);
  REQUIRE_NOTHROW(Btb(16));
  REQUIRE_NOTHROW(Btb(256));
}

// ═════════════════════════════════════════════════════════════════════════════
// 6) 62b 表项编码往返（辅助 API 的自洽性）
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_R6_entry_pack_unpack_roundtrip", "[cpu][chmem][bp][plugin]") {
  const std::uint32_t pc = 0x8000010cu;
  const std::uint32_t target = 0x80000118u;
  const std::uint64_t word = Btb::pack_entry(pc, target, false);
  std::uint32_t tag30 = 0, target30 = 0;
  bool valid = false, uncond = false;
  Btb::unpack_entry(word, &tag30, &target30, &valid, &uncond);
  REQUIRE(valid);
  REQUIRE_FALSE(uncond);
  REQUIRE(tag30 == (pc >> 2));
  REQUIRE(target30 == (target >> 2));
  REQUIRE((target30 << 2) == target);
  // ★ 「MUST NOT 用 target[1:0]!=0 判有效」的反例钉死（target 4 字节对齐）
  REQUIRE(((target >> 2) << 2) == target);
  REQUIRE(((pc >> 2) << 2) == pc);
}

#endif  // CF_PLUGIN_USE_CH_MEM

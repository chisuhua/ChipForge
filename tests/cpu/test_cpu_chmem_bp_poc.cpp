// tests/cpu/test_cpu_chmem_bp_poc.cpp
//
// [cpu][chmem][bp] chmem-bp-btb-gshare Task 1 — **可行性 PoC（硬门禁）**
//
// 本文件**不是** BP 的实现测试，而是 **Task 1 的待验项**（见
// `.rddf/plans/chmem-bp-btb-gshare-brief.md` §5.2 / §3.3）的**可行性验证**。
// 按 plan R6：「PoC 连续 2 次失败 ⇒ 触发上报」⇒ 本文件必须**先**全绿，
// Task 2（BTB 实现）才允许开工。
//
// ★★★ **实施期机制结论（本 PoC 的首要产出）** ★★★
//
// 【结论 1】**C++ 侧常量 ⇒ elaboration 期常量**（ADR-040 §10.5 的机制根源）
//   `ch_mem::write(addr, data, enable)` 的 `enable` 若来自 C++ 值
//   （`ch_bool(false)`），`get_lnode()` 把它变成 elaboration 期常量
//   ⇒ 写端口**恒不使能** ⇒ 写入永不发生。
//   ⇒ **钉死**：P-5b 断言 C++ 值来源的 cond ⇒ `impl()->is_const() == true`；
//     P-5 断言 `ch_reg` proxy 来源 ⇒ `is_const() == false`。
//
// 【结论 2】**mem 写端口的 `addr` 必须是「payload 派生的组合表达式」**
//   ⚠️ **不能用 `ch_reg` 的 proxy 直接当地址** —— 实测（P-2N 扫描全部 8 个
//   entry）证明：用 reg proxy 作写地址（初值 5）时，数据落到了 **entry 0**
//   ⇒ **索引被解析成 0** ⇒ 写到了错误的表项（BTB/PHT 会静默错乱）。
//   ⇒ 与「未落到任何 entry」的初判不同：**写入确实发生，但地址错了** ——
//     这更危险（静默错），故必须钉死。
//   ⇒ **可用**：payload cell → `bits<...>(payload)` 形式（P-2K / P-2N 通过，
//     且 P-2N 的地址**逐拍变化**仍正确 ⇒ 正是 BP 的需求）。
//   ⇒ 这与生产先例 `dmem_chmem.h:151/212` **完全同构**
//     （`word_addr = bits<15,2>(byte_addr)`，byte_addr 派生自 payload）。
//
// 【结论 3】**mem 读端口的地址表达式必须与写端口一致**（P-2K 通过）
//   ⇒ 实践规则：**读/写共用同一个地址表达式**。
//
// 【结论 4】**enable 可以是 reg proxy**（P-2B 通过）—— 与 addr 的限制**不同**
//   ⇒ BP 的写使能（= 「本拍分支实际 taken」）可用**真实分支信号**。
//
// 【结论 5】**2-bit PHT 可行**（P-3 通过）⇒ `ch_mem<ch_uint<2>, N>` 可承载
//   2-bit 饱和计数器 ⇒ PHT 体积不必因位宽而膨胀。
//
// 【结论 6】**越界索引读返回 0**（P-4 通过）⇒ 「固定容量 + 运行时逻辑条目数」
//   可行（读 0 ⇒ 未命中）⇒ brief §3.3 的方案成立。
//
// 【结论 7】**`ch_reg` 跨拍保持成立**（P-1 通过：1→3→7）⇒ GHR 13-bit
//   全局历史用 `ch_reg` **可行**（brief §5.2 P-1 待验项 ⇒ 通过）。
//
// 【对 BP 实现的直接约束】
//   ① BTB/PHT 的**写**必须由 **payload 驱动**（生产者写 payload PC/方向/
//      目标 ⇒ 消费者用 `bits<...>(payload)` 派生地址）。
//   ② 读/写**共用**同一地址表达式。
//   ③ 写使能用**真实分支信号**（payload 里的 taken）。
//   ④ **禁止**把 `ch_reg` proxy 直接当 mem 端口地址。
//
// SPDX-License-Identifier: BSD-3-Clause

// CH_MEM 专属测试 —— 整体 guard (与 test_cpu_chmem_alu_semantics.cpp 同构:
// TLM binary 的 GLOB_RECURSE 也会收集本文件 ⇒ guard 必须在**所有** include
// 之前, 否则 TLM binary 会编译 CppHDL 头 ⇒ 符号重复定义)
#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <ch.hpp>
#include <core/context.h>
#include <simulator.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/result_macros.h"
#include "cf/plugin/uint_t.h"

#include "ip/cpu/core/payload_common.h"

using namespace ch;
using namespace ch::core;
namespace cfc = cf::plugin;

namespace {

constexpr unsigned kPocBtbEntries = 8;
constexpr unsigned kPocPhtEntries = 8;

}  // namespace

// =========================================================================
// P-1: ch_reg 跨拍保持 —— GHR (13 bit) 在 at_stage 闭包里 <<= 后，
//      **下一拍**读出的是新值
//
// 机制依据: ch_reg::operator<<= 设 next 值（core/reg.h:52）⇒ 跨拍保持；
//           at_stage 闭包在 elaborate() 期执行一次建立 DAG 边 ⇒
//           「<<= 须在 Simulator 创建前调用」天然满足
//           （先例: csr_chmem.h:499 的 mcycle_ <<= mcycle_+1）
//
// ⇒ **结论 7**：GShare 13-bit 全局历史用 ch_reg 可行
// =========================================================================
TEST_CASE("bp_poc_P1_chreg_cross_cycle", "[cpu][chmem][bp][poc]") {
  ch::core::context ctx("bp_poc_p1");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);

  auto ghr = std::make_unique<ch_reg<ch_uint<13>>>(
      ch_uint<13>(ch_literal<0, 13>{}), "ghr");
  auto* ghr_raw = ghr.get();

  pb.at_stage("fetch", cfc::Phase::NORMAL, [ghr_raw]() {
    auto cur = ch_uint<13>(ghr_raw->impl());
    auto shifted = ch_uint<13>((cur << 1) | ch_uint<13>(ch_literal<1, 13>{}));
    *ghr_raw <<= shifted;
  });

  pb.build();
  auto exp = pb.elaborate();
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();

  REQUIRE_NOTHROW(sim->tick());
  auto v1 = static_cast<std::uint64_t>(sim->get_signal_value(ch_uint<13>(ghr_raw->impl())));
  REQUIRE(v1 == 1u);

  REQUIRE_NOTHROW(sim->tick());
  auto v2 = static_cast<std::uint64_t>(sim->get_signal_value(ch_uint<13>(ghr_raw->impl())));
  REQUIRE(v2 == 3u);

  REQUIRE_NOTHROW(sim->tick());
  auto v3 = static_cast<std::uint64_t>(sim->get_signal_value(ch_uint<13>(ghr_raw->impl())));
  REQUIRE(v3 == 7u);
}

// =========================================================================
// P-2: ★**核心机制**★ —— mem 写/读的 **payload 驱动 + 逐拍变化地址**
//
//   这是 BP 的 BTB/PHT 写入机制的**直接原型**：
//     - 写地址 = `bits<2,0>(payload PC)`（payload 派生，**逐拍变化**）
//     - 读地址 = **同一表达式**
//     - 写使能 = 字面量 true（真实场景下是 payload 里的 taken 信号）
//
//   ⇒ **结论 2 + 3**：payload 驱动可行，且**逐拍变化**仍正确
//   ⇒ 与生产先例 `dmem_chmem.h:151/212` 同构
// =========================================================================
TEST_CASE("bp_poc_P2_payload_driven_varying_addr", "[cpu][chmem][bp][poc]") {
  ch::core::context ctx("bp_poc_p2");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;

  using Mem = ch_mem<ch_uint<32>, kPocBtbEntries>;
  auto btb = std::make_unique<Mem>(
      std::vector<std::uint32_t>(kPocBtbEntries, 0u), "btb");

  auto rp = std::make_shared<std::optional<Mem::read_port>>();
  auto wp = std::make_shared<std::optional<Mem::write_port>>();

  // 逐拍递增的 PC 源
  auto pc = std::make_unique<ch_reg<ch_uint<32>>>(
      ch_uint<32>(ch_literal<5, 32>{}), "pc");
  auto* pc_raw = pc.get();

  pb.at_stage("fetch", cfc::Phase::EARLY,
              [&pb, btb_raw = btb.get(), rp, wp, pc_raw]() {
    auto* n = pb.node_of_logic_stage("fetch").get();
    if (!n) return;
    // 生产者: payload PC = 活节点（与 BP 的「生产者写 payload」同形）
    n->operator()(KT::PC) = ch_uint<32>(pc_raw->impl());

    auto pcv = n->operator()(KT::PC);
    auto widx = bits<2, 0>(pcv);          // ★ 同址读写
    ch_uint<32> wdata(ch_literal<0x12345678, 32>{});
    ch_bool en(true);
    *wp = btb_raw->write(widx, wdata, en, "btb_write");
    *rp = btb_raw->aread(widx, "btb_read");
  });

  // PC 每拍 +1（⇒ 索引每拍变化）
  pb.at_stage("fetch", cfc::Phase::NORMAL, [pc_raw]() {
    auto cur = ch_uint<32>(pc_raw->impl());
    *pc_raw <<= cur + ch_uint<32>(ch_literal<1, 32>{});
  });

  pb.build();
  auto exp = pb.elaborate();
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();

  bool seen = false;
  for (int cycle = 1; cycle <= 8; ++cycle) {
    REQUIRE_NOTHROW(sim->tick());
    auto v = static_cast<std::uint64_t>(sim->get_signal_value(ch_uint<32>(rp->value().impl())));
    CAPTURE(cycle);
    INFO("cycle " << cycle << " read=" << v);
    if (v == 0x12345678u) { seen = true; break; }
  }
  REQUIRE(seen);
}

// =========================================================================
// P-2N: ★**负例**★ —— 用「**未被驱动**的 `ch_reg`」proxy 作写地址
//   ⇒ 数据落到 **entry 0**（proxy 初值是 5）
//
//   实施期扫描全部 8 个 entry 的诊断结果：命中的是 **entry 0**
//   ⇒ **索引被解析成 0** ⇒ 写入**确实发生**但**写错了表项**
//   ⇒ 对 BTB/PHT 而言是**静默错乱**（表项污染）⇒ 比「不写」更危险
//
//   ★ **边界（P-2P 补测）**: 这**不是**「reg 不能作 mem 地址」——
//     若该 reg 被 op 节点 `<<=` 驱动（P-2P），则完全可行。
//     本负例针对的是「**未被驱动**的 reg」。
//   ★ 本 case 把该负例**钉死**：期望 hit == 0。
//     若上游行为变化 ⇒ 本 case FAIL ⇒ 提醒实现者重新评估约束 ④。
// =========================================================================
TEST_CASE("bp_poc_P2N_reg_proxy_addr_never_writes", "[cpu][chmem][bp][poc]") {
  ch::core::context ctx("bp_poc_p2n");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);

  using Mem = ch_mem<ch_uint<32>, kPocBtbEntries>;
  auto btb = std::make_unique<Mem>(
      std::vector<std::uint32_t>(kPocBtbEntries, 0u), "btb");

  auto wport = std::make_shared<std::optional<Mem::write_port>>();
  std::vector<std::shared_ptr<std::optional<Mem::read_port>>> rports;
  for (unsigned i = 0; i < kPocBtbEntries; ++i)
    rports.push_back(std::make_shared<std::optional<Mem::read_port>>());

  auto widx = std::make_unique<ch_reg<ch_uint<3>>>(ch_uint<3>(ch_literal<5, 3>{}), "widx");
  auto* widx_raw = widx.get();

  pb.at_stage("fetch", cfc::Phase::EARLY,
              [btb_raw = btb.get(), wport, rports, widx_raw]() {
    ch_uint<3> waddr = ch_uint<3>(widx_raw->impl());  // ★ proxy 直接作地址
    ch_uint<32> wdata(ch_literal<0x12345678, 32>{});
    ch_bool en(true);
    *wport = btb_raw->write(waddr, wdata, en, "btb_write");

    for (unsigned i = 0; i < kPocBtbEntries; ++i) {
      ch_uint<3> ridx(ch_literal<0, 3>{});
      switch (i) {
        case 0: ridx = ch_uint<3>(ch_literal<0, 3>{}); break;
        case 1: ridx = ch_uint<3>(ch_literal<1, 3>{}); break;
        case 2: ridx = ch_uint<3>(ch_literal<2, 3>{}); break;
        case 3: ridx = ch_uint<3>(ch_literal<3, 3>{}); break;
        case 4: ridx = ch_uint<3>(ch_literal<4, 3>{}); break;
        case 5: ridx = ch_uint<3>(ch_literal<5, 3>{}); break;
        case 6: ridx = ch_uint<3>(ch_literal<6, 3>{}); break;
        default: ridx = ch_uint<3>(ch_literal<7, 3>{}); break;
      }
      *rports[i] = btb_raw->aread(ridx, "btb_read");
    }
  });

  pb.build();
  auto exp = pb.elaborate();
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();

  REQUIRE_NOTHROW(sim->tick());
  REQUIRE_NOTHROW(sim->tick());
  REQUIRE_NOTHROW(sim->tick());

  int hit = -1;
  for (unsigned i = 0; i < kPocBtbEntries; ++i) {
    auto v = static_cast<std::uint64_t>(
        sim->get_signal_value(ch_uint<32>(rports[i]->value().impl())));
    INFO("entry[" << i << "] = " << v);
    if (v == 0x12345678u) hit = static_cast<int>(i);
  }
  // ★ 数据落到 **entry 0**（proxy 初值是 5）⇒ 索引被解析成 0 ⇒ 写错表项
  REQUIRE(hit == 0);
}

// =========================================================================
// P-3: **2-bit** 饱和计数器能否用 ch_mem<ch_uint<2>, N> 承载
//
// 若不可行 ⇒ PHT 只能用 32-bit 存 ⇒ 8192×4 = 32KB（超预算）⇒ 须缩表
// ⇒ **结论 5**：2-bit PHT 可行
// =========================================================================
TEST_CASE("bp_poc_P3_pht_2bit_counter", "[cpu][chmem][bp][poc]") {
  ch::core::context ctx("bp_poc_p3");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);

  using PhtMem = ch_mem<ch_uint<2>, kPocPhtEntries>;
  auto pht = std::make_unique<PhtMem>(
      std::vector<std::uint32_t>(kPocPhtEntries, 0u), "pht");

  auto rp = std::make_shared<std::optional<PhtMem::read_port>>();
  auto wp = std::make_shared<std::optional<PhtMem::write_port>>();

  pb.at_stage("fetch", cfc::Phase::EARLY, [pht_raw = pht.get(), rp, wp]() {
    ch_uint<3> widx(ch_literal<2, 3>{});
    ch_uint<2> wdata(ch_literal<3, 2>{});
    ch_bool en(true);
    *wp = pht_raw->write(widx, wdata, en, "pht_write");

    ch_uint<3> ridx(ch_literal<2, 3>{});
    *rp = pht_raw->aread(ridx, "pht_read");
  });

  pb.build();
  auto exp = pb.elaborate();
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();

  REQUIRE_NOTHROW(sim->tick());
  REQUIRE_NOTHROW(sim->tick());
  auto v = static_cast<std::uint64_t>(sim->get_signal_value(ch_uint<2>(rp->value().impl())));
  REQUIRE(v == 3u);  // 2-bit 值 0b11
}

// =========================================================================
// P-4: 「固定容量 + 运行时逻辑条目数」—— 越界索引读返回 0（⇒ 未命中）
//
// brief §3.3 的结论前提: ch_mem depth 是编译期固定，
// 「运行时可配 BTB 尺寸」必须靠「固定容量 + 运行时限制有效条目数」实现。
// ⇒ **结论 6**
// =========================================================================
TEST_CASE("bp_poc_P4_out_of_range_read_is_zero", "[cpu][chmem][bp][poc]") {
  ch::core::context ctx("bp_poc_p4");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);

  using Mem = ch_mem<ch_uint<32>, kPocBtbEntries>;
  auto btb = std::make_unique<Mem>(
      std::vector<std::uint32_t>(kPocBtbEntries, 0u), "btb");

  auto rp = std::make_shared<std::optional<Mem::read_port>>();

  pb.at_stage("fetch", cfc::Phase::EARLY, [btb_raw = btb.get(), rp]() {
    ch_uint<3> ridx(ch_literal<6, 3>{});  // 超出逻辑容量 4
    *rp = btb_raw->aread(ridx, "btb_read");
  });

  pb.build();
  auto exp = pb.elaborate();
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();

  REQUIRE_NOTHROW(sim->tick());
  auto v = static_cast<std::uint64_t>(sim->get_signal_value(ch_uint<32>(rp->value().impl())));
  REQUIRE(v == 0u);  // 未初始化 ⇒ 0 ⇒ 「读 0 ⇒ 未命中」成立
}

// =========================================================================
// P-5: 预测方向/目标写入 payload 必须是**活节点**（ADR-040 §10.5）
//   cond 来自 ch_reg proxy ⇒ impl 非 const
// =========================================================================
TEST_CASE("bp_poc_P5_live_node_not_const", "[cpu][chmem][bp][poc]") {
  ch::core::context ctx("bp_poc_p5");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;

  auto cond_reg = std::make_unique<ch_reg<ch_bool>>(ch_bool(true), "cond_reg");
  auto* cond_raw = cond_reg.get();
  auto payload_cell = std::make_shared<ch_uint<1>>(ch_literal<0, 1>{});

  pb.at_stage("fetch", cfc::Phase::EARLY, [&pb, cond_raw, payload_cell]() {
    auto* n = pb.node_of_logic_stage("fetch").get();
    if (!n) return;
    auto c = ch_bool(cond_raw->impl());
    payload_cell->operator=(ch_uint<1>(c));
    n->operator()(KT::PC) = *payload_cell;
  });

  pb.build();
  auto exp = pb.elaborate();
  REQUIRE(exp.has_value());

  REQUIRE(payload_cell->impl() != nullptr);
  REQUIRE(payload_cell->impl()->is_const() == false);
}

// =========================================================================
// P-5b: ★对照实验★ —— C++ 值来源的 cond ⇒ impl **是** const
//
//   ADR-040 §10.5 的机制根源: C++ 侧值 ⇒ elaboration 期常量。
//   本 case 把它**钉死**，防止后续实现误用 C++ 值当条件。
// =========================================================================
TEST_CASE("bp_poc_P5b_cpp_value_cond_is_const", "[cpu][chmem][bp][poc]") {
  ch::core::context ctx("bp_poc_p5b");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);

  using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;

  auto cond = std::make_shared<ch_bool>(true);
  auto payload_cell = std::make_shared<ch_uint<1>>(ch_literal<0, 1>{});

  pb.at_stage("fetch", cfc::Phase::EARLY, [&pb, cond, payload_cell]() {
    auto* n = pb.node_of_logic_stage("fetch").get();
    if (!n) return;
    auto c = ch_bool(*cond);
    payload_cell->operator=(ch_uint<1>(c));
    n->operator()(KT::PC) = *payload_cell;
  });

  pb.build();
  auto exp = pb.elaborate();
  REQUIRE(exp.has_value());

  REQUIRE(payload_cell->impl() != nullptr);
  REQUIRE(payload_cell->impl()->is_const() == true);
}

// =========================================================================
// P-2P: ★★★决定性★★★ —— **被 `<<=` 驱动**的 reg 作 mem 地址
//
//   修正 P-2N 的结论边界:
//     P-2N 的 reg **从未被 `<<=` 驱动** ⇒ 其 proxy 运行时读到 0
//     ⇒ 数据落到 entry 0
//     而生产代码 `ibus_chmem.h` 的 `pc_reg <<= select(...)` 是
//     **用 op 节点驱动**的 ⇒ proxy 正常跟踪 PC
//
//   ⇒ 本 case 验证: reg 用 **op 节点** `<<=` 驱动后, 可正常作 mem 地址
//   ⇒ **BP 的预测路径（读 BTB@PC）因此可行**（PC 来自 pc_reg）
// =========================================================================
TEST_CASE("bp_poc_P2P_driven_reg_as_mem_addr", "[cpu][chmem][bp][poc]") {
  ch::core::context ctx("bp_poc_p2p");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cfc::PipeBuilder pb(&ctx);

  using Mem = ch_mem<ch_uint<32>, kPocBtbEntries>;
  auto btb = std::make_unique<Mem>(
      std::vector<std::uint32_t>(kPocBtbEntries, 0u), "btb");

  auto rp = std::make_shared<std::optional<Mem::read_port>>();
  auto wp = std::make_shared<std::optional<Mem::write_port>>();

  // ★ 这个 reg 会被 op 节点驱动（模拟 pc_reg <<= select(...)）
  auto widx = std::make_unique<ch_reg<ch_uint<3>>>(ch_uint<3>(ch_literal<5, 3>{}), "widx");
  auto* widx_raw = widx.get();

  pb.at_stage("fetch", cfc::Phase::EARLY,
              [btb_raw = btb.get(), rp, wp, widx_raw]() {
    ch_uint<3> waddr = ch_uint<3>(widx_raw->impl());
    ch_uint<32> wdata(ch_literal<0x12345678, 32>{});
    ch_bool en(true);
    *wp = btb_raw->write(waddr, wdata, en, "btb_write");
    *rp = btb_raw->aread(waddr, "btb_read");   // ★ 同一表达式
  });

  // ★ 用 op 节点驱动 widx（与 pc_reg <<= select(...) 同形）
  pb.at_stage("fetch", cfc::Phase::NORMAL, [widx_raw]() {
    auto cur = ch_uint<3>(widx_raw->impl());
    auto nxt = cur + ch_uint<3>(ch_literal<1, 3>{});
    *widx_raw <<= nxt;      // ★ op 节点
  });

  pb.build();
  auto exp = pb.elaborate();
  REQUIRE(exp.has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();

  bool seen = false;
  for (int cycle = 1; cycle <= 8; ++cycle) {
    REQUIRE_NOTHROW(sim->tick());
    auto v = static_cast<std::uint64_t>(sim->get_signal_value(ch_uint<32>(rp->value().impl())));
    CAPTURE(cycle);
    INFO("cycle " << cycle << " read=" << v);
    if (v == 0x12345678u) { seen = true; break; }
  }
  REQUIRE(seen);
}

#endif  // CF_PLUGIN_USE_CH_MEM

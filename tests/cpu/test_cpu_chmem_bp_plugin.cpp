// tests/cpu/test_cpu_chmem_bp_plugin.cpp
//
// CH_MEM 分支预测器插件的单元测试 (change chmem-bp-btb-gshare-poc)
//
// ★★ 覆盖强度 (critic 第四轮 B-1): 上一版 6 case / 52 assertions 对它们声称的
//   性质**零鉴别力** —— critic 实测: 同时施加「BTB 永不写」+「方向学习改回
//   恒 +1」两个变异后, 上一版仍输出「All tests passed (52 assertions in
//   6 test cases)」。⇒ 本版全部断言**断值**（不只断「是活节点」）:
//   1) R1 注册位 + BTB 真被写 ⇒ 断 `BP_PRED_VALID` **值** == 1
//      + 反序注册 ⇒ elaborate **抛异常**（fail-fast, 不静默降级）
//   2) R2 BTB 表项内容 ⇒ 断 `BP_PRED_TARGET` **值** == 0x80000118
//   3) R3 方向学习 ⇒ 先 taken 建项, 再灌 not-taken ⇒ pred_taken 由 1 翻 0
//   4) R7 flush 门控 ⇒ 错路指令**不污染 PHT** ⇒ pred_taken 保持 1
//   5) R4 索引 = PC[9:2] · R5 合法域 fail-fast · R6 62b 编码往返
//
// ★ 驱动器的「方向 / 错路标记」是**随 tick 变化的活表达式**（由 tick 计数寄存器
//   驱动）—— CH_MEM 里 at_stage 闭包**只在 elaboration 跑一次** ⇒ 想在运行期
//   改变分支方向, MUST 让 payload 的值是**活节点**（写成常量则只在 elaboration
//   定值 ⇒ 变不了）。★ 这同时额外验证了 BP 消费的是**活**信号（ADR-040 §10.5）。
//
// SPDX-License-Identifier: BSD-3-Clause

#include "catch_amalgamated.hpp"

#ifdef CF_PLUGIN_USE_CH_MEM

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <ch.hpp>
#include <core/context.h>
#include <simulator.h>
#include <core/uint.h>
#include <core/reg.h>
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

/// 读 payload cell 的**运行期值**（1b）
std::uint64_t read_u1(ch::Simulator& sim, const ch::core::ch_uint<1>& cell) {
  return static_cast<std::uint64_t>(
      sim.get_signal_value(ch::core::ch_uint<1>(cell.impl())));
}

/// 读 payload cell 的**运行期值**（32b）
std::uint64_t read_u32(ch::Simulator& sim, const ch::core::ch_uint<32>& cell) {
  return static_cast<std::uint64_t>(
      sim.get_signal_value(ch::core::ch_uint<32>(cell.impl())));
}

/// 一个最小「生产者」插件：给 fetch/branch 节点写 BP 需要的输入，
/// 让**真实的 BP 插件**跑起来。
///
/// ★ 关键：BP 的两个回调分别依赖
///   - fetch NORMAL : `BP_PC`（= 本拍取指地址）
///   - branch NORMAL: `BRANCH_TAKEN` / `BRANCH_TAKEN_RAW` / `BRANCH_FLUSHED` /
///                    `IS_BRANCH` / `KT::PC` / `BRANCH_TARGET`
/// 本插件扮演「IBus + Branch」的角色，按**分段脚本**驱动这些信号。
class FakeCpuDriver : public cf::plugin::PluginBase {
 public:
  struct Phase {
    std::uint32_t end_tick;  ///< 本段结束 tick（含）；tick < end_tick 属于本段
    bool taken;               ///< 本段内分支的**原始**方向
    bool flushed;             ///< 本段内指令是否是「错路指令」
  };

  FakeCpuDriver(std::uint32_t br_pc, std::uint32_t br_target,
                std::vector<Phase> phases)
      : br_pc_(br_pc), br_target_(br_target), phases_(std::move(phases)) {
    if (phases_.empty()) {
      throw std::runtime_error("FakeCpuDriver: phases must not be empty");
    }
  }

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

      // ★ 方向 / 错路标记 = **随 tick 变化的活表达式**
      const ch::core::ch_bool taken_raw = phase_value(
          [](const Phase& p) { return p.taken; });
      const ch::core::ch_bool flushed = phase_value(
          [](const Phase& p) { return p.flushed; });
      // ★ 与真实 branch plugin 同口径: `BRANCH_TAKEN` 是**被 flush 抑制后**的值,
      //   `BRANCH_TAKEN_RAW` 是**未抑制**的方向（供 PHT 方向学习）。
      const ch::core::ch_bool taken = taken_raw & (!flushed);
      n->operator()(BrPlugin::get_branch_taken_key()) = taken;
      n->operator()(BrPlugin::BRANCH_TAKEN_RAW) = taken_raw;
      n->operator()(BrPlugin::BRANCH_FLUSHED) = flushed;
      n->operator()(BrPlugin::IS_BRANCH) = ch::core::ch_bool(true);

      // ★ tick 计数寄存器自增（每拍 +1）⇒ phase 判据随拍变化
      const ch::core::ch_uint<32> one(static_cast<std::uint32_t>(1));
      ch::core::ch_uint<32> tv(tick_count_);
      tick_count_ <<= tv + one;
    });
  }

  void build(cf::plugin::PipeBuilder&) override {}

 private:
  /// 取「当前 tick 落在哪一段」的**活**布尔表达式
  template <typename Pick>
  ch::core::ch_bool phase_value(Pick pick) const {
    // 末段无上界（兜底）
    ch::core::ch_bool acc(pick(phases_.back()));
    for (std::size_t i = phases_.size() - 1; i-- > 0;) {
      const ch::core::ch_uint<32> c(tick_count_);
      const ch::core::ch_uint<32> thresh(
          static_cast<std::uint32_t>(phases_[i].end_tick));
      acc = select(c < thresh, ch::core::ch_bool(pick(phases_[i])), acc);
    }
    return acc;
  }

  std::uint32_t pc_ = 0x8000010cu;
  std::uint32_t br_pc_;
  std::uint32_t br_target_;
  std::vector<Phase> phases_;
  ch::core::ch_reg<ch::core::ch_uint<32>> tick_count_{
      ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}), "bp_test_tick"};
};

/// 起一套「生产者 + BP」的仿真环境；返回 simulator（调用方负责 tick）
std::unique_ptr<ch::Simulator> make_sim(
    cf::plugin::PipeBuilder& pb,
    std::vector<FakeCpuDriver::Phase> phases,
    bool bp_first = false) {
  if (bp_first) {
    // ★ 反序注册（BP 在生产者之前）—— 用于验证 **fail-fast**（不静默降级）
    pb.register_plugin(std::make_unique<Btb>(256u));
    pb.register_plugin(std::make_unique<FakeCpuDriver>(
        0x8000010cu, 0x80000118u, std::move(phases)));
  } else {
    pb.register_plugin(std::make_unique<FakeCpuDriver>(
        0x8000010cu, 0x80000118u, std::move(phases)));
    pb.register_plugin(std::make_unique<Btb>(256u));
  }
  pb.build();
  REQUIRE(pb.elaborate().has_value());
  auto sim = cfc::auto_throw(pb.create_simulator());
  REQUIRE(sim != nullptr);
  sim->reset();
  return sim;
}

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════
// 1) 注册位顺序：BP 必须在「生产者」之后 ⇒ 否则 BTB 永不写
//    ★ 断**值**（不是「是活节点」）: BP_PRED_VALID == 1 ⇒ 表项真被写过
//    ★ 反序注册 ⇒ **抛异常**（fail-fast），不是静默跑出全 0
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_R1_btb_is_written_when_producer_comes_first",
          "[cpu][chmem][bp][plugin]") {
  ch::core::context ctx("bp_r1");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  // ---- (a) 正序：生产者先注册 ⇒ BTB 真被写 ⇒ 命中 ----
  {
    cf::plugin::PipeBuilder pb(&ctx);
    auto sim = make_sim(pb, {{1000u, /*taken=*/true, /*flushed=*/false}});

    auto* fnode = pb.node_of_logic_stage("fetch").get();
    REQUIRE(fnode != nullptr);

    for (int i = 0; i < 6; ++i) REQUIRE_NOTHROW(sim->tick());

    // ★ 该 cell MUST 是活节点（AC-F2-12）—— 若被折成 elaboration 期常量 ⇒ RED
    const auto valid = fnode->operator()(BP_PRED_VALID);
    REQUIRE(valid.impl() != nullptr);
    REQUIRE_FALSE(valid.impl()->is_const());
    // ★★ 断**值**: BTB 真被写 ⇒ 表项命中 ⇒ valid == 1
    //   （若 BTB 写路径被废（write_en 恒 false）⇒ valid == 0 ⇒ RED ★ 这正是
    //    critic 的 MUTATION-A：上一版对此**零鉴别力**）
    REQUIRE(read_u1(*sim, valid) == 1u);
  }

  // ---- (b) 反序：BP 先注册 ⇒ 生产者的 payload 还没写 ⇒ **fail-fast** ----
  {
    cf::plugin::PipeBuilder pb2(&ctx);
    pb2.register_plugin(std::make_unique<Btb>(256u));
    pb2.register_plugin(std::make_unique<FakeCpuDriver>(
        0x8000010cu, 0x80000118u,
        std::vector<FakeCpuDriver::Phase>{{1000u, true, false}}));
    pb2.build();
    // ★ MUST 抛异常（BP_PC / BRANCH_* 缺失 ⇒ 快速失败）
    //   —— 静默降级为 false 会让 BP「看起来在跑但什么都不学」
    REQUIRE_THROWS(pb2.elaborate());
  }
}

// ═════════════════════════════════════════════════════════════════════════════
// 2) BTB 写→读闭环：表项的 **target 值** MUST 与分支目标逐位相等
//    （上一版只断「是活节点」⇒ BTB 全空时 target=0 也照样绿 ★ 零鉴别力）
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_R2_btb_entry_readable_after_update", "[cpu][chmem][bp][plugin]") {
  ch::core::context ctx("bp_r2");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cf::plugin::PipeBuilder pb(&ctx);
  auto sim = make_sim(pb, {{1000u, /*taken=*/true, /*flushed=*/false}});

  auto* fnode = pb.node_of_logic_stage("fetch").get();
  REQUIRE(fnode != nullptr);

  for (int i = 0; i < 4; ++i) REQUIRE_NOTHROW(sim->tick());

  const auto target = fnode->operator()(BP_PRED_TARGET);
  REQUIRE(target.impl() != nullptr);
  REQUIRE_FALSE(target.impl()->is_const());
  const auto valid = fnode->operator()(BP_PRED_VALID);
  REQUIRE(valid.impl() != nullptr);
  REQUIRE_FALSE(valid.impl()->is_const());
  // ★★ 断**值**: 命中的表项 target MUST == 分支目标 0x80000118
  //   （若 BTB 写路径被废 ⇒ valid=0 且 target=0 ⇒ 两条断言都 RED）
  REQUIRE(read_u1(*sim, valid) == 1u);
  REQUIRE(read_u32(*sim, target) == 0x80000118u);
}

// ═════════════════════════════════════════════════════════════════════════════
// 3) PHT 方向学习：taken ⇒ 饱和 +1；not-taken ⇒ 饱和 −1
//    ★ 上一版场景是「恒 not-taken」⇒ BTB 写使能恒 false ⇒ hit 恒 false ⇒
//      pred_taken 恒 0 ⇒ **无论 PHT 是方向学习 / 恒 +1 / 回绕都绿** ★ 零鉴别力。
//    ★ 本版: 先 taken 建 BTB 项（hit=1）⇒ 再灌 not-taken ⇒ 断言 pred_taken
//      由 1 翻 0（恒 +1 的变异 ⇒ 停在 1 ⇒ RED ★ MUTATION-B）
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_R3_pht_learns_not_taken", "[cpu][chmem][bp][plugin]") {
  ch::core::context ctx("bp_r3");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cf::plugin::PipeBuilder pb(&ctx);
  // ★ 分段脚本: 前 8 拍 taken（建 BTB 项 + PHT 饱和到 3），之后恒 not-taken
  auto sim = make_sim(pb, {{8u, /*taken=*/true, /*flushed=*/false},
                           {1000u, /*taken=*/false, /*flushed=*/false}});

  auto* fnode = pb.node_of_logic_stage("fetch").get();
  REQUIRE(fnode != nullptr);

  const auto valid = fnode->operator()(BP_PRED_VALID);
  const auto taken_cell = fnode->operator()(BP_PRED_TAKEN);
  REQUIRE(valid.impl() != nullptr);
  REQUIRE_FALSE(valid.impl()->is_const());
  REQUIRE(taken_cell.impl() != nullptr);
  REQUIRE_FALSE(taken_cell.impl()->is_const());

  // ---- 阶段 1: taken ⇒ PHT 饱和到 3 ⇒ 预测 taken ----
  for (int i = 0; i < 6; ++i) REQUIRE_NOTHROW(sim->tick());
  REQUIRE(read_u1(*sim, valid) == 1u);        // BTB 已建项 ⇒ 命中
  REQUIRE(read_u1(*sim, taken_cell) == 1u);   // counter >= 2 ⇒ 预测 taken

  // ---- 阶段 2: 灌 not-taken ⇒ 计数器**递减** ⇒ 预测翻成 not-taken ----
  for (int i = 0; i < 8; ++i) REQUIRE_NOTHROW(sim->tick());
  REQUIRE(read_u1(*sim, valid) == 1u);        // BTB 项仍在（not-taken 不写表）
  // ★★ 断**值**: 方向学习生效 ⇒ counter 由 3 递减到 0 ⇒ 预测 not-taken
  //   （若 PHT 退化为「任何方向都 +1」⇒ counter 停在 3 ⇒ 预测恒 taken ⇒ RED）
  REQUIRE(read_u1(*sim, taken_cell) == 0u);
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

// ═════════════════════════════════════════════════════════════════════════════
// 7) flush 门控（B-7）: **错路指令 MUST NOT 污染 PHT**
//    ★ 场景: 先 taken 2 拍（counter 2 ⇒ 预测 taken），再切到「错路 + not-taken」
//    ★ 判据: 有门控 ⇒ counter 停在 2 ⇒ 预测**保持 taken**;
//            无门控 ⇒ counter 2→1→0 ⇒ 预测翻成 not-taken ⇒ RED
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_R7_flushed_branch_does_not_pollute_pht",
          "[cpu][chmem][bp][plugin]") {
  ch::core::context ctx("bp_r7");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cf::plugin::PipeBuilder pb(&ctx);
  auto sim = make_sim(pb, {{2u, /*taken=*/true, /*flushed=*/false},
                            {1000u, /*taken=*/false, /*flushed=*/true}});

  auto* fnode = pb.node_of_logic_stage("fetch").get();
  REQUIRE(fnode != nullptr);

  const auto valid = fnode->operator()(BP_PRED_VALID);
  const auto taken_cell = fnode->operator()(BP_PRED_TAKEN);
  REQUIRE(valid.impl() != nullptr);
  REQUIRE_FALSE(valid.impl()->is_const());
  REQUIRE(taken_cell.impl() != nullptr);
  REQUIRE_FALSE(taken_cell.impl()->is_const());

  // ---- 阶段 1: 正常 taken 2 拍 ⇒ BTB 建项 + counter 升到 2 ----
  for (int i = 0; i < 2; ++i) REQUIRE_NOTHROW(sim->tick());

  // ---- 阶段 2: 错路 + not-taken ⇒ 门控生效 ⇒ PHT 不被污染 ----
  for (int i = 0; i < 6; ++i) REQUIRE_NOTHROW(sim->tick());

  REQUIRE(read_u1(*sim, valid) == 1u);        // BTB 项仍在（错路不写表）
  // ★★ 断**值**: 错路指令**没有**把 PHT 拉低 ⇒ 预测方向保持 taken
  //   （若无 flush 门控 ⇒ counter 2→1→0 ⇒ 预测翻成 not-taken ⇒ RED）
  REQUIRE(read_u1(*sim, taken_cell) == 1u);
}

// ═════════════════════════════════════════════════════════════════════════════
// 8) PHT 饱和**不回头**（ADR-040 §10.5 那一族的回归锚）
//    ★ 场景: 连续 taken 16 拍 ⇒ 计数器 MUST 饱和在 3（**不得** 3→0 回绕）
//    ★ 判据: 饱和后**每一拍**的预测方向 MUST 是 taken
//      —— 若饱和判定被写成 C++ 三目 `?:`（elaboration 期折常量 ⇒ 饱和分支成
//         死代码）⇒ 写入值恒为 (cur+1)&3 ⇒ 计数器 0→1→2→3→0→1… ⇒ 每 4 拍
//         出现一次 0 ⇒ 预测翻 0 ⇒ RED
//    ★ 系统级对应指纹 = `pred_taken ≈ hit/2`（实测 m1: 3996 → 2000）
//    ★ MUST 逐拍采样（只在相位末尾采一次 ⇒ 采样点可能恰好落在饱和值上 ⇒
//      零鉴别力 —— 这正是 critic B-1 的同型陷阱）
// ═════════════════════════════════════════════════════════════════════════════
TEST_CASE("bp_plugin_R8_pht_saturates_without_wraparound", "[cpu][chmem][bp][plugin]") {
  ch::core::context ctx("bp_r8");
  ctx.set_as_current_context();
  ch::core::ctx_swap guard(&ctx);

  cf::plugin::PipeBuilder pb(&ctx);
  auto sim = make_sim(pb, {{1000u, /*taken=*/true, /*flushed=*/false}});

  auto* fnode = pb.node_of_logic_stage("fetch").get();
  REQUIRE(fnode != nullptr);

  const auto valid = fnode->operator()(BP_PRED_VALID);
  const auto taken_cell = fnode->operator()(BP_PRED_TAKEN);
  REQUIRE(valid.impl() != nullptr);
  REQUIRE_FALSE(valid.impl()->is_const());
  REQUIRE(taken_cell.impl() != nullptr);
  REQUIRE_FALSE(taken_cell.impl()->is_const());

  // ★ 前 4 拍用于建 BTB 项 + 让计数器爬到饱和；之后**逐拍**断言
  for (int i = 0; i < 16; ++i) {
    REQUIRE_NOTHROW(sim->tick());
    if (i >= 4) {
      REQUIRE(read_u1(*sim, valid) == 1u);
      // ★★ 断**值**: 饱和后每拍都 MUST 预测 taken（回绕 ⇒ 某拍为 0 ⇒ RED）
      REQUIRE(read_u1(*sim, taken_cell) == 1u);
    }
  }
}

#endif  // CF_PLUGIN_USE_CH_MEM

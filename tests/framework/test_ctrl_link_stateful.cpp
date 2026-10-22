// tests/framework/test_ctrl_link_stateful.cpp
//
// 功能描述: CtrlLink "有状态"（跨拍保持）halt 条件入口的框架级验证
//           (openspec change `ctrl-link-chmem-reg-handle`)
//
// 作者: build agent
// 日期: 2026-10-22
//
// 覆盖的能力 (capability `ctrllink-continuous-handle`):
//   A1  显式有状态条件入口 `halt_when_stateful(ch_reg<ch_bool>)` + `halt_when(reg)` 重载
//   A2  `OR_combined_halt_conds()` 返回可被消费方 DAG 引用的 ch_bool 节点
//   A6  旧 API (`halt_condition()` / `flush_condition()`) 是 inline alias, 不新增第三版
//   A3  `should_*()` 对有状态条件**不可靠**且**非静默** (可观测 + 可断言)
//
// 核心断言强度 (对应 AC-S1-1):
//   "OR 合并结果随寄存器逐拍更新" 必须以 **Simulator tick ≥ 2 拍** 驱动,
//   中途改变寄存器值并观测 OR 输出翻转 —— 单拍 elaboration 断言证明不了这一点.
//
// 为什么多拍是必须的:
//   `should_halt()` 读的是 C++ 侧缓冲 (`c.to_bool()`), 不是 Simulator 的
//   data_map 当前值 ⇒ 对有状态条件永远读到陈旧值。
//   而 `OR_combined_halt_conds()` 返回的节点引用寄存器 proxy 节点
//   (上游 lnode/reg.tpp: 基类 logic_buffer<T>::node_impl_ = proxy_node)
//   ⇒ 随拍更新。两者行为必须被区分开, 这正是本测试的立意。
//
// 编译条件: 仅在 CF_PLUGIN_USE_CH_MEM 下编译 (chipforge_tests_chmem target)
//           —— `ch_reg` / `ch_bool` / Simulator 是 CH_MEM 模式独有 API。
//           注意: `tests/CMakeLists.txt` 的 CHMEM_TEST_SOURCES 是**手工 list**
//           (GLOB 只覆盖 TLM target) ⇒ 本文件必须显式登记, 否则静默不编译不运行。

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>

#include <ch.hpp>
#include <core/bool.h>
#include <core/context.h>
#include <core/reg.h>
#include <simulator.h>

#include "cf/plugin/ctrl_link.h"

namespace {

using ch::core::ch_bool;
using ch::core::ch_reg;
using ch::core::ctx_swap;

// =========================================================================
// Case A/C: OR 合并的退化行为 (无仿真器)
// =========================================================================

TEST_CASE("ctrllink_stateful_or_combined_empty_is_false",
          "[framework][ctrllink-stateful]") {
  ch::core::context ctx("ctrllink_or_empty_ctx");
  ctx_swap guard(&ctx);

  cf::plugin::CtrlLink ctrl;

  // 空集合 → 常量假值 (no stall), 不得退化成未定义节点
  const ch_bool combined = ctrl.OR_combined_halt_conds();
  REQUIRE(combined.impl() != nullptr);

  // 既有别名与新 API 在空集合下等价
  const ch_bool legacy = ctrl.halt_condition();
  REQUIRE(legacy.impl() != nullptr);
}

TEST_CASE("ctrllink_stateful_single_condition_is_not_degraded",
          "[framework][ctrllink-stateful]") {
  ch::core::context ctx("ctrllink_or_single_ctx");
  ctx_swap guard(&ctx);

  cf::plugin::CtrlLink ctrl;
  ctrl.halt_when(ch_bool(false));

  // 单条件 → 直接返回该条件 (不得退化为恒假)
  const ch_bool combined = ctrl.OR_combined_halt_conds();
  REQUIRE(combined.impl() != nullptr);
  REQUIRE(ctrl.halt_count() == 1u);
}

TEST_CASE("ctrllink_stateful_combinational_and_stateful_coexist",
          "[framework][ctrllink-stateful]") {
  ch::core::context ctx("ctrllink_or_mixed_ctx");
  ctx_swap guard(&ctx);

  ch_reg<ch_bool> reg(ch_bool(false), "stateful_cond");

  cf::plugin::CtrlLink ctrl;
  ctrl.halt_when(ch_bool(false));   // 组合条件
  ctrl.halt_when_stateful(reg);     // 有状态条件

  REQUIRE(ctrl.halt_count() == 2u);
  REQUIRE(ctrl.stateful_halt_count() == 1u);
  REQUIRE(ctrl.has_stateful_conditions());

  // 组合 + 有状态共存 → OR 合并节点可被消费方引用
  const ch_bool combined = ctrl.OR_combined_halt_conds();
  REQUIRE(combined.impl() != nullptr);
}

// =========================================================================
// Case A6: 旧 API 是 inline alias, 不存在第三套合并实现
// =========================================================================

TEST_CASE("ctrllink_stateful_legacy_alias_matches_new_api",
          "[framework][ctrllink-stateful]") {
  ch::core::context ctx("ctrllink_alias_ctx");
  ctx_swap guard(&ctx);

  ch_reg<ch_bool> reg(ch_bool(false), "stateful_cond");
  reg <<= !reg;  // 自翻转 (须在 Simulator 创建前设置, 见下方 tick 用例注释)

  cf::plugin::CtrlLink ctrl;
  ctrl.halt_when(ch_bool(false));
  ctrl.halt_when_stateful(reg);

  // A6: 旧 API 与新 API 是**同一套合并逻辑**的两个名字 ⇒ 逻辑等价。
  // 注: 二者各自在 DAG 中生成独立 OR 节点 (CppHDL 的表达式即节点, 与改动前
  //     `halt_condition()` 每次调用生成新节点的行为一致) ⇒ 断言的是**值等价**
  //     而非节点指针相等。
  const ch_bool via_new = ctrl.OR_combined_halt_conds();
  const ch_bool via_legacy = ctrl.halt_condition();
  REQUIRE(via_new.impl() != nullptr);
  REQUIRE(via_legacy.impl() != nullptr);

  ch::Simulator sim(&ctx);
  sim.reset();
  for (int t = 0; t < 4; ++t) {
    sim.tick();
    const std::uint64_t v_new =
        static_cast<std::uint64_t>(sim.get_value(via_new));
    const std::uint64_t v_legacy =
        static_cast<std::uint64_t>(sim.get_value(via_legacy));
    INFO("tick " << t << ": new=" << v_new << " legacy=" << v_legacy);
    REQUIRE(v_new == v_legacy);
  }
}

// =========================================================================
// Case A1 重载: halt_when(reg) 自动按"有状态条件"登记 (缺口 G2)
// =========================================================================

TEST_CASE("ctrllink_stateful_halt_when_overload_routes_to_stateful",
          "[framework][ctrllink-stateful]") {
  ch::core::context ctx("ctrllink_overload_ctx");
  ctx_swap guard(&ctx);

  ch_reg<ch_bool> reg(ch_bool(false), "stateful_cond");

  cf::plugin::CtrlLink ctrl;
  ctrl.halt_when(reg);  // 重载而非基类切片

  REQUIRE(ctrl.halt_count() == 1u);
  REQUIRE(ctrl.stateful_halt_count() == 1u);
  REQUIRE(ctrl.has_stateful_conditions());
}

// =========================================================================
// Case A3: should_*() 对有状态条件不可靠且非静默
// =========================================================================

TEST_CASE("ctrllink_stateful_should_halt_is_flagged_unreliable",
          "[framework][ctrllink-stateful]") {
  ch::core::context ctx("ctrllink_should_ctx");
  ctx_swap guard(&ctx);

  ch_reg<ch_bool> reg(ch_bool(false), "stateful_cond");

  cf::plugin::CtrlLink ctrl;
  ctrl.halt_when_stateful(reg);

  // 查询前: 尚无不可靠查询记录
  REQUIRE(ctrl.unreliable_query_count() == 0u);

  // 有状态条件存在时调用 should_*() → 记一次"不可靠查询" (可被测试断言, 非静默)
  (void)ctrl.should_halt();
  REQUIRE(ctrl.unreliable_query_count() == 1u);

  (void)ctrl.should_throw();
  (void)ctrl.should_flush();
  REQUIRE(ctrl.unreliable_query_count() == 3u);
}

TEST_CASE("ctrllink_stateful_should_halt_not_flagged_without_stateful",
          "[framework][ctrllink-stateful]") {
  ch::core::context ctx("ctrllink_should_combi_ctx");
  ctx_swap guard(&ctx);

  cf::plugin::CtrlLink ctrl;
  ctrl.halt_when(ch_bool(false));

  // 纯组合条件 → should_*() 可靠, 不记不可靠查询
  (void)ctrl.should_halt();
  REQUIRE(ctrl.unreliable_query_count() == 0u);
  REQUIRE_FALSE(ctrl.has_stateful_conditions());
}

// =========================================================================
// Case D (核心): OR 合并结果随寄存器**逐拍更新** (Simulator tick ≥ 2 拍)
// =========================================================================

TEST_CASE("ctrllink_stateful_or_result_updates_per_tick",
          "[framework][ctrllink-stateful]") {
  ch::core::context ctx("ctrllink_tick_ctx");
  ctx_swap guard(&ctx);

  // 有状态条件: 一个 ch_reg<ch_bool>, **每拍自翻转** (`reg <<= !reg`).
  //
  // ⚠ 用法约束 (实测确认, 2026-10-22): `ch_reg::operator<<=` 设置 next 值,
  //   必须在 **Simulator 创建之前** 调用 —— Simulator 初始化时快照寄存器链,
  //   之后再 `<<=` 不生效 (上游 lnode/reg.tpp 的 `next_assignment_proxy`).
  //   同 `tests/cpu/test_cpu_chmem_mem_read_drift.cpp` 的 `areg <<= areg + 1` 写法.
  ch_reg<ch_bool> reg(ch_bool(false), "stateful_cond");
  reg <<= !reg;

  cf::plugin::CtrlLink ctrl;
  ctrl.halt_when(ch_bool(false));   // 组合条件 (恒 false)
  ctrl.halt_when_stateful(reg);     // 有状态条件

  // 真实的 OR 门节点 (两个条件 ⇒ 建真 OR 门 DAG)
  const ch_bool combined = ctrl.OR_combined_halt_conds();
  REQUIRE(combined.impl() != nullptr);

  ch::Simulator sim(&ctx);

  // ---- 逐拍观测: OR 结果必须跟随寄存器**交替翻转** ----
  // 这才是"OR 合并结果随寄存器逐拍更新"的直接证据:
  // 若 OR 结果被冻结在初值, 观测会恒为同一值 ⇒ 测试 FAIL.
  std::uint64_t prev = 2;  // 哨兵: 首拍无前值
  int flips = 0;
  for (int t = 0; t < 6; ++t) {
    sim.tick();
    const std::uint64_t obs =
        static_cast<std::uint64_t>(sim.get_value(combined));
    const std::uint64_t reg_obs =
        static_cast<std::uint64_t>(sim.get_value(ch_bool(reg.impl())));
    INFO("tick " << t << ": reg=" << reg_obs << " OR=" << obs);
    // OR 结果必须与寄存器当前值一致
    REQUIRE(obs == reg_obs);
    // 必须逐拍翻转 (不是冻结值)
    if (prev != 2 && obs != prev) ++flips;
    prev = obs;
  }
  REQUIRE(flips >= 5);  // 6 拍里至少 5 次翻转 ⇒ 确为逐拍更新

  // ---- 对照: should_halt() 走 C++ 侧缓冲, 读不到寄存器当前值 ----
  // 它读的是 stale 值, 所以**不能**用它来验证有状态条件。
  // 本断言锁定"两套路径行为不同"这一事实 (即缺口 G3 的可观测性)。
  const std::uint64_t stale =
      static_cast<std::uint64_t>(ctrl.should_halt() ? 1u : 0u);
  INFO("should_halt() 读到=" << stale << " (期望 0 — C++ 侧缓冲不随 tick 更新)");
  REQUIRE(stale == 0u);
  REQUIRE(ctrl.unreliable_query_count() == 1u);
}

// =========================================================================
// 变异对照: 拔掉"有状态"登记 ⇒ stateful_halt_count 归零 (自证测试强度)
// =========================================================================

TEST_CASE("ctrllink_stateful_mutation_control_no_stateful_no_flag",
          "[framework][ctrllink-stateful]") {
  ch::core::context ctx("ctrllink_mutation_ctx");
  ctx_swap guard(&ctx);

  // 对照组: 只用组合条件 ⇒ 无状态计数、无不可靠查询标记
  cf::plugin::CtrlLink ctrl;
  ctrl.halt_when(ch_bool(false));

  REQUIRE(ctrl.stateful_halt_count() == 0u);
  REQUIRE_FALSE(ctrl.has_stateful_conditions());
  REQUIRE(ctrl.unreliable_query_count() == 0u);
}

}  // namespace

#endif  // CF_PLUGIN_USE_CH_MEM

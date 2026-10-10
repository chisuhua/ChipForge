// tests/framework/test_multi_cycle_fsm.cpp
//
// FsmBase 双模式模板验证 (mfc-extract-fsm-h Step 1 RED → Step 2 GREEN)
//
// 目标: 新增框架级 FsmBase<StateEnum, NStates> 模板 (include/cf/plugin/multi_cycle_fsm.h):
//   - TLM 模式: ad-hoc enum class + tick() (ADR-048 兼容, FsmBase 继承 PluginBase
//     后 tick() 隐藏 private-deleted 基类 tick)
//   - CH_MEM 模式: chlib::ch_state_machine DSL (create_chmem_fsm / state_out)
//   - negotiate(CapabilityTable&) 提供 "multi_cycle_fsm" capability (ADR-082)
//
// RED 状态 (Step 1): multi_cycle_fsm.h 尚不存在 → 本文件编译失败 = RED 成立.
// Family tag: [framework][multi-cycle-fsm] (TLM) / [framework][multi-cycle-fsm][chmem] (CH_MEM)

#include "catch_amalgamated.hpp"
#include <cstdint>
#include <type_traits>

#include "cf/plugin/capability_table.h"
#include "cf/plugin/multi_cycle_fsm.h"  // ← Step 1 RED: 尚不存在

enum class TestState : std::uint8_t { IDLE = 0, RUNNING = 1, DONE = 2 };

struct TestFsm : cf::plugin::multi_cycle_fsm::FsmBase<TestState, 3> {
  // TLM 模式: 派生类实现状态转移 (IDLE→RUNNING→DONE)
  void on_tick(TestState /*current*/) override {
    if (state() == TestState::IDLE) { state_ = TestState::RUNNING; }
    else if (state() == TestState::RUNNING) { state_ = TestState::DONE; }
  }
#ifdef CF_PLUGIN_USE_CH_MEM
  void on_active_state(TestState /*current*/) override {}
  TestState next_state(TestState current) override { return current; }
#endif
};

TEST_CASE("fsm_base_tlm_tick_advances_state", "[framework][multi-cycle-fsm]") {
  TestFsm f;
  REQUIRE(f.state() == TestState::IDLE);
  f.tick();  // IDLE → RUNNING
  REQUIRE(f.state() == TestState::RUNNING);
  f.tick();  // RUNNING → DONE
  REQUIRE(f.state() == TestState::DONE);
}

// AC-5: ADR-082 negotiate 仍工作 — FsmBase 提供 "multi_cycle_fsm" capability
TEST_CASE("fsm_base_negotiate_provides_capability",
          "[framework][multi-cycle-fsm][negotiate]") {
  cf::plugin::CapabilityTable cap;
  TestFsm f;
  f.negotiate(cap);
  REQUIRE(cap.has("multi_cycle_fsm"));
}

#ifdef CF_PLUGIN_USE_CH_MEM
TEST_CASE("fsm_base_chmem_dsl_creates_and_builds",
          "[framework][multi-cycle-fsm][chmem]") {
  ch::core::context ctx("fsm_base_ctx");
  TestFsm f;
  f.set_ch_context(&ctx);
  { ch::core::ctx_swap guard(&ctx); f.create_chmem_fsm(); }
  REQUIRE(f.ch_fsm().is_built());  // DSL select-tree 已发射
}
#endif

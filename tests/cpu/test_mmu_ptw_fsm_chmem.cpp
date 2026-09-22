// tests/cpu/test_mmu_ptw_fsm_chmem.cpp
//
// Phase 6d.6: MMU PTW sv32 5-state FSM PoC 测试
//
// 验证 (ADR-046 ch_state_machine DSL 实装):
//   1. mmu_ptw_fsm_sv32_walk_success: 2 级 walk → DONE, PPN 匹配
//   2. mmu_ptw_fsm_sv32_reserved_encoding_fault: {R=1,W=0,X=0} → FAULT (code 2)
//   3. mmu_ptw_fsm_sv32_invalid_pte_fault: {V=0} → FAULT (code 1)
//
// 驱动模式 (与 cpu_memory_model_chmem 同思路):
//   - PipeBuilder + PtWalkFsmPlugin elaborate → create_simulator
//   - set_input_value(plugin.start()/pte_*) 逐周期喂输入
//   - tick() 推进 FSM; 断言当前拍状态/输出 (CppHDL Simulator 在 tick 内
//     先更新寄存器再求值下游逻辑 → 状态转移与输出同拍可读)
//
// sv32 walk 期望时序 (walk success):
//   drive(start=1)                          → state=L0_WAIT
//   drive(pte_valid=1, 非叶 {V=1,R=0,W=0,X=0}) → state=L1_WAIT
//   drive(pte_valid=1, 叶 {V=1,R=1,W=1,X=0, PPN=0x12345}) → state=DONE, ppn
//   drive()                                 → state=IDLE
//
// 编译条件: CF_PLUGIN_USE_CH_MEM (chipforge_tests_chmem target)

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>

#include <ch.hpp>
#include <core/context.h>
#include <simulator.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/result_macros.h"
#include "cf/plugin/uint_t.h"

#include "ip/cpu/plugins/mmu_ptw_chmem.h"

using namespace ch;
using namespace ch::core;
namespace cfc = cf::plugin;
namespace cfcpu = cf::cpu;

namespace {

// =========================================================================
// 通用: 构建 PtWalkFsmPlugin + elaborate + simulator
// =========================================================================
struct PtwFixture {
  ch::core::context ctx;
  std::unique_ptr<cfc::PipeBuilder> pb;
  std::unique_ptr<ch::Simulator> sim;
  cfcpu::plugins::PtWalkFsmPlugin<ch_uint<32>>* plugin = nullptr;

  PtwFixture()
      : ctx("mmu_ptw_fsm_ctx"), pb(std::make_unique<cfc::PipeBuilder>(&ctx)) {
    ch::core::ctx_swap guard(&ctx);
    auto p = std::make_unique<cfcpu::plugins::PtWalkFsmPlugin<ch_uint<32>>>();
    plugin = p.get();
    pb->register_plugin(std::move(p));
    pb->build();
    pb->elaborate();
    sim = cf::plugin::auto_throw(pb->create_simulator());
    sim->reset();
  }

  // 单周期输入驱动 + tick
  void drive(uint64_t start, uint64_t pte_valid, uint64_t pte_v,
             uint64_t pte_r, uint64_t pte_w, uint64_t pte_x,
             uint64_t pte_ppn) {
    sim->set_input_value(plugin->start(), start);
    sim->set_input_value(plugin->pte_valid(), pte_valid);
    sim->set_input_value(plugin->pte_v(), pte_v);
    sim->set_input_value(plugin->pte_r(), pte_r);
    sim->set_input_value(plugin->pte_w(), pte_w);
    sim->set_input_value(plugin->pte_x(), pte_x);
    sim->set_input_value(plugin->pte_ppn(), pte_ppn);
    sim->tick();
  }

  uint64_t state() {
    return static_cast<uint64_t>(sim->get_value(plugin->state_out()));
  }
  uint64_t done() {
    return static_cast<uint64_t>(sim->get_value(plugin->done()));
  }
  uint64_t fault() {
    return static_cast<uint64_t>(sim->get_value(plugin->fault()));
  }
  uint64_t fault_code() {
    return static_cast<uint64_t>(sim->get_value(plugin->fault_code()));
  }
  uint64_t result_ppn() {
    return static_cast<uint64_t>(sim->get_value(plugin->result_ppn()));
  }
};

}  // anonymous namespace

// =========================================================================
// Test 1: sv32 walk success — 2 级 walk → DONE, PPN 匹配
//
// level-1 PTE (root):  {V=1, R=0, W=0, X=0}   → 非叶指针 → L1_WAIT
// level-0 PTE (leaf):  {V=1, R=1, W=1, X=0}   → 叶 → DONE
// leaf PPN = 0x12345 → result_ppn = 0x12345
// =========================================================================
TEST_CASE("mmu_ptw_fsm_sv32_walk_success", "[cpu][chmem][6d6][mmu][fsm]") {
  PtwFixture f;

  // 拍 0: IDLE + start=1 → 下拍进 L0_WAIT
  f.drive(/*start=*/1, /*pte_valid=*/0, 0, 0, 0, 0, 0);
  REQUIRE(f.state() == 1);  // L0_WAIT

  // 拍 1: 喂 level-1 非叶 PTE (V=1, R=W=X=0) → 下拍进 L1_WAIT
  f.drive(/*start=*/0, /*pte_valid=*/1, /*v=*/1, /*r=*/0, /*w=*/0, /*x=*/0,
          /*ppn=*/0x1000);
  REQUIRE(f.state() == 2);  // L1_WAIT

  // 拍 2: 喂 level-0 叶 PTE (V=1, R=1, W=0, X=0, PPN=0x12345)
  //       → DONE, result_ppn = 0x12345, done=1
  f.drive(/*start=*/0, /*pte_valid=*/1, /*v=*/1, /*r=*/1, /*w=*/0, /*x=*/0,
          /*ppn=*/0x12345);
  REQUIRE(f.state() == 3);  // DONE
  REQUIRE(f.done() == 1);
  REQUIRE(f.fault() == 0);
  REQUIRE(f.result_ppn() == 0x12345);

  // 拍 3: DONE 无条件下拍回 IDLE
  f.drive(0, 0, 0, 0, 0, 0, 0);
  REQUIRE(f.state() == 0);  // IDLE
  REQUIRE(f.done() == 0);

  SUCCEED("sv32 walk: IDLE→L0_WAIT→L1_WAIT→DONE→IDLE, ppn=0x12345");
}

// =========================================================================
// Test 2: reserved encoding fault — level-1 PTE {V=1, R=1, W=1, X=0}
//
// 任务规范: PTE.R=1 && PTE.W=1 是保留编码 (叶 PTE 不允许 R+W 同置)
// → 页错误 (fault_code=2)
// =========================================================================
TEST_CASE("mmu_ptw_fsm_sv32_reserved_encoding_fault", "[cpu][chmem][6d6][mmu][fsm]") {
  PtwFixture f;

  // 拍 0: IDLE + start=1 → L0_WAIT
  f.drive(1, 0, 0, 0, 0, 0, 0);
  REQUIRE(f.state() == 1);

  // 拍 1: 喂保留编码 PTE {V=1, R=1, W=1, X=0} → FAULT (code 2)
  f.drive(0, /*pte_valid=*/1, /*v=*/1, /*r=*/1, /*w=*/1, /*x=*/0, /*ppn=*/0);
  REQUIRE(f.state() == 4);  // FAULT
  REQUIRE(f.fault() == 1);
  REQUIRE(f.done() == 0);
  REQUIRE(f.fault_code() == 2);  // reserved

  // 拍 2: FAULT 无条件下拍回 IDLE
  f.drive(0, 0, 0, 0, 0, 0, 0);
  REQUIRE(f.state() == 0);
  REQUIRE(f.fault() == 0);

  SUCCEED("sv32 reserved encoding: L0_WAIT→FAULT (fault_code=2)");
}

// =========================================================================
// Test 3: invalid PTE fault — level-1 PTE {V=0}
//
// spec §5.4: 任何一级 PTE.V=0 → 页错误 (fault_code=1)
// =========================================================================
TEST_CASE("mmu_ptw_fsm_sv32_invalid_pte_fault", "[cpu][chmem][6d6][mmu][fsm]") {
  PtwFixture f;

  // 拍 0: IDLE + start=1 → L0_WAIT
  f.drive(1, 0, 0, 0, 0, 0, 0);
  REQUIRE(f.state() == 1);

  // 拍 1: 喂无效 PTE {V=0} → FAULT (code 1)
  f.drive(0, /*pte_valid=*/1, /*v=*/0, /*r=*/0, /*w=*/0, /*x=*/0, /*ppn=*/0);
  REQUIRE(f.state() == 4);  // FAULT
  REQUIRE(f.fault() == 1);
  REQUIRE(f.fault_code() == 1);  // invalid

  // 拍 2: FAULT 无条件下拍回 IDLE
  f.drive(0, 0, 0, 0, 0, 0, 0);
  REQUIRE(f.state() == 0);
  REQUIRE(f.fault() == 0);

  SUCCEED("sv32 invalid PTE: L0_WAIT→FAULT (fault_code=1)");
}

#endif  // CF_PLUGIN_USE_CH_MEM
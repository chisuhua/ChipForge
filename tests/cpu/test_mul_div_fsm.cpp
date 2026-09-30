// tests/cpu/test_mul_div_fsm.cpp
//
// 功能描述: MulDivFsmPlugin 单元测试 (mfc-cpu-pipeline-multi-cycle-fsm Phase A)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-28
//
// 测试覆盖 (A.1 + A.3 + A.4 共 6 用例):
//   1. instantiate_default_state          — 构造 + 初始 state_ = IDLE
//   2. setup_build_no_throw               — setup() + build() 不抛异常
//   3. plugin_registers_to_pipe_builder   — PipeBuilder 注册计数 +1
//   4. fsm_mul_state_transition           — MUL 状态机 IDLE → MULTIPLY → WRITE_BACK
//   5. fsm_div_33_cycle                   — DIV 33 cycle 完成 + RESULT Payload Key
//   6. mul_one_cycle_busy                 — MUL 指令 busy_cycles == 1
//
// 设计:
//   - Phase A 最小 PoC 验证 MulDivFsmPlugin FSM 状态机正确性
//   - 不验证 cycle parity (CH_MEM 未实现, 推迟到 Phase D)
//   - 不验证 ch_state_machine DSL (Phase B 才强制)
//   - 与既有 [cpu] 测试共存 (test_mul.cpp 56 行 + test_mul_latency.cpp 90 行 不退化)
//
// 约束:
//   - MulDivFsmPlugin 位于 ip/cpu/arch/riscv/mul_div_fsm.h (与 RiscvMulPlugin 并存)
//   - ADR-046 v2.0 §2.1.1 算术多周期 FSM 豁免首例
//   - Phase A 内部 ad-hoc cycle counter (busy_cycles_), 不依赖 framework cycle-precision

#include "catch_amalgamated.hpp"
#include <cstdint>
#include <memory>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/arch/riscv/mul_div_fsm.h"

using cf::plugin::PipeBuilder;
using cf::plugin::Phase;
using cf::cpu::arch::riscv::MulDivFsmPlugin;
using T = std::uint32_t;

// 1. 实例化默认状态: 构造后 state_ == IDLE, busy_cycles_ == 0
TEST_CASE("instantiate_default_state", "[cpu][mul-div-fsm]") {
  MulDivFsmPlugin<T> mul;
  REQUIRE(mul.state() == MulDivFsmPlugin<T>::State::IDLE);
  REQUIRE(mul.busy_cycles() == 0);
}

// 2. setup + build 不抛异常 (基本生命周期)
//    验证: 构造 + setup() + build() 三步完成, Plugin 注册到 PipeBuilder
TEST_CASE("setup_build_no_throw", "[cpu][mul-div-fsm]") {
  PipeBuilder pb;
  pb.at_stage("execute", Phase::NORMAL, []() {});

  auto mul = std::make_unique<MulDivFsmPlugin<T>>();
  auto* raw = mul.get();
  REQUIRE_NOTHROW(pb.register_plugin(std::move(mul)));
  REQUIRE_NOTHROW(raw->setup(pb));
  REQUIRE_NOTHROW(raw->build(pb));
  REQUIRE(pb.has_stage("execute"));
}

// 3. Plugin 注册到 PipeBuilder 后 plugin_count() +1
TEST_CASE("plugin_registers_to_pipe_builder", "[cpu][mul-div-fsm]") {
  PipeBuilder pb;
  pb.at_stage("execute", Phase::NORMAL, []() {});

  std::size_t before = pb.plugin_count();
  auto mul = std::make_unique<MulDivFsmPlugin<T>>();
  REQUIRE_NOTHROW(pb.register_plugin(std::move(mul)));
  REQUIRE(pb.plugin_count() == before + 1);
}

// 4. FSM 状态机 MUL: IDLE → MULTIPLY → WRITE_BACK (手动驱动, 不依赖 pb.run())
//    验证:
//      - 设指令 → state_ 立即进入 MULTIPLY
//      - tick_state() 推进 → state_ 进入 WRITE_BACK
//      - tick_state() 再推进 → state_ 回到 IDLE (或保持 WRITE_BACK 直到 reset)
TEST_CASE("fsm_mul_state_transition", "[cpu][mul-div-fsm]") {
  PipeBuilder pb;
  pb.at_stage("execute", Phase::NORMAL, []() {});
  MulDivFsmPlugin<T> mul;

  // 初始 IDLE
  REQUIRE(mul.state() == MulDivFsmPlugin<T>::State::IDLE);

  // 设置 MUL 指令 + 操作数
  mul.set_opcode(MulDivFsmPlugin<T>::Opcode::MUL);
  mul.set_operands(3, 4);  // 期望 12
  REQUIRE(mul.state() == MulDivFsmPlugin<T>::State::MULTIPLY);

  // 推进: MUL 单周期完成 → WRITE_BACK
  mul.tick_state();
  REQUIRE(mul.state() == MulDivFsmPlugin<T>::State::WRITE_BACK);

  // 再推进: 回到 IDLE
  mul.tick_state();
  REQUIRE(mul.state() == MulDivFsmPlugin<T>::State::IDLE);

  // 结果验证: RESULT == 3 * 4 == 12
  REQUIRE(mul.result() == 12);
}

// 5. DIV 33 cycle 完成 (核心 PoC 硬指标)
//    验证:
//      - 设 DIV 指令 → state_ 进入 DIVIDE
//      - tick_state() × 33 → state_ 进入 WRITE_BACK
//      - tick_state() × 34 → state_ 回到 IDLE
//      - RESULT == 14/4 == 3 (整数除)
//      - busy_cycles == 33
TEST_CASE("fsm_div_33_cycle", "[cpu][mul-div-fsm]") {
  PipeBuilder pb;
  pb.at_stage("execute", Phase::NORMAL, []() {});
  MulDivFsmPlugin<T> mul;

  // 设置 DIV 指令
  mul.set_opcode(MulDivFsmPlugin<T>::Opcode::DIV);
  mul.set_operands(14, 4);  // 期望 3
  REQUIRE(mul.state() == MulDivFsmPlugin<T>::State::DIVIDE);
  REQUIRE(mul.busy_cycles() == 0);

  // 推进 32 cycle (radix-2 iterative 主迭代)
  for (std::size_t i = 0; i < 32; ++i) {
    mul.tick_state();
    REQUIRE(mul.state() == MulDivFsmPlugin<T>::State::DIVIDE);
  }
  // 第 33 cycle: WRITE_BACK
  mul.tick_state();
  REQUIRE(mul.state() == MulDivFsmPlugin<T>::State::WRITE_BACK);

  // 第 34 cycle: 回 IDLE
  mul.tick_state();
  REQUIRE(mul.state() == MulDivFsmPlugin<T>::State::IDLE);

  // 结果 + cycle 计数验证
  REQUIRE(mul.result() == 3);          // 14 / 4 = 3
  REQUIRE(mul.busy_cycles() == 33);    // 32 iterative + 1 write-back
}

// Phase C (ADR-082) stub provider: 满足 MulDivFsmPlugin 的 capability requires
//   - provide "flush_broadcaster" + "writeback_arbiter"
//   - 空 build() (无业务)
namespace {
struct StubCapabilityProvider : cf::plugin::PluginBase {
  void negotiate(cf::plugin::CapabilityTable& cap) override {
    cap.provide("flush_broadcaster", this);
    cap.provide("writeback_arbiter", this);
  }
  void build(cf::plugin::PipeBuilder&) override {}
};
}  // namespace

// 6. MUL 指令 busy_cycles == 1 (A.3 验收: MUL=1 cycle)
//    通过 pb.run() 触发 execute 闭包 + 内部 counter 自增
//    验证: pb.run() × 1 后 busy_cycles == 1
TEST_CASE("mul_one_cycle_busy", "[cpu][mul-div-fsm]") {
  PipeBuilder pb;
  pb.at_stage("execute", Phase::NORMAL, []() {});
  // Phase C: stub provider 满足 MulDivFsmPlugin 的 capability requires
  REQUIRE_NOTHROW(pb.register_plugin(std::make_unique<StubCapabilityProvider>()));
  auto mul = std::make_unique<MulDivFsmPlugin<T>>();
  auto* raw = mul.get();
  raw->setup(pb);
  raw->build(pb);
  REQUIRE_NOTHROW(pb.register_plugin(std::move(mul)));
  REQUIRE(pb.build());

  // 设置 MUL 指令 + RS1/RS2 Payload
  auto* exec_node = pb.node_of_logic_stage("execute").get();
  REQUIRE(exec_node != nullptr);
  using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;
  (*exec_node)(KeyType::RS1) = static_cast<T>(3);
  (*exec_node)(KeyType::RS2) = static_cast<T>(4);
  raw->set_opcode(MulDivFsmPlugin<T>::Opcode::MUL);

  // 单 cycle: MUL 完成
  pb.run();
  // busy_cycles 在 MUL 完成时 = 1
  REQUIRE(raw->busy_cycles() == 1);
}
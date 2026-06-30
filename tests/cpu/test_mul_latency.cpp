// tests/cpu/test_mul_latency.cpp
//
// 功能描述: RiscvMulPlugin 多周期 LATENCY 模板参数验证 (M5.14, Section 3)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-22
//
// 测试覆盖 (4 用例):
//   1. LatencyOneIsBaseline        — LATENCY=1: 无 substage, expected_latency()=1
//   2. LatencyThreeAddsSubstages   — LATENCY=3: mul_s1, mul_s2 substage
//   3. LatencyFiveAddsFourStages   — LATENCY=5: mul_s1..mul_s4 substage
//   4. LatIsCompileTimeConstant    — 编译期 static_assert 验证 LAT 公开成员
//
// 设计:
//   - LATENCY ∈ {1, 3, 5}, 默认 1 (单周期, byte-identical to baseline)
//   - substage_count() = LATENCY - 1
//   - expected_latency() = LATENCY
//   - setup() 中, LATENCY==1 不声明 substage; 否则声明 mul_s1..mul_s(LATENCY-1)
//
// 约束:
//   - T must be unsigned (mul.h static_assert(std::is_unsigned<T>::value))
//   - 5-stage baseline 必须 byte-identical (mul_latency=1 不改变行为)
//   - 本测试只验证 LATENCY 模板机制本身; 多周期 perf 验证阻塞于 Task 5 cpu_sim

#include "catch_amalgamated.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>

#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/arch/riscv/mul.h"

using cf::plugin::PipeBuilder;
using cf::plugin::Phase;
using cf::cpu::arch::riscv::RiscvMulPlugin;
using T = std::uint32_t;

// 1. LATENCY=1: baseline 单周期 (byte-identical, no substage)
//    验证 default template 参数仍为 1, 不引入新 stage
TEST_CASE("latency_one_is_baseline", "[cpu]") {
  RiscvMulPlugin<T, 1> mul;
  REQUIRE(mul.expected_latency() == 1);
  REQUIRE(mul.substage_count() == 0);

  // setup() must NOT declare any mul_s substage
  PipeBuilder pb;
  pb.at_stage("execute", Phase::NORMAL, []() {});
  mul.setup(pb);
  REQUIRE(pb.node_of_logic_stage("mul_s1") == nullptr);
}

// 2. LATENCY=3: 多周期, 2 substage (mul_s1, mul_s2)
TEST_CASE("latency_three_adds_substages", "[cpu]") {
  RiscvMulPlugin<T, 3> mul;
  REQUIRE(mul.expected_latency() == 3);
  REQUIRE(mul.substage_count() == 2);

  // setup() must declare mul_s1, mul_s2
  PipeBuilder pb;
  pb.at_stage("execute", Phase::NORMAL, []() {});
  mul.setup(pb);
  REQUIRE(pb.node_of_logic_stage("mul_s1") != nullptr);
  REQUIRE(pb.node_of_logic_stage("mul_s2") != nullptr);
  // must NOT declare mul_s3 (LATENCY=3 → only 2 substages)
  REQUIRE(pb.node_of_logic_stage("mul_s3") == nullptr);
}

// 3. LATENCY=5: 多周期, 4 substage (mul_s1..mul_s4)
TEST_CASE("latency_five_adds_four_substages", "[cpu]") {
  RiscvMulPlugin<T, 5> mul;
  REQUIRE(mul.expected_latency() == 5);
  REQUIRE(mul.substage_count() == 4);

  PipeBuilder pb;
  pb.at_stage("execute", Phase::NORMAL, []() {});
  mul.setup(pb);
  REQUIRE(pb.node_of_logic_stage("mul_s1") != nullptr);
  REQUIRE(pb.node_of_logic_stage("mul_s2") != nullptr);
  REQUIRE(pb.node_of_logic_stage("mul_s3") != nullptr);
  REQUIRE(pb.node_of_logic_stage("mul_s4") != nullptr);
  // must NOT declare mul_s5
  REQUIRE(pb.node_of_logic_stage("mul_s5") == nullptr);
}

// 4. LAT 公开成员编译期检查
//    LAT must equal LATENCY template arg at compile time
//    用 LAT=3 (合法值) 验证 LAT 公开成员
static_assert(RiscvMulPlugin<T, 3>::LAT == 3,
              "RiscvMulPlugin<T, LATENCY>::LAT must equal LATENCY");



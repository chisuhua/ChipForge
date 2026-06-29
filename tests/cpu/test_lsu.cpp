// tests/cpu/test_lsu.cpp
//
// 功能描述: RiscvLsuPlugin 单元测试 (M3.10 验证)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (4 用例):
//   1. RV32 实例化
//   2. RV64 实例化
//   3. build() 可调用
//   4. 销毁安全
//
// 约束:

#include "catch_amalgamated.hpp"
#include <cstdint>

#include "ip/cpu/arch/riscv/lsu.h"

using namespace cf::cpu::arch::riscv;
using T32 = std::uint32_t;
using T64 = std::uint64_t;

TEST_CASE("rv32_instantiate", "[cpu]") {
  RiscvLsuPlugin<T32> lsu;
  (void)lsu;
}

TEST_CASE("rv64_instantiate", "[cpu]") {
  RiscvLsuPlugin<T64> lsu;
  (void)lsu;
}

TEST_CASE("build_compiles", "[cpu]") {
  RiscvLsuPlugin<T32> lsu;
  cf::plugin::PipeBuilder pb;
  lsu.build(pb);
}

TEST_CASE("destruction_safe", "[cpu]") {
  {
    RiscvLsuPlugin<T32> lsu;
    (void)lsu;
  }
}



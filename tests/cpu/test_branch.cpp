// tests/cpu/test_branch.cpp
//
// 功能描述: RiscvBranchPlugin 单元测试 (M3.10 验证)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (4 用例):
//   1. BEQ/BNE 条件评估
//   2. BLT/BGE 有符号比较
//   3. BLTU/BGEU 无符号比较
//
// 约束:

#include "catch_amalgamated.hpp"
#include <cstdint>

#include "ip/cpu/arch/riscv/branch.h"
#include "ip/cpu/arch/riscv/decoder_table.h"

using namespace cf::cpu::arch::riscv;
using T = std::uint32_t;

TEST_CASE("beq_bne", "[cpu]") {
  REQUIRE(RiscvBranchPlugin<T>::evaluate_branch(0b000, 5, 5) == true);   // BEQ
  REQUIRE(RiscvBranchPlugin<T>::evaluate_branch(0b000, 5, 6) == false);
  REQUIRE(RiscvBranchPlugin<T>::evaluate_branch(0b001, 5, 5) == false);  // BNE
  REQUIRE(RiscvBranchPlugin<T>::evaluate_branch(0b001, 5, 6) == true);
}

TEST_CASE("blt_bge", "[cpu]") {
  // 有符号: 3 < 5, -1 < 0
  REQUIRE(RiscvBranchPlugin<T>::evaluate_branch(0b100, 3, 5) == true);   // BLT
  REQUIRE(RiscvBranchPlugin<T>::evaluate_branch(0b100, 0xFFFFFFFF, 0) == true);  // -1 < 0
  REQUIRE(RiscvBranchPlugin<T>::evaluate_branch(0b101, 3, 5) == false);  // BGE
  REQUIRE(RiscvBranchPlugin<T>::evaluate_branch(0b101, 0xFFFFFFFF, 0) == false); // -1 >= 0 false
}

TEST_CASE("bltu_bgeu", "[cpu]") {
  // 无符号
  REQUIRE(RiscvBranchPlugin<T>::evaluate_branch(0b110, 3, 5) == true);   // BLTU
  REQUIRE(RiscvBranchPlugin<T>::evaluate_branch(0b110, 0xFFFFFFFF, 5) == false);  // UINT_MAX < 5 false
  REQUIRE(RiscvBranchPlugin<T>::evaluate_branch(0b111, 3, 5) == false);  // BGEU
  REQUIRE(RiscvBranchPlugin<T>::evaluate_branch(0b111, 5, 3) == true);
}



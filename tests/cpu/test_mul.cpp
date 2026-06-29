// tests/cpu/test_mul.cpp
//
// 功能描述: RiscvMulPlugin 单元测试 (M3.10 验证)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (5 用例):
//   1. MUL 基本乘法
//   2. DIV/DIVU 除法
//   3. REM/REMU 取余
//   4. DIV/REM by zero
//   5. 边界条件 (M 扩展除法溢出)
//
// 约束:

#include "catch_amalgamated.hpp"
#include <cstdint>

#include "ip/cpu/arch/riscv/mul.h"

using namespace cf::cpu::arch::riscv;
using T = std::uint32_t;

TEST_CASE("mul", "[cpu]") {
  REQUIRE(RiscvMulPlugin<T>::compute(0b000, 0, 3, 4) == 12);
  REQUIRE(RiscvMulPlugin<T>::compute(0b000, 0, 100, 200) == 20000);
  REQUIRE(RiscvMulPlugin<T>::compute(0b000, 0, 0, 12345) == 0);
}

TEST_CASE("div", "[cpu]") {
  REQUIRE(RiscvMulPlugin<T>::compute(0b100, 0, 20, 6) == 3);   // DIV
  REQUIRE(RiscvMulPlugin<T>::compute(0b101, 0, 20, 6) == 3);   // DIVU
  REQUIRE(RiscvMulPlugin<T>::compute(0b100, 0, 7, 2) == 3);
}

TEST_CASE("rem", "[cpu]") {
  REQUIRE(RiscvMulPlugin<T>::compute(0b110, 0, 20, 6) == 2);   // REM
  REQUIRE(RiscvMulPlugin<T>::compute(0b111, 0, 20, 6) == 2);   // REMU
  REQUIRE(RiscvMulPlugin<T>::compute(0b110, 0, 7, 2) == 1);
}

TEST_CASE("div_by_zero", "[cpu]") {
  // DIV by zero: return -1
  REQUIRE(RiscvMulPlugin<T>::compute(0b100, 0, 10, 0) == 0xFFFFFFFF);
  // REM by zero: return dividend
  REQUIRE(RiscvMulPlugin<T>::compute(0b110, 0, 10, 0) == 10);
}

TEST_CASE("boundary", "[cpu]") {
  // 有符号除法边界: INT_MIN / -1 应该返回 INT_MIN
  T int_min = 0x80000000;
  T neg_one = 0xFFFFFFFF;
  REQUIRE(RiscvMulPlugin<T>::compute(0b100, 0, int_min, neg_one) == int_min);
}



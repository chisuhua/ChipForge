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
//   - 纯 main() + assert

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "ip/cpu/arch/riscv/mul.h"

using namespace cf::cpu::arch::riscv;
using T = std::uint32_t;

static void test_mul() {
  assert(RiscvMulPlugin<T>::compute(0b000, 0, 3, 4) == 12);
  assert(RiscvMulPlugin<T>::compute(0b000, 0, 100, 200) == 20000);
  assert(RiscvMulPlugin<T>::compute(0b000, 0, 0, 12345) == 0);
  printf("  [PASS] test_mul\n");
}

static void test_div() {
  assert(RiscvMulPlugin<T>::compute(0b100, 0, 20, 6) == 3);   // DIV
  assert(RiscvMulPlugin<T>::compute(0b101, 0, 20, 6) == 3);   // DIVU
  assert(RiscvMulPlugin<T>::compute(0b100, 0, 7, 2) == 3);
  printf("  [PASS] test_div\n");
}

static void test_rem() {
  assert(RiscvMulPlugin<T>::compute(0b110, 0, 20, 6) == 2);   // REM
  assert(RiscvMulPlugin<T>::compute(0b111, 0, 20, 6) == 2);   // REMU
  assert(RiscvMulPlugin<T>::compute(0b110, 0, 7, 2) == 1);
  printf("  [PASS] test_rem\n");
}

static void test_div_by_zero() {
  // DIV by zero: return -1
  assert(RiscvMulPlugin<T>::compute(0b100, 0, 10, 0) == 0xFFFFFFFF);
  // REM by zero: return dividend
  assert(RiscvMulPlugin<T>::compute(0b110, 0, 10, 0) == 10);
  printf("  [PASS] test_div_by_zero\n");
}

static void test_boundary() {
  // 有符号除法边界: INT_MIN / -1 应该返回 INT_MIN
  T int_min = 0x80000000;
  T neg_one = 0xFFFFFFFF;
  assert(RiscvMulPlugin<T>::compute(0b100, 0, int_min, neg_one) == int_min);
  printf("  [PASS] test_boundary\n");
}

int main() {
  printf("test_mul:\n");
  test_mul();
  test_div();
  test_rem();
  test_div_by_zero();
  test_boundary();
  printf("[PASS] all RiscvMulPlugin tests\n");
  return 0;
}

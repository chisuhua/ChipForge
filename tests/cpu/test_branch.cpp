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
//   - 纯 main() + assert

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "ip/cpu/arch/riscv/branch.h"
#include "ip/cpu/arch/riscv/decoder_table.h"

using namespace cf::cpu::arch::riscv;
using T = std::uint32_t;

static void test_beq_bne() {
  assert(RiscvBranchPlugin<T>::evaluate_branch(0b000, 5, 5) == true);   // BEQ
  assert(RiscvBranchPlugin<T>::evaluate_branch(0b000, 5, 6) == false);
  assert(RiscvBranchPlugin<T>::evaluate_branch(0b001, 5, 5) == false);  // BNE
  assert(RiscvBranchPlugin<T>::evaluate_branch(0b001, 5, 6) == true);
  printf("  [PASS] test_beq_bne\n");
}

static void test_blt_bge() {
  // 有符号: 3 < 5, -1 < 0
  assert(RiscvBranchPlugin<T>::evaluate_branch(0b100, 3, 5) == true);   // BLT
  assert(RiscvBranchPlugin<T>::evaluate_branch(0b100, 0xFFFFFFFF, 0) == true);  // -1 < 0
  assert(RiscvBranchPlugin<T>::evaluate_branch(0b101, 3, 5) == false);  // BGE
  assert(RiscvBranchPlugin<T>::evaluate_branch(0b101, 0xFFFFFFFF, 0) == false); // -1 >= 0 false
  printf("  [PASS] test_blt_bge\n");
}

static void test_bltu_bgeu() {
  // 无符号
  assert(RiscvBranchPlugin<T>::evaluate_branch(0b110, 3, 5) == true);   // BLTU
  assert(RiscvBranchPlugin<T>::evaluate_branch(0b110, 0xFFFFFFFF, 5) == false);  // UINT_MAX < 5 false
  assert(RiscvBranchPlugin<T>::evaluate_branch(0b111, 3, 5) == false);  // BGEU
  assert(RiscvBranchPlugin<T>::evaluate_branch(0b111, 5, 3) == true);
  printf("  [PASS] test_bltu_bgeu\n");
}

int main() {
  printf("test_branch:\n");
  test_beq_bne();
  test_blt_bge();
  test_bltu_bgeu();
  printf("[PASS] all RiscvBranchPlugin tests\n");
  return 0;
}

// tests/cpu/test_int_alu.cpp
//
// 功能描述: RiscvIntAluPlugin 单元测试 (M3.12 验证)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (6 用例):
//   1. ADD/SUB 基本运算
//   2. AND/OR/XOR 位运算
//   3. SLL/SRL/SRA 移位
//   4. SLT/SLTU 比较
//
// 约束:
//   - 纯 main() + assert

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "ip/cpu/arch/riscv/int_alu.h"
#include "ip/cpu/arch/riscv/decoder_table.h"

using namespace cf::cpu::arch::riscv;
using T = std::uint32_t;

static void test_add_sub() {
  assert(RiscvIntAluPlugin<T>::compute(OpCode::ADD, 3, 4) == 7);
  assert(RiscvIntAluPlugin<T>::compute(OpCode::SUB, 10, 3) == 7);
  assert(RiscvIntAluPlugin<T>::compute(OpCode::ADD, 0, 0) == 0);
  printf("  [PASS] test_add_sub\n");
}

static void test_logic() {
  assert(RiscvIntAluPlugin<T>::compute(OpCode::AND, 0xFF, 0x0F) == 0x0F);
  assert(RiscvIntAluPlugin<T>::compute(OpCode::OR, 0xF0, 0x0F) == 0xFF);
  assert(RiscvIntAluPlugin<T>::compute(OpCode::XOR, 0xFF, 0x0F) == 0xF0);
  printf("  [PASS] test_logic\n");
}

static void test_shift() {
  assert(RiscvIntAluPlugin<T>::compute(OpCode::SLL, 1, 4) == 16);
  assert(RiscvIntAluPlugin<T>::compute(OpCode::SRL, 0xF0, 4) == 0x0F);
  // SRA: 算术右移保留符号
  assert(RiscvIntAluPlugin<T>::compute(OpCode::SRA, 0xFFFFFFF0, 2) == 0xFFFFFFFC);
  printf("  [PASS] test_shift\n");
}

static void test_compare() {
  assert(RiscvIntAluPlugin<T>::compute(OpCode::SLT, 3, 5) == 1);
  assert(RiscvIntAluPlugin<T>::compute(OpCode::SLT, 5, 3) == 0);
  assert(RiscvIntAluPlugin<T>::compute(OpCode::SLTU, 3, 5) == 1);
  assert(RiscvIntAluPlugin<T>::compute(OpCode::SLTU, 0xFFFFFFFF, 5) == 0);
  printf("  [PASS] test_compare\n");
}

int main() {
  printf("test_int_alu:\n");
  test_add_sub();
  test_logic();
  test_shift();
  test_compare();
  printf("[PASS] all RiscvIntAluPlugin tests\n");
  return 0;
}

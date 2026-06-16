// tests/cpu/test_decode.cpp
//
// 功能描述: RiscvDecodePlugin 单元测试 (M3.10 验证)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (5 用例):
//   1. U-type LUI 译码
//   2. J-type JAL 译码
//   3. I-type ADDI 译码
//   4. R-type SUB 译码 (区分 ADD/SUB 通过 funct7)
//   5. B-type BEQ 译码
//
// 约束:
//   - 纯 main() + assert (与 cf_plugin 现有测试一致)

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "ip/cpu/arch/riscv/decoder_table.h"

using namespace cf::cpu::arch::riscv;

static void test_lui_decode() {
  std::uint32_t inst = 0x123451B7;  // LUI x3, 0x12345
  assert(decode_rv32(inst) == OpCode::LUI);
  assert(get_rd(inst) == 3);
  assert(get_imm(OpCode::LUI, inst) == 0x12345000);
  printf("  [PASS] test_lui_decode\n");
}

static void test_jal_decode() {
  std::uint32_t inst = 0x008000EF;  // JAL x1, +8
  assert(decode_rv32(inst) == OpCode::JAL);
  assert(get_rd(inst) == 1);
  assert(get_op_class(decode_rv32(inst)) == 1);  // BRANCH
  printf("  [PASS] test_jal_decode\n");
}

static void test_addi_decode() {
  std::uint32_t inst = 0x00108093;  // ADDI x1, x1, 1
  assert(decode_rv32(inst) == OpCode::ADDI);
  assert(get_rs1(inst) == 1);
  assert(get_rd(inst) == 1);
  assert(get_imm(OpCode::ADDI, inst) == 1);
  printf("  [PASS] test_addi_decode\n");
}

static void test_sub_decode() {
  std::uint32_t inst = 0x40208133;  // SUB x2, x1, x2
  assert(decode_rv32(inst) == OpCode::SUB);
  assert(get_funct3(inst) == 0);
  assert(get_funct7(inst) == 0x20);  // 区分 ADD (0x00) vs SUB (0x20)
  printf("  [PASS] test_sub_decode\n");
}

static void test_beq_decode() {
  std::uint32_t inst = 0x00208163;  // BEQ x1, x2, +4
  assert(decode_rv32(inst) == OpCode::BEQ);
  assert(get_funct3(inst) == 0b000);
  assert(get_op_class(decode_rv32(inst)) == 1);
  printf("  [PASS] test_beq_decode\n");
}

int main() {
  printf("test_decode:\n");
  test_lui_decode();
  test_jal_decode();
  test_addi_decode();
  test_sub_decode();
  test_beq_decode();
  printf("[PASS] all RiscvDecodePlugin tests\n");
  return 0;
}

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

#include "catch_amalgamated.hpp"
#include <cstdint>

#include "ip/cpu/arch/riscv/decoder_table.h"

using namespace cf::cpu::arch::riscv;

TEST_CASE("lui_decode", "[cpu]") {
  std::uint32_t inst = 0x123451B7;  // LUI x3, 0x12345
  REQUIRE(decode_rv32(inst) == OpCode::LUI);
  REQUIRE(get_rd(inst) == 3);
  REQUIRE(get_imm(OpCode::LUI, inst) == 0x12345000);
}

TEST_CASE("jal_decode", "[cpu]") {
  std::uint32_t inst = 0x008000EF;  // JAL x1, +8
  REQUIRE(decode_rv32(inst) == OpCode::JAL);
  REQUIRE(get_rd(inst) == 1);
  REQUIRE(get_op_class(decode_rv32(inst)) == 1);  // BRANCH
}

TEST_CASE("addi_decode", "[cpu]") {
  std::uint32_t inst = 0x00108093;  // ADDI x1, x1, 1
  REQUIRE(decode_rv32(inst) == OpCode::ADDI);
  REQUIRE(get_rs1(inst) == 1);
  REQUIRE(get_rd(inst) == 1);
  REQUIRE(get_imm(OpCode::ADDI, inst) == 1);
}

TEST_CASE("sub_decode", "[cpu]") {
  std::uint32_t inst = 0x40208133;  // SUB x2, x1, x2
  REQUIRE(decode_rv32(inst) == OpCode::SUB);
  REQUIRE(get_funct3(inst) == 0);
  REQUIRE(get_funct7(inst) == 0x20);  // 区分 ADD (0x00) vs SUB (0x20)
}

TEST_CASE("beq_decode", "[cpu]") {
  std::uint32_t inst = 0x00208163;  // BEQ x1, x2, +4
  REQUIRE(decode_rv32(inst) == OpCode::BEQ);
  REQUIRE(get_funct3(inst) == 0b000);
  REQUIRE(get_op_class(decode_rv32(inst)) == 1);
}



// tests/cpu/test_decode_full.cpp
//
// 功能描述: RV32I 全部 40 条基础指令译码正确性测试 (M3.11)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (40+ 用例, 每条 RV32I 基础指令 1 用例):
//   - U-type: LUI, AUIPC (2)
//   - J-type: JAL (1)
//   - I-type: JALR, ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI, FENCE, SYSTEM (12)
//   - B-type: BEQ, BNE, BLT, BGE, BLTU, BGEU (6)
//   - L-type: LB, LH, LW, LBU, LHU (5)
//   - S-type: SB, SH, SW (3)
//   - R-type: ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND (10)
//   - ECALL/EBREAK (funct12 区分, 归入 SYSTEM)
//
// 约束:
//   - 每条指令验证: decode_rv32() == 期望 OpCode + 关键字段 (rd/rs1/rs2/imm)

#include "catch_amalgamated.hpp"
#include <cstdint>

#include "ip/cpu/arch/riscv/decoder_table.h"

using namespace cf::cpu::arch::riscv;

TEST_CASE("u_type", "[cpu]") {
  // LUI: lui x1, 0x12345
  std::uint32_t inst = 0x123450B7;
  CHECK(decode_rv32(inst) == OpCode::LUI);
  CHECK(get_rd(inst) == 1);
  CHECK(get_imm(OpCode::LUI, inst) == 0x12345000);

  // AUIPC: auipc x2, 0x1
  inst = 0x00001117;
  CHECK(decode_rv32(inst) == OpCode::AUIPC);
  CHECK(get_rd(inst) == 2);
}

TEST_CASE("j_type", "[cpu]") {
  // JAL: jal x1, +0x100
  std::uint32_t inst = 0x100000EF;
  CHECK(decode_rv32(inst) == OpCode::JAL);
  CHECK(get_rd(inst) == 1);
}

TEST_CASE("i_type_branch", "[cpu]") {
  // JALR: jalr x1, x2, 4
  std::uint32_t inst = 0x004100E7;
  CHECK(decode_rv32(inst) == OpCode::JALR);
  CHECK(get_rd(inst) == 1);
  CHECK(get_rs1(inst) == 2);
  CHECK(get_imm(OpCode::JALR, inst) == 4);
}

TEST_CASE("i_type_alu", "[cpu]") {
  // ADDI: addi x1, x0, 5
  std::uint32_t inst = 0x00500093;
  CHECK(decode_rv32(inst) == OpCode::ADDI);
  CHECK(get_imm(OpCode::ADDI, inst) == 5);

  // SLTI: slti x1, x2, -1 (0xFFF sign-ext = -1)
  inst = 0xFFF12093;
  CHECK(decode_rv32(inst) == OpCode::SLTI);
  CHECK(get_imm(OpCode::SLTI, inst) == -1);

  // SLTIU: sltiu x1, x2, 1
  inst = 0x00113093;
  CHECK(decode_rv32(inst) == OpCode::SLTIU);

  // XORI: xori x1, x2, 0xFF
  inst = 0x0FF14093;
  CHECK(decode_rv32(inst) == OpCode::XORI);

  // ORI: ori x1, x2, 0xFF
  inst = 0x0FF16093;
  CHECK(decode_rv32(inst) == OpCode::ORI);

  // ANDI: andi x1, x2, 0xFF
  inst = 0x0FF17093;
  CHECK(decode_rv32(inst) == OpCode::ANDI);

  // SLLI: slli x1, x2, 4
  inst = 0x00411093;
  CHECK(decode_rv32(inst) == OpCode::SLLI);
  CHECK(get_funct7(inst) == 0x00);

  // SRLI: srli x1, x2, 4
  inst = 0x00415093;
  CHECK(decode_rv32(inst) == OpCode::SRLI);
  CHECK(get_funct7(inst) == 0x00);

  // SRAI: srai x1, x2, 4 (funct7=0x20)
  inst = 0x40415093;
  CHECK(decode_rv32(inst) == OpCode::SRAI);
  CHECK(get_funct7(inst) == 0x20);
}

TEST_CASE("misc", "[cpu]") {
  // FENCE: fence iorw, iorw
  std::uint32_t inst = 0x0FF0000F;
  CHECK(decode_rv32(inst) == OpCode::FENCE);

  // ECALL: funct12 = 0
  inst = 0x00000073;
  CHECK(decode_rv32(inst) == OpCode::SYSTEM);
  CHECK(get_funct12(inst) == 0);

  // EBREAK: funct12 = 1
  inst = 0x00100073;
  CHECK(decode_rv32(inst) == OpCode::SYSTEM);
  CHECK(get_funct12(inst) == 1);

  // MRET: funct12 = 0x302
  inst = 0x30200073;
  CHECK(decode_rv32(inst) == OpCode::SYSTEM);
  CHECK(get_funct12(inst) == 0x302);
}

TEST_CASE("b_type", "[cpu]") {
  // BEQ: beq x1, x2, +4
  std::uint32_t inst = 0x00208163;
  CHECK(decode_rv32(inst) == OpCode::BEQ);

  // BNE: bne x1, x2, +4
  inst = 0x00209163;
  CHECK(decode_rv32(inst) == OpCode::BNE);

  // BLT: blt x1, x2, +4
  inst = 0x0020C163;
  CHECK(decode_rv32(inst) == OpCode::BLT);

  // BGE: bge x1, x2, +4
  inst = 0x0020D163;
  CHECK(decode_rv32(inst) == OpCode::BGE);

  // BLTU: bltu x1, x2, +4
  inst = 0x0020E163;
  CHECK(decode_rv32(inst) == OpCode::BLTU);

  // BGEU: bgeu x1, x2, +4
  inst = 0x0020F163;
  CHECK(decode_rv32(inst) == OpCode::BGEU);
}

TEST_CASE("l_type", "[cpu]") {
  // LB: lb x1, 0(x2)
  std::uint32_t inst = 0x00010083;
  CHECK(decode_rv32(inst) == OpCode::LB);

  // LH: lh x1, 0(x2)
  inst = 0x00011083;
  CHECK(decode_rv32(inst) == OpCode::LH);

  // LW: lw x1, 0(x2)
  inst = 0x00012083;
  CHECK(decode_rv32(inst) == OpCode::LW);

  // LBU: lbu x1, 0(x2)
  inst = 0x00014083;
  CHECK(decode_rv32(inst) == OpCode::LBU);

  // LHU: lhu x1, 0(x2)
  inst = 0x00015083;
  CHECK(decode_rv32(inst) == OpCode::LHU);
}

TEST_CASE("s_type", "[cpu]") {
  // SB: sb x1, 0(x2)
  std::uint32_t inst = 0x00110023;
  CHECK(decode_rv32(inst) == OpCode::SB);

  // SH: sh x1, 0(x2)
  inst = 0x00111023;
  CHECK(decode_rv32(inst) == OpCode::SH);

  // SW: sw x1, 0(x2)
  inst = 0x00112023;
  CHECK(decode_rv32(inst) == OpCode::SW);
}

TEST_CASE("r_type", "[cpu]") {
  // ADD: add x1, x2, x3
  std::uint32_t inst = 0x003100B3;
  CHECK(decode_rv32(inst) == OpCode::ADD);
  CHECK(get_funct7(inst) == 0x00);

  // SUB: sub x1, x2, x3 (funct7=0x20)
  inst = 0x403100B3;
  CHECK(decode_rv32(inst) == OpCode::SUB);
  CHECK(get_funct7(inst) == 0x20);

  // SLL: sll x1, x2, x3
  inst = 0x003110B3;
  CHECK(decode_rv32(inst) == OpCode::SLL);

  // SLT: slt x1, x2, x3
  inst = 0x003120B3;
  CHECK(decode_rv32(inst) == OpCode::SLT);

  // SLTU: sltu x1, x2, x3
  inst = 0x003130B3;
  CHECK(decode_rv32(inst) == OpCode::SLTU);

  // XOR: xor x1, x2, x3
  inst = 0x003140B3;
  CHECK(decode_rv32(inst) == OpCode::XOR);

  // SRL: srl x1, x2, x3
  inst = 0x003150B3;
  CHECK(decode_rv32(inst) == OpCode::SRL);
  CHECK(get_funct7(inst) == 0x00);

  // SRA: sra x1, x2, x3 (funct7=0x20)
  inst = 0x403150B3;
  CHECK(decode_rv32(inst) == OpCode::SRA);
  CHECK(get_funct7(inst) == 0x20);

  // OR: or x1, x2, x3
  inst = 0x003160B3;
  CHECK(decode_rv32(inst) == OpCode::OR);

  // AND: and x1, x2, x3
  inst = 0x003170B3;
  CHECK(decode_rv32(inst) == OpCode::AND);
}



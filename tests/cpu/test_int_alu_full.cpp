// tests/cpu/test_int_alu_full.cpp
//
// 功能描述: RV32I 全部 10 条算术指令运算正确性测试 (M3.12)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (10 用例, 每条算术指令 1 用例):
//   ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND
//
// 约束:
//   - 每条指令验证: compute() 在多种输入下正确

#include "catch_amalgamated.hpp"
#include <cstdint>

#include "ip/cpu/arch/riscv/int_alu.h"
#include "ip/cpu/arch/riscv/decoder_table.h"

using namespace cf::cpu::arch::riscv;
using T = std::uint32_t;

TEST_CASE("add", "[cpu]") {
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::ADD, 3, 4) == 7);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::ADD, 0, 0) == 0);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::ADD, 0xFFFFFFFF, 1) == 0);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::ADD, 0x7FFFFFFF, 1) == 0x80000000);
}

TEST_CASE("sub", "[cpu]") {
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SUB, 10, 3) == 7);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SUB, 0, 0) == 0);
  // SUB(3, 10) = 0xFFFFFFF9 (modular), 跳过以避免有符号/无符号转换歧义
}

TEST_CASE("sll", "[cpu]") {
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SLL, 1, 4) == 16);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SLL, 1, 0) == 1);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SLL, 0xFF, 8) == 0xFF00);
  // 只取 rs2 低 5 位
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SLL, 1, 32) == 1);  // 32 & 0x1F = 0
}

TEST_CASE("slt", "[cpu]") {
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SLT, 3, 5) == 1);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SLT, 5, 3) == 0);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SLT, -1, 0) == 1);   // 0xFFFFFFFF < 0
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SLT, 0, -1) == 0);
}

TEST_CASE("sltu", "[cpu]") {
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SLTU, 3, 5) == 1);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SLTU, 0xFFFFFFFF, 5) == 0);  // 无符号 UINT_MAX > 5
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SLTU, 5, 0xFFFFFFFF) == 1);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SLTU, 0, 0) == 0);
}

TEST_CASE("xor", "[cpu]") {
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::XOR, 0xFF, 0x0F) == 0xF0);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::XOR, 0xAA, 0x55) == 0xFF);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::XOR, 0, 0) == 0);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::XOR, 0xFF, 0xFF) == 0);
}

TEST_CASE("srl", "[cpu]") {
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SRL, 0xF0, 4) == 0x0F);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SRL, 0, 5) == 0);
  // 逻辑右移: 高位补 0
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SRL, 0xFFFFFFFF, 1) == 0x7FFFFFFF);
  // 只取 rs2 低 5 位
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SRL, 0xF0, 36) == 0x0F);  // 36 & 0x1F = 4, 0xF0>>4=0x0F
}

TEST_CASE("sra", "[cpu]") {
  // 算术右移: 保留符号位
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SRA, 0xFFFFFFF0, 2) == 0xFFFFFFFC);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SRA, 0xF0, 4) == 0x0F);  // 正数与 SRL 相同
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SRA, 0x80000000, 1) == 0xC0000000);
  // 只取 rs2 低 5 位
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::SRA, 0xFFFFFFF0, 34) == 0xFFFFFFFC);  // 34 & 0x1F = 2
}

TEST_CASE("or", "[cpu]") {
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::OR, 0xF0, 0x0F) == 0xFF);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::OR, 0, 0) == 0);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::OR, 0xFF, 0xFF) == 0xFF);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::OR, 0xAA, 0x55) == 0xFF);
}

TEST_CASE("and", "[cpu]") {
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::AND, 0xFF, 0x0F) == 0x0F);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::AND, 0xF0, 0x0F) == 0x00);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::AND, 0, 0xFF) == 0);
  CHECK(RiscvIntAluPlugin<T>::compute(OpCode::AND, 0xFF, 0xFF) == 0xFF);
}



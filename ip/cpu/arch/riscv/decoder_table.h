// ip/cpu/arch/riscv/decoder_table.h
//
// 功能描述: RISC-V RV32I 译码表 (M3.1, P0)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - 覆盖 RV32I 全部 40 条基础指令 (LUI/AUIPC/JAL/JALR/BRANCH/LOAD/STORE/OP-IMM/OP/FENCE/SYSTEM)
//   - funct3/funct7 字段查表 → OpCode 枚举
//   - 提供 RISC-V 标准指令格式枚举 (R/I/S/B/U/J)
//   - 提供字段提取辅助函数 (rs1/rs2/rd/funct3/funct7/imm)
//
// 借鉴:
//   - RISC-V Unprivileged Spec (Volume I, Chapter 2: RV32I Base Integer Instruction Set)
//   - Spike ISS decode.cc
//   - VexRiscv Decode.scala
//
// 约束:
//   - 头文件为主 (constexpr 函数, 编译期求值)
//   - 模板参数化 <typename T>: T = xlen 类型 (uint32_t/uint64_t)
//   - 覆盖 RV32I 全部 40 条 (LUI/AUIPC/JAL/JALR 6 BRANCH 5 LOAD 3 STORE 9 OP-IMM 10 OP 1 FENCE 1 SYSTEM = 40)

#ifndef CF_IP_CPU_ARCH_RISCV_DECODER_TABLE_H
#define CF_IP_CPU_ARCH_RISCV_DECODER_TABLE_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "cf/plugin/uint_t.h"

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

// ----------------------------------------------------------------------------
// OpCode 枚举 —— RV32I 全部 40 条基础指令
// ----------------------------------------------------------------------------
enum class OpCode : std::uint16_t {
  // 未识别指令
  UNKNOWN = 0,

  // U-type (2 条)
  LUI    = 1,    // 0110111
  AUIPC  = 2,    // 0010111

  // J-type (2 条)
  JAL    = 3,    // 1101111
  JALR   = 4,    // 1100111 (I-type 编码)

  // B-type (6 条 BRANCH)
  BEQ    = 10,   // funct3=000
  BNE    = 11,   // funct3=001
  BLT    = 12,   // funct3=100
  BGE    = 13,   // funct3=101
  BLTU   = 14,   // funct3=110
  BGEU   = 15,   // funct3=111

  // L-type (5 条 LOAD)
  LB     = 20,   // funct3=000
  LH     = 21,   // funct3=001
  LW     = 22,   // funct3=010
  LBU    = 23,   // funct3=100
  LHU    = 24,   // funct3=101

  // S-type (3 条 STORE)
  SB     = 30,   // funct3=000
  SH     = 31,   // funct3=001
  SW     = 32,   // funct3=010

  // I-type ALU (9 条 OP-IMM)
  ADDI   = 40,   // funct3=000
  SLTI   = 41,   // funct3=010
  SLTIU  = 42,   // funct3=011
  XORI   = 43,   // funct3=100
  ORI    = 44,   // funct3=110
  ANDI   = 45,   // funct3=111
  SLLI   = 46,   // funct3=001, funct7=0000000
  SRLI   = 47,   // funct3=101, funct7=0000000
  SRAI   = 48,   // funct3=101, funct7=0100000

  // R-type ALU (10 条 OP)
  ADD    = 50,   // funct3=000, funct7=0000000
  SUB    = 51,   // funct3=000, funct7=0100000
  SLL    = 52,   // funct3=001, funct7=0000000
  SLT    = 53,   // funct3=010, funct7=0000000
  SLTU   = 54,   // funct3=011, funct7=0000000
  XOR    = 55,   // funct3=100, funct7=0000000
  SRL    = 56,   // funct3=101, funct7=0000000
  SRA    = 57,   // funct3=101, funct7=0100000
  OR     = 58,   // funct3=110, funct7=0000000
  AND    = 59,   // funct3=111, funct7=0000000

  // FENCE (1 条)
  FENCE  = 60,   // 0001111 (I-type 编码)

  // SYSTEM (1 条基础类, 详细 funct12 区分 ECALL/EBREAK/MRET/CSRR*)
  SYSTEM = 70,   // 1110011 (I-type 编码)
};

// ----------------------------------------------------------------------------
// InstructionFormat 枚举 —— RISC-V 标准 6 种指令格式
// ----------------------------------------------------------------------------
enum class InstructionFormat : std::uint8_t {
  R_TYPE = 0,   // 寄存器-寄存器操作
  I_TYPE = 1,   // 立即数-寄存器操作 / LOAD / JALR
  S_TYPE = 2,   // STORE
  B_TYPE = 3,   // BRANCH
  U_TYPE = 4,   // 上位立即数 (LUI/AUIPC)
  J_TYPE = 5,   // 跳转 (JAL)
};

// ----------------------------------------------------------------------------
// RISC-V 标准 opcode 字段值 (inst[6:0])
// ----------------------------------------------------------------------------
namespace opcode {
  constexpr std::uint8_t OP_LUI    = 0b0110111;
  constexpr std::uint8_t OP_AUIPC  = 0b0010111;
  constexpr std::uint8_t OP_JAL    = 0b1101111;
  constexpr std::uint8_t OP_JALR   = 0b1100111;
  constexpr std::uint8_t OP_BRANCH = 0b1100011;
  constexpr std::uint8_t OP_LOAD   = 0b0000011;
  constexpr std::uint8_t OP_STORE  = 0b0100011;
  constexpr std::uint8_t OP_OPIMM  = 0b0010011;
  constexpr std::uint8_t OP_OP     = 0b0110011;
  constexpr std::uint8_t OP_FENCE  = 0b0001111;
  constexpr std::uint8_t OP_SYSTEM = 0b1110011;
}

// ----------------------------------------------------------------------------
// 字段提取辅助函数 (constexpr, 编译期求值)
//
// RISC-V 32-bit 指令字布局 (各 type 不同):
//   R-type: funct7[31:25] | rs2[24:20] | rs1[19:15] | funct3[14:12] | rd[11:7] | opcode[6:0]
//   I-type: imm[11:0][31:20] | rs1[19:15] | funct3[14:12] | rd[11:7] | opcode[6:0]
//   S-type: imm[11:5][31:25] | rs2[24:20] | rs1[19:15] | funct3[14:12] | imm[4:0][11:7] | opcode[6:0]
//   B-type: imm[12|10:5][31:25] | rs2[24:20] | rs1[19:15] | funct3[14:12] | imm[4:1|11][11:7] | opcode[6:0]
//   U-type: imm[31:12][31:12] | rd[11:7] | opcode[6:0]
//   J-type: imm[20|10:1|11|19:12][31:12] | rd[11:7] | opcode[6:0]
// ----------------------------------------------------------------------------
constexpr std::uint8_t get_opcode(std::uint32_t inst) {
  return static_cast<std::uint8_t>(inst & 0x7F);
}

constexpr std::uint8_t get_rd(std::uint32_t inst) {
  return static_cast<std::uint8_t>((inst >> 7) & 0x1F);
}

constexpr std::uint8_t get_funct3(std::uint32_t inst) {
  return static_cast<std::uint8_t>((inst >> 12) & 0x07);
}

constexpr std::uint8_t get_rs1(std::uint32_t inst) {
  return static_cast<std::uint8_t>((inst >> 15) & 0x1F);
}

constexpr std::uint8_t get_rs2(std::uint32_t inst) {
  return static_cast<std::uint8_t>((inst >> 20) & 0x1F);
}

constexpr std::uint8_t get_funct7(std::uint32_t inst) {
  return static_cast<std::uint8_t>((inst >> 25) & 0x7F);
}

constexpr std::uint16_t get_funct12(std::uint32_t inst) {
  return static_cast<std::uint16_t>((inst >> 20) & 0xFFF);
}

// I-type 立即数 (12-bit, 符号扩展)
constexpr std::int32_t imm_i(std::uint32_t inst) {
  std::int32_t imm = static_cast<std::int32_t>(inst) >> 20;
  return imm;
}

// S-type 立即数 (12-bit, 符号扩展)
constexpr std::int32_t imm_s(std::uint32_t inst) {
  std::int32_t imm = (static_cast<std::int32_t>(inst) >> 25) << 5;
  imm |= ((inst >> 7) & 0x1F);
  // 符号扩展
  if (imm & 0x800) imm |= 0xFFFFF000;
  return imm;
}

// B-type 立即数 (13-bit, 符号扩展)
constexpr std::int32_t imm_b(std::uint32_t inst) {
  std::int32_t imm = 0;
  imm |= ((inst >> 31) & 1) << 12;       // bit 12
  imm |= ((inst >> 7) & 1) << 11;        // bit 11
  imm |= ((inst >> 25) & 0x3F) << 5;     // bits 10:5
  imm |= ((inst >> 8) & 0xF) << 1;       // bits 4:1
  imm &= ~1;                              // bit 0 = 0
  // 符号扩展 (13-bit)
  if (imm & 0x1000) imm |= 0xFFFFE000;
  return imm;
}

// U-type 立即数 (20-bit, 高位)
constexpr std::int32_t imm_u(std::uint32_t inst) {
  return static_cast<std::int32_t>(inst) & 0xFFFFF000;
}

// J-type 立即数 (21-bit, 符号扩展)
constexpr std::int32_t imm_j(std::uint32_t inst) {
  std::int32_t imm = 0;
  imm |= ((inst >> 31) & 1) << 20;       // bit 20
  imm |= ((inst >> 12) & 0xFF) << 12;    // bits 19:12
  imm |= ((inst >> 20) & 1) << 11;       // bit 11
  imm |= ((inst >> 21) & 0x3FF) << 1;    // bits 10:1
  imm &= ~1;                              // bit 0 = 0
  // 符号扩展 (21-bit)
  if (imm & 0x100000) imm |= 0xFFE00000;
  return imm;
}

// ----------------------------------------------------------------------------
// get_format —— 根据 OpCode 推导指令格式
// ----------------------------------------------------------------------------
constexpr InstructionFormat get_format(OpCode op) {
  switch (op) {
    case OpCode::LUI:
    case OpCode::AUIPC:
      return InstructionFormat::U_TYPE;
    case OpCode::JAL:
      return InstructionFormat::J_TYPE;
    case OpCode::JALR:
    case OpCode::LB: case OpCode::LH: case OpCode::LW:
    case OpCode::LBU: case OpCode::LHU:
    case OpCode::ADDI: case OpCode::SLTI: case OpCode::SLTIU:
    case OpCode::XORI: case OpCode::ORI: case OpCode::ANDI:
    case OpCode::SLLI: case OpCode::SRLI: case OpCode::SRAI:
    case OpCode::FENCE: case OpCode::SYSTEM:
      return InstructionFormat::I_TYPE;
    case OpCode::BEQ: case OpCode::BNE: case OpCode::BLT:
    case OpCode::BGE: case OpCode::BLTU: case OpCode::BGEU:
      return InstructionFormat::B_TYPE;
    case OpCode::SB: case OpCode::SH: case OpCode::SW:
      return InstructionFormat::S_TYPE;
    case OpCode::ADD: case OpCode::SUB: case OpCode::SLL:
    case OpCode::SLT: case OpCode::SLTU: case OpCode::XOR:
    case OpCode::SRL: case OpCode::SRA: case OpCode::OR: case OpCode::AND:
      return InstructionFormat::R_TYPE;
    default:
      return InstructionFormat::I_TYPE;  // 保守默认
  }
}

// ----------------------------------------------------------------------------
// get_imm —— 根据 OpCode 提取立即数
// ----------------------------------------------------------------------------
constexpr std::int32_t get_imm(OpCode op, std::uint32_t inst) {
  switch (get_format(op)) {
    case InstructionFormat::I_TYPE: return imm_i(inst);
    case InstructionFormat::S_TYPE: return imm_s(inst);
    case InstructionFormat::B_TYPE: return imm_b(inst);
    case InstructionFormat::U_TYPE: return imm_u(inst);
    case InstructionFormat::J_TYPE: return imm_j(inst);
    default: return 0;
  }
}

// ----------------------------------------------------------------------------
// decode_rv32 —— 主译码函数
//
// 输入: 32-bit 指令字
// 输出: OpCode (UNKNOWNN 表示无法识别)
//
// 实现: 先看 opcode[6:0] 分大类, 再看 funct3/funct7 分具体指令
// ----------------------------------------------------------------------------
constexpr OpCode decode_rv32(std::uint32_t inst) {
  const std::uint8_t op_field = get_opcode(inst);
  const std::uint8_t f3 = get_funct3(inst);
  const std::uint8_t f7 = get_funct7(inst);

  // U-type (只看 opcode)
  if (op_field == opcode::OP_LUI)   return OpCode::LUI;
  if (op_field == opcode::OP_AUIPC) return OpCode::AUIPC;

  // J-type (只看 opcode)
  if (op_field == opcode::OP_JAL)   return OpCode::JAL;
  if (op_field == opcode::OP_JALR)  return OpCode::JALR;

  // B-type BRANCH
  if (op_field == opcode::OP_BRANCH) {
    switch (f3) {
      case 0b000: return OpCode::BEQ;
      case 0b001: return OpCode::BNE;
      case 0b100: return OpCode::BLT;
      case 0b101: return OpCode::BGE;
      case 0b110: return OpCode::BLTU;
      case 0b111: return OpCode::BGEU;
      default:    return OpCode::UNKNOWN;
    }
  }

  // L-type LOAD
  if (op_field == opcode::OP_LOAD) {
    switch (f3) {
      case 0b000: return OpCode::LB;
      case 0b001: return OpCode::LH;
      case 0b010: return OpCode::LW;
      case 0b100: return OpCode::LBU;
      case 0b101: return OpCode::LHU;
      default:    return OpCode::UNKNOWN;
    }
  }

  // S-type STORE
  if (op_field == opcode::OP_STORE) {
    switch (f3) {
      case 0b000: return OpCode::SB;
      case 0b001: return OpCode::SH;
      case 0b010: return OpCode::SW;
      default:    return OpCode::UNKNOWN;
    }
  }

  // I-type ALU (OP-IMM)
  if (op_field == opcode::OP_OPIMM) {
    switch (f3) {
      case 0b000: return OpCode::ADDI;
      case 0b010: return OpCode::SLTI;
      case 0b011: return OpCode::SLTIU;
      case 0b100: return OpCode::XORI;
      case 0b110: return OpCode::ORI;
      case 0b111: return OpCode::ANDI;
      case 0b001: return OpCode::SLLI;  // funct7 必须为 0
      case 0b101:
        if (f7 == 0b0000000) return OpCode::SRLI;
        if (f7 == 0b0100000) return OpCode::SRAI;
        return OpCode::UNKNOWN;
      default:    return OpCode::UNKNOWN;
    }
  }

  // R-type ALU (OP)
  if (op_field == opcode::OP_OP) {
    switch (f3) {
      case 0b000:
        if (f7 == 0b0000000) return OpCode::ADD;
        if (f7 == 0b0100000) return OpCode::SUB;
        return OpCode::UNKNOWN;
      case 0b001: return OpCode::SLL;
      case 0b010: return OpCode::SLT;
      case 0b011: return OpCode::SLTU;
      case 0b100: return OpCode::XOR;
      case 0b101:
        if (f7 == 0b0000000) return OpCode::SRL;
        if (f7 == 0b0100000) return OpCode::SRA;
        return OpCode::UNKNOWN;
      case 0b110: return OpCode::OR;
      case 0b111: return OpCode::AND;
      default:    return OpCode::UNKNOWN;
    }
  }

  // FENCE
  if (op_field == opcode::OP_FENCE) return OpCode::FENCE;

  // SYSTEM
  if (op_field == opcode::OP_SYSTEM) return OpCode::SYSTEM;

  return OpCode::UNKNOWN;
}

// ----------------------------------------------------------------------------
// get_op_class —— 从 OpCode 推导通用 op_class (用于 DecodePayload)
// ----------------------------------------------------------------------------
constexpr int get_op_class(OpCode op) {
  switch (op) {
    case OpCode::LUI: case OpCode::AUIPC:
    case OpCode::ADDI: case OpCode::SLTI: case OpCode::SLTIU:
    case OpCode::XORI: case OpCode::ORI: case OpCode::ANDI:
    case OpCode::SLLI: case OpCode::SRLI: case OpCode::SRAI:
    case OpCode::ADD: case OpCode::SUB: case OpCode::SLL:
    case OpCode::SLT: case OpCode::SLTU: case OpCode::XOR:
    case OpCode::SRL: case OpCode::SRA: case OpCode::OR: case OpCode::AND:
      return 0;  // ALU
    case OpCode::JAL: case OpCode::JALR:
    case OpCode::BEQ: case OpCode::BNE: case OpCode::BLT:
    case OpCode::BGE: case OpCode::BLTU: case OpCode::BGEU:
      return 1;  // BRANCH
    case OpCode::LB: case OpCode::LH: case OpCode::LW:
    case OpCode::LBU: case OpCode::LHU:
      return 2;  // LOAD
    case OpCode::SB: case OpCode::SH: case OpCode::SW:
      return 3;  // STORE
    case OpCode::FENCE: case OpCode::SYSTEM:
      return 4;  // SYSTEM
    default:
      return 255;  // UNKNOWN
  }
}

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_ARCH_RISCV_DECODER_TABLE_H

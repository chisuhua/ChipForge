// ip/cpu/arch/riscv/decode.h
//
// 功能描述: RiscvDecodePlugin — RISC-V 指令译码 (M3.3, P0)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - decode 阶段: 32-bit 指令 → DECODE (op_class + 索引) + RISCV_DETAIL (funct3/funct7/imm)
//   - 使用 decoder_table.h 的 constexpr 查表 (编译期 O(1), 运行时 0 开销)
//   - 同时填 pl::DECODE (通用) 和 pl::RISCV_DETAIL (RISC-V 特有)
//   - 模板参数化 <typename T>: T = xlen 类型
//
// 借鉴:
//   - VexRiscv Decode.scala (funct3/funct7 双查表)
//   - Spike ISS decode (指令字解析)
//
// 约束:
//   - 头文件为主 (无 .cpp, build() 在头文件实现)
//   - D4 合规: 无业务 tick(), 阶段用 at_stage()

#ifndef CF_IP_CPU_ARCH_RISCV_DECODE_H
#define CF_IP_CPU_ARCH_RISCV_DECODE_H

#include <cstdint>
#include <type_traits>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/arch/riscv/decoder_table.h"
#include "ip/cpu/arch/riscv/payload_riscv.h"
#include "ip/cpu/core/payload_common.h"

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

template <typename T>
class RiscvDecodePlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value,
                "RiscvDecodePlugin<T>: T must be unsigned");

 public:
  RiscvDecodePlugin() = default;
  ~RiscvDecodePlugin() override = default;

  RiscvDecodePlugin(const RiscvDecodePlugin&) = delete;
  RiscvDecodePlugin& operator=(const RiscvDecodePlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}

  // ------------------------------------------------------------------------
  // build() — decode 阶段: 读取 INSTRUCTION, 填 DECODE + RISCV_DETAIL
  // ------------------------------------------------------------------------
  void build(cf::plugin::PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;
    using RvKey = payload_keys_riscv<T>;
    using Dp = cf::cpu::core::payload::DecodePayload;

    pb.at_stage("decode", cf::plugin::Phase::NORMAL, [&pb]() {
      auto* n = pb.node_of_logic_stage("decode").get();
      if (!n) return;
      std::uint32_t inst = static_cast<std::uint32_t>(n->operator()(KeyType::INSTRUCTION));

      OpCode op = decode_rv32(inst);
      auto& dec = n->operator()(KeyType::DECODE);
      auto& rv = n->operator()(RvKey::RISCV_DETAIL);

      // 填通用 DecodePayload
      dec.op_class = static_cast<Dp::OpClass>(get_op_class(op));
      dec.writes_rd = (get_rd(inst) != 0);  // rd != x0 才写回
      dec.reads_rs1 = true;                 // 大多数指令都读 rs1
      dec.reads_rs2 = is_rs2_used(op);
      dec.rd_class = 0;                     // 0 = GPR
      dec.rs1_idx = get_rs1(inst);
      dec.rs2_idx = get_rs2(inst);
      dec.rd_idx = get_rd(inst);

      // 填 RISC-V 特有 RiscvDecodeDetail
      rv.funct3 = get_funct3(inst);
      rv.funct7 = get_funct7(inst);
      rv.funct12 = get_funct12(inst);
      rv.imm = get_imm(op, inst);
      rv.csr_addr = (op == OpCode::SYSTEM) ? get_funct12(inst) : 0;
    });
  }

 private:
  // 判断 OpCode 是否使用 rs2 字段
  static bool is_rs2_used(OpCode op) {
    switch (op) {
      // R-type ALU 全部用 rs2
      case OpCode::ADD: case OpCode::SUB: case OpCode::SLL:
      case OpCode::SLT: case OpCode::SLTU: case OpCode::XOR:
      case OpCode::SRL: case OpCode::SRA: case OpCode::OR: case OpCode::AND:
      // S-type STORE 用 rs2
      case OpCode::SB: case OpCode::SH: case OpCode::SW:
      // B-type BRANCH 用 rs2
      case OpCode::BEQ: case OpCode::BNE: case OpCode::BLT:
      case OpCode::BGE: case OpCode::BLTU: case OpCode::BGEU:
        return true;
      default:
        return false;
    }
  }
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_ARCH_RISCV_DECODE_H

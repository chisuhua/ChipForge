// ip/cpu/arch/riscv/int_alu.h
//
// 功能描述: RiscvIntAluPlugin — RV32I/RV64I 整数运算 (M3.4, P0)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - execute 阶段: 读取 RS1/RS2 + RISCV_DETAIL, 写 RESULT
//   - 覆盖 RV32I 全部 10 条算术指令 (ADD/SUB/SLL/SLT/SLTU/XOR/SRL/SRA/OR/AND)
//   - 模板参数化 <typename T>: T = xlen 类型
//
// 约束:
//   - 头文件为主 (无 .cpp)
//   - D4 合规: 无业务 tick(), 阶段用 at_stage()

#ifndef CF_IP_CPU_ARCH_RISCV_INT_ALU_H
#define CF_IP_CPU_ARCH_RISCV_INT_ALU_H

#include <cstdint>
#include <type_traits>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/arch/riscv/decoder_table.h"
#include "ip/cpu/arch/riscv/payload_riscv.h"
#include "ip/cpu/core/payload_common.h"

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

template <typename T>
class RiscvIntAluPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value,
                "RiscvIntAluPlugin<T>: T must be unsigned");

 public:
  RiscvIntAluPlugin() = default;
  ~RiscvIntAluPlugin() override = default;

  RiscvIntAluPlugin(const RiscvIntAluPlugin&) = delete;
  RiscvIntAluPlugin& operator=(const RiscvIntAluPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}

  void build(cf::plugin::PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;
    using RvKey = payload_keys_riscv<T>;

    pb.at_stage("execute", cf::plugin::Phase::NORMAL, [&pb]() {
      auto* n = pb.node_of_logic_stage("execute").get();
      if (!n) return;
      T rs1_val = n->operator()(KeyType::RS1);
      T rs2_val = n->operator()(KeyType::RS2);
      const auto& rv = n->operator()(RvKey::RISCV_DETAIL);

      // 推断 OpCode (简化: 根据 funct3/funct7)
      OpCode op = infer_opcode(rv.funct3, rv.funct7);

      T result = compute(op, rs1_val, rs2_val);
      n->operator()(KeyType::RESULT) = result;
    });
  }

  // 单元测试辅助: 直接执行 ALU 运算
  static T compute(OpCode op, T rs1, T rs2) {
    switch (op) {
      case OpCode::ADD:  return rs1 + rs2;
      case OpCode::SUB:  return rs1 - rs2;
      case OpCode::SLL:  return rs1 << (rs2 & 0x1F);
      case OpCode::SLT:  return (static_cast<std::int32_t>(rs1) <
                                  static_cast<std::int32_t>(rs2)) ? 1 : 0;
      case OpCode::SLTU: return (rs1 < rs2) ? 1 : 0;
      case OpCode::XOR:  return rs1 ^ rs2;
      case OpCode::SRL:  return rs1 >> (rs2 & 0x1F);
      case OpCode::SRA: {
        std::int32_t s = static_cast<std::int32_t>(rs1);
        std::int32_t sh = rs2 & 0x1F;
        return static_cast<T>(s >> sh);
      }
      case OpCode::OR:   return rs1 | rs2;
      case OpCode::AND:  return rs1 & rs2;
      default: return 0;
    }
  }

 private:
  // 从 funct3/funct7 推断 OpCode (execute 阶段没有完整指令字, 用字段)
  static OpCode infer_opcode(std::uint8_t f3, std::uint8_t f7) {
    switch (f3) {
      case 0b000: return (f7 == 0x20) ? OpCode::SUB : OpCode::ADD;
      case 0b001: return OpCode::SLL;
      case 0b010: return OpCode::SLT;
      case 0b011: return OpCode::SLTU;
      case 0b100: return OpCode::XOR;
      case 0b101: return (f7 == 0x20) ? OpCode::SRA : OpCode::SRL;
      case 0b110: return OpCode::OR;
      case 0b111: return OpCode::AND;
      default: return OpCode::UNKNOWN;
    }
  }
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_ARCH_RISCV_INT_ALU_H

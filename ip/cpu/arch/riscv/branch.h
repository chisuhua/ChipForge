// ip/cpu/arch/riscv/branch.h
//
// 功能描述: RiscvBranchPlugin — 分支跳转执行 (M3.5, P1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - execute 阶段: 读取 RS1/RS2, 评估分支条件, 写 BRANCH_TARGET + branch_taken
//   - 覆盖: BEQ/BNE/BLT/BGE/BLTU/BGEU (B-type) + JAL/JALR (J-type)
//   - 模板参数化 <typename T>: T = xlen 类型
//
// 约束:
//   - 头文件为主 (无 .cpp)
//   - D4 合规: 无业务 tick(), 阶段用 at_stage()

#ifndef CF_IP_CPU_ARCH_RISCV_BRANCH_H
#define CF_IP_CPU_ARCH_RISCV_BRANCH_H

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
class RiscvBranchPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value,
                "RiscvBranchPlugin<T>: T must be unsigned");

 public:
  RiscvBranchPlugin() = default;
  ~RiscvBranchPlugin() override = default;

  RiscvBranchPlugin(const RiscvBranchPlugin&) = delete;
  RiscvBranchPlugin& operator=(const RiscvBranchPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}

  void build(cf::plugin::PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;
    using RvKey = payload_keys_riscv<T>;
    using Dp = cf::cpu::core::payload::DecodePayload;

    pb.at_stage("execute", cf::plugin::Phase::NORMAL, [&pb]() {
      auto* n = pb.node_of_logic_stage("execute").get();
      if (n) {
        T rs1_val = n->operator()(KeyType::RS1);
        T rs2_val = n->operator()(KeyType::RS2);
        T pc_val = n->operator()(KeyType::PC);
        const auto& rv = n->operator()(RvKey::RISCV_DETAIL);
        const auto& dec = n->operator()(KeyType::DECODE);

        bool taken = false;
        T target = pc_val + 4;  // 默认 next_pc

        // 根据 funct3 判断分支条件 (B-type)
        switch (rv.funct3) {
          case 0b000: taken = (rs1_val == rs2_val); break;  // BEQ
          case 0b001: taken = (rs1_val != rs2_val); break;  // BNE
          case 0b100: taken = (static_cast<std::int32_t>(rs1_val) <
                               static_cast<std::int32_t>(rs2_val)); break;  // BLT
          case 0b101: taken = (static_cast<std::int32_t>(rs1_val) >=
                               static_cast<std::int32_t>(rs2_val)); break;  // BGE
          case 0b110: taken = (rs1_val < rs2_val); break;  // BLTU
          case 0b111: taken = (rs1_val >= rs2_val); break;  // BGEU
        }

        if (taken) {
          // B-type 目标: PC + imm (imm 已在 RISCV_DETAIL.imm)
          target = pc_val + static_cast<T>(rv.imm);
        }

        // JAL: 无条件跳转, 链接 PC+4 到 RD
        // JALR: 跳转到 rs1+imm, 链接 PC+4 到 RD
        if (dec.op_class == Dp::OpClass::BRANCH && rv.funct7 == 0) {
          // JAL/JALR: 总是跳转
          if (rv.funct3 == 0) {  // JALR
            target = rs1_val + static_cast<T>(rv.imm);
          } else {
            target = pc_val + static_cast<T>(rv.imm);
          }
          taken = true;
          n->operator()(KeyType::RD_DATA) = pc_val + 4;  // link
        }

        // 写结果到通用 payload (RISC-V 特有)
        auto& dec_mut = n->operator()(KeyType::DECODE);
        dec_mut.branch_taken = taken;
        dec_mut.branch_target = target;
        n->operator()(RvKey::BRANCH_TARGET) = target;
      }
    });
  }

  // 单元测试辅助: 评估分支条件
  static bool evaluate_branch(std::uint8_t funct3, T rs1, T rs2) {
    switch (funct3) {
      case 0b000: return rs1 == rs2;
      case 0b001: return rs1 != rs2;
      case 0b100: return static_cast<std::int32_t>(rs1) <
                         static_cast<std::int32_t>(rs2);
      case 0b101: return static_cast<std::int32_t>(rs1) >=
                         static_cast<std::int32_t>(rs2);
      case 0b110: return rs1 < rs2;
      case 0b111: return rs1 >= rs2;
      default: return false;
    }
  }
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_ARCH_RISCV_BRANCH_H

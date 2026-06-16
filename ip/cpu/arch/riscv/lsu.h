// ip/cpu/arch/riscv/lsu.h
//
// 功能描述: RiscvLsuPlugin — 加载存储单元 (M3.7, P1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - memory 阶段: 根据 op_class (LOAD/STORE) 执行地址生成 + 访存
//   - 覆盖: LB/LH/LW/LBU/LHU (LOAD) + SB/SH/SW (STORE) (RV32I 基础)
//   - 当前: 简化地址生成, M4 集成 TLM 事务
//   - 模板参数化 <typename T>: T = xlen 类型
//
// 约束:
//   - 头文件为主 (无 .cpp)
//   - D4 合规: 无业务 tick(), 阶段用 at_stage()

#ifndef CF_IP_CPU_ARCH_RISCV_LSU_H
#define CF_IP_CPU_ARCH_RISCV_LSU_H

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
class RiscvLsuPlugin : public cf::plugin::PluginBase {
  static_assert(std::is_unsigned<T>::value,
                "RiscvLsuPlugin<T>: T must be unsigned");

 public:
  RiscvLsuPlugin() = default;
  ~RiscvLsuPlugin() override = default;

  RiscvLsuPlugin(const RiscvLsuPlugin&) = delete;
  RiscvLsuPlugin& operator=(const RiscvLsuPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}

  void build(cf::plugin::PipeBuilder& pb) override {
    using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;
    using RvKey = payload_keys_riscv<T>;
    using Dp = cf::cpu::core::payload::DecodePayload;

    pb.at_stage("memory", cf::plugin::Phase::NORMAL, [&pb]() {
      auto* n = pb.node_of_logic_stage("memory").get();
      if (n) {
        T rs1_val = n->operator()(KeyType::RS1);
        const auto& rv = n->operator()(RvKey::RISCV_DETAIL);
        const auto& dec = n->operator()(KeyType::DECODE);

        // 地址生成: rs1 + imm
        T addr = rs1_val + static_cast<T>(rv.imm);

        if (dec.op_class == Dp::OpClass::LOAD) {
          n->operator()(KeyType::MEM_ADDR) = addr;
          // M2 stub: 返回 0, M4 集成 TLM 事务
          n->operator()(KeyType::MEM_DATA) = T{0};
        } else if (dec.op_class == Dp::OpClass::STORE) {
          T rs2_val = n->operator()(KeyType::RS2);
          n->operator()(KeyType::MEM_ADDR) = addr;
          n->operator()(KeyType::MEM_DATA) = rs2_val;
        }
      }
    });
  }
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_ARCH_RISCV_LSU_H

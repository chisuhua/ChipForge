// ip/cpu/arch/riscv/csr.h
//
// 功能描述: RiscvCsrPlugin — CSR 寄存器 (M3.8, P2 stub)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - P2 stub: CSR 仅占位, 实际 270+ CSR 推迟到 M3+ 或 Phase 5+
//   - 接口预留: setup()/build() 与其他 Plugin 一致
//   - 未来扩展: mstatus/mtvec/mepc/mcause/mie/mip 等 CSR 表
//
// 约束:
//   - 头文件为主 (无 .cpp 实现)
//   - D4 合规: 无业务 tick(), 占位类不实现具体逻辑

#ifndef CF_IP_CPU_ARCH_RISCV_CSR_H
#define CF_IP_CPU_ARCH_RISCV_CSR_H

#include "cf/plugin/plugin_base.h"

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

class RiscvCsrPlugin : public cf::plugin::PluginBase {
 public:
  RiscvCsrPlugin() = default;
  ~RiscvCsrPlugin() override = default;

  RiscvCsrPlugin(const RiscvCsrPlugin&) = delete;
  RiscvCsrPlugin& operator=(const RiscvCsrPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}
  void build(cf::plugin::PipeBuilder& /*pb*/) override {}
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_ARCH_RISCV_CSR_H

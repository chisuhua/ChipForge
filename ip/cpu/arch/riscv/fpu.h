// ip/cpu/arch/riscv/fpu.h
//
// 功能描述: RiscvFpuPlugin — 浮点单元 (M3.9, P3+ 占位)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - P3+ 占位: FPU 推迟到 Phase 5+ (F/D 扩展)
//   - 当前仅为占位类
//   - 未来扩展: 32 浮点寄存器 (f0-f31) + 浮点运算流水线
//
// 约束:
//   - 头文件为主 (无 .cpp)

#ifndef CF_IP_CPU_ARCH_RISCV_FPU_H
#define CF_IP_CPU_ARCH_RISCV_FPU_H

#include "cf/plugin/plugin_base.h"

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

class RiscvFpuPlugin : public cf::plugin::PluginBase {
 public:
  RiscvFpuPlugin() = default;
  ~RiscvFpuPlugin() override = default;

  RiscvFpuPlugin(const RiscvFpuPlugin&) = delete;
  RiscvFpuPlugin& operator=(const RiscvFpuPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}
  void build(cf::plugin::PipeBuilder& /*pb*/) override {}
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_ARCH_RISCV_FPU_H

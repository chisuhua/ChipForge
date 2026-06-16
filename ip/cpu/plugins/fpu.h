// ip/cpu/plugins/fpu.h
//
// 功能描述: FPUPlugin — 浮点单元 (M2.6, P3+ 占位)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - P3+ 占位: 浮点运算推迟到 P3 阶段 (RV32F/RV64D 支持)
//   - 当前仅为占位类, 不含实际浮点逻辑
//   - 接口预留: setup()/build() 与其他 Plugin 一致
//   - 未来扩展: FPU 寄存器堆 (f0-f31) + 浮点运算流水线
//
// 约束:
//   - D4 合规: 无业务 tick(), 占位类不实现具体逻辑

#ifndef CF_IP_CPU_PLUGINS_FPU_H
#define CF_IP_CPU_PLUGINS_FPU_H

#include "cf/plugin/plugin_base.h"

namespace cf {
namespace cpu {
namespace plugins {

class FPUPlugin : public cf::plugin::PluginBase {
 public:
  FPUPlugin() = default;
  ~FPUPlugin() override = default;

  FPUPlugin(const FPUPlugin&) = delete;
  FPUPlugin& operator=(const FPUPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}
  void build(cf::plugin::PipeBuilder& /*pb*/) override {}
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_FPU_H

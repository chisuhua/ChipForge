// ip/cpu/plugins/mmu.h
//
// 功能描述: MMUPlugin — 内存管理单元 (M2.7, P3+ 占位)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - P3+ 占位: MMU 推迟到 P3 阶段 (虚拟内存支持)
//   - 当前仅为占位类, 不含实际分页逻辑
//   - 接口预留: setup()/build() 与其他 Plugin 一致
//   - 未来扩展: TLB + 页表遍历 (Sv32/Sv39/Sv48)
//
// 约束:
//   - D4 合规: 无业务 tick(), 占位类不实现具体逻辑

#ifndef CF_IP_CPU_PLUGINS_MMU_H
#define CF_IP_CPU_PLUGINS_MMU_H

#include "cf/plugin/plugin_base.h"

namespace cf {
namespace cpu {
namespace plugins {

class MMUPlugin : public cf::plugin::PluginBase {
 public:
  MMUPlugin() = default;
  ~MMUPlugin() override = default;

  MMUPlugin(const MMUPlugin&) = delete;
  MMUPlugin& operator=(const MMUPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}
  void build(cf::plugin::PipeBuilder& /*pb*/) override {}
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_MMU_H

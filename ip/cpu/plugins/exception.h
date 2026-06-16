// ip/cpu/plugins/exception.h
//
// 功能描述: ExceptionPlugin — 异常处理 (M2.8, P3+ 占位)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - P3+ 占位: 异常处理推迟到 P3 阶段
//   - 当前仅为占位类, 不含实际异常处理逻辑
//   - 接口预留: setup()/build() 与其他 Plugin 一致
//   - 未来扩展: mcause/mepc/mtvec CSR + trap handler
//
// 约束:
//   - D4 合规: 无业务 tick(), 占位类不实现具体逻辑

#ifndef CF_IP_CPU_PLUGINS_EXCEPTION_H
#define CF_IP_CPU_PLUGINS_EXCEPTION_H

#include "cf/plugin/plugin_base.h"

namespace cf {
namespace cpu {
namespace plugins {

class ExceptionPlugin : public cf::plugin::PluginBase {
 public:
  ExceptionPlugin() = default;
  ~ExceptionPlugin() override = default;

  ExceptionPlugin(const ExceptionPlugin&) = delete;
  ExceptionPlugin& operator=(const ExceptionPlugin&) = delete;

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {}
  void build(cf::plugin::PipeBuilder& /*pb*/) override {}
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_EXCEPTION_H

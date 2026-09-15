// ip/cpu/plugins/exception.h
//
// 功能描述: ExceptionPlugin — 异常处理 (M2.8, P3+ 占位)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-15
//
// 设计:
//   - P3+ 占位: 异常处理推迟到 P3 阶段
//   - 当前仅为占位类, 不含实际异常处理逻辑
//   - 接口预留: setup()/build() 与其他 Plugin 一致
//   - 未来扩展: mcause/mepc/mtvec CSR + trap handler
//   - plugin-framework-stall commit C: throw_when 演示桩
//     (注册 throw_when([]{return false;}) 演示 API, 不实装真实 trap delivery)
//
// 约束:
//   - D4 合规: 无业务 tick(), 占位类不实现具体逻辑

#ifndef CF_IP_CPU_PLUGINS_EXCEPTION_H
#define CF_IP_CPU_PLUGINS_EXCEPTION_H

#include <memory>

#include "cf/plugin/ctrl_link.h"
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

  // plugin-framework-stall commit C: throw_when 演示桩
  // TODO Phase 5+: 实装真实 trap delivery (mcause/mepc/mtvec CSR + 异常向量)
  // 当前仅注册 throw_when API, lambda 恒 false (不实装触发), 用于演示框架契约.
  void build(cf::plugin::PipeBuilder& pb) override {
    auto throw_demo = std::make_shared<cf::plugin::CtrlLink>();
    // 实际 trap 触发条件: 检测 DECODE.op_class == EXCEPTION 类指令, 异常码非 0
    // 当前为占位: 恒 false, throw 不会触发
    throw_demo->throw_when([]() { return false; });
    pb.register_ctrl_link("execute", throw_demo);
  }
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_EXCEPTION_H

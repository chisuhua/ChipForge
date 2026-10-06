// ip/cpu/plugins/mmu_exception_handler.h
//
// MmuExceptionHandlerPlugin — TLM 版 (cpu-pipeline-mmufault-handler v1, 2026-10-06)
// 消费 cpu_keys::CPU_EXCEPTION_CODE (mmu_exit 闭包在 ip/cpu/plugins/mmu.cpp:128-130
// 已传播 mmu_keys::EXCEPTION_CODE → cpu_keys::CPU_EXCEPTION_CODE), 触发 CtrlLink
// flush_when 让 framework 在 memory LATE 期间 flush pipeline.
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef CF_IP_CPU_PLUGINS_MMU_EXCEPTION_HANDLER_H
#define CF_IP_CPU_PLUGINS_MMU_EXCEPTION_HANDLER_H

#include <cstdint>

#include "cf/plugin/ctrl_link.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "ip/cpu/tlm/cpu_keys.h"

namespace cf {
namespace cpu {
namespace plugins {

template <typename T>
class MmuExceptionHandlerPlugin : public cf::plugin::PluginBase {
 public:
  MmuExceptionHandlerPlugin() = default;
  ~MmuExceptionHandlerPlugin() override = default;

  MmuExceptionHandlerPlugin(const MmuExceptionHandlerPlugin&) = delete;
  MmuExceptionHandlerPlugin& operator=(const MmuExceptionHandlerPlugin&) = delete;

  // 公开测试访问器
  bool mmu_exception_pending() const noexcept {
    return mmu_exception_pending_;
  }

  // reset() — 由 trap handler 完成 mepc/mcause 写后调用, 复位 pending
  // (per cpu-pipeline-mmufault-handler v1 tasks.md 3.1)
  void reset() noexcept { mmu_exception_pending_ = false; }

  void setup(cf::plugin::PipeBuilder& pb) override {
    flush_ctrl_ = std::make_shared<cf::plugin::CtrlLink>();
    flush_ctrl_->flush_when([this]() { return mmu_exception_pending_; });
    pb.register_ctrl_link("memory", flush_ctrl_);
  }

  void build(cf::plugin::PipeBuilder& pb) override {
    pb.at_stage("memory", cf::plugin::Phase::LATE, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("memory").get();
      if (n) {
        const auto exc_code = n->operator()(cf::cpu::tlm::payload::cpu_keys<std::uint32_t>::CPU_EXCEPTION_CODE);
        if (exc_code != 0) {
          mmu_exception_pending_ = true;
        }
      }
    });
  }

 private:
  std::shared_ptr<cf::plugin::CtrlLink> flush_ctrl_;
  bool mmu_exception_pending_ = false;
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_MMU_EXCEPTION_HANDLER_H

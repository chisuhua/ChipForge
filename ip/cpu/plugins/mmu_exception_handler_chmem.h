// ip/cpu/plugins/mmu_exception_handler_chmem.h
//
// MmuExceptionHandlerPlugin — CH_MEM 版 (cpu-pipeline-mmufault-handler v1, 2026-10-06)
//
// 消费 cpu_keys::CPU_EXCEPTION_CODE (ch_uint<8> in CH_MEM mode) 在 memory stage LATE
// combinational network, 输出 mmufault_clear (ch_bool) 到 HazardPlugin stall_ctrl_.
// 设计 (v1 tasks.md 4.1): 用 ch_bool / ch_reg / ch_state_machine DSL (仅 trap PC
// 跳转走 FSM, 符合 ADR-046 v2.0 豁免范围).
//
// 当前状态 (cpu-pipeline-mmufault-handler v1 Phase 4 stub):
//   - 声明 setup / build / reset / mark_mmufault / clear_mmufault / mmufault_pending
//     API 对齐 TLM 版 (但 mmufault_pending / mark_mmufault 是 no-op, 因为 combinational
//     network 在 mmufault_clear ch_bool signal DAG 里维护, 不需要 instance state).
//   - 完整 combinational network (读 CPU_EXCEPTION_CODE → 输出 mmufault_clear 到
//     HazardPlugin stall_ctrl_) 推迟到 v0.10.4 hotfix 后或 mmu_chmem.h 实装后
//     (Phase 6d.6). 当前 mmufault_clear 永远 ch_bool(false) → HazardPlugin RAW stall
//     不受影响.
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef CF_IP_CPU_PLUGINS_MMU_EXCEPTION_HANDLER_CHMEM_H
#define CF_IP_CPU_PLUGINS_MMU_EXCEPTION_HANDLER_CHMEM_H

#include <memory>

#include "cf/plugin/ctrl_link.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/payload.h"

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

  void reset() {}
  void mark_mmufault() noexcept {}
  void clear_mmufault() noexcept {}
  bool mmufault_pending() const noexcept { return false; }

  void setup(cf::plugin::PipeBuilder& pb) override {
    (void)pb;
  }

  void build(cf::plugin::PipeBuilder& pb) override {
    (void)pb;
  }

 private:
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_MMU_EXCEPTION_HANDLER_CHMEM_H

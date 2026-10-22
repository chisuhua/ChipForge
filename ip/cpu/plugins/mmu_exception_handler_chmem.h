// ip/cpu/plugins/mmu_exception_handler_chmem.h
//
// MmuExceptionHandlerPlugin — CH_MEM 版 (mmu-chmem-pipeline-integration Phase C.1)
//
// 消费 cpu_keys::CPU_EXCEPTION_CODE 在 memory stage NORMAL combinational network,
// 输出 mmufault_clear (ch_bool) 到 HazardPlugin stall_ctrl_ (decode stage 合并)。
// 设计 (v1 tasks.md 4.1): 用 ch_bool / ch_reg / ch_state_machine DSL (仅 trap PC
// 跳转走 FSM, 符合 ADR-046 v2.0 豁免范围).
//
// C.1 状态 (2026-10-11 PR mmu-chmem-pipeline-integration C.1 [GREEN] 骨架):
//   - ✅ setup() 创建 stall_ctrl_ 并 register_ctrl_link("decode", ...) — 与
//     HazardPlugin stall_ctrl_ 同一 stage, PipeBuilder::elaborate() 期间
//     合并所有 halt_conds_ 为 OR 网
//   - ✅ build() 注册 at_stage("memory", NORMAL) 闭包骨架
//   - ⏸ build() 闭包内**未**实际读 CPU_EXCEPTION_CODE → 原因:CH_MEM 模式下
//     CPU_EXCEPTION_CODE key 类型原为 `Payload<std::uint8_t>` (TLM-typed,
//     见 ip/cpu/tlm/cpu_keys.h),而 PipeNode 在 CH_MEM 模式实例化为
//     `PipeNode<ch::core::ch_uint<32>>` (cf::cpu::core::payload::keys<T, 32>
//     风格,见 ip/cpu/arch/riscv/payload_riscv.h + ip/cpu/core/payload_common.h)。
//     跨类型 operator() 在 CppHDL 0.x (build/_deps/install/include/cpp-hdl/)
//     下未确认 semantic 兼容,落地 MMUPluginChmem 后应统一 key 类型为 ch_uint<8> 再
//     接线,避免 silent cast 风险
//     ✅ **类型阻塞已解除 (2026-10-22 `chmem-cpu-keys-modality`)**: `cpu_keys` 的
//     4 个 key 已改 mode-aware ⇒ `CPU_EXCEPTION_CODE` 的 cell 类型是
//     `ch::core::ch_uint<8>` (CH_MEM) ⇒ **不再是 POD 常量**, 可直接读节点。
//     ⏸ **剩余阻塞 = 无 CH_MEM 生产者**: CH_MEM 侧写 `CPU_EXCEPTION_CODE` 的
//     是 `RiscvMMUPluginChmem::mmu_exit` 闭包, 归 `chmem-ptw-walk` (未立项);
//     在那之前本闭包保持 no-op (bare path byte-identical) —— 接线归 C.1。
//   - bare path byte-identical: mmufault_clear 恒 ch_bool(false), HazardPlugin
//     RAW stall 不受影响 (Phase A.6 验证 [mmu] 59/59 + [cpu] 128/129 PASS
//     维持,1 pre-existing fail: test_cpu_sim_real_tohost — cpu_sim CLI 路径
//     pipeline 不 tick 根因 defer Phase 6d.6+ cpu-pipeline-tick-rca change)
//
// API 兼容:setup/build/reset/mark_mmufault/clear_mmufault/mmufault_pending 签名
// 不变,组合逻辑不维护 instance state (mmufault_pending 恒 false)
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

  // mark_mmufault / clear_mmufault 保留 no-op stub (combinational network 不需要
  // instance state; mmu_fault_pending 由组合 ch_bool signal 直接驱动 stall_ctrl_)
  void mark_mmufault() noexcept {}
  void clear_mmufault() noexcept {}
  bool mmufault_pending() const noexcept { return false; }

  // setup — 创建 stall_ctrl_ 并注册到 decode stage
  //
  // 与 HazardPlugin stall_ctrl_ 同 stage ("decode"),PipeBuilder::elaborate()
  // 期间合并所有 halt_conds_ 为 OR 网,任一 halt 触发即冻结 fetch。
  // 选择 "decode" 而非 "memory" 是 D3-A 最小:stall + flush 二级联动
  // (decode 冻结, memory 自然停, 不需要单独 memory-stage flush signal)
  void setup(cf::plugin::PipeBuilder& pb) override {
    stall_ctrl_ = std::make_shared<cf::plugin::CtrlLink>();
    pb.register_ctrl_link("decode", stall_ctrl_);
  }

  // build — 注册 combinational network 闭包 (D3-A 最小)
  //
  // 闭包 skeleton 已注册,实际 ch_bool 接线待 Phase B (mmu_chmem.h) 落地后
  // 启用。当前 no-op: CPU_EXCEPTION_CODE 的 key 类型仍是裸 `Payload<std::uint8_t>`
  // (ip/cpu/tlm/cpu_keys.h:48), CH_MEM 下装不下 ch 信号。
  //
  // ⚠ 2026-10-22 `chmem-mmu-keys-modality`: 本文件依赖的 MMU 侧 key
  //    (`mmu_keys::EXCEPTION_CODE`) 已模式化为 `cf::plugin::uint_t<8>`;
  //    但 CPU 侧 `cpu_keys::CPU_EXCEPTION_CODE` 归姊妹 change
  //    `chmem-cpu-keys-modality`(挂 Phase C.1 入口), 本文件保持 no-op 直到
  //    该 change archive。参见 ADR-040 v3.0 §10.5 / §10.8。
  void build(cf::plugin::PipeBuilder& pb) override {
    pb.at_stage("memory", cf::plugin::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("memory").get();
      if (n && stall_ctrl_) {
        // C.1 接线点 (Phase B post-merge, 前置 = chmem-cpu-keys-modality archive):
        //   const auto exc_code = n->operator()(cpu_keys<std::uint32_t>::CPU_EXCEPTION_CODE);
        //   const auto zero8 = ch::core::ch_uint<8>(ch::core::ch_literal<0, 8>{});
        //   const auto mmufault_clear = (exc_code != zero8);
        //   stall_ctrl_->halt_when(mmufault_clear);
        // 维持 bare path byte-identical 直到 Phase B 落地
        //
        // 注: key 模板参数必须保持 **raw** `std::uint32_t`(与消费端同实例化),
        //     不能写 `cpu_keys<ch_uint<8>>` —— Task 类模板的 static_assert
        //     (cpu_keys.h:34-36) 要求 T 是裸整型, 且 PayloadStore 按指针身份索引。
      }
    });
  }

 private:
  std::shared_ptr<cf::plugin::CtrlLink> stall_ctrl_;
};

}  // namespace plugins
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_MMU_EXCEPTION_HANDLER_CHMEM_H

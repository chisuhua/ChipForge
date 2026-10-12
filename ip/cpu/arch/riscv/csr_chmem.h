// ip/cpu/arch/riscv/csr_chmem.h
//
// RiscvCsrPluginChmem — CH_MEM 版 (csr-chmem-pipeline-integration Phase A.2)
//
// RISC-V Zicsr 扩展 (CSRRW/CSRRS/CSRRC + immediate variants) + M-mode trap 路径
// (ECALL/EBREAK/MRET) + 8 个 M-mode CSR 寄存器 (mstatus/mtvec/mepc/mcause/mie/mip/
// mscratch/mcycle) 的 CH_MEM 实装 (per change proposal §1 D1 收窄 scope)。
//
// 设计 (ADR-083 Plugin-style SSOT, ADR-046 v2.0 §2.1.1 算术/控制 FSM 豁免):
//   - TLM 端 RiscvCsrPlugin (ip/cpu/arch/riscv/csr.h) 是 43 行 P2 stub,无业务逻辑
//   - CH_MEM 端独立实现,使用 ch::core::ch_reg / ch::core::ch_uint / ch::core::ch_bool
//     表达 CSR 状态,组合逻辑网络 + ch_state_machine 状态机 (trap 路径)
//   - 与 cpu_factory_chmem.h 7-plugin 流水线集成 (B.1: 第 8 plugin,RegFile 之后注册)
//   - 复用 mmu-chmem-pipeline-integration C.1 (commit 1429f5a) stall_ctrl_ 路径
//     (MRET 触发的 mcause/mepc 写走 mmu_exception_handler 相同 hazard 路径)
//
// A.2 [GREEN] 骨架 (2026-10-11 PR csr-chmem-pipeline-integration):
//   - 类定义 + setup/build empty (本 commit)
//   - D4 合规: 0 业务 tick(), 0 状态机 (FSM 推迟 A.4)
//   - A.3 (CSR value ch_reg 存储) + A.4 (ch_state_machine trap FSM) 后续 commits
//   - cpu_factory_chmem.h 集成 (B.1) 推迟到 A.3+A.4 完成后
//   - bare path byte-identical: 类未注册,无任何功能影响
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef CF_IP_CPU_ARCH_RISCV_CSR_CHMEM_H
#define CF_IP_CPU_ARCH_RISCV_CSR_CHMEM_H

#include "cf/plugin/plugin_base.h"

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

template <typename T>
class RiscvCsrPluginChmem : public cf::plugin::PluginBase {
 public:
  RiscvCsrPluginChmem() = default;
  ~RiscvCsrPluginChmem() override = default;

  RiscvCsrPluginChmem(const RiscvCsrPluginChmem&) = delete;
  RiscvCsrPluginChmem& operator=(const RiscvCsrPluginChmem&) = delete;

  // 公开访问器 (test 用): 当前 mstatus value
  // A.3 落地后从 ch_reg<32> 读回; A.2 骨架阶段 return 0
  std::uint32_t mstatus() const noexcept { return mstatus_; }

  void reset() {
    mstatus_ = 0;
    mtvec_ = 0;
    mepc_ = 0;
    mcause_ = 0;
    mie_ = 0;
    mip_ = 0;
    mscratch_ = 0;
    mcycle_ = 0;
  }

  void setup(cf::plugin::PipeBuilder& /*pb*/) override {
    // A.2 骨架: 空。 A.3 落地时:
    //   - 注册 at_stage("execute", LATE) 闭包处理 CSR 写
    //   - 注册 at_stage("execute", NORMAL) 闭包处理 CSR 读
    //   - 注册 at_stage("execute", NORMAL) 闭包处理 ECALL/EBREAK/MRET trap 触发
  }

  void build(cf::plugin::PipeBuilder& /*pb*/) override {
    // A.2 骨架: 空。 A.3+A.4 落地时实装。
  }

 private:
  // A.2 骨架: 8 个 CSR 暂用 instance state (组合逻辑 A.3 落地后改为 ch_reg 表达)
  std::uint32_t mstatus_ = 0;
  std::uint32_t mtvec_ = 0;
  std::uint32_t mepc_ = 0;
  std::uint32_t mcause_ = 0;
  std::uint32_t mie_ = 0;
  std::uint32_t mip_ = 0;
  std::uint32_t mscratch_ = 0;
  std::uint32_t mcycle_ = 0;
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_ARCH_RISCV_CSR_CHMEM_H

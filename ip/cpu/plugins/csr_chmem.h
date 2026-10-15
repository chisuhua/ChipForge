// ip/cpu/plugins/csr_chmem.h
//
// RiscvCsrPluginChmem — CH_MEM 版 (csr-chmem-pipeline-integration)
//
// RISC-V Zicsr 扩展 (CSRRW/CSRRS/CSRRC + immediate variants) + M-mode trap 路径
// (ECALL/EBREAK/MRET) + 8 个 M-mode CSR 寄存器 (mstatus/mtvec/mepc/mcause/mie/mip/
// mscratch/mcycle) 的 CH_MEM 实装 (per change proposal §1 D1 收窄 scope)。
//
// 设计 (ADR-083 Plugin-style SSOT, ADR-046 v2.0 §2.1.1 算术/控制 FSM 豁免):
//   - TLM 端 RiscvCsrPlugin (ip/cpu/arch/riscv/csr.h) 是 P2 stub, 无业务逻辑
//   - CH_MEM 端独立实现, 使用 ch::core::ch_reg / ch::core::ch_uint / ch::core::ch_bool
//     表达 CSR 状态, 组合逻辑网络 + ch_state_machine 状态机 (trap 路径)
//   - 与 cpu_factory_chmem.h 7-plugin 流水线集成 (B.1: 第 8 plugin, RegFile 之后注册)
//   - 复用 mmu-chmem-pipeline-integration C.1 (commit 1429f5a) stall_ctrl_ 路径
//
// [R1 阶段, 2026-10-14]:
//   - 在 execute-LATE 闭包内重提取 INSTRUCTION 位字段 (csr_addr / funct12 /
//     imm5 / opcode / funct3) — 此为 R3 (CSR_READ_VALUE) 与 R2 (TRAP) 通路的
//     统一信号源 (per design.md §R1 方案 A)
//   - 不写 payload, 仅重提取 (R3/R2 由后续 phase 落地)
//   - 设计关键: at_stage(LATE) 闭包 (而非 build 函数体直接调), 保证 INSTRUCTION
//     已被上游 fetch/decode 阶段驱动 → cpphdl bits<> narrow slice 在 sim tick 时
//     有真实 impl (否则 build 期 elab narrow 出 nullptr impl → SIGABRT)
//
// [R7 路径修正, 2026-10-14]: 本文件从 ip/cpu/arch/riscv/csr_chmem.h git mv 到
//   ip/cpu/plugins/csr_chmem.h, 走 check_plugin_portability.sh Check 2 强制检查。
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef CF_IP_CPU_PLUGINS_CSR_CHMEM_H
#define CF_IP_CPU_PLUGINS_CSR_CHMEM_H

#include <cstdint>
#include <memory>

#include "cf/plugin/plugin_base.h"

#ifdef CF_PLUGIN_USE_CH_MEM
#include <ch.hpp>
#include "ip/cpu/core/payload_common.h"
#endif

namespace cf {
namespace cpu {
namespace arch {
namespace riscv {

#ifdef CF_PLUGIN_USE_CH_MEM
using namespace ch;
using namespace ch::core;
namespace cfc = ::cf::plugin;
#endif

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

#ifdef CF_PLUGIN_USE_CH_MEM
  // R1 测试访问器 (DAG signal references, sim->get_signal_value() 取 post-tick 值)
  ch_uint<32> last_csr_addr_ch() const { return last_csr_addr_; }
  ch_uint<32> last_funct12_ch()  const { return last_funct12_; }
  ch_uint<5>  last_imm5_ch()     const { return last_imm5_; }
  ch_uint<7>  last_opcode_ch()   const { return last_opcode_; }
  ch_uint<3>  last_funct3_ch()   const { return last_funct3_; }
#endif

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
    // A.2 骨架: 空。 A.3 落地时将:
    //   - 在 build() 注册 at_stage("execute", LATE) 闭包 (本 commit 已实装)
    //   - 写 CSR_READ_VALUE / TRAP_PENDING / writeback ch_reg (R3/R2 阶段)
  }

  void build(cf::plugin::PipeBuilder& pb) override {
#ifdef CF_PLUGIN_USE_CH_MEM
    // R1.2 [GREEN]: 在 execute-LATE 阶段注册闭包, 从 INSTRUCTION 重提取 5 个位字段
    //   → ch_signal 然后存为 instance ch_signal (last_*_), 供单测通过
    //     sim->get_signal_value() 读 post-tick DAG 值。
    //
    // 必须用 at_stage 闭包 (而非 build 函数体直接调):
    //   - build 阶段直接调 n->operator()(INSTRUCTION) + bits<> 会让 INSTRUCTION
    //     成为未驱动的 DAG node, cpphdl 5.052 elab narrow → nullptr impl → tick
    //     越界 (SIGABRT)。at_stage 闭包延迟到上游 fetch/decode 写入后才执行。
    //   - LATE phase 是在 execute stage 所有 NORMAL 闭包执行之后, 此时 inputs
    //     (INSTRUCTION/RS1/RS2) 均已被上游 stage 驱动到 execute node。
    pb.at_stage("execute", cfc::Phase::LATE,
                [this, &pb]() {
      auto* n = pb.node_of_logic_stage("execute").get();
      if (!n) {
        return;
      }
      using KeyType = cf::cpu::core::payload::keys<T, 32>;
      auto instr = n->operator()(KeyType::INSTRUCTION);

      // R1 决策: bits<MSB,LSB> 自提取 (不扩 DecodedInst 字段)
      auto funct12_ch = bits<31, 20>(instr);
      auto imm5_ch    = bits<19, 15>(instr);
      auto funct3_ch  = bits<14, 12>(instr);
      auto opcode_ch  = bits<6,  0>(instr);

      // ch 信号类型的重提取 DAG 节点 (instance 成员)
      last_csr_addr_  = funct12_ch;   // bits<31,20>
      last_funct12_   = funct12_ch;
      last_imm5_      = imm5_ch;
      last_opcode_    = opcode_ch;
      last_funct3_    = funct3_ch;
    });
#else
    // TLM 模式: 当前 R1 决策仅针对 CH_MEM 通路, TLM 端 RiscvCsrPlugin 仍为 stub
    (void)pb;
#endif
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

#ifdef CF_PLUGIN_USE_CH_MEM
  // R1.4 单测观察口: execute-LATE 闭包重提取的 ch DAG signals (post-tick 取值)
  // 注: ch_signal 默认值必须用 ch::core::ch_literal<V, W>{} (v0.3.1 M6 修复前
  //   ch_uint<N>(0) 会产生 null impl -> SEGV)
  ch_uint<32> last_csr_addr_{ch::core::ch_literal<0, 32>{}};
  ch_uint<32> last_funct12_{ch::core::ch_literal<0, 32>{}};
  ch_uint<5>  last_imm5_{ch::core::ch_literal<0, 5>{}};
  ch_uint<7>  last_opcode_{ch::core::ch_literal<0, 7>{}};
  ch_uint<3>  last_funct3_{ch::core::ch_literal<0, 3>{}};
#endif
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_CSR_CHMEM_H

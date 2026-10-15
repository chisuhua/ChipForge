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
//     表达 CSR 状态, 组合逻辑网络 (R3 读通路, writeback 写通路)
//   - 与 cpu_factory_chmem.h 7-plugin 流水线集成 (B.1: 第 8 plugin, RegFile 之后注册)
//   - 复用 mmu-chmem-pipeline-integration C.1 (commit 1429f5a) stall_ctrl_ 路径
//
// [R1 阶段, 2026-10-14]:
//   - execute-LATE 闭包从 INSTRUCTION 重提取 5 个位字段 (csr_addr / funct12 /
//     imm5 / opcode / funct3) — R3/R2 通路的统一信号源 (per design.md §R1 方案 A)
//
// [R3 阶段, 2026-10-15]:
//   - execute-LATE 闭包追加: 写 csr_keys::CSR_READ_VALUE / CSR_READ_VALID
//     (8 选 1 mux, 读 mstatus_/mtvec_/.../mcycle_ ch_reg 当前值)
//   - writeback-NORMAL 闭包: 8 个 CSR ch_reg 写 (write-enable + select 树),
//     严格不用 if(ch_bool) (per R4 [High] 风险)
//   - 不实现 trap FSM (R2, 留待后续 session, 等 mmu C.2 merge)
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
#include <core/context.h>
#include <core/reg.h>
#include "ip/cpu/core/payload_common.h"
#include "bundles/csr_bundles.h"
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
  std::uint32_t mstatus() const noexcept {
#ifdef CF_PLUGIN_USE_CH_MEM
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(mstatus_));
#else
    return mstatus_;
#endif
  }

#ifdef CF_PLUGIN_USE_CH_MEM
  // R1 测试访问器 (DAG signal references, sim->get_signal_value() 取 post-tick 值)
  ch_uint<32> last_csr_addr_ch() const { return last_csr_addr_; }
  ch_uint<32> last_funct12_ch()  const { return last_funct12_; }
  ch_uint<5>  last_imm5_ch()     const { return last_imm5_; }
  ch_uint<7>  last_opcode_ch()   const { return last_opcode_; }
  ch_uint<3>  last_funct3_ch()   const { return last_funct3_; }
  // R3.5 单元测试用
  ch_uint<32> last_csr_read_value_ch() const { return last_csr_read_value_; }
  ch_uint<1>  last_csr_read_valid_ch() const { return last_csr_read_valid_; }
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
    // A.2 骨架: 空。 build() 内注册 at_stage 闭包。
  }

  void build(cf::plugin::PipeBuilder& pb) override {
#ifdef CF_PLUGIN_USE_CH_MEM
    // ====================================================================
    // R1.2 + R3.3 [GREEN]: execute-LATE 闭包 — 重提取 5 位字段 + 8 选 1 读 mux
    //   闭包执行顺序: at_stage closure 在 sim tick 时调用 (不在 build 期执行)
    //   此时 INSTRUCTION 已被 fetch/decode 阶段驱动到 execute node, bits<> narrow
    //   slice 出真实 impl (AGENTS.md 架构约束 #2 + R1 commit lesson)
    // ====================================================================
    pb.at_stage("execute", cfc::Phase::LATE, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("execute").get();
      if (n) {
        using KeyType = cf::cpu::core::payload::keys<T, 32>;
        auto instr = n->operator()(KeyType::INSTRUCTION);

        // ── R1.2 重提取 5 个位字段 ──
        auto funct12_ch = bits<31, 20>(instr);
        auto imm5_ch    = bits<19, 15>(instr);
        auto funct3_ch  = bits<14, 12>(instr);
        auto opcode_ch  = bits<6,  0>(instr);
        last_csr_addr_  = funct12_ch;
        last_funct12_   = funct12_ch;
        last_imm5_      = imm5_ch;
        last_opcode_    = opcode_ch;
        last_funct3_    = funct3_ch;

        // ── R3.3 决策: 8 选 1 mux 读旧值 (select 树, 不用 if) ──
        // 编码:
        //   csr_addr == 0x300 → mstatus
        //   csr_addr == 0x305 → mtvec
        //   csr_addr == 0x340 → mscratch
        //   csr_addr == 0x341 → mepc
        //   csr_addr == 0x342 → mcause
        //   csr_addr == 0x344 → mip
        //   csr_addr == 0x304 → mie
        //   csr_addr == 0xB00 → mcycle (R3 决策: 走单独路径, read_valid=false)
        //   default 0
        auto csr_addr_eq_300 = (last_csr_addr_ == ch_uint<32>(ch_literal<0x300, 32>{}));
        auto csr_addr_eq_305 = (last_csr_addr_ == ch_uint<32>(ch_literal<0x305, 32>{}));
        auto csr_addr_eq_340 = (last_csr_addr_ == ch_uint<32>(ch_literal<0x340, 32>{}));
        auto csr_addr_eq_341 = (last_csr_addr_ == ch_uint<32>(ch_literal<0x341, 32>{}));
        auto csr_addr_eq_342 = (last_csr_addr_ == ch_uint<32>(ch_literal<0x342, 32>{}));
        auto csr_addr_eq_344 = (last_csr_addr_ == ch_uint<32>(ch_literal<0x344, 32>{}));
        auto csr_addr_eq_304 = (last_csr_addr_ == ch_uint<32>(ch_literal<0x304, 32>{}));
        auto csr_addr_eq_B00 = (last_csr_addr_ == ch_uint<32>(ch_literal<0xB00, 32>{}));

        auto read_value = select(csr_addr_eq_300, ch_uint<32>(mstatus_),
                         select(csr_addr_eq_305, ch_uint<32>(mtvec_),
                         select(csr_addr_eq_340, ch_uint<32>(mscratch_),
                         select(csr_addr_eq_341, ch_uint<32>(mepc_),
                         select(csr_addr_eq_342, ch_uint<32>(mcause_),
                         select(csr_addr_eq_344, ch_uint<32>(mip_),
                         select(csr_addr_eq_304, ch_uint<32>(mie_),
                         select(csr_addr_eq_B00, ch_uint<32>(mcycle_),
                                ch_uint<32>(ch_literal<0, 32>{})))))))));

        // 写 csr_keys::CSR_READ_VALUE / CSR_READ_VALID 到 PayloadStore
        //   - R3 决策: read_valid 仅在 csr_addr != 0xB00 时为 true (mcycle 单独路径)
        //   - csr_addr == 0xB00 时 RD_DATA 不被覆写 (per R3 决策)
        using CsrKT = cf::cpu::csr::payload::csr_keys<T>;
        n->payloads().put(CsrKT::CSR_READ_VALUE, read_value);
        // CSR_READ_VALID = !mcycle_addr  (R3.3 决策: mcycle 走单独路径, valid=false)
        // 注: csr_keys::CSR_READ_VALID 是 Payload<bool>, 但 cpphdl 5.052 bool 与
        //   ch_bool 互转歧义, 改成 Payload<ch_uint<1>> 在 R3.1 模板 (但已 commit,
        //   此处用 ch_bool() 包装 + static_cast<bool> 转换)
        const bool read_valid_bool = static_cast<bool>(
            select(csr_addr_eq_B00, ch_uint<1>(ch_literal<0, 1>{}),
                   ch_uint<1>(ch_literal<1, 1>{})));
        n->payloads().put(CsrKT::CSR_READ_VALID, read_valid_bool);

        // R3.5 单测观察口
        last_csr_read_value_ = read_value;
        last_csr_read_valid_ = ch_uint<1>(select(csr_addr_eq_B00,
                                                  ch_uint<1>(ch_literal<0, 1>{}),
                                                  ch_uint<1>(ch_literal<1, 1>{})));
      }
    });

    // ====================================================================
    // R3.4 [GREEN]: writeback-NORMAL 闭包 — 8 个 CSR ch_reg 写
    //   8 个独立 write-enable (CSR_WRITE_EN & (CSR_WRITE_ADDR == addr_X))
    //   + ch_reg <<= select(we, CSR_WRITE_VALUE, ch_reg 当前值) 模式
    //   严格不用 if(ch_bool) (per R4 [High] 风险 + AGENTS.md 架构约束 #8)
    //
    // 注: writeback stage 是 ADR-048 canonical order 中第 7 位 (在 RegFile/IntAlu
    //   之后), 当前 R1 commit 已奠定 plugin 位置, writeback 闭包未来 plugin
    //   注册时自然落此位置。本版本 CSR plugin 暂未在 cpu_factory 注册 (B.1
    //   推迟), 闭包自身可独立 verify, 不影响 5-stage 7-plugin baseline。
    // ====================================================================
    pb.at_stage("writeback", cfc::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("writeback").get();
      if (n) {
        using CsrKT = cf::cpu::csr::payload::csr_keys<T>;
        // 注: writeback-NORMAL 写 CSR_WRITE_ADDR/VALUE/EN 是 R3.3 未做 part
        //   (R3.3 只写 read path), R3.4 路径需要上游 execute-LATE 先写
        //   CSR_WRITE_* key。当前阶段 writeback 闭包不读这些 key (否则会
        //   5-stage regression 报未驱动 DAG read 错)。writeback 写 CSR
        //   寄存器走 R3.5 完整版 (execute-LATE 同时写 read + write, writeback
        //   消费), 本 commit 仅落地 read 路径。
        // has() guard 兜底, R3.5 完整版去掉
        if (n->payloads().has(CsrKT::CSR_WRITE_EN)) {
          auto write_addr  = n->operator()(CsrKT::CSR_WRITE_ADDR);
          auto write_value = n->operator()(CsrKT::CSR_WRITE_VALUE);
          // write_en 是 bool (csr_keys 定义 Payload<bool>),
          //   显式 ch_bool() 包装避开 C++17 contextual conversion 歧义
          auto write_en_ch = ch_bool(n->operator()(CsrKT::CSR_WRITE_EN));

          // 8 个独立 write-enable + select 树 (R4 强制: 不用 if(ch_bool))
          auto we_300 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x300, 32>{}));
          auto we_305 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x305, 32>{}));
          auto we_340 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x340, 32>{}));
          auto we_341 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x341, 32>{}));
          auto we_342 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x342, 32>{}));
          auto we_344 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x344, 32>{}));
          auto we_304 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x304, 32>{}));

          mstatus_  <<= select(we_300, ch_uint<32>(write_value), ch_uint<32>(mstatus_));
          mtvec_    <<= select(we_305, ch_uint<32>(write_value), ch_uint<32>(mtvec_));
          mscratch_ <<= select(we_340, ch_uint<32>(write_value), ch_uint<32>(mscratch_));
          mepc_     <<= select(we_341, ch_uint<32>(write_value), ch_uint<32>(mepc_));
          mcause_   <<= select(we_342, ch_uint<32>(write_value), ch_uint<32>(mcause_));
          mip_      <<= select(we_344, ch_uint<32>(write_value), ch_uint<32>(mip_));
          mie_      <<= select(we_304, ch_uint<32>(write_value), ch_uint<32>(mie_));
          // mcycle_ 不写 (自由计数, R2.5 详化, 暂不实装)
        }
      }
    });
#else
    // TLM 模式: 当前 R1+R3 决策仅针对 CH_MEM 通路, TLM 端 RiscvCsrPlugin 仍为 stub
    (void)pb;
#endif
  }

 private:
#ifdef CF_PLUGIN_USE_CH_MEM
  // 8 个 M-mode CSR ch_reg (R3 写通路 / R3 读通路), 初始值 0
  // 注: ch_reg<T> 模板继承 T, 内部含 next_proxy + lnodeimpl, elaboration 期固化
  //   (per AGENTS.md 架构约束 #2: 状态机例外 + ch_state_machine 强制 DSL,
  //    但 R3 写通路是 write-enable + select 树, 不算"状态机", 走 ADR-046 豁免
  //    的"组合逻辑 + ch_reg" 模式)
  ch_reg<ch_uint<32>> mstatus_ {ch::core::ch_literal<0, 32>{}, "csr_mstatus"};
  ch_reg<ch_uint<32>> mtvec_   {ch::core::ch_literal<0, 32>{}, "csr_mtvec"};
  ch_reg<ch_uint<32>> mepc_    {ch::core::ch_literal<0, 32>{}, "csr_mepc"};
  ch_reg<ch_uint<32>> mcause_  {ch::core::ch_literal<0, 32>{}, "csr_mcause"};
  ch_reg<ch_uint<32>> mie_     {ch::core::ch_literal<0, 32>{}, "csr_mie"};
  ch_reg<ch_uint<32>> mip_     {ch::core::ch_literal<0, 32>{}, "csr_mip"};
  ch_reg<ch_uint<32>> mscratch_{ch::core::ch_literal<0, 32>{}, "csr_mscratch"};
  ch_reg<ch_uint<32>> mcycle_  {ch::core::ch_literal<0, 32>{}, "csr_mcycle"};

  // R1.4 单测观察口: execute-LATE 闭包重提取的 ch DAG signals (post-tick 取值)
  ch_uint<32> last_csr_addr_{ch::core::ch_literal<0, 32>{}};
  ch_uint<32> last_funct12_{ch::core::ch_literal<0, 32>{}};
  ch_uint<5>  last_imm5_{ch::core::ch_literal<0, 5>{}};
  ch_uint<7>  last_opcode_{ch::core::ch_literal<0, 7>{}};
  ch_uint<3>  last_funct3_{ch::core::ch_literal<0, 3>{}};

  // R3.5 单测观察口: execute-LATE 闭包读 mux 输出
  ch_uint<32> last_csr_read_value_{ch::core::ch_literal<0, 32>{}};
  ch_uint<1>  last_csr_read_valid_{ch::core::ch_literal<0, 1>{}};
#else
  // TLM 模式: 8 个 CSR POD (R3 CH_MEM 通路实装后, TLM 端 RiscvCsrPlugin 仍为 stub)
  std::uint32_t mstatus_ = 0;
  std::uint32_t mtvec_ = 0;
  std::uint32_t mepc_ = 0;
  std::uint32_t mcause_ = 0;
  std::uint32_t mie_ = 0;
  std::uint32_t mip_ = 0;
  std::uint32_t mscratch_ = 0;
  std::uint32_t mcycle_ = 0;
#endif
};

}  // namespace riscv
}  // namespace arch
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_PLUGINS_CSR_CHMEM_H

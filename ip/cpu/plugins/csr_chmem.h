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
  // CH_MEM 模式: ch_reg 不能用 static_cast<uint64_t> 在 elab 期读回 (M6 教训,
  //   触发 ch_uint<N>::operator uint64_t non-constant node 错误路径, 静默返回 0)。
  //   改为返回 ch_uint<32> DAG signal reference, 调用方走 sim->get_signal_value() 取
  //   post-tick 值 (与 last_csr_*_ch 模式一致, 见 line 80-88 既有访问器)。
  // TLM 模式: 8 个 CSR POD (R3 CH_MEM 通路实装后, TLM 端 RiscvCsrPlugin 仍为 stub),
  //   ch_reg 不存在, 直接返回 POD 成员值。
  std::uint32_t mstatus() const noexcept {
#ifdef CF_PLUGIN_USE_CH_MEM
    // mstatus_ 是 ch_reg<ch_uint<32>>, 公有继承 ch_uint<32> — 返回 DAG signal ref
    //   调用方需用 sim->get_signal_value(mstatus()) 读 post-tick 值
    return 0;  // placeholder, 见 mstatus_ch() 获取 DAG ref
#else
    return mstatus_;
#endif
  }
#ifdef CF_PLUGIN_USE_CH_MEM
  // R3.5 修 F1: 与 last_csr_*_ch 模式一致, 返回 ch_signal DAG reference
  ch_uint<32> mstatus_ch() const { return mstatus_; }
#endif

#ifdef CF_PLUGIN_USE_CH_MEM
  // R1 测试访问器 (DAG signal references, sim->get_signal_value() 取 post-tick 值)
  ch_uint<32> last_csr_addr_ch() const { return last_csr_addr_; }
  ch_uint<32> last_funct12_ch()  const { return last_funct12_; }
  ch_uint<5>  last_imm5_ch()     const { return last_imm5_; }
  ch_uint<7>  last_opcode_ch()   const { return last_opcode_; }
  ch_uint<3>  last_funct3_ch()   const { return last_funct3_; }
  ch_uint<5>  last_rs1_idx_ch()  const { return last_rs1_idx_; }  // R3.5 funct3/rs1 门控用
  // R3.5 单元测试用
  ch_uint<32> last_csr_read_value_ch() const { return last_csr_read_value_; }
  ch_uint<1>  last_csr_read_valid_ch() const { return last_csr_read_valid_; }
  // R3.5 单元测试用
  ch_uint<32> last_csr_write_value_ch() const { return last_csr_write_value_; }
  ch_uint<1>  last_csr_write_en_ch()    const { return last_csr_write_en_; }
  // R2.4 单元测试用
  ch_uint<1>  last_trap_pending_ch() const { return last_trap_pending_; }
  ch_uint<32> last_trap_target_ch()  const { return last_trap_target_; }
  // R2.5 单元测试用
  ch_uint<32> last_mcycle_observed_ch() const { return last_mcycle_observed_; }
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
        auto rs1_idx_ch = bits<19, 15>(instr);  // R3.5 funct3/rs1 门控用
        last_csr_addr_  = funct12_ch;
        last_funct12_   = funct12_ch;
        last_imm5_      = imm5_ch;
        last_opcode_    = opcode_ch;
        last_funct3_    = funct3_ch;
        last_rs1_idx_   = rs1_idx_ch;

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

        // ── R3.5 [GREEN]: 写 CSR_WRITE_* payload + 4 层 funct3/rs1/imm5 门控 ──
        //   write_en = zicsr_valid      (L1: opcode==0x73)
        //     && !ecall_ebreak_mret     (L2: funct3!=000, trap 指令不写 CSR)
        //     && !csrr_read             (L3: !(funct3 in {010,011} && rs1_idx==0), csrr 读惯例)
        //     && !csrwi_read            (L4: !(funct3 in {110,111} && imm5==0), csrwi/csrri 读惯例)
        //     && !mcycle_addr           (L5: csr_addr!=0xB00, mcycle 永不被 CSRR* 写)
        //   write_value = select(funct3[2], uimm_zero_extended, rs1_val)
        //     (CSRRWI/CSRRSI/CSRRCI 写 uimm; CSRRW/CSRRS/CSRRC 写 rs1)
        //   严守 AGENTS.md §架构约束 #8: 禁 if(ch_bool), 全部 ch_uint<1> select 树
        auto funct3_eq_000 = (funct3_ch == ch::core::ch_uint<3>(ch::core::ch_literal<0b000, 3>{}));
        auto funct3_eq_010 = (funct3_ch == ch::core::ch_uint<3>(ch::core::ch_literal<0b010, 3>{}));
        auto funct3_eq_011 = (funct3_ch == ch::core::ch_uint<3>(ch::core::ch_literal<0b011, 3>{}));
        auto funct3_eq_110 = (funct3_ch == ch::core::ch_uint<3>(ch::core::ch_literal<0b110, 3>{}));
        auto funct3_eq_111 = (funct3_ch == ch::core::ch_uint<3>(ch::core::ch_literal<0b111, 3>{}));
        auto rs1_idx_eq_0  = (rs1_idx_ch == ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{}));
        auto imm5_eq_0     = (imm5_ch == ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{}));
        auto opcode_eq_73  = (opcode_ch == ch::core::ch_uint<7>(ch::core::ch_literal<0x73, 7>{}));
        // csrr_read = (funct3 in {010,011}) && (rs1_idx==0)
        auto csrr_read_ch  = (funct3_eq_010 || funct3_eq_011) && rs1_idx_eq_0;
        // csrwi_read = (funct3 in {110,111}) && (imm5==0)
        auto csrwi_read_ch = (funct3_eq_110 || funct3_eq_111) && imm5_eq_0;
        // zicsr_valid = opcode==0x73
        auto zicsr_valid_ch = opcode_eq_73;
        // 显式 ch_uint<1> 包装 (csrr_read/csrwi_read 是 ch_bool, select 第二参数需 ch_uint<1>)
        auto csrr_read_1  = select(csrr_read_ch,
                                    ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{}),
                                    ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}));
        auto csrwi_read_1 = select(csrwi_read_ch,
                                    ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{}),
                                    ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}));
        // 5 层 select 树 (从内到外: !mcycle_addr → !csrwi_read → !csrr_read → !ecall_ebreak_mret → zicsr_valid)
        //   L5 (最内): !mcycle_addr
        //     T: write_en=0 (强制 false, mcycle 永不被 CSRR* 写)
        //     F: write_en=1 (其他 CSR 可写)
        //   L4: !csrwi_read (funct3 in {110,111} && imm5==0)
        //     T: write_en=0
        //     F: write_en=L5 输出
        //   L3: !csrr_read (funct3 in {010,011} && rs1_idx==0)
        //   L2: !ecall_ebreak_mret (funct3==000)
        //   L1 (最外): zicsr_valid (opcode==0x73)
        //     T: write_en=L2 输出
        //     F: write_en=0
        const auto mcycle_layer = select(
            csr_addr_eq_B00,
            ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
            ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{}));
        const auto csrwi_layer = select(
            csrwi_read_1,
            ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
            mcycle_layer);
        const auto csrr_layer = select(
            csrr_read_1,
            ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
            csrwi_layer);
        const auto ecall_layer = select(
            funct3_eq_000,
            ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
            csrr_layer);
        const auto write_en_ch = select(
            zicsr_valid_ch,
            ecall_layer,
            ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}));
        const bool write_en_bool = static_cast<bool>(write_en_ch);
        // write_value: funct3[2] (最高位) → uimm, 否则 → rs1
        //   CSRRW=001 (funct3[2]=0) → rs1
        //   CSRRS=010 (funct3[2]=0) → rs1
        //   CSRRC=011 (funct3[2]=0) → rs1
        //   CSRRWI=101 (funct3[2]=1) → uimm
        //   CSRRSI=110 (funct3[2]=1) → uimm
        //   CSRRCI=111 (funct3[2]=1) → uimm
        auto funct3_bit2 = select(funct3_eq_010, ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
                          select(funct3_eq_011, ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
                          select(funct3_eq_110, ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{}),
                          select(funct3_eq_111, ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{}),
                                 ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{})))));
        // imm5 零扩展到 32-bit (隐式 zero-extend via ch_uint<N> 构造)
        auto uimm_32 = ch::core::ch_uint<32>(imm5_ch);
        auto rs1_val = n->operator()(KeyType::RS1);
        auto write_value_ch = select(funct3_bit2, uimm_32, rs1_val);
        n->payloads().put(CsrKT::CSR_WRITE_ADDR,  last_csr_addr_);
        n->payloads().put(CsrKT::CSR_WRITE_VALUE, write_value_ch);
        n->payloads().put(CsrKT::CSR_WRITE_EN,    write_en_bool);

        // R3.5 单测观察口 (写通路 DAG signal post-tick)
        last_csr_write_value_ = write_value_ch;
        last_csr_write_en_    = write_en_ch;

        // R3.5 单测观察口 (读通路, 已有, 不动)
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
        // R3.5 [GREEN]: has() guard 保留 for 单 plugin 测试兼容性
        //   (csr-chmem-writeback-complete Task 1.2: 实装发现单 plugin 测试路径
        //    无 cpu_factory propagate, writeback 节点缺 CSR_WRITE_* keys)
        //   - cpu_factory_chmem.h Task 1.0 路径: populate_stage + §2c propagate
        //     → writeback 节点有 CSR_WRITE_* keys → has=true → 走 R3.5 完整版
        //   - 单 plugin 测试路径 (test_csr_chmem.cpp 14 case): 无 cpu_factory propagate
        //     → writeback 节点无 CSR_WRITE_* keys → has=false → no-op 零退化
        if (n->payloads().has(CsrKT::CSR_WRITE_EN)) {
        auto write_addr  = n->operator()(CsrKT::CSR_WRITE_ADDR);
        auto write_value = n->operator()(CsrKT::CSR_WRITE_VALUE);
        // write_en 是 bool (csr_keys 定义 Payload<bool>),
        //   显式 ch_bool() 包装避开 C++17 contextual conversion 歧义
        auto write_en_ch = ch_bool(n->operator()(CsrKT::CSR_WRITE_EN));

        // 7 个独立 write-enable + select 树 (R4 强制: 不用 if(ch_bool))
        //   mcycle_ 不写 (R3 决策 + R2.5 自由计数)
        auto we_300 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x300, 32>{}));
        auto we_305 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x305, 32>{}));
        auto we_340 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x340, 32>{}));
        auto we_341 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x341, 32>{}));
        auto we_342 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x342, 32>{}));
        auto we_344 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x344, 32>{}));
        auto we_304 = write_en_ch && (write_addr == ch_uint<32>(ch_literal<0x304, 32>{}));

        // 5 个无冲突 CSR: mstatus/mtvec/mscratch/mip/mie — writeback 单写
        mstatus_  <<= select(we_300, ch_uint<32>(write_value), ch_uint<32>(mstatus_));
        mtvec_    <<= select(we_305, ch_uint<32>(write_value), ch_uint<32>(mtvec_));
        mscratch_ <<= select(we_340, ch_uint<32>(write_value), ch_uint<32>(mscratch_));
        mip_      <<= select(we_344, ch_uint<32>(write_value), ch_uint<32>(mip_));
        mie_      <<= select(we_304, ch_uint<32>(write_value), ch_uint<32>(mie_));

        // ── mcause/mepc 多 driver 仲裁 (Oracle [High] #3 + Task 0.2 smoke result: A last-wins) ──
        //   smoke 实测: writeback-NORMAL 优先级高于 branch-LATE (注册顺序导致)
        //   若直接 `mcause_ <<= select(we_342, write_value, mcause_)` + branch-LATE 也写
        //     → writeback 胜 → ECALL/EBREAK trap 写 mcause=11/3 被覆盖 (R2.4 退化)
        //   仲裁: 在 writeback 内部用 is_trap_branch 判定, trap 写优先
        //     is_trap_branch = funct12 == 0x000 (ECALL) || funct12 == 0x001 (EBREAK)
        //   注: writeback 不读 branch node PC (跨 stage), 用 last_funct12_ instance
        //     判定 (execute-LATE 重提取, branch-LATE 也读同一 instance)
        //
        //   ⚠️ F6 (2026-10-15, post-archive debt): 未来 MmuExceptionHandler CH_MEM 实装
        //     (mmu-chmem-pipeline-integration Phase D, owner change) 会成为 **第三 driver** —
        //     MmuExceptionHandlerPlugin::handle_page_fault() 写 mcause_/mepc_/mtval_ 处理
        //     page fault / access fault, 与 branch-LATE trap 写 (ECALL/EBREAK) +
        //     writeback-NORMAL CSR 写 (CSRRW mcause/mepc) 三方竞争同 cycle.
        //     届时仲裁需扩展为 3 路:
        //       is_trap_branch || mmufault_active → mcause = trap 或 mmufault 值
        //     实装 owner: mmu-chmem-pipeline-integration Phase D (mmu_exception_handler_chmem.h)
        //     本 change 仅登记扩展点, 不实装 (per mmu-chmem spec v0.11.0 owner 决策).
        //   触发条件: 当 mmu-chmem-pipeline-integration 实装 MmuExceptionHandlerPlugin CH_MEM 时
        //     必须先 re-evaluate 此 select 树, 否则 mmu fault 写 mcause=exception code 会
        //     被 CSRRW mcause 写 (last-wins 选 writeback) 覆盖.
        auto funct12_eq_000 = (last_funct12_ == ch_uint<32>(ch_literal<0x000, 32>{}));
        auto funct12_eq_001 = (last_funct12_ == ch_uint<32>(ch_literal<0x001, 32>{}));
        auto is_trap_branch = funct12_eq_000 || funct12_eq_001;
        // branch-LATE trap 写值: ECALL→11, EBREAK→3 (mcause); mepc 需 PC, writeback 不可访问
        //   → mepc 仲裁从简: 1-layer select (trap → 仍走 mcause_ 当前值由 branch 写)
        //     *实际上* branch-LATE 在自己闭包写 mepc=PC+4 (有 has(KeyType::PC) guard);
        //     *真实 5-stage* branch node 有 PC (cpu_factory_chmem propagate), 但 Task 1.2 实施时
        //     为避免破坏单 stage 测试, branch-LATE 的 mepc 写保留原 has(PC) guard
        //   **简化仲裁**: writeback 写 mcause 时 is_trap→11/3 强制优先; mepc 让 branch 写
        //     (writeback 也写 mepc 但当 is_trap=true 时 select is_trap → mcause_ 当前 trap 值)
        //     *正确性*: 5-stage 流水线下 ECALL/EBREAK 在 branch 阶段 active 时, 同 cycle
        //     writeback 阶段可能也在 active (5-stage 不同指令), 但 ECALL trap 写 mcause
        //     必须生效; 用 is_trap 判定 → mcause = trap 值
        auto mcause_branch = select(funct12_eq_000, ch_uint<32>(ch_literal<11, 32>{}),
                             select(funct12_eq_001, ch_uint<32>(ch_literal<3, 32>{}),
                                    ch_uint<32>(mcause_)));
        auto mcause_wb     = select(we_342, ch_uint<32>(write_value), ch_uint<32>(mcause_));
        // 仲裁: is_trap_branch=true → mcause_branch (trap 11/3); else → mcause_wb (CSR 写)
        auto is_trap_1 = select(is_trap_branch,
                                 ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{}),
                                 ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}));
        auto mcause_final = select(is_trap_1,
                                   ch_uint<32>(mcause_branch),
                                   ch_uint<32>(mcause_wb));
        mcause_ <<= mcause_final;

        // mepc 仲裁: writeback 写 mepc 时, 若 is_trap_branch → mcause_branch 值 (不变 mcause);
        //   mepc 走相同仲裁模式 (trap 时保持 mcause_, CSR 写时用 write_value)
        //   **注意**: mepc trap 写依赖 PC+4, branch-LATE 自己写 mepc; writeback 写 mepc 仅
        //     来自 CSRRW mepc. 仲裁: is_trap → mcause_ (保留 trap 时 mcause 不被 CSR 写覆写);
        //   mepc 走相同仲裁模式
        //   **简化**: mepc 仲裁逻辑同 mcause (维持单 driver 语义, 走仲裁 select)
        auto mepc_branch = ch_uint<32>(mepc_);  // branch-LATE 已在 has(PC) 时写 mepc
        auto mepc_wb     = select(we_341, ch_uint<32>(write_value), ch_uint<32>(mepc_));
        auto mepc_final  = select(is_trap_1,
                                  ch_uint<32>(mepc_branch),
                                  ch_uint<32>(mepc_wb));
        mepc_ <<= mepc_final;
        }  // R3.5 has() guard 闭
      }
    });

    // ====================================================================
    // R2.2 [GREEN]: branch-LATE 闭包 — 写 TRAP_PENDING / TRAP_TARGET
    //   + ECALL/EBREAK 时写 mcause + mepc
    //
    // 设计要点 (per design.md §R2):
    //   - trap 优先级 trap > branch > pc+4 (R2 优先级固化, 与 wave5-bp-btb 协调)
    //   - ECALL (funct12=0x000) → mcause=11, mepc=PC+4, trap_target=mtvec
    //   - EBREAK (funct12=0x001) → mcause=3, mepc=PC+4, trap_target=mtvec
    //   - MRET (funct12=0x302) → trap_target=mepc (不写 mcause)
    //   - 其他指令 → trap_pending=false, trap_target=0 (no-op)
    //
    // 时序: at_stage("branch", LATE) — elaboration 注册顺序晚于 IBus
    //   (CSR plugin 在 cpu_factory 位置 5, 晚于 IBus 位置 3), 所以 CSR
    //   branch-LATE 闭包执行时 last_funct12_/last_opcode_ 已被 execute-LATE
    //   闭包写过 (execute stage 在 branch stage 之前)。
    //
    // 注: 本闭包不消费 PC node (PC+4 直接从 branch node KT::PC 读);
    //   mcause/mepc 写不依赖 mmu Phase B 路径 (per R2.3 决策 + tasks.md R2)
    // ====================================================================
    pb.at_stage("branch", cfc::Phase::LATE, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("branch").get();
      if (n) {
        using KeyType = cf::cpu::core::payload::keys<T, 32>;
        using CsrKT = cf::cpu::csr::payload::csr_keys<T>;

        // funct12 来自 execute-LATE 闭包重提取的 DAG signal (R1.2 决策:
        //   execute-LATE 闭包内 bits<31,20> 自提取, instance ch_signal)
        auto funct12_ch = last_funct12_;
        auto funct12_eq_000 = (funct12_ch == ch_uint<32>(ch_literal<0x000, 32>{}));
        auto funct12_eq_001 = (funct12_ch == ch_uint<32>(ch_literal<0x001, 32>{}));
        auto funct12_eq_302 = (funct12_ch == ch_uint<32>(ch_literal<0x302, 32>{}));
        // trap_pending = (funct12==0x000) | (funct12==0x001) | (funct12==0x302)
        //   select 树 (R4 强制: 不用 if(ch_bool))
        auto trap_pending = select(funct12_eq_000, ch_uint<1>(ch_literal<1, 1>{}),
                            select(funct12_eq_001, ch_uint<1>(ch_literal<1, 1>{}),
                            select(funct12_eq_302, ch_uint<1>(ch_literal<1, 1>{}),
                                   ch_uint<1>(ch_literal<0, 1>{}))));

        // trap_target select 树:
        //   MRET (0x302) → mepc_; ECALL/EBREAK (0x000/0x001) → mtvec_; else 0
        // 注: mepc_/mtvec_ 是 ch_reg<ch_uint<32>>, ch_reg 公有继承 ch_uint<32>,
        //   select 返回类型由 true/false 决定 — 必须 ch_uint<32> 同类型
        //   隐式上行 ch_reg<ch_uint<32>> → ch_uint<32> base slice
        auto trap_target = select(funct12_eq_302, ch_uint<32>(mepc_),
                           select(funct12_eq_000, ch_uint<32>(mtvec_),
                           select(funct12_eq_001, ch_uint<32>(mtvec_),
                                  ch_uint<32>(ch_literal<0, 32>{}))));

        // 写 TRAP_PENDING / TRAP_TARGET 到 PayloadStore
        n->payloads().put(CsrKT::TRAP_PENDING,
                          static_cast<bool>(trap_pending));
        n->payloads().put(CsrKT::TRAP_TARGET, trap_target);

        // R2.4 单测观察口 (trap 路径 DAG signal 始终写, 不依赖 PC 驱动)
        last_trap_pending_ = trap_pending;
        last_trap_target_  = trap_target;

        // ECALL/EBREAK 时写 mcause + mepc (mepc = PC+4)
        // has() guard: PC 节点在单 stage 测试可能未驱动 (避免 SEGV)
        if (n->payloads().has(KeyType::PC)) {
          // PC 从 branch node 读 — branch stage 保留 PC pipeline 节点
          auto pc_val = n->operator()(KeyType::PC);
          auto pc_plus_4 = pc_val + ch_uint<32>(ch_literal<4, 32>{});
          mcause_ <<= select(funct12_eq_000, ch_uint<32>(ch_literal<11, 32>{}),
                      select(funct12_eq_001, ch_uint<32>(ch_literal<3, 32>{}),
                              ch_uint<32>(mcause_)));
          mepc_ <<= select(funct12_eq_000, ch_uint<32>(pc_plus_4),
                    select(funct12_eq_001, ch_uint<32>(pc_plus_4),
                            ch_uint<32>(mepc_)));
        }
      }
    });

    // ====================================================================
    // R2.5 [GREEN]: fetch-NORMAL 闭包 — mcycle 自由计数
    //   每 cycle mcycle_ <<= mcycle_ + 1 (carry 自增, free-running)
    //   R2.5 决策 (per design.md Q1 收口): mcycle 必须 free-running, 与
    //   CSR writeback 路径解耦, 5-stage 流水线每 cycle +1
    //
    // 时序: at_stage("fetch", NORMAL) — fetch stage NORMAL phase 每 cycle 跑
    //   Phase enum 仅 EARLY/NORMAL/LATE (无 ALWAYS, per pipe_builder.h:57-60)
    //   5-stage pipeline 默认每 cycle 触发 fetch NORMAL
    //   has() guard: 单 stage 测试无 fetch node → no-op 零退化
    // ====================================================================
    pb.at_stage("fetch", cfc::Phase::NORMAL, [this, &pb]() {
      auto* n = pb.node_of_logic_stage("fetch").get();
      if (n) {
        // mcycle_ 是 ch_reg<ch_uint<32>>, 公有继承 ch_uint<32>
        // mcycle_ + 1 返回 ch_uint<32> (operator+ on ch_uint)
        mcycle_ <<= mcycle_ + ch_uint<32>(ch_literal<1, 32>{});
        // R2.5 单测观察口: 取 mcycle_ 当前值 (post-tick)
        last_mcycle_observed_ = mcycle_;
      }
    });
#else
    // TLM 模式: 当前 R1+R3+R2 决策仅针对 CH_MEM 通路, TLM 端 RiscvCsrPlugin 仍为 stub
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
  ch_uint<5>  last_rs1_idx_{ch::core::ch_literal<0, 5>{}};  // R3.5 funct3/rs1 门控用

  // R3.5 单测观察口: execute-LATE 闭包读 mux 输出
  ch_uint<32> last_csr_read_value_{ch::core::ch_literal<0, 32>{}};
  ch_uint<1>  last_csr_read_valid_{ch::core::ch_literal<0, 1>{}};

  // R3.5 单测观察口: execute-LATE 闭包写通路输出
  ch_uint<32> last_csr_write_value_{ch::core::ch_literal<0, 32>{}};
  ch_uint<1>  last_csr_write_en_   {ch::core::ch_literal<0, 1>{}};

  // R2.4 单测观察口: branch-LATE 闭包 trap 路径输出
  ch_uint<1>  last_trap_pending_{ch::core::ch_literal<0, 1>{}};
  ch_uint<32> last_trap_target_ {ch::core::ch_literal<0, 32>{}};

  // R2.5 mcycle 自由计数观察口 (读 post-tick DAG value, 应近似 cycle 数)
  ch_uint<32> last_mcycle_observed_{ch::core::ch_literal<0, 32>{}};
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

// ip/cpu/plugins/ibus_chmem.h
// Phase 6d 6d.3: IBusPlugin (CH_MEM) — instruction fetch + PC + branch feedback

#ifdef CF_PLUGIN_USE_CH_MEM
#ifndef CF_IP_CPU_PLUGINS_IBUS_CHMEM_H
#define CF_IP_CPU_PLUGINS_IBUS_CHMEM_H

#include <cstdint>
#include <memory>
#include <vector>

#include <ch.hpp>
#include <core/bool.h>
#include <core/context.h>
#include <core/mem.h>
#include <core/operators.h>
#include <core/reg.h>
#include <core/uint.h>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/plugins/branch_predictor_chmem.h"
#include "ip/cpu/arch/riscv/payload_riscv.h"
#include "ip/cpu/plugins/branch_chmem.h"
#include "ip/mmu/tlm/mmu_keys.h"  // mmu-chmem-pipeline-integration Phase 2 Task 2.1 (C2): sv32 消费

using namespace cf::plugin;
using namespace ch;
using namespace ch::core;

namespace cf { namespace cpu { namespace plugins {

template <typename T = std::uint32_t>
class IBusPlugin : public PluginBase {
  static constexpr std::size_t kXlenBits = 32;
  static constexpr std::size_t kMemWords = 16 * 1024;
 public:
  // Phase 6d.4: branch flush signal — when the PREVIOUS cycle's branch was
  // taken, the currently-fetched fall-through instruction is bogus and its
  // register writes / store / branch decision must be suppressed.
  // ★ `inline static` ⇒ **全程序唯一**（类作用域静态成员 = vague linkage,
  //   链接期合并为一份）; `PayloadStore` 按 key 的**地址**索引 ⇒ 唯一性是硬要求。
  //   ★ 对照: **命名空间作用域**的 `static inline` key 才是 per-TU 副本 ⇒
  //   跨 TU 断链（见 `branch_predictor_chmem.h` 的 doctrine 注释）。
  inline static Payload<ch::core::ch_bool> FLUSH{"cpu.flush"};
  IBusPlugin() = default;
  ~IBusPlugin() override = default;
  IBusPlugin(const IBusPlugin&) = delete;
  IBusPlugin& operator=(const IBusPlugin&) = delete;
  void preload_words(const std::vector<uint32_t>& words) { preload_words_ = words; }
  // Phase 6d.4: merge words into preload buffer at given word offset
  // (offset = (p_vaddr - 0x80000000) / 4 for ELF PT_LOAD segments)
  void preload_segment(std::size_t offset_words, const std::vector<uint32_t>& words) {
    if (preload_words_.size() < offset_words + words.size()) {
      preload_words_.resize(offset_words + words.size(), 0);
    }
    for (std::size_t i = 0; i < words.size(); ++i) {
      preload_words_[offset_words + i] = words[i];
    }
  }
  // Phase 6d.4: override PC start (vendored ELFs enter at 0x80000000)
  void set_initial_pc(ch::core::ch_uint<32> pc) { initial_pc_ = pc; }
  void setup(PipeBuilder& pb) override {
    // Phase 6d.4: two-part fetch.
    //  - fetch NORMAL: read pc_reg, issue aread, publish INSTRUCTION/PC(lag)
    //    BEFORE decode consumes them (stage order: fetch < decode).
    //  - branch LATE: pc_reg <<= select(current_branch_taken, target, pc+4).
    //    The aread's 1-cycle latency means the instruction decoded at cycle N
    //    came from pc_reg(N-1); its branch redirects pc_reg(N+1). The
    //    fall-through occupies lag(N+1) (flushed via FLUSH) and the target
    //    arrives at lag(N+2). Using the CURRENT branch (not a stale buffer)
    //    keeps the redirect accurate — no stale-target re-entry.
    pb.at_stage("fetch",  Phase::NORMAL, [this, &pb]{ this->fetch_instr(pb); });
    pb.at_stage("branch", Phase::LATE,   [this, &pb]{ this->update_pc(pb); });
  }
  void build(PipeBuilder&) override {}

 private:
  std::vector<uint32_t> preload_words_;
  ch::core::ch_uint<32> initial_pc_ = ch::core::ch_uint<32>(ch::core::ch_literal<0, 1>{});
  std::unique_ptr<ch::core::ch_mem<ch::core::ch_uint<32>, kMemWords>> imem_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<32>>> pc_reg_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<32>>> pc_lag_reg_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_bool>>    br_taken_buf_;
  // ★ chmem-bp-btb-gshare-poc Task 4: FLUSH 事件数计数器
  //   FLUSH 是 DUT 内部 payload (host 不可见) ⇒ 计数必须在此处 (ip/) 产生。
  //   承载为 ch_reg<ch_uint<32>> ⇒ CppHDL codegen 出 Verilog 信号 `flush_count`
  //   ⇒ host 经 `--public-flat-rw` 读 `top__DOT__flush_count`。
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<32>>> flush_count_;

  // ★ 读延迟（★ 2026-10-25 更正 —— 原注释说「aread/sread 都是 1 拍」，
  //   与上游 `c27543d` 之后的实际行为**矛盾**）：
  //   - `sread` = **1 拍**（本插件的 imem 读用它，见 fetch_instr）
  //   - `aread` = **0 拍**（上游 c27543d 起 Verilog 侧是组合 `assign`）
  //   ⇒ fetch.PC 必须暴露**上一拍**的 PC（pc_lag）以与取到的指令对齐
  //     （decode/branch 一起读 (pc_lag, instr)）。
  //   ★ 推论: BP 的 BTB 查表**可以**用 `aread`（0 拍）⇒ 预测结果同拍可被
  //     branch LATE 的 PC-mux 消费 ⇒ **提前一拍**（见架构简报 v2.2 §2.1）。
  ch::core::ch_reg<ch::core::ch_uint<32>>& get_pc_lag() {
    if (!pc_lag_reg_)
      pc_lag_reg_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<32>>>(
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 1>{}), "pc_lag");
    return *pc_lag_reg_;
  }

  ch::core::ch_mem<ch::core::ch_uint<32>, kMemWords>& get_imem() {
    if (!imem_) {
      std::vector<uint32_t> full(kMemWords, 0);
      std::size_t n = preload_words_.size() < kMemWords ? preload_words_.size() : kMemWords;
      for (std::size_t i = 0; i < n; ++i) full[i] = preload_words_[i];
      imem_ = std::make_unique<ch::core::ch_mem<ch::core::ch_uint<32>, kMemWords>>(full, "imem");
    }
    return *imem_;
  }

  ch::core::ch_reg<ch::core::ch_uint<32>>& get_pc() {
    if (!pc_reg_)
      pc_reg_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<32>>>(
          initial_pc_, "pc");
    return *pc_reg_;
  }

  ch::core::ch_reg<ch::core::ch_uint<32>>& get_flush_count() {
    if (!flush_count_)
      flush_count_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<32>>>(
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}), "flush_count");
    return *flush_count_;
  }

  ch::core::ch_reg<ch::core::ch_bool>& get_br_taken_buf() {
    if (!br_taken_buf_)
      br_taken_buf_ = std::make_unique<ch::core::ch_reg<ch::core::ch_bool>>(
          ch::core::ch_bool(false), "br_taken_buf");
    return *br_taken_buf_;
  }

  void fetch_instr(PipeBuilder& pb) {
    using KT = cf::cpu::core::payload::keys<T, kXlenBits>;
    using MmuKeys = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;
    auto* n = pb.node_of_logic_stage("fetch").get();
    if (!n) return;
    auto& pc_reg = get_pc();
    auto& pc_lag = get_pc_lag();
    auto& imem   = get_imem();
    ch::core::ch_uint<32> pc_val(pc_reg);
    ch::core::ch_uint<32> pc_lag_val(pc_lag);
    auto br_taken_buf = ch::core::ch_bool(get_br_taken_buf());

    // mmu-chmem-pipeline-integration Phase 2 Task 2.1 (C2): sv32 消费
    //   统一 sel 表达式 (ADR-040 v2.0 §4 D4 主逻辑包裹, Phase C 决策):
    //     word_addr = select(PADDR_VALID, PADDR, MMU_VADDR)
    //   Bare 路径 0 额外 latency (D2-A 保护): PADDR_VALID=false (MMU 未注册) 时
    //     sel 回退到 MMU_VADDR (由 tlb_lookup_ifetch 节点 NORMAL phase 写 vaddr;
    //     当前 phase B 未注册 → 默认构造 ch_uint<32>(0) cell = 无害 placeholder)
    //   Sv32 路径 (Phase B 落地后, Sprint 2-3): PADDR_VALID=true 时 sel 选 PADDR,
    //     实现 vaddr → paddr 真翻译
    //   闭包外层 "tlb_lookup_ifetch" 节点不存在时 (Bare) → PADDR_VALID=false → 走 vaddr
    auto* tlb_n = pb.node_of_logic_stage("tlb_lookup_ifetch").get();
    ch::core::ch_uint<1>  paddr_valid = ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{});
    ch::core::ch_uint<32> paddr      = ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{});
    ch::core::ch_uint<32> mmu_vaddr  = pc_val;  // Bare default = pc_val
    if (tlb_n && tlb_n->payloads().has(MmuKeys::PADDR_VALID)) {
      // ⚠ 读**节点**而非常量 (chmem-mmu-keys-modality, 2026-10-22):
      //    MMU payload key 已模式化 ⇒ cell 里装的是 ch 代理 (ch_uint<N>)。
      //    旧写法 `ch_bool(cell)` 会把 DAG 节点**折成 elaboration 期常量**
      //    ⇒ sv32 翻译退化为常量路径 (测试全绿但翻译从未生效)。
      //    `ch_uint<32>(ch_uint<64>)` 走跨宽构造的截断路径
      //    (core/uint.h:104-121, M>N ⇒ bits<N-1,0>) —— sv32 下 vaddr/paddr ≤32bit。
      paddr_valid = tlb_n->operator()(MmuKeys::PADDR_VALID);
      paddr       = ch::core::ch_uint<32>(tlb_n->operator()(MmuKeys::PADDR));
      mmu_vaddr   = ch::core::ch_uint<32>(tlb_n->operator()(MmuKeys::MMU_VADDR));
    }
    auto fetch_addr = select(paddr_valid, paddr, mmu_vaddr);
    auto word_addr = bits<15, 2>(fetch_addr);
    // Phase 6d.7 B' follow-up: use sread (not aread) here so imem keeps
    // its 1-cycle read latency. CppHDL commit 250198a made aread emit a
    // combinational `assign` in Verilog, which would break pc_lag
    // alignment (pc_lag is a 1-cycle ch_reg, but aread would give
    // INSTRUCTION in the same cycle, no longer matching pc_lag).
    // dmem uses aread for 0-cycle reads; imem stays on sread.
    auto rp = imem.sread(word_addr, "ifetch");
    ch::core::ch_uint<32> instr(rp.impl());
    n->operator()(KT::INSTRUCTION) = instr;
    n->operator()(KT::PC)          = pc_lag_val;
    n->operator()(FLUSH)           = br_taken_buf;
    // ── BP 的取指地址 (chmem-bp-btb-gshare-poc Task 1) ──────────────────
    // ★ KT::PC 是 **pc_lag**（上一拍地址）⇒ BP 预测需要 **pc_reg 本拍值**
    //   （BP 预测的是「本拍正在取的那条分支」⇒ 提前一拍）。
    // ★ 写入 BP_PC 供 BranchPredictorPluginChmem 在同一拍查表。
    // ★ 本 PoC **不改变任何行为**（BP 的输出尚未被 PC-mux 消费）。
    n->operator()(cf::cpu::plugins::BP_PC) = cf::plugin::uint_t<32>(pc_val);
    pc_lag <<= pc_val;
  }

  void update_pc(PipeBuilder& pb) {
    using RvKey = cf::cpu::arch::riscv::payload_keys_riscv<T>;
    using BrPlugin = cf::cpu::plugins::BranchPlugin<T>;
    auto& pc_reg = get_pc();
    ch::core::ch_uint<32> pc_val(pc_reg);
    auto* brn = pb.node_of_logic_stage("branch").get();
    auto br_taken = brn ? brn->operator()(BrPlugin::get_branch_taken_key())
                        : ch::core::ch_bool(false);
    auto br_target = brn ? brn->operator()(RvKey::BRANCH_TARGET)
                         : ch::core::ch_uint<32>(ch::core::ch_literal<0, 1>{});
    auto pc_plus_4 = pc_val + ch::core::ch_uint<32>(ch::core::ch_literal<4, 32>{});
    auto next_pc   = select(br_taken, br_target, pc_plus_4);
    pc_reg <<= next_pc;
    get_br_taken_buf() <<= br_taken;

    // ── FLUSH 事件计数 (chmem-bp-btb-gshare-poc Task 4) ────────────────
    // ★ FLUSH 是 DUT 内部 payload ⇒ host 不可见 ⇒ 在此累加。
    // ★ 计数的是**本拍生效的 flush**（即 fetch 阶段发布的 FLUSH 值）——
    //   它是 `br_taken_buf`（上拍 taken）⇒ 与「错路指令被抑制」同拍。
    // ★ ch_reg 是 1 拍延迟 ⇒ host 读到的是「上一拍末」的累计值（差 1 拍，可接受）。
    {
      auto* fch = pb.node_of_logic_stage("fetch").get();
      auto flush_now = fch ? fch->operator()(FLUSH) : ch::core::ch_bool(false);
      auto& fc = get_flush_count();
      ch::core::ch_uint<32> fc_val(fc);
      auto inc = select(flush_now, ch::core::ch_uint<32>(ch::core::ch_literal<1, 32>{}),
                        ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}));
      fc <<= fc_val + inc;
    }
  }
};

}}}  // namespace cf::cpu::plugins
#endif  // CF_IP_CPU_PLUGINS_IBUS_CHMEM_H
#endif  // CF_PLUGIN_USE_CH_MEM
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
#include "ip/cpu/arch/riscv/payload_riscv.h"
#include "ip/cpu/plugins/branch_chmem.h"

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
  static inline Payload<ch::core::ch_bool> FLUSH{"cpu.flush"};
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

  // ch_mem aread/sread both have 1-cycle latency in the simulator: the data
  // proxy at cycle N holds imem[address presented at cycle N-1]. fetch.PC
  // must therefore expose the PREVIOUS cycle's PC (pc_lag) so it stays
  // aligned with the fetched instruction; consumers (decode/branch) read
  // (pc_lag, instr) together.
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

  ch::core::ch_reg<ch::core::ch_bool>& get_br_taken_buf() {
    if (!br_taken_buf_)
      br_taken_buf_ = std::make_unique<ch::core::ch_reg<ch::core::ch_bool>>(
          ch::core::ch_bool(false), "br_taken_buf");
    return *br_taken_buf_;
  }

  void fetch_instr(PipeBuilder& pb) {
    using KT = cf::cpu::core::payload::keys<T, kXlenBits>;
    auto* n = pb.node_of_logic_stage("fetch").get();
    if (!n) return;
    auto& pc_reg = get_pc();
    auto& pc_lag = get_pc_lag();
    auto& imem   = get_imem();
    ch::core::ch_uint<32> pc_val(pc_reg);
    ch::core::ch_uint<32> pc_lag_val(pc_lag);
    auto br_taken_buf = ch::core::ch_bool(get_br_taken_buf());
    auto word_addr = bits<15, 2>(pc_val);
    auto rp = imem.aread(word_addr, "ifetch");
    ch::core::ch_uint<32> instr(rp.impl());
    n->operator()(KT::INSTRUCTION) = instr;
    n->operator()(KT::PC)          = pc_lag_val;
    n->operator()(FLUSH)           = br_taken_buf;
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
  }
};

}}}  // namespace cf::cpu::plugins
#endif  // CF_IP_CPU_PLUGINS_IBUS_CHMEM_H
#endif  // CF_PLUGIN_USE_CH_MEM
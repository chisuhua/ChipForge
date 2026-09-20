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
  IBusPlugin() = default;
  ~IBusPlugin() override = default;
  IBusPlugin(const IBusPlugin&) = delete;
  IBusPlugin& operator=(const IBusPlugin&) = delete;
  void preload_words(const std::vector<uint32_t>& words) { preload_words_ = words; }
  void setup(PipeBuilder& pb) override {
    pb.at_stage("fetch",  Phase::NORMAL, [this, &pb]{ this->fetch_instr(pb); });
    pb.at_stage("branch", Phase::LATE,   [this, &pb]{ this->capture_branch(pb); });
  }
  void build(PipeBuilder&) override {}

 private:
  std::vector<uint32_t> preload_words_;
  std::unique_ptr<ch::core::ch_mem<ch::core::ch_uint<32>, kMemWords>> imem_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<32>>> pc_reg_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_bool>>    br_taken_buf_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<32>>> br_target_buf_;

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
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 1>{}), "pc");
    return *pc_reg_;
  }

  ch::core::ch_reg<ch::core::ch_bool>& get_br_taken_buf() {
    if (!br_taken_buf_)
      br_taken_buf_ = std::make_unique<ch::core::ch_reg<ch::core::ch_bool>>(
          ch::core::ch_bool(false), "br_taken_buf");
    return *br_taken_buf_;
  }

  ch::core::ch_reg<ch::core::ch_uint<32>>& get_br_target_buf() {
    if (!br_target_buf_)
      br_target_buf_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<32>>>(
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 1>{}), "br_target_buf");
    return *br_target_buf_;
  }

  void fetch_instr(PipeBuilder& pb) {
    using KT = cf::cpu::core::payload::keys<T, kXlenBits>;
    auto* n = pb.node_of_logic_stage("fetch").get();
    if (!n) return;
    auto& pc_reg = get_pc();
    auto& imem   = get_imem();
    ch::core::ch_uint<32> pc_val(pc_reg);
    auto br_taken  = ch::core::ch_bool(get_br_taken_buf());
    auto br_target = ch::core::ch_uint<32>(get_br_target_buf());
    auto word_addr = bits<15, 2>(pc_val);
    auto rp = imem.sread(word_addr, ch::core::ch_bool(true), "ifetch");
    ch::core::ch_uint<32> instr(rp.impl());
    n->operator()(KT::INSTRUCTION) = instr;
    n->operator()(KT::PC)          = pc_val;
    auto pc_plus_4 = pc_val + ch::core::ch_uint<32>(ch::core::ch_literal<4, 32>{});
    auto next_pc   = select(br_taken, br_target, pc_plus_4);
    pc_reg <<= next_pc;
  }

  void capture_branch(PipeBuilder& pb) {
    using RvKey = cf::cpu::arch::riscv::payload_keys_riscv<T>;
    using BrPlugin = cf::cpu::plugins::BranchPlugin<T>;
    auto* brn = pb.node_of_logic_stage("branch").get();
    if (!brn) return;
    auto br_taken  = brn->operator()(BrPlugin::get_branch_taken_key());
    auto br_target = brn->operator()(RvKey::BRANCH_TARGET);
    get_br_taken_buf()  <<= br_taken;
    get_br_target_buf() <<= br_target;
  }
};

}}}  // namespace cf::cpu::plugins
#endif  // CF_IP_CPU_PLUGINS_IBUS_CHMEM_H
#endif  // CF_PLUGIN_USE_CH_MEM
// ip/cpu/plugins/dmem_chmem.h
// Phase 6d 6d.3: DMemPlugin (CH_MEM) — data memory + load/store path

#ifdef CF_PLUGIN_USE_CH_MEM
#ifndef CF_IP_CPU_PLUGINS_DMEM_CHMEM_H
#define CF_IP_CPU_PLUGINS_DMEM_CHMEM_H

#include <cstdint>
#include <memory>
#include <vector>

#include <ch.hpp>
#include <core/bool.h>
#include <core/context.h>
#include <core/mem.h>
#include <core/operators.h>
#include <core/uint.h>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/plugins/ibus_chmem.h"

using namespace cf::plugin;
using namespace ch;
using namespace ch::core;

namespace cf { namespace cpu { namespace plugins {

template <typename T = std::uint32_t>
class DMemPlugin : public PluginBase {
  static constexpr std::size_t kXlenBits = 32;
  static constexpr std::size_t kMemWords = 16 * 1024;
 public:
  static inline Payload<ch::core::ch_uint<kXlenBits>> LOAD_DATA{"cpu.dmem_load_data"};

  DMemPlugin() = default;
  ~DMemPlugin() override = default;
  DMemPlugin(const DMemPlugin&) = delete;
  DMemPlugin& operator=(const DMemPlugin&) = delete;

  void preload(const std::vector<uint8_t>& bytes) { preload_bytes_ = bytes; }
  void preload_words(const std::vector<uint32_t>& words) { preload_words_ = words; }
  // Phase 6d.4: merge words into preload buffer at given word offset
  void preload_segment(std::size_t offset_words, const std::vector<uint32_t>& words) {
    if (preload_words_.size() < offset_words + words.size()) {
      preload_words_.resize(offset_words + words.size(), 0);
    }
    for (std::size_t i = 0; i < words.size(); ++i) {
      preload_words_[offset_words + i] = words[i];
    }
  }
  // Phase 6d.4: add always-on async read port at fixed word offset
  // so tests can probe dmem[tohost_word] from outside the simulator.
  void set_tohost_probe_word(std::size_t word) {
    tohost_probe_word_ = word;
    tohost_probe_enabled_ = true;
  }

  void setup(PipeBuilder& pb) override {
    pb.at_stage("memory", Phase::NORMAL, [this, &pb]{ this->mem_access(pb); });
  }
  void build(PipeBuilder&) override {}

 private:
  std::vector<uint8_t>  preload_bytes_;
  std::vector<uint32_t> preload_words_;
  std::size_t tohost_probe_word_ = 0;
  bool tohost_probe_enabled_ = false;
  std::unique_ptr<ch::core::ch_mem<ch::core::ch_uint<32>, kMemWords>> mem_;
  std::unique_ptr<ch::core::ch_mem<ch::core::ch_uint<32>, kMemWords>::read_port> tohost_probe_port_;

  ch::core::ch_mem<ch::core::ch_uint<32>, kMemWords>& get_mem() {
    if (!mem_) {
      std::vector<uint32_t> init(kMemWords, 0);
      // Prefer word preload over byte preload
      if (!preload_words_.empty()) {
        std::size_t n = preload_words_.size() < kMemWords ? preload_words_.size() : kMemWords;
        for (std::size_t i = 0; i < n; ++i) init[i] = preload_words_[i];
      } else if (!preload_bytes_.empty()) {
        for (std::size_t i = 0; i + 4 <= preload_bytes_.size(); i += 4) {
          uint32_t w = (static_cast<uint32_t>(preload_bytes_[i])      ) |
                       (static_cast<uint32_t>(preload_bytes_[i+1]) << 8 ) |
                       (static_cast<uint32_t>(preload_bytes_[i+2]) << 16) |
                       (static_cast<uint32_t>(preload_bytes_[i+3]) << 24);
          if (i/4 < kMemWords) init[i/4] = w;
        }
      }
      mem_ = std::make_unique<ch::core::ch_mem<ch::core::ch_uint<32>, kMemWords>>(init, "dmem");
    }
    return *mem_;
  }

  void mem_access(PipeBuilder& pb) {
    using KT = cf::cpu::core::payload::keys<T, kXlenBits>;
    auto* n = pb.node_of_logic_stage("memory").get();
    if (!n) return;
    auto& mem = get_mem();

    auto inst = n->operator()(KT::INSTRUCTION);
    auto opcode = bits<6, 0>(inst);
    auto funct3 = bits<14, 12>(inst);
    auto op_store = ch::core::ch_uint<7>(ch::core::ch_literal<0x23, 7>{});
    auto op_load  = ch::core::ch_uint<7>(ch::core::ch_literal<0x03, 7>{});
    auto f3_010   = ch::core::ch_uint<3>(ch::core::ch_literal<2, 3>{});

    auto is_store = ch::core::ch_bool(opcode == op_store);
    auto is_load  = ch::core::ch_bool(opcode == op_load);
    auto is_sw = is_store && ch::core::ch_bool(funct3 == f3_010);
    auto is_lw = is_load  && ch::core::ch_bool(funct3 == f3_010);

    // Phase 6d.4: branch flush — suppress stores from the wrong-path
    // fall-through instruction fetched right after a taken branch.
    auto* fch = pb.node_of_logic_stage("fetch").get();
    auto flush = fch ? fch->operator()(IBusPlugin<T>::FLUSH)
                     : ch::core::ch_bool(false);
    is_sw = is_sw && !flush;

    auto byte_addr = n->operator()(KT::RESULT);
    auto word_addr = bits<15, 2>(byte_addr);

    auto wdata = n->operator()(KT::RS2);
    mem.write(word_addr, wdata, is_sw, "store_write");

    auto rp = mem.sread(word_addr, is_lw, "load_read");
    ch::core::ch_uint<kXlenBits> rdata(rp.impl());

    n->operator()(LOAD_DATA) = rdata;
    n->operator()(KT::RD_DATA) = select(is_lw, rdata, n->operator()(KT::RD_DATA));

    // Phase 6d.4: tohost probe — always-on async read at fixed word offset.
    // Emits a named proxy node ("tohost_probe_data_proxy") so the test can
    // verify RVTEST_PASS wrote dmem[tohost] == 1 from outside the simulator.
    if (tohost_probe_enabled_ && !tohost_probe_port_) {
      auto probe_addr = ch::core::ch_uint<15>(static_cast<std::uint32_t>(tohost_probe_word_));
      tohost_probe_port_ = std::make_unique<
          ch::core::ch_mem<ch::core::ch_uint<32>, kMemWords>::read_port>(
          mem.aread(probe_addr, "tohost_probe"));
    }
  }
};

}}}  // namespace cf::cpu::plugins
#endif  // CF_IP_CPU_PLUGINS_DMEM_CHMEM_H
#endif  // CF_PLUGIN_USE_CH_MEM
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
    auto f3_000   = ch::core::ch_uint<3>(ch::core::ch_literal<0, 3>{});
    auto f3_001   = ch::core::ch_uint<3>(ch::core::ch_literal<1, 3>{});
    auto f3_010   = ch::core::ch_uint<3>(ch::core::ch_literal<2, 3>{});
    auto f3_100   = ch::core::ch_uint<3>(ch::core::ch_literal<4, 3>{});
    auto f3_101   = ch::core::ch_uint<3>(ch::core::ch_literal<5, 3>{});

    auto is_store = ch::core::ch_bool(opcode == op_store);
    auto is_load  = ch::core::ch_bool(opcode == op_load);
    // Load variants: lb(000), lh(001), lw(010), lbu(100), lhu(101)
    auto is_lb  = is_load && ch::core::ch_bool(funct3 == f3_000);
    auto is_lh  = is_load && ch::core::ch_bool(funct3 == f3_001);
    auto is_lw  = is_load && ch::core::ch_bool(funct3 == f3_010);
    auto is_lbu = is_load && ch::core::ch_bool(funct3 == f3_100);
    auto is_lhu = is_load && ch::core::ch_bool(funct3 == f3_101);
    // Store variants: sb(000), sh(001), sw(010)
    auto is_sb  = is_store && ch::core::ch_bool(funct3 == f3_000);
    auto is_sh  = is_store && ch::core::ch_bool(funct3 == f3_001);
    auto is_sw  = is_store && ch::core::ch_bool(funct3 == f3_010);

    // Phase 6d.4: branch flush — suppress stores from the wrong-path
    // fall-through instruction fetched right after a taken branch.
    auto* fch = pb.node_of_logic_stage("fetch").get();
    auto flush = fch ? fch->operator()(IBusPlugin<T>::FLUSH)
                     : ch::core::ch_bool(false);

    auto byte_addr = n->operator()(KT::RESULT);
    auto word_addr = bits<15, 2>(byte_addr);
    auto byte_off = bits<1, 0>(byte_addr);  // 0..3 within word (little-endian)
    auto half_off = bits<1, 1>(byte_addr);  // 0 or 2 (bit1 selects half-word slot)

    // Read-modify-write for sub-word stores: aread reads current word.
    auto rp = mem.aread(word_addr, "load_read");
    ch::core::ch_uint<kXlenBits> rdata(rp.impl());
    n->operator()(LOAD_DATA) = rdata;

    // Byte shift amount select tree (0, 8, 16, 24)
    auto is_b0 = ch::core::ch_bool(byte_off == ch::core::ch_uint<2>(ch::core::ch_literal<0, 2>{}));
    auto is_b1 = ch::core::ch_bool(byte_off == ch::core::ch_uint<2>(ch::core::ch_literal<1, 2>{}));
    auto is_b2 = ch::core::ch_bool(byte_off == ch::core::ch_uint<2>(ch::core::ch_literal<2, 2>{}));
    auto byte_shift = select(is_b0, ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{}),
                          select(is_b1, ch::core::ch_uint<5>(ch::core::ch_literal<8, 5>{}),
                          select(is_b2, ch::core::ch_uint<5>(ch::core::ch_literal<16, 5>{}),
                                         ch::core::ch_uint<5>(ch::core::ch_literal<24, 5>{}))));
    auto half_shift = select(ch::core::ch_bool(half_off == ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{})),
                            ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{}),
                            ch::core::ch_uint<5>(ch::core::ch_literal<16, 5>{}));

    // Byte mask: 0xFF, 0xFF00, 0xFF0000, 0xFF000000 by byte_off
    auto byte_mask = select(is_b0, ch::core::ch_uint<32>(ch::core::ch_literal<0x000000FF, 32>{}),
                          select(is_b1, ch::core::ch_uint<32>(ch::core::ch_literal<0x0000FF00, 32>{}),
                          select(is_b2, ch::core::ch_uint<32>(ch::core::ch_literal<0x00FF0000, 32>{}),
                                         ch::core::ch_uint<32>(ch::core::ch_literal<0xFF000000, 32>{}))));
    auto half_mask = select(ch::core::ch_bool(half_off == ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{})),
                            ch::core::ch_uint<32>(ch::core::ch_literal<0x0000FFFF, 32>{}),
                            ch::core::ch_uint<32>(ch::core::ch_literal<0xFFFF0000, 32>{}));

    // Sign-extension masks
    auto byte_sign_bit = bits<7, 7>((rdata >> byte_shift));
    auto byte_sign_ext = select(byte_sign_bit,
                               ch::core::ch_uint<32>(ch::core::ch_literal<0xFFFFFF00, 32>{}),
                               ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}));
    auto half_sign_bit = bits<15, 15>((rdata >> half_shift));
    auto half_sign_ext = select(half_sign_bit,
                               ch::core::ch_uint<32>(ch::core::ch_literal<0xFFFF0000, 32>{}),
                               ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}));

    auto wdata = n->operator()(KT::RS2);

    // Load: extract + sign/zero extend
    auto byte_val = (rdata >> byte_shift) & ch::core::ch_uint<32>(ch::core::ch_literal<0xFF, 32>{});
    auto byte_signed = byte_val | byte_sign_ext;
    auto half_val = (rdata >> half_shift) & ch::core::ch_uint<32>(ch::core::ch_literal<0xFFFF, 32>{});
    auto half_signed = half_val | half_sign_ext;
    auto load_result = select(is_lw, rdata,
                          select(is_lb || is_lbu, select(is_lb, byte_signed, byte_val),
                          select(is_lh || is_lhu, select(is_lh, half_signed, half_val),
                                                 rdata)));
    n->operator()(KT::RD_DATA) = select(is_load, load_result, n->operator()(KT::RD_DATA));

    // Store: RMW — clear target byte/half in old word, OR in shifted wdata
    auto sb_wdata = wdata << byte_shift;
    auto sh_wdata = wdata << half_shift;
    auto sb_new = (rdata & ~byte_mask) | (sb_wdata & byte_mask);
    auto sh_new = (rdata & ~half_mask) | (sh_wdata & half_mask);
    auto store_wdata = select(is_sw, wdata,
                           select(is_sh, sh_new, sb_new));
    auto is_store_active = (is_sb || is_sh || is_sw) && !flush;
    mem.write(word_addr, store_wdata, is_store_active, "store_write");

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
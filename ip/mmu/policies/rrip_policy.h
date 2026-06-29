// ip/mmu/policies/rrip_policy.h
//
// 功能描述: RRIPPolicy —— Re-Reference Interval Prediction
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29

#ifndef CF_IP_MMU_POLICIES_RRIP_POLICY_H
#define CF_IP_MMU_POLICIES_RRIP_POLICY_H

#include <array>
#include <cstdint>
#include <string>

#include "ip/mmu/policies/tlb_replacement_policy.h"

namespace cf {
namespace ip {
namespace mmu {

template <std::size_t ENTRIES, std::size_t WAYS>
class RRIPPolicy : public TLBReplacementPolicy<ENTRIES, WAYS> {
  static constexpr std::size_t kSets = ENTRIES / WAYS;
  static constexpr uint8_t kMaxRRPV = 3;  // 2-bit RRIP: 0-3

 public:
  RRIPPolicy() {
    for (auto& s : rrpv_) {
      for (auto& r : s) r = kMaxRRPV;
    }
  }

  void on_access(uint32_t set, uint32_t way) override {
    rrpv_[set][way] = 0;  // hit → RRPV=0
  }
  void on_insert(uint32_t set, uint32_t way) override {
    rrpv_[set][way] = kMaxRRPV - 1;  // long re-reference
  }
  uint32_t select_victim(uint32_t set) override {
    while (true) {
      for (uint32_t w = 0; w < WAYS; ++w) {
        if (rrpv_[set][w] == kMaxRRPV) return w;
      }
      // Increment all RRIPVs
      for (uint32_t w = 0; w < WAYS; ++w) {
        if (rrpv_[set][w] < kMaxRRPV) ++rrpv_[set][w];
      }
    }
  }
  std::string name() const override { return "RRIP"; }

 private:
  std::array<std::array<uint8_t, WAYS>, kSets> rrpv_{};
};

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_POLICIES_RRIP_POLICY_H

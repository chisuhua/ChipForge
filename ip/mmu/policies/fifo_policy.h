// ip/mmu/policies/fifo_policy.h
//
// 功能描述: FIFOPolicy —— FIFO 队列 (circular index)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29

#ifndef CF_IP_MMU_POLICIES_FIFO_POLICY_H
#define CF_IP_MMU_POLICIES_FIFO_POLICY_H

#include <array>
#include <cstdint>
#include <string>

#include "ip/mmu/policies/tlb_replacement_policy.h"

namespace cf {
namespace ip {
namespace mmu {

template <std::size_t ENTRIES, std::size_t WAYS>
class FIFOPolicy : public TLBReplacementPolicy<ENTRIES, WAYS> {
  static_assert(ENTRIES % WAYS == 0, "ENTRIES must be multiple of WAYS");
  static constexpr std::size_t kSets = ENTRIES / WAYS;

 public:
  FIFOPolicy() {
    for (auto& s : fifo_idx_) s = 0;
  }

  void on_access(uint32_t, uint32_t) override {}
  void on_insert(uint32_t set, uint32_t) override {
    fifo_idx_[set] = (fifo_idx_[set] + 1) % WAYS;
  }
  uint32_t select_victim(uint32_t set) override {
    return fifo_idx_[set];
  }
  std::string name() const override { return "FIFO"; }

 private:
  std::array<uint32_t, kSets> fifo_idx_{};
};

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_POLICIES_FIFO_POLICY_H

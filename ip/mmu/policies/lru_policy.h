// ip/mmu/policies/lru_policy.h
//
// 功能描述: LRUPolicy —— 近似 LRU (global counter 简化)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29
//
// 注意: REFERENCE IMPLEMENTATION ONLY
//   简化版: 用全局 timestamp 计数器, on_access 时记录 timestamp,
//   select_victim 返回 timestamp 最小的 way. 严格 LRU 需 per-set LRU list,
//   推迟到 mmu-tlb-ptw-impl 阶段按需升级.

#ifndef CF_IP_MMU_POLICIES_LRU_POLICY_H
#define CF_IP_MMU_POLICIES_LRU_POLICY_H

#include <array>
#include <cstdint>
#include <string>

#include "ip/mmu/policies/tlb_replacement_policy.h"

namespace cf {
namespace ip {
namespace mmu {

template <std::size_t ENTRIES, std::size_t WAYS>
class LRUPolicy : public TLBReplacementPolicy<ENTRIES, WAYS> {
  static constexpr std::size_t kSets = ENTRIES / WAYS;

 public:
  LRUPolicy() {
    for (auto& s : timestamps_) {
      for (auto& t : s) t = 0;
    }
    global_counter_ = 1;
  }

  void on_access(uint32_t set, uint32_t way) override {
    timestamps_[set][way] = global_counter_++;
  }
  void on_insert(uint32_t set, uint32_t way) override {
    timestamps_[set][way] = global_counter_++;
  }
  uint32_t select_victim(uint32_t set) override {
    uint32_t oldest = timestamps_[set][0];
    uint32_t oldest_way = 0;
    for (uint32_t w = 1; w < WAYS; ++w) {
      if (timestamps_[set][w] < oldest) {
        oldest = timestamps_[set][w];
        oldest_way = w;
      }
    }
    return oldest_way;
  }
  std::string name() const override { return "LRU"; }

 private:
  std::array<std::array<uint64_t, WAYS>, kSets> timestamps_{};
  uint64_t global_counter_ = 1;
};

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_POLICIES_LRU_POLICY_H

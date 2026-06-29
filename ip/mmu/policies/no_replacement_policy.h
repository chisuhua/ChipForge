// ip/mmu/policies/no_replacement_policy.h
//
// 功能描述: NoReplacementPolicy —— 1-way / 全关联 no-op
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29

#ifndef CF_IP_MMU_POLICIES_NO_REPLACEMENT_POLICY_H
#define CF_IP_MMU_POLICIES_NO_REPLACEMENT_POLICY_H

#include <cstdint>
#include <string>

#include "ip/mmu/policies/tlb_replacement_policy.h"

namespace cf {
namespace ip {
namespace mmu {

template <std::size_t ENTRIES, std::size_t WAYS>
class NoReplacementPolicy : public TLBReplacementPolicy<ENTRIES, WAYS> {
 public:
  void on_access(uint32_t, uint32_t) override {}
  uint32_t select_victim(uint32_t) override { return 0; }
  void on_insert(uint32_t, uint32_t) override {}
  std::string name() const override { return "None"; }
};

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_POLICIES_NO_REPLACEMENT_POLICY_H

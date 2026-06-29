// ip/mmu/lib/multi_level_tlb.h
//
// 功能描述: MultiLevelTLB —— N 级 TLB 编排器 (mmu-ip-skeleton, 6.1-6.6)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29

#ifndef CF_IP_MMU_LIB_MULTI_LEVEL_TLB_H
#define CF_IP_MMU_LIB_MULTI_LEVEL_TLB_H

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "ip/mmu/lib/tlb_base.h"
#include "ip/mmu/lib/tlb_lookup.h"

namespace cf {
namespace ip {
namespace mmu {

class MultiLevelTLB {
 public:
  // levels 按 L0..Ln-1 顺序 (L0 最快最小, Ln-1 最慢最大)
  explicit MultiLevelTLB(std::vector<std::unique_ptr<TLBBase>> levels,
                          bool shadow_fill_enabled = true);

  TLBLookup lookup(uint64_t vaddr, uint16_t asid);
  void refill_from_ptw(uint64_t vaddr, uint16_t asid, uint64_t paddr, uint8_t perms);
  void invalidate_vaddr(uint64_t vaddr, uint16_t asid);
  void invalidate_asid(uint16_t asid);
  void invalidate_all();

  std::size_t num_levels() const { return levels_.size(); }
  TLBBase* level(std::size_t i) const { return levels_.at(i).get(); }

  uint64_t total_hit_count() const;
  uint64_t total_miss_count() const;
  uint64_t total_evict_count() const;

 private:
  std::vector<std::unique_ptr<TLBBase>> levels_;
  bool shadow_fill_enabled_;
};

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_LIB_MULTI_LEVEL_TLB_H

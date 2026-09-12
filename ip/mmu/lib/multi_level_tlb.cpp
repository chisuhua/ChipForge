// ip/mmu/lib/multi_level_tlb.cpp
//
// 功能描述: MultiLevelTLB 实装 (mmu-ip-skeleton, 6.3-6.6)

#include "ip/mmu/lib/multi_level_tlb.h"

namespace cf {
namespace ip {
namespace mmu {

MultiLevelTLB::MultiLevelTLB(std::vector<std::unique_ptr<TLBBase>> levels,
                             bool shadow_fill_enabled)
    : shadow_fill_enabled_(shadow_fill_enabled) {
  if (levels.empty()) {
    throw std::invalid_argument("MultiLevelTLB: levels must be non-empty");
  }
  levels_ = std::move(levels);
}

TLBLookup MultiLevelTLB::lookup(uint64_t vaddr, uint16_t asid) {
  // mmu-tlb-ptw-impl Decision 4: 返回最深 hit (deepest = largest level index).
  // 查询所有 level, 选取 level 编号最大的命中; 同时把最深命中
  // shadow fill 到所有浅层 level (替代原"首个 hit 立即返回"语义).
  TLBLookup best{};
  std::size_t best_level = 0;
  bool found = false;
  for (std::size_t i = 0; i < levels_.size(); ++i) {
    TLBLookup r = levels_[i]->lookup(vaddr, asid);
    if (r.hit && (!found || i > best_level)) {
      best = r;
      best_level = i;
      found = true;
    }
  }
  if (!found) return TLBLookup::make_miss();
  if (shadow_fill_enabled_) {
    for (std::size_t j = 0; j < best_level; ++j) {
      levels_[j]->insert_from(vaddr, best.paddr, asid, best.perms);
    }
  }
  return best;
}

void MultiLevelTLB::refill_from_ptw(uint64_t vaddr, uint16_t asid,
                                    uint64_t paddr, uint8_t perms) {
  // 写入最深层
  if (!levels_.empty()) {
    levels_.back()->insert(vaddr, paddr, asid, perms);
    // shadow fill 浅层
    if (shadow_fill_enabled_) {
      for (std::size_t i = 0; i + 1 < levels_.size(); ++i) {
        levels_[i]->insert_from(vaddr, paddr, asid, perms);
      }
    }
  }
}

void MultiLevelTLB::invalidate_vaddr(uint64_t vaddr, uint16_t asid) {
  for (auto& lvl : levels_) lvl->invalidate_vaddr(vaddr, asid);
}

void MultiLevelTLB::invalidate_asid(uint16_t asid) {
  for (auto& lvl : levels_) lvl->invalidate_asid(asid);
}

void MultiLevelTLB::invalidate_all() {
  for (auto& lvl : levels_) lvl->invalidate_all();
}

uint64_t MultiLevelTLB::total_hit_count() const {
  uint64_t total = 0;
  for (const auto& lvl : levels_) total += lvl->hit_count();
  return total;
}

uint64_t MultiLevelTLB::total_miss_count() const {
  uint64_t total = 0;
  for (const auto& lvl : levels_) total += lvl->miss_count();
  return total;
}

uint64_t MultiLevelTLB::total_evict_count() const {
  uint64_t total = 0;
  for (const auto& lvl : levels_) total += lvl->evict_count();
  return total;
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf

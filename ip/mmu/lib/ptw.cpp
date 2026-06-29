// ip/mmu/lib/ptw.cpp
//
// 功能描述: PageTableWalker 接口实装 (mmu-ip-skeleton, 7.3-7.7)

#include "ip/mmu/lib/ptw.h"

namespace cf {
namespace ip {
namespace mmu {

PTW::PTW() : mode_(SvMode::Sv39), max_inflight_(2), max_levels_(3) {}
PTW::PTW(SvMode mode, std::size_t max_inflight)
    : mode_(mode), max_inflight_(max_inflight) {
  switch (mode) {
    case SvMode::Bare:  max_levels_ = 0; break;
    case SvMode::Sv32:  max_levels_ = 2; break;  // Sv32: 2-level walk
    case SvMode::Sv39:  max_levels_ = 3; break;  // Sv39: 3-level walk
    case SvMode::Sv48:  max_levels_ = 4; break;  // Sv48: 4-level walk
  }
}

std::size_t PTW::max_levels() const { return max_levels_; }

void PTW::start_walk(uint64_t vaddr, uint16_t asid,
                     WalkCallback on_success, FaultCallback on_fault) {
  vaddr_ = vaddr;
  asid_ = asid;
  current_level_ = 0;
  busy_ = true;
  done_ = false;
  result_fault_ = 0;
  on_success_ = std::move(on_success);
  on_fault_ = std::move(on_fault);
  // stub: 起始 PTE 地址 = satp.PPN << 12 + vaddr[31:22] << 2 (Sv32 简例)
  // 完整实装推迟到 mmu-tlb-ptw-impl
  current_pte_paddr_ = 0;
}

void PTW::advance(uint64_t pte_raw, std::size_t level) {
  if (!busy_) return;
  PTE pte = decode_pte(pte_raw, mode_);

  if (!pte.v) {
    // Invalid PTE → page fault (12)
    result_fault_ = 12;
    done_ = true;
    busy_ = false;
    if (on_fault_) on_fault_(result_fault_);
    return;
  }

  if (current_level_ + 1 >= max_levels_) {
    // Leaf reached → success
    result_paddr_ = (pte.ppn << 12) | (vaddr_ & 0xFFF);
    result_perms_ = (pte.r ? 0x01 : 0) | (pte.w ? 0x02 : 0) |
                    (pte.x ? 0x04 : 0) | (pte.u ? 0x08 : 0);
    done_ = true;
    busy_ = false;
    if (on_success_) on_success_(result_paddr_, result_perms_);
    return;
  }

  // Non-leaf → continue walk
  current_pte_paddr_ = next_pte_paddr(pte.ppn, current_level_ + 1);
  ++current_level_;
}

uint64_t PTW::next_pte_paddr(uint64_t pte_ppn, std::size_t level) const {
  // stub: 完整 PTE 地址计算推迟到 mmu-tlb-ptw-impl
  return (pte_ppn << 12);
}

PTE PTW::decode_pte(uint64_t raw, SvMode mode) {
  PTE pte;
  pte.raw = raw;
  pte.v = (raw >> 0) & 1;
  pte.r = (raw >> 1) & 1;
  pte.w = (raw >> 2) & 1;
  pte.x = (raw >> 3) & 1;
  pte.u = (raw >> 4) & 1;
  pte.g = (raw >> 5) & 1;
  pte.a = (raw >> 6) & 1;
  pte.d = (raw >> 7) & 1;
  pte.rsw = (raw >> 8) & 0x3;
  switch (mode) {
    case SvMode::Sv32: pte.ppn_bits = 10; break;
    case SvMode::Sv39: pte.ppn_bits = 44; break;
    case SvMode::Sv48: pte.ppn_bits = 52; break;
    default: pte.ppn_bits = 0;
  }
  pte.ppn = raw >> 10;
  return pte;
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf

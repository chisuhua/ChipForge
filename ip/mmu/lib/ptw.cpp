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

void PTW::start_walk(uint64_t vaddr, uint16_t asid, uint64_t satp_ppn,
                     WalkCallback on_success, FaultCallback on_fault) {
  vaddr_ = vaddr;
  asid_ = asid;
  satp_ppn_ = satp_ppn;
  current_level_ = 0;
  busy_ = true;
  done_ = false;
  result_fault_ = 0;
  on_success_ = std::move(on_success);
  on_fault_ = std::move(on_fault);
  // mmu-paddr-consume-and-real-memory (P1#3 task §4.1, Oracle C2):
  // mode-correct root PTE 地址计算
  //   Sv32: PTE 4 字节, VPN[1] = vaddr[31:22]
  //   Sv39: PTE 8 字节, VPN[2] = vaddr[38:30]
  //   Sv48: PTE 8 字节, VPN[3] = vaddr[47:39]
  switch (mode_) {
    case SvMode::Sv32:
      current_pte_paddr_ = (satp_ppn << 12) + (((vaddr >> 22) & 0x3FF) * 4);
      break;
    case SvMode::Sv39:
      current_pte_paddr_ = (satp_ppn << 12) + (((vaddr >> 30) & 0x1FF) * 8);
      break;
    case SvMode::Sv48:
      current_pte_paddr_ = (satp_ppn << 12) + (((vaddr >> 39) & 0x1FF) * 8);
      break;
    default:
      current_pte_paddr_ = 0;
      break;
  }
  current_pte_l2_ = 0;
  current_pte_l1_ = 0;
  current_pte_l0_ = 0;
}

void PTW::advance(uint64_t pte_raw, std::size_t level) {
  if (!busy_) return;
  PTE pte = decode_pte(pte_raw, mode_);

  // 存储当前级别 PTE (用于异常时审计)
  if (current_level_ == 0) current_pte_l2_ = pte_raw;
  else if (current_level_ == 1) current_pte_l1_ = pte_raw;
  else if (current_level_ == 2) current_pte_l0_ = pte_raw;

  if (!pte.v) {
    // Invalid PTE → page fault (12 for exec, 13 for read, 15 for write — caller decides)
    result_fault_ = 12;
    done_ = true;
    busy_ = false;
    if (on_fault_) on_fault_(result_fault_);
    return;
  }

  // Reserved encoding per RISC-V spec: W=1 且 R=0 (writing without read is illegal)
  // Pre-fix bug (Oracle C1): 旧实现检查 r && w && x (R+W+X 合法叶子 PTE 被误判 fault 15)
  if (pte.w && !pte.r) {
    result_fault_ = 15;
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

  // Non-leaf → continue walk (Sv39 3-level: L2 → L1 → L0)
  // next level PTE paddr = pte.ppn << 12 + vaddr[VPN_i] * 8
  current_pte_paddr_ = next_pte_paddr(pte.ppn, current_level_ + 1);
  ++current_level_;
}

void PTW::stub_write_pte(std::size_t idx, const PTE& pte) {
  if (idx < kPteStubSize) pte_stub_memory_[idx] = pte;
}

PTE PTW::stub_read_pte(std::size_t idx) const {
  if (idx < kPteStubSize) return pte_stub_memory_[idx];
  return PTE{};
}

void PTW::advance_from_stub() {
  if (!busy_) return;
  const std::uint64_t ppn = current_pte_paddr_ >> 12;
  const std::size_t idx = ppn & 0xFFF;
  const PTE pte = pte_stub_memory_[idx];
  advance(pte.raw, current_level_);
}

// mmu-paddr-consume-and-real-memory (P1#3 task §4.1, Oracle C2):
// 从 MemoryInterface 真内存读 32-bit PTE, 替代 stub 路径
// Sv32-only (MemoryInterface 32-bit 接口, design.md D2 注记)
// mem == nullptr → 降级 stub 路径 (向后兼容, 现有 47 mmu 测试 0 回归)
// 越界读 (PicolibcHostMemory 返回 0) → decode_pte 后 V=0 → advance() 走 fault 12 路径
void PTW::advance_from_real_memory(MemoryInterface* mem) {
  if (!busy_) return;
  if (mem == nullptr) {
    advance_from_stub();
    return;
  }
  const std::uint32_t pte_raw = mem->read_word(current_pte_paddr_);
  advance(static_cast<std::uint64_t>(pte_raw), current_level_);
}

// mmu-paddr-consume-and-real-memory (P1#3 task §4.1, Oracle C2):
// mode-correct next PTE 地址计算
//   Sv32 (2-level): current_level=0 (root) → next=1 (leaf)
//     VPN[0] = vaddr[21:12], PTE 4 字节
//   Sv39 (3-level): current_level=0 (L2) → next=1 (L1) → next=2 (L0 leaf)
//     VPN[i] = vaddr[12+i*9 : 20+i*9], PTE 8 字节
//   Sv48 (4-level): 同 Sv39 模式多一层
uint64_t PTW::next_pte_paddr(uint64_t pte_ppn, std::size_t next_level) const {
  std::size_t pte_size = (mode_ == SvMode::Sv32) ? 4 : 8;
  std::size_t vpn_shift = 0;
  switch (mode_) {
    case SvMode::Sv32:
      // 2-level: next_level=1 (leaf) → VPN[0]=vaddr[21:12]
      vpn_shift = 12;
      break;
    case SvMode::Sv39:
      // 3-level: next_level=1 (L1) → VPN[1]=vaddr[29:21]
      //          next_level=2 (L0 leaf) → VPN[0]=vaddr[20:12]
      vpn_shift = (next_level == 1) ? 21 : 12;
      break;
    case SvMode::Sv48:
      // 4-level: next_level=1 → VPN[2]=vaddr[38:30]
      //          next_level=2 → VPN[1]=vaddr[29:21]
      //          next_level=3 → VPN[0]=vaddr[20:12]
      vpn_shift = (next_level == 1) ? 30
                 : (next_level == 2) ? 21 : 12;
      break;
    default:
      return 0;
  }
  return (pte_ppn << 12) + (((vaddr_ >> vpn_shift) & 0x1FF) * pte_size);
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

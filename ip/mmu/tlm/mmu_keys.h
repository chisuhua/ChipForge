// ip/mmu/tlm/mmu_keys.h
//
// 功能描述: MMU Plugin Payload<T> Key 集合 (mmu-ip-skeleton, 8.1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29
//
// 设计:
//   - 10 个 inline Payload<T> Key, 命名空间 cf::ip::mmu::payload
//   - 与 ip/cpu/arch/riscv/payload_riscv.h 同构
//   - 用于 MMUPlugin::at_stage 闭包内跨阶段数据传递

#ifndef CF_IP_MMU_TLM_MMU_KEYS_H
#define CF_IP_MMU_TLM_MMU_KEYS_H

#include <cstdint>

#include "cf/plugin/payload.h"
#include "cf/plugin/uint_t.h"

namespace cf {
namespace ip {
namespace mmu {

namespace payload {

template <typename T = std::uint64_t>
struct mmu_keys {
  static_assert(std::is_same<T, std::uint32_t>::value ||
                    std::is_same<T, std::uint64_t>::value,
                "T must be uint32_t (RV32) or uint64_t (RV64)");

  static inline cf::plugin::Payload<T>       VADDR{"mmu.vaddr"};
  static inline cf::plugin::Payload<T>       PADDR{"mmu.paddr"};
  static inline cf::plugin::Payload<std::uint8_t>  PERMS{"mmu.perms"};
  static inline cf::plugin::Payload<bool>     PTW_ACTIVE{"mmu.ptw_active"};
  static inline cf::plugin::Payload<T>       PTW_VADDR{"mmu.ptw_vaddr"};
  static inline cf::plugin::Payload<std::uint16_t> PTW_ASID{"mmu.ptw_asid"};
  static inline cf::plugin::Payload<std::uint64_t> PTW_L0_RAW{"mmu.ptw_l0_raw"};
  static inline cf::plugin::Payload<std::uint64_t> PTW_L1_RAW{"mmu.ptw_l1_raw"};
  static inline cf::plugin::Payload<std::uint64_t> PTW_L2_RAW{"mmu.ptw_l2_raw"};
  static inline cf::plugin::Payload<std::uint8_t>  PTW_FAULT{"mmu.ptw_fault"};

  // mmu-tlb-ptw-impl commit 8 新增 4 Key
  static inline cf::plugin::Payload<T>       MMU_VADDR{"mmu.mmu_vaddr"};
  static inline cf::plugin::Payload<std::uint8_t>  EXCEPTION_CODE{"mmu.exception_code"};
  static inline cf::plugin::Payload<T>       SATP_PPN{"mmu.satp_ppn"};
  static inline cf::plugin::Payload<std::uint8_t>  SATP_MODE{"mmu.satp_mode"};

  // mmu-paddr-consume-and-real-memory (P1#3 task §4+§5): PADDR_VALID 标志
  // 由 MMUPlugin::do_lookup() 在 tlb_lookup_ifetch/loadstore 节点 NORMAL phase
  // 同步写入 (hit→true, fault→false), IBus/DBus 读取前用 has() 守卫
  // (Oracle review 2026-09-25 C6 推荐: 与 PADDR 同点同 phase 原子写, 避免时序歧义)
  static inline cf::plugin::Payload<bool>    PADDR_VALID{"mmu.paddr_valid"};
};

}  // namespace payload
}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_TLM_MMU_KEYS_H

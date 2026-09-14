// ip/cpu/tlm/cpu_keys.h
//
// 功能描述: CPU → MMU IPC Payload Key 集合 (mmu-cache-integration commit 2/9)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-13
//
// 设计:
//   - 4 个 Payload Key 让 CPU 写 / MMU 读
//   - SAT (satp CSR value): CPU csrrw 指令写入 → MMUPlugin::csr_write_satp 闭包读取
//   - SFENCE_VADDR (rs1): CPU SFENCE.VMA 指令写入 → MMUPlugin::sfence_vma 闭包读取
//   - SFENCE_ASID (rs2): CPU SFENCE.VMA 指令写入 → MMUPlugin::sfence_vma 闭包读取
//   - CPU_EXCEPTION_CODE: MMU 异常码 12/13/15 传播到 CPU exception path
//   - 放 ip/cpu/ 而非 ip/mmu/: MMU 闭包依赖 Key identity (全局 static), mmu 不反向依赖 CPU 头
//   - mmu_keys.h::EXCEPTION_CODE 是 MMU 输出, CPU_EXCEPTION_CODE 是 CPU 输入, 方向不同
//
// D4 合规: 0 业务 tick(), 0 状态机, 闭包驱动

#ifndef CF_IP_CPU_TLM_CPU_KEYS_H
#define CF_IP_CPU_TLM_CPU_KEYS_H

#include <cstdint>
#include <type_traits>

#include "cf/plugin/payload.h"
#include "cf/plugin/uint_t.h"

namespace cf {
namespace cpu {
namespace tlm {
namespace payload {

template <typename T = std::uint64_t>
struct cpu_keys {
  static_assert(std::is_same<T, std::uint32_t>::value ||
                    std::is_same<T, std::uint64_t>::value,
                "T must be uint32_t (RV32) or uint64_t (RV64)");

  // CPU → MMU: satp CSR value (low 4 bits = MODE, high bits = PPN)
  static inline cf::plugin::Payload<T> SAT{"cpu.sat"};

  // CPU → MMU: SFENCE.VMA rs1 (vaddr; 0 means x0 register, invalidate-all-vaddr)
  static inline cf::plugin::Payload<T> SFENCE_VADDR{"cpu.sfence_vaddr"};

  // CPU → MMU: SFENCE.VMA rs2 (asid; 0 means x0 register, invalidate-all-asid)
  static inline cf::plugin::Payload<T> SFENCE_ASID{"cpu.sfence_asid"};

  // MMU → CPU: exception code 12/13/15 (page fault)
  static inline cf::plugin::Payload<std::uint8_t> CPU_EXCEPTION_CODE{
      "cpu.exception_code"};
};

}  // namespace payload
}  // namespace tlm
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_TLM_CPU_KEYS_H

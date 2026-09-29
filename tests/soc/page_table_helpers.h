// tests/soc/page_table_helpers.h
//
// cpu-factory-satp-mapping (v0.10.2): Plant identity-mapping Sv32 PTE into
// PicolibcHostMemory window for [cpu-l1-mmu-demo] e2e test.
//
// Design:
//   - Single 4MB superpage leaf PTE (RISC-V Sv32 4MB megapage at root level).
//     current_level_+1 >= max_levels_ triggers leaf detection in PTW.
//   - vaddr must be 4MB-aligned (Sv32 4MB superpage constraint: PPN[9:0] = 0).
//   - pte_base must NOT overlap loaded ELF .text sections (test macro chooses
//     window_base + 60*1024 = last 4KB of 64KB window as the safe region).
//   - Helper writes a single 4-byte PTE: VPN1 index into root page table.
//
// Constraint: GLOB_RECURSE in tests/CMakeLists.txt only picks *_test.cpp files,
// so this header is not compiled standalone (it's an inline-only header).
//
// PTE format (Sv32 leaf PTE):
//   bits [31:10] = PPN (22 bits, identity: PPN = vaddr >> 12)
//   bits [9:0]  = flags (V | R | W | X | U | A | D = 0xDF; spec-compliant
//                  access/dirty bits set so first-access doesn't fault if
//                  PTW ever adds A/D check)
//   bits [9:8]  = RSW (reserved for supervisor; 0)
// PTW checks V=1 + W&&!R forbidden; otherwise treats as leaf.

#ifndef CF_TESTS_SOC_PAGE_TABLE_HELPERS_H
#define CF_TESTS_SOC_PAGE_TABLE_HELPERS_H

#include <cstdint>
#include <stdexcept>

#include "ip/cpu/picolibc_host_memory.h"

namespace cf {
namespace soc {
namespace test {

// Plant identity-mapping 4MB superpage leaf PTE at the root page table.
//
// Args:
//   mem     - PicolibcHostMemory holding the RAM window.
//   pte_base - 4KB-aligned physical address where the root page table lives.
//              MUST NOT overlap any ELF .text section (use window_base + 60*1024).
//   vaddr   - 4MB-aligned virtual address to map. Identity translation means
//              the leaf PTE's PPN = vaddr >> 12 (same physical address).
//
// Pre: pte_base must be 4KB-aligned, in_window(pte_base + VPN1*4 + 3) must hold.
// Post: PTW walks root[vpn1] to leaf PTE, identity translation paddr = vaddr.
inline void plant_identity_page_table(cf::cpu::PicolibcHostMemory& mem,
                                     std::uint64_t pte_base,
                                     std::uint64_t vaddr) {
  // Sv32 4MB superpage: PPN[9:0] must be 0, so vaddr must be 4MB-aligned.
  if ((vaddr & 0x3FFFFFULL) != 0) {
    throw std::invalid_argument(
        "plant_identity_page_table: vaddr 0x" + std::to_string(vaddr) +
        " not 4MB-aligned (Sv32 4MB superpage requires PPN[9:0]=0)");
  }

  const std::uint32_t vpn1 =
      static_cast<std::uint32_t>((vaddr >> 22) & 0x3FFu);
  const std::uint32_t ppn =
      static_cast<std::uint32_t>((vaddr >> 12) & 0x3FFFFFu);
  // PTE PPN at bits [31:10] (Sv32 22-bit PPN shifted left by 10);
  // flags V|R|W|X|U|A|D = 0xDF in bits [7:0]; RSW bits [9:8] = 0.
  const std::uint32_t pte_value =
      static_cast<std::uint32_t>((ppn << 10) | 0xDFu);

  mem.write_word(pte_base + vpn1 * 4u, pte_value);
}

}  // namespace test
}  // namespace soc
}  // namespace cf

#endif  // CF_TESTS_SOC_PAGE_TABLE_HELPERS_H
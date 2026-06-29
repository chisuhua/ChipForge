// ip/mmu/lib/tlb_entry.h
//
// 功能描述: TLB Entry 模板化定义 (mmu-ip-skeleton, 2.1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29
//
// 设计:
//   - 模板化 TLBEntry<TAG_BITS, ASID_BITS> —— 1:1 映射 CppHDL ch_array
//   - 仅 include cf/plugin/uint_t.h (位宽 typedef), 不依赖 Plugin 框架
//   - 字段全 uint_t<N> 或 bool_t, 无 std::optional / std::variant
//   - VPN 字段按 Sv39/Sv48 通用 (PPN 长度由调用方决定)
//
// 约束:
//   - lib/ 纯 C++ (D4 Plugin 范式强制: lib 不依赖 cf::plugin::* 除位宽 typedef)
//   - 头文件, 无 .cpp
//   - 全 POD 类型, HDL 1:1 友好
//   - 编译期 static_assert 验证 TAG_BITS/ASID_BITS 合法

#ifndef CF_IP_MMU_LIB_TLB_ENTRY_H
#define CF_IP_MMU_LIB_TLB_ENTRY_H

#include <cstdint>

#include "cf/plugin/uint_t.h"

namespace cf {
namespace ip {
namespace mmu {

// ----------------------------------------------------------------------------
// TLBEntry<TAG_BITS, ASID_BITS> —— TLB 单条 entry 模板化定义
//
// 模板参数:
//   TAG_BITS   VPN tag 位宽 (典型 sv39=27, sv48=37)
//   ASID_BITS  ASID 位宽 (典型 RISC-V=9, GPU=12-16, 0=无 ASID)
//
// 字段语义:
//   valid      entry 是否有效
//   tag        VPN 高位 (与 vaddr[63:63-TAG_BITS+1] 比对)
//   pfn        PPN (物理页号, 长度由调用方决定 — Sv39=44, Sv48=52)
//   asid       ASID (与 vaddr 转换时的 ASID 比对)
//   perms      权限位 (R/W/X/U + reserved)
//   global     global mapping (RISC-V Global bit, ASID 忽略)
//
// 注: pfn 长度 = (xlen - 12) bits; Sv39: xlen=64 → 52 bits; Sv32: xlen=32 → 20 bits
//     此模板的 pfn 字段用 uint_t<64> 容纳, 实际有效位由调用方解释
// ----------------------------------------------------------------------------
template <std::size_t TAG_BITS, std::size_t ASID_BITS>
struct TLBEntry {
  static_assert(TAG_BITS > 0 && TAG_BITS <= 52,
                "TAG_BITS must be in (0, 52]");
  static_assert(ASID_BITS >= 0 && ASID_BITS <= 16,
                "ASID_BITS must be in [0, 16]");

  cf::plugin::bool_t    valid{false};
  cf::plugin::uint_t<TAG_BITS>  tag{0};
  cf::plugin::uint_t<64>       pfn{0};   // Sv39: 44 bits; Sv48: 52 bits
  cf::plugin::uint_t<ASID_BITS> asid{0};
  cf::plugin::uint_t<8>         perms{0};
  cf::plugin::bool_t           global{false};

  // 默认构造: 全部无效
  TLBEntry() = default;
};

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_LIB_TLB_ENTRY_H

// ip/mmu/lib/tlb_lookup.h
//
// 功能描述: TLBLookup 查询结果结构 (mmu-ip-skeleton, 2.2)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29
//
// 设计:
//   - TLBLookup 是 lib/ 算法的查询结果契约
//   - 5 字段: hit (valid bit 替代 std::optional) / paddr / perms / fault / fault_code
//   - 全 uint_t<N> 或 bool_t
//   - 8 字节对齐 (满足 memcpy 要求)
//
// 约束:
//   - lib/ 纯 C++ (D4 范式强制)
//   - 头文件, 无 .cpp
//   - HDL 1:1 友好 (valid bit 替代 std::optional)

#ifndef CF_IP_MMU_LIB_TLB_LOOKUP_H
#define CF_IP_MMU_LIB_TLB_LOOKUP_H

#include <cstdint>

#include "cf/plugin/uint_t.h"

namespace cf {
namespace ip {
namespace mmu {

// ----------------------------------------------------------------------------
// TLBLookup —— TLB 查询结果 (5 字段, 接口稳定)
//
// 字段语义:
//   hit         valid bit: true=命中, false=miss
//   paddr       物理地址 (仅 hit 时有效)
//   perms       权限位 R/W/X/U + reserved (RISC-V 4 位 + 4 reserved)
//   fault       是否触发 fault (权限不足 / reserved / etc.)
//   fault_code  异常码 (0=无, 1=page fault, 2/3=不同类型 page fault,
//                          5/7=access fault, 参考 RISC-V Privileged Spec)
//
// 性能: 5 字段 = 1+8+1+1+1 = 12 字节 + 4 padding = 16 字节 (8 字节对齐)
// ----------------------------------------------------------------------------
struct TLBLookup {
  cf::plugin::bool_t    hit{false};
  cf::plugin::uint_t<64> paddr{0};
  cf::plugin::uint_t<8>  perms{0};
  cf::plugin::bool_t    fault{false};
  cf::plugin::uint_t<4>  fault_code{0};

  // 默认构造: 全部 miss / 无 fault
  TLBLookup() = default;

  // 工厂: hit 构造
  static TLBLookup make_hit(uint64_t paddr_, uint8_t perms_) {
    TLBLookup r;
    r.hit = true;
    r.paddr = paddr_;
    r.perms = perms_;
    return r;
  }

  // 工厂: miss 构造 (默认)
  static TLBLookup make_miss() { return TLBLookup{}; }

  // 工厂: fault 构造
  static TLBLookup make_fault(uint8_t fault_code_) {
    TLBLookup r;
    r.fault = true;
    r.fault_code = fault_code_;
    return r;
  }
};

// 注: paddr 是 uint64, 强制结构体 8 字节对齐; sizeof 通常为 16 字节

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_LIB_TLB_LOOKUP_H

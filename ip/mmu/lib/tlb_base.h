// ip/mmu/lib/tlb_base.h
//
// 功能描述: TLBBase 抽象基类 (mmu-ip-skeleton, 2.3)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29
//
// 设计:
//   - TLBBase 是 MultiLevelTLB 编排器持多态的抽象基类
//   - 8 个纯虚方法 + 1 工厂 (create 静态)
//   - virtual 允许 (基类是多态容器, HDL 1:1 不直接转 TLBBase)
//   - 仅 include cf/plugin/uint_t.h (位宽 typedef), 不依赖 Plugin 框架
//
// 约束:
//   - lib/ 纯 C++ (D4 范式强制)
//   - 头文件, 无 .cpp
//   - TLB<ENTRIES, WAYS, ...> 模板类继承 TLBBase, 提供具体实装

#ifndef CF_IP_MMU_LIB_TLB_BASE_H
#define CF_IP_MMU_LIB_TLB_BASE_H

#include <cstdint>

#include "cf/plugin/uint_t.h"
#include "ip/mmu/lib/tlb_lookup.h"

namespace cf {
namespace ip {
namespace mmu {

// ----------------------------------------------------------------------------
// TLBBase —— 单级 TLB 抽象基类 (MultiLevelTLB 多态容器)
//
// 设计动机:
//   - MultiLevelTLB 持 std::vector<std::unique_ptr<TLBBase>>
//   - 各 TLB 实例可独立配置 (entries/ways/policy/ports)
//   - virtual 允许 (基类不直接转 HDL, 仅作为多态接口)
//
// 8 纯虚方法:
//   1. lookup(vaddr, asid) -> TLBLookup        : 查询
//   2. insert(vaddr, paddr, asid, perms)      : refill / 直接写入 (计 evict)
//   3. insert_from(vaddr, paddr, asid, perms)  : shadow fill (不污染统计)
//   4. invalidate_vaddr(vaddr, asid)          : 单条失效
//   5. invalidate_asid(asid)                  : 整 ASID 失效
//   6. invalidate_all()                       : 全清
//   7. hit_count()                            : 统计查询
//   8. miss_count()                           : 统计查询
//   9. evict_count()                          : 统计查询
//   10. name()                                : 层级名 ("L0"/"L1"/...)
// ----------------------------------------------------------------------------
class TLBBase {
 public:
  virtual ~TLBBase() = default;

  // 查询 (hit: 返回 paddr/perms; miss: hit=false; fault: fault=true)
  virtual TLBLookup lookup(uint64_t vaddr, uint16_t asid) const = 0;

  // 写入 (覆盖同 vaddr 或选 victim; 计 evict 统计)
  virtual void insert(uint64_t vaddr, uint64_t paddr,
                      uint16_t asid, uint8_t perms) = 0;

  // 写入 (shadow fill 专用; 不计 evict 统计)
  virtual void insert_from(uint64_t vaddr, uint64_t paddr,
                           uint16_t asid, uint8_t perms) = 0;

  // 失效 (单条 / 整 ASID / 全清)
  virtual void invalidate_vaddr(uint64_t vaddr, uint16_t asid) = 0;
  virtual void invalidate_asid(uint16_t asid) = 0;
  virtual void invalidate_all() = 0;

  // 统计查询
  virtual uint64_t hit_count() const = 0;
  virtual uint64_t miss_count() const = 0;
  virtual uint64_t evict_count() const = 0;

  // 层级名 ("L0"/"L1"/"L2"/...)
  virtual const char* name() const = 0;
};

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_LIB_TLB_BASE_H

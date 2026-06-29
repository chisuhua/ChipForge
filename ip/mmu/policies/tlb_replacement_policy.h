// ip/mmu/lib/tlb_replacement_policy.h
//
// 功能描述: TLB 替换策略抽象接口 (mmu-ip-skeleton, 4.1)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-29
//
// 设计:
//   - 模板化 TLBReplacementPolicy<ENTRIES, WAYS> —— 4 虚方法 + 1 工厂
//   - 与 ip/cache/policies/ReplacementPolicy 命名空间隔离 (cf::ip::mmu::policies vs cf::ip::cache::policies)
//   - lib/ 纯 C++, 0 依赖 Plugin 框架
//
// 约束:
//   - 头文件为主, create 工厂实现在 tlb_replacement_policy.cpp
//   - 4 策略 (None/FIFO/LRU/RRIP) 在 policies/ 目录

#ifndef CF_IP_MMU_LIB_TLB_REPLACEMENT_POLICY_H
#define CF_IP_MMU_LIB_TLB_REPLACEMENT_POLICY_H

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace cf {
namespace ip {
namespace mmu {

template <std::size_t ENTRIES, std::size_t WAYS>
class TLBReplacementPolicy {
 public:
  virtual ~TLBReplacementPolicy() = default;
  virtual void on_access(uint32_t set, uint32_t way) = 0;
  virtual uint32_t select_victim(uint32_t set) = 0;
  virtual void on_insert(uint32_t set, uint32_t way) = 0;
  virtual std::string name() const = 0;

  static std::unique_ptr<TLBReplacementPolicy> create(const std::string& name);
};

}  // namespace mmu
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_MMU_LIB_TLB_REPLACEMENT_POLICY_H

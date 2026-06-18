// ip/cache/policies/no_replacement_policy.h
//
// 功能描述: "无替换" 策略 —— 1-way 直接映射的默认行为 (Phase 1.4)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-18
//
// 适用场景:
//   - L1CachePlugin 当前是 256 sets × 1 way direct-mapped
//   - 1-way 没有"选择受害者"语义 (唯一 way 必然被替换)
//   - 作为 L1CachePlugin 构造参数的默认 policy, 保持 Phase 1.3 行为零变化
//
// 设计要点:
//   - 3 个方法全部 no-op (不维护任何状态, 不计算任何值)
//   - select_victim(set) 返回 0 (1-way 唯一 way)
//   - name() 返回 "None" (工厂方法 / JSON 配置用)
//
// 详见:
//   - openspec/changes/cache-policy-foundation/specs/cache-replacement-policy-abstraction/spec.md REQ-CRPA-003

#ifndef CF_IP_CACHE_POLICIES_NO_REPLACEMENT_POLICY_H
#define CF_IP_CACHE_POLICIES_NO_REPLACEMENT_POLICY_H

#include <cstdint>
#include <string>

#include "ip/cache/policies/replacement_policy.h"

namespace cf {
namespace ip {
namespace cache {
namespace policies {

// ----------------------------------------------------------------------------
// NoReplacementPolicy —— 1-way direct-mapped 的 no-op 替换策略
//
// 等价于 L1CachePlugin Phase 1.3 的 hard-coded 行为:
//   - 唯一 way 必然被替换 (不需要选择)
//   - 无需记录 recency / timestamp
//   - 是 L1CachePlugin 默认 policy, 保证向后兼容
// ----------------------------------------------------------------------------
class NoReplacementPolicy final : public ReplacementPolicy {
 public:
  NoReplacementPolicy() = default;
  ~NoReplacementPolicy() override = default;

  /// @brief no-op: 不维护任何状态
  void on_access(uint32_t /*set*/, uint32_t /*way*/) override {
    // 1-way 直接映射无 recency 概念
  }

  /// @brief 1-way 唯一 way 必然被替换
  /// @return 始终返回 0
  uint32_t select_victim(uint32_t /*set*/) override {
    return 0u;
  }

  /// @brief no-op: 不记录任何 timestamp
  void on_insert(uint32_t /*set*/, uint32_t /*way*/) override {
    // 1-way 直接映射无 timestamp 概念
  }

  /// @brief 返回策略名 "None"
  std::string name() const override {
    return "None";
  }
};

}  // namespace policies
}  // namespace cache
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_CACHE_POLICIES_NO_REPLACEMENT_POLICY_H

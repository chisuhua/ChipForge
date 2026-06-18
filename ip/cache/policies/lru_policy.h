/** @file lru_policy.h
 *  @brief REFERENCE IMPLEMENTATION ONLY
 *
 *  适用于 L1CachePlugin 1-way 直接映射（256 sets × 1 way）。
 *  Phase 1.5 L2CachePlugin 实施时此文件将被 8-way 完整 LRU 整体替换，
 *  不做向前兼容迁移。如需 L2 LRU 请等待 Phase 1.5。
 *
 *  ----------------------------------------------------------------------------
 *  警告 (Warning):
 *    本文件实现是 L1CachePlugin 1-way 场景的极简 LRU 演示, **不**适用于 8-way
 *    L2 Cache.  8-way LRU 需要 per-set LRU 栈 (8 元素 deque + O(1) move-to-front)
 *    + recency bits, 实现复杂度远超当前简化版.
 *
 *    Phase 1.5 L2CachePlugin 实施时:
 *      - 此文件将被整体替换为 8-way LRU
 *      - 不做任何向前兼容迁移 (API 不保留, 内部数据结构不保留)
 *      - 如需 L2 LRU, 请等待 Phase 1.5, **不要**在本文件上扩展
 *
 *    本文件存在的目的:
 *      1. 验证 ReplacementPolicy 抽象接口的完整性 (4 虚方法都能调用)
 *      2. 验证 L1CachePlugin 注入点的连通性 (可选 policy 切换)
 *      3. 作为未来 8-way LRU 实现的"参考骨架" (注释级别, 非代码级别)
 *  ----------------------------------------------------------------------------
 */
//
// 功能描述: 1-way 简化的 LRU 替换策略 (Phase 1.4 reference impl)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-18
//
// 设计选择 (Phase 1.4 简化):
//   - select_victim(set) -> 0     (1-way 唯一 way 必然被替换)
//   - on_access(set, way)         全局 access counter 单调递增 (无 per-set 状态)
//   - on_insert(set, way)         记录 (set, way) 的 timestamp (= 当前 counter)
//   - name() -> "LRU"
//
// 设计权衡:
//   - 不使用 <set> / <unordered_set> 外部容器 (保持零外部依赖, 仅 vector + counter)
//   - 当前 1-way 不需要真正的 LRU 栈; counter 仅用于"interface smoke test"
//   - 8-way 完整 LRU 在 Phase 1.5 实施
//
// 详见:
//   - openspec/changes/cache-policy-foundation/specs/cache-replacement-policy-abstraction/spec.md REQ-CRPA-004
//   - openspec/changes/cache-policy-foundation/design.md §Decision 3

#ifndef CF_IP_CACHE_POLICIES_LRU_POLICY_H
#define CF_IP_CACHE_POLICIES_LRU_POLICY_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ip/cache/policies/replacement_policy.h"

namespace cf {
namespace ip {
namespace cache {
namespace policies {

// ----------------------------------------------------------------------------
// LRUPolicy —— 1-way LRU reference implementation (Phase 1.4)
//
// **不适用于 8-way L2 Cache**. 详见文件顶部警告.
//
// 内部状态 (最小集):
//   - access_counter_   全局单调递增, 每次 on_access 增 1
//   - timestamps_       [(set, way) -> timestamp] 列表 (按 on_insert 顺序追加)
//
// 简化说明:
//   - 1-way 场景下 LRU 与 LFU / FIFO 行为等价 (只有 1 个 way 可选)
//   - counter 单调递增模拟"时间推进", 真实硬件用 timestamp 寄存器
//   - timestamps_ 用 vector<pair> 而非 unordered_map: 1-way 场景 N=O(num_sets)
//     vector O(N) 扫描仍可接受; 8-way 时改用 per-set LRU 栈
// ----------------------------------------------------------------------------
class LRUPolicy final : public ReplacementPolicy {
 public:
  LRUPolicy() = default;
  ~LRUPolicy() override = default;

  /// @brief 记录 (set, way) 被访问, 全局 counter 增 1
  /// @note 1-way 场景下不维护 per-set 状态 (无意义)
  void on_access(uint32_t /*set*/, uint32_t /*way*/) override {
    ++access_counter_;
  }

  /// @brief 1-way 唯一 way 必然被替换
  /// @return 始终返回 0
  uint32_t select_victim(uint32_t /*set*/) override {
    return 0u;
  }

  /// @brief 记录 (set, way) 插入时的 timestamp (= 当前 access_counter_)
  void on_insert(uint32_t set, uint32_t way) override {
    // 简化版: 直接 push_back, 不查重 (1-way 同一 (set, way) 不会重复 insert
    // 直到被替换; 而替换路径由 select_victim 触发, 此处只在 refill 后调用)
    timestamps_.emplace_back(set, way);
  }

  /// @brief 返回策略名 "LRU"
  std::string name() const override {
    return "LRU";
  }

  // ------------------------------------------------------------------------
  // 测试辅助 API (header-only; 仅供单元测试 / 调试使用)
  // ------------------------------------------------------------------------

  /// @brief 当前 access counter 值 (供单元测试验证 on_access 是否触发)
  uint64_t access_counter() const { return access_counter_; }

  /// @brief timestamps_ 大小 (供单元测试验证 on_insert 是否触发)
  std::size_t timestamp_count() const { return timestamps_.size(); }

 private:
  uint64_t access_counter_ = 0;                  ///< 全局单调递增访问计数
  std::vector<std::pair<uint32_t, uint32_t>> timestamps_;  ///< (set, way) 插入序列
};

}  // namespace policies
}  // namespace cache
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_CACHE_POLICIES_LRU_POLICY_H

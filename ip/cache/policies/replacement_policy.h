// ip/cache/policies/replacement_policy.h
//
// 功能描述: Cache 替换策略抽象接口 (Phase 1.4)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-18
//
// 设计目标:
//   - 为 L1/L2 Cache 提供统一的替换策略抽象接口
//   - 通过字符串工厂方法支持 JSON 配置或构造参数切换实现
//   - 接口稳定性优先于策略完备性 (本 change 只落地 None + LRU)
//   - 其他策略 (PLRU / Random / FIFO / RRIP) 推迟到 Phase 1.5 L2CachePlugin
//
// 命名空间: cf::ip::cache::policies (与 cpptlm::Policy 隔离)
//
// 文件结构:
//   - replacement_policy.h     声明抽象基类 (本文件, 无 .cpp 内联)
//   - replacement_policy.cpp   工厂方法 create() 实现 (编译单元)
//   - no_replacement_policy.h  NoReplacementPolicy 头文件 (header-only)
//   - lru_policy.h             LRUPolicy 头文件 (header-only)
//
// 详见:
//   - openspec/changes/cache-policy-foundation/proposal.md
//   - openspec/changes/cache-policy-foundation/design.md
//   - docs/architecture/overview.md §"可插拔策略模式"

#ifndef CF_IP_CACHE_POLICIES_REPLACEMENT_POLICY_H
#define CF_IP_CACHE_POLICIES_REPLACEMENT_POLICY_H

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace cf {
namespace ip {
namespace cache {
namespace policies {

// 前向声明: 具体策略类 (避免 replacement_policy.h 反向依赖 policy 子类头文件)
class NoReplacementPolicy;
class LRUPolicy;

// ----------------------------------------------------------------------------
// ReplacementPolicy —— Cache 替换策略抽象基类
//
// 4 个虚方法覆盖访问/插入/选择受害者/标识 4 个生命周期:
//   - on_access(set, way)     cache 命中或访问某 way 时调用 (LRU 更新 recency)
//   - select_victim(set)      选择一个 way 作为受害者 (替换时使用)
//   - on_insert(set, way)     新 line 插入某 way 时调用 (LRU 记录 timestamp)
//   - name()                  策略名 (工厂方法 / 日志 / JSON 配置用)
//
// 1 个静态工厂方法:
//   - create(name)            按字符串名创建具体策略; 未知名抛 std::runtime_error
//
// 实现约束:
//   - set / way 用 uint32_t (足够覆盖 8-way / 64 sets)
//   - 工厂方法返回 std::unique_ptr (工厂方法不能返回栈对象)
//   - 子类禁止派生多继承 (避免菱形); 未来扩展策略按子类继承即可
//   - create() 实现位于 replacement_policy.cpp (避免与策略头文件的循环依赖)
// ----------------------------------------------------------------------------
class ReplacementPolicy {
 public:
  virtual ~ReplacementPolicy() = default;

  /// @brief 通知策略某 set 的某 way 被访问 (读命中 / 写命中 / 任何触发时)
  /// @param set  访问的 set 索引 (0 ~ num_sets-1)
  /// @param way  访问的 way 索引 (0 ~ associativity-1)
  /// 默认实现: no-op (由具体策略重写)
  virtual void on_access(uint32_t set, uint32_t way) = 0;

  /// @brief 选择一个 set 中的受害者 way (用于 cache miss 时的替换)
  /// @param set  需要选择受害者的 set 索引
  /// @return     选中的 way 索引 (0 ~ associativity-1)
  virtual uint32_t select_victim(uint32_t set) = 0;

  /// @brief 通知策略新 line 已插入某 set 的某 way (refill 后)
  /// @param set  插入的 set 索引
  /// @param way  插入的 way 索引
  virtual void on_insert(uint32_t set, uint32_t way) = 0;

  /// @brief 返回策略名 (用于工厂方法 / 日志 / JSON 配置)
  /// @return  策略名字符串 (如 "None" / "LRU")
  virtual std::string name() const = 0;

  /// @brief 按字符串名创建具体策略实例 (工厂方法)
  /// @param name 策略名 ("None" / "LRU")
  /// @return     std::unique_ptr<ReplacementPolicy> 持有新创建的具体策略
  /// @throw      std::runtime_error 当 name 不被识别时
  /// @note       实现位于 replacement_policy.cpp (依赖具体策略的完整定义)
  static std::unique_ptr<ReplacementPolicy> create(const std::string& name);
};

}  // namespace policies
}  // namespace cache
}  // namespace ip
}  // namespace cf

#endif  // CF_IP_CACHE_POLICIES_REPLACEMENT_POLICY_H

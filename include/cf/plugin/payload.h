// include/cf/plugin/payload.h
//
// 功能描述: 类型安全 Key (Phase 0 P0 #2)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-08
//
// 设计目标:
//   - Payload<T> 是类型 + 名称的组合 descriptor
//   - 全局静态对象作为 "Key" 使用
//   - 跨 PipeNode 隔离(每个 PipeNode 有自己的 std::map<PayloadKeyBase*, std::any>)
//   - 编译期类型检查: operator()(Payload<T>) 只能从 T 类型的 PipeNode 取出
//
// 借鉴:
//   - VexRiscv Stageable[T] 静态对象模式
//   - CppHDL ch_state_machine 内部结构
//
// 约束:
//   - 头文件 (无 .cpp, 仅模板)
//   - 与 cf::plugin::uint_t<N> 配合使用
//   - Phase 0 仅 TLM 模式(无 ch_state_machine 跨模式支持)

#ifndef CF_PLUGIN_PAYLOAD_H
#define CF_PLUGIN_PAYLOAD_H

#include <any>
#include <map>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>

namespace cf {
namespace plugin {

// 前向声明 PipeNode (P0 #3 完整定义)
class PipeNode;

// ----------------------------------------------------------------------------
// PayloadKeyBase —— 类型擦除基类
// 用于在 std::map 中以多态方式存储不同 T 的 Payload Key
// ----------------------------------------------------------------------------
class PayloadKeyBase {
 public:
  explicit PayloadKeyBase(std::string name) : name_(std::move(name)) {}
  virtual ~PayloadKeyBase() = default;

  // 禁止拷贝 (静态描述符不应被复制)
  PayloadKeyBase(const PayloadKeyBase&) = delete;
  PayloadKeyBase& operator=(const PayloadKeyBase&) = delete;

  // 名称标识 (用于调试和唯一性检查)
  const std::string& name() const noexcept { return name_; }

  // 类型擦除的 typeid (用于 type-safe 取出)
  virtual const std::type_info& type() const noexcept = 0;

  // 比较 (按指针身份; 同一全局静态对象地址相同)
  bool operator<(const PayloadKeyBase& other) const noexcept {
    return this < &other;
  }

 private:
  std::string name_;
};

// ----------------------------------------------------------------------------
// Payload<T> —— 类型安全 Key 模板
// 用法: cf::plugin::Payload<uint64_t> addr_key{"addr"};
//       (作为全局静态对象, 在 Plugin 中声明)
// ----------------------------------------------------------------------------
template <typename T>
class Payload : public PayloadKeyBase {
 public:
  explicit Payload(std::string name) : PayloadKeyBase(std::move(name)) {}

  // 禁止拷贝 (单例语义)
  Payload(const Payload&) = delete;
  Payload& operator=(const Payload&) = delete;

  // 类型信息 (用于 type-safe 访问)
  const std::type_info& type() const noexcept override { return typeid(T); }

  // 静态类型查询
  static constexpr const std::type_info& static_type() noexcept {
    return typeid(T);
  }
};

// ----------------------------------------------------------------------------
// PayloadStore —— 跨 PipeNode 隔离的 Payload 存储
// 每个 PipeNode 持有一个 PayloadStore; 不同 PipeNode 的同 key Payload 互不干扰
// ----------------------------------------------------------------------------
class PayloadStore {
 public:
  PayloadStore() = default;
  ~PayloadStore() = default;

  // 禁止拷贝
  PayloadStore(const PayloadStore&) = delete;
  PayloadStore& operator=(const PayloadStore&) = delete;

  // 写入: type-checked
  template <typename T>
  void put(const Payload<T>& key, T value) {
    auto& cell = cells_[&key];
    // 编译期已确保 T == Payload<T>::static_type()
    cell = std::move(value);
  }

  // 读取: type-checked (运行时 typeid 二次校验); 缺失时返回默认构造值
  template <typename T>
  const T& get(const Payload<T>& key) const {
    auto it = cells_.find(&key);
    if (it == cells_.end()) {
      // 默认构造并插入 (便于 `n(key) = value` 直接写)
      it = cells_.emplace(&key, T{}).first;
    }
    if (it->second.type() != typeid(T)) {
      throw std::runtime_error("Payload type mismatch: " + key.name() +
                               " (expected " + typeid(T).name() +
                               ", got " + it->second.type().name() + ")");
    }
    return std::any_cast<const T&>(it->second);
  }

  // 可变读取 (用于修改); 缺失时默认构造
  template <typename T>
  T& get(const Payload<T>& key) {
    auto it = cells_.find(&key);
    if (it == cells_.end()) {
      it = cells_.emplace(&key, T{}).first;
    }
    if (it->second.type() != typeid(T)) {
      throw std::runtime_error("Payload type mismatch: " + key.name());
    }
    return std::any_cast<T&>(it->second);
  }

  // 检查是否存在
  template <typename T>
  bool has(const Payload<T>& key) const {
    auto it = cells_.find(&key);
    return it != cells_.end() && it->second.type() == typeid(T);
  }

  // 清空
  void clear() noexcept { cells_.clear(); }

  // 大小 (调试用)
  std::size_t size() const noexcept { return cells_.size(); }

 private:
  std::map<const PayloadKeyBase*, std::any> cells_;
};

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_PAYLOAD_H

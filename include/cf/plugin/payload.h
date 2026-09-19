// include/cf/plugin/payload.h
//
// 功能描述: 类型安全 Key (Phase 0 P0 #2) + Phase 6c elaboration 双模兼容
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-16 (Phase 6c M1: PayloadStore cell 双模化)
//
// 设计目标:
//   - Payload<T> 是类型 + 名称的组合 descriptor (VexRiscv Stageable[T] 同构)
//   - 全局静态对象作为 "Key" 使用 (key identity = &Payload<T> 指针)
//   - 跨 PipeNode 隔离(每个 PipeNode 有自己的 PayloadStore)
//   - 编译期类型检查: get<T>/put<T> 只能从 T 类型取出
//   - Phase 6c 双模: TLM 默认 (POD 值) + CF_PLUGIN_USE_CH_MEM (ch_uint<N>/ch_bool 代理)
//
// 借鉴:
//   - VexRiscv Stageable[T] 静态对象模式 (key = static object address)
//   - CppHDL ch_state_machine 内部结构
//
// 约束:
//   - 头文件 (无 .cpp, 仅模板)
//   - 与 cf::plugin::uint_t<N> 配合使用 (uint_t.h 决定 cell 类型)
//   - TLM 模式 (默认): cell 装 POD 值, 运行时每周期读写
//   - CH_MEM 模式: cell 装 ch_uint<N>/ch_bool 句柄, elaboration 一次发射 lnode DAG

#ifndef CF_PLUGIN_PAYLOAD_H
#define CF_PLUGIN_PAYLOAD_H

#include <any>
#include <map>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>

#ifdef CF_PLUGIN_USE_CH_MEM
#include <ch.hpp>  // ch::core::ch_uint<N>, ch::core::ch_bool
#endif

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
// 用法: cf::plugin::Payload<cf::plugin::uint_t<32>> pc_key{"pc"};
//       (作为全局静态对象, 在 Plugin 中声明)
//
// 双模兼容:
//   - TLM 模式 (默认): T = uint_t<32> = uint32_t (POD)
//   - CH_MEM 模式: T = uint_t<32> = ch::core::ch_uint<32> (lnode DAG 句柄)
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

  // 静态类型查询 (用于编译期检查)
  static constexpr const std::type_info& static_type() noexcept {
    return typeid(T);
  }
};

#ifdef CF_PLUGIN_USE_CH_MEM
// ----------------------------------------------------------------------------
// Phase 6c CH_MEM 模式: PayloadStore cell 装 ch 代理对象
//
// 设计: std::any 保留 (类型擦除仍在 elaboration 期一次性使用), 但 T = ch 类型
//   - get<T>() 返回 ch_uint<N>& / ch_bool& 引用, 赋值即发射 lnode DAG assign 节点
//   - put<T>() 显式写入 ch 信号 (elaboration 时立即发射)
//   - cell 缺失时默认构造 T{} (ch_uint<N>() = 空句柄; ch_bool() = false)
//
// 为什么不直接 std::any 装 lnodeimpl*:
//   - ch_uint<N> 含 static_cast 行为 (zext/trunc 推断), 装代理可保留全部 API
//   - 上层 get<T>(payload_key) 返回引用, 业务代码 `result = a + b` 透明工作
// ----------------------------------------------------------------------------
class PayloadStore {
 public:
  PayloadStore() = default;
  ~PayloadStore() = default;

  // 禁止拷贝
  PayloadStore(const PayloadStore&) = delete;
  PayloadStore& operator=(const PayloadStore&) = delete;

  // 写入: type-checked (elaboration 时一次, typeid 校验仍保留)
  template <typename T>
  void put(const Payload<T>& key, T value) {
    auto& cell = cells_[&key];
    // 编译期已确保 T == Payload<T>::static_type()
    // std::any 装 ch 代理 (ch_uint<N> 是 lnodeimpl* 的浅包装, 拷贝无副作用)
    cell = std::move(value);
  }

// 读取: type-checked; const 路径缺失时抛异常 (const 正确性要求，不能 emplace)
    // Phase 6c M6: 原代码 cells_.emplace 在 const 方法中编译通过但因 const 正确性
    // 漏洞未被检出。改为 throw-on-miss 使其变为真正的只读方法，
    // 同时避免 ch_uint<N>/ch_bool 默认构造生成 null impl 污染 DAG。
    template <typename T>
    const T& get(const Payload<T>& key) const {
      auto it = cells_.find(&key);
      if (it == cells_.end()) {
        throw std::runtime_error(
            "PayloadStore cell missing: " + key.name() +
            " (CH_MEM elaboration: populate before at_stage accesses)");
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

#else
// ----------------------------------------------------------------------------
// Phase 0-1 TLM 模式 (默认): PayloadStore cell 装 POD 值
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
#endif  // CF_PLUGIN_USE_CH_MEM

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_PAYLOAD_H
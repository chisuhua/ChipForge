// include/cf/plugin/storage.h
//
// 功能描述: TLM 模式存储抽象 (Phase 1.3+) —— 为 Plugin-style 业务代码
//           提供可平滑升级到 CppHDL `ch_mem` 的统一接口。
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10
//
// 设计动机:
//   Phase 0/1 的 L1CachePlugin 直接持有 `std::array<T, N>`, 读写语义无 cycle
//   边界(同 `pb.run()` 内的写对下游阶段立即可见)。该模式在 Phase 5/6 迁移
//   到 CppHDL `ch_mem` 时, 需要大量适配工作 —— `ch_mem` 的 `sread` /
//   `write` 端口与 `operator[]` 完全不同的接口签名。
//
//   `array_store<T, N>` 提供:
//     - Phase 1/2/3/4: 纯 POD 容器 (`std::array<T, N>` 后端), 行为等同
//       直接 `std::array` (即同 `pb.run()` 内 RAW 立即可见)。
//     - Phase 5/6: 切换为 `ch_mem` 后端实现(届时本类内部重新实现,
//       对外 API 不变), 提供 `ch_mem` 风格的"读返回上一周期提交值"。
//
// 使用方式:
//   using TagStore = cf::plugin::storage::array_store<
//       cf::plugin::uint_t<L1CachePlugin::kTagBits>,
//       L1CachePlugin::kNumSets>;
//   TagStore tags_;  // 直接 operator[] 访问
//
// 阶段迁移:
//   - Phase 1.x (TLM): 当前实现, 单缓冲, RAW 立即可见
//   - Phase 6 (RTL):  内部持有 `ch_mem` 影子, `commit()` 在 `pb.run()`
//                     末尾统一提交, 读取返回"上一周期提交值"
//
// 配套决策: ADR-040 (TLM→HDL 移植性约束)
//
// 约束:
//   - 头文件 (无 .cpp, 与 cf_plugin INTERFACE 库保持一致)
//   - 编译期零开销 (TLM 模式下 `array_store` 仅是 `std::array` 的类型包装)
//   - 不引入 `ch::core::context` 依赖 (TLM 模式禁止)
//   - 接口与 CppHDL `ch_mem::aread/sread/write` 保持语义同构 (Phase 6 切换时
//     仅改实现, 不改业务调用方)

#ifndef CF_PLUGIN_STORAGE_H
#define CF_PLUGIN_STORAGE_H

#include <array>
#include <cstddef>
#include <type_traits>

namespace cf {
namespace plugin {
namespace storage {

// ----------------------------------------------------------------------------
// array_store<T, N> —— TLM 模式下的存储包装
//
// 设计目标:
//   1. 对外 API 与 `std::array<T, N>` 一致(`operator[]` / `at()` / `data()`),
//      Phase 1 业务代码无感替换 (从 `std::array<T,N> tags_` 改为
//      `array_store<T,N> tags_`, 调用点不变)。
//   2. 为 Phase 5/6 切换到 `ch_mem` 后端预留空间: 届时改用双缓冲
//      (current_ / shadow_), 暴露 `read()` / `write()` / `commit()` 风格 API。
//   3. TLM 模式下不引入性能开销 (与直接 `std::array` 等价)。
//
// 当前实现 (Phase 1): 单缓冲, 即写即读
// 未来实现 (Phase 6): 双缓冲, `commit()` 之前 read 看不到 write
//
// 类型约束:
//   - T 必须是 trivially copyable (允许未来替换为 ch_mem 时做位拷贝)
//   - N 必须是编译期常量
// ----------------------------------------------------------------------------
template <typename T, std::size_t N>
class array_store {
  static_assert(std::is_trivially_copyable<T>::value,
                "array_store<T,N>: T must be trivially copyable "
                "(Phase 6 ch_mem 迁移需要)");

 public:
  using value_type     = T;
  using size_type      = std::size_t;
  using iterator       = T*;
  using const_iterator = const T*;

  // ------------------------------------------------------------------------
  // 构造 —— 默认零初始化, 与 std::array 一致
  // ------------------------------------------------------------------------
  constexpr array_store() = default;
  ~array_store() = default;

  array_store(const array_store&) = default;
  array_store& operator=(const array_store&) = default;
  array_store(array_store&&) noexcept = default;
  array_store& operator=(array_store&&) noexcept = default;

  // ------------------------------------------------------------------------
  // 元素访问 (与 std::array 一致)
  // ------------------------------------------------------------------------
  // 读 —— TLM 模式: 返回当前值 (与 std::array 等价)
  //      Phase 6 切换后: 将返回"上一周期 commit 提交的值"
  constexpr T&       operator[](size_type i)       noexcept { return data_[i]; }
  constexpr const T& operator[](size_type i) const noexcept { return data_[i]; }

  T&       at(size_type i)       { return data_.at(i); }
  const T& at(size_type i) const { return data_.at(i); }

  T*       data()       noexcept { return data_.data(); }
  const T* data() const noexcept { return data_.data(); }

  // ------------------------------------------------------------------------
  // 容量 (与 std::array 一致)
  // ------------------------------------------------------------------------
  static constexpr size_type size() noexcept { return N; }
  static constexpr bool      empty()    noexcept { return N == 0; }

  // ------------------------------------------------------------------------
  // Phase 6 钩子 (Phase 1 模式下是 no-op)
  //
  // 当 array_store 内部实现切换为双缓冲 (current_/shadow_), 这些方法将
  // 把 shadow_ 提交到 current_。Phase 1 模式下无操作。
  // ------------------------------------------------------------------------
  void commit() noexcept { /* Phase 1: no-op, 单缓冲无 commit 语义 */ }

  // 重置所有元素为零 (供测试间隔离使用)
  void reset() noexcept {
    for (size_type i = 0; i < N; ++i) data_[i] = T{};
  }

  // ------------------------------------------------------------------------
  // 迭代器支持 (与 std::array 一致)
  // ------------------------------------------------------------------------
  iterator       begin()        noexcept { return data_.data(); }
  iterator       end()          noexcept { return data_.data() + N; }
  const_iterator begin()  const noexcept { return data_.data(); }
  const_iterator end()    const noexcept { return data_.data() + N; }
  const_iterator cbegin() const noexcept { return data_.data(); }
  const_iterator cend()   const noexcept { return data_.data() + N; }

 private:
  std::array<T, N> data_{};  // TLM 模式: 直接持有; Phase 6 替换为 ch_mem
};

}  // namespace storage
}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_STORAGE_H

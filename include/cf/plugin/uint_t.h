// include/cf/plugin/uint_t.h
//
// 功能描述: 编译期类型切换 (uint_t<N> / bool_t)
// 借鉴: VexRiscv Stageable[T] + CppHDL ch_state_machine 模式
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-08
//
// 设计目标:
//   - 在 TLM 模式下表现为标准 C++ 整数类型 (uint64_t 等)
//   - 在 RTL 模式下可被 CppHDL ch_uint<N> 替换 (Phase 6 实施)
//   - Phase 0 仅用编译期 typedef, 行为与标准类型一致
//
// 约束: 头文件为主, 仅模板特例化

#ifndef CF_PLUGIN_UINT_T_H
#define CF_PLUGIN_UINT_T_H

#include <cstdint>
#include <type_traits>

namespace cf {
namespace plugin {

// uint_t<N> —— 编译期位宽切换
// Phase 0: typedef 到标准整数类型
// Phase 6: 可由 BundleMapper 替换为 ch_uint<N>
template <unsigned N>
struct uint_t_impl {
  // 选择最接近且 >= N 的标准类型
  using type = typename std::conditional<
      N <= 8, uint8_t,
      typename std::conditional<
          N <= 16, uint16_t,
          typename std::conditional<
              N <= 32, uint32_t,
              typename std::conditional<N <= 64, uint64_t,
                                        // 兜底: > 64 位暂不支持
                                        uint64_t>::type>::type>::type>::type;
};

template <unsigned N>
using uint_t = typename uint_t_impl<N>::type;

// bool_t —— 1 位布尔
// Phase 0: typedef 到 bool; Phase 6: 可替换为 ch_bool
using bool_t = bool;

// 编译期断言: uint_t<N> 是无符号整数
static_assert(std::is_unsigned<uint_t<8>>::value, "uint_t<8> must be unsigned");
static_assert(std::is_unsigned<uint_t<32>>::value, "uint_t<32> must be unsigned");
static_assert(std::is_unsigned<uint_t<64>>::value, "uint_t<64> must be unsigned");
static_assert(std::is_same<bool_t, bool>::value, "bool_t must be bool (Phase 0)");

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_UINT_T_H

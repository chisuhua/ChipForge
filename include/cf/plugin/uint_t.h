// include/cf/plugin/uint_t.h
//
// 功能描述: 编译期类型切换 (uint_t<N> / bool_t)
// 借鉴: VexRiscv Stageable[T] + CppHDL ch_state_machine 模式
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-16 (Phase 6c M1 翻转: 加 CF_PLUGIN_USE_CH_MEM 开关)
//
// 设计目标:
//   - TLM 仿真模式 (默认): uint_t<N> 表现标准 C++ 整数类型 (uint64_t 等)
//   - RTL 仿真模式 (CF_PLUGIN_USE_CH_MEM): uint_t<N> = ch::core::ch_uint<N>
//   - Phase 6c 前向兼容: 编译开关默认 OFF, 现有 TLM 测试零回归
//   - Phase 6c 启用后: at_stage 闭包内 lambda 体发射 lnode DAG 到 ch::toVerilog
//
// 约束:
//   - 头文件为主, 仅模板特例化
//   - 切换 CF_PLUGIN_USE_CH_MEM 时, 业务代码须使用 ch::core::ch_uint<N> 的 API:
//     select(), bits<N-1,0>(), operator=, ch_reg<T>, ch_mem<T,N>

#ifndef CF_PLUGIN_UINT_T_H
#define CF_PLUGIN_UINT_T_H

#include <cstdint>
#include <type_traits>

#ifdef CF_PLUGIN_USE_CH_MEM
// ============================================================================
// Phase 6c elaboration 模式: uint_t<N> = ch::core::ch_uint<N>, bool_t = ch::core::ch_bool
//
// 使用场景: 新代码 / PoC / Phase 6c 路径 (RegFile + IntAlu + 5 级流水线算术子集)
//
// 纪律约束 (W3-4 M2 增补):
//   - at_stage 闭包内禁止 `if (uint_t / bool_t)` —— ch_bool 有 explicit operator bool()
//     contextual conversion, 编译期不会失败, 但运行期会发射"未知语义节点"
//   - 必须用 select(cond, true_val, false_val) 显式表达条件
//   - 详见 tools/check_plugin_portability.sh 新增检查 (M5)
// ============================================================================
#include <ch.hpp>  // ch_uint, ch_bool, ch_reg, ch_mem, select

namespace cf {
namespace plugin {

template <unsigned N>
using uint_t = ch::core::ch_uint<N>;
using bool_t = ch::core::ch_bool;

// 注: ch_uint<N> 不是标准无符号整数, std::is_unsigned<ch_uint<N>>::value 不适用
// M1 验证:
//   - static_assert(std::is_unsigned<uint_t<8>>::value, ...);  ← 删除（不适用 ch_uint）
//   - static_assert(std::is_same<bool_t, bool>::value, ...);   ← 删除（bool_t 是 ch_bool）

// 编译期宽度常量 (从 ch_uint 提取)
template <unsigned N>
struct uint_width {
  static constexpr unsigned value = N;
};

template <unsigned N>
constexpr unsigned uint_width_v = N;

}  // namespace plugin
}  // namespace cf

#else
// ============================================================================
// Phase 0-1 TLM 仿真模式 (默认): uint_t<N> typedef 到标准 C++ 整数类型
//
// 使用场景: 现有所有 TLM 仿真测试 (ibustest, dpustest, regfile_test, ...)
// 零回归: 保持原行为
// ============================================================================
namespace cf {
namespace plugin {

template <unsigned N>
struct uint_t_impl {
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

template <unsigned N>
struct uint_width {
  static constexpr unsigned value = sizeof(uint_t<N>) * 8;
};

template <unsigned N>
constexpr unsigned uint_width_v = sizeof(uint_t<N>) * 8;

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_USE_CH_MEM

#endif  // CF_PLUGIN_UINT_T_H
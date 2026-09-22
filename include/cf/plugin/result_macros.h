// include/cf/plugin/result_macros.h
//
// 功能描述: Result 范式工具宏与抹平模板 (v0.6 ADR-047)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-22
//
// 设计:
//   - PB_TRY(expr): 内部传播宏 (do-while + `_r` 变量避免 shadow)
//   - PB_EXPECT(cond, err): 条件检查宏, 失败返回 unexpected
//   - auto_throw<T>(Result<T>): 业务抹平工具, 错误转 throw, 成功返回值
//   - 与 std::expected 链式调用 (.and_then / .or_else) 共存 (PB_TRY 局部变量名固定 `_r`)
//
// 警告:
//   - 不要在 lambda 闭包内使用 PB_TRY (lambda 推断 Result 类型可能不明确)
//   - PB_TRY 仅在返回 Result<...> 的函数体内使用
//   - auto_throw 会丢失 Result 信息 (转 PluginException.what()); 慎用于 hot path

#ifndef CF_PLUGIN_RESULT_MACROS_H
#define CF_PLUGIN_RESULT_MACROS_H

#include <utility>

#include "cf/plugin/plugin_error.h"

namespace cf {
namespace plugin {

/**
 * PB_TRY(expr) — 内部传播宏.
 *
 *   - 求值 expr (类型必须为 Result<T> 或 std::expected<T, PluginError>)
 *   - 若非 ok, 从当前函数 `return std::unexpected(_r.error())`
 *   - 使用 do-while + `_r` 局部变量名, 避免宏内变量 shadow 与多次求值
 *
 * 使用:
 *   Result<void> build_cpu_impl() {
 *     PB_TRY(pb.register_plugin(p));
 *     PB_TRY(pb.at_stage("fetch", Phase::NORMAL, cb));
 *     return {};
 *   }
 */
#define PB_TRY(expr)                                                       \
  do {                                                                     \
    auto _r = (expr);                                                      \
    if (!_r.has_value()) {                                                 \
      return std::unexpected(_r.error());                                \
    }                                                                      \
  } while (0)

/**
 * PB_EXPECT(cond, err) — 条件检查宏.
 *
 *   - 若 cond 为 false, 从当前函数 `return std::unexpected(err)`
 *   - 用于参数校验, 不抛异常
 *
 * 使用:
 *   PB_EXPECT(!stage_name.empty(), PluginError::EmptyStageName);
 */
#define PB_EXPECT(cond, err)                                               \
  do {                                                                     \
    if (!(cond)) {                                                         \
      return std::unexpected(err);                                         \
    }                                                                      \
  } while (0)

/**
 * auto_throw<T> — 业务抹平工具.
 *
 *   - Result<T> 错误: 抛 PluginException (使用 to_exception 转换)
 *   - Result<T> 成功: unwrap 返回 T (std::move if unique_ptr)
 *
 * 使用:
 *   auto pb = auto_throw(build_cpu_impl(config, mem));
 */
template <typename T>
T auto_throw(Result<T> r) {
  if (!r.has_value()) {
    throw to_exception(r.error(), "");
  }
  return std::move(*r);
}

/**
 * auto_throw<void> 特化 — Result<void> 抹平.
 */
inline void auto_throw(Result<void> r) {
  if (!r.has_value()) {
    throw to_exception(r.error(), "");
  }
}

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_RESULT_MACROS_H
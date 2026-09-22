// include/cf/plugin/plugin_error.h
//
// 功能描述: Plugin framework 静态配置错误类型 + Result 别名 (v0.6 ADR-047)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-22
//
// 设计:
//   - enum class PluginError: 20 字段, 编译期类型安全的错误码
//   - Result<T> = std::expected<T, PluginError>: C++23 标准库
//   - plugin_error_message(err, detail): 工厂函数, 输出可读消息
//   - to_exception(err, stage): 转换为 PluginException (CLI/Run 路径)
//   - ADR-047 例外清单:
//       1. PipeBuilder::run() 仍 throw (CtrlLink throw_when)
//       2. PayloadStore::get() 仍 throw (at_stage 热路径)
//       3. CppHDL CH_ASSERT 子仓库范围外
//       4. PluginBase::setup()/build() 虚函数 — 回调体内部业务代码自由选择

#ifndef CF_PLUGIN_PLUGIN_ERROR_H
#define CF_PLUGIN_PLUGIN_ERROR_H

#include <expected>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "cf/plugin/plugin_exception.h"

namespace cf {
namespace plugin {

enum class PluginError {
  NullPlugin,         // register_plugin(nullptr)
  NullCallback,        // at_stage(... , nullptr)
  NullCommitHook,     // register_commit_hook(nullptr)
  NullCtrlLink,       // register_ctrl_link(... , nullptr)
  NullConnector,      // register_stage_payload_connector(..., nullptr)
  EmptyStageName,     // at_stage("", ...)
  EmptyParentStageName,  // declare_substage("", ...)
  NullContextPtr,     // elaborate() but ctx_ unset
  CtxNotProvided,     // elaborate(ctx) but ctx null
  AlreadyElaborated,  // elaborate() called twice
  DuplicatePlugin,    // same plugin instance registered twice
  DuplicateStageName, // at_stage("X", ...) when "X" already exists
  UnknownStage,       // query API: stage name not in pipeline
  StageNotFound,      // build/elaborate referenced missing substage
  BuildFailed,        // pb.build() caught generic std::exception
  PluginBuildFailed,  // pb.build() caught PluginException from plugin callback
  ElaborationFailed,  // pb.elaborate(ctx) caught CH_MEM error
  ConnectorFailed,    // register_stage_payload_connector failed
  UnsupportedPipelineDepth,  // CPU pipeline depth out of {1,3,5,7,10}
  UnsupportedMulLatency,     // multiplier latency not in valid set
};

template <typename T>
using Result = std::expected<T, PluginError>;

inline std::string plugin_error_message(PluginError err,
                                        const std::string& detail = "") {
  std::string base;
  switch (err) {
    case PluginError::NullPlugin:           base = "null plugin pointer"; break;
    case PluginError::NullCallback:        base = "null stage callback"; break;
    case PluginError::NullCommitHook:      base = "null commit hook"; break;
    case PluginError::NullCtrlLink:        base = "null ctrl link"; break;
    case PluginError::NullConnector:       base = "null payload connector"; break;
    case PluginError::EmptyStageName:      base = "empty stage name"; break;
    case PluginError::EmptyParentStageName: base = "empty parent stage name"; break;
    case PluginError::NullContextPtr:      base = "null elaboration context"; break;
    case PluginError::CtxNotProvided:      base = "elaboration context not provided"; break;
    case PluginError::AlreadyElaborated:   base = "pipeline already elaborated"; break;
    case PluginError::DuplicatePlugin:     base = "duplicate plugin registration"; break;
    case PluginError::DuplicateStageName:  base = "duplicate stage name"; break;
    case PluginError::UnknownStage:        base = "unknown stage name"; break;
    case PluginError::StageNotFound:       base = "stage not found in pipeline"; break;
    case PluginError::BuildFailed:         base = "pipeline build failed"; break;
    case PluginError::PluginBuildFailed:   base = "plugin build callback failed"; break;
    case PluginError::ElaborationFailed:   base = "pipeline elaboration failed"; break;
    case PluginError::ConnectorFailed:     base = "stage payload connector failed"; break;
    case PluginError::UnsupportedPipelineDepth: base = "unsupported pipeline depth"; break;
    case PluginError::UnsupportedMulLatency:    base = "unsupported multiplier latency"; break;
  }
  if (detail.empty()) {
    return base;
  }
  return base + " (" + detail + ")";
}

inline PluginException to_exception(PluginError err,
                                    const std::string& stage = "") {
  return PluginException(err, stage);
}

// ─── PluginException 构造函数实现 (v0.6 ADR-047 桥接) ──────────
//   - 在 plugin_error.h 提供 out-of-line 实现, 避免 plugin_error.h ↔ plugin_exception.h 循环依赖
//   - 用户应 #include "cf/plugin/plugin_error.h" 获得完整功能
inline PluginException::PluginException(const std::string& stage_name,
                                        const std::string& message)
    : std::runtime_error(message),
      stage_name_(stage_name),
      error_(PluginError::BuildFailed) {}

inline PluginException::PluginException(PluginError err,
                                        const std::string& stage)
    : std::runtime_error(plugin_error_message(err, stage)),
      stage_name_(stage),
      error_(err) {}

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_PLUGIN_ERROR_H
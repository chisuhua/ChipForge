// include/cf/plugin/plugin_exception.h
//
// 功能描述: Plugin framework 异常类型 (plugin-framework-stall commit A + v0.6 ADR-047)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-22
//
// 设计:
//   - 派生自 std::runtime_error (有现成 message() API, 与 std 异常体系集成)
//   - 携带 stage_name (触发异常的 stage) 便于调试
//   - 由 PipeBuilder::run() 末尾在 should_throw() 为真时抛出
//   - 抛出时跳过 commit_storages() (与 pb-stall-loop spec Scenario 4 一致)
//   - v0.6 增强: 新增 PluginError 构造重载, 兼容 Result 范式 to_exception()

#ifndef CF_PLUGIN_PLUGIN_EXCEPTION_H
#define CF_PLUGIN_PLUGIN_EXCEPTION_H

#include <stdexcept>
#include <string>
#include <utility>

namespace cf {
namespace plugin {

// 前向声明 (避免 plugin_error.h 与 plugin_exception.h 循环依赖)
enum class PluginError;

class PluginException : public std::runtime_error {
 public:
  explicit PluginException(const std::string& stage_name,
                           const std::string& message);

  // v0.6 ADR-047: 桥接 Result 范式 (auto_throw / to_exception)
  explicit PluginException(PluginError err,
                           const std::string& stage = "");

  const std::string& stage_name() const noexcept { return stage_name_; }
  PluginError error() const noexcept { return error_; }

 private:
  std::string stage_name_;
  PluginError error_;
};

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_PLUGIN_EXCEPTION_H

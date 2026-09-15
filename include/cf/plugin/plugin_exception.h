// include/cf/plugin/plugin_exception.h
//
// 功能描述: Plugin framework 异常类型 (plugin-framework-stall commit A)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-15
//
// 设计:
//   - 派生自 std::runtime_error (有现成 message() API, 与 std 异常体系集成)
//   - 携带 stage_name (触发异常的 stage) 便于调试
//   - 由 PipeBuilder::run() 末尾在 should_throw() 为真时抛出
//   - 抛出时跳过 commit_storages() (与 pb-stall-loop spec Scenario 4 一致)

#ifndef CF_PLUGIN_PLUGIN_EXCEPTION_H
#define CF_PLUGIN_PLUGIN_EXCEPTION_H

#include <stdexcept>
#include <string>
#include <utility>

namespace cf {
namespace plugin {

class PluginException : public std::runtime_error {
 public:
  explicit PluginException(const std::string& stage_name,
                           const std::string& message)
      : std::runtime_error(message), stage_name_(stage_name) {}

  const std::string& stage_name() const noexcept { return stage_name_; }

 private:
  std::string stage_name_;
};

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_PLUGIN_EXCEPTION_H

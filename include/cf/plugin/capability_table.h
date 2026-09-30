// include/cf/plugin/capability_table.h
//
// 功能描述: Plugin 间 capability 协商表 (ADR-082 Plugin::negotiate())
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-30
//
// 设计 (ADR-082 §1.4):
//   - CapabilityTable: Plugin 间共享, 收集 provide/require
//   - provide(key, handle): Plugin 声明提供的能力
//   - require(key): Plugin 声明需要的能力, 返回 handle (缺失返回空 std::any)
//   - unresolved(): 返回所有未满足的 require key 列表
//   - 简化版: 不使用模板类型化 key (避免 binary bloat, ADR-082 R3 风险)
//     Plugin 业务代码可包装 handle 为具体类型 (std::any_cast)
//
// 约束:
//   - 头文件 (无 .cpp)
//   - cf::plugin namespace
//   - 仅依赖标准库 (<any>, <string>, <string_view>, <unordered_map>, <vector>)
//   - Phase C 首个消费 PoC: MulDivFsmPlugin::negotiate() 声明 requires
//
#ifndef CF_PLUGIN_CAPABILITY_TABLE_H
#define CF_PLUGIN_CAPABILITY_TABLE_H

#include <any>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cf {
namespace plugin {

class CapabilityTable {
 public:
  // provide: Plugin 声明提供某个 capability
  // 后注册的 provide 覆盖先注册的 (与 ADR-048 "unique" 规则冲突的 fallback)
  void provide(std::string_view key, std::any handle) {
    providers_[std::string(key)] = std::move(handle);
  }

  // require: Plugin 声明需要某个 capability
  // 返回 std::any (空 = 缺失). 同时记录到 requirements_ 用于 unresolved() 检查
  std::any require(std::string_view key) {
    std::string k(key);
    requirements_.push_back(k);
    auto it = providers_.find(k);
    if (it == providers_.end()) return {};
    return it->second;
  }

  // has: 不记录到 requirements_ 的 require (仅查询)
  bool has(std::string_view key) const {
    return providers_.find(std::string(key)) != providers_.end();
  }

  // unresolved: 返回所有 require() 调用中未满足的 key 列表
  std::vector<std::string> unresolved() const {
    std::vector<std::string> u;
    for (const auto& k : requirements_) {
      if (providers_.find(k) == providers_.end()) {
        u.push_back(k);
      }
    }
    return u;
  }

  // 已注册 providers 数 (测试用)
  std::size_t provider_count() const noexcept { return providers_.size(); }
  // 已 require 数 (测试用)
  std::size_t requirement_count() const noexcept { return requirements_.size(); }

 private:
  std::unordered_map<std::string, std::any> providers_;
  std::vector<std::string> requirements_;
};

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_CAPABILITY_TABLE_H

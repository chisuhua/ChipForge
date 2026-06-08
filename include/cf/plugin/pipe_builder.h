// include/cf/plugin/pipe_builder.h
//
// 功能描述: PipeBuilder 编排器 (Phase 0 P0 #4)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-08
//
// 设计:
//   - register_plugin: 接管 Plugin 所有权
//   - at_stage: 注册阶段回调
//   - declare_substage: 声明子阶段 (Phase 0 仅声明, 无深度调度)
//   - node_of_logic_stage: 查找阶段对应的 PipeNode
//   - build: 编译入口 (调用所有 Plugin 的 setup/build)
//   - run: 执行入口 (按注册顺序调用所有 at_stage 回调)
//
// 借鉴:
//   - chlib/stream_builder.h (链式 API)
//   - chlib/pipeline.h (阶段注册)

#ifndef CF_PLUGIN_PIPE_BUILDER_H
#define CF_PLUGIN_PIPE_BUILDER_H

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cf/plugin/pipe_node.h"
#include "cf/plugin/plugin_base.h"

namespace cf {
namespace plugin {

enum class Phase {
  EARLY,
  NORMAL,
  LATE
};

inline const char* phase_name(Phase p) noexcept {
  switch (p) {
    case Phase::EARLY:  return "EARLY";
    case Phase::NORMAL: return "NORMAL";
    case Phase::LATE:   return "LATE";
  }
  return "UNKNOWN";
}

class PipeBuilder {
 public:
  using StageCallback = std::function<void()>;

  PipeBuilder() = default;
  ~PipeBuilder() = default;

  PipeBuilder(const PipeBuilder&) = delete;
  PipeBuilder& operator=(const PipeBuilder&) = delete;

  void register_plugin(std::unique_ptr<PluginBase> plugin) {
    if (!plugin) throw std::invalid_argument("plugin is null");
    plugins_.push_back(std::move(plugin));
  }

  void at_stage(const std::string& stage_name, Phase phase, StageCallback cb) {
    if (stage_name.empty()) throw std::invalid_argument("empty stage name");
    if (!cb) throw std::invalid_argument("null callback");
    stages_.push_back(StageEntry{stage_name, phase, std::move(cb)});
    if (nodes_.find(stage_name) == nodes_.end()) {
      nodes_.emplace(stage_name, std::make_shared<PipeNode>(stage_name));
    }
  }

  void declare_substage(const std::string& parent, const std::string& sub, int /*depth*/ = 0) {
    substage_parent_[sub] = parent;
    if (nodes_.find(parent) == nodes_.end()) {
      nodes_.emplace(parent, std::make_shared<PipeNode>(parent));
    }
    if (nodes_.find(sub) == nodes_.end()) {
      nodes_.emplace(sub, std::make_shared<PipeNode>(sub));
    }
  }

  std::shared_ptr<PipeNode> node_of_logic_stage(const std::string& stage_name) const {
    auto it = nodes_.find(stage_name);
    if (it == nodes_.end()) return nullptr;
    return it->second;
  }

  void build() {
    for (auto& p : plugins_) p->setup(*this);
    for (auto& p : plugins_) p->build(*this);
  }

  void run() {
    for (auto& s : stages_) s.callback();
  }

  void reset_all() {
    for (auto& [_, n] : nodes_) n->reset();
  }

  std::size_t plugin_count() const noexcept { return plugins_.size(); }
  std::size_t stage_count() const noexcept { return stages_.size(); }
  std::size_t node_count() const noexcept { return nodes_.size(); }

  bool has_stage(const std::string& name) const {
    return std::any_of(stages_.begin(), stages_.end(),
                       [&](const StageEntry& s) { return s.name == name; });
  }

  std::vector<std::string> stage_names() const {
    std::vector<std::string> out;
    out.reserve(stages_.size());
    for (auto& s : stages_) out.push_back(s.name);
    return out;
  }

 private:
  struct StageEntry {
    std::string name;
    Phase phase;
    StageCallback callback;
  };

  std::vector<std::unique_ptr<PluginBase>> plugins_;
  std::vector<StageEntry> stages_;
  std::unordered_map<std::string, std::shared_ptr<PipeNode>> nodes_;
  std::map<std::string, std::string> substage_parent_;
};

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_PIPE_BUILDER_H

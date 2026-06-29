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
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
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
    // M4G-extend G.X: per-tid dispatch — for each tid in [0, n_threads),
    // dispatch set_tid(tid) to every plugin then run stages.
    // n_threads_ = 1 (default) preserves M4G baseline byte-identical behavior.
    for (std::uint8_t tid = 0; tid < n_threads_; ++tid) {
      for (auto& p : plugins_) p->set_tid(tid);
      for (auto& s : stages_) s.callback();
    }
    commit_storages();
  }

  // set_n_threads —— 配置 per-cycle dispatch 的 tid 数量 (M4G-extend G.X)
  // 默认 1: 单线程 byte-identical. SMT/超标的扩展通过 config.n_threads 注入.
  void set_n_threads(std::uint8_t n) { n_threads_ = n; }
  std::uint8_t n_threads() const noexcept { return n_threads_; }

  // ------------------------------------------------------------------------
  // 存储 commit 钩子注册 (Phase 1.3+)
  //
  // 业务 plugin 在 build() 期间通过此 API 注册一个 commit 钩子;
  // pb.run() 末尾 (所有 at_stage 回调执行之后) 会按注册顺序调用所有钩子.
  //
  // 用途: 对接 cf::plugin::storage::array_store<T, N> (见 storage.h)
  //   - Phase 1 (TLM):  array_store::commit() 是 no-op (单缓冲, 即写即读)
  //   - Phase 6 (RTL):  array_store::commit() 切换为双缓冲提交, 读返回
  //                     "上一周期 commit 提交值" — 对齐 ch_mem::sread
  //
  // 典型用法 (L1CachePlugin::build):
  //   pb.register_commit_hook([this] { tags_.commit(); });
  //   pb.register_commit_hook([this] { data_.commit(); });
  //   pb.register_commit_hook([this] { valid_.commit(); });
  //
  // 配套决策: ADR-040 (TLM→HDL 移植性约束)
  // 约束:
  //   - 必须从 Plugin::build() 内调用 (不在 at_stage 回调内)
  //   - 钩子按注册顺序执行 (保证依赖顺序: tags_ 在 data_ 之前)
  //   - 多次注册同一 storage 会被多次 commit (幂等性由 storage 自己负责)
  // ------------------------------------------------------------------------
  // M4G-extend G.X Gap B (M4G-extend-tid-and-hooks):
  //   - register_commit_hook() + commit_storages() = OoO 提交原语 (Phase 5+ ROB 设计)
  //   - cf::plugin::CtrlLink::flush_when(cond) = mispredict-squash 原语 (分支恢复)
  //   - 两者成对使用: commit_hook 在每拍提交指令, flush_when 在 mispredict 时清空
  //   - 参考: ip/cpu/docs/dse_architecture_v2_design_research.md §3 E.1 (ROB 设计)
  //   - 推迟到 Phase 5+ 的完整 OoO: ROB / IQ / PRF / LSQ / Rename / MUL-latency / Cache-latency
  // ------------------------------------------------------------------------
  using CommitHook = std::function<void()>;

  void register_commit_hook(CommitHook hook) {
    if (!hook) throw std::invalid_argument("null commit hook");
    commit_hooks_.push_back(std::move(hook));
  }

  std::size_t commit_hook_count() const noexcept { return commit_hooks_.size(); }

  // 立即执行所有 commit 钩子 (pb.run() 末尾自动调用, 一般不直接用)
  void commit_storages() {
    for (auto& h : commit_hooks_) h();
  }

  void reset_all() {
    for (auto& [_, n] : nodes_) n->reset();
  }

  std::size_t plugin_count() const noexcept { return plugins_.size(); }
  std::size_t stage_count() const noexcept { return stages_.size(); }
  std::size_t node_count() const noexcept { return nodes_.size(); }

  // plugins() —— 返回 plugin 列表只读引用 (M4.12, 供 CpuFactory 测试断言)
  const std::vector<std::unique_ptr<PluginBase>>& plugins() const noexcept {
    return plugins_;
  }

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
  std::vector<CommitHook> commit_hooks_;
  std::uint8_t n_threads_ = 1;  // M4G-extend: 默认 1, factory 端注入
};

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_PIPE_BUILDER_H

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

#include "cf/plugin/ctrl_link.h"
#include "cf/plugin/pipe_node.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/plugin_exception.h"

#ifdef CF_PLUGIN_USE_CH_MEM
// Forward declarations for CppHDL types (used in elaboration API only)
namespace ch::core {
class context;
class lnodeimpl;
struct ch_bool;
}  // namespace ch::core
#endif

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

#ifdef CF_PLUGIN_USE_CH_MEM
// ----------------------------------------------------------------------------
// StagePayloadMapEntry: type-erased payload connector descriptor
// Used to insert pipeline_reg between stages during elaboration.
// ----------------------------------------------------------------------------
struct StagePayloadMapEntry {
  std::string stage;             // Which stage's payload cell to update
  const PayloadKeyBase* key;     // Payload key (type-erased)

  // Connector functor: takes (prev_lnodeimpl*, stall, flush, name)
  // and returns the lnodeimpl* that the cell should reference.
  std::function<
      ch::core::lnodeimpl*(ch::core::lnodeimpl* /*prev*/,
                          ch::core::ch_bool /*stall*/,
                          ch::core::ch_bool /*flush*/,
                          const std::string& /*name*/)>
      connector;

  // Type-erased applier (internal): reads cell, calls connector, writes result
  // back. Created by register_stage_payload_connector when T is known.
  std::function<void(PipeBuilder&, ch::core::ch_bool, ch::core::ch_bool)> applier;
};
#endif

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
    // cpu-pipeline-stubs-replace commit A: 阶段+相位有序调度 (canonical order)
    // (stage_first_occurrence_order) × Phase(EARLY→NORMAL→LATE). 桶内保持稳定排序
    // (按 stages_ 插入序). n_threads 循环和 commit_storages 保留 M4G 行为.
    // 第一次 run() 时构建 canonical_order_ 缓存（缓存 stage 首现序）; 后续 at_stage
    // 不会影响 in-progress run() — 见 canonical_order() lazy 计算.
    //
    // plugin-framework-stall commit A: 插入 CtrlLink stall loop.
    // 每个 stage 入口先查 should_stall_stage(): true 则 skip 该 stage 的所有
    // phase 闭包, false 则按 EARLY→NORMAL→LATE 顺序执行 (与既有 cpu-pipeline-
    // stubs-replace commit A 行为一致). 末尾追加 throw_when 全局异常检查:
    // 任一 CtrlLink should_throw() 为真则抛 PluginException, 跳过 commit_storages().
    for (std::uint8_t tid = 0; tid < n_threads_; ++tid) {
      for (auto& p : plugins_) p->set_tid(tid);
      const auto order = canonical_stage_order();
      for (const auto& stage_name : order) {
        // plugin-framework-stall commit A: stall check (per-stage OR-merge)
        if (should_stall_stage(stage_name)) {
          continue;
        }
        // 同 stage+同 phase 桶内按 stages_ 插入序稳定排序
        for (int p_idx = 0; p_idx < 3; ++p_idx) {
          const Phase target_phase = static_cast<Phase>(p_idx);
          for (const auto& s : stages_) {
            if (s.name == stage_name && s.phase == target_phase) {
              s.callback();
            }
          }
        }
      }
    }
    // plugin-framework-stall commit A: throw_when 全局异常检查
    for (const auto& [stage, ctrls] : stage_ctrl_links_) {
      for (const auto& c : ctrls) {
        if (c && c->should_throw()) {
          throw PluginException(stage, "throw_when condition triggered");
        }
      }
    }
    commit_storages();
  }

 private:
  // 阶段首现序（按 stages_ 首次出现位置排序）。同阶段多个闭包按 stages_
  // 插入序稳定排序（同一 phase 桶内）。这是 cpu-pipeline-stubs-replace Layer 0
  // 修复：之前 run() 完全忽略阶段名按插入序调用，导致 RegFilePlugin 读闭包
  // (在 LATE 组) 物理上排在 ALU execute 闭包之后，破坏真实数据流。
  std::vector<std::string> canonical_stage_order() const {
    std::vector<std::string> order;
    order.reserve(stages_.size());
    for (const auto& s : stages_) {
      if (std::find(order.begin(), order.end(), s.name) == order.end()) {
        order.push_back(s.name);
      }
    }
    return order;
  }

 public:

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

  // ------------------------------------------------------------------------
  // plugin-framework-stall commit A: CtrlLink stage binding
  //
  // 设计: 用 shared_ptr<CtrlLink> 保持 CtrlLink = delete 拷贝约束,
  //       shared_ptr 持有 lambda 捕获对象, 生命周期由 PipeBuilder 管理.
  //
  // 多次调用同名 stage 累加 (不覆盖), 应在 pb.build() 之前调用.
  //
  // 注: flush_when / bypass 不框架自动消费 (推迟到 cpu-pipeline-mispredict)
  // ------------------------------------------------------------------------
  void register_ctrl_link(const std::string& stage_name,
                          std::shared_ptr<CtrlLink> ctrl) {
    if (stage_name.empty()) throw std::invalid_argument("empty stage name");
    if (!ctrl) throw std::invalid_argument("null ctrl_link");
    stage_ctrl_links_[stage_name].push_back(std::move(ctrl));
  }

  bool should_stall_stage(const std::string& stage_name) const {
    auto it = stage_ctrl_links_.find(stage_name);
    if (it == stage_ctrl_links_.end()) return false;
    for (const auto& c : it->second) {
      if (c && c->should_halt()) return true;
    }
    return false;
  }

  std::size_t ctrl_link_count(const std::string& stage_name) const noexcept {
    auto it = stage_ctrl_links_.find(stage_name);
    return (it == stage_ctrl_links_.end()) ? 0 : it->second.size();
  }

  std::shared_ptr<CtrlLink> get_ctrl_link(const std::string& stage_name,
                                          std::size_t index) const {
    auto it = stage_ctrl_links_.find(stage_name);
    if (it == stage_ctrl_links_.end()) return nullptr;
    if (index >= it->second.size()) return nullptr;
    return it->second[index];
  }

  void clear_ctrl_links() noexcept { stage_ctrl_links_.clear(); }

#ifdef CF_PLUGIN_USE_CH_MEM
  // ------------------------------------------------------------------------
  // Phase 6c M2 Spike 1: type-erased stage connector infrastructure
  //
  // register_stage_payload_connector - register a pipeline_reg connector
  //   between stages for the given payload key. The connector wraps the
  //   previous stage's signal in chlib::pipeline_reg<N>() and writes the
  //   pipelined value to the target stage's cell.
  //
  // commit_payload_map - execute all registered connectors, inserting
  //   pipeline registers into the elaborated circuit.
  //
  // elaborate(ctx) - run at_stage() callbacks for elaboration, then
  //   commit all registered payload connectors.
  // ------------------------------------------------------------------------

  template <typename T>
  void register_stage_payload_connector(
      const std::string& stage_name,
      const Payload<T>& key,
      std::function<
          ch::core::lnodeimpl*(ch::core::lnodeimpl* /*prev*/,
                              ch::core::ch_bool /*stall*/,
                              ch::core::ch_bool /*flush*/,
                              const std::string& /*name*/)> connector) {
    if (stage_name.empty()) throw std::invalid_argument("empty stage name");
    if (!connector) throw std::invalid_argument("null connector");

    // Capture stable global pointer (Payload<T> is a global static)
    const auto* key_ptr = &key;

    StagePayloadMapEntry entry;
    entry.stage = stage_name;
    entry.key = key_ptr;
    entry.connector = connector;

    // Create type-erased applier that knows T at compile time
    entry.applier = [stage_name, key_ptr, conn = std::move(connector)](
        PipeBuilder& pb,
        ch::core::ch_bool stall,
        ch::core::ch_bool flush) {
      auto node = pb.node_of_logic_stage(stage_name);
      if (!node) return;
      const auto& typed_key = *static_cast<const Payload<T>*>(key_ptr);
      auto& cell = node->payloads().template get<T>(typed_key);
      auto* prev = cell.impl();
      auto* result = conn(prev, stall, flush, "pipe_reg_" + stage_name);
      // Wrap result back into T (both ch_uint<N> and ch_bool have
      // explicit constructors from lnodeimpl*)
      T pipelined(result);
      cell = pipelined;
    };

    stage_payload_map_[stage_name].push_back(std::move(entry));
  }

  std::size_t stage_payload_count() const noexcept {
    std::size_t cnt = 0;
    for (const auto& [stage, entries] : stage_payload_map_) {
      cnt += entries.size();
    }
    return cnt;
  }

  void commit_payload_map(
      ch::core::ch_bool default_stall = ch::core::ch_bool(false),
      ch::core::ch_bool default_flush = ch::core::ch_bool(false)) {
    for (auto& [stage, entries] : stage_payload_map_) {
      for (auto& entry : entries) {
        if (entry.applier) {
          entry.applier(*this, default_stall, default_flush);
        }
      }
    }
  }

  // Phase 6c M2 Spike 2: elaborate() — run at_stage() callbacks in
  // canonical order (same as run() but without TLM commit, CtrlLink
  // stall check, or commit_storages), then insert pipeline registers
  // via commit_payload_map().
  void elaborate(ch::core::context& ctx) {
    const auto order = canonical_stage_order();
    for (const auto& stage_name : order) {
      for (int p_idx = 0; p_idx < 3; ++p_idx) {
        const Phase target_phase = static_cast<Phase>(p_idx);
        for (const auto& s : stages_) {
          if (s.name == stage_name && s.phase == target_phase) {
            s.callback();
          }
        }
      }
    }

    // Insert all registered pipeline registers (default stall=0, flush=0)
    ch::core::ch_bool default_stall(false);
    ch::core::ch_bool default_flush(false);
    commit_payload_map(default_stall, default_flush);
  }
#endif

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
  // plugin-framework-stall commit A: stage → vector<shared_ptr<CtrlLink>>
  // OR 合并语义在 should_stall_stage() 内执行
  std::unordered_map<std::string, std::vector<std::shared_ptr<CtrlLink>>>
      stage_ctrl_links_;
#ifdef CF_PLUGIN_USE_CH_MEM
  // Phase 6c M2 Spike 1: stage → vector<StagePayloadMapEntry>
  std::unordered_map<std::string, std::vector<StagePayloadMapEntry>>
      stage_payload_map_;
#endif
};

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_PIPE_BUILDER_H

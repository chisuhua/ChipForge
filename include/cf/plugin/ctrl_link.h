// include/cf/plugin/ctrl_link.h
//
// 功能描述: CtrlLink 控制 API (Phase 0 P0 #5) + Phase 6c 双模兼容
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-16 (Phase 6c M2: CtrlLink ch_bool化)
//
// 借鉴:
//   - chlib/pipeline.h::pipeline_stall_ctrl (OR 合并逻辑)
//   - chlib/stream.h::stream_halt_when (ch_stream valid/ready halt 模式)
//
// 双模语义:
//
//   维度        | TLM 模式 (默认)                          | CH_MEM 模式
//   ------------|------------------------------------------|----------------------------------------
//   条件类型    | std::function<bool()> 运行期谓词          | ch::core::ch_bool 硬件信号句柄
//   评估时机    | 每个 pb.run() 周期 should_*() 求值         | elaboration 期连成 OR 合并网络
//              |                                          | Simulator::tick() 读当前值
//   halt_when  | halt_when(std::function<bool()>)         | halt_when(ch_bool)
//   throw_when | 同上                                      | 同上
//   flush_when | 同上                                      | 同上
//   bypass     | bypass(key, std::function<bool()>)       | bypass(key, ch_bool)
//
// 编译开关 CF_PLUGIN_USE_CH_MEM (默认 OFF = TLM 兼容)
//   ON: halt_when(ch_bool) 等接 ch 信号句柄, 应该_halt() 通过 Simulator 读 OR 合并值
//   OFF: 原行为, std::function<bool()> 运行期谓词

#ifndef CF_PLUGIN_CTRL_LINK_H
#define CF_PLUGIN_CTRL_LINK_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cf/plugin/payload.h"

#ifdef CF_PLUGIN_USE_CH_MEM
#include <cstdio>

#include <ch.hpp>
#include <core/bool.h>
#include <core/reg.h>
#endif

namespace cf {
namespace plugin {

#ifdef CF_PLUGIN_USE_CH_MEM
// ============================================================================
// Phase 6c CH_MEM 模式: CtrlLink 持有 ch_bool 信号句柄 + elaboration OR 合并
// ============================================================================
class CtrlLink {
 public:
  // 注: 不需要 std::function; ch_bool 是 elaboration 期的硬件信号句柄
  using Condition = ch::core::ch_bool;

  CtrlLink() = default;
  ~CtrlLink() = default;

  // 注: CtrlLink 是 ch 信号持有者, 浅拷贝无副作用
  CtrlLink(const CtrlLink&) = default;
  CtrlLink& operator=(const CtrlLink&) = default;

  // ------------------------------------------------------------------------
  // halt_when / throw_when / flush_when: 注册 ch_bool 信号
  // 在 elaboration 期, PipeBuilder 把所有 halt_conds OR 合并接到 stage stall 信号
  // ------------------------------------------------------------------------
  CtrlLink& halt_when(Condition cond) {
    halt_conds_.push_back(cond);
    return *this;
  }

  // ------------------------------------------------------------------------
  // A1: 显式的"有状态"（跨拍保持）halt 条件入口
  //
  // 背景 (openspec change ctrl-link-chmem-reg-handle, 缺口 G2):
  //   `ch_reg<ch_bool>` 已经是 `ch_bool` 的派生类 (上游 core/reg.h:32
  //   `template <typename T> class ch_reg : public T`), 且其基类
  //   `logic_buffer<ch_bool>::node_impl_` 被上游指向 reg 的 **proxy 节点**
  //   (上游 lnode/reg.tpp: `static_cast<logic_buffer<T>*>(this)->node_impl_ = proxy_node;`)
  //   ⇒ 切片成 `ch_bool` 后, OR 门仍引用寄存器节点 ⇒ 随拍更新 ✓
  //   (能力早已具备, 本入口的增量是**让"这是有状态条件"在类型与文档上可见**).
  //
  // 契约:
  //   - OR 合并结果随该寄存器值**逐拍更新** (真 DAG 行为, 不读 C++ 侧缓冲)
  //   - `should_halt()` 对该条件**不可靠** (读 C++ 侧缓冲 ⇒ 陈旧值);
  //     必须走 DAG 消费路径 `OR_combined_halt_conds()`
  // ------------------------------------------------------------------------
  CtrlLink& halt_when_stateful(const ch::core::ch_reg<ch::core::ch_bool>& cond) {
    // 切片到基类: 保留 proxy 节点指针 ⇒ OR 门随拍更新
    halt_conds_.push_back(static_cast<ch::core::ch_bool>(cond));
    ++stateful_halt_count_;
    return *this;
  }

  // 便捷重载: 直接把 `ch_reg<ch_bool>` 传给 `halt_when` 时自动按"有状态条件"登记,
  // 避免"能编译能跑但意图被静默丢弃" (缺口 G2 的核心).
  // 既有 `halt_when(ch_bool)` 的签名与行为**完全不变** —— 本重载是纯增量.
  CtrlLink& halt_when(const ch::core::ch_reg<ch::core::ch_bool>& cond) {
    return halt_when_stateful(cond);
  }

  CtrlLink& throw_when(Condition cond) {
    throw_conds_.push_back(cond);
    return *this;
  }

  CtrlLink& flush_when(Condition cond) {
    flush_conds_.push_back(cond);
    return *this;
  }

  template <typename T>
  CtrlLink& bypass(const Payload<T>& key, Condition src_active) {
    bypass_map_[&key] = src_active;
    return *this;
  }

  // ------------------------------------------------------------------------
  // should_halt / should_throw / should_flush: 在 CH_MEM 模式下读 ch_bool OR 合并值
  //
  // ⚠ 语义契约 (openspec change ctrl-link-chmem-reg-handle, 缺口 G3):
  //   本组方法读的是 **C++ 侧缓冲** (`c.to_bool()`), **不是** Simulator 的
  //   data_map 当前值 ⇒ 对 "有状态条件" (由 `ch_reg<ch_bool>` 支撑, 跨拍保持)
  //   **永远读到陈旧/初始值**, 不可作为该条件的验证路径.
  //
  //   - 组合条件: 本组方法可靠 (行为与本 change 之前一致)
  //   - 有状态条件: 必须走 DAG 消费路径 `OR_combined_halt_conds()`
  //                 (返回的 ch_bool 节点引用寄存器节点 ⇒ 随拍更新 ✓)
  //
  //   为避免"静默返回陈旧值", 本组方法在存在有状态条件时:
  //     ① 递增 `unreliable_query_count()` (可被测试断言)
  //     ② 首次触发时 emit 一次 stderr 告警 (非静默)
  //
  // 注: 在 elaboration-only 模式下 (pb.elaborate() 之后无 run()), 这些方法
  //     实际上不被框架调用 (PipeBuilder 在 elaborate() 里直接连
  //     `halt_condition()` / `flush_condition()` 到 stage stall 信号).
  //     但保留 API 以兼容 M2-M3 业务代码.
  // ------------------------------------------------------------------------
  bool should_halt() const {
    note_unreliable_query("should_halt");
    bool result = false;
    for (const auto& c : halt_conds_) {
      result = result || static_cast<bool>(c.to_bool());
      if (result) return true;  // 短路
    }
    return result;
  }

  bool should_throw() const {
    note_unreliable_query("should_throw");
    bool result = false;
    for (const auto& c : throw_conds_) {
      result = result || static_cast<bool>(c.to_bool());
      if (result) return true;
    }
    return result;
  }

  bool should_flush() const {
    note_unreliable_query("should_flush");
    bool result = false;
    for (const auto& c : flush_conds_) {
      result = result || static_cast<bool>(c.to_bool());
      if (result) return true;
    }
    return result;
  }

  bool bypass_active(const PayloadKeyBase& /*key*/) const {
    // PoC: M3+ 实现 (需配合 PipeBuilder 读 bypass_map_)
    return false;
  }

  std::size_t halt_count() const noexcept { return halt_conds_.size(); }
  std::size_t throw_count() const noexcept { return throw_conds_.size(); }
  std::size_t flush_count() const noexcept { return flush_conds_.size(); }
  std::size_t bypass_count() const noexcept { return bypass_map_.size(); }

  void clear() noexcept {
    halt_conds_.clear();
    throw_conds_.clear();
    flush_conds_.clear();
    bypass_map_.clear();
  }

  // ------------------------------------------------------------------------
  // A2: OR 合并的**唯一实现** — 返回 ch_bool **DAG 节点**, 供消费方接进自己的 DAG.
  //
  //   - 空集合 → 常量假值 (no stall)
  //   - 单条件 → 直接返回该条件 (**不退化到恒假**)
  //   - 多条件 → 逐个 OR 合并, 建真 OR 门 DAG
  //
  //   用 `operator|` (1-bit OR) 而非已 deprecated 的 `ch_bool::operator||`
  //   (上游 core/operators_logic.h:166 的 [[deprecated]] 标注指向 `operator|`).
  //
  //   既有 `halt_condition()` / `flush_condition()` 改为 **inline alias 转发**
  //   (A6) —— 全仓只此一处合并实现, 杜绝"三个语义重叠的合并 API".
  //   既有调用点 (`pipe_builder.h` elaborate() 里的 stall/flush 聚合) 零改动.
  // ------------------------------------------------------------------------
  ch::core::ch_bool OR_combined_halt_conds() const {
    return or_merge_conds(halt_conds_);
  }

  // W3-1: halt_condition() — OR 合并 halt_conds_ 返回 ch_bool 信号 (A6: inline alias)
  ch::core::ch_bool halt_condition() const { return OR_combined_halt_conds(); }

  // W3-2: flush_condition() — OR 合并 flush_conds_ 返回 ch_bool 信号 (A6: inline alias)
  ch::core::ch_bool flush_condition() const { return or_merge_conds(flush_conds_); }

  // ------------------------------------------------------------------------
  // 有状态条件诊断 (A3) — 让"should_*() 对有状态条件不可靠"可被观测/断言
  // ------------------------------------------------------------------------

  // 已登记的有状态 halt 条件数量
  std::size_t stateful_halt_count() const noexcept { return stateful_halt_count_; }

  // 是否含至少一个有状态 halt 条件
  bool has_stateful_conditions() const noexcept { return stateful_halt_count_ > 0; }

  // 在"存在有状态条件"的前提下调用 should_*() 的次数
  // (这些查询对有状态条件**不可靠** ⇒ 调用方应改走 OR_combined_halt_conds())
  std::size_t unreliable_query_count() const noexcept { return unreliable_query_count_; }

 private:
  // 唯一的 OR 合并实现 (A2/A6)
  static ch::core::ch_bool or_merge_conds(const std::vector<Condition>& conds) {
    if (conds.empty()) return ch::core::ch_bool(false);
    ch::core::ch_bool result = conds[0];
    for (std::size_t i = 1; i < conds.size(); ++i) {
      result = result | conds[i];  // operator| (1-bit OR), 非 deprecated operator||
    }
    return result;
  }

  // 存在有状态条件时, 记一次"不可靠查询" + 首次 emit 告警 (非静默)
  void note_unreliable_query(const char* which) const {
    if (stateful_halt_count_ == 0) return;
    ++unreliable_query_count_;
    if (!stateful_warned_) {
      stateful_warned_ = true;
      std::fprintf(stderr,
                   "[cf::plugin::CtrlLink] WARNING: %s() called while %zu "
                   "stateful (ch_reg<ch_bool>) halt condition(s) are registered.\n"
                   "  should_*() reads the C++-side buffer (stale for stateful "
                   "conditions).\n"
                   "  Use OR_combined_halt_conds() (DAG path) instead.\n",
                   which, stateful_halt_count_);
    }
  }

  std::vector<Condition> halt_conds_;
  std::vector<Condition> throw_conds_;
  std::vector<Condition> flush_conds_;
  std::unordered_map<const PayloadKeyBase*, Condition> bypass_map_;
  std::size_t stateful_halt_count_ = 0;
  mutable std::size_t unreliable_query_count_ = 0;
  mutable bool stateful_warned_ = false;
};

#else
// ============================================================================
// Phase 0/1 TLM 模式 (默认): 原行为 std::function<bool()> 运行期谓词
// ============================================================================
class CtrlLink {
 public:
  using Condition = std::function<bool()>;

  CtrlLink() = default;
  ~CtrlLink() = default;

  CtrlLink(const CtrlLink&) = delete;
  CtrlLink& operator=(const CtrlLink&) = delete;

  CtrlLink& halt_when(Condition cond) {
    if (!cond) return *this;
    halt_conds_.push_back(std::move(cond));
    return *this;
  }

  CtrlLink& throw_when(Condition cond) {
    if (!cond) return *this;
    throw_conds_.push_back(std::move(cond));
    return *this;
  }

  CtrlLink& flush_when(Condition cond) {
    if (!cond) return *this;
    flush_conds_.push_back(std::move(cond));
    return *this;
  }

  template <typename T>
  CtrlLink& bypass(const Payload<T>& key, Condition src_active) {
    bypass_map_[&key] = std::move(src_active);
    return *this;
  }

  bool should_halt() const {
    for (auto& c : halt_conds_) if (c()) return true;
    return false;
  }

  bool should_throw() const {
    for (auto& c : throw_conds_) if (c()) return true;
    return false;
  }

  bool should_flush() const {
    for (auto& c : flush_conds_) if (c()) return true;
    return false;
  }

  bool bypass_active(const PayloadKeyBase& key) const {
    auto it = bypass_map_.find(&key);
    if (it == bypass_map_.end()) return false;
    return it->second ? it->second() : false;
  }

  std::size_t halt_count() const noexcept { return halt_conds_.size(); }
  std::size_t throw_count() const noexcept { return throw_conds_.size(); }
  std::size_t flush_count() const noexcept { return flush_conds_.size(); }
  std::size_t bypass_count() const noexcept { return bypass_map_.size(); }

  void clear() noexcept {
    halt_conds_.clear();
    throw_conds_.clear();
    flush_conds_.clear();
    bypass_map_.clear();
  }

 private:
  std::vector<Condition> halt_conds_;
  std::vector<Condition> throw_conds_;
  std::vector<Condition> flush_conds_;
  std::unordered_map<const PayloadKeyBase*, Condition> bypass_map_;
};
#endif  // CF_PLUGIN_USE_CH_MEM

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_CTRL_LINK_H
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
#include <ch.hpp>
#include <core/bool.h>
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
  // 注: 在 elaboration-only 模式下 (pb.elaborate() 之后无 run()), 这些方法
  //     实际上不被框架调用. 但保留 API 以兼容 M2-M3 业务代码 (CtrlLink 注册后
  //     PipeBuilder 直接连到 stage stall 信号, 不调用 should_*()).
  //
  //     如果调用方需要"在 elaboration 期查看 OR 合并后的值", 可用:
  //       auto stall = OR_combined_halt_conds(); // 返回 ch_bool
  // ------------------------------------------------------------------------
  bool should_halt() const {
    // PoC: 用 ch_bool::to_bool() 逐个读 OR 合并. M2 实装时改成调用 simulator.
    bool result = false;
    for (const auto& c : halt_conds_) {
      result = result || static_cast<bool>(c.to_bool());
      if (result) return true;  // 短路
    }
    return result;
  }

  bool should_throw() const {
    bool result = false;
    for (const auto& c : throw_conds_) {
      result = result || static_cast<bool>(c.to_bool());
      if (result) return true;
    }
    return result;
  }

  bool should_flush() const {
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

  // PoC: M2 阶段实现 OR 合并方法, 返回 ch_bool 信号 (供 PipeBuilder 连线)
  // ch_bool OR_combined_halt() const {
  //   if (halt_conds_.empty()) return ch_bool(false);
  //   ch_bool result = halt_conds_[0];
  //   for (size_t i = 1; i < halt_conds_.size(); ++i) {
  //     result = result || halt_conds_[i];
  //   }
  //   return result;
  // }

 private:
  std::vector<Condition> halt_conds_;
  std::vector<Condition> throw_conds_;
  std::vector<Condition> flush_conds_;
  std::unordered_map<const PayloadKeyBase*, Condition> bypass_map_;
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
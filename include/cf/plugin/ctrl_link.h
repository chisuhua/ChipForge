// include/cf/plugin/ctrl_link.h
//
// 功能描述: CtrlLink 控制 API (Phase 0 P0 #5)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-08
//
// 借鉴:
//   - chlib/pipeline.h::pipeline_stall_ctrl (OR 合并逻辑)

#ifndef CF_PLUGIN_CTRL_LINK_H
#define CF_PLUGIN_CTRL_LINK_H

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cf/plugin/payload.h"

namespace cf {
namespace plugin {

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

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_CTRL_LINK_H

// include/cf/plugin/multi_cycle_fsm.h
//
// 功能描述: 框架级多周期 FSM 通用模板 (ADR-046 v2.0 §2.1.1 算术豁免配套)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-10 (mfc-extract-fsm-h Step 2)
//
// 设计 (spec.md Requirement + design.md §3):
//   - 单一 FSM 描述同时服务 TLM 与 CH_MEM 两种编译模式
//   - TLM 模式: ad-hoc enum class + tick() (ADR-048 兼容, ADR-025 tick 禁令豁免
//     因 FsmBase 继承 PluginBase 后 tick() 隐藏 private-deleted 基类 tick)
//   - CH_MEM 模式: chlib::ch_state_machine DSL (#ifdef CF_PLUGIN_USE_CH_MEM)
//   - negotiate() 提供 "multi_cycle_fsm" capability (ADR-082)
//   - 注意: 本文件是框架头 (include/cf/plugin/), check_plugin_portability.sh
//     Check 2 只扫 ip/*/plugins/*.h — 框架头允许 #ifdef CH_MEM 块 (同 payload.h)
//
// 约束:
//   - 不 #define CF_PLUGIN_USE_CH_MEM (Check 6: 仅允许 CMake 命令行)
//   - StateEnum 必须是 enum class, entry 状态 enum 值必须为 0 (ADR-046 §2.3
//     state_reg 初值 0_d 约束)
//   - NStates ∈ [2, 16]
//
// Design gap 1 修正 (rdd-planner P1 记录): FsmBase 必须继承 PluginBase,
//   否则 register_plugin(std::unique_ptr<PluginBase>) 无法接收派生类;
//   FsmBase::tick() 自然隐藏 PluginBase private-deleted tick(), 无歧义.

#ifndef CF_PLUGIN_MULTI_CYCLE_FSM_H
#define CF_PLUGIN_MULTI_CYCLE_FSM_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "cf/plugin/capability_table.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"

#ifdef CF_PLUGIN_USE_CH_MEM
#include <chlib/state_machine.h>
#include <core/context.h>
#endif

namespace cf {
namespace plugin {
namespace multi_cycle_fsm {

template <typename StateEnum, std::size_t NStates>
class FsmBase : public cf::plugin::PluginBase {
  static_assert(std::is_enum_v<StateEnum>, "StateEnum must be enum class");
  static_assert(NStates >= 2 && NStates <= 16, "NStates must be in [2, 16]");

 public:
  // ---- TLM 模式 API (默认编译) ----
  // 派生类 override: 状态转移逻辑 (从 advance_fsm switch 提取)
  virtual void on_tick(StateEnum /*current*/) = 0;

  StateEnum state() const noexcept { return state_; }

  // 推进 FSM 一拍: 隐藏 PluginBase private-deleted tick(), 无歧义
  void tick() { on_tick(state_); }

  // 兼容层: 业务测试 helper tick_state() 的薄包装 (mul_div_fsm.h 现有 API 不破坏)
  void tick_state() { tick(); }

  // ---- ADR-082 negotiate: 声明 multi_cycle_fsm capability ----
  void negotiate(::cf::plugin::CapabilityTable& cap) override {
    cap.provide("multi_cycle_fsm", this);
  }

  // ---- build() 默认空实现 (派生类 override 注册 at_stage) ----
  void build(::cf::plugin::PipeBuilder& /*pb*/) override {}

#ifdef CF_PLUGIN_USE_CH_MEM
  // ---- CH_MEM 模式 API (ch_state_machine DSL) ----
  // 派生类 override (实现在 _chmem.h 配对文件):
  //   on_active_state: 每 state 的 on_active 回调 (接线 transition_when)
  //   next_state:      状态转换条件查询 (可选)
  virtual void on_active_state(StateEnum /*current*/) {}
  virtual StateEnum next_state(StateEnum current) { return current; }

  chlib::ch_state_machine<StateEnum, NStates>& ch_fsm() { return ch_fsm_; }
  ch::core::context* ch_context() const noexcept { return ch_ctx_; }
  void set_ch_context(ch::core::context* ctx) noexcept { ch_ctx_ = ctx; }

  // 一次性构建 CH_MEM DSL (elaborate 期调一次, 同旧 create_fsm() 语义)
  void create_chmem_fsm() {
    if (ch_fsm_built_) {
      // Already built; nothing to do. 用 if/else 包裹 (非早返), 与
      // check_plugin_portability.sh at_stage 启发式保持一致.
    } else {
      ch_fsm_built_ = true;
      if (!ch_ctx_) {
        // Fallback: 私有 context. 业务测试应先 set_ch_context().
        ch_ctx_ = new ch::core::context("multi_cycle_fsm_ctx");
      }
      ch::core::ctx_swap guard(ch_ctx_);
      ch_fsm_.set_entry(static_cast<StateEnum>(0));  // entry 必须 enum 值 0
      // 每个 state 注册 on_active → 派生类 on_active_state hook 接线 transition_when
      for (std::size_t i = 0; i < NStates; ++i) {
        const StateEnum s = static_cast<StateEnum>(i);
        ch_fsm_.state(s).on_active([this, s]() { this->on_active_state(s); });
      }
      ch_fsm_.build();
    }
  }

  // 兼容层: 旧 API 名 create_fsm() → create_chmem_fsm() (proposal R1 mitigation,
  // deprecate 1 version 后删除)
  void create_fsm() { create_chmem_fsm(); }

  // 当前状态输出信号 (ch_uint, 供 Simulator get_value 断言)
  ch::core::ch_uint<chlib::ch_state_machine<StateEnum, NStates>::STATE_BITS>
  state_out() {
    create_chmem_fsm();
    ch::core::ctx_swap guard(ch_ctx_);
    return ch_fsm_.current_state_uint();
  }

 protected:
  chlib::ch_state_machine<StateEnum, NStates> ch_fsm_;
  ch::core::context* ch_ctx_ = nullptr;
  bool ch_fsm_built_ = false;
#endif  // CF_PLUGIN_USE_CH_MEM

 protected:
  StateEnum state_ = static_cast<StateEnum>(0);
};

}  // namespace multi_cycle_fsm
}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_MULTI_CYCLE_FSM_H

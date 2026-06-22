// include/cf/plugin/pipe_node.h
//
// 功能描述: PipeNode 节点 (Phase 0 P0 #3) —— 5 态状态机 + Phase 1.5 仲裁
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - 封装 PayloadStore (类型擦除的 Key-Value 存储)
//   - 简单状态机: IDLE / FIRING / MOVING / BLOCKED / CANCELING
//   - 提供 is_*() 派生状态查询
//   - operator()(Payload<T>&) 提供类型安全访问
//   - Phase 1.5+: 集成 PipeArbitration arb_ 字段 (M1.6, 蓝图 §6.2.3)
//     注意: 5 态方法与 arb_ 字段并存, 互不委托。 业务 Plugin 可自由选择
//     用 5 态方法 (老 API) 或 arb_ 字段 (新 API, 跨阶段 IPC 更轻量)
//
// 借鉴:
//   - declarative-hybrid-framework.md §7.1 (PipeNode 设计)
//   - CppHDL ch_state_machine 内部结构 (状态字段)

#ifndef CF_PLUGIN_PIPE_NODE_H
#define CF_PLUGIN_PIPE_NODE_H

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "cf/plugin/payload.h"
#include "cf/plugin/pipe_arbitration.h"

namespace cf {
namespace plugin {

class PipeNode {
 public:
  enum class State {
    IDLE,       // 空闲, 无 valid
    FIRING,     // valid 已置位, 等待下游 ready
    MOVING,     // valid && ready 握手完成, 数据正在搬移
    BLOCKED,    // 下游未 ready, 等待
    CANCELING   // 收到 cancel 信号, 清理中
  };

  explicit PipeNode(std::string name) : name_(std::move(name)) {}

  ~PipeNode() = default;

  PipeNode(const PipeNode&) = delete;
  PipeNode& operator=(const PipeNode&) = delete;
  PipeNode(PipeNode&&) = default;
  PipeNode& operator=(PipeNode&&) = default;

  const std::string& name() const noexcept { return name_; }

  State state() const noexcept { return state_; }

  bool is_idle() const noexcept { return state_ == State::IDLE; }
  bool is_firing() const noexcept { return state_ == State::FIRING; }
  bool is_moving() const noexcept { return state_ == State::MOVING; }
  bool is_blocked() const noexcept { return state_ == State::BLOCKED; }
  bool is_canceling() const noexcept { return state_ == State::CANCELING; }

  // 状态机驱动 (由 PipeBuilder::run() 调用)
  void assert_valid() {
    if (state_ == State::IDLE) state_ = State::FIRING;
  }
  void assert_ready() {
    if (state_ == State::FIRING) state_ = State::MOVING;
    else if (state_ == State::BLOCKED) state_ = State::FIRING;
  }
  void deassert_ready() {
    if (state_ == State::MOVING) state_ = State::BLOCKED;
  }
  void cancel() {
    if (state_ != State::IDLE) state_ = State::CANCELING;
  }
  void complete_cancel() {
    if (state_ == State::CANCELING) state_ = State::IDLE;
  }
  void reset() noexcept { state_ = State::IDLE; }

  // Payload 访问 (类型安全)
  template <typename T>
  T& operator()(const Payload<T>& key) {
    return payloads_.get(key);
  }

  template <typename T>
  const T& operator()(const Payload<T>& key) const {
    return payloads_.get(key);
  }

  template <typename T>
  void put(const Payload<T>& key, T value) {
    payloads_.put(key, std::move(value));
  }

  template <typename T>
  bool has(const Payload<T>& key) const {
    return payloads_.has(key);
  }

  // 直接访问 PayloadStore (供 CtrlLink 等需要细粒度控制的场景)
  PayloadStore& payloads() noexcept { return payloads_; }
  const PayloadStore& payloads() const noexcept { return payloads_; }

  // 工厂: create_node(name) -> unique_ptr<PipeNode>
  static std::unique_ptr<PipeNode> create(std::string name) {
    return std::make_unique<PipeNode>(std::move(name));
  }

  // ------------------------------------------------------------------------
  // Phase 1.5+: PipeArbitration arb_ 字段 (M1.6, 蓝图 §6.2.3)
  //
  // 用法 (新代码, 跨阶段 IPC 推荐):
  //   n->arb().assert_valid();
  //   n->arb().assert_ready();
  //   if (n->arb().fired()) { ... }
  //
  // 与 5 态方法并存, 互不委托:
  //   - 5 态方法 (assert_valid/assert_ready/...) 保持原状, 内部用 state_ 字段
  //   - arb_ 字段独立, 业务 Plugin 自由选择
  //   - 蓝图意图: 长期看, 老 Plugin 可逐步迁移到 arb_, 但不强制
  //
  // const 访问: arb() 返回 const 引用 (只读)
  // 可写访问:  arb_mut() 返回引用 (供老代码测试或迁移期)
  // ------------------------------------------------------------------------
  const PipeArbitration& arb() const noexcept { return arb_; }
  PipeArbitration& arb_mut() noexcept { return arb_; }

  // arb_ 直接访问 (兼容老代码别名, 等价 arb_mut())
  PipeArbitration& arbitration() noexcept { return arb_; }

  // ------------------------------------------------------------------------
  // M5-DSE M5.18: lane 派发字段 (2-wide superscalar, decision.md Decision 3)
  //
  // 语义:
  //   - lane 标识本 node 当前 cycle 所属的 dispatch lane (0..n_lanes-1)
  //   - 单发射路径 (n_lanes=1) lane_ 永远 = 0, byte-identical to baseline
  //   - 2-wide 路径: factory 端在 at_stage 闭包内 fetch_add(1) % n_lanes 决定 lane
  //   - 仅是 factory 端调度状态, plugin 不需要感知 (与 m4g-extend tid 模式同形)
  //
  // 设计:
  //   - 字段: lane_ 初始 0, set_lane 不抛异常, get_lane 返回当前值
  //   - 与 arb_ 字段并存, 不委托 (5 态方法 + arb_ + lane_ 三者独立)
  //   - 不暴露到 Node 外部, 仅供 set_lane 触发; 业务 Plugin 可读
  // ------------------------------------------------------------------------
  void set_lane(std::uint8_t lane) noexcept { lane_ = lane; }
  std::uint8_t lane() const noexcept { return lane_; }

  static const char* state_name(State s) noexcept {
    switch (s) {
      case State::IDLE:      return "IDLE";
      case State::FIRING:    return "FIRING";
      case State::MOVING:    return "MOVING";
      case State::BLOCKED:   return "BLOCKED";
      case State::CANCELING: return "CANCELING";
    }
    return "UNKNOWN";
  }

 private:
  std::string name_;
  State state_ = State::IDLE;
  PayloadStore payloads_;
  // Phase 1.5+: 仲裁字段 (M1.6, 蓝图 §6.2.3)。 默认构造: 全 false (idle)。
  // 与 state_ 字段并存, 不委托; 业务 Plugin 自选用法。
  PipeArbitration arb_;
  // M5-DSE M5.18: lane 派发字段 (2-wide superscalar). 默认 0 = 单发射 baseline.
  std::uint8_t lane_ = 0;
};

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_PIPE_NODE_H

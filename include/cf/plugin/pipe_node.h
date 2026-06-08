// include/cf/plugin/pipe_node.h
//
// 功能描述: PipeNode 节点 (Phase 0 P0 #3)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-08
//
// 设计:
//   - 封装 PayloadStore (类型擦除的 Key-Value 存储)
//   - 简单状态机: IDLE / FIRING / MOVING / BLOCKED / CANCELING
//   - 提供 is_*() 派生状态查询
//   - operator()(Payload<T>&) 提供类型安全访问
//
// 借鉴:
//   - declarative-hybrid-framework.md §7.1 (PipeNode 设计)
//   - CppHDL ch_state_machine 内部结构 (状态字段)

#ifndef CF_PLUGIN_PIPE_NODE_H
#define CF_PLUGIN_PIPE_NODE_H

#include <memory>
#include <string>
#include <utility>

#include "cf/plugin/payload.h"

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
};

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_PIPE_NODE_H

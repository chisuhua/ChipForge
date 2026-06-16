// include/cf/plugin/pipe_arbitration.h
//
// 功能描述: 仲裁三态结构 (Phase 1.5+) —— Plugin 跨阶段 IPC 握手与取消
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计动机:
//   cf_plugin Phase 0 的 PipeNode 已经有 5 态状态机 (IDLE/FIRING/MOVING/
//   BLOCKED/CANCELING, 见 pipe_node.h) + 5 个状态转移方法 (assert_valid/
//   assert_ready/deassert_ready/cancel/complete_cancel)。
//
//   但这 5 态是"驱动方写、读方查"模型 —— 业务 Plugin 难以独立组合。
//   蓝图 §1.1.5 要求: 提取 valid/ready/cancel 三态为独立小类, 让
//   StageLink / DirectLink / CtrlLink 都能组合, 而非各自重复实现。
//
//   本文件提供:
//     - PipeArbitration 三态结构 (valid/ready/cancel + 派生方法)
//     - Move 语义支持, 避免复制开销
//     - 编译期零开销 (POD 结构, 无虚函数)
//
// 借鉴:
//   - chlib/pipeline.h::pipeline_stall_ctrl (OR 合并逻辑)
//   - VexRiscv Plugin.scala arbitration (valid/ready 握手)
//
// 配套决策: ADR-033 (Plugin-style 强制); 蓝图 §6.2.3
//
// 约束:
//   - 头文件 (无 .cpp)
//   - POD 结构, 编译期零开销
//   - 不引入 ch::core::context 依赖 (TLM 模式禁止)
//   - 与 PipeNode 5 态方法并存: 现有 L1CachePlugin 调用方式不变,
//     内部可委托到 arb_ 字段 (C3 commit 实施)
//
// 与蓝图 §6.2.3 的微小调整:
//   蓝图原建议放 ip/cpu/core/pipe_arbitration.h (IP 层); 实际放 cf_plugin
//   层, 因为仲裁是通用框架概念。 理由: 任何 Plugin (CPU/Cache/Memory) 都
//   需仲裁, 不应耦合到 IP 层。 详情见 C2 commit message。

#ifndef CF_PLUGIN_PIPE_ARBITRATION_H
#define CF_PLUGIN_PIPE_ARBITRATION_H

#include <cstddef>
#include <type_traits>

namespace cf {
namespace plugin {

// ----------------------------------------------------------------------------
// PipeArbitration —— 跨阶段 IPC 握手与取消的三态结构
//
// 字段语义 (与 chlib pipeline_stall_ctrl / VexRiscv arbitration 同构):
//   - valid   上游产生有效数据, 请求握手
//   - ready   下游可接受, 表示握手成功
//   - cancel  取消 (异常 / 分支预测错误 / flush 等)
//
// 派生方法 (用于 at_stage 闭包内, 全部 inline):
//   - fired()    握手成功 (valid && ready, !cancel)
//   - blocked()  valid 已置但下游未 ready
//   - canceling() 收到 cancel 信号, 清理中
//   - idle()     空闲
//   - reset()    全部置 false
// ----------------------------------------------------------------------------
struct PipeArbitration {
  bool valid   = false;
  bool ready   = false;
  bool cancel  = false;

  // ------------------------------------------------------------------------
  // 派生状态查询 (全部 inline, 闭包内零开销)
  // ------------------------------------------------------------------------
  // 握手成功: valid && ready, 且未取消
  constexpr bool fired() const noexcept {
    return valid && ready && !cancel;
  }
  // 阻塞: valid 已置, 但下游未 ready (等待反压)
  constexpr bool blocked() const noexcept {
    return valid && !ready && !cancel;
  }
  // 取消中: cancel 信号已置
  constexpr bool canceling() const noexcept {
    return cancel;
  }
  // 空闲: 全部置 false
  constexpr bool idle() const noexcept {
    return !valid && !ready && !cancel;
  }

  // ------------------------------------------------------------------------
  // 状态转移 (与 PipeNode 5 态方法语义同构, 但更轻量, 不含 state enum)
  // ------------------------------------------------------------------------
  // 置 valid (由上游产生有效数据时调用)
  constexpr void assert_valid() noexcept {
    valid = true;
  }
  // 置 ready (由下游可接受时调用)
  constexpr void assert_ready() noexcept {
    ready = true;
  }
  // 取消 ready (由下游反压时调用)
  constexpr void deassert_ready() noexcept {
    ready = false;
  }
  // 置 cancel (异常 / flush / 分支预测错误)
  constexpr void cancel_op() noexcept {
    cancel = true;
    valid = false;  // cancel 隐含放弃 valid (语义上正在清理)
  }
  // 取消完成 (清理结束, 回到 idle)
  constexpr void complete_cancel() noexcept {
    cancel = false;
    ready = false;
  }
  // 一次性置 valid + ready (用于测试或初始化)
  constexpr void set_fired() noexcept {
    valid = true;
    ready = true;
  }
  // 全部置 false
  constexpr void reset() noexcept {
    valid = false;
    ready = false;
    cancel = false;
  }
};

// ----------------------------------------------------------------------------
// 编译期检查: POD 结构 + 体积 (必须放在类定义之后, 此时类型完整)
// ----------------------------------------------------------------------------
static_assert(std::is_trivially_copyable<PipeArbitration>::value,
              "PipeArbitration must be trivially copyable "
              "(at_stage 闭包内零开销访问)");
static_assert(std::is_standard_layout<PipeArbitration>::value,
              "PipeArbitration must be standard-layout (3 bool, 1 byte packed, no padding)");
static_assert(sizeof(PipeArbitration) == sizeof(bool) * 3,
              "PipeArbitration must be 3 bytes (no padding)");

}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_PIPE_ARBITRATION_H

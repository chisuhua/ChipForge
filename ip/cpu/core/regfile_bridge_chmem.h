// ip/cpu/core/regfile_bridge_chmem.h
//
// 功能描述: 寄存器堆单元（层 2）→ 核内 payload（层 1）的**桥接 plugin**
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-29
//
// ============================================================================
// 归属 (ADR-091 v1.12 §D3a.7 —— 两层范式)
// ============================================================================
//
// ★★★ 本文件是「**层 2 → 层 1 的唯一通道**」（★ ADR-091 v1.12 §D3a.7）
//   · ★ **层 2** = `RegFileComponent`（★ `ip/cpu/core/regfile_chmem.h`）
//   · ★ **层 1** = `PluginBase` + `at_stage` + `Payload<T>`
//   · ★ 本文件 = 桥接 plugin（★ 读 Component 的 `io()` ⇒ 写 `Payload`）
//
// ★★ **为什么必须有桥接层**（★ ADR-091 v1.12 §D3a.7）：
//   ★ `ch::Component` **无 stage 概念**（★ 用「直连 DAG」保证顺序）
//   ⇒ ★ 它的输出**不进任何 stage** ⇒ ★ 核内其它 plugin 若用 `Payload`
//   读它 ⇒ ★ **跨 stage 可见性无保障** ⇒ ★ **破 G-1n**
//   ⇒ ★ ⇒ ★ 层 2 的输出 MUST 由本桥接 plugin **发布成 `Payload`**，
//     ★ 由 `Payload` 的 stage 顺序机制保证可见性。
//
// ★★ **桥接是「直连」语义**（★ 不经 payload 中转）⇒ ★ 它**不受** G-1n
//   约束；★ 但**本 plugin 作为 payload 生产者**，★ MUST 注册在
//   **消费者之前**（★ G-1n 的「注册顺序 = 首现序」仍适用）。
//
// ============================================================================
// ★★★ ctx 共用（硬约束 G-1p）—— 本文件是 G-1p 的**唯一执行点**
// ============================================================================
//
// ★ `regfile_->build(pb.ctx())` ⇒ ★ **必须传 ChipForge 的 ctx**
//   ⇒ ★ **MUST NOT** `build(nullptr)` ⇒ ★ 子 ctx ⇒ node-id aliasing
//   ⇒ ★ 组件的 output ports 从 simulator 的 `data_map` 消失
//   ⇒ ★ 观测**静默读不到** ⇒ ★ **最坏假绿**
//   ⇒ ★ 上游已踩此坑（★ `CppHDL/include/device.h:12-19` 明载）。
//
// ★★ **build 时机**：★ `Component::build()` MUST 在**所有 `at_stage` 闭包
//   之前**完成 ⇒ ★ ⇒ ★ 本 plugin 在 **`setup(pb)`** 内建 Component
//   （★ `PipeBuilder::build()` 按 `plugins_` 顺序调 `setup()`
//   ⇒ ★ 早于 `elaborate()` 的 stage 闭包）★。
//
// ★★ **MUST NOT** include `<ch.hpp>`（★ ADR-091 G-1b）。
//
// ============================================================================

#ifndef CF_IP_CPU_CORE_REGFILE_BRIDGE_CHMEM_H
#define CF_IP_CPU_CORE_REGFILE_BRIDGE_CHMEM_H

// ★ 只 include 具体头（MUST NOT <ch.hpp> —— G-1b）
#include <core/bool.h>
#include <core/uint.h>

#include <memory>
#include <string>

#include "cf/plugin/payload.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/core/regfile_chmem.h"

namespace cf {
namespace cpu {
namespace core {

/// ★★ **寄存器堆桥接 plugin**（★ 层 2 → 层 1）
///
/// ★ 职责：★ 持有 `RegFileComponent`（层 2）⇒ ★ 在 `setup()` 里用
///   **ChipForge 的 ctx** 建它 ⇒ ★ 在 `at_stage` 闭包里把它的 `io()` 输出
///   **发布成 `Payload`**（层 1）。
///
/// ★★ **产出哪些 payload**（★ 复用 `ip/cpu/core/payload_common.h` 现有 key
///   ⇒ ★ **不另造 IO 类型** —— ADR-091 v1.12 §D3a.7 用户裁决 B）：
///   · `RS1`     = 读端口 A 的数据（★ `ch_uint<32>`）
///   · `RS2`     = 读端口 B 的数据
///   · `RD_DATA` = 写数据（★ 供 writeback 消费）
///   · `RD_IDX`  = 写索引（★ `ch_uint<5>`）
///
/// ★★ **地址来源**：★ 读/写地址由**外部**提供（本 plugin 不产生地址）
///   ⇒ ★ 桥接 plugin 的接口 = 若干 `ch_in` 端口（★ 由装配层或上游 stage 驱动）。
template <typename T = ch::core::ch_uint<32>>
class RegFileBridgePlugin : public cf::plugin::PluginBase {
 public:
  using Addr = ch::core::ch_uint<5>;

  /// ★★ 桥接 plugin 的**输入端口**（★ 由装配层绑定）
  struct Ports {
    ch::core::ch_in<Addr>* rs1_addr = nullptr;   // ★ 读地址 A
    ch::core::ch_in<Addr>* rs2_addr = nullptr;   // ★ 读地址 B
    ch::core::ch_in<Addr>* rd_addr = nullptr;    // ★ 写地址
    ch::core::ch_in<T>* rd_data = nullptr;        // ★ 写数据
    ch::core::ch_in<ch::core::ch_bool>* rd_we = nullptr;  // ★ 写使能
  };

  void bind_ports(const Ports& p) { ports_ = p; }

  void setup(cf::plugin::PipeBuilder& pb) override {
    // ★★ **G-1p（★ 本文件是 G-1p 的唯一执行点）**：
    //   ★ MUST 传 ChipForge 的 ctx（★ `pb.ctx()` —— 框架缺口 G-6）
    //   ★ MUST NOT `build(nullptr)` ⇒ ★ 子 ctx ⇒ node-id aliasing ⇒ 假绿
    regfile_ = std::make_unique<RegFileComponent>(nullptr, "regfile");
    regfile_->build(pb.ctx());

    // ★★ **驱动 Component 的输入**：★ 把 `Ports` 的 `ch_in` 接到 Component 的
    //   同名 `io()` 端口（★ 直连 ⇒ 无 stage 依赖）
    // ★★ `operator<<=` 对 **input port** = **设置 driver**（★ `set_driver`）
    //   ⇒ ★ 这正是 ADR-091 **G-1d** 的成因（★ 被驱动的 `ch_in` 不是顶层端口）
    //   ⇒ ★ Component 的 `io()` 输入端口靠它被外部驱动 ★
    if (ports_.rs1_addr) regfile_->io().rs1_addr <<= ch::core::ch_uint<5>(*ports_.rs1_addr);
    if (ports_.rs2_addr) regfile_->io().rs2_addr <<= ch::core::ch_uint<5>(*ports_.rs2_addr);
    if (ports_.rd_addr)  regfile_->io().rd_addr  <<= ch::core::ch_uint<5>(*ports_.rd_addr);
    if (ports_.rd_data)  regfile_->io().rd_data  <<= ch::core::ch_uint<32>(*ports_.rd_data);
    // ★ `ch_bool` **无**从 `port` 的隐式转换 ⇒ ★ MUST 走 `port::impl()`
    if (ports_.rd_we)    regfile_->io().rd_we    <<= ch::core::ch_bool(ports_.rd_we->impl());

    // ★★ **发布成 payload**（★ 层 2 → 层 1 的唯一通道）
    //   ★ stage 名 = "decode"（★ RISC-V 5 级的 ID 段 ⇒ ★ 读寄存器在此）
    //   ★ ★ phase = EARLY ⇒ ★ 早于同 stage 的其它闭包（★ G-1l：★ 上游先写）
    pb.at_stage("decode", cf::plugin::Phase::EARLY, [this, &pb] {
      auto* n = pb.node_of_logic_stage("decode").get();
      // ★★★ **D4 约束 4**：★ `at_stage` 回调内 **MUST NOT** 用「条件早返」
      //   形式 ⇒ ★ 必须用 `if (cond) { ... }` 包裹主逻辑
      //   ⇒ ★ `tools/check_plugin_portability.sh` Check 1 强制
      //   ⇒ ⚠️ ★ 该检查是**纯文本 grep**（★ 判定用**原行**，★ 含注释）
      //     ⇒ ★ **注释里也不能写「条件早返」的字面形式**（★ 否则假阳性）
      if (n) {
        using KT = cf::cpu::core::payload::keys<T, 32>;
        n->operator()(KT::RS1) =
            ch::core::ch_uint<32>(regfile_->io().rs1_data);
        n->operator()(KT::RS2) =
            ch::core::ch_uint<32>(regfile_->io().rs2_data);
        n->operator()(KT::RD_DATA) =
            ch::core::ch_uint<32>(regfile_->io().rd_data);
        n->operator()(KT::RD_IDX) =
            ch::core::ch_uint<5>(regfile_->io().rd_addr);
      }
    });
  }

  void build(cf::plugin::PipeBuilder&) override {}

  /// ★ 供测试读取层 2 单元（★ 验证「reg_N 节点名」等）
  const RegFileComponent* unit() const { return regfile_.get(); }

 private:
  std::unique_ptr<RegFileComponent> regfile_;
  Ports ports_{};
};

}  // namespace core
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CORE_REGFILE_BRIDGE_CHMEM_H

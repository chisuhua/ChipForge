// ip/cpu/core/ibus_port_chmem.h
//
// 功能描述: CH_MEM 开放核的**取指端口化实现**（ibbus 取指走 `ch_stream` 端口，
//           **不再**用内部 `ch_mem` + ELF 预载）
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-28
//
// ============================================================================
// 归属 (ADR-091 v1.4 §D8 S3)
// ============================================================================
// ★ 本文件是 ADR-091「CH_MEM 开放核形态与端口架构」§S3 的**取指侧**落地：
//   · 预载核（`ip/cpu/plugins/ibus_chmem.h`）= 内部 `ch_mem` + elaboration 期
//     预载 ELF ⇒ **封闭核**（顶层只有 clock/reset）
//   · 本文件 = 取指请求/响应走**对外端口** ⇒ **开放核**（顶层有 imem 端口）
//   ⇒ ★ 二者共享 `ip/cpu/arch/riscv/`（ISA 语义）与
//     `ip/cpu/core/payload_common.h`（payload 契约）⇒ **不分叉语义**
//
// ============================================================================
// ★ 契约层 vs 实现层的粒度（★ 重要 —— ADR-091 §D3a 的实现化澄清）
// ============================================================================
// ★ **契约层**说「核对外暴露 `ch_stream<CpuBusReq>` / `ch_stream<CpuBusResp>`」；
// ★ **实现层**是**逐字段端口** —— 因为 CppHDL 的 `ch_stream` 语义就是
//   「payload / valid / ready 三元组」，而 `as_master_direction()` 的实现是
//   ★ **逐字段** `make_output(payload, valid)` + `make_input(ready)`
//   （★ 上游 `bundle/stream_bundle.h:32-41`；★ 上游 `test_verilator_stream_e2e.cpp`
//     的 fixture 也是逐字段 `ch_in`/`ch_out`，而非一个 `ch_stream` 端口）
// ⇒ ★ 本文件按**逐字段**声明端口，字段清单与 `CpuBusReq`/`CpuBusResp`
//   （`ip/cpu/core/bus_iface_chmem.h`）一一对应 ⇒ ★ 契约未变，只是落到端口粒度。
//
// ============================================================================
// ★ 时序等价性（★ H-3 等价性自检的前提 —— ADR-091 §D6 H-3）
// ============================================================================
// ★ 预载核的取指：`imem.sread(word_addr)` ⇒ **1 拍**（★ 上游 250198a 起 aread 是
//   0 拍组合读，故 imem **必须**用 sread 以保持 `pc_lag` 对齐 —— 见
//   `ibus_chmem.h` 的 `fetch_instr` 注释）
// ★ 端口核的取指：**请求当拍发出 ⇒ 响应下拍回来** ⇒ **也是 1 拍**
// ⇒ ★ 二者天然对齐 ⇒ ★ H-3 的「5 观测量逐位相等」有成立的前提。
// ⇒ ★ 但 ★ CH_MEM 是**组合 DAG**（无 stage 寄存器）⇒ 「跨拍」必须**显式**用
//   `ch_reg` 表达 ⇒ ★ 本文件用 `instr_reg` 把响应**打一拍**，对齐 `sread` 语义。
//
// ============================================================================
// ★ 握手成本（ADR-091 §D3a.2）
// ============================================================================
// ★ 本版本**只实现「`ready` 恒真」的无阻塞流**（★ §D3a.2 的第一档）：
//   · 请求侧 `valid` 恒真、`ready` 恒真 ⇒ 从设备**不产生背压**
//   · ⇒ 单周期同步语义与预载核一致 ⇒ ★ 等价性自检可成立
// ★ 「`ready` 可拉低」（背压）⇒ ★ **未实现** ⇒ CPU 侧 stall 机制待后续
//   （★ 属 ADR-091 §D8 S3 的后续增量）⇒ ★ **诚实标注：当前不支持背压**。
//
// ============================================================================
// CH_MEM 纪律 (ADR-040 v3.0)
// ============================================================================
//   · 本文件 CH_MEM-only ⇒ 不引 `cf::plugin::uint_t` / `cpptlm`
//   · ★ **MUST NOT** include `<ch.hpp>`（★ `module.h` 缺 `inline` ⇒ 多 TU 链接失败）
//     ⇒ 只 include 具体头
//   · ★ 一切 `ch_*` 字面量构造 MUST 在 `ctx_swap` 保护内（ADR-091 §D5.1 G-1e/G-1f）
//
// ============================================================================

#ifndef CF_IP_CPU_CORE_IBUS_PORT_CHMEM_H
#define CF_IP_CPU_CORE_IBUS_PORT_CHMEM_H

// ★ 只 include 具体头（MUST NOT <ch.hpp>）
#include <core/bool.h>
#include <core/reg.h>
#include <core/traits.h>
#include <core/uint.h>

#include "cf/plugin/payload.h"
// ★ payload key 契约（★ 与预载核**共享** ⇒ 新旧核语义不分叉 —— ADR-091 §D1）
#include "ip/cpu/core/branch_iface_chmem.h"
#include "ip/cpu/core/payload_common.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/plugin_exception.h"

namespace cf {
namespace cpu {
namespace core {

/// ★ 取指端口组（★ 逐字段 —— 对应 `ch_stream<CpuBusReq>` / `ch_stream<CpuBusResp>`）
///
/// ★★ **MUST 用指针持有**：CppHDL 的 `port<T, Dir>` **拷贝赋值是 deleted**
///   （`core/io.h:222` / `:270`）⇒ 值语义注入（`ports_ = p`）**编译不过**
///   ⇒ ★ 改为存指针。
///   ★ 生命周期安全：端口与 plugin **同由 `PipeBuilder` 持有** ⇒ 指针始终有效。
template <typename T>
struct IBusPortPorts {
  // ── 请求侧（核 = master）：输出 payload/valid，输入 ready ──
  ch::core::ch_out<ch::core::ch_uint<32>>* req_addr = nullptr;   // 取指地址（字节）
  ch::core::ch_out<ch::core::ch_uint<1>>*  req_valid = nullptr;  // 请求有效
  ch::core::ch_in<ch::core::ch_uint<1>>*   req_ready = nullptr;   // 从设备就绪
  // ── 响应侧（核 = slave）：输入 payload/valid ──
  ch::core::ch_in<ch::core::ch_uint<32>>*  rsp_data = nullptr;   // 指令字
  ch::core::ch_in<ch::core::ch_uint<1>>*   rsp_valid = nullptr;  // 响应有效
  // ── 观测（顶层端口 ⇒ ADR-091 §D6 H-2）──
  ch::core::ch_out<ch::core::ch_uint<32>>* pc = nullptr;         // 当前 PC（逐拍）
  /// ★ 「本拍发布的指令是错路」⇒ 供 dmem 抑制 store（ADR-091 §D3a.5 ③）
  ch::core::ch_out<ch::core::ch_uint<1>>* flush = nullptr;
};

// ============================================================================
// IBusPortPlugin —— 取指走端口的 CH_MEM plugin
// ============================================================================
// ★ 与预载核 `ip/cpu/plugins/ibus_chmem.h` 的差异**仅在取指数据源**：
//   · 预载核：`imem.sread(word_addr)`（内部 ch_mem + 预载 ELF）
//   · 本插件：`req_addr/req_valid` 发出请求，`rsp_data/rsp_valid` 取回指令
// ★ ★ **不含** ELF 预载 / 内部 `ch_mem` ⇒ ★ **真开放核**（外部供指）
template <typename T>
class IBusPortPlugin : public cf::plugin::PluginBase {
 public:
  using Ports = IBusPortPorts<T>;

  /// 绑定端口（★ 由装配方 `declare_*` 后注入 —— 端口是顶层接口，
  ///   MUST 由装配层统一声明，避免 plugin 各自散着声明）
  /// ★★ MUST 传**指针**（★ `port` 的拷贝赋值 deleted ⇒ 值语义注入编译不过）
  void bind_ports(Ports ports) { ports_ = ports; }

  /// 覆盖取指 PC 起点（vendored ELF 从 0x80000000 进入）
  /// ★★ MUST 是**纯 C++ 值**（`uint32_t`），MUST NOT 是 `ch_uint` ——
  ///   ★ `ch_uint` 的成员初始化器会在 **plugin 构造期**创建字面量节点，
  ///   而构造期**通常没有 active context** ⇒ 静默失败（值 0）⇒ 假绿。
  ///   ⇒ ★ 运行期初值在 `get_pc()`（elaboration 期，有 ctx）才转成字面量。
  void set_initial_pc(std::uint32_t pc) { initial_pc_ = pc; }

  void setup(cf::plugin::PipeBuilder& pb) override {
    // ★ fetch NORMAL：发请求 + 收响应（★ 与预载核同 stage 同 phase ⇒ 时序对齐）
    pb.at_stage("fetch", cf::plugin::Phase::NORMAL,
                [this, &pb] { this->fetch(pb); });
    // ★★★ branch LATE：**pc 重定向**（★ ADR-091 §D3a.5 —— S3.2b）
    //   ★ MUST 用 `branch` stage 的 **LATE** phase（★ 与预载核同款）：
    //     · `canonical_stage_order()` 只看 stage 名字**首现序**
    //       ⇒ ★ 若放 `fetch LATE`，它会排在 `branch NORMAL` **之前**
    //       ⇒ ★ 读不到本拍 branch 的结果（★ 滞后一拍 ⇒ 多 1 拍错路）
    //     · `branch LATE` 保证晚于 branch 生产者的 NORMAL/EARLY
    pb.at_stage("branch", cf::plugin::Phase::LATE,
                [this, &pb] { this->update_pc(pb); });
  }

  void build(cf::plugin::PipeBuilder&) override {}

 private:
  // ── 状态寄存器 ──
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<32>>> pc_reg_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<32>>> pc_lag_reg_;
  /// ★ 响应打拍（对齐 `sread` 的 1 拍语义 —— 见文件头「时序等价性」）
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<32>>> instr_reg_;
  /// ★★ `FLUSH` 打拍（★ ADR-091 §D3a.5 ③ 的 1 拍对齐）
  ///   ★ 拍 N 的 `taken=1` ⇒ 拍 N 取到的是**顺序地址 A+4**（错路，已在飞）
  ///   ⇒ ★ 拍 N+1 才发布它的 `(PC, INSTRUCTION)` ⇒ ★ `FLUSH` MUST 在拍 N+1 有效
  ///   ⇒ ★ 因此 MUST `flush_reg <<= branch_taken`
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<1>>> flush_reg_;

  Ports ports_{};
  /// ★ 纯 C++ 值（★ 禁 `ch_uint` 成员初始化器 —— 见 `set_initial_pc` 注释）
  std::uint32_t initial_pc_ = 0x80000000u;

  ch::core::ch_reg<ch::core::ch_uint<32>>& get_pc() {
    if (!pc_reg_) {
      // ★ `to_operand` 支持整数 ⇒ 运行期初值在此转成字面量（此时有 ctx）
      pc_reg_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<32>>>(
          initial_pc_, "open_pc");
    }
    return *pc_reg_;
  }

  ch::core::ch_reg<ch::core::ch_uint<32>>& get_pc_lag() {
    if (!pc_lag_reg_) {
      pc_lag_reg_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<32>>>(
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}), "open_pc_lag");
    }
    return *pc_lag_reg_;
  }

  ch::core::ch_reg<ch::core::ch_uint<1>>& get_flush_reg() {
    if (!flush_reg_) {
      flush_reg_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<1>>>(
          ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}), "open_flush");
    }
    return *flush_reg_;
  }

  ch::core::ch_reg<ch::core::ch_uint<32>>& get_instr_reg() {
    if (!instr_reg_) {
      instr_reg_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<32>>>(
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}), "open_instr");
    }
    return *instr_reg_;
  }

  void fetch(cf::plugin::PipeBuilder& pb) {
    using KT = cf::cpu::core::payload::keys<T, 32>;
    auto* n = pb.node_of_logic_stage("fetch").get();
    if (!n) return;

    auto& pc = get_pc();
    auto& pc_lag = get_pc_lag();
    auto& instr_reg = get_instr_reg();

    const ch::core::ch_uint<32> pc_val(pc);

    // ── 请求侧（当拍发出；★ `ready` 恒真 ⇒ 无阻塞流）──
    if (ports_.req_addr) *ports_.req_addr = pc_val;
    if (ports_.req_valid) {
      *ports_.req_valid = ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{});
    }

    // ── 响应侧（★ 打一拍 ⇒ 对齐 `sread` 的 1 拍语义）──
    // ★ 组合 DAG 里「跨拍」MUST 显式用 ch_reg 表达（文件头「时序等价性」）
    if (ports_.rsp_data) {
      const ch::core::ch_uint<32> rsp_data(*ports_.rsp_data);
      instr_reg <<= rsp_data;
    }

    // ── 发布（与预载核同 key 同语义）──
    n->operator()(KT::INSTRUCTION) = ch::core::ch_uint<32>(instr_reg);
    n->operator()(KT::PC) = ch::core::ch_uint<32>(pc_lag);
    // ★ 顶层观测端口（H-2）
    if (ports_.pc) *ports_.pc = pc_val;

    // ── `FLUSH` 发布（★ ADR-091 §D3a.5 ③）──
    // ★ 消费者 = dmem（`is_store & !flush`）⇒ ★ `fetch` 的 stage 首现序
    //   早于 `memory` ⇒ ★ 可见性天然成立（ADR-040 v3.0 §10.6）
    {
      namespace BK = cf::cpu::core::branch_keys;
      const auto flush_val = ch::core::ch_uint<1>(get_flush_reg());
      n->operator()(BK::FLUSH) = ch::core::ch_bool(
          ch::core::operator==(flush_val, ch::core::ch_uint<1>(
              ch::core::ch_literal<1, 1>{})));
      if (ports_.flush) *ports_.flush = flush_val;
    }

    // ── PC 锁存（★ `pc` 的 next 由 `update_pc` 在 `branch LATE` 决定）──
    pc_lag <<= pc_val;
  }

  /// ★★ `branch LATE`：pc 重定向（★ ADR-091 §D3a.5 —— S3.2b）
  ///
  /// ★ 形态与预载核 `ibus_chmem.h::update_pc` 的**无 BP 路径同构**：
  ///   `pc <<= select(branch_taken, branch_target, pc + 4)`
  /// ★ 差异：★ 开放核**不读 BP 预测臂**（★ BP 归 `feat-bp-btb-gshare`）
  ///   ⇒ ★ 两臂（taken / 顺序）足够 ⇒ ★ 语义更简单、无预测器耦合
  void update_pc(cf::plugin::PipeBuilder& pb) {
    namespace BK = cf::cpu::core::branch_keys;
    auto& pc = get_pc();
    auto& flush_reg = get_flush_reg();
    const ch::core::ch_uint<32> pc_val(pc);

    auto* brn = pb.node_of_logic_stage("branch").get();

    // ★★ **fail-open + 显式登记**（★ ADR-091 §D3a.5 ④）：
    //   ★ branch 节点不存在（★ A2/A4 分阶段装配 ⇒ 只装 ibus 不装 branch）⇒
    //   ★ 退化为**顺序取指**（`taken=0`）⇒ ★ 与预载核
    //   `brn == nullptr ⇒ br_taken = false` **同口径**
    ch::core::ch_bool br_taken = ch::core::ch_bool(
        ch::core::operator==(ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
                             ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{})));
    ch::core::ch_uint<32> br_target(
        ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}));
    if (brn && brn->payloads().has(BK::BRANCH_TAKEN)) {
      br_taken = brn->operator()(BK::BRANCH_TAKEN);
    }
    if (brn && brn->payloads().has(BK::BRANCH_TARGET)) {
      br_target = brn->operator()(BK::BRANCH_TARGET);
    }

    // ★ pc mux：★ taken ⇒ target；否则 ⇒ pc + 4
    //   ★ MUST 用 `select()`（★ G-1m：`ch_bool` 禁用于 `?:`）
    const auto pc_next = ch::core::ch_uint<32>(ch::core::select(
        br_taken, br_target,
        ch::core::ch_uint<32>(pc_val + ch::core::ch_uint<32>(
            ch::core::ch_literal<4, 32>{}))));
    pc <<= pc_next;

    // ★★ `FLUSH` 打 1 拍（★ 见 `flush_reg_` 注释的 1 拍对齐推导）
    flush_reg <<= ch::core::ch_uint<1>(ch::core::select(
        br_taken, ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{}),
        ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{})));
  }
};

}  // namespace core
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CORE_IBUS_PORT_CHMEM_H

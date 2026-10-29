// ip/cpu/core/regfile_chmem.h
//
// 功能描述: CH_MEM 开放核的**寄存器堆单元**（★ 层 2 = `ch::Component`）
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-29
//
// ============================================================================
// 归属 (ADR-091 v1.12 §D3a.7 —— 两层范式)
// ============================================================================
//
// ★★★ **两层范式**（★ ADR-091 v1.12 §D3a.7 —— A4 裁决）：
//   · ★ **层 1** = 核内 stage 间（★ `PluginBase` + `at_stage` + `Payload<T>`）
//   · ★ **层 2** = 可复用 IP 单元（★ `ch::Component` + `io()` struct）
//   · ★ **层 2 → 层 1 的唯一通道 = 桥接 plugin**（★ `regfile_bridge_chmem.h`）
//
// ★ 本文件是**层 2**（★ `ch::Component` 子类）⇒ ★ 它**不感知** `Payload`、
//   不感知 stage ⇒ ★ 可**独立测试** + 可被 SoC 复用。
//
// ============================================================================
// ★★★ 上游先例与移植说明（★ 两处**必须不同**）
// ============================================================================
//
// ★ 上游有**同形态**的先例（★ `CppHDL/include/cpu/pipeline/rv32i_regfile.h`
//   的 `Rv32iRegFile : public ch::Component`）★ 但有两处**必须不同**：
//
//   ① ★★ **上游用 `describe()` 里的「局部 `ch_reg`」**（★ 默认名 `"reg"`）
//      ⇒ ★ **契约名 `reg_N` 会丢** ⇒ ★ parity runner 按 `reg_<i>` 采集
//      ⇒ ★★ **MUST** 用 `std::vector` **成员** + **显式命名** `reg_N`
//      （★ 与预载核 `ip/cpu/plugins/reg_file_chmem.h:160-172` **同款做法**）
//      ⇒ ★★ 预载核注释实证：★ `std::array` 默认构造会**泄漏 32 对孤儿
//      `regimpl`/`proxy` 节点**进 ctx ⇒ ★ 与显式命名的 `reg_0..reg_31`
//      **撞名** ⇒ ★ Verilator 报 `Duplicate declaration of signal '_reg_N'`
//
//   ② ★★ **上游的 `ch_reg` 是 `describe()` 的局部变量** ⇒ ★ 无「无 active
//      ctx ⇒ 静默不建节点」的窗口；★ 本实现用**懒创建成员**（★ `get_regs()`）
//      ⇒ ★ 创建点 MUST 在 ★ **`describe()` 内**（★ 此时 current ctx 有效
//      ⇒ ★ `Component::build_internal` 的 `ctx_swap` 已生效）
//
// ============================================================================
// ★★★ ctx 共用（硬约束 G-1p）
// ============================================================================
//
// ★ `Component::build(external_ctx)` MUST 传 **ChipForge 的 ctx**
//   （★ 由桥接 plugin 调 `regfile_->build(pb.ctx())` ⇒ ★ 依赖框架缺口
//   **G-6**：`PipeBuilder::ctx()` 只读 accessor）。
// ★ ★★ **MUST NOT** `build(nullptr)` ⇒ ★ 那会自建**子 ctx**
//   （★ `CppHDL/src/component.cpp:115-117`）⇒ ★ **node-id aliasing**
//   ⇒ ★ 组件的 output ports **从 simulator 的 `data_map` 消失**
//   ⇒ ★ 观测**静默读不到** ⇒ ★ **最坏假绿**
//   ⇒ ★ ★ 上游已踩此坑（★ `CppHDL/include/device.h:12-19` 明载）。
//
// ★★ **MUST NOT** include `<ch.hpp>`（★ ADR-091 G-1b：★ `module.h:13` 缺
//   `inline` ⇒ ★ 2+ TU include 即 **multiple definition**）
//   ⇒ ★ 只 include 具体头（★ `component.h` **不** include `module.h` ⇒ ★ 安全）。
//
// ★★ **内部节点名 MUST NOT 与顶层端口名同名**（★ G-1o）
//   ⇒ ★ 本单元的 io 端口名一律带 `regfile_` 前缀。
//
// ============================================================================

#ifndef CF_IP_CPU_CORE_REGFILE_CHMEM_H
#define CF_IP_CPU_CORE_REGFILE_CHMEM_H

// ★ 只 include 具体头（MUST NOT <ch.hpp> —— G-1b）
#include <component.h>
#include <core/bool.h>
#include <core/io.h>
#include <core/literal.h>
#include <core/operators.h>
#include <core/reg.h>
#include <core/uint.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace cf {
namespace cpu {
namespace core {

/// ★★ **寄存器堆单元**（★ 层 2 = `ch::Component`）
///
/// ★ 形态对标上游 `CppHDL/include/cpu/pipeline/rv32i_regfile.h`
///   （★ `Rv32iRegFile : public ch::Component` + `io()` struct）。
///
/// ★ ★ **接口载体 = `Payload<T>`**（★ ADR-091 v1.12 §D3a.7 —— 用户裁决 B）：
///   ★ 本单元**不感知** `Payload`（★ 层 2 不依赖层 1）
///   ⇒ ★ 边界转换由**桥接 plugin**（★ `regfile_bridge_chmem.h`）承担。
///
/// ★★ **MUST NOT** 用 `ch_stream`（★ 越层：★ ADR-092 §D4 定 `ch_stream`
///   = IP 间 / 核↔从设备接口；★ 且它带 FIFO ⇒ 引入延迟 ⇒ H-3 更不可比）。
class RegFileComponent : public ch::Component {
 public:
  using Addr = ch::core::ch_uint<5>;   // ★ RISC-V xlen=32 ⇒ 5-bit 索引（ADR-087）
  using Data = ch::core::ch_uint<32>;
  static constexpr std::size_t kNumRegs = 32;

  // ★ `__io` 宏（CppHDL `include/core/io.h:312`）⇒ ★ 生成 `io_type` struct
  //   + `io_storage_` + `io()` accessor。
  // ★ ★ 成员顺序 = aggregate 初始化顺序 ⇒ ★ `create_ports()` 必须按同序传。
  __io(
      // ── 读端口 A ──
      ch::core::ch_in<Addr> rs1_addr;
      ch::core::ch_out<Data> rs1_data;
      // ── 读端口 B ──
      ch::core::ch_in<Addr> rs2_addr;
      ch::core::ch_out<Data> rs2_data;
      // ── 写端口 ──
      ch::core::ch_in<Addr> rd_addr;
      ch::core::ch_in<Data> rd_data;
      ch::core::ch_in<ch::core::ch_bool> rd_we;
  )

  explicit RegFileComponent(ch::Component* parent = nullptr,
                             const std::string& name = "regfile")
      : ch::Component(parent, name) {}

  /// ★★ **端口创建**：★ MUST **显式给名字**（★ aggregate 初始化）
  ///
  /// ★★ 为什么不能用默认构造后赋值：★ `ch::core::port` 的 `operator=` 被
  ///   `delete` ⇒ ★ **不能**事后赋值 ⇒ ★ ⇒ ★ 只能**直接 aggregate 初始化**
  ///   （★ 此时 `ch_in`/`ch_out` 的 `explicit port(name, sloc)` 构造生效
  ///   ⇒ ★ 节点名保留）。
  ///
  /// ★ ★ 名字一律带 `regfile_` 前缀（★ G-1o：★ MUST NOT 与顶层端口同名）。
  void create_ports() override {
    new (io_storage_) io_type{
        ch::core::ch_in<Addr>("regfile_rs1_addr"),
        ch::core::ch_out<Data>("regfile_rs1_data"),
        ch::core::ch_in<Addr>("regfile_rs2_addr"),
        ch::core::ch_out<Data>("regfile_rs2_data"),
        ch::core::ch_in<Addr>("regfile_rd_addr"),
        ch::core::ch_in<Data>("regfile_rd_data"),
        ch::core::ch_in<ch::core::ch_bool>("regfile_rd_we"),
    };
  }

  /// ★★ **行为描述**：★ 32 路寄存器 + 2 读口 + 1 写口
  void describe() override {
    auto& regs = get_regs();
    write_regs<1>(regs);   // ★ x0 恒 0 ⇒ ★ 从 1 开始
    // ★ `ch_out` 用 `operator=` 赋值（★ `port` 的 `operator<<=` 是**连接**语义）
    io().rs1_data = mux<kNumRegs - 1>(io().rs1_addr, regs);
    io().rs2_data = mux<kNumRegs - 1>(io().rs2_addr, regs);
  }

  /// ★ 供**桥接 plugin** / 测试读写的寄存器（★ 节点名 `reg_N`）
  const std::vector<ch::core::ch_reg<Data>>& regs() const { return *regs_; }

 private:
  /// ★★ **懒创建**（★ 避免「无 active ctx ⇒ 静默不建节点」窗口 —— G-1c 同族）
  ///
  /// ★ MUST 在 `describe()` 内首次调用（★ 此时 current ctx 有效）。
  std::vector<ch::core::ch_reg<Data>>& get_regs() {
    if (!regs_) {
      regs_ = std::make_unique<std::vector<ch::core::ch_reg<Data>>>();
      regs_->reserve(kNumRegs);
      for (std::size_t i = 0; i < kNumRegs; ++i) {
        // ★★ **显式命名**（★ MUST NOT 用默认名 —— 否则 32 个同名节点
        //   ⇒ ★ 撞名 ⇒ ★ Verilator `Duplicate declaration of signal '_reg_N'`
        //   —— 实证见预载核 `ip/cpu/plugins/reg_file_chmem.h:160-172`）
        //
        // ★★★ **名字 MUST 带 `open_` 前缀**（★ G-1o）：
        //   ★ **观测契约名 = `reg_<i>`**（★ parity runner 按 `reg_<i>` 采集）
        //   ★ ★ 但契约名由**顶层 `ch_out` 端口**承担（★ ADR-091 v1.12 §D3a.9：
        //     每个顶层观测量 MUST 是顶层 `ch_out` ⇒ ★ H-2）
        //   ★ ★ 若内部 `ch_reg` 也叫 `reg_<i>` ⇒ ★ **与顶层端口同名**
        //     ⇒ ★ CppHDL 唯一化把**端口改名**（`reg_1` → `reg_1_1`）
        //     ⇒ ★ **破契约**（★ 正是 G-1o 要防的）
        //   ⇒ ★ ⇒ ★ **内部实现名用 `open_reg_<i>`**（★ 不撞名）
        //   ⇒ ★ ★ 契约名不受影响：★ `resolve_node_id(ctx, "reg_1")` 会命中
        //     **顶层端口节点**（★ 顶层端口也在 `ctx.get_nodes()` 里）✔
        regs_->emplace_back(
            ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{}),
            ("open_reg_" + std::to_string(i)).c_str());
      }
    }
    return *regs_;
  }

  /// ★★ **地址等值判据**（★ **节点级**比较 —— 不可退化为常量）
  ///
  /// ★★ 为什么索引 MUST 是**模板参数**：★ `ch_literal<V, W>` 的 `V` 是
  ///   **编译期常量**（★ `ch_literal` 是字面量类型）⇒ ★ 若把运行期索引
  ///   塞进 `ch_uint<5>`，★ 该值会被**常量折叠** ⇒ ★ 比较结果成为常量
  ///   ⇒ ★ **信号退化为常量** ⇒ ★ **假绿**（★ ADR-091 §K19 已记录同族坑）
  ///   ⇒ ★ ⇒ ★ 用**模板递归展开**让每个索引成为编译期常量。
  template <std::size_t I>
  static ch::core::ch_bool addr_eq(const ch::core::ch_in<Addr>& addr) {
    return ch::core::ch_bool(ch::core::operator==(
        ch::core::ch_uint<5>(addr),
        ch::core::ch_uint<5>(ch::core::ch_literal<I, 5>{})));
  }

  /// ★ 写端口展开（★ 模板递归）
  template <std::size_t I>
  void write_regs(std::vector<ch::core::ch_reg<Data>>& regs) {
    if constexpr (I < kNumRegs) {
      const auto sel = addr_eq<I>(io().rd_addr);
      // ★ `select` 嵌套（★ MUST NOT 用 `&&` —— ADR-091 §K17 已证伪其可用性）
      // ★ `ch_bool` **无**从 `port` 的隐式转换（★ 只有 `ch_uint` 有
      //   `operator ch_uint<W>()`）⇒ ★ MUST 走 `port::impl()`。
      const auto we_i = ch::core::ch_bool(ch::core::select(
          sel, ch::core::ch_bool(io().rd_we.impl()),
          ch::core::ch_bool(ch::core::ch_literal<0, 1>{})));
      regs[I] <<= ch::core::ch_uint<32>(ch::core::select(
          we_i, ch::core::ch_uint<32>(io().rd_data),
          ch::core::ch_uint<32>(regs[I])));
      write_regs<I + 1>(regs);
    }
  }

  /// ★ 32 路 mux（★ select 链，★ 模板递归）
  template <std::size_t I>
  static ch::core::ch_uint<32> mux(const ch::core::ch_in<Addr>& addr,
                                    const std::vector<ch::core::ch_reg<Data>>& regs) {
    if constexpr (I == 0) {
      return ch::core::ch_uint<32>(regs[0]);
    } else {
      const auto sel = addr_eq<I>(addr);
      return ch::core::ch_uint<32>(ch::core::select(
          sel, ch::core::ch_uint<32>(regs[I]), mux<I - 1>(addr, regs)));
    }
  }

  std::unique_ptr<std::vector<ch::core::ch_reg<Data>>> regs_;
};

}  // namespace core
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CORE_REGFILE_CHMEM_H

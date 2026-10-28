// ip/cpu/core/dmem_port_chmem.h
//
// 功能描述: CH_MEM 开放核的**数据访存端口化实现**（访存走端口，
//           **不再**用核内 `ch_mem`）
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-28
//
// ============================================================================
// 归属 (ADR-091 v1.6/v1.7 §D3a.4 + §D8 S3)
// ============================================================================
// ★ 本文件是 ADR-091「CH_MEM 开放核形态与端口架构」§S3 的**访存侧**落地：
//   · 预载核（`ip/cpu/plugins/dmem_chmem.h`）= 核内 `ch_mem` + 预载 ELF
//     ⇒ **封闭核**（顶层只有 clock/reset）
//   · 本文件 = 访存请求/响应走**对外端口** ⇒ **开放核**（顶层有 dmem 端口）
// ⇒ ★ 二者共享 `ip/cpu/core/payload_common.h`（payload 契约）与
//   `ip/cpu/arch/riscv/`（ISA 语义）⇒ **不分叉语义**
//
// ★★ **MUST NOT include `ip/cpu/plugins/dmem_chmem.h`** —— 它 include `<ch.hpp>`
//   ⇒ ★ `module.h:13` 的 `build_hierarchical_name` **缺 `inline`**
//   ⇒ ★ 任何 2+ TU include 即链接失败 ⇒ ★ 只**参考其语义**，不 include。
//   （★ 同款纪律：MUST NOT include ADR-092 冻结集里的任何文件。）
//
// ============================================================================
// ★★★ 时序形态（★ A0 裁决，ADR-091 §D3a.4 —— 推导见 plan §K13~K15）★★★
// ============================================================================
//
// ★ 预载核的三侧时序**不对称**（实测事实）：
//   · 取指读 `imem.sread()`   = **1 拍**
//   · 访存读 `mem.aread()`    = **0 拍**（组合）
//   · 访存写 `mem.write()`    = **同拍生效**
//
// ★ 本实现的形态：
//   ┌ 请求侧（核 = master）**打 1 拍** ⇒ 端口在**拍 N** 呈现本拍算出的请求
//   │   addr_reg / data_reg / size_reg / we_reg
//   └ 响应侧（核 = slave）**组合消费** `rsp_data` ⇒ **消费拍 = 拍 N+1**
//
// ★★ **为什么这样切**：
//   ★ 端口形态下外部从设备是「拍边界驱动」⇒ 它在拍 N 才知道拍 N 的读地址
//     ⇒ **无法在拍 N 组合给出该地址的数据** ⇒ ⇒ 地址 MUST **提前 1 拍**可见
//     ⇒ ⇒ **硬约束 G-1k**（ADR-091 §D5.1）
//   ★ 请求打 1 拍 ⇒ 端口在拍 N 呈现地址 ⇒ 外部在拍 N 组合供数
//     ⇒ CPU 在拍 N+1 组合消费 ⇒ ★ **数据消费拍 = 地址呈现拍 + 1**
//   ★ 写请求与地址同拍发出 ⇒ 外部在拍 N 执行写
//     ⇒ ★ **写生效的绝对拍 = 拍 N = 与预载核一致** ✔
//
// ★★ **后果（诚实标注）**：
//   ★ 访存**读**的消费拍比预载核晚 1 拍 ⇒ ★ 预载核是**全组合 5 级**
//     （`cpu_factory_chmem.h:431-435` 无 stage 寄存器）而本实现带访存级寄存器
//     ⇒ ⇒ **流水结构不同构** ⇒ ⇒ ★★ **`cycles`（拍数）与 `pchash`
//     （per-cycle ordered FNV-1a over PC）在「开放核 vs 预载核」之间
//     结构性不可比** ⇒ ⇒ ★ MUST 走 ADR-089 例外白名单（★ architect 已扩张）
//     ⇒ ★★ **H-3 的实质判据 = 3 个终值观测量（tohost/regs/memsig）逐位相等**
//     ⇒ ★ 详见 ADR-091 §D6 H-3
//
// ★★ **写语义由「从设备」承担**（★ ADR-091 v1.7 —— 推导见 plan §K16）：
//   ★ CPU 侧**不做** RMW（★ 预载核靠 `aread` 拿到「当前字」才能算合并值
//     ⇒ 那会让**写依赖读响应** ⇒ 写晚 1 拍 ⇒ 需 stall/forwarding）
//   ★ 本实现只发「字节地址 + 原始写数据 + 字节宽度(`size`) + 写使能」
//   ★ ⇒ **从设备**按 `address` 低 2 位 + `size` 推 byte-strobe 并 RMW
//   ★ ⇒ ⇒ **写不依赖读响应** ⇒ ⇒ **写无延迟** ⇒ ⇒ **无需 stall/forwarding**
//
// ★★ **读侧不需要 `size`**：★ 从设备总是返回**整个对齐字**（低 32 位有效）
//   ⇒ ★ CPU 侧按 `address` 低 2 位（byte offset）做 byte/half 提取
//   ⇒ ⇒ 读语义与预载核 `dmem_chmem.h` 的 byte select 树**逐位同构**。
//
// ============================================================================
// ★★ 诚实标注：本版本**未**实现的项
// ============================================================================
//   ✗ **背压（`ready` 可拉低）**：★ 本版本只实现「`ready` 恒真」的无阻塞流
//     ⇒ ★ `req_ready` 被读入但**不参与**任何决策（★ 恒真假设）
//     ⇒ ★ ADR-091 §D3a.2 第 ③ 档 ⇒ ★ **显式登记为已知边界**
//   ✓ **flush 抑制**（★ S3.2b 已落地）：★ `we = is_store & !flush`
//     ⇒ ★ `flush` 由 `ibus_port_chmem.h` 在 `fetch` stage 写（★ 打 1 拍）
//     ⇒ ★ 错路 store 不写内存（★ 与预载核 `is_store_active` 的 `!flush` 同款）
//   ✗ **MMU 翻译**：★ 地址取 `KT::RESULT`（vaddr）**直接**作字节地址
//     ⇒ ★ Bare 模式（★ 与预载核的 `PADDR_VALID=false` 路径同款）
//     ⇒ ★ Sv32 翻译归 `mmu-chmem-phase-b-c-launch`
//   ✗ **`memsig` 观测**：★ ★ **开放核不持有内存** ⇒ 核侧算不出「最终字」
//     ⇒ ⇒ memsig 的观测来源 MUST 由**从设备 / host 侧**承担
//     ⇒ ⇒ ★ **归 S5 的观测来源设计决策**（★ 本版本不实现）
//
// ============================================================================
// CH_MEM 纪律 (ADR-040 v3.0 / ADR-091 §D5.1)
// ============================================================================
//   · 本文件 CH_MEM-only ⇒ 不引 `cf::plugin::uint_t` / `cpptlm`
//   · ★ **MUST NOT** include `<ch.hpp>`（★ `module.h` 缺 `inline` ⇒ 多 TU 链接失败）
//     ⇒ 只 include 具体头
//   · ★ **MUST NOT** 在 plugin 成员初始化器里创建 `ch_*` 字面量（★ G-1i）
//     ⇒ 所有 `ch_reg` 惰性创建（★ 在 elaboration 期 ⇒ 有 active context）
//   · ★ 端口 MUST 用**指针持有**（★ `port<T,Dir>` 的拷贝赋值是 deleted）
//
// ============================================================================

#ifndef CF_IP_CPU_CORE_DMEM_PORT_CHMEM_H
#define CF_IP_CPU_CORE_DMEM_PORT_CHMEM_H

#include <cstdint>
#include <memory>

// ★ 只 include 具体头（MUST NOT <ch.hpp>）
#include <core/bool.h>
#include <core/operators.h>        // ★ select
#include <core/operators_width.h>  // ★ bits<MSB, LSB>
#include <core/reg.h>
#include <core/traits.h>
#include <core/uint.h>

#include "cf/plugin/payload.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
// ★ payload 契约（★ 与预载核**共享** ⇒ 新旧核语义不分叉 —— ADR-091 §D1）
#include "ip/cpu/core/branch_iface_chmem.h"
#include "ip/cpu/core/payload_common.h"

namespace cf {
namespace cpu {
namespace core {

/// ★ 数据访存端口组（★ 逐字段 —— 对应 `ch_stream<CpuBusReq>` / `ch_stream<CpuBusResp>`）
///
/// ★★ **MUST 用指针持有**：CppHDL 的 `port<T, Dir>` **拷贝赋值是 deleted**
///   （`core/io.h:222` / `:270`）⇒ 值语义注入（`ports_ = p`）**编译不过**
///   ⇒ ★ 改为存指针。
///   ★ 生命周期安全：端口与 plugin **同由 `PipeBuilder` 持有** ⇒ 指针始终有效。
template <typename T>
struct DMemPortPorts {
  // ── 请求侧（核 = master）：输出 payload/valid，输入 ready ──
  ch::core::ch_out<ch::core::ch_uint<64>>* req_addr = nullptr;   // 字节地址
  ch::core::ch_out<ch::core::ch_uint<64>>* req_data = nullptr;   // 写数据（未移位）
  ch::core::ch_out<ch::core::ch_uint<3>>*  req_size = nullptr;   // 访问字节数 1/2/4/8
  ch::core::ch_out<ch::core::ch_uint<1>>*  req_we = nullptr;     // 写使能
  ch::core::ch_out<ch::core::ch_uint<1>>*  req_valid = nullptr;  // 请求有效
  ch::core::ch_in<ch::core::ch_uint<1>>*   req_ready = nullptr;  // 从设备就绪
  // ── 响应侧（核 = slave）：输入 payload/valid ──
  ch::core::ch_in<ch::core::ch_uint<64>>*  rsp_data = nullptr;   // 读数据（整个对齐字）
  ch::core::ch_in<ch::core::ch_uint<1>>*   rsp_valid = nullptr;  // 响应有效
  // ── 观测（顶层端口 ⇒ ADR-091 §D6 H-2）──
  // ★ ★ **为什么需要它**：★ CH_MEM 的 payload（`KT::RD_DATA`）在 **elaboration 期**
  //   写入的是**组合连线** ⇒ ★ 运行期它随 `rsp_data` 端口变化
  //   ⇒ ★ 但 payload store 里的 C++ 侧值**不会更新** ⇒ ★ 无法从 payload 逐拍读
  //   ⇒ ⇒ ★ **必须**有一个顶层 `ch_out` 承载该组合结果才能逐拍观测。
  ch::core::ch_out<ch::core::ch_uint<32>>* rd_data = nullptr;   // 加载结果（观测）
};

// ============================================================================
// DMemPortPlugin —— 访存走端口的 CH_MEM plugin
// ============================================================================
// ★ 与预载核 `ip/cpu/plugins/dmem_chmem.h` 的差异**仅在访存数据源**：
//   · 预载核：`mem.aread(word_addr)` / `mem.write(...)`（核内 `ch_mem`）
//   · 本插件：`req_*` 发出请求，`rsp_data` 取回数据（★ 核内**无** `ch_mem`）
// ★ ★ **不含** ELF 预载 / 内部 `ch_mem` ⇒ ★ **真开放核**（外部供给）
template <typename T>
class DMemPortPlugin : public cf::plugin::PluginBase {
 public:
  using Ports = DMemPortPorts<T>;

  /// 绑定端口（★ 由装配方 `declare_*` 后注入 —— 端口是顶层接口，
  ///   MUST 由装配层统一声明，避免 plugin 各自散着声明）
  /// ★★ MUST 传**指针**（★ `port` 的拷贝赋值 deleted ⇒ 值语义注入编译不过）
  void bind_ports(Ports ports) { ports_ = ports; }

  void setup(cf::plugin::PipeBuilder& pb) override {
    // ★ 与预载核同 stage 同 phase（★ 预载核：`at_stage("memory", Phase::NORMAL)`）
    pb.at_stage("memory", cf::plugin::Phase::NORMAL,
                [this, &pb] { this->mem_access(pb); });
  }

  void build(cf::plugin::PipeBuilder&) override {}

 private:
  // ── 请求侧寄存器（★ 全部打 1 拍 ⇒ 端口在拍 N 呈现本拍算出的请求）──
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<64>>> addr_reg_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<64>>> data_reg_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<3>>>  size_reg_;
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<1>>>  we_reg_;
  // ★ 读侧对齐量：★ 消费拍 = 拍 N+1 ⇒ byte offset MUST 取**拍 N** 的值
  //   （★ `addr_reg_` 在拍 N+1 已是拍 N+1 的地址 ⇒ 不能用它取 offset）
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<2>>>  off_reg_;
  // ★ 读侧符号扩展标志（lb/lh ⇒ 1；lbu/lhu ⇒ 0）
  std::unique_ptr<ch::core::ch_reg<ch::core::ch_uint<1>>>  signed_reg_;

  Ports ports_{};

  // ── 惰性创建（★ G-1i：MUST NOT 在成员初始化器里创建 `ch_*` 字面量）──
  ch::core::ch_reg<ch::core::ch_uint<64>>& get_addr_reg() {
    if (!addr_reg_) {
      addr_reg_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<64>>>(
          ch::core::ch_uint<64>(ch::core::ch_literal<0, 64>{}), "open_dmem_addr");
    }
    return *addr_reg_;
  }
  ch::core::ch_reg<ch::core::ch_uint<64>>& get_data_reg() {
    if (!data_reg_) {
      data_reg_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<64>>>(
          ch::core::ch_uint<64>(ch::core::ch_literal<0, 64>{}), "open_dmem_data");
    }
    return *data_reg_;
  }
  ch::core::ch_reg<ch::core::ch_uint<3>>& get_size_reg() {
    if (!size_reg_) {
      size_reg_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<3>>>(
          ch::core::ch_uint<3>(ch::core::ch_literal<0, 3>{}), "open_dmem_size");
    }
    return *size_reg_;
  }
  ch::core::ch_reg<ch::core::ch_uint<1>>& get_we_reg() {
    if (!we_reg_) {
      we_reg_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<1>>>(
          ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}), "open_dmem_we");
    }
    return *we_reg_;
  }
  ch::core::ch_reg<ch::core::ch_uint<2>>& get_off_reg() {
    if (!off_reg_) {
      off_reg_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<2>>>(
          ch::core::ch_uint<2>(ch::core::ch_literal<0, 2>{}), "open_dmem_off");
    }
    return *off_reg_;
  }
  ch::core::ch_reg<ch::core::ch_uint<1>>& get_signed_reg() {
    if (!signed_reg_) {
      signed_reg_ = std::make_unique<ch::core::ch_reg<ch::core::ch_uint<1>>>(
          ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}), "open_dmem_signed");
    }
    return *signed_reg_;
  }

  void mem_access(cf::plugin::PipeBuilder& pb) {
    using KT = cf::cpu::core::payload::keys<T, 32>;
    auto* n = pb.node_of_logic_stage("memory").get();
    if (!n) return;

    // ── ① 指令解码（★ 与预载核同款：从 `KT::INSTRUCTION` 取 opcode/funct3）──
    const auto inst = n->operator()(KT::INSTRUCTION);
    // ★★★ **显式转 `ch_uint<N>`**（★ 防御性写法，★ 非实测约束）：★ `bits<>` 的
    //   返回类型由 `make_uint_result<W>` 决定 ⇒ ★ 显式写出目标类型让
    //   「位宽意图」在阅读时确定 ⇒ ★ 也避免依赖 `auto` 推导的隐式宽度。
    //   ★ （★ 早期版本曾报「Invalid operands for binary operation」⇒ 但该日志
    //     已被后续修复覆盖，★ 变异对照显示本形态与其它写法**等效** ⇒
    //     ★ **不作为硬约束登记**。）
    const auto opcode = ch::core::ch_uint<7>(ch::core::bits<6, 0>(inst));
    const auto funct3 = ch::core::ch_uint<3>(ch::core::bits<14, 12>(inst));

    const auto op_store = ch::core::ch_uint<7>(ch::core::ch_literal<0x23, 7>{});
    const auto op_load  = ch::core::ch_uint<7>(ch::core::ch_literal<0x03, 7>{});
    const auto f3_000   = ch::core::ch_uint<3>(ch::core::ch_literal<0, 3>{});
    const auto f3_001   = ch::core::ch_uint<3>(ch::core::ch_literal<1, 3>{});
    const auto f3_010   = ch::core::ch_uint<3>(ch::core::ch_literal<2, 3>{});
    const auto f3_100   = ch::core::ch_uint<3>(ch::core::ch_literal<4, 3>{});
    const auto f3_101   = ch::core::ch_uint<3>(ch::core::ch_literal<5, 3>{});

    // ★★★ **用 1-bit `&` / `|`**（★ 防御性写法，★ **非实测约束**）：
    //   ★ 上游 `ch_bool::operator&&` / `operator||` 已标 deprecated
    //   （★ 提示语：「use operator& for 1-bit AND; select() is for value muxing」）
    //   ⇒ ★ 用 `&` / `|` 规避 deprecated 路径。
    //   ★ （★ 早期怀疑「`||` 失效」⇒ ★ **变异对照证伪**：★ 改回 `&&` / `||`
    //     后本文件仍 **92 assertions 全绿** ⇒ ★ 该怀疑**不成立**
    //     ⇒ ★ **不作为硬约束登记** ⇒ ★ 真实的假失败成因是**测试指令编码错误**
    //     （★ `funct3` 未落在 bits[14:12] ⇒ `lbu` 被解码成 `lb`）。）
    // ★★★ **显式限定调用 `ch::core::operator==` / `operator&` / `operator|`**
    //   （★ 防御性写法，★ **非实测约束** —— ★ 早期怀疑已被变异对照证伪）：
    //   ★ `ch_uint<N>` **没有**自己的 `operator==` ⇒ ★ 继承 `logic_buffer` 的
    //   ★   `bool operator==(const logic_buffer&) { return node_impl_ == other.node_impl_; }`
    //   ★   ⇒ ★ 那个成员版本比较的是**节点指针**（★ 若被选中 ⇒ 比较按指针）
    //   ★ 同时 `ch::core::operator==`（自由函数模板）是**节点级**（`eq_op`）
    //   ★ ⇒ ★ 两者**并存** ⇒ ★ 显式限定调用让「走节点级」**无歧义**。
    //   ★ （★ **变异对照结论**：★ 改回 `a == b` / `a & b` / `a | b` 后本文件
    //     仍 **92 assertions 全绿** ⇒ ★ 实测重载决议**选中了自由函数**
    //     ⇒ ★ 该怀疑**不成立** ⇒ ★ **不作为硬约束登记**。）
    const auto is_store = ch::core::ch_bool(ch::core::operator==(opcode, op_store));
    const auto is_load  = ch::core::ch_bool(ch::core::operator==(opcode, op_load));
    const auto f3_000_b = ch::core::ch_bool(ch::core::operator==(funct3, f3_000));
    const auto f3_001_b = ch::core::ch_bool(ch::core::operator==(funct3, f3_001));
    const auto f3_010_b = ch::core::ch_bool(ch::core::operator==(funct3, f3_010));
    const auto f3_100_b = ch::core::ch_bool(ch::core::operator==(funct3, f3_100));
    const auto f3_101_b = ch::core::ch_bool(ch::core::operator==(funct3, f3_101));
    const auto is_lb  = ch::core::ch_bool(ch::core::operator&(is_load,  f3_000_b));
    const auto is_lh  = ch::core::ch_bool(ch::core::operator&(is_load,  f3_001_b));
    const auto is_lw  = ch::core::ch_bool(ch::core::operator&(is_load,  f3_010_b));
    const auto is_lbu = ch::core::ch_bool(ch::core::operator&(is_load,  f3_100_b));
    const auto is_lhu = ch::core::ch_bool(ch::core::operator&(is_load,  f3_101_b));
    const auto is_sb  = ch::core::ch_bool(ch::core::operator&(is_store, f3_000_b));
    const auto is_sh  = ch::core::ch_bool(ch::core::operator&(is_store, f3_001_b));
    const auto is_sw  = ch::core::ch_bool(ch::core::operator&(is_store, f3_010_b));

    // ── ② 地址（★ Bare：vaddr 字节地址直接用；★ MMU 翻译不在本版本）──
    const auto vaddr = n->operator()(KT::RESULT);
    // ★★★ **显式零扩展用 `zext<64>`**（★ 防御性写法，★ 非实测约束）：★
    //   `ch_uint<M>(ch_uint<N>)` 的跨宽构造在上游语义**未文档化**
    //   （★ `ibus_chmem.h:232` 注释只覆盖 M>N 的截断路径）⇒ ★ 显式 `zext`
    //   让「零扩展」意图无歧义。
    //   ★ （★ 早期版本曾出现 null impl ⇒ ★ 但变异对照显示与跨宽构造**等效**
    //     ⇒ ★ **不作为硬约束登记**。）
    const auto addr64 = ch::core::zext<64>(vaddr);
    // ★ byte offset（★ 用于读侧提取 + 由从设备推 byte-strobe）
    const auto byte_off = ch::core::ch_uint<2>(ch::core::bits<1, 0>(addr64));

    // ── ③ 访问字节宽度（★ 写语义的载体 —— ADR-091 v1.7）──
    const auto size1 = ch::core::ch_bool(ch::core::operator|(
        ch::core::operator|(is_lb, is_lbu), is_sb));
    const auto size2 = ch::core::ch_bool(ch::core::operator|(
        ch::core::operator|(is_lh, is_lhu), is_sh));
    const auto size3 = ch::core::ch_uint<3>(ch::core::ch_literal<1, 3>{});
    const auto size4 = ch::core::ch_uint<3>(ch::core::ch_literal<2, 3>{});
    const auto size8 = ch::core::ch_uint<3>(ch::core::ch_literal<4, 3>{});
    const auto size_val = ch::core::ch_uint<3>(ch::core::select(
        size1, size3, ch::core::select(size2, size4, size8)));

    // ── ④ 符号扩展标志（lb/lh ⇒ 1；lbu/lhu ⇒ 0）──
    const auto signed_flag = ch::core::ch_bool(ch::core::operator|(is_lb, is_lh));

    // ── ⑤ 写数据（★ 原始 RS2，★ **不做 RMW** —— 由从设备按 size 做 strobe）──
    const auto wdata = n->operator()(KT::RS2);
    // ★ 零扩展（同 ② ⇒ `zext` 而非跨宽构造）
    const auto wdata64 = ch::core::zext<64>(wdata);

    // ── ⑥ 请求侧**打 1 拍**（★ ⇒ 端口在拍 N 呈现本拍算出的请求）──
    get_addr_reg() <<= addr64;
    get_data_reg() <<= wdata64;
    get_size_reg() <<= size_val;
    // ★★★ **MUST 用 `select()`，MUST NOT 用 `?:`**（★ **实测约束 G-1r**
    //   —— ★ 本轮唯一被变异对照坐实的「布尔语义」约束）：
    //   ★ `ch_bool::operator bool` 是 **explicit** ⇒ ★ 在 C++ 条件上下文
    //   ★ （`?:` / `if`）里走 **contextual conversion** ⇒ ★ **静默取 false**
    //   ★ ⇒ ★ **变异对照 RED**（★ 把 `select()` 换回 `?:` ⇒ 62 assertions
    //   ★   ⇒ 2 处 FAILED：★ **写使能恒 0**（写请求发不出去 ⇒ 最坏假绿：
    //   ★   端口在、值全 0）+ ★ **符号扩展失效**（零扩展冒充符号扩展））
    //   ★ ⇒ ★ MUST 用**节点级 `select()`**（mux）替代 `?:`。
    // ★★★ **错路 store MUST 被抑制**（★ ADR-091 §D3a.5 ③ —— S3.2b）
    //   ★ 语义：`we = is_store & !flush`
    //   ★ `flush` 由 **ibus** 在 `fetch` stage 写（★ 打 1 拍 ⇒ 与发布的
    //     `(PC, INSTRUCTION)` 对齐）⇒ ★ 本 stage（`memory`）读 ⇒ ★ 可见
    //     （★ `fetch` 的 stage 首现序早于 `memory` —— ADR-040 v3.0 §10.6）
    //   ★ ★ **fail-open + 显式登记**（ADR-091 §D3a.5 ④）：★ 读不到 `FLUSH`
    //     （★ 只装 dmem 不装 ibus 的合法形态，如本文件的单测）⇒ ★ 视为 0
    //     ⇒ ★ 该风险由 **A6 的 H-3 等价性自检**兜底（终值 `memsig` 会不等）
    namespace BK = cf::cpu::core::branch_keys;
    ch::core::ch_bool flush_now = ch::core::ch_bool(ch::core::operator==(
        ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
        ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{})));
    if (auto* fch = pb.node_of_logic_stage("fetch").get()) {
      if (fch->payloads().has(BK::FLUSH)) {
        flush_now = fch->operator()(BK::FLUSH);
      }
    }
    // ★★★ **`we = is_store ? (flush ? 0 : 1) : 0`**（★ 纯嵌套 `select`）
    //   ★ 纪律（★ 本轮实测踩坑 —— G-1m 同族）：
    //     · **MUST NOT** 用 `ch_uint<1>(ch_bool)` 做布尔取反
    //       ⇒ ★ 该构造会读 C++ 侧值 ⇒ ★ 节点**折成常量** ⇒ ★ 抑制静默失效
    //     · **MUST NOT** 用混宽 `operator&(ch_bool, ch_uint<1>)`
    //       ⇒ ★ 结果宽度/语义依赖模板推导 ⇒ ★ 可读性差
    //   ★ ⇒ ★ 两层 `select`，cond 用 `ch_bool`，分支用 `ch_uint<1>` ⇒ 无歧义。
    const auto not_flushed = ch::core::ch_uint<1>(ch::core::select(
        flush_now, ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}),
                   ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{})));
    get_we_reg() <<= ch::core::ch_uint<1>(ch::core::select(
        is_store, not_flushed,
        ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{})));
    get_off_reg() <<= byte_off;
    // ★★★ **MUST 显式转 `ch_uint<1>`**（★ 实测约束 G-1p）：★
    //   `ch_reg<ch_uint<1>> <<= ch_bool` 的类型路径与 `ch_uint<1>` **不同**
    //   ⇒ ★ 实测：`signed_reg_` 恒为 0（★ 而 `size_reg_`（`ch_uint<3>`）正确）
    //   ⇒ ★ ⇒ `lb`/`lh` 的符号扩展**静默失效**（★ 零扩展冒充符号扩展）
    //   ⇒ ★ ⇒ MUST 显式 `ch_uint<1>(ch_bool)`。
    get_signed_reg() <<= ch::core::ch_uint<1>(ch::core::select(
        signed_flag, ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{}),
                     ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{})));

    // ── ⑦ 端口输出（★ 组合读 reg ⇒ 呈现「本拍 sequential 更新后的值」）──
    if (ports_.req_addr) {
      *ports_.req_addr = ch::core::ch_uint<64>(get_addr_reg());
    }
    if (ports_.req_data) {
      *ports_.req_data = ch::core::ch_uint<64>(get_data_reg());
    }
    if (ports_.req_size) {
      *ports_.req_size = ch::core::ch_uint<3>(get_size_reg());
    }
    if (ports_.req_we) {
      *ports_.req_we = ch::core::ch_uint<1>(get_we_reg());
    }
    if (ports_.req_valid) {
      // ★ 本版本只实现「`ready` 恒真」的无阻塞流 ⇒ `valid` 恒真
      // ★ （★ 背压未实现 ⇒ 已在文件头「诚实标注」登记）
      *ports_.req_valid = ch::core::ch_uint<1>(ch::core::ch_literal<1, 1>{});
    }
    // ★ `req_ready` **只读不用**（★ 恒真假设；★ MUST NOT 内部赋值 ⇒ G-1d）

    // ── ⑧ 响应侧**组合消费**（★ 消费拍 = 拍 N+1 ⇒ 数据对应**拍 N** 的地址）──
    // ★ 与预载核同款：★ 从设备返回**整个对齐字** ⇒ CPU 侧按 byte_off 提取
    if (ports_.rsp_data) {
      const auto rsp64 = ch::core::ch_uint<64>(*ports_.rsp_data);
      // ★ 显式转（G-1l）：★ `bits<>` 的结果直接参与 `>>` / `&` 会报
      //   「Invalid operands for binary operation」
      const auto word = ch::core::ch_uint<32>(ch::core::bits<31, 0>(rsp64));

      // ★ byte / half 提取（★ 与预载核 `dmem_chmem.h` 的 byte select 树同款）
      const auto off = ch::core::ch_uint<2>(get_off_reg());
      const auto is_b0 = ch::core::ch_bool(off == ch::core::ch_uint<2>(
          ch::core::ch_literal<0, 2>{}));
      const auto is_b1 = ch::core::ch_bool(off == ch::core::ch_uint<2>(
          ch::core::ch_literal<1, 2>{}));
      const auto is_b2 = ch::core::ch_bool(off == ch::core::ch_uint<2>(
          ch::core::ch_literal<2, 2>{}));
      const auto byte_shift = ch::core::ch_uint<5>(ch::core::select(
          is_b0, ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{}),
          ch::core::select(is_b1, ch::core::ch_uint<5>(ch::core::ch_literal<8, 5>{}),
                           ch::core::select(is_b2, ch::core::ch_uint<5>(ch::core::ch_literal<16, 5>{}),
                                            ch::core::ch_uint<5>(ch::core::ch_literal<24, 5>{})))));
      const auto is_h1 = ch::core::ch_bool(off == ch::core::ch_uint<2>(
          ch::core::ch_literal<2, 2>{}));
      const auto half_shift = ch::core::ch_uint<5>(ch::core::select(
          is_h1, ch::core::ch_uint<5>(ch::core::ch_literal<16, 5>{}),
          ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{})));

      // ★ 符号扩展（★ lb/lh ⇒ 符号扩展；lbu/lhu ⇒ 零扩展）
      // ★ 移位结果显式转（G-1l）
      const auto byte_shifted = ch::core::ch_uint<32>(word >> byte_shift);
      const auto half_shifted = ch::core::ch_uint<32>(word >> half_shift);
      const auto byte_sign_bit = ch::core::ch_bool(ch::core::bits<7, 7>(byte_shifted));
      const auto byte_sign_ext = ch::core::ch_uint<32>(ch::core::select(
          byte_sign_bit, ch::core::ch_uint<32>(ch::core::ch_literal<0xFFFFFF00, 32>{}),
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{})));
      const auto half_sign_bit = ch::core::ch_bool(ch::core::bits<15, 15>(half_shifted));
      const auto half_sign_ext = ch::core::ch_uint<32>(ch::core::select(
          half_sign_bit, ch::core::ch_uint<32>(ch::core::ch_literal<0xFFFF0000, 32>{}),
          ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{})));

      const auto byte_val = ch::core::ch_uint<32>(
          byte_shifted & ch::core::ch_uint<32>(ch::core::ch_literal<0xFF, 32>{}));
      const auto byte_signed = ch::core::ch_uint<32>(byte_val | byte_sign_ext);
      const auto half_val = ch::core::ch_uint<32>(
          half_shifted & ch::core::ch_uint<32>(ch::core::ch_literal<0xFFFF, 32>{}));
      const auto half_signed = ch::core::ch_uint<32>(half_val | half_sign_ext);

      // ★ 消费拍的 load/store 判定 MUST 取**拍 N** 的值
      //   （★ `we_reg_` 打 1 拍 ⇒ 拍 N+1 组合期读到的正是拍 N 的写使能）
      // ★ **`ch_bool` 没有 `operator!`**（★ 实测：报「Invalid operand for unary
      //   operation」）⇒ ★ 用**比较**代替取反：★ `we == 0` ⇒ 读
      const auto we_c = ch::core::ch_uint<1>(get_we_reg());
      const auto is_load_c = ch::core::ch_bool(
          we_c == ch::core::ch_uint<1>(ch::core::ch_literal<0, 1>{}));
      const auto size_c = ch::core::ch_uint<3>(get_size_reg());
      const auto signed_c = ch::core::ch_bool(ch::core::ch_uint<1>(get_signed_reg()));

      const auto is_size1_c = ch::core::ch_bool(size_c == ch::core::ch_uint<3>(
          ch::core::ch_literal<1, 3>{}));
      const auto is_size2_c = ch::core::ch_bool(size_c == ch::core::ch_uint<3>(
          ch::core::ch_literal<2, 3>{}));
      const auto is_size4_c = ch::core::ch_bool(size_c == ch::core::ch_uint<3>(
          ch::core::ch_literal<4, 3>{}));

      // ★ 符号扩展 vs 零扩展（★ 由拍 N 的 signed 标志决定）
      const auto byte_final = ch::core::ch_uint<32>(ch::core::select(
          signed_c, byte_signed, byte_val));
      const auto half_final = ch::core::ch_uint<32>(ch::core::select(
          signed_c, half_signed, half_val));

      const auto load_result = ch::core::ch_uint<32>(ch::core::select(
          is_size4_c, word,
          ch::core::select(is_size2_c, half_final, byte_final)));

      n->operator()(KT::RD_DATA) = ch::core::ch_uint<32>(ch::core::select(
          is_load_c, load_result, n->operator()(KT::RD_DATA)));

      // ★ 顶层观测端口（H-2）—— ★ 连到 `RD_DATA` 的**同一组合节点**
      //   ⇒ ★ 保证「观测到的」与「下游消费的」是同一个值（★ 单一真相）
      if (ports_.rd_data) {
        *ports_.rd_data = ch::core::ch_uint<32>(n->operator()(KT::RD_DATA));
      }
    }
  }
};

}  // namespace core
}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CORE_DMEM_PORT_CHMEM_H

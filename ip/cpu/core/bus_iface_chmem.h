// ip/cpu/core/bus_iface_chmem.h
//
// 功能描述: CH_MEM 开放核的**总线接口契约** —— `CpuBusReq` / `CpuBusResp`
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-28
//
// ============================================================================
// 归属 (ADR-091 v1.3 §D3a.3)
// ============================================================================
// ★ 本文件是 ADR-091「CH_MEM 开放核形态与端口架构」的**接口契约落地**：
//   · 核对外暴露 `ch_stream<CpuBusReq>` / `ch_stream<CpuBusResp>`
//   · 取指与访存共用同一组载荷 ⇒ ★ **一套契约，三处复用**：
//       ① ip 级单测  ⇒ 接 `verify_mem_model_chmem.h`（验证实现 / slave）
//       ② SoC 级     ⇒ 接真实 Cache / MMU IP（同为 slave ⇒ 角色对称）
//       ③ 未来总线   ⇒ 可再包一层 AXI4 适配
//
// ============================================================================
// 契约要点 (逐条对应 ADR-091 v1.3)
// ============================================================================
//   ① CH_MEM-only：字段类型一律 `ch::core::ch_uint<N>`（★ 不是
//      `cf::plugin::uint_t` ⇒ 保持零 TLM 兼容 ⇒ ADR-092）
//      ⇒ ★ 本文件 **MUST NOT** include `<ch.hpp>`
//        （★ `ch.hpp` → `module.h` ⇒ 其 `build_hierarchical_name` 缺 `inline`
//          ⇒ **任何 2+ TU include 即链接失败** ⇒ 实测踩到）
//        ⇒ 只 include 具体头（`core/bundle/bundle_base.h` · `core/uint.h`）
//
//   ② 真名纪律：★ **MUST NOT** 叫 `MemReqBundle` / `MemRespBundle`
//      —— 那两个名字在代码中**不存在**（实际类型是 `cf::bundles::MemReq` /
//      `MemResp`，POD 形态）⇒ 沿用会**继承悬空引用** ⇒ ADR-092 §D7
//      ⇒ ★ 且**刻意不与**老核 bundle 重名 ⇒ 形态专属命名 ⇒ 零歧义
//
//   ③ 宽度纪律（H-1）：★ **每个字段 MUST ≤ 64 bit**
//      —— `VerilatorBackend::build_port_access_table()` 对 `size() > 64` 的
//         `type_input`/`type_output` 端口返回 `PortBitwidthOverflow`
//      —— ★ bundle 端口**按字段展开**（`as_master_direction()` ⇒ 逐字段
//         `make_output`/`make_input`）⇒ 总宽可超，**单字段不可超**
//      —— 当前 `address`/`data` 各 64 bit **恰好卡在上限** ⇒ ★ 禁止再加宽
//
//   ④ 方向语义：请求侧 = master（输出 payload/valid，输入 ready）
//                响应侧 = slave （输入 payload/valid，输出 ready）
//      —— 与 CppHDL `ch_stream` 的 `as_master_direction()` /
//         `as_slave_direction()` 一一对应
//
//   ⑤ 握手成本（ADR-091 §D3a.2）：从设备 MUST 可将 `ready` 恒真
//      ⇒ 退化为**无阻塞流** ⇒ 单周期同步场景不被握手拖慢
//      ⇒ ★ MUST NOT 默认假设「握手免费」⇒ 时序判据分两档测
//
// ============================================================================
// CH_MEM 纪律 (ADR-040 v3.0)
// ============================================================================
//   · 本文件是 CH_MEM-only ⇒ 不参与 TLM 路径 ⇒ 不引 `cf::plugin::uint_t`
//   · 本文件**只含头**（无 .cpp）
//   · 字段一律 `ch::core::ch_uint<N>`；★ 禁裸 `bool` / `uint64_t`
//
// ============================================================================

#ifndef CF_IP_CPU_CORE_BUS_IFACE_CHMEM_H
#define CF_IP_CPU_CORE_BUS_IFACE_CHMEM_H

// ★ 只 include 具体头（MUST NOT <ch.hpp> —— 见文件头 ①）
// ★★ **顺序要紧**：★ `core/traits.h` 定义 `ch_width_v`（bundle_base.h 的
//    bundle_layout.h 依赖它）⇒ MUST 在 bundle_base.h **之前**
//    （`ch.hpp` 正是靠这个顺序让 bundle_base 可用的 —— 而我们不能 include ch.hpp）
#include <core/traits.h>
#include <core/bool.h>
#include <core/bundle/bundle_base.h>
#include <core/bundle/bundle_meta.h>   // ★ CH_BUNDLE_FIELDS_T 宏在此
#include <core/uint.h>

namespace cf {
namespace cpu {

// ★★ `CH_BUNDLE_FIELDS_T` 宏内部用的是**非限定名** `bundle_base<Self>`
//    ⇒ MUST 在本作用域 `using` 之（★ 上游 `bundle/common_bundles.h:13` 同款做法）
//    ⇒ 否则展开时报「expected nested-name-specifier before 'bundle_base'」
using ch::core::bundle_base;

// ============================================================================
// CpuBusReq —— 总线请求载荷（核 → 从设备）
// ============================================================================
// ★ 字段布局沿用 AXI 风格（与老核 `cf::bundles::MemReq` 同形）⇒ 便于迁移，
//   但**刻意不同名**（见文件头 ②）⇒ 避免与老核 POD bundle 歧义。
template <typename AddrT = ch::core::ch_uint<64>,
          typename DataT = ch::core::ch_uint<64>,
          typename IdT = ch::core::ch_uint<8>,
          typename BurstLenT = ch::core::ch_uint<8>>
struct CpuBusReq : public ch::core::bundle_base<CpuBusReq<AddrT, DataT, IdT, BurstLenT>> {
  using Self = CpuBusReq<AddrT, DataT, IdT, BurstLenT>;

  // ── 宽度常量（H-1：逐字段 ≤ 64 bit）──
  static constexpr unsigned kAddressWidth = AddrT::width;
  static constexpr unsigned kDataWidth = DataT::width;
  static constexpr unsigned kIdWidth = IdT::width;
  static constexpr unsigned kBurstLenWidth = BurstLenT::width;

  // ── 载荷字段 ──
  // ★★ **MUST NOT 带字面量初始化器**（★ 实测约束 G-1f）：
  //   `AddrT address{0}` 会在**默认构造时**就创建字面量节点
  //   ⇒ ★ **无 active context 时报「No active context for literal creation」**
  //   ⇒ 值退化为默认且**不报错** ⇒ 假绿通道。
  //   ⇒ `ch_uint()` / `ch_bool()` 的**默认构造不创建节点**（null impl）⇒ 安全。
  //   ⇒ 上游 `bundle/common_bundles.h` 的 `fifo_bundle` 同款做法（`T data;` 无初始化器）。
  AddrT address;           // 请求地址（物理/虚拟由接在核上的 MMU 决定）
  DataT data;              // 写数据（读操作时未使用）
  BurstLenT burst_len;     // 突发长度
  IdT id;                  // 请求标识
  ch::core::ch_bool is_write;  // true = 写 / false = 读

  CH_BUNDLE_FIELDS_T(address, data, burst_len, id, is_write)

  // ── 方向语义（契约 ④）──
  /// master 方向：输出请求（payload/valid），输入 ready
  void as_master_direction() {
    this->make_output(address, data, burst_len, id, is_write);
    this->make_input(/* ready 由 ch_stream 自身提供 */);
  }

  /// slave 方向：输入请求，输出 ready
  void as_slave_direction() {
    this->make_input(address, data, burst_len, id, is_write);
    this->make_output(/* ready 由 ch_stream 自身提供 */);
  }

  // ★ 刻意**不**提供 `payload_valid()` 之类的「返回常量 ch_bool」辅助 ——
  //   ★ `ch_bool(true)` 每次调用都会**创建字面量节点**
  //   ⇒ 无 active context 时报「No active context for literal creation」
  //   ⇒ ★ 且语义空洞（恒 true 无信息量）⇒ 删除优于保留。
  //   ⇒ payload 有效性由 `ch_stream` 自身的 `valid` 信号承载。
};

// ============================================================================
// CpuBusResp —— 总线响应载荷（从设备 → 核）
// ============================================================================
template <typename DataT = ch::core::ch_uint<64>, typename IdT = ch::core::ch_uint<8>>
struct CpuBusResp : public ch::core::bundle_base<CpuBusResp<DataT, IdT>> {
  using Self = CpuBusResp<DataT, IdT>;

  static constexpr unsigned kDataWidth = DataT::width;
  static constexpr unsigned kIdWidth = IdT::width;

  // ── 载荷字段 ──
  // ★★ 同上（G-1f）：MUST NOT 带字面量初始化器
  DataT data;              // 读数据
  IdT id;                  // 对应请求的标识
  ch::core::ch_bool error; // 从设备报错
  ch::core::ch_bool last;  // 突发末拍

  CH_BUNDLE_FIELDS_T(data, id, error, last)

  /// master 方向：输出响应（payload/valid），输入 ready
  void as_master_direction() {
    this->make_output(data, id, error, last);
    this->make_input(/* ready 由 ch_stream 自身提供 */);
  }

  /// slave 方向：输入响应，输出 ready
  void as_slave_direction() {
    this->make_input(data, id, error, last);
    this->make_output(/* ready 由 ch_stream 自身提供 */);
  }
};

}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CORE_BUS_IFACE_CHMEM_H

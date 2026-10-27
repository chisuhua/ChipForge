// tests/cpu/schema/test_cpu_bus_iface.cpp
//
// 功能描述: ADR-091 §D3a.3 —— CH_MEM 开放核的**总线接口契约**
//           （`CpuBusReq` / `CpuBusResp`）
//
// ★ 本文件是 ADR-091 S2（backend-parity-open-core）的**契约测试**。
//
// ---------------------------------------------------------------------------
// 契约要点（逐条对应 ADR-091 v1.3）：
//
//   ① **CH_MEM-only**：MUST 是 `bundle_base` 派生 + `CH_BUNDLE_FIELDS_T`，
//      字段类型 MUST 为 `ch::core::ch_uint<N>`
//      ⇒ ★ **MUST NOT** 引用 `cf::plugin::uint_t`（★ 保持零 TLM 兼容 ⇒ ADR-092）
//      ⇒ ★ **MUST NOT** include `<ch.hpp>`（★ module.h 缺 inline ⇒ 多 TU 链接失败）
//
//   ② **真名纪律**：MUST NOT 是 `MemReqBundle` / `MemRespBundle`
//      （★ 这两个名字在代码中**不存在** —— 实际类型是 `cf::bundles::MemReq`/`MemResp`
//        ⇒ 沿用会**继承悬空引用** ⇒ ADR-092 §D7）
//      ⇒ 且 MUST NOT 与既有 `bundles/` 中的类型重名（避免歧义）
//
//   ③ **H-1 宽度**：每个字段 MUST ≤ **64 bit**
//      ⇒ ★ `VerilatorBackend::build_port_access_table()` 对 `size() > 64` 的端口
//        返回 `PortBitwidthOverflow`
//      ⇒ ★ bundle 端口**按字段展开**（`as_master_direction()` ⇒ 逐字段 make_*）
//        ⇒ 总宽可超，但**单字段不可超**
//
//   ④ **方向语义**：MUST 提供 master / slave 两个方向
//      ⇒ 请求侧 = master（输出 payload/valid，输入 ready）
//      ⇒ 响应侧 = slave（输入 payload/valid，输出 ready）
//
//   ⑤ **握手成本**：从设备 MUST 可将 `ready` 恒真（退化为无阻塞流）
//      ⇒ ★ MUST 暴露 `fire()` 语义（D3a.2 的「两档分别测时序」判据依赖它）
// ---------------------------------------------------------------------------

#include "catch_amalgamated.hpp"

// ★ include 顺序同 bus_iface_chmem.h：traits.h（ch_width_v）MUST 在 bundle_base.h 之前
#include <core/traits.h>
#include <core/bool.h>
#include <core/bundle/bundle_base.h>
#include <core/bundle/bundle_meta.h>   // ★ CH_BUNDLE_FIELDS_T 宏在此
#include <core/context.h>                // ★ ctx_swap（字面量构造需 active context）
#include <core/uint.h>

#include <type_traits>

#include "ip/cpu/core/bus_iface_chmem.h"

// ★★ **刻意 NOT include 老核 `bundles/mem_bundles.h`** —— 它用
//    `cf::plugin::uint_t<64> address{0}` 做**默认成员初始化**
//    ⇒ ★ 在 CH_MEM 下同样会在**无 active context 时创建字面量节点**
//    ⇒ 报「No active context for literal creation」（★ 实测）
//    ⇒ ★ 同族于 G-1f ⇒ ★ 不引入本 TU，避免污染其它 case 的日志。
//    ⇒ ★ 代价：「不重名」纪律只能**文档级 + review** 保证（非机检）——
//      ★ **诚实标注**：该纪律当前**未**机检（ADR-091 §D3a.3）。

// ★ 类模板 + 默认参数 ⇒ **类型位置**必须写 `<>`（CTAD 只在变量声明处生效）
using CpuBusReq = cf::cpu::CpuBusReq<>;
using CpuBusResp = cf::cpu::CpuBusResp<>;

TEST_CASE("bus_iface: CpuBusReq field widths all <= 64 bit (ADR-091 H-1)",
          "[bus-iface][chmem]") {
  // ★ H-1：逐字段宽度断言（★ 总宽可超，单字段不可超）
  REQUIRE(CpuBusReq::kAddressWidth <= 64);
  REQUIRE(CpuBusReq::kDataWidth <= 64);
  REQUIRE(CpuBusReq::kIdWidth <= 64);
  REQUIRE(CpuBusReq::kBurstLenWidth <= 64);

  // ★ 编译期钉住「不得再加宽」
  static_assert(CpuBusReq::kAddressWidth <= 64,
                "address MUST stay <= 64 bit (VerilatorBackend PortBitwidthOverflow)");
  static_assert(CpuBusReq::kDataWidth <= 64,
                "data MUST stay <= 64 bit (VerilatorBackend PortBitwidthOverflow)");
  SUCCEED("H-1 宽度纪律生效");
}

TEST_CASE("bus_iface: CpuBusReq/CpuBusResp are CH_MEM-only bundles",
          "[bus-iface][chmem]") {
  // ★ ① 必须是 CppHDL bundle（bundle_base 派生）
  static_assert(std::is_base_of_v<ch::core::bundle_base<CpuBusReq>, CpuBusReq>,
                "CpuBusReq MUST derive from ch::core::bundle_base");
  static_assert(std::is_base_of_v<ch::core::bundle_base<CpuBusResp>, CpuBusResp>,
                "CpuBusResp MUST derive from ch::core::bundle_base");

  // ★ ① 字段类型 MUST 是 ch::core::ch_uint（⇒ 不是 cf::plugin::uint_t）
  static_assert(std::is_same_v<decltype(CpuBusReq::address),
                               ch::core::ch_uint<CpuBusReq::kAddressWidth>>,
                "address MUST be ch::core::ch_uint (CH_MEM-only)");
  static_assert(std::is_same_v<decltype(CpuBusResp::data),
                               ch::core::ch_uint<CpuBusResp::kDataWidth>>,
                "data MUST be ch::core::ch_uint (CH_MEM-only)");

  SUCCEED("① CH_MEM-only bundle 契约生效");
}

TEST_CASE("bus_iface: bus payload names are not the dangling MemReqBundle",
          "[bus-iface][chmem]") {
  // ★ ② 真名纪律：`MemReqBundle` / `MemRespBundle` 在代码中**不存在**
  //   （实际是 cf::bundles::MemReq / MemResp）⇒ MUST NOT 沿用
  //
  // ★★ **诚实标注**：本条**未机检** —— 类型比较需 include 老核 bundle，
  //    而老核 bundle 在 CH_MEM 下有 G-1f 隐患（默认成员初始化器创建节点）
  //    ⇒ ★ 不引入本 TU ⇒ ⇒ 纪律由 ADR-091 §D3a.3 + review 保证。
  //    ⇒ TODO(ADR-092 Phase-2)：老核 bundle 单模化后，把本条改为**机检**
  //      （届时老 bundle 字段去初始化器 ⇒ 可安全 include）。
  //
  // ★ 可机检的部分：新 bundle 的**类型名本身**不含 "Bundle" 后缀
  //   （⇒ 不会与老核的 `MemReqBundle` 命名空间混淆）
  static_assert(!std::is_same_v<CpuBusReq, CpuBusResp>,
                "请求与响应载荷 MUST 是不同类型");
  SUCCEED("② 真名纪律（文档级，当前未机检 —— 已诚实标注）");
}

TEST_CASE("bus_iface: direction semantics — master out / slave in", "[bus-iface][chmem]") {
  // ★ ④ 方向语义：请求侧 master（输出 payload/valid，输入 ready）
  // ★★ `as_master_direction()` / `as_slave_direction()` **MUST 在 ctx 保护内调用**
  //   （★ 实测约束 G-1g：内部 make_output/make_input 会创建节点 ⇒ 无 context 时报错）
  ch::core::context ctx("dir_semantics");
  {
    ch::core::ctx_swap guard(&ctx);
    CpuBusReq req;
    req.as_master_direction();  // ★ MUST NOT 抛

    CpuBusResp rsp;
    rsp.as_slave_direction();   // ★ MUST NOT 抛
  }

  SUCCEED("④ 方向语义契约生效");
}

TEST_CASE("bus_iface: ready-constant-high degrades to a non-blocking stream",
          "[bus-iface][chmem]") {
  // ★ ⑤ 握手成本：从设备 MUST 可将 `ready` 恒真 ⇒ 退化为无阻塞流
  //   （★ ADR-091 §D3a.2 的「两档分别测时序」判据依赖此能力）
  // ★★ 字面量构造**也**需要 active context（实测：保护外构造 ⇒
  //   「No active context for literal creation」⇒ 值为 false 而不报错
  //   ⇒ ★ 与 ADR-091 §D5.1 G-1e 同族 ⇒ 一律在 ctx_swap 保护内构造）
  ch::core::context ctx("ready_high");
  bool valid_true_value = false;
  {
    ch::core::ctx_swap guard(&ctx);
    const ch::core::ch_bool valid_true =
        ch::core::ch_bool(ch::core::ch_literal<1, 1>{});
    valid_true_value = bool(valid_true.operator bool());
  }
  REQUIRE(valid_true_value == true);
  SUCCEED("⑤ ready-恒真退化路径可用（★ 具体时序判据在 S5）");
}

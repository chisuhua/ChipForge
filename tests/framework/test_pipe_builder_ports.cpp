// tests/framework/test_pipe_builder_ports.cpp
//
// 功能描述: ADR-091 §D5 G-1 —— PipeBuilder 顶层端口声明能力的**机制验证**
//           （裸 ctx + ch_in/ch_out ⇒ 生成的网表顶层端口列表）
//
// ★ 本文件在 ADR-091 S1 落地前是**机制探针**（PoC），S1 落地后转为契约测试。
//
// ---------------------------------------------------------------------------
// 验证 3 件事（对应 ADR-091 G-1 的三个设计约束，均为实测得出，非推断）：
//
//   A. 在**裸 context**（无 CppHDL Component / ch_device）里创建
//      `ch::core::ch_in<T>` / `ch_out<T>` 节点 ⇒ 它们出现在
//      `ch::toVerilog()` 生成的网表**顶层端口列表**中
//      ★ 这条成立 ⇒ ChipForge 无需引入 CppHDL 的 Component 体系
//      （实测：上游 `VerilatorBackend::try_initialize(ctx*, data_map_t&)`
//        只要 ctx + data_map ⇒ 不需要 ch_device）
//
//   B. `ch_logic_in` 的构造是 `if (ctx) { ctx->create_input(...) }`
//      ⇒ ★ **无 current context 时静默不建节点**（impl() == nullptr）
//      ⇒ 端口会「消失」而**不报错** ⇒ 生成网表少端口 ⇒ ★ **假绿通道**
//      ⇒ 故 G-1 的 API MUST fail-closed 断言 ctx 非空
//
//   C. `ch_in` 若被**内部驱动**（有 driver），它**不是**顶层端口
//      （上游 `verilator_backend.cpp:617` 的 skip 逻辑：inputimpl::driver() != nullptr
//        ⇒ 不进 port_access_；codegen 侧同理不发射为顶层 input）
//      ⇒ ★ 端口方向必须严格：请求输出用 `ch_out`，响应输入用 `ch_in`
//      且 `ch_in` MUST NOT 被内部逻辑赋值
// ---------------------------------------------------------------------------
//
// 归属: ADR-091（CH_MEM 开放核形态与端口架构）§D5 G-1 / §D6 H-2
// 关联: ADR-089（双后端差分 parity 门禁）· ADR-092（TLM 参考实现的冻结）

#include "catch_amalgamated.hpp"

// ★★ MUST NOT include <ch.hpp> here —— 它 include 了 module.h, 而
//    module.h:13 的 build_hierarchical_name **缺 inline** ⇒ 任何 2+ TU
//    include <ch.hpp> 即链接失败 (multiple definition)。
//    ⇒ 只 include 需要的具体头 (ADR-091 G-1 的同款约束: 框架头亦不得 include ch.hpp)。
#include <ast/ast_nodes.h>
#include <codegen_verilog.h>
#include <core/context.h>
#include <core/interpreter_backend.h>
#include <core/literal.h>
#include <core/io.h>
#include <core/uint.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"
#include "cf/plugin/plugin_exception.h"

#include <type_traits>

#include <cstdio>
#include <fstream>
#include <string>

namespace {

// 在生成的网表里查某个端口名是否出现在顶层端口声明行
bool port_declared_in_verilog(const std::string& verilog_path,
                              const std::string& port_name) {
  std::ifstream f(verilog_path);
  if (!f) return false;
  std::string line;
  while (std::getline(f, line)) {
    // 顶层端口声明形如 "    input 32 poc_in;" / "    output 32 poc_out;"
    const bool is_decl = line.find("input") != std::string::npos ||
                         line.find("output") != std::string::npos;
    if (is_decl && line.find(port_name) != std::string::npos) return true;
  }
  return false;
}

}  // namespace


/// ★ 解包 `Result<Port>`（★ ADR-047：静态配置头禁 throw ⇒ 端口声明走
///   `Result`）⇒ ★ 失败即 fail-closed（★ 不产生「半成品端口对象」）
///
/// ★★★ **MUST 按值返回**（★ 不可返回引用）：★ `Result<P>` 是**临时对象**
///   ⇒ ★ 若返回 `*r`（引用）⇒ ★ 引用悬垂（★ full-expression 结束后临时对象
///   ★ 析构 ⇒ ★ 端口节点的 `impl_` 随之失效 ⇒ ★ 端口「消失」⇒ 假绿）
///   ⇒ ★ 按值返回 + C++17 copy elision ⇒ ★ 不触发 `port` 的 deleted 拷贝赋值
template <typename P>
P must(cf::plugin::Result<P> r) {
  REQUIRE(r.has_value());
  return *r;
}
TEST_CASE("ports: bare ctx can declare top-level ch_in/ch_out (ADR-091 G-1 A)",
          "[ports][chmem]") {
  ch::core::context ctx("port_probe");
  {
    ch::core::ctx_swap guard(&ctx);

    ch::core::ch_in<ch::core::ch_uint<32>> poc_in("poc_in");
    ch::core::ch_out<ch::core::ch_uint<32>> poc_out("poc_out");

    // A1: 两个端口都必须真的建了节点（impl() 非空）
    REQUIRE(poc_in.impl() != nullptr);
    REQUIRE(poc_out.impl() != nullptr);

    // A2: 给 ch_out 一个 driver（否则「未驱动」节点可能不被发射）
    poc_out = ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{});
  }

  const std::string vpath = "/tmp/opencode/poc_ports_top.v";
  ch::toVerilog(vpath, &ctx);

  // A3: ★ 两个端口都必须出现在网表的**顶层端口声明**里
  REQUIRE(port_declared_in_verilog(vpath, "poc_in"));
  REQUIRE(port_declared_in_verilog(vpath, "poc_out"));
}

TEST_CASE("ports: ch_in without current context is silently null (ADR-091 G-1 B)",
          "[ports][chmem]") {
  ch::core::context ctx("port_probe2");
  {
    ch::core::ctx_swap guard(&ctx);
    ch::core::ch_in<ch::core::ch_uint<32>> with_ctx("poc2_in");
    REQUIRE(with_ctx.impl() != nullptr);
  }
  // ★ 离开 ctx_swap 后 current context 恢复为上一个（可能是 nullptr）
  ch::core::ch_in<ch::core::ch_uint<32>> without_ctx("poc3_in");
  // ★ 断言这个「静默不建节点」的行为 —— 它是 G-1 必须 fail-closed 的**根因**
  //   （若上游将来改成 fail-fast，本断言会红 ⇒ 届时 G-1 的断言可放宽）
  REQUIRE(without_ctx.impl() == nullptr);
}


// ===========================================================================
// 以下 4 个 case 是 **ADR-091 §D5 G-1 / G-5a 的契约测试**（S1 的 RED 面）
// ===========================================================================

// ★★ 下面 4 个 case 依赖 `cf::plugin::PipeBuilder` 的 **CH_MEM 模态** API
//    （`PipeBuilder(ctx*)` / `to_verilog` / `create_simulator` 全在
//     `pipe_builder.h` 的 `#ifdef CF_PLUGIN_USE_CH_MEM` 内）
//    ⇒ TLM binary 编译时跳过；★ CH_MEM binary 需**显式**加入
//      `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES`（★ K4 实测知识：
//      TLM binary 是 GLOB_RECURSE 自动收集，CH_MEM binary 是显式列表）
#ifdef CF_PLUGIN_USE_CH_MEM

TEST_CASE("ports: PipeBuilder declare_in/out land in netlist top-level ports",
          "[ports][chmem]") {
  ch::core::context ctx("pb_ports");
  cf::plugin::PipeBuilder pb(&ctx);

  auto cpu_req = must(pb.declare_out<ch::core::ch_uint<32>>("cpu_req"));
  auto cpu_rsp = must(pb.declare_in<ch::core::ch_uint<32>>("cpu_rsp"));
  REQUIRE(cpu_req.impl() != nullptr);
  REQUIRE(cpu_rsp.impl() != nullptr);

  // ★ 驱动 cpu_req（output 需 driver 才会被发射）
  // ★★ **赋值 MUST 在 ctx_swap 保护下做** —— 否则字面量创建会失败
  //    （实测：[ERROR] No active context for literal creation ⇒ output 无 driver
  //     ⇒ 网表里该 output 的值是未定义/X ⇒ ★ 假绿通道）
  {
    ch::core::ctx_swap guard(&ctx);
    cpu_req = ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{});
  }

  const std::string vpath = "/tmp/opencode/pb_ports_top.v";
  ch::toVerilog(vpath, &ctx);
  REQUIRE(port_declared_in_verilog(vpath, "cpu_req"));
  REQUIRE(port_declared_in_verilog(vpath, "cpu_rsp"));
}

TEST_CASE("ports: PipeBuilder with null ctx fails closed on declare", "[ports][chmem]") {
  // ★ G-1c：ctx 为 null MUST 抛异常 —— MUST NOT 静默不建节点
  //   （ch_logic_in 构造是 `if (ctx) {...}` ⇒ 无 ctx 时静默跳过 ⇒ 假绿通道）
  // ★★★ **v1.8 修正**：★ 原实现是 `throw PluginException` ⇒ ★ 违反 ADR-047
  //   Check 10（静态配置头文件禁 throw）⇒ ★ 改为返回
  //   `std::unexpected(PluginError::NullContextPtr)` ⇒ ★ **fail-closed 语义
  //   不变**（★ 调用方拿不到端口 ⇒ 不会静默不建节点 ⇒ 假绿通道仍堵死）。
  cf::plugin::PipeBuilder pb(nullptr);
  const auto r_in = pb.declare_in<ch::core::ch_uint<32>>("null_in");
  REQUIRE_FALSE(r_in.has_value());
  REQUIRE(r_in.error() == cf::plugin::PluginError::NullContextPtr);
  const auto r_out = pb.declare_out<ch::core::ch_uint<32>>("null_out");
  REQUIRE_FALSE(r_out.has_value());
  REQUIRE(r_out.error() == cf::plugin::PluginError::NullContextPtr);
}

TEST_CASE("ports: G-6 — PipeBuilder exposes the ctx for ch::Component sharing",
          "[ports][chmem]") {
  // ★★ **G-6**（ADR-091 v1.12 §D5）：★ `PipeBuilder` 暴露**只读 ctx accessor**
  //   ⇒ ★ 供 `ch::Component::build(external_ctx)` **共用 ctx**
  //   ⇒ ★ 满足硬约束 **G-1p**（★ 子 ctx ⇒ node-id aliasing ⇒ 数据丢失 ⇒ 假绿）
  //
  // ★ 本 case 只验「accessor 契约」；★ 「Component 真能共用 ctx」由
  //   `tests/cpu/test_cpu_open_core_regfile.cpp`（A4-1）验。

  // ★ ① 正常路径：accessor 返回构造时传入的 ctx（**同一指针**）
  ch::core::context ctx("g6_ctx");
  cf::plugin::PipeBuilder pb(&ctx);
  REQUIRE(pb.ctx() == &ctx);

  // ★ ② 空 ctx 路径：MUST 返回 nullptr（★ **不抛** ⇒ ADR-047 Check 10 合规）
  //   ⇒ ★ 且 `declare_*` 仍 fail-closed（★ G-1c 语义不变）
  cf::plugin::PipeBuilder pb_null(nullptr);
  REQUIRE(pb_null.ctx() == nullptr);
  REQUIRE_FALSE(pb_null.declare_out<ch::core::ch_uint<32>>("g6_null").has_value());

  // ★ ③ accessor MUST **只读**：★ 返回 `context*` ⇒ ★ 调用方**不能**替换
  //   PipeBuilder 的 ctx（★ 生命周期仍由 caller 管理）
  static_assert(std::is_same_v<decltype(pb.ctx()), ch::core::context*>,
                "ctx() MUST return a raw context pointer (read-only accessor)");
  // ★ 注：★ 成员函数的 const 限定体现在**成员指针类型**里
  //   ⇒ ★ `is_const_v<decltype(&C::f)>` **不适用**（★ 它判的是类型自身的 const）
  static_assert(std::is_same_v<decltype(&cf::plugin::PipeBuilder::ctx),
                               ch::core::context* (cf::plugin::PipeBuilder::*)() const>,
                "ctx() MUST be a const member function");
}

TEST_CASE("ports: G-1d — driven ch_in is not a top-level port", "[ports][chmem]") {
  // ★ G-1d（行为判据）：有内部 driver 的 `ch_in` **不是**顶层端口
  //   （上游 verilator_backend.cpp:617 的 skip 逻辑：inputimpl::driver() != nullptr
  //     ⇒ 不进 port_access_；codegen 侧同样不发射为顶层 input）
  //
  // ★ 注：`ch_in` 的「值赋值不可用」**无法**用 is_assignable 检出
  //   （其 operator= 是模板 + static_assert 在**函数体内** ⇒ 签名匹配 ⇒ 报 true）
  //   ⇒ 真正的阻止发生在**实例化时**（编译错误）⇒ 本仓库改用**行为判据**。

  // 编译期可检的部分：ch_out 可赋值（output 需内部 driver 才会被发射）
  static_assert(std::is_assignable_v<ch::core::ch_out<ch::core::ch_uint<32>>&,
                                      ch::core::ch_uint<32>>,
                "ch_out MUST be assignable —— output 端口需要内部 driver");

  ch::core::context ctx("driven_in");
  {
    ch::core::ctx_swap guard(&ctx);
    ch::core::ch_in<ch::core::ch_uint<32>> driven("driven_in_port");
    REQUIRE(driven.impl() != nullptr);

    // ★ 制造内部 driver：把该 input 端口接到一个内部节点上
    auto* driver_node = ctx.create_output(32, "driven_in_driver",
                                          std::source_location::current());
    REQUIRE(driver_node != nullptr);
    auto* in_node = static_cast<ch::core::inputimpl*>(driven.impl());
    in_node->set_driver(driver_node);
  }

  const std::string vpath = "/tmp/opencode/driven_in_top.v";
  ch::toVerilog(vpath, &ctx);
  // ★ 被内部驱动的 ch_in MUST NOT 出现在顶层端口列表
  REQUIRE_FALSE(port_declared_in_verilog(vpath, "driven_in_port"));
}

TEST_CASE("ports: create_simulator accepts an injected backend", "[ports][chmem]") {
  ch::core::context ctx("pb_backend");
  cf::plugin::PipeBuilder pb(&ctx);

  // ★ G-5a：注入后端 ⇒ active_backend_name() 反映该后端
  auto sim = pb.create_simulator(std::make_unique<ch::InterpreterBackend>());
  REQUIRE(sim.has_value());
  REQUIRE(sim.value()->active_backend_name() == "interpreter");

  // ★ nullptr ⇒ 保持既有行为（向后兼容）
  auto sim_default = pb.create_simulator();
  REQUIRE(sim_default.has_value());
  REQUIRE(sim_default.value()->active_backend_name() != "interpreter");
}

#endif  // CF_PLUGIN_USE_CH_MEM


// ============================================================================
// ★ C2（2026-10-28）：G-1h / G-1i / G-1j 的**机检**（★ 此前**无任何测试**）
// ============================================================================
// ★ 背景：★ ADR-091 §D5.1 声明「MUST 逐条机检」⇒ ★ 但 G-1e/f/g/h/i/j/k
//   **无任何对应测试** ⇒ ★ 只是文档级纪律 ⇒ ★ 本节补上机检。
// ★ ★ 三个 case 的断言对象都是「**纪律被违反时的可观测行为**」
//   （★ 而非「实现正确时的行为」）⇒ ★ ⇒ ★ 它们**恒绿**
//   ⇒ ★ ⇒ ★ **它们的作用是：★ 撤掉对应纪律 ⇒ ★ 本 case MUST RED**
//   ⇒ ★ ★ 这就是 ADR-091 §D6 V-9「变异对照」纪律的**可执行形式**。
namespace {

/// ★ 测试用 plugin：★ `setup()` 注册一个 stage 闭包，★ 闭包内**创建 ch 节点**
///   （写 payload）⇒ ★ 两条假绿通道的**共同触发点**：
///   · 若 `elaborate()` 不自建 `ctx_swap` ⇒ ★ 闭包内建节点**静默为 null**（G-1h）
///   · 若 `build()` 未被调用 ⇒ ★ 闭包**从未注册** ⇒ ★ 逻辑静默不执行（G-1j）
class StageWritingPlugin : public cf::plugin::PluginBase {
 public:
  void setup(cf::plugin::PipeBuilder& pb) override {
    pb.at_stage("g1_stage", cf::plugin::Phase::NORMAL, [this, &pb] {
      auto* n = pb.node_of_logic_stage("g1_stage").get();
      if (!n) return;
      n->operator()(payload_) =
          cf::plugin::uint_t<32>(ch::core::ch_literal<7, 32>{});
    });
  }

  void build(cf::plugin::PipeBuilder&) override {}

  cf::plugin::Payload<cf::plugin::uint_t<32>> payload_{"g1.test.value"};
};

}  // namespace

TEST_CASE("ports: G-1h — elaborate() creates stage nodes without an explicit ctx_swap",
          "[ports][chmem]") {
  // ★ G-1h：★ `elaborate()` **MUST 自建 `ctx_swap`** ⇒ ★ 否则 stage 闭包内
  //   创建的 ch 节点**静默失败**（值 0 / null impl）⇒ ★ 假绿通道。
  // ★ 本 case **故意不**手工 `ctx_swap` ⇒ ★ 只依赖框架的自动 swap。
  ch::core::context ctx("g1h_ctx");
  cf::plugin::PipeBuilder pb(&ctx);
  auto plugin = std::make_unique<StageWritingPlugin>();
  auto* plugin_ptr = plugin.get();
  pb.register_plugin(std::move(plugin));

  REQUIRE(pb.elaborate().has_value());

  // ★ 断言：闭包内的 payload 节点**确实被创建**（非 null impl）
  auto* n = pb.node_of_logic_stage("g1_stage").get();
  REQUIRE(n != nullptr);
  REQUIRE(n->payloads().has(plugin_ptr->payload_));
  REQUIRE(n->operator()(plugin_ptr->payload_).impl() != nullptr);
}

TEST_CASE("ports: G-1i — a ch literal created without an active context yields a null impl",
          "[ports][chmem]") {
  // ★ G-1i：★ plugin 的**成员初始化器**里创建 `ch_*` 字面量 ⇒ ★ 构造期
  //   **通常无 active context** ⇒ ★ 静默失败（impl == nullptr）
  //   ⇒ ★ ⇒ ★ 这就是「MUST 惰性创建」的原因。
  // ★ 本 case 钉住该**机制**：★ 无 ctx ⇒ null impl；有 ctx ⇒ 非 null。
  ch::core::ch_uint<32> v_no_ctx(ch::core::ch_literal<0, 32>{});
  REQUIRE(v_no_ctx.impl() == nullptr);   // ★ 静默失败（★ 无报错）

  ch::core::context ctx("g1i_ctx");
  {
    ch::core::ctx_swap guard(&ctx);
    ch::core::ch_uint<32> v_with_ctx(ch::core::ch_literal<0, 32>{});
    REQUIRE(v_with_ctx.impl() != nullptr);
  }
}

TEST_CASE("ports: G-1j — elaborate() auto-builds so stage closures are registered",
          "[ports][chmem]") {
  // ★ G-1j：★ `build()` MUST 在 `elaborate()` 之前 ⇒ ★ 否则 `setup()` 从未
  //   运行 ⇒ ★ `at_stage` 闭包**从未注册** ⇒ ★ **全部逻辑静默不执行**
  //   （★ ADR-091 自称「最严重的假绿通道」）。
  // ★ 框架已做**自动 build**加固 ⇒ ★ 本 case **故意不**调 `pb.build()`。
  ch::core::context ctx("g1j_ctx");
  cf::plugin::PipeBuilder pb(&ctx);
  pb.register_plugin(std::make_unique<StageWritingPlugin>());

  REQUIRE(pb.elaborate().has_value());

  // ★ 断言：stage 闭包**已被注册**（⇒ 逻辑会执行）
  const auto names = pb.stage_names();
  REQUIRE(names.size() == 1u);
  REQUIRE(names[0] == "g1_stage");
}

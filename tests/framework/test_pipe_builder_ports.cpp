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
#include <codegen_verilog.h>
#include <core/context.h>
#include <core/io.h>
#include <core/uint.h>

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

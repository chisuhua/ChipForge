// tests/cpu/test_cpu_open_core_ibus.cpp
//
// 功能描述: ADR-091 §D8 S3 —— CH_MEM 开放核的**取指端口化**验证
//           （无 ELF 预载 ⇒ 指令只能从对外端口进来）
//
// ★ 本文件是 S3 的契约测试。
//
// ---------------------------------------------------------------------------
// 验证 4 件事：
//
//   ① **端口进网表顶层**：★ 开放核的顶层端口**不止** clock/reset
//      （★ 对照：预载核只有 2 端口 ⇒ 这是「封闭核 → 开放核」的可判定差异）
//
//   ② **无预载也能取到指令**：★ 逐拍驱动 `imem_rsp_data` / `imem_rsp_valid` /
//      `imem_req_ready` ⇒ ★ 核必须**发出取指请求**并**消费响应**
//      （★ **不** 依赖任何内部 `ch_mem` / ELF 预载）
//
//   ③ **请求地址正确**：★ 核发出的 `imem_req_addr` MUST 逐拍等于 PC 序列
//      （⇒ 证明取指地址源是 PC，不是别的东西）
//
//   ④ **PC 递增**：★ PC MUST 每拍 +4（★ 本版本只做顺序取指，分支重定向待后续）
//
// ★ 时序等价性说明（ADR-091 §D8 S3）：
//   预载核 = `imem.sread()`（1 拍）· 端口核 = 请求当拍 → 响应下拍（也是 1 拍）
//   ⇒ ★ 天然对齐 ⇒ H-3 等价性自检（5 观测量逐位相等）有成立前提。
//
// ---------------------------------------------------------------------------
// 归属: ADR-091（CH_MEM 开放核形态与端口架构）§D8 S3 / §D6 H-2
// 关联: ADR-089（双后端差分 parity 门禁）· ADR-092（TLM 冻结）

#include "catch_amalgamated.hpp"

// ★ 只 include 具体头（MUST NOT <ch.hpp> —— module.h 缺 inline ⇒ 多 TU 链接失败）
#include <codegen_verilog.h>
#include <core/bool.h>
#include <core/context.h>
#include <core/reg.h>
#include <core/traits.h>
#include <core/uint.h>

#include <fstream>
#include <string>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "tests/cpu/open_core_verilog_helper_chmem.h"
#include "ip/cpu/core/ibus_port_chmem.h"

namespace {

// ★ 在生成的网表里查顶层端口声明
// ★★★ **精确名匹配**（★ ADR-091 v1.11 V-15）⇒ ★ 共享 helper
//   （★ 原本本文件与 dmem 测试各有一份**子串匹配**实现 ⇒ ★ 掩盖端口改名
//     ⇒ ★ 假绿；★ 现抽到 `open_core_verilog_helper_chmem.h` ⇒ 逻辑只有一份）
using cf_test::open_core::top_port_declared;

// 读一个顶层 output 端口的当前值
std::uint32_t read_out(const ch::Simulator& sim,
                        const ch::core::ch_out<ch::core::ch_uint<32>>& port) {
  // ★ get_port_value 返回 sdata_type ⇒ 需显式转 uint64
  return static_cast<std::uint32_t>(static_cast<std::uint64_t>(sim.get_port_value(port)));
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
TEST_CASE("open_core: ibus ports reach netlist top level (ADR-091 S3-1)",
          "[open-core][chmem]") {
  ch::core::context ctx("open_core_ports");
  cf::plugin::PipeBuilder pb(&ctx);

  // ★ 端口由装配层统一声明（顶层接口，MUST NOT 由 plugin 各自散着声明）
  auto req_addr  = must(pb.declare_out<ch::core::ch_uint<32>>("imem_req_addr"));
  auto req_valid = must(pb.declare_out<ch::core::ch_uint<1>>("imem_req_valid"));
  auto req_ready = must(pb.declare_in<ch::core::ch_uint<1>>("imem_req_ready"));
  auto rsp_data  = must(pb.declare_in<ch::core::ch_uint<32>>("imem_rsp_data"));
  auto rsp_valid = must(pb.declare_in<ch::core::ch_uint<1>>("imem_rsp_valid"));
  auto pc_port   = must(pb.declare_out<ch::core::ch_uint<32>>("open_pc"));

  // ★ PluginBase 的拷贝/移动构造均被 delete ⇒ plugin MUST 用
  //   「make_unique + 构造后注入」的标准用法（与 cpu_factory_chmem.h 同款）
  auto ibus = std::make_unique<cf::cpu::core::IBusPortPlugin<ch::core::ch_uint<32>>>();
  ibus->bind_ports({&req_addr, &req_valid, &req_ready, &rsp_data, &rsp_valid, &pc_port});
  ibus->set_initial_pc(0x80000000u);
  pb.register_plugin(std::move(ibus));

  {
    ch::core::ctx_swap guard(&ctx);
    REQUIRE(pb.build().has_value());
    REQUIRE(pb.elaborate().has_value());
  }

  const std::string vpath = "/tmp/opencode/open_core_ports_top.v";
  ch::toVerilog(vpath, &ctx);

  // ★ ① 五个取指端口 + PC 观测端口 MUST 都在顶层
  REQUIRE(top_port_declared(vpath, "imem_req_addr"));
  REQUIRE(top_port_declared(vpath, "imem_req_valid"));
  REQUIRE(top_port_declared(vpath, "imem_req_ready"));
  REQUIRE(top_port_declared(vpath, "imem_rsp_data"));
  REQUIRE(top_port_declared(vpath, "imem_rsp_valid"));
  REQUIRE(top_port_declared(vpath, "open_pc"));
}

TEST_CASE("open_core: ibus fetches over ports without any ELF preload "
          "(ADR-091 S3-2/3/4)",
          "[open-core][chmem]") {
  ch::core::context ctx("open_core_fetch");
  cf::plugin::PipeBuilder pb(&ctx);

  auto req_addr  = must(pb.declare_out<ch::core::ch_uint<32>>("imem_req_addr"));
  auto req_valid = must(pb.declare_out<ch::core::ch_uint<1>>("imem_req_valid"));
  auto req_ready = must(pb.declare_in<ch::core::ch_uint<1>>("imem_req_ready"));
  auto rsp_data  = must(pb.declare_in<ch::core::ch_uint<32>>("imem_rsp_data"));
  auto rsp_valid = must(pb.declare_in<ch::core::ch_uint<1>>("imem_rsp_valid"));
  auto pc_port   = must(pb.declare_out<ch::core::ch_uint<32>>("open_pc"));

  // ★ PluginBase 的拷贝/移动构造均被 delete ⇒ plugin MUST 用
  //   「make_unique + 构造后注入」的标准用法（与 cpu_factory_chmem.h 同款）
  auto ibus = std::make_unique<cf::cpu::core::IBusPortPlugin<ch::core::ch_uint<32>>>();
  ibus->bind_ports({&req_addr, &req_valid, &req_ready, &rsp_data, &rsp_valid, &pc_port});
  ibus->set_initial_pc(0x80000000u);
  pb.register_plugin(std::move(ibus));

  // ★★ `PipeBuilder::elaborate(ctx&)` **内部不做 `ctx_swap`** ⇒ 调用方 MUST 在
  //   `ctx_swap` 保护下调用（★ 既有 runner 亦然：parity_sim_runner.cpp:328 /
  //   cpu_verilator_sim.cpp:272）⇒ 否则 stage 闭包内创建的 `ch_*` 节点
  //   **静默失败**（值 0 / null impl）⇒ ★ 假绿通道（ADR-091 §D5.1 G-1h）
  std::unique_ptr<ch::Simulator> sim_holder;
  {
    ch::core::ctx_swap guard(&ctx);
    // ★★ `build()` MUST 在 `elaborate()` 之前 —— `build()` 跑 plugin 的
    //   `setup()`/`build()`（★ 注册 `at_stage` 闭包），`elaborate()` 才执行它们。
    //   ★ 漏调 `build()` ⇒ stage 从未注册 ⇒ **全部逻辑静默不执行**（假绿通道）。
    //   ★ 既有 runner 靠 `CpuFactoryChmem::build_cpu()` 内部调 `build()`。
    REQUIRE(pb.build().has_value());
    REQUIRE(pb.elaborate().has_value());
    auto sim_res = pb.create_simulator();
    REQUIRE(sim_res.has_value());
    sim_holder = std::move(sim_res.value());
  }
  auto& sim = *sim_holder;

  // ★ 从设备侧恒 ready（★ 本版本只实现「ready 恒真」的无阻塞流）
  sim.set_input_value(req_ready, 1);

  std::vector<std::uint32_t> req_addr_seen;
  std::vector<std::uint32_t> pc_seen;

  // ★ 逐拍：喂一条假指令（★ 无任何真实 ELF ⇒ 证明指令只能来自端口）
  const std::uint32_t kFakeInsn = 0x00000013u;  // nop

  for (int i = 0; i < 5; ++i) {
    sim.set_input_value(rsp_data, kFakeInsn);
    sim.set_input_value(rsp_valid, 1);
    sim.tick();
    req_addr_seen.push_back(static_cast<std::uint32_t>(static_cast<std::uint64_t>(sim.get_port_value(req_addr))));
    pc_seen.push_back(read_out(sim, pc_port));
  }

  // ★ ③ 取指地址 MUST 逐拍 = PC 序列
  //   ★ **tick 后**组合读看到的是**已更新**的 pc（★ Simulator 的 eval 顺序：
  //   sequential 先应用 next ⇒ 组合读见新值）⇒ ★ 第 i 拍读到 0x80000000+(i+1)*4
  //   ⇒ ★ 与预载核**同款**（预载核的 `imem.sread(word_addr)` 地址也是组合读 pc）
  for (std::size_t i = 0; i < req_addr_seen.size(); ++i) {
    REQUIRE(req_addr_seen[i] == 0x80000000u + static_cast<std::uint32_t>(i + 1) * 4u);
  }

  // ★ ④ PC MUST 每拍 +4（同款 tick 后语义）
  for (std::size_t i = 0; i < pc_seen.size(); ++i) {
    REQUIRE(pc_seen[i] == 0x80000000u + static_cast<std::uint32_t>(i + 1) * 4u);
  }

  // ★ ② 请求 valid MUST 恒真（无阻塞流）
  for (int i = 0; i < 3; ++i) {
    sim.tick();
    REQUIRE(static_cast<std::uint32_t>(static_cast<std::uint64_t>(sim.get_port_value(req_valid))) == 1u);
  }
}

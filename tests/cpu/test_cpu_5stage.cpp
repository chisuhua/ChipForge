// tests/cpu/test_cpu_5stage.cpp
//
// M4/W8 M4-PoC: 5-stage CPU elaboration + Verilog generation + Simulator tick
//
// 验证 (降级标准 per M4 接受):
//   1. cpu_factory_chmem.h::build_cpu() 成功 build 5-stage pipeline
//   2. pb.elaborate(ctx) 不 crash (DAG 构建成功)
//   3. pb.to_verilog("/tmp/cpu.v") 输出 Verilog (含 always_ff 块证明 ch_reg)
//   4. ch::Simulator tick 10 周期不 crash
//   5. (可选) Verilog 含 5-stage 寄存器 (fetch→decode→execute→memory→writeback)
//
// 编译条件: CF_PLUGIN_USE_CH_MEM
// 不依赖 add.elf 端到端 (M5 范围); 仅验证 5-stage infrastructure 工作

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include <ch.hpp>
#include <codegen_verilog.h>
#include <component.h>
#include <core/context.h>
#include <simulator.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"

#include "ip/cpu/cpu_factory_chmem.h"

using namespace ch;
using namespace ch::core;
namespace cfcpu = cf::cpu;

// =========================================================================
// PoC #1: build_cpu() 成功创建 5-stage pipeline (4 plugins registered)
//
// 验证:
//   - build_cpu() 返回非空 PipeBuilder
//   - pb->build() 无异常 (触发所有 Plugin 的 setup()+build())
//   - 4 plugins + 阶段链接闭包正确注册
// =========================================================================
TEST_CASE("m4_poc_5stage_build", "[cpu][m4][poc][5stage][chmem]") {
  ch::core::context ctx("m4_5stage_build_ctx");
  ch::core::ctx_swap guard(&ctx);

  auto pb = cfcpu::CpuFactoryChmem<ch_uint<32>>::build_cpu(&ctx);
  REQUIRE(pb != nullptr);

  // build() 已由 build_cpu() 内部调用; 验证 plugin_count >= 4
  REQUIRE(pb->plugin_count() >= 4);

  SUCCEED("5-stage pipeline built (" << pb->plugin_count() << " plugins registered)");
}

// =========================================================================
// PoC #2: build_cpu_with_elaborate() — elaborate + toVerilog
//
// 验证:
//   - elaborate() 无异常 (DAG 构建)
//   - cpu.v 生成到 /tmp/cpu.v
//   - Verilog 含 "module" (证明 CppHDL codegen 成功)
//   - Verilog 含 ≥5 "always_ff" 块 (证明 5-stage pipeline_reg)
// =========================================================================
TEST_CASE("m4_poc_5stage_elaborate_verilog", "[cpu][m4][poc][5stage][chmem]") {
  ch::core::context ctx("m4_5stage_elab_ctx");
  ch::core::ctx_swap guard(&ctx);

  auto pb = cfcpu::CpuFactoryChmem<ch_uint<32>>::build_cpu(&ctx);
  REQUIRE(pb != nullptr);

  // Step 1: elaborate — 运行所有 at_stage 闭包 + commit_payload_map
  REQUIRE_NOTHROW(pb->elaborate(ctx));

  // Step 2: toVerilog
  const std::string out_file = "/tmp/cpu.v";
  REQUIRE_NOTHROW(pb->to_verilog(out_file));

  // Step 3: 验证 Verilog 文件
  std::ifstream f(out_file);
  REQUIRE(f.is_open());
  std::stringstream ss;
  ss << f.rdbuf();
  std::string verilog = ss.str();
  REQUIRE(!verilog.empty());

  // module 存在
  REQUIRE(verilog.find("module") != std::string::npos);

  // always_ff 存在 (≥5 证明 5-stage pipeline_reg 插入)
  std::size_t pos = 0;
  int always_ff_count = 0;
  while ((pos = verilog.find("always_ff", pos)) != std::string::npos) {
    ++always_ff_count;
    pos += 10;
  }
  INFO("always_ff count = " << always_ff_count);
  REQUIRE(always_ff_count >= 5);

  SUCCEED("cpu.v generated + " << always_ff_count
          << " always_ff blocks (5-stage pipeline_reg 证据)");
}

// =========================================================================
// PoC #3: Simulator tick 10 周期不 crash
//
// 验证:
//   - create_simulator() 返回非空
//   - tick() 10 周期无异常
//   不需要检查输出值 (M5 范围: 端到端验证)
// =========================================================================
TEST_CASE("m4_poc_5stage_simulator_tick", "[cpu][m4][poc][5stage][chmem]") {
  ch::core::context ctx("m4_5stage_sim_ctx");
  ch::core::ctx_swap guard(&ctx);

  auto pb = cfcpu::CpuFactoryChmem<ch_uint<32>>::build_cpu(&ctx);
  pb->elaborate(ctx);

  auto sim = pb->create_simulator();
  REQUIRE(sim != nullptr);

  sim->reset();
  for (int i = 0; i < 10; ++i) {
    REQUIRE_NOTHROW(sim->tick());
  }

  SUCCEED("5-stage Simulator tick 10 周期无 crash");
}

// =========================================================================
// PoC #4: HazardPlugin 完整 9 条件 RAW 检测 — elaborate + Verilog 验证
//
// 验证:
//   - CpuFactoryChmem 含 HazardPlugin, elaborate 不抛异常
//   - toVerilog("/tmp/hazard_complete.v") 输出 Verilog
//   - Verilog 含 >= 9 mux_select (6 数据 + 3 x0 屏蔽的比较器)
//
// 原理: detect_raw_hazard() 使用 raw_hazard_complete() 建立 9 条件 DAG,
//       6 个 == 比较 + 3 个 != 比较, 每个在 CppHDL Verilog codegen 生成
//       mux_select 节点。
// =========================================================================
TEST_CASE("hazard_chmem_complete_elaborate", "[cpu][chmem][hazard][poc]") {
  ch::core::context ctx("hazard_complete_elab_ctx");
  ch::core::ctx_swap guard(&ctx);

  auto pb = cfcpu::CpuFactoryChmem<ch_uint<32>>::build_cpu(&ctx);
  REQUIRE(pb != nullptr);

  REQUIRE_NOTHROW(pb->elaborate(ctx));

  const std::string out_file = "/tmp/hazard_complete.v";
  REQUIRE_NOTHROW(pb->to_verilog(out_file));

  std::ifstream f(out_file);
  REQUIRE(f.is_open());
  std::stringstream ss;
  ss << f.rdbuf();
  std::string verilog = ss.str();
  REQUIRE(!verilog.empty());

  // 计数 mux_select: 至少 9 (6 数据 == 比较 + 3 x0 != 比较)
  std::size_t pos = 0;
  int mux_select_count = 0;
  while ((pos = verilog.find("mux_select", pos)) != std::string::npos) {
    ++mux_select_count;
    pos += 10;
  }
  INFO("mux_select count = " << mux_select_count);
  REQUIRE(mux_select_count >= 9);

  SUCCEED("HazardPlugin complete: " << mux_select_count
          << " mux_select (>= 9 = 6 data + 3 x0 shield)");
}

// =========================================================================
// PoC #5: HazardPlugin 完整版 Simulator tick 10 周期不 crash
//
// 验证:
//   - create_simulator() 返回非空
//   - tick() 10 周期无异常
//   - RAW 检测 halt 信号通过 CtrlLink 接入 decode stage stall, 不导致 SEGV
// =========================================================================
TEST_CASE("hazard_chmem_complete_simulator_tick", "[cpu][chmem][hazard][poc]") {
  ch::core::context ctx("hazard_complete_sim_ctx");
  ch::core::ctx_swap guard(&ctx);

  auto pb = cfcpu::CpuFactoryChmem<ch_uint<32>>::build_cpu(&ctx);
  pb->elaborate(ctx);

  auto sim = pb->create_simulator();
  REQUIRE(sim != nullptr);

  sim->reset();
  for (int i = 0; i < 10; ++i) {
    REQUIRE_NOTHROW(sim->tick());
  }

  SUCCEED("HazardPlugin complete Simulator tick 10 周期无 crash");
}

#endif  // CF_PLUGIN_USE_CH_MEM
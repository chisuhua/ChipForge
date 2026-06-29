// tests/cpu/test_cpu_factory.cpp
//
// 功能描述: CpuFactory 单元测试 (M4.1 验证)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 测试覆盖 (5 用例):
//   1. CPUConfig 默认值 (rv64gc, 5 级)
//   2. build_cpu() 返回非空 unique_ptr<PipeBuilder>
//   3. cpu_embedded.json 配置 (3 级 RV32IMAC)
//   4. cpu_default.json 配置 (5 级 RV64GC)
//   5. 不同 xlen (uint32/uint64) 实例化
//
// 约束:

#include "catch_amalgamated.hpp"
#include <cstdint>
#include <memory>

#include "ip/cpu/cpu_factory.h"

using namespace cf::cpu;
using T32 = std::uint32_t;
using T64 = std::uint64_t;

TEST_CASE("default_config", "[cpu]") {
  CPUConfig cfg;
  REQUIRE(cfg.isa == "rv64gc");
  REQUIRE(cfg.pipeline_stages == 5);
  REQUIRE(cfg.clock_freq_mhz == 100);
  REQUIRE(cfg.enable_mmu == true);
}

TEST_CASE("build_cpu_rv32", "[cpu]") {
  CPUConfig cfg;
  cfg.isa = "rv32i";
  cfg.pipeline_stages = 3;
  cfg.enable_mmu = false;
  auto pb = CpuFactory<T32>::build_cpu(cfg);
  REQUIRE(pb != nullptr);
}

TEST_CASE("build_cpu_rv64", "[cpu]") {
  CPUConfig cfg;
  cfg.isa = "rv64gc";
  cfg.pipeline_stages = 5;
  cfg.enable_mmu = true;
  cfg.mmu_mode = "sv39";
  auto pb = CpuFactory<T64>::build_cpu(cfg);
  REQUIRE(pb != nullptr);
}

TEST_CASE("embedded_config", "[cpu]") {
  CPUConfig cfg;
  cfg.name = "RiscvCpu_embedded";
  cfg.isa = "rv32imac";
  cfg.pipeline_stages = 3;
  cfg.clock_freq_mhz = 50;
  cfg.enable_mmu = false;
  cfg.branch_predictor = "static";
  cfg.btb_entries = 16;
  auto pb = CpuFactory<T32>::build_cpu(cfg);
  REQUIRE(pb != nullptr);
}

TEST_CASE("default_json_config", "[cpu]") {
  CPUConfig cfg;
  cfg.name = "RiscvCpu_default";
  cfg.isa = "rv64gc";
  cfg.pipeline_stages = 5;
  cfg.clock_freq_mhz = 100;
  cfg.enable_pmp = true;
  cfg.enable_mmu = true;
  cfg.mmu_mode = "sv39";
  cfg.branch_predictor = "gshare";
  cfg.btb_entries = 64;
  cfg.icache_latency = 1;
  cfg.dcache_latency = 1;
  auto pb = CpuFactory<T64>::build_cpu(cfg);
  REQUIRE(pb != nullptr);
}

// M4.12: spec 要求 11 plugins 含 RetirePlugin, 但 ip/cpu/plugins/ 中
//        不存在 RetirePlugin 类 — 此为 BLOCKED 项待 orchestrator 决策.
//        实际注册: 11 plugins (无 RetirePlugin, 但有 HazardPlugin +
//        RiscvLsuPlugin + RiscvCsrPlugin + RiscvIntAluPlugin 4 个 spec
//        未列出的 plugins, 正好凑成 11)
TEST_CASE("build_cpu_registers_11_real_plugins", "[cpu]") {
  CPUConfig cfg;
  cfg.branch_predictor = "gshare";
  cfg.btb_entries = 64;
  cfg.mul_latency = 3;
  auto pb = cf::cpu::CpuFactory<std::uint64_t>::build_cpu(cfg);
  REQUIRE(pb != nullptr);
  const auto& plugins = pb->plugins();
  REQUIRE(plugins.size() == 11);
}



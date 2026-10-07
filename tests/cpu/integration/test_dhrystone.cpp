// tests/cpu/integration/test_dhrystone.cpp
//
// Dhrystone DMIPS/MHz 集成测试 (mfc-cpu-pipeline-multi-cycle-fsm Phase G)
// 跑 tests/cpu/manual_elf/dhrystone/dhrystone.elf (Number_Of_Runs=2000)
// 计算 DMIPS/MHz = 1757 * N / cycle_count (VAX 11/780 reference).
// mfc hard gate: DMIPS/MHz >= 1.4 → cycle_count <= 2,510,000.
//
// Family tag: [cpu-integration][dhrystone]
// SPDX-License-Identifier: BSD-3-Clause

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <fstream>

#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/cpu_factory.h"
#include "ip/cpu/picolibc_host_memory.h"
#include "tools/cpu_sim/elf_loader.h"

namespace {

constexpr std::uint64_t kMaxCycles = 100000000;  // 100M cycle cap (mfc Phase G C': 验证 livelock vs performance vs 10M cap)

constexpr int kDhrystoneRuns = 2000;
constexpr std::uint64_t kMaxCyclesForDmips14 =
    (1757ULL * kDhrystoneRuns) / 14 * 10;  // = 2,510,000 (= 1757*2000*10/14)

void runDhrystone(cf::cpu::CPUConfig::MulImpl mul_impl,
                 std::uint64_t& cycles_out, bool& exited_out, int& ec_out) {
  std::string elf_path =
      "tests/cpu/manual_elf/dhrystone/dhrystone.elf";
  if (!std::ifstream(elf_path).good()) {
    FAIL("dhrystone.elf not found: " + elf_path);
  }
  auto elf = cf::tools::load_elf_full(elf_path);
  // manual_elf/dhrystone/ use direct sw to address 0 (no .tohost section)
  std::uint64_t tohost_addr = (elf.tohost_addr != UINT64_MAX)
                                 ? elf.tohost_addr
                                 : 0;
  std::uint64_t window_base = elf.entry_addr &
                              ~static_cast<std::uint64_t>(0xFFFF);
  if (window_base == 0 && elf.entry_addr != 0) window_base = elf.entry_addr;
  cf::cpu::PicolibcHostMemory::Config mem_cfg{
      .base_addr = window_base, .size = 64 * 1024,
      .tohost_addr = tohost_addr};
  cf::cpu::PicolibcHostMemory mem(mem_cfg);
  for (const auto& sec : elf.sections) {
    mem.load_section(sec.first, sec.second);
  }
  cf::cpu::CPUConfig cfg;
  cfg.isa = "rv32im";
  cfg.enable_mmu = false;
  cfg.mul_impl = mul_impl;
  auto pb = cf::cpu::CpuFactory<std::uint32_t>::build_cpu(cfg, &mem);
  using KeyType = cf::cpu::core::payload::keys<std::uint32_t, 32>;
  auto fn = pb->node_of_logic_stage("fetch");
  if (fn) fn->operator()(KeyType::PC) = static_cast<std::uint32_t>(elf.entry_addr);
  cycles_out = 0;
  exited_out = false;
  ec_out = -1;
  for (std::uint64_t i = 0; i < kMaxCycles; ++i) {
    pb->run();
    ++cycles_out;
    if (mem.exited()) {
      exited_out = true;
      ec_out = mem.exit_code();
      break;
    }
  }
}

}  // namespace

// LEGACY mode baseline: Phase A.7 实测 mul.elf LEGACY 5 cycles tohost=1 PASS.
// Dhrystone LEGACY 应 tohost=1 (RiscvMulPlugin<U,1> 单周期).
TEST_CASE("dhrystone_legacy_tohost_pass", "[cpu-integration][dhrystone]") {
  std::uint64_t cycles;
  bool exited;
  int ec;
  runDhrystone(cf::cpu::CPUConfig::MulImpl::LEGACY, cycles, exited, ec);
  INFO("cycles=" << cycles << " ec=" << ec);
  REQUIRE(exited);
  REQUIRE(ec == 0);
}

// FSM mode DMIPS/MHz hard gate (mfc tasks G.2 v0.10.0):
// DMIPS/MHz >= 1.4 (VAX 11/780 reference, Dhrystone cycles per iter <= 1255 @ 1MHz)
// = 1757 * N / cycles_per_iter >= 1.4
// = cycles_per_iter <= 1757 / 1.4 = 1255 cycles per iter
// = total cycles >= 1757 * 2000 / 1.4 = 2,510,000
TEST_CASE("dhrystone_fsm_dmips_mhz_minimum_1_4", "[cpu-integration][dhrystone]") {
  std::uint64_t cycles;
  bool exited;
  int ec;
  runDhrystone(cf::cpu::CPUConfig::MulImpl::FSM, cycles, exited, ec);
  INFO("cycles=" << cycles << " ec=" << ec);
  REQUIRE(ec == 0);
  REQUIRE(cycles <= kMaxCyclesForDmips14);
}

// 性能指标输出 (informational, 不阻塞).
TEST_CASE("dhrystone_fsm_dmips_mhz_metric", "[cpu-integration][dhrystone][performance]") {
  std::uint64_t cycles;
  bool exited;
  int ec;
  runDhrystone(cf::cpu::CPUConfig::MulImpl::FSM, cycles, exited, ec);
  // DMIPS/MHz = 1757 * N / cycles (VAX 11/780 reference, 1 MHz = 1e6 cycles/sec)
  // 简化公式: VAX 1 Dhrystone iteration = 1757 cycles @ 1MHz
  // DMIPS/MHz = cycles_per_iter / VAX_ref = 1757 * N / cycles
  const double dmips_per_mhz =
      (1757.0 * kDhrystoneRuns) / static_cast<double>(cycles);
  INFO("cycles=" << cycles << " dmips_per_mhz=" << dmips_per_mhz
       << " (mfc hard gate: >= 1.4)");
  REQUIRE(exited);
  REQUIRE(ec == 0);
  // performance gate 留 informational 不 REQUIRE
}
// tests/cpu/integration/test_dhrystone.cpp
//
// Dhrystone DMIPS/MHz 集成测试 (feat-dhrystone-dmips-gate)
// 跑 tests/cpu/manual_elf/dhrystone/dhrystone.elf (Number_Of_Runs=2000)
// 计算 DMIPS/MHz = N*1e6/(1757*cycles) (VAX 11/780 reference).
// mfc hard gate: DMIPS/MHz >= 1.4 → cycle_count <= 813,043.
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

/* feat-dhrystone-dmips-gate AC2: 降 cap 5M (从 100M).
 *   旧 100M cap 永远 timeout → wall-clock 超 ctest timeout → SIGTERM.
 *   5M cap 是真 gate 813,043 × 6 余量, 顺带治愈 AGENTS.md §known SIGTERM.
 */
constexpr std::uint64_t kMaxCycles = 5000000;  // 5M cycle cap

constexpr int kDhrystoneRuns = 2000;

/* feat-dhrystone-dmips-gate B2: 真 gate 813,043 cycles (= N*1e6/(1757*1.4)).
 *   旧公式 1757*N/14*10 = 2,510,000 是错误 (3.087× 宽, 因 1757²/1e6).
 *   公式修为 (N*1e6)/(1757*14)*10 = 813,043 (标准 DMIPS 公式).
 */
constexpr std::uint64_t kMaxCyclesForDmips14 =
    (kDhrystoneRuns * 1'000'000ULL) / (1757 * 14) * 10;  // = 813,043

/* feat-dhrystone-dmips-gate B1 part c: hardcode base=0x80000000.
 *   旧 window_base = entry & ~0xFFFF; if(window_base==0 && entry!=0) window_base=entry
 *   让 .text@0x0 (旧 link.ld base=0) 整段被 load_section 静默 drop (Oracle H2 升级).
 *   修法: 与 link_dhrystone.ld 锁 base=0x80000000 一致.
 */
constexpr std::uint64_t kDhrystoneBase = 0x80000000;

void runDhrystone(cf::cpu::CPUConfig::MulImpl mul_impl,
                 std::uint64_t& cycles_out, bool& exited_out, int& ec_out) {
  std::string elf_path =
      "tests/cpu/manual_elf/dhrystone/dhrystone.elf";
  if (!std::ifstream(elf_path).good()) {
    FAIL("dhrystone.elf not found: " + elf_path);
  }
  auto elf = cf::tools::load_elf_full(elf_path);
  /* feat-dhrystone-dmips-gate B1 H1: link_dhrystone.ld 锚定 tohost symbol @ 0x80001000.
   *   旧 fallback tohost=0 (无 .tohost section) → picolibc_host_memory.h write_byte 静默 drop.
   *   修法: tohost_addr=base+0x1000 (与 link_dhrystone.ld ALIGN 锁).
   */
  const std::uint64_t tohost_addr = kDhrystoneBase + 0x1000;
  cf::cpu::PicolibcHostMemory::Config mem_cfg{
      .base_addr = kDhrystoneBase, .size = 64 * 1024,
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

// LEGACY mode baseline: RiscvMulPlugin<U,1> 单周期.
// feat-dhrystone-dmips-gate: Dhrystone LEGACY 应 tohost=1.
TEST_CASE("dhrystone_legacy_tohost_pass", "[cpu-integration][dhrystone]") {
  std::uint64_t cycles;
  bool exited;
  int ec;
  runDhrystone(cf::cpu::CPUConfig::MulImpl::LEGACY, cycles, exited, ec);
  /* feat-dhrystone-dmips-gate B2: 标准 DMIPS/MHz = N*1e6/(1757*cycles).
   *   旧 1757*N/cycles 错 3.087× 宽 (1757²/1e6).
   */
  const double dmips_per_mhz =
      (double)kDhrystoneRuns * 1e6 / (1757.0 * (double)cycles);
  INFO("cycles=" << cycles << " ec=" << ec
       << " dmips_per_mhz=" << dmips_per_mhz << " (mfc hard gate: >= 1.4)");
  REQUIRE(exited);
  REQUIRE(ec == 0);
}

// feat-dhrystone-dmips-gate B2: FSM mode DMIPS/MHz 1.4 硬门禁
//   标准公式: cycles <= N*1e6/(1757*1.4) = 813,043
//   真 gate (旧 2,510,000 是 3.087× 宽错误)
TEST_CASE("dhrystone_fsm_dmips_mhz_minimum_1_4", "[cpu-integration][dhrystone]") {
  std::uint64_t cycles;
  bool exited;
  int ec;
  runDhrystone(cf::cpu::CPUConfig::MulImpl::FSM, cycles, exited, ec);
  const double dmips_per_mhz =
      (double)kDhrystoneRuns * 1e6 / (1757.0 * (double)cycles);
  INFO("cycles=" << cycles << " ec=" << ec
       << " dmips_per_mhz=" << dmips_per_mhz << " (mfc hard gate: >= 1.4)");
  REQUIRE(exited);
  REQUIRE(ec == 0);
  REQUIRE(cycles <= kMaxCyclesForDmips14);
}

// 性能指标输出 (informational, 不阻塞).
TEST_CASE("dhrystone_fsm_dmips_mhz_metric", "[cpu-integration][dhrystone][performance]") {
  std::uint64_t cycles;
  bool exited;
  int ec;
  runDhrystone(cf::cpu::CPUConfig::MulImpl::FSM, cycles, exited, ec);
  // feat-dhrystone-dmips-gate B2: 标准 DMIPS/MHz 公式
  const double dmips_per_mhz =
      (double)kDhrystoneRuns * 1e6 / (1757.0 * (double)cycles);
  INFO("cycles=" << cycles << " dmips_per_mhz=" << dmips_per_mhz
       << " (mfc hard gate: >= 1.4)");
  REQUIRE(exited);
  REQUIRE(ec == 0);
  // performance gate 留 informational 不 REQUIRE
}

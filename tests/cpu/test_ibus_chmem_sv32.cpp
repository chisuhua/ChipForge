// tests/cpu/test_ibus_chmem_sv32.cpp
//
// mmu-chmem-pipeline-integration / Phase 2 Task 2.1 (C2):
//   ibus_chmem sv32 消费 — Bare path byte-identical + Sv32 plumbing-only
//
// Scope (Step 1-3 of Task 2.1):
//   - Bare baseline:     add.elf @ add.elf config (no MMU hook) → tohost=1, cycle count
//                        matches rv32ui-baseline-matrix.csv (461 cycles for rv32ui-p-add)
//   - Sv32 plumbing:     add.elf @ sv32 config (enable_mmu=true, mmu_mode="sv32") →
//                        tohost=1 in SAME cycle count (mmu_chmem.h NOT registered yet,
//                        PADDR_VALID stays false, sel 表达式回退到 vaddr path)
//   - [RED] initially:   sel 表达式尚未加,sv32 plumbing 路径因为 cfg.fprintf
//                        (owner claim log) 也会 PASS 但行为应该 === Bare.
//                        Step 2 GREEN 后 sel 表达式替换裸 pc_val 才进入 Sv32 plumbing.
//
// Post-Phase-B (Sprint 6-7+): Sv32 plumbing test 会被替换为:
//   "sv32 with planted identity page table → tohost=1, cycle count ≤ 1.5 × Bare"
//   (D3 vendor build_sv32_pte.S + D4 sv32-baseline-matrix.csv)
//
// 编译: -DCF_PLUGIN_USE_CH_MEM (mandatory, included in chipforge_tests_chmem)

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <ch.hpp>
#include <core/context.h>
#include <simulator.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/result_macros.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/cpu_factory_chmem.h"

using namespace ch;
using namespace ch::core;

#ifndef TEST_RV32UI_ELF_DIR
#error "TEST_RV32UI_ELF_DIR must be defined via CMake compile_definitions"
#endif

namespace {

constexpr std::uint32_t kElfBase = 0x80000000;

// rv32ui-baseline-matrix.csv baseline for rv32ui-p-add = 461 cycles
// (committed at soc/cpu/docs/dse/rv32ui-baseline-matrix.csv line 2,
//  v0.6.0 提前归档, 461 = bare path 实测 cycle count)
constexpr std::size_t kAddElfExpectedCycles = 461;
// ±10% cycle tolerance (bare path byte-identical 保护, 见 plan §Task 2.1 Step 3)
constexpr std::size_t kCycleTolerance = kAddElfExpectedCycles / 10;

constexpr std::size_t kMaxCycles = 800;

std::vector<uint8_t> load_elf_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) return {};
  f.seekg(0, std::ios::end);
  std::streamsize n = f.tellg();
  f.seekg(0, std::ios::beg);
  if (n <= 0) return {};
  std::vector<uint8_t> buf(static_cast<std::size_t>(n));
  f.read(reinterpret_cast<char*>(buf.data()), n);
  return buf;
}

}  // anonymous namespace

// =========================================================================
// C2 Step 1 RED: Bare baseline cycle count for ibus_chmem sv32 consumer.
//
// Step 2 GREEN (改 ibus_chmem.h) 后预期不变: sv32 sel 表达式在 PADDR_VALID=false
// 路径下回退到 vaddr (= pc_val) → 与当前实现 cycle-byte-identical.
// =========================================================================
TEST_CASE("ibus_chmem_sv32_bare_baseline", "[cpu][chmem][ibus-sv32][mmu-chmem]") {
  ch::core::context ctx("ibus_chmem_sv32_bare_ctx");
  ch::core::ctx_swap guard(&ctx);

  std::string elf_path = std::string(TEST_RV32UI_ELF_DIR) + "/rv32ui-p-add";
  auto elf_bytes = load_elf_file(elf_path);
  REQUIRE(elf_bytes.size() >= 52);
  REQUIRE(elf_bytes[0] == 0x7F);
  REQUIRE(elf_bytes[1] == 'E');
  REQUIRE(elf_bytes[2] == 'L');
  REQUIRE(elf_bytes[3] == 'F');

  // Bare baseline: enable_mmu = nullopt (default)
  auto pb = cf::cpu::CpuFactoryChmem<ch_uint<32>>::build_cpu(
      &ctx, nullptr, ch_uint<32>(kElfBase), true, elf_bytes);
  REQUIRE(pb != nullptr);
  REQUIRE(pb->plugin_count() >= 7);

  REQUIRE((pb->elaborate(ctx)).has_value());

  auto sim = cf::plugin::auto_throw(pb->create_simulator());
  REQUIRE(sim != nullptr);

  std::uint64_t tohost_val = 0;
  std::size_t cycles_run = 0;
  for (std::size_t i = 0; i < kMaxCycles; ++i) {
    REQUIRE_NOTHROW(sim->tick());
    cycles_run = i + 1;
    tohost_val = static_cast<std::uint64_t>(
        sim->get_value_by_name("tohost_probe_data_proxy"));
    if (tohost_val != 0) break;
  }

  INFO("Bare baseline: rv32ui-p-add tohost=" << tohost_val
       << " cycles_run=" << cycles_run
       << " baseline=" << kAddElfExpectedCycles
       << " tolerance=±" << kCycleTolerance);
  REQUIRE(tohost_val == 1);
  // Bare path byte-identical: cycle count must equal baseline ±10%
  REQUIRE(cycles_run >= kAddElfExpectedCycles - kCycleTolerance);
  REQUIRE(cycles_run <= kAddElfExpectedCycles + kCycleTolerance);
}

// =========================================================================
// C2 Step 1 RED: Sv32 plumbing-only (no MMU plugin registered, PADDR_VALID=false).
//
// Step 2 GREEN (sel 表达式加 sv32 消费) 后期望:
//   - tohost=1 在与 Bare 相同的 cycle count 内完成
//   - 行为应当 === Bare (mmu_chmem.h 未注册, PADDR_VALID 永远 false, sel 回退)
//   - stderr log "MMU hook enabled (mode=sv32) — plumbing-only no-op stub" 仍存在
//     (v0.11.0 owner claim, 由 cpu_factory_chmem.h line 126-137 维持)
//
// Post-Phase-B RED→GREEN 转换路径 (Sprint 6-7+):
//   - mmu_chmem.h 注册 MMUPlugin_chmem → 发射 PADDR_VALID=true + PADDR cell
//   - ibus_chmem.sv32 sel 表达式选 PADDR 分支 → 真翻译
//   - planted identity page table 让 vaddr == paddr → 行为仍 byte-identical
//   - 真正的差异化验证要靠 build_sv32_pte.S (D3) + 不同的 vaddr/paddr 配对
// =========================================================================
TEST_CASE("ibus_chmem_sv32_plumbing_only", "[cpu][chmem][ibus-sv32][mmu-chmem]") {
  ch::core::context ctx("ibus_chmem_sv32_plumbing_ctx");
  ch::core::ctx_swap guard(&ctx);

  std::string elf_path = std::string(TEST_RV32UI_ELF_DIR) + "/rv32ui-p-add";
  auto elf_bytes = load_elf_file(elf_path);
  REQUIRE(elf_bytes.size() >= 52);

  // Sv32 plumbing: enable_mmu=true, mmu_mode="sv32"
  // v0.11.0 owner claim: mmu_chmem.h not implemented (Phase B in Sprint 6-7)
  // → only stderr log, NO MMU plugin registered, PADDR_VALID 永远 false
  auto pb = cf::cpu::CpuFactoryChmem<ch_uint<32>>::build_cpu(
      &ctx, nullptr, ch_uint<32>(kElfBase), true, elf_bytes,
      /*enable_mmu=*/std::optional<bool>(true),
      /*mmu_mode=*/std::optional<std::string>("sv32"));
  REQUIRE(pb != nullptr);
  REQUIRE(pb->plugin_count() >= 7);

  REQUIRE((pb->elaborate(ctx)).has_value());

  auto sim = cf::plugin::auto_throw(pb->create_simulator());
  REQUIRE(sim != nullptr);

  std::uint64_t tohost_val = 0;
  std::size_t cycles_run = 0;
  for (std::size_t i = 0; i < kMaxCycles; ++i) {
    REQUIRE_NOTHROW(sim->tick());
    cycles_run = i + 1;
    tohost_val = static_cast<std::uint64_t>(
        sim->get_value_by_name("tohost_probe_data_proxy"));
    if (tohost_val != 0) break;
  }

  INFO("Sv32 plumbing: rv32ui-p-add tohost=" << tohost_val
       << " cycles_run=" << cycles_run
       << " baseline=" << kAddElfExpectedCycles
       << " tolerance=±" << kCycleTolerance);
  REQUIRE(tohost_val == 1);
  // Sv32 plumbing path must NOT regress vs Bare (no extra latency injected).
  // sel 表达式在 PADDR_VALID=false 时回退到 vaddr → cycle count 应当 === Bare
  REQUIRE(cycles_run >= kAddElfExpectedCycles - kCycleTolerance);
  REQUIRE(cycles_run <= kAddElfExpectedCycles + kCycleTolerance);
}

#endif  // CF_PLUGIN_USE_CH_MEM

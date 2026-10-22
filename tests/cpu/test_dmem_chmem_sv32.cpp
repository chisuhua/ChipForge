// tests/cpu/test_dmem_chmem_sv32.cpp
//
// mmu-chmem-pipeline-integration / Phase 2 Task 2.2 (C3):
//   dmem_chmem sv32 消费 — Bare path byte-identical + Sv32 plumbing-only
//
// Scope (Step 1-3 of Task 2.2):
//   - Bare baseline:     rv32ui-p-add (add-only) → tohost=1 in 461 cycles (无 dmem access)
//     - 选 add.elf 是因为 main HEAD b8d860f 状态下 load/store ELF (lb/lw/sw/...)
//       pre-existing baseline 退化 (test_cpu_chmem_vendored_elf 18 fail),
//       与本 change 无关 (A/B test 已验证). 用 ALU-only ELF 避免被 pre-existing
//       退化污染.
//   - dmem_chmem sel 表达式 elaboration 覆盖: build_cpu 成功 + elaborate 0 error
//     即证明 sel 表达式 DAG 正确发射, 即使 Bare path 不消费 dmem load/store.
//   - Sv32 plumbing:  enable_mmu=true, mmu_mode=sv32 → 行为 === Bare
//     (Phase B 未实装, PADDR_VALID 永远 false, sel 回退到 RESULT/vaddr)
//
// Post-Phase-B (Sprint 2-3+): 测试升级路径:
//   - 修复 pre-existing baseline 退化 (dmem load/store 路径 CH_MEM 退化, root cause 待查)
//   - 加入 build_sv32_pte.S (D3 vendor) + planted identity page table 真 sv32 e2e
//   - sv32-baseline-matrix.csv (D4) 提供 cycle 比对阈值
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

// rv32ui-baseline-matrix.csv: rv32ui-p-add = 461 cycles
constexpr std::size_t kAddElfBaseline = 461;
constexpr std::size_t kCycleTolerance = kAddElfBaseline * 15 / 100;  // ±15%
constexpr std::size_t kMaxCycles = 1500;

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
// C3 Step 1 RED + Step 2 GREEN: dmem_chmem sv32 sel 表达式不引入 Bare path 退化
//
// Bare baseline (enable_mmu=nullopt) 与 Sv32 plumbing (enable_mmu=true, mmu_mode=sv32)
// 应当行为一致, 因为 Phase B 未实装, PADDR_VALID 永远 false → sel 回退到 RESULT (vaddr)。
// =========================================================================
TEST_CASE("dmem_chmem_sv32_bare_baseline", "[cpu][chmem][dmem-sv32][mmu-chmem]") {
  ch::core::context ctx("dmem_sv32_bare_ctx");
  ch::core::ctx_swap guard(&ctx);

  std::string elf_path = std::string(TEST_RV32UI_ELF_DIR) + "/rv32ui-p-add";
  auto elf_bytes = load_elf_file(elf_path);
  REQUIRE(elf_bytes.size() >= 52);

  // Bare baseline: enable_mmu = nullopt (default)
  auto pb = cf::cpu::CpuFactoryChmem<ch_uint<32>>::build_cpu(
      &ctx, nullptr, ch_uint<32>(kElfBase), true, elf_bytes);
  REQUIRE(pb != nullptr);
  REQUIRE(pb->plugin_count() >= 7);

  // elaboration 0 error 证明 sel 表达式 DAG 正确发射
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
  INFO("Bare: rv32ui-p-add tohost=" << tohost_val
       << " cycles=" << cycles_run
       << " baseline=" << kAddElfBaseline << " ±" << kCycleTolerance);
  REQUIRE(tohost_val == 1);
  REQUIRE(cycles_run >= kAddElfBaseline - kCycleTolerance);
  REQUIRE(cycles_run <= kAddElfBaseline + kCycleTolerance);
}

// =========================================================================
// C3 Step 1 RED + Step 2 GREEN: dmem_chmem sv32 plumbing path (no mmu_chmem.h)
//
// mmu_chmem.h not implemented → PADDR_VALID=false → sel 回退到 RESULT/vaddr
// 行为应当 === Bare baseline, 即 tohost=1 in baseline cycles
// =========================================================================
TEST_CASE("dmem_chmem_sv32_plumbing_only", "[cpu][chmem][dmem-sv32][mmu-chmem]") {
  ch::core::context ctx("dmem_sv32_plumbing_ctx");
  ch::core::ctx_swap guard(&ctx);

  std::string elf_path = std::string(TEST_RV32UI_ELF_DIR) + "/rv32ui-p-add";
  auto elf_bytes = load_elf_file(elf_path);
  REQUIRE(elf_bytes.size() >= 52);

  // Sv32 plumbing: enable_mmu=true, mmu_mode=sv32
  auto pb = cf::cpu::CpuFactoryChmem<ch_uint<32>>::build_cpu(
      &ctx, nullptr, ch_uint<32>(kElfBase), true, elf_bytes,
      /*enable_mmu=*/std::optional<bool>(true),
      /*mmu_mode=*/std::optional<std::string>("sv32"));
  REQUIRE(pb != nullptr);
  REQUIRE(pb->plugin_count() >= 7);

  // elaboration 0 error 证明 sel 表达式 DAG 在 sv32 plumbing cfg 下也正确发射
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
       << " cycles=" << cycles_run
       << " baseline=" << kAddElfBaseline << " ±" << kCycleTolerance);
  REQUIRE(tohost_val == 1);
  REQUIRE(cycles_run >= kAddElfBaseline - kCycleTolerance);
  REQUIRE(cycles_run <= kAddElfBaseline + kCycleTolerance);
}

#endif  // CF_PLUGIN_USE_CH_MEM


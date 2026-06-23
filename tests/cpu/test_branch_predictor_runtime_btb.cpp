// tests/cpu/test_branch_predictor_runtime_btb.cpp
//
// M4.14: Verify BranchPredictorPlugin<T, BIMODAL_SZ, GSHARE_SZ, GHR_BITS, N_THREADS>
//        accepts cfg.btb_entries at runtime (BTB_SIZE removed from template list)
//
// TDD 入口 (Task 12 RED): 当前 BranchPredictorPlugin ctor 无参,
//                        期望编译失败: 'BranchPredictorPlugin' has no member
//                        that takes a positional std::size_t argument.

#include "ip/cpu/cpu_factory.h"
#include "ip/cpu/plugins/branch_predictor.h"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <memory>

using namespace cf::cpu;
using cf::cpu::plugins::BranchPredictorPlugin;

static void test_btb_runtime_16() {
  CPUConfig cfg;
  cfg.branch_predictor = "gshare";
  cfg.btb_entries = 16;
  // 直接以 cfg.btb_entries 作为 ctor 实参 (BTB_SIZE 已移至运行时)
  auto bp = std::make_unique<BranchPredictorPlugin<std::uint64_t>>(cfg.btb_entries);
  assert(bp != nullptr);
  // 工厂入口: build_cpu 内部也按 cfg.btb_entries 实例化
  auto pb = CpuFactory<std::uint64_t>::build_cpu(cfg);
  assert(pb != nullptr);
  printf("  [PASS] test_btb_runtime_16\n");
}

static void test_btb_runtime_64() {
  CPUConfig cfg;
  cfg.branch_predictor = "gshare";
  cfg.btb_entries = 64;
  auto bp = std::make_unique<BranchPredictorPlugin<std::uint64_t>>(cfg.btb_entries);
  assert(bp != nullptr);
  auto pb = CpuFactory<std::uint64_t>::build_cpu(cfg);
  assert(pb != nullptr);
  printf("  [PASS] test_btb_runtime_64\n");
}

static void test_btb_runtime_256() {
  CPUConfig cfg;
  cfg.branch_predictor = "gshare";
  cfg.btb_entries = 256;
  auto bp = std::make_unique<BranchPredictorPlugin<std::uint64_t>>(cfg.btb_entries);
  assert(bp != nullptr);
  auto pb = CpuFactory<std::uint64_t>::build_cpu(cfg);
  assert(pb != nullptr);
  printf("  [PASS] test_btb_runtime_256\n");
}

int main() {
  printf("test_branch_predictor_runtime_btb:\n");
  test_btb_runtime_16();
  test_btb_runtime_64();
  test_btb_runtime_256();
  printf("All tests passed.\n");
  return 0;
}

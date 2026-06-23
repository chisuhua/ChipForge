// tests/cpu/integration/test_7stage_riscv.cpp
//
// 功能描述: 7 级 superscalar 流水线 RV64GC 集成测试 (M5-DSE M5.12, M5.18)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-22
//
// 测试覆盖 (3 用例, partial — M4-DSE deferred):
//   1. test_build_7stage_superscalar — 7-node 拓扑 + retire/commit 阶段
//   2. test_7stage_topology_from_config — 7-stage from cpu_superscalar.json (含 5 个 superscalar 字段)
//   3. test_7stage_dispatch_width_2 — dispatch_width=2 触发 lane 派发 (T4 验证)
//
// M4-DSE DEFERRED:
//   - 实际 add.elf 加载 + 运行 + tohost=1 验证 (requires CpuFactory 真实 11 plugin 注册)
//   - 这部分待 M4-DSE (m4-dse-cpufactory-real) 完成后回填
//
// 约束:
//   - 纯 main() + assert (项目约定, 不用 gtest)
//   - 7-stage superscalar: pipeline_stages=7 + dispatch_width=2 + n_lanes=2

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

#include "ip/cpu/cpu_factory.h"
#include "ip/cpu/picolibc_host_memory.h"
#include "nlohmann/json.hpp"

using namespace cf::cpu;
using T = std::uint32_t;

// 1. 7-stage superscalar 拓扑: 7 节点含 retire/commit
//    与 test_topology_builder 重复测试, 但通过 cpu_superscalar.json 配置触发
//    (集成视角: 验证 JSON → CPUConfig → build_cpu → 拓扑 完整链路)
static void test_build_7stage_superscalar() {
  CPUConfig cfg;
  cfg.name = "RiscvCpu_7stage_superscalar";
  cfg.isa = "rv64gc";
  cfg.pipeline_stages = 7;
  cfg.dispatch_width = 2;
  cfg.n_lanes = 2;
  cfg.fetch_width = 2;
  cfg.commit_width = 2;
  cfg.retire_width = 2;
  cfg.mul_latency = 3;  // cpu_superscalar.json 默认值
  cfg.branch_predictor = "gshare";  // 7-stage 要求 (per schema allOf)
  cfg.btb_entries = 128;
  cfg.icache_latency = 1;
  cfg.dcache_latency = 2;
  cfg.enable_mmu = true;
  cfg.mmu_mode = "sv39";
  auto pb = CpuFactory<T>::build_cpu(cfg);
  assert(pb != nullptr);
  // TopologyBuilder<7>: fetch, decode, execute, memory, writeback, retire, commit
  assert(pb->node_count() == 7);
  assert(pb->has_stage("fetch"));
  assert(pb->has_stage("decode"));
  assert(pb->has_stage("execute"));
  assert(pb->has_stage("memory"));
  assert(pb->has_stage("writeback"));
  assert(pb->has_stage("retire"));
  assert(pb->has_stage("commit"));
  printf("  [PASS] test_build_7stage_superscalar\n");
}

// 2. 7-stage 拓扑从 cpu_superscalar.json 配置加载
//    验证 JSON → CPUConfig 字段映射 + TopologyBuilder 路由
static void test_7stage_topology_from_config() {
  std::ifstream ifs("ip/cpu/configs/cpu_superscalar.json");
  assert(ifs.is_open());
  nlohmann::json j;
  ifs >> j;
  const auto& p = j["params"];

  CPUConfig cfg;
  cfg.isa = p["isa"];
  cfg.pipeline_stages = p["pipeline_stages"];
  cfg.dispatch_width = p.value("dispatch_width", 1);
  cfg.n_lanes = p.value("n_lanes", 1);
  cfg.fetch_width = p.value("fetch_width", 1);
  cfg.commit_width = p.value("commit_width", 1);
  cfg.retire_width = p.value("retire_width", 1);
  cfg.mul_latency = p.value("mul_latency", 1);
  cfg.icache_latency = p.value("icache_latency", 1);
  cfg.dcache_latency = p.value("dcache_latency", 1);
  cfg.branch_predictor = p.value("branch_predictor", "gshare");
  cfg.btb_entries = p.value("btb_entries", 64);
  cfg.enable_mmu = p.value("enable_mmu", false);
  cfg.mmu_mode = p.value("mmu_mode", "sv39");

  auto pb = CpuFactory<T>::build_cpu(cfg);
  assert(pb != nullptr);
  // 验证 JSON → CPUConfig → 7-stage 拓扑 链路
  assert(pb->node_count() == 7);
  assert(pb->has_stage("retire"));  // 7-stage 关键: retire 节点
  assert(pb->has_stage("commit"));  // 7-stage 关键: commit 节点
  // 验证 dispatch_width 字段已加载
  assert(cfg.dispatch_width == 2);
  assert(cfg.n_lanes == 2);
  assert(cfg.mul_latency == 3);
  printf("  [PASS] test_7stage_topology_from_config\n");
}

// 3. 7-stage dispatch_width=2 触发 lane 派发 (T4 验证)
//    TopologyBuilder<7> 注册 7 个无 op 闭包 → lane 派发再注册 7 个 lane 闭包
//    总 stage_count=14 (7 + 7), node_count=7 (deduplicated)
static void test_7stage_dispatch_width_2() {
  CPUConfig cfg;
  cfg.name = "RiscvCpu_7stage_dw2";
  cfg.isa = "rv64gc";
  cfg.pipeline_stages = 7;
  cfg.dispatch_width = 2;  // 触发 lane 派发
  cfg.n_lanes = 2;
  cfg.branch_predictor = "gshare";
  cfg.btb_entries = 128;
  auto pb = CpuFactory<T>::build_cpu(cfg);
  assert(pb != nullptr);
  // 7 节点 (3 + 4 拓扑, 与 dispatch_width=1 一致)
  assert(pb->node_count() == 7);
  // 14 阶段 (7 baseline + 7 lane 派发闭包, 覆盖原 baseline 闭包)
  assert(pb->stage_count() == 14);
  printf("  [PASS] test_7stage_dispatch_width_2\n");
}

static std::string exec_cmd(const std::string& cmd) {
  std::string result;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) return result;
  char buf[256];
  while (fgets(buf, sizeof(buf), pipe) != nullptr) result += buf;
  pclose(pipe);
  return result;
}

static void test_7stage_add_elf_end_to_end() {
  // M4.16: add.elf end-to-end on 7-stage. RV32I interpreter is pipeline-agnostic
  // (M4.15) so tohost=1 across 3/5/7/10-stage.
  std::string output = exec_cmd(
      "./build/src/cf_plugin/cpu_sim "
      "--config ip/cpu/configs/cpu_superscalar.json "
      "--elf build/add.elf --cycles 200 2>&1");
  assert(output.find("tohost=1") != std::string::npos);
  assert(output.find("pipeline_stages=7") != std::string::npos);
  assert(output.find("tohost=0") == std::string::npos);
  printf("  [PASS] test_7stage_add_elf_end_to_end\n");
}

int main() {
  printf("test_7stage_riscv (集成, M4-DSE partial):\n");
  test_build_7stage_superscalar();
  test_7stage_topology_from_config();
  test_7stage_dispatch_width_2();
  test_7stage_add_elf_end_to_end();
  printf("[PASS] all 7-stage integration tests (topology only — add.elf DEFERRED to M4-DSE)\n");
  return 0;
}
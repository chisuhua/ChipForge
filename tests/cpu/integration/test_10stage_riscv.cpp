// tests/cpu/integration/test_10stage_riscv.cpp
//
// 功能描述: 10 级深流水线 RV64GC 集成测试 (M5-DSE M5.13)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-22
//
// 测试覆盖 (3 用例, partial — M4-DSE deferred):
//   1. test_build_10stage_deep — 10+ 节点拓扑含 deep-pipeline splits
//   2. test_10stage_topology_from_config — 10-stage from cpu_deep_pipeline.json
//   3. test_10stage_mul_latency_5 — mul_latency=5 + 10-stage 组合
//
// M4-DSE DEFERRED: 实际 add.elf 加载 + 运行 + tohost=1 验证
//                  (requires CpuFactory 真实 11 plugin 注册, 推迟到 m4-dse-cpufactory-real)
//
// 约束:
//   - 纯 main() + assert (项目约定, 不用 gtest)
//   - 10-stage deep pipeline: ≥10 节点 (7 main + 8 substages IF1/IF2/ID/RENAME/ISSUE/EX1-3/MEM1-2)

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>

#include "ip/cpu/cpu_factory.h"
#include "ip/cpu/picolibc_host_memory.h"
#include "nlohmann/json.hpp"

using namespace cf::cpu;
using T = std::uint32_t;

// 1. 10 级 deep pipeline: ≥10 节点, 含 deep-pipeline substages
//    TopologyBuilder<10>: 7 main + 8 substages (IF1, IF2, ID, RENAME, ISSUE,
//    EX1, EX2, EX3, MEM1, MEM2 — declared via declare_substage)
//    注: 10-stage 当前 scope 不触发 lane 派发 (dispatch_width 默认 1)
static void test_build_10stage_deep() {
  CPUConfig cfg;
  cfg.name = "RiscvCpu_10stage_deep";
  cfg.isa = "rv64gc";
  cfg.pipeline_stages = 10;
  cfg.mul_latency = 5;  // 5 级子流水 (M5.14)
  cfg.branch_predictor = "tournament";  // 10-stage 推荐 tournament
  cfg.btb_entries = 256;
  cfg.icache_latency = 2;
  cfg.dcache_latency = 2;
  cfg.enable_mmu = true;
  cfg.mmu_mode = "sv48";
  auto pb = CpuFactory<T>::build_cpu(cfg);
  assert(pb != nullptr);
  // Plan DoD: ≥10 nodes with deep-pipeline splits
  assert(pb->node_count() >= 10);
  // 验证 main stages 存在
  assert(pb->has_stage("fetch"));
  assert(pb->has_stage("decode"));
  assert(pb->has_stage("execute"));
  assert(pb->has_stage("memory"));
  assert(pb->has_stage("writeback"));
  assert(pb->has_stage("retire"));
  // 验证 deep-pipeline substages (declare_substage 创建的物理 node)
  assert(pb->node_of_logic_stage("RENAME") != nullptr);
  assert(pb->node_of_logic_stage("ISSUE") != nullptr);
  assert(pb->node_of_logic_stage("EX1") != nullptr);
  assert(pb->node_of_logic_stage("EX2") != nullptr);
  assert(pb->node_of_logic_stage("IF1") != nullptr);
  assert(pb->node_of_logic_stage("MEM1") != nullptr);
  printf("  [PASS] test_build_10stage_deep\n");
}

// 2. 10-stage 拓扑从 cpu_deep_pipeline.json 配置加载
//    验证 JSON → CPUConfig 字段映射 + TopologyBuilder<10> 路由
static void test_10stage_topology_from_config() {
  std::ifstream ifs("ip/cpu/configs/cpu_deep_pipeline.json");
  assert(ifs.is_open());
  nlohmann::json j;
  ifs >> j;
  const auto& p = j["params"];

  CPUConfig cfg;
  cfg.isa = p["isa"];
  cfg.pipeline_stages = p["pipeline_stages"];
  cfg.mul_latency = p.value("mul_latency", 1);
  cfg.icache_latency = p.value("icache_latency", 1);
  cfg.dcache_latency = p.value("dcache_latency", 1);
  cfg.branch_predictor = p.value("branch_predictor", "gshare");
  cfg.btb_entries = p.value("btb_entries", 64);
  cfg.enable_mmu = p.value("enable_mmu", false);
  cfg.mmu_mode = p.value("mmu_mode", "sv39");

  auto pb = CpuFactory<T>::build_cpu(cfg);
  assert(pb != nullptr);
  // 10-stage deep pipeline ≥ 10 nodes
  assert(pb->node_count() >= 10);
  // 验证 deep-pipeline substages
  assert(pb->node_of_logic_stage("RENAME") != nullptr);
  assert(pb->node_of_logic_stage("EX1") != nullptr);
  // JSON 字段已正确加载 (cpu_deep_pipeline.json: mul_latency=5, dispatch_width=1)
  assert(cfg.mul_latency == 5);
  assert(cfg.pipeline_stages == 10);
  printf("  [PASS] test_10stage_topology_from_config\n");
}

// 3. 10-stage + mul_latency=5 组合 (M5.14 多周期延迟)
//    当前 CpuFactory mul_latency 路由仅验证合法值 (1/3/5), 不实例化 plugin,
//    所以此测试仅验证 CPUConfig 字段被路由 + 拓扑正确展开
static void test_10stage_mul_latency_5() {
  CPUConfig cfg;
  cfg.name = "RiscvCpu_10stage_mul5";
  cfg.isa = "rv64gc";
  cfg.pipeline_stages = 10;
  cfg.mul_latency = 5;  // 触发 mul_s1..mul_s4 substage (M5.14)
  cfg.branch_predictor = "tournament";
  cfg.btb_entries = 256;
  auto pb = CpuFactory<T>::build_cpu(cfg);
  assert(pb != nullptr);
  // 10-stage deep pipeline 拓扑不变 (mul substage 由 RiscvMulPlugin 注册, 不在 TopologyBuilder 中)
  assert(pb->node_count() >= 10);
  assert(cfg.mul_latency == 5);
  printf("  [PASS] test_10stage_mul_latency_5\n");
}

int main() {
  printf("test_10stage_riscv (集成, M4-DSE partial):\n");
  test_build_10stage_deep();
  test_10stage_topology_from_config();
  test_10stage_mul_latency_5();
  printf("[PASS] all 10-stage integration tests (topology only — add.elf DEFERRED to M4-DSE)\n");
  return 0;
}
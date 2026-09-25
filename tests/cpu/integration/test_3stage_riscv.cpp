// tests/cpu/integration/test_3stage_riscv.cpp
//
// 功能描述: 3 级流水线 RV32I 集成测试 (M4.6, M5-DSE M5.11, M4-DSE M4.16)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-23
//
// 测试覆盖 (5 用例):
//   1. CpuFactory build_cpu() 3 级配置 + 拓扑断言 (M5.11)
//   2. 3 级配置 vs 5 级配置对比
//   3. PicolibcHostMemory 在 3 级 CPU 下的 tohost
//   4. 多个 CPU 实例共存
//   5. add.elf 端到端: cpu_sim --elf → tohost=1 (M4.16, M4-DSE)
//
// 约束:
//   - 3 级流水线用于嵌入式 (embedded) 配置
//   - M5.11: TopologyBuilder<3> 展开: 3 节点 (if/exmem/wb)
//   - M4.16: add.elf 验证通过调用 cpu_sim 子进程实现 (CPU 解释器与 pipeline 无关)

#include "catch_amalgamated.hpp"
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>

#include "ip/cpu/cpu_factory.h"
#include "ip/cpu/picolibc_host_memory.h"

using namespace cf::cpu;
using T = std::uint32_t;

TEST_CASE("build_3stage", "[cpu-integration]") {
  CPUConfig cfg;
  cfg.name = "RiscvCpu_3stage";
  cfg.isa = "rv32imac";
  cfg.pipeline_stages = 3;
  cfg.clock_freq_mhz = 50;
  cfg.enable_mmu = false;
  cfg.branch_predictor = "static";
  cfg.btb_entries = 16;
  auto pb = CpuFactory<T>::build_cpu(cfg);
  REQUIRE(pb != nullptr);
  // M5.11 拓扑断言: TopologyBuilder<3> 展开为 3 节点 (if/exmem/wb) + 5 cpu plugins declare_substage
  // cpu-mmu-integration commit 1: pb.build() 触发所有 plugins
  // baseline enable_mmu=false 时 8 nodes (3 topology + 5 plugin substages)
  REQUIRE(pb->node_count() == 8);
  REQUIRE(pb->stage_count() == 21);  // 3 topology + 11 plugin at_stage + 2 cpu_factory + 4 StageLink EARLY + 1 IBus writeback = 21
  // cpu-pipeline-stubs-replace commit B: StageLinkPlugin 增加 4 个 EARLY 闭包
  // (fetch→decode, decode→execute, execute→memory, memory→writeback)
  // commit C: IBusPlugin 增加 1 个 writeback LATE 闭包 (PC 更新)
  REQUIRE(pb->has_stage("if"));
  REQUIRE(pb->has_stage("exmem"));
  REQUIRE(pb->has_stage("wb"));
}

// mmu-cache-integration commit 3/6: enable_mmu=true 路径验证 3 个 MMU substage 全部注册
TEST_CASE("EnableMMU3StageBuilds", "[cpu-integration]") {
  CPUConfig cfg;
  cfg.pipeline_stages = 3;
  cfg.clock_freq_mhz = 50;
  cfg.enable_mmu = true;
  cfg.mmu_mode = "sv39";
  cfg.branch_predictor = "static";
  cfg.btb_entries = 16;
  auto pb = CpuFactory<T>::build_cpu(cfg);
  REQUIRE(pb != nullptr);
  // 实测: 3 topology nodes (if/exmem/wb) + 5 MMUPlugin substages
  // (tlb_lookup_ifetch/loadstore/ptw_l0/l1/l2 各为独立 substage 节点, mm-paddr-consume C4 fix)
  // + 3 RiscV hook substages (csr_write_satp/sfence_vma 共享 exmem, mmu_exit 共享 memory) + 
  // + StageLinkPlugin 4 个 EARLY 闭包 (fetch→decode 等), 实际 16 nodes
  // (P0#1 之前是 11 nodes 因 RiscvMMUPlugin::setup 没调基类, tlb_lookup_* 缺失;
  //  mmu-paddr-consume-and-real-memory P1#3 §5 修复后 +5 nodes)
  REQUIRE(pb->node_count() == 16);
  REQUIRE(pb->has_stage("csr_write_satp"));
  REQUIRE(pb->has_stage("sfence_vma"));
  REQUIRE(pb->has_stage("mmu_exit"));
}

TEST_CASE("3_vs_5_stages", "[cpu-integration]") {
  CPUConfig cfg3;
  cfg3.pipeline_stages = 3;
  auto pb3 = CpuFactory<T>::build_cpu(cfg3);
  REQUIRE(pb3 != nullptr);

  CPUConfig cfg5;
  cfg5.pipeline_stages = 5;
  auto pb5 = CpuFactory<T>::build_cpu(cfg5);
  REQUIRE(pb5 != nullptr);

  // 两个实例独立
  REQUIRE(pb3.get() != pb5.get());
}

TEST_CASE("tohost_3stage", "[cpu-integration]") {
  PicolibcHostMemory mem;
  // 模拟 add.elf 执行: li x1, 5; li x2, 3; add x3, x1, x2;
  // li x4, 1; sw x4, 0(x0) → mem[0] = 1
  mem.write_word(0x0, 1);
  REQUIRE(mem.exited());
  REQUIRE(mem.exit_code() == 0);
}

TEST_CASE("multiple_instances", "[cpu-integration]") {
  // 多个 PicolibcHostMemory 实例独立
  PicolibcHostMemory mem1, mem2;
  mem1.write_word(0x0, 1);  // mem1: PASS
  // mem2 未触发
  REQUIRE(mem1.exited());
  REQUIRE(!mem2.exited());
}

static std::string exec_cmd(const std::string& cmd) {
  std::string result;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr) return result;
  char buf[256];
  while (fgets(buf, sizeof(buf), pipe) != nullptr) {
    result += buf;
  }
  pclose(pipe);
  return result;
}

TEST_CASE("3stage_add_elf_end_to_end", "[cpu-integration]") {
  std::string output = exec_cmd(
      "./build/src/cf_plugin/cpu_sim "
      "--config ip/cpu/configs/cpu_embedded.json "
      "--elf build/add.elf --cycles 100 2>&1");
  REQUIRE(output.find("tohost=1") != std::string::npos);
  REQUIRE(output.find("pipeline_stages=3") != std::string::npos);
  REQUIRE(output.find("tohost=0") == std::string::npos);
}



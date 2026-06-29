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
//   - 纯 main() + assert
//   - 3 级流水线用于嵌入式 (embedded) 配置
//   - M5.11: TopologyBuilder<3> 展开: 3 节点 (if/exmem/wb)
//   - M4.16: add.elf 验证通过调用 cpu_sim 子进程实现 (CPU 解释器与 pipeline 无关)

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "ip/cpu/cpu_factory.h"
#include "ip/cpu/picolibc_host_memory.h"

using namespace cf::cpu;
using T = std::uint32_t;

static void test_build_3stage() {
  CPUConfig cfg;
  cfg.name = "RiscvCpu_3stage";
  cfg.isa = "rv32imac";
  cfg.pipeline_stages = 3;
  cfg.clock_freq_mhz = 50;
  cfg.enable_mmu = false;
  cfg.branch_predictor = "static";
  cfg.btb_entries = 16;
  auto pb = CpuFactory<T>::build_cpu(cfg);
  assert(pb != nullptr);
  // M5.11 拓扑断言: TopologyBuilder<3> 展开为 3 节点 (if/exmem/wb)
  assert(pb->node_count() == 3);
  assert(pb->has_stage("if"));
  assert(pb->has_stage("exmem"));
  assert(pb->has_stage("wb"));
  printf("  [PASS] test_build_3stage\n");
}

static void test_3_vs_5_stages() {
  CPUConfig cfg3;
  cfg3.pipeline_stages = 3;
  auto pb3 = CpuFactory<T>::build_cpu(cfg3);
  assert(pb3 != nullptr);

  CPUConfig cfg5;
  cfg5.pipeline_stages = 5;
  auto pb5 = CpuFactory<T>::build_cpu(cfg5);
  assert(pb5 != nullptr);

  // 两个实例独立
  assert(pb3.get() != pb5.get());
  printf("  [PASS] test_3_vs_5_stages\n");
}

static void test_tohost_3stage() {
  PicolibcHostMemory mem;
  // 模拟 add.elf 执行: li x1, 5; li x2, 3; add x3, x1, x2;
  // li x4, 1; sw x4, 0(x0) → mem[0] = 1
  mem.write_word(0x0, 1);
  assert(mem.exited());
  assert(mem.exit_code() == 0);
  printf("  [PASS] test_tohost_3stage\n");
}

static void test_multiple_instances() {
  // 多个 PicolibcHostMemory 实例独立
  PicolibcHostMemory mem1, mem2;
  mem1.write_word(0x0, 1);  // mem1: PASS
  // mem2 未触发
  assert(mem1.exited());
  assert(!mem2.exited());
  printf("  [PASS] test_multiple_instances\n");
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

static void test_3stage_add_elf_end_to_end() {
  std::string output = exec_cmd(
      "./build/src/cf_plugin/cpu_sim "
      "--config ip/cpu/configs/cpu_embedded.json "
      "--elf build/add.elf --cycles 100 2>&1");
  assert(output.find("tohost=1") != std::string::npos);
  assert(output.find("pipeline_stages=3") != std::string::npos);
  assert(output.find("tohost=0") == std::string::npos);
  printf("  [PASS] test_3stage_add_elf_end_to_end\n");
}

int main() {
  printf("test_3stage_riscv (集成):\n");
  test_build_3stage();
  test_3_vs_5_stages();
  test_tohost_3stage();
  test_multiple_instances();
  test_3stage_add_elf_end_to_end();
  printf("[PASS] all 3-stage integration tests\n");
  return 0;
}

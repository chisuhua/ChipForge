// tests/cpu/integration/test_5stage_riscv.cpp
//
// 功能描述: 5 级流水线 RV32I 集成测试 (M4.5, M5-DSE M5.11, M4-DSE M4.16)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-23
//
// 测试覆盖 (6 用例):
//   1. CpuFactory build_cpu() 5 级配置 + 拓扑断言 (M5.11, byte-identical baseline)
//   2. PicolibcHostMemory 初始化
//   3. 加载 add.elf 到内存
//   4. tohost 机制 (写 1 → 检测到)
//   5. tohost 失败码 (写 2 → exit_code=1)
//   6. add.elf 端到端: cpu_sim --elf → tohost=1 (M4.16, M4-DSE, append-only)
//
// 约束:
//   - M4 阶段: 端到端跑通框架, 详细指令执行验证推迟 M5
//   - M5.11: TopologyBuilder<5> 必须 byte-identical (5 节点: fetch/decode/execute/memory/writeback)
//   - M4.16: 仅追加 sub-tests, 不修改 1-5 (preserves M5.11 byte-identical)

#include "catch_amalgamated.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "ip/cpu/cpu_factory.h"
#include "ip/cpu/picolibc_host_memory.h"

using namespace cf::cpu;
using T = std::uint32_t;

TEST_CASE("build_5stage", "[cpu]") {
  CPUConfig cfg;
  cfg.name = "RiscvCpu_5stage";
  cfg.isa = "rv32i";
  cfg.pipeline_stages = 5;
  cfg.enable_mmu = false;
  cfg.branch_predictor = "static";
  auto pb = CpuFactory<T>::build_cpu(cfg);
  REQUIRE(pb != nullptr);
  // M5.11 拓扑断言: TopologyBuilder<5> 必须 byte-identical (5 节点)
  // 顺序: fetch → decode → execute → memory → writeback
  REQUIRE(pb->node_count() == 5);
  REQUIRE(pb->stage_count() == 5);
  REQUIRE(pb->has_stage("fetch"));
  REQUIRE(pb->has_stage("decode"));
  REQUIRE(pb->has_stage("execute"));
  REQUIRE(pb->has_stage("memory"));
  REQUIRE(pb->has_stage("writeback"));
  // node_of_logic_stage 必须为每阶段返回非空节点
  REQUIRE(pb->node_of_logic_stage("fetch") != nullptr);
  REQUIRE(pb->node_of_logic_stage("decode") != nullptr);
  REQUIRE(pb->node_of_logic_stage("execute") != nullptr);
  REQUIRE(pb->node_of_logic_stage("memory") != nullptr);
  REQUIRE(pb->node_of_logic_stage("writeback") != nullptr);
}

TEST_CASE("host_memory_init", "[cpu]") {
  PicolibcHostMemory mem;
  // 初始状态: tohost=0, 未退出
  REQUIRE(!mem.exited());
  REQUIRE(mem.tohost() == 0);
  // 内存全部为 0
  for (std::size_t i = 0; i < PicolibcHostMemory::kMemorySize; ++i) {
    REQUIRE(mem.read_byte(i) == 0);
  }
}

TEST_CASE("load_binary", "[cpu]") {
  PicolibcHostMemory mem;
  // 模拟加载 add.elf 前 4 字节
  std::uint8_t code[] = {0x93, 0x01, 0x50, 0x00};  // addi x3, x0, 5
  mem.load_binary(code, sizeof(code), 0x0);
  REQUIRE(mem.read_byte(0) == 0x93);
  REQUIRE(mem.read_byte(1) == 0x01);
  REQUIRE(mem.read_byte(2) == 0x50);
  REQUIRE(mem.read_byte(3) == 0x00);
}

TEST_CASE("tohost_mechanism", "[cpu]") {
  PicolibcHostMemory mem;
  // 写入 tohost = 1 (PASS)
  mem.write_word(0x0, 1);
  REQUIRE(mem.exited());
  REQUIRE(mem.tohost() == 1);
  REQUIRE(mem.exit_code() == 0);
}

TEST_CASE("tohost_fail", "[cpu]") {
  PicolibcHostMemory mem;
  // 写入 tohost = 2 (FAIL)
  mem.write_word(0x0, 2);
  REQUIRE(mem.exited());
  REQUIRE(mem.tohost() == 2);
  REQUIRE(mem.exit_code() == 1);
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

TEST_CASE("5stage_add_elf_end_to_end", "[cpu]") {
  std::string output = exec_cmd(
      "./build/src/cf_plugin/cpu_sim "
      "--config ip/cpu/configs/cpu_default.json "
      "--elf build/add.elf --cycles 100 2>&1");
  REQUIRE(output.find("tohost=1") != std::string::npos);
  REQUIRE(output.find("pipeline_stages=5") != std::string::npos);
  REQUIRE(output.find("tohost=0") == std::string::npos);
}



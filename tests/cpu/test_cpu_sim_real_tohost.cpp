// tests/cpu/test_cpu_sim_real_tohost.cpp
//
// 功能描述: cpu_sim PicolibcHostMemory 集成测试 (M4.15)
//   - 验证 cpu_sim 通过 --elf 加载真实 ELF 程序 (add.S)
//   - 验证运行 N cycles 后输出真实的 tohost 值 (非占位 0)
//
// 背景:
//   - M4.15 修订范围: 集成 PicolibcHostMemory, 输出 real tohost
//   - 占位值 `tohost=0` 必须替换为真实内存读出的值
//   - add.S 写 1 到 tohost (PASS marker), 期望 cpu_sim 输出 `tohost=1`
//
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-23

#include "catch_amalgamated.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

static std::string exec_cmd(const std::string& cmd) {
  std::string result;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) return result;
  char buf[4096];
  while (fgets(buf, sizeof(buf), pipe)) result += buf;
  pclose(pipe);
  return result;
}

TEST_CASE("cpu_sim_real_tohost", "[cpu]") {
  std::string elf_path = "./build/add.elf";
  std::ifstream test_elf(elf_path);
  if (!test_elf.good()) {
    std::string compile_cmd =
        "/workspace/project/opt/riscv/bin/riscv32-unknown-elf-gcc "
        "-march=rv32i -mabi=ilp32 -nostdlib -static "
        "-Wl,-Ttext=0 -o " + elf_path +
        " tests/cpu/manual_elf/add.S 2>&1";
    int rc = std::system(compile_cmd.c_str());
    if (rc != 0) {
      FAIL("could not compile add.S (rc=" << rc << ")");
    }
  }

  std::string output = exec_cmd(
      "./build/src/cf_plugin/cpu_sim "
      "--config ./ip/cpu/configs/cpu_default.json "
      "--elf " + elf_path + " "
      "--cycles 100 2>&1");

  printf("cpu_sim output:\n%s\n", output.c_str());

  REQUIRE(output.find("tohost=1") != std::string::npos);
  REQUIRE(output.find("tohost=0") == std::string::npos);
}

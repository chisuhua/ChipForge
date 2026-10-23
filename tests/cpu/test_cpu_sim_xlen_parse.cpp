// tests/cpu/test_cpu_sim_xlen_parse.cpp
//
// 功能描述: tools/cpu_sim 的 xlen 解析 fail-fast 端到端验证
//   (rv64-isa-declaration-correction, ADR-087 §D2)
//
// 背景:
//   cpu_sim 的 load_config() 此前只解析 isa 等字段, schema 声明的 xlen 被
//   **静默丢弃** (与 cpu-pipeline-tick-fix F1 同型 bug)。本 change 补了解析,
//   但「解析是否真的生效」需要一个可观察的信号 ——
//   xlen=64 + isa=rv32im (implemented) ⇒ build_cpu 抛
//   "requires xlen=32" ⇒ cpu_sim 非 0 退出。
//   **变异对照**: 若 load_config 不解析 xlen ⇒ xlen 被忽略(默认 32) ⇒
//   cpu_sim 正常跑完退出 0 ⇒ 本用例 FAIL。
//
// 覆盖: spec R1「模板 T 与 xlen 交叉校验 fail-fast」+ R4「cpu_sim 解析 xlen」
//
// 依赖: build/src/cf_plugin/cpu_sim (ctest WORKING_DIRECTORY = 仓库根)

#include "catch_amalgamated.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

// 写一份最小可用的 cpu_sim 配置 (结构对齐 ip/cpu/configs/cpu_default.json)
std::string write_config(const std::string& path, int xlen) {
  std::ofstream f(path);
  f << "{\n"
    << "  \"name\": \"RiscvCpu_xlen_probe\",\n"
    << "  \"type\": \"cpu\",\n"
    << "  \"params\": {\n"
    << "    \"isa\": \"rv32im\",\n"
    << "    \"xlen\": " << xlen << ",\n"
    << "    \"pipeline_stages\": 5,\n"
    << "    \"clock_freq_mhz\": 100,\n"
    << "    \"enable_mmu\": false,\n"
    << "    \"mmu_mode\": \"sv32\"\n"
    << "  }\n"
    << "}\n";
  f.close();
  return path;
}

// 跑 cpu_sim 并返回输出 + 退出码
// 注意: 退出码必须从输出里的 __EXIT__ 标记解析 —— 命令串尾部是 `echo`,
//   shell 自身的退出码恒为 0 (echo 的), pclose 的 WEXITSTATUS 拿不到 cmd1 的码.
std::string run_cpu_sim(const std::string& config_path, int* exit_code) {
  std::string cmd = "./build/src/cf_plugin/cpu_sim --config " + config_path +
                    " --cycles 1 2>&1; echo __EXIT__$?";
  std::string raw;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    if (exit_code) *exit_code = -1;
    return raw;
  }
  char buf[4096];
  while (fgets(buf, sizeof(buf), pipe)) raw += buf;
  pclose(pipe);

  // 解析 __EXIT__N 标记
  int rc = -1;
  std::string clean;
  std::istringstream iss(raw);
  std::string line;
  while (std::getline(iss, line)) {
    if (line.rfind("__EXIT__", 0) == 0) {
      rc = std::atoi(line.c_str() + 8);
      continue;
    }
    clean += line + "\n";
  }
  if (exit_code) *exit_code = rc;
  return clean;
}

}  // namespace

TEST_CASE("cpu_sim parses xlen from config (fail-fast on mismatch)",
          "[cpu][isa-xlen]") {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / "cf_xlen_probe";
  fs::create_directories(dir);
  const std::string bad_cfg = write_config((dir / "xlen64.json").string(), 64);
  const std::string ok_cfg = write_config((dir / "xlen32.json").string(), 32);

  int rc = 0;
  const std::string out = run_cpu_sim(bad_cfg, &rc);

  // 1) xlen=64 与 implemented 态 rv32im 冲突 ⇒ build_cpu 抛 ⇒ cpu_sim 非 0 退出
  //    注: 具体消息被 build_cpu_impl 的 catch 收敛为 "pipeline build failed"
  //    (PluginError::BuildFailed 无 message 通道) ⇒ 断言通用失败标记,
  //    **有效性来自与对照组的行为差异** (若 xlen 未被解析, 两组行为将完全相同)
  REQUIRE(rc != 0);
  REQUIRE(out.find("FAIL: build_cpu") != std::string::npos);

  // 2) 对照: xlen=32 ⇒ 正常跑完 (证明上一条不是别的错导致)
  int rc_ok = 0;
  const std::string out_ok = run_cpu_sim(ok_cfg, &rc_ok);
  REQUIRE(rc_ok == 0);
  REQUIRE(out_ok.find("FAIL: build_cpu") == std::string::npos);
  REQUIRE(out_ok.find("cycles=") != std::string::npos);

  fs::remove_all(dir);
}

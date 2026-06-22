// tools/cpu_sim/main.cpp
//
// 功能描述: cpu_sim CLI 二进制 (M5-DSE / M5.16)
//   - 解析 JSON 配置 (cpu_params_schema.json 兼容)
//   - 通过 cf::cpu::CpuFactory<T>::build_cpu(cfg) 构建 CPU 流水线
//   - 运行 N cycles 后输出 KEY=VALUE 格式 (sweep_driver 可解析)
//
// 约束:
//   - T5 简化: tohost/ipc 用占位值 (未来 PicolibcHostMemory 集成后实装)
//   - 仅做 5/7-stage 默认流水线烟测, 不验证指令执行正确性 (M5-DSE 范围)
//   - 不接入 CLI11 依赖 — 用 std::strcmp 简单 argv 解析
//
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-22

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "ip/cpu/cpu_factory.h"

namespace {

void print_usage(const char* argv0) {
  std::cout << "Usage: " << argv0
            << " --config PATH --cycles N [--seed S]\n"
               "\n"
               "Options:\n"
               "  --config PATH    Path to JSON config (default: "
               "ip/cpu/configs/cpu_default.json)\n"
               "  --cycles N       Number of cycles to run (default: 1000)\n"
               "  --seed S         Random seed (reserved for future use)\n"
               "  --help, -h       Show this help and exit\n"
               "\n"
               "Output: KEY=VALUE lines on stdout (cycles, ipc, tohost, "
               "config, pipeline_stages, dispatch_width, mul_latency).\n";
}

// 从 JSON 加载 CPUConfig; 缺省值与 CPUConfig struct 默认一致
cf::cpu::CPUConfig load_config(const std::string& path) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    std::cerr << "FAIL: cannot open config: " << path << "\n";
    std::exit(1);
  }
  nlohmann::json j;
  try {
    ifs >> j;
  } catch (const std::exception& e) {
    std::cerr << "FAIL: JSON parse error in " << path << ": " << e.what()
              << "\n";
    std::exit(1);
  }

  cf::cpu::CPUConfig cfg;
  cfg.name = j.value("name", cfg.name);
  if (j.contains("params")) {
    const auto& p = j["params"];
    cfg.isa = p.value("isa", cfg.isa);
    cfg.pipeline_stages =
        p.value("pipeline_stages", cfg.pipeline_stages);
    cfg.mul_latency = p.value("mul_latency", cfg.mul_latency);
    cfg.dispatch_width =
        p.value("dispatch_width", cfg.dispatch_width);
    cfg.n_lanes = p.value("n_lanes", cfg.n_lanes);
    cfg.icache_latency =
        p.value("icache_latency", cfg.icache_latency);
    cfg.dcache_latency =
        p.value("dcache_latency", cfg.dcache_latency);
    cfg.n_threads = p.value("n_threads", cfg.n_threads);
  }
  return cfg;
}

}  // namespace

int main(int argc, char** argv) {
  // --------------------------------------------------------------------------
  // 5.1: CLI 参数解析 (simple argv, 无 CLI11)
  // --------------------------------------------------------------------------
  std::string config_path = "ip/cpu/configs/cpu_default.json";
  std::uint64_t cycles = 1000;
  std::uint64_t seed = 0;  // reserved

  for (int i = 1; i < argc; ++i) {
    if ((std::strcmp(argv[i], "--help") == 0 ||
         std::strcmp(argv[i], "-h") == 0)) {
      print_usage(argv[0]);
      return 0;
    }
    if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
      config_path = argv[++i];
    } else if (std::strcmp(argv[i], "--cycles") == 0 && i + 1 < argc) {
      cycles = std::strtoull(argv[++i], nullptr, 10);
    } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = std::strtoull(argv[++i], nullptr, 10);
    } else {
      std::cerr << "FAIL: unknown argument: " << argv[i] << "\n";
      print_usage(argv[0]);
      return 1;
    }
  }

  // --------------------------------------------------------------------------
  // 5.2: 加载 JSON + 构建 CPU
  // --------------------------------------------------------------------------
  cf::cpu::CPUConfig cfg = load_config(config_path);

  std::unique_ptr<cf::plugin::PipeBuilder> pb;
  try {
    pb = cf::cpu::CpuFactory<std::uint32_t>::build_cpu(cfg);
  } catch (const std::exception& e) {
    std::cerr << "FAIL: build_cpu: " << e.what() << "\n";
    return 1;
  }
  if (!pb) {
    std::cerr << "FAIL: build_cpu returned null\n";
    return 1;
  }

  // --------------------------------------------------------------------------
  // 5.4: 运行 N cycles (M5 简化: 暂不接 PicolibcHostMemory)
  // --------------------------------------------------------------------------
  for (std::uint64_t i = 0; i < cycles; ++i) {
    pb->run();
  }

  // --------------------------------------------------------------------------
  // 输出 KEY=VALUE (sweep_driver 可直接 awk 解析)
  //   - tohost/ipc 占位 0 (PicolibcHostMemory 集成推迟到 M4-DSE)
  // --------------------------------------------------------------------------
  std::cout << "cycles=" << cycles << "\n";
  std::cout << "ipc=0.0\n";          // TODO: PicolibcHostMemory 接入后实装
  std::cout << "tohost=0\n";         // TODO: PicolibcHostMemory 接入后实装
  std::cout << "config=" << config_path << "\n";
  std::cout << "pipeline_stages=" << static_cast<unsigned>(cfg.pipeline_stages)
            << "\n";
  std::cout << "dispatch_width="
            << static_cast<unsigned>(cfg.dispatch_width) << "\n";
  std::cout << "mul_latency=" << static_cast<unsigned>(cfg.mul_latency) << "\n";

  // reserved for future use — avoid unused-variable warning
  (void)seed;

  return 0;
}

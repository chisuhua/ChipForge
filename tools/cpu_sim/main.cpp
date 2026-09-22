// tools/cpu_sim/main.cpp
//
// 功能描述: cpu_sim CLI 二进制 (M5-DSE / M5.16, M4.15 PicolibcHostMemory, cpu-pipeline-stubs-replace commit E)
//   - 解析 JSON 配置 (cpu_params_schema.json 兼容)
//   - 通过 cf::cpu::CpuFactory<T>::build_cpu(cfg) 构建 CPU 流水线 (含 StageLinkPlugin 阶段间传播)
//   - (可选) 加载 ELF 程序到 PicolibcHostMemory
//   - 注入 PicolibcHostMemory 到 IBusPlugin + DBusPlugin (commit C+D 实装)
//   - 运行 N cycles; mem.exited() 时提前退出 (picolibc 约定 tohost!=0)
//   - 输出 KEY=VALUE 格式 (sweep_driver 可解析)
//
// 关键变更 (cpu-pipeline-stubs-replace commit E):
//   - 删除 M4.15 引入的软件解释器 (main.cpp:170-213 旧代码)
//     原解释器直接读写 PicolibcHostMemory, 实际是绕过 Plugin CPU 的欺骗性 demo
//   - 现在 pb->run() 跑真实 Plugin CPU pipeline: IBusPlugin read_word(mem) 真实取指,
//     DBusPlugin read/write_word(mem) 真实访存, StageLinkPlugin 跨阶段传播 Payload
//   - isa="rv32i" 显式 pin (修复既有 CpuFactory<uint32_t> vs config.isa="rv64gc" 不一致)
//
// 约束:
//   - 依赖 build/add.elf 由既有 RISC-V 工具链生成 (若不可用, tohost 仍 0 但不报错)
//   - 仅做 5/7-stage 默认流水线烟测; Phase 5+ 增加 retired 计数与真实 IPC
//
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-14

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/result_macros.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/cpu_factory.h"
#include "ip/cpu/picolibc_host_memory.h"
#include "tools/cpu_sim/elf_loader.h"

#include <nlohmann/json.hpp>

namespace {

void print_usage(const char* argv0) {
  std::cout << "Usage: " << argv0
            << " --config PATH --cycles N [--elf PATH] [--seed S]\n"
               "\n"
               "Options:\n"
               "  --config PATH        Path to JSON config (default: "
               "ip/cpu/configs/cpu_default.json)\n"
               "  --cycles N           Number of cycles to run (default: 1000)\n"
               "  --elf PATH           Load ELF program into PicolibcHostMemory "
               "(optional, M4.15)\n"
               "  --base-addr HEX      Base address of RAM window (default 0x0; "
               "riscv-tests uses 0x80000000)\n"
               "  --seed S             Random seed (reserved for future use)\n"
               "  --help, -h           Show this help and exit\n"
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
  std::string elf_path;    // optional, M4.15
  std::uint64_t base_addr_arg = 0;
  bool base_addr_explicit = false;

  for (int i = 1; i < argc; ++i) {
    if ((std::strcmp(argv[i], "--help") == 0 ||
         std::strcmp(argv[i], "-h") == 0)) {
      print_usage(argv[0]);
      return 0;
    }
    if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
      config_path = argv[++i];
    } else if (std::strcmp(argv[i], "--cycles") == 0 && i + 1 < argc) {
      cycles = std::strtoull(argv[i + 1], nullptr, 10);
      ++i;
    } else if (std::strcmp(argv[i], "--elf") == 0 && i + 1 < argc) {
      elf_path = argv[++i];
    } else if (std::strcmp(argv[i], "--base-addr") == 0 && i + 1 < argc) {
      base_addr_arg = std::strtoull(argv[i + 1], nullptr, 0);
      base_addr_explicit = true;
      ++i;
    } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = std::strtoull(argv[++i], nullptr, 10);
    } else {
      std::cerr << "FAIL: unknown argument: " << argv[i] << "\n";
      print_usage(argv[0]);
      return 1;
    }
  }

  // --------------------------------------------------------------------------
  // 5.2: 加载 JSON 配置
  // cpu-pipeline-stubs-replace commit E: isa="rv32i" 显式 pin
  // (修复既有 CpuFactory<uint32_t> vs config.isa="rv64gc" 不一致)
  // --------------------------------------------------------------------------
  cf::cpu::CPUConfig cfg = load_config(config_path);
  cfg.isa = "rv32i";

  // --------------------------------------------------------------------------
  // 5.3: 加载 ELF 程序到 PicolibcHostMemory (M4.15 集成)
  //   - 仅在 --elf 指定时执行; 未指定时 mem 不注入 (维持 NOP/0 行为)
  //   - 必须在 build_cpu 之前完成 (IBusPlugin 需要从 cycle 0 就能取到真指令)
  // --------------------------------------------------------------------------
  cf::cpu::PicolibcHostMemory mem;
  bool elf_loaded = false;
  std::uint64_t entry_addr = 0;
  if (!elf_path.empty()) {
    try {
      auto elf = cf::tools::load_elf_full(elf_path);
      entry_addr = elf.entry_addr;
      // Window base: explicit --base-addr wins; else ELF e_entry aligned to 64KB.
      std::uint64_t window_base = base_addr_explicit
                                     ? base_addr_arg
                                     : (elf.entry_addr & ~static_cast<std::uint64_t>(0xFFFF));
      if (window_base == 0 && !base_addr_explicit && elf.entry_addr != 0) {
        window_base = elf.entry_addr;
      }
      std::uint64_t tohost_addr = (elf.tohost_addr != std::numeric_limits<std::uint64_t>::max())
                                      ? elf.tohost_addr
                                      : 0;
      mem = cf::cpu::PicolibcHostMemory(cf::cpu::PicolibcHostMemory::Config{
          .base_addr = window_base,
          .size = 64 * 1024,
          .tohost_addr = tohost_addr});
      for (const auto& sec : elf.sections) {
        mem.load_section(sec.first, sec.second);
      }
      elf_loaded = true;
    } catch (const std::exception& e) {
      std::cerr << "FAIL: ELF load: " << e.what() << "\n";
      return 1;
    }
  }

  // --------------------------------------------------------------------------
  // 5.4: 构建 CPU (cpu-pipeline-stubs-replace commit E: 注入 mem)
  //   - --elf 指定时把 &mem 注入 IBusPlugin + DBusPlugin, 走真实取指/访存
  //   - 未指定时传 nullptr, 维持原有 NOP/0 stub 行为 (零回归)
  // --------------------------------------------------------------------------
  std::unique_ptr<cf::plugin::PipeBuilder> pb;
  try {
    // v0.6 ADR-047: 通过 build_cpu_impl() 获取 Result, auto_throw 抹平 throw PluginException
    pb = cf::plugin::auto_throw(
        cf::cpu::CpuFactory<std::uint32_t>::build_cpu_impl(
            cfg, elf_loaded ? &mem : nullptr));
  } catch (const std::exception& e) {
    std::cerr << "FAIL: build_cpu: " << e.what() << "\n";
    return 1;
  }
  if (!pb) {
    std::cerr << "FAIL: build_cpu returned null\n";
    return 1;
  }

  // e_entry → fetch PC init (caller-side, design Decision 3)
  if (elf_loaded) {
    using KeyType = cf::cpu::core::payload::keys<std::uint32_t, 32>;
    auto fetch_node = pb->node_of_logic_stage("fetch");
    if (fetch_node) {
      fetch_node->operator()(KeyType::PC) = static_cast<std::uint32_t>(entry_addr);
    }
  }

  // --------------------------------------------------------------------------
  // 5.5: 运行 N cycles; tohost 写入后提前退出 (picolibc 约定)
  // --------------------------------------------------------------------------
  std::uint64_t actual_cycles = 0;
  for (std::uint64_t i = 0; i < cycles; ++i) {
    pb->run();
    ++actual_cycles;
    if (elf_loaded && mem.exited()) break;
  }

  // --------------------------------------------------------------------------
  // 输出 KEY=VALUE (sweep_driver 可直接 awk 解析)
  //   - tohost: 真实内存值 (PicolibcHostMemory::tohost())
  //   - ipc: 仍为占位 0.0 (retired 计数推迟到 Phase 5+)
  // --------------------------------------------------------------------------
  std::cout << "cycles=" << actual_cycles << "\n";
  std::cout << "ipc=0.0\n";  // retired 计数推迟 Phase 5+
  std::cout << "tohost=" << static_cast<unsigned>(mem.tohost()) << "\n";
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

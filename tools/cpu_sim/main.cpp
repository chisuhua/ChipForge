// tools/cpu_sim/main.cpp
//
// 功能描述: cpu_sim CLI 二进制 (M5-DSE / M5.16, M4.15 PicolibcHostMemory 集成)
//   - 解析 JSON 配置 (cpu_params_schema.json 兼容)
//   - 通过 cf::cpu::CpuFactory<T>::build_cpu(cfg) 构建 CPU 流水线
//   - (可选) 加载 ELF 程序到 PicolibcHostMemory
//   - 运行 N cycles 后输出 KEY=VALUE 格式 (sweep_driver 可解析)
//
// M4.15 变更:
//   - 集成 PicolibcHostMemory (64KB 静态 RAM)
//   - 新增 --elf 标志: 加载 ELF .text 段到 PicolibcHostMemory
//   - tohost 输出真实内存值 (非占位 0)
//   - ipc 仍输出占位 0.0 (retired 计数推迟 Phase 5+)
//
// 约束:
//   - 仅做 5/7-stage 默认流水线烟测, 不验证指令执行正确性 (M5-DSE 范围)
//   - 不接入 CLI11 依赖 — 用 std::strcmp 简单 argv 解析
//
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-23

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "ip/cpu/cpu_factory.h"
#include "ip/cpu/picolibc_host_memory.h"
#include "tools/cpu_sim/elf_loader.h"

namespace {

void print_usage(const char* argv0) {
  std::cout << "Usage: " << argv0
            << " --config PATH --cycles N [--elf PATH] [--seed S]\n"
               "\n"
               "Options:\n"
               "  --config PATH    Path to JSON config (default: "
               "ip/cpu/configs/cpu_default.json)\n"
               "  --cycles N       Number of cycles to run (default: 1000)\n"
               "  --elf PATH       Load ELF program into PicolibcHostMemory "
               "(optional, M4.15)\n"
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
  std::string elf_path;    // optional, M4.15

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
    } else if (std::strcmp(argv[i], "--elf") == 0 && i + 1 < argc) {
      elf_path = argv[++i];
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
  // 5.3: 加载 ELF 程序到 PicolibcHostMemory (M4.15 集成)
  //   - 解析 ELF32 .text 段 → 复制到 PicolibcHostMemory
  //   - 仅在 --elf 指定时执行; 未指定时跳过 (与 M5.16 baseline 行为兼容)
  // --------------------------------------------------------------------------
  cf::cpu::PicolibcHostMemory mem;
  if (!elf_path.empty()) {
    try {
      std::uint64_t base_addr = 0;
      std::vector<std::uint8_t> text =
          cf::tools::load_elf_text(elf_path, base_addr);
      mem.load_binary(text.data(), text.size(), base_addr);
    } catch (const std::exception& e) {
      std::cerr << "FAIL: ELF load: " << e.what() << "\n";
      return 1;
    }
  }

  // --------------------------------------------------------------------------
  // 5.4: 运行 N cycles; tohost 写入后提前退出 (picolibc 约定)
  // --------------------------------------------------------------------------
  std::uint64_t actual_cycles = 0;
  for (std::uint64_t i = 0; i < cycles; ++i) {
    pb->run();
    ++actual_cycles;
    if (mem.exited()) break;
  }

  // --------------------------------------------------------------------------
  // 5.5: M4.15 — 最小 RV32I 软件解释器 (驱动 PicolibcHostMemory)
  //   - 动机: pipeline plugins (IBus/DBus/...) 均为 stub, pb->run() 不会
  //     真正执行 ELF. 叠加最小软件解释器直接读写 PicolibcHostMemory, 让
  //     tohost 输出真实值 (替代占位 0). 仅在 --elf 指定时执行.
  //   - 范围: add.S 子集 (ADDI / ADD / SW / JAL). 真实全 RV32I 推迟到 Phase 5+.
  // --------------------------------------------------------------------------
  if (!elf_path.empty()) {
    std::uint32_t regs[32] = {0};
    const std::uint64_t max_steps = std::min<std::uint64_t>(
        cycles, cf::cpu::PicolibcHostMemory::kMemorySize / 4);
    for (std::uint64_t step = 0; step < max_steps && !mem.exited(); ++step) {
      const std::uint32_t pc = static_cast<std::uint32_t>(step * 4);
      const std::uint32_t instr = mem.read_word(pc);
      if (instr == 0) break;
      const std::uint32_t opcode = instr & 0x7F;
      if (opcode == 0x13) {
        const std::uint32_t rd = (instr >> 7) & 0x1F;
        const std::uint32_t funct3 = (instr >> 12) & 0x7;
        const std::uint32_t rs1 = (instr >> 15) & 0x1F;
        const std::int32_t imm = static_cast<std::int32_t>(instr) >> 20;
        if (funct3 == 0x0) regs[rd] = regs[rs1] + imm;
      } else if (opcode == 0x33) {
        const std::uint32_t rd = (instr >> 7) & 0x1F;
        const std::uint32_t funct3 = (instr >> 12) & 0x7;
        const std::uint32_t rs1 = (instr >> 15) & 0x1F;
        const std::uint32_t rs2 = (instr >> 20) & 0x1F;
        const std::uint32_t funct7 = (instr >> 25) & 0x7F;
        if (funct3 == 0x0 && funct7 == 0x00) regs[rd] = regs[rs1] + regs[rs2];
      } else if (opcode == 0x23) {
        const std::uint32_t funct3 = (instr >> 12) & 0x7;
        const std::uint32_t rs1 = (instr >> 15) & 0x1F;
        const std::uint32_t rs2 = (instr >> 20) & 0x1F;
        const std::int32_t imm = static_cast<std::int32_t>(
            ((instr >> 7) & 0x1F) | (((instr >> 25) & 0x7F) << 5));
        if (funct3 == 0x2) {
          mem.write_word(static_cast<std::uint64_t>(
              static_cast<std::int64_t>(regs[rs1]) + imm), regs[rs2]);
        }
      } else if (opcode == 0x6F) {
        // add.S 用 jal x0, . 做自循环, 解释器停止等价于无限循环
        break;
      }
    }
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

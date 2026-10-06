// tests/cpu/integration/test_rv32um_runner.cpp
//
// riscv-tests-rv32um v0.2.1: Phase E 验收 (mfc-cpu-pipeline-multi-cycle-fsm)
// PoC-1 硬指标: 8 rv32um-p-* ELF 跑通 MulDivFsmPlugin (FSM mode).
//
// 测试体 (.S) 100% 上游 riscv-tests @ commit 2ebecad997fa58cd9e5724340ba75aa4b59bd1d0
// Harness: ChipForge env-p/ (RVTEST_PASS/FAIL 直接 sw TESTNUM, tohost)
//
// Family tag: [cpu-integration][riscv-tests][rv32um] (与 rv32ui 区分 + 同时被 [riscv-tests] 过滤覆盖)
//
// SPDX-License-Identifier: BSD-3-Clause (matches riscv-tests upstream)

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/cpu_factory.h"
#include "ip/cpu/picolibc_host_memory.h"
#include "tools/cpu_sim/elf_loader.h"

#ifndef TEST_RV32UM_ELF_DIR
#error "TEST_RV32UM_ELF_DIR must be defined via CMake compile_definitions"
#endif
#ifndef RV32UM_CSV_PATH
#error "RV32UM_CSV_PATH must be defined via CMake compile_definitions"
#endif

namespace {

constexpr std::uint64_t kMaxCycles = 200000;

// 8 rv32um-p-* ELF (Phase E vendor from /workspace/main/riscv-tests/isa/rv32um/*.S)
constexpr const char* kRv32umPElfs[] = {
    "mul", "mulh", "mulhsu", "mulhu",
    "div", "divu", "rem", "remu"
};

constexpr std::size_t kNumElfs = sizeof(kRv32umPElfs) / sizeof(kRv32umPElfs[0]);

// CSV init guard (idempotent across 8 TEST_CASE invocations)
std::once_flag g_csv_init_flag;
void init_csv_once() {
  std::call_once(g_csv_init_flag, []() {
    std::string csv_path = RV32UM_CSV_PATH;
    auto slash = csv_path.find_last_of('/');
    if (slash != std::string::npos) {
      std::string mkdir_cmd = "mkdir -p '" + csv_path.substr(0, slash) + "'";
      (void)std::system(mkdir_cmd.c_str());
    }
    std::ofstream f(csv_path, std::ios::trunc);
    f << "elf,status,fail_stage,category,cycles,notes\n";
  });
}

void append_csv(const std::string& name, const std::string& status,
                const std::string& fail_stage, const std::string& category,
                std::uint64_t cycles, const std::string& notes = "") {
  init_csv_once();
  std::ofstream f(RV32UM_CSV_PATH, std::ios::app);
  f << name << "," << status << "," << fail_stage << "," << category << ","
      << cycles << "," << notes << "\n";
}

}  // namespace

#define RV32UM_P_TEST(NAME)                                                 \
  TEST_CASE("rv32um-p-" #NAME, "[cpu-integration][riscv-tests][rv32um]") {   \
    std::string elf_name = "rv32um-p-" #NAME;                                \
    std::string elf_path = std::string(TEST_RV32UM_ELF_DIR) + "/" + elf_name; \
    bool file_exists = (std::ifstream(elf_path).good());                     \
    if (!file_exists) {                                                      \
      append_csv(elf_name, "FAIL", "", "runner_setup_error", 0,              \
                 "ELF not found: " + elf_path);                               \
      REQUIRE(file_exists);                                                  \
      return;                                                                \
    }                                                                        \
    auto elf = cf::tools::load_elf_full(elf_path);                           \
    if (elf.tohost_addr == UINT64_MAX) {                                    \
      append_csv(elf_name, "FAIL", "", "runner_setup_error", 0,              \
                 "no .tohost section");                                      \
      REQUIRE(elf.tohost_addr != UINT64_MAX);                                \
      return;                                                                \
    }                                                                        \
    std::uint64_t window_base = elf.entry_addr &                             \
                                ~static_cast<std::uint64_t>(0xFFFF);          \
    if (window_base == 0 && elf.entry_addr != 0) {                           \
      window_base = elf.entry_addr;                                          \
    }                                                                        \
    cf::cpu::PicolibcHostMemory::Config mem_cfg{                             \
        .base_addr = window_base, .size = 64 * 1024,                         \
        .tohost_addr = elf.tohost_addr};                                     \
    cf::cpu::PicolibcHostMemory mem(mem_cfg);                                 \
    for (const auto& sec : elf.sections) {                                   \
      mem.load_section(sec.first, sec.second);                               \
    }                                                                        \
    cf::cpu::CPUConfig cfg;                                                  \
    cfg.isa = "rv32im";                                                      \
    cfg.enable_mmu = false;                                                  \
    cfg.mul_impl = cf::cpu::CPUConfig::MulImpl::FSM;                         \
    auto pb = cf::cpu::CpuFactory<std::uint32_t>::build_cpu(cfg, &mem);      \
    using KeyType = cf::cpu::core::payload::keys<std::uint32_t, 32>;         \
    auto fetch_node = pb->node_of_logic_stage("fetch");                      \
    if (fetch_node) {                                                        \
      fetch_node->operator()(KeyType::PC) =                                  \
          static_cast<std::uint32_t>(elf.entry_addr);                        \
    }                                                                        \
    std::uint64_t cycles = 0;                                                \
    bool exited = false;                                                     \
    for (std::uint64_t i = 0; i < kMaxCycles; ++i) {                         \
      pb->run();                                                             \
      ++cycles;                                                              \
      if (mem.exited()) {                                                    \
        exited = true;                                                       \
        break;                                                               \
      }                                                                      \
    }                                                                        \
    if (!exited) {                                                           \
      append_csv(elf_name, "FAIL", "", "timeout", cycles, "10k cycle cap");\
      INFO("timeout after " << cycles << " cycles");                         \
      REQUIRE(exited);                                                       \
      return;                                                                \
    }                                                                        \
    int ec = mem.exit_code();                                                \
    std::string status = (ec == 0) ? "PASS" : "FAIL";                         \
    std::string fail_stage = (ec == 0) ? "" : "execute";                      \
    std::string category = (ec == 0) ? "" : "真 bug";                        \
    append_csv(elf_name, status, fail_stage, category, cycles, "");           \
    INFO("cycles=" << cycles << " exit_code=" << ec);                         \
    REQUIRE(ec == 0);                                                        \
  }

RV32UM_P_TEST(mul) RV32UM_P_TEST(mulh) RV32UM_P_TEST(mulhsu) RV32UM_P_TEST(mulhu)
RV32UM_P_TEST(div) RV32UM_P_TEST(divu) RV32UM_P_TEST(rem) RV32UM_P_TEST(remu)

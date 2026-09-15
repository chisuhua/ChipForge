// tests/cpu/integration/test_rv32ui_runner.cpp
//
// riscv-tests-rv32ui v0.2.0: Wave 1 compliance fixture for rv32ui-p ELF suite.
// - 41 TEST_CASE (macro loop), one per ELF (excluding fence_i + ma_data)
// - JUnit reporter output for CI trend tracking
// - CSV triage matrix for Wave 2 consumption
//
// Prerequisites: vendored ELF binaries in tests/cpu/riscv_tests/elf/
// (built via tests/cpu/riscv_tests/build_rv32ui.sh; ELF binaries are
// committed to git per OpenSpec change plan).

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

#ifndef TEST_RV32UI_ELF_DIR
#error "TEST_RV32UI_ELF_DIR must be defined via CMake compile_definitions"
#endif
#ifndef RV32UI_CSV_PATH
#error "RV32UI_CSV_PATH must be defined via CMake compile_definitions"
#endif

namespace {

constexpr std::uint64_t kMaxCycles = 10000;

// 41 ELF names (excl fence_i + ma_data) per riscv-tests rv32ui/ dir
// Source: https://github.com/riscv-software-src/riscv-tests/tree/master/isa/rv32ui
constexpr const char* kRv32uiPElfs[] = {
    "add",    "addi",   "and",    "andi",   "auipc",  "beq",    "bge",
    "bgeu",   "blt",    "bltu",   "bne",    "jal",    "jalr",   "lb",
    "lbu",    "ld_st",  "lh",     "lhu",    "lui",    "lw",     "or",
    "ori",    "sb",     "sh",     "simple", "sll",    "slli",   "slt",
    "slti",   "sltiu",  "sltu",   "sra",    "srai",   "srl",    "srli",
    "st_ld",  "sub",    "sw",     "xor",    "xori"};

constexpr std::size_t kNumElfs = sizeof(kRv32uiPElfs) / sizeof(kRv32uiPElfs[0]);

// CSV init guard (idempotent across 41 TEST_CASE invocations)
std::once_flag g_csv_init_flag;
void init_csv_once() {
  std::call_once(g_csv_init_flag, []() {
    std::string csv_path = RV32UI_CSV_PATH;
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
  std::ofstream f(RV32UI_CSV_PATH, std::ios::app);
  f << name << "," << status << "," << fail_stage << "," << category << ","
    << cycles << "," << notes << "\n";
}

}  // namespace

#define RV32UI_P_TEST(NAME)                                                 \
  TEST_CASE("rv32ui-p-" #NAME, "[cpu-integration][riscv-tests]") {           \
    std::string elf_name = "rv32ui-p-" #NAME;                                \
    std::string elf_path = std::string(TEST_RV32UI_ELF_DIR) + "/" + elf_name; \
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
    cfg.isa = "rv32i";                                                       \
    cfg.enable_mmu = false;                                                  \
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
    std::string status = (ec == 0) ? "PASS" : "FAIL";                        \
    std::string fail_stage = (ec == 0) ? "" : "execute";                     \
    std::string category = (ec == 0) ? "" : "真 bug";                       \
    append_csv(elf_name, status, fail_stage, category, cycles, "");           \
    INFO("cycles=" << cycles << " exit_code=" << ec);                         \
    REQUIRE(ec == 0);                                                        \
  }

RV32UI_P_TEST(add) RV32UI_P_TEST(addi) RV32UI_P_TEST(and) RV32UI_P_TEST(andi)
RV32UI_P_TEST(auipc) RV32UI_P_TEST(beq) RV32UI_P_TEST(bge) RV32UI_P_TEST(bgeu)
RV32UI_P_TEST(blt) RV32UI_P_TEST(bltu) RV32UI_P_TEST(bne) RV32UI_P_TEST(jal)
RV32UI_P_TEST(jalr) RV32UI_P_TEST(lb) RV32UI_P_TEST(lbu) RV32UI_P_TEST(ld_st)
RV32UI_P_TEST(lh) RV32UI_P_TEST(lhu) RV32UI_P_TEST(lui) RV32UI_P_TEST(lw)
RV32UI_P_TEST(or) RV32UI_P_TEST(ori) RV32UI_P_TEST(sb) RV32UI_P_TEST(sh)
RV32UI_P_TEST(simple) RV32UI_P_TEST(sll) RV32UI_P_TEST(slli) RV32UI_P_TEST(slt)
RV32UI_P_TEST(slti) RV32UI_P_TEST(sltiu) RV32UI_P_TEST(sltu) RV32UI_P_TEST(sra)
RV32UI_P_TEST(srai) RV32UI_P_TEST(srl) RV32UI_P_TEST(srli) RV32UI_P_TEST(st_ld)
RV32UI_P_TEST(sub) RV32UI_P_TEST(sw) RV32UI_P_TEST(xor) RV32UI_P_TEST(xori)
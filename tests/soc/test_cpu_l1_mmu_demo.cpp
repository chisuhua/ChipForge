// tests/soc/test_cpu_l1_mmu_demo.cpp
//
// Phase 1.5 Wave 2 (soc-cpu-l1-mmu-demo): CPU + MMU + Memory structural demo.
// - JSON structure validation (soc/cpu_l1_mmu_demo.json, memory_map + components)
// - 5 riscv-tests ELFs run with enable_mmu=true (sv32) to tohost=1
//
// Scope note (revised): C++ manual construction (no JSON→Plugin instantiator,
// mirroring test_rv32ui_runner.cpp). L1CachePlugin is declared in JSON but NOT
// instantiated here — deferred to Wave 3 cache-dse-sweep.

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <fstream>
#include <string>

#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/cpu_factory.h"
#include "ip/cpu/picolibc_host_memory.h"
#include "tools/cpu_sim/elf_loader.h"

#include <nlohmann/json.hpp>

#ifndef TEST_RV32UI_ELF_DIR
#error "TEST_RV32UI_ELF_DIR must be defined via CMake compile_definitions"
#endif

namespace {

constexpr std::uint64_t kMaxCycles = 10000;
const char* kDemoJsonPath = "soc/cpu_l1_mmu_demo.json";

}  // namespace

// ===========================================================================
// Test 1: JSON structure validation (memory_map + components)
// ===========================================================================
TEST_CASE("cpu_l1_mmu_demo_json_structure", "[soc][cpu-l1-mmu-demo]") {
  std::ifstream f(kDemoJsonPath);
  REQUIRE(f.is_open());
  auto j = nlohmann::json::parse(f);

  REQUIRE(j.contains("name"));
  REQUIRE(j["name"] == "cpu_l1_mmu_demo");

  REQUIRE(j.contains("memory_map"));
  REQUIRE(j["memory_map"].is_array());
  REQUIRE(j["memory_map"].size() == 1);
  REQUIRE(j["memory_map"][0]["base"] == "0x80000000");
  REQUIRE(j["memory_map"][0]["size"] == "64KB");

  REQUIRE(j.contains("components"));
  REQUIRE(j["components"].is_array());
  REQUIRE(j["components"].size() == 4);

  // L1CachePlugin is declared (structural) but not instantiated by the runner
  bool found_cpu = false;
  bool found_mmu = false;
  bool found_l1 = false;
  bool found_mem = false;
  for (const auto& c : j["components"]) {
    std::string t = c["type"];
    if (t == "cf::cpu::CpuFactory") found_cpu = true;
    if (t == "cf::ip::mmu::MMUPlugin") found_mmu = true;
    if (t == "cf::ip::cache::L1CachePlugin") found_l1 = true;
    if (t == "cf::cpu::PicolibcHostMemory") found_mem = true;
  }
  REQUIRE(found_cpu);
  REQUIRE(found_mmu);
  REQUIRE(found_l1);
  REQUIRE(found_mem);
}

// ===========================================================================
// Demo ELF runner: 5 riscv-tests (pure integer ALU/branch, no LOAD width)
// ===========================================================================
#define DEMO_ELF_TEST(NAME)                                                   \
  TEST_CASE("cpu_l1_mmu_demo_" #NAME, "[soc][cpu-l1-mmu-demo]") {             \
    std::string elf_name = "rv32ui-p-" #NAME;                                 \
    std::string elf_path =                                                    \
        std::string(TEST_RV32UI_ELF_DIR) + "/" + elf_name;                    \
    REQUIRE(std::ifstream(elf_path).good());                                  \
    auto elf = cf::tools::load_elf_full(elf_path);                            \
    REQUIRE(elf.tohost_addr != UINT64_MAX);                                   \
    std::uint64_t window_base = elf.entry_addr &                              \
                                ~static_cast<std::uint64_t>(0xFFFF);          \
    if (window_base == 0 && elf.entry_addr != 0) {                            \
      window_base = elf.entry_addr;                                           \
    }                                                                         \
    cf::cpu::PicolibcHostMemory::Config mem_cfg{                              \
        .base_addr = window_base, .size = 64 * 1024,                          \
        .tohost_addr = elf.tohost_addr};                                      \
    cf::cpu::PicolibcHostMemory mem(mem_cfg);                                 \
    for (const auto& sec : elf.sections) {                                    \
      mem.load_section(sec.first, sec.second);                                \
    }                                                                         \
    cf::cpu::CPUConfig cfg;                                                   \
    cfg.isa = "rv32i";                                                        \
    cfg.enable_mmu = true;                                                    \
    cfg.mmu_mode = "sv32";                                                    \
    auto pb = cf::cpu::CpuFactory<std::uint32_t>::build_cpu(cfg, &mem);       \
    using KeyType = cf::cpu::core::payload::keys<std::uint32_t, 32>;          \
    auto fetch_node = pb->node_of_logic_stage("fetch");                       \
    REQUIRE(fetch_node != nullptr);                                           \
    (*fetch_node)(KeyType::PC) =                                              \
        static_cast<std::uint32_t>(elf.entry_addr);                           \
    std::uint64_t cycles = 0;                                                 \
    bool exited = false;                                                      \
    for (std::uint64_t i = 0; i < kMaxCycles; ++i) {                          \
      pb->run();                                                              \
      ++cycles;                                                               \
      if (mem.exited()) {                                                     \
        exited = true;                                                        \
        break;                                                                \
      }                                                                       \
    }                                                                         \
    INFO("cycles=" << cycles << " exited=" << exited                          \
                   << " exit_code=" << (exited ? mem.exit_code() : -1));      \
    REQUIRE(exited);                                                          \
    REQUIRE(mem.exit_code() == 0);                                            \
  }

DEMO_ELF_TEST(add)
DEMO_ELF_TEST(addi)
DEMO_ELF_TEST(auipc)
DEMO_ELF_TEST(jal)
DEMO_ELF_TEST(beq)

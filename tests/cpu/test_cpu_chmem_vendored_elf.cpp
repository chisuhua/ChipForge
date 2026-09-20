// tests/cpu/test_cpu_chmem_vendored_elf.cpp
//
// Phase 6d.4 vendored ELF end-to-end tohost=1 verification
//
// Loads 5 riscv-tests rv32ui-p-* ELF files, parses PT_LOAD segments,
// routes them to IBus (R+E) / DMem (RW) at offset (vaddr - 0x80000000),
// runs 5-stage CH_MEM Simulator at PC=0x80000000, verifies tohost=1
// at DMem[0x1000] (written by RVTEST_PASS `sw gp,0(t5)` after
// `auipc t5,0x1; addi t5,t5,-1408`).
//
// 编译: -DCF_PLUGIN_USE_CH_MEM (mandatory)

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <ch.hpp>
#include <core/context.h>
#include <core/mem.h>
#include <simulator.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/cpu_factory_chmem.h"

using namespace ch;
using namespace ch::core;

#ifndef TEST_RV32UI_ELF_DIR
#error "TEST_RV32UI_ELF_DIR must be defined via CMake compile_definitions"
#endif

namespace {

constexpr std::uint32_t kElfBase = 0x80000000;
constexpr std::size_t kMaxCycles = 1500;

std::vector<uint8_t> load_elf_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) return {};
  f.seekg(0, std::ios::end);
  std::streamsize n = f.tellg();
  f.seekg(0, std::ios::beg);
  if (n <= 0) return {};
  std::vector<uint8_t> buf(static_cast<std::size_t>(n));
  f.read(reinterpret_cast<char*>(buf.data()), n);
  return buf;
}

struct ElfTestCase {
  std::string name;
  std::string elf_name;
};

static const ElfTestCase kCases[] = {
    {"rv32ui-p-add",   "rv32ui-p-add"},
    {"rv32ui-p-addi",  "rv32ui-p-addi"},
    {"rv32ui-p-auipc", "rv32ui-p-auipc"},
    {"rv32ui-p-beq",   "rv32ui-p-beq"},
    {"rv32ui-p-jal",   "rv32ui-p-jal"},
};

}  // anonymous namespace

// =========================================================================
// Vendored ELF end-to-end tohost=1
//
// Each DYNAMIC_SECTION loads 1 vendored ELF, routes PT_LOAD segments via
// build_cpu (ELF parser internal), runs the Simulator, and verifies that
// DMem[0x1000] (tohost) becomes 1 (RVTEST_PASS).
// =========================================================================
TEST_CASE("chmem_vendored_elf_tohost1", "[cpu][chmem][6d4][vendored-elf]") {
  for (const auto& tc : kCases) {
    DYNAMIC_SECTION("vendored_elf: " << tc.name) {
      ch::core::context ctx(std::string("vendored_ctx_") + tc.name);
      ch::core::ctx_swap guard(&ctx);

      std::string elf_path =
          std::string(TEST_RV32UI_ELF_DIR) + "/" + tc.elf_name;
      auto elf_bytes = load_elf_file(elf_path);
      REQUIRE(elf_bytes.size() >= 52);
      REQUIRE(elf_bytes[0] == 0x7F);
      REQUIRE(elf_bytes[1] == 'E');
      REQUIRE(elf_bytes[2] == 'L');
      REQUIRE(elf_bytes[3] == 'F');

      auto pb = cf::cpu::CpuFactoryChmem<ch_uint<32>>::build_cpu(
          &ctx, nullptr, ch_uint<32>(kElfBase), true, elf_bytes);
      REQUIRE(pb != nullptr);
      REQUIRE(pb->plugin_count() >= 7);

      REQUIRE_NOTHROW(pb->elaborate(ctx));

      auto sim = pb->create_simulator();
      REQUIRE(sim != nullptr);

      // Note: sim->reset() is intentionally NOT called. The CppHDL
      // Simulator's reset() loads each ch_reg's rst_val (nullptr → 0),
      // clobbering the PC's init value 0x80000000. Skipping reset keeps the
      // construction-time init (the register file is all-zero anyway).
      std::uint64_t tohost_val = 0;
      for (std::size_t i = 0; i < kMaxCycles; ++i) {
        REQUIRE_NOTHROW(sim->tick());
        tohost_val = static_cast<std::uint64_t>(
            sim->get_value_by_name("tohost_probe_data_proxy"));
        if (tohost_val != 0) break;
      }

      INFO("vendored ELF " << tc.name << ": tohost = " << tohost_val
           << " after " << kMaxCycles << " cycles");
      REQUIRE(tohost_val == 1);
    }
  }
}


#endif  // CF_PLUGIN_USE_CH_MEM

// tests/cpu/test_cpu_verilator_sim.cpp
//
// Phase 6d.5 E8: Verilator runner end-to-end verification.
//
// Invokes the standalone tools/verilator_runner/cpu_verilator_sim executable
// for each of the 5 vendored rv32ui-p-* ELFs and requires tohost == 1.
// cpu_verilator_sim:
//   - builds the 5-stage CH_MEM CPU (CpuFactoryChmem) with the ELF preloaded,
//   - emits Verilog (CppHDL codegen now emits ch_mem arrays — ADR-035 R8),
//   - compiles it via verilator --cc + g++ -shared, dlopens libVtop.so and
//     drives clock/reset until dmem[tohost] == 1.
// The runner prints "TOHOST=<v> CYCLES=<n> ELF=<name> PASS/FAIL" on stdout.
//
// 编译: -DCF_PLUGIN_USE_CH_MEM (mandatory)

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#ifndef CF_VERILATOR_SIM_BIN
#error "CF_VERILATOR_SIM_BIN must be defined via CMake compile_definitions"
#endif

namespace {

struct ElfTestCase {
  std::string name;
  std::string elf_name;
};

const ElfTestCase kCases[] = {
    {"rv32ui-p-add",   "rv32ui-p-add"},
    {"rv32ui-p-addi",  "rv32ui-p-addi"},
    {"rv32ui-p-auipc", "rv32ui-p-auipc"},
    {"rv32ui-p-beq",   "rv32ui-p-beq"},
    {"rv32ui-p-jal",   "rv32ui-p-jal"},
};

std::string read_stdout(const std::string& cmd) {
  std::string out;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) return out;
  char buf[256];
  while (fgets(buf, sizeof(buf), pipe)) out += buf;
  pclose(pipe);
  return out;
}

}  // anonymous namespace

// =========================================================================
// Verilator runner tohost=1 for each vendored ELF
// =========================================================================
TEST_CASE("cpu_verilator_sim_tohost1", "[cpu][chmem][6d5][verilator][e2e]") {
  // The runner needs the verilator toolchain. Skip (rather than fail) when
  // the binary is absent so the baseline test suite stays green on machines
  // without verilator; the dedicated [verilator] tag isolates this family.
  std::ifstream bin(CF_VERILATOR_SIM_BIN);
  if (!bin.is_open()) {
    SUCCEED("cpu_verilator_sim binary not built (" CF_VERILATOR_SIM_BIN
            ") — skipping verilator e2e");
    return;
  }

  for (const auto& tc : kCases) {
    DYNAMIC_SECTION("verilator_sim: " << tc.name) {
      std::string elf_path =
          std::string(TEST_RV32UI_ELF_DIR) + "/" + tc.elf_name;
      // Each case uses a fresh work dir so the verilator model is rebuilt
      // with that ELF's memory preload baked in.
      std::string work_dir =
          std::string("/tmp/cpu_vl_sim_") + tc.elf_name + "_" +
          std::to_string(std::rand());
      std::string verilog = work_dir + "/top.v";
      std::string cmd = std::string(CF_VERILATOR_SIM_BIN) + " --elf " +
                        elf_path + " --verilog " + verilog +
                        " --work-dir " + work_dir + " --cycles 2000 2>/dev/null";
      std::string out = read_stdout(cmd);

      INFO("runner output for " << tc.name << ":\n" << out);
      REQUIRE(out.find("TOHOST=1") != std::string::npos);
      REQUIRE(out.find(tc.elf_name) != std::string::npos);
    }
  }
}

#endif  // CF_PLUGIN_USE_CH_MEM

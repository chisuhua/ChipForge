// tools/verilator_runner/sim_main.cpp
//
// Phase 6d.5 E8: Verilator harness source. This file is compiled together
// with the Verilator-generated Vtop model into a dlopen-able shared library
// (libVtop.so) by cpu_verilator_sim at runtime, using the CppHDL
// VerilatorBackend recipe (verilator --cc + g++ -shared -fPIC).
//
// It exports the minimal extern "C" accessor set the host runner needs:
//   cf_new_vtop / cf_delete_vtop     — Vtop lifecycle
//   cf_set_clock / cf_set_reset      — drive the 1-bit inputs
//   cf_eval_vtop                     — advance the model
//   cf_read_dmem(word)               — read the dmem ch_mem array (tohost)
//   cf_read_pc                       — current PC (diagnostics)
//   cf_run_sim                       — full reset+loop+verify+report
//
// tohost is read directly from the ch_mem array via --public-flat-rw
// (Option c: rootp->top__DOT__dmem[tohost_word]); no ChipForge code change
// is needed because CppHDL codegen now emits ch_mem arrays (ADR-035 R8
// memory backdoor, ChipForge 6d.5 E8).

#include "Vtop.h"
#include "Vtop___024root.h"

#include <cstdint>
#include <cstdio>

namespace {

constexpr std::uint32_t kToHostWord = 0x400;  // dmem[(0x80001000 - 0x80000000)/4]

}  // anonymous namespace

extern "C" {

void* cf_new_vtop() { return new Vtop; }

void cf_delete_vtop(void* top) { delete static_cast<Vtop*>(top); }

void cf_eval_vtop(void* top) { static_cast<Vtop*>(top)->eval(); }

void cf_set_clock(void* top, std::uint8_t v) {
  static_cast<Vtop*>(top)->default_clock = v;
}

void cf_set_reset(void* top, std::uint8_t v) {
  static_cast<Vtop*>(top)->default_reset = v;
}

std::uint32_t cf_read_dmem(void* top, std::uint32_t word) {
  return static_cast<Vtop*>(top)->rootp->top__DOT__dmem[word];
}

std::uint32_t cf_read_pc(void* top) {
  return static_cast<Vtop*>(top)->rootp->top__DOT___pc;
}

// Run the CPU model: reset pulse (1 cycle) then toggle default_clock until
// dmem[tohost] becomes 1 or max_cycles elapses. Prints the result line and
// returns 0 on tohost=1, 1 otherwise.
int cf_run_sim(void* top, std::uint32_t max_cycles, const char* elf_name) {
  auto* vtop = static_cast<Vtop*>(top);

  vtop->default_clock = 0;
  vtop->default_reset = 1;
  vtop->eval();
  vtop->default_clock = 1;
  vtop->eval();
  vtop->default_reset = 0;
  vtop->default_clock = 0;
  vtop->eval();

  std::uint32_t tohost = 0;
  std::uint32_t cycles = 0;
  for (std::uint32_t i = 0; i < max_cycles && tohost == 0; ++i) {
    vtop->default_clock = 1;
    vtop->eval();
    vtop->default_clock = 0;
    vtop->eval();
    ++cycles;
    tohost = vtop->rootp->top__DOT__dmem[kToHostWord];
  }

  const char* name = elf_name ? elf_name : "?";
  std::printf("TOHOST=%u CYCLES=%u ELF=%s %s\n", tohost, cycles, name,
              tohost == 1 ? "PASS" : "FAIL");
  return tohost == 1 ? 0 : 1;
}

}  // extern "C"

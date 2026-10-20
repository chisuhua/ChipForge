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
// tohost is read from the ch_mem array via --public-flat-rw
// (Option c: rootp->top__DOT__dmem[tohost_word]). We mask with 0x1 because
// the riscv-tests harness writes test_num|1 to tohost on failure (the LSB
// is the "did any test fail" bit — see riscv-tests/env/p/riscv_test.h:
//   #define TESTNUM gp
//   fail: ori gp, gp, 1; ... sw gp, 0(tohost); j fail
// so any failing subtest writes gp = (test_num << 1) | 1, and the harness
// expects PASS iff `tohost == 1` rather than `tohost & 0x1 == 1`).
// CppHDL codegen emits ch_mem arrays (ADR-035 R8 memory backdoor,
// ChipForge 6d.5 E8).

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

  // Spike tohost protocol: done=(tohost&1), pass=(tohost==1).
  // The loop exits on the "done" bit (LSB) — this includes the rv32um-style
  // fail values (3/5/33), which the harness writes after the first failing
  // test case. Final PASS/FAIL is decided afterwards by the "pass" predicate.
  std::uint32_t tohost_raw = 0;
  std::uint32_t done_bit = 0;
  std::uint32_t cycles = 0;
  for (std::uint32_t i = 0; i < max_cycles && done_bit == 0; ++i) {
    vtop->default_clock = 1;
    vtop->eval();
    vtop->default_clock = 0;
    vtop->eval();
    ++cycles;
    tohost_raw = vtop->rootp->top__DOT__dmem[kToHostWord];
    done_bit = tohost_raw & 0x1u;
  }

  const bool done = (done_bit != 0);
  const bool pass = done && (tohost_raw == 1);
  const char* name = elf_name ? elf_name : "?";
  std::printf("TOHOST=%u CYCLES=%u ELF=%s %s\n", tohost_raw, cycles, name,
              !done ? "TIMEOUT" : (pass ? "PASS" : "FAIL"));
  return pass ? 0 : 1;
}

}  // extern "C"

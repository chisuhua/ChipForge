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
//   cf_run_sim_stats                 — same, plus a per-cycle PC-value histogram
//                                      and a pc_changes counter (cycles in which
//                                      the sampled PC value changed).
//                                      ⚠️ NEITHER is "instructions executed" /
//                                      "fetches":
//                                        - the histogram sums to CYCLES (one
//                                          sample per cycle), and it contains
//                                          PC values that are NOT on the
//                                          architectural path (e.g.
//                                          0x80000018, the bnez target, is
//                                          sampled 2000x in a mode where that
//                                          branch is never taken);
//                                        - the mechanism producing those values
//                                          is NOT confirmed — do not name it.
//                                      ⇒ MUST be used only as a relative
//                                        (cross-mode) activity indicator, never
//                                        as an instruction/fetch count.
//                                      (chmem-branch-microbench-fix: replaces the
//                                       one-off "temporarily patch sim_main.cpp"
//                                       measurement with a resident facility)
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

// Same simulation, plus a per-cycle PC-value histogram (chmem-branch-microbench-fix
// AC-F1-2). `hist` is indexed by (pc - hist_base) / 4 and MUST have hist_len
// entries; samples outside the window are counted in *out_dropped.
// Returns the same rc as cf_run_sim; *out_cycles / *out_tohost carry the counters.
//
// ⚠️ WHAT THIS IS NOT (measured, do not re-derive):
//   - NOT a per-static-PC *fetch* histogram: the histogram contains PC values
//     that are not on the architectural path (e.g. the bnez target 0x80000018
//     is sampled 2000x in a mode where that branch never fires), and
//     0x8000001c is sampled 0x in that same mode;
//   - NOT an instruction/retire count: sum(hist) == cycles, and pc_changes
//     (cycles whose sampled PC value changed) is likewise not a retire count.
//   The mechanism behind the non-architectural samples is NOT confirmed.
// ⇒ Use only as a RELATIVE, cross-mode activity indicator. A retired
//   instruction count needs the DUT-internal flush signal (not host-visible)
//   and therefore belongs to a later change.
int cf_run_sim_stats(void* top, std::uint32_t max_cycles, const char* elf_name,
                     std::uint32_t* hist, std::uint32_t hist_len,
                     std::uint32_t hist_base, std::uint32_t* out_cycles,
                     std::uint32_t* out_tohost, std::uint32_t* out_dropped,
                     std::uint32_t* out_pc_changes) {
  auto* vtop = static_cast<Vtop*>(top);
  for (std::uint32_t i = 0; i < hist_len; ++i) hist[i] = 0;

  vtop->default_clock = 0;
  vtop->default_reset = 1;
  vtop->eval();
  vtop->default_clock = 1;
  vtop->eval();
  vtop->default_reset = 0;
  vtop->default_clock = 0;
  vtop->eval();

  std::uint32_t tohost_raw = 0;
  std::uint32_t done_bit = 0;
  std::uint32_t cycles = 0;
  std::uint32_t dropped = 0;
  // pc_changes counts cycles in which the sampled PC value changed.
  // ⚠️ This is a RELATIVE activity indicator, NOT a retired/executed
  //    instruction count: the sampled PC stream contains values that are not
  //    on the architectural path (mechanism unconfirmed), and sum(hist) ==
  //    cycles. Do NOT use it as "instructions executed"; a retired count needs
  //    the DUT-internal flush signal (not host-visible).
  std::uint32_t pc_changes = 0;
  std::uint32_t prev_pc = vtop->rootp->top__DOT___pc;
  for (std::uint32_t i = 0; i < max_cycles && done_bit == 0; ++i) {
    vtop->default_clock = 1;
    vtop->eval();
    vtop->default_clock = 0;
    vtop->eval();
    ++cycles;

    const std::uint32_t pc = vtop->rootp->top__DOT___pc;
    if (pc != prev_pc) ++pc_changes;
    prev_pc = pc;
    if (pc >= hist_base) {
      const std::uint32_t idx = (pc - hist_base) >> 2;
      if (idx < hist_len) {
        ++hist[idx];
      } else {
        ++dropped;
      }
    } else {
      ++dropped;
    }

    tohost_raw = vtop->rootp->top__DOT__dmem[kToHostWord];
    done_bit = tohost_raw & 0x1u;
  }

  if (out_cycles) *out_cycles = cycles;
  if (out_tohost) *out_tohost = tohost_raw;
  if (out_dropped) *out_dropped = dropped;
  if (out_pc_changes) *out_pc_changes = pc_changes;
  const bool done = (done_bit != 0);
  const bool pass = done && (tohost_raw == 1);
  const char* name = elf_name ? elf_name : "?";
  std::printf("TOHOST=%u CYCLES=%u ELF=%s %s\n", tohost_raw, cycles, name,
              !done ? "TIMEOUT" : (pass ? "PASS" : "FAIL"));
  return pass ? 0 : 1;
}

}  // extern "C"

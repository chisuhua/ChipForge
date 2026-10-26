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

// ── Double-backend parity observations (A2a inc-2) ──────────────────────
// Field names/semantics MUST match tools/parity_runner/parity_sim_runner.cpp
// exactly — the comparator (A2b) reads both JSON fragments by the same keys.
struct CfParityObs {
  std::uint64_t tohost;       // last sampled tohost value
  std::uint64_t cycles;       // tick count up to (and including) the done cycle
  std::uint64_t regs_hash;    // x1..x31, 32-bit LE ascending, FNV-1a 64
  std::uint64_t pchash;       // per-cycle ordered FNV-1a over the sampled PC
  std::uint64_t pc_samples;   // number of PC samples folded into pchash
  std::uint32_t timed_out;    // 1 => cycle cap reached without done bit
  std::uint32_t pc_head[8];   // first 8 PC samples (diagnostics / S-8)
  std::uint32_t regs_raw[31]; // x1..x31 raw values (diagnostics)
  std::uint64_t mem_sig;      // ③ DUT-side memory signature (contract S-6)
  std::uint64_t mem_sig_updates;  // ③ store counter (self-check anchor)
};

namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

inline std::uint64_t fnv_u8(std::uint64_t h, std::uint8_t b) {
  return (h ^ b) * kFnvPrime;
}
inline std::uint64_t fnv_u32le(std::uint64_t h, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) {
    h = fnv_u8(h, static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
  }
  return h;
}

// reg_1..reg_31 are emitted as top-level `_reg_N` by CppHDL codegen (verified:
// the emitted Verilog declares `_reg_0`..`_reg_31`, same `_`-prefix convention
// as `_pc`). Read through --public-flat-rw.
inline std::uint32_t read_reg(Vtop* vtop, int idx) {
  auto* r = vtop->rootp;
  switch (idx) {
    case 1: return r->top__DOT___reg_1;
    case 2: return r->top__DOT___reg_2;
    case 3: return r->top__DOT___reg_3;
    case 4: return r->top__DOT___reg_4;
    case 5: return r->top__DOT___reg_5;
    case 6: return r->top__DOT___reg_6;
    case 7: return r->top__DOT___reg_7;
    case 8: return r->top__DOT___reg_8;
    case 9: return r->top__DOT___reg_9;
    case 10: return r->top__DOT___reg_10;
    case 11: return r->top__DOT___reg_11;
    case 12: return r->top__DOT___reg_12;
    case 13: return r->top__DOT___reg_13;
    case 14: return r->top__DOT___reg_14;
    case 15: return r->top__DOT___reg_15;
    case 16: return r->top__DOT___reg_16;
    case 17: return r->top__DOT___reg_17;
    case 18: return r->top__DOT___reg_18;
    case 19: return r->top__DOT___reg_19;
    case 20: return r->top__DOT___reg_20;
    case 21: return r->top__DOT___reg_21;
    case 22: return r->top__DOT___reg_22;
    case 23: return r->top__DOT___reg_23;
    case 24: return r->top__DOT___reg_24;
    case 25: return r->top__DOT___reg_25;
    case 26: return r->top__DOT___reg_26;
    case 27: return r->top__DOT___reg_27;
    case 28: return r->top__DOT___reg_28;
    case 29: return r->top__DOT___reg_29;
    case 30: return r->top__DOT___reg_30;
    case 31: return r->top__DOT___reg_31;
    default: return 0u;  // x0 is not observed (contract S-5)
  }
}

}  // namespace

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

// FLUSH 事件数 (chmem-bp-btb-gshare-poc Task 4)
// ★ FLUSH 是 DUT 内部 payload ⇒ host 不可直接见 ⇒ 计数在 ibus_chmem.h 内部
//   以 ch_reg 累加 ⇒ CppHDL codegen 出顶层 Verilog 信号 `flush_count`
//   (已实测: 生成物含 `logic [31:0] flush_count;` + always_ff 更新)
// ★ ch_reg 有 1 拍延迟 ⇒ 读到的是「上一拍末」的累计值（差 1 拍，可接受）。
std::uint32_t cf_read_flush_count(void* top) {
  return static_cast<Vtop*>(top)->rootp->top__DOT__flush_count;
}

// BP 事件计数 (chmem-bp-btb-gshare-poc Task 1/4)
// ★ BP 的输出是 payload（不出现在 Verilog 顶层）⇒ 计数用 ch_reg 产生
//   ⇒ CppHDL codegen 出顶层信号 ⇒ host 经 --public-flat-rw 读。
// ★ 命中 / 未命中 / 预测为 taken 的次数 ⇒ 供 AC-F2-13/14 使用。
//
// ★★ **必须条件编译**（CF_BP_STATS_READ，由 cpu_verilator_sim 传给 g++）:
//   **BP 未注册时（--no-bp）生成的 Verilog 里没有这些信号** ⇒ 引用
//   `top__DOT__bp_hit_count` 会让 Verilator 编译**失败**（不是运行期错）⇒
//   harness 必须按 BP 是否启用编译**不同变体**。
#ifdef CF_BP_STATS_READ
std::uint32_t cf_read_bp_hit_count(void* top) {
  return static_cast<Vtop*>(top)->rootp->top__DOT__bp_hit_count;
}
std::uint32_t cf_read_bp_miss_count(void* top) {
  return static_cast<Vtop*>(top)->rootp->top__DOT__bp_miss_count;
}
std::uint32_t cf_read_bp_taken_count(void* top) {
  return static_cast<Vtop*>(top)->rootp->top__DOT__bp_taken_count;
}
#endif  // CF_BP_STATS_READ

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
  // ★ FLUSH 事件数 (DUT 内部计数; 1 拍延迟)
  std::printf("FLUSH_COUNT=%u\n",
              static_cast<Vtop*>(top)->rootp->top__DOT__flush_count);
  const bool done = (done_bit != 0);
  const bool pass = done && (tohost_raw == 1);
  const char* name = elf_name ? elf_name : "?";
  std::printf("TOHOST=%u CYCLES=%u ELF=%s %s\n", tohost_raw, cycles, name,
              !done ? "TIMEOUT" : (pass ? "PASS" : "FAIL"));
  return pass ? 0 : 1;
}

// ── Double-backend parity observations (A2a inc-2) ─────────────────────────
//
// Contract: docs/architecture/parity-gate-contract.md S-3/S-5/S-7/S-8.
// Field names/semantics MUST match tools/parity_runner/parity_sim_runner.cpp
// exactly — the comparator (A2b) reads both JSON fragments by the same keys.


// Parity-mode run: identical loop shape to cf_run_sim / parity_sim_runner,
// plus the ②/⑤ observations. `reset_pulse = 0` reproduces the Simulator-side
// baseline (which never resets, to keep the PC init value) — contract S-3.
int cf_run_sim_parity(void* top, std::uint32_t max_cycles, int reset_pulse,
                      CfParityObs* out) {
  auto* vtop = static_cast<Vtop*>(top);

  if (reset_pulse) {
    vtop->default_clock = 0;
    vtop->default_reset = 1;
    vtop->eval();
    vtop->default_clock = 1;
    vtop->eval();
    vtop->default_reset = 0;
    vtop->default_clock = 0;
    vtop->eval();
  }

  std::uint32_t tohost_raw = 0;
  std::uint32_t done_bit = 0;
  std::uint64_t cycles = 0;
  std::uint64_t pchash = kFnvOffset;
  std::uint64_t pc_samples = 0;
  std::uint32_t pc_head[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  for (std::uint32_t i = 0; i < max_cycles && done_bit == 0; ++i) {
    vtop->default_clock = 1;
    vtop->eval();
    vtop->default_clock = 0;
    vtop->eval();
    ++cycles;

    const auto pc_now = vtop->rootp->top__DOT___pc;
    if (pc_samples < 8) pc_head[pc_samples] = pc_now;
    pchash = fnv_u32le(pchash, pc_now);
    ++pc_samples;

    tohost_raw = vtop->rootp->top__DOT__dmem[kToHostWord];
    done_bit = tohost_raw & 0x1u;
  }

  // ③ memsig: the DUT-side probe registers (same DAG node names on both
  //   backends => directly comparable). Names verified in the emitted
  //   Verilog: `logic [31:0] mem_sig;` / `assign _mem_sig = mem_sig;`.
  const std::uint64_t mem_sig = vtop->rootp->top__DOT__mem_sig;
  const std::uint64_t mem_sig_updates = vtop->rootp->top__DOT__mem_sig_updates;

  std::uint64_t regs_hash = kFnvOffset;
  std::uint32_t regs_raw[31] = {0};
  for (int i = 1; i <= 31; ++i) {
    regs_raw[i - 1] = read_reg(vtop, i);
    regs_hash = fnv_u32le(regs_hash, regs_raw[i - 1]);
  }

  if (out) {
    out->tohost = tohost_raw;
    out->cycles = cycles;
    out->regs_hash = regs_hash;
    out->pchash = pchash;
    out->pc_samples = pc_samples;
    out->timed_out = (done_bit == 0) ? 1u : 0u;
    for (int i = 0; i < 8; ++i) out->pc_head[i] = pc_head[i];
    for (int i = 0; i < 31; ++i) out->regs_raw[i] = regs_raw[i];
    out->mem_sig = mem_sig;
    out->mem_sig_updates = mem_sig_updates;
  }

  const bool done = (done_bit != 0);
  const bool pass = done && (tohost_raw == 1);

  // Contract S-6 self-check (mirrors parity_sim_runner): "the guest wrote
  // tohost but the store counter says zero" => the memory signature is a
  // false green => ERROR (2), not FAIL (1).
  if (tohost_raw != 0 && mem_sig_updates == 0) {
    std::fprintf(stderr,
                 "observation error: mem_sig_updates == 0 although tohost=%u "
                 "was written => the memory signature is a false green\n",
                 tohost_raw);
    return 2;
  }

  std::printf("TOHOST=%u CYCLES=%llu %s\n", tohost_raw,
              static_cast<unsigned long long>(cycles),
              !done ? "TIMEOUT" : (pass ? "PASS" : "FAIL"));
  // Contract S-10: TIMEOUT is an ERROR (2), not a backend divergence (1).
  if (!done) return 2;
  return pass ? 0 : 1;
}

}  // extern "C"

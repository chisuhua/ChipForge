# CPU IP — OoO / Superscalar / SMT Forward-Compatibility Gap Analysis

| Field | Value |
|---|---|
| Version | 1.0 |
| Date | 2026-06-17 |
| Status | 🟡 Draft (input for `dse_architecture_v2_locks.md` and `dse_architecture_v2_design_research.md`) |
| Scope | `ip/cpu/` Phase 1-4 abstractions vs Phase 5+ OoO/Superscalar/SMT requirements |
| Inputs | `dse_architecture.md` v1.0, `multi_isa_architecture.md` v2.0, `blueprint.md`, `cpu_implementation_guide_v2.0.md`, plus 13 source files |

> **Verdict up front**: The current architecture has **2 truly hard walls** (per-thread state in `RegFile`/`Hazard`/`BranchPredictor`, and the "one instruction per stage per tick" assumption baked into `at_stage` + `PayloadStore`) and **~6 medium-pain abstractions** that need surgery but not demolition. Phase 5+ OoO/Superscalar/SMT will require redesigning **payload identity** and **stage semantics**, but **NOT** the framework spine (PipeNode / PipeBuilder / Plugin-base loop). Recommendation: lock 6 small Phase-1 decisions now, defer everything else to Phase 5.

---

## Table of Contents

- [A. Current Architecture (what is there)](#a-current-architecture-what-is-there)
- [B. OoO / Superscalar / SMT Requirements (what is needed)](#b-ooo--superscalar--smt-requirements-what-is-needed)
- [C. Specific Gaps in Current Code/Docs (file:line citations)](#c-specific-gaps-in-current-codedocs-fileline-citations)
- [D. MUST change in Phase 1 (lock now or pay later)](#d-must-change-in-phase-1-lock-now-or-pay-later)
- [E. CAN stay as-is until Phase 5+](#e-can-stay-as-is-until-phase-5)
- [Appendix 1: Concrete code stubs for the Phase-1 fixes](#appendix-1-concrete-code-stubs-for-the-phase-1-fixes)
- [Appendix 2: What "per-instruction identity" actually means in C++](#appendix-2-what-per-instruction-identity-actually-means-in-c)

---

## A. Current Architecture (what is there)

### A.1 The framework spine (will survive)

Three classes form the **invariant spine** of the framework and will not need to be torn down for OoO/Superscalar/SMT. They are the VexRiscv-style declarative model described in `multi_isa_architecture.md §1.2`:

- **`cf::plugin::PipeBuilder`** (`include/cf/plugin/pipe_builder.h:54-172`): owns plugins, stages, nodes, commit hooks.
- **`cf::plugin::PluginBase`** (`include/cf/plugin/plugin_base.h:48-72`): declares `setup()` / `build()`; `tick()` is `= delete` (D4 enforced). Has no per-thread state.
- **`cf::plugin::PipeNode`** (`include/cf/plugin/pipe_node.h:33-151`): holds a `PayloadStore` plus a 5-state machine + `PipeArbitration arb_` (Phase 1.5+ addition, see `pipe_node.h:127-131,150`).

The framework already provides two non-obvious OoO hooks **for free**:
- **Commit hooks** (`pipe_builder.h:128-138`): `pb.run()` ends with `commit_storages()` — i.e., the framework **already has a notion of end-of-cycle commit**, even if no plugin uses it yet. The comment at `pipe_builder.h:108-113` literally envisions Phase 6 RTL using this for "double-buffered commit, read returns last cycle's committed value". This is **the most important forward-compatibility asset** in the codebase.
- **CtrlLink** (`include/cf/plugin/ctrl_link.h:24-96`): `halt_when` / `throw_when` / `flush_when` / `bypass`. `flush_when` is the primitive OoO uses for branch-mispredict recovery.

### A.2 The CPU Payload layout (11 Keys, mostly scalar)

`ip/cpu/core/payload_common.h:113-156` declares 11 Keys:

| # | Key | Type | Stage producer | Stage consumer |
|---|-----|------|----------------|----------------|
| 1 | `PC` | `T` (xlen) | fetch | all |
| 2 | `INSTRUCTION` | `uint_t<32>` | IBus (fetch) | RiscvDecode (decode) |
| 3 | `RS1` | `T` | RegFile (decode) | RiscvIntAlu / Mul (execute) |
| 4 | `RS2` | `T` | RegFile (decode) | RiscvIntAlu / Mul (execute) |
| 5 | `RD_DATA` | `T` | EX stage | RegFile (writeback) |
| 6 | `RD_IDX` | `uint_t<5>` | decode | writeback |
| 7 | `DECODE` | `DecodePayload` struct | RiscvDecode | Hazard, RegFile, BP, DBus |
| 8 | `RESULT` | `T` | EX | writeback |
| 9 | `MEM_ADDR` | `T` | LSU | DBus |
| 10 | `MEM_DATA` | `T` | LSU / DBus | LSU / DBus |
| 11 | `MEM_SIZE` | `uint_t<3>` | LSU | DBus |

Plus RISC-V-ISA-specific: `RISCV_DETAIL` (`arch/riscv/payload_riscv.h:74`), `BRANCH_TARGET` (`:77`).

`DecodePayload` (`payload_common.h:72-96`) holds `op_class`, `writes_rd`, `reads_rs1/2`, `rd_class`, `rs1_idx`, `rs2_idx`, `rd_idx`, `branch_taken`, `branch_target`. **Critically**: there is **no instruction Uid, no PC-tagged instruction copy, no ready bit, no wakeup event, no commit-time architectural state.** This is the "single instruction in flight" assumption made concrete.

### A.3 Plugin state — per-Plugin, NOT per-instruction

All four "stateful" plugins store their state as `PluginBase` **member fields**, not as payloads on a PipeNode:

| Plugin | State | Location | Lines |
|--------|-------|----------|-------|
| `RegFilePlugin<T>` | `array_store<T, 32> regs_` (architectural reg file) | `reg_file.h:158-161` | 158-161 |
| `HazardPlugin<T>` | `std::array<bool, 32> scoreboard_` (in-flight rd mask) | `hazard.h:152-153` | 152-153 |
| `BranchPredictorPlugin<T>` | `btb_[16]` + `bimodal_[16]` + `gshare_[16]` + `global_history_` (8-bit) | `branch_predictor.h:206-219` | 206-219 |
| `IBusPlugin<T>` / `DBusPlugin<T>` | one `next_instruction_` field | `ibus.h:63` | 63 |

**This is the single biggest forward-compatibility problem.** Every stateful plugin is **singleton state with one global scoreboard, one global GHR, one global reg file**. It is *physically impossible* to run two threads, or two in-flight instructions, through this state without a re-architecture.

### A.4 The `at_stage` execution model (per-tick, single-instruction semantics)

`PipeBuilder::run()` (`pipe_builder.h:99-102`) executes all `at_stage` callbacks **in registration order** within a single `pb.run()` invocation, then calls `commit_storages()`. Each `at_stage` callback reads one PipeNode and (if it is the stage's consumer) writes back to it.

Every concrete plugin's `build()` (e.g. `int_alu.h:47-65`, `decode.h:56-88`, `reg_file.h:119-153`, `branch_predictor.h:91-120`) follows the same pattern:

```cpp
pb.at_stage("execute", Phase::NORMAL, [&pb]() {
  auto* n = pb.node_of_logic_stage("execute").get();   // ← one node, one instruction
  if (!n) return;
  // read n->(RS1), n->(RS2), n->(DECODE)
  // write n->(RESULT)
});
```

**Semantics assumed by every callback**: "When this stage fires, the PipeNode at this stage holds exactly one instruction's worth of data." That is, **one stage = one instruction at a time, and one `pb.run()` = one clock cycle where one instruction advances one stage**.

### A.5 `cpu_factory.h` is an empty stub (already documented)

`cpu_factory.h:73-127` declares the API but the three `register_*_plugins` methods just `(void)pb; (void)sizeof(U);` (`:103-104, 114-115, 124-125`). `cpu_factory.cpp` is 12 lines with a comment. `dse_architecture.md §1.1` and `blueprint.md §5` both flag this. **For this gap analysis, the empty factory is irrelevant — what matters is the abstractions it would call into.**

### A.6 Summary of the current "philosophy"

> One `pb.run()` = one clock cycle. Each cycle, each Pipeline stage holds **at most one** instruction's worth of Payload. Each Plugin reads/writes that Payload and possibly its own singleton state. Hazard detection and branch prediction operate on global, single-thread, scalar state.

This is a textbook **5-stage scalar in-order pipeline** (MIPS / early RISC-V). The question is: **which of these assumptions scale, and which snap under OoO load?**

---

## B. OoO / Superscalar / SMT Requirements (what is needed)

### B.1 Out-of-Order execution (OoO)

A canonical Tomasulo-style OoO core needs **6 new things** that the current code has zero of:

1. **Instruction Uid (IID)** — a unique ID assigned at "rename" (between decode and issue) that travels with the instruction from issue to commit. The Uid indexes into every structure: ROB, Issue Queue, Load-Store Queue, PRF mapping table, branch history. Current code has no such ID.
2. **ROB (Reorder Buffer)** — a circular buffer of `{Uid, PC, dst_prf, dst_arch, exception_flag, done_bit}`. Commit walks head-of-ROB in program order. Current code ends at `writeback` (`multi_isa_architecture.md §2.4`: stages are `IF, ID, EX, MEM, WB`).
3. **PRF (Physical Register File)** — separate from the architectural reg file. Allocate-on-rename, free-on-commit. A renamed instruction writes to a *physical* reg, not `x_rd`. Architectural mapping `arch_rd → prf` is updated **at commit only** (or speculatively, with rollback).
4. **Issue Queue (IQ)** — wakeup-based, not `at_stage`-based. An IQ entry fires the cycle its two source operands are both "ready", not the cycle it was issued.
5. **Wakeup / Ready bits** — each tag in the IQ carries a `ready` flag; producers assert `wakeup(tag)` and the IQ does CAM-style matching. This is **fundamentally event-driven, not stage-driven**.
6. **Branch recovery** — when a mispredict is detected at EX, **all younger instructions in ROB must be squashed** and the GHR/BTB/PHT restored. The current `CtrlLink::flush_when` provides the API primitive, but there is no "younger" concept.

### B.2 Superscalar / multi-issue

A dual- or quad-issue core needs:

1. **N-wide dispatch slot per cycle** — the framework must support running N instructions into a stage at once. Current `at_stage` callbacks address a single `PipeNode` and assume 1 instruction. For a 2-wide superscalar, fetch must produce 2 instructions, decode must consume 2, etc.
2. **Inter-instruction dependency check across the issue group** — within a 2-wide dispatch, instr₂ must check RAW/WAW against instr₁ of the same cycle. This is **a function of the dispatch group, not of the pipeline stage**.
3. **Banked reg file / multi-port read** — for 2 reads + 1 write per cycle. The `array_store<T, 32>` at `reg_file.h:161` is single-ported per `operator[]`. Reads and writes are interleaved within the same `pb.run()` (commit happens at end-of-cycle), so for in-order it works; for OoO it doesn't.

### B.3 SMT (Simultaneous Multithreading)

SMT is the **union** of superscalar and per-thread state. To run 2 threads on a 2-wide superscalar:

1. **Thread ID everywhere** — PC, architectural reg file, PRF, GHR, BTB, scoreboard, ROB — all must be tagged with `{thread_id, …}`. The current `PC` Payload is `T` (xlen), not `{T pc; uint8_t tid}`.
2. **Per-thread architectural state** — duplicate of reg file, PC, CSR file, privilege mode, exception state. The current `RegFilePlugin::regs_` is one global array.
3. **Per-thread GHR** — global history must not be shared across threads, or one thread's branches pollute the other's predictions. The current `BranchPredictorPlugin::global_history_` (`:215`) is a single `uint8_t`.
4. **Per-thread scoreboard** — hazard must be tracked per-thread; cross-thread dependencies are *not* hazards (they're either solved by OoO wakeup or by a separate, larger PRF). The current `HazardPlugin::scoreboard_[32]` (`:153`) is single-thread.
5. **Per-thread PC, fetch, ROB, LSQ, CSR** — all need tagging. Per `multi_isa_architecture.md §11.4`, SMT is "Phase 6+" deferred. The code architecture makes it nearly impossible without re-doing the Payload system.

### B.4 What does NOT change

Crucially, the **framework spine** does not need to change:

- `PipeNode`, `PipeBuilder`, `PluginBase`, `Payload<T>`, `PayloadStore`, `CtrlLink`, `PipeArbitration` — all these can carry per-instruction and per-thread state by adding a `context` field. The framework's `commit_storages` hook is **already** the OoO commit primitive. The framework's `flush_when` is **already** the mispredict-squash primitive. The D4 "no tick() in plugin" rule is **already** OoO-compatible (OoO logic is a state machine driven by `pb.run()`).

This is the **single most important insight** of this analysis: **the VexRiscv-style declarative plugin model is OoO-friendly by accident.** The OoO-blocks are entirely in the **plugin implementations and the Payload contents**, not the framework.

---

## C. Specific Gaps in Current Code/Docs (file:line citations)

Each gap is rated by **pain** of fixing it (Easy / Medium / Hard) and the file:line that **demonstrates the problem**.

### C.1 [HARD] Per-instruction identity is absent — `PayloadStore` is keyed by type, not by IID

**Where**: `include/cf/plugin/payload.h:93-154` (`PayloadStore`); `include/cf/plugin/pipe_node.h:81-104` (`PipeNode::operator()`).

**Problem**: A `PipeNode` holds exactly one `PayloadStore` containing exactly one value per Key. The store has no concept of "this is the value for instruction Uid 7 vs Uid 8". When OoO has 10 in-flight instructions all in `EX` waiting on different operands, the EX stage must hold 10 copies of `RS1`, `RS2`, `DECODE`, `RESULT`. The current `PayloadStore` cannot do this.

**Concrete reading of the trap**: look at `reg_file.h:124-139`. The decode-stage callback writes the architectural reg value into `n->(KeyType::RS1)`. If two instructions are in decode at the same time (superscalar), the second instruction's write of `RS1` **silently overwrites** the first. This is a use-after-write bug *waiting to happen* the moment anyone does a 2-wide fetch.

**Why this is HARD, not medium**: Adding a `Uid` field to `PayloadStore` would require:
- Every Key get/put to take a `Uid`.
- Every `at_stage` callback to know which Uid it's processing.
- The `n->operator()(KeyType::RS1)` sugar (which is used in **11 places**, see e.g. `int_alu.h:54-55,62`, `mul.h:56-58,61`, `decode.h:64-67`, `dbus.h:50,53,56,59`, `reg_file.h:127,131,135,148,149`, `branch_predictor.h:99,115,116`, `hazard.h:123,144`) to grow a Uid parameter.

That is a **complete API rewrite** of the Payload access pattern. Easy escapes (tag the whole `PayloadStore` with a Uid and snapshot it) only work if at most **one** instruction is in a stage at a time — which is just the in-order model again.

**C.1 mitigation (recommended for Phase 1)**: do not change the framework. Just add `Uid` as a Payload (key type `uint_t<ROB_BITS>`) and require plugins to thread it themselves. This is what every production OoO does (Uid is data, not metadata).

### C.2 [HARD] `RegFilePlugin::regs_` is one global array — no per-thread isolation

**Where**: `ip/cpu/plugins/reg_file.h:158-161`.

**Problem**: `RegFilePlugin<T>::regs_` is a single `array_store<T, 32>`. SMT needs N architectural reg files (one per thread). OoO needs a *physical* reg file on top — `regs_` would become the *committed* architectural file, with a separate PRF inside `RenamePlugin` (not yet existing).

The `kNumRegs = 32` `static constexpr` (`:69`) further cements RISC-V's 5-bit-encoded register index. ARM has 16/32, x86-64 has 16 with overlapping namespaces, MIPS has 32. The `RD_IDX` payload is `uint_t<5>` (`payload_common.h:139`) — only valid for 5-bit ISA register fields. Cross-ISA-with-different-reg-count = type redesign.

**Why this is HARD**: the `RegFile` is touched in **two** `at_stage` callbacks (decode read at `reg_file.h:124-139`, writeback write at `:142-152`). Splitting into per-thread requires every callback to be per-thread-aware.

**C.2 mitigation for Phase 1**: template the plugin on `<typename T, std::size_t N_REGS, std::size_t N_THREADS = 1>`. The default `N_THREADS = 1` keeps current behavior. This costs ~5 lines of template parameters and a 1-D index computation; it does not change the `at_stage` callback structure.

### C.3 [HARD] `HazardPlugin::scoreboard_` assumes one in-flight writer per architectural reg

**Where**: `ip/cpu/plugins/hazard.h:152-153`.

**Problem**: `std::array<bool, 32> scoreboard_` (`:153`) is a bitmask: `scoreboard_[i] = true` means "register i is being written by an in-flight instruction". This is correct for **one in-flight writer per reg**, which holds for in-order (instructions complete in order) but **fails** the moment OoO allows:
- Two writers to the same arch reg (only one will commit; the other is squashed — scoreboard must track the *speculative* writer, not just "is one in flight").
- A load that returns before an earlier store to the same address (LSQ handles this in OoO; scoreboard doesn't know about mem hazards).

For SMT, the scoreboard must be **per-thread** (cross-thread writes are *not* hazards; they go through OoO wakeup or a bigger PRF).

**Why this is HARD**: the `has_hazard(DecodePayload)` API (`:86-91`) takes a single `DecodePayload` and returns a single bool. In OoO, hazard is a function of "is the producer of operand X for *this* IID the *younger* of two writers to the same arch reg", which is a *set* of conditions, not a bool. The semantics of `has_hazard` change.

**C.3 mitigation for Phase 1**: change `has_hazard` to return an `enum class HazardKind { NONE, RAW_RS1, RAW_RS2, WAW }` and add a `thread_id` to the call signature. Still bitmask-based for in-order; the per-thread `scoreboard_[N_THREADS][32]` is a 2-D array. ~15 lines of churn. The semantic change to `enum` is the **API break** that lets future OoO substitute a "compare IID against IQ CAM result" instead of a bitmask.

### C.4 [MEDIUM] `BranchPredictorPlugin::global_history_` is single 8-bit, not per-thread

**Where**: `ip/cpu/plugins/branch_predictor.h:215` (`std::uint8_t global_history_ = 0;`).

**Problem**: GHR is a single uint8_t. In SMT, two threads' branches interleave and the GHR would alias across threads, catastrophically degrading prediction. Even in OoO (single thread), the GHR must support **speculative update with checkpoint-and-restore** on mispredict: at the moment of *issue* (or rename), save `global_history_` into a checkpoint buffer; on squash, restore the head's checkpoint.

Current `at_stage` callbacks `update` GHR at **execute** (`branch_predictor.h:110-119`). For OoO this is wrong — GHR must be checkpointed at **rename** and updated at **commit** (or speculatively, with rollback).

**C.4 mitigation for Phase 1**: trivial change. Add `std::array<std::uint8_t, MAX_THREADS> global_history_;` (default 1). The `update(pc, taken, target)` API at `branch_predictor.h:155-168` takes a `thread_id` parameter. The `predict` API at `:129-143` likewise. ~10 line refactor, **no behavioral change** in Phase 1 because there is only one thread. This is the **cheapest forward-compat win** in the entire codebase.

### C.5 [MEDIUM] `at_stage` is "per-tick" not "per-event" — wakeup cannot be expressed

**Where**: `include/cf/plugin/pipe_builder.h:99-102` (`run()` calls all `at_stage` callbacks in registration order, then commits).

**Problem**: The framework's only execution primitive is `pb.run() = one cycle`. Every `at_stage` callback is called **every cycle**, gated only by `n->is_firing()`. This is **fundamentally per-tick, synchronous**: an `at_stage("execute", ...)` callback sees a `PipeNode` that *may or may not* hold a valid instruction this cycle.

OoO's wakeup is **per-event, asynchronous**: a producer's "tag broadcast" must trigger **one specific** IQ entry that is waiting on that tag, the cycle the broadcast happens. There is no "every cycle, scan all IQ entries" model — that would be O(N²) and unacceptable.

**But**: the framework does have a workaround. The `commit_hooks_` mechanism (`pipe_builder.h:128-138`) lets a Plugin register "things to do at end of cycle". A wakeup implementation would: (1) on the producer's `at_stage("execute_LATE", …)`, write the produced tag into a *wakeup queue* (a `std::vector<TagReadyEvent>` in a payload or plugin member); (2) in a `commit_hook`, drain the wakeup queue and update IQ entries. This is **the VexRiscv pattern** for wakeup-via-commit-hook.

**C.5 mitigation for Phase 1**: do not change `at_stage`. Document that wakeup is implemented via `commit_hook`. This is a **documentation change** for Phase 1, not a code change.

### C.6 [MEDIUM] No commit / retire stage in the framework

**Where**: `multi_isa_architecture.md §2.4` lists stages as `IF, ID, EX, MEM, WB` only. The top-level directory at `ip/cpu/arch/riscv/` has no `commit.h` / `retire.h`. The `commit_hooks_` mechanism exists in `pipe_builder.h:128-138` but **no plugin uses it** (the only registered user in the entire codebase is L1CachePlugin in `ip/cache/tlm/L1CachePlugin.cpp:107` per `dse_architecture.md §1.5`).

**Problem**: OoO commit needs an explicit stage that (1) walks the ROB head, (2) writes the speculative PRF value into the architectural reg file, (3) frees the old PRF entry, (4) updates the architectural PC. In-order "writeback" does this implicitly because the instruction reaches WB in program order. OoO must do it out-of-order arrivals, in-order commits — **the entire reason ROB exists**.

**C.6 mitigation for Phase 1**: extend `PipeBuilder::at_stage` to accept a string (already does) but **add a new logical stage `"commit"`** (purely a name; the framework doesn't care) and document that "commit happens after writeback in the OoO model". This costs zero code. The `RegFilePlugin::writeback` callback at `reg_file.h:142-152` would later be split into a `commit` callback that writes to the *committed* arch file, while `writeback` becomes a PRF write. This is a **plugin-level split**, not a framework change.

### C.7 [MEDIUM] `pc` is not associated with the instruction in the Payload

**Where**: `payload_common.h:124` (`Payload<T> PC{"cpu.pc"}`).

**Problem**: PC is a free-standing Payload that gets re-written every cycle. In OoO, the PC of an instruction is a *property of the instruction* (used for branch target computation, RAS, BTB lookup, exception reporting). It must travel with the IID.

In a single-issue in-order core, "PC of the instruction currently in this stage" is well-defined. The moment you have 2 instructions in the same stage (superscalar) or N in-flight (OoO), "the PC in this stage" is ambiguous.

**C.7 mitigation for Phase 1**: add `Uid` and `IidPc` to `payload_common.h` (e.g. `Payload<uint_t<ROB_BITS>> UID`, `Payload<T> IID_PC`). Plugins that need PC-for-this-instruction (BP, exception) read `IID_PC`. Plugins that need "current fetch PC" (next-PC redirect) read `PC`. This is purely additive.

### C.8 [EASY] No "ready" / wakeup bit on producer

**Where**: nowhere — there is no `ready` Payload in `payload_common.h:113-156`.

**Problem**: An OoO functional unit signals "my result is ready" by writing to the wakeup queue (see C.5). In the current code, the result is *written into the EX-stage Payload* and *immediately read by RegFile* on the next cycle (in-order: 1 cycle to EX, 1 cycle to MEM, 1 cycle to WB). For OoO, the producer needs to say "I'm done with Uid 7" — a one-bit status per Uid.

**C.8 mitigation for Phase 1**: no change needed in Phase 1. Add `Payload<bool> READY` later when implementing wakeup.

### C.9 [EASY] `kNumRegs = 32` and `XLEN ∈ {32, 64}` static_asserts block cross-ISA reg counts

**Where**: `payload_common.h:113-118`:
```cpp
static_assert(XLEN == 32 || XLEN == 64, ...);
static_assert(std::is_same<T, std::uint32_t>::value ||
              std::is_same<T, std::uint64_t>::value, ...);
```
and `reg_file.h:69` (`static constexpr std::size_t kNumRegs = 32;`), `hazard.h:61`, `branch_predictor.h:61-64` (all `static constexpr`).

**Problem**: These cement RISC-V. ARM has 16 reg indexes, x86-64 has 16 with overlapping L/A segments. The `RD_IDX` Payload is `uint_t<5>` (`payload_common.h:139`) — 5 bits — which is exactly the RISC-V reg-field width.

**C.9 mitigation for Phase 1**: change `static_assert(XLEN == 32 || XLEN == 64, …)` to `static_assert(XLEN >= 32 && XLEN <= 1024, "XLEN must be a power of 2 between 32 and 1024")` and template `RegFilePlugin<T, N_REGS>`. This costs nothing for RISC-V (32 is the default) and is a 5-line change. The `RD_IDX` Payload width must be templated on the ISA's reg-field width.

### C.10 [EASY] `pipe_builder.h:78-86` `declare_substage` always creates new PipeNode

**Where**: `include/cf/plugin/pipe_builder.h:78-86` (already noted in `dse_architecture.md §5.2`).

**Problem**: `declare_substage(parent, sub, /*depth*/)` always creates a fresh `PipeNode`. The `depth` parameter is **commented out and ignored**. `dse_architecture.md §5.3` already proposes `merge_stage()` to fix this; this is on the M4-DSE roadmap.

**C.10 impact on OoO**: low. OoO doesn't need `merge_stage`. The `merge_stage` fix is a Phase-1 in-order improvement, not an OoO blocker. Listed here only for completeness.

### C.11 [EASY] `RegFilePlugin::build` is double-defined — `.cpp` overrides `.h` and uses `keys_rv32::` unconditionally

**Where**: `ip/cpu/plugins/reg_file.cpp:39-77` re-defines `RegFilePlugin<T>::build(...)` that *uses* `pl::keys_rv32::DECODE` and `pl::keys_rv32::RS1` (`:47, 52, 58, 69, 72`). The header version at `reg_file.h:119-153` uses `cf::cpu::core::payload::keys<T, sizeof(T) * 8>` — i.e., `T`-templated. For a `RegFilePlugin<uint64_t>`, the `.cpp` version will read 32-bit keys for 64-bit instructions.

This is **a real bug** (also flagged in `dse_architecture.md §1.2` and §10.2) that the M4-DSE roadmap fixes. It does not block OoO, but it blocks any `uint64_t` use at all.

**C.11 impact on OoO**: none directly, but **MUST be fixed** for Phase 4 (RV64 integration).

### C.12 [MEDIUM] `CtrlLink::flush_when` exists but no plugin uses it

**Where**: `include/cf/plugin/ctrl_link.h:46-50` (API exists); search confirms no CPU plugin calls it.

**Problem**: Branch mispredict in OoO must flush everything *speculatively younger* than the branch. The `flush_when` primitive is exactly right, but no plugin currently constructs a `CtrlLink`. The `branch_predictor.h:110-119` `update` callback would naturally call `flush_when(mispredicted)` in the OoO model. Today it just updates a counter.

**C.12 impact on OoO**: the API is there, the use site is missing. This is a **plugin-level gap** (BP must be re-implemented to call `flush_when`), not a framework gap. The framework is already OoO-ready in this respect.

### C.13 [HARD] `RegFilePlugin`'s commit is implicit in `writeback` — there is no separate "commit" hook

**Where**: `reg_file.h:142-152` (the `at_stage("writeback", …)` writes directly to `regs_`).

**Problem**: In OoO, the instruction reaches "writeback" out of program order, but it must commit to architectural state **in program order**. The current code conflates "functional unit result is ready" (OoO writeback) with "this instruction is now visible to subsequent fetches" (OoO commit). They are different events.

**C.13 mitigation for Phase 1**: do not change `RegFilePlugin` in Phase 1. Add a new plugin `CommitPlugin` (in `ip/cpu/plugins/commit.h`, future) that registers `at_stage("commit", Phase::LATE, …)` and reads from PRF, writes to arch reg file. The current `RegFilePlugin::writeback` callback becomes a no-op stub in the OoO model (or moves to a "writeback to PRF" callback that targets the *physical* reg file). This is a Phase 5+ refactor, not a Phase 1 lock.

### C.14 [MEDIUM] `PluginBase` has no `thread_id` awareness

**Where**: `include/cf/plugin/plugin_base.h:48-72` (the whole class).

**Problem**: A plugin instance is a singleton. For SMT, two threads share the same BP, the same RegFile, the same Hazard. Either:
- (a) the plugin stores `std::array<state, MAX_THREADS>` internally, or
- (b) **the factory instantiates one plugin per thread** (e.g. `RegFilePlugin per_thread[2]`), and `at_stage` callbacks are wired with the correct `thread_id` in their closure.

Both work. (a) is what production designs do (one set of CAMs, multiplexed by tid). (b) is what VexRiscv does for some plugins.

**C.14 mitigation for Phase 1**: do not change `PluginBase`. Add an optional `uint8_t thread_id` parameter to `at_stage` callbacks (already there in spirit — the callback is a `std::function<void()>`; a closure can capture tid). The factory instantiates per-thread plugins and passes tid to the closure.

This is a **factory-level concern**, not a framework-level concern. `cpu_factory.h` (the empty stub) is the right place to grow this.

### C.15 [EASY→MEDIUM] The framework has no "dispatch group" concept

**Where**: `pipe_builder.h:69-76` (`at_stage` takes a single `std::string` stage name).

**Problem**: For superscalar, a 2-wide dispatch at "decode" produces 2 instructions that need to be processed together. The current API assumes 1 instruction = 1 stage = 1 node.

**C.15 mitigation for Phase 1**: do not change the API. The pattern is: create **two PipeNodes with the same logical name** ("decode_lane0" and "decode_lane1") and have the **fetch plugin** write 2 instructions, one per lane. The "single instruction in flight" assumption is preserved **per lane**. Lane-level parallelism is exposed via `at_stage("decode_lane0", …)` and `at_stage("decode_lane1", …)`. This is how VexRiscv does it.

This costs zero in Phase 1; it just means **the logical stage name in `at_stage` should not be hardcoded** to "execute" but to a parameter that the factory sets. The plugins already do this (e.g. `pb.at_stage("execute", …)` is a string literal, but it is *captured by name*, so the factory could re-route by `at_stage(cfg.execute_stage_name, …)`).

**C.15 actual fix for Phase 1**: stop using string literals like `"execute"` inside plugin `build()`. Instead, take the stage name as a parameter to the plugin ctor (e.g. `BranchPredictorPlugin<T>(const CPUConfig& cfg)` with a member `std::string execute_stage_`). The `dse_architecture.md §6.3` `setup_with_config(pb, const void*)` proposal is exactly the right hook for this. **Recommended adoption**.

### C.16 [MEDIUM] `commit_hooks_` are per-`PipeBuilder`, not per-instruction

**Where**: `pipe_builder.h:128-138`.

**Problem**: A commit hook runs at end of `pb.run()`, after all `at_stage` callbacks. It operates on plugin state. For OoO, the "commit" operation is per-instruction: ROB head → write arch state → free PRF entry → free IQ entry. The current `commit_hooks_` is the right **mechanism** (run-at-end-of-cycle), but the **scope** is "all in-flight instructions", not "this one specific IID".

**C.16 mitigation for Phase 1**: do not change `commit_hooks_`. The ROB walk happens *inside* one of the hooks. The hook iterates the ROB head, commits one instruction at a time. The framework does not need to know per-instruction — it just provides the "end of cycle" call point.

### C.17 [EASY] `RiscvDecodePlugin` uses string literal "decode" in `build`

**Where**: `ip/cpu/arch/riscv/decode.h:61` (`pb.at_stage("decode", …)`).

**Problem**: Same as C.15. The string "decode" is hardcoded. For a 2-wide superscalar, "decode" might need to be "decode_lane0" / "decode_lane1". The factory needs to control this.

**C.17 mitigation for Phase 1**: same as C.15. Pass stage names via plugin ctor.

---

## D. MUST change in Phase 1 (lock now or pay later)

The items below are the **6 forward-compatibility locks** that should be added to the Phase 1 design. Each is small (≤ 30 lines of churn), each preserves current behavior in single-thread in-order mode, and each prevents a **Phase 5 rewrite of an abstraction** that the codebase already uses in 11+ places.

### D.1 Add `Uid` and `ThreadId` as Payloads (no framework change)

**Add to `payload_common.h`**:
```cpp
static inline cf::plugin::Payload<cf::plugin::uint_t<8>>   UID{"cpu.uid"};          // 0..255, ROB index
static inline cf::plugin::Payload<cf::plugin::uint_t<2>>   THREAD_ID{"cpu.tid"};   // 0..3
static inline cf::plugin::Payload<T>                       IID_PC{"cpu.iid_pc"};   // PC tagged to IID
```

**Cost**: 3 lines in one file. **No plugin** needs to be touched. Plugins that *want* to use them do so. In Phase 1, `UID` is just `0` everywhere; it becomes meaningful in Phase 5.

**Why now**: the 11 `at_stage` callbacks that read `PC` (e.g. `branch_predictor.h:99, 115`, `int_alu.h:54-55`, `reg_file.h:127`) can be migrated to read `IID_PC` *incrementally*. The moment a second instruction is in a stage (which is the first day of superscalar work), `PC` becomes ambiguous and the migration is forced — at which point the migration is **11 files of edits under time pressure** instead of a leisurely Phase 1 cleanup.

### D.2 Add `thread_id` parameter to stateful plugins (template, not API)

**Change `RegFilePlugin<T>` → `RegFilePlugin<T, N_REGS=32, N_THREADS=1>`** (`reg_file.h:62`).
**Change `HazardPlugin<T>` → `HazardPlugin<T, N_REGS=32, N_THREADS=1>`** (`hazard.h:55`).
**Change `BranchPredictorPlugin<T>` → `BranchPredictorPlugin<T, BTB_SIZE=16, …, N_THREADS=1>`** (`branch_predictor.h:55`).

Storage becomes:
- `RegFilePlugin::regs_` = `std::array<array_store<T, N_REGS>, N_THREADS>`.
- `HazardPlugin::scoreboard_` = `std::array<std::array<bool, N_REGS>, N_THREADS>`.
- `BranchPredictorPlugin::global_history_` = `std::array<uint8_t, N_THREADS>`.

**Cost**: ~30 lines per plugin, header-only. Default `N_THREADS = 1` keeps current ABI.

**Why now**: this is **not** a behavior change for Phase 1. It is a template parameter that is `1` until Phase 5. But once Phase 5 wants SMT, this is the difference between "add a new plugin" and "rewrite the existing plugin and break every test that uses it".

### D.3 `HazardPlugin::has_hazard` returns an enum, not a bool

**Change `bool has_hazard(const DecodePayload&)` → `HazardKind has_hazard(const DecodePayload&, uint8_t tid)`** (`hazard.h:86-91`).

```cpp
enum class HazardKind : uint8_t { NONE = 0, RAW_RS1, RAW_RS2, WAW };
HazardKind has_hazard(const DecodePayload& dec, uint8_t tid) const;
```

**Cost**: ~5 lines, the function body is unchanged. `build()` at `hazard.h:116-150` is updated to thread `tid` through (the tid comes from `n->(KeyType::THREAD_ID)`, which is `0` in Phase 1).

**Why now**: changing the return type of `has_hazard` from `bool` to `enum` is an **API break** that touches every caller. The only in-tree caller is `hazard.h:126` itself (the `build()` callback). There are 0 external callers in the test suite (tests use `has_raw`/`has_waw` directly, not `has_hazard`). So the cost is **near zero**. The benefit: in Phase 5, OoO replaces the bitmask check with a CAM lookup that returns *which* operand is pending — the enum lets the new code coexist with the old in a single interface.

### D.4 `BranchPredictorPlugin::predict` and `update` take `tid`

**Change** (`:129-143` and `:155-168`):
```cpp
T predict(T pc, uint8_t tid) const;
void update(T pc, bool taken, T target, uint8_t tid);
```

**Cost**: trivial. `tid = 0` in Phase 1. `global_history_` becomes `std::array<uint8_t, N_THREADS> global_history_{}` and is indexed by `tid`.

**Why now**: cheapest forward-compat win in the entire codebase. 10 line refactor. No behavior change in single-thread mode.

### D.5 `RegFilePlugin::build` string literals → stage-name member

**Change** (`reg_file.h:119-153`): add `std::string decode_stage_` and `std::string writeback_stage_` members, set in ctor from `CPUConfig`. Replace string literals `"decode"` and `"writeback"` with `decode_stage_` and `writeback_stage_`.

**Cost**: ~15 lines.

**Why now**: the moment anyone wants 2-wide superscalar with `decode_lane0` / `decode_lane1` stage names, every plugin's `build()` must be re-templatized. Doing it now in **one** plugin (RegFile) proves the pattern; doing it later means doing it in **11** plugins at once.

### D.6 Document `at_stage` + `commit_hook` as the OoO commit primitive

**Add to `pipe_builder.h` header comment** (`:1-17` and `:104-125`): explicitly state that:
- `at_stage` = "this stage's logic per cycle"
- `commit_hook` = "end-of-cycle commit, OoO commit primitive"
- `flush_when` = "mispredict squash primitive"

The framework already provides these. The **only** missing piece is documentation. This costs zero code.

### D.7 (Optional) `setup_with_config` (the `dse_architecture.md §6.3` proposal)

`dse_architecture.md §6.3` proposes adding `PluginBase::setup_with_config(pb, const void* cfg)`. **Adopt this** as proposed. It is the hook for the factory to pass `CPUConfig` to plugins (per D.2/D.4/D.5 above), and it does not touch any existing code path.

---

## E. CAN stay as-is until Phase 5+

The items below are **not Phase 1 work**. They are Phase 5+ work that the framework already supports (or that requires designing a new plugin). Do not touch them in Phase 1.

### E.1 ROB, PRF, IQ, LSQ, Rename

These are **new plugins** (`CommitPlugin`, `RenamePlugin`, `IssueQueuePlugin`, `LSQPlugin`). They are not abstractions to refactor; they are new code. The framework's `at_stage` + `commit_hook` already supports them.

**Do not** start designing them in Phase 1. The risk is over-engineering based on theoretical OoO needs. Wait until there is a real workload demanding OoO.

### E.2 Per-instruction Payload storage (the "Uid-keyed PayloadStore")

**Do not** refactor `PayloadStore` to key by IID. The OoO model is: each PipeNode holds at most one instruction (the current "stage" view). Multiple in-flight instructions are held **in the IQ / ROB / LSQ**, not in `PayloadStore`. The "Uid-keyed store" temptation is **wrong**: it duplicates the ROB into the Payload system.

The correct OoO model is:
- `PipeNode` = **one in-flight instruction** (just like today).
- `ROB` = a plugin member, indexed by IID, holds `{arch_dst, prf_dst, done_bit, exception}`.
- `IQ` = a plugin member, indexed by IID, holds `{op_class, prf_src1, prf_src2, ready1, ready2}`.
- `PRF` = a plugin member, a large `array_store<T, 4096>` (configurable), indexed by physical reg ID.

The Payload system **stays as-is**. This is the design point that makes the current architecture OoO-friendly.

### E.3 `CtrlLink::halt_when` semantic upgrade

The current `CtrlLink::halt_when` takes a `std::function<bool()>` and OR-merges all halt conditions. For OoO backpressure, the halt condition must reference **a specific IID's IQ entry**, not a global state. The Plugin-level code can compute the halt condition with whatever scope it wants; the framework's `bool should_halt()` is fine.

**Do not** change `CtrlLink`. Use it as-is.

### E.4 `RiscvDecodePlugin`'s specific decode logic

`decode.h:56-88` is RISC-V-specific and will not be touched by OoO. The OoO `RenamePlugin` will read the `DECODE` Payload after `RiscvDecodePlugin` writes it, then allocate PRF entries. The decode plugin's job (turn instruction word into `DECODE` + `RISCV_DETAIL`) is unchanged.

### E.5 `RegFilePlugin::writeback` writing directly to `regs_`

For in-order, this is correct. For OoO, **the writeback callback becomes a PRF write**, and a **new CommitPlugin** does the arch reg file write at commit time. The existing `RegFilePlugin::writeback` is a Phase 5+ refactor, not a Phase 1 refactor. It is also the **single thing** that an OoO port will have to change in this plugin — making RegFile the one plugin that **does** get redesigned in Phase 5+.

**Why this is OK**: RegFile is the only plugin whose writeback *semantic* changes (PRF vs arch). Hazard, BP, IBus, DBus, RegFile read all stay semantically the same (they read arch state). The Phase 5 rewrite is **scoped to RegFile's writeback callback** + a new CommitPlugin.

### E.6 The empty `cpu_factory.h` stub

`cpu_factory.h:73-127` is an empty stub. M4-DSE will fill it in (per `dse_architecture.md §7`). The OoO additions happen in **the same factory file**, when Phase 5 instantiates `IssueQueuePlugin`, `RenamePlugin`, `CommitPlugin` (in addition to the existing 11). Do not pre-design the factory for OoO in Phase 1.

### E.7 `payload_common.h` `XLEN == 32 || 64` static_assert

`payload_common.h:114-118` cements RISC-V's XLEN. This is **fine for Phase 1** (RISC-V is the only target). The Phase 5 OoO model is **per-ISA**, not cross-ISA. Cross-ISA is `dse_architecture.md §2.3` deferral, not OoO. Leave it.

### E.8 `array_store` Phase 6 double-buffer

`include/cf/plugin/storage.h:69-137` documents that `array_store` is single-buffered in Phase 1 and will become double-buffered in Phase 6 for `ch_mem` parity. This is RTL-only. It does not affect TLM OoO modeling (where each `pb.run()` is a cycle and explicit `commit()` is the OoO commit point). **Do not** refactor `array_store` in Phase 1.

---

## Summary of Forward-Compatibility Locks

| Lock | Phase-1 cost | Phase-5 cost if NOT locked | Severity if NOT locked |
|------|-------------|----------------------------|-------------------------|
| D.1: `UID`/`THREAD_ID`/`IID_PC` Payloads | 3 lines, header | Touches 11 `at_stage` callbacks | Medium |
| D.2: template plugins on `N_THREADS` | ~90 lines (3 plugins × 30) | Rewrite 3 plugins + every test | **High** |
| D.3: `HazardKind` enum | 5 lines | API break at every caller site | Medium |
| D.4: `tid` in `predict`/`update` | 10 lines | Rewrite BP twice (single-thread → multi-thread) | **High** (cheapest to lock) |
| D.5: stage-name members in plugins | ~15 lines × plugins | 11 plugins × stage-name change | **High** (most plugins affected) |
| D.6: documentation of `at_stage`+`commit_hook`+`flush_when` | 0 lines | Designers reinvent these in Phase 5 | Low |
| D.7: `setup_with_config` | 3 lines | Factory can't pass `CPUConfig` to plugins | Medium |

**Total Phase-1 work**: ~150 lines of churn in headers, plus 0 lines of behavior change. **Prevents**: ~2000 lines of refactor in Phase 5+.

---

## Honest Assessment: What WILL NOT Scale

Let me be **brutally honest** about which abstractions cannot survive into OoO without redesign:

1. **`RegFilePlugin::writeback` writing to `regs_`** (`reg_file.h:142-152`). This is the only plugin whose **writeback semantics** are wrong for OoO. The writeback will need to become a PRF write. The architectural reg file write moves to a new `CommitPlugin` at a new `"commit"` stage. **Phase 5+ rewrite, scoped to one plugin.** Acceptable.

2. **`HazardPlugin`'s bitmask scoreboard** (`hazard.h:152-153`). This is a single in-flight-writer assumption baked into a `std::array<bool, 32>`. OoO needs a "set of speculative writers" tracked by IID. The plugin's `has_hazard` API would have to be replaced by an IQ entry's `ready1/ready2` flags. **Phase 5+ redesign, scoped to one plugin.** Acceptable.

3. **`BranchPredictorPlugin::global_history_` (single uint8_t)** (`:215`). The GHR checkpoint-and-restore on mispredict is missing. Single global for SMT. **Phase 5+ redesign, scoped to one plugin.** Acceptable.

4. **The `kNumRegs = 32` and `XLEN ∈ {32, 64}` static_asserts** (`reg_file.h:69`, `payload_common.h:114-118`). Cross-ISA. **Phase 5+** if cross-ISA is ever attempted. **Acceptable** for in-order RISC-V scope.

What is **not** acceptable is **redesigning the framework spine** — `PipeNode`, `PipeBuilder`, `PluginBase`, `Payload<T>`, `PayloadStore`, `CtrlLink`, `PipeArbitration`. **None of these need to change** for OoO. The VexRiscv model (which the current architecture deliberately copies, per `multi_isa_architecture.md §1.3`) is OoO-compatible by design. The current code is faithful to that model. **The framework is fine. The plugin implementations are where the work is.**

The single design choice that would be **catastrophic** to undo: **per-Plugin singleton state in `RegFilePlugin::regs_`, `HazardPlugin::scoreboard_`, `BranchPredictorPlugin::btb_/bimodal_/gshare_/global_history_`**. Phase 1 must lock the `N_THREADS` template parameter (D.2) so that these do not become 11-place rewrites in Phase 5.

---

## Appendix 1: Concrete code stubs for the Phase-1 fixes

### A.1.1 `payload_common.h` additions (D.1)

```cpp
template <typename T = std::uint32_t, unsigned XLEN = sizeof(T) * 8>
struct keys {
  // ... existing 11 keys ...

  // Forward-compat: Phase 5+ OoO/Superscalar/SMT hooks
  // In Phase 1: always 0; never read by current plugins
  static inline cf::plugin::Payload<cf::plugin::uint_t<8>> UID{"cpu.uid"};
  static inline cf::plugin::Payload<cf::plugin::uint_t<2>> THREAD_ID{"cpu.tid"};
  static inline cf::plugin::Payload<T>                      IID_PC{"cpu.iid_pc"};
};
```

### A.1.2 `RegFilePlugin` (D.2, D.5)

```cpp
template <typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>
class RegFilePlugin : public cf::plugin::PluginBase {
  static_assert(N_REGS >= 1 && N_THREADS >= 1);
  // ...
 private:
  std::string decode_stage_;
  std::string writeback_stage_;
  std::array<cf::plugin::storage::array_store<T, N_REGS>, N_THREADS> regs_;
};
```

### A.1.3 `HazardPlugin` (D.2, D.3)

```cpp
template <typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>
class HazardPlugin : public cf::plugin::PluginBase {
 public:
  enum class HazardKind : std::uint8_t { NONE = 0, RAW_RS1, RAW_RS2, WAW };
  HazardKind has_hazard(const DecodePayload& dec, uint8_t tid) const;
  // ...
 private:
  std::array<std::array<bool, N_REGS>, N_THREADS> scoreboard_{};
};
```

### A.1.4 `BranchPredictorPlugin` (D.2, D.4)

```cpp
template <typename T,
          std::size_t BTB_SIZE   = 16,
          std::size_t BIMODAL_SZ = 16,
          std::size_t GSHARE_SZ  = 16,
          std::uint8_t GHR_BITS  = 8,
          std::size_t N_THREADS  = 1>
class BranchPredictorPlugin : public cf::plugin::PluginBase {
 public:
  T predict(T pc, uint8_t tid = 0) const;
  void update(T pc, bool taken, T target, uint8_t tid = 0);
  // ...
 private:
  std::array<uint8_t, N_THREADS> global_history_{};
  // btb_, bimodal_, gshare_ stay per-instance for now (Phase 5 may also
  // tag these with tid if per-thread BTB is desired)
};
```

### A.1.5 `PluginBase` (D.7)

```cpp
class PluginBase {
 public:
  virtual ~PluginBase() = default;
  virtual void setup(PipeBuilder&) {}
  virtual void build(PipeBuilder&) = 0;
  // NEW: optional config hook (default fallback to setup())
  virtual void setup_with_config(PipeBuilder& pb, const void* /*cfg*/) {
    setup(pb);
  }
 private:
  void tick() = delete;
};
```

### A.1.6 `pipe_builder.h` documentation (D.6)

Add to the file-level comment:
```cpp
// Framework primitives for OoO/Superscalar/SMT forward compatibility:
//   - at_stage(name, phase, cb)   = per-cycle logic for stage `name`
//   - commit_hook(cb)             = end-of-cycle commit (OoO commit point)
//   - CtrlLink::flush_when(cond)  = speculative squash on mispredict
// Per-instruction identity (IID) and per-thread tagging are Plugin-level
// concerns, not framework concerns. See ip/cpu/docs/dse_architecture_v2_locks.md and
// ip/cpu/docs/dse_architecture_v2_design_research.md.
```

---

## Appendix 2: What "per-instruction identity" actually means in C++

A common OoO-design mistake is to think "I need a PayloadStore that holds N instructions per PipeNode". This is **architecturally wrong**. Here is the correct mental model:

**Each `PipeNode` still holds exactly one instruction** (the one currently in that stage). The "many in-flight" view is held in **Plugin members**, not in PipeNodes:

| "Many in-flight" concept | Where it lives in the current code | What it becomes in OoO |
|--------------------------|-----------------------------------|------------------------|
| ROB (reorder buffer) | (does not exist) | `CommitPlugin::rob_[ROB_SIZE]` (plugin member) |
| IQ (issue queue) | (does not exist) | `IssueQueuePlugin::iq_[IQ_SIZE]` |
| PRF (physical reg file) | `RegFilePlugin::regs_[32]` becomes architectural | `RenamePlugin::prf_[PRF_SIZE]` (large, e.g. 4096 entries) |
| GHR checkpoints | (does not exist) | `BranchPredictorPlugin::ghr_checkpoint_stack_` |
| Wakeup events | (does not exist) | `IssueQueuePlugin::wakeup_queue_` (drained in `commit_hook`) |
| Per-thread state | (does not exist) | `RegFilePlugin::regs_[N_THREADS][N_REGS]` etc. |

**The PayloadStore stays a single-instruction view.** The plugins own the OoO structures. This is exactly how VexRiscv works (one set of `Stageables` per stage, one set of plugin members holding the OoO data structures), and it is **the architectural choice that makes the current framework OoO-compatible**.

If the Phase 5 designer is tempted to "make PayloadStore hold N instructions", **resist**. The IQ and ROB are already doing that. The Payload system is for **single-stage-per-cycle** communication only.

---

*Document end. This is input for `dse_architecture_v2_locks.md` § "Phase 1 must lock" and `dse_architecture_v2_design_research.md` §3.*

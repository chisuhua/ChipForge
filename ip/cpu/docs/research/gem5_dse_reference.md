# gem5 DSE Reference Report — Out-of-Order, Superscalar, SMT, Branch Predictors, DSE Patterns

| Field | Value |
|---|---|
| Version | 1.0 |
| Date | 2026-06-17 |
| Status | 🔵 Reference (informs `dse_architecture_v2_design_research.md` planning) |
| Scope | ChipForge IP/CPU Phase 5+ DSE work, derived from gem5 source |
| Upstream | gem5 @ `/workspace/project/gem5/` |
| Audience | Authors of `dse_architecture_v2_design_research.md` and PipeBuilder/PluginBase evolution |

> **Purpose**: Document the actual gem5 implementation patterns that the
> ChipForge plugin-based DSE architecture will need to accommodate when
> OoO, superscalar/multi-issue, and SMT are added in Phase 5+. Every claim
> cites a gem5 `file:line`. The intent is to identify abstractions that
> **must** be designed in Phase 1 so that later OoO/Superscalar/SMT
> extensions do not require rewriting `CpuFactory` or any of the existing
> 11 plugins.

---

## Table of Contents

0. [Quick Reference — what gem5 is doing at a glance](#0-quick-reference)
1. [Out-of-Order execution infrastructure](#1-out-of-order-execution-infrastructure)
2. [Superscalar / multi-issue support](#2-superscalar--multi-issue-support)
3. [SMT (Simultaneous Multithreading)](#3-smt-simultaneous-multithreading)
4. [Branch prediction variants](#4-branch-prediction-variants)
5. [DSE pattern — how gem5 parameterizes CPU families](#5-dse-pattern)
6. [What this means for ChipForge](#6-what-this-means-for-chipforge)
7. [Concrete API additions recommended for Phase 1](#7-concrete-api-additions-for-phase-1)
8. [Approximate LoC budget for an OoO port](#8-approximate-loc-budget-for-an-ooo-port)
9. [File:line index of all citations](#9-fileline-index-of-all-citations)

---

## 0. Quick Reference

| Subsystem | gem5 LoC | Key files | Plugin-portability |
|---|---|---|---|
| Base O3 CPU + tick loop | 1,507 + 626 (header) | `cpu.cc/hh` | **Hard** — owns all stages, not pluggable |
| IEW (Issue/Execute/Writeback) | 1,628 + 484 (header) | `iew.cc/hh` | **Hard** — central to OoO + SMT |
| ROB | 526 + 349 (header) | `rob.cc/hh` | **Medium** — algorithm is reusable as concept |
| IQ (Issue Queue) | 1,703 + 641 (header) | `inst_queue.cc/hh` | **Medium** — wakeup/select logic reusable |
| Rename + FreeList | 1,515 + 545 + 53 + 132 | `rename.cc/hh`, `free_list.cc/hh`, `rename_map.cc/hh` | **Medium** — concept reusable |
| LSQ + LSQUnit + MemDep | 1,620 + 1,682 + 1682 | `lsq.cc/hh`, `lsq_unit.cc/hh`, `mem_dep_unit.cc/hh` | **Medium** — store-fwd logic reusable |
| Commit | 1,537 + 504 (header) | `commit.cc/hh` | **Medium** — policy reusable |
| Fetch + Decode | 1,676 + 761 | `fetch.cc/hh`, `decode.cc/hh` | **Hard** — gem5-specific |
| FU pool | 168 (header) | `fu_pool.hh` | **High** — already parametric |
| Base CPU | 900 (header) | `base.hh` | N/A (interface only) |
| Branch predictors | ~6,000 across `pred/` | `bpred_unit.cc`, `tournament.cc`, `gshare.cc`, `bi_mode.cc`, `tage.cc`, `tage_base.cc` | **High** — already pure SimObject with parameters |
| Minor (in-order) | ~3,000 | `cpu.cc/hh`, `pipeline.cc/hh` etc. | Reference comparison |

> **Headline conclusion**: gem5 **does not use a plugin pattern** for its
> CPU models. The OoO CPU is a single 1,500-line C++ class (`gem5::o3::CPU`)
> that hard-composes 7 sub-stages. The closest gem5 comes to "plugin" is
> the **branch predictor**, which is a `SimObject` selected via a
> `SimObject` parent parameter (`BranchPredictor.conditionalBranchPred =
> TournamentBP(...)`). The ChipForge plugin model is **more aggressive**
> than gem5's — it tries to make the whole pipeline pluggable, which
> gem5 doesn't attempt. This has implications: ChipForge must define its
> own "OoO shell" plugin family (RobPlugin, IqPlugin, LsqPlugin,
> RenamePlugin) and keep the in-order plugins as a special case.

---

## 1. Out-of-Order execution infrastructure

### 1.1 Class hierarchy

```
BaseCPU (src/cpu/base.hh:106)
  └─ CPU (src/cpu/o3/cpu.hh:96)        // gem5::o3::CPU — the OoO class
       │                                // Holds all 7 stages as members
       │                                // Not polymorphic beyond BaseCPU
       ├─ BAC    (src/cpu/o3/bac.hh)   // Branch Address Calc
       ├─ FTQ    (src/cpu/o3/ftq.hh)   // Fetch Target Queue (decoupled FE)
       ├─ Fetch  (src/cpu/o3/fetch.hh)
       ├─ Decode (src/cpu/o3/decode.hh)
       ├─ Rename (src/cpu/o3/rename.hh:78)
       ├─ IEW    (src/cpu/o3/iew.hh:87) // Issue/Execute/Writeback fused
       │    ├─ InstructionQueue (inst_queue.hh:178)  // The IQ
       │    │    └─ IQUnit (inst_queue.hh:82)        // Per-op-class sub-IQ
       │    └─ LSQ (lsq.hh:76)
       │         └─ std::vector<LSQUnit> (lsq_unit.hh:88) // Per-thread
       ├─ Commit (src/cpu/o3/commit.hh:91)
       ├─ ROB    (src/cpu/o3/rob.hh:71)
       ├─ PhysRegFile  (src/cpu/o3/regfile.hh)
       ├─ UnifiedFreeList  (free_list.hh:124)
       └─ UnifiedRenameMap (rename_map.hh:168)
            └─ std::array<SimpleRenameMap, CCRegClass+1> (rename_map.hh:171)
```

Stages are **value members**, not pointers (`cpu.hh:415-448`). The CPU
constructs them all in its initializer list and wires them with
`setXxxQueue()` calls. There is no virtual dispatch at the stage level.

### 1.2 Reorder Buffer (ROB)

- **File**: `src/cpu/o3/rob.hh` (349 lines), `src/cpu/o3/rob.cc` (526 lines)
- **Backing structure**: `std::list<DynInstPtr> instList[MaxThreads]`
  (rob.hh:289) — one per thread, doubly linked list of dynamic
  instructions
- **Per-thread partitioning** (SMT): `unsigned threadEntries[MaxThreads]`
  and `maxEntries[MaxThreads]` (rob.hh:283,286) — entries are dynamically
  repartitioned via `entryAmount(num_threads)` and `resetEntries()`
  (rob.hh:169,166)
- **Width parameters** (from `BaseO3CPUParams`):
  - `commitWidth = 8` (BaseO3CPU.py:127) — max instructions retired/cycle
  - `squashWidth = Optional` (BaseO3CPU.py:128) — if unset, instant
    squash; otherwise multi-cycle squash
  - `numROBEntries = 192` (BaseO3CPU.py:188)
  - `numRobs = 1` (BaseO3CPU.py:172) — multiple physical ROBs (rarely > 1)
- **Status enum** (rob.hh:78-83): `Running`, `Idle`, `ROBSquashing`
- **Key methods**:
  - `insertInst(const DynInstPtr&)` (rob.hh:117)
  - `retireHead(ThreadID tid)` (rob.hh:154) — calls free physical regs
  - `squash(squash_num, tid)` (rob.hh:207) — called on mispredict/mem
    violation
  - `isHeadReady(tid)` (rob.hh:160) — head-of-ROB ready to commit?
  - `canCommit()` (rob.hh:163) — any thread has committable head?
  - `numFreeEntries(tid)` (rob.hh:175) — drives back-pressure to rename
  - `getMaxEntries(tid)` / `getThreadEntries(tid)` (rob.hh:178,182) —
    accounting
- **LoC**: 349 (header) + 526 (impl) = **875 LoC**

### 1.3 Issue Queue (IQ)

- **File**: `src/cpu/o3/inst_queue.hh` (641 lines), `src/cpu/o3/inst_queue.cc` (1,703 lines)
- **Sub-structure**: `IQUnit` is a per-op-class SimObject
  (inst_queue.hh:82) with its own params (`IQUnitParams`,
  `src/cpu/o3/IQUnit.py`) — supports having **separate IQs per op class**
  (Int/Fp/Mem/Vec branches)
- **Default sub-IQs** (from `BaseO3CPU.py:187`):
  - `instQueues = VectorParam.IQUnit(IQUnit(), "Vector of IQs")` — by
    default a single combined IQ; can be overridden to give Int/FP/Vec
    their own IQs (see O3CPU.py and ARM/RISC-V configs)
- **Backing structures** (inst_queue.hh:417-490):
  - `std::list<DynInstPtr> instList[MaxThreads]` — per-thread tracking
  - `ReadyInstQueue readyInsts[Num_OpClasses]` (line 453) — per-op-class
    priority queues
  - `std::list<ListOrderEntry> listOrder` (line 480) — age-ordered list
    of op-class heads for **oldest-first across-op-class selection**
  - `DependencyGraph<DynInstPtr> dependGraph` (line 501) — register
    dependency tracking
  - `std::vector<bool> regScoreboard` (line 533) — secondary scoreboard
  - `MemDepUnit memDepUnit[MaxThreads]` (line 399) — per-thread memory
    dep predictor
- **Wakeup/select** (inst_queue.cc — the famous ~800 LoC):
  - `scheduleReadyInsts()` (inst_queue.hh:320) — moves ready insts from
    `instList` to `readyInsts[op_class]`
  - `getInstToExecute()` (inst_queue.hh:290) — selects one per cycle
    (or per `issueWidth`) from the oldest ready queue
  - `wakeDependents(completed_inst)` (inst_queue.hh:332) — walks
    `dependGraph`, marks consumers ready
  - `FUCompletion` event (inst_queue.hh:185-213) — schedules a future
    wakeup matching the FU's execute latency
- **Width parameters**:
  - `issueWidth = 8` (BaseO3CPU.py:120) — max issued/cycle (shared
    across op classes, see `totalWidth` in inst_queue.hh:511)
  - Each `IQUnit` has its own `numEntries` (default typically 32-64)
- **SMT resource sharing** (inst_queue.cc:78-115):
  - `SMTQueuePolicy` enum: `Dynamic` | `Partitioned` | `Threshold`
    (defined `cpu/o3/SMT.py:46`)
  - In `Dynamic` mode: any thread can use all entries
  - In `Partitioned`: each thread gets `numEntries / numThreads`
  - In `Threshold`: each thread gets up to
    `smtIQThreshold% × numEntries`
- **LoC**: 641 (header) + 1,703 (impl) = **2,344 LoC** — the most
  algorithmically dense part of gem5

### 1.4 Register Rename table

- **Files**:
  - `src/cpu/o3/rename_map.hh` (302 lines), `src/cpu/o3/rename_map.cc` (132 lines)
  - `src/cpu/o3/free_list.hh` (197 lines), `src/cpu/o3/free_list.cc` (53 lines)
  - `src/cpu/o3/rename.hh` (545 lines), `src/cpu/o3/rename.cc` (1,515 lines)
- **Two-level architecture** (rename_map.hh:168-297):
  - `UnifiedRenameMap` — top-level, wraps an `std::array<SimpleRenameMap,
    CCRegClass+1>` (line 171), one `SimpleRenameMap` per register class
    (Int/FP/Vec/VecPred/Mat/CC/Misc)
  - `SimpleRenameMap` (rename_map.hh:71-159) — single class, holds
    `std::vector<PhysRegIdPtr> map` (line 76) indexed by architectural
    register number
- **Pair-of-registers return** (rename_map.hh:105):
  - `RenameInfo = pair<PhysRegIdPtr, PhysRegIdPtr>` — `{new_phys_reg,
    old_phys_reg}`. The old one is what the previous architectural
    writer held; it's freed when the new writer retires.
- **Per-thread maps** (cpu.hh:442-445):
  - `PerThreadUnifiedRenameMap renameMap` — speculative map updated on
    rename
  - `PerThreadUnifiedRenameMap commitRenameMap` — committed map; this is
    what architectural reads see
  - Defined as `std::array<UnifiedRenameMap, MaxThreads>` (rename_map.hh:185)
- **Free list** (free_list.hh:124-192):
  - `UnifiedFreeList` holds `std::array<SimpleFreeList, CCRegClass+1>`
    of FIFO queues of free physical register IDs
  - `getReg(RegClassType type)` (line 162) — pop a free reg
  - `addReg(freed_reg)` (line 173) — push back on commit
- **Width parameters**:
  - `numPhysIntRegs = 256` (BaseO3CPU.py:174) — total physical int regs
  - `numPhysFloatRegs = 256` (BaseO3CPU.py:177)
  - `numPhysVecRegs = 256`, `numPhysVecPredRegs = 32`, `numPhysMatRegs = 2`,
    `numPhysCCRegs = 0` (BaseO3CPU.py:180-186)
  - `renameWidth = 8` (BaseO3CPU.py:108) — max renamed/cycle
- **Rename stage** (rename.cc) — does the bookkeeping:
  - `renameSrcRegs(inst, tid)` (rename.hh:253) — look up sources
  - `renameDestRegs(inst, tid)` (rename.hh:256) — allocate from free
    list, write speculative map
  - `removeFromHistory(seq_num, tid)` (rename.hh:250) — at commit
  - Per-thread `historyBuffer[MaxThreads]` (rename.hh:325) — list of
    `RenameHistory` (line 301) entries to undo on squash
  - `instsInProgress`, `loadsInProgress`, `storesInProgress[MaxThreads]`
    (rename.hh:378-388) — count of insts sent to back-end but not yet
    counted in occupancy
  - Stalls driven by: `calcFreeROBEntries`, `calcFreeIQEntries`,
    `calcFreeLQEntries`, `calcFreeSQEntries` (rename.hh:262-271)
- **LoC**: rename 1,515+545 = 2,060, plus rename_map 132+302 = 434, plus
  free_list 53+197 = 250. Total **~2,744 LoC**

### 1.5 Wakeup / select logic

This is **not** a separate class — it's spread across `inst_queue.cc`:

- **Wakeup** (dataflow): `wakeDependents()` walks
  `dependGraph<DynInstPtr>` (inst_queue.hh:501) and calls
  `addIfReady()` for each consumer whose all sources are ready
  (inst_queue.cc, in `wakeDependents` impl)
- **FU completion scheduling** (inst_queue.hh:185-213): each issued
  instruction gets a `FUCompletion` event scheduled at `curTick() +
  fuPool->getOpLatency(op_class)`; when the event fires, it calls
  `processFUCompletion()` which sets the instruction executed and adds
  it to the writeback queue. This is what allows **back-to-back
  scheduling** without speculative execution.
- **Select** (oldest across op classes): `getInstToExecute()` consults
  `readyInsts[Num_OpClasses]` priority queues and `listOrder` to pick
  the globally oldest ready instruction
- **Issue constraint**: `totalWidth` per cycle
  (inst_queue.hh:511, derived from `issueWidth`)

### 1.6 LSQ (Load-Store Queue) including store-forwarding

- **File**: `src/cpu/o3/lsq.hh` (1,007 lines), `src/cpu/o3/lsq.cc` (1,620 lines)
- **File**: `src/cpu/o3/lsq_unit.hh` (593 lines), `src/cpu/o3/lsq_unit.cc` (1,682 lines)
- **File**: `src/cpu/o3/mem_dep_unit.hh` (285 lines), `src/cpu/o3/mem_dep_unit.cc` (~similar)
- **File**: `src/cpu/o3/store_set.hh` (~200 lines) — store-set predictor
- **Two-level structure**:
  - `LSQ` (lsq.hh:76) — the per-CPU coordinator, holds the
    `DcachePort` (line 84) and `std::vector<LSQUnit> thread` (line 987)
  - `LSQUnit` (lsq_unit.hh:88) — per-thread, contains:
    - `StoreQueue storeQueue` (line 463) — `CircularQueue<SQEntry>`
    - `LoadQueue loadQueue` (line 466) — `CircularQueue<LQEntry>`
    - `MemDepUnit` (per-thread, in `inst_queue`)
- **`LSQRequest`** (lsq.hh:219-597) — per-memory-op state machine with
  flags (`IsLoad`, `WriteBackToRegister`, `Sent`, `Retry`, `Complete`,
  `TranslationSquashed`, `Discarded`, `LSQEntryFreed`, `WritebackScheduled`,
  `WritebackDone`, `IsAtomic`) (lines 226-254) and states (`NotIssued`,
  `Translation`, `Request`, `Fault`, `PartialFault`) (lines 257-264)
- **Width parameters**:
  - `LQEntries = 32` (BaseO3CPU.py:142)
  - `SQEntries = 32` (BaseO3CPU.py:143)
  - `LSQDepCheckShift = 4` (BaseO3CPU.py:144) — address shift for
    conservative mem-dep check
  - `LSQCheckLoads = True` (BaseO3CPU.py:147) — check load-load
    dependencies, not just load-store
  - `cacheStorePorts = 200` (BaseO3CPU.py:75) — per-cycle store
    writeback port count
  - `cacheLoadPorts = 200` (BaseO3CPU.py:78)
- **Memory dep predictor** (mem_dep_unit.hh:90-285): tracks store sets
  with `LFSTSize` (Last Fetched Store Table, BaseO3CPU.py:157),
  `SSITSize` (Store Set ID Table, line 158), `SSITAssoc` (line 159),
  `store_set_clear_period` (line 152)
- **Store forwarding algorithm** (lsq_unit.cc:1408-1591):
  1. On a load's execute, walk `storeQueue` from `load_inst->sqIt`
     backwards (older stores) (line 1412)
  2. For each candidate store, compute address overlap using
     `AddrRangeCoverage` enum (lsq_unit.hh:200-205):
     `NoAddrRangeCoverage` | `PartialAddrRangeCoverage` |
     `FullAddrRangeCoverage`
  3. **Full coverage** (line 1474-1551): copy store data into
     `load_inst->memData`, build a fake `ReadReq` packet, schedule a
     `WritebackEvent` at `curTick()` (1-cycle forwarding latency, see
     TODO at line 1543)
  4. **Partial coverage** (line 1552-1589): set `stalled = true`,
     remember `stallingStoreIsn` and `stallingLoadIdx`, call
     `iewStage->rescheduleMemInst(load_inst)`, clear issued, clear
     effAddr. The load will be re-issued after the partial store
     writebacks.
  5. **No coverage**: fall through to actual memory access (line 1593)
- **Forwarding latencies** (hard-coded as comments in lsq_unit.cc:1543):
  - 1 cycle for full store-to-load forward — explicitly marked
    `// @todo: Need to make this a parameter.`
  - This is a **DSE knob** ChipForge should parameterize from day 1
- **AMO/atomic support** (lsq.hh:599-621 `SingleDataRequest`,
  `SplitDataRequest` line 644-691): split requests for unaligned
  accesses; UnsquashableDirectRequest line 627 for TLB shootdowns
- **SMT resource sharing** (lsq.hh:951-967):
  - `maxLSQAllocation(SMTQueuePolicy, entries, numThreads, threshold)`
    implements `Dynamic` | `Partitioned` | `Threshold` policies for
    LQ/SQ per-thread caps
- **LoC**: lsq 1,620+1,007 = 2,627, lsq_unit 1,682+593 = 2,275,
  mem_dep_unit ~285+, store_set ~200+. Total **~5,400 LoC** — the
  largest OoO subsystem in gem5

---

## 2. Superscalar / multi-issue support

### 2.1 Width parameters (all in `BaseO3CPU.py`)

| Width param | Default | Lines | Used by |
|---|---|---|---|
| `fetchWidth` | 8 | 86 | `Fetch::fetch()` (per-cycle PC advance) |
| `fetchBufferSize` | 64 (bytes) | 87 | pre-decoded inst buffer |
| `fetchQueueSize` | 32 (micro-ops/thread) | 88-90 | per-thread fetch-queue |
| `decodeWidth` | 8 | 101 | `Decode::decode()` (per-cycle decode) |
| `renameWidth` | 8 | 108 | `Rename::renameInsts()` |
| `dispatchWidth` | 8 | 119 | IEW dispatch into IQ+LSQ |
| `issueWidth` | 8 | 120 | IQ scheduling per cycle |
| `wbWidth` | 8 | 121 | IEW writeback per cycle |
| `commitWidth` | 8 | 127 | Commit per cycle |
| `squashWidth` | Optional | 128 | If unset, instant squash |

**All eight widths default to 8 and are independent parameters** — you
can build a 4-wide fetch / 2-wide issue / 8-wide commit machine
asymmetrically. This is the OoO norm: most CPUs are fetch-bound or
issue-bound, not commit-bound.

### 2.2 How IQ/ROB scale with width

- **IQ**: `issueWidth` is multiplied into the `scheduleReadyInsts()`
  loop, which calls `getInstToExecute()` up to `totalWidth` times
  (inst_queue.hh:511, also referenced as `totalWidth` in
  inst_queue.hh:557 `IQStats` constructor parameter)
- **ROB**: `commitWidth` directly limits how many `retireHead(tid)`
  calls happen per cycle in `Commit::commitInsts()` (commit.cc,
  not read in detail but parameter is `const unsigned commitWidth`
  in commit.hh:397)
- **Squash handling**: when `squashWidth` is unset, the whole
  `instList[MaxThreads]` is cleared in one cycle; when set, the
  ROB uses a persistent `InstIt squashIt[MaxThreads]` iterator
  (rob.hh:316) and removes `squashWidth` entries per cycle

### 2.3 Functional Unit pool (the issue-side hardware)

- **File**: `src/cpu/o3/fu_pool.hh` (212 lines), `src/cpu/o3/fu_pool.cc` (similar)
- **File**: `src/cpu/o3/FuncUnitConfig.py` — Python declaration of FU
  counts and latencies
- **Class**: `FUPool : public SimObject` (fu_pool.hh:75) with:
  - `std::array<Cycles, Num_OpClasses> maxOpLatencies` (line 79)
  - `std::array<bool, Num_OpClasses> pipelined` (line 81)
  - `std::bitset<Num_OpClasses> capabilityList` (line 84)
  - `std::vector<bool> unitBusy` (line 87)
  - Per-op-class `FUIdxQueue fuPerCapList[Num_OpClasses]` (line 126)
    — round-robin allocation
- **Key methods**:
  - `getUnit(OpClass capability)` (line 178) — returns FU idx or
    `NoCapableFU=-2` / `NoFreeFU=-1`
  - `getOpLatency(OpClass)` (line 193) — execute latency
  - `isPipelined(OpClass)` (line 198) — whether back-to-back issue
    allowed
- **Allows** declaring a machine with N IntALUs, M FPUs, etc. via the
  Python `FuncUnitConfig` class
- **FUPool declaration in BaseO3CPU**: not shown in BaseO3CPU.py
  lines 1-259 because it's a per-arch config (e.g.,
  `src/arch/arm/ArmCPU.py`); the default uses
  `src/cpu/o3/FUPool.py` which sets up generic IntALU/IntMultDiv/FPAlu/
  FPMultDiv/ReadPort/WritePort/SIMD_*

### 2.4 Stage delays (also part of the multi-issue parameters)

- `fetchToBacDelay`, `bacToFetchDelay` (BaseO3CPU.py:81,99)
- `fetchToDecodeDelay`, `decodeToFetchDelay` (lines 100,82)
- `decodeToRenameDelay`, `renameToDecodeDelay` (lines 107,92)
- `renameToIEWDelay`, `iewToRenameDelay` (lines 113-115, 103-105)
- `iewToCommitDelay`, `commitToIEWDelay` (lines 110-112, 123-125)
- `renameToROBDelay` (line 126)
- `issueToExecuteDelay` (line 116-118) — internal to IEW, controls
  how many cycles between issue and execution
- `backComSize`, `forwardComSize` (lines 135-140) — TimeBuffer sizes
  for inter-stage communication (default 5)

These all default to 1 cycle and are **separate** from widths; the
O3CPU has its own time-buffer system (TimeBuffer<TimeStruct> etc.)
modelled in `src/cpu/timebuf.hh` and `src/cpu/o3/comm.hh`.

### 2.5 Decoupled front-end (FTQ)

- `decoupledFrontEnd = Bool(False)` (BaseO3CPU.py:222) — turns on
  Fetch Target Queue (FTQ)
- `numFTQEntries = 8` (line 223-227) — max in-flight fetch targets
- `fetchTargetWidth = 32 bytes` (line 233-237) — bytes per FT
- `maxFTPerCycle = 4` (line 238)
- `maxTakenPredPerCycle = 1` (line 239-241)
- `minInstSize = 1 byte` (line 228-232) — minimum fetch granularity
- `BAC` (Branch Address Calculation) is a **separate stage** added
  between fetch and decode when decoupled FE is on
  (cpu.hh:415, bac.cc/hh)

This decoupled FE is **not** classical multi-issue but is the modern
high-performance alternative — it allows fetch to run ahead and feed
multiple fetch targets into the back-end without stalling on taken
branches.

### 2.6 LoC summary for multi-issue

- `cpu.cc/hh` already covers it (no extra code needed for widths —
  they're loops over `xxxWidth`)
- `fu_pool.cc/hh` is the only dedicated FU pool code (~400 LoC)
- `decoupledFrontEnd` + `ftq.cc/hh` adds another ~600 LoC if enabled

---

## 3. SMT (Simultaneous Multithreading)

### 3.1 ThreadState / TC plumbing

- **File**: `src/cpu/o3/thread_state.hh`, `src/cpu/o3/thread_state.cc`
- **`BaseCPU::numThreads`** (base.hh:414) — the actual SMT thread count
  (`<= SMT_MAX_THREADS`, a compile-time constant)
- **`BaseCPU::threadContexts`** (base.hh:286) — `std::vector<ThreadContext*>`
  indexed by ThreadID
- **O3 `CPU::thread`** (cpu.hh:561) — `std::vector<ThreadState*>` —
  per-thread state with PC, LR, syscall, etc.
- **`CPU::activeThreads`** (cpu.hh:451) — `std::list<ThreadID>` — the
  currently-scheduled threads (e.g. some may be suspended)
- **`updateThreadPriority()`** (cpu.hh:238) — reorders `activeThreads`

### 3.2 IEW (Issue/Execute/Writeback) for SMT

- **File**: `src/cpu/o3/iew.hh:87-484`, `src/cpu/o3/iew.cc:1-1628`
- **Key insight** (iew.hh:69-86, the docstring):

  > "IEW handles both single threaded and SMT IEW
  > (issue/execute/writeback). It handles the dispatching of
  > instructions to the LSQ/IQ as part of the issue stage"

  This is the **only** major component that explicitly handles SMT —
  IEW is where the I-Q and LSQ are shared between threads.

- **Per-thread IQ tracking** (inst_queue.hh:155):
  - `unsigned count[MaxThreads]` — per-thread count
  - `unsigned maxEntries[MaxThreads]` — per-thread cap
  - `int entryAmount(ThreadID num_threads)` (inst_queue.hh:170-178) —
    computes per-thread allocation under the policy
  - `resetEntries()` (inst_queue.cc:153-168) — recalculates on thread
    activation/deactivation
- **Per-thread LSQ tracking** (lsq_unit.hh):
  - `std::vector<LSQUnit> thread` (lsq.hh:987) — one LSQUnit per thread
  - `maxLSQAllocation(SMTQueuePolicy, entries, numThreads, threshold)`
    (lsq.hh:951-967) — caps per-thread LQ+SQ
- **Dispatch loop** (iew.cc:1448-1453) — the SMT dispatch:

  ```cpp
  for (ThreadID tid : *activeThreads) {
      checkSignalsAndUpdate(tid);
      dispatch(tid);
  }
  ```

  Each thread can dispatch up to `dispatchWidth` insts/cycle
  (subject to IQ/LSQ capacity). **There is no per-thread fairness
  enforcement here** — the loop just walks the list in priority order.

- **SMT policies** (`src/cpu/o3/SMT.py`):
  - `SMTFetchPolicy`: `RoundRobin` | `Branch` | `IQCount` | `LSQCount`
    (line 42-43) — which thread to fetch from
  - `SMTQueuePolicy`: `Dynamic` | `Partitioned` | `Threshold`
    (line 46-47) — how IQ/ROB/LSQ are shared
  - `CommitPolicy`: `RoundRobin` | `OldestReady` (line 50-51) — which
    thread to commit from
  - Param defaults: `smtFetchPolicy = "RoundRobin"`,
    `smtLSQPolicy = "Partitioned"`, `smtROBPolicy = "Partitioned"`,
    `smtCommitPolicy = "RoundRobin"` (BaseO3CPU.py:191-200)
  - `smtNumFetchingThreads = 1` (line 190) — how many threads fetch
    simultaneously (typically 1-2 for 2-way SMT)
  - `smtLSQThreshold = 100`, `smtROBThreshold = 100` (line 195,199)

### 3.3 Resource partitioning

The shared structures use one of three policies (defined
`enums/SMTQueuePolicy.hh` and used at multiple sites):

| Policy | ROB | IQ | LSQ |
|---|---|---|---|
| **Dynamic** | any thread can use all entries | same | same |
| **Partitioned** | each gets `numEntries / numThreads` | each IQ unit allocates `numEntries / numThreads` | each thread gets `entries / numThreads` |
| **Threshold** | each can use up to `threshold%` of total | same | same |

**Code sites**:
- ROB: `entryAmount(tid)` (rob.cc, calls
  `ROB::robPolicy` initialized in constructor from
  `params.smtROBPolicy`)
- IQ: `IQUnit::IQUnit` (inst_queue.cc:67-116) — handles the three
  policies at IQUnit construction
- LSQ: `LSQ::maxLSQAllocation` (lsq.hh:951-967)

### 3.4 Fetch policy (which thread gets fetched)

- `SMTFetchPolicy` controls which thread's PC is used for the next
  fetch
- `RoundRobin`: alternate threads each cycle
- `IQCount`: pick the thread with fewest IQ entries
- `LSQCount`: pick the thread with fewest LSQ entries
- `Branch`: pick the thread with no in-flight branch

### 3.5 Commit policy

- `Commit::getCommittingThread()` (commit.hh:300) — selects thread
  to commit
- `Commit::roundRobin()` (commit.hh:303) — implementation
- `Commit::oldestReady()` (commit.hh:306) — picks thread whose head
  ROB inst is oldest
- Uses `priority_list` member (commit.hh:378) — ordered list of
  ThreadIDs to try in turn

### 3.6 SMT-specific stats

- `IEWStats::instsToCommit`, `writebackCount`, `producerInst`,
  `consumerInst` are all `init(cpu->numThreads)` (iew.cc:217-230) —
  per-thread counters
- `IEW::writebackInsts()` (iew.cc:1380-1427) iterates per thread
  using `inst->threadNumber` (line 1390)
- `IEW::squash(tid)` is per-thread (iew.cc:438-470)

### 3.7 LoC for SMT support

- SMT is **not** a separate code block — it's **scattered** across
  every stage with `for (ThreadID tid : *activeThreads)` loops
  (grep `src/cpu/o3` finds 9 files: bac.cc, commit.cc, lsq.cc,
  rename.cc, inst_queue.cc, iew.cc, rob.cc, decode.cc, fetch.cc).
- The per-thread data structures (parallel arrays of size
  `MaxThreads`) are the dominant pattern
- The SMT policies themselves are a few hundred LoC total in
  `commit.cc` and `inst_queue.cc`

---

## 4. Branch prediction variants

### 4.1 Common wrapper: `BPredUnit`

- **File**: `src/cpu/pred/bpred_unit.hh` (516 lines),
  `src/cpu/pred/bpred_unit.cc` (~600 lines)
- **C++ class**: `branch_prediction::BPredUnit : public SimObject`
  (bpred_unit.hh:71)
- **Role**: thin wrapper that **composes** a `BTB`, a `RAS`, a
  `ConditionalPredictor`, and an `IndirectPredictor` into a single
  "branch predictor" object that fetch can call
- **API** (bpred_unit.hh:97-128):
  - `predict(inst, seqNum, pc, tid)` — returns whether taken + target
  - `update(done_sn, tid)` — commit-time update
  - `squash(squashed_sn, tid)` — undo speculatively updated state
  - `squash(squashed_sn, corr_target, actually_taken, tid, from_commit)`
    — corrective update on mispredict
- **Selection**: via Python parent parameters (see §4.5)

### 4.2 Sub-predictor parameterization

From `src/cpu/pred/BranchPredictor.py:199-247` (the `BranchPredictor`
class), the BP is composed of:

| Sub-predictor | Python class | Default | Lines |
|---|---|---|---|
| `btb` (Branch Target Buffer) | `Param.BranchTargetBuffer(SimpleBTB())` | SimpleBTB (4096 entries) | 236 |
| `ras` (Return Address Stack) | `Param.ReturnAddrStack(ReturnAddrStack())` | 16 entries | 237-239 |
| `conditionalBranchPred` | `Param.ConditionalPredictor` | **TournamentBP**(numThreads=Parent.numThreads) | 240-242, 202-207 |
| `indirectBranchPred` | `Param.IndirectPredictor(SimpleIndirectPredictor())` | SimpleIndirectPredictor | 243-247 |

### 4.3 TournamentBP

- **File**: `src/cpu/pred/tournament.hh`, `src/cpu/pred/tournament.cc`
- **Python params** (BranchPredictor.py:267-278):
  - `localPredictorSize = 2048` (2-bit local PHT)
  - `localCtrBits = 2`
  - `localHistoryTableSize = 2048` (per-branch history table)
  - `globalPredictorSize = 8192` (global 2-bit PHT)
  - `globalCtrBits = 2`
  - `choicePredictorSize = 8192` (chooser)
  - `choiceCtrBits = 2`
- **3 sub-tables**: local (per-branch history), global (GHR-indexed),
  choice (meta-predictor that picks between local and global)

### 4.4 GShareBP

- **File**: `src/cpu/pred/gshare.hh`, `src/cpu/pred/gshare.cc`
- **Python params** (BranchPredictor.py:1162-1168):
  - `global_predictor_size = 512` — size of GHR-XOR-indexed PHT
  - `global_counter_bits = 2` — counter width
- **Single table** indexed by `PC XOR GHR`
- **LoC**: smallest of the conditional predictors (~150 LoC impl)

### 4.5 BiModeBP

- **File**: `src/cpu/pred/bi_mode.hh`, `src/cpu/pred/bi_mode.cc`
- **Python params** (BranchPredictor.py:281-289):
  - `globalPredictorSize = 8192`
  - `globalCtrBits = 2`
  - `choicePredictorSize = 8192`
  - `choiceCtrBits = 2`
- **2 PHTs + a choice table** — uses "bias bits" to reduce
  aliasing vs. gshare
- **LoC**: ~300 LoC

### 4.6 TAGE

- **Files**:
  - `src/cpu/pred/tage.hh`, `src/cpu/pred/tage.cc`
  - `src/cpu/pred/tage_base.hh`, `src/cpu/pred/tage_base.cc` (~1500 LoC
    for the TAGE core)
  - `src/cpu/pred/tage_sc_l.hh`, `src/cpu/pred/tage_sc_l.cc` (TAGE-SC-L
    combination)
  - `src/cpu/pred/ltage.hh`, `src/cpu/pred/ltage.cc` (LTAGE = TAGE + Loop)
  - `src/cpu/pred/loop_predictor.hh`, `src/cpu/pred/loop_predictor.cc`
  - `src/cpu/pred/statistical_corrector.hh`,
    `src/cpu/pred/statistical_corrector.cc`
- **Python params** (BranchPredictor.py:292-368 for `TAGEBase`):
  - `nHistoryTables = 7` (line 302) — number of TAGE component tables
  - `minHist = 5` (line 303) — minimum history length
  - `maxHist = 130` (line 304) — maximum history length
  - `tagTableTagWidths = [0,9,9,10,10,11,11,12]` (line 306-308) — per-table
    tag widths
  - `logTagTableSizes = [13,9,9,9,9,9,9,9]` (line 309-311) — per-table
    log2 of size
  - `tagTableCounterBits = 3` (line 318) — prediction counter width
  - `tagTableUBits = 2` (line 319) — usefulness counter width
  - `histBufferSize = 2097152` (line 321-324) — speculative path buffer
  - `pathHistBits = 16` (line 326) — path history
  - `logUResetPeriod = 18` (line 327-329) — useful-counter reset
  - `numUseAltOnNa = 1` (line 330) — USE_ALT_ON_NA counter count
  - `initialTCounterValue = 1 << 17` (line 331)
  - `useAltOnNaBits = 4` (line 332)
  - `maxNumAlloc = 1` (line 334-336) — max entries allocated on
    mispredict
  - `noSkip = []` (line 339) — vector of enabled tables
- **LTAGE** adds a Loop Predictor (`logSizeLoopPred`, `withLoopBits`,
  `loopTableAgeBits`, `loopTableConfidenceBits`, `loopTableTagBits`,
  `loopTableIterBits`, `logLoopTableAssoc`) (lines 373-413)
- **TAGE-SC-L** (8KB and 64KB variants in BranchPredictor.py:417-817)
  adds a Statistical Corrector
- **MultiperspectivePerceptron** (BranchPredictor.py:820-1160) — the
  "MPP" / "ageedor" perceptron, ~30+ parameters including
  `budgetbits`, `num_local_histories`, `block_size`, `pcshift`,
  `threshold`, `bias0/1/mostly0/mostly1`, `nbest`, `tunebits`, `fudge`,
  `n_sign_bits`, `imli_mask1/4`, `recencypos_mask`, `decay`,
  `record_mask`, `hash_taken`, `tuneonly`, `extra_rounds`, `speed`,
  `initial_theta`
- **LoC**: TAGE alone is the largest single predictor — tage_base.cc is
  ~1,500 LoC; tage.cc ~300; tage_sc_l.cc ~500; tage_sc_l_64KB.cc ~400;
  tage_sc_l_8KB.cc ~400; multiperspective_perceptron.cc ~2,000+;
  loop_predictor.cc ~400. **Total TAGE family ~5,000+ LoC**

### 4.7 LocalBP (2-bit local predictor, reference)

- **File**: `src/cpu/pred/2bit_local.hh`, `src/cpu/pred/2bit_local.cc`
- **Python params** (BranchPredictor.py:258-264):
  - `localPredictorSize = 2048`
  - `localCtrBits = 2`
- **LoC**: ~80 (smallest)

### 4.8 BTB variants

- **Abstract**: `BranchTargetBuffer(ClockedObject)` (BranchPredictor.py:81-87)
  - `numThreads = Param.Unsigned(Parent.numThreads, "Number of threads")`
    (line 87) — so BTB is already per-thread-aware
- **`SimpleBTB`** (BranchPredictor.py:122-145):
  - `numEntries = 4096`
  - `tagBits = 16`
  - `associativity = 1`
  - `btbReplPolicy` (LRU default), `btbIndexingPolicy`
- **File**: `src/cpu/pred/simple_btb.hh`, `src/cpu/pred/simple_btb.cc`
  (~400 LoC)
- **`BTBSetAssociative`** indexing policy
  (BranchPredictor.py:101-119):
  - `num_entries`, `assoc`, `set_shift`, `tag_bits`, `numThreads`
- **File**: `src/cpu/pred/btb_entry.hh` (template-based)

### 4.9 RAS (Return Address Stack)

- **File**: `src/cpu/pred/ras.hh`, `src/cpu/pred/ras.cc`
- **Python** (BranchPredictor.py:72-78):
  - `numThreads = Param.Unsigned(Parent.numThreads)`
  - `numEntries = Param.Unsigned(16)`

### 4.10 Indirect predictor

- **`SimpleIndirectPredictor`** (BranchPredictor.py:173-196):
  - `indirectSets = 256` — target cache sets
  - `indirectWays = 2` — ways
  - `indirectTagSize = 16` — tag bits
  - `indirectPathLength = 3` — path history
  - `speculativePathLength = 256` — speculative path buffer
  - `indirectGHRBits = 13`

### 4.11 DSE knobs for branch predictors

The full set of branch-predictor parameters that DSE can sweep:

1. **Type** (`BranchPredictor.conditionalBranchPred`):
   `LocalBP` | `TournamentBP` | `BiModeBP` | `TAGE` | `LTAGE` |
   `TAGE_SC_L_8KB` | `TAGE_SC_L_64KB` | `MultiperspectivePerceptron8KB`
   | `MultiperspectivePerceptron64KB` |
   `MultiperspectivePerceptronTAGE8KB` |
   `MultiperspectivePerceptronTAGE64KB` | `GShareBP`
2. **BTB**: `numEntries ∈ {256, 512, 1024, 2048, 4096}`,
   `associativity ∈ {1, 2, 4}`
3. **RAS**: `numEntries ∈ {8, 16, 32, 64}`
4. **Conditional params** (per predictor): see §4.3-4.7
5. **`speculativeHistUpdate`** (BranchPredictor.py:158, 214-217) —
   bool: whether to update histories speculatively vs. only at commit
6. **`requiresBTBHit`** (line 219-227) — for advanced front-ends
7. **`updateBTBAtSquash`** (line 228-234) — BTB update timing
8. **`takenOnlyHistory`** (line 250-255) — modern server CPU mode
9. **`numThreads`** — propagated to all sub-predictors

---

## 5. DSE pattern — how gem5 parameterizes CPU families

### 5.1 CPU family hierarchy

gem5 ships **three major CPU families**, each in its own subdirectory:

| Family | Location | Style | Class | LoC estimate |
|---|---|---|---|---|
| `Simple` (Atomic / Timing) | `src/cpu/simple/` | Single-cycle or simple timing | `BaseAtomicSimpleCPU`, `BaseTimingSimpleCPU` | ~2,000 |
| `Minor` (in-order pipeline) | `src/cpu/minor/` | Fixed 4-stage in-order | `MinorCPU` | ~3,000 |
| `O3` (OoO) | `src/cpu/o3/` | 7+1 stage OoO | `O3CPU` (subclasses `BaseO3CPU` per-ISA) | ~15,000 |

All three inherit from `BaseCPU` (base.hh:106) and differ only in the
tick implementation and per-stage machinery. They are selected at
**config-script time** (e.g., the user's `configs/example/se.py`).

### 5.2 Per-ISA O3 subclasses

- **`O3CPU.py`** (src/cpu/o3/O3CPU.py:26-54): dynamic ISA selection
  - Imports `ArmO3CPU`, `MipsO3CPU`, `PowerO3CPU`, `RiscvO3CPU`,
    `SparcO3CPU`, `X86O3CPU` based on `buildEnv[USE_*_ISA]`
  - Each per-ISA subclass provides ISA-specific decode, register file,
    and a per-ISA `O3Checker` if needed
- **`BaseO3CPU.py`** is the **shared** parameter declaration for all
  O3 CPUs (the one we read in detail)

### 5.3 Parameterization via SimObject parent

The gem5 DSE pattern is:

1. **All CPU models are `SimObject`s** (Python class inheriting
   `BaseCPU`/`SimObject`).
2. **All parameters are declared as `Param.*` in the Python class**
   (e.g., `fetchWidth = Param.Unsigned(8, "Fetch width")`).
3. **Sub-components are also `SimObject`s**, parameterized independently,
   and **passed as parent params**. Example: `branchPred =
   Param.BranchPredictor(BranchPredictor(conditionalBranchPred=
   TournamentBP(...)), "Branch Predictor")` (BaseO3CPU.py:202-207).
4. **The Python config script chooses the exact combination** of
   widths, predictor type, IQ size, etc.
5. **Sweeping** is done by writing multiple config scripts (e.g.,
   `o3-feng-4wide.py`, `o3-feng-8wide.py`) or by using the
   `m5.commands` mechanism to override parameters at instantiation
   time.
6. **No built-in sweep harness** — gem5 expects users to write shell
   loops or use their own scripts (e.g., `gem5art`).

### 5.4 O3 parameters that are "hard to vary"

These parameters are **declared** in `BaseO3CPU.py` and can in theory
be swept, but in practice they are rarely changed because changing
them invalidates compile-time assumptions:

- `MaxThreads` (in `src/cpu/o3/limits.hh`) — a compile-time
  `static constexpr int` (typically 4 or 8). All `instList[MaxThreads]`
  arrays are sized at this. **Requires recompile** to change.
- `MaxWidth` (same file) — compile-time max for `fetchWidth`,
  `issueWidth`, `wbWidth`, etc. Currently set so the default 8 fits.
  `IEW::IEW()` checks `if (issueWidth > MaxWidth) fatal(...)` at
  runtime (iew.cc:97-100), so widening needs a recompile.

### 5.5 What gem5 DSE *doesn't* do

- No automatic Pareto-frontier search — users write their own
- No per-knob validation against valid ranges — the SimObject
  framework does check types but doesn't reject "out of range"
  numeric values
- No automatic sweep across all CPU families — you have to write
  configs for each
- No notion of "preset DSE points" (e.g., "low power", "high perf")
  — users compose manually

---

## 6. What this means for ChipForge

### 6.1 The plugin abstraction is more aggressive than gem5

ChipForge's plugin model (`cf::plugin::PluginBase` + `cf::plugin::PipeBuilder`)
aims to make the **entire CPU** pluggable. gem5 only makes the
**branch predictor** pluggable (the rest of the O3 pipeline is one
giant C++ class). The two architectures map as follows:

| ChipForge concept | gem5 equivalent | gem5 pluggable? |
|---|---|---|
| `PluginBase::build()` | (not a gem5 concept) | — |
| `PipeBuilder::at_stage(name, phase, cb)` | (not a gem5 concept — gem5 uses C++ class composition) | — |
| `BranchPredictorPlugin` (planned template params) | `BPredUnit` + sub-predictors | ✅ Yes |
| `HazardPlugin` | `Scoreboard` (src/cpu/o3/scoreboard.hh, 45 LoC) — trivial | Could be |
| `RegFilePlugin` | `PhysRegFile` (src/cpu/o3/regfile.hh) | Not really |
| `RiscvIntAluPlugin` | (ISA-specific extension) | Not really |
| (none yet) | `ROB`, `IQ`, `LSQ`, `Rename` | ❌ No |
| (none yet) | `IEW` itself (the dispatch+issue+execute+writeback glue) | ❌ No |

**Key gap**: ChipForge currently has no abstraction for ROB, IQ, LSQ,
Rename, Commit, or the IEW glue. If these are designed as concrete
classes (not plugins), future OoO work will require **massive
refactoring** of the in-order pipeline to insert them.

### 6.2 Recommended path: define OoO as a "shell" plugin family

Instead of trying to make ROB/IQ/LSQ each a plugin (which would
fragment the implementation), the recommended approach is to define a
**single OoO shell plugin** that owns all of ROB/IQ/LSQ/Rename/Commit
internally and **emits** hooks for the existing in-order plugins to
attach to. This mirrors gem5's approach (a monolithic O3 class with
SimObject sub-predictors).

Concretely, the recommended Phase 1 abstractions (see §7) are
**minimal** — just enough to allow an OoO implementation to be added
in Phase 5+ without breaking existing plugins.

### 6.3 Parameterization lessons

gem5's pattern of `Param.Unsigned(width, "description")` with default
values, declared once in Python, mapped to C++ via code generation, is
**already** what ChipForge's JSON-schema approach aims to do. The
main missing piece is **typed range constraints** (gem5 doesn't
enforce them; it relies on `fatal()` at runtime — see
`if (issueWidth > MaxWidth) fatal(...)` iew.cc:97).

### 6.4 What ChipForge should NOT try to copy

1. **`TimeBuffer<T>`** (src/cpu/timebuf.hh) — gem5's inter-stage
   communication. It is ~300 LoC and is tightly coupled to the O3
   stage decomposition. **Don't import this abstraction**; it's
   gem5-specific.
2. **The 7-stage CPU class composition** — `class CPU { Fetch f;
   Decode d; Rename r; IEW i; ... };` — this is C++ value-member
   composition with hand-wired `setXxxQueue()` calls
   (e.g., cpu.cc ~1507 LoC). It is not a plugin pattern; trying to
   replicate it 1:1 would not add value.
3. **The branch-predictor-as-SimObject pattern** is good and reusable
   in principle, but ChipForge's `BranchPredictorPlugin` already
   captures it.

### 6.5 What ChipForge SHOULD copy

1. **Width parameters declared once, in a single place**
   (`BaseO3CPU.py` lines 86-128). ChipForge's `CPUConfig` should
   follow the same pattern.
2. **SMT queue policies as a closed enum** (`SMT.py:42-50`):
   `Dynamic` | `Partitioned` | `Threshold` for IQ/ROB/LSQ.
   ChipForge's future OoO can adopt the same names.
3. **Compile-time `MaxThreads` and `MaxWidth`** as `static constexpr`
   (in `src/cpu/o3/limits.hh`). These bound the per-thread arrays
   and the per-stage width checks. ChipForge's `PipelineLimits` could
   be analogous.
4. **A per-`OpClass` issue priority queue** for age-ordered
   wakeup/select. The `ReadyInstQueue readyInsts[Num_OpClasses]` plus
   `listOrder` age-ordering (inst_queue.hh:453,480) is a clean
   design.
5. **Per-thread circular queues for LQ and SQ**
   (`CircularQueue<LQEntry>`, `CircularQueue<SQEntry>` in
   lsq_unit.hh:208-209). The `LSQ` parameterization is exemplary:
   one `LSQUnit` per thread, owned by a coordinator `LSQ`.

---

## 7. Concrete API additions for Phase 1

To enable future OoO without rewriting `CpuFactory` or the existing
11 plugins, Phase 1 should add the following minimal abstractions
(references to current ChipForge code in
`/workspace/project/ChipForge/include/cf/plugin/pipe_builder.h:54-172`
and `/workspace/project/ChipForge/include/cf/plugin/plugin_base.h:48-72`).

### 7.1 `PipeBuilder` extensions

| Addition | Purpose | OoO enabler |
|---|---|---|
| `void bind_payload_layout(Layout)` | Declare the PipeNode payload schema (fields, widths) | OoO needs DynInst-like payload shared across stages |
| `void issue_queue(std::shared_ptr<IssueQueue> iq)` | Register a pluggable IQ | OoO IQ |
| `void lsq(std::shared_ptr<LSQ> lsq)` | Register a pluggable LSQ | OoO LSQ (with store-fwd) |
| `void rob(std::shared_ptr<ROB> rob)` | Register a pluggable ROB | OoO ROB |
| `void commit(std::shared_ptr<Commit> c)` | Register pluggable commit | OoO commit |
| `void rename_unit(std::shared_ptr<RenameTable> rt)` | Register a pluggable rename table | OoO rename |
| `void smt_thread_count(unsigned n)` | Declare SMT thread count (default 1) | Forward-compat with SMT |
| `void smt_queue_policy(SMTQueuePolicy)` | IQ/ROB/LSQ sharing policy (Dynamic/Partitioned/Threshold) | Forward-compat with SMT |
| `unsigned smt_max_threads() const` | Compile-time max | Forward-compat |
| `unsigned width_xxx() const` accessors for `fetchWidth`, `issueWidth`, etc. | Plugins can read the configured width | Multi-issue DSE |

### 7.2 `PluginBase` extensions

| Addition | Purpose | OoO enabler |
|---|---|---|
| `virtual void on_issue(const Inst&)` | Optional hook for IQ entry | OoO IQ can call plugins when an inst is "issued" |
| `virtual void on_complete(const Inst&)` | Optional hook for writeback | OoO writeback can wake plugins |
| `virtual void on_commit(const Inst&)` | Optional hook for retire | OoO commit can notify plugins |
| `virtual void on_squash(SequenceNum)` | Optional hook for squash | OoO branch mispredict / mem violation |
| `virtual unsigned issue_latency(OpClass) const` | Optional FU latency query | FU-pool parameterization |
| `virtual bool is_capable(OpClass) const` | Optional FU capability | FU-pool parameterization |

These can all default to no-op (return 0 / false), so existing
in-order plugins need not change.

### 7.3 New payload fields

The current `DecodePayload` (referenced in `ip/cpu/docs/dse_architecture.md:166-167`)
lacks OoO fields. Add:

- `InstSeqNum seq_num` — global sequence number
- `PhysRegIdPtr phys_dest[4]` — renamed destination (vs. arch)
- `PhysRegIdPtr phys_src[4]` — renamed sources
- `RobIdx rob_idx` — index into ROB
- `bool in_iq`, `bool issued`, `bool executed`, `bool can_commit` —
  pipeline state bits
- `OpClass op_class` — already implied by `op_class` field; verify
  it's the same enum
- `bool is_load`, `bool is_store`, `bool is_atomic`, `bool is_branch` —
  gating flags (probably already derivable from the RISC-V detail
  payload)

### 7.4 New `OpClass` enum

Already partially exists (referenced in `inst_queue.hh` and
`fu_pool.hh:84`, `op_class.hh` is in `src/cpu/`). Need to mirror
gem5's values:

```
IntAlu, IntMult, IntDiv,
FloatAlu, FloatMult, FloatDiv, FloatSqrt, FloatCvt, FloatMultAcc,
SimdAdd, SimdAlu, SimdCmp, SimdCvt, SimdMisc, SimdMult, SimdMultAcc,
SimdShift, SimdSqrt, SimdFloatAdd, SimdFloatAlu, SimdFloatCmp,
SimdFloatCvt, SimdFloatDiv, SimdFloatMisc, SimdFloatMult,
SimdFloatMultAcc, SimdFloatSqrt, SimdShiftAcc,
SimdReduce, SimdReduceAdd, SimdReduceAlu, SimdReduceCmp,
SimdReduceCvt, SimdReduceMisc, SimdReduceMult, SimdReduceShift,
SimdReduceSqrt,
MemRead, MemWrite, MemAtomic, MemCast,
Branch, IndirectBranch, CallDirect, CallIndirect, Return,
No_OpClass
```

A Phase 1 minimum could be just `IntAlu | FloatAlu | MemRead |
MemWrite | Branch | No_OpClass` to keep the surface small.

### 7.5 New `SMTQueuePolicy` enum

```cpp
enum class SMTQueuePolicy { Dynamic, Partitioned, Threshold };
```

— exactly mirroring `src/cpu/o3/SMT.py:46-47`.

---

## 8. Approximate LoC budget for an OoO port

If ChipForge later implements OoO, the expected LoC, based on gem5's
distillation:

| Subsystem | gem5 LoC | ChipForge effort (educated guess) |
|---|---|---|
| `PipeBuilder` + `PluginBase` extensions (Phase 1) | — | ~400 |
| `DynInst` payload extensions | (gem5 has `dyn_inst.cc` 459 + `dyn_inst.hh` ~1,200) | ~1,000 |
| `ROB` | 875 | ~500 (smaller — no need to replicate all of gem5's edge cases) |
| `IQ` (age-ordered priority queue) | 2,344 | ~1,500 |
| `Rename` + `FreeList` + `RenameMap` | 2,744 | ~1,500 |
| `LSQ` + `LSQUnit` + `MemDep` | 5,400 | ~3,000 (most of the complexity is in store-fwd and dep prediction) |
| `IEW` glue (dispatch + issue + writeback) | 1,628 + 484 | ~1,000 |
| `Commit` (with SMT policies) | 1,537 + 504 | ~800 |
| SMT thread-state plumbing | scattered ~500 | ~300 |
| **Total OoO port** | **~15,000** | **~10,000** |

The OoO port is roughly **5× the size** of the current ChipForge CPU
IP (`ip/cpu/` is ~3,000 LoC of plugin code today). This is why
deferring it to Phase 5+ is appropriate, but also why Phase 1 must
lay the **abstraction groundwork** to avoid a 10,000-line
find-and-replace when OoO lands.

---

## 9. File:line index of all citations

This is a flat list, sorted by file, for quick lookup.

### gem5 source — base / pred / simple / minor

- `src/cpu/base.hh:106` — `class BaseCPU : public ClockedObject`
- `src/cpu/base.hh:202,210` — `virtual Port& getDataPort/getInstPort`
- `src/cpu/base.hh:286` — `std::vector<ThreadContext*> threadContexts`
- `src/cpu/base.hh:414` — `ThreadID numThreads`
- `src/cpu/base.hh:600-833` — `FetchCPUStats`, `ExecuteCPUStats`,
  `CommitCPUStats` structs
- `src/cpu/pred/BranchPredictor.py:72-78` — `ReturnAddrStack`
- `src/cpu/pred/BranchPredictor.py:81-87` — `BranchTargetBuffer`
- `src/cpu/pred/BranchPredictor.py:90-119` — `BTBSetAssociative`
- `src/cpu/pred/BranchPredictor.py:122-145` — `SimpleBTB`
- `src/cpu/pred/BranchPredictor.py:199-255` — `BranchPredictor` (composing class)
- `src/cpu/pred/BranchPredictor.py:258-264` — `LocalBP`
- `src/cpu/pred/BranchPredictor.py:267-278` — `TournamentBP`
- `src/cpu/pred/BranchPredictor.py:281-289` — `BiModeBP`
- `src/cpu/pred/BranchPredictor.py:292-352` — `TAGEBase` (the big one)
- `src/cpu/pred/BranchPredictor.py:356-362` — `TAGE`
- `src/cpu/pred/BranchPredictor.py:373-413` — `LoopPredictor`
- `src/cpu/pred/BranchPredictor.py:417-580` — `TAGE_SC_L_TAGE`,
  `TAGE_SC_L_64KB`, `TAGE_SC_L_8KB`
- `src/cpu/pred/BranchPredictor.py:587-595` — `LTAGE`
- `src/cpu/pred/BranchPredictor.py:615-705` — `StatisticalCorrector`
- `src/cpu/pred/BranchPredictor.py:798-817` — `TAGE_SC_L_64KB`,
  `TAGE_SC_L_8KB` (concrete)
- `src/cpu/pred/BranchPredictor.py:820-1160` — `MultiperspectivePerceptron`
  family (8KB / 64KB, TAGE variants)
- `src/cpu/pred/BranchPredictor.py:1162-1168` — `GShareBP`
- `src/cpu/pred/bpred_unit.hh:71` — `class BPredUnit : public SimObject`
- `src/cpu/pred/bpred_unit.hh:97-128` — predict / update / squash API
- `src/cpu/simple/base.hh:83-100` — `BaseSimpleCPU` (the third family)

### gem5 source — O3 CPU

- `src/cpu/o3/BaseO3CPU.py:56-259` — full parameter declaration
- `src/cpu/o3/BaseO3CPU.py:75-78` — `cacheStorePorts`, `cacheLoadPorts`
- `src/cpu/o3/BaseO3CPU.py:81-118` — all pipeline delays
- `src/cpu/o3/BaseO3CPU.py:86-90` — fetch widths/buffers
- `src/cpu/o3/BaseO3CPU.py:101` — `decodeWidth`
- `src/cpu/o3/BaseO3CPU.py:108` — `renameWidth`
- `src/cpu/o3/BaseO3CPU.py:119-121` — `dispatchWidth`, `issueWidth`,
  `wbWidth`
- `src/cpu/o3/BaseO3CPU.py:123-128` — `commitWidth`, `squashWidth`
- `src/cpu/o3/BaseO3CPU.py:142-159` — LQEntries, SQEntries,
  `LSQDepCheckShift`, `LSQCheckLoads`, `store_set_clear_period`,
  `LFSTSize`, `SSITSize`, `SSITAssoc`
- `src/cpu/o3/BaseO3CPU.py:172-188` — `numRobs`, physical reg counts,
  `numROBEntries`
- `src/cpu/o3/BaseO3CPU.py:190-200` — `smtNumFetchingThreads`,
  `smtFetchPolicy`, `smtLSQPolicy`, `smtROBPolicy`,
  `smtCommitPolicy`, thresholds
- `src/cpu/o3/BaseO3CPU.py:202-207` — `branchPred = BranchPredictor(...)`
- `src/cpu/o3/BaseO3CPU.py:222-241` — decoupled FE params
- `src/cpu/o3/O3CPU.py:26-54` — per-ISA dispatch
- `src/cpu/o3/SMT.py:42-43` — `SMTFetchPolicy` enum
- `src/cpu/o3/SMT.py:46-47` — `SMTQueuePolicy` enum
- `src/cpu/o3/SMT.py:50-51` — `CommitPolicy` enum
- `src/cpu/o3/cpu.hh:96` — `class CPU : public BaseCPU`
- `src/cpu/o3/cpu.hh:415-448` — value members of all 7 stages
- `src/cpu/o3/cpu.hh:442-445` — `renameMap`, `commitRenameMap`
- `src/cpu/o3/cpu.hh:451` — `activeThreads`
- `src/cpu/o3/cpu.hh:470-479` — `StageIdx` enum
- `src/cpu/o3/cpu.hh:482-494` — `TimeBuffer<...>` declarations
- `src/cpu/o3/cpu.hh:561` — `std::vector<ThreadState*> thread`
- `src/cpu/o3/cpu.cc` — 1,507 LoC (driver)
- `src/cpu/o3/iew.hh:69-86` — IEW docstring ("handles both single
  threaded and SMT IEW")
- `src/cpu/o3/iew.hh:87` — `class IEW`
- `src/cpu/o3/iew.hh:115-119` — `dispatchStatus[MaxThreads]`,
  `exeStatus`, `wbStatus`
- `src/cpu/o3/iew.hh:131` — `IEW(CPU*, const BaseO3CPUParams&)`
- `src/cpu/o3/iew.hh:297` — `void tick()`
- `src/cpu/o3/iew.hh:337-340` — per-thread queues and skid buffers
- `src/cpu/o3/iew.hh:359-365` — `instQueue`, `ldstQueue`, `fuPools`
- `src/cpu/o3/iew.hh:394-410` — `dispatchWidth`, `issueWidth`,
  `wbWidth`, `wbNumInst`, `wbCycle`
- `src/cpu/o3/iew.cc:77-127` — IEW constructor with width checks
- `src/cpu/o3/iew.cc:93-104` — `MaxWidth` runtime check
- `src/cpu/o3/iew.cc:119-122` — per-thread status init
- `src/cpu/o3/iew.cc:438-470` — `squash(tid)` per-thread
- `src/cpu/o3/iew.cc:1138-1377` — `executeInsts()` (the IQ + LSQ
  integration)
- `src/cpu/o3/iew.cc:1380-1427` — `writebackInsts()` (per-thread)
- `src/cpu/o3/iew.cc:1429-1550` — `IEW::tick()` (the main loop,
  SMT dispatch loop at line 1448)
- `src/cpu/o3/rob.hh:71` — `class ROB`
- `src/cpu/o3/rob.hh:78-83` — `Status` enum
- `src/cpu/o3/rob.hh:90` — `SMTQueuePolicy robPolicy`
- `src/cpu/o3/rob.hh:97` — `ROB(CPU*, const BaseO3CPUParams&)`
- `src/cpu/o3/rob.hh:129,154,160,163,166,169,172,175,178,182,186,190,194,198`
  — public API (head/tail/retire/squash/etc.)
- `src/cpu/o3/rob.hh:283-289` — `threadEntries[MaxThreads]`,
  `maxEntries[MaxThreads]`, `instList[MaxThreads]`
- `src/cpu/o3/rob.hh:295` — `squashWidth`
- `src/cpu/o3/rob.hh:316` — `InstIt squashIt[MaxThreads]`
- `src/cpu/o3/inst_queue.hh:82-159` — `class IQUnit` (per-op-class SimObject)
- `src/cpu/o3/inst_queue.hh:137` — `SMTQueuePolicy iqPolicy`
- `src/cpu/o3/inst_queue.hh:178` — `class InstructionQueue`
- `src/cpu/o3/inst_queue.hh:185-213` — `FUCompletion` event
- `src/cpu/o3/inst_queue.hh:290,320,332` — getInstToExecute,
  scheduleReadyInsts, wakeDependents
- `src/cpu/o3/inst_queue.hh:399` — `MemDepUnit memDepUnit[MaxThreads]`
- `src/cpu/o3/inst_queue.hh:417-453` — `instList[MaxThreads]`,
  `instsToExecute`, `readyInsts[Num_OpClasses]`
- `src/cpu/o3/inst_queue.hh:480,485,490` — `listOrder`, `queueOnList`,
  `readyIt` (age-ordering)
- `src/cpu/o3/inst_queue.hh:501` — `DependencyGraph<DynInstPtr>`
- `src/cpu/o3/inst_queue.hh:511,514` — `totalWidth`, `numPhysRegs`
- `src/cpu/o3/inst_queue.hh:533` — `std::vector<bool> regScoreboard`
- `src/cpu/o3/inst_queue.cc:67-116` — `IQUnit` constructor with 3
  SMT policies
- `src/cpu/o3/inst_queue.cc:153-168` — `resetEntries()`
- `src/cpu/o3/rename.hh:78` — `class Rename`
- `src/cpu/o3/rename.hh:253-256` — `renameSrcRegs`, `renameDestRegs`
- `src/cpu/o3/rename.hh:262-271` — `calcFreeROBEntries`,
  `calcFreeIQEntries`, `calcFreeLQEntries`, `calcFreeSQEntries`
- `src/cpu/o3/rename.hh:301-320` — `RenameHistory` struct
- `src/cpu/o3/rename.hh:325` — `historyBuffer[MaxThreads]`
- `src/cpu/o3/rename.hh:355-388` — per-thread insts, skid buffer,
  renameMap, freeList, freeingInProgress, instsInProgress,
  loadsInProgress, storesInProgress
- `src/cpu/o3/rename.hh:398-404` — `FreeEntries` struct
- `src/cpu/o3/rename.hh:409` — `FreeEntries freeEntries[MaxThreads]`
- `src/cpu/o3/rename.hh:415` — `bool emptyROB[MaxThreads]`
- `src/cpu/o3/rename.hh:444,445,450` — `renameWidth`, `toIEWIndex`,
  `blockThisCycle`
- `src/cpu/o3/rename_map.hh:71-159` — `SimpleRenameMap`
- `src/cpu/o3/rename_map.hh:105` — `RenameInfo = pair<PhysRegIdPtr,
  PhysRegIdPtr>`
- `src/cpu/o3/rename_map.hh:168-297` — `UnifiedRenameMap`
- `src/cpu/o3/rename_map.hh:171` — `std::array<SimpleRenameMap,
  CCRegClass+1> renameMaps`
- `src/cpu/o3/rename_map.hh:185` — `PerThreadUnifiedRenameMap =
  std::array<UnifiedRenameMap, MaxThreads>`
- `src/cpu/o3/free_list.hh:71-108` — `SimpleFreeList`
- `src/cpu/o3/free_list.hh:124-192` — `UnifiedFreeList`
- `src/cpu/o3/free_list.hh:132` — `std::array<SimpleFreeList,
  CCRegClass+1> freeLists`
- `src/cpu/o3/lsq.hh:76` — `class LSQ`
- `src/cpu/o3/lsq.hh:84-149` — `DcachePort` (the port to L1 cache)
- `src/cpu/o3/lsq.hh:219-597` — `LSQRequest` (the per-mem-op state
  machine, with its 13 flags and 5 states)
- `src/cpu/o3/lsq.hh:599-621` — `SingleDataRequest`
- `src/cpu/o3/lsq.hh:627-642` — `UnsquashableDirectRequest`
- `src/cpu/o3/lsq.hh:644-691` — `SplitDataRequest`
- `src/cpu/o3/lsq.hh:951-967` — `maxLSQAllocation(SMTQueuePolicy,
  entries, numThreads, threshold)` — the SMT policy
- `src/cpu/o3/lsq.hh:987` — `std::vector<LSQUnit> thread`
- `src/cpu/o3/lsq_unit.hh:88` — `class LSQUnit`
- `src/cpu/o3/lsq_unit.hh:91` — `MaxDataBytes = MaxVecRegLenInBytes`
- `src/cpu/o3/lsq_unit.hh:95-147` — `LSQEntry`
- `src/cpu/o3/lsq_unit.hh:149-196` — `SQEntry` (extends LSQEntry with
  store data)
- `src/cpu/o3/lsq_unit.hh:200-205` — `AddrRangeCoverage` enum
- `src/cpu/o3/lsq_unit.hh:208-209` — `LoadQueue = CircularQueue<...>`,
  `StoreQueue = CircularQueue<...>`
- `src/cpu/o3/lsq_unit.hh:425-447` — `WritebackEvent` (used for
  forwarding)
- `src/cpu/o3/lsq_unit.cc:1408-1591` — `read()`'s store-forwarding
  logic
- `src/cpu/o3/lsq_unit.cc:1412,1434-1437,1445,1453,1474,1492,1552,1567,1574,1581`
  — the forwarding path
- `src/cpu/o3/mem_dep_unit.hh:90` — `class MemDepUnit`
- `src/cpu/o3/commit.hh:91` — `class Commit`
- `src/cpu/o3/commit.hh:91-89` — comprehensive docstring
- `src/cpu/o3/commit.hh:121,123` — `commitStatus[MaxThreads]`,
  `commitPolicy`
- `src/cpu/o3/commit.hh:286-306` — `commitInsts`, `commitHead`,
  `getInsts`, `markCompletedInsts`, `getCommittingThread`,
  `roundRobin`, `oldestReady`
- `src/cpu/o3/commit.hh:378` — `priority_list`
- `src/cpu/o3/commit.hh:394,397,400` — `renameWidth`, `commitWidth`,
  `numThreads`
- `src/cpu/o3/fu_pool.hh:75-212` — `class FUPool : public SimObject`
- `src/cpu/o3/fu_pool.hh:79-87` — capability / busy / pending-free data
- `src/cpu/o3/fu_pool.hh:126` — `FUIdxQueue fuPerCapList[Num_OpClasses]`
- `src/cpu/o3/fu_pool.hh:150-163` — sentinel returns
  (`NoNeedFU = -3`, `NoCapableFU = -2`, `NoFreeFU = -1`)
- `src/cpu/o3/fu_pool.hh:178-200` — `getUnit`, `freeUnitNextCycle`,
  `getOpLatency`, `isPipelined`

### ChipForge source

- `ip/cpu/docs/dse_architecture.md:1-1028` — existing Phase 1 DSE doc
- `ip/cpu/docs/dse_architecture.md:166-167` — DecodePayload fields
- `ip/cpu/docs/dse_architecture.md:194-220` — switching ISA cost
- `ip/cpu/docs/dse_architecture.md:249-288` — current `CPUConfig` extension
- `ip/cpu/docs/dse_architecture.md:312-323` — `DseSpace` (the sweep input)
- `ip/cpu/docs/dse_architecture.md:353-394` — `declare_substage` problem
  and `merge_stage` proposal
- `ip/cpu/docs/dse_architecture.md:472-547` — BranchPredictorPlugin
  template parameterization
- `ip/cpu/docs/dse_architecture.md:550-562` — per-plugin改造 strategy
- `ip/cpu/docs/dse_architecture.md:566-583` — `setup_with_config`
- `ip/cpu/docs/dse_architecture.md:601-689` — proposed
  `CpuFactory::build_cpu` real implementation
- `ip/cpu/docs/dse_architecture.md:823-882` — Phase A-F roadmap
- `ip/cpu/docs/dse_architecture.md:886-898` — what's deferred to Phase 5+
- `include/cf/plugin/pipe_builder.h:54-172` — current `PipeBuilder`
  API
- `include/cf/plugin/pipe_builder.h:64-67` — `register_plugin`
- `include/cf/plugin/pipe_builder.h:69-76` — `at_stage`
- `include/cf/plugin/pipe_builder.h:78-86` — `declare_substage` (with
  the depth param noted as broken)
- `include/cf/plugin/pipe_builder.h:88-92` — `node_of_logic_stage`
- `include/cf/plugin/pipe_builder.h:94-97` — `build()`
- `include/cf/plugin/pipe_builder.h:99-102` — `run()`
- `include/cf/plugin/pipe_builder.h:128-138` — `register_commit_hook`
  (existing! storage-related)
- `include/cf/plugin/plugin_base.h:48-72` — current `PluginBase`
- `include/cf/plugin/plugin_base.h:60,65,71` — `setup`, `build`, `tick=delete`

---

*Report end. Use this to inform `dse_architecture_v2_design_research.md`.*

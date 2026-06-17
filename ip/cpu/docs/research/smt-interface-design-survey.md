# SMT (Simultaneous Multithreading) Interface Design Survey

| Field | Value |
|-------|-------|
| Task ID | bg_41470f52 |
| Agent | librarian |
| Date | 2026-06-17 |
| Scope | Production CPU SMT implementations, RISC-V status, interface patterns for plugin-based extensibility |
| Word Count | ~2,550 words |
| Status | 🔵 Reference (informs `dse_architecture_v2_design_research.md` §7) |

---

## 1. Architectural Overview

### 1.1 What is SMT?

SMT is the simultaneous execution of instructions from **multiple software threads** on a single physical core, sharing all execution resources dynamically on a per-cycle basis. It is distinct from prior forms of hardware multithreading.

> "Simultaneous multithreading ... is a technique permitting several independent threads to issue instructions to a superscalar's multiple functional units in a single cycle. We present several models of simultaneous multithreading and compare them with alternative organizations: a wide superscalar, a fine-grain multithreaded processor, and single-chip, multiple-issue multiprocessing architectures."
> — Tullsen, Eggers, Levy, *Simultaneous Multithreading: Maximizing On-Chip Parallelism*, ISCA 1995.

The original Tullsen paper showed SMT can deliver "4 times the throughput of a superscalar, and double that of fine-grain multithreading" and that "a single simultaneous multithreaded processor with 10 functional units outperforms by 24% a conventional 8-processor multiprocessor with a total of 32 functional units."

### 1.2 SMT vs. CMP vs. Fine-Grained Multithreading

| Scheme | Stage occupancy | Resource sharing | When context switches |
|--------|----------------|------------------|----------------------|
| **Fine-grained MT** (e.g., Tera MTA, Sun MAJC) | One thread per pipeline stage | Statically time-sliced; only one thread in the pipeline at a time | Every cycle, round-robin |
| **Coarse-grained MT** | One thread until long-latency event (L2 miss) | Statically, but switch hides long latency | After cache miss |
| **CMP** (Chip Multiprocessor) | Each core is its own pipeline | No sharing of execution units | Never (software schedules across cores) |
| **SMT** | All stages can have μops from multiple threads simultaneously | All resources competitively shared or partitioned; thread ID tags on shared state | Never (continuous issuance) |

Fine-grained MT eliminates *vertical waste* (pipeline bubbles from stalls). SMT eliminates *both vertical and horizontal waste* (idle functional units when a single thread lacks ILP).

### 1.3 Resource-Sharing Decisions (the four classic options)

Once the design has decided "we will have N threads", every shared resource must be classified as one of:

1. **Static partitioning** — Each thread owns a fixed slice. E.g., `LSQEntries / numThreads` per thread. (gem5 implements this for LSQ.)
2. **Dynamic partitioning** — Hardware re-allocates ways/entries on demand based on observed behavior. (Vantage, Stanford ISCA'11.)
3. **Competitive sharing (full sharing)** — Threads compete freely; one thread may consume all entries.
4. **Watermarked sharing** — Hybrid: each thread gets a guaranteed minimum (`watermark`); above that, entries are competitively shared. AMD introduced watermarking for scheduler queues in Zen 3.

### 1.4 ICache Partitioning (or not)

- **Fully shared, no thread tag:** A single-line ICache serving both threads; thrashing is possible.
- **Thread-tagged lines:** Each line carries a TID; OS/hardware can guarantee replacement fairness.
- **Static way partition:** Each thread owns N/2 ways. Cheap, but inflexible.
- **Banking / port-duplication:** Doubles fetch bandwidth at the cost of energy.

Intel's first HT implementation in Pentium 4 / Xeon (Netburst) shared the ICache and tracked thread in the BTB.

---

## 2. SMT in Commercial CPUs

| Vendor / CPU | Threads/core | Issue width (peak) | Total order code | Key SMT trait |
|--------------|--------------|-------------------|------------------|---------------|
| Intel Pentium 4 (Netburst, 2002) | 2 | 4 (3 ALU + 1 FP) | HT | Shared ICache, duplicated arch state, ICount-style fetch |
| Intel Nehalem → Skylake → Alder Lake | 2 | 4–6 | HT | Same model; P-cores only on hybrid parts |
| Intel Lunar Lake (mobile) | 1 (SMT **disabled**) | 4 | — | AMD cites this as a competitive advantage for Zen 5 mobile |
| AMD Bulldozer (2011) | 2 (FPU only; INT was CMP-style) | 2 INT + 2 FP per "module" | CMT | "Clustered multithreading" — INT resources partitioned, FP shared |
| AMD Zen 1–5 (2017–2024) | 2 | 4–6 (Zen 4: 6-wide) | SMT | 3-way sharing policy: competitive / watermarked / partitioned |
| IBM POWER5 (2004) | 2 | 5+ | SMT2 | First commercial CMP+SMT |
| IBM POWER6 (2007) | 2 | 7 | SMT2 | Per-thread ICache priority |
| IBM POWER7 (2010) | 4 | 6 issue / 12 exec | SMT4 | 8 cores, eDRAM L3, "fused" core pairs |
| IBM POWER8 (2014) | 8 | 8 dispatch / 10 issue / 16 exec pipes | SMT8 | "Fused core", mode-switchable ST/SMT2/SMT4/SMT8 |
| IBM POWER9 (2017) | 4 (SMT4) or 8 (SMT8) | 8 dispatch / 16 exec | SMT4/SMT8 | Sliced micro-arch; 2x GPR rename files (124 entries each) |
| IBM POWER10 (2020) | 8 | 10 issue | SMT8 | Matrix-math accelerator (MMA) |
| Oracle SPARC T4 (2011) | 8 | 2-issue OoO | CMT (Oracle's term) | 8 strands/core, dynamically threaded |
| Oracle SPARC T5/M5/M6/S7/M7 (2012–2017) | 8 | 2-issue OoO | CMT | "Dynamic Threading" — 1–8 strands, Solaris "Critical Thread" |
| Fujitsu SPARC64 XII (2017) | 2 | 4 issue | SMT | Per-thread ROB head |
| Apple M1 / M2 / M3 (2020–2024) | **1 (no SMT)** | 8 (Firestorm) | — | Apple confirmed: no HyperThreading |
| SiFive U74 / U84 (2021) | 1 | 2 (in-order) | none | In-order, single-thread |
| SiFive P870 (2023) | 1 (announced) | 6 OoO | none | "Highest single-threaded performance in portfolio" — explicit choice |
| Andes NX45 / AX45 | 1 (or 2 with custom) | 2 (in-order) | vendor ACE | Configurable multi-core, not SMT |

### Key citations

- **Intel HT (Netburst):** Marr et al. *Hyper-Threading Technology Architecture and Microarchitecture.* ITJ Q1 2002.
- **AMD Zen 1 SMT overview:** L. Cutress, "AMD Zen Architecture" coverage; AMD *Ryzen Software Optimization*, GDC 2019.
- **AMD Zen 4 SMT:** "SMT uplift for Zen 4 increased to 34% compared to 25% for Zen 3." *Zen 4 Hot Chips 2023 paper.*
- **POWER7/8/9:** Sinharoy et al. *POWER7: IBM's Next-Generation Server Processor.* IEEE Micro 2010. Starke et al. *IBM POWER8 processor core microarchitecture.* IBM J. Res. Dev 2015. Thompto, *IBM POWER9 SMT Deep Dive*, ORNL Summit Workshop 2018.
- **Oracle SPARC:** Turullols & Sivaramakrishnan. *SPARC T5: 16-core CMT Processor.* Hot Chips 2012.

---

## 3. RISC-V H-Extension vs. SMT — Critical Distinction

**There is no standard RISC-V SMT extension.** What exists is the **`H` (Hypervisor) extension** — and it has nothing to do with hardware multithreading.

> "This chapter describes the RISC-V hypervisor extension, which virtualizes the supervisor-level architecture to support the efficient hosting of guest operating systems atop a type-1 or type-2 hypervisor. The hypervisor extension changes supervisor mode into hypervisor-extended supervisor mode (HS-mode) ... The hypervisor extension is enabled by setting bit 7 in the `misa` CSR, which corresponds to the letter H."
> — *RISC-V "H" Extension for Hypervisor Support, Version 1.0* (ratified).

The H-extension adds VS-mode, a second stage of address translation, and CSR mirror sets (`vsstatus`, `vsepc`, etc.). It lets a hypervisor present multiple *virtual* vCPUs to a guest OS but is **not** hardware multithreading — it is **time-multiplexed** single-threaded execution (or scheduled across multiple physical cores).

### What RISC-V vendors do for SMT today

| Vendor | Product | SMT support |
|--------|---------|-------------|
| SiFive | U54/U74/U84 (S-series) | None — single-thread in-order |
| SiFive | P870 (2023) | None — single-thread; "highest single-threaded performance" |
| Ventana | Veyron V1 | None (RV64GCB, OoO) |
| Andes | NX45 / AX45 / A45 | None (multi-core only) |
| Tenstorrent | Ascalon (planned) | None publicly disclosed |
| Rivos | Internal RISC-V server core (2024+) | Not disclosed; Rivos licenses **Andes NX45** only for *control* functions, not the main pipeline |
| **ArchiTek** | CRVS core (research) | **Yes — explicitly SMT** (Chichibu RISC-V SMT core) |

> "The CRVS core uses the SMT to simultaneously process multiple requests. The data flow in the core is as follows. First, the fetch selector selects the PC of one of the threads for instruction fetching. ... The thread control unit (TCU) is an ... thread control in the CRVS core, and it stores the PCs ... and the LTN, which is the logical thread number."
> — Watanabe et al., *Implementation of a RISC-V SMT Core in an AI processor*, CARRV/ICECS 2022.

**Bottom line for ChipForge:** There is no RISC-V ISA barrier to SMT, but there is no standard naming convention or CSR layout for the thread-context (TID) field. The hypervisor spec's `hgatp` / `vsstatus` are irrelevant here. TID widths in current vendor practice: 1 bit for SMT2 (AMD/Intel/POWER SMT2), 2 bits for SMT4 (POWER9 in SMT4), 3 bits for SMT8 (POWER8/9/10). Pick a TID width at design time and let the SDK toolchain know.

---

## 4. Key SMT Implementation Components

### 4.1 Thread State / Hardware Context

**Always duplicated per thread** (the minimum required for two `mret` targets to differ):

- Program counter (PC), next PC, exception PC
- Architectural register file (GPR / FPR / Vector)
- Rename map table (alias table / RAT)
- Privilege mode, ASID/VMID
- Interrupt state (mip, mie, mstatus.MPIE)
- ROB head/tail pointers (per-thread retirement)
- Trap state (mcause, mepc, mtval, mtinst)
- Performance counters (some)
- Branch predictor thread history (PHT local histories, RAS pointers)

**Never duplicated** (thread-tagged in shared state):

- L1 ICache, L1 DCache
- Issue queue entries
- Load/Store queue entries (may be partitioned)
- Functional units, register file physical entries
- Branch predictor global history

### 4.2 Issue Queue (IQ) Partitioning

| Style | Description | Used by |
|-------|-------------|---------|
| Per-thread IQ | Physical IQ split in N pieces, each thread owns N/2 | IBM POWER5 (in some modes) |
| Shared with priority | Single shared IQ; fetch policy (ICOUNT) decides which thread's instructions go in | AMD Zen 1/2 (competitive), Intel HT (per Marr 2002) |
| Shared with watermark | Each thread gets a guaranteed minimum; rest is shared | AMD Zen 3/4/5 (the only way to defend against SQUIP attacks) |
| Fully shared | No accounting; first-come, first-served | Sun MAJC for low thread counts |

### 4.3 Load/Store Queue (LSQ)

- **Static partition (gem5 default `Partitioned`):** each thread gets `LQEntries / numThreads`. Predictable, but reduces 1T performance.
- **Dynamic:** allocate on demand to whoever needs it; OS can be blind to allocation.
- **Threshold (gem5):** each thread is guaranteed at most `smtLSQThreshold` entries, then it's competitive.
- **Hybrid per-thread + shared (SAMIE-LSQ, HPCA 2018):** banked per-thread LQ with a small shared overflow. Reduces "LSQ is full" stalls.

Zen 3+ watermarks the SQ; Zen 2 competitively shares it.

### 4.4 Branch Predictor

Two choices:

- **Shared predictor, TID-tagged history entries.** This is what Intel Netburst / P4 did — the BTB has a `tid` field per entry.
- **Per-thread predictor state.** Each thread has its own PHT, RAS, and possibly its own BTB way partition. Most OoO SMT designs (POWER, AMD Zen) take this approach because thread interference in the predictor destroys accuracy.

POWER9: "Branch prediction: support for up to 40 predicted taken branches in-flight, ST mode. Twenty predicted taken branches per thread in SMT2 mode and ten predicted taken branches per thread in SMT4 mode."

### 4.5 Register Rename

- **Per-thread rename table** with thread-private mapping → simplest, but requires N×map-table storage.
- **Unified rename with TID tag** → register file physical entries are shared; the map table has a TID prefix. Used in Intel HT (Marr 2002) and POWER.
- **Register file split by mode** (POWER8): "GPR registers are mapped onto a pair of 124-entry GPR register files. In ST mode, the two register files have the same contents. In SMT2 mode, each thread uses one GPR register file. In SMT4 mode, each GPR register file supports two threads. In SMT8 mode, each GPR register file supports four threads."

This is the cleanest "partition-on-demand" pattern: the *allocator* decides at hardware-init time which physical PRF entries are visible to which thread, but the *execution units* remain fully shared.

### 4.6 ROB

Three known patterns:

- **Per-thread ROB** (POWER4, PowerPC970). N independent small ROBs.
- **Unified ROB with TID-tagged entries** (Intel, AMD Zen). Single big ROB, each entry has a `tid` field; the retirement logic walks the global head pointer but only retires entries belonging to the current oldest thread.
- **Tournament-style retirement** (Tullsen 1995 original): oldest-first across threads.

The Zen 1/2/3 retire queue is "competitively shared" (AMD GDC 2019 deck). Zen 4 added watermark.

### 4.7 Fetch Policy

The single most important SMT microarchitectural knob. The original Tullsen 1995 paper introduced five:

| Policy | Selection rule | When good |
|--------|---------------|-----------|
| **RR.1.N** (Round-Robin) | Take thread i, then (i+1) mod N | Baseline |
| **BRCOUNT** | Fewest unresolved branches in decode/rename/queue | Wrong-path heavy code |
| **MISSCOUNT** | Fewest outstanding L1 misses | Memory-bound workloads |
| **ICOUNT.2.N** | Fewest instructions in pre-execute stages | General purpose — **industry default** |
| **IQPOSN** | Fewest instructions at queue head (oldest) | Approximation of ICOUNT, cheaper |

Notation: `ICOUNT.M.N` = "fetch from up to M threads, up to N instructions per thread per cycle". `ICOUNT.1.8` is single-thread fetch (P4 default). `ICOUNT.2.4` = 2 threads, 4 μops each. `ICOUNT.2.8` = 2 threads, 8 μops total (Intel Core i7 default).

The ICOUNT philosophy: a thread with its hands in the IQ is not the thread that should be allowed to fill it more. Tullsen (ISCA'96): "ICOUNT achieves three purposes: (1) it prevents any one thread from filling the IQ, (2) it gives highest priority to threads that are moving instructions through the IQ most efficiently, (3) it provides a more even mix of instructions from the available threads, maximizing the parallelism in the queues."

**D-Join / F-Join** are "Data-ready Join" and "Fetch Join" policies from El-Moursy & Albonesi 2003 — they augment ICOUNT with a "unready count" gate (UCG) to throttle threads whose consumers are still waiting for long-latency loads. POWER9 implements similar "Thread Switch Fetch Priority" / "Decode Hold" mechanisms.

### 4.8 ICache / ITLB

The ICache is the *one* resource that is most often **fully shared** (Intel P4, AMD Zen, POWER, SPARC T5). Reasons:
1. Both threads execute the same ISA, so code layout is similar.
2. ICache is read-mostly (no write-back thrash).
3. Splitting the ICache costs area and energy.

The ITLB *is* sometimes partitioned, especially for virtualization (separate ASID spaces). AMD Zen ICache + ITLB are shared across threads but TLBs tag entries with TID.

---

## 5. Interface Design for Plugin-Based SMT

The pattern that best fits ChipForge's `PipeBuilder + PluginBase` model is the **MLIR `MLIRContext`** pattern, where a single host object carries an explicit `Threading` enum (`ENABLED | DISABLED`) and a pluggable `setThreadPool(...)` interface. Each `Op`/`Pass` can query `isMultithreadingEnabled()` at construction.

The cleanest abstraction for "2 threads in flight" is **thread context** as a first-class object, distinct from **stage** (which is a function unit):

### 5.1 Layered Model

```
┌─────────────────────────────────────────────────────────────┐
│ Core: ThreadContextPool (the "2 threads" knob)             │
│   - vector<unique_ptr<ThreadContext>> contexts[N]          │
│   - getTidBits() → 0 (1T), 1 (SMT2), 2 (SMT4)              │
└────────────────────────┬────────────────────────────────────┘
                         │ N feeds into...
┌────────────────────────▼────────────────────────────────────┐
│ Stage: Fetch (SHARED)  →  outputs tagged Payload.tid         │
│   - policy: ICOUNT(D) | RR | MLP-aware                      │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│ Stage: Decode (SHARED, time-multiplexed)                    │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│ Stage: Rename (PRIVATE per-thread rename tables)            │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│ Stage: IQ (SHARED, with ICount gating)                      │
│   - each Payload has tid; pick logic filters by tid         │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│ Stage: ROB (SHARED, single circular buffer, tid per entry)  │
│   - retire-walk must respect per-thread head pointer        │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│ Stage: LSQ (DUAL — one LQ/SQ per thread, statically split)  │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│ Stage: EX units (SHARED),  PRF (PARTITIONED on SMT mode)    │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 Required Abstractions in Phase 1 (forward-looking)

To avoid painting yourself into a corner, the following should be **payload-level concepts from day 1**, even if `N=1` today:

1. **`Payload` carries a `tid : uint8_t` field** (1-2 bits sufficient). Most stages ignore it; the rename and ROB stages key off it. A `1T` mode is just the special case where `tid == 0` always.

2. **`ThreadContext` struct** owned by the core, containing: PC, next-PC, rename map table, ROB head, trap CSRs (`mcause`/`mepc`), per-thread counters. Stage plugins do *not* own these; they read/write through a `core->ctx(tid)` accessor.

3. **Stages are classified as `Shared | PerThread | Hybrid`** at registration time. This is a build-time enum, not a runtime decision:
   - `Shared` (e.g., Fetch, IQ, ICache, EX) — receive payloads from multiple tids.
   - `PerThread` (e.g., Rename, ROB head, ICount counter) — `PerThread<Plugin>` instantiates N copies. This is the analog of POWER8's "GPR register file" partition-on-demand.
   - `Hybrid` (e.g., LSQ, PRF) — single Plugin with N internal partitions; exposes `partition_for(tid)`.

4. **Fetch policy is a *separate* plugin** that sits *above* the Fetch stage. It examines `ThreadContext::icount[tid]` and returns the next `tid` to feed. This is exactly the POWER9 "Thread Switch Fetch Priority" interface.

5. **ICount counters live on the `ThreadContext`, not in the IQ plugin.** The fetch policy is then testable in isolation: it can be swapped (RR → ICOUNT → MLP-aware) without touching the IQ.

### 5.3 Adding a New Thread Context at Runtime

The cleanest pattern is **explicit `add_context()` on the core**, not implicit "spawn a stage". For ChipForge:

```cpp
class Cpu {
public:
  ThreadContext& addContext(InitialState);  // returns a ref, increments TID_W
  void removeContext(Tid);                  // drains ROB/LSQ for that TID
private:
  std::array<ThreadContext, MAX_THREADS> ctxs_;  // compile-time bound
  uint8_t active_tids_ = 1;
};
```

The `MAX_THREADS` constant is what locks in TID width in the Payload. If you want SMT2 today but SMT4 later, **do not** parameterize TID width on a runtime config; parameterize the *number of active contexts* and accept that some bits are unused. The hardware cost is a few bits in Payload; the design cost of re-encoding the wire format later is much higher.

### 5.4 Sharing a Stage (e.g., Shared IQ)

Stages don't need to know about thread counts. They just see `Payload` with `tid` already stamped. The shared IQ plugin then becomes a free-pick scheduler with one extra constraint: a payload's `tid` may be filtered by the ICount policy *at the IQ arbiter* (for back-pressure). This is the Zen 1 IQ design.

### 5.5 Duplicating a Stage (e.g., Per-Thread ROB)

Two options:

1. **Macro: `register_per_thread<RobStage>(N)`** — instantiates N copies of the plugin; each gets a distinct `tid` filter on input. Clean, but `N` is compile-time.

2. **One plugin, N partitions inside.** `RobStage` constructor takes `numContexts`; allocates `N` sub-buffers indexed by tid. Use this for the LSQ and PRF where the "stage" is logically one thing but is physically split.

For Phase 1, prefer option (1) — it lets you reason about resource costs per thread, and `register_per_thread<RobStage>(1)` is the same code path as `register_per_thread<RobStage>(4)` in 1T mode.

### 5.6 The Single-Payload → Multi-Context Pivot

The current ChipForge model with one `Payload` per in-flight instruction is **already SMT-ready**, *as long as* the Payload has a `tid` field. The work to support SMT is not in the *Payload*; it is in:

1. The **ThreadContextPool** (currently a singleton → becomes N-fold).
2. The **per-thread rename tables** (currently a single RAT → an array of RATs).
3. The **fetch policy** (currently implicit → first-class plugin).
4. The **LSQ/PRF partitioning** (currently implicit shared → first-class `partition_for(tid)`).

The dse_architecture_v2 proposal should:

- **Lock in `Payload.tid` at 1-2 bits from the start** (cheaper than retrofitting).
- **Define `ThreadContext` as a separate object** from any Stage plugin. A stage should be able to *get* a context but should not *own* one.
- **Make the fetch policy a plugin**, even if it's RR-1T for Phase 1. (Avoids the "no place to put the ICount counter" problem.)
- **Treat the LSQ and the PRF as `Hybrid` (per-thread-partitioned) from the start**, even if the partition size = total/1 in 1T mode. The microarchitecture literature (AMD Zen, POWER8) shows that fully dynamic re-partitioning at mode-switch time is straightforward; the hard problem is *not* in the partition logic but in the *Payload plumbing* for the `tid` field.

---

## 6. References (canonical)

### Foundational papers
- **Tullsen, Eggers, Levy.** *Simultaneous Multithreading: Maximizing On-Chip Parallelism.* ISCA 1995.
- **Tullsen et al.** *Exploiting Choice: Instruction Fetch and Issue on an Implementable Simultaneous Multithreading Processor.* ISCA 1996.
- **Lo, Eggers, Emer, Levy, Stamm, Tullsen.** *Converting Thread-Level Parallelism to Instruction-Level Parallelism via Simultaneous Multithreading.* ACM TOCS 1997.
- **El-Moursy, Albonesi.** *Front-End Policies for Improved Issue Efficiency in SMT Processors.* HPCA 2003.
- **Eyerman, Eeckhout.** *A Memory-Level Parallelism Aware Fetch Policy for SMT Processors.* HPCA 2007.
- **Ungerer, Robič, Šilc.** *A Survey of Processors with Explicit Multithreading.* ACM CSUR 2003.

### Commercial CPU documents
- **Intel** Marr et al. *Hyper-Threading Technology Architecture and Microarchitecture.* ITJ Q1 2002.
- **AMD** *Ryzen Processor Software Optimization.* GDC 2019.
- **AMD** *Zen 4 / EPYC 9004 architecture white paper.* 2023.
- **AMD** *Zen 4 Hot Chips 35 paper.* 2023.
- **IBM** Sinharoy et al. *POWER7: IBM's Next-Generation Server Processor.* IEEE Micro 2010.
- **IBM** *POWER8 Processor Core Microarchitecture.* IBM J. Res. Dev 2015.
- **IBM** *POWER9 Processor User's Manual.* OpenPOWER v2.0/v2.1.
- **IBM** *IBM POWER9 processor and system features for computing in the cognitive era.* IBM J. Res. Dev 2018.
- **Oracle** *SPARC S7 Processor-Based Server Architecture.*
- **Oracle** Turullols, Sivaramakrishnan, Parrish. *SPARC T5: 16-core CMT Processor.* Hot Chips 2012.
- **SiFive** *U74 Core Complex Manual.*
- **SiFive** *P870 Hot Chips 2023 paper.*

### RISC-V SMT
- **RISC-V International** *H Extension for Hypervisor Support v1.0* (ratified).
- **Watanabe et al.** *Implementation of a RISC-V SMT Core in an AI processor.* CARRV 2022.
- **UPC HLIB framework thesis** *Design and Implementation of a RISC-V Superscalar Out-of-Order SMT Core Front-End Engine.* 2018.
- **Rivos + Andes partnership announcement.** Sep 2024.
- **SQUIP attack** (reveals AMD Zen 2 vs Zen 3 sharing policies). USENIX Security 2022.

### Architecture & analysis
- **Chips and Cheese** *Disabling Zen 5's Op Cache and Exploring its Clustered Decoder.* Jan 2025.
- **Chips and Cheese** *Discussing AMD's Zen 5 at Hot Chips 2024.* Sep 2024.
- **Abhinav Upadhyay** *How Simultaneous Multithreading Works Under the Hood.* Jul 2024.
- **MLIR** *MLIRContext multi-threading model.* LLVM.

---

**Confidence notes / uncertainties:**
- The Rivos-internal server core's SMT specifics are not public; the "Rivos uses Andes NX45" detail comes from a press release and is for *control* functions, not the main pipeline.
- The "D-Join / F-Join" terminology appears mostly in El-Moursy/Albonesi derivative work; POWER9's actual implementation uses "Thread Switch Fetch Priority" (a superset concept).
- Intel's Lunar Lake "no SMT" decision is a rumor repeated in the Zen 5 Hot Chips 2024 deck; Intel has not confirmed the architecture name officially.

---

*Document extracted from background task bg_41470f52 output. Original agent: librarian.*

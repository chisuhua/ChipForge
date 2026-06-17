# CPU Design Space Exploration (DSE) Frameworks — Tools and Methodology Survey

| Field | Value |
|-------|-------|
| Task ID | bg_a592c1cb |
| Agent | librarian |
| Date | 2026-06-17 |
| Scope | Academic, industrial, and Chisel-based DSE frameworks for CPU architecture exploration |
| Word Count | ~3,000 words |
| Status | 🔵 Reference (informs `dse_architecture_v2_design_research.md` §8) |

---

## 0. Scope and provenance note

The brief referenced a UCLA "Horizon / Marvel" DSE toolchain. Based on the published record, the relevant body of work is **Prabhat Mishra's line of ADL-driven ASIP/DSE research** (originally UCI, now U. Florida). The Mishra group developed the **EXPRESSION ADL**, **EXPRESS** retargetable compiler, **SIMPRESS** retargetable simulator, the **V-SAT** visual exploration front-end, and a synthesizable-RTL generator built on "functional abstraction" primitives — together forming an integrated DSE flow for pipelined embedded processors and ASIPs. The "Horizon" name is also the name of a 1988 SRC/Tera shared-memory MIMD supercomputer paper that is unrelated to architectural DSE. To avoid confusion, this report treats the Mishra/EXPRESSION toolchain as the relevant UCLA/UC-Irvine entry and flags Horizon-1988 only to disambiguate.

Similarly, the "Open-PEPP" entry resolves to **OpenDSE** (SDARG/opendse on GitHub), an open-source Java framework from the TU Dresden System Design / Falko Hoftberger / Martin Lukasiewycz group that follows the Y-chart model. Related follow-on work from TU Dresden's compiler-construction chair includes **Mocasin** (dataflow DSE for heterogeneous platforms) and **mpsym** (group-theoretic symmetry reduction for hierarchical DSE).

---

## 1. Academic DSE tools

### 1.1 Mishra/EXPRESSION (UC-Irvine → U. Florida)

EXPRESSION is an Architecture Description Language (ADL) plus a retargetable compiler/simulator toolkit for ASIP design space exploration. Architectures are captured through a mixed behavioral/structural representation; the ADL drives automatic generation of an ILP-optimizing compiler (EXPRESS), a cycle-accurate structural simulator (SIMPRESS), and a VHDL/RTL model via "functional abstraction" primitives (Halambi et al., DATE 1999; Mishra & Dutt, *Processor Description Languages*, Morgan Kaufmann 2008).

**What it parameterizes**: pipeline depth, issue width, functional-unit mix, in-order vs. out-of-order, register file size, memory hierarchy, custom instructions (the "P+I space" of pipeline structure + instruction set). The synthesis-driven variant (Mishra, Kejariwal & Dutt, RSP 2003; Mishra et al., VLSI Design 2004) generates **synthesizable Verilog/VHDL** from EXPRESSION and feeds it to Synopsys Design Compiler to get real area, clock frequency and power — the full PPA trifecta — within hours instead of weeks.

**Reported DSE result**: orders-of-magnitude reduction in architecture-to-RTL time; trade-off curves across area, frequency and power for a set of pipelined processor families. The V-SAT GUI and the retargetable toolkit are the key automation enablers: the *same* ADL specification drives compiler, simulator, and synthesizable RTL.

**Relevance to ChipForge**: the closest published analogue of "ADL-driven CPU DSE with real PPA" before the Chisel era. Authors' claim ("reduction of specification and exploration time by at least an order of magnitude") is a useful benchmark for any C++-based DSE wrapper to beat.

### 1.2 OpenDSE (TU Dresden; formerly at sourceforge)

OpenDSE is an open-source Y-chart DSE framework for embedded systems (`https://github.com/felixreimann/opendse`, `https://sdarg.github.io/opendse/`). Architecture is described as a graph of resources; applications are data-dependent task graphs. It uses `opt4j` for metaheuristic optimization (NSGA-II etc.) and `jmpi` for real-time analysis, and is released under MIT.

**What it parameterizes**: mapping of tasks to resources, scheduling, and the architecture graph itself (resource types, instance counts, connectivity). It targets system-level DSE, not RTL microarchitectural parameters.

**Adjacent Dresden work**:
- **Mocasin** (Chair for Compiler Construction, Jeronimo Castrillon) — open-source rapid prototyping tool for mapping software to heterogeneous CPU/FPGA platforms, supports KPN/SDF/task-graph MoCs, and applies symmetry reduction.
- **mpsym** — C++ library using computational group theory to prune and accelerate DSE for hierarchical clustered many-cores (e.g., Kalray MPPA3 Coolidge). Reports up to 8.6× speed-up and 30× better exploration result for simulated annealing.
- **PANACA-based NoC DSE** with MOEA and DPR-aware mapping.
- **Generative architecture platform DSE** (Müller et al.) — uses Answer Set Programming (ASPmT) to *generate* the architecture template itself, not just choose between templates.

**Relevance to ChipForge**: OpenDSE is the cleanest open-source reference for a *Java* DSE core; Mocasin/mpsym show the latest in symmetry- and algebra-based pruning for combinatorially explosive spaces. Any C++ DSE wrapper should at minimum match their pluggable evaluation backend.

### 1.3 ESESC (UC Santa Cruz, Renau group)

ESESC is a fast multicore simulator with detailed performance, power and thermal models for out-of-order, in-order, and (beta) GPU cores (`https://masc.soe.ucsc.edu/esesc/`, `https://github.com/masc-ucsc/esesc`).

**Architecture**: superscalar OoO core model, configurable memory hierarchy, multicore (homogeneous + heterogeneous), MIPS64r6 and RISC-V ISAs via QEMU, ~50 MIPS with sampling. Power is integrated with **McPAT** (on the fly, using ESESC's GStat counters — not the older Wattch). Thermal is from HotSpot. Repo includes a `libpeq` analytical SRAM/cache power model derived from statistical analysis of CACTI sweeps, intended to substitute for the full McPAT at a fraction of the runtime.

**What it parameterizes**: every core, cache, coherence and uncore microarchitectural parameter needed for sweep studies. McPAT 1.0 integration is patch-based; DSE use typically wraps a script that mutates the ESESC config and re-runs.

**Speed/citation**: HPCA 2013 (Ardestani & Renau, "ESESC: A Fast Multicore Simulator Using Time-Based Sampling"). The design target is **statistical sampling** so that 250-K-point CMP design spaces become tractable.

**Relevance to ChipForge**: the most widely cited out-of-order superscalar DSE backend in academic CPU DSE papers. LibPeq's idea — *analytical surrogate derived from a precomputed CACTI sweep* — is essentially the same architecture used in the predictive-modeling literature of §4.

### 1.4 Graphite (MIT Carbon)

Graphite is a parallel, distributed simulator for multicores built on Pin (`https://github.com/mit-carbon/Graphite`, HPCA 2010 — Miller, Kasture, Kurian, Beckmann, Celio, Eastep, Agarwal). Scales by partitioning the target core set across host processes; demonstrates 1024-core simulation.

**What it parameterizes**: tiled multicore architectures with private L1/L2 caches, simulated at the instruction level using Pin. Intentionally limited to a fixed tile template; this is a *parameterizable but constrained* target.

**Relevance to ChipForge**: pure throughput scaling rather than Pareto-frontier search. Useful as a "single design point in N seconds at 1024 cores" backend; not a DSE framework by itself.

### 1.5 Sniper Multi-Core Simulator (Ghent / Intel ExaScienceLab)

Sniper is built on the Graphite infrastructure but uses an **interval core model** — a mechanistic analytical model that "jumps" between miss events, trading cycle-accuracy for ~orders-of-magnitude speed-up (`https://snipersim.org/`, `https://snipersim.org/documents/sniper-manual.pdf`). SC 2011 (Carlson, Heirman, Eeckhout).

**Accuracy/speed**: validated within ~25% of real Intel Core 2 / Nehalem systems; ~2 MIPS for 16 cores on an 8-core host. Power/area via integrated **McPAT** (since v3.2), with custom DRAM power models. Generates **CPI stacks** and **power stacks** for root-cause analysis.

**DSE wrappers that use Sniper**:
- **MultiExplorer** (UFMS-Brazil) — Sniper + McPAT + NSGA-II, dark-silicon-aware DSE.
- **Sniper/McPAT** (Carlson et al., PACT 2012) — 22.1% performance / 8.3% power error vs. real hardware.

**What it parameterizes**: full interval-model core (issue width, ROB, IQ, LSQ, BP), private/shared caches, prefetchers, DVFS, heterogeneous tile mixes. The de-facto x86 DSE backend.

**Relevance to ChipForge**: Sniper + McPAT + NSGA-II is the closest published baseline a ChipForge "DSE methodology v2" should compare against. The interval-simulation trade-off (cycle-accuracy vs. multi-MIPS throughput) is a useful contrast point to Chisel/RTL elaboration cycles.

### 1.6 Other academic DSE efforts worth knowing

- **RapidChiplet** (EPFL/ETH, arXiv:2311.06081) — chiplet interconnect DSE with latency/throughput proxies at 10⁴× speed of BookSim, sweeping topology, chiplet count/size, PHY placement, packaging, routing. Pareto frontiers in latency-throughput-area.
- **CHASE / ChipletSIM / ChipletDSE** (CUHK, ISPD 2026) — multi-fidelity chiplet DSE (Pareto-based, low- → high-fidelity).
- **Design Space Exploration for Chiplet-Assembly-Based Processors** (Illinois TVLSI 2020) — first microarchitectural DSE for chiplets; up to 35% EDP improvement over a single best system, 72% cost improvement over SoC methodology.
- **PriME** (Princeton) — MPI-based parallel manycore simulator positioned as a more flexible alternative to Graphite.
- **GPGPU-Accelerated Parallel Simulation of Thousand-Core Platforms** (Pinto et al., CCGRID 2011) — porting distributed simulation to GPUs; instruction-accurate, not cycle-accurate.
- **CarbonPATH**, **ChipletGym**, **MAHL** — chiplet DSE using simulated annealing, RL and LLM-based coarse-then-fine exploration respectively.
- **OneDSE** (arXiv:2505.03771, 2025) — workload-aware neural surrogate for CPU DSE; explicitly identifies the three failure modes of prior predictors (slow metric prediction, no generalization across unseen workloads, no cross-microarchitecture transfer).
- **Kang & Kumar (2008), Bai et al. (2021), Zheng et al. (2024)** — earlier ML-based surrogate simulators cited in the OneDSE paper.

---

## 2. Industrial EDA tools (publicly documented only)

### 2.1 Synopsys DesignWare ARC

The Synopsys ARC processor family (ARC 600, 700, AS2xx, 770D, 625D) is explicitly marketed as **highly configurable and extensible**. The configuration surface includes:

- Register file: 16/32 entries base, extendible to 60.
- Instruction + data cache: 2 KB to 32 KB.
- Closely-coupled memories: 1 KB to 512 KB (ICCM) and 2 KB to 256 KB (DCCM).
- MUL/MAC widths, optional FPU (single + double), XY memory banks for DSP, MMU/MPU, ECC, watchdog, interrupts (up to 32 two-level).
- **APEX** (ARC Processor EXtension) — user-defined instructions, registers, flags, condition codes.
- Memory ports and peripherals tightly couplable to remove bus infrastructure.

**Configuration tool**: the **ARChitect wizard** is a drag-and-drop configuration UI; the white paper "Rapid Architectural Exploration in Designing Application-Specific Processors" makes the ASIP-DSE framing explicit — performance/area/energy trade-off exploration driven by application C-code.

**What is exposed as a DSE knob**: an enumerated list of architectural *options* (each boolean or small range) plus APEX-extensible datapath elements. The wizard is a single-point configurator; there is no public scripting/CLI DSE loop described in vendor docs, though internal Synopsys tools (e.g., MetaWare-driven profiling + Design Compiler for the area/power numbers) close the loop.

### 2.2 Cadence Tensilica Xtensa

The Xtensa platform is "the first configurable and extensible processor".

**Configurable knobs (Xtensa LX / LX7 / LX8 / NX)**: 5/7-stage pipeline depth, branch predictor (improved BTB in LX8), configurable I-cache and D-cache sizes, local memory sizes, **L2 memory option** (configurable as fixed-address memory, L2 cache, or both, with a reported 50% system-performance improvement on LX8), windowed watchdog, dual-core lockstep, multiple bus interfaces (AXI4, AXI3, ACE-Lite, PIF, AHB-Lite, iDMA), FPU (single + double precision), FLIX (VLIW) width (3-way 64-bit), write buffer depth (1–32), pre-fetch unit, optional second load/store unit with data cache, optional VLIW.

**Extensibility**: Designer-defined Queues, Ports, Lookups, custom instructions/execution units/register files/I/Os. TIE (Tensilica Instruction Extension) is the analog of ARC APEX.

**DSE flow**: Xtensa Processor Generator + **Cadence Stratus HLS** is sold as an integrated HW/SW co-design flow: "performance-based HW/SW partitioning and automated HW accelerator micro-architecture exploration, optimization, and RTL production".

**What is exposed as a DSE knob**: a UI-driven enumerative set of architectural *options* (each with a small range) plus designer-defined TIE extensions. A "sweep" is normally a manual exercise through the Xplorer IDE; no public scripting CLI is documented.

### 2.3 Imperas (now part of Synopsys as ImperasDV)

Imperas ships **riscvOVPsim** (free reference ISS) and the commercial **ImperasDV** verification product. The Imperas reference model (ImperasFPM) "can be configured and extended to match the processor under test, including custom features" and connects to RTL via the **RVVI-TRACE** "step-and-compare" interface in SystemVerilog/UVM, Verilator, Mentor Questa or Synopsys VCS flows.

**What is exposed as a DSE knob**: the *ImperasFPM* parameter set mirrors the RISC-V extensions and privilege levels supported by the golden reference model. Imperas publicly markets this as a *verification* tool ("step-and-compare"), not a DSE backend per se, but because the reference model is fully configurable across the same RISC-V + custom-extension axis that an RTL exploration uses, it is widely used as the golden reference against which swept RTL configs are validated. Imperas mentions "configure/customize these extendable platforms or develop your own platforms" — confirming platform-template DSE on top of RISC-V.

**Relevance to ChipForge**: a natural "golden model" for any ChipForge sweep that includes ISA customisation (e.g., custom RISC-V extensions). The RVVI-TRACE interface is the de-facto RISC-V verification contract.

### 2.4 RISC-V Foundation

The RISC-V Foundation's specifications are explicitly modular: M, A, F, D, V, C, B, H, K, P, J extensions, plus 32/64/128-bit XLEN, plus 32 vs. 16 vs. 64 general-purpose register counts, plus optional compressed instruction. Profiles (RVA23, RVb23) bundle sets for a given application class. The architectural "knobs" are therefore primarily **which extension letters are implemented** and the **core XLEN**.

For implementation-level knobs, the RISC-V ecosystem defers to the silicon vendors — there is no central DSE tool from the Foundation.

### 2.5 SiFive IP Builder / Core Designer

SiFive Core Designer is a web portal that turns a configuration of SiFive's RISC-V cores into release-candidate RTL + testbench + bare-metal BSP + FPGA bitstream + documentation, with "no complex EDA tools or scripting languages to learn".

**What is exposed as a DSE knob**: starting from a pre-configured standard core (E20/E21/E24/E31/E34/E76, S51/S54/S76, U54/U74) the user adjusts ISA (RV32/64, I/M/A/C/F/D extensions, SiFive Custom Instruction Extension), performance levels, memory map, privilege modes (M/U/S), interrupt count, debug options, security options, ports. Each is a finite enumerated choice; the parameter space is a *discrete menu* of pre-verified derivatives rather than a continuous sweep. The 2018 record of booting Linux on a custom SiFive SoC in 6 months from concept is the headline case study.

**Relevance to ChipForge**: SiFive is the "menu-of-derivatives" model of CPU DSE. It explicitly trades depth of exploration for **out-of-the-box verification** — every released derivative is pre-verified RTL. This is the opposite trade-off from Chipyard's "all parameters free" model.

---

## 3. Chisel-based DSE

### 3.1 Chipyard's "Configs" system

Chipyard (`https://chipyard.readthedocs.io/en/main/`, `https://github.com/ucb-bar/chipyard`) is a Berkeley RISC-V SoC research framework that aggregates Rocket, BOOM, Hwacha, Gemmini, NVDLA, and dozens of other generators. Its configuration model is the **Rocket-Chip parameter system**.

**Key concepts** (verbatim from Chipyard docs):

- A **config** is a collection of generator parameters set to specific values.
- **Additive config fragments** (naming convention `With<Name>`, `++`-composed) override or extend each other.
- **Non-additive configs** (naming convention `<Name>`) typically define a new top-level.
- Composition order matters: fragments are applied **right-to-left / bottom-to-top**.
- The `site`/`here`/`up` maps give the value of a key as seen from the root of the config hierarchy (`site`), the current level (`here`), and one level up (`up`).
- Two "mixin" kinds: one for the lazy module (`CanHave<Mixin>`) — defines logical connections and exchanges config info — and one for the lazy module implementation (`CanHave<Mixin>ModuleImp`) — does the actual Chisel RTL elaboration.
- The **TilePluginProvider** extension allows optional per-tile fragments (e.g., `WithTraceIO`, `WithTilePrefetchers`) to discover and configure generators without hard dependencies.

**The DSE-relevant fact** is acknowledged in the docs: "a significant challenge with the Rocket parameter system is being able to identify the correct parameter to use, and the impact that parameter has on the overall system. We are still investigating methods to facilitate parameter exploration and discovery."

**What Chipyard exposes as a DSE knob**: essentially every parameter in every generator it aggregates. Heterogeneous SoCs (e.g., `DualLargeBoomAndSingleRocketConfig`) are assembled by mixing `WithN{Small|Medium|Large|...}BoomCores(n)` and `WithNBigCores(n)` fragments; the order of `++` determines hart IDs.

### 3.2 Rocket Chip knobs (in `BaseSubsystemConfig` and friends)

From Rocket-Chip source:

- **Tile**: `WithNBigCores`/`WithNMedCores`/`WithNSmallCores`/`With1TinyCore`/`WithNHugeCores`, `WithB`, `WithScratchpadsOnly`, `WithCloneRocketTiles`, `WithNExtTopInterrupts`.
- **Bus**: `WithDefaultMemPort`, `WithDefaultMMIOPort`, `WithDefaultSlavePort`, `WithNoMemPort`, `WithNoMMIOPort`, `WithNoSlavePort`, `WithCoherentBusTopology`, `WithIncoherentBusTopology`, `WithIncoherentTiles`.
- **Cache/ICache/DCache params**: `nSets`=64, `nWays`=4, `rowBits`, `nTLBEntries`=32, `tagECC`/`dataECC`, `prefetch`, `blockBytes`=64, `latency`=2, `fetchBytes`=4 (ICache); `nMSHRs`, `nSDQ`=17, `nRPQ`=16, `nMMIOs`, `pipelineWayMux`, `clockGate`, `scratch` (DCache).
- **Core constants** (exposed as `case object` Fields): `WordBits`, `StoreDataQueueDepth`=17, `ReplayQueueDepth`=16, `NMSHRs` (e.g., `Knob("L1D_MSHRS")`), `LRSCCycles`=32, `CoreInstBits`=16 or 32, `FetchWidth`, `RetireWidth`, `UseVM`, `FDivSqrt`, `NCustomMRWCSRs`, `ASIdBits`, `PgLevels`, `pgIdxBits`=12.
- **Subsystem**: `WithRV32`, `WithFP16`, `WithHypervisor`, `WithTimebase`, `WithDTS`, `WithNMemoryChannels(n)`, `WithNBanks(n)`, `WithCluster(depth, location=InCluster(...))` for L1/L2 cluster topology, `WithRoccExample` (RoCC accelerator).
- **ECC/issue history**: the "Remove parameters for some things that aren't parameterizable" commit shows that not everything is parameterizable — ISA constants like `XLen`, `UseCompressed`, `UseVM` are deliberately kept *outside* the parameter system.

The Rocket-Chip parameter system is the most explicit, well-documented, in-production CPU DSE knob set in the open-source world. ChipForge's own knobs should be designed against this benchmark.

### 3.3 BOOM v3 parameters and TAGE/Tournament BP sweeps

The BOOM `BoomCoreParams` case class exposes:

- **Front-end**: `fetchWidth` (default 1, "Large" config 8), `decodeWidth` (1, "Large" 3), `maxBrCount` (4 → 16), `numFetchBufferEntries` (16 → 24), `ftq: FtqParameters` with `nEntries` (32 in Large).
- **Issue / IQs**: `issueParams: Seq[IssueParams]`, each `(issueWidth, numEntries, iqType, dispatchWidth)`. The Large config is `IQT_MEM(1, 16, _, 3)`, `IQT_INT(3, 32, _, 3)`, `IQT_FP(1, 24, _, 3)`.
- **ROB / Registers**: `numRobEntries` (64 → 96), `numIntPhysRegisters` (96 → 100), `numFpPhysRegisters` (64 → 96).
- **LSQ**: `numLdqEntries` (16 → 24), `numStqEntries` (16 → 24).
- **Branch prediction**:
  - `enableBranchPrediction: Boolean`
  - `branchPredictor: Function2[BranchPredictionBankResponse, Parameters, Tuple2[Seq[BranchPredictorBank], BranchPredictionBankResponse]]` — *the function from response to predictor banks is itself a parameter*. This is the lever for switching between GShare, TAGE-L, TAGE-SC, BOOM v2 2BPD, etc.
  - `globalHistoryLength: Int = 64` (Large) / `bpdMaxMetaLength: Int = 120` (Large)
  - `localHistoryLength: Int = 32` (Large `WithBoom2BPD`: 1) / `localHistoryNSets: Int = 128` (Large `WithBoom2BPD`: 0)
  - `numRasEntries: Int = 32`, `enableRasTopRepair: Boolean = true`.

**Sweeping TAGE / Tournament BP**: the canonical pattern is to instantiate different `BranchPredictorBank` modules inside the `branchPredictor` function. For example, `WithTAGELBPD` wires `LoopBranchPredictorBank` → `TageBranchPredictorBank` → `BTBBranchPredictorBank` → `FAMicroBTBBranchPredictorBank` → `BIMBranchPredictorBank` into a Tournament chain, with TAGE table sizes parameterised by `BoomTageParams(tableInfo = Seq((nEntries, historyLen, tagBits)))`. `WithBoom2BPD` uses a *GShare-as-1-table-TAGE* (tableInfo = `Seq((256, 16, 7))`) to mimic the BOOM v2 predictor at a fraction of the TAGE-L cost. `WithNSmallBooms`/`WithNMediumBooms`/`WithNLargeBooms` then plug each of these BPD variants into differently sized cores — *exactly* the kind of DSE chip designers want.

**What this gives ChipForge**: the canonical pattern for "branch predictor = config, not instance". To replicate this in C++, expose `branch_predictor_factory` as a parameter (a callable that takes a history-length budget and returns a predictor instance) — that one indirection unlocks the TAGE-L / TAGE-SC / GShare / 2BPD dimension as a single DSE axis.

---

## 4. DSE methodology

### 4.1 Evaluation methods (per-point)

The seminal 2004 survey by Gries frames DSE as two orthogonal problems: (I) how is a single design point evaluated, and (II) how is the design space covered.

**Evaluation** is one of:
1. **Cycle-accurate simulation** (highest fidelity, slowest: 1–10⁴ KIPS, hours-to-weeks per benchmark × point).
2. **Statistical sampling** (SMARTS, TurboSMARTS — cuts simulation time by orders of magnitude with rigorous statistical guarantees; the technique ESESC builds on).
3. **Analytical/mechanistic models** (Sniper interval model; McPAT for power/area; LibPeq in ESESC).
4. **Predictive/surrogate models** (regression trees, ANNs, Gaussian processes, normalizing flows).
5. **RTL synthesis + gate-level power** (the EXPRESSION/Mishra and Tensilica/Stratus approach — closest to "real PPA" but slowest per point).

Pimentel's later tutorial chapter makes the trade-off explicit: simulation is accurate but prohibits exhaustive search; analytical models are fast but lossy; the trend is **hybrid** — analytical models to *prune*, simulation to *verify* the survivors.

### 4.2 Predictive modeling vs. full simulation

A 2006 ASPLOS paper by Ipek, McKee, de Supinski, Schulz & Caruana introduced the **ANN-based surrogate** for CPU DSE: train a neural network on ~1% of a 250K-point CMP design space and predict IPC at untested points with 4–5% error. The Lee & Brooks 2006/2007 HPCA paper showed that a single regression model can be reused across multiple DSE studies (Pareto-frontier analysis, pipeline-depth analysis, heterogeneity analysis), replacing ~900–1000 heuristic-driven simulations per study with one ~1000-point training pass — net time savings of three to four orders of magnitude.

A later refinement by Dubach et al. added a benchmark-representativeness step to give **whole-suite means** (cycles, energy, ED, ED²) instead of single-benchmark predictions, requiring 5× fewer training simulations than prior predictors and asymptotically an order of magnitude fewer than exhaustive sweep.

Recent extensions — **OneDSE** (arXiv:2505.03771, 2025), **SurroFlow** (uncertainty-aware normalizing-flow surrogate), and tree-based models (Multiple Additive Regression Trees, MART) — push the state of the art toward (a) workload-aware predictors that generalise across unseen workloads, (b) uncertainty quantification for active-learning sample selection, and (c) cross-microarchitecture transfer. ASSENT combines GA exploration for the discrete "what is in the design" axis with a continuous-space NN+ MILP fine-tuning step.

The take-away for ChipForge: **predictive modeling is the single most leveraged technique in modern CPU DSE literature** — every order-of-magnitude improvement in per-point evaluation time cascades into an order of magnitude wider Pareto sweep. ESESC's `libpeq` is a 2008-era hand-engineered special case of the same idea.

### 4.3 Parameter collapsing / pruning

The literature uses several terms:

- **Parameter collapsing / pruning** in the Lee & Brooks line: identify parameters that have negligible impact on the objectives (via ANOVA / variable clustering / significance testing) and fix them at a representative value, *collapsing* the high-dimensional design space into a low-dimensional one that can be searched more thoroughly.
- **Parameter dependency / interdependency pruning** (Palesi & Givargis, CODES 2002): build a graph of parameter dependencies, *prune* sub-spaces that cannot contain Pareto-optimal points because of infeasible combinations. Their GA-based extension uses **SPEA2** to find the Pareto front in the remaining feasible space.
- **Active learning** in regression-tree-based DSE: select the next design points to simulate based on the model's uncertainty (maximin space-filling sampling, MART, ASSENT).
- **Symmetry reduction** (mpsym, Goens/Nicolai/Castrillon 2022): identify symmetric architectures and only explore one representative per orbit, with reported 8.6× speed-up and 30× better SA results on Kalray MPPA3.
- **Incompatibility constraints and choice constraints** at the *graph* level (Design Space Graph, ADSG-ADORE, DLR 2024): the DSE graph itself encodes "this knob is invalid if that other knob is set this way", so impossible sub-trees are never traversed.

Gries summarizes the criterion: "*the right search strategy highly depends on the design space and its characteristics*; there is a trade-off between the effort required to configure an algorithm for a given design space, the quality of the results, and the total number of design points evaluated."

### 4.4 Pareto-frontier computation

Three families dominate:

1. **NSGA-II** (Non-dominated Sorting Genetic Algorithm II, Deb et al. 2002) — the de-facto standard. Used in the data-flow crypto processor DSE, the MultiExplorer Sniper/McPAT dark-silicon DSE, the clustered MPSoC DSE of Frid/Sruk/Jakobović, and most chiplet DSE papers. Produces a well-spread Pareto front, scales to mixed-discrete + continuous variable spaces, but is sample-inefficient and gets trapped in local minima.
2. **Weighted-sum / scalarization** — collapse the multi-objective problem to a single scalar objective and rerun for many weights. Trivially parallel; can miss non-convex parts of the Pareto front. Used in **MODNAS** and other gradient-based differentiable NAS / DSE work.
3. **ε-constraint** — optimize one objective subject to upper bounds on the others. Guaranteed to find non-convex parts of the front; can be combined with the scalarized search (the OneDSE paper uses this).
4. **Simulated annealing** (CarbonPATH) and **reinforcement learning** (ChipletGym) are also used for chiplet-specific DSE.
5. **GA + parameter-interdependency** is the classical hybrid (Palesi & Givargis): SPEA2 on top of Platune's dependency model.

### 4.5 Commonly tracked metrics

- **Performance**: IPC, CPI, total cycles, speedup, CPI-stack breakdown (Sniper: cycles lost per component — branch mispredict, L1/L2/LLC miss, frontend bubbles).
- **Area**: mm², gate count, register count, often via McPAT or post-synthesis reports.
- **Power**: total power (W), dynamic/static split, per-component power stack, power density (W/mm²) — for dark-silicon-aware DSE.
- **Energy**: total energy, energy-delay product (EDP), energy-delay-area product (EDAP), energy-delay-area² (EDA²P). McPAT reports EDA²P and EDAP explicitly.
- **Frequency / timing**: FMax (MHz, GHz), critical path delay.
- **Cost** (chiplet DSE): per-chiplet manufacturing cost, yield, total system cost.
- **Quality-of-result**: per-benchmark deviation, worst-case slack, multi-program throughput, weighted speedup (used in Many-Threaded workloads).
- **Reliability / fault tolerance**: bit-flip rates, SEU coverage (Pimentel — emerging axis).

---

## 5. Lessons learned from failed / difficult DSE attempts

The collective DSE literature converges on a small set of recurring failure modes. They should be treated as a checklist for the ChipForge DSE v2 design.

1. **Combinatorial explosion is not a *scale* problem, it's a *dimension* problem.** Palesi & Givargis: "the number of possible mappings quickly becomes intractable." Herget et al. on dCPS DSE: "the design space of dCPS is so much larger than classical SoC design spaces that it is unclear if state of the art is able to handle it." The lesson is that increasing the dimensionality of free parameters (rather than the range of each one) is what kills DSE, and that *parameter dependency modeling* is the single most effective countermeasure.

2. **Per-point evaluation time is the bottleneck.** Lee & Brooks: "the most effective heuristics, variants of steepest descent and genetic search, require between 900 and 1,000 simulations per optimization problem … in contrast, our regression models require 1,000 simulations per design space since they may be formulated once and used in multiple studies." Predictive modeling wins not because the surrogate is "more accurate" but because **it is reusable across studies**, amortising training cost over many DSE questions.

3. **Workload coverage is hidden in the metric.** A predictor trained on one workload does not generalise — OneDSE (2025) lists this as failure mode #2 of prior ML-based predictors, and Dubach et al. showed 5× training-data reduction by benchmark representativeness selection. The ChipForge lesson: a metric *and* a workload distribution are inseparable; report "IPC on parsec-benchmark-X" not "IPC".

4. **Parameter interaction breaks one-at-a-time sensitivity.** Gries: "*confounding between parameters is the rule, not the exception.*" Lee & Brooks' "varying all design parameters simultaneously instead of fixing most non-depth parameters" is the canonical methodological improvement. ChipForge should resist the temptation to plot "IPC vs. issue width while holding everything else constant" — that's not a Pareto front.

5. **Evaluation fidelity is not monotone.** MultiExplorer: "dark-silicon aware" Pareto analysis *required* using power density as a constraint, not as a second objective, because the true feasible region is disjoint in the unconstrained objective space. Several DSE efforts "fail to produce optimal designs" because they use inaccurate estimates for early pruning — i.e., they exploit evaluation fidelity at the wrong hierarchy level.

6. **Discrete/combinatorial + continuous mixed variables break standard continuous optimizers.** ADSG / ADORE / SBArchOpt papers flag this repeatedly; the right tool is NSGA-II or simulated annealing with discrete-aware moves, not gradient descent. ChipForge should match this: keep config-space topology in NSGA-II; keep continuous micro-tunings (e.g., FMax vs. Vdd) in a continuous sub-optimizer.

7. **Symmetry is common and underexploited.** mpsym (TU Dresden 2022) — the 8.6× / 30× numbers — argues most DSE papers waste 30× compute exploring isomorphic points. The ChipForge Config layer should explicitly identify symmetries (e.g., "two BOOM-Large cores and one Rocket-Small" is the same as "two BOOM-Large cores and one Rocket-Small" in any order) and only explore one representative.

8. **Combinatorial search can be trapped by infeasible regions.** Frid et al. on clustered MPSoC: "five multi-objective algorithms for design space exploration of sparsely connected clustered multiprocessor platforms … each one deals with infeasible solutions in the design space by using different strategies for mapping application to platform, as well as penalization of infeasible solutions." Without explicit infeasibility handling, the GA "wastes its budget on illegal configs" — the canonical lesson is to fail fast at the **graph** level (ADSG-style incompatibility constraints) before the algorithm runs.

9. **The "right" tool depends on the question.** Gries' framework: "*the designer has to balance the accuracy of the evaluation, the time it takes to evaluate one design point (including the implementation of the evaluation model), the precision/granularity of the design space coverage, and last but not least the possibilities for automating the exploration process.*" There is no universal DSE tool — a ChipForge DSE v2 must declare which axis it owns (microarchitectural sweeps vs. system-level mapping) and which it delegates.

10. **Abstraction crossing is where papers die.** The 2020 chiplet DSE paper had to integrate core pipeline simulation, cache simulation, NoC simulation, *and* cost/yield modeling — each at a different fidelity — into a single IntLP solver. RapidChiplet chose latency/throughput proxies (10⁴× faster than BookSim, 0.28% latency error) because full NoC simulation in the DSE inner loop was infeasible. The generalisable lesson: pick the lowest fidelity that preserves the *ranking* of design points, not the lowest fidelity that preserves *absolute values*; a proxy with 30% absolute error and 0% rank error is more useful than a McPAT-grade model that's 1000× slower.

---

## 6. References

**Academic DSE tools**
- Mishra & Dutt, *Processor Description Languages*, Morgan Kaufmann 2008
- EXPRESSION ADL home (UCI)
- Mishra, Kejariwal & Dutt, "Rapid exploration of pipelined processors through automatic generation of synthesizable RTL models", RSP 2003
- Mishra et al., "Synthesis-driven exploration of pipelined embedded processors", VLSI 2004
- Mishra, "Functional abstraction driven design space exploration …", ISSS 2001
- OpenDSE GitHub
- Mocasin
- mpsym
- Generative ASP-based DSE
- ESESC repo; tutorial slides
- Graphite repo; HPCA 2010
- Sniper; Sniper/McPAT PACT 2012
- MultiExplorer
- RapidChiplet — arXiv:2311.06081
- CHASE — ISPD 2026
- OneDSE — arXiv:2505.03771
- PriME
- Pinto et al. GPGPU sim

**Industrial EDA**
- Synopsys ARC options
- Synopsys ASIP exploration white paper
- Cadence Tensilica overview
- Cadence HW/SW co-design white paper
- Xtensa LX7 datasheet
- Xtensa ISA summary
- ImperasDV
- Imperas + Mentor
- SiFive Core Designer tutorial
- SiFive "Cloud Accelerated Idea to Silicon"
- RISC-V specs

**Chisel-based DSE**
- Chipyard configs/parameters/mixins
- Chipyard heterogeneous SoCs
- Rocket-Chip `Configs.scala`
- Rocket-Chip subsystem `Configs.scala`
- Rocket-Chip cache ECC issue
- BOOM parameterization
- BOOM BPD
- BOOM config-mixins (Tenstorrent fork)

**Methodology**
- Gries, "Methods for Evaluating and Covering the Design Space during Early Design Development"
- Pimentel, "Methodologies for Design Space Exploration"
- Herget et al., "Design Space Exploration for Distributed Cyber-Physical Systems"
- Palesi & Givargis, "Multi-objective design space exploration using genetic algorithms", CODES 2002
- Lee & Brooks, "Pareto Frontier Analysis …" HPCA 2007
- Ipek et al., "Efficient architectural design space exploration via predictive modeling"
- Dubach et al., "Rapid Early-Stage Microarchitecture Design Using Predictive Models"
- McPAT
- Frid, Sruk & Jakobović, "DSE of Clustered Sparsely Connected MPSoC Platforms"
- Optimisation of a data-flow crypto processor using NSGA-II
- ADSG / ADORE

---

Report delivered (~3,000 words). Key findings and a few **disambiguations** that may matter for your `dse_architecture.md` writeup:

1. **"Horizon/Marvel (UCLA)"** is not in the public record under that name. The relevant body of work is **Mishra's EXPRESSION / functional-abstraction / RTL-synthesis-driven DSE** (originally UCI, now U. Florida). I treated that as the UCLA-adjacent entry. The only "Horizon" paper I found was a 1988 SRC/Tera supercomputer that has nothing to do with DSE — flagging it to avoid misleading citations in your report.

2. **"Open-PEPP (TU Dresden)"** is more accurately **OpenDSE** (SDARG/opendse, MIT-licensed Java Y-chart framework), with related Dresden work in **Mocasin** (compiler-construction chair) and **mpsym** (symmetry-reduced hierarchical DSE).

3. **Industrial DSE is more constrained than the academic literature.** SiFive Core Designer and Xtensa/ARC are all *menu-of-derivatives* with enumerated options — not continuous sweeps. The actual continuous-sweep DSE happens *internally* at those companies; the publicly visible tool is a single-point configurator. This is a useful contrast to Chipyard's "all parameters free" model.

4. **Predictive modeling is the single most leveraged technique** in the published DSE literature since 2006 (Ipek, Lee/Brooks, Dubach, OneDSE 2025). Any DSE v2 that doesn't include a surrogate-modeling tier is leaving an order-of-magnitude on the table.

5. **BOOM's `branchPredictor: Function2[BankResponse, Parameters, …]` parameter** is the cleanest open-source example of "branch predictor family = a single DSE axis" — worth replicating in your C++ API as a `branch_predictor_factory` callable.

---

*Document extracted from background task bg_a592c1cb output. Original agent: librarian.*

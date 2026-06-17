# Open-Source RISC-V Superscalar / OoO DSE Reference Report

Comparative reference for the ChipForge DSE architecture. Each section focuses on **how the project exposes DSE axes and parameter sweeps**, not on background or marketing. All claims link to source-of-truth files.

---

## 1. Berkeley BOOM (SonicBOOM / BOOMv3)

| Field | Value |
|---|---|
| Repo | <https://github.com/riscv-boom/riscv-boom> (moved from `ucb-bar/riscv-boom`) |
| License | **BSD 3-Clause** (open source) |
| HDL | Chisel 3 / Scala |
| ISA | RV64GCB |
| CoreMark/MHz | 6.2 (SonicBOOM) |
| LoC | ~25–30 kChisel (estimated) |

### Pipeline Structure (default Medium, 3-wide)
- `fetchWidth=8` (max 8 instructions/cycle from i-cache), `decodeWidth=3` ([`parameters.scala`](https://github.com/riscv-boom/riscv-boom/blob/master/src/main/scala/common/parameters.scala))
- ROB: 96 entries, banked `W=decodeWidth` wide for superscalar dispatch/commit ([ROB docs](https://riscv-boom.github.io/riscv-boom/sections/reorder-buffer.html))
- Split Issue Queues: integer, memory, FP — each with its own `issueWidth` ([`issue-units.rst`](https://github.com/riscv-boom/riscv-boom/blob/master/docs/sections/issue-units.rst))
- LSQ: 24 LDQ + 24 STQ entries (Large config)
- Phys regs: 100 INT + 96 FP (Large config)
- BPU: TAGE-L (configurable BTB/BIM/TAGE variants) — `WithTAGELBPD` mixin
- FTQ (Fetch Target Queue) tracks inflight branches
- Scheduler policy: **un-ordered** (R10K-style) or **age-ordered** (collapsing queue) — selectable
- Uncore: reuses Rocket-Chip L1/L2/L3 + TileLink

### DSE Axes (from `src/main/scala/common/parameters.scala`)
- `fetchWidth`, `decodeWidth`, `dispatchWidth`, `commitWidth` (orthogonal in principle; tied by `W=decodeWidth` in practice — see ROB docs footnote [1])
- `numRobEntries`
- `issueParams: Seq[IssueParams]` — per-IQ (`issueWidth`, `numEntries`, `iqType={INT,FP,MEM}`, `dispatchWidth`)
- `numLdqEntries`, `numStqEntries`
- `numIntPhysRegisters`, `numFpPhysRegisters`
- `numFetchBufferEntries`, `maxBrCount`
- `ftq: FtqParameters(nEntries=...)`
- BPU: `enableGHistStallRepair`, `enableBTBFastRepair`, `enableSFBOpt` (Store-Fwd-Bypass)
- `enableAgePriorityIssue`, `enableFastLoadUse`, `enableCommitMapTable`, `enableFastPNR`
- `enablePrefetching`
- L1 I/D cache `nSets`, `nWays`, `rowBits`, `beatBytes` (via Rocket-Chip)

### Parameter System
- **Rocket-Chip `Config`/`Key` system** (Cake-pattern + dynamic `site`/`here`/`up` scope)
- `BoomCoreParams` case class is the knob bundle; `WithNLargeBooms(n)` etc. instantiate tiles
- Sample configs: `WithNSmallBooms`, `WithNMediumBooms`, `WithNLargeBooms`, `WithNMegaBooms` in `src/main/scala/common/config-mixins.scala`
- Heterogeneous multi-tile via `WithNBigCores + WithNLargeBooms` composition
- Documented discovery pain point: *"a significant challenge with the Rocket parameter system is being able to identify the correct parameter to use, and the impact that parameter has on the overall system"* — direct quote from Chipyard docs

### Plugin / Modularity
- **Not a plugin system.** Monolithic Chisel RTL: `Fetch`, `Rename`, `Dispatch`, `ROB`, `IssueUnit`, `Execute`, `LSU`, `BranchUnit`, `FPU` are all concrete classes in a fixed module hierarchy
- Variation happens through the `Config` parameter system, not by hot-swapping modules
- `WithTAGELBPD`, `WithBoomCacheBuffer` etc. are config mixins, not runtime plugins

### Issue-Width Sweep
- **Same source supports width 1 / 2 / 3 / 4 / 6 / 8** out of the box via the `Config` system
- `WithNSmallBooms` = 1-wide, `WithNMediumBooms` = 2-wide, `WithNLargeBooms` = 3-wide, `WithNMegaBooms` = 4-wide
- Constrained only by the manual definition of each sample configuration

### Why it's relevant for ChipForge
- The **Cake-pattern `Config` system is the gold standard** for declaring a design point from composable fragments. Each `With…` mixin is a partial config; `++` composes them. Direct analogue: ChipForge's `CpuConfig` struct + `with_*` builder methods.
- The `issueParams: Seq[IssueParams]` model — **a list of issue-queue descriptors, each with its own width/entries/type** — is exactly the IR ChipForge's PipeBuilder needs. Don't hardcode INT/MEM/FP queues; emit a vector of descriptors and let the plugin iterate.
- **Orthogonal dispatch/commit/issue widths** is the most important DSE axis. BOOM ties them in code (`W=decodeWidth`) but the docs explicitly note they need not be — ChipForge should keep them independent struct fields.
- Documented pain point: discoverability of which key affects what. ChipForge should ship a **knob→effect** table (or auto-generate from a Doxygen comment on each `CpuConfig` field).

---

## 2. XiangShan (雁栖湖 Yanqihu / 昆明湖 Kunminghu)

| Field | Value |
|---|---|
| Repo | <https://github.com/OpenXiangShan/XiangShan> |
| License | **Apache-2.0** (open source) |
| HDL | Chisel 3 / Scala |
| ISA | RV64 (supports RVA23 profile) |
| LoC | ~50–60 kChisel + supporting SoC (estimated; multi-variant) |

### Pipeline Structure
**Yanqihu (1st gen, archived)**: 11-stage, 6-wide decode/rename, 192 ROB, 64 LQ, 48 SQ, 160 Int PRF + 160 FP PRF, 16-entry RS per FU, TAGE-SC-L BPU, 6.2 CoreMark/MHz (HPCA'25 talk).

**Kunminghu (2nd gen, current)**:
- 6-wide decode/rename/dispatch ([`Parameters.scala`](https://github.com/OpenXiangShan/XiangShan/blob/master/src/main/scala/xiangshan/Parameters.scala))
- 224-entry Int regfile, 192-entry FP regfile, 128-entry Vec regfile
- **256-entry ROB** with μop compression (up to 6-μop/entry, 8-entry retire/cycle)
- 256-entry Rename Buffer (RAB) — decouples μop commit from regfile update
- **Front-end**: decoupled, multi-level composite prefetcher, BTB+TAGE-SC (FTQ-like)
- 32 KB L1I, 32 KB L1D, 64-entry fully-assoc DTLB, 32-entry ITLB, 2 K-entry STLB
- 1 MB non-inclusive L2, 6 MB non-inclusive L3 (LLC)
- Move elimination, instruction fusion
- Vector + Hypervisor extensions

### DSE Axes
- `DecodeWidth`, `RenameWidth`, `CommitWidth` (default 8 — superscalar behind a 6-wide front-end)
- `RobSize`, `LoadQueueSize`, `StoreQueueSize`
- `intPreg`, `fpPreg`, `vfPreg` (register file entries + banking)
- `IssueQueueSize` (default 20), `IssueQueueCompEntrySize` (default 12)
- `SchdBlockParams` — list of `{IssueBlockParams, ExeUnitParams, FuConfig}` per scheduler domain (Int / FP / Vec / Mem)
- Per-EXU latency, wakeup config, writeback/read port lists
- `dcacheParametersOpt`, `L2CacheParamsOpt` (cache hierarchy)
- **YAML runtime config** via `YamlParser` ([deepwiki](https://deepwiki.com/OpenXiangShan/XiangShan/8.2-configuration-parameters)) — overrides `XSCoreParameters` fields without recompiling Scala

### Parameter System
- Inherits Rocket-Chip `Config` / `Key` system
- Adds its own deep case-class hierarchy: `XSCoreParameters` → `BackendParams` → `SchdBlockParams` → `IssueBlockParams` → `ExeUnitParams` → `FuConfig`
- **`YamlParser`** maps YAML files onto `XSCoreParameters` / `SoCParameters` — lets users change cache sizes and structural params without editing Scala
- `ArgParser` for command-line overrides
- `Move elimination`, `instruction fusion`, `ROB compression` are toggles

### Plugin / Modularity
- **No plugin system.** Monolithic Chisel with a heavy backend reorganization. `BackendV2SchdParams` is one massive `object` with hardcoded scheduler blocks
- Recent commits (2024–2025) show **a major backend rewrite** (v2 → v3) — a sign that the monolithic structure makes evolution painful
- Configuration is layered: 1st-gen (Yanqihu) and 2nd-gen (Kunminghu) live in the same repo via branches/dirs

### Issue-Width Sweep
- **Not designed for width sweeps.** The default is 6-wide; the codebase has minimal `1-wide`/`2-wide` sample configs
- Widths are baked into `IssueBlockParams` lists; changing width means re-listing the entire FU set
- Sweeping would require nontrivial refactoring of the backend objects

### Why it's relevant for ChipForge
- **`SchdBlockParams` / `IssueBlockParams` / `ExeUnitParams` / `FuConfig` 4-level descriptor hierarchy** is the most detailed example in the wild. It explicitly captures: FU kinds, writeback ports, read ports, latency, wakeup rules. ChipForge's PipeBuilder should use a similar 3- or 4-level descriptor for its `EXE` unit model.
- **YAML-driven runtime configuration** is a high-value DSE feature — `YamlParser` lets researchers toggle `RobSize`, `IssueQueueSize` without a Chisel recompile. ChipForge should consider a similar `.yaml`/`.toml` overlay on top of the C++ `CpuConfig`.
- **Rename Buffer (RAB)** — a 256-entry intermediate buffer between ROB and regfile commit — is an interesting state-modification point. If ChipForge ever simulates aggressive OoO, decoupling rename-snapshot from ROB-commit via a similar buffer is a known technique.
- The painful v2→v3 rewrite is a cautionary tale: monolithic Chisel "configs" with deeply nested case classes become unmaintainable when you sweep too many orthogonal axes. ChipForge's C++ template/plugin approach is more amenable to incremental evolution.

---

## 3. RISC-V Chipyard Framework

| Field | Value |
|---|---|
| Repo | <https://github.com/ucb-bar/chipyard> |
| License | **BSD 3-Clause** (open source) |
| Purpose | Aggregation / generation framework on top of Rocket-Chip |

### Role in DSE
Chipyard is **not a core** — it is the meta-framework that:
- Aggregates Rocket, BOOM, Gemmini, NVDLA, Hwacha, SiFive cores, custom generators
- Provides the **Config / Keys / Traits / Mixins** idiom for the whole SoC
- Delivers flow drivers: Verilator sim, VCS sim, FireSim FPGA-accel, Hammer VLSI, IOBinders, FIRRTL passes

### DSE Axes Exposed
- **Tile mix**: `WithNRocketCores(n) ++ WithNBoomCores(m) ++ WithNSmallCores(k)` — heterogeneous tiles in one SoC
- **Cache hierarchy**: `WithL1CacheWays(4)`, `WithL2TLBs(512)`, `WithL2Sets(1024)`, `WithInclusiveCache(512)`
- **Bus width**: `WithSystemBusWidth(128)`
- **Memory system**: `WithAsyncTiles`, `WithTilePrefetchers`
- **Trace / debug**: `WithTraceIO`
- **Discovered via `make find-config-fragments`** — Chipyard actively catalogs what knobs exist

### Parameter System
- Inherits Rocket-Chip `Config` / `Key` system
- Adds **`TilePluginProvider`**: a generator classpath scan that injects per-tile behavior. Generators implement `chipyard.config.TilePluginProvider` under `generators/<name>/chipyard`; Chipyard discovers them at elaboration time
- **Keys are `Option[T]` with `None` = no-op** — default safe, opt-in to features
- "Config fragments additively set keys; traits optionally consume them" — strict separation of *declaration* (key) from *application* (trait) from *value* (config)

### Plugin / Modularity
- **First-class plug-and-play generators.** Each accelerator is its own `generators/<name>/` repo, brought in via mill build, discovered at elaboration
- Top-level `DigitalTop` is a `LazyModule` that **lazily negotiates** parameters between modules via Diplomacy + Chisel traits
- `CanHavePeriphery…` traits = additive mixin pattern (Cake pattern)
- **Discoverability via `make find-config-fragments`** — runtime introspection of what knobs exist

### Issue-Width Sweep
- **Indirect.** Sweeping the BOOM core's width uses `WithN{Small|Medium|Large|Mega}Booms` mixins. Sweeping the SoC's width means choosing how many of each tile type
- No automated "sweep all widths 1/2/4/8" tool — you write each `Config` class manually

### Why it's relevant for ChipForge
- **`make find-config-fragments`-style introspection is gold.** ChipForge's `CpuConfig` should be introspectable: a CLI like `chipforge config list` that prints every knob, its type, its default, and (ideally) a description. The C++ struct already has the info at compile time — emit it via a small `static constexpr` registration.
- **`TilePluginProvider` pattern = service discovery for SoC builders.** ChipForge's `IP` modules are an analogue. The lesson: keep the *protocol* (what a tile plugin must implement) tiny and discoverable.
- **`Option[T] = no-op` discipline for keys** is the cleanest way to make configs composable. ChipForge's `std::optional<T>` fields on `CpuConfig` should follow the same rule: a field set to `nullopt` means "don't touch the default", not "zero it out".
- **The doc itself admits DSE is hard**: *"We are still investigating methods to facilitate parameter exploration and discovery."* This is the real gap in the ecosystem — and a place where a C++/Python tooling approach (ChipForge's advantage) can shine.

---

## 4. Rocket Chip Generator

| Field | Value |
|---|---|
| Repo | <https://github.com/chipsalliance/rocket-chip> |
| License | **BSD 3-Clause** (open source, part of ChipsAlliance) |
| HDL | Chisel 3 / Scala |
| ISA | RV32 / RV64 (Rocket is in-order scalar) |

### Pipeline Structure (default BigCore)
- Single-issue, in-order, 5-stage (IF / ID / EX / MEM / WB)
- TLB-based MMU, BTB, RAS, simple 1-bit saturating branch predictor (or BTB + BHT in newer)
- L1 I-cache + D-cache, L2/L3 via SiFive uncore

### DSE Axes
Rocket is a small core; DSE axes are mostly:
- `nMSHRs` (Miss-Status Handling Registers)
- `nTLBEntries`
- L1 `nSets` / `nWays` / `nMSHRs`
- `fpu: Option[FPUParams]` — FPU presence, latency, divSqrt
- `useAtomicsOnlyForIO`, `useBackupBackupDTLB`
- `bpd: BPDParams` (predictor selection)
- `nPMPs` (Physical Memory Protection count)
- `nL2TLBEntries`, `nL2TLBWays`
- `coreParams: CoreParams` — `decodeWidth`, `issueWidth` (forced to 1 for Rocket), `bTBEntries`, `bhtEntries`, `rasSize`
- Uncore: `nTiles`, `L1toL2Network`, `bankedL2`

### Parameter System
- **Origin of the `Config` / `Key` system** that the whole RISC-V ecosystem (BOOM, XiangShan, Chipyard, SiFive cores) inherits
- Three scoping objects: `site` (root of hierarchy), `here` (current level), `up` (next level up)
- **`Config` is `case class` + `++` for composition**; "configs are additive, can override each other, and can be composed of other configs"
- A `Config` is a *function* `Parameters => Parameters`; passing it through `++` layers them on a stack
- Default small: `freechips.rocketchip.system.DefaultSmallConfig`; big: `DefaultBigConfig`

### Plugin / Modularity
- **Tiles are reusable** but there is no hot-plug plugin system at the core level
- **Uncore + diplomacy** for cross-module parameter negotiation — adapters negotiate bus widths, clock ratios, etc. before elaboration
- `LazyModule` + `LazyModuleImp` split: the module's *logical* connections (tile A needs AXI to memory) vs. *physical* elaboration are two phases

### Issue-Width Sweep
- **No — Rocket is fundamentally single-issue.** For width sweeps, the ecosystem turns to BOOM. Rocket is the "control group" / baseline.

### Why it's relevant for ChipForge
- **The `Config`/`Key` Cake pattern is the foundational idiom.** ChipForge's `CpuConfig` struct follows the same conceptual model: a flat-or-nested set of typed knobs that can be `++`-composed. The lesson: prefer **typed structs with default values** over a long `std::vector<std::pair<string, variant>>`.
- **`site`/`here`/`up` scoping** is what makes cross-tile parameter references (e.g. "L2 ways = 2× L1 ways") work. ChipForge's `CpuConfig` should let one field reference another (e.g. `FetchWidth * 2`) — a small expression layer, not free-form strings.
- **Diplomacy pattern**: parameters can be *negotiated* between modules rather than fixed by one side. This is a powerful DSE feature for clock ratios, bus widths, queue depths that depend on producer/consumer rates. ChipForge can implement a lightweight version: when an IP block instantiates, it can request min/max on a shared queue depth.
- **Submodules for tools**: rocket-chip pulls in `chisel3`, `firrtl`, `hardfloat`, `torture` as **submodules** — a single command updates the toolchain. ChipForge's CMake `FetchContent_Declare` plays the same role.

---

## 5. Hummingbirdv2 (Nuclei E203) & Andes AX65 (AE6501)

> **Important distinction**: Hummingbirdv2 is an **open-source** in-order 2-stage RV32 core from **Nuclei System Technology** (riscv-mcu/e203_hbirdv2). It is **not** an Andes product. The user asked about Hummingbirdv2/AE6501 (Andes); these are different families. Both are covered below.

### 5a. Hummingbirdv2 / E203 (Nuclei) — open source

| Field | Value |
|---|---|
| Repo | <https://github.com/riscv-mcu/e203_hbirdv2> |
| License | **Apache-2.0** |
| HDL | Verilog (hand-written, no HDL meta-language) |
| ISA | RV32IMAC or RV32EMAC |
| LoC | ~20 kLoC Verilog (estimated; ~40 .v files in `rtl/e203/`) |

#### Pipeline Structure
- **2-stage pipeline** by convention: F (IFU) → D/EX/WB (EXU). MEM stage is optional / off the critical path
- Machine mode only
- ITCM (instruction TCM) + DTCM (data TCM) — single-cycle access, no real L1 cache
- Simple BPU (lite-BPU)
- No FPU in default config
- **NICE** (Nuclei Instruction Co-unit Extension) — 4 custom instruction groups (Custom-0/1/2/3) for user-defined HW co-units

#### DSE Axes
Parameters are `\`define` macros in `rtl/e203/core/config.v`:
- `E203_CFG_ADDR_SIZE_IS_16/24/32` — bus address width
- `E203_CFG_REGNUM_IS_16/32` — number of architectural regs
- `E203_CFG_HAS_ITCM` / `E203_CFG_ITCM_ADDR_BASE/WIDTH` — TCM presence and size
- `E203_CFG_HAS_DTCM` / `E203_CFG_DTCM_ADDR_BASE/WIDTH` — DTCM presence and size
- `E203_CFG_REGFILE_LATCH_BASED` — flop vs. latch regfile
- `E203_CFG_HAS_ECC` — ECC on TCMs
- `E203_CFG_HAS_NICE` — NICE extension
- `E203_CFG_SUPPORT_AMO` — atomic extension
- `E203_CFG_SUPPORT_SHARE_MULDIV` — combined MUL/DIV unit
- `E203_CFG_SUPPORT_MCYCLE_MINSTRET` — performance CSRs
- `E203_CFG_DEBUG_HAS_JTAG` — debug module

#### Parameter System
- **Preprocessor `\`define` macros** in a single `config.v` file. No typed parameter system, no `Config` class, no plugin
- Each feature is a hard `ifdef`. Variant = different preprocessor pass

#### Plugin / Modularity
- **NICE is a true plugin slot** — 4 reserved instruction groups (Custom-0..3) connect to external user modules via the `nice_*` bus
- Otherwise: no plugin system. Monolithic Verilog with `ifdef`-selected features

#### Issue-Width Sweep
- **No.** Hummingbirdv2 is fundamentally scalar; width is not a knob. Changing width means re-writing the EXU and regfile

#### Why it's relevant for ChipForge
- **`config.v` `\`define` knobs are the worst-case DSE UX.** Recompile to change a TCM size, no type checking, no validation, no discovery beyond reading the source. ChipForge should treat this as a *negative* reference — anything more typed is better.
- **NICE is the right idea at the wrong level.** 4 custom opcode slots + a clean ICB bus is a clean way to expose *user extensions*, but it conflates user-extensibility with architectural configuration. ChipForge's `Plugin` interface should separate "structural DSE knobs" (CpuConfig fields) from "behavioral extensions" (Plugin classes).
- **Hand-written Verilog with `ifdef` does not scale to DSE.** This is the implicit argument for ChipForge's C++/TLM + generator approach: parameterized RTL that can sweep `issueWidth=1..8` at runtime is strictly more DSE-friendly than a tree of `ifdef`s.

### 5b. Andes AX65 (AE6501 family) — **commercial, NOT open source**

| Field | Value |
|---|---|
| Vendor | Andes Technology (<https://www.andestech.com>) |
| License | **Commercial IP license only** — no source available |
| Reference | <https://www.andestech.com/en/products-solutions/andescore-processors/riscv-ax65/> |

#### Pipeline Structure (from datasheet)
- 64-bit, 4-wide decode, **13-stage** superscalar out-of-order pipeline
- 128-entry ROB
- 8 functional pipelines (4 INT, 2 FP, 2 LSU)
- TAGE branch predictor + loop predictor
- 4–8 insts/cycle fetch
- 64 KB private I-cache + 64 KB private D-cache per core
- Split 2-level TLB with multiple concurrent walkers
- Up to 64 outstanding load/store instructions
- AndeStar V5 ISA (RISC-V GCB + scalar crypto + CMO + Andes extensions)
- SMP up to 8 cores, MESI coherence
- 9.25 CoreMark/MHz, 4.9 DMIPS/MHz, 8.25 specint2006/GHz

#### DSE Axes
**Not publicly documented** — Andes exposes the IP via license + AndesCore config tools, not via source. Customers get a "Configurator" GUI to pick core variants (A45 / A65 / N25 / D45 etc.), but the parameter ranges are NDA-gated.

#### Why we include it
- It's the **commercial high-end reference** for what a production RISC-V OoO core achieves, and it tells us what axes *customers* care about: 4-wide decode, 128 ROB, 13-stage pipeline, TAGE+loop predictor, SMP coherence. These are the headline numbers ChipForge's DSE should be able to *reproduce* (even at small scale) to validate the parameter model.
- It validates that **stage count, ROB, IQ, predictor type, issue width, register count, LSU depth** are the universal DSE axes across both open and commercial RISC-V OoO.

#### Why it's NOT a reference for ChipForge's implementation
- Closed source — no code to learn from
- The Configurator GUI is the wrong shape for research DSE (no scriptable sweep)

---

## 6. NaxRiscv (SpinalHDL, superscalar OoO)

| Field | Value |
|---|---|
| Repo | <https://github.com/SpinalHDL/NaxRiscv> |
| License | **MIT** (open source) |
| HDL | SpinalHDL / Scala |
| Stars | ~310 (active development) |
| ISA | RV32 / RV64, IMAFDCSU (boots Linux) |

### Pipeline Structure (high-perf config)
- **Superscalar OoO with register renaming** ([intro](https://spinalhdl.github.io/NaxRiscv-Rtd/main/NaxRiscv/introduction/index.html))
- Reference config: **2 decode, 3 execution units, 2 retire** (the README's "ex:" wording; actual configs vary)
- Decentralized hardware elaboration — **empty top-level parameterized entirely by plugins**
- 2.93 DMIPS/MHz, 5.02 CoreMark/MHz, 1.67 Embench-IoT baseline @ 155 MHz + 13.3 KLUT on Artix 7-3
- Non-blocking D-cache with multiple refill + writeback slots
- **BTB + GSHARE + RAS** branch predictors
- Hardware-refilled MMU (SV32, SV39)
- 3-cycle load-to-use via speculative cache-hit predictor
- Pipelines visualized in Konata (gem5 format) via Verilator

### DSE Axes
- Decode width, retire width, # of execution units — set at plugin instantiation time
- Issue queue: 2D matrix (rows × decodeCount columns), uniform entries across FUs
- ROB size, free-list size, load/store queue depth
- Regfile: dual-port (FPGA-friendly, distributed-RAM XOR LVT) **or** latch-based (ASIC); multiple write/read ports
- Branch predictor: GSHARE table size, BTB entries, RAS depth
- D-cache: refill slots, writeback slots, MSHR count
- TLB: hardware-refilled, set/associative

### Parameter System
- **No separate parameter file** — every knob is a constructor argument of a `Plugin` subclass
- Plugin instances are passed to `NaxRiscvConfig(plugins = Seq(...))`
- Example ([VexRiscv plugin pattern](https://github.com/SpinalHDL/VexRiscv), same author): `new BranchPlugin(earlyBranch = false, catchAddressMisaligned = false)`
- The "parameter sweep" is a Scala `for` loop generating multiple `Gen*` main objects

### Plugin / Modularity — **the strongest example in the survey**
- **Everything is a plugin.** Even the PC manager is a plugin; even the register file is a plugin
- Toplevel is *empty* until plugins are added. A minimal config still has 30+ plugins (e.g. `DocPlugin, MmuPlugin, FetchPlugin, PcPlugin, FetchCachePlugin, AlignerPlugin, FrontendPlugin, DecompressorPlugin, DecoderPlugin, RfTranslationPlugin, RfDependencyPlugin, RfAllocationPlugin, DispatchPlugin, BranchContextPlugin, HistoryPlugin, DecoderPredictionPlugin, BtbPlugin, GSharePlugin, Lsu2Plugin, DataCachePlugin, RobPlugin, CommitPlugin, RegFilePlugin, CsrRamPlugin, PrivilegedPlugin, PerformanceCounterPlugin, ALU0_ExecutionUnitBase, ALU0_IntFormatPlugin, ALU0_SrcPlugin, ALU0_IntAluPlugin, …` — visible in [issue #81 log output](https://github.com/SpinalHDL/NaxRiscv/issues/81))
- **Service framework** for cross-plugin contracts: a plugin can `provide ExceptionService` and another can `consume` it. Decoder service, CSR service, etc.
- **Stageable framework**: plugins declare named signals (`BYPASSABLE_EXECUTE_STAGE`, `RS1`, `REGFILE_WRITE_DATA`) that are auto-pipelined between stages. This is the killer feature — no manual signal routing between plugins.
- **Auto-multiport memory transformation** — a logical single-port RAM description is auto-fan-out to N ports at elaboration, so plugin code stays simple even when width grows
- **Custom instruction plugin** is a ~30-line file (see SimdAddPlugin in VexRiscv README)

### Issue-Width Sweep
- **Yes, but manually.** Sweeping width = changing `decodeCount` and editing the per-FU plugin list. The plugin list is reconstructed, not parameterized
- The framework is amenable to a sweep — every per-width variant is just a different `Seq[Plugin]` — but no automated sweep tool ships

### Why it's relevant for ChipForge
- **The Stageable framework is the most important idea in this entire report.** Signals like `RS1`, `REGFILE_WRITE_DATA` are *declared once* and automatically routed across the pipeline depth. In ChipForge's TLM, the equivalent would be a typed signal table on `CpuConfig` (e.g. `std::vector<StageSignal>`) that the PipeBuilder auto-instantiates with the right latency. This removes an enormous amount of plumbing code from per-stage plugins.
- **Service framework** (DecoderService, ExceptionService, CsrService) is exactly the contract model ChipForge's `Plugin` interface needs. Don't let plugins know about each other directly — let them publish/consume services.
- **Plugin is a class with `setup` + `build` lifecycle.** Setup runs before elaboration (asks for services, declares decodings); build runs after (instantiates hardware). The two-phase split keeps the elaboration order deterministic. ChipForge's `Plugin` should mirror this.
- **Custom instruction in 30 lines** is the bar. ChipForge should target: a `SimdAddPlugin`-style demo that adds a new instruction to a `CpuConfig` and re-elaborates in <100 lines of C++.
- **Auto-multiport memory** is a useful elaboration technique: write the FU as if it has 1 read port, the framework duplicates for issueWidth=4. ChipForge's C++ templates can do this at compile time without a framework, but a `for<auto N : widths>` generator is the analogue.
- **The 30+ plugins for a minimal config is a warning** — the framework requires the user to know the full plugin DAG. ChipForge should provide **named presets** (`SmallCpuPlugins()`, `MediumOoOPlugins()`) that bundle the common case.

---

## 7. VexRiscv (SpinalHDL, in-order) — same author, relevant plugin reference

| Field | Value |
|---|---|
| Repo | <https://github.com/SpinalHDL/VexRiscv> |
| License | **MIT** (open source) |
| HDL | SpinalHDL / Scala |
| ISA | RV32IMAFC[D] (configurable) |

### Pipeline Structure
- 2 to 5+ stages (Fetch → Decode → Execute → [Memory] → [WriteBack])
- In-order scalar; 1.44 DMIPS/MHz in max-perf config
- 23+ named `Gen*.scala` configurations in `src/main/scala/vexriscv/demo/`
- AXI4 / Avalon / Wishbone ready
- Linux-capable variant
- RV32 only

### DSE Axes (every one is a Plugin constructor arg)
- `IBusCachedPlugin`: `cacheSize`, `bytePerLine`, `wayCount`, `twoCycleRam`, `asyncTagMemory`, `addressWidth`, `cpuDataWidth`, `memDataWidth`
- `DBusCachedPlugin`: same
- `RegFilePlugin`: `regFileReadyKind = SYNC | ASYNC`, `zeroBoot`
- `HazardSimplePlugin`: `bypassExecute`, `bypassMemory`, `bypassWriteBack`, `bypassWriteBackBuffer`
- `BranchPlugin`: `prediction = NONE | STATIC | DYNAMIC | DYNAMIC_TARGET`, `earlyBranch`
- `CsrPlugin`: configurable interrupt/exception set
- `MmuPlugin`: `regionType`
- `MmuPluginNapot`: PMP region count
- `FpuPlugin`: `withDouble`, `asyncRegFile`, `mulWidthA`, `mulWidthB`, FMA latencies
- `MulDivIterativePlugin`: 33-cycle division via M-stage
- `DebugPlugin`: external debug support
- `EmbeddedRiscvJtag`: JTAG TAP
- `YamlPlugin`: emits CPU state to YAML for OpenOCD

### Parameter System
- **`VexRiscvConfig(plugins = Seq(...))`** — pure constructor-arg parameters
- No `Config`/`Key` system like Chisel. Just Scala case-class args
- Each Gen* is a `def main(args)` that constructs one CPU

### Plugin / Modularity — the canonical example
- Same architecture as NaxRiscv: Stageable, Service, two-phase setup/build
- **22+ pre-built configurations** in `src/main/scala/vexriscv/demo/` from `GenSmallest` (504 LUT) to `GenFullLinuxCapable` (3 kLUT)
- README documents 30-line custom-instruction plugin (SimdAddPlugin) — the gold standard
- Random-config regression test framework: `VEXRISCV_REGRESSION_SEED` generates *random* plugin combinations and validates them. This is **literally random DSE**.

### Issue-Width Sweep
- **No — VexRiscv is fundamentally scalar.** But the same author (Dolu1990) used the same plugin framework to build NaxRiscv, which is superscalar — so the framework demonstrably scales to width sweeps

### Why it's relevant for ChipForge
- **The `def main(args)` → `Gen*` pattern is the cleanest "1 config = 1 executable" model in the survey.** ChipForge's `chipforge elaborate --config=foo.yaml --output=foo.v` is the direct equivalent. Each `Gen*` is a small Scala program; each ChipForge config file is a small C++/YAML/TOML input.
- **Random-config regression** is a powerful DSE tool: instead of testing a hand-picked 5-point sweep, test 1000 random points and verify ISA conformance on all of them. ChipForge can ship a similar `--random-config` mode in its regression harness.
- **The README is a textbook on plugin-based DSE** — borrow the structure of the Plugins section (one table per plugin with `Parameters | type | description`) for ChipForge's plugin docs.
- **22 named configurations are too many to maintain by hand** — VexRiscv proves that plugin frameworks still need a *higher-level preset layer* to be usable. ChipForge should provide preset `CpuConfig` bundles: `SmallScalarPreset`, `MediumOoOPreset`, `WideOoOPreset`.

---

## Cross-Cutting Findings — DSE Axis Taxonomy

Aggregated from the 6 projects above, the DSE axes that **every** project exposes:

| Axis | BOOM | XiangShan | Chipyard | Rocket | E203 | AX65 | NaxRiscv | VexRiscv |
|---|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| Fetch width | ✓ | ✓ | ✓ | — | — | ✓ | ✓ | — |
| Decode width | ✓ | ✓ | ✓ | — | — | ✓ | ✓ | — |
| Issue width | ✓ | ✓ | — | — | — | ✓ | ✓ | — |
| Commit / retire width | ✓ | ✓ | — | — | — | ✓ | ✓ | — |
| ROB size | ✓ | ✓ | — | — | — | ✓ | ✓ | — |
| Issue queue size / # of IQs | ✓ | ✓ | — | — | — | ✓ | ✓ | — |
| LSQ size (LDQ + STQ) | ✓ | ✓ | — | — | — | ✓ | ✓ | — |
| Physical register count | ✓ | ✓ | — | — | — | ✓ | ✓ | — |
| Branch predictor type | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| BTB / BHT entries | ✓ | ✓ | ✓ | ✓ | — | ✓ | ✓ | — |
| RAS depth | ✓ | ✓ | — | — | — | — | ✓ | — |
| L1 I-cache (sets/ways/MSHR) | ✓ | ✓ | ✓ | ✓ | — | ✓ | ✓ | ✓ |
| L1 D-cache (sets/ways/MSHR) | ✓ | ✓ | ✓ | ✓ | — | ✓ | ✓ | ✓ |
| L2 / L3 cache | — | ✓ | ✓ | ✓ | — | ✓ | ✓ | — |
| TLB / MMU entries | — | ✓ | ✓ | ✓ | — | ✓ | ✓ | — |
| FPU presence | ✓ | ✓ | ✓ | ✓ | — | ✓ | — | ✓ |
| Pipeline depth (# stages) | partial | ✓ | — | — | partial | ✓ | partial | ✓ |
| CSR subset | — | — | — | — | ✓ | — | — | ✓ |
| ISA extension toggles | — | ✓ | — | — | ✓ | ✓ | ✓ | ✓ |
| Custom-instruction slot | — | — | — | — | ✓ (NICE) | — | ✓ | ✓ |

**Universal axes** (every OoO project exposes these): fetch/decode/issue/commit width, ROB size, IQ size, LSQ size, phys reg count, BPU type+size, L1 I/D cache config.

**Variability axes** (only some expose): L2/L3, MMU, FPU details, CSR subset, RAS.

**Closed-set axes** (mostly fixed once a project exists): pipeline depth (only XiangShan/AX65 treat it as a sweepable knob), core topology (in-order vs OoO).

---

## Cross-Cutting Findings — Parameter System Comparison

| Project | Mechanism | Type checking | Discoverability | Composable | Sweep-friendly |
|---|---|:-:|:-:|:-:|:-:|
| BOOM | Rocket-Chip `Config`/`Key` (Cake pattern) | ✓ | ✗ (manual) | ✓ (`++`) | medium |
| XiangShan | Rocket-Chip `Config` + `YamlParser` overlay | ✓ | medium (`ArgParser`) | ✓ | medium |
| Chipyard | Rocket-Chip `Config` + `make find-config-fragments` | ✓ | ✓ (CLI tool) | ✓ | high (for SoC) |
| Rocket-Chip | Original `Config`/`Key` (Cake pattern) | ✓ | ✗ | ✓ | medium |
| Hummingbirdv2 | `\`define` macros in `config.v` | ✗ | ✗ (read source) | ✗ | low |
| AX65 | Andes Core Configurator GUI (closed) | n/a | n/a | n/a | n/a |
| NaxRiscv | Plugin constructor args (Scala case classes) | ✓ | ✗ (read code) | ✓ (list) | medium |
| VexRiscv | Plugin constructor args (Scala case classes) | ✓ | medium (README) | ✓ (list) | high (random test) |

**Best in class**:
- **Discoverability**: Chipyard (`make find-config-fragments`)
- **Type safety + composability**: Rocket-Chip family (BOOM, XiangShan, Chipyard)
- **Plugin architecture**: NaxRiscv / VexRiscv (Stageable + Service frameworks)
- **Easiest for research DSE**: VexRiscv (random-config regression) + Chipyard (CLI discovery)

**Worst in class**: Hummingbirdv2 `\`define` macros — recompile to change a TCM size.

---

## Synthesis — what to steal for ChipForge's DSE

Ranked by impact:

1. **Adopt the Rocket-Chip `Config` / `Key` Cake pattern at the type level.** ChipForge's `CpuConfig` is already the right shape. Add a small **discovery CLI** (`chipforge config list`) that prints every knob, default, and description. The C++ struct metadata (via `static constexpr` or a registration macro) makes this trivial — Chipyard needed a custom tool to do it.

2. **Adopt the NaxRiscv Stageable / Service framework, ported to C++.** A `Stageable<T>` type that auto-pipelined through the TLM stages would eliminate ~30% of ChipForge's per-stage plumbing. `Service<T>` contracts (DecoderService, ExceptionService, BranchService) would decouple plugins.

3. **Use the `\`define`-vs-typed-config dichotomy as a polarity check.** Hummingbirdv2 is a warning; the typed `CpuConfig` is the right side of the line. Don't drift toward "just use preprocessor macros for speed".

4. **YAML/TOML overlay on top of the C++ struct** (XiangShan's `YamlParser`). Researchers want to run 50 sweeps overnight. Recompiling C++ for each is a non-starter. A `.yaml` overlay that maps onto `CpuConfig` fields without code changes is essential.

5. **Provide preset bundles** — `SmallScalarPreset()`, `MediumOoOPreset()`, `WideOoOPreset()`. VexRiscv's 22 hand-written `Gen*` files prove that plugin frameworks still need a *preset layer*; without it, users face a 30+ plugin DAG.

6. **Random-config regression** (VexRiscv). A `--random-config` mode that picks a random `CpuConfig`, elaborates, runs the ISA conformance suite, and records the point. Finds bugs that hand-picked sweeps miss.

7. **Keep `dispatchWidth`, `commitWidth`, `issueWidth`, `fetchWidth`, `decodeWidth` as independent struct fields.** BOOM's docs explicitly tie them in code (`W=decodeWidth`) but the design is more general; ChipForge should preserve that orthogonality from day one.

8. **Descriptor vectors for IQs and FUs** (XiangShan's `SchdBlockParams` → `IssueBlockParams` → `ExeUnitParams`). The ChipForge PipeBuilder should iterate a `std::vector<IssueQueueDesc>` and a `std::vector<FunctionalUnitDesc>` rather than hardcode INT/MEM/FP units. This is the key abstraction for issue-width sweeps.

9. **The universal DSE axes** (from the taxonomy above) are the **must-have** set for `CpuConfig` v1.0. ISA-extension toggles, CSR subset, and pipeline-depth are stretch goals.

10. **The ANDES AX65 numbers are the validation target**: 4-wide decode, 128 ROB, 13-stage, TAGE+loop predictor, 8 functional pipes. ChipForge doesn't need to reach those in v1, but the parameter model should be able to *describe* them (even if not yet synthesize them at high speed) so researchers can study small-scale OoO at fraction of the complexity.

---

## Direct Source File URLs

**BOOM**
- Parameters: <https://github.com/riscv-boom/riscv-boom/blob/master/src/main/scala/common/parameters.scala>
- Config mixins: <https://github.com/riscv-boom/riscv-boom/blob/master/src/main/scala/common/config-mixins.scala>
- Issue unit design: <https://github.com/riscv-boom/riscv-boom/blob/master/docs/sections/issue-units.rst>
- ROB design: <https://github.com/riscv-boom/riscv-boom/blob/master/docs/sections/reorder-buffer.rst>
- Parameterization docs: <https://docs.boom-core.org/en/latest/sections/parameterization.html>

**XiangShan**
- Top-level parameters: <https://github.com/OpenXiangShan/XiangShan/blob/master/src/main/scala/xiangshan/Parameters.scala>
- Backend scheduler params: <https://github.com/OpenXiangShan/XiangShan/blob/master/src/main/scala/xiangshan/backend/BackendParams.scala>
- HPCA'25 microarch slides: <https://tutorial.xiangshan.cc/hpca25/slides/20250302-HPCA25-2-Microarchitecture.pdf>
- DeepWiki config docs: <https://deepwiki.com/OpenXiangShan/XiangShan/8.2-configuration-parameters>

**Chipyard**
- Configs/Parameters/Mixins: <https://chipyard.readthedocs.io/en/latest/Chipyard-Basics/Configs-Parameters-Mixins.html>
- Keys/Traits/Configs: <https://github.com/ucb-bar/chipyard/blob/1.13.0/docs/Customization/Keys-Traits-Configs.rst>

**Rocket-Chip**
- README + parameterization: <https://github.com/chipsalliance/rocket-chip/blob/master/README.md>
- DIPLOMACY pattern: <https://carrv.github.io/2017/papers/cook-diplomacy-carrv2017.pdf>

**Hummingbirdv2 (Nuclei)**
- Core: <https://github.com/riscv-mcu/e203_hbirdv2>
- Core docs: <https://doc.nucleisys.com/hbirdv2/core/core.html>
- `config.v`: <https://github.com/riscv-mcu/e203_hbirdv2/blob/master/rtl/e203/core/config.v>

**Andes AX65 (commercial, reference only)**
- Product page: <https://www.andestech.com/en/products-solutions/andescore-processors/riscv-ax65/>
- Press release: <https://www.globenewswire.com/news-release/2022/11/02/2546765/0/en/Andes-Technology-Unveils-the-AndesCore-AX60-Series-an-Out-of-Order-Superscalar-Multicore-RISC-V-Processor-Family.html>

**NaxRiscv**
- Repo: <https://github.com/SpinalHDL/NaxRiscv>
- RTD docs: <https://spinalhdl.github.io/NaxRiscv-Rtd/main/NaxRiscv/introduction/index.html>
- Frontend (issue queue design): <https://spinalhdl.github.io/NaxRiscv-Rtd/main/NaxRiscv/frontend/index.html>
- Backend (commit + regfile): <https://spinalhdl.github.io/NaxRiscv-Rtd/main/NaxRiscv/backend/index.html>
- Plugin list (from issue #81): <https://github.com/SpinalHDL/NaxRiscv/issues/81>

**VexRiscv**
- Repo: <https://github.com/SpinalHDL/VexRiscv>
- Plugin list + custom-instruction example: <https://github.com/SpinalHDL/VexRiscv/blob/master/README.md>
- `VexRiscvConfig` definition: <https://github.com/SpinalHDL/VexRiscv/blob/master/src/main/scala/vexriscv/VexRiscv.scala>
- Demo configs: <https://github.com/SpinalHDL/VexRiscv/tree/master/src/main/scala/vexriscv/demo>

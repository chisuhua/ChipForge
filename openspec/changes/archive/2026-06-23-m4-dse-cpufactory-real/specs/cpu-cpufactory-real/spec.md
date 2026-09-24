## ADDED Requirements

### Requirement: CpuFactory registers 11 real plugins in EARLY/NORMAL/LATE order

`ip/cpu/cpu_factory.h::CpuFactory<T>::build_cpu(config)` MUST register 11 RISC-V plugins via three phase-specific private methods (`register_early_plugins`, `register_normal_plugins`, `register_late_plugins`) invoked in order. The 5 stub comments referencing "M4-DSE 启动时删除" / "M4-DSE 实施" MUST be removed.

**Actual registered plugins** (matches `ip/cpu/plugins/` and `ip/cpu/arch/riscv/` source):

- **EARLY (fetch stage)**: `IBusPlugin<U>` (uses `cfg.icache_latency`) → `BranchPredictorPlugin<U, BIMODAL_SZ, GSHARE_SZ, GHR_BITS, N_THREADS>(cfg.btb_entries)` (runtime BTB size, see M4.14)
- **NORMAL (decode/execute stages)**: `RiscvDecodePlugin<U>` → `HazardPlugin<U>` → `RiscvIntAluPlugin<U>` → `RiscvMulPlugin<U, LATENCY>` (LATENCY ∈ {1, 3, 5} from `cfg.mul_latency` via internal switch) → `RiscvBranchPlugin<U>` → `RiscvLsuPlugin<U>` → `RiscvCsrPlugin` (no template param)
- **LATE (memory/writeback stages)**: `DBusPlugin<U>` (uses `cfg.dcache_latency`) → `RegFilePlugin<U>`

The `mul_latency` switch is moved from `build_cpu` into `register_normal_plugins` to keep all normal-stage plugin instantiations in one place. The original `mul_latency` switch block in `build_cpu` (lines 237-260 in the pre-M4.12 file) is removed.

#### Scenario: 5-stage build_cpu returns 11-plugin PipeBuilder

- **WHEN** `CpuFactory<std::uint64_t>::build_cpu(cfg)` is invoked with `cfg.pipeline_stages == 5`
- **THEN** the returned `unique_ptr<PipeBuilder>` SHALL contain exactly 11 registered plugins
- **AND** the public API signature `CpuFactory<T>::build_cpu(const CPUConfig&)` SHALL remain unchanged (0 breaking)

#### Scenario: 3/7/10-stage build_cpu preserve plugin order

- **WHEN** `CpuFactory<T>::build_cpu(cfg_3stage)` or `build_cpu(cfg_7stage)` or `build_cpu(cfg_10stage)` is invoked
- **THEN** the 11 plugins SHALL be registered in the same EARLY/NORMAL/LATE order
- **AND** the difference between 5/7/10-stage builds SHALL be only the topology expansion (handled by TopologyBuilder<N_STAGES>::expand), not the plugin order

#### Scenario: stub comments removed

- **WHEN** `grep -nE "M4-DSE 启动时删除|M4-DSE 实施|M4 stub" ip/cpu/cpu_factory.h` is executed
- **THEN** zero matches SHALL be returned

### Requirement: BranchPredictor BTB_SIZE template parameter moved to runtime

`ip/cpu/plugins/branch_predictor.h::BranchPredictorPlugin` MUST remove the `BTB_SIZE` template parameter and replace it with the runtime constant `cfg.btb_entries` passed via the constructor. The template signature SHALL be `BranchPredictorPlugin<T, BIMODAL_SZ, GSHARE_SZ, GHR_BITS, N_THREADS>` (5 template params, down from 6).

The `btb_` member SHALL be `std::vector<BtbEntry>` sized at construction time. A runtime accessor `btb_size()` SHALL return the runtime BTB size. The `static_assert(BTB_SIZE power-of-2)` constraint is removed (runtime `cfg.btb_entries` is validated by `cpu_params_schema.json` enum: 16/32/64/128/256).

#### Scenario: BTB size as runtime constructor parameter

- **WHEN** `BranchPredictorPlugin<std::uint32_t, BIMODAL_SZ, GSHARE_SZ, GHR_BITS, N_THREADS>(cfg.btb_entries)` is constructed with `cfg.btb_entries == 16` or `64` or `256`
- **THEN** the BTB table SHALL allocate exactly that many entries
- **AND** the 5-stage, 7-stage, and 10-stage builds SHALL all share the same `BranchPredictorPlugin<U, 16, 16, 8, 1>` template instance (only 6 instantiations total: 3 predictors × 2 xlen)

#### Scenario: backward-compatible with existing test callers

- **WHEN** existing test files (`tests/cpu/test_branch_predictor.cpp`, `tests/cpu/test_forward_compat.cpp`) that previously passed `BTB_SIZE` as a template arg are compiled
- **THEN** they SHALL be updated to pass `cfg.btb_entries` (or `16` literal) as the constructor argument, e.g. `BranchPredictorPlugin<T> bp(16);` instead of `BranchPredictorPlugin<T, 16, 16, 16, 8, 1> bp;`

### Requirement: cpu_sim outputs real tohost from PicolibcHostMemory

`tools/cpu_sim/main.cpp` MUST integrate `PicolibcHostMemory` (64KB static RAM) to load ELF programs and detect the `tohost` exit marker. The placeholder output `tohost=0` MUST be replaced with `PicolibcHostMemory::tohost()`.

Since pipeline plugins (IBus/DBus/etc.) are still stubs at M4.15, the `tohost` value cannot be set by `pb->run()`. A minimal RV32I software interpreter MUST be added in `tools/cpu_sim/main.cpp` (when `--elf` is specified) that:
1. Reads instructions from `PicolibcHostMemory` at address 0 (the loaded .text section)
2. Decodes 4 RV32I instruction types used in `add.S`: `ADDI` (0x13), `ADD` (0x33), `SW` (0x23), `JAL` (0x6F)
3. Maintains x0-x31 registers and writes to `PicolibcHostMemory` on `SW` instructions
4. Stops on `JAL` (add.S's self-loop) or when `mem.exited()` is true

`ipc=0.0` remains a placeholder because retired-instruction counting is deferred to Phase 5+ (when pipeline plugins become real).

#### Scenario: cpu_sim --elf add.elf outputs tohost=1

- **WHEN** `cpu_sim --config ip/cpu/configs/cpu_default.json --elf build/add.elf --cycles 100` is invoked
- **THEN** the output SHALL contain `tohost=1` (real value from PicolibcHostMemory)
- **AND** the output SHALL contain `pipeline_stages=5` and `cycles=100`
- **AND** the placeholder `tohost=0` SHALL NOT appear

#### Scenario: cpu_sim without --elf preserves stub behavior

- **WHEN** `cpu_sim --config ip/cpu/configs/cpu_default.json --cycles 100` is invoked without `--elf`
- **THEN** the output SHALL contain `tohost=0` (PicolibcHostMemory initial state, preserved for backward compatibility)

### Requirement: add.elf end-to-end passes on 3/5/7/10-stage

The `add.elf` (10 RV32I instructions, compiled with `riscv32-unknown-elf-gcc` from `tests/cpu/manual_elf/add.S`) MUST run to completion with `tohost=1` (PASS) on all four pipeline depths: 3-stage (`cpu_embedded.json`), 5-stage (`cpu_default.json`), 7-stage (`cpu_superscalar.json`), and 10-stage (`cpu_deep_pipeline.json`).

The interpreter is **pipeline-agnostic** (reads from PicolibcHostMemory, not from pipeline plugins), so the same `add.elf` runs identically across all 4 depths.

#### Scenario: 3/5/7/10-stage add.elf tohost=1

- **WHEN** `cpu_sim --config <stage_config>.json --elf build/add.elf --cycles <cycles>` is invoked for each of the 4 stages
- **THEN** the output SHALL contain `tohost=1` and `pipeline_stages=<N>` for that stage

### Requirement: 576-sweep real tohost data with degenerate Pareto

`tools/dse/sweep_driver.py` MUST collect real `tohost` data for the 7-dimension 576-config design space (4 pipeline_stages × 3 branch_predictor × 3 btb_entries × 2 xlen × 2 mul_latency × 2 icache_latency × 2 dcache_latency = 576) by passing `--elf build/add.elf` to `cpu_sim`.

The real data MUST be written to `results/sweep.json` with exactly 576 rows, each containing `config`, `cycles`, `ipc`, `tohost`, `area_estimate` fields. With M4.15's pipeline-agnostic interpreter, the `cycles` and `ipc` values are constant across configs (all configs execute the same 11-instruction add.elf in 100 cycles), so the Pareto frontier is **degenerate** (0 non-dominated configurations). This is expected — meaningful Pareto optimization requires pipeline-real retired counting, deferred to Phase 5+.

#### Scenario: 576-row sweep.json with real tohost=1

- **WHEN** `python3 tools/dse/sweep_driver.py --cpu-sim ./build/src/cf_plugin/cpu_sim --cycles 100 --seed 0 --parallel 4 --elf build/add.elf --output results/sweep.json` is invoked
- **THEN** `results/sweep.json` SHALL contain exactly 576 entries
- **AND** all 576 entries SHALL have `tohost=1` (real value, replacing M5.16's 576/576 `tohost=0` stub)
- **AND** all 576 entries SHALL have `cycles=100.0` and `ipc=0.0` (constant, due to interpreter pipeline-agnostic behavior)

#### Scenario: Pareto frontier degenerate (documented limitation)

- **WHEN** `python3 tools/dse/pareto_analyzer.py --input results/sweep.json --maximize ipc --minimize cycles area_estimate --output results/pareto.json` is invoked
- **THEN** `results/pareto.json` SHALL be generated (may be empty or have very few entries due to constant cycles/ipc)
- **AND** the output SHALL note the degenerate state (pareto_analyzer falls back to other axes like `pipeline_stages` vs `mul_latency`)

### Requirement: integration test coverage for all 4 pipeline depths

`tests/cpu/integration/{test_3stage,test_5stage,test_7stage,test_10stage}_riscv.cpp` MUST each contain a new sub-test that runs `cpu_sim --elf build/add.elf` on the corresponding stage's config and asserts `tohost=1`. The sub-tests are added ADDITIVELY — existing sub-tests (topology, host_memory_init, tohost_mechanism) are preserved.

The `src/cf_plugin/CMakeLists.txt` MUST set `WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}` on `test_3stage_riscv` and `test_5stage_riscv` to allow relative paths (`./build/src/cf_plugin/cpu_sim`, `./build/add.elf`). `test_7stage_riscv` and `test_10stage_riscv` already have this WORKING_DIRECTORY.

#### Scenario: 4-stage end-to-end add.elf integration tests

- **WHEN** `ctest -R "test_[0-9]+stage_riscv"` is executed
- **THEN** all 4 tests SHALL pass
- **AND** each test SHALL invoke `cpu_sim --config <stage>.json --elf build/add.elf` and assert `tohost=1` in stdout

#### Scenario: 5-stage byte-identical preserved

- **WHEN** `ctest -R test_5stage -V` is executed
- **THEN** the 5 original sub-tests (`test_build_5stage`, `test_host_memory_init`, `test_load_binary`, `test_tohost_mechanism`, `test_tohost_fail`) SHALL still pass (M5.11 byte-identical)
- **AND** the new `test_5stage_add_elf_end_to_end` sub-test SHALL also pass (6/6 total)

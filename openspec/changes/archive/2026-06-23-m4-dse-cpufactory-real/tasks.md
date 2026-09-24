## 1. M4.12 Real Plugin Registration (CpuFactory stub → real)

- [ ] 1.1 Implement `register_early_plugins(PipeBuilder& pb, const CPUConfig& cfg)` in `ip/cpu/cpu_factory.cpp` — wires `IcacheFetchPlugin` (uses cfg.icache_latency) and `BtbPlugin` (uses cfg.btb_entries)
- [ ] 1.2 Implement `register_normal_plugins(PipeBuilder& pb, const CPUConfig& cfg)` in `ip/cpu/cpu_factory.cpp` — wires `DecodePlugin`, `RiscvAluPlugin`, `RiscvMulPlugin<T, LATENCY>` (LATENCY from cfg.mul_latency ∈ {1, 3, 5}), `RiscvBranchPlugin`, `BranchPredictor<T, KIND>` (KIND from cfg.branch_predictor)
- [ ] 1.3 Implement `register_late_plugins(PipeBuilder& pb, const CPUConfig& cfg)` in `ip/cpu/cpu_factory.cpp` — wires `DcacheLsuPlugin` (uses cfg.dcache_latency), `RegFilePlugin`, `RetirePlugin`
- [ ] 1.4 Invoke all three `register_*_plugins` from `CpuFactory<T>::build_cpu(config)` in the EARLY → NORMAL → LATE order
- [ ] 1.5 Remove 5 stub comments in `ip/cpu/cpu_factory.h` lines 239, 240, 340, 344, 347 (all "M4-DSE 启动时删除" / "M4-DSE 实施" markers)
- [ ] 1.6 Verify `tests/cpu/test_cpu_factory.cpp` (M4 baseline, 4 test cases) still passes — 0 breaking API change
- [ ] 1.7 Verify `openspec validate --specs` shows cpu-cpufactory-real passes 8/8 requirements

## 2. M4.13 reg_file writeback → retire single-direction flow fix

- [ ] 2.1 Refactor `ip/cpu/arch/riscv/reg_file.cpp::RegFilePlugin::tick()` to enforce single-direction: writeback stage writes to `reg_file_` storage only; retire stage independently reads `reg_file_` and fires `commit_hook`
- [ ] 2.2 Add invariant check `commit_count <= writeback_count` (single-direction prefix) — assert in debug builds
- [ ] 2.3 Verify `test_5stage_riscv.cpp` (M5.11 RISCV_TEST_ADD_PATTERN) still byte-identical — 0 regression
- [ ] 2.4 Compile `add.elf` (10 RV64I instructions) with `riscv64-unknown-elf-gcc` if not already present
- [ ] 2.5 Verify `cpu_sim --config configs/cpu_5stage.json --elf add.elf --cycles 100` sets `tohost=1` within 100 cycles
- [ ] 2.6 If test_5stage_riscv byte-identical breaks: revert M4.13 commit immediately, debug separately

## 3. M4.14 BranchPredictor template instantiation convergence

- [ ] 3.1 Remove `BTB_ENTRIES` template parameter from `BranchPredictor<T, KIND, BTB_ENTRIES>` → `BranchPredictor<T, KIND>` in `ip/cpu/branch_predictor.h`
- [ ] 3.2 Add `cfg.btb_entries` field access in `BranchPredictor` constructor (runtime constant, not template)
- [ ] 3.3 Update `BranchPredictor` implementation `arch/riscv/branch_predict.cpp` — replace compile-time `std::array<BTBEntry, BTB_ENTRIES>` with `std::vector<BTBEntry>` sized at construction time
- [ ] 3.4 Add `if (cfg.btb_entries == 16) { ... } else if (cfg.btb_entries == 64) { ... } else if (cfg.btb_entries == 256) { ... }` runtime branch in `BranchPredictor::tick()` for gshare hash table size
- [ ] 3.5 Verify `nm --size-sort build/src/cf_plugin/cpu_sim | grep BranchPredictor` total symbol size ≤ 1.5 MB (≤ 50% of M5 baseline ~3.0 MB)
- [ ] 3.6 Run M5-DSE 18-case BTB regression (5/7/10-stage × 3 BTB sizes × 2 predictors) — all cases must pass
- [ ] 3.7 Verify 41/41 ctest baseline still passes (M5.11 + M5.12 + M5.13 + 4 integration tests)

## 4. M4.15 cpu_sim real ipc output

- [ ] 4.1 Remove stub code path in `tools/cpu_sim/main.cpp` that outputs `ipc=0.0` regardless of config
- [ ] 4.2 Integrate `CpuFactory<T>::build_cpu(config)` to construct a real `PipeBuilder` from `CPUConfig`
- [ ] 4.3 Add `--stub` CLI flag in `cpu_sim/main.cpp` for regression compatibility with M5.16 sweep toolchain
- [ ] 4.4 Wire `run_cycles(N)` invocation and output real `cycles` and `ipc = n_retired / cycles` values
- [ ] 4.5 Verify `cpu_sim --config configs/cpu_5stage.json --cycles 100` outputs real `cycles` and `ipc` in [0.0, 2.0]
- [ ] 4.6 Verify `cpu_sim --config configs/cpu_5stage.json --cycles 100 --stub` outputs `cycles=100, ipc=0.0` (regression)
- [ ] 4.7 Verify M5.16 sweep toolchain unit tests that use `--stub` still pass

## 5. M4.16 add.elf end-to-end on 3/5/7/10-stage

- [ ] 5.1 Verify `add.elf` is compiled and available (Step 2.4 artifact) — 10 RV64I instructions, riscv64-unknown-elf-gcc
- [ ] 5.2 Run `cpu_sim --config configs/cpu_3stage.json --elf add.elf --cycles 100` — assert `tohost=1`
- [ ] 5.3 Run `cpu_sim --config configs/cpu_5stage.json --elf add.elf --cycles 100` — assert `tohost=1` AND byte-identical with M5.11 baseline
- [ ] 5.4 Run `cpu_sim --config configs/cpu_7stage.json --elf add.elf --cycles 100` — assert `tohost=1`
- [ ] 5.5 Run `cpu_sim --config configs/cpu_10stage.json --elf add.elf --cycles 100` — assert `tohost=1`
- [ ] 5.6 If any stage fails tohost=1: debug via commit_count + retire_count trace, revert + investigate

## 6. M4.17 576-sweep real data and Pareto frontier

- [ ] 6.1 Run `python3 tools/dse/sweep_driver.py --cpu-sim ./build/src/cf_plugin/cpu_sim --cycles 100 --seed 0 --parallel 4 --output results/sweep.json` (no `--stub`)
- [ ] 6.2 Verify `results/sweep.json` contains exactly 576 entries
- [ ] 6.3 Verify majority of entries have non-zero `ipc` (some pathological configs may legitimately have `ipc=0.0` due to instruction retirement failure within 100 cycles)
- [ ] 6.4 Run `python3 tools/dse/pareto_analyzer.py results/sweep.json --output results/pareto.json`
- [ ] 6.5 Verify `results/pareto.json` contains a strict subset of the 576 sweep rows
- [ ] 6.6 Verify every non-frontier row is dominated by at least one frontier row
- [ ] 6.7 Verify ASCII chart output shows non-trivial distribution (not all 0.0)
- [ ] 6.8 Commit `results/sweep.json` and `results/pareto.json` (replaces M5.16 stub-data)

## 7. M4.18 Integration test coverage for all pipeline depths

- [ ] 7.1 Upgrade `tests/cpu/integration/test_5stage_riscv.cpp` to use `add.elf` (replace M5.11 RISCV_TEST_ADD_PATTERN) — assert `tohost=1` via `cpu_sim --elf`
- [ ] 7.2 Replace `tests/cpu/integration/test_7stage_riscv.cpp` (M5-DSE placeholder, topology-only) with `test_7stage_add_elf.cpp` — assert `tohost=1`
- [ ] 7.3 Replace `tests/cpu/integration/test_10stage_riscv.cpp` (M5-DSE placeholder) with `test_10stage_add_elf.cpp` — assert `tohost=1`
- [ ] 7.4 Add `tests/cpu/integration/test_3stage_add_elf.cpp` (optional, 3-stage was not in M5-DSE test matrix) — assert `tohost=1`
- [ ] 7.5 Update `src/cf_plugin/CMakeLists.txt` to register new test targets and add.elf as a test fixture
- [ ] 7.6 Run `ctest` — verify 41/41 baseline + new add_elf tests all PASS (target: 45/45)

## 8. M4.19 Performance baseline documentation

- [ ] 8.1 Create `docs/performance/m4-cpufactory-real-baseline.md` with 576-row table (columns: pipeline_stages, branch_predictor, btb_entries, xlen, mul_latency, icache_latency, dcache_latency, cycles, ipc, area_estimate)
- [ ] 8.2 Update `ip/README.md` CpuFactory section: status "stub" → "real (11 plugins)"
- [ ] 8.3 Add 11 plugin names grouped by EARLY/NORMAL/LATE phase to `ip/README.md`
- [ ] 8.4 Run full `ctest` final verification: target 45/45 PASS
- [ ] 8.5 Run `openspec validate --specs` final: target 13/13 PASS (12 existing + 1 new cpu-cpufactory-real)
- [ ] 8.6 Run `verify-architecture` skill for CpuFactory 真实化验收 (M3/M4/M5 三阶段一致性)

## 9. PR and merge

- [ ] 9.1 Create atomic commits for each Phase (M4.12 → M4.19), each commit is independently buildable + ctest-passing
- [ ] 9.2 Generate PR description at `.omo/plans/m4-dse-cpufactory-real-pr-description.md` (Summary + Test Plan + Risk Assessment)
- [ ] 9.3 Push branch to origin and create PR
- [ ] 9.4 PR review and merge to main
- [ ] 9.5 Run `openspec archive m4-dse-cpufactory-real -y` (skip prompt)
- [ ] 9.6 Verify `openspec validate --specs` shows 13/13 PASS post-archive
- [ ] 9.7 Update `docs/architecture/overview.md` roadmap: M4-DSE status → ✅ DONE

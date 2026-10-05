## ADDED Requirements

### Requirement: `CpuFactoryChmem::build_cpu` 可选参数扩展

**Scope clarification**: `cpu_factory_chmem.h` is `#ifdef CF_PLUGIN_USE_CH_MEM`-only (file-line 11-13 raises `#error` outside CH_MEM mode). All `build_cpu` behavioral Scenarios below apply ONLY in CH_MEM mode. The TLM-mode factory path is `ip/cpu/cpu_factory.h` (separate file, out of this change's scope).

The function `cf::cpu::CpuFactoryChmem<T>::build_cpu` SHALL accept three additional optional parameters (`enable_mmu`, `mmu_mode`, `enable_cache`) appended **after** the existing four parameters. Each new parameter SHALL use `std::optional` to express three-state semantics.

ABI-compat rule: existing callsites (4-or-5-positional-arg calls) SHALL compile without modification, because new parameters have defaults (`std::nullopt`).

#### Scenario: 现有调用方零修改 (grep 验证: 6 文件 / 12 call sites)
- **WHEN** any of the following 6 CH_MEM-mode callsite files is rebuilt (verified via `grep -rn 'CpuFactoryChmem.*build_cpu\|CpuFactoryChmem<.*>::build_cpu' tests/cpu tools/verilator_runner`):
  - `tools/verilator_runner/cpu_verilator_sim.cpp` (1 call)
  - `tests/cpu/test_cpu_rtl_regfile_alu.cpp` (1 call)
  - `tests/cpu/test_cpu_memory_model_chmem.cpp` (3 calls: lines 127, 177, 395)
  - `tests/cpu/test_cpu_decoded_inst_migration.cpp` (1 call: line 322)
  - `tests/cpu/test_cpu_chmem_vendored_elf.cpp` (1 call: line 94)
  - `tests/cpu/test_cpu_5stage.cpp` (5 calls: lines 53, 75, 122, 152, 193)
- **THEN** the build SHALL succeed with 0 error 0 warning, AND the resulting `chipforge_tests` + `chipforge_tests_chmem` binaries SHALL pass all existing test cases unchanged (no behavioral regression).

#### Scenario: CH_MEM enable_mmu 三态语义 (MMU hook 仅占位)
- **WHEN** `build_cpu` is invoked with `enable_mmu = std::nullopt` (or omitted)
- **THEN** the function SHALL behave as the original 4-parameter version: no MMU hook, no additional log output, preserving current 7-plugin pipeline contract.

- **WHEN** `build_cpu` is invoked with `enable_mmu = true` AND `mmu_mode = "bare"`
- **THEN** the function SHALL emit a stderr/log line `"MMU hook enabled (mode=bare) — mmu_chmem.h not yet implemented, see change verilator-mmu-bare-plumbing-e2e"` AND SHALL NOT register any MMU plugin (CH_MEM-mode MMU plugin requires `ip/mmu/tlm/mmu_chmem.h` which is not in scope of this change). The 7-plugin pipeline contract SHALL remain unchanged. The downstream consumer (`cpu_verilator_sim`) SHALL still reach `tohost=1` because MMU is a no-op in this state.

- **WHEN** `build_cpu` is invoked with `enable_mmu = true` AND `mmu_mode = "sv32"` (or "sv39"/"sv48")
- **THEN** the function SHALL emit a stderr/log line `"MMU hook enabled (mode=<mmu_mode>) — mmu_chmem.h not yet implemented, see change verilator-mmu-bare-plumbing-e2e"` AND SHALL NOT register any MMU plugin (same reason as above). NO `RiscvMMUPlugin` instantiation SHALL occur in CH_MEM mode (RiscvMMUPlugin is a TLM-mode class; CH_MEM mode does not reference it).

- **WHEN** `build_cpu` is invoked with `enable_mmu = false` (explicit)
- **THEN** the function SHALL NOT emit any MMU-related log line AND SHALL NOT register any MMU plugin (explicit-disable suppresses hook).

#### Scenario: L1Cache CH_MEM fail-fast (无 silent degradation)
- **WHEN** `build_cpu` is invoked with `enable_cache = true` (regardless of `enable_mmu`)
- **THEN** the function SHALL throw `std::runtime_error` with message containing the substring `"L1Cache CH_MEM not implemented; refer to change verilator-l1cache-e2e-coverage"`. The function SHALL NOT silently degrade to no-cache behavior (per v0.10.4 hotfix lesson: silent degradation hides regressions). The throw message SHALL additionally reference the change responsible for wiring replacement (`verilator-l1cache-e2e-coverage` §What Changes will include a task that replaces this throw with L1CachePlugin CH_MEM registration).

### Requirement: `cpu_verilator_sim` CLI 三个新 flag

The executable `cpu_verilator_sim` SHALL accept three new CLI flags: `--enable-mmu`, `--mmu-mode <string>`, `--enable-cache`. These flags SHALL be forwarded to the `CpuFactoryChmem::build_cpu` call inside `generate_verilog()`.

#### Scenario: `--help` 输出新增三项
- **WHEN** user invokes `cpu_verilator_sim --help`
- **THEN** the usage output SHALL list all 8 flags (5 existing + 3 new) with their defaults, AND a "Note: --enable-cache requires L1Cache CH_MEM (see change verilator-l1cache-e2e-coverage in wave4)" line SHALL appear at the end.

#### Scenario: `--enable-mmu` 单 flag 启用 (CH_MEM hook 仅占位)
- **WHEN** user invokes `cpu_verilator_sim --elf tests/cpu/manual_elf/add.elf --enable-mmu` (without `--mmu-mode`)
- **THEN** `mmu_mode` SHALL default to `"bare"` per current behavior. The Verilog output SHALL complete elaboration with 0 error (the function SHALL emit a TODO log line per the `enable_mmu` Scenario above, but no MMU plugin SHALL appear in Verilog since `mmu_chmem.h` is not yet implemented). The simulation SHALL still reach `tohost=1` (within the 5-ELF baseline cycle count × 1.2 cap) because MMU hook is a no-op.

#### Scenario: `--mmu-mode sv32` 参数透传 (CH_MEM hook 仅占位)
- **WHEN** user invokes `cpu_verilator_sim --elf <elf>. --enable-mmu --mmu-mode sv32`
- **THEN** `build_cpu` SHALL receive `enable_mmu=true`, `mmu_mode="sv32"`, AND the elaboration SHALL succeed (0 error). A TODO log line SHALL be emitted: `"MMU hook enabled (mode=sv32) — mmu_chmem.h not yet implemented, see change verilator-mmu-bare-plumbing-e2e"`. Simulation SHALL still reach `tohost=1` because MMU hook is a no-op (no actual sv32 translation occurs; the Sv32 encoding is purely logged for downstream consumption).

#### Scenario: `--enable-cache` 触发 fail-fast
- **WHEN** user invokes `cpu_verilator_sim --elf <elf>. --enable-cache`
- **THEN** `cpu_verilator_sim` SHALL exit with non-zero status AND stderr SHALL contain the substring `"L1Cache CH_MEM not implemented; refer to change verilator-l1cache-e2e-coverage"`.

#### Scenario: 多 flag 组合
- **WHEN** user invokes `cpu_verilator_sim --elf <elf>. --enable-mmu --mmu-mode bare --enable-cache`
- **THEN** the `--enable-cache` fail-fast SHALL trigger FIRST (fail-fast ordering is implementation-defined; spec only requires L1 abort message to be present in stderr).

### Requirement: ELF vendor 模板扩展

The script `tests/cpu/manual_elf/build_manual_elf.sh` SHALL register two new template names: `mmu_bare` and `l1cache_basic`. Each template SHALL produce a vendored `.S` source file and a compiled `.elf` artifact using the existing `riscv64-unknown-elf-as` + `riscv64-unknown-elf-ld` toolchain (or current toolchain at the time of execution).

#### Scenario: `mmu_bare` 模板产出
- **WHEN** user invokes `bash tests/cpu/manual_elf/build_manual_elf.sh mmu_bare`
- **THEN** `tests/cpu/manual_elf/build_mmu_bare.S` SHALL exist AND `tests/cpu/manual_elf/mmu_bare.elf` SHALL be a valid RV32 ELF that, when run with `cpu_verilator_sim --enable-mmu --elf mmu_bare.elf`, reaches `tohost=1` (verifies the Bare translation plumbing path).

#### Scenario: `l1cache_basic` 模板产出
- **WHEN** user invokes `bash tests/cpu/manual_elf/build_manual_elf.sh l1cache_basic`
- **THEN** `tests/cpu/manual_elf/build_l1cache_basic.S` SHALL exist AND `tests/cpu/manual_elf/l1cache_basic.elf` SHALL be a valid RV32 ELF that exercises one cache hit + one cache miss path (per `link.ld` memory layout).

#### Scenario: 现有模板 ABI 兼容
- **WHEN** user invokes any of the existing 8 templates (`add`, `and`, `div`, `mul`, `or`, `sll`, `srli`, `sub`)
- **THEN** the corresponding `.S` and `.elf` artifacts SHALL be byte-identical to HEAD (no regression in existing manual_elf contents).

### Requirement: 3 架构门禁零失败

The change SHALL NOT introduce any regression to the 3 architecture gates: `tools/verify_adr.sh`, `tools/verify_plugin_decision.sh`, `tools/check_plugin_portability.sh`.

#### Scenario: 门禁静态检查 0 失败
- **WHEN** the change is committed and pushed to `main` or `develop` branch
- **THEN** GitHub Actions workflow `architecture-gates.yml` SHALL pass all 3 steps with exit code 0.

#### Scenario: 双模约束检查 0 失败
- **WHEN** `tools/check_plugin_portability.sh` runs against the modified `ip/cpu/cpu_factory_chmem.h`
- **THEN** the script SHALL NOT flag any of the 4 ADR-040 Tier-1 checks (early-return in `at_stage`, `ch_mem` penetration in `ip/*/tlm/`, `pb.run()` in `Plugin::build()`, raw `std::array` storage).

### Requirement: AGENTS.md + CHANGELOG 同步（无数字变化）

The change SHALL update AGENTS.md "已知测试状态" segment and CHANGELOG.md v0.10.x segment WITHOUT modifying any "实测数字" (PASS/FAIL counts).

#### Scenario: AGENTS.md 仅文字扩展
- **WHEN** this change's archive step runs
- **THEN** AGENTS.md SHALL contain a new line in the "已知测试状态" segment indicating "[verilator] 1/1 PASS (5 ELF × tohost=1) — v0.10.x 扩展 CLI flags 3 项 (--enable-mmu/--mmu-mode/--enable-cache), 不增 test cases". No existing PASS/FAIL count SHALL be altered.

#### Scenario: CHANGELOG v0.10.x 新增条目
- **WHEN** `CHANGELOG.md` is updated as part of archive
- **THEN** a new entry SHALL be added under the v0.10.x segment with title matching the change name (`verilator-cpu-factory-extensible-params`) and body describing: (a) 3 CLI flag additions, (b) `build_cpu` 3 optional parameter additions, (c) 2 ELF template additions, (d) L1Cache CH_MEM fail-fast hook, (e) downstream blockers (Change 2a + 2b).

#### Scenario: honesty_audit 数字无变化
- **WHEN** `bash tools/v0100-bootstrap.sh review` runs against the post-archive tree
- **THEN** the `§honesty_audit` segment SHALL be byte-identical to HEAD pre-change (because no test case PASS/FAIL count has been modified).
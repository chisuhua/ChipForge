## ADDED Requirements

### Requirement: 3 个 `[cache-verilator]` TEST_CASE family 扩展

The file `tests/cache/test_l1cache_refill_verilator.cpp` SHALL contain exactly 3 TEST_CASEs under the `[cache-verilator]` family tag. Each TEST_CASE top-level comment SHALL contain the literal substring `**plumbing only — semantic assertions beyond hit/miss NOT verified**`.

The file SHALL be conditionally compiled: only when `CF_PLUGIN_USE_CH_MEM` is defined (CH_MEM mode). The file SHALL be registered in `tests/CMakeLists.txt` `CHMEM_TEST_SOURCES` list, with `CF_L1CACHE_VERILATOR_BASELINE_CSV` compile definition.

#### Scenario: skip-when-absent 一致性
- **WHEN** `CF_VERILATOR_SIM_BIN` file is not present at build time
- **THEN** all 3 TEST_CASEs SHALL `SUCCEED("verilator binary not built — skipping")` and return early, matching `tests/cpu/test_cpu_verilator_sim.cpp:63-68` skip pattern.

#### Scenario: TEST_CASE 1 cache hit tohost=1
- **WHEN** TEST_CASE `l1cache_refill_verilator_hit_tohost1` runs against the 5 vendored `tests/cpu/riscv_tests/elf/rv32ui-p-{add,addi,auipc,beq,jal}` ELFs with `cpu_verilator_sim --enable-cache --elf <elf> --cycles 5000`
- **THEN** each ELF SHALL reach `tohost=1` AND the cycle count SHALL be `≤ baseline_csv[elf][cache_only] × 1.2`, where `baseline_csv[elf][cache_only]` is loaded from `CF_L1CACHE_VERILATOR_BASELINE_CSV`.

#### Scenario: TEST_CASE 2 cache miss tohost=1
- **WHEN** TEST_CASE `l1cache_refill_verilator_miss_tohost1` runs against `tests/cpu/manual_elf/l1cache_basic.elf` (which exercises 1 cache hit + 1 cache miss path) with `cpu_verilator_sim --enable-cache --elf l1cache_basic.elf --cycles 5000`
- **THEN** the ELF SHALL reach `tohost=1` AND the cycle count SHALL be `≤ baseline_csv[l1cache_basic][cache_only] × 1.5` (manual_elf 接受标准, 不在 5 ELF baseline 表)。

#### Scenario: TEST_CASE 3 L1Cache + MMU 联动
- **WHEN** TEST_CASE `l1cache_refill_verilator_mmu_bare_full_chain` runs against `tests/cpu/manual_elf/l1cache_basic.elf` with `cpu_verilator_sim --enable-mmu --mmu-mode bare --enable-cache --elf l1cache_basic.elf --cycles 5000`
- **THEN** the ELF SHALL reach `tohost=1` AND the cycle count SHALL be `≤ baseline_csv[l1cache_basic][cache_mmu_bare] × 1.5` AND stderr SHALL contain `"Bare"` substring (来自 cpu_factory_chmem 透传 mmu_mode, 与 Change 1 §3 CLI 测试一致)。

**Note**: TEST_CASE 3 同时依赖 `cache-phase1.5-4way` archive (L1Cache CH_MEM 完整实装) + `verilator-mmu-bare-plumbing-e2e` (Change 2a) archive (`--enable-mmu --mmu-mode bare` plumbing verified)。任一未 archive 则 TEST_CASE 3 fail-fast, 接受降级为 `[cache-verilator] 2/3 PASS` (release 标注)。

### Requirement: L1Cache cycle baseline CSV 落盘

The file `tests/cache/test_l1cache_refill_verilator_baselines.csv` SHALL contain two sections: (1) `cache_only` mode — 5 ELF rows (add/addi/auipc/beq/jal), (2) `cache_mmu_bare` mode — 1 ELF row (l1cache_basic.elf).

Schema: `elf_name,mode,median_cycles,verilator_version` (header in first line).

#### Scenario: CSV schema 合规
- **WHEN** the CSV file is parsed by the test harness
- **THEN** each row SHALL have exactly 4 columns matching the schema. The first row SHALL be the header. Section delimiters (空行 + `# cache_only section` + 空行 + `# cache_mmu_bare section`) SHALL separate the two mode groups for reader clarity.

#### Scenario: median 5-run 校准
- **WHEN** any row's 5-run median changes by > 20% from the previously committed CSV
- **THEN** the change SHALL be flagged as ARCHITECTURAL CHANGE in `tasks.md` §10 (per design.md §D6 mitigation).

#### Scenario: Verilator 版本锚定
- **WHEN** the Verilator version is upgraded from `5.052` to `5.060` (or any other version bump)
- **THEN** the CSV `verilator_version` column SHALL be updated AND a new baseline regeneration commit SHALL be made.

### Requirement: 修改 `mmu-cache-integration-test` spec 增 Verilator full-chain requirement

The spec file `openspec/specs/mmu-cache-integration-test/spec.md` SHALL be delta-updated with a new ADDED Requirement titled "L1Cache + MMU Verilator full-chain e2e" matching this change's TEST_CASE 3 contract.

#### Scenario: spec delta 落地
- **WHEN** the spec delta is archived (post-change-archive)
- **THEN** `openspec/specs/mmu-cache-integration-test/spec.md` SHALL contain an `## ADDED Requirements` section with at least one Requirement whose Scenario asserts `--enable-mmu --mmu-mode bare --enable-cache` 链路在 Verilator 仿真下 tohost=1 + cycle ≤ baseline × 1.5。

### Requirement: 零回归 + 架构门禁

The change SHALL NOT introduce any regression to existing test family counts:
- `[verilator]` 1/1 PASS
- `[mmu-verilator]` 5/5 PASS (Change 2a)
- `[mmu]` 53/53 PASS
- `[cache]` 既有 21 baseline PASS (cache-phase1.5-4way 实装后 ≥24)
- `[cpu-l1-mmu-demo]` 6/6 PASS
- `[cpu-integration]` 81/81 PASS
- `[cpu]` 19/19 PASS

The 3 architecture gates (`tools/verify_adr.sh`, `tools/verify_plugin_decision.sh`, `tools/check_plugin_portability.sh`) SHALL all pass with exit code 0.

#### Scenario: `[verilator]` 1/1 不退化
- **WHEN** `bash tools/run_chipforge_tests.sh --tag "[verilator]"` runs
- **THEN** the result SHALL be 1 case passed (`cpu_verilator_sim_tohost1` for 5 ELF), 0 failed.

#### Scenario: `[mmu-verilator]` 5/5 不退化
- **WHEN** `bash tools/run_chipforge_tests.sh --tag "[mmu-verilator]"` runs (post Change 2a archive)
- **THEN** the result SHALL be 5 cases passed, 0 failed.

#### Scenario: `[cache-verilator]` 3/3 PASS (or 2/3 degraded)
- **WHEN** `bash tools/run_chipforge_tests.sh --tag "[cache-verilator]"` runs
- **THEN** the result SHALL be 3 cases passed (or 2 cases passed + 1 deferred, marked accordingly in release notes).

#### Scenario: 3 架构门禁 0 失败
- **WHEN** the change is committed and pushed
- **THEN** GitHub Actions workflow `architecture-gates.yml` SHALL pass all 3 steps with exit code 0.

### Requirement: AGENTS.md + CHANGELOG 同步 (含实测数字)

The change SHALL update AGENTS.md "已知测试状态" segment and CHANGELOG.md v0.9.x segment WITH the new `[cache-verilator]` family PASS count.

#### Scenario: AGENTS.md 新增 [cache-verilator] 行
- **WHEN** this change's archive step runs
- **THEN** AGENTS.md SHALL contain a new line in "已知测试状态" segment with content matching: `**[cache-verilator]** \`3/3 PASS\` (cache hit + miss + L1Cache+MMU 联动) — **plumbing only — semantic assertions beyond hit/miss NOT verified** — change verilator-l1cache-e2e-coverage v0.9.x`.

#### Scenario: CHANGELOG v0.9.x 新增条目
- **WHEN** `CHANGELOG.md` is updated
- **THEN** a new entry SHALL be added under v0.9.x segment with title matching change name (`verilator-l1cache-e2e-coverage`) and body describing: (a) 3 TEST_CASEs under `[cache-verilator]` family, (b) L1Cache cycle baseline CSV, (c) L1Cache + MMU full-chain Verilator e2e (TEST_CASE 3), (d) cache-phase1.5-4way archive dependency.

#### Scenario: honesty_audit 数字按 HEAD 实测
- **WHEN** `bash tools/v0100-bootstrap.sh review` runs against post-archive tree
- **THEN** the `§honesty_audit` segment SHALL reflect the new `[cache-verilator] 3/3 PASS` count AND the `[mmu-verilator] 5/5` / `[verilator] 1/1` / `[mmu] 53/53` / `[cpu-l1-mmu-demo] 6/6` counts SHALL be byte-identical to v0.10.4 hotfix baseline (no silent drift across wave4-wave5 boundary).
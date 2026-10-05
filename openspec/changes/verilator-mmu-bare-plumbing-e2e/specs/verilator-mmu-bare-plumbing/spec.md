## ADDED Requirements

### Requirement: 3 个 `[mmu-verilator]` TEST_CASE family 扩展 (修复 C2 缩 scope)

The file `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` SHALL contain exactly **3 TEST_CASEs** under the `[mmu-verilator]` family tag. Each TEST_CASE top-level comment SHALL contain the literal substring `**plumbing only — translation semantics NOT verified**` to prevent future maintainers from mistaking these tests for translation correctness verification.

The file SHALL be conditionally compiled: only when `CF_PLUGIN_USE_CH_MEM` is defined (CH_MEM mode). The file SHALL be registered in `tests/CMakeLists.txt` `CHMEM_TEST_SOURCES` list.

**Scope 修订声明** (修复 Oracle/Metis C2 critical): 本 change 仅验证 **CLI plumbing**, 不验证 MMU 语义。TLM 端 v0.10.4 hotfix 回归防护由 `[mmu]` family (`test_mmu_cache_integration.cpp::EndToEndTranslationThroughBridge` + `[tlb-refill]` 2 cases) 负责; CH_MEM+Verilator 链路 MMU 语义层承认无防护, 由 `cpu-pipeline-mmufault-handler` follow-up 闭环后承接。

#### Scenario: skip-when-absent 一致性
- **WHEN** `CF_VERILATOR_SIM_BIN` file is not present at build time
- **THEN** all 3 TEST_CASEs SHALL `SUCCEED("verilator binary not built — skipping")` and return early, matching `tests/cpu/test_cpu_verilator_sim.cpp:63-68` skip pattern.

#### Scenario: TEST_CASE 1 baseline 5 ELF tohost=1
- **WHEN** TEST_CASE `mmu_bare_plumbing_tohost1_baseline_5_elf` runs against the 5 vendored `tests/cpu/riscv_tests/elf/rv32ui-p-{add,addi,auipc,beq,jal}` ELFs with `cpu_verilator_sim --enable-mmu --mmu-mode bare --elf <elf> --cycles 2000` (修复 S8: 与 `cpu_verilator_sim.cpp:39` 默认 max_cycles 一致)
- **THEN** each ELF SHALL reach `tohost=1` AND the cycle count SHALL be `≤ baseline_csv[elf] × 1.2`, where `baseline_csv[elf]` is loaded from `CF_MMU_VERILATOR_BASELINE_CSV` compile definition. The `1.2` cap is empirical (20% scheduling jitter tolerance, validated in tasks.md §2 calibration step).

#### Scenario: TEST_CASE 2 elaboration 0 error + verilator --cc 编译通过
- **WHEN** TEST_CASE `mmu_bare_plumbing_elaboration_zero_error` runs: `CpuFactoryChmem<ch_uint<32>>::build_cpu(&ctx, nullptr, kElfBase, true, elf, true, "bare", false)` called AND `pb->elaborate(ctx)` runs AND `pb->to_verilog(verilog_path)` produces Verilog file AND popen `verilator --cc --public-flat-rw -j 0 -Wno-WIDTH -Wno-UNOPTFLAT --top-module top <verilog>` runs
- **THEN** the elaboration SHALL complete with 0 error AND `pb->to_verilog` SHALL produce non-empty Verilog file AND the `verilator --cc` invocation SHALL exit with status 0.

**Note** (修复 C1): The Verilog output SHALL **NOT** contain `RiscvMMUPlugin` instantiation (RiscvMMUPlugin is TLM-only class, not emitted in CH_MEM elaboration). The MMU hook is a no-op log message per `cpu_factory_chmem.h` Change 1 spec.

#### Scenario: TEST_CASE 3 full-chain no-cache (manual_elf)
- **WHEN** TEST_CASE `mmu_bare_plumbing_no_cache_full_chain` runs: `cpu_verilator_sim --enable-mmu --mmu-mode bare --elf tests/cpu/manual_elf/mmu_bare.elf --cycles 2000` runs (NOT `--enable-cache`)
- **THEN** the runner SHALL output `TOHOST=1 PASS` (cycle count ≤ 2000, manual_elf 不在 baseline 5 ELF 表, 接受宽松上限) AND the assembly path SHALL exercise 1 MMU-related CSR write (e.g., `csrw satp, ...`) to validate the MMU bridge hookup (the ELF is auto-loaded into imem+dmem by `cpu_verilator_sim`).

### Requirement: cycle baseline CSV 落盘

The file `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv` SHALL contain a header line `elf_name,mode,median_cycles,verilator_version` followed by 5 data lines (one per vendored ELF: add/addi/auipc/beq/jal), all with `mode=bare_mmu` column value. The `median_cycles` SHALL be computed from 5 runs per ELF and stored as integer.

CSV second line SHALL contain a comment: `# cap=median*1.2 (empirical 20% jitter tolerance, see spec §TEST_CASE 1)` to document cap rationale (修复 S7).

#### Scenario: CSV schema 合规
- **WHEN** the CSV file is parsed by the test harness
- **THEN** each row SHALL have exactly 4 columns matching the schema. The first row SHALL be the header. The second row SHALL be the cap documentation comment.

#### Scenario: median 5-run 校准
- **WHEN** any ELF's 5-run median changes by > 20% from the previously committed CSV
- **THEN** the change SHALL be flagged as ARCHITECTURAL CHANGE in `tasks.md` §10 (per design.md §D6 mitigation).

#### Scenario: Verilator 版本锚定
- **WHEN** the Verilator version is upgraded from `5.052` to `5.060` (or any other version bump)
- **THEN** the CSV `verilator_version` column SHALL be updated AND a new baseline regeneration commit SHALL be made (to prevent cross-version cycle drift).

### Requirement: 零回归 + 架构门禁 (修复 S3 删 [cpu] 19/19)

The change SHALL NOT introduce any regression to existing test family counts:
- `[mmu]` 53/53 PASS (含 `[tlb-refill]` 2 cases 用 `satp_ppn_` workaround, 提供 v0.10.4 hotfix TLM 端防护)
- `[verilator]` 1/1 PASS
- `[cpu-l1-mmu-demo]` 6/6 PASS
- `[cpu-integration]` 81/81 PASS
- `[chmem]` 9/9 PASS
- `[riscv-tests]` 40/40 PASS

**注意** (修复 S3): 原 spec 草稿中"`[cpu] 19/19 PASS`"是 fabricated 数字（AGENTS.md grep 无此计数），本 Requirement 已删除该 baseline 承诺。实际 `[cpu]` family case 数需在 archive 前实测或仅引用有据可查的 family（如 `[chmem]` 9/9、`[cpu-integration]` 81/81）。

The 3 architecture gates (`tools/verify_adr.sh`, `tools/verify_plugin_decision.sh`, `tools/check_plugin_portability.sh`) SHALL all pass with exit code 0.

#### Scenario: `[mmu]` 53/53 不退化
- **WHEN** `bash tools/run_chipforge_tests.sh --tag "[mmu]"` runs
- **THEN** the result SHALL be 53 cases passed, 0 failed (matching v0.10.4 hotfix verified count).

#### Scenario: `[verilator]` 1/1 不退化
- **WHEN** `bash tools/run_chipforge_tests.sh --tag "[verilator]"` runs
- **THEN** the result SHALL be 1 case passed (`cpu_verilator_sim_tohost1` for 5 ELF), 0 failed.

#### Scenario: `[cpu-l1-mmu-demo]` 6/6 不退化
- **WHEN** `bash tools/run_chipforge_tests.sh --tag "[cpu-l1-mmu-demo]"` runs
- **THEN** the result SHALL be 6 cases passed, 0 failed.

#### Scenario: 3 架构门禁 0 失败
- **WHEN** the change is committed and pushed
- **THEN** GitHub Actions workflow `architecture-gates.yml` SHALL pass all 3 steps with exit code 0.

### Requirement: AGENTS.md + CHANGELOG 同步 (修复 C2 5→3 PASS)

The change SHALL update AGENTS.md "已知测试状态" segment and CHANGELOG.md v0.10.x segment WITH the new `[mmu-verilator]` family PASS count (**3/3** expected, real-tested — 修复 C2: 从原 5 改为 3)。

#### Scenario: AGENTS.md 新增 [mmu-verilator] 行
- **WHEN** this change's archive step runs
- **THEN** AGENTS.md SHALL contain a new line in "已知测试状态" segment with content matching: `**[mmu-verilator]** \`3/3 PASS\` (5 ELF × tohost=1 + elaboration 0 error + manual_elf full-chain) — **plumbing only — translation semantics NOT verified** — change verilator-mmu-bare-plumbing-e2e v0.10.x (CLI plumbing only; v0.10.4 hotfix TLM 防护由 [mmu] family 负责)`.

#### Scenario: CHANGELOG v0.10.x 新增条目
- **WHEN** `CHANGELOG.md` is updated
- **THEN** a new entry SHALL be added under v0.10.x segment with title matching change name (`verilator-mmu-bare-plumbing-e2e`) and body describing: (a) 3 TEST_CASEs under `[mmu-verilator]` family (CLI plumbing only), (b) cycle baseline CSV at `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv`, (c) explicit scope declaration that v0.10.4 hotfix regression protection is owned by TLM `[mmu]` family, (d) downstream `cpu-pipeline-mmufault-handler` follow-up for true sv32 e2e + MMU exception handler.

#### Scenario: honesty_audit 数字按 HEAD 实测
- **WHEN** `bash tools/v0100-bootstrap.sh review` runs against post-archive tree
- **THEN** the `§honesty_audit` segment SHALL reflect the new `[mmu-verilator] 3/3 PASS` count AND the `[verilator] 1/1` / `[mmu] 53/53` / `[cpu-l1-mmu-demo] 6/6` / `[cpu-integration] 81/81` counts SHALL be byte-identical to v0.10.4 hotfix baseline (no silent drift).

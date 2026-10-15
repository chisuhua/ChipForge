# cpu-pipeline-tick-rca Specification

## Purpose

Define the diagnostic methodology contract for the `cpu-pipeline-tick-rca` change: a pure investigation (no product code changes) of why 5 RISC-V simulation tests FAIL when calling `cpu_sim` CLI but PASS when calling C++ API directly. The deliverable is a diagnostic report (in `openspec/changes/cpu-pipeline-tick-rca/design.md`) identifying the most likely root cause and proposing fix direction for the follow-up `cpu-pipeline-tick-fix` change.

## ADDED Requirements

### Requirement: Investigation MUST follow 4-line + control-experiment methodology

The diagnostic process MUST investigate root cause candidates in the following order with explicit control experiment prepended:

1. **Control experiment** (L4 discriminator) — add temporary `--no-mmu` CLI flag to `tools/cpu_sim`, run `cpu_sim --no-mmu --elf build/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 1000` and observe whether `tohost=1` appears in output.
2. **L4 (Config Divergence)** — investigate whether `cfg.enable_mmu` differs between CLI (default `true` from `CPUConfig` struct since `load_config()` does not parse `enable_mmu` from JSON) and C++ API runner (explicit `false` in `test_rv32ui_runner.cpp:109`).
3. **L2 (fetch CtrlLink halt_when)** — investigate `ip/cpu/plugins/ibus.h:54-67` `fetch_ctrl->halt_when` lambda and whether `PTW_ACTIVE` misfires in `enable_mmu=true` configuration.
4. **L3 (StageLinkPlugin EARLY)** — investigate `ip/cpu/plugins/stage_link.h` EARLY closures for cross-stage payload propagation.
5. **L1 (build_cpu_impl + auto_throw)** — static exclusion of the `std::unique_ptr<PipeBuilder>` + Result wrapper + auto_throw difference between CLI and C++ API paths.

#### Scenario: L4 confirmed via control experiment
- **WHEN** control experiment with `--no-mmu` produces `tohost=1` in cpu_sim output
- **THEN** L4 is confirmed as root cause and the diagnostic report MUST document that `cfg.enable_mmu=true` (CLI default) vs `cfg.enable_mmu=false` (runner explicit) is the primary divergence
- **AND** subsequent L2/L3/L1 investigation is OPTIONAL (only for secondary root causes)

#### Scenario: L4 excluded, L2 deep dive
- **WHEN** control experiment with `--no-mmu` still produces `tohost=0`
- **THEN** L4 is excluded and L2 deep dive MUST follow with runtime log points 1 and 2 (defined below)

### Requirement: Investigation MUST respect file scope boundary

The diagnostic process MUST NOT read or modify files outside the documented whitelist:

- **Whitelist (read + write log allowed, P0 重做对齐 tasks.md §0.7)**: `tools/cpu_sim/main.cpp`, `ip/cpu/cpu_factory.h`, `ip/cpu/cpu_factory_chmem.h` (reference only), `ip/cpu/plugins/ibus.h`, `ip/cpu/plugins/dbus.h`, `ip/cpu/plugins/stage_link.h`, `ip/cpu/plugins/mmu.h`, `ip/cpu/plugins/mmu_exception_handler.h` (TLM only), `ip/mmu/tlm/MMUPlugin.h` (Bare shortcut 判定必读), `ip/mmu/tlm/MMUPlugin.cpp`, `ip/mmu/tlm/mmu_keys.h` (PTW_ACTIVE key 定义), `include/cf/plugin/pipe_builder.h`, `include/cf/plugin/result_macros.h`, `ip/cpu/picolibc_host_memory.h`, `tests/cpu/configs/*.json` (5 FAIL test config 对照用)
- **Exclusion list (read-forbidden, write-forbidden, P0 重做对齐 tasks.md)**: `src/cf_plugin/bridge/*`, `**/*_chmem.h` (CH_MEM files), `build/_deps/**` (CppTLM/CppHDL source), `ip/mmu/lib/**` (mmu internals), `ip/mmu/chmem/**`

#### Scenario: Encountering CH_MEM file during investigation
- **WHEN** investigation naturally leads to a CH_MEM file (e.g., `mmu_exception_handler_chmem.h`)
- **THEN** investigator MUST NOT read or modify that file; instead document in the per-line stop-and-document block that "CH_MEM counterpart is excluded per file scope"

### Requirement: Each investigation line MUST produce a stop-and-document block

For L4, L2, L3 (when investigated), the design.md MUST contain a stop-and-document block with this structure:

```
### <line_id> Stop-and-Document
- **结论**: [confirmed / excluded / partial]
- **证据**: <command + output (≤ 10 lines)>
- **信心度**: [HIGH / MEDIUM / LOW]
- **下一步**: [pivot to next line / skip to §5 综合]
```

#### Scenario: Inconclusive investigation result
- **WHEN** investigation cannot definitively confirm or exclude a line within reasonable effort (e.g., 1 hour for L2, 30 min for L3)
- **THEN** the stop-and-document block MUST be marked `inconclusive` (not forced into confirmed/excluded) and the next-line investigation MUST proceed

### Requirement: Temporary debug log MUST be reverted before archive

Any temporary `std::cerr` debug log added during investigation (control experiment flag, log point 1/2/3, etc.) MUST be reverted before the change is committed or archived.

#### Scenario: Adding --no-mmu temporary flag
- **WHEN** investigator adds `--no-mmu` to `tools/cpu_sim/main.cpp` for control experiment
- **THEN** the change MUST be applied via patch file or `git stash` (not committed to HEAD)
- **AND** the change MUST be reverted via `git checkout tools/cpu_sim/main.cpp` after the experiment concludes
- **AND** the revert MUST be verified by re-running `./build/bin/chipforge_tests [cpu]` and confirming baseline 128/129 (1 FAIL) is preserved

#### Scenario: Adding log points in framework files
- **WHEN** investigator adds `std::cerr` log to `ip/cpu/plugins/ibus.h` or `ip/cpu/plugins/stage_link.h`
- **THEN** the change MUST be reverted before the change is committed
- **AND** `git status --porcelain` MUST be empty for these files (verified before tasks.md §6.3)
- **AND** the architecture gate (`bash tools/verify_plugin_decision.sh` + `tools/check_plugin_portability.sh`) MUST pass 0 FAIL as a fallback verification

### Requirement: Final state MUST preserve 0-regression baseline

The change MUST preserve the 0-regression baseline after commit. Specifically:

- `[riscv-tests]` MUST be 48/48 PASS
- `[cpu]` MUST be 128/129 PASS (1 FAIL `cpu_sim_real_tohost` preserved — change does NOT fix it)
- `[cpu-integration]` MUST be 30/35 PASS (4 FAIL `*_add_elf_end_to_end` + 1 SIGTERM dhrystone preserved)
- `[cpu-l1-mmu-demo]` MUST be 6/6 PASS
- `[cpu-integration][mul-div-fsm-integration]` MUST be 4/4 PASS
- `[mmu]` MUST be 59/59 PASS
- `[mmu-verilator]` MUST be 3/3 PASS
- Architecture gate (33 + 8 + 12) MUST be 0 FAIL

#### Scenario: Temporary debug log not reverted
- **WHEN** a temporary debug log accidentally persists in a framework file
- **THEN** the architecture gate's static checks (specifically `check_plugin_portability.sh` for `ch_mem` symbol leakage in TLM files) MAY catch the regression
- **AND** the change MUST NOT be committed until the log is reverted

#### Scenario: --no-mmu flag accidentally committed
- **WHEN** `--no-mmu` CLI flag is accidentally committed in `tools/cpu_sim/main.cpp`
- **THEN** the change fails scope contract (no product code changes allowed)
- **AND** MUST be reverted before archive

### Requirement: Diagnosis MUST follow compound root-cause framework (P0 重做新增)

The diagnostic report (`design.md` §Why + §5) MUST present the root cause as a compound of **three layers** (F1+F2+F3) + **one validatable counter-factual** (F4), NOT as a single-factor "framework-level pipeline tick bug":

- **F1 — config 保真度缺陷**: `tools/cpu_sim/main.cpp:83-113` `load_config()` silently drops `enable_mmu / mmu_mode / satp_ppn / enable_pmp / branch_predictor / btb_entries / clock_freq_mhz / OoO 宽度 / ghr_bits / ext_*`. Static evidence (10 JSON fields parsed).
- **F2 — 测试 config 意图缺陷**: 4/5 FAIL test configs explicitly declare `enable_mmu: true` (`cpu_default.json:9` / `cpu_superscalar.json:16` / `cpu_deep_pipeline.json:18`; `cpu_embedded.json:9` declares false but F1 silently ignores), contradicting spec backing `openspec/specs/riscv-tests-fixture/spec.md:74-80` "Fixture SHALL use enable_mmu=false for Wave 1 M-mode bare-metal riscv-tests".
- **F3 — MMU 瞬态语义 + v0.10.4 Bare-shortcut 严格化放大**: `enable_mmu=true` + satp 未编程 → `cpu_factory.h:444` auto-encode Sv39 → `ip/mmu/tlm/MMUPlugin.cpp:109` Bare shortcut strict judgment (MODE≠Bare, no identity translation) → `do_lookup` 走真翻译 → TLB miss → `PTW_ACTIVE=1` → `ibus.h:62-67` fetch CtrlLink `halt_when` 持续 true → **fetch 永 stall** (decode/memory payload 全 0; writeback PC +4 不受影响).
- **F4 — 反向对照 (待验证)**: 同 ELF + `enable_mmu=true` in C++ API path. Acceptable outcomes: `{tohost=1, F4 confirmed + 三层 confirmed, L2 demoted}` / `{tohost=0 cycles exhausted, F4 confirmed}` / `{terminate with exception (PTW fault throw path), F4 confirmed as compound evidence per Oracle 风险记录}`. **MUST** be answered by §1 control experiment before §2 deep dive.

#### Scenario: Diagnostic report asserts single-factor root cause
- **WHEN** the diagnostic report's §5 asserts root cause is ONLY F1 (or only F2 / F3 / framework-level bug)
- **AND** does NOT present F4 counter-factual validation result
- **THEN** the change fails this requirement and MUST be revised before archive

#### Scenario: Diagnostic report uses "framework-level" / "pipeline can't tick" as root cause category
- **WHEN** the diagnostic report's §5 categorizes root cause as "framework-level" or describes "pipeline can't tick"
- **THEN** the change fails this requirement: C++ API path PASS 48/48 already proves `pb->run()` + payload propagation is reachable; the difference is in the input path side, NOT in framework. Per `proposal.md` ⚠️ Naming Caveat: real symptom is **fetch stage 永 stall**, pipeline tick is alive.

### Requirement: Fix recommendation MUST be D4-compliant (保持 `CPUConfig::enable_mmu` 默认值)

The `design.md` §5.3 fix recommendation section MUST NOT include "change `CPUConfig::enable_mmu` default to `false`" as a candidate. Rationale (D4 决策 2026-10-15):

- v0.10.4 hotfix (`a3afcfa`) 已锁定 Bare shortcut 严格判定 (与默认 enable_mmu=true 配套), 是 mmufault-handler v1 依赖的稳定语义
- `cpu_factory.h:444` `make_satp_value(Sv39, 0)` 自动编码与默认 true 配套
- 改默认会再次打破 `EnableMMU5StageBuilds` `node_count()==13` 与 `mmu-riscv-isa-adapter/spec.md:95` "MUST return exactly 12 plugins" 等既有 spec 契约 (历史: `cpu-mmu-integration` B2 / CHANGELOG:1303)
- `[mmu] 59/59` + `[cpu-l1-mmu-demo] 6/6` + `[mmu-verilator] 3/3` 全 PASS 依赖此默认

Allowed fix direction instead: (a) `load_config()` 解析 `enable_mmu` 等字段 + (b) 测试 config 改 `enable_mmu:false` (F2 配套) + (c) cpu_sim CLI 加 `--no-mmu` override flag. Coordinate with `mmu-chmem-pipeline-integration` owner for (a) config 传播路径.

#### Scenario: Fix recommendation section includes "改 `CPUConfig::enable_mmu` 默认 false" as candidate
- **WHEN** `design.md` §5.3 lists "改 `CPUConfig::enable_mmu` 默认 false" as a fix candidate
- **THEN** the change MUST NOT be archived until that candidate is removed/replaced with the D4-compliant version

### Requirement: Diagnostic report MUST recommend fix direction without producing patch

The `design.md` MUST contain a fix recommendation section (`§5.3 修复建议`) that:

- Lists ranked fix candidates with abstract description (NOT concrete patch)
- Each candidate includes: trigger condition, affected files (allowed scope only), risk assessment (LOW/MEDIUM/HIGH), coordination requirements with other in-flight changes
- The actual patch implementation is OUT OF SCOPE for this change and reserved for the follow-up `cpu-pipeline-tick-fix` change

#### Scenario: Fix recommendation for L4 confirmed (D4-compliant)
- **WHEN** L4 (config divergence) is confirmed as one root-cause layer
- **THEN** §5.3 MUST recommend at least: (a) `load_config()` 解析 `enable_mmu` 等被静默丢字段 (PRIMARY), (b) 4 个测试 config 改为 `enable_mmu:false` 配合 spec `riscv-tests-fixture` (PRIMARY), (c) cpu_sim CLI 加 `--no-mmu` override flag (可选)
- **AND** MUST NOT list "改 `CPUConfig::enable_mmu` 默认 false" as candidate
- **AND** MUST flag coordination requirement with `mmu-chmem-pipeline-integration` change owner (config 传播路径)

#### Scenario: Fix recommendation for L2 confirmed
- **WHEN** L2 is confirmed (fetch CtrlLink halt_when misfires + F4 反向验证 L2 升为主要)
- **THEN** §5.3 MUST recommend modifying `ibus.h:54-67` halt_when lambda to add Bare-mode guard, AND/OR `MmuExceptionHandler` payload propagation
- **AND** MUST flag MEDIUM risk: stall semantics consumed by `HazardPlugin` and `PTW` paths, Bare-mode byte-identical preservation required

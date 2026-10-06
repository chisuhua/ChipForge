# cpu-pipeline-mmufault-handler Specification

## Purpose
TBD - created by archiving change cpu-pipeline-mmufault-handler. Update Purpose after archive.
## Requirements
### Requirement: `MmuExceptionHandlerPlugin` 实装（TLM + CH_MEM 双模）

The plugin `cf::cpu::MmuExceptionHandlerPlugin<T>` SHALL be implemented in `ip/cpu/plugins/mmu_exception_handler.h` (TLM mode) and `ip/cpu/plugins/mmu_exception_handler_chmem.h` (CH_MEM mode), with strict file separation per ADR-040 v2.0 dual-mode constraint.

The TLM mode plugin SHALL:
- Listen on `mmu_keys::EXCEPTION_CODE` Payload Key in `at_stage("memory", Phase::LATE, ...)` closure
- When `EXCEPTION_CODE != 0`, write `cpu_keys::CPU_EXCEPTION_CODE` Payload Key
- Trigger `CtrlLink::flush_when(mmu_exception_pending)` per ADR-045
- Call `HazardPlugin::clear_mmufault()` to clear hazard retry loop

The CH_MEM mode plugin SHALL:
- Mirror TLM behavior using `ch::core::ch_bool` / `ch_reg` / `ch_state_machine` DSL (per ADR-046 v2.0 FSM exemption, only for trap PC jump state machine)
- Maintain trap PC jump state (IDLE → TRAP_PENDING → JUMP → IDLE)
- Write `ch_uint<32>` CPU_EXCEPTION_CODE value

#### Scenario: TLM 模式 page fault 处理
- **WHEN** `mmu_keys::EXCEPTION_CODE = 1` (page fault) is written at `memory` stage
- **THEN** `MmuExceptionHandlerPlugin` SHALL write `cpu_keys::CPU_EXCEPTION_CODE = 1` to writeback stage, AND trigger CtrlLink flush_when signal, AND call `HazardPlugin::clear_mmufault()` within the same cycle.

#### Scenario: CH_MEM 模式 trap PC jump
- **WHEN** `mmu_keys::EXCEPTION_CODE = 2` (access fault) is written at `memory` stage in CH_MEM mode
- **THEN** `MmuExceptionHandlerPlugin` ch_state_machine SHALL transition `IDLE → TRAP_PENDING → JUMP`, AND write `PC = 0x80000010` (hardcoded mtvec), AND clear `HazardPlugin::clear_mmufault()`.

#### Scenario: at_stage 闭包无状态机 (D4 验证)
- **WHEN** `bash tools/verify_plugin_decision.sh` runs against the new plugin source
- **THEN** the script SHALL NOT detect any `enum class State` or `switch(state_)` in `mmu_exception_handler.h` body — only `ch_state_machine` DSL in CH_MEM variant (ADR-046 v2.0 豁免范围)。

### Requirement: `cpu_factory.h:390` 修复 (satp_value 透传)

The hardcoded `/*satp_value=*/0` at `ip/cpu/cpu_factory.h:390` SHALL be replaced with `cpu_config.satp_value` (already passed through helpers per `cpu-factory-satp-mapping` change archive).

#### Scenario: ABI 兼容
- **WHEN** the 5 TLM factory callsites are rebuilt (`tools/cpu_sim`, `test_cpu_factory`, `test_3stage_riscv`, `test_5stage_riscv`, `test_10stage_riscv`)
- **THEN** the build SHALL succeed with 0 error 0 warning, and `cfg.mmu_mode="sv32"` SHALL now actually register `RiscvMMUPlugin` with sv32 satp_value (not inert 0 hardcode).

#### Scenario: sv32 mode 实装测试
- **WHEN** `cfg.mmu_mode="sv32"` AND `cpu_config.satp_value=(1ULL << 31)` (Sv32 MODE encoding)
- **THEN** `RiscvMMUPlugin::do_lookup` SHALL encode `satp_value_` correctly (per v0.10.4 hotfix ctor `ip/mmu/tlm/MMUPlugin.cpp:47`).

### Requirement: `cpu_factory_chmem.h::build_cpu` 联动注册

The function `cf::cpu::CpuFactoryChmem<T>::build_cpu` SHALL register `MmuExceptionHandlerPlugin<T>` when `enable_mmu.value_or(false) == true && mmu_mode.value_or("bare") != "bare"`.

#### Scenario: enable_mmu=false 不注册
- **WHEN** `build_cpu` is called with `enable_mmu = false` (explicit)
- **THEN** `MmuExceptionHandlerPlugin` SHALL NOT be registered, regardless of `mmu_mode` value.

#### Scenario: enable_mmu=true, mmu_mode=bare 不注册
- **WHEN** `build_cpu` is called with `enable_mmu = true` AND `mmu_mode = "bare"`
- **THEN** `MmuExceptionHandlerPlugin` SHALL NOT be registered (Bare mode has no exceptions).

#### Scenario: enable_mmu=true, mmu_mode=sv32 注册
- **WHEN** `build_cpu` is called with `enable_mmu = true` AND `mmu_mode = "sv32"` (or "sv39" / "sv48")
- **THEN** `MmuExceptionHandlerPlugin<T>` SHALL be registered to handle page faults and access faults.

### Requirement: `HazardPlugin::clear_mmufault()` 新 API

The class `cf::cpu::HazardPlugin<T>` SHALL expose a new public method `void clear_mmufault()` that resets the internal hazard retry counter associated with MMU faults.

#### Scenario: clear_mmufault() 重置 hazard
- **WHEN** `HazardPlugin::clear_mmufault()` is called AND the internal retry counter is currently non-zero
- **THEN** the internal retry counter SHALL be reset to 0, AND `has_hazard()` SHALL return `false` for the next cycle.

#### Scenario: 多次 clear 幂等
- **WHEN** `clear_mmufault()` is called multiple times consecutively
- **THEN** the result SHALL be idempotent — internal state SHALL remain at the cleared value.

### Requirement: TEST_CASE 6 `mmu_sv32_translation_verilator_e2e_flipped` (承接 Change 2a follow-up)

The file `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` SHALL be extended with TEST_CASE 6 `mmu_sv32_translation_verilator_e2e_flipped` under the `[mmu-verilator][e2e][sv32]` family tags. This TEST_CASE is the direct follow-up to Change 2a's Non-Goals §NG1.

#### Scenario: 真 sv32 translation 跑通
- **WHEN** TEST_CASE 6 runs with `cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf tests/cpu/manual_elf/sv32_pte.elf --cycles 5000`
- **THEN** the runner SHALL output `TOHOST=1 PASS` AND the cycle count SHALL be `≤ baseline_csv[sv32_pte][sv32_mmu] × 1.5` (放宽至 Change 2a × 1.2 的 1.25 倍, 因为 sv32 walk + trap PC 跳转开销更大)。

#### Scenario: trap handler PC 跳转 (0x80000010)
- **WHEN** the sv32 PTE program triggers a page fault (e.g., access unmapped region)
- **THEN** `MmuExceptionHandlerPlugin` SHALL jump PC to `0x80000010` (hardcoded mtvec per design.md §D3) within 2 cycles, AND the trap handler program SHALL complete `tohost=1` write.

#### Scenario: hazard retry 循环清理
- **WHEN** the page fault is triggered AND `HazardPlugin::clear_mmufault()` is called
- **THEN** the CPU pipeline SHALL NOT enter hazard retry loop (cycle count SHALL stay below 5000 max_cycles), AND subsequent instructions after trap return SHALL execute normally.

#### Scenario: skip-when-absent 一致性
- **WHEN** `CF_VERILATOR_SIM_BIN` file is not present at build time
- **THEN** TEST_CASE 6 SHALL `SUCCEED("verilator binary not built — skipping")` and return early.

### Requirement: sv32 PTE 程序 vendor `tests/cpu/manual_elf/build_sv32_pte.S`

The script `tests/cpu/manual_elf/build_manual_elf.sh` SHALL register a new template name `sv32_pte` that produces `tests/cpu/manual_elf/build_sv32_pte.S` and `tests/cpu/manual_elf/sv32_pte.elf`.

The assembly source SHALL:
- Set `satp` CSR to `(1ULL << 31) | 0x80050000` (Sv32 mode + PPN)
- Build L0 + L1 PTE tables in memory
- Write `tohost=1` to 0x80001000
- Trap handler at `0x80000010` (per design.md §D3 hardcode)

#### Scenario: sv32_pte vendor 产出
- **WHEN** user invokes `bash tests/cpu/manual_elf/build_manual_elf.sh sv32_pte`
- **THEN** `tests/cpu/manual_elf/build_sv32_pte.S` SHALL exist AND `tests/cpu/manual_elf/sv32_pte.elf` SHALL be a valid RV32 ELF that exercises one sv32 page walk.

#### Scenario: 现有模板 ABI 兼容
- **WHEN** user invokes any of the existing 10 templates (`add`, `and`, `div`, `mul`, `or`, `sll`, `srli`, `sub`, `mmu_bare`, `l1cache_basic`)
- **THEN** the corresponding `.S` and `.elf` artifacts SHALL be byte-identical to HEAD (no regression in existing manual_elf contents).

### Requirement: 零回归 + 架构门禁

The change SHALL NOT introduce any regression to existing test family counts:
- `[mmu-verilator]` 3/3 → 4/4 PASS (TEST_CASE 6 加入)
- `[mmu]` 53/53 PASS
- `[cpu-integration]` 81/81 PASS (硬不退化, CPU pipeline 改动影响 5+7+10 stage 全部)
- `[cpu]` 19/19 PASS
- `[cpu-l1-mmu-demo]` 6/6 PASS
- `[verilator]` 1/1 PASS

The 3 architecture gates SHALL all pass with exit code 0.

#### Scenario: `[mmu-verilator]` 3/3 → 4/4 PASS
- **WHEN** `bash tools/run_chipforge_tests.sh --tag "[mmu-verilator]"` runs (post archive)
- **THEN** the result SHALL be 6 cases passed, 0 failed.

#### Scenario: `[cpu-integration]` 81/81 不退化
- **WHEN** `bash tools/run_chipforge_tests.sh --tag "[cpu-integration]"` runs
- **THEN** the result SHALL be 81 cases passed, 0 failed (硬约束, 因本 change 改动 CPU pipeline, 影响所有 stage)。

#### Scenario: 3 架构门禁 0 失败
- **WHEN** the change is committed and pushed
- **THEN** GitHub Actions workflow `architecture-gates.yml` SHALL pass all 3 steps with exit code 0.

### Requirement: AGENTS.md + CHANGELOG 同步

The change SHALL update AGENTS.md "已知测试状态" segment and CHANGELOG.md v0.10.x segment WITH the new `[mmu-verilator]` 4/4 PASS count + cpu-pipeline-mmufault-handler 引用。

#### Scenario: AGENTS.md "[mmu-verilator]" 升级
- **WHEN** this change's archive step runs
- **THEN** AGENTS.md SHALL update the existing `[mmu-verilator]` line from `3/3 PASS` to `4/4 PASS` AND append a new line: `**[cpu-pipeline-mmufault-handler]** \`1 change\` (v0.10.x 真 sv32 Verilator e2e 翻转 owner; per design.md §承接 Change 2a follow-up)`。

#### Scenario: CHANGELOG v0.10.x 新增条目
- **WHEN** `CHANGELOG.md` is updated
- **THEN** a new entry SHALL be added under v0.10.x segment with title matching change name (`cpu-pipeline-mmufault-handler`) and body describing: (a) `MmuExceptionHandlerPlugin` 实装, (b) `cpu_factory.h:390` 修复, (c) HazardPlugin::clear_mmufault() 新 API, (d) TEST_CASE 6 真 sv32 e2e 翻转, (e) wave5 mfc-... Phase G Verilator hard gate 联动。

#### Scenario: honesty_audit 数字按 HEAD 实测
- **WHEN** `bash tools/v0100-bootstrap.sh review` runs against post-archive tree
- **THEN** the `§honesty_audit` segment SHALL reflect `[mmu-verilator] 4/4 PASS` AND the `[mmu] 53/53` / `[cpu-integration] 81/81` / `[cpu-l1-mmu-demo] 6/6` counts SHALL be byte-identical to v0.10.4 hotfix baseline (no silent drift).


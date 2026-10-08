# Spec Delta — mmufault-verilator-sv32-e2e-flip

> **Capability**: 测试层 (tests/mmu, tests/soc, tests/cpu/manual_elf)
> **Status**: PROPOSED (待 v1 archive + Phase 6d.6 实装后启动)
> **Change**: `mmufault-verilator-sv32-e2e-flip`

## Purpose

承接 `verilator-mmu-bare-plumbing-e2e` (Change 2a) 的 Non-Goals §NG1，**真 sv32 translation 端到端验证**（CH_MEM + Verilator 后端）。本 spec 定义 sv32 e2e 翻转测试的契约。

## ADDED Requirements

### Requirement: TEST_CASE 6 mmu_sv32_translation_verilator_e2e_flipped

The system SHALL 在 `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` 实装 TEST_CASE 6 `mmu_sv32_translation_verilator_e2e_flipped`，family tags `[mmu-verilator][e2e][sv32]`。

**body 实现要求**:
- popen `cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf tests/cpu/manual_elf/sv32_pte.elf --cycles 5000`
- REQUIRE TOHOST=1 (parse stdout)
- REQUIRE cycle ≤ sv32 baseline × 1.5 (baseline 来自 `test_mmu_bare_plumbing_verilator_baselines.csv` mode=sv32_mmu 行)
- REQUIRE trap PC 跳转断言 (via `cpu_keys::CPU_EXCEPTION_CODE` payload read)

**Where**: `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` (现有 3 TEST_CASE 后追加)

**Rationale**: 承接 Change 2a `verilator-mmu-bare-plumbing-e2e` §Non-Goals NG1 "不验证 sv32 translation 语义" — 本 spec 是该 NG 的 owner。

#### Scenario: sv32 translation success

- **WHEN** `cpu_verilator_sim --elf sv32_pte.elf --enable-mmu --mmu-mode sv32 --cycles 5000` 执行
- **AND** sv32_pte.elf 含 `csrw satp, ...` + L0/L1 PTE + trap handler
- **THEN** TEST_CASE 6 PASS
- **AND** trap handler @ 0x80000010 被 CPU_EXCEPTION_CODE 触发跳转
- **AND** tohost=1 输出

#### Scenario: sv32 translation failure (page fault)

- **WHEN** `cpu_verilator_sim --elf <page_fault_test>.elf --enable-mmu --mmu-mode sv32 --cycles 5000` 执行
- **AND** PTE 标记为 invalid
- **THEN** CPU_EXCEPTION_CODE = 12 (page fault)
- **AND** trap PC 跳转到 0x80000010
- **AND** CPU pipeline hazard retry 循环清理（cycle ≤ 5000）

### Requirement: TEST_CASE 7 cpu_l1_mmu_demo_sv32_translation_flipped

The system SHALL 在 `tests/soc/test_cpu_l1_mmu_demo.cpp` 实装 TEST_CASE 7 `cpu_l1_mmu_demo_sv32_translation_flipped`，family tags `[soc][cpu-l1-mmu-demo][sv32]`。

**body 实现要求**:
- flip `cfg.enable_mmu = true` (移除 v0.10.2 workaround)
- 用 `sv32_pte.elf` 替换 `rv32ui-p-add` ELF
- REQUIRE tohost=1 (CPU pipeline MMU exception handler 正常卸载 fault)

**Where**: `tests/soc/test_cpu_l1_mmu_demo.cpp` 现有 6 TEST_CASE 后追加

**Rationale**: `[cpu-l1-mmu-demo]` 6/6 PASS 当前依赖 `enable_mmu=false` workaround (per AGENTS.md line 40)；本 change archive 后移除 workaround，真 sv32 translation 翻转。

#### Scenario: 真 sv32 e2e 翻转

- **WHEN** `cfg.enable_mmu=true` 且 `cfg.mmu_mode="sv32"`
- **AND** ELF 是 `sv32_pte.elf` (csrw satp + L0/L1 PTE + trap handler)
- **THEN** TEST_CASE 7 PASS
- **AND** tohost=1 输出
- **AND** `[cpu-l1-mmu-demo]` family 6/6 → 7/7 PASS

### Requirement: sv32 baseline CSV

The system SHALL 在 `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv` 追加 5 行 mode=`sv32_mmu` baseline 数据 (rv32ui-p-add/addi/auipc/beq/jal × median_cycles)。

**Schema**: `elf_name,mode,median_cycles,verilator_version`（4 列, header 存在）

**Where**: `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv` 现有 5 行 bare_mmu 后追加

**Rationale**: TEST_CASE 6 cycle ≤ baseline × 1.5 断言需 sv32 baseline 数据；sv32_mmu cycle 应 ≥ bare_mmu cycle（PTW walk 开销）。

#### Scenario: sv32 baseline 数据生成

- **WHEN** Phase 6d.6 (`mmu_chmem.h`) 实装后
- **AND** `cpu_verilator_sim --enable-mmu --mmu-mode sv32` 真翻译可用
- **THEN** 跑 5 ELF × 5 runs × median cycle
- **AND** 追加 5 行 mode=`sv32_mmu` 到 baseline CSV
- **AND** 验证 median_cycles > bare_mmu 对应行（PTW walk 开销）

## Acceptance Scenarios

### Scenario: v2 change 端到端验证

- **WHEN** `openspec change validate mmufault-verilator-sv32-e2e-flip` 校验 0 error
- **AND** `[mmu-verilator] 5/5` + `[mmu] 53/53` + `[cpu-integration] 81/81` + `[cpu-l1-mmu-demo] 7/7` + `[cpu] 19/19` + `[verilator] 1/1` 0 regression
- **THEN** 本 spec archive, v2 链路完整

## Out of Scope

- TLM/CH_MEM MmuExceptionHandlerPlugin 实装 → 已在 v1 (`cpu-pipeline-mmufault-handler`) 覆盖
- HazardPlugin::clear_mmufault() API → 已在 v1 覆盖
- cpu_factory.h satp_value 修复 → 已由 v0.10.2 archive change `cpu-factory-satp-mapping` 覆盖
- MMU/PTW FSM 实装 → 由 Phase 6d.6 独立 change 覆盖
- wave5 mfc Phase G (DMIPS/MHz ≥1.4) 验证 → 由 mfc owner 覆盖

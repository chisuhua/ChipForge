# Spec Deltas — debug-cpu-l1-mmu-demo-deep-rca

> **范围**: 独立 deep RCA change，调查 [cpu-l1-mmu-demo] 5/6 FAIL 根因 (与父 hotfix debug-cpu-l1-mmu-demo-paddr-regression 不同根因)
> **父 hotfix**: debug-cpu-l1-mmu-demo-paddr-regression (v0.10.0 archive, 2026-09-28) 修 [cpu] + [cpu-integration] 5 回归
> **本 change 目标**: 修复 [cpu-l1-mmu-demo] 剩余 5/6 FAIL (add/addi/auipc/jal/beq 5 个 riscv-tests ELF 在 10000 cycle 内未写 tohost=1)

## ADDED Requirements

### Requirement: cpu-l1-mmu-demo 6/6 PASS

系统 SHALL 让 [cpu-l1-mmu-demo] 测试 family 在 main HEAD commit 上 6/6 通过 (test cases: 6 | 6 passed | 0 failed)。

#### Scenario: add ELF passes

- **WHEN** cpu_sim 加载 rv32ui-p-add ELF (`tests/cpu/riscv_tests/elf/rv32ui-p-add`) 并跑 ≤10000 cycles
- **THEN** PicolibcHostMemory.tohost_addr 写入 1, exited() = true, exit_code() = 0
- **AND** cycles 累计 ≤ 10000

#### Scenario: addi ELF passes

- **WHEN** cpu_sim 加载 rv32ui-p-addi ELF 并跑 ≤10000 cycles
- **THEN** exited() = true, exit_code() = 0
- **AND** cycles ≤ 10000

#### Scenario: auipc ELF passes

- **WHEN** cpu_sim 加载 rv32ui-p-auipc ELF 并跑 ≤10000 cycles
- **THEN** exited() = true, exit_code() = 0
- **AND** cycles ≤ 10000

#### Scenario: jal ELF passes

- **WHEN** cpu_sim 加载 rv32ui-p-jal ELF 并跑 ≤10000 cycles
- **THEN** exited() = true, exit_code() = 0
- **AND** cycles ≤ 10000

#### Scenario: beq ELF passes

- **WHEN** cpu_sim 加载 rv32ui-p-beq ELF 并跑 ≤10000 cycles
- **THEN** exited() = true, exit_code() = 0
- **AND** cycles ≤ 10000

#### Scenario: regression guard

- **WHEN** [cpu-l1-mmu-demo] 修复后跑全套 ctest
- **THEN** `[cpu]` ≥117/117 (debug-cpu-l0-mmu-demo-paddr-regression 已 archive), `[cpu-integration]` ≥81/81, `[mmu]` ≥53/53, `[riscv-tests]` ≥40/40 全部不退化
- **AND** verify_adr.sh + verify_plugin_decision.sh + check_plugin_portability.sh 0 FAILED (D4 + ADR-040 v2.0 + ADR-047 架构纪律)

### Requirement: deep RCA trace capability

系统 SHALL 提供 trace 能力以支持 deep RCA 调查 (与父 hotfix 共享 trace macros)。

#### Scenario: per-ELF trace capture

- **WHEN** 跑 [cpu-l1-mmu-demo] with TRACE=CpuDebug (trace per ELF)
- **THEN** 5 个 FAIL case (add/addi/auipc/jal/beq) 的 trace 分别输出到独立文件
- **AND** trace 文件 lifecycle 短 (不入版本控制, docs/notes/ 下保留)

#### Scenario: 4 嫌疑排除流程

- **WHEN** debug-cpu-l1-mmu-demo-deep-rca Phase B 跑
- **THEN** 4 嫌疑按概率排序独立验证 (A: kMaxCycles / B: ELF 加载顺序 / C: CPU mis-execute / D: PTE base_addr)
- **AND** 每个嫌疑 2 分钟内可验证/排除 (走 TDD 5 步)

### Requirement: openspec follow-up change 跟踪完整性

系统 SHALL 维持 [cpu-l1-mmu-demo] 6/6 PASS 状态的清晰跟踪，避免文档腐烂重演 (v0.7.0 → v0.8.0 快照引用问题)。

#### Scenario: CHANGELOG 实测数字

- **WHEN** [cpu-l1-mmu-demo] 修复后写 v0.10.1 或 v0.11.0 CHANGELOG entry
- **THEN** §Verification 段数字必须基于 HEAD commit 实测，禁止快照引用
- **AND** v0100-bootstrap.sh review ## honesty_audit 段声称 vs 实测 对账表全部 ✅

#### Scenario: v0100-bootstrap.sh hard_prerequisites 引用

- **WHEN** [cpu-l1-mmu-demo] 修复前
- **THEN** v0100-bootstrap.sh hard_prerequisites table 列 debug change 指向 `debug-cpu-l1-mmu-demo-deep-rca/`
- **AND** review_recommendations Phase A→F 步骤全文指向新 change

## REMOVED Requirements

_None — 本 change 是纯增量 (deep RCA investigation + 修复), 不删除任何已 archive change 的 requirement_

## RENAMED Requirements

_None_

## MODIFIED Requirements

_None — 不修改父 hotfix 或 v0.8.0 P1#3 已 archive requirements_
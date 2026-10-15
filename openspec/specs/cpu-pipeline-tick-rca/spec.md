## Purpose

Document the diagnostic methodology contract for the `cpu-pipeline-tick-rca` change (a pure investigation / no product code changes) of why 5 RISC-V simulation tests FAIL when calling `cpu_sim` CLI but PASS when calling C++ API directly.

## Requirements

### Requirement: P0 重做 + P2 实测应保留 5-测试独立基线台账 + 2×2 control experiment 矩阵 (D3 决策)

The diagnostic report MUST contain:
- §0.8 5-测试独立基线台账 (测试名 | config 路径 | 声明 enable_mmu | 实际生效 enable_mmu | 关键差异)
- §0.8.4 2×2 control experiment 数据 ([A] --no-mmu × 4 config + [B] default path × 5 tests)
- §0.8.7 FINAL Diagnosis with runtime evidence (write_word 闭包 + halt_when + fetch DBG)
- §0.8.9 后续 `cpu-pipeline-tick-fix` change scope (F1+F2+F3a+F3b D4-compliant 修复)

### Requirement: 根因叙事 MUST 是 F1+F2+F3 三层复合 (而非单因子 L4 或 framework-level bug)

Root cause MUST be presented as compound (3 layers), NOT as single-factor "framework-level pipeline tick bug":

- F1 — config 保真度缺陷 (load_config 静默丢 enable_mmu/mmu_mode/satp_ppn/branch_predictor/btb_entries 等)
- F2 — 测试 config 意图缺陷 (4/5 测试 config 显式 enable_mmu:true，与裸机烟测本意不符)
- F3 — MMU 瞬态语义 (enable_mmu=true + satp 未编程 + Bare-shortcut 严格化 + PTW 永不 inactivate → fetch 永 stall)

Diagnostic report MUST NOT categorize root cause as "framework-level" (C++ API 路径 48/48 PASS 已证 pb->run() 机制可达，差异在输入侧).

### Requirement: 修复建议 MUST 是 D4-compliant (保持 CPUConfig::enable_mmu 默认 true)

The diagnostic report's fix recommendation section MUST NOT list "改 CPUConfig::enable_mmu 默认 false" as a candidate. Rationale:

- v0.10.4 hotfix (`a3afcfa`) Bare shortcut 严格化 + `cpu_factory.h:444` Sv39 auto-encode 与默认 true 配套
- 改默认会再次打破 `[mmu] 59/59` / `[cpu-l1-mmu-demo] 6/6` / `[mmu-verilator] 3/3` 既有契约

Allowed fix direction: (a) load_config 解析 enable_mmu 等字段 + (b) 测试 config 改 enable_mmu:false + (c) cpu_sim CLI 加 --no-mmu override flag + (F3a) MMUPlugin.cpp:109 Bare shortcut 加 satp_mode_is_bare 守卫 + (F3b) ibus.h:62-66 halt_when 加 Bare-mode 双重守卫

### Requirement: Investigation MUST preserve 0-regression baseline

The change MUST preserve:
- `[riscv-tests]` 48/48 PASS (硬约束)
- `[cpu]` 128/129 PASS (1 FAIL cpu_sim_real_tohost 保持)
- `[cpu-integration]` 30/36 PASS (4 FAIL + 1 SIGTERM dhrystone + 1 其他保持)
- `[cpu-l1-mmu-demo]` 6/6 PASS
- `[cpu-integration][mul-div-fsm-integration]` 4/4 PASS
- `[mmu]` 59/59 PASS
- `[mmu-verilator]` 3/3 PASS
- 架构 gate (verify_adr + verify_plugin_decision + check_plugin_portability) 0 FAIL

### Requirement: Investigation MUST NOT 修改 product code

0 product code changes outside openspec/changes/<name>/ + .rddf/state/builder/<name>.json. 临时 debug cerr / patch 全 revert 由 git status --porcelain 兜底.

#### Scenario: Fix recommendation for L4/F3 confirmed (D4-compliant)

- WHEN §0.8.7 FINAL Diagnosis confirmed F3 chain
- THEN fix recommendation MUST list F1+F2+F3a+F3b (NOT 改默认 false)
- AND MUST flag coordination with mmu-chmem-pipeline-integration (config 传播路径)
- AND MUST flag `[mmu] 59/59` + `[cache] EndToEndTranslationThroughBridge` 强依赖

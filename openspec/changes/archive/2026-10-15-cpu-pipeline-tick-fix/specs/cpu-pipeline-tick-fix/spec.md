# cpu-pipeline-tick-fix Specification

## Purpose

Document the contract for cpu-pipeline-tick-fix: 4 minimal product code changes that flip 5 cpu_sim CLI tests from FAIL to PASS while preserving Bare-mode byte-identical behavior with v0.10.4.

## ADDED Requirements

### Requirement: cpu_sim CLI Bare-mode identity translation + stall behavior preserved (cpu-pipeline-tick-fix F1+F2+F3b)

#### Scenario: 5 cpu_sim CLI tests PASS (5 FAIL → 5 PASS)
- **GIVEN** `enable_mmu=false` in 3 cpu_*_test.json configs (cpu_default/superscalar/deep_pipeline + cpu_embedded stays at false)
- **WHEN** `cpu_sim --elf add.elf --config <config>.json` runs for 5 topologys (3/5/7/10-stage + cpu_embedded 3-stage)
- **THEN** `tohost=1` written within cycles<30 (vs baseline cycles=1000 stall)

#### Scenario: F1 load_config parses 8 previously-dropped JSON fields (cpu-pipeline-tick-fix F1)
- **GIVEN** JSON `{"params": {"enable_mmu": false, "mmu_mode": "sv39", "satp_ppn": 0, ...}}`
- **WHEN** `load_config(config_path)` runs
- **THEN** `cfg.enable_mmu = false`, `cfg.mmu_mode = "sv39"`, `cfg.satp_ppn = 0` 等 8 字段均从 JSON 正确消费 (vs baseline 全部回落到 struct 默认值)

#### Scenario: F3b ibus halt_when Bare-mode guard via SATP_MODE (cpu-pipeline-tick-fix F3b)
- **GIVEN** `enable_mmu=true` + satp CSR unwritten (satp_value MODE=0 即 Bare mode per RISC-V Spec §4.3.1)
- **WHEN** `fetch_ctrl->halt_when` lambda runs
- **THEN** returns `false` (Bare-mode identity translation 已就位, 无需 PTW stall; 避免 PTW_ACTIVE 永久 true 的 v0.10.4 `a3afcfa` strict Bare-shortcut 退化的"sv_mode=Sv39 + satp_ppn=0 误走真翻译 → PTW stall 永远"问题)

### Requirement: D4 sentinel — DO NOT change CPUConfig::enable_mmu default

The change MUST NOT modify `ip/cpu/cpu_factory.h` struct default `bool enable_mmu = true;` (kept per D4 决策 2026-10-15 user-locked). Modifying default would break:
- v0.10.4 hotfix `a3afcfa` Bare-shortcut strict judgment
- `cpu_factory.h:444` `make_satp_value(Sv39, 0)` Sv39 auto-encode
- `[mmu] 59/59` + `[mmu-verilator] 3/3` + `[cache] EndToEndTranslationThroughBridge` 既有一致性

#### Scenario: cpu_factory.h untouched
- **GIVEN** the change scope (F1+F2+F3a+F3b)
- **WHEN** inspecting the diff (`git diff HEAD~.. ip/cpu/cpu_factory.h`)
- **THEN** no lines changed (only the 5 product code files listed in the diff)

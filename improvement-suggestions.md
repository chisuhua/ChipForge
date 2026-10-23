# Improvement Suggestions Index

Pending improvements registered by `add-improve` HARD-GATE. After approval, move to `improvement-approved.md`.

| 提案 | 优先级 | 来源 | 添加时间 |
|------|--------|------|----------|
| [cpu-pipeline-tick-fix](.rddf/improvements/cpu-pipeline-tick-fix.md) | P1 | cpu-pipeline-tick-rca successor (archive 2026-10-15) | 2026-10-15 |
| [feat-dhrystone-dmips-gate](.rddf/improvements/feat-dhrystone-dmips-gate.md) | P0 | Dhrystone harness 修复 (H1+H2+H3 + B2 公式 + B3 -O2) — 真基线后推 BTB | 2026-10-16 |
| [chmem-mext-fsm-cycle-parity](.rddf/improvements/chmem-mext-fsm-cycle-parity.md) | P2 | chmem-mext-comb-datapath follow-up — CH_MEM M 扩展多周期 FSM + cycle parity + 可综合除法器 (blocked on ctrl-link-chmem-reg-handle) | 2026-10-20 |
| [ctrl-link-chmem-reg-handle](.rddf/improvements/ctrl-link-chmem-reg-handle.md) | P1 | chmem-mext-fsm-cycle-parity 的硬前置 — CtrlLink 支持有状态 (ch_reg<ch_bool>) halt/flush 条件 + 补 OR_combined_halt_conds() + 修 should_* 契约 | 2026-10-20 |
| [rv64-isa-declaration-correction](.rddf/improvements/rv64-isa-declaration-correction.md) | P0 | ADR-087 Change 1 — `isa` 三态语义 + `xlen` 字段 + `isa→xlen` 映射与编译期一致性断言 + 4 个 JSON/schema 纠偏 + 撤销 cpu_sim pin + 删 reg_file.cpp 死代码 | 2026-10-23 |
| [chmem-tlm-backend-freeze](.rddf/improvements/chmem-tlm-backend-freeze.md) | P0 | 用户指令「路线图只要 chmem 版本，移除不能生成 Verilog 的老版本」— B0 阶段：TLM 冻结门禁脚本 + 4 死代码清理 + ADR-088 起草 + ADR-083 v1.1 修订 | 2026-10-23 |
| [chmem-tlm-coverage-parity-ledger](.rddf/improvements/chmem-tlm-coverage-parity-ledger.md) | P1 | 回归可见性兜底（Oracle R2）— 451 个 TLM 独占 case 逐条三态台账 + 批量退役检测规则 + CI 软门禁 | 2026-10-23 |

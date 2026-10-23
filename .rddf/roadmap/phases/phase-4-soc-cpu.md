---
id: phase-4
kind: phase
status: active
primary_scope: soc-cpu
co_scopes: [ip-mmu]
phase_refs: []
主题: v0.10.x-v0.11.0 Wave 5 ISA 覆盖 + BP
---

# Phase 4: v0.10.x-v0.11.0 Wave 5 ISA 覆盖 + BP

**目标**: mfc FSM 收官 + mmu-chmem CH_MEM 集成 + rv32um 覆盖 + 分支预测 (BTB/GShare)

**状态**: 🔄 active (2026-10-08)

## 完成条件
- [ ] `mfc-cpu-pipeline-multi-cycle-fsm` Phase H archive (50/60 → 60/60)
- [ ] `mmu-chmem-pipeline-integration` archive (Phase A 已 commit a3afcfa, B-E 待)
- [ ] [riscv-tests] rv32um 8/8 PASS (当前 0/8, 真 radix-2 实装)
- [ ] [cpu-l1-mmu-demo] 7/7 PASS (workaround 移除)
- [ ] [mmu-verilator] 5/5 PASS (真 sv32 translation)
- [ ] DMIPS/MHz ≥ 1.4 (硬门禁)
- [ ] **PoC-1**: MUL/DIV 1c/33c rv32um 100% · **PoC-2**: RV32C ≥95% · **PoC-3**: ICache ≥90%
- [ ] ADR-082 negotiate capability 集成（mfc Phase G）
- [ ] **PoC-6**: CoreMark/MHz ≥ 1.9 (BP 完成) — **defer → v1.0.0** (决策 2026-10-19, 见 `objective-soc-cpu-v0110-launch.md §11`)
- [ ] **TLM 冻结门禁上线**（B0）: CI 可执行，TLM-only 文件新增 feature 函数即 fail — 见 `objective-ip-cpu-tlm-backend-retirement` Gate 1
- [ ] **TLM 死代码清理**（B0）: 4 文件 / 201 行删除，双 binary 0 退化
- [ ] **ADR-088 Proposed**（B0）: TLM 仿真后端退役与 CH_MEM 单后端收敛（冻结条款 + 语义裁决表 + 覆盖平价台账 + 删除判据）
- [ ] **覆盖平价台账建立**（B0）: 451 个 TLM 独占 case 三态标注 100% 覆盖，`[framework]` 94 条标「移植」
- [ ] **ISA 声明纠偏落地**（ADR-087 D1/D2）: `isa` 默认 `rv32im` + `xlen` 字段 + schema 三态 — 见 `feat-rv64-isa-declaration-integrity`

## 关联
- OpenSpec: `wave5-isa-coverage-and-bp` initiative
- 版本节点: v0.10.x → v0.11.0
- ADRs: ADR-046 (FSM 豁免) + ADR-082 (negotiate) + ADR-070 (RVC) + ADR-087 (ISA 声明真实性, Proposed)
- Active changes: `mmu-chmem-pipeline-integration` (6/166), `mfc-cpu-pipeline-multi-cycle-fsm` (50/60), `mfc-extract-fsm-h` (0/20)
- 相关 objective: `objective-soc-cpu-v0110-launch` · `objective-ip-cpu-tlm-backend-retirement` (B0 冻结段)
- 相关 feature: `feat-rv64-isa-declaration-integrity` · `feat-tlm-backend-retirement` (B0 段)

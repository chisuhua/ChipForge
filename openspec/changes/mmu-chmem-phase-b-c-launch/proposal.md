---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: ready
# Status 修订 (2026-10-18, F2 P0 入口):
#   从 placeholder -> ready. 由 build agent 2026-10-18 在 mmu-chmem-pipeline-integration
#   Phase 2 done 收档 + 跨 owner 协商预约 (events.jsonl evt_20261018_120000) 之后,
#   走 ADR-0049-auto-decision P0 入口 (complex case). Sprint 2-5 启动期 (2026-10-23 ~)
#   进入 P1 plan gen + P1.5 deps check.
depends_on:
  - mmu-chmem-pipeline-integration  # ✅ Phase 2 done 2026-10-17 (Phase A + C.2/C.3 sel 已落 commit 9373808+5b9f74a+1429f5a+8bb55a9+06bb1d3+992dbf5+c71c9e7)
  - csr-chmem-writeback-complete    # ✅ archived 2026-10-15 (R3.5 写通路完整, R11.1 重审入口就绪)
# Note: 其他 planned 前置 (非 openspec change, 仅作 prose, 不入 depends_on yaml)
#   - bp-btb-coordination-3items:   Sprint 2-5 协商项 (pc_reg mux / stall / fetch stage 协议)
#   - fetch-stall-protocol-v0.2:    Sprint 2-5 v0.2 起草 (Phase 1.1 requirements doc v0.1 → v0.2, 解 critic 5 BLOCKERS)
#   - CtrlLink API gap (FP-4):      hazard_chmem.h:152-154 admitted, Phase C.5 wiring 段物理前置
#   - pre-existing baseline 退化:   18 ELF FAIL (Phase 6d.7 owner) — 不阻塞本 change, 由 feat-phase-6d-rtl-verification 修
supersedes: []  # 空 — 而非 [mmu-chmem-pipeline-integration],原因 OpenSpec archive 校验历史 pattern 偏好 [], see archive/2026-10-13-chmem-verilator-rv32um-coverage/precedent. 语义上"接管" mmu-chmem-pipeline-integration Phase B-E 在 prose §What Changes 表显式标注.
related_to:
  - ADR-040 v2.0 (TLM↔CH_MEM 双模同语义)
  - ADR-046 (多周期协议引擎 FSM 豁免 — PtWalkFsmPlugin 复用入口)
  - ADR-049 (MMU paddr 消费契约)
  - ADR-083 (Plugin-style 单一 source of truth — mmu.h + mmu_chmem.h 双文件分离)
  - ADR-0049 (LLM auto-decision P0 入口, 4-option 决议)
  - objective-mmu-chmem-phase-b-e.md  (review_by=2027-01-06)
  - tasks.md mmu-chmem-pipeline-integration Phase B-E deferral comments
  - .rddf/protocols/fetch_stall_requirements_v0.11.0.md (5 BLOCKERS, v0.2 起草 baseline)
---

# mmu-chmem-phase-b-c-launch — mmu-chmem Phase B/E 真翻译 + C wiring 收官 (v0.11.0 Sprint 2-5 follow-up)

> **状态**: 🟡 占位 change (`status: placeholder`), `mmu-chmem-pipeline-integration` Phase 2 done 后, 接管 Sprint 2-5 (2026-10-23 ~ 2026-11-30, 5.5 周) 至 Sprint 6-7 (12-01 ~ 12-22, 3.0 周) 实施窗口。
>
> **战略位置**: v0.11.0 launch gate §8 Sprint 1 判据 #1 (`[riscv-tests] 48/48` rv32um 8/8 ✅ 已兑现 per mfc-extract-fsm-h archive 2026-10-10) + 判据 #2 (5 cpu_sim CLI PASS ✅ 已兑现 per cpu-pipeline-tick-fix `082b07a` archive 2026-10-16) 之外, **判据 #3** = `[cpu-l1-mmu-demo] 7/7 PASS` + `[mmu-verilator] 4/4 PASS` (现状 3/3 baseline + D.1 新增 TEST_CASE 4 sv32 e2e flip = 4/4) + Phase E archive = 本 change owner.
>
> **不在本 Sprint (10-17 ~ 10-22) scope**: 本占位 change 仅为 metadata anchor, 本周不动 code/test; `mmu-chmem-pipeline-integration` Phase A ✅ + C.2/C.3 ✅ 已在 main 落地 (commit `9373808`+`5b9f74a`+`c71c9e7`), 本 change 接管其 Phase B/C/E 未完成部分 (Phase 2 已 done 收档 2026-10-17).

## Why

### 触发 (2026-10-17 Sprint 17th 期)

`mmu-chmem-pipeline-integration` (2 子任务完成) 阶段性收丰:

- **已完成**: Phase A (根因 Bug 修复) + Phase C.2/C.3 (ibus/dmem sv32 消费 sel 表达式 + RED 单测) + Phase C.1/C.4/C.5 skeleton shipped (mmu_exception_handler_chmem + cpu_factory_chmem owner claim + hazard_chmem mmufault_pending state)
- **未完成**: Phase B (MMUPlugin_chmem + RiscvMMUPlugin_chmem 单级 ch_mem TLB + 复用 PtWalkFsmPlugin) + Phase C wiring (CPU_EXCEPTION_CODE 接线 + fprintf 删除 + halt_when 接线) + Phase C.6 (集成测试) + Phase D (sv32 PTE ELF + baseline CSV + 4 architecture gate) + Phase E (AGENTS.md + spec.md + CHANGELOG + archive)

### 不在主 path 上的原因

- **依赖 `csr-chmem-writeback-complete` archive (已 ✅ 2026-10-15)**: R3.5 写通路完整版 (7 M-mode CSR + mcause/mepc/mtvec trap 路径 + TRAP_PENDING/TRAP_TARGET payload) 就绪, 但 R11.1 (trap PC 跳转可行性重新评估) **必须在 Phase C 启动前完成**
- **依赖 `bp-btb-coordination-3items` (planned Sprint 2-5)**: fetch stage at_stage 注册顺序 + pc_reg next-mux 优先级 + BP 冻结期间不推测 3 项协议, **未启动**
- **依赖 `fetch-stall-protocol-v0.2` (planned Sprint 2-5)**: Phase 1.1 requirements doc v0.1 → v0.2 protocol,经 critic REJECT (5 BLOCKERS) 已撤回, **v0.2 起草在 Sprint 2-5 启动期必须先解决 5 BLOCKER**
- **依赖 CtrlLink `halt_when()` toggle API gap 解锁 (FP-4)**: `hazard_chmem.h:152-154` 显式承认, Phase C.1 + C.5 wiring 段物理不通

### 失败教训 (历史 context)

- `mmufault-verilator-sv32-e2e-flip` Part a/b 拆分假设 1-4h 可 ship 是 v0.10.0 window 的工程估算, 实测需 v0.11.0 中长期 Sprint 6+ 才能落地
- 决策 #2 "接受 Part a/b 拆分" 与 owner 真空 (`cpu_factory_chmem.h:80-85` 自证 "owner TBD") 在 v0.10.4 hotfix 暴露后已被 Oracle D4-A + D5-TLM 锁定, 但真 sv32 翻译跨 Sprint 6+ 仍需独立 change owner

## What Changes

### In scope (与 `mmu-chmem-pipeline-integration` scope diff)

| 范畴 | mmu-chmem-pipeline-integration (Phase A done) | mmu-chmem-phase-b-c-launch (Sprint 2-5) |
|------|---------|---------|
| **MMUPlugin CH_MEM 真翻译层** (单级 ch_mem TLB + PtWalkFsmPlugin 复用) | ❌ Phase B 标签, 未实装 | ✅ Sprint 2-3 (10-23 ~ 11-06, 2 周 = 10d) |
| **RiscvMMUPlugin_chmem 实装** (elaboration-time satp ch_reg 初值 + at_stage 闭包) | ❌ Phase B.2, 未实装 | ✅ Sprint 2 (与 MMUPlugin 同步) |
| **ibus_chmem sv32 PADDR_VALID=true 路径激活** | ✅ sel 表达式已落 (裸, 等 plugin 注册) | ✅ Sprint 4 (B 完成后) — wiring 实装 + CtrlLink 解锁后启用真翻译 |
| **dmem_chmem sv32 PADDR_VALID=true 路径激活** | ✅ sel 表达式已落 (裸) | ✅ Sprint 4 |
| **mmu_exception_handler_chmem CPU_EXCEPTION_CODE wiring** | ⚠️ skeleton shipped | ✅ Sprint 4 — 实现 `ch_bool mmufault_clear = (CPU_EXCEPTION_CODE != 0)` 组合逻辑 |
| **cpu_factory_chmem fprintf 删除 + enable_mmu=true MMUPluginChmem 注册** | ⚠️ owner 注释改, fprintf 仍存 | ✅ Sprint 4 — 真注册路径启用 |
| **hazard_chmem halt_when() wiring (CtrlLink API gap 解)** | ⚠️ skeleton (mark/clear/pending state) | ✅ Sprint 5 — 解 FP-4 + 实装 stall → halt_when toggle |
| **`test_cpu_chmem_mmu_pipeline` 集成测试** | ❌ C.6, 未做 | ✅ Sprint 5 (5d) — 真 sv32 PTE ELF + plant_identity_page_table + tohost=1 |
| **`test_mmu_bare_plumbing_verilator` TEST_CASE 4 (sv32 e2e flip)** | ❌ D.1, 未做 | ✅ Sprint 6 (3d) — Verilator 后端真 sv32 PTE 验证 |
| **`build_sv32_pte.S` vendor** | ❌ D.3, 未做 | ✅ Sprint 6 (1d) |
| **sv32-baseline-matrix.csv** | ❌ D.4, 未做 | ✅ Sprint 6 (2d) — 5 ELF × median cycle |
| **`test_cpu_l1_mmu_demo` TEST_CASE 7 真 sv32 翻转** | ❌ D.2, 未做 | ✅ Sprint 6 (3d) — 承接 mmufault Part a |
| **AGENTS.md workaround 移除 + spec.md 新增 Requirement + CHANGELOG v0.11.0 段 + archive** | ❌ Phase E | ✅ Sprint 7 (2d) — 文档 + archive 收官 |

### Out of scope (D1 锁定, 与原 change 一致)

- 多级 TLB / LRU/RRIP / ASID≠0 / Sv39/48 / 权限检查 / megapage 完整 PPN 拼接
- CSR plugin 已实装 (per csr-chmem-writeback-complete archive), trap PC 跳转可行性 R11.1 重审在 Phase C 启动期执行
- C.5 wiring 段的 CtrlLink toggle API (FP-4) 解锁在前置依赖中处理

## Execution Plan (高层)

```yaml
pre_flight:
  1. R11.1 重审 (trap PC 跳转可行性)  [0.5d, mfc-chmem owner]
  2. fetch-stall-protocol-v0.2 起草    [2d, 解 critic 5 BLOCKERS, deps prerequisite]
  3. bp-btb-coordination-3items 协商   [0.5d, deps prerequisite]
  4. CtrlLink API gap (FP-4) 解锁       [1d, Plugin framework owner]
Sprint_2 (10-23 ~ 11-06, 10d):
  B.2: RiscvMMUPlugin_chmem 实装        [3d]
  B.3: MMUPlugin_chmem 单级 TLB 实装    [5d]
  B.4: test_mmu_chmem_basic 单元测试    [2d]
Sprint_3 (11-06 ~ 11-30, 5d):
  B.5: Phase B 集成回归 (chipforge_tests_chmem 全套) [5d]
Sprint_4 (11-30 ~ 12-15, 5d):
  C.1: mmu_exception_handler_chmem wiring [2d]
  C.4: cpu_factory_chmem fprintf 删除 + MMUPluginChmem 注册 [2d]
  C.5: hazard_chmem halt_when() wiring   [1d]
Sprint_5 (12-15 ~ 12-22, 3d):
  C.6: test_cpu_chmem_mmu_pipeline 集成测试 [3d]
Sprint_6 (12-22 ~ 01-13, 5d):
  D.3: build_sv32_pte.S vendor           [1d]
  D.4: sv32-baseline-matrix.csv          [2d]
  D.1: test_mmu_bare_plumbing_verilator TEST_CASE 4 [3d] (与 sv32-baseline-matrix 并行)
  D.2: test_cpu_l1_mmu_demo TEST_CASE 7  [3d] (并行)
Sprint_7 (01-13 ~ 01-24, 2d):
  D.5: 4 architecture gate 0 失败         [0.5d]
  D.6: tools/run_chipforge_tests.sh --all [1.5d]
  E.1-E.6: docs + archive 收官           [1d, 与 D.5/D.6 并行]
```

### Coordinator Items (启动期与 mfc / wave5-bp-btb 协商)

1. **R11.1 重审** — mfc-chmem owner 启动 Phase C 时执行, 评估是否能在本 change 实装 trap PC 跳转 (ibus_chmem.h NORMAL select 覆写 PC 节点)
2. **fetch_stall protocol v0.2** — wave5-bp-btb owner 起草, 解 critic 5 BLOCKERS (B1-B5 在 fetch_stall_requirements_v0.11.0.md §B 段), 解锁 Phase C 启动硬前置
3. **bp-btb 3 项协调** — pc_reg next-mux / stall 语义 / fetch stage at_stage 顺序
4. **pre-existing baseline 退化修复 (Phase 6d.7)** — 由 feat-phase-6d-rtl-verification owner 跟进; 不在本 change scope

## Acceptance Criteria

- `[mmu] 59/59 PASS` 维持 + `[mmu][chmem] 3/3 PASS` (B.4 新增) + `[cpu-integration][mmu-chmem] 1/1 PASS` (C.6 新增) + `[cpu-l1-mmu-demo] 7/7 PASS` (D.2 新增) + `[mmu-verilator] 4/4 PASS` (D.1 新增 TEST_CASE 4)
- 4 architecture gate (verify_adr / verify_plugin_decision / check_plugin_portability / doc_link_check) 0 失败
- `bash tools/run_chipforge_tests.sh --all` 全 PASS (含 chmem baseline byte-identical)
- `openspec change validate mmu-chmem-phase-b-c-launch` 0 error
- `openspec archive mmu-chmem-phase-b-c-launch -y` + mmu-chmem-pipeline-integration 同步归档 (因 Phase 2 done, 由本 change 收官)

## References

- `openspec/changes/mmu-chmem-pipeline-integration/` — Phase 2 done 的 source of truth (proposal §1 + §11 + tasks.md 583 行)
- `openspec/changes/csr-chmem-writeback-complete/` — 已 archive, R3.5 写通路完整 (R11.1 重审入口)
- `.rddf/roadmap/objectives/objective-mmu-chmem-phase-b-e.md` (review_by=2027-01-06)
- `.rddf/roadmap/objectives/objective-v0110-launch.md` §6 Sprint Plan (Phase B Sprint 2-5, Phase C+D Sprint 6-7)
- `.rddf/protocols/fetch_stall_requirements_v0.11.0.md` (5 BLOCKERS, v0.2 起草 baseline)
- `docs/architecture/roadmap-evolution.md` §4-§4.5 (v0.11.0 架构演进)
- `docs/lessons/phase-6c-elaboration-substrate.md` (CH_MEM 模式 15 陷阱)

---

**文档版本**: v0.1 placeholder (Phase 2 done 后撰写)
**作者注释**: 本 change 由 build agent 2026-10-17 在 `mmu-chmem-pipeline-integration` Phase 2 收档期作为 follow-up placeholder 起草, 具体 Phase A/B/C/D/E tasks 见 tasks.md; 设计/实施细节将由 rdd-builder P1 plan gen + rdd-planner 在 Sprint 2-5 启动期填充.

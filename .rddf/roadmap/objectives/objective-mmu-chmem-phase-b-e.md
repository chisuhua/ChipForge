---
id: objective-mmu-chmem-phase-b-e
status: active
created: 2026-10-08
last_revised: 2026-10-08
review_by: 2027-01-06
owner: rdd-planner
priority: P0
manual_deps: []
supersedes: null
theme: mmu-chmem Phase B-E CH_MEM 集成实施
---

# Objective: objective-mmu-chmem-phase-b-e

## 1. 驱动诊断（Why now）

Phase A 根因修复已 commit (`a3afcfa`, 2026-10-08): Bare shortcut (D4-A) + shadow 双 bug (D5-TLM) 修复, [mmu] 59/59 PASS。但 **CH_MEM 集成层 owner 真空仍未关闭**:

- `cpu_factory_chmem.h:80-85` 自证 "Real sv32 CH_MEM owner TBD"
- `ibus_chmem.h` / `dmem_chmem.h` 缺失 sv32 消费
- `mmu_exception_handler_chmem.h` combinational network 未实装
- `hazard_chmem.h:151-153` mark/clear mmufault 未实装
- `mmu_chmem.h` (D1 收窄版) 未创建 — 单级 ch_mem TLB + 复用 PtWalkFsmPlugin

## 2. 目标愿景 + 完成判据

Phase B-E 全绿 (HANDOFF §10 退出标准):

| 判据 | 目标 |
|------|------|
| B1-B5 (B1 跳过) | 全 PASS, [chmem] 0 退化 |
| C1-C6 | 全 PASS, [cpu-integration] 81/81 + [cpu-l1-mmu-demo] 7/7 |
| D1-D6 | 全 PASS, [verilator] 1/1 + [mmu-verilator] 4/4 + [mmu] 59/59 + [cpu-l1-mmu-demo] 7/7 + [cpu] 19/19 |
| E1-E6 | 全部 commit |
| Architecture Gates | 4 个 0 error |
| `openspec archive mmu-chmem-pipeline-integration` | 成功 |

## 3. 架构依据

- ADR-040 v2.0: CH_MEM 是新正道 (`-DCF_PLUGIN_USE_CH_MEM` → `ch_uint<N>` + elaborate)
- ADR-083: Plugin-style 单一 source of truth (双文件 `mmu.h` + `mmu_chmem.h`)
- ADR-049: MMU paddr 消费契约
- ADR-046: 多周期 FSM 豁免 (PtWalkFsmPlugin 复用)
- HANDOFF §1 D1-D5 (Oracle 2026-10-07 锁定 5 项决议)

## 5. 反例

- 多级 TLB / LRU/RRIP / ASID≠0 / Sv39/48 / 权限检查 / megapage 完整 PPN 拼接 — **全部 out-of-scope** (D1)
- CSR plugin — CH_MEM 物理不可能 (Oracle R11), mmufault_clear 走最小 combinational

## 9. 目标依赖与 Decision Gate

### 9.1 前置 objective 依赖
- Phase A 已 commit (`a3afcfa`) — Bare + shadow 修复 [mmu] 59/59
- `mfc-extract-fsm-h` fetch stall framework 扩展 — Phase C (ibus_chmem.h) 硬前置
- `bp-btb` owner 3 项协调协议 — Phase C 入口 (HANDOFF §3)

### 9.2 Go / No-Go Decision Gate
- **Phase B Go**: [mmu] 59/59 维持 + A5 PPN 验证结论已记录
- **Phase C Go**: bp-btb 协调 3 项协议达成 (HANDOFF §3 备选: C2 推迟到协议达成)
- **Phase D Go**: B/C 全绿 + 4 architecture gates
- **No-Go 触发**:
  - bp-btb 协调未达成 → Phase C 阻塞
  - fetch stall framework 未 ready → ibus_chmem.h 无法消费 stall
  - CH_MEM elaborate 0 error 无法达成

## 10. next_sprint_candidates
- `phase-b-mmu-chmem-sv32` — MMUPlugin CH_MEM 实装 (10d)
- `phase-c-ibus-dmem-sv32-consume` — IBus/DBus sv32 消费 (11d)
- `bp-btb-coordination-3items` — fetch stage 协议 (0.5d, 先决)

## 11. 跟踪台账（append-only）
| Sprint | kind | 内容 | Decision/调整 | 原因 |
|--------|------|------|---------------|------|
| sprint-2026-10 | sprint-review | objective 创建 (Phase A 已 commit) | — | initial |
| sprint-2026-10-2 | deferral-rationale | **Phase B (B.2-B.5 mmu_chmem.h) 推迟到 Sprint 6+** | Defer Phase B (B.2-B.5) 到 Sprint 6 起头 (12-01) 或 Sprint 7 (12-23),与 Phase D/E 同步推进 | 3 次 deep agent 派发累计 1.5h+ 探查 CppHDL API (ch_uint / ch_reg / ch_mem / ch_state_machine / pipe_node) 均未产出代码 (`ip/mmu/plugins/mmu_chmem.h` 不存在);Oracle 2026-10-09 6 答案给全仍不够,需交互式 CppHDL API 探查 + build/test 反馈循环,超出 deep agent single-shot 30-min timeout 能力。Sprint 1-2 (10-09 ~ 11-30) 集中 Track A 真 radix-2 实装 (mfc-extract-fsm-h);Track B 推迟期不阻塞 Track A |


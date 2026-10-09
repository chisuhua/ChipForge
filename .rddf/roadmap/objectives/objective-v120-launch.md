---
id: objective-v120-launch
status: active
created: 2026-10-09
last_revised: 2026-10-09
review_by: 2028-04-30
owner: rdd-planner
priority: P0
manual_deps:
  - objective-v100-launch
  - objective-mmu-chmem-phase-b-e
supersedes: null
theme: v1.2.0 launch gate — 4-way RRIP + PMP + Debug + Spike lockstep 验证基建版
---

# Objective: objective-v120-launch

## 1. 驱动诊断（Why now）

v1.0.0 收官后进入验证基建版。v1.2.0 核心目标是用 Spike lockstep 对拍建立 1M 指令零分歧的强验证基础，4-way RRIP Cache 替换 LRU，PMP 内存保护实装，gdbstub 远程调试可用。Linux-sim 启动到 shell 作为 SOFT 交付（**R6 触发可降级**）。

## 2. 目标愿景 + 完成判据

v1.2.0 launch 全绿：

| 判据 | 当前（v1.0.0 收官后）| 目标（v1.2.0）|
|------|------|------|
| Cache 替换算法 | LRU 4-way | **4-way RRIP**（PoC-8 miss 率降 ≥30%）|
| 内存保护 | 无 | **PMP 8/16 regions** |
| 调试 | 无 | **gdbstub + Debug Spec 0.13**（PoC-10）|
| 对拍 | 无 | **Spike lockstep 1M 零分歧 HARD**（PoC-9）|
| Linux 启动 | 无 | **Linux-sim shell (SOFT, R6 可降级)** |
| CI 门禁数 | 11 | **11（lockstep HARD 门禁）** |

## 3. 架构依据

- ADR-080 试点（v1.2.0 软门禁，v1.3.0 转硬门禁）
- ADR-044: VIPT 安全已 v0.9.0 锁
- ADR-046: 多周期 FSM 豁免（沿用）
- 决策 1/2/3 锁定
- 风险决策树 R2/R6

## 5. 反例

- 非目标：v1.2.0 不做 Linux-on-FPGA HARD（推迟 v1.3.0）
- 非目标：v1.2.0 不做 RV32F（FPU 推迟 v1.3.0 内测，R7 触发可宣布 soft-float 路线）
- 非目标：v1.2.0 不做商业化（chip-selector 推迟 v1.3.0）

## 9. 目标依赖与 Decision Gate

### 9.1 前置 objective 依赖
- `objective-v100-launch` — v1.0.0 launch 全绿（S/U + BTB + FreeRTOS）
- `objective-mmu-chmem-phase-b-e` — MMU CH_MEM 完整（sv32 真翻译）

### 9.2 Go / No-Go Decision Gate
- **Go 条件**（全部满足）：
  - [ ] **PoC-9 HARD 门禁**：Spike lockstep 1M 条零分歧
  - [ ] **PoC-8**：4-way RRIP miss 率降 ≥30%
  - [ ] PMP 8/16 regions 实装 + 验证
  - [ ] gdbstub 远程调试可用（PoC-10）
  - [ ] Linux-sim shell (SOFT)
  - [ ] 4 architecture gates 0 error
- **No-Go 触发**（任一）：
  - **R2 PoC-9 对拍失败**（连续 4 周失败率 >5%）：对拍降级核心指令流（ALU/LSU/Branch），CH_MEM 独占特性放弃 TLM 覆盖
  - **R6 v1.2.0 Linux-sim 失败**（启动 6 个月内未到 shell）：降级 v1.2.0 为"验证基建版"，Linux-sim 并入 v1.3.0 合并交付
  - **R7 RV32F 不足**（2028 Q3 rv32uf 通过率 <50%）：宣布 soft-float 为 v1.x 正式路线，HW FPU 推迟 v2.0（≥2030）

## 10. next_sprint_candidates
- `cache-4way-rrip` — 4-way RRIP 替换算法实装（3-4 周，PoC-8）
- `pmp-8-16-regions` — PMP 内存保护实装（2-3 周）
- `gdbstub-debug-spec-013` — Debug Spec 0.13 远程调试（2-3 周，PoC-10）
- `spike-lockstep-1m-trace` — Spike lockstep 1M 条对拍（HARD 门禁，4-6 周，PoC-9）
- `linux-sim-shell-soft` — Linux-sim shell SOFT 交付（4-6 周，R6 可降级）

## 11. 跟踪台账（append-only）
| Sprint | kind | 内容 | Decision/调整 | 原因 |
|--------|------|------|---------------|------|
| sprint-2026-10 | sprint-review | objective 创建 | — | initial |

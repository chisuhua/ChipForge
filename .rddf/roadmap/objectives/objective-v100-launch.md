---
id: objective-v100-launch
status: active
created: 2026-10-09
last_revised: 2026-10-09
review_by: 2027-06-30
owner: rdd-planner
priority: P0
manual_deps:
  - objective-v0110-launch
  - feat-bp-btb-gshare
supersedes: null
theme: v1.0.0 launch gate — Wave 5 S/U + RV32A + BTB 收官 (PoC-4/5/6/7/14)
---

# Objective: objective-v100-launch

## 1. 驱动诊断（Why now）

Wave 5 收官后进入 Wave 6 linux-and-productization 阶段，v1.0.0 是产品化的首发版本。当前债务：

1. **S-mode/U-mode 缺失** — v0.10.0 仅有 M-mode，S/U trap 64 组合未验证（PoC-4 必需）
2. **RV32A 原子操作未实装** — LR/SC + AMO* 6 类全部 stub（PoC-5 必需）
3. **静态分支预测** — 无 BTB/GShare/RAS，CoreMark/MHz 受限（PoC-6 必需）
4. **中断控制器缺失** — PLIC + CLINT 完整实装待 v1.0.0（PoC-7 必需）
5. **HARD 门禁未达成** — FPGA CoreMark/MHz 与 VexiiRiscv single-issue 偏差 ≤10%（PoC-14 2026-09-29 新增）

## 2. 目标愿景 + 完成判据

v1.0.0 launch 全绿：

| 判据 | 当前（v0.10.0-v0.11.0 收官后）| 目标（v1.0.0）|
|------|------|------|
| [riscv-tests] | 48/48 | **48/48 + rv32ua 100%** |
| CoreMark/MHz | 1.9 (PoC-6 软门禁) | **≥ 2.3** |
| FMAX | — | **≥ 100 MHz** |
| 指令集 | RV32IMAC + RV32C | **+ RV32A** |
| Privilege | M-mode | **M+S+U 三层** |
| 中断 | 无 | **PLIC + CLINT** |
| OS 验证 | riscv-tests 100% | **FreeRTOS demo 10M cycle** |
| **HARD PoC-14** | — | **FPGA CoreMark vs VexiiRiscv ≤ 10%** |

## 3. 架构依据

- ADR-070~076: S/U mode + AMO + BTB + PLIC/CLINT + FreeRTOS（7 条新规划）
- ADR-082: negotiate capability（已 v0.10.0 实装）
- ADR-046: 多周期 FSM 豁免（mfc 沿用）
- ADR-040 v2.0: CH_MEM 是新正道（v0.10.0 已落地，v1.0.0 沿用）
- 决策 1/2/3 锁定（不抛弃 Plugin 范式 / ASIC 友好 / DSE-as-a-Service）

## 5. 反例

- 非目标：不做 ASIC 物理实现（v1.3.0 前纯 FPGA 验证）
- 非目标：不做 XiangShan OoO 性能对标
- 非目标：v1.0.0 不引入 dual-issue（推迟 v1.4 PoC-12）
- 非目标：v1.0.0 不做 ASID≠0 完整支持（推迟 v1.2.0/v1.3.0）

## 9. 目标依赖与 Decision Gate

### 9.1 前置 objective 依赖
- `objective-v0110-launch` — v0.11.0 launch 全绿（v0.10.0 收官 + mfc + mmu-chmem + BP）
- `feat-bp-btb-gshare` — BTB 4K + GShare 13-bit + RAS 8-entry（PoC-6 owner）

### 9.2 Go / No-Go Decision Gate
- **Go 条件**（全部满足）：
  - [ ] [riscv-tests] 48/48 + rv32ua 100%
  - [ ] CoreMark ≥ 2.3 + FMAX ≥ 100 MHz
  - [ ] **PoC-14（HARD 门禁）**：FPGA CoreMark/MHz 与 VexiiRiscv single-issue 偏差 ≤ 10%
  - [ ] FreeRTOS demo 10M cycle 稳定
  - [ ] 4 architecture gates 0 error
- **No-Go 触发**（任一）：
  - R1 BTB 不足（CoreMark 提升 < 10%）：停 GShare，启动 Learn 通路 RCA，bp_mode 锁 static
  - PoC-14 偏差 > 10%：触发 v1.0.0 推迟 + ASIC 路线降级

## 10. next_sprint_candidates
- `cpu-pipeline-smode-umode` — S/U mode 完整实装（6-8 周）
- `cpu-pipeline-amo-lrsc` — RV32A 原子操作（3-4 周）
- `cpu-pipeline-bp-btb-gshare` — 动态分支预测（4-6 周，feat-bp-btb-gshare 承接）
- `soc-freertos-demo` — PLIC/CLINT + FreeRTOS 集成（4-5 周）
- `vexii-riscv-parity-poc` — PoC-14 客观对拍（远期，v1.0.0 启动期）

## 11. 跟踪台账（append-only）
| Sprint | kind | 内容 | Decision/调整 | 原因 |
|--------|------|------|---------------|------|
| sprint-2026-10 | sprint-review | objective 创建 | — | initial |

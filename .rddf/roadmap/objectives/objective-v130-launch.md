---
id: objective-v130-launch
status: active
created: 2026-10-09
last_revised: 2026-10-09
review_by: 2029-03-31
owner: rdd-planner
priority: P0
manual_deps:
  - objective-v120-launch
supersedes: null
theme: v1.3.0 launch gate — Linux-on-FPGA + chip-selector 商业化 + ADR-080 HARD
---

# Objective: objective-v130-launch

## 1. 驱动诊断（Why now）

v1.3.0 是商业化首发版本：Linux-on-FPGA 端到端（HARD）+ chip-selector CLI（ADR-079 DSE-as-a-Service 商业化核心）+ ADR-080 TLM↔CH_MEM byte-equal 转硬门禁（v1.2.0 软门禁升级）。R3/R7/R8 风险决策树在此版本收口。

## 2. 目标愿景 + 完成判据

v1.3.0 launch 全绿：

| 判据 | 当前（v1.2.0 收官后）| 目标（v1.3.0）|
|------|------|------|
| chip-selector CLI | 无 | **8 配置 TLM 扫描 < 10 min**（PoC-11）|
| TLM↔CH_MEM IPC 偏差 | — | **≤ 15%**（R3 触发条件，>15% 触发决策 3 回退）|
| **ADR-080 HARD 门禁** | 软门禁（v1.2.0）| **TLM↔CH_MEM byte-equal 硬门禁** |
| FPGA 交付 | Linux-sim SOFT | **Linux-on-FPGA HARD** |
| 浮点 | 无 | **RV32F soft-float（内测, R7 触发路线降级）** |
| 商业化 | 无 | **DSE-as-a-Service**（决策 3）|
| FPGA CoreMark | — | **≥ 2.5** |
| CI 门禁数 | 11 | **11（+ ADR-080 byte-equal HARD）** |
| **PoC-14 复测（HARD）** | — | **FPGA CoreMark/MHz vs VexiiRiscv ≤ 10%** |

## 3. 架构依据

- ADR-079: chip-selector 契约（v1.3.0 商业化 SSOT）
- ADR-080: TLM↔CH_MEM 双模对拍硬门禁（v1.2.0 软 → v1.3.0 硬）
- 决策 1/2/3 锁定（不抛弃 Plugin / ASIC 友好 / DSE-aaS）
- 风险决策树 R3/R7/R8

## 5. 反例

- 非目标：v1.3.0 不做 ASIC 物理实现（决策 2：FPGA 用于验证，ASIC 物理 ≥ v2.0）
- 非目标：v1.3.0 不做 dual-issue（推迟 v1.4 PoC-12/15）
- 非目标：v1.3.0 不做商业化生态扩展（推迟 v1.4+）

## 9. 目标依赖与 Decision Gate

### 9.1 前置 objective 依赖
- `objective-v120-launch` — v1.2.0 launch 全绿（lockstep HARD + 4-way RRIP + PMP + gdbstub + Linux-sim SOFT）

### 9.2 Go / No-Go Decision Gate
- **Go 条件**（全部满足）：
  - [ ] **PoC-11**: chip-selector 8 配置 TLM < 10 min + IPC 偏差 ≤ 15%
  - [ ] **ADR-080 HARD 门禁**: chip-selector 产出必须 TLM ↔ CH_MEM byte-equal
  - [ ] Linux-on-FPGA 端到端（OpenSBI → Linux → Shell 实际跑在 FPGA）
  - [ ] FPGA CoreMark ≥ 2.5
  - [ ] **PoC-14 复测（HARD）**: FPGA CoreMark/MHz vs VexiiRiscv single-issue 偏差 ≤ 10%
  - [ ] 4 architecture gates 0 error
- **No-Go 触发**（任一）：
  - **R3 PoC-11 偏差**（TLM vs CH_MEM IPC > 15%）：chip-selector 砍性能预测，只保 RTL 生成；商业化定位从 DSE-as-a-Service 退到 RTL-as-a-Service（**决策 3 回退条件**）
  - **R7 RV32F 不足**（2028 Q3 rv32uf 通过率 <50%）：宣布 soft-float 为 v1.x 正式路线，HW FPU 推迟 v2.0（≥2030），CHANGELOG 标注
  - **R8 Shuttle 错过**（2029 年无 ASIC Shuttle 成本窗口）：降级为"FPGA 产品化"路线，chip-selector 转向 FPGA 向导（commercial license → FPGA board bundle）

## 10. next_sprint_candidates
- `chip-selector-cli` — 商业化 CLI 工具实装（6-8 周，PoC-11 + ADR-079）
- `linux-on-fpga-end-to-end` — FPGA 板级 Linux 启动（8-10 周，R6 不可降级）
- `adr-080-tlm-chmem-byte-equal` — TLM↔CH_MEM 双模对拍硬门禁（4-6 周）
- `vexii-riscv-parity-fpga` — FPGA CoreMark 客观对拍（远期）

## 11. 跟踪台账（append-only）
| Sprint | kind | 内容 | Decision/调整 | 原因 |
|--------|------|------|---------------|------|
| sprint-2026-10 | sprint-review | objective 创建 | — | initial |

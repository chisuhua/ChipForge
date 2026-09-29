# RISC-V CPU SoC 实施路线图

> **属主**：`soc/cpu/` — RISC-V CPU 中心 SoC（CPU + MMU + Cache + Memory + Interconnect + Peripheral）
> **最后更新**：2026-09-29（execution-roadmap.md §3.2/§3.3 新增 VexiiRiscv 客观对拍 HARD 门禁列 + PoC-14/15；与 §3.1 timeline + §3.2 deliverables + §3.3 14 PoC 完整对齐；新增 `openspec/changes/vexii-riscv-parity-poc/` 4 artifacts）

## 📌 这个目录解决什么问题

> **新会话第一件事**: 读完本节就能判断"你要找的内容在哪个文件"

| 你想知道什么 | 读哪个文件 |
|--------------|----------|
| 当前 SoC 路线图（v0.10.0 → v1.3.0）整体目标 | [`execution-roadmap.md §1`](./execution-roadmap.md) |
| 4 个版本节点（v0.10.0 / v1.0.0 / v1.2.0 / v1.3.0）什么时候做什么 | [`execution-roadmap.md §3`](./execution-roadmap.md) |
| **v0.10.0 → v1.3.0 每个版本的架构图 + 关键产出** | [`execution-roadmap.md §3.5`](./execution-roadmap.md) |
| 14 个 PoC 速查（哪个版本验收什么） | [`execution-roadmap.md §3.3`](./execution-roadmap.md) |
| 8 个风险 + 砍分叉条件 | [`execution-roadmap.md §3.4`](./execution-roadmap.md) |
| 3 大决策（范式 / ASIC / DSE-aaS）的详细论证 | [`references/decision-1-plugin-evolution.md`](./references/decision-1-plugin-evolution.md) |
| 18 条 ADR 矩阵 + 依赖图 | [`references/adr-matrix.md`](./references/adr-matrix.md) |
| 14 PoC 完整规范 + ADR 锚点 + 失败砍分叉（含 PoC-14/15 VexiiRiscv 客观对拍） | [`references/poics-and-risks.md`](./references/poics-and-risks.md) |
| 与 VexRiscv/VexiiRiscv/XiangShan 对比 | [`references/multi-core-comparison.md`](./references/multi-core-comparison.md) |
| 历史 phase 文档（v0.1.x ~ v0.8.0 完成的实施记录） | `archive/`（仅溯源，不执行） |

## 目录结构

```
soc/cpu/docs/roadmap/
├── README.md                          ← 本文件（索引 + 导航表）
├── execution-roadmap.md               ← 主控路线图（v0.10.0 → v1.3.0）
├── references/                        ← 深度内容（按需查阅）
│   ├── decision-1-plugin-evolution.md
│   ├── adr-matrix.md
│   ├── multi-core-comparison.md
│   └── poics-and-risks.md
└── archive/                           ← 历史 phase 文档（仅溯源，不执行）
    ├── phase-1-tlm-foundation.md
    ├── phase-1.5-stall-and-validate.md
    ├── phase-2-baremetal.md
    ├── phase-3-rtos.md
    ├── phase-4-linux.md
    └── phase-5-rtl.md
```

## 主控文档

### **[📍 execution-roadmap.md — ChipForge 超越路径规划 v0.10.0 → v1.3.0](./execution-roadmap.md)**

骨架结构（2026-09-28 更新）：

| § | 内容 | 何时读 |
|---|------|--------|
| 1 | 目标（技术 / 差异化 / 超越 / 非目标）| 战略汇报 / 项目立项 |
| 2 | 3 大决策锁定（范式不动 / ASIC 友好 / DSE-as-a-Service）| 任何 ADR / PoC 决策时 |
| 3 | 实施路径：4 版本节点 timeline + 产出 + 14 PoC + 8 风险 | 版本节点规划时 |
| **3.5** | **架构演进与架构图（v0.10.0 → v1.3.0 四版本节点 ASCII 图）** | **实施具体阶段时（每次实施前必读 §3.5 对应版本）** |
| 4 | 验收与同步机制（CI 门禁 + OpenSpec 同步 + 文档同步规则）| 任何 change archive 前 |
| 5 | v0.10.0 启动状态（高挥发，每周校准）| 每周一例会 |
| 6 | 参考文档索引（references/）| 见下表 |
| 7 | 历史归档索引（archive/）| 仅溯源 |
| 附录 | 术语表 | 任何时候 |

**§3.5 架构演进章节（2026-09-28 新增）**:

| 子章节 | 内容 |
|--------|------|
| §3.5.1 | 4 版本节点时间线 + 关键架构能力栈 |
| §3.5.2 | v0.10.0 架构目标 + ADR-082 negotiate + MUL/DIV FSM + RV32C + ICache |
| §3.5.3 | v1.0.0 架构目标 + S/U mode + AMO + BTB/GShare/RAS + PLIC/CLINT |
| §3.5.4 | v1.2.0 架构目标 + 4-way RRIP + PMP + gdbstub + Spike lockstep |
| §3.5.5 | v1.3.0 架构目标 + Linux-on-FPGA + chip-selector CLI |
| §3.5.6 | 4 版本节点架构演进对比表（13 个维度） |
| §3.5.7 | 与现有文档的衔接（ADR + 章节锚点） |

## 参考文档（references/）

| 文档 | 内容 | 何时读 |
|------|------|--------|
| [`references/decision-1-plugin-evolution.md`](./references/decision-1-plugin-evolution.md) | VexiiRiscv 5 病灶 × ChipForge 机制级回应（5 条 × 4 维 + 客观回退信号）| 起草任何 Plugin 范式相关 ADR 时 |
| [`references/adr-matrix.md`](./references/adr-matrix.md) | 18 条 ADR + 2 CI 门禁整合 + 依赖图 + 门禁分层决策树 | 任何 ADR 起效/降级/回退时 |
| [`references/multi-core-comparison.md`](./references/multi-core-comparison.md) | VexRiscv / VexiiRiscv / XiangShan / ChipForge 47 行 × 4 列对比 + 8 个 🚀 独占维度（2026-09-29 新增 ADR-080 双模对拍协议） | 对外汇报 / 论文 / 商业化展示 |
| [`references/poics-and-risks.md`](./references/poics-and-risks.md) | 14 PoC 完整规范（含 ADR 锚点 + 失败砍分叉）+ 8 风险反向决策树 | 任何 PoC 启动/失败决策时 |

## 历史归档（archive/）

6 个历史 phase 文档归档保留（commit 链路 + 子任务清单溯源），不再作为执行依据。

| 归档文档 | 当时状态 | 当前覆盖 |
|---------|---------|---------|
| `archive/phase-1-tlm-foundation.md` | ✅ v0.1.x 完成 | execution-roadmap.md §3.2 v0.10.0 |
| `archive/phase-1.5-stall-and-validate.md` | ✅ v0.4.1–v0.8.0 完成 | execution-roadmap.md §3.2 v0.10.0 |
| `archive/phase-2-baremetal.md` | ⏳ 被覆盖 | execution-roadmap.md §3.2 v0.10.0 / v1.0.0 |
| `archive/phase-3-rtos.md` | ⏳ 被覆盖 | execution-roadmap.md §3.2 v1.0.0 PoC-7 |
| `archive/phase-4-linux.md` | ⏳ 被覆盖 | execution-roadmap.md §3.2 v1.2.0 / v1.3.0 |
| `archive/phase-5-rtl.md` | ⏳ 被覆盖 | execution-roadmap.md §3.2 v1.2.0 PoC-9 + ADR-060 |

## 相关文档

| 文档 | 内容 |
|------|------|
| [`./execution-roadmap.md`](./execution-roadmap.md) | **主控路线图（v0.10.0 → v1.3.0）** |
| [`../architecture.md`](../architecture.md) | SoC 系统架构（数据流、IP 集成状态） |
| [`../../../../ip/mmu/docs/architecture.md`](../../../../ip/mmu/docs/architecture.md) | MMU 内部微架构 |
| [`../../../../docs/architecture/plugin-framework.md`](../../../../docs/architecture/plugin-framework.md) | Plugin 框架设计 |
| [`../../../../docs/roadmap/README.md`](../../../../docs/roadmap/README.md) | 全局路线图入口（含 Phase 0/6） |
| [`../../../../docs/roadmap/strategy/a-plus-c-hybrid.md`](../../../../docs/roadmap/strategy/a-plus-c-hybrid.md) | 战略入口（A+C Hybrid） |
| [`../../../../docs/roadmap/strategy/execution-roadmap.md`](../../../../docs/roadmap/strategy/execution-roadmap.md) | 框架级执行（Phase 6d → v0.8.0 → v0.9.0）—— **与本文件互补，不是替代** |
| [`../../../../docs/architecture/adr.md`](../../../../docs/architecture/adr.md) | ADR 注册表 |

## 🎯 与框架级 execution-roadmap 的区别（重要）

> **新会话易混淆点**: 项目里有 **两个** `execution-roadmap.md`

| 维度 | `docs/roadmap/strategy/execution-roadmap.md` | `soc/cpu/docs/roadmap/execution-roadmap.md`（本目录） |
|------|------------------------------------------|---------------------------------------------------|
| **范围** | 框架级 v0.8.0（Phase 6d → v0.9.0）| SoC 级 v0.10.0 → v1.3.0 |
| **关注** | 框架能力完整化 + 清债 | CPU SoC 产品化 + 商业化 |
| **决策依据** | ADR-040/046/047/048/049（技术债清理） | ADR-070~083（产品化）+ 3 大决策锁定 |
| **更新频率** | 周级别（每 archive change 同步） | 季度级别（版本节点驱动） |
| **Owner** | ChipForge Build Team | soc/cpu/ SoC 团队 |

**记忆口诀**:
- "**战略** execution-roadmap" = 看 `docs/roadmap/strategy/`
- "**SoC** execution-roadmap" = 看 `soc/cpu/docs/roadmap/`
- 两者通过 `§6.6` / `§3.5.7` 衔接（2026-09-28 新增）

**详细文档结构变更历史**: 见 [`../../../../docs/MIGRATION_LOG.md`](../../../../docs/MIGRATION_LOG.md)（2026-09-28 新增）
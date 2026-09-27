# RISC-V CPU SoC 实施路线图

> **属主**：`soc/cpu/` — RISC-V CPU 中心 SoC（CPU + MMU + Cache + Memory + Interconnect + Peripheral）
> **最后更新**：2026-09-27（ADR 编号 050~062 平移到 070~082，避开 active change 撞号；3 决策锁定，12 PoC 已编号；v0.10.0 启动就绪）

## 目录结构

```
soc/cpu/docs/roadmap/
├── README.md                          ← 本文件（索引）
├── execution-roadmap.md               ← 主控路线图（最小骨架：目标/产出/路径）
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

骨架结构：

| § | 内容 |
|---|------|
| 1 | 目标（技术 / 差异化 / 超越 / 非目标）|
| 2 | 3 大决策锁定（范式不动 / ASIC 友好 / DSE-as-a-Service）|
| 3 | 实施路径：4 版本节点 timeline + 产出 + 12 PoC 速查 + 8 风险速查 |
| 4 | 验收与同步机制（CI 门禁 + OpenSpec 同步 + 文档同步规则）|
| 5 | 立即可执行的下一步（v0.10.0 启动第一周动作清单）|
| 6 | 参考文档索引（references/）|
| 7 | 历史归档索引（archive/）|
| 附录 | 术语表 |

## 参考文档（references/）

| 文档 | 内容 | 何时读 |
|------|------|--------|
| [`references/decision-1-plugin-evolution.md`](./references/decision-1-plugin-evolution.md) | VexiiRiscv 5 病灶 × ChipForge 机制级回应（5 条 × 4 维 + 客观回退信号）| 起草任何 Plugin 范式相关 ADR 时 |
| [`references/adr-matrix.md`](./references/adr-matrix.md) | 18 条 ADR + 2 CI 门禁整合 + 依赖图 + 门禁分层决策树 | 任何 ADR 起效/降级/回退时 |
| [`references/multi-core-comparison.md`](./references/multi-core-comparison.md) | VexRiscv / VexiiRiscv / XiangShan / ChipForge 47 行 × 4 列对比 + 7 个 🚀 独占维度 | 对外汇报 / 论文 / 商业化展示 |
| [`references/poics-and-risks.md`](./references/poics-and-risks.md) | 12 PoC 完整规范（含 ADR 锚点 + 失败砍分叉）+ 8 风险反向决策树 | 任何 PoC 启动/失败决策时 |

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
| [`../../../../docs/architecture/adr.md`](../../../../docs/architecture/adr.md) | ADR 注册表 |
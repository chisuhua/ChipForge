# CPU IP 文档索引

## 当前架构设计（权威文档）

| 文档 | 说明 | 状态 |
|------|------|------|
| [multi_isa_architecture.md](multi_isa_architecture.md) | 多 ISA 架构设计：三层分离（core/arch/tlm+rtl）、ExecContext 接口、Plugin 分类体系 | **Active** |
| [dse_architecture.md](dse_architecture.md) | **DSE 实现方案 (v1.0, 2026-06-17)** — 流水线深度/分支预测/MUL 延迟/ISA 扩展等可探索维度的完整代码改动路径, 含实证校核 (哪些是真 stub / 哪些是文档超前) | 🟡 Accepted (待实施 M4-DSE / M5-DSE) |

## 实施文档（v2.0 拆分后，2026-06-16；DSE 子阶段 2026-06-17）

| 文档 | 作用 | 读者 | 改动频率 |
|------|------|------|----------|
| [cpu_implementation_guide_v2.0.md](cpu_implementation_guide_v2.0.md) | 决策入口 — F1-F6 决议 + 议题 1-8 完整选择 | 决策审计 | **只读** (决议不变) |
| [blueprint.md](blueprint.md) | 静态架构蓝图 — 微架构/目录/Plugin 套件/CpuFactory/cf_plugin 扩展点/方法学/VexRiscv 关系 | 架构师 | 极少 (重大设计变更时) |
| [status.md](status.md) | 任务状态看板 — M1-M5 49 个子任务 + **M4-DSE / M5-DSE 子任务** PASS/FAIL/进度% | 项目管理者 | **高频** (每次 Mx.y 完成即更新) |
| [implementation-plan/README.md](implementation-plan/README.md) | 总体实施规划 — 范围/议题 1-8 实施层细节/测试金字塔/风险/M1-M5 总览 | 实施者 | 中 (范围/风险调整时) |
| [implementation-plan/M1-cpu-skeleton.md](implementation-plan/M1-cpu-skeleton.md) | M1 详细任务清单 (8 个子任务) | 实施者 | 任务粒度变化时 |
| [implementation-plan/M2-core-plugins.md](implementation-plan/M2-core-plugins.md) | M2 详细任务清单 (9 个子任务) | 实施者 | 任务粒度变化时 |
| [implementation-plan/M3-riscv-plugins.md](implementation-plan/M3-riscv-plugins.md) | M3 详细任务清单 (12 个子任务) | 实施者 | 任务粒度变化时 |
| [implementation-plan/M4-integration.md](implementation-plan/M4-integration.md) | M4 详细任务清单 (**19 个子任务**, 含 M4.12-M4.19 DSE 接线) | 实施者 | 任务粒度变化时 |
| [implementation-plan/M5-verification.md](implementation-plan/M5-verification.md) | M5 详细任务清单 (**19 个子任务**, 含 M5.10-M5.19 DSE 验证) | 实施者 | 任务粒度变化时 |

> **拆分说明**: 原 `cpu_implementation_guide_v2.0.md` 1220 行混杂 4 类内容, 已拆分为 4 类文档。 详细迁移表见 v2.0 §3。

## 历史参考文档

以下文档记录了早期设计探索过程，保留用于理解架构演进。**实现时请以 multi_isa_architecture.md 为准。**


## 架构演进路线

```
VexRiscvArch.md (回调模型, CpuPlugin)
    │
    ▼ 引入 CppTLM 的 SimObject/EventQueue/ch_stream
VexRiscvOnCppTLM.md (tick 模型, PipelinePlugin + Stageable<T>)
    │
    ▼ 引入多 ISA 需求，抽象 ExecContext 接口
multi_isa_architecture.md (tick + ExecContext 委托, 三层分离)
    │
    ▼ 引入 Plugin-style 范式 + ISA/CPU 架构解耦 + Phase 1.4 L1Cache 方法学验证
v2.0 决策 (F1-F6 + 议题 1-8) + 4 类拆分文档 (blueprint / plan / status / entry)
```

## 相关文档

- [项目架构总览](../../../docs/architecture/overview.md) — ISA 无关性原则、ch_stream 接口设计
- [接口设计详解](../../../docs/architecture/interface-design.md) — Bundle 定义规范

# CPU IP 文档索引

## 当前架构设计（权威文档）

| 文档 | 说明 | 状态 |
|------|------|------|
| [multi_isa_architecture.md](multi_isa_architecture.md) | 多 ISA 架构设计：三层分离（core/arch/tlm+rtl）、ExecContext 接口、Plugin 分类体系 | **Active** |

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
```

## 相关文档

- [项目架构总览](../../../docs/architecture/overview.md) — ISA 无关性原则、ch_stream 接口设计
- [接口设计详解](../../../docs/architecture/interface-design.md) — Bundle 定义规范

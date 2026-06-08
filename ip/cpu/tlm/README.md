# CPU TLM 实现

> ⚠️ **Status: Phase 5 Planning** — RTL not yet implemented.
> 
> ⚠️ **DEPRECATED 术语 (2026-06-08)**:
> - `PipelineCore` → 废弃，改用 `PipeBuilder`（见 [multi_isa_architecture.md v2.0 §2.4](../docs/multi_isa_architecture.md)）
> - `Stageable` → 废弃，改用 `Payload<T>`（见 [multi_isa_architecture.md v2.0 §2.3](../docs/multi_isa_architecture.md)）
> - 权威设计：[`docs/architecture/declarative-hybrid-framework.md` v2.0.2](../../docs/architecture/declarative-hybrid-framework.md)
> - 决策依据：[`.omo/drafts/decision-plugin-framework-2026-06-08.md`](../../.omo/drafts/decision-plugin-framework-2026-06-08.md)

## 概述
基于 CppTLM 框架的 RISC-V ISS（指令集模拟器），采用 Plugin 架构（[Plugin-style 设计](../../docs/roadmap/phases/phase-1-tlm-foundation.md)）。

## 核心组件
- **PipeBuilder** *(原 PipelineCore)*：声明式编译期调度生成器（见 [multi_isa_architecture.md v2.0 §2.4](../docs/multi_isa_architecture.md)）
- **Plugin 系统**：各指令功能模块（ALU、MUL、DIV、LSU、CSR 等）—— 业务逻辑用 `at_stage()` 注册，无 `tick()`
- **Payload<T>** *(原 Stageable)*：类型安全 Key，跨阶段数据通路声明（见 [multi_isa_architecture.md v2.0 §2.3](../docs/multi_isa_architecture.md)）

## 性能目标
- TLM 仿真速度：≥ 1 MIPS（单核）
- 指令精度：cycle-approximate

## 依赖
- CppTLM 框架（SimObject、EventQueue、ch_stream）
- RISC-V ISA 解码库

## 文件结构（规划）
```
tlm/
├── src/
│   ├── RiscvCpuTlm.h          # 顶层 SimObject（TLM 模块）
│   ├── PipeBuilder.h           # 声明式调度器（原 PipelineCore.h）
│   ├── Payload.h                # 类型安全 Key（原 Stageable.h）
│   └── plugins/
│       ├── AluPlugin.h          # 每个 Plugin 继承 cf::plugin::PluginBase
│       ├── MulPlugin.h          # 业务逻辑用 at_stage() 注册
│       ├── LsuPlugin.h          # 无 tick()，无状态机
│       ├── CsrPlugin.h
│       └── BranchPlugin.h
└── CMakeLists.txt
```

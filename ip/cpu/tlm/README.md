# CPU TLM 实现

## 概述
基于 CppTLM 框架的 RISC-V ISS（指令集模拟器），采用 Plugin + Stageable 架构。

## 核心组件
- **PipelineCore**：流水线调度引擎，管理阶段推进和冒险检测
- **Plugin 系统**：各指令功能模块（ALU、MUL、DIV、LSU、CSR 等）
- **Stageable**：跨阶段数据通路声明

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
│   ├── RiscvCpuTlm.h          # 顶层 SimObject
│   ├── PipelineCore.h          # 流水线调度
│   ├── Stageable.h             # 跨阶段数据通路
│   └── plugins/
│       ├── AluPlugin.h
│       ├── MulPlugin.h
│       ├── LsuPlugin.h
│       ├── CsrPlugin.h
│       └── BranchPlugin.h
└── CMakeLists.txt
```

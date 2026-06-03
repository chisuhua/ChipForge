# 核心技术选型

## 1. 选型概述

ChipForge 的技术栈选择以**TLM/RTL 统一建模**和**设计空间探索**为核心目标，在仿真精度、开发效率和可扩展性之间寻求最优平衡。

## 2. TLM 框架选型：CppTLM

### 2.1 候选方案对比

| 维度 | CppTLM | SystemC TLM-2.0 | gem5 | SST |
|------|--------|----------------|------|-----|
| **学习曲线** | ⭐⭐⭐ 现代 C++ | ⭐⭐ 复杂模板 | ⭐⭐ 庞大代码基 | ⭐⭐⭐ 组件化 |
| **RTL 集成** | ✓ Bundle 共享 | ✗ 需桥接层 | ✗ 不支持 | ✗ 不支持 |
| **ISA 灵活性** | ✓ 通过 Plugin | ✓ 但耦合深 | ✓ 原生多ISA | △ 需适配 |
| **DSE 支持** | ✓ JSON 配置 | △ 需自建 | ✓ 内置 | ✓ 内置 |
| **仿真速度** | ~1-10 MIPS | ~0.5-5 MIPS | ~1-5 MIPS | ~1-10 MIPS |
| **代码量** | 轻量 | 中等 | 庞大 (50万+行) | 中等 |

### 2.2 选择 CppTLM 的理由

1. **Bundle 共享是关键差异化**：TLM 和 RTL 使用相同数据结构，无需编写适配层
2. **现代 C++ 设计**：利用模板和概念（concepts）实现零成本抽象
3. **轻量可控**：代码基小，易于理解和定制
4. **JSON 配置原生支持**：组件参数化开箱即用

### 2.3 不选 SystemC 的理由

- TLM-2.0 协议复杂，学习曲线陡峭
- sc_module/sc_port 模板嵌套过深
- 与 RTL 框架集成需要额外适配层
- 社区发展缓慢，现代 C++ 特性支持不足

### 2.4 不选 gem5 的理由

- 代码基庞大（50万+行），修改成本高
- 不支持 TLM/RTL 混合验证
- ISA 扩展需修改核心框架
- 非模块化设计，难以独立复用组件

## 3. RTL 框架选型：CppHDL

### 3.1 候选方案对比

| 维度 | CppHDL | Chisel/FIRRTL | SpinalHDL | Verilog |
|------|--------|--------------|-----------|---------|
| **与 TLM 集成** | ✓ Bundle 共享 | ✗ Scala 生态 | ✗ Scala 生态 | ✗ 无 |
| **生成质量** | 待验证 | ✓ 成熟 | ✓ 成熟 | N/A |
| **调试体验** | ✓ C++ 原生 | △ JVM | △ JVM | △ 波形 |
| **参数化** | ✓ 模板 | ✓ Scala | ✓ Scala | △ generate |
| **成熟度** | ⭐⭐ 发展中 | ⭐⭐⭐⭐ 成熟 | ⭐⭐⭐ 成熟 | ⭐⭐⭐⭐⭐ |

### 3.2 选择 CppHDL 的理由

1. **与 CppTLM 语言统一**：同一语言、同一 Bundle、同一工具链
2. **渐进迁移**：TLM 组件可逐步替换为 RTL 实现
3. **调试友好**：C++ 原生调试器直接介入
4. **学习成本低**：团队无需掌握 Scala

### 3.3 已知风险

- CppHDL 成熟度不如 Chisel/SpinalHDL
- Verilog 生成质量需持续验证
- 综合工具兼容性待确认
- 社区支持有限

### 3.4 风险缓解策略

- Phase 5 前持续跟踪 CppHDL 发展
- 准备 Chisel 备选方案（通过 FIRRTL 桥接）
- 定期用 Verilator 验证生成代码质量
- 关键路径保留手写 Verilog 接口

## 4. 仿真器选型

| 工具 | 用途 | 阶段 |
|------|------|------|
| CppTLM 内置引擎 | TLM 仿真 | Phase 1-4 |
| Verilator | RTL 仿真 | Phase 5 |
| Spike | 参考 ISS | 全阶段 |
| QEMU | 系统仿真参考 | Phase 4 |

## 5. 构建与工具链

| 工具 | 用途 | 选型理由 |
|------|------|---------|
| CMake | 构建系统 | 跨平台、生态成熟 |
| GCC/Clang | C++ 编译器 | C++20 支持完善 |
| GoogleTest | 单元测试 | 与 CMake 集成好 |
| Ninja | 构建加速 | 增量编译快 |
| JSON for Modern C++ | 配置解析 | Header-only、高性能 |

## 6. 参考框架借鉴

| 参考项目 | 借鉴内容 | 应用场景 |
|---------|---------|---------|
| gem5 | DSE 方法论、统计框架 | 性能建模 |
| SST | 组件化架构、消息传递 | 模块解耦 |
| Chipyard | SoC 生成器理念 | JSON 配置驱动 |
| VexRiscv | Plugin 架构、Stageable | CPU 设计 |

## 7. 技术栈演进计划

| 阶段 | 核心技术 | 新增工具 |
|------|---------|---------|
| Phase 1 | CppTLM + CMake | 基础框架 |
| Phase 2 | + Spike + riscv-tests | 验证工具 |
| Phase 3 | + Zephyr RTOS | RTOS 支持 |
| Phase 4 | + Linux + QEMU | 系统级 |
| Phase 5 | + CppHDL + Verilator | RTL 工具链 |

## 8. 相关文档
- [项目架构总览](overview.md)
- [背景与目标](background-and-goals.md)
- [接口设计详解](interface-design.md)

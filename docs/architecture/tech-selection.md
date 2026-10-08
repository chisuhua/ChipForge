# 核心技术选型

## 1. 选型概述

> **核心改动（ADR-083, 2026-10-08）**：ChipForge 用 **Plugin-style**（D4 决策）设计硬件 IP，**业务 IP 不再分别为 TLM 和 RTL 各写一份**。技术选型围绕"**Plugin-style 单一 source of truth + CppHDL 设施后端**"展开。**CppTLM 不再作为业务 IP 的设计框架**，仅作为外部依赖保留（bridge 适配层用）。

技术栈核心目标：
- **单一 source of truth**：业务 IP 一份代码（Plugin-style）+ 双模式编译
- **设计空间探索**：DSE 工具链 + Pareto 前沿 + 敏感性分析
- **多芯片可扩展**：RISC-V / GPU 等异构 SoC 形态
- **仿真精度阶梯**：TLM POD 直仿 → CH_MEM Simulator → Verilog → Verilator 联合仿真

## 2. 设计模式选型：Plugin-style（D4 决策，ADR-037 + ADR-083）

### 2.1 候选方案对比

| 维度 | Plugin-style (cf::plugin) | tick() 命令式 | VexRiscv Plugin (Scala) | SpinalHDL (Scala) |
|------|---------------------------|---------------|------------------------|-------------------|
| **范式** | 声明式（`at_stage` 调度由框架决定） | 命令式（`tick()` 自管理） | 声明式（`service` 调度） | 声明式 |
| **调度拥有者** | `PipeBuilder` | 模块自身 | SpinalHDL framework | SpinalHDL compiler |
| **跨阶段通信** | `Payload<T>` 类型安全 | 裸指针 / std::variant | Plugin context | SpinalHDL flow |
| **TLM↔CH_MEM 切换** | 编译期 `-DCF_PLUGIN_USE_CH_MEM` | 需重写业务代码 | n/a（Scala 单模式） | n/a |
| **RTL 输出** | `pb.elaborate(ctx)` → Verilog | 需手写翻译层 | Scala → Verilog | Scala → Verilog |
| **学习曲线** | ⭐⭐ C++ 现代 | ⭐⭐⭐ 简单但易乱 | ⭐⭐⭐ Scala | ⭐⭐ Scala 生态 |
| **ChipForge 实测** | ✅ L1Cache/MMU/CPU 11 plugins | ❌ D4 决策禁 | n/a | n/a |

### 2.2 选择 Plugin-style 的理由（ADR-083）

1. **单一 source of truth**：业务 IP 一份代码经由双模式编译产出不同仿真后端——不需要"TLM 副本 + RTL 副本"双份维护
2. **编译期模式切换**：`uint_t<N>` 类型抽象 + `pb.run()` vs `pb.elaborate(ctx)` 调度入口分派，零运行时开销
3. **调度由框架拥有**：`PipeBuilder::run()` 按注册顺序执行 at_stage 回调，调度不散落在 `tick()` 调用顺序里
4. **类型安全**：`Payload<T>` 编译期类型检查 + 运行期 typeid 二次校验，跨阶段数据流零裸指针
5. **VexRiscv 工业验证**：SpinalHDL/VexRiscv 40+ 复杂 Plugin 共享同一流水线骨架，证明范式可扩展性
6. **可演进**：未来 Phase 7+ 真实 RTL 实体（`ip/<area>/rtl/`）可与 Plugin-style 共存（Plugin-style 主管功能 RTL 主管布局布线）

### 2.3 不选 `tick()` 命令式的理由

- 模块数量 > 20 + 阶段 > 10 时，tick() 间隐式时序约束散落在调用顺序细节里，几乎不可能静态验证
- 切 RTL 时需重写所有业务逻辑（不是重构）
- ADR-025 已 lock `PluginBase::tick() = delete`

### 2.4 不选 VexRiscv/SpinalHDL（Scala 路线）的理由

- 团队无 Scala 经验
- C++ 生态丰富（gdb/profiling/CMake/Catch2）
- CppHDL 提供等价 elaboration 能力（lnode DAG + toVerilog）

## 3. 硬件后端选型：CppHDL（CH_MEM 模式必依赖）

### 3.1 候选方案对比

| 维度 | CppHDL | Chisel/FIRRTL | SpinalHDL | 手写 Verilog |
|------|--------|--------------|-----------|--------------|
| **与 Plugin-style 集成** | ✓ lnode DAG elaboration | ✗ Scala 生态 | ✗ Scala 生态 | ✗ 无 |
| **生成质量** | ⭐⭐⭐ Verilator 5.052 PASS | ⭐⭐⭐⭐⭐ 成熟 | ⭐⭐⭐⭐⭐ 成熟 | ⭐⭐⭐⭐⭐ 手动 |
| **调试体验** | ✓ C++ 原生（gdb / addr2line） | △ JVM / Chisel repl | △ JVM | △ 波形 |
| **参数化** | ✓ C++ 模板 | ✓ Scala | ✓ Scala | △ generate |
| **成熟度** | ⭐⭐⭐ 实战验证（Phase 6c/6d） | ⭐⭐⭐⭐⭐ Berkeley 主流 | ⭐⭐⭐⭐ 法国主导 | ⭐⭐⭐⭐⭐ |

### 3.2 选择 CppHDL 的理由

1. **同语言**：业务代码（Plugin-style C++）+ 后端代码（CppHDL C++）= 单一工具链
2. **CH_MEM 模式直仿**：`Simulator::tick()` 无需 EDA 工具，CH_MEM binary ~300s 全套验证
3. **Verilator 联合仿真**：`toVerilog(ctx)` → `.v` → `verilator --cc` → 联合仿真，[verilator] 1/1 PASS（5 ELF tohost=1，10 assertions）
4. **不需引入新生态**：团队无需学 Scala/FIRRTL/Chisel
5. **可演进**：手写 Verilog（关键 IP）+ CppHDL 自动生成（通用逻辑）混搭，保留工艺灵活性

### 3.3 已知风险

- CppHDL 成熟度不如 Chisel/SpinalHDL（社区较小）
- 综合工具兼容性待持续验证（Vivado/Yosys/Design Compiler）
- chlib 组件库仅 25 个文件，对超复杂设计可能不够

### 3.4 风险缓解策略

- 关键 IP 路径保留手写 Verilog 接口（`ip/<area>/rtl/` 占位待 Phase 7+ 实装）
- Verilator 5.052+ 周期持续验证生成代码质量（CHANGELOG 跟踪）
- 跨工具验证：`Simulator::tick()` 直仿 vs Verilator 联合仿真 trace 对比
- 必要时通过 FIRRTL 桥接到 Chisel 备选路径（推迟到 Phase 7+ 真需要时）

## 4. Legacy 仿真内核：CppTLM（外部依赖，bridge 适配层用）

> **角色变更（ADR-083, 2026-10-08）**：CppTLM 不再作为业务 IP 的设计框架，仅作为外部依赖保留。`src/cf_plugin/bridge/` 适配层仍依赖 CppTLM 接口以桥接外部 CppTLM-based 系统。

### 4.1 CppTLM 在 ChipForge 中的当前用途

- **保留的用途**：`src/cf_plugin/bridge/{l1_cache_bridge, mmu_bridge}.cpp` 适配层
- **未来可能用途**：外部 CppTLM-based 系统（如 gem5 集成、SystemC 桥接）——推迟到真需要时
- **明确不用的场景**：
  - ❌ 业务 IP 不继承 `cpptlm::ChStreamModuleBase` / `cpptlm::SimObject`
  - ❌ 业务 IP 不调 `REGISTER_MODULE("TypeName", ClassName)`
  - ❌ 业务 IP 不写 `ch_stream<T>` 跨阶段通信

### 4.2 业务 IP 跨阶段通信替代

`cf::plugin::Payload<T>` + `PipeNode` 替代 `ch_stream<T>`：

```cpp
// 业务代码 — TLM 模式
static cf::plugin::Payload<uint64_t> addr_key{"addr"};
auto addr = node(addr_key);  // 编译期类型检查

// 业务代码 — CH_MEM 模式（同一份代码 + #ifdef CF_PLUGIN_USE_CH_MEM）
// 编译期 uint_t<N> 自动展开为 ch::core::ch_uint<N>
// pb.elaborate(ctx) 一次性把跨阶段数据流绑定为 lnode 引用
```

## 5. 仿真器选型

| 工具 | 用途 | 阶段 | 实测 |
|------|------|------|------|
| C++ 直仿（`pb.run()`） | TLM 模式 | 默认 | ~0.6s 全套（chipforge_tests 432/432） |
| CppHDL Simulator (`ch::core::Simulator::tick()`) | CH_MEM 模式直仿 | `-DCF_PLUGIN_USE_CH_MEM` | ~300s 全套（chipforge_tests_chmem 48/48） |
| Verilator 5.052+ | CH_MEM Verilog 联合仿真 | `--enable-mmu/--enable-cache` | `[verilator]` 1/1 PASS（5 ELF, 10 assertions） |
| Spike | 参考 ISS | 全阶段 | co-simulation / golden reference |
| QEMU | 系统仿真参考 | Phase 4 | Linux kernel 验证 |

## 6. 构建与工具链

| 工具 | 用途 | 选型理由 |
|------|------|---------|
| CMake | 构建系统 | 跨平台、生态成熟 |
| GCC 16.1.0 / Clang | C++ 编译器 | C++20 支持完善 |
| Catch2 v3.7.0 | 测试框架 | 与 CMake 集成好，vendored 在 tests/catch2/ |
| Ninja | 构建加速 | 增量编译快 |
| nlohmann/json | 配置解析 | Header-only、高性能 |

## 7. 参考框架借鉴

| 参考项目 | 借鉴内容 | 应用场景 |
|---------|---------|---------|
| gem5 | DSE 方法论、统计框架 | 性能建模 |
| SST | 组件化架构、消息传递 | 模块解耦 |
| Chipyard | SoC 生成器理念 | JSON 配置驱动 |
| **VexRiscv** | **Plugin 架构 + Stageable** | **CPU 设计（Plugin-style 灵感来源）** |
| **SpinalHDL** | **elaboration DSL（执行即布线）** | **CH_MEM elaboration 借鉴** |
| **CppHDL** | **chlib + ch_state_machine** | **多周期 FSM 豁免（ADR-046）** |

## 8. 技术栈演进计划（修订）

| 阶段 | 核心技术 | 新增工具 | 备注 |
|------|---------|---------|------|
| Phase 0 | cf::plugin 脚手架 + TLM POD 直仿 | CMake | ✅ 完成 (2026-06-08) |
| Phase 1 | + L1CachePlugin + 5 缓存测试 | GoogleTest → Catch2 | ✅ 完成 |
| Phase 2 | + CPU 11 Plugin + DSE 576-config sweep | Catch2 + nlohmann/json | ✅ 完成 |
| Phase 6c | + CH_MEM elaboration (-DCF_PLUGIN_USE_CH_MEM) | CppHDL ch_uint/ch_reg/ch_mem | ✅ 完成 (ADR-040 v2.0) |
| Phase 6d | + CH_MEM 5-stage + Verilator | CppHDL Simulator + toVerilog + Verilator | 🟡 进行中 |
| Phase 7+ | + 真实 RTL 实体 (ip/<area>/rtl/) + 工艺映射 | Yosys / Design Compiler | 预留 |

## 9. 相关文档
- [项目架构总览](overview.md)
- [背景与目标](background-and-goals.md)
- [ADR-083 Plugin-style 单一 source of truth](adr/ADR-083-plugin-style-single-source-of-truth.md)
- [ADR-040 TLM→HDL 移植性约束 v2.0](adr/ADR-040-tlm-hdl-portability-constraints.md)
- [ADR-037 Plugin 作为设计范式](adr/ADR-037-plugin-as-design-paradigm.md)

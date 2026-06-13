# Phase 0：Plugin 最小脚手架

> **Status**: ✅ Completed (2026-06-08, 5/5 P0 组件 + 全部退出标准 v2 达成)
> **Milestone**: M0 - Plugin 脚手架可运行
> **Depends on**: None
> **决策依据**: `.omo/drafts/decision-plugin-framework-2026-06-08.md`
> **目标版本**: ChipForge 0.0.x（脚手架基线）
> **退出日期**: 2026-06-08

**目标**：建立 Plugin-style 设计的基础脚手架（scaffolding），为 Phase1 业务逻辑（L1CachePlugin 等）提供可执行的运行时支撑。

> **关键概念区分**：
> - **脚手架（scaffolding）**：提供"挂载点"和"基本结构"，每个 IP 自带自己的调度与实现
> - **框架（framework）**：完整的调度/管理/可视化能力（推迟到 Phase6）
>
> Phase0 **不是** "完整 Plugin 框架"——它是**让 Plugin-style 业务逻辑能跑起来**的最小集。

---

## 1. 任务清单（5 个 P0 交付物）

### 1.1 `PluginBase` 接口（2 天）

- [ ] 定义 `cf::plugin::PluginBase` 抽象基类
  - 暴露 `setup(PipeBuilder&)`（跨 Plugin 引用声明，默认空实现）
  - 暴露 `build(PipeBuilder&)`（实际生成逻辑，强制纯虚）
- [ ] **编译期断言禁止 `tick()`**：用 `static_assert` 或 `final` 类 + 删除的 `tick()` 阻止误用
- [ ] 单元测试 `test_plugin_lifecycle.cpp`
  - 测试 setup → build 调用顺序
  - 测试子类强制实现 build

**借鉴源**：VexRiscv `Plugin.scala`（仅 25 行，2 个方法）

### 1.2 `Payload<T>` 类型安全 Key（2 天）

- [ ] 定义 `cf::plugin::Payload<T>` 模板
  - 全局静态对象（描述符）
  - 类型擦除存储于 `PipeNode::payloads_`
- [ ] 提供 `operator()(const Payload<T>&)` 类型安全访问
- [ ] 单元测试 `test_payload.cpp`
  - 编译期类型检查
  - 同一 Key 在不同 PipeNode 间的隔离

**借鉴源**：VexRiscv `Stageable[T]` + CppHDL `ch_state_machine` 静态对象模式

### 1.3 `PipeNode` 节点（3 天）

- [ ] 定义 `cf::plugin::PipeNode`
  - 内部 `std::map<PayloadKeyBase*, std::any>` 存储 Payload
  - 简单 valid/ready/cancel 状态机
  - 派生状态方法：`is_firing()` / `is_moving()` / `is_blocked()` / `is_canceling()`
- [ ] 提供 `create_node(name)` 工厂（由 PipeBuilder 调用）
- [ ] 单元测试 `test_pipe_node.cpp`
  - 状态机转换正确性
  - Payload 多 Key 访问

**借鉴源**：`declarative-hybrid-framework.md` §7.1 + CppHDL `ch_state_machine` 内部结构

### 1.4 `PipeBuilder` 编排器（4 天）

- [ ] 定义 `cf::plugin::PipeBuilder`
  - `register_plugin(std::unique_ptr<Plugin>)`
  - `at_stage(stage_name, phase, callback)`
  - `declare_substage(parent, sub, depth)`（最小实现：仅声明，无深度调度）
  - `node_of_logic_stage(stage_name)` 查找
  - `build()` 编译入口
  - `run()` 顺序执行所有 at_stage 回调
- [ ] 仿 `chlib/stream_builder.h` 链式 API 风格
- [ ] 单元测试 `test_pipe_builder.cpp`
  - 调度确定性（同一组 Plugin 多次执行结果一致）
  - 多 Plugin 注册顺序保证
  - 阶段命名冲突检测

**借鉴源**：`chlib/stream_builder.h`（链式 API 形态）+ `ch_state_machine`（build 编译模式）

### 1.5 `CtrlLink` 控制 API（3 天）

- [ ] 定义 `cf::plugin::CtrlLink`（继承 `PipeLink`）
- [ ] 四种控制 API：
  - `halt_when(cond)` —— 阻塞下游 ready
  - `throw_when(cond)` —— 注入 cancel
  - `flush_when(cond)` —— 清空寄存器
  - `bypass(key, src)` —— 旁路转发
- [ ] 多条件 OR 合并（`pipeline_stall_ctrl` 复用）
- [ ] 单元测试 `test_ctrl_link.cpp`
  - 单/多条件 halt 行为
  - flush/throw 优先级

**借鉴源**：`chlib/pipeline.h::pipeline_stall_ctrl/flush_ctrl`（OR 合并逻辑已实现）

---

## 2. 退出标准

Phase0 必须**全部满足**以下标准才能进入 Phase1：

### 2.1 功能标准

- [ ] 5 个 P0 组件全部实现且单元测试通过
- [ ] 一个**最小验证 Plugin**（~10 行代码）能在 PipeBuilder 下端到端跑通
  - 示例：`struct HelloPlugin : PluginBase { void build(PB& pb) { pb.at_stage("greet", NORMAL, []{ printf("Hello\n"); }); } };`
- [ ] 与原 CppTLM/CppHDL 框架无冲突（独立编译）

### 2.2 质量标准

- [ ] **调度确定性证明**：同一组 Plugin 注册顺序相同 → 多次执行结果一致
- [ ] **零 TODO 残留**（与 CppTLM 零债务原则一致）
- [ ] 单元测试覆盖率 ≥ 80%
- [ ] API 文档（Doxygen）完整

### 2.3 集成标准

- [ ] 与 `cpptlm::ChStreamModuleBase` 共存无冲突
- [ ] 与 `ch::Component` 共存无冲突
- [ ] 编译期类型安全（错类型 Payload Key 编译失败）

---

## 3. 显式不做（推迟到 Phase6）

| 推迟项 | 理由 |
|--------|------|
| `enum class ImplMode` 枚举 | Phase0 仅做 TLM 模式验证；多模式由 Phase6 引入 |
| `BundleMapper` 模板 | Phase0 用编译期类型切换（`uint_t<N>`）足够 |
| `CompareDriver` / `ScoreBoard` | 验证基础设施在 Phase5/6 引入 |
| JSON `pipeline_stages` 解析 | Phase6 引入 |
| RTL AST 生成（VerilogCodeGen 集成） | Phase6 引入 |
| 模块级 `impl_mode_override` | Phase6 引入 |
| `CtrlLink` 的 `bypass` 完整语义 | Phase0 仅做最简版；完整 OR 合并等 Phase6 |
| 调度算法（依赖分析、最优调度）| Phase0 仅做顺序执行 |

---

## 3.5 Plugin 存储修改规则（Tier-1 强制）

> 本节是 [ADR-040 §2.1](../../architecture/adr.md#adr-040) Tier-1 约束的 Phase 0 退出标准。Phase 0 → Phase 1 过渡时，**所有** `ip/*/tlm/` 业务代码必须满足以下 6 条规则。任意一条违规 = CI FAIL = 阻塞合并。

| # | 规则 | 理由 | 检查工具 | 违规示例 |
|---|------|------|----------|----------|
| 1 | 无 `void tick()` 业务重写 | 调度由框架确定性决定，Plugin 不持有时序 | `tools/verify_plugin_decision.sh` Check 1 | `void MyPlugin::tick() { ... }` ❌ |
| 2 | 无状态机（`enum class State` + `switch state_`）| 控制流必须通过 `at_stage` 表达 | `tools/verify_plugin_decision.sh` Check 2 | `enum class State { IDLE, BUSY }; switch (state_) {...}` ❌ |
| 3 | Bundle 字段用 `cf::plugin::uint_t<N>` | 为 `ch_uint<N>` 升级保留类型别名空间 | `tools/verify_plugin_decision.sh` Check 3 | `ch_uint<32> addr;` 或 `uint64_t addr;` ❌ |
| 4 | **`at_stage` 回调内无 `if (cond) return;` 早返** | RTL 中无"早返"概念，需用 `when` 条件驱动；早返导致 commit 边界不一致 | `tools/check_plugin_portability.sh` Check 1 | `if (hit) return;` 在 `at_stage` 闭包内 ❌ |
| 5 | **`ip/*/tlm/` 业务代码无 `ch_mem` / `ch_reg` / `ch_uint` / `ch::core::context` 渗透** | TLM 模式无 `ch::core::context` 依赖；ch_mem 仅在 RTL 路径出现 | `tools/check_plugin_portability.sh` Check 2 | `#include "core/mem.h"` 或 `ch_mem<T, N> tags_;` 在 `ip/*/tlm/**/*.cpp` ❌ |
| 6 | **Plugin 内部不调用 `pb.run()`** | `pb.run()` 是顶层入口；Plugin 回调应只读/写 Payload，不触发调度 | `tools/check_plugin_portability.sh` Check 3 | `pb.run();` 在 `L1CachePlugin::build()` 内 ❌ |

**完整约束与 5 步迁移手册**：见 [ADR-040](../../architecture/adr.md#adr-040) §2.1 + §4。

**为什么是 Tier-1 强制**：L1CachePlugin Phase 1.2 已暴露三类不匹配点（API 形态 / 时序语义 / 抽象层级）。若 Phase 1 业务代码沿用"裸 `std::array` + 早返 + shift+mask 位提取"模式，Phase 5/6 升级时需逐文件重写，违反 ADR-037 的"Plugin-style 业务代码 Phase 6 不重写"承诺。Tier-1 约束通过 CI 强制把不匹配点消灭在源头。

**Tier-2 警告**（不阻塞）：存储优先 `cf::plugin::storage::array_store`、位提取走 helper、阶段名复用统一字典、测试 API 暴露。详见 [ADR-040 §2.1](../../architecture/adr.md#adr-040)。

---

## 4. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| R1: 脚手架工作低估（实际 4 周而非 2-3 周）| 中 | 中 | 每周评估进度，必要时调整范围或拆为 Phase0a/0b |
| R2: 与 `ChStreamModuleBase` 命名/语义冲突 | 中 | 中 | 严格区分：Plugin 是声明式逻辑单元，ChStreamModuleBase 是 TLM 模块类；命名不重叠 |
| R3: 命名冲突（`halt_when` vs `stream_halt_when` 等）| 低 | 中 | 已识别 4 项冲突（见决策文档 §3.5），按 D6-D9 方案处理 |
| R4: Payload 模板性能问题（typeid 查询）| 低 | 低 | 测试时 benchmark；如必要改用编译期分发 |
| R5: Plugin 风格不被 C++17 静态类型系统接受 | 低 | 高 | Phase0 退出标准强制要求"在 PipeBuilder 下端到端跑通" |

---

## 5. Phase0 工期估算

| 任务 | 工时 |
|------|------|
| PluginBase | 2 天 |
| Payload<T> | 2 天 |
| PipeNode | 3 天 |
| PipeBuilder | 4 天 |
| CtrlLink | 3 天 |
| 测试 + 文档 | 2-3 天 |
| **总计** | **~14-16 工作日（2.5-3 周）** |

---

## 6. 与后续 Phase 的衔接

### 6.1 Phase0 → Phase1 接口稳定性承诺

Phase0 完成后，**以下接口在 Phase1-5 期间**保持稳定**（仅 Phase6 才升级）：

- `cf::plugin::PluginBase::setup(PipeBuilder&)` / `build(PipeBuilder&)`
- `cf::plugin::Payload<T>` 模板与 `operator()` 访问
- `cf::plugin::PipeNode` 状态机 API
- `cf::plugin::PipeBuilder` 的 `at_stage` / `register_plugin` / `build` / `run` / `node_of_logic_stage`
- `cf::plugin::CtrlLink` 的 `halt_when` / `throw_when` / `flush_when` / `bypass`

这意味着 Phase1 业务逻辑（L1CachePlugin）使用上述接口后，**不需要重写**即可在 Phase6 升级到完整框架。

### 6.2 Phase0 与现有框架的关系

| 现有框架 | Phase0 关系 |
|----------|------------|
| `cpptlm::ChStreamModuleBase` | **正交**：Plugin 是声明式逻辑单元，ChStreamModuleBase 是 TLM 模块类。两者不重叠，**无需桥接** |
| `ch::Component` | **正交**：Component 是 CppHDL RTL 描述，Plugin 是声明式逻辑单元。两者并行存在 |
| `chlib::PipelineStage` / `PipelineChain` | **可借鉴**：Phase0 的 StageLink 可调用这些已有组件作为 RTL 后端实现 |
| `chlib::stream_*_when` | **共存**：保留 chlib 自由函数（D6 决策），Plugin 路径用 `ctrl_link.halt_when()` 对象方法 |

### 6.3 与 v2.0.1 路线图的对应关系

v2.0.1 §12.2 中"Phase 1a: Plugin/Pipe 核心机制（4-6 周）"等同于本 Phase0，但**范围缩小**：
- ✅ 包含：Plugin/Payload/PipeNode/PipeBuilder/CtrlLink
- ❌ 不包含：JSON 解析 / CompareDriver / 完整 BundleMapper / Phase 调度算法

v2.0.1 §12.2 中"Phase 1b（JSON + 验证基础设施）"与"Phase 1c（端到端 IP）"被**合并到 Phase1**——因为 Phase1 的 L1CachePlugin 既是端到端 IP 又是验证用例。

---

## 7. 决策可追溯

本 Phase0 的所有设计决策来源于：
- **决策记录**: `.omo/drafts/decision-plugin-framework-2026-06-08.md`（D1-D11）
- **架构文档**: `docs/architecture/declarative-hybrid-framework.md` v2.0.2 §12.2
- **现有设计**: `ip/cpu/docs/multi_isa_architecture.md` v2.0（详细设计参考）
- **外部研究**: VexRiscv / SpinalHDL / CppHDL chlib（实施参考）

任何对本 Phase0 范围/接口的修改，**必须**同步更新决策记录。

---

*Phase0 完成后，建议立即开始 Phase1（L1CachePlugin "Hello World"），验证 Plugin 风格在 TLM 模式下的可行性。*

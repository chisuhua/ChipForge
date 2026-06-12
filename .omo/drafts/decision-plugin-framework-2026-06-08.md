# 决策记录：Plugin最小脚手架 + L1CachePlugin作为Phase1 Hello World

| 字段 | 值 |
|------|-----|
| 决策ID | DECISION-2026-06-08-01 |
| 决策日期 | 2026-06-08 |
| 决策状态 | **Accepted**（2026-06-08, 与 `docs/architecture/adr.md` ADR-037 Accepted 状态一致; D1-D11 全部落地: PluginBase 5/5 测试, Payload<T>, PipeNode, PipeBuilder, CtrlLink 四种控制 API）|
| 提出方 | Prometheus（基于多轮对话分析） |
| 决策影响 | 项目路线图重塑、技术债优先级变更 |
| 关联文档 | `.omo/plans/plugin-framework-revision-plan.md` |

---

## 1. 决策背景（触发本决策的对话）

本决策起源于三轮关键对话，每一轮都纠正或精炼了前一轮的结论：

### 第1轮：用户初始问题
> "请检查docs/roadmap实施路线图计划，并给出是否需要预先构建一个声明式电路设计的MVP阶段，在通过声明式混合电路设计后，我们再考虑按计划实施，或者调整计划？"

**Prometheus 初始立场**（Option B：调整计划，不插入 MVP）：
- 认为 Plugin 模型是 Phase5 工具，与 Phase1-4 正交
- 建议把 Plugin 模型推迟到 Phase6（约 12-20 周）
- 推荐"先按原路线图走"

### 第2轮：用户的关键纠正
> "如果一开始没有考虑Plugin的声明式设计 TLM模块，后期（Phase6)再设计成Plugin的声明式的设计，是不是相当于前期的设计完全作废？Plugin模型不仅仅是未了RTL协同，而是证明这种设计风格可以替代tick()风格的设计。"

**用户核心论点**：
1. **Plugin 是范式，不是工具**——它要"证明声明式风格可以替代 tick() 风格"
2. **后期改造 = 前期作废**——tick()-based 代码改造成 Plugin-style 几乎全重写
3. **Plugin 设计需要在 Day1 采用**——决定每个 IP 的设计风格

**Prometheus 修正**：接受 Plugin 是范式，提出"Phase1 业务逻辑用 Plugin-style"。

### 第3轮：用户对 "manual_runner" 概念的挑战
> "对于第2项，我理解Plugin-style的Tlm电路设计依赖于Plugin框架， 而你提到mannual_runner也应该是框架要实现的内容之一。我们要先讨论清楚这问题。"

**用户核心论点**：
- Plugin-style 业务逻辑**依赖于** Plugin 框架
- "manual_runner" 不是"零框架"——它是"最小框架"
- 必须先把"manual_runner" 的真实工作量算清楚

**Prometheus 修正**：
- 承认"manual_runner"是误称，改用 **"Plugin最小框架"（600-800 行 / 2-4 周）**
- 引入 **Phase0 = Plugin最小脚手架** 的概念
- 把"框架 vs 脚手架"的关键区分讲清楚

### 第4轮：用户最终决策
> "我同意Plugin最小框架说法，我现在认为这Plugin框架是提供了一个基础的脚手架，不同的电路模块需要不同的调度和具体实现，我们制定的路线文档中要加入对其架构的探索，我觉的'Hello work'应该选取一个真实的Plugin来实现，比如L1CachePlugin, Phase0先打好基础，我们在明确了方向后再进入属于业务逻辑的Phase1实现。"

**用户最终指令**：
1. **Plugin框架 = 基础脚手架**（不预先规定调度，每个电路自己决定）
2. **路线图加入架构探索阶段**（Phase0 = 探索）
3. **Hello World = 真实 Plugin**（L1CachePlugin，不是占位）
4. **Phase0 先打基础** → **Phase1 再做业务**（顺序明确）

---

## 2. 决策的"三段式论证"

### 2.1 论证一：Plugin 是范式，不是工具

**证据**：
- SpinalHDL/VexRiscv 验证：Plugin 是 CPU 设计的主流范式（VexRiscv 有 40+ Plugin 共享同一流水线骨架）
- 范式转移的代价：如果 Phase1-5 用 `tick()` 风格，Phase6 改 Plugin-style 需要**重写所有 IP 业务逻辑**（不是简单重构）
- 范式区分的判据：
  - 工具：可加可减，不影响业务结构
  - 范式：决定业务结构，加减会破坏一致性

**结论**：必须从 Day1 采用 Plugin-style，否则项目架构不连贯。

### 2.2 论证二：Plugin 框架 = 脚手架，不等于完整框架

**关键区分**：
- **脚手架（scaffolding）**：提供"挂载点"和"基本结构"，具体调度由每个 IP 自己决定
- **框架（framework）**：提供完整的调度/管理/可视化能力

**Plugin 最小脚手架的最小集**（5 个 P0）：
1. `PluginBase`（仅 `setup` + `build`，无 tick）
2. `Payload<T>` 类型安全 Key
3. `PipeNode`（PayloadMap + 简单 valid/ready 状态）
4. `PipeBuilder`（注册 + `at_stage` + 调度）
5. `CtrlLink` 四种控制 API

**工作量估算**：~600-800 行 / 2-3 周 / 14 工作日

**对比完整 PipeBuilder 框架**（`declarative-hybrid §12.2` Phase 1a/1b/1c）：
- 总计 12-20 周
- 增量：调度算法、JSON 解析、ScoreBoard、CompareDriver、RTL 生成等

**脚手架 vs 框架的边界**：
| 维度 | 脚手架（Phase0） | 框架（Phase6） |
|------|------------------|----------------|
| Plugin 生命周期 | setup + build | + 延迟构建 + 重做 |
| 调度 | 顺序 / 简单并行 | 依赖分析 / 最优调度 |
| JSON 配置 | 无 | pipeline_stages 解析 |
| 验证 | 无 ScoreBoard | CompareDriver + ScoreBoard |
| RTL 生成 | 无 | VerilogCodeGen 集成 |

**结论**：脚手架是 Phase0 的真实工作量，必须先完成。

### 2.3 论证三：L1CachePlugin 作为 Hello World

**选择理由**：
- L1Cache 是真实硬件（不是占位）
- 涉及所有 Plugin 核心概念（lookup / refill / 状态寄存器 / 内存接口）
- 与 `cpptlm::CacheTLM` 已有代码可比对（功能等价基线）
- 复杂度适中（不是 Hello World 级别的 `printf`，但也不是完整 RV32I）

**L1CachePlugin 验证设计风格**：
- 无 `tick()`、无状态机
- 所有逻辑用 `at_stage()` 注册
- Bundle 字段用 `uint_t<N>`（§5.6 编译期切换）
- 阶段间通信通过 `Payload<T>` Key

**Phase1 退出标准**：
1. L1CachePlugin 在 TLM 模式下端到端跑通
2. 与 `cpptlm::CacheTLM` 同输入比对结果一致
3. 业务逻辑代码无 `tick()`、无状态机（静态检查）
4. Bundle 字段使用 `uint_t<N>`（编译期验证）
5. 所有阶段用 `at_stage()` 注册

---

## 3. 外部研究输入（背景研究的核心发现）

### 3.1 VexRiscv Plugin 模型

- **极简接口**：`Plugin.scala` 仅 25 行，2 个方法（`setup` + `build`）
- 40+ 复杂 Plugin 共享同一骨架
- 调度采用**显式顺序**（非数据流分析）
- 类型安全通过 `Stageable[T>` Key
- 服务定位通过 `pipeline.service[T]`

**对 ChipForge 的启示**：Plugin 接口不需要复杂钩子，setup+build 足够。

### 3.2 SpinalHDL Component 模型

- 使用 `addPrePopTask` 机制实现"延迟 plugin 钩子"
- 编译期生成 RTL，Phase 系统（`PhaseNetlist`、`PhaseCheck` 等）驱动转换
- 编译流程：`FIBER_INIT → FIBER_SETUP → FIBER_BUILD → FIBER_CHECK → FIBER_EMIT`

**对 ChipForge 的启示**：C++17 同步 `build()` 已够用，无需复杂的"延迟钩子"。

### 3.3 CppHDL 现有可借鉴模式

| 模式 | 来源文件 | 行数 | 作为 Phase0 什么 |
|------|---------|------|------------------|
| 链式声明式 API | `chlib/stream_builder.h` | 156 | `PipeBuilder` API 形态 |
| 回调注册 + build | `chlib/state_machine.h` | 284 | `Plugin` 基类骨架 |
| valid/ready 流水线 | `chlib/stream_pipeline.h::stream_m2s_pipe` | 147 | `StageLink` RTL 后端 |
| 多条件 OR 合并 | `chlib/pipeline.h::pipeline_stall_ctrl` | 393 | `CtrlLink` 内部逻辑 |

### 3.4 CppHDL 现有 RV32I 5级流水线

- 位置：`CppHDL/include/cpu/pipeline/rv32i_pipeline.h`（274 行 + 22 文件 ~5600 行）
- 风格：**命令式 `Component::describe()`**（与 Plugin 风格**正交**）
- 价值：**功能等价基线**——Phase0 的 L1CachePlugin 跑通后，可与 `cpptlm::CacheTLM` + `Rv32iPipeline` 的行为做对照

### 3.5 已识别的命名冲突

| 冲突 | 现有 | 新设计 | 决策 |
|------|------|--------|------|
| halt 命名 | `stream_halt_when` 自由函数 | `CtrlLink::halt_when` 对象方法 | **方案 C**：两者共存，明确层级差异 |
| 旧术语 PipelineCore | `ip/cpu/tlm/README.md` L9 | PipeBuilder | 废弃 PipelineCore |
| 旧术语 Stageable | `ip/cpu/tlm/README.md` L11 | Payload<T> | 废弃 Stageable |
| 命名撞车 | `PluginLoader`（dlopen） | `Plugin`（声明式） | 文档明确两者无关 |

---

## 4. 决策内容

### 4.1 核心决策（D1-D5）

| ID | 决策 | 理由 |
|----|------|------|
| **D1** | 路线图前插入 **Phase0 = Plugin最小脚手架**（2-3 周） | Plugin 框架是脚手架，必须先打基础 |
| **D2** | **Phase1 Hello World = L1CachePlugin**（真实 Plugin，不是占位） | 验证 Plugin 风格可行性 |
| **D3** | **Phase0 显式不做**：ImplMode / BundleMapper / CompareDriver / ScoreBoard / JSON 解析 / RTL 生成 | 推迟到 Phase6，避免 Phase0 范围蔓延 |
| **D4** | **Phase1 业务逻辑强制采用 Plugin-style**（无 `tick()`、无状态机、Bundle 字段用 `uint_t<N>`） | 防止"用 tick() 风格，Phase6 再改造"的范式重写 |
| **D5** | **Phase6 = 完整 PipeBuilder 框架 + RTL 生成**（12-20 周） | 在 Plugin-style 业务逻辑稳定后再投入完整框架 |

### 4.2 命名冲突决策（D6-D9）

| ID | 决策 | 理由 |
|----|------|------|
| **D6** | 保留 `chlib::stream_halt_when` 自由函数 + 新增 `CtrlLink::halt_when` 对象方法 | 零破坏现有 28 个 chlib 测试 |
| **D7** | 废弃 `PipelineCore`（旧 tlm/README 术语）| 与新 `PipeBuilder` 不一致 |
| **D8** | 废弃 `Stageable`（旧 tlm/README 术语）| 与新 `Payload<T>` 不一致 |
| **D9** | 文档明确 `PluginLoader`（dlopen）与 `Plugin`（声明式）无关 | 仅命名撞车，语义不同 |

### 4.3 Phase0 范围（5 个 P0 交付物）

| # | 组件 | 工作量 | 借鉴源 |
|---|------|--------|--------|
| 1 | `PluginBase`（仅 setup+build，无 tick） | 2 天 | VexRiscv 25 行 Plugin |
| 2 | `Payload<T>` 类型安全 Key（全局静态对象） | 2 天 | VexRiscv `Stageable[T]` |
| 3 | `PipeNode`（PayloadMap + 简单 valid/ready 状态） | 3 天 | CppHDL `ch_state_machine` |
| 4 | `PipeBuilder`（注册 Plugin + at_stage + 调度） | 4 天 | `chlib/stream_builder.h` |
| 5 | `CtrlLink`（halt_when/throw_when/flush_when/bypass） | 3 天 | `chlib::pipeline_stall_ctrl` |
| **总计** | | **~14 工作日（2-3 周）** | |

### 4.4 Phase1 退出标准（D10）

1. ✅ L1CachePlugin 在 TLM 模式下端到端跑通
2. ✅ 与 `cpptlm::CacheTLM` 同输入比对结果一致
3. ✅ 业务逻辑代码无 `tick()`、无状态机（静态检查）
4. ✅ Bundle 字段使用 `uint_t<N>`（编译期验证）
5. ✅ 所有阶段用 `at_stage()` 注册

### 4.5 Phase6 触发条件（D11）

满足以下任一条件启动 Phase6：
- Phase1 L1CachePlugin + 至少 2 个其他 Plugin-style IP 稳定运行
- 出现"第三个需要 TLM↔RTL 协同的 IP"（强制触发）
- 用户主动决定启动完整框架工作

---

## 5. 替代方案与拒绝理由

### 5.1 Option A：不插入 Plugin MVP，按原路线图走

**方案**：Phase1-5 用 tick() 风格，Phase6 再改造为 Plugin-style

**拒绝理由**：
- ❌ 违反 D4：业务逻辑强制 Plugin-style
- ❌ Phase6 改造 = Phase1-5 IP 业务逻辑全部重写
- ❌ 范式不一致：项目后期可能混用 tick() 和 Plugin-style

**何时重新评估**：如果用户改变"Plugin 是范式"的判断

### 5.2 Option B：直接做完整 PipeBuilder 框架（12-20 周）

**方案**：不做 Phase0 脚手架，直接投入 Phase6 完整框架

**拒绝理由**：
- ❌ 12-20 周内没有任何业务 IP 可跑
- ❌ 框架设计无业务反馈（容易过度设计）
- ❌ 与 ChipForge AGENTS.md "零债务原则" 冲突（每 Phase 必须有可运行产物）

**何时重新评估**：如果项目目标是"先有完整框架再有业务"（本项目不是）

### 5.3 Option C：调整路线图不插入新阶段

**方案**：把 Plugin 框架工作塞入 Phase1 工作量内

**拒绝理由**：
- ❌ Phase1 工作量从 6-10 周 → 18-30 周
- ❌ 风险：Phase1 延期会阻塞所有后续 Phase
- ❌ 不符合用户"先打基础，再做业务"的明确指令

**何时重新评估**：如果用户改变"先打基础，再做业务"的判断

---

## 6. 影响与风险

### 6.1 正面影响

- ✅ 业务逻辑代码（Phase1-5）与框架升级（Phase6）解耦
- ✅ 脚手架工作量小（2-3 周）可控
- ✅ L1CachePlugin 作为真实 Hello World，验证 Plugin 风格可行性
- ✅ 与 SpinalHDL/VexRiscv 主流范式对齐

### 6.2 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| R1: Plugin 风格不被 TLM 框架接受 | 高：Phase1 业务逻辑无法运行 | Phase0 退出标准强制要求"在 TLM 模式下跑通" |
| R2: Phase0 脚手架工作低估 | 中：可能 4 周而非 2-3 周 | 每组件单独验证，发现问题及时调整范围 |
| R3: 与现有 `ChStreamModuleBase` 冲突 | 中：两个模块抽象共存混乱 | 严格区分：Plugin 是声明式逻辑单元，ChStreamModuleBase 是 TLM 模块类 |
| R4: L1CachePlugin 与 `cpptlm::CacheTLM` 行为不一致 | 中：基线对照失败 | Phase1 退出标准 2 强制要求一致 |
| R5: 命名冲突未识别 | 低：未来代码混淆 | D6-D9 已给出明确决策 |
| R6: 用户对 Plugin-style 设计哲学改变 | 高：决策基础动摇 | 决策文档 §8 提供重新审视指引 |

### 6.3 决策的"可逆性"

| 决策 | 可逆性 | 难度 |
|------|--------|------|
| D1: 插入 Phase0 | 高：删除 Phase0 文件即可 | 低 |
| D2: L1CachePlugin 作为 Hello World | 高：换其他 Plugin | 低 |
| D4: Plugin-style 强制 | **低**：若 Phase1 已用 Plugin-style，事后改回 tick() 几乎全重写 | 高 |
| D6-D9: 命名决策 | 高：改名 + 改文档 | 中 |
| D5: Phase6 推迟 | 高：提前启动 | 低 |

**最不可逆的决策**：D4（Plugin-style 强制）。这是本决策的核心承诺。

---

## 7. 验证标准（如何判断决策是否正确）

### 7.1 短期验证（Phase0 完成时）

- [ ] 5 个 P0 组件全部实现并测试通过
- [ ] 一个最小验证 Plugin（10 行代码）能跑通
- [ ] 与原 CppTLM/CppHDL 框架无冲突

### 7.2 中期验证（Phase1 完成时）

- [ ] L1CachePlugin 在 TLM 模式下端到端跑通
- [ ] 与 `cpptlm::CacheTLM` 行为一致（功能等价基线）
- [ ] 业务代码无 `tick()`（grep 静态检查）
- [ ] Bundle 字段用 `uint_t<N>`（编译期验证）

### 7.3 长期验证（Phase6 完成时）

- [ ] 完整 PipeBuilder 框架支持所有 L1CachePlugin 业务代码
- [ ] 业务代码完全不变（无重写）
- [ ] 第三个 Plugin-style IP 跑通（证明可扩展性）
- [ ] TLM↔RTL 协同验证可用

### 7.4 决策反例（什么情况下决策是错的）

- ❌ Phase0 完成后无任何 Plugin 能跑通 → 脚手架设计有问题
- ❌ L1CachePlugin 与 `cpptlm::CacheTLM` 行为无法对齐 → 范式选择有问题
- ❌ Phase6 改造时仍需重写业务代码 → D4 的"框架升级不影响业务"承诺失败
- ❌ 出现"用 tick() 风格反而更简单"的情况 → Plugin 范式不适合此项目

---

## 8. 未来审视指引

### 8.1 何时应重新审视本决策

| 触发条件 | 应做什么 |
|----------|---------|
| Phase0 完成但 2 周后仍无法跑通最小 Plugin | 重新评估脚手架设计 |
| Phase1 L1CachePlugin 与 CacheTLM 行为不一致 | 重新评估 Plugin 风格在 TLM 下的适用性 |
| Phase6 启动后发现业务代码需大改 | 重新评估 D4 的"不重写"承诺 |
| 项目方向改变（不再是 RISC-V 虚拟原型） | 全面重评 Plugin 范式 |
| 业界出现新的、明显更优的电路设计范式 | 调研 + 重新评估 |

### 8.2 重新审视时的关键问题

1. **Plugin 范式是否仍是项目最佳选择**？（如果否，参考 Option A）
2. **脚手架 vs 完整框架的边界是否合理**？（检查 Phase0 范围）
3. **D4（Plugin-style 强制）是否造成过度限制**？（检查 Phase1 业务代码复杂度）
4. **L1CachePlugin 作为 Hello World 是否合适**？（检查 Phase1 工作量）
5. **命名决策是否经得起代码量增长**？（检查 D6-D9）

### 8.3 不应重新审视的情况

- ❌ "实施太慢"（这是工作量问题，不是决策问题）
- ❌ "文档写得不够好"（这是文档维护问题）
- ❌ "某个具体代码 bug"（这是实施细节问题）

---

## 9. 决策参考依据汇总

### 9.1 直接相关文档

| 文档 | 关键章节 | 作用 |
|------|---------|------|
| `docs/architecture/declarative-hybrid-framework.md` v2.0.1 | §4 Plugin 模型 / §7 PipeBuilder / §12 路线图 | 完整 Plugin 设计 |
| `ip/cpu/docs/multi_isa_architecture.md` v2.0 | §2 PipeNode/PipeLink/PipeBuilder / §3 Plugin | 1100+ 行 CPU 侧设计 |
| `docs/roadmap/README.md` | 当前 5 个 Phase 概览 | 路线图基线 |
| `docs/architecture/adr.md` | ADR-025~036（Plugin/Pipeline 12 条）| 决策注册表 |
| `CppHDL/include/chlib/*.h` | 现有 25 个组件 | Phase0 借鉴源 |

### 9.2 后台研究结果

| Task ID | 主题 | 关键发现 |
|---------|------|---------|
| `bg_26642156` | ChipForge 内部 Plugin 模式盘点 | multi_isa_architecture.md 是权威；chlib 有 4 个可借鉴模式 |
| `bg_1a3dca56` | 外部 Plugin 架构研究（VexRiscv/SpinalHDL/Bluespec/Gem5）| VexRiscv Plugin 仅 25 行；SpinalHDL 编译流程启示 |

### 9.3 关键参考实现

- VexRiscv `Plugin.scala`（25 行）—— 极简 Plugin 接口的范本
- SpinalHDL `Component.addPrePopTask` —— 延迟构建机制
- CppHDL `chlib/stream_builder.h`（156 行）—— 链式 API 形态
- CppHDL `chlib/state_machine.h`（284 行）—— 回调注册 + build 编译
- CppHDL `chlib/stream_pipeline.h::stream_m2s_pipe`（147 行）—— StageLink RTL 后端
- CppHDL `chlib/pipeline.h::pipeline_stall_ctrl`（393 行）—— CtrlLink OR 合并

---

## 10. 决策状态变更历史

| 日期 | 状态 | 变更 |
|------|------|------|
| 2026-06-08 | Proposed | 初始创建（基于 4 轮对话 + 2 个后台研究） |
| 2026-06-09 | Accepted | 事实上的 Accepted 确认: plugin-docs-extraction 计划 12/12 任务完成,ADR 实现率 63% → 79%,Phase 0 5/5 P0 组件 + 51/51 单元测试 PASS,见 `state/final-report-plugin-docs-extraction.md` |
| 待定 | Superseded | 出现新的更优决策时变更 |

---

*本决策记录是 ChipForge 项目战略层的关键决策之一。建议每季度审视一次，或在 Phase0/Phase1/Phase6 关键节点重新评估。*

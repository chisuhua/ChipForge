# Plugin 框架文档修订计划

| 字段 | 值 |
|------|-----|
| 计划版本 | 1.0 |
| 计划日期 | 2026-06-08 |
| 计划状态 | **Draft（待用户最终确认）** |
| 关联决策 | `.omo/drafts/decision-plugin-framework-2026-06-08.md` |
| 预计总工作量 | ~7-9 工作日（含文档+验证） |
| 风险等级 | 中（D4 决策不可逆） |

---

## 1. 修订目标

将 Plugin 框架的架构探索（Phase0）和 L1CachePlugin 作为 Hello World（Phase1）这两项核心决策，准确反映到项目文档体系中。

### 1.1 核心目标

1. **明确 Plugin 范式定位**：在所有架构文档中统一"Plugin 是范式而非工具"的认知
2. **建立 Phase0 路径**：让路线图清晰显示"先打脚手架，再做业务"
3. **同步命名空间**：废弃旧术语（PipelineCore、Stageable），统一新术语（PipeBuilder、Payload<T>）
4. **保留决策可追溯**：决策依据完整记录，未来可重新审视

### 1.2 非目标

- ❌ 不实际编写任何 Plugin 代码（这是 Phase0 实施，不是文档计划）
- ❌ 不修改 CppTLM/CppHDL 框架代码
- ❌ 不删除任何现有文档（仅标注废弃）
- ❌ 不创建新 IP 实现（如 L1Cache 的 TLM/rtl 实际代码）

---

## 2. 文档清单

### 2.1 新建文档（4 项）

| # | 路径 | 用途 | 内容大纲 | 估时 |
|---|------|------|---------|------|
| N1 | `docs/roadmap/phases/phase-0-plugin-scaffolding.md` | Phase0 任务清单 | 5 个 P0 交付物 + 退出标准 + 风险 | 1.5 天 |
| N2 | `docs/roadmap/phases/phase-1-tlm-foundation.md` | Phase1 任务清单 | 7 个原任务 + 替换 Hello World 为 L1CachePlugin | 1 天 |
| N3 | `.omo/drafts/decision-plugin-framework-2026-06-08.md` | 决策记录 | 决策背景/论证/范围/风险/验证（**已完成**）| 0（已创建）|
| N4 | `.omo/plans/plugin-framework-revision-plan.md` | 本文档 | 修订计划（**已完成**）| 0（已创建）|

### 2.2 修改文档（5 项）

| # | 路径 | 修改内容 | 估时 |
|---|------|---------|------|
| M1 | `docs/roadmap/README.md` | 在 Phase1 前插入 Phase0；在 Phase5 后追加 Phase6 | 0.5 天 |
| M2 | `docs/architecture/declarative-hybrid-framework.md` v2.0.1 → v2.0.2 | §12.2 改写为"Phase0 Plugin最小脚手架"；§4.8 强化；§6.8 说明 L1CachePlugin 验证 | 2 天 |
| M3 | `ip/cpu/tlm/README.md` | 废弃 `PipelineCore` / `Stageable` 术语；指向新设计文档 | 0.5 天 |
| M4 | `docs/architecture/adr.md` | 新增 ADR-037（Plugin 范式决策）、更新 ADR-025~036 状态 | 1 天 |
| M5 | `docs/architecture/declarative-hybrid-framework.md` §12.0.3 | 责任归属表：明确 Phase0 Owner = TBD；Phase1 Owner = TBD | 0.5 天 |

### 2.3 标注废弃（1 项，不删除）

| # | 路径 | 操作 | 说明 |
|---|------|------|------|
| D1 | `ip/cpu/tlm/README.md` L9-11 | 标注废弃 + 引用 N1 | PipelineCore / Stageable 旧术语 |

### 2.4 验证脚本（1 项，可选）

| # | 路径 | 用途 | 估时 |
|---|------|------|------|
| V1 | `tools/verify_plugin_decision.sh` | 机械验证文档符合决策 | 0.5 天 |

### 2.5 总体文档数量

| 操作类型 | 数量 | 估计工作量 |
|----------|------|-----------|
| 新建 | 4 项 | 3 工作日 |
| 修改 | 5 项 | 4.5 工作日 |
| 标注废弃 | 1 项 | 含在 M3 中 |
| 验证脚本 | 1 项（可选）| 0.5 工作日 |
| **合计** | **10 项** | **~7.5-8 工作日** |

---

## 3. 修订顺序（依赖图）

### 3.1 执行顺序

```
第一波（无依赖，可并行）：
├── N1：phase-0-plugin-scaffolding.md（新建）
├── N2：phase-1-tlm-foundation.md（新建）
└── M3：ip/cpu/tlm/README.md 废弃旧术语（修改）

第二波（依赖第一波）：
├── M1：roadmap/README.md 插入 Phase0/Phase6（依赖 N1、N2）
├── M2：declarative-hybrid-framework.md v2.0.1 → v2.0.2（依赖 N1）
└── M4：adr.md 新增 ADR-037（依赖 N1）

第三波（依赖第二波）：
└── M5：declarative-hybrid-framework.md §12.0.3 责任归属（依赖 M2）

第四波（验证，可选）：
└── V1：verify_plugin_decision.sh（依赖所有文档完成）
```

### 3.2 关键依赖说明

- **M1 依赖 N1/N2**：roadmap 总览必须知道 Phase0/Phase1 文档存在
- **M2 依赖 N1**：架构文档的 §12.2 必须引用 Phase0 文档
- **M5 依赖 M2**：责任表修改是 v2.0.2 的一部分
- **V1 依赖所有**：验证脚本是最后一步

---

## 4. 各文档详细规划

### 4.1 N1：`docs/roadmap/phases/phase-0-plugin-scaffolding.md`

**目标读者**：Phase0 实施工程师
**长度**：~150-200 行
**结构**：

```markdown
# Phase 0: Plugin最小脚手架

> Status: Not Started
> Milestone: M0 - Plugin脚手架可运行
> Depends on: None
> 决策依据: .omo/drafts/decision-plugin-framework-2026-06-08.md

**目标**: 建立 Plugin-style 设计的基础脚手架

## 任务清单（5 个 P0）

### 1. PluginBase（2 天）
- [] 定义 cf::plugin::PluginBase 接口
- [] 仅暴露 setup(PipeBuilder&) 和 build(PipeBuilder&)
- [] 禁止 tick()（编译期断言）
- [] 单元测试

### 2. Payload<T>（2 天）
- [] 定义 cf::plugin::Payload<T> 模板
- [] 全局静态对象 + 类型擦除
- [] 单元测试（编译期类型检查）

### 3. PipeNode（3 天）
- [] 定义 cf::plugin::PipeNode
- [] 内部 std::map<PayloadKeyBase*, std::any>
- [] 简单 valid/ready 状态机（is_firing/moving/blocked/canceling）
- [] 单元测试

### 4. PipeBuilder（4 天）
- [] 定义 cf::plugin::PipeBuilder
- [] register_plugin + at_stage + 顺序/简单并行调度
- [] 仿 chlib/stream_builder.h 链式 API
- [] 单元测试 + 调度确定性证明

### 5. CtrlLink（3 天）
- [] 定义 cf::plugin::CtrlLink
- [] halt_when / throw_when / flush_when / bypass API
- [] 多条件 OR 合并
- [] 单元测试

## 退出标准

- [ ] 5 个组件全部实现并单元测试通过
- [ ] 一个最小验证 Plugin（~10 行）能跑通
- [ ] 与原 CppTLM/CppHDL 框架无冲突
- [ ] 调度确定性证明
- [ ] 零 TODO 残留

## 显式不做（推迟到 Phase6）

- ImplMode 枚举
- BundleMapper 模板
- CompareDriver / ScoreBoard
- JSON pipeline_stages 解析
- RTL 生成

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| 脚手架工作低估 | 每周评估，必要时调整范围 |
| 与 ChStreamModuleBase 冲突 | Phase0 退出标准强制要求兼容 |
| 命名冲突未识别 | 已识别 4 项（见决策文档 §3.5）|
```

### 4.2 N2：`docs/roadmap/phases/phase-1-tlm-foundation.md`

**目标读者**：Phase1 实施工程师
**长度**：~100-150 行
**结构**：

```markdown
# Phase 1: 基础 TLM 平台

> Status: Not Started
> Milestone: M1 - L1CachePlugin "Hello World"
> Depends on: Phase 0
> 决策依据: .omo/drafts/decision-plugin-framework-2026-06-08.md

**目标**: 验证 Plugin-style 设计在 TLM 模式下的可行性

## 任务清单

### 1. Bundle 层定义
- [] MemReqBundle / MemRespBundle
- [] CacheReqBundle / CacheRespBundle
- [] L1CachePluginBundle
- [] IntBundle

### 2. L1CachePlugin 实现（Hello World）
- [] L1CachePlugin 类
- [] lookup / refill 两阶段
- [] tags/data/valid 三个 ch_mem
- [] 单元测试（与 cpptlm::CacheTLM 对比）

### 3. 最小 SoC 装配
- [] 最小 SoC JSON（l1_cache_minimal.json）
- [] traffic_gen -> L1CachePlugin -> memory

### 4. 验证
- [] L1CachePlugin 在 TLM 模式下端到端跑通
- [] 与 cpptlm::CacheTLM 同输入比对一致
- [] 业务代码无 tick()（grep 静态检查）
- [] Bundle 字段用 uint_t<N>（编译期验证）

## 退出标准

满足 Phase1 退出标准（D10）：
- [ ] L1CachePlugin 在 TLM 模式下端到端跑通
- [ ] 与 cpptlm::CacheTLM 行为一致
- [ ] 业务代码无 tick()
- [ ] Bundle 字段使用 uint_t<N>
- [ ] 所有阶段用 at_stage() 注册

## 设计风格约束（D4 强制）

- 禁止 tick()（编译期断言 + 静态检查）
- 禁止状态机（所有控制流用 at_stage + Payload）
- 强制使用 uint_t<N>（不直接用 ch_uint<N> 或 uint64_t）
- 所有 IO 通过 ch_stream<Bundle>
```

### 4.3 M1：`docs/roadmap/README.md`

**修改内容**：
- 在 Phase 1 前插入 Phase 0
- 在 Phase 5 后追加 Phase 6
- 在文档导航中引用新创建的 phase 文档

**修改示意**：
```diff
| Phase 1 | [基础框架搭建](phases/phase-1-foundation.md) | Not Started |
+ | **Phase 0** | [Plugin最小脚手架](phases/phase-0-plugin-scaffolding.md) | Not Started |
+ | Phase 1 | [基础 TLM 平台（Hello World = L1CachePlugin）](phases/phase-1-tlm-foundation.md) | Not Started |
+ | **Phase 6** | 完整 PipeBuilder 框架 + RTL 生成 | Not Started |
```

### 4.4 M2：`docs/architecture/declarative-hybrid-framework.md` v2.0.1 → v2.0.2

**主要修改**：

| 章节 | 修改类型 | 内容 |
|------|---------|------|
| 文档头 | 版本号 | 2.0.1 → 2.0.2 |
| §12.2 | **重写** | "Phase 1 拆分方案" 改为 "Phase 0 Plugin 最小脚手架" |
| §12.2.x | **新增** | 5 个 P0 交付物 |
| §12.3-12.4 | 编号调整 | 推迟到 Phase6 |
| §4.8 | 强化 | 增加"脚手架 vs 框架"区分 |
| §6.8 | 补充 | 说明 Phase1 L1CachePlugin 是脚手架验证用例 |
| §12.0.3 | 新增行 | Phase0 Owner、Phase1 Owner |
| §12.5 | 新增条目 | 维护规则第 5 条：决策可追溯 |

### 4.5 M3：`ip/cpu/tlm/README.md`

**修改内容**：
- L9-11 标注废弃
- 指向新设计文档（multi_isa_architecture.md v2.0）

**修改示意**：
```diff
+ > ⚠️ **DEPRECATED** (2026-06-08)
+ > 本节中的 PipelineCore 和 Stageable 术语已废弃。
+ > 替代术语：PipelineCore → PipeBuilder；Stageable → Payload<T>
+ > 权威设计：见 [multi_isa_architecture.md v2.0](multi_isa_architecture.md)

## 核心组件
- **PipelineCore**：流水线调度引擎，管理阶段推进和冒险检测
+ - **PipeBuilder**：声明式编译期调度生成器（替代 PipelineCore）
- **Plugin 系统**：各指令功能模块（ALU、MUL、DIV、LSU、CSR 等）
- **Stageable**：跨阶段数据通路声明
+ - **Payload<T>**：类型安全 Key（替代 Stageable）
```

### 4.6 M4：`docs/architecture/adr.md`

**修改内容**：
- 新增 ADR-037：Plugin 范式决策记录
- 更新 ADR-025~036 状态（部分从"未实施"变更为"Phase0 范围"）

**ADR-037 大纲**：
```markdown
## ADR-037: Plugin 作为设计范式（不是工具）

**状态**: Accepted (2026-06-08)
**决策者**: User + Prometheus
**背景**: 见 .omo/drafts/decision-plugin-framework-2026-06-08.md
**决策内容**:
- 业务逻辑强制采用 Plugin-style 设计（D4）
- 路线图插入 Phase0 = Plugin 最小脚手架
- 推迟完整 PipeBuilder 框架到 Phase6
**影响**: 重塑 Phase 1-5 实施路径
**影响 ADR**: ADR-025~036 状态变更
```

---

## 5. 成功标准

### 5.1 文档完成标准

- [ ] N1 文档创建完成，含 5 个 P0 任务 + 退出标准
- [ ] N2 文档创建完成，含 L1CachePlugin 任务清单
- [ ] M1 路线图 README 更新，新阶段引用正确
- [ ] M2 架构文档升级至 v2.0.2，§12.2 重写
- [ ] M3 tlm/README 旧术语标注废弃
- [ ] M4 adr.md 新增 ADR-037
- [ ] M5 §12.0.3 责任归属更新

### 5.2 一致性标准

- [ ] 所有文档中"Phase0 = Plugin 最小脚手架"表述一致
- [ ] 所有文档中"L1CachePlugin = Phase1 Hello World"表述一致
- [ ] 所有文档中命名术语（PipeBuilder、Payload<T>）一致
- [ ] 决策文档与执行文档无矛盾

### 5.3 可追溯标准

- [ ] 决策文档存在并完整
- [ ] 决策文档在所有相关文档中引用
- [ ] 修订计划文档存在
- [ ] 所有修改可在 git log 中追溯

---

## 6. 风险与回滚

### 6.1 风险矩阵

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| R1: 文档间内容不一致 | 中 | 中 | 完成时做一次交叉验证 |
| R2: 决策被用户拒绝 | 中 | 高 | 本计划是 Draft 状态，等用户确认才执行 |
| R3: 决策基础改变（Plugin 范式被否定）| 低 | 高 | 决策文档 §8 提供重新审视指引 |
| R4: Phase0 文档被误读为"框架已就绪" | 中 | 中 | 显式标注"脚手架"而非"框架" |

### 6.2 回滚方案

如果决策被拒绝（最坏情况）：

1. **删除新文档**（N1, N2）：N1 + N2 文件直接删除
2. **回滚修改**（M1, M2, M3, M4, M5）：git revert
3. **保留决策文档**：作为"被拒绝的方案"保留，供未来参考
4. **不删除** `.omo/drafts/decision-plugin-framework-2026-06-08.md`：作为决策历史

### 6.3 渐进回滚

如果用户对决策部分接受：

| 决策 | 部分接受场景 | 处理 |
|------|------------|------|
| D1 插入 Phase0 | 同意脚手架但调整范围 | 保留 N1，调整任务清单 |
| D2 L1CachePlugin | 改用其他 Plugin | 修改 N2 |
| D4 Plugin-style 强制 | 部分场景允许 tick() | N2 增加豁免条件 |
| D6-D9 命名 | 接受部分 | 调整相关文档 |

---

## 7. 执行时间表（建议）

### 7.1 单人串行执行

| 天数 | 任务 | 备注 |
|------|------|------|
| Day 1 | N1（Phase0 文档） + M3（tlm/README 废弃） | 基础文档 |
| Day 2 | N2（Phase1 文档） + M4（adr.md） | 应用文档 |
| Day 3 | M2（架构文档升级，最重的） | 核心文档 |
| Day 4 | M1（roadmap README）+ M5（责任归属） | 收尾 |
| Day 5 | 交叉验证 + 最终审核 | 质量保证 |

### 7.2 多人并行执行（如果有资源）

| 并行组 | 任务 |
|--------|------|
| 组 A（IP/Plugin 方向）| N1 + M3 + M5 |
| 组 B（架构方向）| M2 + M4 |
| 组 C（路线图方向）| N2 + M1 |

---

## 8. 待用户确认的决策点

在执行本计划前，请用户确认以下决策点：

| # | 决策点 | 建议 | 替代方案 |
|---|--------|------|----------|
| Q1 | 接受决策文档（DECISION-2026-06-08-01）？ | 接受 | 修改 / 拒绝 |
| Q2 | 接受 Phase0 范围（5 个 P0，2-3 周）？ | 接受 | 调整范围 |
| Q3 | 接受 Phase1 Hello World = L1CachePlugin？ | 接受 | 改其他 Plugin |
| Q4 | 接受命名决策（D6-D9，4 项冲突解决方案）？ | 接受 | 调整 |
| Q5 | 接受本修订计划（10 项文档操作）？ | 接受 | 调整范围 |

---

## 9. 计划完成后的下一步

修订计划执行完成后：

1. **Phase0 实施开始**：按 `phase-0-plugin-scaffolding.md` 任务清单开工
2. **用户审阅决策**：用户可以在任何时候重新审视 `.omo/drafts/decision-plugin-framework-2026-06-08.md`
3. **季度审视**：建议每季度检查决策的执行情况
4. **触发条件审视**：在 Phase0/Phase1/Phase6 关键节点重新评估

---

## 10. 计划状态变更历史

| 日期 | 状态 | 变更 |
|------|------|------|
| 2026-06-08 | Draft | 初始创建 |

---

*本计划依赖决策文档 `decision-plugin-framework-2026-06-08.md` 的最终确认。在用户确认前，不会执行实际的文档修改。*

---

## 11. 相关计划

本计划（v1.0 Draft，2026-06-08）的范围聚焦于"路线图修订 + 决策文档创建"。**插件架构文档的抽离**（将 `declarative-hybrid-framework.md` §4/§7 抽离到独立文档 `plugin-framework.md`）已在另一个独立计划中处理：

- **计划**：[`.omo/plans/plugin-docs-extraction.md`](plugin-docs-extraction.md) v1.0（2026-06-09）
- **范围**：新建 `docs/architecture/plugin-framework.md`（~580 行插件架构权威）+ 修正 5 条 STALE ADR（ADR-025~033）+ 修复 `tools/verify_adr.sh` 3 处脚本缺陷 + 移除 `declarative-hybrid-framework.md` §4/§7（1390 → 1241 行）
- **状态**：执行中（12 实施任务 + 4 验证任务）
- **预计工作量**：6-9 工作日（纯文档，零代码改动）
- **目标**：ADR 实现率从 63% → 79%，消除 §4/§7 与代码的"100% not in code" 描述背离

两个计划的关系：
- 本计划（v1.0）：路线图层（roadmap README + ADR-037 范式决策 + plugin-framework-revision-plan.md）
- 抽离计划（v1.0）：架构层（plugin-framework.md 文档 + ADR 状态修正 + 验证脚本修复）

抽离计划完成后，§4.8 跨抽象层关系（Plugin 与 ChStreamModuleBase 正交性）仍在主文档 `declarative-hybrid-framework.md` 中；插件架构内部设计（§4.1-§4.7 + §7 全部）已迁出至 `plugin-framework.md`。

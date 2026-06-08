# Phase 6：完整 PipeBuilder 框架 + RTL 生成

> **Status**: Not Started
> **Milestone**: M6 - 完整 Plugin 框架 + RTL 协同验证
> **Depends on**: Phase 0/1/2/3/4/5 完成，且至少 2-3 个 Plugin-style IP 稳定运行
> **决策依据**: `.omo/drafts/decision-plugin-framework-2026-06-08.md`
> **目标版本**: ChipForge 0.3.x

**目标**：将 Phase 0 提供的"Plugin 最小脚手架"升级为完整 PipeBuilder 框架，并集成 RTL 生成能力。

> **本文件为 v2.0.2 占位文档**。Phase 6 启动时将按 Phase 0-5 的反馈细化任务清单。

---

## 1. Phase 6 范围（来自 declarative-hybrid-framework.md §12.2.4）

| 子阶段 | 内容 | 工时 |
|--------|------|------|
| **Phase 6a** | 调度算法（依赖分析、最优调度）+ JSON `pipeline_stages` 解析 | 4-6 周 |
| **Phase 6b** | 验证基础设施（CompareDriver + ScoreBoard）+ 模块级 `impl_mode_override` | 4-6 周 |
| **Phase 6c** | 完整 RTL 生成（VerilogCodeGen 集成）+ 通用 TLM↔RTL 桥接 | 4-8 周 |
| **总计** | | **12-20 周** |

## 2. 触发条件

满足以下任一条件启动 Phase 6：
- Phase 1 L1CachePlugin + 至少 2 个其他 Plugin-style IP 稳定运行
- 出现"第三个需要 TLM↔RTL 协同的 IP"
- 用户主动决定启动

## 3. 与 Phase 0 脚手架的关系

Phase 0 提供的 5 个接口（`PluginBase` / `Payload<T>` / `PipeNode` / `PipeBuilder` / `CtrlLink`）在 Phase 6 期间**保持稳定**（仅扩展，不破坏现有业务代码）。Phase 1-5 业务逻辑（L1CachePlugin 等）不需要重写即可享受 Phase 6 的能力。

## 4. 推迟的设计项（v2.0.1 提案，本阶段才实施）

- `enum class ImplMode {TLM_ONLY, RTL_ONLY, COMPARE, SHADOW}` 完整实现
- `BundleMapper` 完整模板（Phase 0 仅用 `uint_t<N>` 编译期切换）
- `CompareDriver` + `ScoreBoard` 基类
- JSON `pipeline_stages` 完整解析
- 完整 RTL AST 生成（VerilogCodeGen 集成）
- 通用 TLM↔RTL 桥接（扩展 `HybridCacheWrapper` 模式）
- 性能基准测试套件
- DSE 集成（参数扫描 + Pareto 分析）

## 5. 决策可追溯

本 Phase 6 的所有设计决策来源于：
- **决策记录**: `.omo/drafts/decision-plugin-framework-2026-06-08.md`（D1, D5, D11）
- **v2.0.1 §12.2 Phase 1a/1b/1c 拆分方案**：本阶段重新映射为 6a/6b/6c

任何对 Phase 6 范围/接口的修改，**必须**同步更新决策记录。

---

*本文件为占位，详细任务清单待 Phase 6 启动时基于 Phase 0-5 反馈细化。*

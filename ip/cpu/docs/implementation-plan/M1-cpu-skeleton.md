# M1 — CPU 核心框架层

> **本文件位置**: `ip/cpu/docs/implementation-plan/M1-cpu-skeleton.md`
> **状态**: 🟡 待启动 (用户授权后启动)
> **估算**: 3-4 d
> **总体任务清单**: 见 [`README.md` §6 M1 行](README.md)

## 1. 目标

扩展 cf_plugin 框架层, 落地 CPU 模块复用 cf_plugin 所需的全部扩展点。**不实施任何具体 Plugin**。为 M2/M3 的 Plugin 套件提供基础。

## 2. 任务清单

| # | 任务 | 文件 | 验收 | 估时 |
|---|------|------|------|------|
| **M1.1** | 扩展 `cf::plugin::PipeBuilder`: 增加 `at_stage(logic_stage, phase, lambda)` | `include/cf/plugin/pipe_builder.h` | test_pipe_builder PASS | 1d |
| **M1.2** | 扩展 `cf::plugin::PipeBuilder`: 增加 `declare_substage(parent, sub_name, depth)` | `include/cf/plugin/pipe_builder.h` | test_pipe_builder PASS | 0.5d |
| **M1.3** | 新增 `cf::plugin::PipeLink`: StageLink (valid/ready 握手 + Payload 寄存一拍) | `include/cf/plugin/pipe_link.h` | test_ctrl_link PASS | 0.5d |
| **M1.4** | 新增 `cf::plugin::PipeLink`: DirectLink (组合直连, 无寄存) | `include/cf/plugin/pipe_link.h` | test_ctrl_link PASS | 0.3d |
| **M1.5** | 新增 `cf::plugin::PipeArbitration` 结构: valid/ready/cancel 三态 + 派生状态 | `include/cf/plugin/pipe_arbitration.h` | test_pipe_node PASS | 0.3d |
| **M1.6** | 集成 PipeArbitration 到 PipeNode (PipeNode 持有 `arb_` 成员) | `include/cf/plugin/pipe_node.h` | test_pipe_node PASS | 0.3d |
| **M1.7** | 新增 `ip/cpu/core/payload_common.h`: DecodePayload + 通用 Payload Key (PC/INSTRUCTION/RS1/RS2/RD_DATA/RD_IDX/DECODE/RESULT) | `ip/cpu/core/payload_common.h` | test_payload PASS | 0.3d |
| **M1.8** | 4/4 框架级单元测试 PASS (test_pipe_node / test_pipe_builder / test_payload / test_ctrl_link) | `ip/cpu/tests/unit/` | ctest 4/4 PASS | (累计) |

## 3. 依赖

- ✅ cf_plugin Phase 0 已落地 (PluginBase / Payload / PipeNode / PipeBuilder / CtrlLink)
- ❌ M2: 依赖 M1.7 (DecodePayload 通用 Key)

## 4. 完成判据

- [ ] M1.1-M1.7 全部 7 个子任务代码 commit
- [ ] M1.8: ctest 4/4 框架级单元测试 PASS
- [ ] docs/lessons/m1-cpu-framework.md 记录 B2 摩擦 (如有)
- [ ] 16/16 ctest 全局不退化 (Phase 1.2 16 个测试不能被新代码破坏)

## 5. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 修改 cf::plugin::PipeBuilder 破坏下游 (L1CachePlugin) | M1 启动前先跑 L1CachePlugin 4/4 单元测试基线; M1 完成后复测 |
| declare_substage 实现复杂度超预期 | 议题 1 选 C 推荐方案 A (直接改 cf_plugin); 若不达预期降级为方案 B (新建 ip/cpu/core/pipe_builder.h 包装) |
| 单元测试覆盖不足 (at_stage 早返陷阱) | 复用 L1Cache 6 维度方法学 D6 测试便利 (见 `../../docs/lessons/phase-1.2-l1cacheplugin.md`) |

## 6. ADR 需求

- ADR-XXX: PipeBuilder 扩展 at_stage/declare_substage 决策 (议题 1 选 C 推荐方案 A)

## 7. 任务编号约定

`M1.x` 其中 x = 1..8 (与本文件 §2 表格 # 列对应)

## 相关文档

- 总体任务: [`README.md`](README.md) §6 M1 行
- 静态架构 (Plugin 套件 + cf_plugin 扩展点): [`../blueprint.md`](../blueprint.md) §4, §6
- L1Cache 6 维度方法学: [`../../docs/lessons/phase-1.2-l1cacheplugin.md`](../../docs/lessons/phase-1.2-l1cacheplugin.md)
- 任务状态: [`../status.md`](../status.md)

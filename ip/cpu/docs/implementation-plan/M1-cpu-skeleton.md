# M1 — CPU 核心框架层

> **本文件位置**: `ip/cpu/docs/implementation-plan/M1-cpu-skeleton.md`
> **状态**: 🟡 进行中 (2026-06-16 启动, 探索阶段完成, 待 3 决策确认后进入 C2-C6 commit 实施)
> **估算**: 3-4 d → 实际 ~1.1 d (探索发现大量基线复用, 详见下文)
> **总体任务清单**: 见 [`README.md` §6 M1 行](README.md)

---

## 0. 实际状态 (2026-06-16 探索报告)

### 0.1 探索基线

本节记录 M1 启动前的代码状态调查, 用于校准 v2.0 文档中的 M1 任务清单。 调查范围:

- `include/cf/plugin/*.h` (8 个头文件, cf_plugin Phase 0 全部)
- `ip/cache/tlm/L1CachePlugin.{h,cpp}` (1st 完整 Plugin 范例)
- `src/cf_plugin/tests/` (15 个测试, 用 assert + main() 模式, 不是 gtest)
- `ip/cpu/tlm/`, `ip/cpu/test/`, `ip/cpu/configs/` (cpu IP 当前状态)

### 0.2 基线复用 (M1 任务实质完成度)

| M1 任务 | 计划描述 | 实际状态 | 后续动作 |
|---------|----------|----------|----------|
| **M1.1** | 扩展 `PipeBuilder::at_stage(logic_stage, phase, lambda)` | ✅ **已存在** `pipe_builder.h:69-76` | 公告: 0 实施量, 复用 cf_plugin Phase 0 |
| **M1.2** | 扩展 `PipeBuilder::declare_substage(parent, sub, depth)` | ✅ **已存在** `pipe_builder.h:78-86` | 公告: 0 实施量, 复用 cf_plugin Phase 0 |
| **M1.3** | 新增 `PipeLink: StageLink` (valid/ready 握手 + Payload 寄存一拍) | ⚠️ **部分覆盖** CtrlLink 存在 (`ctrl_link.h:24-96`, 有 halt_when/throw_when/flush_when/bypass) | 扩展 CtrlLink, 增加 StageLink/DirectLink 模式区分 (见 D-β) |
| **M1.4** | 新增 `PipeLink: DirectLink` (组合直连, 无寄存) | ⚠️ **部分覆盖** 同上 | 同 M1.3 |
| **M1.5** | 新增 `PipeArbitration` (valid/ready/cancel 三态 + 派生状态) | ❌ **未存在** | 新增 `include/cf/plugin/pipe_arbitration.h` (见 D-γ) |
| **M1.6** | 集成 PipeArbitration 到 PipeNode (PipeNode 持有 `arb_`) | ⚠️ **部分覆盖** PipeNode 已有 5 态 (IDLE/FIRING/MOVING/BLOCKED/CANCELING) + 5 个状态转移方法 (`assert_valid/assert_ready/deassert_ready/cancel/complete_cancel`), 但无 `arb_` 字段 | 增量加 `arb_` 字段, 不破坏现有 L1CachePlugin (C3 commit) |
| **M1.7** | 新增 `ip/cpu/core/payload_common.h` (DecodePayload + 8 通用 Key) | ❌ **未存在** (`ip/cpu/tlm/` 只有 README) | 新增 (C4 commit) |
| **M1.8** | 4/4 框架级测试 PASS | ⚠️ **3/4 已存在** (test_pipe_builder, test_pipe_node, test_payload, test_ctrl_link 各 1 套) | 4 套测试全 PASS 后确认 (C6 commit) |

### 0.3 M1 估时重校准

| 任务 | 原估算 | 重估算 | 理由 |
|------|--------|--------|------|
| M1.1+M1.2 | 1.5 d | **0 d** | cf_plugin Phase 0 已实现 |
| M1.3+M1.4 | 0.8 d | **0.3 d** | CtrlLink 已存在, 仅扩展 StageLink/DirectLink 模式 |
| M1.5 | 0.3 d | **0.3 d** | 全新文件, 小类 |
| M1.6 | 0.3 d | **0.2 d** | 仅加 arb_ 字段, 兼容现有 5 态方法 |
| M1.7 | 0.3 d | **0.3 d** | 全新文件, 8 Key + DecodePayload struct |
| M1.8 | 累计 | **0 d** | 跑 ctest 验证 |
| **M1 总计** | **3-4 d** | **~1.1 d** | 节省 2-3 d 复用成本 |

### 0.4 探索中的其他发现

**D-1: 测试结构不与 IP 目录绑定** — 所有测试在 `src/cf_plugin/tests/`, **不是** `ip/ccpu/tests/`。 L1CachePlugin 4/4 测试在 `src/cf_plugin/tests/test_l1_cache_plugin_unit.cpp`。 推论: M1 的 4 个测试中, 3 个已有 (test_pipe_builder/test_ctrl_link/test_pipe_node/test_payload), 1 个新增应该是 `test_payload_common.cpp` 放在 `src/cf_plugin/tests/` (与 cf_plugin 头文件同区), **不是**新建 `ip/cpu/tlm/tests/`。

**D-2: 命名空间约定** — L1CachePlugin 用 `cf::ip::cache::tlm::L1CachePlugin` (cf::ip::<module>::tlm::*), 蓝图说"新增 `cf::plugin::PipeLink` / `PipeArbitration`" (cf::plugin::* 框架层), `ip/cpu/core/payload_common.h` 用 `cf::cpu::core::payload::*` (cf::cpu::core::* IP 层, 但**不应**用 cf::ip::cpu:: — 与 L1Cache 的 cf::ip::cache 区分)。

**D-3: 匿名 namespace 静态 Payload Key 模式** — L1CachePlugin.cpp 47-77 行展示标准模式: 匿名 namespace 内定义 `cf::plugin::Payload<T> g_xxx{"module.key"};`, 跨阶段靠指针身份匹配。 `payload_common.h` 8 Key 应遵循此模式。

**D-4: D4 + ADR-040 合规** — L1CachePlugin 完整展示:
- 无业务 tick() (PluginBase::tick() 是 private deleted)
- Bundle 字段用 `cf::plugin::uint_t<N>`
- 阶段用 `pb.at_stage()`
- 跨阶段通信用 `Payload<T>` Key
- 存储用 `cf::plugin::storage::array_store<T, N>` + `pb.register_commit_hook`
- at_stage 闭包内**不早返** (全分支 if/else, 适配 HDL)

M1 任务必须 100% 符合 D4 + ADR-040。 这是 v2.0 决策约束 (议题 1 选 C 承诺)。

### 0.5 关键决策点 (M1 实施前需用户确认)

**D-α**: **M1 启动授权** (本 commit 公告此事, 等用户 OK 后进入 C2-C6)
**D-β**: **PipeLink 命名** — 走法 1 (兼容 CtrlLink 内部加 StageLink/DirectLink 模式, 不破坏 L1Cache) vs 走法 2 (重命名 CtrlLink → PipeLink, 改 L1CachePlugin)。 **建议: 走法 1**
**D-γ**: **PipeArbitration 实现位置** — 独立小类持有 valid/ready/cancel 三个 bool + 派生方法 (蓝图意图, 推荐) vs PipeNode state 字段的 façade (侵入性大)。 **建议: 独立小类**

### 0.6 M1 实际启动路径 (C2-C6 commit 拆分)

| # | 内容 | 文件 | 估时 | 风险 |
|---|------|------|------|------|
| **C1** | 公告 M1 启动 + 探索报告 (本文) | `M1-cpu-skeleton.md` + `status.md` | 0.1 d | 0 |
| **C2** | 新增 `cf::plugin::PipeArbitration` (含 StageLink/DirectLink 模式) | `include/cf/plugin/pipe_arbitration.h` | 0.3 d | 低 |
| **C3** | `PipeNode` 加 `arb_` 字段 + L1CachePlugin 兼容性测试 PASS | `include/cf/plugin/pipe_node.h` | 0.2 d | 中 |
| **C4** | `ip/cpu/core/payload_common.h` (8 Key + DecodePayload) | `ip/cpu/core/payload_common.h` | 0.3 d | 低 |
| **C5** | `test_payload_common` 4-6 用例 (与 cf_plugin 既有测试同区) | `src/cf_plugin/tests/test_payload_common.cpp` | 0.2 d | 低 |
| **C6** | 4 框架级测试 PASS 验证 + status.md 更新 | (跑 ctest + 改 status.md §1) | 0 d | 0 |
| **总计** | | | **~1.1 d** | |

---

## 1. 目标

扩展 cf_plugin 框架层, 落地 CPU 模块复用 cf_plugin 所需的全部扩展点。**不实施任何具体 Plugin**。为 M2/M3 的 Plugin 套件提供基础。

**M1 启动后, M1.1 + M1.2 实质工作量为零** (cf_plugin Phase 0 已实现), M1 实际工作集中在 M1.3-M1.7。 详见 §0 探索报告。

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

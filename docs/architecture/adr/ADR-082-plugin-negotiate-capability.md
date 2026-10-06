# ADR-082: Plugin::negotiate() capability 协商（v0.10.0）

> **Status**: 🚧 Drafting (2026-09-27, for v0.10.0)
> **类别**: Plugin / 框架扩展
> **关联 ADR**: ADR-037 v2.0 (Plugin 范式) · ADR-046 (FSM 豁免) · ADR-047 (Result 范式) · ADR-048 (注册规范序) · ADR-070 (RVC) · ADR-082 (LaneArbiter)
> **关联变更**: `openspec/changes/mfc-cpu-pipeline-multi-cycle-fsm/`（首个消费方）
> **首个消费 PoC**: PoC-1 MUL/DIV（ADR-046 FSHM 豁免需要声明 `requires: flush_broadcaster`）
> **拟批准触发**: 首个 change (mfc-cpu-pipeline-multi-cycle-fsm) archive 时同步

---

## Context

### 1.1 VexiiRiscv 5 病灶 #3 的 ChipForge 子病灶

VexiiRiscv 在 VexiiRiscv Introduction doc 中明确列出 5 条 VexRiscv 病灶，第 3 条原文：

> "The VexRiscv plugin system has hits some limits"

Dolu1990 把该子病灶拆为 3 个子项：

1. **服务查找靠类型猜**：`pluginOf[T]` 式隐式查找，依赖顺序由实例化次序隐式决定
2. **编译期无协商**：Plugin A 需要"流水线至少 3 级"这种约束没有表达通道，只能靠运行时崩溃
3. **错误处理无范式**：配置错误 = 抛异常/段错误，调用方无法区分"我的配置错了"还是"框架 bug"

VexiiRiscv 的修法是 **Fiber/Retainer 模式**：Plugin 在 `setup()` 阶段用类型化 key 声明 `provides` / `requires`，框架拓扑排序，缺依赖 → 编译期/链接期 fail-fast。

### 1.2 ChipForge 当前防御现状

| 子病灶 | ChipForge 已建防线 | 证据 |
|--------|--------------------|------|
| 隐式顺序 | **ADR-048**：Plugin 注册规范序 + `check_canonical_ordering()` 运行时断言 | `test_canonical_ordering.cpp` 4 测试 PASS |
| 编译期协商载体 | **ADR-037 v2.0**：elaboration 语义兑现，`pb.elaborate(ctx)` 一次性发射 DAG | `[elaborate]` 4 API PoC |
| 错误处理范式 | **ADR-047**：静态配置期 `std::expected<T, PluginError>` Result 范式 | `verify_plugin_decision.sh` Check 5 |
| 静默 cell 读 | PayloadStore fail-fast (v0.3.1 M6)：`get()` 缺失抛异常 | `check_plugin_portability.sh` Check 8 |

**唯一未解决真实缺口**：没有 **capability 协商 API**（VexiiRiscv Fiber/Retainer 的对应物）。当前若 `MulDivFsmPlugin` 需要 `flush_broadcaster`，只能通过 CtrlLink 的隐式订阅找到 `BranchPlugin`，无法静态验证依赖存在。

### 1.3 真实缺口的可观察证据（v0.10.0 PoC-1 触发）

`mfc-cpu-pipeline-multi-cycle-fsm` change 起草时遇到：
- ADR-046（FSM 豁免）要求 `MulDivFsmPlugin` 走 `ch_state_machine`，但 FSM 需要在 pipeline flush 时能同步清空状态
- 当前实现：FSM 内部硬编码"假设 BranchPlugin 已注册 CtrlLink flush publisher"
- 若用户配置 `MulDivFsmPlugin` 但未配置 `BranchPlugin`：运行时 fetch 到 mispredict 时 flush 广播不触发，FSM 状态错乱 → 难调试的 functional bug

**当前唯一防御**：依靠 `at_stage` 闭包运行期崩溃 + gdb。无 elaboration 期 fail-fast。

---

## Decision

### 1.4 引入 `Plugin::negotiate(CapabilityTable&)` 生命周期钩子

**新增 API**（在 `include/cf/plugin/plugin_base.h`）：

```cpp
class CapabilityTable {
public:
    // Plugin declares what it provides (typed key registry)
    template<typename T>
    void provide(cf::string_view key, T handle);

    // Plugin declares what it requires (returns nullptr if missing)
    template<typename T>
    T* require(cf::string_view key) const;

    // Enumerate all providers (for topological sort)
    auto providers() const -> std::vector<std::pair<string_view, typeid_t>>;
    auto requirements_unresolved() const -> std::vector<string_view>;
};

class PluginBase {
public:
    virtual ~PluginBase() = default;

    // Existing setup/build unchanged
    virtual void setup(PipeBuilder& pb) = 0;
    virtual void build(PipeBuilder& pb) = 0;

    // NEW: capability negotiation (called BEFORE build())
    virtual void negotiate(CapabilityTable& cap) { /* default: no-op */ }

    // NEW: post-negotiate validation hook (can throw MulDivResult-style)
    virtual std::expected<void, PluginError> validate() { return {}; }
};
```

### 1.5 `PipeBuilder::build()` 调用顺序

```
1. 注册所有 Plugin (existing)
2. 拓扑排序（基于 negotiate 输出的 requires 边）
3. for each Plugin in topo order:
     cap_table = new CapabilityTable
     plugin->negotiate(cap_table)   // registra provide/require
     if plugin->validate() returns err:
       throw PluginException(err)     // ADR-047 fail-fast
4. for each Plugin in topo order:
     plugin->build(pb)                // existing
```

### 1.6 三大实例契约

**契约 A — MulDivFsmPlugin (首个消费方)**：

```cpp
class MulDivFsmPlugin : public Plugin {
    void negotiate(CapabilityTable& cap) override {
        cap.provide<MultiCycleFsmHandle>("multi_cycle_fsm");
        cap.require<FlushBroadcasterHandle>("flush_broadcaster");
        cap.require<WritebackArbiterHandle>("writeback_arbiter");
    }

    void build(PipeBuilder& pb) override {
        auto* flush = cap_.require<FlushBroadcasterHandle>("flush_broadcaster");
        if (!flush) throw PluginException("missing flush_broadcaster");
        // wire flush subscription via CtrlLink
    }
};
```

**契约 B — BranchPlugin (首个 provider)**：

```cpp
class BranchPlugin : public Plugin {
    void negotiate(CapabilityTable& cap) override {
        cap.provide<FlushBroadcasterHandle>("flush_broadcaster");
        cap.provide<BranchPredictionHandle>("branch_prediction");
    }
};
```

**契约 C — HazardPlugin (另一个 provider)**：

```cpp
class HazardPlugin : public Plugin {
    void negotiate(CapabilityTable& cap) override {
        cap.provide<WritebackArbiterHandle>("writeback_arbiter");
        cap.provide<HazardDetectionHandle>("hazard_detection");
    }
};
```

**契约 D — HazardPlugin::clear_mmufault() capability (cpu-pipeline-mmufault-handler v1, 2026-10-06 follow-up)**：

cpu-pipeline-mmufault-handler v1 archive (commit `2d79a48`, archive `2026-10-06-cpu-pipeline-mmufault-handler`) 在 HazardPlugin TLM 版新增 `mark_mmufault() / clear_mmufault() / mmufault_pending()` API（private 字段 `bool mmufault_pending_`）。该 capability 在 ADR-082 v2 升级时需正式 declare：

```cpp
class HazardPlugin : public Plugin {
    void negotiate(CapabilityTable& cap) override {
        cap.provide<WritebackArbiterHandle>("writeback_arbiter");
        cap.provide<HazardDetectionHandle>("hazard_detection");
        // ADR-082 v2 follow-up: provide mmufault coordination capability
        cap.provide<MmufaultCoordinationHandle>("mmufault_coordination");
    }
};
```

MmuExceptionHandlerPlugin CH_MEM combinational 实装时 (mmu_chmem.h Phase 6d.6) consume `mmufault_coordination`，通过 CapabilityTable 拿 HazardPlugin handle 而非 dynamic_cast（CI 第 8 条门禁 grep `dynamic_cast` in `build()` 必须 = 0，per §1.7 约束 1）。v1 tasks.md 12.4 已记录此 follow-up，避免 future refactor 误删 HazardPlugin::clear_mmufault() 导致 MmuExceptionHandler CH_MEM 无 consumer 时 link fail。

### 1.7 行为约束

1. **强制走 negotiate 拿 handle**（禁止类型猜）：CI 第 8 条门禁 grep `dynamic_cast` in `build()` 必须 = 0
2. **缺依赖 → elaboration fail-fast**（ADR-047）：`build()` 顶部 `cap.require<...>(...) == nullptr` → throw `PluginException`
3. **拓扑排序结果 = Plugin 注册顺序的子集**（向后兼容）：若所有 Plugin 的 `requires` 都是 `provides` 超集，则排序 = 注册顺序（保留 ADR-048 行为）
4. **negotiate 默认空实现**（向后兼容）：旧 Plugin 不需要迁移，新增 `negotiate()` 即可获得 capability

---

## Consequences

### 正面

1. **Elaborate 期 fail-fast**：缺依赖 → elaboration throw，不再运行期难调试的 functional bug
2. **配置可证**：用户给一份 SoC JSON，能跑出 `PluginProviderMap` 列出所有 capabilities；debug 时一眼看清"哪个 capability 缺失"
3. **D4 约束完整化**：D4 的 9 项 CI 门禁加上本 ADR 共 10 项，覆盖 Plugin 范式全维度
4. **VexiiRiscv 对等能力**：ChipForge Plugin 机制能力 ≡ VexiiRiscv Fiber/Retainer（C++17 vs Scala macro 实现差异，机制等价）

### 负面

1. **Plugin 帧多了 `negotiate()` 方法**：每个 Plugin 必须重写该方法或用默认空实现（业务 Plugin 都需要更新；波及 ~11 个 Plugin）
2. **CapabilityTable 注册顺序敏感**：若两个 Plugin 都 provide 同名 capability，框架按注册顺序保留先注册的（后注册的 provide 被忽略）。ADR-048 扩展为"唯一性检查"
3. **拓扑排序死锁风险**：若 Plugin A requires B requires A → elaboration throw `cycle detected`（VexiiRiscv 同样处理）
4. **CI 第 8 条门禁误报风险**：业务代码可能 `dynamic_cast` 是历史遗留，必须迁移；短期落地会有"false positive"修改面

### 中性

1. **不破坏现有 build() 语义**：build() 调用前 negotiate() 已完成；业务 Plugin 的 build() 体改动最小
2. **不影响双模**：TLM 与 CH_MEM 共享同一 negotiate()，因为 capability 是 build() 前置，不在 elaboration 期表达

---

## Alternatives Considered

### 替代 A — 不做 negotiate，依赖 ADR-048 显式注册序

- **优点**：零工作量
- **缺点**：缺依赖 → 运行期崩溃，无法提前验证。否决原因：v0.10.0 PoC-1 的 MulDivFsmPlugin 强需求缺依赖 fail-fast（理由 #1.3 已证）

### 替代 B — 借鉴 SpinalHDL Fiber 风格用 Scala-style 宏实现

- **优点**：类型化更完整
- **缺点**：C++17 无 Scala 宏；用模板元编程实现 complexity 高 10×。否决原因：D4 框架目标是"业务代码简单"，Fiber-style 是 SpinalHDL Scala 生态独有武器，C++ 强模仿得不偿失

### 替代 C — 仅在 CI 加 `dynamic_cast` = 0 grep 门禁，不做 negotiate API

- **优点**：最小改动
- **缺点**：禁了类型猜但没提供合法渠道；Plugin 间仍然要靠隐式顺序。否决原因：门禁没有正反馈机制，业务 Plugin 不知道如何"正确沟通"

---

## Implementation Plan

### Phase A — 核心 API（`include/cf/plugin/plugin_base.h`）

- 定义 `CapabilityTable` 类（模板化 `provide<T>` / `require<T>`）
- 在 `PluginBase` 添加 virtual `negotiate()` 与 `validate()`
- 默认实现：negotiate 空，validate 返回 `ok()`

### Phase B — `PipeBuilder::build()` 编排

- 收集所有 Plugin 的 negotiate 输出（build 协议分离）
- 拓扑排序（基于 requires 边，失败 → throw `cycle detected`）
- validate 检查 + throw
- 然后按 topo order 调用 build()

### Phase C — 第一个消费 PoC（mfc-cpu-pipeline-multi-cycle-fsm）

- `MulDivFsmPlugin::negotiate()` 声明 requires
- `BranchPlugin::negotiate()` / `HazardPlugin::negotiate()` 声明 provides
- CI 第 8 条门禁上线（`build()` 内 `dynamic_cast` = 0）

### Phase D — 全 Plugin 迁移（v1.0.0 内）

- 11 个 TLM Plugin 全部添加 `negotiate()`（即使空实现）
- Plugin 文档更新"如何使用 negotiate"

---

## Validation & Verification

### 静态验证

- `tools/verify_plugin_decision.sh` 新增第 8 项：`grep -rn dynamic_cast include/cf/ src/cf_plugin/ ip/ | grep -v test_ | wc -l == 0`
- PR 阻塞

### 动态验证

- `[framework][negotiate]` family 新增测试：
  - `negotiate_test_basic`：两个 Plugin，require+provide 匹配 → 通过
  - `negotiate_test_missing_provider`：require 缺失 → throw
  - `negotiate_test_cycle`：A requires B, B requires A → throw
  - `negotiate_test_dynamic_cast_forbidden`：build() 内 dynamic_cast → CI 失败

### 端到端验证

- mfc-cpu-pipeline-multi-cycle-fsm archive 时，验证 MulDivFsmPlugin 走 negotiate 路径，`[cpu-integration][multi-cycle]` PASS

---

## Risks & Rollback

### 风险 R1 — CI 第 8 条门禁误伤历史 Plugin

- **信号**：门禁上线后 `grep` 命中 >20 处
- **缓解**：渐进迁移 —— 先把 11 个 Plugin 加 negotiate() 空实现，build() 体改造分 change 单独跟

### 风险 R2 — 拓扑排序复杂度导致大配置 elaborates 慢

- **信号**：SoC 含 ≥30 个 Plugin 时 `pb.elaborate()` 时间 >1 分钟
- **缓解**：增量式 negotiate（每个 Plugin 只声明直接 requires，不递归），排序 O(N+E) 而非 O(N²)

### 风险 R3 — CapabilityTable 模板膨胀导致二进制 size 增大

- **信号**：链接后 binary 增长 >20%
- **缓解**：用 typeid 索引 + void* handle，避免每个 capability 类型实例化

### 回退

- 若 Phase A/B 实施失败：保留 `negotiate()` virtual 方法但 `PipeBuilder::build()` 不调用，默认空实现等同于"无 negotiate"
- 若 CI 第 8 条门禁误伤 >50% Plugin：临时将该门禁改 `soft`（仅警告不阻塞），待迁移完成再升 `hard`
- 若 VexiiRiscv Fiber 真的胜出：D4 v2.1 框架评估（不早于 2029，理由 #3 信号 2 触发条件：CI 豁免 >20 处）

---

## Related

- **ADR-046**（已落地）：FSM 豁免 —— 提供 negotiate() 第一个真实需求（MulDivFsmPlugin requires flush_broadcaster）
- **ADR-047**（已落地）：Result 范式 —— 提供 `std::expected<void, PluginError>` 用于 validate() 返回
- **ADR-048**（已落地）：注册规范序 —— 提供向后兼容的排序基础
- **ADR-070**（v0.10.0 起草）：RVC 解码 —— 第二个 consume negotiate() 的 Plugin（2-phase fetch 需要声明 requires）
- **references/decision-1-plugin-evolution.md** §3（理由 #3 深度展开）：VexiiRiscv 病灶 #3 的 ChipForge 对策
# ADR-041：Bridge 适配层允许 `tick()` 模式（业务 Plugin vs Bridge 适配边界）

| 字段 | 值 |
|------|-----|
| 状态 | ✅ Accepted（2026-06-17, doc-code-realignment 实施） |
| 来源 | 本次会话 doc-code-realignment §5 桥接模式合法性整理（基于 L1CachePlugin ↔ CppTLM CacheTLM 桥接实施经验，2026-06-10 ~ 2026-06-17） |
| 决策 | 区分"业务 Plugin"（严格无 tick）与"Bridge 适配层"（允许 tick，但 body 仅协议转换 + 委托 pb.run()），明确两类组件的命名空间、目录布局和 D4 强制范围 |
| 关联 ADR | ADR-025（Plugin 基类无 tick）、ADR-037（Plugin 作为设计范式）、ADR-040（TLM→HDL 移植性约束，含 Tier-1 限制） |

---

## 1. 背景与动机

### 1.1 Phase 1.3 L1CachePlugin 桥接方案的实施经验

Phase 1.3（`ip/cache/tlm/L1CachePlugin.{h,cpp}` + `src/cf_plugin/bridge/L1CacheTLMBridge.{h,cpp}` + `L1CacheTLMBridgeAdapter.{h,cpp}`）的桥接方案（详见 `.omo/drafts/decision-phase-1.3-bridge-2026-06-10.md`）实现了如下架构：

```
+-- Business Logic: L1CachePlugin (cf::plugin::PluginBase)
|   - 无 tick()，业务逻辑在 pb.run() 内顺序执行
|   - 51/51 单元测试 PASS
|
+-- Bridge Adapter Layer: L1CacheTLMBridge (cpptlm::ChStreamModuleBase)
    +-- L1CacheTLMBridgeAdapter (桥接 StreamAdapter)
    - tick() 由 CppTLM EventQueue 周期调用
    - tick() body 内仅做协议转换 + 委托 plugin_->pb.run()
```

D4 决策（Plugin-style 强制）禁止业务 IP 实现 `tick()`。但 Bridge 适配层是 CppTLM 框架的 EventQueue 调度的对象，**必须**实现 `tick()` 才能接收周期回调。这造成表面矛盾：

- D4 说"禁止 tick"
- Bridge 类必须实现 `tick()`
- 一刀切禁用 tick 会让 Plugin 失去与 CppTLM 框架的桥接能力

### 1.2 不区分的代价

如果不显式区分"业务 Plugin"和"Bridge 适配"，会出现：

| 风险 | 后果 |
|------|------|
| 业务 IP 偷渡 tick() | D4 失去强制力，状态机/事件循环潜入业务代码 |
| Bridge 类被人当作"业务 IP" | Bridge 出现在 `ip/<name>/` 下，被 ADR-040 Tier-1 限制误伤（不能调 ch_mem/ch_reg 等） |
| 文档/新人混淆 | 新人看到 `L1CacheTLMBridge` 有 tick()，误以为 Plugin 也能加 tick() |

### 1.3 桥接模式合法性的 ADR 缺失

D4 决策时（Plugin 作为范式）只规定了"业务 Plugin 无 tick"，但没有显式 ADR 说明 Bridge 适配层允许 tick 的合法性边界，导致 2026-06-10 ~ 2026-06-17 期间多次出现"这违反 D4 吗？"的咨询。

---

## 2. 决策

### 2.1 引入两类组件边界

| 维度 | 业务 Plugin（严格） | Bridge 适配层（宽松） |
|------|--------------------|----------------------|
| **基类** | `cf::plugin::PluginBase` | `cpptlm::ChStreamModuleBase` 或 CppTLM 标准模板 |
| **tick()** | ❌ 禁止（D4 强制，`private: void tick() = delete;`） | ✅ 允许（必须由 EventQueue 调度） |
| **tick() body 约束** | N/A | 仅协议转换 + `plugin_->pb.run()` 委托，**不**允许业务状态变更 |
| **业务逻辑** | 在 `pb.run()` 内顺序执行 | 不直接执行业务，仅调用内层 Plugin |
| **位提取/算术** | ✅ 允许（Plugin 是业务代码） | ❌ Tier-1 限制（ADR-040）：不调 `ch_mem` / `ch_reg` / `ch_uint` 等 RTL 原语 |
| **典型位置** | `ip/<name>/tlm/<Name>Plugin.{h,cpp}` | `src/cf_plugin/bridge/<Name>TLMBridge{Adapter}.{h,cpp}` |
| **命名空间** | `cf::<feature>::` | `cf::bridge::` |
| **示例** | `L1CachePlugin`, `cpu/plugins/*` | `L1CacheTLMBridge`, `L1CacheTLMBridgeAdapter` |
| **D4 强制** | ✅ 受 D4 静态检查 | ❌ 不受 D4 强制（D4 仅针对 `cf::plugin::PluginBase` 派生） |

### 2.2 D4 强制范围限定

D4（Plugin-style 强制）的**检查目标**明确为 `cf::plugin::PluginBase` 的派生类：

- `cf::plugin::PluginBase` 的所有派生类在编译期必须 `private: void tick() = delete;`（由 PluginBase 基类提供）
- 任何 override `tick()` 的尝试在编译期失败（`= delete` 触发）
- **Bridge 适配层不继承 `cf::plugin::PluginBase`**，故不受 D4 强制

### 2.3 Bridge tick() body 严格约束

为防止 Bridge 偷渡业务逻辑，Bridge 的 `tick()` body 必须满足：

```cpp
// ✅ 允许
void L1CacheTLMBridge::tick() override {
    // 1. 协议转换（StreamAdapter 接收/发送 Packet）
    // 2. 转换为 Plugin 输入 Bundle
    // 3. 委托业务执行
    plugin_->pb.run();  // 唯一业务执行点
    // 4. 协议转换（输出 Bundle → StreamAdapter 发送）
}

// ❌ 禁止
void L1CacheTLMBridge::tick() override {
    if (hit) { /* 业务状态变更 */ }  // ← 业务逻辑应在 Plugin 内
    plugin_->pb.run();
}
```

### 2.4 命名空间与目录布局

| 组件类型 | 命名空间 | 目录 | CMake 目标 |
|----------|----------|------|------------|
| 业务 Plugin | `cf::<feature>::` | `ip/<name>/tlm/` | `cf_plugin_<name>` |
| Bridge 适配 | `cf::bridge::` | `src/cf_plugin/bridge/` | `cf_plugin_bridge` |
| CppTLM 标准模块 | `cpptlm::` | `CppTLM/include/tlm/` | 框架层（不受本 ADR 约束） |

**禁止** Bridge 类出现在 `ip/<name>/` 下（`ip/` 仅放业务 Plugin，由 D4 强制）。

---

## 3. 后果

### 3.1 正面后果

- ✅ D4 强制范围明确：业务 Plugin 受 D4，Bridge 不受 D4 但受 ADR-040 Tier-1 限制
- ✅ 新人能清晰区分两类组件的命名空间和目录
- ✅ 业务代码不与 CppTLM EventQueue 调度耦合（Plugin 通过 Bridge 适配）
- ✅ Bridge 复用价值高（同一个 Bridge 模板可桥接不同 Plugin 实例）
- ✅ 文档/代码一致性：本 ADR 明确 tick 模式合法性，消除 2026-06-10 ~ 2026-06-17 的咨询歧义

### 3.2 负面后果与缓解

| 负面后果 | 缓解 |
|----------|------|
| Bridge 引入额外间接层（性能开销） | Bridge 仅做协议转换 + 委托，无业务开销，TLM 仿真 < 5% 开销（实测 2026-06-10） |
| Bridge 目录 `src/cf_plugin/bridge/` 跨 IP 共享，命名冲突 | 命名规范 `<Name>TLMBridge{Adapter}`，如 `L1CacheTLMBridge`、`L1CacheTLMBridgeAdapter` |
| 新人可能误把 Bridge 写到 `ip/<name>/` | CI grep 脚本（`tools/verify_no_ghost_refs.sh` 不直接覆盖，但 ADR-040 Tier-1 静态检查覆盖） |

### 3.3 与其他 ADR 的关系

- **ADR-025（Plugin 基类无 tick）**：本 ADR 是 ADR-025 的"补充例外"——基类确实无 tick，但 Bridge 不继承基类，故无冲突
- **ADR-037（Plugin 作为范式）**：Plugin 范式继续成立，Bridge 是"Plugin 范式 + 框架适配"的组合
- **ADR-040（TLM→HDL 移植性约束）**：Bridge 适配层受 ADR-040 Tier-1 限制（不调 ch_mem/ch_reg 等 RTL 原语）

---

## 4. 参考

- `docs/architecture/adr.md` §2.1 决策表 ADR-041
- `.omo/drafts/decision-phase-1.3-bridge-2026-06-10.md`（Phase 1.3 桥接决策记录）
- `ip/cache/tlm/L1CachePlugin.h`（业务 Plugin 示例）
- `src/cf_plugin/bridge/L1CacheTLMBridge.h`（Bridge 适配示例）
- `src/cf_plugin/bridge/L1CacheTLMBridgeAdapter.h`（Bridge StreamAdapter 适配示例）
- ADR-025, ADR-037, ADR-040（关联 ADR）

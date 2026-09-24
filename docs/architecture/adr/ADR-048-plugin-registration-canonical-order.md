# ADR-048: Plugin Registration Canonical Order（插件注册规范序）

> **Status**: ✅ Accepted (v0.7.0, 2026-09-24)
> **类别**: Plugin / 顺序断言
> **关联 ADR**: ADR-040 v2.0（移植性） · ADR-045（CtrlLink Stall Loop） · ADR-037（Plugin 作为设计范式）
> **关联变更**: `openspec/changes/cpu-pipeline-canonical-ordering-assert/`

---

## Context

### 背景

`plugin-framework-stall` (v0.1.3) 落地了 `PipeBuilder::run()` CtrlLink stall loop，但 `cpu_factory.h::register_early_plugins()` 当前没有任何断言保证 `MMUPlugin::build()` 先于 `IBusPlugin::build()` 注册。

`pipe_builder.h::canonical_stage_order()` 是 stage 首现序（`pipe_builder.h:129-138`），由 `at_stage` 注册顺序决定。若 `IBusPlugin::build()` 先于 `MMUPlugin::build()`：

- `fetch` 排在 `tlb_lookup_ifetch` 前 → stall 晚 1 cycle 生效
- 缺失 cycle fetch 读 stale PADDR=0

当前正确性依赖 `CpuFactory` 调用顺序（`mmu-cache-integration` commit 6/9 把 `RiscvMMUPlugin` 放在注册表末尾）。未来重构 `CpuFactory` 或允许多 MMU 拓扑时会静默回归。

### 缺口

1. **隐式契约**：`[mmu]` 47 测试 + `[cpu-l1-mmu-demo]` 6 测试 + `[riscv-tests]` 30 测试当前全绿，只是因为 v0.2.2 fixed 4 hidden bug 后调用顺序恰好对了——不是显式契约。
2. **Wave 3 P1#3 阻塞**：`mmu-paddr-consume-and-real-memory` 的 IBus/DBus 真消费 `pl::PADDR` 需要 stall 链保护前提。

### 约束

- **无权侵入 Plugin 业务代码**：计数器不能加在各 Plugin::build() 体内（避免循环依赖 + layer 污染）。
- **零运行时开销**（断言通过后）：check 仅在 `build_cpu()` 退出前执行一次，不在热路径。
- **跨 TU 一致性**：计数器在 header 内用 `inline int`（C++17 inline variable），保证所有 TU 共享同一地址。

---

## Decision

**D1=A 变体（运行时计数器 + 异常断言）**：

### 机制

1. `ip/cpu/cpu_factory.h` 头部声明 4 个 `inline int` 计数器（`PLUGIN_SEQ`, `MMU_REG_ORDER`, `IBUS_REG_ORDER`, `DBUS_REG_ORDER`），位于 `cf::cpu::detail` 命名空间。
2. `reset_canonical_counters()` 在每个 `build_cpu()` 入口置零。
3. `MMUPlugin` 注册点：`MMU_REG_ORDER = ++PLUGIN_SEQ`
4. `IBusPlugin` 注册点：`IBUS_REG_ORDER = ++PLUGIN_SEQ`
5. `check_canonical_ordering()` 在 `pb->build()` 前校验：若 `MMU_REG_ORDER >= IBUS_REG_ORDER` 且 `MMU_REG_ORDER > 0`，则 `throw std::logic_error`。

### 与 D1=B（PipeBuilder 级校验）的比较

| 维度 | D1=A（选中） | D1=B |
|------|-------------|------|
| 范围 | CpuFactory 级 | PipeBuilder 框架级 |
| 侵入性 | cpu_factory.h 单文件 | pipe_builder.h + ABI 改变 |
| 运行时开销 | 一次检查 | 同 |
| 可扩展性 | 仅 cpu_factory 路径 | 所有 使用注册 |
| 复杂度 | 低（4 计数器 + 检查函数） | 高（register_order 字段） |

D1=A 被选中因为：最小侵入、compile-time 断言不可行（`inline int` 非 constexpr）、当前所有 MMU 使用路径均通过 `CpuFactory::build_cpu()`。

### 约束解决

- **无权侵入 Plugin 代码**：计数器在 cpu_factory.h 注册点递增，不在 Plugin::build() 内。
- **跨 TU 一致性**：`inline int` C++17 保证。
- **零运行时开销**：check_canonical_ordering() 在 build_cpu() 尾部调一次，不在热路径。

---

## Consequences

### 收益

1. **显式契约**：隐式调用顺序升级为运行时断言，阻塞静默回归。
2. **Wave 3 解锁**：P1#3 mmu-paddr-consume 的 IBus/DBus 真消费有 stall 链保护前提。
3. **未来重构保护**：多 MMU 拓扑（Phase 5+）或 CpuFactory 顺序调整时断言生效。
4. **工具链友好**：`std::logic_error` 可在测试中捕获（`REQUIRE_THROWS_AS`）或 gdb 跟踪。

### 成本

1. **每次 `build_cpu()` 多 1 次断言检查**（非热路径，成本可忽略）。
2. **计数器是全局变量**：需要 `reset_canonical_counters()` 在每个 `build_cpu()` 开头清除，防止跨测试用例污染。
3. **不覆盖 Framework 级校验**：绕开 `CpuFactory` 直接构造 `PipeBuilder` 的路径无保护。
4. **运行时检查**（非编译期）：若需编译期保护，需要未来 `PipeBuilder::register_plugin` 的 constexpr 化（D1=B 扩展）。

### 后续变更影响

1. `P1#3 mmu-paddr-consume-and-real-memory`：直接受益（canonical ordering 已保证）。
2. `P2#6 phase-1.5-wave-4`（CSR/exception/mispredict）：无 canonical ordering 风险。
3. **未来多 MMU 拓扑**：断言只校验 `MMU_REG_ORDER > 0` → `IBUS_REG_ORDER > MMU_REG_ORDER`，即至少一个 MMU 在 IBus 前。多个 MMU 可自然扩展。

---

## Implementation（已在 v0.7.0 落地）

### 变更文件

| 文件 | 变更类型 | 行数 |
|------|---------|------|
| `ip/cpu/cpu_factory.h` | 新增计数器 + check + MMU 移入 register_early_plugins | +56 LOC |
| `tests/cpu/integration/test_canonical_ordering.cpp` | 新建（4 test cases） | +72 LOC |
| `docs/architecture/adr/ADR-048-*.md` | 新建（本文档） | — |

### 关键实现细节

1. **计数器声明**（`ip/cpu/cpu_factory.h` 顶部）：
   ```cpp
   inline int PLUGIN_SEQ = 0;    // 全局注册序列
   inline int MMU_REG_ORDER = 0;  // MMUPlugin 的注册槽位（0=未注册）
   inline int IBUS_REG_ORDER = 0; // IBusPlugin 的注册槽位
   inline int DBUS_REG_ORDER = 0; // DBusPlugin 的注册槽位
   ```

2. **注册顺序调整**：MMUPlugin 的 `if (config.enable_mmu)` 块从 `build_cpu()` 尾部（旧位置 `mmu-cache-integration` commit 6/9）移入 `register_early_plugins()` 头部，先于 `IBusPlugin` 注册。所有 MMU 配置（SvMode 枚举映射、TLB 几何向量）随移至新位置。

3. **Check 时机**：校验点在 `pb->build()` 之前，map + lane dispatch 之后。

### CI 门禁

- `verify_adr.sh`：新增 ADR-048 行
- `verify_plugin_decision.sh`：无变更（D4 不变）
- `check_plugin_portability.sh`：无变更（无新检查项）

---

## Risk

| 风险 | 描述 | 应对 |
|------|------|------|
| R1 | `inline int` 跨 TU 不共享 | C++17 保证；与 `register_plugin()` 在 header 内而非 .cpp 内一致 |
| R2 | 多 MMU 拓扑时断言误报 | 断言只校验 `> 0 → < IBUS`，多个 MMU 任意一个在 IBus 前即可通过 |
| R3 | 计数器跨测试用例污染 | `reset_canonical_counters()` 在每个 `build_cpu()` 入口清除 |
| R4 | 绕开 CpuFactory 的路径无保护 | 当前所有 MMU 使用路径均通过 `CpuFactory::build_cpu()` |
| R5 | `std::logic_error` 被 `build_cpu_impl` catch 链吃掉 | 已在 `build_cpu_impl()` try-catch 中验证：`std::logic_error` → `PluginError::BuildFailed`（但 check 在 `build_cpu_impl` 内委托 build_cpu → 异常被 catch）→ **需注意**：check 的 `std::logic_error` 不应被 catch 吞掉 |


## 后续项（P1#3 规划）

**MMU 配置 JSON 驱动**：
- 当前 `register_early_plugins()` 中硬编码 TLB 几何（`{"L0", 8, 8, 1, 1, "LRU"}` 等）和 `SvMode` 枚举映射
- `ip/mmu/configs/params_schema.json` 已存在，但未被 `CPUConfig` / MMU 注册点反化
- 目标：P1#3 `mmu-paddr-consume-and-real-memory` 时改为 JSON 配置驱动，从 `soc/*.json` 读取 TLB 层级大小和替换策略

---

## 关联文档

- **变更工作区**: `openspec/changes/cpu-pipeline-canonical-ordering-assert/`
- **ADR-040 v2.0**: TLM→HDL 移植性约束（三级约束模型 + array_store 抽象）
- **ADR-045**: Plugin CtrlLink 消费契约 + PipeBuilder::run() Stall Loop
- **策略**: `docs/roadmap/strategy/a-plus-c-hybrid.md` §6.1 P0#1 条目
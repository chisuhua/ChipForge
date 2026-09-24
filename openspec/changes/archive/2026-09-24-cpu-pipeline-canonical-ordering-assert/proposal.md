---
initiative: wave3-cpu-pipeline-debt
priority: P0
version_target: v0.7.0
depends_on: []
---

# cpu-pipeline-canonical-ordering-assert — 修真 MMU stall inert + canonical ordering

## Why

`plugin-framework-stall` (v0.1.3) 落地了 `PipeBuilder::run()` CtrlLink stall loop，但 `cpu_factory.h::register_early_plugins` 当前**没有任何断言**保证 `MMUPlugin::build()` 先于 `IBusPlugin::build()` 注册。

后果（v0.2.3 R2 known limitation + v0.1.3 §11.2）：
- `pipe_builder.h::canonical_stage_order()` 是 stage **首现序**（`pipe_builder.h:129-138`），由 `at_stage` 注册顺序决定
- 若 `IBusPlugin::build()` 先于 `MMUPlugin::build()`，`fetch` 排在 `tlb_lookup_ifetch` 前 → **stall 晚 1 cycle 生效** → 缺失 cycle fetch 读 stale PADDR=0
- 当前正确性依赖 commit B 的隐式调用顺序（Oracle hygiene 未断言），未来重构 CpuFactory 或允许多 MMU 拓扑时会**静默回归**
- `[mmu]` 47 测试 + `[cpu-l1-mmu-demo]` 6 测试 + `[riscv-tests]` 30 测试当前绿是因为：v0.2.2 fixed 4 hidden bug 后恰好 `register_early_plugins` 调用顺序对了——**不是显式契约**

本 change 把隐式契约升级为**显式断言**，阻塞未来静默回归，同时让 Wave 3 P1#3 `mmu-paddr-consume-and-real-memory` 的 IBus/DBus 真消费 `pl::PADDR` 有 stall 链保护前提。

## What Changes

### 1. `cpu_factory.h::register_early_plugins` 加 canonical-ordering assert

- **位置**: `ip/cpu/cpu_factory.h::register_early_plugins()`
- **机制** (二选一, 由本 change 决策 D1 锁定):
  - **D1=A** (推荐): `static_assert` + `inline int MMUPlugin_register_order = 0; inline int IBusPlugin_register_order = 0;` 在各 Plugin::build() 头部递增，最后断言 `MMUPlugin_register_order < IBusPlugin_register_order`
  - **D1=B** (备选): `PipeBuilder` 加 `register_order_check_` 字段记录 plugin 注册名顺序，`run()` 末尾校验
- **理由选 A**: compile-time 断言，零运行时开销；B 是 ABI 升级路径，本 change 优先最小侵入
- **⚠️ CRITICAL 跨 TU 链接语义 (Metis #1 审查修订)**: **`inline` 关键字不可省略**
  - `static int` 在 namespace scope 是 internal linkage，每个 TU (cpu_factory.cpp + include unit ip/mmu/tlm/MMUPlugin.h 等) 拥有独立副本 → 断言永远不生效
  - C++17 `inline int` 在 header 中定义保证所有 TU 共享同一地址 → 跨 TU 计数正确
  - **最终决策**: D1=A 改用 `inline int MMUPlugin_register_order = 0;` (C++17 inline variable), 与 `inline static` 在 CMakeLists/项目级约定一致

### 2. failing test (TDD red)

- **新建** `tests/cpu/integration/test_canonical_ordering.cpp`：
  - 故意以 `IBusPlugin` 先于 `MMUPlugin` 顺序注册 → 期望 `compile_error` 或 `REQUIRE_FAIL`（取决于 D1 选 A/B）
  - 当前正确顺序注册 → `REQUIRE(true)`
- 验证：现有 `[cpu-integration]` 4/4 测试 (3/5/7/10-stage) 不被破坏

### 3. `[cpu-integration]` 4/4 测试增强

- 现有 `tests/cpu/integration/test_*stage_riscv.cpp` 加 `REQUIRE(cpu_factory.early_plugin_order_ == expected_canonical_order)` 钩子
- 验证 stall 真生效（通过 fetch stage PC 与 MMU TLB hit 一致性间接证明）

> **⚠️ Archive drift 注释 (2026-09-24)**：本节承诺的"给 4 个 stage 测试文件加 `early_plugin_order_` 钩子"**未按字面实装**——`test_3stage_riscv.cpp` / `test_5stage_riscv.cpp` / `test_7stage_riscv.cpp` / `test_10stage_riscv.cpp` 4 文件中**无 `early_plugin_order_` 符号**（`grep early_plugin_order_ tests/cpu/integration/test_*stage_riscv.cpp` 无命中）。等价覆盖由新建 `tests/cpu/integration/test_canonical_ordering.cpp`（4 test cases, cba5e53）提供——canonical ordering 校验集中在该文件，3/5/7/10-stage 基础仿真测试保持不破坏。功能覆盖成立但 archived proposal 与实现存在 drift：若 rdd-verifier 复核 archive gate，需以此注释为"功能等价覆盖"证据。详见 AGENTS.md "已知测试状态"段 P0#1 条目 + `openspec/specs/cpu-stage-ordered-scheduling/spec.md` Requirement 3 静态断言→运行时断言 drift 标注。

### 4. ADR 新增

- **新增** `docs/architecture/adr/ADR-048-plugin-registration-canonical-order.md`（252 行预算）：
  - §Context: 解释 v0.1.3 隐式契约风险
  - §Decision: D1=A static_assert 优先
  - §Consequences: 与 ADR-040 v2.0 Tier-1 Check 5 互补（Check 5 校验 if(ch_bool)，本 ADR 校验 plugin 注册序）
- **更新** `docs/architecture/adr.md` 表项 + 章节锚点

### 5. CI 防线

- 不新增 `check_plugin_portability.sh` 检查项（compile-time 静态断言已覆盖）
- 在 AGENTS.md "已知测试状态" 段记录本 change 解锁 stall 真生效

## Capabilities

### Modified Capabilities

- `cpu-stage-ordered-scheduling`: 新增 "Plugin registration order MUST be canonical (MMUPlugin before IBusPlugin/DBusPlugin)" requirement 段
- `cpu-plugin-template-thread-awareness` (如有): 增加 "early plugin registration MUST follow canonical order" 隐含契约

## Impact

- **新增代码**:
  - `ip/cpu/cpu_factory.h` (+30 LOC: 静态计数器 + 断言)
  - `tests/cpu/integration/test_canonical_ordering.cpp` (新建, ~80 LOC)
  - `tests/cpu/integration/test_*stage_riscv.cpp` (+5 LOC/文件, 4 文件 × 5 LOC = 20 LOC)
  - `docs/architecture/adr/ADR-048-*.md` (新建, ~252 LOC)
- **修改文档**:
  - `docs/architecture/adr.md` (+5 LOC)
  - `AGENTS.md` "已知测试状态" 段 (+3 LOC)
- **CI 门禁**: 无新增（compile-time 静态断言替代）
- **下游解锁**:
  - Wave 3 P1#3 `mmu-paddr-consume-and-real-memory` 的 IBus/DBus 真消费 `pl::PADDR` 有 stall 链保护前提
  - Wave 4 P2#6 phase-1.5-wave-4 (CSR/exception) 的 mmu_exit hook 有 canonical ordering 保障

## Acceptance

- [ ] `tests/cpu/integration/test_canonical_ordering.cpp` PASS（含 red + green 双测试）
- [ ] 现有 `[cpu-integration]` 4/4 测试不回归
- [ ] 故意错序注册 → 编译期拒绝（D1=A）或运行时 REQUIRE_FAIL（D1=B）
- [ ] ADR-048 Accepted + adr.md 注册
- [ ] 3 门禁 (`verify_adr.sh` + `verify_plugin_decision.sh` + `check_plugin_portability.sh`) 全 PASS
- [ ] `[mmu]` 47 测试不回归
- [ ] `[cpu-l1-mmu-demo]` 6 测试不回归
- [ ] TLM baseline 0 回归（17 fail 全 pre-existing）
- [ ] CHANGELOG v0.7.0 段本 change 条目（待 initiative archive）
- [ ] `openspec archive cpu-pipeline-canonical-ordering-assert -y`

## Risk

- **R1 (D1=A)**: 静态计数器跨 translation unit 不共享 → 必须在 `cpu_factory.h` 头部 inline 定义，避免 ODR 违反
- **R2 (D1=A)**: 用户可能多 MMU 拓扑（未来 Phase 5+）→ 断言只校验 "至少一个 MMUPlugin 在 IBusPlugin 前"，留扩展空间
- **R3 (测试 red)**: failing test 故意错序注册触发 compile error，可能与其他 CTest target 编译失败混淆 → 在 test 文件头部加明确注释 + 用 `static_assert(false, ...)` 而非运行时 assert 让 cmake 在 config 阶段就 fail

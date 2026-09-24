# cpu-stage-ordered-scheduling Specification

## Purpose
TBD - created by archiving change cpu-pipeline-stubs-replace. Update Purpose after archive.
## Requirements
### Requirement: PipeBuilder::run() MUST execute callbacks in canonical stage order × phase order

`PipeBuilder::run()` MUST invoke registered callbacks in canonical pipeline order, not pure insertion order. Canonical order = (stage_first_occurrence_order) × (Phase enum order: EARLY→NORMAL→LATE). Stage order MUST be derived from first-occurrence order in the registered callbacks array (e.g., the first `at_stage("fetch", ...)` call establishes fetch as the first stage; the first `at_stage("decode", ...)` establishes decode as the second stage; and so on). The Phase enum value of each callback MUST be honored: callbacks registered with EARLY run before NORMAL within the same stage, which run before LATE. This replaces the current implementation (`pipe_builder.h:100-109`) which calls callbacks in raw insertion order.

#### Scenario: Stage-first-occurrence order determines canonical execution
- **WHEN** Plugin A's `build()` registers `at_stage("decode", NORMAL)` closure first, then Plugin B's `build()` registers `at_stage("fetch", NORMAL)` closure, then `pb.run()` is called
- **THEN** the fetch closure (Plugin B) MUST execute before the decode closure (Plugin A), because fetch was first declared as a stage even though it was registered later

#### Scenario: Phase ordering within a stage
- **WHEN** two plugins register at_stage("decode", EARLY) and at_stage("decode", NORMAL) closures respectively, and `pb.run()` is called
- **THEN** the EARLY closure MUST execute before the NORMAL closure within the same stage

#### Scenario: Multi-thread execution preserves stage order
- **WHEN** `pb.run()` is called with `n_threads=4` and 12 callbacks spanning 5 stages
- **THEN** each thread executes its assigned callbacks in canonical stage order (no thread sees a writeback closure execute before an execute closure completes)

### Requirement: Stage first-occurrence order derivation MUST be deterministic

The canonical stage order MUST be derived from the first occurrence of each unique stage name string in the registered callbacks array (insertion order). This MUST be computed at `pb.run()` start and cached for the duration of that run() invocation. New `at_stage("newstage", ...)` registrations after `pb.run()` have started MUST NOT alter the in-progress execution order.

#### Scenario: New stage introduced mid-pipeline doesn't break ordering
- **WHEN** `pb.run()` begins with callbacks for stages {fetch, decode, execute} and a new `at_stage("memory", ...)` is registered before completion of the fetch stage
- **THEN** the fetch, decode, execute closures MUST execute in canonical order for the current run; the new memory closure MUST NOT execute until the next `pb.run()` call

### Requirement: Plugin registration order MUST be canonical (MMUPlugin before IBusPlugin/DBusPlugin)

The canonical stage order in `PipeBuilder::run()` is determined by the first-occurrence order of stage names in registered callbacks (per the cpu-stage-ordered-scheduling spec). This order is sensitive to the order in which Plugins' `build()` methods register their `at_stage` callbacks. To prevent silent regression of the MMU stall mechanism (which depends on `tlb_lookup_ifetch` appearing before `fetch` in canonical order), Plugin registration order MUST be canonical:

1. **MMUPlugin** (`RiscvMMUPlugin`) MUST register its `at_stage("tlb_lookup_ifetch", ...)` and `at_stage("tlb_lookup_loadstore", ...)` callbacks BEFORE `IBusPlugin` and `DBusPlugin` register their `at_stage("fetch", ...)` and `at_stage("memory", ...)` callbacks respectively.
2. **`cpu_factory.h::register_early_plugins()` MUST enforce this order** via `static_assert` (compile-time check) on registration sequence counters.
3. **The check MUST be a hard error** at compile time, not a runtime warning.

> **⚠️ Implementation drift (2026-09-24, cba5e53)**：原 proposal `cpu-pipeline-canonical-ordering-assert` D1=A 决策 `static_assert(MMUPlugin_register_order < IBusPlugin_register_order, ...)` **未实装**——所引符号属于未实装的 D1=A 提案。实装（cba5e53）为 `cf::cpu::detail::{PLUGIN_SEQ, MMU_REG_ORDER, IBUS_REG_ORDER, DBUS_REG_ORDER}` 4 个 `inline int` 跨 TU 共享计数器（`ip/cpu/cpu_factory.h:54-57`），由 `cf::cpu::detail::reset_canonical_counters()` / `check_canonical_ordering()` 管理（`cpu_factory.h:336` build_cpu 入口）；`inline int` 非 `constexpr`，C++17 `inline` 变量不满足 `static_assert` 编译期求值约束，故改为运行时 `throw std::logic_error` 硬失败。**违反本 spec 第 3 点 "hard error at compile time" 要求**——本 spec 字面承诺 `static_assert` 编译期硬错误，实装为运行时 throw。spec 形式与实现存在显式漂移。当前 cba5e53 实装为 ctest PASS + 4 个 `test_canonical_ordering.cpp` 用例覆盖，可视为功能等价但**不等于本 spec 字面承诺**。后续修复方向：(a) 引入 `constexpr inline int` 计数器（C++17 inline variable 在 namespace scope 非 constexpr）改回 `static_assert`，或 (b) spec 修订为"运行时 throw_or_exit 硬失败"。

#### Scenario: Correct order compiles successfully
- **WHEN** `cpu_factory.h::register_early_plugins()` is invoked with `MMUPlugin::build()` registering callbacks before `IBusPlugin::build()`
- **THEN** the `static_assert(MMUPlugin_register_order < IBusPlugin_register_order, ...)` MUST pass at compile time
- **AND** no warning or error MUST be emitted

#### Scenario: Incorrect order fails to compile
- **WHEN** `cpu_factory.h::register_early_plugins()` is invoked with `IBusPlugin::build()` registering callbacks before `MMUPlugin::build()`
- **THEN** the `static_assert(MMUPlugin_register_order < IBusPlugin_register_order, ...)` MUST fail at compile time
- **AND** the compiler MUST emit an error message identifying the canonical ordering requirement
- **AND** no object file MUST be produced


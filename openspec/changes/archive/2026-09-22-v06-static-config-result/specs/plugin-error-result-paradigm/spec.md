# plugin-error-result-paradigm

> **Capability**: cf::plugin 框架静态配置 API 错误传播范式（Result 类型 + PluginError 枚举）
> **OpenSpec change**: `v06-static-config-result`
> **关联 ADR**: ADR-047（新增）
> **版本**: v0.6.0
>
> **Metis 修订 (2026-09-22)**：迁移列表 12 API → **10 API**（移除 `node_of_logic_stage`/`get_ctrl_link`，避免 ~185 处业务 plugin 连锁影响）。

## ADDED Requirements

### Requirement: PluginError 枚举定义

The system MUST provide a `cf::plugin::PluginError` enum class with at least the following fields for parameter validation and configuration errors:
- `NullPlugin`, `NullCallback`, `NullCommitHook`, `NullCtrlLink`, `NullConnector`
- `EmptyStageName`, `EmptyParentStageName`
- `NullContextPtr`, `CtxNotProvided`, `AlreadyElaborated`
- `DuplicatePlugin`, `DuplicateStageName`, `UnknownStage`, `StageNotFound`
- `BuildFailed`, `PluginBuildFailed`, `ElaborationFailed`, `ConnectorFailed`
- `UnsupportedPipelineDepth`, `UnsupportedMulLatency`

#### Scenario: PluginError 枚举被引用时类型安全

- **WHEN** 业务代码拼写错误（如 `PluginError::NULL_PLUGIN` 而非 `PluginError::NullPlugin`）
- **THEN** 编译器 MUST 拒绝并报错

#### Scenario: PluginError 转换为可读消息

- **WHEN** 调用 `plugin_error_message(PluginError::EmptyStageName, "at_stage")` 时
- **THEN** MUST 返回包含错误描述与 detail 的字符串（如 `"empty stage name (at_stage)"`）

### Requirement: Result 类型别名

The system MUST provide `cf::plugin::Result<T>` as an alias for `std::expected<T, cf::plugin::PluginError>`.

#### Scenario: Result<void> 用于 void 替代

- **WHEN** PipeBuilder 静态配置 API 改签名时
- **THEN** MUST 使用 `Result<void>`（即 `std::expected<void, PluginError>`）作为返回类型

#### Scenario: Result<T> 用于值返回

- **WHEN** PipeBuilder `create_simulator()` 返回值类型时
- **THEN** MUST 使用 `Result<std::unique_ptr<ch::Simulator>>`（携带错误信息）

### Requirement: 静态配置 API 签名

The system MUST migrate the following **10** PipeBuilder APIs from `void` (with internal throw) to `Result<>` return types:

`register_plugin`, `at_stage`, `declare_substage`, `register_commit_hook`, `register_ctrl_link`, `register_stage_payload_connector`, `build`, `elaborate`, `to_verilog`, `create_simulator`

#### Scenario: 排除查询型 API 不迁移

- **WHEN** 评估是否迁移 `node_of_logic_stage` 与 `get_ctrl_link` 时
- **THEN** MUST 保留原签名（`shared_ptr<T>` 或 `nullptr` 返回）；不在本 change scope；理由：~185 处业务 plugin 调用点零影响

#### Scenario: register_plugin 接受 null 参数

- **WHEN** 调用 `pb.register_plugin(nullptr)`
- **THEN** MUST 返回 `std::unexpected(PluginError::NullPlugin)`（而非 throw）

#### Scenario: at_stage 接受空 stage 名

- **WHEN** 调用 `pb.at_stage("", Phase::NORMAL, callback)`
- **THEN** MUST 返回 `std::unexpected(PluginError::EmptyStageName)`

#### Scenario: register_ctrl_link 接受 null

- **WHEN** 调用 `pb.register_ctrl_link("stage_a", nullptr)`
- **THEN** MUST 返回 `std::unexpected(PluginError::NullCtrlLink)`

#### Scenario: elaborate 在 ctx 为空时

- **WHEN** 调用 `pb.elaborate()` 但 `ctx_` 未设置
- **THEN** MUST 返回 `std::unexpected(PluginError::NullContextPtr)`

### Requirement: PluginException 兼容性

The system MUST keep `cf::plugin::PluginException` (derived from `std::runtime_error`) backward compatible:
- Existing 38-line implementation MUST be preserved
- A new constructor `PluginException(PluginError err, std::string stage = "")` MUST be added
- The `runtime_error(message)` constructor path MUST be preserved for callers using catch (std::runtime_error)

#### Scenario: 旧 catch 代码继续工作

- **WHEN** 旧业务代码写 `catch (std::runtime_error& e)`
- **THEN** MUST 仍然能捕获从框架 `build_cpu()` 包装层抛出的 `PluginException`

#### Scenario: 新增 PluginError 构造重载

- **WHEN** 调用 `PluginException(PluginError::EmptyStageName, "at_stage")`
- **THEN** MUST 生成携带 `stage_name() == "at_stage"` 与 std::runtime_error `what()` 消息的异常对象

### Requirement: PB_TRY 内部传播宏

The system MUST provide `PB_TRY(expr)` macro that early-returns the error if the expression yields a non-`ok` `Result`.

#### Scenario: PB_TRY 失败时 early-return

- **WHEN** `PB_TRY(pb->register_plugin(p))` 在 `register_plugin` 返回 `std::unexpected(...)` 时
- **THEN** MUST 从当前函数 `return std::unexpected(_r.error())`

#### Scenario: PB_TRY 成功时继续

- **WHEN** `PB_TRY(pb->register_plugin(p))` 在 `register_plugin` 返回成功时
- **THEN** MUST 继续执行后续语句

### Requirement: auto_throw 业务抹平工具

The system MUST provide `auto_throw<T>(Result<T>)` that throws `PluginException` on error and unwraps value on success.

#### Scenario: auto_throw 在 CpuFactory 包装层使用

- **WHEN** `CpuFactory::build_cpu()` 调用 `auto_throw(build_cpu_impl(...))` 且 `build_cpu_impl` 返回 error
- **THEN** MUST 抛出 `PluginException`，保留原 `PluginError` 信息

### Requirement: 热路径例外清单

The following APIs MUST NOT be migrated to Result (保留 `throw` 语义):
- `PipeBuilder::run()` — CtrlLink `throw_when` 运行时语义
- `PayloadStore::get()` (const) — at_stage 回调内热路径
- `PayloadStore::get()` (mutable) — 同上
- `PayloadStore::put()` — 同上
- `PluginBase::setup()` / `PluginBase::build()` 虚函数 — 回调体内部业务代码可自由选择

#### Scenario: run() 抛 PluginException 不变

- **WHEN** 在 `pb.run()` 期间 CtrlLink `should_throw()` 触发
- **THEN** MUST 抛 `PluginException(stage_name, "throw_condition_triggered")`（与 v0.5.0 行为一致）

#### Scenario: PayloadStore::get miss fail-fast 不变

- **WHEN** `PayloadStore::get(key)` 在 cell 缺失时
- **THEN** MUST 抛 `std::runtime_error`（CH_MEM 模式 fail-fast）

### Requirement: C++23 标准强制

The system MUST compile under `-std=c++23` for:
- 主项目 ChipForge (`CMakeLists.txt`)
- `cf_plugin` INTERFACE library (`src/cf_plugin/CMakeLists.txt`)
- 两个 test targets (`tests/CMakeLists.txt`)
- Verilator sim runner (`tools/verilator_runner/CMakeLists.txt`，Metis H5 修订)

#### Scenario: 主项目默认标准

- **WHEN** 配置 CMake 时
- **THEN** `set(CMAKE_CXX_STANDARD 23)` MUST 在 `CMakeLists.txt` L24 设置

#### Scenario: cf_plugin INTERFACE 库编译特性

- **WHEN** `cf_plugin` 被 link 进 `chipforge_tests` 或 `chipforge_tests_chmem`
- **THEN** MUST 传播 `cxx_std_23` 编译特性

#### Scenario: Verilator runner C++23 一致

- **WHEN** `cpu_verilator_sim` target 编译时
- **THEN** MUST 使用 `cxx_std_23`（与主项目一致；Check 12 兜底）

### Requirement: CI 门禁强制

The system MUST enforce 3 new checks in `tools/check_plugin_portability.sh`:

**Check 10**: 禁止静态配置头文件 `throw std::*/PluginException`（exceptions: `payload.h`, `ctrl_link.h`）
**Check 11**: P0 API 签名同步（**10 个** API 必须包含 `Result` 或 `expected`，不能还是 `void`；Metis H1 修订）
**Check 12**: C++23 编译标志强制（**4 个** CMake 文件不能出现 `cxx_std_(17|20)`：主 CMakeLists.txt + src/cf_plugin/CMakeLists.txt + tests/CMakeLists.txt + tools/verilator_runner/CMakeLists.txt；Metis H5 修订）

#### Scenario: 新增 throw 被 Check 10 拒绝

- **WHEN** 有人在 `pipe_builder.h` 添加 `throw std::runtime_error(...)`（除 throw_when/should_throw）
- **THEN** `bash tools/check_plugin_portability.sh` MUST 返回非零退出码

#### Scenario: API 回退到 void 被 Check 11 拒绝

- **WHEN** 有人把 `register_plugin` 改回 `void`
- **THEN** Check 11 MUST 报错并阻止 PR 合并

#### Scenario: C++ 标准回退被 Check 12 拒绝

- **WHEN** 有人把 `cxx_std_23` 改回 `cxx_std_20`
- **THEN** Check 12 MUST 报错并阻止 PR 合并

### Requirement: ADR-047 文档化

The system MUST create `docs/architecture/adr/ADR-047-static-config-result-paradigm.md` documenting:
- Why: P1 缺口定义（CH_MEM elaboration 路径不可重入 + SoC 调试缺失效传播链）
- Decision: **10** 静态配置 API 改为 Result + PluginError（Metis H1 修订）
- Exceptions: 4 项热路径例外清单 + 2 项查询型 API 排除说明
- Migration: P0 (6h) + P1 (7h) + P2 (8h)（Metis 修订：P0 8h→6h）

#### Scenario: ADR-047 被 verify_adr.sh 验证

- **WHEN** 运行 `bash tools/verify_adr.sh`
- **THEN** MUST 检测 ADR-047 文件存在并标记为 `✅ PASS`（实际 PASS 计数取决于 verify_adr.sh 当前覆盖 ADR 数量；Metis H4 修订：不是 "32/32"，需在 P1 实施时核对实际数字）

### Requirement: 现有 ADR 同步

The system MUST supplement the following existing ADRs with a note about Result-paradigm path:
- ADR-026 (at_stage)
- ADR-031 (CtrlLink) — `register_ctrl_link()` 返回 Result
- ADR-032 (PipeBuilder) — 整段扩充 Result 范式决策
- ADR-040 v3.0（行末加 ADR-047 引用）
- ~~ADR-030 (PipeNode)~~ — Metis H1 修订：因 `node_of_logic_stage` 不迁移，ADR-030 不需补丁

#### Scenario: ADR-032 包含 Result 路径说明

- **WHEN** 阅读 ADR-032 (PipeBuilder)
- **THEN** MUST 在文档中包含"`register_plugin` / `at_stage` / `build` / `elaborate` 等 API 返回 `Result<void>`"的描述

### Requirement: CpuFactory 内部 Result 化 + 对外兼容

The system MUST refactor `CpuFactory::build_cpu()` to:
1. Internal implementation returns `Result<std::unique_ptr<PipeBuilder>>`
2. All `register_plugin` / `at_stage` / `build` / `elaborate` calls wrapped in `PB_TRY`
3. External `build_cpu()` retains `throw PluginException` wrapper for backward compatibility with `cpu_sim/main.cpp`

#### Scenario: build_cpu 包装层抛 PluginException

- **WHEN** 旧 CLI 入口 `cpu_sim/main.cpp` 调用 `CpuFactory::build_cpu(config, mem)`
- **THEN** MUST 行为与 v0.5.0 一致（成功返回 `unique_ptr<PipeBuilder>`，失败抛 `PluginException`）

#### Scenario: build_cpu_impl 返回 Result

- **WHEN** 内部调用链任一环节失败（如重复 stage 名）
- **THEN** `build_cpu_impl` MUST 返回 `std::unexpected(PluginError::Xxx)`，不抛出异常

### Requirement: 文档同步

The system MUST update:
- `docs/architecture/error-handling.md` §9 — 新增"cf::plugin Result 范式"
- `docs/methodology/plugin-style-design-methodology-v1.md` — 补充 Result 范式到方法学约束
- `CHANGELOG.md` — v0.6.0 条目

#### Scenario: error-handling.md §9 存在

- **WHEN** 阅读 `docs/architecture/error-handling.md`
- **THEN** MUST 包含 §9 "cf::plugin Result 范式" 小节，描述 PluginError + Result 别名 + PB_TRY 用法

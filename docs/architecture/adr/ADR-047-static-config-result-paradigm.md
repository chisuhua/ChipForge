# ADR-047: 静态配置期错误处理 Result 范式

> **Status**: ✅ Accepted (v0.6.0, 2026-09-22)
> **类别**: Plugin / 错误传播
> **关联 ADR**: ADR-026 (at_stage) · ADR-031 (CtrlLink) · ADR-032 (PipeBuilder) · ADR-040 v3.0 (移植性) · ADR-046 (FSM 豁免)
> **关联变更**: `openspec/changes/v06-static-config-result/`
> **Metis 修订**: 12 API → **10 API** (移除 `node_of_logic_stage`/`get_ctrl_link`，避免 ~185 处业务 plugin 连锁影响)

---

## Context

### 背景

ChipForge v0.5.0 (Phase 6d 收官) 的 cf::plugin 框架静态配置 API 全部 `void` + `throw`：

| 文件 | throw 次数 | 类型 |
|------|----------|------|
| `include/cf/plugin/pipe_builder.h` | 8 | `std::invalid_argument` |
| `include/cf/plugin/pipe_builder.h` | 3 | `PluginException` |
| `include/cf/plugin/payload.h` | 6 | `std::runtime_error` (CH_MEM fail-fast) |
| `include/cpu/cpu_factory.h` | 2 | `std::invalid_argument` |

### 缺口

1. **CH_MEM elaboration 路径阻塞**：throw 无法穿透 lnode DAG 边界，配置错误必须以值类型传播。
2. **SoC 调试缺失效传播链**：故障定位只能依赖 gdb 跟踪，无法在 at_stage 闭包内部 catch 框架错误。
3. **CppHDL 子仓库 267 处 `CHERROR/assert`**：与 cf::plugin 的"throw + 运行时 catch"模式不一致。

### 约束

- **C++23**：`std::expected<T, E>` 是 C++23 标准库，GCC 13.3 + `-std=c++23` 已实测支持。
- **CppHDL 已是 C++23**，主项目仍是 C++17 已成技术债。
- **CppTLM (C++17)**：通过 Itanium C++ ABI 兼容层与 C++23 父项目共存（实测 `_GLIBCXX_USE_CXX11_ABI=1` 一致）。
- **向后兼容**：业务代码 (`tools/cpu_sim/main.cpp`、16 个 plugin) 不可一次性 break。

---

## Decision

主项目 + `cf_plugin` INTERFACE 库 + tests + verilator_runner 升 C++23；引入 `PluginError` 强类型枚举 + `Result<T>` 别名；将 **10 个**静态配置 API 从 `void` + `throw` 迁移到 `Result<T, PluginError>` 返回。

### 10 个迁移 API

| API | Before | After |
|-----|--------|-------|
| `PipeBuilder::register_plugin(unique_ptr<PluginBase>)` | `void` (throw `invalid_argument`) | `Result<void>` |
| `PipeBuilder::at_stage(string, Phase, StageCallback)` | `void` (throw) | `Result<void>` |
| `PipeBuilder::declare_substage(string, string, int=0)` | `void` | `Result<void>` (新增 `EmptyParentStageName` 校验) |
| `PipeBuilder::register_commit_hook(CommitHook)` | `void` (throw) | `Result<void>` |
| `PipeBuilder::register_ctrl_link(string, shared_ptr<CtrlLink>)` | `void` (throw) | `Result<void>` |
| `PipeBuilder::register_stage_payload_connector<T>(...)` | `void` (throw) | `Result<void>` |
| `PipeBuilder::build()` | `void` | `Result<void>` (try-catch 三层包装 plugin callback) |
| `PipeBuilder::elaborate(ch::core::context&)` / `PipeBuilder::elaborate()` | `void` / `void` | `Result<void>` / `Result<void>` |
| `PipeBuilder::to_verilog(string)` | `void` | `Result<void>` |
| `PipeBuilder::create_simulator()` | `unique_ptr<ch::Simulator>` | `Result<unique_ptr<ch::Simulator>>` |

### 不迁移 API（ADR-047 例外清单）

| API | 不迁移理由 |
|-----|-----------|
| `PipeBuilder::run()` | CtrlLink `throw_when` 运行时语义（设计约定） |
| `PayloadStore::get()` (const) | at_stage 回调内热路径 |
| `PayloadStore::get()` (mutable) | 同上 |
| `PayloadStore::put()` | 同上 |
| `PluginBase::setup()` / `PluginBase::build()` 虚函数 | 回调体内部业务代码可自由选择 |
| `PipeBuilder::node_of_logic_stage(string)` | **Metis H1 修订**：返回 `shared_ptr<PipeNode>` 或 `nullptr`；不迁移避免 ~185 处 `Plugin::build()` 内调用点连锁影响；留 v0.7 单独 change |
| `PipeBuilder::get_ctrl_link(string)` | **Metis H1 修订**：同上 |

### 新增工具

- `include/cf/plugin/plugin_error.h`：20 字段 `enum class PluginError` + `Result<T>` 别名 + `plugin_error_message()` 工厂 + `to_exception()` 转换。
- `include/cf/plugin/result_macros.h`：`PB_TRY(expr)` 内部传播宏 + `PB_EXPECT(cond, err)` 条件检查宏 + `auto_throw<T>(Result<T>)` 业务抹平模板（含 `void` 特化）。
- `include/cf/plugin/plugin_exception.h` 增强：新增 `PluginException(PluginError, stage)` 构造重载。

### 业务侧迁移

`CpuFactory::build_cpu()` 内部新增 `build_cpu_impl()` 返回 `Result<unique_ptr<PipeBuilder>>`，对外 `build_cpu()` 保留 `throw PluginException` 包装层（用 `auto_throw` 实现），兼容现有 CLI 入口 `tools/cpu_sim/main.cpp`。

### CI 门禁

新增 3 项 `tools/check_plugin_portability.sh` Check：

| Check | 验证内容 |
|-------|---------|
| **Check 10** | 禁止静态配置头文件 `throw std::*/PluginException`（exceptions: `payload.h`、`ctrl_link.h`） |
| **Check 11** | P0 API 签名同步（**10 个** API 必须包含 `Result` 或 `expected`，不能还是 `void`） |
| **Check 12** | C++23 编译标志强制（**4 个** CMake 文件不能出现 `cxx_std_(17|20)`） |

---

## Consequences

### 收益

1. **CH_MEM elaboration 可重入**：值类型错误穿透 DAG 边界，与 CppHDL `lnodeimpl` 体系对齐。
2. **编译期类型安全**：`enum class PluginError` 防止字符串拼写错误。
3. **统一错误传播**：10 API 同形 `Result<T, PluginError>`，新增 API 时复制模式即可。
5. **C++23 技术债消除**：与 CppHDL C++23 ABI 一致。

### 成本

1. **业务 plugin 调用点**: ~185 处 `node_of_logic_stage` / `get_ctrl_link` 调用保持原签名（Metis 修订），业务代码零改造。
2. **`auto_throw()` 抹平层**: 业务侧 `CpuFactory::build_cpu_impl` → `auto_throw` → `build_cpu` 抛 `PluginException`，对 `cpu_sim` CLI 入口保持原调用风格。
3. **`-Wno-unused-result` 临时抑制**: P0 阶段临时加 `-Wno-unused-result` 到 `src/cf_plugin/CMakeLists.txt`，抑制 `std::expected` 的 `[[nodiscard]]` 警告；P1 完成后移除。
4. **CppTLM C++17 ABI**：通过 `_GLIBCXX_USE_CXX11_ABI=1` 一致保证；按需 `visibility pragma` 或 `-Wno-psabi`。

### 风险监控

| 风险 | 监控命令 | 触发条件 |
|------|---------|--------|
| R1 C++23 regression | `ctest --test-dir build` | 任何 FAIL = 立即回滚 |
| R2 CppTLM ABI 冲突 | `cmake --build build 2>&1 \| grep -i "abi\|odr\|mismatch"` | ABI 警告 = 加 `-Wno-psabi` |
| R3 [[nodiscard]] 警告 | `cmake --build build 2>&1 \| grep warning` | ~109 个未处理点 = 系统化排查；P0 用 `-Wno-unused-result` 抑制 |
| R4 plugin callback throw | `tests/cpu/test_*` | 任何 build_test 异常 = 加 try-catch |
| R5 `at_stage` 回调 `PayloadStore::get()` 仍 throw | ADR-047 例外清单 | 设计约定，超出本迁移范围 |

---

## Migration Plan

### P0 (6h / 1 人天, v0.6.0)

1. C++23 编译标志升级（4 个 CMake 文件 + 临时 `-Wno-unused-result`）
2. 新建 `plugin_error.h` + `result_macros.h` + 增强 `plugin_exception.h`
3. PipeBuilder **10 个 API 迁移** (签名改 + 业务侧用 `auto_throw` 抹平)

### P1 (7h / 1 人天, v0.6.0)

1. `CpuFactory::build_cpu_impl()` Result 化
2. `tools/cpu_sim/main.cpp` `auto_throw` 抹平
3. ADR-047 新建 + ADR-026/031/032/040 补丁
4. CI Check 10/11/12 新增 + 移除 `-Wno-unused-result` 抑制

### P2 (8h / 1 人天, 后续 PR)

1. **5 个** `REQUIRE_THROWS_*` + **~30 个**隐式中断测试断言改写（Metis H2 修订）
2. `plugin_error_message()` 完整填写 20 字段 switch + 单测
3. `docs/architecture/error-handling.md` §9 + `plugin-style-design-methodology-v1.md` 补丁
4. CHANGELOG v0.6.0 条目

---

## 关联文档

- **变更工作区**: `openspec/changes/v06-static-config-result/`
- **ADR-026 (at_stage)**: 补丁——"`at_stage()` 返回 `Result<void>`"
- **ADR-031 (CtrlLink)**: 补丁——"`register_ctrl_link()` / `get_ctrl_link()` 返回 `Result`"（后者 Metis H1 不迁移）
- **ADR-032 (PipeBuilder)**: 补丁——整段扩充 Result 范式决策
- **ADR-040 v3.0**: 行末添加 ADR-047 引用
- **P1 缺口定义**: `.omo/architecture-reviews/2026-09-22-v0.5.0-phase6d-closeout.md`
- **Metis 审查报告**: 内联（2026-09-22, 5m17s, REVISE verdict）
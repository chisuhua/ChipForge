# v0.6 静态配置期错误处理 Result 范式迁移

> **OpenSpec change**: `v06-static-config-result`
> **关联文档**: [设计文档](../../docs/superpowers/specs/2026-09-22-v0.6-result-error-handling-design.md) · [P1 缺口定义](../../.omo/architecture-reviews/2026-09-22-v0.5.0-phase6d-closeout.md)
> **关联 ADR**: ADR-026 (at_stage) · ADR-030 (PipeNode) · ADR-031 (CtrlLink) · ADR-032 (PipeBuilder) · ADR-040 v3.0 (移植性) · ADR-046 (FSM 豁免) · 新增 **ADR-047**
> **关联 Oracle 报告**: 内联（2026-09-22，3m30s，A→H 8 节 + 推荐执行路径）
> **目标版本**: v0.6.0

---

## Why

ChipForge v0.5.0 Phase 6d 架构验证报告标记了**唯一 Critical P1 项**：应用层错误处理缺失（`include/cf/plugin/*.h` 0 处结构化错误，CppHDL 子仓库却有 267 处 `CHERROR/assert`）。框架静态配置 API 全部 `void` + `throw`，与 CH_MEM elaboration 路径不可重入冲突，导致 SoC 级调试时缺少 IP 失效传播链，故障定位只能依赖 gdb 跟踪。

v0.6 是 Phase 6d 闭环后首个迭代，将主项目 + `cf_plugin` 升 C++23 + 12 个静态配置 API 迁移到 `std::expected<T, PluginError>` 值类型错误传播，并新增 ADR-047 锁定范式。

## What Changes

### 编译器与构建

- **修改** `CMakeLists.txt` L24：`CMAKE_CXX_STANDARD 17 → 23`（主项目默认标准升级）
- **修改** `src/cf_plugin/CMakeLists.txt` L45：`cxx_std_17 → cxx_std_23`（INTERFACE 库传播）
- **修改** `tests/CMakeLists.txt` L50 + L130：`cxx_std_20 → cxx_std_23`（两个 test target）
- **修改** `tools/verilator_runner/CMakeLists.txt` L33：`cxx_std_20 → cxx_std_23`（Verilator sim runner，第 4 个 CMake 文件，Check 12 必须覆盖）
- **不修改** CppTLM (C++17 子仓库, libcpptlm_core.a 通过 Itanium ABI 兼容)
- **不修改** CppHDL (已是 C++23)

### 新增头文件

- **新增** `include/cf/plugin/plugin_error.h`（~120 行）
  - `enum class PluginError`（20 字段：参数校验 10 + 配置冲突 4 + 构建/elaboration 5 + 运行期 3）
  - `template<typename T> using Result = std::expected<T, PluginError>` 别名
  - `std::string plugin_error_message(PluginError, const std::string& detail = "")` 工厂
  - `PluginException to_exception(PluginError, const std::string& stage = "")` 转换
- **新增** `include/cf/plugin/result_macros.h`（~30 行）
  - `PB_TRY(expr)` 内部传播宏
  - `PB_EXPECT(cond, err)` 条件检查宏
  - `auto_throw<T>(Result<T>)` 业务侧抹平工具
- **增强** `include/cf/plugin/plugin_exception.h`（保留 38 行 + 新增 PluginError 构造重载）

### **BREAKING** API 签名变化（`include/cf/plugin/pipe_builder.h`）

> **Metis 修订 (2026-09-22)**：原列表 12 API 中移除 2 个查询型 API（`node_of_logic_stage` / `get_ctrl_link`），改为 10 API。原因：返回类型为 `shared_ptr<T>` 的查询语义与 Result 范式契合度低；保留原签名可避免 ~185 处业务 plugin 调用点的破坏性连锁影响（`Plugin::build()` 内 `pb.node_of_logic_stage(...).get()` 等）。两个 API 留待 v0.7 单独 change 处理（语义可能改为 `OrError<T>` 等）。

| API | Before | After |
|-----|--------|-------|
| `PipeBuilder::register_plugin(unique_ptr<PluginBase>)` | `void` | `Result<void>` |
| `PipeBuilder::at_stage(string, Phase, StageCallback)` | `void` | `Result<void>` |
| `PipeBuilder::declare_substage(string, string, int=0)` | `void` | `Result<void>`（新增 EmptyParentStageName 校验） |
| `PipeBuilder::register_commit_hook(CommitHook)` | `void` | `Result<void>` |
| `PipeBuilder::register_ctrl_link(string, shared_ptr<CtrlLink>)` | `void` | `Result<void>` |
| `PipeBuilder::register_stage_payload_connector<T>(string, ConnectorFn<T>)` | `void` | `Result<void>` |
| `PipeBuilder::build()` | `void` | `Result<void>`（+ 内部 try-catch 包装 plugin callback） |
| `PipeBuilder::elaborate(ch::core::context&)` / `PipeBuilder::elaborate()` | `void` / `void` | `Result<void>` / `Result<void>` |
| `PipeBuilder::to_verilog(string)` | `void` | `Result<void>` |
| `PipeBuilder::create_simulator()` | `unique_ptr<ch::Simulator>` | `Result<unique_ptr<ch::Simulator>>` |
| ~~`PipeBuilder::node_of_logic_stage(string)`~~ | ~~`shared_ptr<PipeNode>` 或 `nullptr`~~ | **保留原签名**（不在本 change scope） |
| ~~`PipeBuilder::get_ctrl_link(string)`~~ | ~~`shared_ptr<CtrlLink>` 或 `nullptr`~~ | **保留原签名**（不在本 change scope） |

**不破坏**：
- `PipeBuilder::run()` 仍为 `void`（CtrlLink `throw_when` 运行时语义保留 `throw PluginException`）
- `PayloadStore::get()` 仍为 `const T&`（at_stage 回调内热路径保留 throw `std::runtime_error`）
- `PayloadStore::put()` 不变
- `PluginException` 类签名向后兼容
- `PipeBuilder::node_of_logic_stage(string)` 仍为 `shared_ptr<PipeNode>` 或 `nullptr`（Metis 修订，~185 处调用点零影响）
- `PipeBuilder::get_ctrl_link(string)` 仍为 `shared_ptr<CtrlLink>` 或 `nullptr`（Metis 修订）

### 业务代码适配（`ip/cpu/cpu_factory.h` + `ip/cpu/cpu_factory_chmem.h`）

- **修改** `CpuFactory::build_cpu()` 内部全 Result 化（`PB_TRY` 包裹所有 `register_plugin` / `at_stage` / `register_ctrl_link` / `build` / `elaborate` 调用）
- **保留** 对外 `build_cpu()` 抛 `PluginException` 包装层（兼容现有 `cpu_sim/main.cpp` 调用点）
- **新增** `CpuFactory::build_cpu_impl(...)` 内部函数，返回 `Result<unique_ptr<PipeBuilder>>`

### ADR 升级

- **新增** `docs/architecture/adr/ADR-047-static-config-result-paradigm.md`（~80 行）
- **修改** `docs/architecture/adr.md` 表（添加 ADR-047 行）
- **补丁** ADR-026 (at_stage) / ADR-030 (PipeNode) / ADR-031 (CtrlLink) / ADR-032 (PipeBuilder) 各加 Result 路径说明
- **引用** ADR-040 v3.0 行末添加 ADR-047 引用（与移植性正交）

### CI 门禁增强

- **新增** `tools/check_plugin_portability.sh` **Check 10**：禁止静态配置头文件 `throw std::*/PluginException`（exceptions: `payload.h`、`ctrl_link.h`）
- **新增** **Check 11**：P0 API 签名同步（**10 个 API**必须包含 `Result` 或 `expected`，不能还是 `void`）
- **新增** **Check 12**：C++23 编译标志强制（**4 个 CMake 文件**不能出现 `cxx_std_(17|20)`：主 CMakeLists.txt + src/cf_plugin/CMakeLists.txt + tests/CMakeLists.txt + tools/verilator_runner/CMakeLists.txt）

### 文档同步

- **新增** `docs/architecture/error-handling.md §9`（"cf::plugin Result 范式"，~50 行）
- **补丁** `docs/methodology/plugin-style-design-methodology-v1.md`（补充 Result 范式到方法学约束）
- **新增** `CHANGELOG.md` v0.6.0 条目

### 工作量与验收

> **Metis 修订 (2026-09-22)**：总工作量从 24h 降至 22h（P0: 8h → 6h，移除 2 API 节省 ~2h；P1 不变 7h；P2 不变 8h）。

- **总工作量**：22h（约 3 人/天 或 1 人/周，含 review 缓冲 1.5 周）
- **P0**：6h（C++23 升级 + PluginError 设计 + **10 API 签名改**）
- **P1**：7h（CpuFactory 适配 + ADR + CI checks）
- **P2 (本 change scope 内可选)**：8h（5 个 `REQUIRE_THROWS_*` 改写 + 隐式中断 ~30 个断言适配 + 文档补全）

## Capabilities

### New Capabilities

- `plugin-error-result-paradigm`：cf::plugin 框架的静态配置 API 错误传播范式（Result 类型 + PluginError 枚举 + PB_TRY 宏 + auto_throw 抹平工具）。决定哪些 API 返回 Result、哪些保留 throw、错误类型如何定义。这是 v0.6 的核心 capability，后续 Plugin 业务代码迁移到 Result 范式时引用此 spec。

### Modified Capabilities

无（现有 specs 不涉及错误处理范式层面。本次改动仅在实现层，不修改其他 capability 的 REQUIREMENTS）。

> 注：ADR-026/030/031/032 的补充属于 ADR 注册表层面，非 capability 修改。

## Impact

### 受影响代码

| 模块 | 改动 |
|------|------|
| `include/cf/plugin/plugin_error.h` | 新增（~120 行） |
| `include/cf/plugin/result_macros.h` | 新增（~30 行） |
| `include/cf/plugin/plugin_exception.h` | 增强（+PluginError 构造重载） |
| `include/cf/plugin/pipe_builder.h` | **BREAKING** 12 API 签名改 + 内部 PB_TRY 化 |
| `ip/cpu/cpu_factory.h` | 内部 Result 化（PB_TRY 包裹 7 plugins + ~80 at_stage） |
| `ip/cpu/cpu_factory_chmem.h` | 同上（CH_MEM 版本） |
| `tools/cpu_sim/main.cpp` | 用 `auto_throw()` 抹平（不破 CLI 接口） |
| `CMakeLists.txt` | CXX_STANDARD 17 → 23 |
| `src/cf_plugin/CMakeLists.txt` | cxx_std_17 → cxx_std_23 |
| `tests/CMakeLists.txt` | cxx_std_20 → cxx_std_23（×2 targets） |
| `tools/verilator_runner/CMakeLists.txt` | cxx_std_20 → cxx_std_23（Metis H5 修订，第 4 个 CMake 文件） |
| `tools/check_plugin_portability.sh` | +Check 10/11/12 |
| `docs/architecture/adr.md` | +ADR-047 + 4 个 ADR 补 Result 行 |
| `docs/architecture/adr/ADR-047-*.md` | 新增 |
| `docs/architecture/error-handling.md` | +§9 |
| `CHANGELOG.md` | +v0.6.0 条目 |

### 受影响测试

> **Metis 修订 (2026-09-22, H2)**：原 proposal "53 个 `REQUIRE_THROWS_*`" 数字夸大；准确数字 = **5 个 throw 断言** + **~30 个隐式中断**（如 `REQUIRE(pb.node_of_logic_stage("x") != nullptr)` 等值类型断言）。
> 由于本 change 移除了 `node_of_logic_stage`/`get_ctrl_link` 迁移，隐式中断范围缩到仅 `register_plugin` / `at_stage` 等 10 API 改签名后的 ~30 个 `auto_throw` 替换点（实际触发需在 P1 实施时验证）。throw 断言本身保持不变（`auto_throw` 抹平层兼容）。

- **P0/P1 暂不动**：现有 **5 个** `REQUIRE_THROWS_*` 断言保留（CpuFactory 对外 throw 包装层不变，`build()` 内部 try-catch 转 `PluginException`，行为兼容）
- **P2 (可选 PR)**：5 个 `REQUIRE_THROWS_*` + ~30 个隐含断言改写为 Result 值类型检查（独立 PR，工作量重估）

### 受影响 API（业务侧调用方）

| 调用方 | 改动 |
|--------|------|
| `tools/cpu_sim/main.cpp` | 用 `auto_throw(pb.register_plugin(...))` 抹平，调用风格不变 |
| 16 个 `ip/*/plugins/*.h` 的 at_stage 回调 | **不变**（回调函数体不在迁移范围） |
| 16 个 `ip/*/plugins/*.h` 的 setup/build 虚函数 | **不变**（基类签名不变；Metis H1 修订后 ~185 处 `node_of_logic_stage` 调用点零影响） |
| `tests/` 下现有 **5 个** `REQUIRE_THROWS_AS` 断言 | **不变**（P2 阶段处理） |

### 依赖与外部影响

- **CppTLM (C++17)**：作为静态库链接，Itanium ABI 兼容 C++23 父项目，零修改
- **CppHDL (C++23)**：子仓库，无需修改
- **Verilator 5.052**：已支持 C++20/23，无影响
- **GCC 13.3.0**：`std::expected` 在 `-std=c++23` 下完整支持（已实测）
- **CI 阻塞门禁**：PR 必须通过 `verify_adr.sh`（新增 ADR-047）+ `check_plugin_portability.sh`（12 checks）+ `ctest` 全 PASS

### 风险

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| R1: C++23 边缘特性 regression | 10% | 高 | 独立分支先升级 + 全量 ctest 验证零 regression |
| R2: CppTLM (C++17 ABI) inline 模板冲突 | 15% | 中 | `_GLIBCXX_USE_CXX11_ABI=1` 一致；按需 visibility pragma；Metis F2 审查时增加 `grep -i "abi\|odr\|mismatch"` 扫描 |
| R3: ~109 个 `[[nodiscard]]` 警告暴露遗漏点（`std::expected` 默认 `[[nodiscard]]`） | 30% | 中 | P0 阶段临时加 `-Wno-unused-result` 抑制；P1 逐点 `PB_TRY` 修复 |
| R4: `pb.build()` 内部 plugin callback throw 被短路 | 25% | 中 | try-catch 包装：(a) PluginException→PluginBuildFailed；(b) std::exception→通用错误（Metis F3） |
| R5: `at_stage` 回调 PayloadStore::get() 仍 throw（设计约定） | 80% | 低 | ADR-047 例外清单明确记录；超出本迁移范围 |
| R6: cpu_sim/main.cpp 调用点遗漏 | 20% | 低 | `auto_throw()` 抹平工具；CPU simulator 单测验证 |

---

## Acceptance Criteria

### P0 完成验收（8h 内）

- [ ] `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)` 全量编译通过
- [ ] `ctest --test-dir build --output-on-failure -j$(nproc)` **100% PASS**（零 regression）
- [ ] `include/cf/plugin/plugin_error.h` + `result_macros.h` 已存在
- [ ] `include/cf/plugin/pipe_builder.h` 12 个 P0 API 已改为 `Result<>` / `std::expected<>` 返回
- [ ] `include/cf/plugin/plugin_exception.h` 新增 `PluginError` 构造重载

### P1 完成验收（7h 内）

- [ ] `CpuFactory::build_cpu()` 内部全 PB_TRY 化（业务代码不感知）
- [ ] `CpuFactory::build_cpu()` 对外 throw 包装层保留（`cpu_sim/main.cpp` 兼容）
- [ ] `tools/cpu_sim/main.cpp` 用 `auto_throw()` 抹平
- [ ] ADR-047 文件 + adr.md 表 + ADR-026/030/031/032 补 Result 行
- [ ] `tools/check_plugin_portability.sh` 新增 Check 10/11/12 通过
- [ ] `tools/verify_adr.sh` 新增 ADR-047 验证项通过

### 最终回归闸门

- [ ] `bash tools/verify_adr.sh` 32/32 PASS（31 旧 + 1 新 ADR-047）— **Metis H4 修订：实际应为 39/39（38 旧 + 1 新），需在 P1.3 实施时验证 verify_adr.sh 实际 PASS 计数**
- [ ] `bash tools/check_plugin_portability.sh` 12/12 PASS（9 旧 + 3 新）
- [ ] `bash tools/verify_plugin_decision.sh` 3+4/3 PASS
- [ ] `ctest --test-dir build --output-on-failure` 100% PASS（含 chipforge_tests + chipforge_tests_chmem）
- [ ] `./bin/chipforge_tests` TLM baseline 0 回归
- [ ] `./bin/chipforge_tests_chmem` 100% PASS（CH_MEM baseline 0 回归）

### Out of Scope（本 change 不做）

- ❌ 5 个 `REQUIRE_THROWS_*` + ~30 个隐式中断测试断言改写（P2 独立 PR，Metis H2 修订）
- ❌ 16 个业务 plugin 的 at_stage 调用点改造（保留 void 委托 throw）
- ❌ `node_of_logic_stage` / `get_ctrl_link` API 签名迁移（Metis H1 修订：保持 `shared_ptr`/`nullptr`，避免 ~185 处业务 plugin 连锁影响，留待 v0.7 单独 change）
- ❌ `PayloadStore::get()` 改 Result（at_stage 热路径，超出本迁移范围）
- ❌ `PipeBuilder::run()` 改 Result（CtrlLink `throw_when` 运行时语义）
- ❌ CppTLM 子仓库升级（C++17 子仓库）
- ❌ CppHDL 子仓库升级（已是 C++23）
- ❌ 6d.x 系列变更（已归档）

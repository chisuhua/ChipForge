# v0.6 静态配置期错误处理 Result 范式迁移 — 设计文档

> **OpenSpec change**: `v06-static-config-result`
> **详细设计**: 见 [`docs/superpowers/specs/2026-09-22-v0.6-result-error-handling-design.md`](../../docs/superpowers/specs/2026-09-22-v0.6-result-error-handling-design.md)（499 行，含完整 API 签名变化 + 实施计划 + 风险评估）
> **提案**: 见 `proposal.md`
>
> **Metis 修订 (2026-09-22)**：迁移列表 12 API → **10 API**（移除 `node_of_logic_stage`/`get_ctrl_link`）；P0 工作量 8h → 6h；总工作量 24h → 22h；新增 verilator_runner CMakeLists.txt 升级任务；P2 数字 53 → 5+~30。

---

## Context

### 现状

ChipForge v0.5.0 (Phase 6d 收官, 2026-09-22) 的 cf::plugin 框架静态配置 API 全部 `void` + `throw`：

- `include/cf/plugin/pipe_builder.h` 8 处 `throw std::invalid_argument` + 3 处 `throw PluginException`
- `include/cf/plugin/payload.h` 6 处 `throw std::runtime_error` (CH_MEM fail-fast)
- `include/cf/plugin/plugin_exception.h` 已存在 (38 行, 仅 `run()` 的 CtrlLink `throw_when` 使用)
- `include/cpu/cpu_factory.h` 2 处 `throw std::invalid_argument`
- 业务调用点：~109 处（ip/*/plugins）+ ~95 处（tests）

> **Metis 修订**：业务调用点分布 — `node_of_logic_stage` 与 `get_ctrl_link` 调用点 ~185 处，集中在 `Plugin::build()` 内部；这些 API 不在本 change scope 迁移，~185 处调用点零影响。

### 约束

- **CH_MEM elaboration 路径阻塞**：throw 无法穿透 lnode DAG 边界，配置错误必须以值类型传播
- **C++17 标准**：`std::expected` 是 C++23，GCC 13.3 + `-std=c++23` 已实测支持
- **CppHDL 已是 C++23**，主项目仍是 C++17 已成技术债
- **向后兼容**：业务代码（`tools/cpu_sim/main.cpp`、16 个 plugin）不可一次性 break
- **CI 阻塞门禁**：3 脚本（verify_adr + verify_plugin_decision + check_plugin_portability）任何失败 = PR blocked

### 干系人

- 框架维护者：`include/cf/plugin/` 9 个 header
- IP 业务代码：`ip/cpu/plugins/`、`ip/cache/tlm/`、`ip/memory/` 等
- 测试：75 个 test file，403 个 test case
- CI：`.github/workflows/architecture-gates.yml`

## Goals / Non-Goals

**Goals:**

1. 主项目 + `cf_plugin` INTERFACE + tests + verilator_runner 升 C++23（**5 行** CMake 改动，4 个 CMake 文件）
2. **10 个**静态配置 API 改为 `std::expected<T, PluginError>` 返回（Metis H1 修订）
3. 新增 `enum class PluginError` (20 字段) + `Result<T>` 别名 + 工厂函数
4. `CpuFactory::build_cpu()` 内部 Result 化 + 对外保留 throw 包装层（兼容现有 CLI 入口）
5. 新增 ADR-047 + 现有 ADR-026/031/032 补 Result 路径（ADR-030 不需补丁，因 `node_of_logic_stage` 不迁移）
6. 新增 CI Check 10/11/12 防止未来回退
7. **22 小时**工作量（3 人/天 或 1 人/周；Metis 修订：P0 8h→6h）

**Non-Goals:**

- ❌ CppTLM 子仓库升级（C++17 静态库，Itanium ABI 兼容 C++23 父项目，零修改）
- ❌ CppHDL 子仓库升级（已是 C++23）
- ❌ `PayloadStore::get()` 改 Result（at_stage 回调内热路径）
- ❌ `PipeBuilder::run()` 改 Result（CtrlLink `throw_when` 运行时语义）
- ❌ 16 个业务 plugin 的 at_stage 调用点改造（保留 void 委托 throw）
- ❌ `node_of_logic_stage` / `get_ctrl_link` API 签名迁移（Metis H1：保持 `shared_ptr<T>`/`nullptr`，避免 ~185 处业务 plugin 连锁影响；留 v0.7 单独 change）
- ❌ **5 个** `REQUIRE_THROWS_*` + **~30 个**隐式中断 测试断言改写（P2 独立 PR，Metis H2 修订）

## Decisions

### D1: C++23 + `std::expected` 而非 vendor polyfill

**决定**：将主项目升 C++23，使用标准库 `std::expected<T, E>`。

**理由**：
- GCC 13.3 + `-std=c++23` 完整支持 `std::expected`（已实测）
- 无 vendor 依赖（无 `tl::expected` 或 `boost::outcome`）
- 与 CppHDL 子仓库 C++23 ABI 一致（`libcpphdl.a` C++23 ABI）
- CppTLM C++17 ABI 通过 Itanium C++ ABI 兼容层与 C++23 父项目共存

**备选**：
- ❌ C++17 + 手写 Result<T,E>：增加 ~120 行维护成本
- ❌ C++17 + vendor polyfill（tl::expected）：增加第三方依赖
- ❌ 升级 C++20 + vendor polyfill：与 CppHDL C++23 不一致

### D2: 仅静态配置期 API 改 Result，热路径保留 throw

**决定**：`PipeBuilder::run()` 的 CtrlLink `throw_when` 机制保留 `throw PluginException`；`PayloadStore::get()`（at_stage 回调内调用）保留 `throw std::runtime_error`。

**理由**：
- 运行期 throw 是 CtrlLink 设计语义（用户显式声明 `should_throw` 触发）
- at_stage 回调内 `PayloadStore::get()` 每周期调用（热路径），Result 化需要改 16 个 plugin 闭包体，超出本迁移范围
- 测试中 `REQUIRE_THROWS_AS(pb.run(), PluginException)` 不需改

**ADR-047 例外清单**（明确记录）：
1. `PipeBuilder::run()` — CtrlLink `throw_when` 运行时语义
2. `PayloadStore::get()` — at_stage 回调内热路径
3. `CppHDL` `CH_ASSERT` — 子仓库范围外
4. `PluginBase` 虚函数（`setup`/`build`）— 回调体内部业务代码可自由选择

### D3: `CpuFactory::build_cpu()` 内部 Result 化 + 对外 throw 包装层

**决定**：分两步迁移：
1. 内部 `build_cpu_impl()` 全 Result 化（`PB_TRY` 包裹所有 `register_plugin`/`at_stage`/`build`）
2. 对外 `build_cpu()` 保留 `throw PluginException` 包装层（兼容 `cpu_sim/main.cpp` 入口）

**理由**：
- `build_cpu()` 是 CLI 工具唯一入口，一次性 break 会阻塞整个 CPU 测试家族
- 包装层允许分阶段迁移（v0.6 内部 + v0.7 业务侧）
- 业务侧 `auto_throw()` 抹平工具提供统一转换点

**备选**：
- ❌ 一次性改所有调用方：需要同步改 5 个 cpu-integration 测试 + 16 个 plugin，~40 处变更
- ❌ 保留 void API + 内部 throw：未兑现 Result 范式的核心价值

### D4: 错误类型用 `enum class PluginError` 而非 `std::string`

**决定**：`PluginError` 用强类型枚举，20 个具名字段（参数校验 10 + 配置冲突 4 + 构建/elaboration 5 + 运行期 1）。

**理由**：
- 编译期拼写检查（避免字符串错误码拼写错误）
- 转换 `std::string` 通过 `plugin_error_message()` 工厂按需生成
- 可扩展性：新增错误只需加枚举值 + 工厂 switch case

**备选**：
- ❌ 直接 `std::string` 错误消息：失去类型安全，需约定错误码字符串
- ❌ `std::error_code` 体系：与 std lib 紧耦合，增加头文件依赖

### D5: CI 强制门禁 3 个新检查

**决定**：新增 `tools/check_plugin_portability.sh` Check 10/11/12：
- **Check 10**：禁止静态配置头文件 `throw std::*/PluginException`（exceptions: `payload.h`、`ctrl_link.h`）
- **Check 11**：P0 API 签名同步（**10 个** API 必须包含 `Result` 或 `expected`，不能还是 `void`；Metis H1 修订）
- **Check 12**：C++23 编译标志强制（**4 个** CMake 文件不能出现 `cxx_std_(17|20)`：主 CMakeLists.txt + src/cf_plugin/CMakeLists.txt + tests/CMakeLists.txt + tools/verilator_runner/CMakeLists.txt；Metis H5 修订）

**理由**：
- 防止未来 contributor 回退到 throw
- C++23 是技术债消除（CppHDL 已升），强标 `cxx_std_23` 防退步
- 9 → 12 checks（ADR-040 v3.0 + ADR-046 + 新 Check 10/11/12）

## Risks / Trade-offs

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | C++23 边缘特性 regression | 10% | 高 | 独立分支先升级 + 全量 ctest 验证零 regression；GCC 13 C++23 已生产级 |
| R2 | CppTLM (C++17 ABI) inline 模板冲突 | 15% | 中 | `_GLIBCXX_USE_CXX11_ABI=1` 一致；按需 visibility pragma；Metis F2 审查时 `cmake --build build 2>&1 \| grep -i "abi\|odr\|mismatch"` 扫描；按需 `-Wno-psabi` |
| R3 | ~109 个业务 `[[nodiscard]]` 警告暴露遗漏 | 30% | 中 | P0 临时加 `-Wno-unused-result`（Metis A3 修订）；P1 逐点 `PB_TRY` 修复后取消抑制 |
| R4 | `pb.build()` 内部 plugin callback throw 被短路 | 25% | 中 | try-catch 三层包装：(a) `PluginException`→`PluginBuildFailed`；(b) `std::exception`→`BuildFailed`；(c) `...`→`BuildFailed`（Metis F3 修订） |
| R5 | `at_stage` 回调内 `PayloadStore::get()` 仍 throw（设计约定） | 80% | 低 | ADR-047 例外清单明确记录；超出本迁移范围 |
| R6 | `cpu_sim/main.cpp` 调用点遗漏 | 20% | 低 | `auto_throw()` 抹平工具；CPU simulator 单测验证 |

### 性能影响

**零**。迁移范围是静态配置期 API（`register_*` / `build` / `elaborate` / `to_verilog`），每个程序生命周期调用一次或几次。`std::expected` 开销（一次分支预测 + Error 枚举拷贝）在此上下文可忽略。热路径（`run()` tick 循环）不变。

### ABI 兼容性

- `cf_plugin` 是 INTERFACE 库（头文件 only），无 ABI 问题
- `libcpptlm_core.a` C++17 ABI 通过 C 链接兼容层 + Itanium ABI 一致保证
- `libcpphdl.a` 已是 C++23 ABI，一致

## Migration Plan

### 阶段 0（P0, 6h / 1 人天）

> **Metis 修订**：8h → 6h，因移除 2 个查询型 API。

```
0.0  C++23 编译标志升级 + ABI 兜底 (1h)
     ├─ CMakeLists.txt: L24 17→23
     ├─ src/cf_plugin/CMakeLists.txt: L45 cxx_std_17→cxx_std_23
     ├─ tests/CMakeLists.txt: L50/L130 cxx_std_20→cxx_std_23
     ├─ tools/verilator_runner/CMakeLists.txt: L33 cxx_std_20→cxx_std_23 (Metis H5 修订)
     └─ src/cf_plugin/CMakeLists.txt: 临时加 -Wno-unused-result (Metis A3 修订)

     ⏸ Gate 0.0: ctest 100% PASS（零 regression；ABI grep 无匹配）

0.1  PluginError 设计 (2h)
     ├─ 新建 include/cf/plugin/plugin_error.h (120 行)
     │   - enum class PluginError (20 字段)
     │   - template<T> using Result = std::expected<T, PluginError>
     │   - plugin_error_message() 工厂
     │   - to_exception() 转换
     ├─ 新建 include/cf/plugin/result_macros.h (30 行)
     │   - PB_TRY (do-while + `_r` 变量避免 shadow)
     │   - PB_EXPECT (条件检查)
     │   - auto_throw (含 void 特化)
     └─ 增强 include/cf/plugin/plugin_exception.h
         - 新增 PluginError 构造重载

0.2  PipeBuilder 10 API 迁移 (3h)
     ├─ register_plugin → Result<void>
     ├─ at_stage → Result<void>
     ├─ declare_substage → Result<void> (新增 EmptyParentStageName 校验)
     ├─ register_commit_hook → Result<void>
     ├─ register_ctrl_link → Result<void>
     ├─ register_stage_payload_connector<T> → Result<void> (模板签名明确)
     ├─ build → Result<void> (内部 try-catch 三层包装 plugin callback)
     ├─ elaborate(ctx) / elaborate() → Result<void> (无参版本内部 PB_TRY 委托)
     ├─ to_verilog → Result<void>
     └─ create_simulator → Result<unique_ptr<ch::Simulator>>
     [排除] node_of_logic_stage / get_ctrl_link (Metis H1 修订)

     ⏸ Gate 0.2: ctest 100% PASS（行为不变，签名变）
```

### 阶段 1（P1, 7h / 1 人天）

```
1.1  CpuFactory 内部 Result 化 (3h)
     ├─ 新增 CpuFactory::build_cpu_impl() 返回 Result
     └─ 保留 CpuFactory::build_cpu() throw 包装层

1.2  cpu_sim/main.cpp 抹平 (1h)
     └─ 用 auto_throw() 桥接（不破 CLI 接口）

1.3  ADR 升级 (2h)
     ├─ 新建 docs/architecture/adr/ADR-047-static-config-result-paradigm.md
     ├─ adr.md 表添加 ADR-047
     ├─ ADR-026/030/031/032 补 Result 行
     └─ ADR-040 v3.0 行末添加 ADR-047 引用

1.4  CI 门禁 (1h)
     ├─ tools/check_plugin_portability.sh 新增 Check 10/11/12
     └─ tools/verify_adr.sh 添加 ADR-047 验证项

     ⏸ Gate P1: 全部 CI PASS + ctest 100% PASS
```

### 阶段 2（P2, 8h / 1 人天，后续 PR）

> **Metis 修订 (H2)**：原"53 个 REQUIRE_THROWS_*" 实际为 5 个 throw + ~30 个隐式中断。

```
2.1  5 个 REQUIRE_THROWS_* + ~30 个隐式中断 改写 (4h)
2.2  plugin_error_message() 完整填写 (1h)
2.3  error-handling.md §9 (2h)
2.4  plugin-style-design-methodology-v1.md 补 Result 范式 (0.5h)
2.5  CHANGELOG.md v0.6.0 条目 (10min)
2.6  ctest + CI 全量回归 (1h)
```

### 回滚策略

| 阶段 | 回滚方式 |
|------|---------|
| P0 0.1 升级后失败 | `git revert <commit>` 即可，CMake 4 行改动 |
| P0 0.2 plugin_error.h 失败 | 删除新文件即可，未影响既有 API |
| P0 0.3 API 签名改失败 | `git revert <commit>`，签名回到 void |
| P1 完成 | 整体回滚 commit，业务侧 throw 包装层兼容 |
| P2 失败 | 独立 PR，回滚不影响 P0/P1 |

## Open Questions

### Q1: `pb.build()` 内部 plugin callback 抛出的异常如何处理？

**待决**：
- 选项 A：捕获 `PluginException` 转 `PluginError::PluginBuildFailed`（精细）
- 选项 B：捕获 `std::exception` 转通用 `PluginError::BuildFailed`（粗糙）
- 选项 C：try-catch + 透传异常信息（混合）

**倾向**：A（精细），保留 plugin 来源信息便于调试。

### Q2: `plugin_error_message()` 工厂是否需要支持 i18n？

**决定**：v0.6 仅英文消息，i18n 留 v0.7+。

**理由**：
- 当前 CppHDL `CH_ASSERT` 仅英文
- 错误消息面向开发者，非终端用户
- 多语言化需要 catalog 文件 + 额外传递 context

### Q3: ADR-047 是否需要拆分多个 ADR？

**决定**：单 ADR-047（覆盖 Result 范式全决策）。

**备选**：
- ADR-047a（错误类型）+ ADR-047b（API 迁移）+ ADR-047c（CI 门禁）
- 优点：每个 ADR 变更范围小
- 缺点：3 个文件重复引用，查找成本高

**倾向**：单 ADR（与现有 ADR-040/046 风格一致）。

---

## 关联文档

- **详细设计**: [`docs/superpowers/specs/2026-09-22-v0.6-result-error-handling-design.md`](../../docs/superpowers/specs/2026-09-22-v0.6-result-error-handling-design.md)（499 行）
- **提案**: [`proposal.md`](./proposal.md)
- **Specs**: [`specs/plugin-error-result-paradigm/spec.md`](./specs/plugin-error-result-paradigm/spec.md)
- **Tasks**: [`tasks.md`](./tasks.md)
- **ADR-040 v3.0**: [`../../docs/architecture/adr/ADR-040-tlm-hdl-portability-constraints.md`](../../docs/architecture/adr/ADR-040-tlm-hdl-portability-constraints.md)
- **ADR-046**: [`../../docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md`](../../docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md)
- **P1 缺口定义**: [`../../.omo/architecture-reviews/2026-09-22-v0.5.0-phase6d-closeout.md`](../../.omo/architecture-reviews/2026-09-22-v0.5.0-phase6d-closeout.md)
- **Phase 6d 验证报告**: 内联（2026-09-22，3m30s）

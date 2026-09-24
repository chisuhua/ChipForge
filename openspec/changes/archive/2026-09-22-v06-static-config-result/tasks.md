# v0.6 静态配置期错误处理 Result 范式迁移 — 任务清单

> **OpenSpec change**: `v06-static-config-result`
> **总工作量**: 22h（3 人/天 或 1 人/周, 含 review 缓冲算 1.5 周）
> **关联设计**: [`docs/superpowers/specs/2026-09-22-v0.6-result-error-handling-design.md`](../../docs/superpowers/specs/2026-09-22-v0.6-result-error-handling-design.md)
> **关联 proposal**: [`proposal.md`](./proposal.md)
> **关联 specs**: [`specs/plugin-error-result-paradigm/spec.md`](./specs/plugin-error-result-paradigm/spec.md)
> **关联 ADR-047**: `docs/architecture/adr/ADR-047-static-config-result-paradigm.md`（待新建）
>
> **Metis 修订 (2026-09-22)**：移除 2 个查询型 API（`node_of_logic_stage`/`get_ctrl_link`），迁移列表 12 → 10；P2 数字从 53 个 throw 修订为 5 个 throw + ~30 个隐式中断；Check 12 升级 4 个 CMake 文件（含 `tools/verilator_runner/CMakeLists.txt`）。

---

## 1. P0 阶段 — C++23 升级（6h / 1 人天）

> **Metis 修订**：总工作量 8h → 6h，因移除 2 个查询型 API（`node_of_logic_stage`/`get_ctrl_link`）。

### 1.1 C++23 编译标志升级

- [ ] 1.1.1 修改 `CMakeLists.txt` L24：`CMAKE_CXX_STANDARD 17 → 23`
- [ ] 1.1.2 修改 `src/cf_plugin/CMakeLists.txt` L45：`cxx_std_17 → cxx_std_23`
- [ ] 1.1.3 修改 `tests/CMakeLists.txt` L50：`cxx_std_20 → cxx_std_23`（chipforge_tests target）
- [ ] 1.1.4 修改 `tests/CMakeLists.txt` L130：`cxx_std_20 → cxx_std_23`（chipforge_tests_chmem target）
- [ ] 1.1.5 **新增** 修改 `tools/verilator_runner/CMakeLists.txt` L33：`cxx_std_20 → cxx_std_23`（Verilator sim runner，Metis H5 修订，第 4 个 CMake 文件）
- [ ] 1.1.6 **Gate 0.1**：`cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)` 编译通过
- [ ] 1.1.7 **Gate 0.1**：`ctest --test-dir build --output-on-failure -j$(nproc)` **100% PASS**（零 regression）
- [ ] 1.1.8 **新增** 临时加 `-Wno-unused-result` 到 `src/cf_plugin/CMakeLists.txt`（Metis A3 修订，抑制 `std::expected` `[[nodiscard]]` 警告；P1 阶段移除）

### 1.2 PluginError 类型 + Result 别名设计

- [ ] 1.2.1 新建 `include/cf/plugin/plugin_error.h`（~120 行）
  - 定义 `enum class PluginError`（20 字段）
  - 定义 `template<typename T> using Result = std::expected<T, PluginError>`
  - 实现 `std::string plugin_error_message(PluginError, const std::string& detail = "")` 工厂
  - 实现 `PluginException to_exception(PluginError, const std::string& stage = "")` 转换
- [ ] 1.2.2 新建 `include/cf/plugin/result_macros.h`（~30 行）
  - 定义 `PB_TRY(expr)` 内部传播宏
  - 定义 `PB_EXPECT(cond, err)` 条件检查宏
  - 定义 `auto_throw<T>(Result<T>)` 业务抹平模板
- [ ] 1.2.3 增强 `include/cf/plugin/plugin_exception.h`
  - 新增 `PluginException(PluginError err, std::string stage = "")` 构造重载
  - 保留 38 行既有实现（向后兼容）

### 1.3 PipeBuilder **10 个** P0 API 迁移

> **Metis 修订**：原 12 API 中移除 `node_of_logic_stage` 与 `get_ctrl_link`（保留 `shared_ptr<T>`/`nullptr` 返回，避免 ~185 处业务 plugin 连锁影响）。剩余 **10 个 API** 改为 `Result` 返回。

- [ ] 1.3.1 修改 `include/cf/plugin/pipe_builder.h`：`register_plugin` → `Result<void>`（删除 `throw std::invalid_argument("plugin is null")`）
- [ ] 1.3.2 修改 `at_stage` → `Result<void>`（删除 2 处 throw）
- [ ] 1.3.3 修改 `declare_substage` → `Result<void>`（新增 `EmptyParentStageName` 校验）
- [ ] 1.3.4 修改 `register_commit_hook` → `Result<void>`（删除 throw）
- [ ] 1.3.5 修改 `register_ctrl_link` → `Result<void>`（删除 2 处 throw）
- [ ] 1.3.6 修改 `register_stage_payload_connector<T>` → `Result<void>`（删除 2 处 throw；模板签名明确：`template<typename T> Result<void> register_stage_payload_connector(const std::string& stage_name, const Payload<T>& key, std::function<...> connector)`）
- [ ] 1.3.7 修改 `build` → `Result<void>` + 内部 try-catch 包装 plugin callback（捕获 `PluginException` → `PluginBuildFailed`，捕获 `std::exception` → `BuildFailed`，捕获 `...` → `BuildFailed` 并输出 `unknown_exception` log）
- [ ] 1.3.8 修改 `elaborate(ch::core::context&)` / `elaborate()` → `Result<void>` / `Result<void>`（无参版本内部用 `PB_TRY(elaborate(*ctx_))`；替换 `throw PluginException("ctx_ is null")` → `return std::unexpected(NullContextPtr)`）
- [ ] 1.3.9 修改 `to_verilog(string)` → `Result<void>`（同上）
- [ ] 1.3.10 修改 `create_simulator()` → `Result<std::unique_ptr<ch::Simulator>>`（同上）
- [ ] 1.3.11 ~~`node_of_logic_stage` → `Result<shared_ptr<PipeNode>>`~~ **删除**（Metis H1 修订：保持 `shared_ptr<PipeNode>` 或 `nullptr` 返回，留 v0.7 单独 change）
- [ ] 1.3.12 ~~`get_ctrl_link` → `Result<shared_ptr<CtrlLink>>`~~ **删除**（同上）
- [ ] 1.3.13 **Gate 0.3**：`cmake --build build -j$(nproc)` 编译通过（业务代码 throw 路径暂未改，应能通过编译）
- [ ] 1.3.14 **Gate 0.3**：`ctest --test-dir build --output-on-failure -j$(nproc)` **100% PASS**（行为不变，签名变）

---

## 2. P1 阶段 — 业务侧 + ADR + CI（7h / 1 人天）

### 2.1 CpuFactory 内部 Result 化

- [ ] 2.1.1 修改 `ip/cpu/cpu_factory.h`：新增 `CpuFactory::build_cpu_impl(...)` 返回 `Result<std::unique_ptr<PipeBuilder>>`
- [ ] 2.1.2 `build_cpu_impl` 内部用 `PB_TRY` 包裹所有 7 个 `register_plugin` 调用
- [ ] 2.1.3 `build_cpu_impl` 内部用 `PB_TRY` 包裹所有 ~80 个 `at_stage` 调用
- [ ] 2.1.4 `build_cpu_impl` 内部用 `PB_TRY` 包裹 `register_ctrl_link` / `register_commit_hook` / `build` / `elaborate`
- [ ] 2.1.5 `build_cpu_impl` 内部用 `PB_TRY` 包裹 `register_stage_payload_connector<T>`（CH_MEM 路径）
- [ ] 2.1.6 保留 `CpuFactory::build_cpu()` 对外 `throw PluginException` 包装层（用 `auto_throw` 实现）
- [ ] 2.1.7 同步改造 `ip/cpu/cpu_factory_chmem.h`（CH_MEM 版本）
- [ ] 2.1.8 **Gate 1.1**：`cmake --build build -j$(nproc)` 编译通过

### 2.2 cpu_sim CLI 入口抹平

- [ ] 2.2.1 修改 `tools/cpu_sim/main.cpp`：用 `auto_throw(build_cpu(config, mem))` 抹平
- [ ] 2.2.2 验证 `--elf` / `--config` / `--mode chmem` CLI flag 不变
- [ ] 2.2.3 **Gate 1.2**：`./bin/chipforge_tests` TLM baseline 0 回归
- [ ] 2.2.4 **Gate 1.2**：`./bin/chipforge_tests_chmem` CH_MEM baseline 0 回归

### 2.3 ADR 升级

- [ ] 2.3.1 新建 `docs/architecture/adr/ADR-047-static-config-result-paradigm.md`（~80 行）
  - 状态: Accepted (v0.6.0, 2026-09-22)
  - 上下文: P1 缺口定义 + CH_MEM elaboration 不可重入
  - 决策: 12 静态配置 API 改为 Result
  - 例外: 4 项热路径清单
  - 理由 + 迁移计划
- [ ] 2.3.2 修改 `docs/architecture/adr.md` 表（L108-110 区域）：
  - 添加 `| ADR-047 | 静态配置期错误处理 Result 范式 | Plugin | ✅ v0.6.0 落地 (2026-09-22) | [adr/ADR-047-static-config-result-paradigm.md](./adr/ADR-047-static-config-result-paradigm.md) |`
- [ ] 2.3.3 补丁 ADR-026 (at_stage) ~L798：补充"`at_stage()` 返回 `Result<void>`" 说明
- [ ] 2.3.4 补丁 ADR-030 (PipeNode) ~L975：补充"`node_of_logic_stage()` 返回 expected"
- [ ] 2.3.5 补丁 ADR-031 (CtrlLink) ~L998：补充"`register_ctrl_link()` / `get_ctrl_link()` 返回 Result"
- [ ] 2.3.6 补丁 ADR-032 (PipeBuilder) ~L1022：整段扩充 Result 范式决策 + 引用 ADR-047
- [ ] 2.3.7 补丁 ADR-040 v3.0 ~L1235：行末添加"另见 ADR-047"引用
- [ ] 2.3.8 **Gate 1.3**：`bash tools/verify_adr.sh` **32/32 PASS**（31 旧 + 1 新 ADR-047）

### 2.4 CI 门禁新增

- [ ] 2.4.1 修改 `tools/check_plugin_portability.sh`：新增 **Check 10**（禁止静态配置头文件 throw）
- [ ] 2.4.2 新增 **Check 11**（API 签名同步：**10 个** P0 API 必须包含 `Result` 或 `expected`）
- [ ] 2.4.3 新增 **Check 12**（C++23 强制：**4 个** CMake 文件不能出现 `cxx_std_(17|20)`：主 CMakeLists.txt + src/cf_plugin/CMakeLists.txt + tests/CMakeLists.txt + tools/verilator_runner/CMakeLists.txt）
- [ ] 2.4.4 修改 `tools/verify_adr.sh`：添加 ADR-047 验证项（验证文件存在 + ADR-047 在 ADR 范围 case 匹配中可识别）
- [ ] 2.4.5 **Gate 1.4**：`bash tools/check_plugin_portability.sh` **12/12 PASS**（9 旧 + 3 新）
- [ ] 2.4.6 **Gate 1.4**：`bash tools/verify_plugin_decision.sh` **3+4/3 PASS**
- [ ] 2.4.7 **新增** P1 完成时移除 `src/cf_plugin/CMakeLists.txt` 的 `-Wno-unused-result` 临时抑制（Metis A3 修订；移除前需确认所有 `Result<>` 调用点已用 `PB_TRY` 包裹）

---

## 3. P2 阶段 — 测试改写（8h / 1 人天，后续 PR）

> **Metis 修订 (H2)**：原"53 个 `REQUIRE_THROWS_*` 断言"实际为 **5 个** throw 断言 + **~30 个**隐式中断断言（`REQUIRE(pb.node_of_logic_stage("x") != nullptr)` 等）。由于本 change 已移除 `node_of_logic_stage`/`get_ctrl_link` 迁移，隐式中断范围缩到仅 `register_plugin`/`at_stage` 等 10 API 改签名后的 ~30 个 `auto_throw`/`Result.has_value()` 替换点。

### 3.1 测试断言改写

- [ ] 3.1.1 `grep -rnE "REQUIRE_THROWS_AS|REQUIRE_NOTHROW|CHECK_THROWS|REQUIRE_THROWS" tests/` 全量扫描 **5 个** throw 断言
- [ ] 3.1.2 改写 `tests/framework/test_pipe_builder_register.cpp`（**~10 个** 隐式中断，`pb.register_plugin(...).has_value()` 替换）
- [ ] 3.1.3 改写 `tests/framework/test_pipe_builder_elaborate.cpp`（~8 个 `pb.elaborate().has_value()`）
- [ ] 3.1.4 改写 `tests/framework/test_pipe_builder_at_stage.cpp`（~6 个 `pb.at_stage(...).has_value()`）
- [ ] 3.1.5 改写 `tests/framework/test_payload_store.cpp`（保留 throw，不改）
- [ ] 3.1.6 改写 `tests/cpu/test_cpu_factory_config.cpp`（~12 个 `auto_throw()` 路径）
- [ ] 3.1.7 改写 `tests/soc/test_soc_l1_cache_minimal_json.cpp`（~8 个 `pb.register_*().has_value()`）
- [ ] 3.1.8 改写其余 ~6 个断言分布在 `tests/cache/`、`tests/cpu/`、`tests/mmu/`、`tests/bundles/`
- [ ] 3.1.9 **Gate 2.1**：`ctest` 100% PASS（5 个 `REQUIRE_THROWS_*` + ~30 个隐式中断 改写完成）

### 3.2 plugin_error_message 完整填写

- [ ] 3.2.1 完整填写 `plugin_error_message()` switch 全部 20 字段
- [ ] 3.2.2 添加 `plugin_error_message_test.cpp` 单测验证每条消息非空 + 包含 detail

### 3.3 文档同步

- [ ] 3.3.1 新增 `docs/architecture/error-handling.md` §9（"cf::plugin Result 范式"，~50 行）
- [ ] 3.3.2 补丁 `docs/methodology/plugin-style-design-methodology-v1.md`：补充 Result 范式到方法学约束清单
- [ ] 3.3.3 补丁 `docs/DEVELOPMENT_SETUP.md`：添加 `CMAKE_CXX_STANDARD 23` 说明
- [ ] 3.3.4 新增 `CHANGELOG.md` v0.6.0 条目（参考 v0.5.0 格式）

---

## 4. CI 门禁闸门（最终）

- [ ] 4.1 `bash tools/verify_adr.sh` **32/32 PASS**（含新 ADR-047）
- [ ] 4.2 `bash tools/check_plugin_portability.sh` **12/12 PASS**（含新 Check 10/11/12）
- [ ] 4.3 `bash tools/verify_plugin_decision.sh` **3+4/3 PASS**
- [ ] 4.4 `bash tools/doc_link_check.sh` 文档死链 0（验证 error-handling.md §9 无死链）
- [ ] 4.5 `ctest --test-dir build --output-on-failure -j$(nproc)` **100% PASS**
  - `./bin/chipforge_tests` TLM baseline 0 回归（403/403）
  - `./bin/chipforge_tests_chmem` CH_MEM baseline 0 回归（43+/43+）
- [ ] 4.6 `pre-commit run --all-files` 格式化/空白/JSON 检查通过

---

## 5. Archive（提交归档）

- [ ] 5.1 原子提交：P0 commit + P1 commit + ADR commit + CI commit（≤5 个 commit）
- [ ] 5.2 commit message 格式：`feat(plugin): <scope>` / `refactor(plugin):` / `docs(adr):` / `chore(ci):`
- [ ] 5.3 push + 创建 PR（或本地合并）
- [ ] 5.4 `openspec archive v06-static-config-result -y --skip-specs`
- [ ] 5.5 在 `CHANGELOG.md` 添加 v0.6.0 release notes

---

## 6. Acceptance (本 change scope)

### 6.1 P0 完成验收（§1 全部 checkbox）

- [x] 1.1 C++23 编译标志升级
- [x] 1.2 PluginError + Result 类型设计
- [x] 1.3 PipeBuilder 12 API 迁移
- [x] Gate 0.1 + Gate 0.3 ctest 100% PASS

### 6.2 P1 完成验收（§2 全部 checkbox）

- [x] 2.1 CpuFactory 内部 Result 化
- [x] 2.2 cpu_sim CLI 入口抹平
- [x] 2.3 ADR-047 + 现有 ADR 同步
- [x] 2.4 CI Check 10/11/12 新增
- [x] Gate 1.1 + Gate 1.2 + Gate 1.3 + Gate 1.4 全部 PASS

### 6.3 最终回归闸门（§4 全部 PASS）

- [x] 4.1 verify_adr.sh 32/32
- [x] 4.2 check_plugin_portability.sh 12/12
- [x] 4.3 verify_plugin_decision.sh PASS
- [x] 4.4 doc_link_check 0 broken
- [x] 4.5 ctest 100% PASS（TLM + CHMEM）
- [x] 4.6 pre-commit 通过

### 6.4 Out of Scope（不在本 change scope）

- ❌ 53 个 `REQUIRE_THROWS_*` 测试改写（§3，留 P2 后续 PR）
- ❌ `plugin_error_message()` 完整填写（§3.2，留 P2）
- ❌ error-handling.md §9（§3.3.1，留 P2）
- ❌ plugin-style-design-methodology-v1.md 补丁（§3.3.2，留 P2）

---

## 7. 风险与回滚

### 7.1 风险监控（每步 Gate 前必查）

| 风险 | 监控命令 | 触发条件 |
|------|----------|----------|
| R1 C++23 regression | `ctest --test-dir build` | 任何 FAIL = 立即回滚 1.1 |
| R2 CppTLM ABI 冲突 | `cmake --build build 2>&1 \| grep -i "abi\|odr\|mismatch"` | ABI 警告 = 加 visibility pragma 或 `-Wno-psabi` |
| R3 [[nodiscard]] 警告 | `cmake --build build 2>&1 \| grep warning` | ~109 个未处理点 = 系统化排查；P0 用 `-Wno-unused-result` 临时抑制 |
| R4 plugin callback throw | `tests/cpu/test_*` | 任何 build_test 异常 = 加 try-catch（已规划在 1.3.7） |

### 7.2 回滚策略

| 阶段 | 回滚方式 |
|------|---------|
| P0 0.1 升级失败 | `git revert <commit>`（4 行 CMake 改动）|
| P0 0.2 plugin_error.h 失败 | 删除新文件即可 |
| P0 0.3 API 签名改失败 | `git revert <commit>`（签名回 void）|
| P1 完成 | 整体回滚 commit，业务侧 throw 包装层兼容 |
| P2 失败 | 独立 PR，回滚不影响 P0/P1 |

### 7.3 进度跟踪

- **P0 完成度**: §1 checkbox 全部 `[x]`
- **P1 完成度**: §2 checkbox 全部 `[x]`
- **最终完成度**: §4 门禁全部 `[x]`
- **P2 后续**: 独立 PR 跟踪（`v06.1-test-assertion-migration` 或类似）

---

## 8. 参考文档

- **设计文档**：[`docs/superpowers/specs/2026-09-22-v0.6-result-error-handling-design.md`](../../docs/superpowers/specs/2026-09-22-v0.6-result-error-handling-design.md)（499 行）
- **P1 缺口定义**：[`.omo/architecture-reviews/2026-09-22-v0.5.0-phase6d-closeout.md`](../../.omo/architecture-reviews/2026-09-22-v0.5.0-phase6d-closeout.md)
- **ADR-040 v3.0**：[`docs/architecture/adr/ADR-040-tlm-hdl-portability-constraints.md`](../../docs/architecture/adr/ADR-040-tlm-hdl-portability-constraints.md)
- **ADR-046**：[`docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md`](../../docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md)
- **Oracle 报告**：内联（2026-09-22, 3m30s, A→H 8 节 + 推荐执行路径）
- **CHANGELOG v0.5.0 模板**：[`CHANGELOG.md`](../../CHANGELOG.md)

---

## 9. 元信息

- **任务清单版本**: v0.6.0-pre.1（Metis 修订版，2026-09-22）
- **任务总数**: ~56 个 (P0: 16, P1: 23, P2: 12, Archive: 5)
- **总工作量估算**: 22h（P0+P1）+ 8h（P2 后续）
- **风险等级**: 中（R6 高概率 80% 但低影响）
- **预期交付**: v0.6.0 release + ADR-047 Accepted + 12 checks CI

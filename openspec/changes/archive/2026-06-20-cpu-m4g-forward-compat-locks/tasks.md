# Tasks: cpu-m4g-forward-compat-locks

> **总工时**: ~2 天 (~108 行 header churn + ~150 行测试)
> **执行模式**: 主分支直接改（变更范围小且仅 header）
> **依赖**: M4 完成（✅ 19/19 子任务 PASS, ctest 35/35）
> **Oracle 评审**: ✅ bg_df09c224 (2026-06-17)
> **作为前置**: M4-DSE / M5-DSE 依赖本 change 完成后插件已模板化

## 1. D.1: 添加 UID / THREAD_ID / IID_PC Payloads

- [x] 1.1 修改 `ip/cpu/core/payload_common.h`，在 `keys<T, XLEN>` 结构体**现有 11 个 Key 之后**添加 3 行 `inline Payload`：`UID` (uint_t<8>, 编号 #12)、`THREAD_ID` (uint_t<2>, 编号 #13)、`IID_PC` (T, 编号 #14)
- [x] 1.2 验证 `grep -c "Payload<cf::plugin::uint_t<8>> UID" ip/cpu/core/payload_common.h` ≥ 1
- [x] 1.3 验证 `grep -c "Payload<cf::plugin::uint_t<2>> THREAD_ID" ip/cpu/core/payload_common.h` ≥ 1
- [x] 1.4 验证 `grep -c "Payload<T> IID_PC" ip/cpu/core/payload_common.h` ≥ 1
- [x] 1.5 验证现有 `test_payload_common` 7 个用例 PASS（不退化），新 Key 追加在第 11 之后
- [x] 1.6 验证 `ctest --test-dir build -R payload_common` PASS
- [x] 1.7 **[C4 修订]** 验证 `keys<T, XLEN>` 中 14 个 Payload Key 数量：`(grep -c "static inline" ip/cpu/core/payload_common.h) == 14`

## 2. D.2: 模板化 RegFilePlugin

- [x] 2.1 修改 `ip/cpu/plugins/reg_file.h`：模板签名从 `<typename T>` 改为 `<typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>`
- [x] 2.2 添加 3 个 `static_assert`：`T` 是 unsigned、`N_REGS in [1, 128]`、`N_THREADS in [1, 4]`、`N_REGS` 是 2 的幂或 1
- [x] 2.3 修改存储：`array_store<T, 32>` → `std::array<array_store<T, N_REGS>, N_THREADS>`
- [x] 2.4 修改所有 `read_reg(idx)` → `read_reg(idx, tid = 0)`，`write_reg(idx, val)` → `write_reg(idx, val, tid = 0)`
- [x] 2.5 在 `decode`/`writeback` 回调中从 `n->(KeyType::THREAD_ID)` 读取 `tid`（Phase 1 硬编码 tid=0）
- [x] 2.6 验证编译通过：`cmake --build build` 不报错
- [x] 2.7 验证现有 `test_reg_file` 4-6 用例 PASS（默认参数下行为零变化）
- [x] 2.8 **[C1 修订]** 修改 `ip/cpu/plugins/reg_file.cpp`：将 `pl::keys_rv32::DECODE`/`RS1`/`RS2`/`RD_DATA` 改为 T 推导的 `pl::keys<T, sizeof(T) * 8>::*`（5 处：line 47, 52, 58, 69, 72）。理由：D.2 模板化后 `RegFilePlugin<std::uint64_t>` 实例化需要 `keys<uint64_t, 64>` 类型，RV32 硬编码会编译失败
- [x] 2.9 **[C3 修订]** 确认 `reg_file.cpp` 显式实例化沿用默认参数写法：`template class RegFilePlugin<std::uint32_t>;` / `template class RegFilePlugin<std::uint64_t>;`（保持单参数形式，由 C++17 默认模板实参补全 `N_REGS=32, N_THREADS=1`）。不要写为全形式 `RegFilePlugin<std::uint32_t, 32, 1>`
- [x] 2.10 **[C5 修订]** 在 `ip/cpu/cpu_factory.h` stub 中添加编译期 smoke test：在 `register_late_plugins<U>` 体内添加 `RegFilePlugin<U> rf_smoke_;`（声明局部变量即可，不调用 `build()`）。验证 M4G 实施后 CpuFactory 编译能拉起模板实例化链。M4-DSE 启动前删除此 smoke test

## 3. D.2: 模板化 HazardPlugin

- [x] 3.1 修改 `ip/cpu/plugins/hazard.h`：模板签名从 `<typename T>` 改为 `<typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>`
- [x] 3.2 添加 2 个 `static_assert`：`T` 是 unsigned、`N_REGS/N_THREADS` 范围
- [x] 3.3 修改 scoreboard：`std::array<bool, 32>` → `std::array<std::array<bool, N_REGS>, N_THREADS>`
- [x] 3.4 修改所有 `has_raw(idx)` → `has_raw(idx, tid = 0)`，`has_waw(idx)` → `has_waw(idx, tid = 0)`
- [x] 3.5 在 `mark_in_flight`/`clear_in_flight` 方法接受 `tid = 0` 默认参数
- [x] 3.6 验证编译通过：`cmake --build build` 不报错
- [x] 3.7 验证现有 `test_hazard` 4-6 用例 PASS

## 4. D.2 + D.4: 模板化 BranchPredictorPlugin

- [x] 4.1 修改 `ip/cpu/plugins/branch_predictor.h`：模板签名从 `<typename T>` 改为 `<typename T, BTB_SIZE = 16, BIMODAL_SZ = 16, GSHARE_SZ = 16, GHR_BITS = 8, N_THREADS = 1>`
- [x] 4.2 添加 `static_assert` 约束：`N_THREADS in [1, 4]`
- [x] 4.3 修改 `global_history_`：`std::uint8_t` → `std::array<std::uint8_t, N_THREADS>`
- [x] 4.4 修改 `predict(T pc)` → `predict(T pc, std::uint8_t tid = 0)`，使用 `global_history_[tid]`
- [x] 4.5 修改 `update(T pc, bool taken, T target)` → `update(T pc, bool taken, T target, std::uint8_t tid = 0)`，更新 `global_history_[tid]`
- [x] 4.6 验证编译通过：`cmake --build build` 不报错
- [x] 4.7 验证现有 `test_branch_predictor` 4-6 用例 PASS（默认 tid=0）

## 5. D.3: HazardKind enum

- [x] 5.1 在 `ip/cpu/plugins/hazard.h` 添加 `enum class HazardKind : std::uint8_t { NONE, RAW_RS1, RAW_RS2, WAW };`
- [x] 5.2 修改 `has_hazard` 返回类型：`bool` → `HazardKind`
- [x] 5.3 修改 `has_hazard` 签名：`(const DecodePayload& dec)` → `(const DecodePayload& dec, std::uint8_t tid = 0)`
- [x] 5.4 实现 4 个分支检测：`RAW_RS1` (RS1 读 + has_raw)、`RAW_RS2` (RS2 读 + has_raw)、`WAW` (RD 写 + has_waw)、`NONE`（默认）
- [x] 5.5 更新 `HazardPlugin::build()` 回调中的 in-tree 调用者（1 处：`hazard.h:126`），检查 `h != HazardKind::NONE` 而非 `h == true`
- [x] 5.6 **[C2 修订]** 更新 `tests/cpu/test_hazard.cpp` 4 处断言：`assert(hz.has_hazard(dec))` → `assert(hz.has_hazard(dec) != HazardKind::NONE)`（line 34, 44, 56, 65）。理由：`enum class` 不能隐式转 `bool`，D.3 实施后 4 处编译失败
- [x] 5.7 验证 `grep -rn "has_hazard" ip/cpu/ tests/cpu/` 报告 1 个 in-tree 业务调用者（hazard.h build 回调）+ 4 个测试调用者（test_hazard.cpp 修订后）
- [x] 5.8 验证编译通过：`cmake --build build` 不报错
- [x] 5.9 验证现有 `test_hazard` 5 用例 PASS（line 34/44/56/65 修订后）

## 6. 单元测试：test_forward_compat

- [x] 6.1 新建 `tests/cpu/test_forward_compat.cpp` (~150 行)
- [x] 6.2 添加 3 个 D.1 测试：`D1_UidPayloadExists` / `D1_ThreadIdPayloadExists` / `D1_IidPcPayloadExists`
- [x] 6.3 添加 4 个 D.2 测试：`D2_RegFilePluginTemplated` (N_REGS=8) / `D2_RegFilePluginMultiThread` (N_THREADS=2) / `D2_HazardPluginMultiThread` / `D2_BranchPredictorMultiThread`
- [x] 6.4 添加 1 个 D.3 测试：`D3_HazardKindEnum` (4 个 enum 值)
- [x] 6.5 添加 1 个 D.4 测试：`D4_BranchPredictorTidParam` (per-thread GHR 隔离)
- [x] 6.6 添加 1 个回归测试：`ExistingRegFileTests` (默认参数下行为零变化)
- [x] 6.7 在 `tests/cpu/CMakeLists.txt` 注册 `test_forward_compat`
- [x] 6.8 验证 `ctest --test-dir build -R ForwardCompat` 8+/8+ PASS
- [x] 6.9 验证全量 ctest 不退化：`ctest --test-dir build` 35 + 8+ 全 PASS

## 7. 文档同步

- [x] 7.1 修改 `ip/cpu/docs/blueprint.md` §5 (CpuFactory)：添加 "M4G 后已锁定 N_REGS/N_THREADS 模板参数" 标注
- [x] 7.2 修改 `ip/cpu/docs/status.md`：添加 §4.2 "M4G 子阶段" 段，含 G.1-G.8 任务状态表
- [x] 7.3 修改 `ip/cpu/docs/README.md`：实施文档索引表添加 `M4G-forward-compat-locks.md` 行
- [x] 7.4 验证文档链接可点击：`grep -c "M4G-forward-compat-locks" ip/cpu/docs/README.md` ≥ 1
- [x] 7.5 验证 `tools/verify_no_ghost_refs.sh` 仍 PASS
- [x] 7.6 验证 `tools/verify_adr.sh` 仍 PASS（无新 ADR）

## 8. git 验收

- [x] 8.1 commit 1：`M4G: [D.1] add UID/THREAD_ID/IID_PC Payloads`（payload_common.h 改动）
- [x] 8.2 commit 2：`M4G: [D.2] template RegFilePlugin/HazardPlugin/BranchPredictorPlugin`（3 个插件模板化）
- [x] 8.3 commit 3：`M4G: [D.3] HazardKind enum + has_hazard signature`（hazard.h 改动）
- [x] 8.4 commit 4：`M4G: [G.7] add test_forward_compat suite`（新增测试）
- [x] 8.5 commit 5：`M4G: [G.8] sync blueprint/status/README docs`（文档同步）
- [x] 8.6 验证 `git diff --stat main..HEAD` 报告 ~108 行 header churn + ~150 行测试 + ~30 行文档
- [x] 8.7 验证 `tools/run_chipforge_tests.sh` 全量 PASS（35/35 + 8+ 全绿）

## 9. PR 准备

- [x] 9.1 PR description 引用 Oracle 评审 `bg_df09c224` (2026-06-17)
- [x] 9.2 PR description 引用 `ip/cpu/docs/implementation-plan/M4G-forward-compat-locks.md` 作为详细任务清单
- [x] 9.3 PR description 标注：本 change 是 M4-DSE / M5-DSE 的前置依赖
- [x] 9.4 PR description 引用 `dse_architecture_v2_locks.md` (v2.0 锁定决策文档)
- [x] 9.5 Request review
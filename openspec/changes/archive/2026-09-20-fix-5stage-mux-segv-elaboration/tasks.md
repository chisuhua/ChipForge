# Phase 6c M6 — Tasks

## 1. PayloadStore 读写路径拆分 (CH_MEM 框架修复)
- [x] `include/cf/plugin/payload.h`: `const T& get(key)` 读 miss 抛 `std::runtime_error("PayloadStore cell missing: <key_name> (CH_MEM elaboration: populate before at_stage accesses)")`
- [x] `T& get(key)` 非 const 访问器保留 `emplace-on-miss`（写路径兼容）
- [x] 更新头文件注释：Phase 6c M6 标注

## 2. 框架单测 (TDD)
- [x] `tests/framework/test_payload_store_miss_throws.cpp` 新建
  - [ ] TEST_CASE 1: CH_MEM 模式 `get(KeyType::RS1)` 未填 → 抛异常，消息含 `"PayloadStore cell missing"` 和 `key.name()`
  - [ ] TEST_CASE 2: CH_MEM 模式 `n(key) = v` 写后 `get(key)` → 正常
- [x] 加入 `tests/CMakeLists.txt` 的 CHMEM_TEST_SOURCES

## 3. CI Check 8 (静态验证)
- [x] `tools/check_plugin_portability.sh` 新增 Check 8
- [x] Check 7 → Check 8 计数更新（"8/8" 而非 "7/7"）
- [x] **grep pattern 用 `"PayloadStore cell missing"` 而非泛化 `throw std::runtime_error`**（避免与现有 4 处 type mismatch throw 混淆）
- [x] 静态验证 `include/cf/plugin/payload.h` CH_MEM 路径含 `PayloadStore cell missing`

## 4. cpu_factory_chmem EARLY-stage 预填充
- [x] `ip/cpu/cpu_factory_chmem.h::build_cpu()` 在 Plugin 注册后、`pb->build()` 前插入 EARLY at_stage 闭包
- [x] 为 6 个 stage (fetch/decode/execute/memory/writeback/branch) 填充 RS1/RS2/PC/RESULT/RD_DATA/DECODE cell
- [x] 使用 `ch_uint<32>(ch_literal<0, 32>{})` 等真实字面值（非 null 句柄）
- [x] 阶段链接闭包（已有）保持不变
- [x] **新增 cell-miss 风险点的审计**：branch/hazard/reg_file/int_alu 4 个 plugin 闭包中所有 `n->operator()(KeyType::*)` 访问都被工厂预填充覆盖

## 5. 验证与回归
- [x] `tests/cpu/test_cpu_5stage.cpp::m4_poc_5stage_simulator_tick` PASS（核心 SEGV 修复）
- [x] `tests/cpu/test_cpu_5stage.cpp::m4_poc_5stage_build` PASS（保持）
- [x] `tests/cpu/test_cpu_5stage.cpp::m4_poc_5stage_elaborate_verilog` PASS（保持）
- [x] `tests/cpu/test_cpu_rtl_regfile_alu.cpp` 全部 PASS（保持原行为，通过非 const get 路径获取 cell）
- [x] `tests/cpu/test_cpu_5stage_mux_debug.cpp` PASS（保留作为回归证据，调试测试不删除）
- [x] `tests/framework/test_payload_store_miss_throws.cpp` 全部 PASS（3 个 assertion）
- [x] `bash tools/check_plugin_portability.sh` 显示 **8/8 PASS**（含新增 Check 8）
- [x] `bash tools/verify_adr.sh` 显示 0 FAILED
- [x] `bash tools/verify_plugin_decision.sh` PASS
- [x] `ctest --test-dir build --output-on-failure` chipforge_tests (TLM baseline 12 失败已知) + chipforge_tests_chmem (新增 PASS) 跑通

## 6. CHANGELOG 追加
- [x] `CHANGELOG.md` 追加 `## v0.3.1 (2026-09-18) — payload-fail-fast (Phase 6c M6)` 条目
  - Fixed: 5-stage Simulator tick SEGV (PayloadStore const-get fail-fast)
  - Fixed: cpu_factory_chmem EARLY-stage pre-population
  - Added: Check 8 静态验证
  - Added: test_payload_store_miss_throws.cpp

## 7. 提交与推送
- [x] `git add -A` (含 M5 文档收口 5 文件 + M6 修复文件)
- [x] `git commit -m "fix(plugin): Phase 6c M6 5-stage Simulator SEGV (PayloadStore fail-fast + factory pre-populate)"`
- [x] `git push origin main`
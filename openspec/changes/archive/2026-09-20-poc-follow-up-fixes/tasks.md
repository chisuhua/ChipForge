## 1. Fix #1: M3/W6 byte-equal 完整版

- [ ] 1.1 验证 `reg_file_chmem.h` 实例成员 `regs_` 跨 context 安全 (5-stage 集成下)
  - 两个 RegFilePlugin 实例在不同 `ch::core::context` (栈分配复用同一地址) 各自 elaborate
  - 验证各自 `regs_` 持有独立 lnodeimpl*, 不悬垂不串扰
- [ ] 1.2 验证 `PayloadStore` cell put/get 在 `cpu_factory_chmem.h` 5-stage 集成下 TLM↔CH_MEM byte-equal
  - stage A at_stage 闭包 `n->operator()(KeyType::PC) = pc_val;`
  - stage B at_stage 闭包 `auto pc = n->operator()(KeyType::PC);` 读回 byte-equal
- [ ] 1.3 新增 `tests/cpu/test_cpu_rtl_regfile_alu.cpp::m3_poc_5stage_byte_equal` TEST_CASE
  - TLM reference trace (纯 C++ 模拟) vs CH_MEM Simulator trace 在 10+ cycle byte-equal
- [ ] 1.4 验证所有 byte-equal 测试 PASS, ctest 全绿

## 2. Fix #2: chbool context pollution 测试隔离

- [ ] 2.1 分析 `tests/framework/test_cppHDL_hello_poc.cpp::cpphdl_poc_chbool_contextual_conversion` 全量跑失败根因
  - 当前实现已 SECTION 入口前 `set_as_current_context()`, 但 thread_local 污染来自其他 TEST_CASE 先执行
- [ ] 2.2 用 `TEST_CASE_METHOD` fixture 显式隔离 (Metis 决策: 不用 catch2 `[.]` tag, 因 catch2 v3 隐藏测试可能被静默跳过导致伪 PASS)
  - 实现 `ChmemPocFixture` 类, 构造时 `set_as_current_context()`, 析构时 reset
  - 所有 `cpphdl_poc_*` PoC 改用 `TEST_CASE_METHOD(ChmemPocFixture, ...)`
- [ ] 2.3 验证 `./bin/chipforge_tests_chmem` 全量跑 `cpphdl_poc_chbool_contextual_conversion` 4 assertions 全部 PASS
- [ ] 2.4 验证其他 PoC 测试 (`cpphdl_poc_*` 系列) 不受 thread_local context 污染影响

## 3. Fix #3: HazardPlugin 完整 CH_MEM 版

- [ ] 3.1 `ip/cpu/plugins/hazard_chmem.h` 实装完整 RAW 检测 9 条件
  - `id_rs1 == ex_rd`, `id_rs2 == ex_rd`, `id_rs1 == mem_rd`, `id_rs2 == mem_rd`
  - `id_rs1 == wb_rd`, `id_rs2 == wb_rd`
  - x0 屏蔽: `ex_rd != 0`, `mem_rd != 0`, `wb_rd != 0`
- [ ] 3.2 新增 `tests/cpu/test_cpu_5stage.cpp::hazard_chmem_complete_elaborate` PoC
  - HazardPlugin 完整版能 elaborate + `toVerilog("hazard.v", ctx)` 输出含 9 mux_select
- [ ] 3.3 新增 `tests/cpu/test_cpu_5stage.cpp::hazard_chmem_complete_simulator_tick` PoC
  - CppHDL Simulator 跑 10+ cycle 无 SEGV, RAW 检测 halt 信号正确
- [ ] 3.4 验证 HazardPlugin 完整版与 `id_decode` 集成正确 (5-stage 流水线)

## 4. CI 门禁保留

- [ ] 4.1 `bash tools/check_plugin_portability.sh` 仍 8/8 PASS
- [ ] 4.2 `bash tools/verify_adr.sh` 仍 0 FAILED, 0 STALE
- [ ] 4.3 `bash tools/verify_plugin_decision.sh` 仍 3+4/3 PASS
- [ ] 4.4 `./bin/chipforge_tests` TLM baseline 0 回归 (391 PASS / 12 baseline 已知 FAIL)
- [ ] 4.5 `./bin/chipforge_tests_chmem` 全量 PASS (含 Fix #2 修复)

## 5. Archive

- [ ] 5.1 commit + push: `fix(plugin): Phase 6c PoC follow-up (#95 byte-equal + chbool pollution + HazardPlugin complete)`
- [ ] 5.2 CHANGELOG v0.3.2 patch (可选): PoC follow-up 修复条目
- [ ] 5.3 `openspec archive poc-follow-up-fixes --skip-validation` (验证已通过)
- [ ] 5.4 通知 `phase-6d-prerequisites` change: PoC follow-up 已 archive, prerequisites 可继续推进

## 6. 验证（Acceptance Criteria）

- [ ] `tests/cpu/test_cpu_rtl_regfile_alu.cpp::m3_poc_5stage_byte_equal` PASS（10+ cycle byte-equal）
- [ ] `tests/cpu/test_cpu_5stage.cpp::hazard_chmem_complete_elaborate` PASS
- [ ] `tests/cpu/test_cpu_5stage.cpp::hazard_chmem_complete_simulator_tick` PASS
- [ ] `./bin/chipforge_tests_chmem` 全量 PASS（含 cpphdl_poc_chbool_contextual_conversion 修复）
- [ ] `bash tools/check_plugin_portability.sh` 显示 `8/8 PASS`
- [ ] `bash tools/verify_adr.sh` 显示 0 FAILED
- [ ] `bash tools/verify_plugin_decision.sh` PASS
- [ ] TLM 兼容测试（chipforge_tests）回归：391 PASS baseline 保持

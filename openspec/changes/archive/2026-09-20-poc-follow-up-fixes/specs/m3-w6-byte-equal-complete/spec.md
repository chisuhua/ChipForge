## ADDED Requirements

### Requirement: M3/W6 byte-equal 完整版 (#95)

`reg_file_chmem.h` 实例成员 `regs_` MUST 在 5-stage 集成下跨 context 安全; `PayloadStore` cell put/get MUST 在 `cpu_factory_chmem` 5-stage 集成下 TLM↔CH_MEM 完全 byte-equal。

#### Scenario: RegFile instance member cross-context safety

- **WHEN** 两个 RegFilePlugin 实例在不同 `ch::core::context` (栈分配复用同一地址) 中各自 elaborate
- **THEN** 各自的 `regs_` (std::unique_ptr<std::array<ch_reg<ch_uint<32>>, 32>>) 持有独立 lnodeimpl* 列表, 不悬垂不串扰

#### Scenario: PayloadStore cell put/get byte-equal in 5-stage

- **WHEN** `cpu_factory_chmem.h::build_cpu()` 完整 5-stage 集成下, stage A at_stage 闭包 `n->operator()(KeyType::PC) = pc_val;`
- **THEN** stage B at_stage 闭包 `auto pc = n->operator()(KeyType::PC);` 读回值与 pc_val 在 CppHDL Simulator tick 后 byte-equal (TLM 参考)

#### Scenario: M3/W6 byte-equal PoC verification

- **WHEN** `tests/cpu/test_cpu_rtl_regfile_alu.cpp` 新增 `m3_poc_5stage_byte_equal` TEST_CASE
- **THEN** TLM reference trace (纯 C++ 模拟) vs CH_MEM Simulator trace 在 10+ cycle byte-equal

### Requirement: chbool context pollution 测试隔离

`chipforge_tests_chmem` 全量跑时 `cpphdl_poc_chbool_contextual_conversion` 因 thread_local context 残留失败的根因 MUST 修复, 不得依赖测试执行顺序。

#### Scenario: cpphdl_poc_chbool_contextual_conversion 全量跑 PASS

- **WHEN** `./bin/chipforge_tests_chmem` 全量跑 (不指定 filter)
- **THEN** `cpphdl_poc_chbool_contextual_conversion` 4 assertions 全部 PASS (与单独跑一致)
- **AND** 无其他 TEST_CASE 受 thread_local context 污染影响

#### Scenario: Test 隔离机制 (catch2 tag 或 TEST_CASE_METHOD fixture)

- **WHEN** 新增 `tests/framework/test_cppHDL_hello_poc.cpp` 改造用 catch2 `[.]` tag 或 `TEST_CASE_METHOD` fixture
- **THEN** 每个 PoC SECTION 入口前显式 `ch::core::context::set_as_current_context()` 隔离

#### Scenario: 不依赖上游 CppHDL 修复

- **WHEN** `chlib::node_builder::build_literal` 在 ctx null 时仍返回 nullptr (上游未修复)
- **THEN** 测试隔离必须 catch2 层解决, 不修改 CppHDL extern

### Requirement: HazardPlugin 完整 CH_MEM 版

`hazard_chmem.h` RAW 检测 MUST 完整化 (Phase 6c 仅 6 条件 PoC 简化版), Phase 6d 6d.2 启动前 MUST 完成完整版。

#### Scenario: HazardPlugin 完整 RAW 检测 9 条件

- **WHEN** `hazard_chmem.h::raw_hazard(id_rs1, id_rs2, ex_rd, mem_rd, wb_rd)` 实装
- **THEN** 9 条件 OR-merge: `id_rs1 == ex_rd`, `id_rs2 == ex_rd`, `id_rs1 == mem_rd`, `id_rs2 == mem_rd`, `id_rs1 == wb_rd`, `id_rs2 == wb_rd`, `ex_rd != 0`, `mem_rd != 0`, `wb_rd != 0` (x0 屏蔽)

#### Scenario: HazardPlugin 完整版 elaborate + toVerilog

- **WHEN** `tests/cpu/test_cpu_5stage.cpp::hazard_chmem_complete_elaborate` 跑
- **THEN** HazardPlugin 完整版能 elaborate + `toVerilog("hazard.v", ctx)` 输出含 9 mux_select

#### Scenario: HazardPlugin 完整版 5-stage sim

- **WHEN** `tests/cpu/test_cpu_5stage.cpp::hazard_chmem_complete_simulator_tick` 跑 (10+ cycle)
- **THEN** CppHDL Simulator 跑通无 SEGV, RAW 检测 halt 信号正确

### Requirement: CI 门禁保留

`check_plugin_portability.sh` MUST 维持 8/8 PASS + `verify_adr.sh` MUST 维持 0 FAILED + TLM baseline MUST 0 回归。

#### Scenario: check_plugin_portability.sh 8/8 PASS

- **WHEN** `bash tools/check_plugin_portability.sh` 跑
- **THEN** 8/8 PASS (含新增 Check 8 PayloadStore fail-fast)

#### Scenario: verify_adr.sh 0 FAILED

- **WHEN** `bash tools/verify_adr.sh` 跑
- **THEN** 0 FAILED, 0 STALE, 31+ PASS / 10 EXPECTED_MISSING (与 Phase 6c 一致)

#### Scenario: TLM baseline chipforge_tests 0 回归

- **WHEN** `./bin/chipforge_tests` 全量跑
- **THEN** 391 PASS / 12 FAIL (baseline 已知 12 失败保持不变, 不新增)

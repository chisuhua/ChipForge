# Phase 6c PoC Follow-up Fixes

> **关联 OpenSpec**: `openspec/changes/poc-follow-up-fixes/` (4/4 artifacts complete, openspec validate PASS)
> **关联 ADR**: ADR-040 v2.0, ADR-046
> **关联文档**: [Phase 6c lessons §已知推迟项](../lessons/phase-6c-elaboration-substrate.md)

## Why

Phase 6c `plugin-elaboration-substrate` 已完成 (v0.3.0 + v0.3.1 发布), 但端到端 PoC 验证留下 2 个 known issues 不阻塞 Phase 6c 收官但 Phase 6d 启动前必须闭环:

1. **M3/W6 byte-equal 完整版 (#95)**: `reg_file_chmem.h::get_regs()` 静态 singleton 跨 context 悬垂 + `PayloadStore` cell put/get 在 CH_MEM 模式下不稳定。Phase 6c 通过单元级 PoC 验证, 但 TLM↔CH_MEM byte-equal 在联合测试 (cpu_factory_chmem 5-stage) 上尚未完整验证。
2. **chbool context pollution**: `chipforge_tests_chmem` 全量跑时 `cpphdl_poc_chbool_contextual_conversion` 测试因 thread_local context 残留失败, 但**单独跑** PASS。Phase 6d 端到端验证需要稳定的 test 隔离。

**Why now**: Phase 6d 启动硬前置 (工具链 + ADR-037 v2.0 + PoC follow-up fixes)。**PoC follow-up fixes 必须在 Phase 6d main change 启动前闭环**, 否则 `tests/cpu/test_cpu_chmem_riscv_tests.cpp` 5 指令 `tohost=1` 端到端验证受 singleton 跨 context 悬垂影响。

## What Changes

### Fix #1: M3/W6 byte-equal 完整版 (#95)

- **`reg_file_chmem.h::get_regs()`**: 已升级为 plugin 实例成员 (`std::unique_ptr<regs_array_t> regs_`) + 延迟初始化。但 5-stage 集成下多个 RegFilePlugin 实例的 `regs_` 是否正确隔离 + 跨 context 安全复用未 PoC 验证。
- **`PayloadStore` cell put/get CH_MEM**: 在 `cpu_factory_chmem.h::build_cpu()` 5-stage 集成下, `n->operator()(KeyType::PC) = val` 后另一 stage 闭包 `n->operator()(KeyType::PC)` 读是否拿回 val 值 (而非 null impl 或 default 0) 需 byte-equal 验证。

**新增能力**: `m3_w6_byte_equal_complete` PoC 验证 5-stage 集成下 TLM↔CH_MEM 完全 byte-equal。

### Fix #2: chbool context pollution 测试隔离 (Metis/Oracle 2026-09-20 修正: 改方案 B)

- **`test_cppHDL_hello_poc.cpp::cpphdl_poc_chbool_contextual_conversion`**: 当前实现已在 SECTION 入口前 `set_as_current_context()`, 但全量跑 (其他 TEST_CASE 先执行) 时 thread_local 污染仍导致 2/4 assertion 失败。
- **修复方向**:
  - (A) **PoC 测试顺序隔离**: `chipforge_tests_chmem` 在 `cpphdl_poc_*` PoC 之间不共享 context (用 catch2 `[.]` tag 隔离或 TEST_CASE_METHOD fixture)
  - (B) **TEST_CASE_METHOD fixture**: 每个 TEST_CASE 有独立的 ChmemPocFixture, 构造时 `set_as_current_context()`

**决策 (Metis 2026-09-20)**: **采用方案 B 直接**: 方案 A 使用 `[.]` tag 是 catch2 v3 隐藏测试 (默认不包含), 可能导致"全量跑 0 tests ran"被误解为 PASS, 而非真正修复。方案 B TEST_CASE_METHOD fixture 在每个 TEST_CASE 入口显式管理 context 生命周期, 验证更可靠。

### Fix #3: HazardPlugin 完整 CH_MEM 版

- **Phase 6c W7**: `hazard_chmem.h` 仅骨架 + PoC (RAW 检测 6 条件 OR-merge)
- **Phase 6d 6d.2 启动前**: HazardPlugin 必须**完整**实装 (id_rs1/rs2 vs ex/mem/wb rd **完整检测**, 不是 PoC 6 条件简化版)

**新增能力**: `hazard_chmem_complete` PoC 验证 HazardPlugin 完整 RAW 检测 (含 5-stage 流水线回退)。

### 兼容 / 不破坏

- TLM baseline `[framework]` 0 回归 (89/89 PASS, 275 assertions)
- CH_MEM `[chmem]` + `[cpphdl]` + `[elaborate]` 全部 PASS 不变
- `check_plugin_portability.sh` 8/8 PASS 不变

## Capabilities

### New Capabilities

- `m3-w6-byte-equal-complete`: M3/W6 #95 修复 + 5-stage 集成下 TLM↔CH_MEM byte-equal PoC
- `chbool-context-pollution-fix`: PoC 测试 thread_local context 隔离 (catch2 fixture 或 TEST_CASE_METHOD)
- `hazard-chmem-complete`: HazardPlugin 完整 RAW 检测 (非 PoC 6 条件简化版)

### Modified Capabilities

(无 — 此 change 不修改 spec-level 行为, 仅修复 PoC 缺陷)

## Impact

- **影响文件**:
  - `ip/cpu/plugins/hazard_chmem.h` (Fix #3: 完整化)
  - `tests/framework/test_cppHDL_hello_poc.cpp` (Fix #2: SECTION 隔离强化)
  - `tests/cpu/test_cpu_rtl_regfile_alu.cpp` (Fix #1: byte-equal 完整版)
  - `tests/CMakeLists.txt` (Fix #2: 可选 tag 隔离)
- **测试**: 全部 `chipforge_tests_chmem` 测试 PASS (含 `cpphdl_poc_chbool_contextual_conversion` 全量跑 PASS)
- **依赖**: Phase 6d main change 启动硬前置 (M3/W6 #95 修复不完整, 5-stage 集成 byte-equal 无法保证)
- **不破坏**: TLM 兼容路径 + ADR-040 v2.0 + ADR-046 + `check_plugin_portability.sh` 8/8 PASS

## Tasks

1. **Fix #1: M3/W6 byte-equal 完整版** (2 天)
   - 验证 `reg_file_chmem.h` 实例成员 regs_ 跨 context 安全 (5-stage 集成下)
   - `PayloadStore` cell put/get 5-stage byte-equal 验证
   - 新增 `m3_poc_5stage_byte_equal` PoC
2. **Fix #2: chbool context pollution 测试隔离** (1 天)
   - 用 catch2 `[poc]` + `[.]` tag 隔离 PoC 测试执行顺序
   - 或 `TEST_CASE_METHOD` fixture 显式 set_as_current_context
   - 验证 `chipforge_tests_chmem` 全量跑 `cpphdl_poc_chbool_contextual_conversion` PASS
3. **Fix #3: HazardPlugin 完整 CH_MEM 版** (2 天)
   - `hazard_chmem.h` 完整 RAW 检测 (id_rs1/rs2 vs ex/mem/wb rd 完整 9 条件)
   - 新增 `hazard_chmem_complete_elaborate` + `hazard_chmem_complete_simulator_tick` PoC
4. **CI 门禁** (0.5 天)
   - `check_plugin_portability.sh` 仍 8/8 PASS
   - `verify_adr.sh` 仍 0 FAILED
   - `chipforge_tests_chmem` 全量 PASS
   - TLM `chipforge_tests` baseline 0 回归
5. **commit + archive** (0.5 天)
   - commit message: `fix(plugin): Phase 6c PoC follow-up (#95 byte-equal + chbool pollution + HazardPlugin complete)`
   - `openspec archive poc-follow-up-fixes`
   - CHANGELOG v0.3.2 patch (可选)

## 风险与回退

| 风险 | 回退 |
|------|------|
| Fix #2 catch2 tag 隔离不能 100% 解决 thread_local 污染 | **回退**: 拆分 `cpphdl_poc_chbool_contextual_conversion` 为独立 test binary |
| Fix #3 HazardPlugin 完整化引入新 bug | **回退**: 保留 Phase 6c 简化版, Phase 6d 6d.2 重新完整化 |
| Fix #1 byte-equal 暴露 PayloadStore 深层 bug | **回退**: 在 `const T& get(key) const` 抛 fail-fast 路径上加 `REQUIRE_NOTHROW` 包装 |

## 时间盒

| 子任务 | 估时 |
|------|------|
| Fix #1 | 2 天 |
| Fix #2 | 1 天 |
| Fix #3 | 2 天 |
| CI + commit + archive | 1 天 |
| **总计** | **~5-6 天 (1 周)** |

**Phase 6d prereqs 启动阻塞**: 此 change 完成后, Phase 6d main change 才能启动 (否则 5-stage 端到端 byte-equal 验证基础不稳)。

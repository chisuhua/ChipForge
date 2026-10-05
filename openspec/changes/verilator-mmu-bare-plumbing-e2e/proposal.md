---
initiative: wave5-isa-coverage-and-bp
priority: P2
version_target: v0.10.0
depends_on:
  - verilator-cpu-factory-extensible-params
---

# verilator-mmu-bare-plumbing-e2e — Verilator 链路 MMU Bare 模式 + TLB prefill-hit 端到端验证（**plumbing only**, v0.10.0）

## Why

`tools/verilator_runner/cpu_verilator_sim`（Phase 6d.5 E8）当前**唯一** Verilator e2e 测试 `cpu_verilator_sim_tohost1` 仅覆盖"5-stage CH_MEM CPU + Bare translation + 无 Cache"单一路径——`tests/mmu/` 9 个测试、 `tests/cache/` 7 个测试**全部走 TLM 或 CppHDL Simulator,无任何 Verilator 端到端验证**。后果:

1. wave5 `mfc-cpu-pipeline-multi-cycle-fsm` Phase G "DMIPS/MHz ≥1.4 (CH_MEM + Verilator 实测)"硬门禁**当前被现状卡住**——MFC pipeline 不能开 enable_mmu 验证 cycle-equal。
2. AGENTS.md 明确: 真 sv32 translation 测试需 `cpu-pipeline-mmufault-handler` change 闭环后翻转 `enable_mmu=sv32`（前置依赖未建,P1 priority）。

**本 change 范围（修复 C2 Oracle 审查后缩 scope）**: 引入**诚实降级为 CLI plumbing 验证**的 MMU Verilator 端到端测试。**只验证**:
- (a) `--enable-mmu --mmu-mode bare` CLI flag 透传 + `CpuFactoryChmem::build_cpu` hook 触发,
- (b) elaboration 0 error + Verilog 文件生成 + verilator --cc 编译通过,
- (c) `mmu_bare.elf` 跑至 `tohost=1` (走 no-op MMU hook + Bare identity translation)。

**重要 scope 修订说明** (修复 Oracle/Metis C2 critical): 本 change **不**包含:
- ~~TEST_CASE 2 TLB prefill-hit identity 断言~~ → TLB prefill API 行为已在 TLM/CppHDL Simulator 单元测试覆盖（`tests/mmu/test_ptw_tlb_refill_integration.cpp` 53 cases 中），重复 e2e 验证价值低且 `chipforge_tests_chmem` 不链接 MMU lib 源码会 link error
- ~~TEST_CASE 3 sv32+ppn=0 边界回归防护~~ → v0.10.4 类回归防护已在 TLM suite（`test_mmu_cache_integration.cpp` EndToEnd + `[mmu]` 53/53）覆盖；e2e 链路断言在 popen 输出中**物理不可达**（popen 拿不到 MMU 内部 `paddr`/`PTW::start_walk()`）；真 sv32 翻译由 `cpu-pipeline-mmufault-handler` follow-up 闭环后承接

**回归防护归 TLM suite 说明**: v0.10.4 hotfix 修复的 `MMUPlugin::do_lookup` Bare shortcut 误判（`ip/mmu/tlm/MMUPlugin.cpp:96-109`）防护**已在 TLM 层**（`test_mmu_cache_integration.cpp::EndToEndTranslationThroughBridge` + `[mmu]` 53/53 含 `[tlb-refill]` 2 cases 用 `satp_ppn_` workaround）。本 change 不复制 e2e 防护,**承认 CH_MEM+Verilator 链路的 MMU 语义层无防护**这一现状——此缺口由 `cpu-pipeline-mmufault-handler` follow-up 填补 (它会实装真 sv32 translation + MMU exception handler,届时才有可验证的真 e2e)。

## What Changes

### 1. 新增 Verilator MMU Bare CLI plumbing 端到端测试 (`[mmu-verilator]` family)

**位置**: `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` (CH_MEM-only, `#ifdef CF_PLUGIN_USE_CH_MEM`)

**3 个新 TEST_CASE** (修复 C2: 从原 5 个缩为 3 个, 删除 TLB prefill + sv32+ppn=0):

| # | TEST_CASE 名 | 验证内容 |
|---|-------------|---------|
| 1 | `mmu_bare_plumbing_tohost1_baseline_5_elf` | 5 个 rv32ui-p ELF (add/addi/auipc/beq/jal) 跑至 tohost=1，cycle 数 ≤ baseline × 1.2 |
| 2 | `mmu_bare_plumbing_elaboration_zero_error` | `CpuFactoryChmem::build_cpu` w/ `enable_mmu=true` + `mmu_mode="bare"` + CH_MEM elaborate 0 error + verilator --cc 编译通过 |
| 3 | `mmu_bare_plumbing_no_cache_full_chain` | `--enable-mmu --mmu-mode bare` (无 `--enable-cache`) 跑通 manual_elf mmu_bare.elf |

**关键约束** (从 change name `-bare-plumbing-` 即声明):
- 所有 TEST_CASE 注释必须显式标注 `**plumbing only — translation semantics NOT verified**`
- 不可隐式声明 sv32 翻译正确性 (留待 follow-up `cpu-pipeline-mmufault-handler` 闭环)
- `popen cpu_verilator_sim` 仅消费 stdout/stderr/tohost/cycle, **不**断言 MMU 内部 state

### 2. cycle baseline 表生成 (前置 Spike + 实测)

**位置**: `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv`

`cpu_verilator_sim --enable-mmu --mmu-mode bare --elf <rv32ui-p-X>` 跑 5 次取 median，记录各 ELF cycle 中位数到 CSV。

**cap × 1.2 数学依据** (修复 S7): `1.2` 是经验值 (20% 调度抖动容差), 在首个 baseline 生成时校准：若 stddev/median > 10%, cap 放宽至 × 1.5 (tasks.md §2 显式记录校准步骤)。CSV header 第二行附 `cap=median*1.2` 注释。

### 3. ~~SV32+PPN=0 边界回归测试~~ (修复 C2 删除)

**原计划**: 构造 sv32+ppn=0 边界配置验证 paddr != vaddr + 触发 PTW walk
**删除原因**: 
- e2e 链路 popen 拿不到 MMU 内部 state (Metis #1 Critical)
- v0.10.4 hotfix 已在 TLM suite 防护, e2e 重复价值低
- chipforge_tests_chmem 不链接 MMU lib 源码, 直接构造 MMUPlugin/MultiLevelTLB 会 link error
**承接**: `cpu-pipeline-mmufault-handler` follow-up (TEST_CASE 6) 在 cpu_pipeline 修复 + sv32 PTE vendor 后, 在 CH_MEM 真 sv32 翻译链路做端到端防护

### 4. cycle 校准实验前置 + cycle baseline CSV 落盘

**位置**: `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv` (跟踪文件)

跑 `--enable-mmu --mmu-mode bare` 与 `--enable-mmu --mmu-mode bare --enable-cache` 两组各 5 次,生成 5 ELF × 2 mode × 5 run = 50 数据点 CSV。`tests/CMakeLists.txt` 加 `target_compile_definitions(chipforge_tests_chmem PRIVATE CF_MMU_VERILATOR_BASELINE_CSV="${CMAKE_SOURCE_DIR}/tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv")`。

### 5. CHANGELOG/AGENTS.md 同步

遵守 AGENTS.md §honesty_audit 数字维护原则:
- CHANGELOG v0.10.x §Verification 段新增 3 个 `[mmu-verilator]` PASS cases (修复: 从原 5 改为 3)
- AGENTS.md "已知测试状态" 段增加:
  - `[mmu-verilator]` `3/3 PASS` (新增, real-tested 数字)
  - `[verilator]` 数字不变 (1/1, 5 ELF, Change 1 archive 后)

**注意**: 必须按 HEAD 实测写入, **禁止快照引用**。`bash tools/v0100-bootstrap.sh review` 校准。

### 6. ADR 锚点

- **ADR-040 v2.0** (✅): 9 项 CI 检查; `mmu_bare_plumbing_verilator.cpp` 走 CppHDL Simulator (非 business Plugin), 不增加 D4 约束检查
- **ADR-046 v2.0** (✅): 本 change 不实装新 FSM, 不涉及 ch_state_machine DSL
- **ADR-082** (🚧 Drafting): 本 change 不强制用 `negotiate()`, 仅消费 Change 1 留的 hook

## Capabilities

### New Capabilities

- `verilator-mmu-bare-plumbing`: 定义 `cpu_verilator_sim --enable-mmu --mmu-mode bare` 路径在 Verilator 端到端的 **CLI plumbing 验证** contract, 包括 5 ELF tohost=1 + elaboration 0 error + manual_elf full-chain。**显式声明不验证翻译语义** (TLM 层已有防护)。

### Modified Capabilities

无 (修复 S2 OpenSpec delta 流程: 本 change 缩 scope 后, 不再需要修改 `mmu-tlb-lookup-insert` + `mmu-cpptlm-bridge` 两个 existing specs——原计划修改它们是为了支持已删除的 TEST_CASE 2/3)。

**TLM 端回归防护归 TLM suite 责任** (修复 C2 后的边界声明):
- v0.10.4 hotfix class 防护责任在 `[mmu]` family (`test_mmu_cache_integration.cpp::EndToEndTranslationThroughBridge` + `[tlb-refill]` 2 cases)
- 本 change 在 CH_MEM + Verilator 链路**仅验证 CLI plumbing, MMU 语义层承认无防护**
- CH_MEM + Verilator 链路 MMU 语义层防护由 `cpu-pipeline-mmufault-handler` follow-up (TEST_CASE 6) 闭环后承接

## Impact

- **修改代码** (~120 LOC, 不包括 baseline CSV):
  - `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` (新建, ~150 LOC, 缩 scope 后)
  - `tests/CMakeLists.txt` (+5 LOC: 新 test source + baseline CSV compile_definitions)
  - `tests/cpu/manual_elf/build_mmu_bare.S` 已存在 (Change 1 vendor), 本 change 不重写
- **新增测试 cases**: **3 个 TEST_CASE** (`[mmu-verilator]` family) — 修复: 从原 5 减至 3
- **新增 baseline 数据文件**: `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv`
- **修改 specs**: 无 (修复 S2: 缩 scope 后无 spec delta)
- **新增文档**:
  - `openspec/changes/verilator-mmu-bare-plumbing-e2e/design.md` (~150 LOC)
  - `openspec/changes/verilator-mmu-bare-plumbing-e2e/tasks.md` (~120 LOC, TDD 5 步 + Spike)
  - `openspec/changes/verilator-mmu-bare-plumbing-e2e/specs/verilator-mmu-bare-plumbing/spec.md` (~70 LOC, 缩 scope 后)
  - CHANGELOG.md v0.10.x 段 +5-8 行
  - AGENTS.md "已知测试状态" 段 +2-3 行
- **依赖关系**:
  - **硬阻塞**: Change 1 `verilator-cpu-factory-extensible-params` (archive 后才启动本 change)
  - **被阻塞下游**: 真 sv32 Verilator e2e 翻转由 `cpu-pipeline-mmufault-handler` follow-up 闭环后承接
- **风险**:
  - **R1** (cycle 可比性): Change 1 baseline (CPU-only) 与本 change (enable_mmu=true, no-op hook) cycle 数差异应极小 (MMU hook 仅 emit log 不改电路), tasks.md §2 显式建表确认
  - **R2** (L1Cache CH_MEM 缺失): 本 change 不启用 `--enable-cache`, 仅 `--enable-mmu --mmu-mode bare` 无 cache, 不撞 Change 1 的 L1Cache fail-fast
  - **R3** (Sv32 walk path) **移除** (修复 C2): 不再有 sv32+ppn=0 边界测试, 不触发 PTW walk, hazard 重试循环风险移除
  - **R4** (新增, 修复 C2 后): **MMU 语义层 e2e 无防护** —— 本 change archive 后, wave5 `mfc-...` Phase G "DMIPS/MHz ≥1.4 (CH_MEM + Verilator)" 硬门禁可消费 enable_mmu 路径但**仅 plumbing**, MMU 语义验证需 `cpu-pipeline-mmufault-handler` follow-up
- **估时**: 2 周 (修复 C2: 从原 3-4 周缩至 2 周, 因 5→3 TEST_CASE + 无 spec delta)
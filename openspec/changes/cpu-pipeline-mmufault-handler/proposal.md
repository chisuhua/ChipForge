---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.10.0
status: placeholder
depends_on:
  - verilator-mmu-bare-plumbing-e2e
---

# cpu-pipeline-mmufault-handler — CPU pipeline 加 MMU exception handler（**真 sv32 Verilator e2e 翻转前置, wave5 P1 placeholder, v0.10.0**）

## Why

`verilator-mmu-bare-plumbing-e2e` (Change 2a, P2) 显式 Non-Goals §NG1 声明: **不**验证 sv32 translation 语义、PTW walk、page fault 路径——这是诚实的 plumbing-only 范围。

但**真 sv32 Verilator e2e 翻转**(AGENTS.md 明确记录) 阻塞于:
- `cpu_factory.h:390` 硬写 `/*satp_value=*/0` 让 `cfg.mmu_mode="sv32"` config inert (v0.10.0 hotfix 已部分修, 但 CPU pipeline 仍缺 MMU exception handler)
- CPU pipeline 在 vaddr=0 PTW fault 后陷入 hazard 重试循环 (无 exception handler 卸载 fault)
- AGENTS.md 原文: "真 sv32 translation 测试需 (a) `cpu-pipeline-mmufault-handler` change (CPU pipeline 加 MMU exception handler, P1 priority), 然后 (b) flip `cfg.enable_mmu=true` 在本测试 (独立跟踪)"

后果: Change 2a archive 后, [mmu-verilator] 5/5 PASS 但**仅 plumbing 验证**; wave5 `mfc-cpu-pipeline-multi-cycle-fsm` Phase G "DMIPS/MHz ≥1.4" 硬门禁**仍未达到 sv32 翻译能力**——必须先解决 CPU pipeline MMU exception handler。

**本 change 范围**: 填补 CPU pipeline MMU exception handling gap, 作为 Change 2a follow-up 的 owner, 让真 sv32 Verilator e2e 翻转可独立跟踪。

## What Changes

### 1. CPU pipeline MMU exception handler 实装

**位置**: `ip/cpu/plugins/mmu_exception_handler.h` (新建, ~100 LOC)

新增 `MmuExceptionHandlerPlugin`, 监听 `mmu_keys::EXCEPTION_CODE` Payload Key, 在 `memory` stage LATE 闭包内:
- 捕获 page fault / access fault
- 写 `cpu_keys::CPU_EXCEPTION_CODE` Payload Key
- 触发 `CtrlLink::flush_when(mmu_exception_pending)` (per ADR-045)
- 跳转到 trap handler PC (CSR `mtvec`, 简化: hardcode 0x80000010)
- 清空 hazard retry 循环 (新增 `HazardPlugin::clear_mmufault()`)

**关键约束** (Phase 6d 集成):
- 遵守 D4 无状态机禁令 — exception handler 走 `CtrlLink` flush 信号而非 enum class state
- 遵守 ADR-040 v2.0 双模约束 — `mmu_exception_handler.h` 与 `mmu_exception_handler_chmem.h` 双文件分离 (TLM + CH_MEM)
- 遵守 ADR-046 v2.0 FSM 豁免 — 仅在 trap handler PC 跳转逻辑使用 `ch_state_machine` (ch_state_machine 是 FSM DSL 例外)

### 2. `cpu_factory.h:390` 修复

**位置**: `ip/cpu/cpu_factory.h:390` (TLM 模式工厂)

当前硬写 `/*satp_value=*/0`, 让 `cfg.mmu_mode="sv32"` config inert (v0.10.0 hotfix Phase C 部分修复但 cpu_factory.h 仍未跟随)。

修复: 删除硬写 0, 改为 `cpu_config.satp_value` 透传 (helpers + ctor propagation 已在 `cpu-factory-satp-mapping` change 实装)。

### 3. `cpu_factory_chmem.h::build_cpu` 联动修复

**位置**: `ip/cpu/cpu_factory_chmem.h` (复用 Change 1 archive 后的 `enable_mmu` 参数)

当 `enable_mmu=true && mmu_mode != "bare"` 时, 在 7-plugin 流水线**注册** `MmuExceptionHandlerPlugin` (与 Change 1 的 MMUPlugin 注册逻辑绑定)。CH_MEM 版 `mmu_exception_handler_chmem.h` 同步实装。

### 4. 真 sv32 Verilator e2e 翻转（**承接 Change 2a follow-up**）

**位置**: `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` (已有文件, 不新建)

新增 TEST_CASE 6 `mmu_sv32_translation_verilator_e2e_flipped`:
- 启用 `enable_mmu=true, mmu_mode="sv32"`
- 准备 sv32 PTE 程序 ELF (新增 vendor: `tests/cpu/manual_elf/build_sv32_pte.S`)
- 跑 `cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf sv32_pte.elf --cycles 5000`
- REQUIRE tohost=1, cycle ≤ baseline × 1.5
- 断言 page fault 正确触发 (异常后 trap handler PC 跳转, 不 hazard 重试循环)

### 5. CHANGELOG/AGENTS.md 同步

遵守 AGENTS.md §honesty_audit:
- CHANGELOG v0.10.x §Verification 段新增 `[mmu-verilator]` 从 5/5 升级到 6/6 PASS (TEST_CASE 6 加入)
- AGENTS.md "[mmu-verilator]" 行更新 `5/5 PASS` → `6/6 PASS`
- AGENTS.md §Verification 段新增 `cpu-pipeline-mmufault-handler` 引用

## Capabilities

### New Capabilities

- `cpu-pipeline-mmufault-handler`: 定义 CPU pipeline 在 MMU page fault / access fault 时跳转 trap handler 的契约, 包括 hazard retry 循环清理、`CtrlLink` flush 信号、`mtvec` PC 跳转。

### Modified Capabilities

- **`mmu-cache-integration-test`** (`openspec/specs/mmu-cache-integration-test/spec.md`): 新增 "MMU exception propagation to CPU pipeline" requirement — `mmu_keys::EXCEPTION_CODE` 在 `memory` stage 消费, 触发 `CtrlLink` flush。
- **`cpu-mmu-exception-routing`** (`openspec/specs/cpu-mmu-exception-routing/spec.md`, TBD 占位): 从 TBD 升级为具体 spec, 记录 `CPU_EXCEPTION_CODE` Payload Key 写入路径。
- **`verilator-mmu-bare-plumbing`** (Change 2a spec): 新增 TEST_CASE 6 `mmu_sv32_translation_verilator_e2e_flipped` requirement (承接 Change 2a follow-up)。

## Impact

- **修改代码** (~300 LOC):
  - `ip/cpu/plugins/mmu_exception_handler.h` (新建 TLM, ~100 LOC)
  - `ip/cpu/plugins/mmu_exception_handler_chmem.h` (新建 CH_MEM, ~120 LOC)
  - `ip/cpu/cpu_factory.h:390` 修复 (satp_value 透传, ~10 LOC)
  - `ip/cpu/cpu_factory_chmem.h` 联动 (MmuExceptionHandlerPlugin 注册, ~30 LOC)
  - `tests/cpu/manual_elf/build_sv32_pte.S` (新建 vendor, ~40 LOC)
  - `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` 追加 TEST_CASE 6 (~80 LOC)
- **新增测试 cases**: 1 个 TEST_CASE (Change 2a 文件追加, 不新建文件)
- **新增 baseline 数据**: `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv` 追加 sv32 PTE 程序行
- **修改 specs**:
  - `openspec/specs/mmu-cache-integration-test/spec.md` (delta)
  - `openspec/specs/cpu-mmu-exception-routing/spec.md` (TBD → 具体)
  - `openspec/specs/verilator-mmu-bare-plumbing/spec.md` (新增 TEST_CASE 6 requirement)
- **依赖关系**:
  - **前置依赖 #1**: Change 2a `verilator-mmu-bare-plumbing-e2e` (P2, archive 后才启动本 change)
  - **前置依赖 #2**: `verilator-cpu-factory-extensible-params` (Change 1, 间接通过 Change 2a)
  - **下游承接**: 真 sv32 Verilator e2e 翻转（task 4 描述的 TEST_CASE 6）
  - **下游影响**: wave5 `mfc-cpu-pipeline-multi-cycle-fsm` Phase G "DMIPS/MHz ≥1.4" 硬门禁本 change archive 后可消费 sv32 路径
- **风险**:
  - **R1** (CPU pipeline hazard 重试循环): 修复 CPU pipeline MMU exception handler 涉及 hazard + flush + trap handler PC 联动, 估时 +1 周。
  - **R2** (Phase 6d.6/6d.7 FSM 集成): MMU exception handler 与 `mmu_ptw_chmem.h` FSM + `l1_cache_refill_fsm_chmem.h` FSM 状态机交互复杂, ADR-046 v2.0 豁免范围需明确边界。
  - **R3** (CPU pipeline stability): 引入 MMU exception 后 CPU pipeline 在普通 (非 MMU) 测试中可能不稳定, [cpu-integration] 81/81 不退化需重点验证。
- **估时**: 4-6 周（CPU pipeline 修复 + trap handler 实装 + CH_MEM + TEST_CASE 6 + 5+1=6 个回归测试）
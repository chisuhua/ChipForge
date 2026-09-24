# Phase 6d FSM CH_MEM — 6d.6 + 6d.7 + ADR-040 v3.0 + Check 9

## Why

Phase 6d 6d.6（MMU/PTW sv32 5 状态 FSM）和 6d.7（L1Cache refill FSM）是 Oracle 2026-09-20 修订路径中**剩余 CH_MEM 模式 FSM 重构工作**（audit session_id=ses_f405b4b2dffedOAwvAlKeXWW1p）。

**关键 D4 豁免**: D4 §2 禁止业务 Plugin 用状态机（`enum class State` + `switch(state_)`），但 ADR-046 明确豁免**多周期协议引擎**，条件是：
1. 文件顶部 `#define CF_PLUGIN_USE_FSM_EXEMPT`
2. 使用 `chlib::ch_state_machine` DSL（不裸 switch）

PTW（5 状态：IDLE/L0_WAIT/L1_WAIT/DONE/FAULT）与 L1Cache refill（4 状态：IDLE/LOOKUP/MISS/REFILL_WAIT）都是多周期协议引擎，符合 ADR-046 豁免条件。

**Oracle 关键修正（2026-09-20）**: MMU/PTW 是 **sv32 2-level 5 状态**，**不是 sv39 3-level**（不要 7 状态）。

## What Changes

### 6d.6 MMU/PTW sv32 5 状态 ch_state_machine

- **新建** `ip/cpu/plugins/mmu_ptw_chmem.h`
- 顶部 `#define CF_PLUGIN_USE_FSM_EXEMPT`（豁免 D4 §2）
- `enum class PTW_State { IDLE, L0_WAIT, L1_WAIT, DONE, FAULT }`（**sv32 2-level**）
- `ch_state_machine<PTW_State, 5>` DSL 实装 sv32 walk
  - IDLE → L0_WAIT（请求 L0 页目录 PTE）
  - L0_WAIT → L1_WAIT（PTE 是 huge page → fault；非 leaf → L1 页表）
  - L1_WAIT → DONE（PTE 读出 PPN + 权限检查）
  - 任何状态遇 reserved encoding / V=0 / 权限错 → FAULT
- 触发 page fault → FAULT 状态返回 exception code 到 CPU payload
- 新增 `tests/cpu/test_cpu_mmu_ptw_fsm_chmem.cpp`：3 PoC（sv32 walk success / page fault reserved / huge page fault）

### 6d.7 L1Cache refill FSM

- **新建** `ip/cache/tlm/l1_cache_refill_fsm_chmem.h`
- 顶部 `#define CF_PLUGIN_USE_FSM_EXEMPT`
- `enum class CacheState { IDLE, LOOKUP, MISS, REFILL_WAIT }`
- `ch_state_machine<CacheState, 4>` DSL 实装
  - IDLE → LOOKUP（每周期查 tag）
  - LOOKUP → IDLE（hit）或 MISS（miss）
  - MISS → REFILL_WAIT（向内存请求 cacheline）
  - REFILL_WAIT → IDLE（cacheline 回填完成）
- 新增 `tests/cpu/test_cpu_l1cache_refill_fsm_chmem.cpp`：3 PoC（hit / miss → refill / refill race）

### Check 9 (CI 扩展)

- **修改** `tools/check_plugin_portability.sh`：新增 Check 9
  - grep `CF_PLUGIN_USE_FSM_EXEMPT` 标记的文件，确认它们用 `ch_state_machine` 而非裸 switch
  - 标记豁免文件白名单（mmu_ptw_chmem.h + l1_cache_refill_fsm_chmem.h）
- 8/8 → 9/9 PASS

### ADR-040 v3.0

- **修改** `docs/architecture/adr.md`：v3.0 修订
  - 新增 §FSM 豁免段（ADR-046 引用 + CF_PLUGIN_USE_FSM_EXEMPT grep 约定）
  - 新增 §Verilator 集成段（引用 sibling CppHDL ADR-035 + 5.052 验证结果）
- ADR-046 验证（ch_state_machine DSL 已有，本 change 用之）

## Upstream Dependency (BLOCKING)

**`phase-6d-verilator-sim`**（E8 Verilator sim 跑 ELF）。Oracle 6d.6 task 6.6 明确：
> 必须 Verilator sim 验证 (ch_state_machine 简化实现不能在 CppHDL sim 跑 cycle-accurate)

本 change 必须等 `phase-6d-verilator-sim` archive 后才能跑完整 Verilator 验证 6d.6/6d.7 的 cycle-accurate FSM 测试。**路径修正**（Oracle 2026-09-21）：如该断言不准（CppHDL ch_state_machine 应该能 cycle-accurate 仿真），本 change 可独立启动 CppHDL sim 验证 6d.6/6d.7，仅 6d.5 E8 仍等 sibling ADR-035。

## Acceptance

- [ ] `ip/cpu/plugins/mmu_ptw_chmem.h` 实装（sv32 5 状态）
- [ ] `ip/cache/tlm/l1_cache_refill_fsm_chmem.h` 实装（4 状态）
- [ ] `tests/cpu/test_cpu_mmu_ptw_fsm_chmem.cpp` 3 PoC PASS
- [ ] `tests/cpu/test_cpu_l1cache_refill_fsm_chmem.cpp` 3 PoC PASS
- [ ] `tools/check_plugin_portability.sh` 9/9 PASS
- [ ] ADR-040 v3.0 文档化 + ADR-046 验证
- [ ] Verilator sim 跑 6d.6/6d.7 cycle-accurate（依赖 `phase-6d-verilator-sim`）
- [ ] `chipforge_tests_chmem` 0 回归

## Capabilities

| Capability | 新增 | 描述 |
|-----------|------|------|
| `mmu-ptw-fsm` | ✓ | MMU/PTW sv32 5 状态 ch_state_machine FSM（CH_MEM 模式） |
| `l1cache-refill-fsm` | ✓ | L1Cache refill 4 状态 ch_state_machine FSM（CH_MEM 模式） |

## Impact

- **现有 TLM 路径不变**（mmu_ptw_chmem.h 是新文件，原 `ip/mmu/tlm/MMUPlugin.{h,cpp}` 不动）
- **CI gate 新增** Check 9（grep `CF_PLUGIN_USE_FSM_EXEMPT` 豁免文件白名单）
- **ADR-040 v3.0** 接受 = Phase 6d 完整收官
- **CHANGELOG v0.5.0**（Phase 6d 全部完成）

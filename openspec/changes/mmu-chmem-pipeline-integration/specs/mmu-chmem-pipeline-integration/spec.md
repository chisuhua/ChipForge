# mmu-chmem-pipeline-integration Specification

## Purpose

This specification captures the new CH_MEM MMU/PTW pipeline integration capabilities delivered by the `mmu-chmem-pipeline-integration` change (v0.11.0 owner, Oracle 2026-10-07 locked D1-D5).

The change closes the CH_MEM MMU owner gap documented in `ip/cpu/cpu_factory_chmem.h:80-85` (`"Real sv32 CH_MEM owner TBD"`), repairs two pre-existing bugs in the TLM MMU path (Bare shortcut misjudgment + derived-class shadow), and integrates the sv32 translation result into the 5-stage CH_MEM pipeline (IBus fetch + DBus load/store + HazardPlugin mmufault_clear).

**Scope** (Oracle D1-B narrowed):
- ✅ sv32 translation (single-level TLB + 复用 PtWalkFsmPlugin)
- ✅ Bare mode identity translation
- ✅ EXCEPTION_CODE propagation + pipeline stall during PTW_ACTIVE=1
- ✅ 5-指令 ELF tohost=1 (add/addi/auipc/jal/beq + lui/sw/bne)
- ✅ Bare path byte-identical protection (zero sv32-induced latency)

**Out of scope** (deferred to v0.11.1+; see proposal §11):
- Sv39/Sv48 translation
- Multi-level TLB (L0/L1/Ln-1)
- LRU/RRIP/FIFO replacement
- ASID≠0 paths
- Permission checks (R/W/X vs access type)
- Megapage (4MB) full PPN concatenation
- trap PC jump / mepc / mtvec CSR access (CH_MEM has no CSR plugin — Oracle R11)
- CSRPlugin CH_MEM implementation
- split_id topology
- MMUTLMBridge CH_MEM

## ADDED Requirements

### Requirement: MMUPlugin CH_MEM Plugin implemented (narrow scope, Oracle D1-B)

A new CH_MEM-mode MMU plugin SHALL be implemented at `ip/mmu/tlm/MMUPlugin_chmem.h` (header-only, `#ifdef CF_PLUGIN_USE_CH_MEM` wrapped). The plugin SHALL:
- Implement a single-level direct-mapped TLB (8 entries, configurable in PR) using `ch_mem`/`ch_reg` storage.
- Reuse the existing `PtWalkFsmPlugin` from `ip/cpu/plugins/mmu_ptw_chmem.h` **without modification** for Page Table Walker FSM.
- Emit payload keys `mmu_keys<T>::PADDR`, `PADDR_VALID`, `MMU_VADDR`, `PTW_ACTIVE` at the corresponding `at_stage` closures.
- Configure `satp_value` and `satp_ppn` as elaboration-time `ch_reg` initial values (no runtime CSR write — Oracle R11 / D5-CH_MEM constraint).
- NOT implement: multi-level TLB orchestration, LRU/RRIP, ASID≠0, Sv39/Sv48, permission checks, megapage full PPN concatenation.

A corresponding `RiscvMMUPlugin_chmem.h` SHALL be implemented at `ip/cpu/plugins/mmu_chmem.h` to satisfy the `RiscvMMUPlugin` hook surface (csr_write_satp/sfence_vma/mmu_exit signatures) — but in CH_MEM mode these hooks are no-ops because there is no CSR plugin in the CH_MEM pipeline (`cpu_factory_chmem.h:304-310` registration list has no CSR plugin).

#### Scenario: MMUPlugin_chmem compiles under CF_PLUGIN_USE_CH_MEM
- **WHEN** `cmake --build build` invoked with `-DCF_PLUGIN_USE_CH_MEM=ON`
- **THEN** `ip/mmu/tlm/MMUPlugin_chmem.h` SHALL compile with 0 errors
- **AND** the existing `tests/mmu/test_mmu_chmem.cpp` (3 cases: TLB hit, TLB miss→PTW walk, PTW fault) SHALL PASS
- **AND** the `[mmu][chmem]` family ctest entry SHALL register in `tests/CMakeLists.txt`

#### Scenario: TLB hit returns correct PADDR in 1 cycle
- **WHEN** the single-level TLB contains a valid entry matching vaddr + asid=0
- **AND** `MMUPluginChmem::do_lookup` invoked at `at_stage("mmu_lookup", NORMAL)`
- **THEN** `mmu_keys<T>::PADDR` SHALL equal TLB.paddr within the same cycle
- **AND** `PADDR_VALID` SHALL be true
- **AND** `PTW_ACTIVE` SHALL remain 0 (no FSM entry)

#### Scenario: TLB miss triggers PtWalkFsmPlugin
- **WHEN** the single-level TLB has no valid entry for vaddr + asid=0
- **AND** `MMUPluginChmem::do_lookup` invoked
- **THEN** `PTW_ACTIVE` SHALL be 1 (FSM started)
- **AND** `PtWalkFsmPlugin::start` SHALL be invoked with the request vaddr
- **AND** after FSM transitions to DONE (typically L0_WAIT → L1_WAIT → DONE for sv32)
- **AND** `PADDR` SHALL equal `PtWalkFsmPlugin::result_ppn` shifted to page boundary
- **AND** `PADDR_VALID` SHALL be true and `PTW_ACTIVE` SHALL return to 0

#### Scenario: PTW V=0 fault propagates EXCEPTION_CODE
- **WHEN** `PtWalkFsmPlugin` consumes a PTE with V=0
- **THEN** `PADDR_VALID` SHALL be false
- **AND** `EXCEPTION_CODE` SHALL equal 1 (invalid PTE per RISC-V Privileged Spec §5.4)
- **AND** `PTW_ACTIVE` SHALL return to 0

#### Scenario: Bare mode identity translation
- **WHEN** `cfg.sv_mode == Bare` (or `satp_value.MODE == 0`)
- **AND** `do_lookup` invoked for any vaddr
- **THEN** `PADDR` SHALL equal vaddr (identity translation, no FSM)
- **AND** `PADDR_VALID` SHALL be true
- **AND** `PTW_ACTIVE` SHALL remain 0

### Requirement: Bare shortcut bug fix at MMUPlugin.cpp:109 (Oracle D4-A)

The buggy disjunction condition `|| satp_ppn_ == 0` at `ip/mmu/tlm/MMUPlugin.cpp:109` SHALL be removed. After the fix, `MMUPlugin::do_lookup` Bare shortcut judgement SHALL be:

```cpp
if (sv_mode_ == SvMode::Bare || satp_mode == 0) { /* identity */ }
```

The condition `satp_ppn_ == 0` SHALL NOT participate in Bare judgement. (RISC-V Privileged Spec §4.3.1: MODE=0 means Bare, all other fields including PPN are ignored — Sv-mode + PPN=0 is a legitimate Sv-mode state during CPU startup before OS writes satp CSR.)

#### Scenario: Bare shortcut 3 cases PASS
- **WHEN** ctest `[mmu][bare-shortcut]` invoked (3 test cases)
- **THEN** Case 1 (Sv32 + satp_ppn=0) SHALL reach `PADDR == vaddr` (identity translation triggers correctly)
- **AND** Case 2 (Sv32 + satp_ppn=0x12345 + TLB stub hit) SHALL reach `PADDR == TLB.paddr` (normal Sv32 path unaffected)
- **AND** Case 3 (Sv39 + satp_ppn=0xABCDE + TLB stub hit) SHALL reach `PADDR == TLB.paddr` (Sv39 path unaffected)
- **AND** the existing `[mmu] 53/53` test family SHALL maintain PASS count with zero **unexpected** regression (tests that encoded the buggy assumption may be updated with rationale)

#### Scenario: Regression preserved
- **WHEN** the Bare shortcut fix is committed
- **THEN** `[mmu] 53/53 PASS` SHALL be maintained (allow updates to tests encoding buggy assumptions with rationale)
- **AND** `[cpu-integration] 81/81 PASS` SHALL be maintained
- **AND** `[cpu-l1-mmu-demo] 6/6 PASS` SHALL be maintained

### Requirement: Shadow dual bug fix in RiscvMMUPlugin (Oracle D5-TLM, Oracle R2)

The shadow double-bug in `ip/cpu/plugins/mmu.h` and `ip/cpu/plugins/mmu.cpp` SHALL be fixed by **deleting the derived-class 三件套**:
1. Derived `set_satp_value` setter shadowing the base non-virtual function (`mmu.h:100`).
2. Derived `satp_value()` getter shadowing the base getter (`mmu.h:99`).
3. Derived redundant `satp_value_` member shadowing the base member (`mmu.h:103`).

After deletion, `RiscvMMUPlugin` SHALL inherit `MMUPlugin`'s setters/getters directly. The `RiscvMMUPlugin::csr_write_satp` implementation SHALL reuse the existing `cf::cpu::detail::extract_satp_ppn` helper (single source of truth, per `mmu.h:44-52` self-documentation). The buggy 48-bit mask `& 0x0FFFFFFFFFFFFULL` at `mmu.cpp:37` that incorrectly folds Sv32 MODE bit31 into the PPN SHALL be removed.

#### Scenario: ctor sets base satp_value correctly
- **WHEN** `RiscvMMUPlugin(Sv32, ...)` constructed
- **THEN** `MMUPlugin::satp_value()` SHALL equal `(1ULL << 31)` (Sv32 MODE bit, set by base ctor)
- **AND** `MMUPlugin::satp_ppn()` SHALL equal 0 (PPN fed later via csr_write_satp)

#### Scenario: csr_write_satp updates base satp_value (Bug #1 fix)
- **WHEN** `RiscvMMUPlugin::csr_write_satp(0x80040000)` invoked
- **THEN** `MMUPlugin::satp_value()` SHALL equal `0x80040000` (shadow fix #1: base member now updated)
- **AND** `MMUPlugin::satp_ppn()` SHALL equal `0x40000` (shadow fix #2: 48-bit mask removed)

#### Scenario: Sv32 MODE bit does not leak into PPN (Bug #2 fix)
- **WHEN** `csr_write_satp(0x80040000)` invoked (where bit31=1 is Sv32 MODE indicator)
- **THEN** `satp_ppn()` SHALL equal `0x40000`, NOT `0x80040000`
- **AND** PTW root PPN SHALL point to the page table root, NOT to a stray address with bit31 set

#### Scenario: derived getter/setter call sites audited
- **WHEN** `grep -rn "set_satp_value\|MMUPlugin::satp_value\|satp_ppn()" --include="*.cpp" --include="*.h"` invoked across codebase
- **THEN** all call sites SHALL be verified semantically compatible with the unified base accessor
- **AND** no call site SHALL break after deletion (audit recorded in PR description)

### Requirement: MmuExceptionHandler combinational network for mmufault_clear (Oracle D3-A minimum)

The stub at `ip/cpu/plugins/mmu_exception_handler_chmem.h` SHALL be extended with a combinational network that computes `mmufault_clear = (cpu_keys<T>::CPU_EXCEPTION_CODE != 0)` and routes the signal to `HazardPlugin::stall_ctrl_->halt_when(...)`. The combinational network SHALL be implemented as an `at_stage("memory", NORMAL, [...])` closure that reads `CPU_EXCEPTION_CODE` (ch_uint<8> in CH_MEM mode).

This implements the **minimum viable scope** per Oracle D3-A. trap PC jump / mepc / mtvec CSR access is **NOT** in scope because CH_MEM pipeline has no CSR plugin (`cpu_factory_chmem.h:304-310` registration list has no CSR — Oracle R11); trap PC jump is deferred to a separate change that introduces `CSRPlugin_chmem.h` (v0.11.1+ candidate).

#### Scenario: non-zero EXCEPTION_CODE produces mmufault_clear=true
- **WHEN** `cpu_keys<T>::CPU_EXCEPTION_CODE` is non-zero (any bit set)
- **THEN** `mmufault_clear` SHALL be true
- **AND** `HazardPlugin::stall_ctrl_` SHALL halt fetch (`halt_when(true)`)
- **AND** `mmufault_pending()` SHALL return true

#### Scenario: zero EXCEPTION_CODE releases stall
- **WHEN** `cpu_keys<T>::CPU_EXCEPTION_CODE` is 0
- **THEN** `mmufault_clear` SHALL be false
- **AND** `HazardPlugin::stall_ctrl_` SHALL release fetch (`halt_when(false)`)
- **AND** `mmufault_pending()` SHALL return false

#### Scenario: API surface preserved (TLM version compatibility)
- **WHEN** the CH_MEM stub is extended
- **THEN** `setup` / `build` / `reset` / `mark_mmufault` / `clear_mmufault` / `mmufault_pending` API signatures SHALL remain identical to the TLM version
- **AND** the existing `[cpu]` test family SHALL not regress

### Requirement: IBus/DBus CH_MEM sv32 translation consumption (Oracle D2-A)

The CH_MEM fetch and memory bus plugins SHALL consume the sv32 translation result emitted by `MMUPluginChmem`. Bare mode paths SHALL be **byte-identical** to the pre-change baseline (zero sv32-induced latency, zero extra combinational logic in bare path).

- `ip/cpu/plugins/ibus_chmem.h` SHALL read `mmu_keys<T>::MMU_VADDR` and `PADDR_VALID`, and use `MMU_VADDR` as the aread index when `PADDR_VALID=true` (Bare / untranslated fallback to `pc_val`).
- `ip/cpu/plugins/dmem_chmem.h` SHALL read the same payload keys for load/store paths.
- Both plugins SHALL implement `CtrlLink::halt_when(PTW_ACTIVE=1)` to freeze fetch/memory during PTW walk.
- ADR-048 canonical ordering: MMU substage closure SHALL register **before** IBus fetch NORMAL closure (orchestration with `wave5-bp-btb` owner for `fetch` stage substage declaration ownership).

#### Scenario: sv32 translated fetch uses MMU_VADDR
- **WHEN** `PADDR_VALID=true` and `MMU_VADDR` is set by `MMUPluginChmem::do_lookup`
- **AND** IBus fetch aread invoked
- **THEN** the aread index SHALL equal `MMU_VADDR` (translated)
- **AND** instruction fetch SHALL use the translated address, NOT `pc_val`

#### Scenario: Bare mode byte-identical
- **WHEN** `sv_mode == Bare` (or no MMU enabled)
- **AND** IBus fetch aread invoked
- **THEN** the aread index SHALL equal `pc_val` (pre-change behavior, byte-identical)
- **AND** the existing `[verilator] 1/1 case PASS (5 ELF tohost=1)` baseline SHALL be preserved
- **AND** the existing `[cpu-integration] 81/81` baseline SHALL be preserved

#### Scenario: PTW_ACTIVE freezes fetch
- **WHEN** `mmu_keys<T>::PTW_ACTIVE` is 1 (FSM in walk)
- **THEN** IBus fetch SHALL be stalled (`CtrlLink::halt_when(true)`)
- **AND** DBus memory access SHALL be stalled
- **AND** stall SHALL release when `PTW_ACTIVE` returns to 0

#### Scenario: cpu_factory_chmem owner gap closed
- **WHEN** `cfg.enable_mmu=true` passed to `cpu_factory_chmem`
- **THEN** `MMUPluginChmem` AND `RiscvMMUPluginChmem` SHALL both register to PipeBuilder
- **AND** the `"Real sv32 CH_MEM owner TBD"` self-documented comment at `cpu_factory_chmem.h:80-85` SHALL be replaced with implementation-specific documentation
- **AND** `cfg.enable_mmu=false` (or nullopt) SHALL remain byte-identical to baseline (no plugin registered)

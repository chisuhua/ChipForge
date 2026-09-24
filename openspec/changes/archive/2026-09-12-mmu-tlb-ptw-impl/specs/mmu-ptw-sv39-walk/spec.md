## ADDED Requirements

### Requirement: PTW MUST walk Sv39 three-level page table on TLB miss

The PageTableWalker MUST perform Sv39 three-level page table walk when `MMUPlugin::at_stage("tlb_lookup_*")` detects a TLB miss. The walk MUST read PTE from three levels (L2 root, L1, L0 leaf) via **member state** in lib/, NOT via Payload Keys (lib/ has zero Payload dependency per AGENTS.md).

#### Scenario: Walk complete with leaf PTE having R/W/X/U perms
- **WHEN** TLB miss for vaddr=0x4000_0000, satp.PPN=root_paddr, sv_mode=Sv39
- **THEN** PTW MUST walk L2 PTE → L1 PTE → L0 PTE (3 PTE reads via `pte_stub_memory_[(ppn >> 12) & 0xFFF]`), then write {paddr, perms} back to TLB and `MMUPlugin` MUST write `pl::PADDR` + `pl::MMU_VADDR` on `ptw_l0` substage completion

#### Scenario: Walk complete with leaf PTE missing R perm for load access
- **WHEN** TLB miss and leaf PTE has `{R=0, W=1, X=0}` for access_type=LOAD
- **THEN** PTW MUST set internal `fault_code = 13`; MMUPlugin MUST write `pl::EXCEPTION_CODE = 13` on `ptw_l0` completion

#### Scenario: Walk complete with leaf PTE missing X perm for exec access
- **WHEN** TLB miss and leaf PTE has `{R=1, W=1, X=0}` for access_type=EXEC
- **THEN** PTW MUST set internal `fault_code = 12`; MMUPlugin MUST write `pl::EXCEPTION_CODE = 12` on `ptw_l0` completion

#### Scenario: Walk terminates with invalid L0 PTE (V=0)
- **WHEN** Sv39 walk reaches L0 and PTE has `V=0`
- **THEN** PTW MUST set internal `fault_code = 15`; MMUPlugin MUST write `pl::EXCEPTION_CODE = 15`

#### Scenario: Walk terminates with reserved PTE encoding
- **WHEN** Sv39 walk reaches L0 PTE with `R=1, W=1, X=1` (reserved encoding)
- **THEN** PTW MUST set internal `fault_code = 15`; MMUPlugin MUST write `pl::EXCEPTION_CODE = 15`

### Requirement: PTW MUST hold member state in lib/, no Payload dependency

The PTW class in `ip/mmu/lib/ptw.h` MUST hold walk state in member variables (`current_pte_l2_/l1_/l0_`, `pending_walk_`, `walk_stage_` enum). PTW MUST NOT depend on `cf::plugin::Payload` (lib/ purity enforced by AGENTS.md).

#### Scenario: PTW state across 3 cycles (member advance, not Payload)
- **WHEN** PTW::start_walk(vaddr, asid, satp_ppn) is called at cycle 0
- **THEN** cycle 0: `walk_stage_ = WalkL2`; cycle 1: `advance()` reads L2 → `walk_stage_ = WalkL1`, `current_pte_l2_` populated; cycle 2: `advance()` reads L1 → `walk_stage_ = WalkL0`; cycle 3: `advance()` reads L0 → `walk_stage_ = WriteBack`

#### Scenario: lib/ has no Payload include
- **WHEN** `grep -E '#include.*Payload' ip/mmu/lib/ptw.h ip/mmu/lib/ptw.cpp` is run
- **THEN** grep MUST return 0 matches (no Payload include in lib/)

#### Scenario: No tick() in PTW class
- **WHEN** `tools/verify_plugin_decision.sh` inspects `ip/mmu/lib/ptw.cpp` and `ip/mmu/lib/ptw.h`
- **THEN** script MUST report no `void tick()` method override

### Requirement: MMUPlugin MUST advance PTW via substage chain (no CtrlLink stall)

The `MMUPlugin` MUST advance the PTW by calling `ptw_.advance()` in three sequential `at_stage()` invocations: `ptw_l2` → `ptw_l1` → `ptw_l0`. Each cycle consumes one PTW substage. No `CtrlLink::halt_when()` call is made in this change (CtrlLink framework not ready; deferred to `mmu-cache-integration`).

#### Scenario: PTW advances via 3 substages
- **WHEN** PTW is started at cycle 0 and MMUPlugin's substage chain runs
- **THEN** cycle 0: `at_stage("ptw_l2")` runs (no-op, walk already started); cycle 1: `at_stage("ptw_l1")` runs (advance L2→L1); cycle 2: `at_stage("ptw_l0")` runs (advance L1→L0 + CheckPerms + WriteBack) and writes `pl::PADDR`/`pl::MMU_VADDR`

#### Scenario: MMUPlugin does not call CtrlLink::halt_when
- **WHEN** `grep -E 'halt_when' ip/mmu/tlm/MMUPlugin.cpp ip/mmu/tlm/MMUPlugin.h` is run
- **THEN** grep MUST return 0 matches (no CtrlLink stall in this change)

#### Scenario: Pipeline advances naturally via substage chain (3-cycle latency)
- **WHEN** MMUPlugin receives PTW miss at cycle 0
- **THEN** upstream pipeline sees `pl::PTW_ACTIVE = 1` for cycles 1-2; cycle 3 sees `pl::PADDR` written and `pl::PTW_ACTIVE` cleared

### Requirement: PTW MUST read PTE from stub memory array (HDL-friendly)

The PTW MUST read PTE values from a stub `std::array<PTE, 4096>` (32KB stub memory, compile-time sized) for Sv39 walk. Index convention: `pte_stub_memory_[(ppn >> 12) & 0xFFF]`. PTE memory integration with `cpptlm::MasterPort` is deferred to `mmu-cache-integration` change.

#### Scenario: PTE memory stub
- **WHEN** PTW reads PTE at root_paddr during Sv39 walk
- **THEN** PTW MUST read from `pte_stub_memory_[(root_ppn >> 12) & 0xFFF]` where `root_ppn = satp_ppn`

#### Scenario: Index convention determinism
- **WHEN** test fills `pte_stub_memory_[42] = test_pte` with `test_pte.ppn = 42 << 12`
- **THEN** Sv39 walk with vaddr whose root_paddr maps to index 42 MUST read `test_pte`

#### Scenario: Test fixture determinism
- **WHEN** `tests/mmu/test_mmu_plugin.cpp` constructs a fresh PTW per test case
- **THEN** `pte_stub_memory_` is zero-initialized by default (`std::array` value-init); tests MUST explicitly fill required entries before triggering walk

### Requirement: HDL-friendly constraint enforced via existing CI gates

PTW implementation MUST preserve HDL 1:1 mapping. No `std::optional`, no `std::variant`, no `virtual`, no dynamic allocation. Verified by existing `tools/check_plugin_portability.sh` (already a PR-blocking CI gate per commit f0c15fd).

#### Scenario: HDL constraint enforced via existing gate
- **WHEN** `bash tools/check_plugin_portability.sh` inspects `ip/mmu/lib/ptw.cpp` after this change
- **THEN** script MUST report no `std::optional`, no `ch_mem`/`ch_reg`/`ch_uint`/`ch::core` leakage, no early-return pattern

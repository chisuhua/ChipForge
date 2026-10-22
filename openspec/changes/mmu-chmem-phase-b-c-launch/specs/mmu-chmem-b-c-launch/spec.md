## ADDED Requirements

### Requirement: CH_MEM MMUPlugin with single-level TLB + PtWalkFsmPlugin reuse SHALL achieve sv32 translation

The CH_MEM MMU subsystem (`MMUPlugin_chmem` + `RiscvMMUPlugin_chmem`) SHALL implement sv32 translation end-to-end within the 5-stage CPU pipeline. A fetch or memory access with `cfg.enable_mmu=true` and a valid satp config SHALL route vaddr → TLB lookup (8 entries, fully associative) → on hit, emit `PADDR` + `PADDR_VALID=true`; on miss, SHALL leave `PADDR_VALID=false` (consumer falls back to the raw vaddr — byte-identical to Bare behavior, and **not** a fault). Page-table walk on miss, PTE-driven `EXCEPTION_CODE`, and TLB refill by the walker are **out of scope for this change** (2026-10-22 route-C split; owned by `chmem-ptw-walk`). Bare mode (`cfg.enable_mmu=false` or unset) SHALL be byte-identical to the pre-`MMUPlugin_chmem` baseline per ADR-040 v2.0 D2-A.

#### Scenario: sv32 TLB hit (single level) within the pipeline
- **WHEN** the CPU executes fetch/load/store with `cfg.enable_mmu=true`, `cfg.satp_mode=sv32`, and a vaddr whose VPN is present in the 8-entry TLB
- **THEN** the pipeline SHALL emit `PADDR_VALID=true` + `PADDR = tlb_entry_paddr[31:12] \| vaddr[11:0]` within 1 cycle
- **AND** `EXCEPTION_CODE=0` (hit is never a fault)
- **AND** `PTW_ACTIVE=false` (no PTW state machine engagement)
- **AND** the aread index for fetch/dmem SHALL equal `PADDR` (not the raw vaddr)

#### Scenario: sv32 TLB miss leaves PADDR_VALID=false (route C — no PTW in this change)
- **WHEN** the CPU executes fetch/load/store with `cfg.enable_mmu=true` and a vaddr whose VPN is NOT in the 8-entry TLB
- **THEN** `MMUPlugin_chmem` SHALL leave `PADDR_VALID=false` and `PTW_ACTIVE=false` (no walker)
- **AND** SHALL leave `EXCEPTION_CODE=0` (a TLB miss is not a fault)
- **AND** the consumer (`ibus_chmem` / `dmem_chmem`) SHALL fall back to the raw vaddr ⇒ behavior byte-identical to Bare
- **AND** the TLB refill port (`entry_*` ch_reg handles) SHALL remain drivable so that the deferred walker (`chmem-ptw-walk`) can refill entries without changing this contract

#### Scenario: sv32 TLB miss triggers PTW FSM and resolves to page table walk — DEFERRED to `chmem-ptw-walk`
- **WHEN** the CPU executes fetch/load/store with `cfg.enable_mmu=true` and a vaddr whose VPN is NOT in the 8-entry TLB
- **THEN** `MMUPlugin_chmem` SHALL set `PTW_ACTIVE=true`
- **AND** the pipeline SHALL halt fetch/dmem until `PTW_ACTIVE` transitions to 0 (CtrlLink::halt_when toggle)
- **AND** `PtWalkFsmPlugin` SHALL walk sv32 root PTE then level-1 PTE against the in-CH_MEM test memory backend
- **AND** upon FSM DONE, `PADDR` SHALL equal the leaf PTE's PPN shifted by 12 + page offset
- **AND** the TLB SHALL refill the matched entry (FIFO, no LRU/RRIP per D1 narrowing)
- **AND** in the next pipeline cycle, fetch/dmem SHALL resume with `PADDR_VALID=true`

#### Scenario: sv32 PTW fault (V=0 PTE) propagates exception_code=1 — DEFERRED to `chmem-ptw-walk`
- **WHEN** PTW FSM reads a PTE with V=0 from the test memory backend
- **THEN** FSM SHALL transition to FAULT state
- **AND** `MMUPlugin_chmem` SHALL emit `EXCEPTION_CODE=1` (instruction access fault for fetch, or load/store access fault for memory access per RISC-V spec §4.3.1)
- **AND** `PADDR_VALID=false`
- **AND** downstream `MmuExceptionHandlerPlugin` combinational network SHALL derive `mmufault_clear = (CPU_EXCEPTION_CODE != 0)` and propagate to HazardPlugin::stall_ctrl_->halt_when

#### Scenario: Bare mode byte-identical protection (D2-A)
- **WHEN** `cfg.enable_mmu` is unset (default nullopt) or explicitly false
- **THEN** no MMU plugin SHALL register (cpu_factory_chmem 4-parameter branch)
- **AND** `PADDR_VALID=false` always (no MMU keys emitted)
- **AND** `ibus_chmem` fetch path SHALL fall back to `mmu_vaddr` (= `pc_val`) via select expression — byte-identical to the pre-`MMUPlugin_chmem` baseline
- **AND** `dmem_chmem` load/store path SHALL fall back to `vaddr_byte_addr` (= `mem_addr`) via select expression — byte-identical to the pre-`MMUPlugin_chmem` baseline
- **AND** all `[verilator]` baseline ELF (5 ELF, 96 assertions) SHALL continue to PASS tohost=1 within cycle budget

### Requirement: Phase E archive SHALL close the mmu-chmem lifecycle

After Phase B/C/D completion (Phase A already committed `a3afcfa` 2026-10-10), `mmu-chmem-pipeline-integration` + `mmu-chmem-phase-b-c-launch` SHALL be archived in the same batch. The archive SHALL remove the runtime `fprintf` in `ip/cpu/cpu_factory_chmem.h:117-126` (replaced with the production MMU registration path), update `AGENTS.md` line 41 `workaround` markers with `[v0.11.0 follow-up: mmu-chmem-pipeline-integration]`, append v0.11.0 section entries to `CHANGELOG.md`, add two new Requirement sections to `openspec/specs/{verilator-mmu-bare-plumbing,cpu-l1-mmu-demo}/spec.md`, and the `openspec change validate mmu-chmem-phase-b-c-launch` SHALL return 0 errors. Pre-existing baseline degradation (chmem_vendored 18 ELF FAIL) is acknowledged as `feat-phase-6d-rtl-verification` Phase 6d.7 owner responsibility and SHALL NOT block the archive.

#### Scenario: mmufault exception propagation test reaches tohost=1
- **WHEN** `tests/cpu/integration/test_cpu_chmem_mmu_pipeline` runs with `cfg.enable_mmu=true` and a vendored sv32 PTE ELF (`build_sv32_pte.S`)
- **AND** `plant_identity_page_table(mem, satp_ppn_page, elf.entry_addr)` is set up
- **THEN** the test SHALL achieve `tohost=1` within 10000 cycles
- **AND** the pipeline SHALL NOT stall indefinitely (PTW FSM terminates within 5-7 cycles)

#### Scenario: Verilator sv32 e2e flip (TEST_CASE 4)
- **WHEN** `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` runs with `cfg.enable_mmu=true` and `--mmu-mode sv32`
- **AND** the Verilator harness runs `cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf sv32_pte.elf --cycles 5000`
- **THEN** `tohost == 1` SHALL be observed
- **AND** cycle count SHALL be ≤ `sv32-baseline-matrix.csv` × 1.5 (Median cycle baseline established in D.4)
- **AND** `EXCEPTION_CODE` SHALL be 0 in success case (no fault propagated)
- **AND** all existing `[mmu-verilator] 3/3` baseline SHALL continue to PASS (no regression)

# mmu-ptw-tlb-refill Specification

> **Type**: NEW
> **Date**: 2026-09-15 (revised 2026-09-16)
> **Purpose**: Contract that the PTW success callback SHALL refill the unified TLB after writing PADDR, closing the `tlb_lookup_ifetch` stall chain so that subsequent fetches hit the TLB instead of repeating the walk.
>
> **Scope note (revised)**: This spec governs **TLB refill only**. "PTW real-memory wiring" (reading PTEs from `PicolibcHostMemory` instead of `pte_stub_memory_`) is a separate concern deferred to Wave 3 `cpu-pipeline-multi-cycle`, where real PADDR consumption also lands. The Wave 2 demo uses **stub PTE planting** (see companion spec `soc-cpu-mmu-demo-topology`), not real page tables.

## ADDED Requirements

### Requirement: PTW success callback SHALL refill the unified TLB

The PTW success callback (the closure that fires after PTW walk completion, registered via `ptw_->on_success(...)` in `MMUPlugin.cpp`) SHALL, in addition to writing the physical address (PADDR) to the `tlb_lookup_ifetch` node, insert a TLB entry mapping the walked virtual address to the resolved PTE via `MultiLevelTLB::refill_from_ptw(vaddr, asid, paddr, perms)`.

The correct API is **`multi_tlb_->refill_from_ptw(vaddr, current_asid_, paddr, perms)`** (see `ip/mmu/lib/multi_level_tlb.cpp:44`), NOT `tlb_.insert(vpn, pte)`. The success callback signature is `(uint64_t paddr, uint8_t perms)` — it does NOT receive a `PTE` object. The callback closure MUST capture `this` (or an equivalent reference) to access `current_asid_` and `multi_tlb_`.

#### Scenario: PTW success inserts valid TLB entry
- **WHEN** the PTW walks a VA and resolves to a valid PTE (the `on_success` callback fires)
- **THEN** `multi_tlb_->refill_from_ptw(vaddr, current_asid_, paddr, perms)` SHALL be called BEFORE the `PTW_ACTIVE` flag is cleared
- **THEN** subsequent `tlb_lookup_ifetch` reads with the same VPN SHALL hit the TLB without triggering a new walk

#### Scenario: success callback never sees V=0
- **WHEN** the PTW walks a VA and resolves to an invalid PTE (V=0)
- **THEN** the `on_fault` callback SHALL fire (see `ptw.cpp:51-57`), NOT `on_success`
- **THEN** no TLB entry SHALL be inserted (nothing refilled on fault)
- **THEN** `PTW_ACTIVE` SHALL be cleared by the fault callback (existing behavior, `MMUPlugin.cpp:65-68`)
- **NOTE**: the V=0 path is handled by the fault callback, so there is NO "refill fails on V=0" branch to test in the success path. The page-fault trap delivery itself is deferred to Wave 4 `cpu-pipeline-exception`.

### Requirement: PTW refill order SHALL be PADDR-write then TLB-insert then ACTIVE-clear

The PTW success callback SHALL execute the following operations in order:
1. Write PADDR to the `tlb_lookup_ifetch` node (existing behavior, since commit B of `plugin-framework-stall`)
2. Insert TLB entry via `multi_tlb_->refill_from_ptw(...)` (NEW in Wave 2)
3. Clear `PTW_ACTIVE` flag (existing behavior)

#### Scenario: CtrlLink releases within the same cycle as refill
- **WHEN** the PTW success callback fires
- **THEN** by end of the callback: the `tlb_lookup_ifetch` node has the PADDR value AND the unified TLB contains the VPN entry AND `PTW_ACTIVE = false`

### Requirement: refill SHALL populate the unified TLB (I+D shared)

The MMUPlugin uses a unified `MultiLevelTLB` (`multi_tlb_`) shared by both `tlb_lookup_ifetch` and `tlb_lookup_loadstore` stages. The PTW success callback SHALL refill this unified TLB, covering both instruction-fetch and data load/store accesses to the same VPN.

#### Scenario: unified TLB refill covers both sides
- **WHEN** the MMUPlugin uses a unified TLB structure (as it does — `multi_tlb_` is the single TLB member)
- **THEN** the PTW success callback SHALL insert the PTE into the unified TLB, satisfying both IBusPlugin fetch and DBusPlugin load/store accesses to the same VPN

### Requirement: fetch stall chain SHALL close within one PTW cycle

The IBusPlugin fetch CtrlLink (`halt_when(tlb_lookup_ifetch.PTW_ACTIVE)`) SHALL release exactly once per PTW completion (no repeated stalls for the same VPN). This validates that the TLB refill is the correct fix for the `tlb_lookup_ifetch` repeated-miss bug identified in `roadmap/phase-1.5-stall-and-validate.md:195` ("`tlb_lookup_ifetch` 在 PTW 期间不 stall / 无 TLB refill").

#### Scenario: back-to-back fetches to same VPN stall at most once
- **WHEN** cycle N triggers PTW for VPN X
- **AND** cycle N completes PTW (refill TLB[X], clear PTW_ACTIVE)
- **AND** cycle N+1 fetches VPN X
- **THEN** cycle N+1 fetch SHALL NOT be stalled (TLB hit on first try after refill)

#### Scenario: different VPN still stalls when its PTE is not in TLB
- **WHEN** VPN Y is not in the TLB
- **THEN** `tlb_lookup_ifetch` SHALL miss on VPN Y and trigger a new PTW walk (correct behavior, not regressed)

## Out of scope (explicitly deferred)

- **PTW real-memory wiring** (reading PTE from `PicolibcHostMemory` instead of `pte_stub_memory_`) → Wave 3 `cpu-pipeline-multi-cycle`, coupled with real PADDR consumption.
- **Canonical stage ordering fix** (MMUPlugin registering before IBusPlugin so `tlb_lookup_ifetch` precedes `fetch`) → Wave 3, where multi-cycle stall span makes the ordering observable.
- **Page-fault trap delivery** (V=0 → trap) → Wave 4 `cpu-pipeline-exception`.

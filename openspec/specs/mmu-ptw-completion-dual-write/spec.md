# mmu-ptw-completion-dual-write Specification

## Purpose
TBD - created by archiving change mmu-cache-integration. Update Purpose after archive.
## Requirements
### Requirement: MMUPlugin PTW completion callback MUST dual-write `pl::PADDR` + `pl::MMU_VADDR`

`MMUPlugin::do_lookup()` PTW `WalkCallback` and `FaultCallback` closures (registered via `ptw_->start_walk(vaddr, asid, satp_ppn, on_success, on_fault)`) MUST capture the `vaddr` parameter by value in their capture list (`[node, vaddr](...)`) and MUST write `pl::MMU_VADDR = vaddr` in BOTH the success and fault paths. The producer-side contract guarantees no stale vaddr crosses `pb.run()` boundaries.

#### Scenario: PTW WalkCallback writes MMU_VADDR on success
- **WHEN** `ptw_->start_walk(0x40000000ULL, 0, 0x1000ULL, walk_cb, fault_cb)` is invoked and the walk eventually completes with `paddr = 0x80000000ULL`
- **THEN** `walk_cb` SHALL write `(*node)(Key::PADDR) = 0x80000000ULL` AND `(*node)(Key::MMU_VADDR) = 0x40000000ULL` (the captured vaddr, not the result paddr)

#### Scenario: PTW FaultCallback writes MMU_VADDR on fault
- **WHEN** PTW walk encounters invalid PTE (V=0) or reserved encoding (R=1,W=1,X=1) and `fault_cb(fault_code)` fires
- **THEN** `fault_cb` SHALL write `(*node)(Key::EXCEPTION_CODE) = fault_code` AND `(*node)(Key::MMU_VADDR) = 0x40000000ULL` (captured vaddr, even on fault path)

#### Scenario: Closure captures vaddr by value, not by reference
- **WHEN** the `do_lookup` lambda is invoked multiple times in sequence (different `vaddr` each time)
- **THEN** each WalkCallback/FaultCallback closure SHALL capture its own `vaddr` snapshot (capture by value `[node, vaddr]`, NOT `[&node, &vaddr]`), preventing vaddr aliasing across concurrent PTW walks

### Requirement: Bug fix MUST NOT break existing TLB hit dual-write path

The MMU_VADDR dual-write MUST be added to PTW completion paths WITHOUT modifying the existing TLB hit dual-write path (`MMUPlugin.cpp:54-55`). The hit path `(*node)(Key::PADDR) = result.paddr; (*node)(Key::MMU_VADDR) = vaddr;` MUST remain unchanged.

#### Scenario: TLB hit dual-write preserved
- **WHEN** `MMUPlugin::at_stage("tlb_lookup_*")` runs and `multi_tlb_->lookup(vaddr, asid)` returns hit
- **THEN** both `pl::PADDR` and `pl::MMU_VADDR` SHALL be written in the SAME closure iteration (atomic dual-write within `do_lookup` lambda)

### Requirement: Test coverage for closure capture pattern

A regression test SHALL exist that proves the `[node, vaddr]` capture pattern works in PTW WalkCallback. Direct PTW API testing (constructing `PTW`, calling `stub_write_pte`, `start_walk`, `advance` 3 times for Sv39) is acceptable as indirect proof that MMUPlugin's closure pattern (which uses the same lambda capture syntax) captures vaddr correctly.

#### Scenario: PTW WalkCallback closure captures vaddr
- **WHEN** `tests/mmu/test_ptw_unit.cpp` constructs a `PTW`, plants a valid leaf PTE via `stub_write_pte`, calls `start_walk(test_vaddr, ...)` with a lambda that captures `test_vaddr`, and advances 3 times (L2→L1→L0 leaf)
- **THEN** the WalkCallback SHALL fire with the captured vaddr (verifiable via test-side closure capturing `test_vaddr`)


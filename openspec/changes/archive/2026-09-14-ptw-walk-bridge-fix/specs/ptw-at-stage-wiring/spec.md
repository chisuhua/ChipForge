## ADDED Requirements

### Requirement: PTW walk MUST advance via MMUPlugin at_stage closures

`MMUPlugin::at_stage("ptw_l0")`, `at_stage("ptw_l1")`, `at_stage("ptw_l2")` (and corresponding closures for Sv32/Sv48 levels) MUST advance the PageTableWalker one step per cycle when `busy_` is true. Each closure MUST call `ptw_->advance_from_stub()` (a new PTW public method that reads from internal `pte_stub_memory_` and dispatches to `advance(pte.raw, current_level_)`). This requirement closes the mmu-cache-integration bug where PTW walks never complete (the at_stage closures were empty lambdas at commit 7).

#### Scenario: Sv39 3-level walk completes within pb.run() cycle (TLM 拓扑下 1 cycle 完成)
- **WHEN** `MMUPlugin` is constructed with Sv39 + 2-level TLB, `stub_write_pte` plants a valid L2→L1→L0 PTE chain with leaf ppn=0x80000, `lookup(vaddr=0x40000000)` returns miss, `ptw_->start_walk(0x40000000, 0, 0x1000, walk_cb, fault_cb)` is invoked
- **THEN** after `≥1` `pb.run()` cycle (current TLM topology: 3 sub-pipe closures run sequentially within one pb.run()), `walk_cb` SHALL fire with `paddr=0x80000000`, `perms != 0`, `pl::PADDR = 0x80000000`, `pl::MMU_VADDR = 0x40000000`

#### Scenario: PTW advances only while busy
- **WHEN** `ptw_->is_busy()` returns false (no active walk)
- **THEN** `advance_from_stub()` SHALL be a no-op (return immediately) without invoking any callback

#### Scenario: advance_from_stub reads stub memory at correct index
- **WHEN** `current_pte_paddr_ = 0x12345678` (any valid PTE paddr in stub memory range)
- **THEN** `advance_from_stub()` SHALL compute `idx = (current_pte_paddr_ >> 12) & 0xFFF`, read `pte_stub_memory_[idx]`, and pass `pte.raw` to `advance()` with `current_level_`

### Requirement: PTW::advance_from_stub MUST be public API

`PTW::advance_from_stub()` MUST be declared as a public method in `PTW` class (header `ip/mmu/lib/ptw.h`). The MMUPlugin at_stage closures call it via `ptw_->advance_from_stub()`. The method MUST NOT mutate `pte_stub_memory_` (read-only). It MUST NOT bypass the `advance()` state machine (must call `advance()` internally with the read PTE raw).

#### Scenario: advance_from_stub is reachable from MMUPlugin
- **WHEN** `MMUPlugin::at_stage("ptw_l0")` lambda body is compiled
- **THEN** `ptw_->advance_from_stub()` MUST be accessible (public method, not private/protected)

#### Scenario: PTW stub memory not modified by advance_from_stub
- **WHEN** `advance_from_stub()` reads `pte_stub_memory_[idx]`
- **THEN** the stub memory entry SHALL NOT be mutated; only `current_level_`, `current_pte_paddr_`, and callback state MAY be modified (per `advance()` semantics)

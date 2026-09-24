## ADDED Requirements

### Requirement: MMUPlugin MUST emit both pl::PADDR and pl::MMU_VADDR for VIPT

The `MMUPlugin::at_stage("tlb_lookup_*")` MUST emit both `pl::PADDR` (physical address, for downstream consumers like `L1CachePlugin` paddr tag) and `pl::MMU_VADDR` (mirrored virtual address, for VIPT L1 cache index lookup) on TLB hit. This is the ADR-044 §3.2 data-flow contract on the **MMU side**.

#### Scenario: TLB hit emits both paddr and vaddr
- **WHEN** `at_stage("tlb_lookup_ifetch")` finds TLB hit for vaddr=0x4000_0000, paddr=0x8000_0000
- **THEN** `pl::PADDR = 0x8000_0000` MUST be written AND `pl::MMU_VADDR = 0x4000_0000` MUST be written (within same cycle)

#### Scenario: TLB miss defers writes until walk completes
- **WHEN** `at_stage("tlb_lookup_ifetch")` triggers PTW walk
- **THEN** only `pl::PTW_ACTIVE = 1` MUST be written in cycle 0; `pl::PADDR` and `pl::MMU_VADDR` MUST be written only when PTW completes (cycle 3, in `ptw_l0` substage)

#### Scenario: Both keys are Payload<T>-typed
- **WHEN** `mmu_keys.h` is inspected
- **THEN** both `PADDR` and `MMU_VADDR` keys MUST be declared with concrete types (`cf::plugin::uint_t<64>` for `PADDR`, `cf::plugin::uint_t<64>` for `MMU_VADDR`)

### Requirement: MMU-side VIPT emission MUST be HDL-friendly

The MMU-side dual-write logic MUST preserve HDL 1:1 mapping (no business tick(), no state machine, no std::optional/virtual/dynamic_alloc). Enforced by `tools/check_plugin_portability.sh` + `tools/verify_plugin_decision.sh` (both already CI gates).

#### Scenario: HDL-friendly constraint enforced
- **WHEN** `bash tools/check_plugin_portability.sh` inspects `ip/mmu/tlm/MMUPlugin.cpp` after this change
- **THEN** script MUST report no `std::optional`, no `ch_mem`/`ch_reg`/`ch_uint` leakage, no early-return in `at_stage()`

#### Scenario: D4 compliance enforced
- **WHEN** `bash tools/verify_plugin_decision.sh` inspects `ip/mmu/tlm/MMUPlugin.cpp`
- **THEN** script MUST report no `void tick()` override, no `enum class State` / `switch (state_)` pattern

### Requirement: L1CachePlugin SHALL NOT be modified in this change

The L1Cache-side VIPT consumption (reading `pl::MMU_VADDR` for index extraction, with PIPT fallback when absent) MUST be implemented in the `mmu-cache-integration` change. This change SHALL NOT modify `L1CachePlugin`.

The L1CachePlugin side of the VIPT data-flow contract (reading `pl::MMU_VADDR` for index extraction, with PIPT fallback when absent) is **explicitly deferred** to the `mmu-cache-integration` change. This change does NOT modify `L1CachePlugin` per ADR-044 §6 Phase 1.5 roadmap.

#### Scenario: L1CachePlugin unchanged in this change
- **WHEN** `git diff --stat f217ae0..HEAD -- ip/cache/tlm/L1CachePlugin.h ip/cache/tlm/L1CachePlugin.cpp` is run after this change
- **THEN** diff MUST be empty (no L1Cache modification)

#### Scenario: L1Cache consumption readiness tracked as future work
- **WHEN** `docs/architecture/overview.md` snapshot is read after this change
- **THEN** it MUST list `mmu-cache-integration` as the next milestone with explicit "L1Cache consumes pl::MMU_VADDR" as scope

#### Scenario: Integration test scope is MMU-side only
- **WHEN** `tests/cache/test_mmu_cache_integration.cpp` is reviewed
- **THEN** test MUST verify only MMU-side dual-write consistency (`pl::PADDR` and `pl::MMU_VADDR` both set correctly on hit); MUST NOT test L1Cache consuming `pl::MMU_VADDR` (that's `mmu-cache-integration` test)

### Requirement: VIPT safety static_assert MUST hold in L1CachePlugin (existing, unchanged)

The ADR-044 §2.2 safety condition (`idx_bits + offset_bits ≤ 12`) MUST remain statically asserted in `L1CachePlugin.h` (already landed in commit baa3757).

#### Scenario: Compile-time safety in L1CachePlugin (unchanged)
- **WHEN** `ip/cache/tlm/L1CachePlugin.h` is compiled
- **THEN** `static_assert(kIdxBits + kOffsetBits <= 12, "VIPT safety...")` MUST be present and pass (8+4=12)

#### Scenario: TLBFactory rejects unsafe configuration (mmu-side enforcement)
- **WHEN** JSON config has `idx_bits=10, line_size_bytes=64` (offset_bits=6, idx+offset=16 > 12)
- **THEN** `TLBFactory::create(config)` MUST throw with message containing "VIPT safety violated" (preventing unsafe C++ template instantiation)

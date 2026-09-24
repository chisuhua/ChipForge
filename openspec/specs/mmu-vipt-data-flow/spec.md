# mmu-vipt-data-flow Specification

## Purpose
TBD - created by archiving change mmu-tlb-ptw-impl. Update Purpose after archive.
## Requirements
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

The L1CachePlugin MUST consume `pl::MMU_VADDR` (declared in `cf::ip::mmu::payload::mmu_keys<T>`) for VIPT index extraction when present, falling back to `pl::g_addr` (current issue_request path) for PIPT mode when MMU is absent. ADR-044 §3.2 data-flow contract is now FULLY enforced on both producer (MMUPlugin) and consumer (L1CachePlugin) sides.

#### Scenario: L1CachePlugin modified in this change (replaces prior "unchanged" scenario)
- **WHEN** `git diff --stat <mmu-cache-integration-base>..HEAD -- ip/cache/tlm/L1CachePlugin.cpp` is run after this change merges
- **THEN** diff MUST show modifications to L1CachePlugin.cpp `at_stage("lookup")` closure (adding MMU_VADDR has() ternary + idx_src/tag_src selection); L1CachePlugin.h MUST be unchanged

#### Scenario: L1CachePlugin consumes MMU_VADDR for VIPT index
- **WHEN** `L1CachePlugin::at_stage("lookup")` runs and the lookup node has `pl::MMU_VADDR` set (e.g., from preceding `MMUPlugin::at_stage("tlb_lookup_*")`)
- **THEN** the cache index MUST be `extract_idx(pl::MMU_VADDR)` and the cache tag MUST be `extract_tag(pl::PADDR)` (VIPT path, mirroring ADR-044 §3.2 producer-side contract)

#### Scenario: L1CachePlugin falls back to PIPT when MMU output absent
- **WHEN** `L1CachePlugin::at_stage("lookup")` runs and the lookup node does NOT have `pl::MMU_VADDR` set (e.g., unit test using `issue_request` path)
- **THEN** the cache index MUST be `extract_idx(pl::g_addr)` and the cache tag MUST be `extract_tag(pl::g_addr)` (PIPT fallback; behavior identical to pre-change baseline; 21 [cache] tests MUST remain green)

#### Scenario: has() guard prevents implicit cell creation
- **WHEN** lookup node has no MMU output (PIPT fallback path)
- **THEN** `n->has(pl::MMU_VADDR)` MUST be checked BEFORE `n->operator()(pl::MMU_VADDR)` to prevent `PayloadStore::get()` lazy-insert side effect (which would contaminate cells_ map and falsely indicate MMU presence on subsequent runs)

#### Scenario: PTW completion writes MMU_VADDR (stale vaddr prevention)
- **WHEN** PTW walk completes after TLB miss and `WalkCallback`/`FaultCallback` fires
- **THEN** the callback MUST write `pl::MMU_VADDR = <captured vaddr>` (not just `pl::PADDR = result.paddr`); producer-side contract — see `mmu-ptw-completion-dual-write` spec for closure capture pattern

### Requirement: VIPT safety static_assert MUST hold in L1CachePlugin (existing, unchanged)

The ADR-044 §2.2 safety condition (`idx_bits + offset_bits ≤ 12`) MUST remain statically asserted in `L1CachePlugin.h` (already landed in commit baa3757).

#### Scenario: Compile-time safety in L1CachePlugin (unchanged)
- **WHEN** `ip/cache/tlm/L1CachePlugin.h` is compiled
- **THEN** `static_assert(kIdxBits + kOffsetBits <= 12, "VIPT safety...")` MUST be present and pass (8+4=12)

#### Scenario: TLBFactory rejects unsafe configuration (mmu-side enforcement)
- **WHEN** JSON config has `idx_bits=10, line_size_bytes=64` (offset_bits=6, idx+offset=16 > 12)
- **THEN** `TLBFactory::create(config)` MUST throw with message containing "VIPT safety violated" (preventing unsafe C++ template instantiation)


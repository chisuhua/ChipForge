## MODIFIED Requirements

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

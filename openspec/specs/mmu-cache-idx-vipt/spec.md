# mmu-cache-idx-vipt Specification

## Purpose
TBD - created by archiving change mmu-cache-integration. Update Purpose after archive.
## Requirements
### Requirement: L1CachePlugin MUST consume `pl::MMU_VADDR` for VIPT index when present

`L1CachePlugin::at_stage("lookup")` MUST read `pl::MMU_VADDR` (Payload Key declared by `cf::ip::mmu::payload::mmu_keys<T>::MMU_VADDR`) when the key is present in the lookup node's PayloadStore. When present, the cache index MUST be derived from `pl::MMU_VADDR` using the existing `extract_idx()` helper, and the cache tag MUST be derived from `pl::PADDR` (Payload Key declared by `mmu_keys<T>::PADDR`) using the existing `extract_tag()` helper. This implements ADR-044 §3.2 VIPT data-flow contract on the cache side.

#### Scenario: VIPT index extraction when MMU output present
- **WHEN** `L1CachePlugin::at_stage("lookup")` runs and the lookup node has `pl::MMU_VADDR` set (e.g., from a preceding `MMUPlugin::at_stage("tlb_lookup_*")`)
- **THEN** the cache index SHALL be `extract_idx(MMU_VADDR)` and the cache tag SHALL be `extract_tag(PADDR)`

#### Scenario: PIPT fallback when MMU output absent
- **WHEN** `L1CachePlugin::at_stage("lookup")` runs and the lookup node does NOT have `pl::MMU_VADDR` set (e.g., unit test using `issue_request` path)
- **THEN** the cache index SHALL be `extract_idx(g_addr)` and the cache tag SHALL be `extract_tag(g_addr)` (behavior identical to pre-change baseline; zero regression for 21 [cache] tests)

### Requirement: L1Cache MUST detect MMU output via `PipeNode::has()` guard

`L1CachePlugin::at_stage("lookup")` MUST use `n->has(MMU_VADDR)` (and `n->has(PADDR)`) for presence detection BEFORE calling `n->operator()(key)` (which has lazy-insert side effect per `PayloadStore::get()` semantics, see `include/cf/plugin/payload.h:114-117`). The guard pattern MUST be `bool has_mmu = n->has(key); has_mmu ? n->operator()(key) : n->operator()(g_addr);` to prevent implicit cell creation in `cells_` map.

#### Scenario: Guard prevents implicit cell creation
- **WHEN** lookup node has no MMU output, and the L1Cache closure executes
- **THEN** after `pb.run()`, `n->has(MMU_VADDR)` SHALL still return false (cells_ map not contaminated by lazy insert)

### Requirement: L1Cache VIPT fallback MUST preserve D4 compliance

`L1CachePlugin::at_stage("lookup")` MUST express VIPT vs PIPT fallback using `if/else` full branches (D4 §2.3 — HDL 1:1 friendly, no early return). The `bool has_mmu` ternary SHALL be the sole decision point. No `enum class State` + `switch (state_)` pattern permitted.

#### Scenario: D4 early-return absence verified
- **WHEN** `tools/verify_plugin_decision.sh` runs against `ip/cache/tlm/L1CachePlugin.cpp`
- **THEN** script SHALL report no `if (cond) return;` early-return pattern in any `at_stage()` closure

### Requirement: L1Cache MUST NOT silently consume `pl::MMU_VADDR` from a stale prior transaction

The stale-vaddr risk identified by Oracle review of `mmu-tlb-ptw-impl` (commit 0 bug fix) MUST be defended by `MMUPlugin` always writing `pl::MMU_VADDR` in BOTH TLB hit AND PTW completion paths (success and fault). `L1CachePlugin::lookup` MUST NOT defensively zero `MMU_VADDR` before reading; the contract places the burden on the producer (MMU), not the consumer (Cache). See `mmu-ptw-completion-dual-write` spec for producer-side contract.

#### Scenario: TLB hit writes MMU_VADDR
- **WHEN** `MMUPlugin::at_stage("tlb_lookup_ifetch")` runs and TLB lookup returns hit
- **THEN** `pl::MMU_VADDR = vaddr` SHALL be written in the same closure as `pl::PADDR = result.paddr` (atomic dual-write)

#### Scenario: PTW completion writes MMU_VADDR
- **WHEN** PTW walk completes after TLB miss and `WalkCallback` fires
- **THEN** `pl::MMU_VADDR = vaddr` SHALL be written in the WalkCallback closure (the captured `vaddr` parameter from `start_walk()`), preventing stale vaddr from prior transaction

#### Scenario: PTW fault writes MMU_VADDR
- **WHEN** PTW walk encounters page fault and `FaultCallback` fires
- **THEN** `pl::MMU_VADDR = vaddr` SHALL be written in the FaultCallback closure, even on the fault path (L1Cache can still use vaddr for index; exception code provides fault reason)


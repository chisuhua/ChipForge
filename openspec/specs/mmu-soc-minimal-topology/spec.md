# mmu-soc-minimal-topology Specification

## Purpose
TBD - created by archiving change mmu-cache-integration. Update Purpose after archive.
## Requirements
### Requirement: `soc/mmu_minimal.json` MUST use full chain topology

The SoC topology for MMU integration testing MUST be `traffic_gen → mmu_bridge → l1_cache_bridge → memory` (full chain), NOT `traffic_gen → mmu_bridge → memory` (MMU-only). The minimal-JSON-without-cache topology would only test MMU standalone (already covered by `mmu-tlb-ptw-impl` 29/29 PASS); the full-chain topology is the unique value of this change (validates MMU→Cache VIPT data flow end-to-end).

#### Scenario: JSON declares 4 modules
- **WHEN** `soc/mmu_minimal.json` is parsed
- **THEN** `modules` array SHALL contain exactly 4 entries: `tg` (TrafficGenTLM), `mmu` (MMUTLMBridge), `l1` (L1CacheTLMBridge), `mem` (MemoryTLM)

#### Scenario: JSON declares 3 connections
- **WHEN** `soc/mmu_minimal.json` is parsed
- **THEN** `connections` array SHALL contain exactly 3 entries: tg→mmu, mmu→l1, l1→mem

### Requirement: `soc/mmu_minimal.json` MUST mirror `soc/l1_cache_minimal.json` structure

The MMU minimal JSON MUST use the same schema structure as the existing `soc/l1_cache_minimal.json` (modules array with `name`/`type`/`params`, connections array with `src`/`dst`/`latency`). The JSON file SHALL live at `soc/mmu_minimal.json` (NOT `soc/cpu/configs/mmu_minimal.json` — that directory does not exist in the repository).

#### Scenario: JSON file lives under soc/ (not soc/cpu/configs/)
- **WHEN** `git ls-files | grep mmu_minimal.json` runs in the repository root
- **THEN** exactly one file SHALL match: `soc/mmu_minimal.json`

### Requirement: MMUPlugin JSON params SHALL be valid Sv39 config

The `mmu` module's `params` in `soc/mmu_minimal.json` MUST include `sv_mode: "sv39"`, `levels: [{"name": "L0", "entries": 8, "associativity": 8, ...}, ...]` (VIPT-safe 8/8 config), and `ptw_max_inflight: 2`. The `l1` module's `params` SHALL include `num_sets: 256, idx_bits: 8, line_data_bits: 512` (matches current L1CachePlugin geometry).

#### Scenario: JSON params pass schema validation
- **WHEN** `cpptlm::validateAll(soc/mmu_minimal.json)` is called
- **THEN** validation SHALL succeed for both MMU params schema (`ip/mmu/configs/params_schema.json`) and L1Cache params schema (`ip/cache/configs/params_schema.json`)

### Requirement: `tests/soc/test_mmu_minimal_json.cpp` SHALL mirror `tests/soc/test_soc_l1_cache_minimal_json.cpp`

The test file MUST contain exactly 4 test cases mirroring the l1_cache_minimal_json test: top-level fields (name, modules, connections), module count and required params, connection count and src/dst fields, params schema strict validation. No full `instantiateAll()` simulation (deferred — would require cpptlm::run() loop wiring not in this change).

#### Scenario: Test file contains 4 test cases
- **WHEN** `./build/bin/chipforge_tests "[soc][MMUMinimalJson]"` runs
- **THEN** 4 test cases SHALL be executed and SHALL all PASS


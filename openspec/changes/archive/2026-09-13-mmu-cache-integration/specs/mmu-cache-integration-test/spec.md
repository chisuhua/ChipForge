## ADDED Requirements

### Requirement: `tests/cache/test_mmu_cache_integration.cpp` SHALL exist with ≥3 test cases

The integration test file SHALL exist at `tests/cache/test_mmu_cache_integration.cpp` and SHALL contain at least 3 `TEST_CASE` entries tagged `[cache][MMUCacheIntegration]`. Each test MUST construct a `cf::plugin::PipeBuilder` with `L1CachePlugin` and either (a) write `pl::MMU_VADDR` + `pl::PADDR` Keys directly via `lookup->put(...)` to simulate MMU output, or (b) use `issue_request` path to verify PIPT fallback.

#### Scenario: Test file exists and compiles
- **WHEN** `cmake --build build --target chipforge_tests` runs
- **THEN** `test_mmu_cache_integration.cpp` SHALL compile without errors and link into `chipforge_tests` binary

#### Scenario: Test verifies VIPT index from MMU_VADDR
- **WHEN** a `TEST_CASE` plants `lookup->put(MMU_VADDR, 0x400007F0ULL)` and `lookup->put(PADDR, 0x80000000ULL)`, pre-fills `set 0x7F` with tag `0x80000`, and calls `pb.run()`
- **THEN** `helper->read_response(lookup).hit` SHALL be `true` (idx=0x7F from vaddr matches, tag=0x80000 from paddr matches)

#### Scenario: Test verifies PIPT fallback
- **WHEN** a `TEST_CASE` calls `helper->issue_request(lookup, req)` with `req.address = 0x00012345800ULL` (no MMU Keys set), pre-fills `set 0x80` with tag `0x12345`, and calls `pb.run()`
- **THEN** `helper->read_response(lookup).hit` SHALL be `true` (idx from g_addr, tag from g_addr, behavior identical to pre-change baseline)

### Requirement: Integration test MUST NOT use MMUPlugin at_stage chain

The test SHALL directly write `pl::MMU_VADDR` / `pl::PADDR` Keys to the lookup node (mocking MMU output) rather than constructing a full `MMUPlugin` instance with full `at_stage` wiring. Rationale: isolating the cache-side consumer logic from the MMU-side producer logic, mirroring the test philosophy of `test_l1_cache_plugin_unit.cpp` (which isolates cache logic from CPU side).

#### Scenario: Test does NOT construct MMUPlugin
- **WHEN** `tests/cache/test_mmu_cache_integration.cpp` is read
- **THEN** it SHALL NOT `#include "ip/mmu/tlm/MMUPlugin.h"` and SHALL NOT call `pb.register_plugin<MMUPlugin>(...)`

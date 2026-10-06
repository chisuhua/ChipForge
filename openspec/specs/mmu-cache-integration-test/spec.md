# mmu-cache-integration-test Specification

## Purpose
TBD - created by archiving change mmu-cache-integration. Update Purpose after archive.
## Requirements
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

## ADDED Requirements

### Requirement: MMU exception propagation to CPU pipeline (cpu-pipeline-mmufault-handler v1)

When MMU PTW walk raises a fault, the exception code MUST propagate through `cpu_keys::CPU_EXCEPTION_CODE` Payload Key to `MmuExceptionHandlerPlugin::at_stage("memory", Phase::LATE)` closure. The Plugin MUST set `mmu_exception_pending_ = true` when the code is non-zero, triggering `CtrlLink::flush_when(mmu_exception_pending_)` to unload the fault via pipeline flush + trap PC jump.

#### Scenario: Page fault (code 12) sets CPU_EXCEPTION_CODE + flush
- **WHEN** `RiscvMMUPlugin::at_stage` walks L0/L1 PTE and finds V=0
- **AND** `mmu_exit` closure (`ip/cpu/plugins/mmu.cpp:128-130`) writes `cpu_keys::CPU_EXCEPTION_CODE = 12`
- **THEN** `MmuExceptionHandlerPlugin::at_stage("memory", Phase::LATE)` MUST consume code 12 and set `mmu_exception_pending_ = true`
- **AND** `CtrlLink::flush_when(mmu_exception_pending_)` MUST return true, causing PipeBuilder to skip `memory` stage all 3 phases

#### Scenario: Access fault (code 13) sets CPU_EXCEPTION_CODE + flush
- **WHEN** MMU walk encounters permission violation and writes `cpu_keys::CPU_EXCEPTION_CODE = 13`
- **THEN** `MmuExceptionHandlerPlugin` MUST set `mmu_exception_pending_ = true` and trigger `flush_when`

#### Scenario: Reserved encoding (code 15) sets CPU_EXCEPTION_CODE + flush
- **WHEN** MMU walk encounters R=1,W=1,X=1 PTE and writes `cpu_keys::CPU_EXCEPTION_CODE = 15`
- **THEN** `MmuExceptionHandlerPlugin` MUST set `mmu_exception_pending_ = true` and trigger `flush_when`

#### Scenario: Successful translation does NOT set CPU_EXCEPTION_CODE
- **WHEN** MMU walk completes successfully (TLB hit or PTW success)
- **THEN** `cpu_keys::CPU_EXCEPTION_CODE` MUST remain at default 0
- **AND** `MmuExceptionHandlerPlugin::mmu_exception_pending()` MUST be false
- **AND** pipeline continues normally without flush


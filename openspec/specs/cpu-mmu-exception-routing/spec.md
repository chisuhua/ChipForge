# cpu-mmu-exception-routing Specification

## Purpose
MMU exception routing through `cpu_keys::CPU_EXCEPTION_CODE` Payload Key from MMU PTW fault to CPU pipeline exception handler. Producer is `ip/mmu/tlm/MMUPlugin.cpp:139-141` (PTW fault writes `mmu_keys::EXCEPTION_CODE`); propagator is `ip/cpu/plugins/mmu.cpp:128-130` `mmu_exit` closure (reads `mmu_keys::EXCEPTION_CODE` and writes `cpu_keys::CPU_EXCEPTION_CODE`); consumer is `ip/cpu/plugins/mmu_exception_handler.h` `MmuExceptionHandlerPlugin::at_stage("memory", Phase::LATE)` (cpu-pipeline-mmufault-handler v1, 2026-10-06). Direction: MMU → CPU (one-way, MMU never reads CPU_EXCEPTION_CODE).
## Requirements
### Requirement: MMU exception code 12/13/15 MUST propagate to CPU exception path

When `MMUPlugin::at_stage` raises a page fault (V=0 PTE → fault 12; reserved encoding → fault 15; access fault → fault 13), the exception code MUST propagate via `pl::EXCEPTION_CODE` Payload Key to the CPU `at_stage("mmu_exit")` closure. The CPU closure MUST read this code and route it to the CPU's exception path (e.g., `mcause` CSR write or trap handler dispatch).

#### Scenario: Page fault (fault 12) propagates from MMU to CPU
- **WHEN** MMU walk encounters V=0 PTE and writes `pl::EXCEPTION_CODE = 12`
- **THEN** `RiscvMMUPlugin::at_stage("mmu_exit")` MUST read `EXCEPTION_CODE = 12` and propagate it to CPU exception path (e.g., write to `pl::CPU_EXCEPTION_CODE` or `mcause` CSR)

#### Scenario: Access fault (fault 13) propagates from MMU to CPU
- **WHEN** MMU walk encounters permission violation and writes `pl::EXCEPTION_CODE = 13`
- **THEN** the CPU exception path MUST receive `EXCEPTION_CODE = 13`

#### Scenario: Reserved encoding (fault 15) propagates from MMU to CPU
- **WHEN** MMU walk encounters R=1,W=1,X=1 PTE and writes `pl::EXCEPTION_CODE = 15`
- **THEN** the CPU exception path MUST receive `EXCEPTION_CODE = 15`

#### Scenario: Successful translation does NOT raise exception
- **WHEN** MMU walk completes successfully (TLB hit or PTW success)
- **THEN** `pl::EXCEPTION_CODE` MUST NOT be set (default 0 = no exception); CPU pipeline continues normally


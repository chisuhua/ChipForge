# Spec: chmem-dmem-read-latency

> **Capability**: CH_MEM dmem 读路径时序
> **Change**: `chmem-dmem-read-latency`
> **Created**: 2026-10-20

## ADDED Requirements

### Requirement: CH_MEM dmem 读结果 SHALL 与生成 Verilog 同构

The CH_MEM data memory (`ip/cpu/plugins/dmem_chmem.h`) SHALL return the value of the addressed word to the consumer, such that a load's result equals the contents of the address that the load instruction specifies — identical in the CH_MEM Simulator backend and in the Verilator backend.

This is violated today because CppHDL classifies memory read ports as sequential instructions (upstream `openspec/specs/mem-read-timing/spec.md` R3), so the Simulator's `aread` lags one tick behind the generated Verilog's continuous `assign` (upstream R4). The CH_MEM datapath is fully combinational and therefore cannot absorb that lag the way the legacy datapath's downstream register does.

#### Scenario: Load returns the addressed word's contents
- **WHEN** a load instruction executes with address `A` and `mem[A]` holds `V`
- **THEN** the value written to the destination register SHALL be `V`
- **AND** the result SHALL NOT be the contents of the address used by the preceding instruction

#### Scenario: Load-use dependency in consecutive instructions
- **WHEN** a load is immediately followed by an instruction that reads the loaded register
- **THEN** the second instruction SHALL observe the loaded value
- **AND** this SHALL hold identically in the CH_MEM Simulator and in the Verilator backend

#### Scenario: The known read-port skew SHALL remain pinned
- **WHEN** the CH_MEM test suite runs
- **THEN** a test SHALL assert the currently observed one-tick skew of `ch_mem::aread` in the Simulator, so that an upstream change to the read-port phase classification is detected
- **AND** that test SHALL reference the upstream capability `mem-read-timing` (R3/R4) as the governing contract

### Requirement: The upstream read-port contract SHALL NOT be silently violated

The CH_MEM dmem SHALL satisfy the upstream contract documented in CppHDL's `openspec/specs/mem-read-timing/spec.md`, which states that the Simulator/Verilog read-port drift is absorbed "by the downstream register providing the 1-cycle dependency". Where the CH_MEM datapath cannot provide that absorption, the adaptation SHALL be recorded explicitly in this project's specifications rather than left implicit.

#### Scenario: Adaptation direction is recorded
- **WHEN** this capability's requirements are reviewed
- **THEN** the chosen adaptation (backend-synchronous read port, upstream phase-classification change, or explicit backend semantic switch) SHALL be documented with its impact on the CH_MEM datapath
- **AND** the trade-off of each direction (backend consistency, upstream requirement conflict, pipeline impact) SHALL be stated

#### Scenario: Pre-existing load/store failures are tracked rather than orphaned
- **WHEN** the CH_MEM Simulator executes the vendored `rv32ui-p-*` ELF suite
- **THEN** any failing ELF SHALL be attributable to a documented root cause and an owning change
- **AND** the 10 load/store ELF (`lb`, `lbu`, `ld_st`, `lh`, `lhu`, `lw`, `sb`, `sh`, `sw`, `st_ld`) SHALL be listed in the project's known-issues documentation with this root cause

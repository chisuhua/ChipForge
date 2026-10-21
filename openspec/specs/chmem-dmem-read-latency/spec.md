# chmem-dmem-read-latency Specification

## Purpose
Defines the timing contract of the CH_MEM data-memory read path, and pins the alignment
between the CH_MEM Simulator's `ch_mem::aread` and the generated Verilog's continuous
`assign` as a regression anchor.

> **Status (2026-10-21)**: the read path is **correct** — the 10 vendored `rv32ui-p-*`
> load/store ELF failures are resolved. Upstream CppHDL aligned the two backends
> (commit `c27543d`, archived as `sim-mem-read-timing-align-2026-10`); the CH_MEM datapath
> required **no change**. The authoritative known-issue ledger is the `AGENTS.md`
> known-issues section.
## Requirements
### Requirement: CH_MEM dmem 读结果 SHALL 与生成 Verilog 同构

The CH_MEM data memory (`ip/cpu/plugins/dmem_chmem.h`) SHALL return the value of the addressed word to the consumer, such that a load's result equals the contents of the address that the load instruction specifies — identical in the CH_MEM Simulator backend and in the Verilator backend.

This used to be violated because CppHDL classified memory read ports as sequential instructions (upstream `openspec/specs/mem-read-timing/spec.md` R3), so the Simulator's `aread` lagged one tick behind the generated Verilog's continuous `assign` (upstream R4). The CH_MEM datapath is fully combinational and therefore could not absorb that lag the way the legacy datapath's downstream register does. Upstream CppHDL has since aligned the two backends (commit `c27543d`, `fix(sim): align ch_mem::aread with generated Verilog 0-cycle assign`, archived as `sim-mem-read-timing-align-2026-10`), and this project requires the CH_MEM datapath to preserve that alignment.

#### Scenario: Load returns the addressed word's contents
- **WHEN** a load instruction executes with address `A` and `mem[A]` holds `V`
- **THEN** the value written to the destination register SHALL be `V`
- **AND** the result SHALL NOT be the contents of the address used by the preceding instruction

#### Scenario: Load-use dependency in consecutive instructions
- **WHEN** a load is immediately followed by an instruction that reads the loaded register
- **THEN** the second instruction SHALL observe the loaded value
- **AND** this SHALL hold identically in the CH_MEM Simulator and in the Verilator backend

#### Scenario: The read-port skew SHALL remain absent
- **WHEN** the CH_MEM test suite runs
- **THEN** a test SHALL assert that `ch_mem::aread` in the Simulator observes the **current** address's contents on every tick, matching the generated Verilog's 0-cycle `assign`
- **AND** that test SHALL assert that the observed value is **not** the contents of the *previous* address — a test asserting only inequality between current and previous would also pass under a coincidental match, so the magnitude check SHALL be kept
- **AND** that test SHALL be sensitive to the upstream read-port phase classification regressing to posedge evaluation, so that an upstream change back to sequential classification is detected
- **AND** that test SHALL reference the upstream capability `mem-read-timing` as the governing contract

### Requirement: The upstream read-port contract SHALL NOT be silently violated

The CH_MEM dmem SHALL satisfy the upstream contract documented in CppHDL's `openspec/specs/mem-read-timing/spec.md`. That contract stated that the Simulator/Verilog read-port drift is absorbed "by the downstream register providing the 1-cycle dependency", and it was therefore incompatible with the CH_MEM datapath's fully combinational read path. Upstream rewrote the contract together with the implementation (commit `c27543d`; `mem-read-timing` R3/R4 updated in the archived `sim-mem-read-timing-align-2026-10`), and added cross-backend parity coverage (`eccf58f`, archived as `dual-backend-mem-read-timing-parity-2026-10`). This project SHALL track that upstream contract and SHALL record any future divergence explicitly in this project's specifications rather than leaving it implicit.

#### Scenario: Adaptation direction is recorded
- **WHEN** this capability's requirements are reviewed
- **THEN** the chosen adaptation (backend-synchronous read port, upstream phase-classification change, or explicit backend semantic switch) SHALL be documented with its impact on the CH_MEM datapath
- **AND** the trade-off of each direction (backend consistency, upstream requirement conflict, pipeline impact) SHALL be stated
- **AND** the outcome SHALL state which direction was actually adopted upstream and whether the CH_MEM datapath required any change as a result

#### Scenario: Load/store ELF failures are tracked rather than orphaned
- **WHEN** the CH_MEM Simulator executes the vendored `rv32ui-p-*` ELF suite
- **THEN** any failing ELF SHALL be attributable to a documented root cause and an owning change
- **AND** the 10 load/store ELF (`lb`, `lbu`, `ld_st`, `lh`, `lhu`, `lw`, `sb`, `sh`, `sw`, `st_ld`) — which failed until the upstream read-port alignment landed — SHALL NOT be listed as currently failing, and the resolution SHALL be recorded in the project's known-issues documentation


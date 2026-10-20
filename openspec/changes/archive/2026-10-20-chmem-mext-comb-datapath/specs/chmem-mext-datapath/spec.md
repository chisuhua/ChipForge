# Spec: chmem-mext-datapath

> **Capability**: CH_MEM 路径 RV32UM M 扩展组合式数据通路
> **Change**: `chmem-mext-comb-datapath`
> **Created**: 2026-10-20

## ADDED Requirements

### Requirement: CH_MEM IntAlu SHALL compute all 8 RV32M ops combinationally

The CH_MEM integer ALU (`ip/cpu/arch/riscv/int_alu_chmem.h`) SHALL recognize RV32M instructions (opcode `OP_OP` = 0x33 AND `funct7` = 0x01) and produce the correct `RD_DATA` for all 8 operations — MUL (`funct3`=000), MULH (001), MULHSU (010), MULHU (011), DIV (100), DIVU (101), REM (110), REMU (111) — using combinational `ch_uint` expressions (no FSM, no stall). Because CppHDL provides no signed integer type, signed operations (MULH/MULHSU/DIV/REM) SHALL be implemented via sign-magnitude conversion on `ch_uint`. The M-op select layers SHALL be appended after the existing RV32I select tree so that later-write-wins `select` semantics guarantee M-op results override any base-ALU mis-capture by shared `funct3`.

#### Scenario: Unsigned multiply ops produce correct low and high words
- **WHEN** the ALU evaluates a MUL instruction with `rs1=3, rs2=4`
- **THEN** `RD_DATA == 12` (low 32 bits)
- **AND** for MULHU with `rs1 = rs2 = 0xFFFFFFFF`, `RD_DATA == 0xFFFFFFFE` (high 32 bits of the 64-bit product)
- **AND** the 64-bit intermediate product width SHALL follow the `mul_op` rule (M+N) and be indexed via `bits<63,32>` for the high word

#### Scenario: Signed M-op variants use sign-magnitude conversion
- **WHEN** the ALU evaluates MULH with `rs1 = -2 (0xFFFFFFFE), rs2 = 3`
- **THEN** `RD_DATA == 0xFFFFFFFF` (high 32 bits of the sign-extended 64-bit product)
- **AND** MULHSU with `rs1 = -1, rs2 = 0x80000000` SHALL produce `RD_DATA == 0xFFFFFFFF`
- **AND** DIV with `rs1 = -14 (0xFFFFFFF2), rs2 = 4` SHALL produce `RD_DATA == 0xFFFFFFFD` (-3)
- **AND** REM with `rs1 = -14, rs2 = 4` SHALL produce `RD_DATA == 0xFFFFFFFE` (-2, sign follows dividend)

#### Scenario: RISC-V special cases SHALL be honored
- **WHEN** the ALU evaluates DIV/DIVU with `rs2 == 0`
- **THEN** `RD_DATA == 0xFFFFFFFF` (all ones)
- **AND** REM/REMU with `rs2 == 0` SHALL return the dividend (`rs1`)
- **AND** DIV with `rs1 == INT_MIN (0x80000000), rs2 == -1` SHALL return INT_MIN (0x80000000); REM SHALL return 0

### Requirement: RV32I CH_MEM semantics SHALL remain byte-identical

The addition of the M-op select layers SHALL NOT change the computed `RD_DATA`/`RESULT` for any RV32I instruction. Since every M-op select condition is gated by `funct7 == 0x01`, and all legal RV32I instructions carry `funct7` ∈ {0x00, 0x20}, `is_mext` SHALL be identically false on every RV32I execution path.

#### Scenario: Existing rv32ui coverage remains green
- **WHEN** the developer runs `./build/bin/chipforge_tests_chmem "[chmem]"` from the repository root
- **THEN** the 38 pre-existing rv32ui DYNAMIC_SECTION results MUST still pass
- **AND** no functional regression is observed in the `[chmem]` family

#### Scenario: Base-ALU funct7 gate hardening
- **WHEN** implementing the M-op layer
- **THEN** the six base R-type ops that currently lack a `funct7` gate (`is_sll`, `is_slt`, `is_sltu`, `is_xor`, `is_or`, `is_and` at `int_alu_chmem.h:155-169`) SHALL receive an `f7_00` gate on their `is_op` side, so that base-op and M-op discrimination are mutually exclusive and do not depend on `select` ordering
- **AND** this gate MUST NOT be applied to the `is_opimm` side (bits[31:25] of I-type hold immediate bits, not funct7) — see `int_alu_chmem.h:148-150`

### Requirement: All 8 rv32um ELF SHALL pass on CH_MEM and Verilator

With the combinational M-op datapath in place, the vendored `rv32um-p-*` ELF suite SHALL execute to completion with a passing RISC-V test harness signature on both the CH_MEM Simulator and the Verilator backend.

**Special-case coverage mapping** (disassembly-verified 2026-10-20 against `tests/cpu/riscv_tests/elf/rv32um-p-*`) — these are the ELF cases that force the RISC-V-mandated edge semantics:

| ELF | test | operands | expected | edge covered |
|---|---|---|---|---|
| `rv32um-p-div` | test_6 | `div(INT_MIN, 1)` | `0x80000000` | INT_MIN magnitude |
| `rv32um-p-div` | test_7 | `div(INT_MIN, -1)` | `0x80000000` | **INT_MIN ÷ -1 → INT_MIN (no overflow)** |
| `rv32um-p-div` | test_8 / 9 / 10 | `div(INT_MIN,0)` / `div(1,0)` / `div(0,0)` | `0xFFFFFFFF` | **signed divide-by-zero → all ones** |
| `rv32um-p-rem` | test_7 | `rem(INT_MIN, -1)` | `0x00000000` | **INT_MIN % -1 → 0** |
| `rv32um-p-rem` | test_8 / 9 / 10 | `rem(INT_MIN,0)` / `rem(1,0)` / `rem(0,0)` | `0x80000000` / `0x1` / `0x0` | **signed modulo-by-zero → dividend** |
| `rv32um-p-divu` | test_8 / 9 / 10 | `divu(INT_MIN,0)` / `divu(1,0)` / `divu(0,0)` | `0xFFFFFFFF` | **unsigned divide-by-zero → all ones** |
| `rv32um-p-remu` | test_8 / 9 / 10 | `remu(INT_MIN,0)` / `remu(1,0)` / `remu(0,0)` | `0x80000000` / `0x1` / `0x0` | **unsigned modulo-by-zero → dividend** |

#### Scenario: CH_MEM Simulator rv32um all pass
- **WHEN** the developer runs `./build/bin/chipforge_tests_chmem "chmem_vendored_elf_tohost1"` from the repository root
- **THEN** all 8 rv32um DYNAMIC_SECTION results (mul/mulh/mulhsu/mulhu/div/divu/rem/remu) SHALL verify `DMem[0x1000] (tohost) == 1`
- **AND** the 38 rv32ui sections SHALL still pass (46 total)

#### Scenario: Verilator rv32um all pass (real, not masked)
- **WHEN** the developer runs `./build/bin/chipforge_tests_chmem "[verilator]"` from the repository root
- **THEN** all 8 rv32um ELF SHALL reach a passing harness signature (real `tohost == 1`, not the `& 0x1u` mask artifact)
- **AND** the `[mmu-verilator]` 3/3 baseline SHALL continue to PASS

#### Scenario: Special-case guards are explicit (not inherited from the CppHDL default)
- **WHEN** the combinational M-op datapath emits any divide or modulo node
- **THEN** that node SHALL be guarded explicitly with a safe divisor (`b_safe = select(b == 0, 1, b)` / `b_safe_mag = select(abs32(b) == 0, 1, abs32(b))`), and the result SHALL be overridden with the RISC-V-mandated special value (`select(b == 0, all_ones, …)` for DIV/DIVU; `select(b == 0, a, …)` for REM/REMU)
- **AND** the implementation SHALL NOT rely on the CppHDL default divide-by-zero behaviour (`CppHDL/include/ast/instr_op_arith.h:52-73` returns 0 and writes to stderr), because the vendored ELF drives `rs2 == 0` for the unsigned variants (`divu` / `remu` test_8/9/10, see the coverage mapping above)

### Requirement: CH_MEM 组合式 M-op 与 TLM radix-2 cycle 不可比 SHALL 被显式声明

The CH_MEM path resolves every RV32M operation in a single combinational cycle, whereas the TLM path implements a multi-cycle radix-2 iterative FSM (`ip/cpu/arch/riscv/mul_div_fsm.h:144-148` — MUL=1c / MULH=3c / DIV=33c). The two datapaths are therefore **not cycle-homologous**: their cycle counts SHALL NOT be compared against each other, and a parity assertion between them is **not a meaningful acceptance criterion** — it is neither a pass nor a failure condition, it is out of scope.

This change SHALL revise the cycle-comparison scenarios of the `chmem-rv32um-coverage` and `verilator-rv32um-coverage` capabilities via `## MODIFIED Requirements` delta specs (removing the ±5% / +50c parity requirement), and SHALL defer cycle-parity acceptance to change `chmem-mext-fsm-cycle-parity`.

#### Scenario: Cycle parity against the TLM baseline is explicitly out of scope
- **WHEN** the CH_MEM Simulator or the Verilator backend completes an rv32um ELF
- **THEN** the cycle index SHALL still be reported (Catch2 `INFO` / runner stdout) and recorded in the commit message for manual inspection
- **AND** the executor SHALL NOT treat a cycle-count difference against `soc/cpu/docs/dse/rv32um-baseline-matrix.csv` as a test failure
- **AND** this deferral SHALL be recorded as a `## MODIFIED Requirements` delta in the two affected capabilities, so that it is visible as a deliberate scope decision rather than an omitted check

### Requirement: Non-goals SHALL be explicitly deferred

This change SHALL NOT register `MulDivFsmPlugin` in `cpu_factory_chmem.h`, SHALL NOT modify the `mul_div_fsm_chmem.h` stub bodies, and SHALL NOT introduce CtrlLink stall wiring. Cycle-accurate CH_MEM M-extension SHALL be tracked as a separate follow-up change, blocked on the CtrlLink API gap documented at `hazard_chmem.h:163-167`.

#### Scenario: Datapath-only scope
- **WHEN** reviewing the change diff
- **THEN** the registered plugin list in `cpu_factory_chmem.h:322-333` SHALL be unchanged
- **AND** `mul_div_fsm_chmem.h` SHALL only receive a header comment clarifying it is a DSL mechanism fixture, not a datapath candidate

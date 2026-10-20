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
- **THEN** the six base R-type ops that currently lack a `funct7` gate (`is_sll`, `is_slt`, `is_sltu`, `is_xor`, `is_or`, `is_and` at `int_alu_chmem.h:155-169`) SHALL optionally receive an `f7_00` gate on their `is_op` side
- **AND** this gate MUST NOT be applied to the `is_opimm` side (bits[31:25] of I-type hold immediate bits, not funct7) — see `int_alu_chmem.h:148-150`

### Requirement: All 8 rv32um ELF SHALL pass on CH_MEM and Verilator

With the combinational M-op datapath in place, the vendored `rv32um-p-*` ELF suite SHALL execute to completion with a passing RISC-V test harness signature on both the CH_MEM Simulator and the Verilator backend.

#### Scenario: CH_MEM Simulator rv32um all pass
- **WHEN** the developer runs `./build/bin/chipforge_tests_chmem "chmem_vendored_elf_tohost1"` from the repository root
- **THEN** all 8 rv32um DYNAMIC_SECTION results (mul/mulh/mulhsu/mulhu/div/divu/rem/remu) SHALL verify `DMem[0x1000] (tohost) == 1`
- **AND** the 38 rv32ui sections SHALL still pass (46 total)

#### Scenario: Verilator rv32um all pass (real, not masked)
- **WHEN** the developer runs `./build/bin/chipforge_tests_chmem "[verilator]"` from the repository root
- **THEN** all 8 rv32um ELF SHALL reach a passing harness signature (real `tohost == 1`, not the `& 0x1u` mask artifact)
- **AND** the `[mmu-verilator]` 3/3 baseline SHALL continue to PASS

### Requirement: Non-goals SHALL be explicitly deferred

This change SHALL NOT register `MulDivFsmPlugin` in `cpu_factory_chmem.h`, SHALL NOT modify the `mul_div_fsm_chmem.h` stub bodies, and SHALL NOT introduce CtrlLink stall wiring. Cycle-accurate CH_MEM M-extension SHALL be tracked as a separate follow-up change, blocked on the CtrlLink API gap documented at `hazard_chmem.h:163-167`.

#### Scenario: Datapath-only scope
- **WHEN** reviewing the change diff
- **THEN** the registered plugin list in `cpu_factory_chmem.h:322-333` SHALL be unchanged
- **AND** `mul_div_fsm_chmem.h` SHALL only receive a header comment clarifying it is a DSL mechanism fixture, not a datapath candidate

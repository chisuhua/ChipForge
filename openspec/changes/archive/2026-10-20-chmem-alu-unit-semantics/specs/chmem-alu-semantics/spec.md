# Spec: chmem-alu-semantics

> **Capability**: CH_MEM ALU 单元级语义锚
> **Change**: `chmem-alu-unit-semantics`
> **Created**: 2026-10-20

## ADDED Requirements

### Requirement: CH_MEM ALU 必须有跑真实 select 树的单元语义测试

The CH_MEM RISC-V ALU (`ip/cpu/arch/riscv/int_alu_chmem.h`) SHALL be covered by a unit test that **executes the real 19-layer select tree** and asserts the numeric value of `RD_DATA` — not merely that a Verilog file is generated, and not a comparison between two hand-written C++ mirrors.

The test SHALL instantiate `RiscvIntAluPlugin` in an isolated `ch::core::context`, populate the `execute` node payloads (`RS1`/`RS2`/`PC` + `DECODED_INST`) **before** `PipeBuilder::elaborate()`, then read the result back via `PipeBuilder::create_simulator()` + `Simulator::tick()` + `Simulator::get_value(RD_DATA)`.

The test SHALL assert on `RD_DATA` (the ALU arithmetic result), NOT on `RESULT` (which is the load/store byte address `select(is_store||is_load, rs1+imm, result)` and is not equivalent for ALU semantics).

Expected values SHALL be derived independently from the RISC-V ISA specification. The test MUST NOT derive expected values from `RiscvIntAluPlugin::compute_static()` (the in-implementation C++ mirror) — doing so would make the assertion self-referential.

#### Scenario: All 8 RV32M operations produce ISA-correct results
- **WHEN** the test evaluates MUL, MULH, MULHSU, MULHU, DIV, DIVU, REM, REMU through the real select tree
- **THEN** each result SHALL equal the RISC-V-specified value
- **AND** MULH/MULHSU SHALL use the high 32 bits of the 64-bit product (sign-extension for MULH, sign-extension of rs1 with zero-extension of rs2 for MULHSU)
- **AND** DIV SHALL round toward zero and REM SHALL take the sign of the dividend

#### Scenario: RISC-V special cases are honored
- **WHEN** the divisor is zero
- **THEN** DIV and DIVU SHALL produce `0xFFFFFFFF` and REM and REMU SHALL produce the dividend
- **AND** when `rs1 == INT_MIN` and `rs2 == -1`, DIV SHALL produce `INT_MIN` and REM SHALL produce `0`

#### Scenario: M-op and base-op discrimination is mutually exclusive
- **WHEN** the same `funct3` is evaluated with `funct7 == 0x01` (M-op) and with `funct7 == 0x00` (base op)
- **THEN** each SHALL produce its respective ISA-correct result
- **AND** the two results SHALL differ for every `funct3` in `{0..7}` — otherwise the pair has no discriminating power

#### Scenario: RV32I base operations are unchanged
- **WHEN** the test evaluates ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND
- **THEN** each SHALL equal its RISC-V-defined value
- **AND** the `f7_00` hardening on the six previously un-gated R-type operations SHALL NOT change any legal RV32I result

#### Scenario: I-type instructions are not affected by the R-type funct7 gate
- **WHEN** the test evaluates I-type instructions whose encoded bits[31:25] are `0x20` (i.e. a negative immediate occupies the funct7 field)
- **THEN** XORI, ANDI, ORI, SLTI, SLTIU SHALL still produce their ISA-correct results
- **AND** SLLI SHALL produce the correct shift result

#### Scenario: Top-level select branches are intact
- **WHEN** the test evaluates LUI and AUIPC
- **THEN** LUI SHALL produce the immediate and AUIPC SHALL produce `pc + imm`

### Requirement: The semantic anchor SHALL be proven to detect regressions

The unit test SHALL be validated by mutation testing: deliberately breaking the implementation SHALL cause the test to fail. At minimum, the following mutations SHALL be detected: (a) removing the `funct7 == 0x01` M-op gate, and (b) deleting the M-op select layers.

#### Scenario: Removing the M-op gate is detected
- **WHEN** the `is_mext` gate is changed from `is_op && (funct7 == 0x01)` to `is_op`
- **THEN** the RV32I regression group and the discrimination group SHALL fail

#### Scenario: Deleting the M-op select layers is detected
- **WHEN** the eight M-op `select` layers are removed from the tree
- **THEN** the M-op semantics, M-op edge-case, and discrimination groups SHALL fail

#### Scenario: Reordering the M-op layers alone is not a regression
- **WHEN** the eight M-op `select` layers are moved before the base tree while the `f7_00` hardening remains
- **THEN** the test MAY still pass, because the hardening makes the base-op and M-op discriminators mutually exclusive — the reordering is then semantically equivalent, not a defect

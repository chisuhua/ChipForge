# csr-chmem-zicsr Specification

## Purpose
TBD - created by archiving change csr-chmem-pipeline-integration. Update Purpose after archive.
## Requirements
### Requirement: RiscvCsrPluginChmem registered as 8th CH_MEM plugin

A new CH_MEM-mode CSR plugin SHALL be implemented at `ip/cpu/arch/riscv/csr_chmem.h` (header-only, `#ifdef CF_PLUGIN_USE_CH_MEM` wrapped). The plugin SHALL:
- Inherit `cf::plugin::PluginBase`
- Register as the 8th plugin in `cpu_factory_chmem.h` (position 5, after RegFile, before IntAlu)
- Maintain 8 M-mode CSR registers as `ch::core::ch_reg<ch_uint<32>>` instance members (mstatus / mtvec / mepc / mcause / mie / mip / mscratch / mcycle)
- Implement a 2-state FSM (IDLE → TRAP_PENDING) per ADR-046 v2.0 §2.1.1 exemption (`CF_PLUGIN_USE_FSM_EXEMPT` + `chlib::ch_state_machine` DSL)
- Emit payload keys `csr_keys<T>::CSR_READ_VALUE`, `TRAP_PENDING`, `TRAP_TARGET` at corresponding `at_stage` closures
- Consume payload key `cpu_keys<T>::INSTRUCTION` at `at_stage("execute", LATE)` for csr_addr re-extraction (per R1 decision)

#### Scenario: 8-plugin CH_MEM pipeline compiles
- **WHEN** `cmake --build build` invoked with `-DCF_PLUGIN_USE_CH_MEM=ON`
- **THEN** `ip/cpu/arch/riscv/csr_chmem.h` SHALL compile with 0 errors
- **AND** `ip/cpu/cpu_factory_chmem.h` SHALL register `RiscvCsrPluginChmem<T>` as the 8th plugin (position 5)
- **AND** the 7→8 plugin transition SHALL preserve `[chmem] 9/9` and `[verilator] 1/1 (48 ELF)` baselines with zero unexpected regression

#### Scenario: plugin order follows ADR-048 canonical ordering
- **WHEN** the 8-plugin registration list is inspected at runtime
- **THEN** the order SHALL be: Decoder (1) → Hazard (2) → IBus (3) → RegFile (4) → **Csr (5)** → IntAlu (6) → Branch (7) → DMem (8)
- **AND** `check_canonical_ordering()` runtime assertion SHALL pass (per ADR-048)
- **AND** CSR position 5 SHALL allow IntAlu (position 6) execute-LATE to consume CSR_READ_VALUE without being overridden

### Requirement: 8 Zicsr instructions functional in CH_MEM mode

The CH_MEM CSR plugin SHALL implement 4 classes × 2 variants = 8 Zicsr instructions:
- **CSRRW / CSRRWI**: Atomic read/write CSR
- **CSRRS / CSRRSI**: Atomic read CSR, set bits by mask
- **CSRRC / CSRRCI**: Atomic read CSR, clear bits by mask

Each instruction SHALL be decoded by the existing `DecoderPlugin` (funct3 from bits[14:12], immediate from bits[19:15] for -I variants). The CSR plugin SHALL consume the `INSTRUCTION` key at `at_stage("execute", LATE)` and re-extract csr_addr from `bits<31,20>(INSTRUCTION)` (per R1 decision).

#### Scenario: CSRRW writes CSR and returns old value to rd
- **WHEN** `csrrw x1, mstatus` (0x300) executed with `x1 = initial_value` and `mstatus = old_mstatus`
- **THEN** `mstatus` SHALL equal `initial_value` after the instruction retires
- **AND** `x1` SHALL equal `old_mstatus` (CSRRW semantics: read old, write new, return old)
- **AND** the writeback path SHALL consume `csr_keys<T>::CSR_READ_VALUE` (per R3 decision) to provide `old_mstatus` to rd

#### Scenario: CSRRS read-only bits not written
- **WHEN** `csrrs x1, mcycle` (0xB00, read-only) executed with `x1 = mask`
- **THEN** `mcycle` SHALL NOT be modified (read-only CSR)
- **AND** `x1` SHALL equal current `mcycle` value (CSRRS always reads regardless of mask)
- **AND** `mcycle` SHALL continue to increment every cycle (free-running counter, per Q1 in design.md §6)

#### Scenario: CSRRWI immediate variant
- **WHEN** `csrrwi x1, mstatus, 0x5` executed
- **THEN** `mstatus` SHALL equal `0x5` (immediate value, zero-extended from 5-bit field)
- **AND** `x1` SHALL equal old `mstatus`

#### Scenario: 8 instructions × 3 CSR × N test cases all PASS
- **WHEN** ctest `[cpu][csr-chmem]` invoked (5 test cases per tasks.md §Phase C.2)
- **THEN** Case 1 (CSRRW x1, mstatus) SHALL PASS — x1 = old mstatus, mstatus = new value
- **AND** Case 2 (CSRRS x1, mcycle) SHALL PASS — x1 = mcycle (read-only)
- **AND** Case 3 (ECALL) SHALL PASS — mcause = 11, mepc = ECALL PC + 4
- **AND** Case 4 (MRET after ECALL) SHALL PASS — PC = mepc
- **AND** Case 5 (EBREAK) SHALL PASS — mcause = 3

### Requirement: Trap path (ECALL/EBREAK/MRET) functional in CH_MEM mode

The CH_MEM CSR plugin SHALL implement 3 trap instructions using the 2-state FSM (IDLE → TRAP_PENDING):
- **ECALL** (funct12=0x000): Write `mcause=11`, write `mepc=PC+4`, write `mtvec` target PC, transition FSM IDLE → TRAP_PENDING, set `TRAP_PENDING=true` and `TRAP_TARGET=mtvec`
- **EBREAK** (funct12=0x001): Write `mcause=3`, write `mepc=PC+4`, write `mtvec` target PC, transition FSM IDLE → TRAP_PENDING
- **MRET** (funct12=0x302): Read `mepc`, set `TRAP_PENDING=true` and `TRAP_TARGET=mepc`, transition FSM TRAP_PENDING → IDLE (consume trap)

The trap PC redirect SHALL be implemented per R2 decision: self-built CtrlLink + new payload keys `csr_keys<T>::TRAP_PENDING` (ch_bool) and `TRAP_TARGET` (ch_uint<32>), consumed by IBus at `at_stage("branch", NORMAL)` (registered after IBus branch-LATE, so IBus NORMAL wins in elaboration order).

#### Scenario: ECALL triggers trap redirect
- **WHEN** `ecall` executed at PC=P
- **THEN** `mcause` SHALL equal 11 (E-call from M-mode per RISC-V Privileged Spec §3.1.9)
- **AND** `mepc` SHALL equal P+4
- **AND** `TRAP_PENDING` SHALL be true
- **AND** `TRAP_TARGET` SHALL equal `mtvec` value
- **AND** next fetched PC SHALL equal `mtvec` (IBus NORMAL select wins over branch-LATE)

#### Scenario: MRET returns to mepc
- **WHEN** `mret` executed after ECALL
- **THEN** `TRAP_PENDING` SHALL be true
- **AND** `TRAP_TARGET` SHALL equal `mepc` value
- **AND** next fetched PC SHALL equal `mepc`
- **AND** `mstatus.MIE` SHALL be restored (per RISC-V Privileged Spec §3.1.6.5)

#### Scenario: EBREAK triggers trap
- **WHEN** `ebreak` executed at PC=P
- **THEN** `mcause` SHALL equal 3 (Breakpoint per RISC-V Privileged Spec §3.1.9)
- **AND** `mepc` SHALL equal P+4
- **AND** `TRAP_PENDING` SHALL be true with `TRAP_TARGET = mtvec`

#### Scenario: trap priority over branch
- **WHEN** `beq x1, x2, target` AND `mret` are both in the pipeline (hypothetical, due to hazard stall)
- **THEN** `mret` TRAP_PENDING SHALL win (per `trap > branch` priority from `objective-v0110-launch.md §8`)
- **AND** `TRAP_TARGET` SHALL equal `mepc`, NOT the branch target
- **AND** this priority SHALL be explicitly documented as the v0.11.0 baseline; `wave5-bp-btb` change (currently a ghost dependency — change directory does not exist) shall negotiate bp-btb interactions in its own change

### Requirement: M-mode CSR subset (8 registers) implemented with ch_reg storage

The CH_MEM CSR plugin SHALL maintain 8 M-mode CSR registers as `ch::core::ch_reg<ch_uint<32>>` instance members (NOT static, per `reg_file_chmem.h:13-29` iron rule ① to avoid cross-context use-after-free):
- **mstatus** (0x300): SDR/MIE/TW/TVM fields (MRET path needs)
- **mtvec** (0x305): trap handler base address (ECALL/MRET path needs)
- **mepc** (0x341): trap return PC (MRET needs)
- **mcause** (0x342): trap cause (3/7/11, ECALL/MRET path needs)
- **mie** (0x304): interrupt enable (read path; write deferred to Phase 5+)
- **mip** (0x344): interrupt pending (read path; write deferred)
- **mscratch** (0x340): trap scratch (per Q2 in design.md §6, recommended NOT implemented to reduce scope; if implemented, optional)
- **mcycle** (0xB00): cycle counter (read-only, free-running, per Q1 in design.md §6)

Each register SHALL be initialized with `ch::core::ch_literal<0, 32>{}` (real literal, per CH_MEM v0.3.1 M6 upstream fix; `ch_uint<32>(0)` is forbidden). mcycle SHALL increment every cycle via `at_stage("fetch", ALWAYS, [...])` closure with a free-running ch_reg carry chain.

#### Scenario: 8 CSR registers initialized to 0
- **WHEN** `RiscvCsrPluginChmem` constructed
- **THEN** all 8 registers SHALL be initialized to 0 via `ch::core::ch_literal<0, 32>{}`
- **AND** `csr_read_value` SHALL return 0 for any address before first CSRR* instruction

#### Scenario: mcycle free-running counter
- **WHEN** 1000 cycles elapsed since pipeline start
- **THEN** `mcycle` SHALL equal 1000 (or 1000 ± a small constant for fetch stage delay)
- **AND** reading `mcycle` via CSRR* SHALL return the current cycle count

#### Scenario: CSR read-write preserves mcycle (read-only enforcement)
- **WHEN** `csrrw x1, mcycle, x2` (x2 = 0xDEADBEEF) executed
- **THEN** `mcycle` SHALL remain its current value (CSRRS-like semantics for read-only bits, per RISC-V Privileged Spec §2.1)
- **AND** `x1` SHALL equal current `mcycle` value
- **AND** no bits in `mcycle` SHALL be written

#### Scenario: write paths restricted to writable CSR
- **WHEN** `csrrw x1, mie, x2` (x2 = 0x1) executed
- **THEN** `mie` SHALL equal 0x1 (mie is writable, unlike mcycle)
- **AND** `x1` SHALL equal old `mie` value


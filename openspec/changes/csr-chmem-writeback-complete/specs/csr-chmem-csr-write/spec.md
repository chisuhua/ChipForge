# csr-chmem-csr-write Specification

## Purpose
TBD - created by archiving change csr-chmem-writeback-complete. Update Purpose after archive.
## ADDED Requirements
### Requirement: CSR writeback 7 M-mode CSR registers actually update

The CH_MEM CSR plugin (`ip/cpu/plugins/csr_chmem.h` `RiscvCsrPluginChmem<T>`) SHALL actually update 7 of 8 M-mode CSR registers when CSRRW/CSRRS/CSRRC instructions write them. The 8th register (mcycle) SHALL remain read-only as a real-time cycle counter (R3 decision + R2.5 free-running counter).

The 7 writable CSR registers SHALL be:
- `mstatus` (0x300)
- `mtvec` (0x305)
- `mscratch` (0x340)
- `mepc` (0x341)
- `mcause` (0x342)
- `mip` (0x344)
- `mie` (0x304)

The 1 read-only CSR SHALL be:
- `mcycle` (0xB00) — main counter, `mcycle_ <<= mcycle_ + 1` per cycle (R2.5), never overwritten by CSRR*/CSRWI/CSRRSI/CSRRCI

The writeback SHALL be implemented via two coordinated at_stage closures (R3.5 decision #1):
1. `at_stage("execute", LATE)` — write payload keys `csr_keys<T>::CSR_WRITE_ADDR/VALUE/EN` (3 lines)
2. `at_stage("writeback", NORMAL)` — consume CSR_WRITE_* keys, 8 independent write-enable signals + 8 `ch_reg <<= select(we, write_value, ch_reg)` statements

The 1-bit write enable SHALL be implemented via ch_uint<1> select tree (per AGENTS.md §架构约束 #8: 禁 if(ch_bool)):
```cpp
const bool write_en_bool = static_cast<bool>(
    select(zicsr_valid_ch,
           select(csr_addr_eq_B00, ch_uint<1>(ch_literal<0, 1>{}),
                                       ch_uint<1>(ch_literal<1, 1>{})),
           ch_uint<1>(ch_literal<0, 1>{})));
```

`write_en_bool` SHALL be `true` if and only if:
- The current instruction is a Zicsr instruction (`zicsr_valid_ch` evaluates to `true`)
- AND the target CSR address is NOT `mcycle` (`csr_addr_eq_B00` evaluates to `false`)

#### Scenario: CSRRW writes mstatus and writeback register actually updates
- **WHEN** `csrrw x1, mstatus, x10` (inst encoding 0x300510F3, rs1=x10=0xCAFEBABE) executed
- **THEN** at `at_stage("execute", LATE)` the `csr_keys<T>::CSR_WRITE_ADDR` payload SHALL be `0x300` (mstatus)
- **AND** the `csr_keys<T>::CSR_WRITE_VALUE` payload SHALL be `0xCAFEBABE` (rs1 value)
- **AND** the `csr_keys<T>::CSR_WRITE_EN` payload SHALL be `true`
- **AND** at `at_stage("writeback", NORMAL)` the `mstatus_` ch_reg SHALL receive `mstatus_ <<= select(true, 0xCAFEBABE, mstatus_)` (select tree resolves to write 0xCAFEBABE)

#### Scenario: CSRRS mcycle write enable is false (mcycle read-only)
- **WHEN** `csrrs x1, mcycle, x10` (inst encoding 0xB0005073, rs1=x10=0xCAFEBABE) executed
- **THEN** at `at_stage("execute", LATE)` the `csr_keys<T>::CSR_WRITE_ADDR` payload SHALL be `0xB00` (mcycle)
- **AND** the `csr_keys<T>::CSR_WRITE_VALUE` payload SHALL be `0xCAFEBABE` (rs1 value, but irrelevant since write_en=false)
- **AND** the `csr_keys<T>::CSR_WRITE_EN` payload SHALL be `false` (mcycle 永不被 CSRR* 写 per R3 decision)
- **AND** at `at_stage("writeback", NORMAL)` `mcycle_` ch_reg SHALL NOT receive any `<<=` (preserving R2.5 free-running counter)
- **AND** `mcycle_` SHALL continue to increment per cycle via `mcycle_ <<= mcycle_ + 1` in `at_stage("fetch", NORMAL)` (R2.5)

#### Scenario: Non-Zicsr instruction write enable is false
- **WHEN** a non-Zicsr instruction executed (e.g. CSRRW/CSRRS/CSRRC substitute with `csr_addr != 0xB00` and `zicsr_valid_ch=false`, or ECALL/EBREAK/MRET with funct12 ∈ {0x000, 0x001, 0x302})
- **THEN** the `csr_keys<T>::CSR_WRITE_EN` payload SHALL be `false`
- **AND** no CSR register SHALL receive any `<<=` update

#### Scenario: CSRRS/CSRRC simplified to CSRRW semantics
- **WHEN** `csrrs x1, mstatus, x10` (inst encoding 0x30052073) executed
- **THEN** the `csr_keys<T>::CSR_WRITE_VALUE` payload SHALL be `0xCAFEBABE` (rs1 value as mask, not `mstatus | x10` as RISC-V spec mandates)
- **NOTE**: This deviates from RISC-V spec (CSRRS semantics: `mstatus |= mask`). Full set/clear mask expansion is deferred to a future change. Current implementation writes rs1 value directly for all CSRRW/CSRRS/CSRRC, treating them all as CSRRW semantics. Documented as known scope-partial limitation.

#### Scenario: 5-stage pipeline baseline regression guard
- **WHEN** `tests/cpu/test_cpu_rtl_regfile_alu.cpp::m3_poc_5stage_byte_equal` executed after R3.5 implementation
- **THEN** all 185 assertions SHALL still pass (regression gate for writeback enabling ch_reg actual writes)
- **AND** `[cpu][csr-chmem]` SHALL pass 16/16 cases (~118 assertions, 14 baseline + 2 new R3.5 cases)
- **AND** `[chmem]` SHALL pass 37/38 (1 known cpphdl_poc_chbool_contextual_conversion context pollution flake, unchanged from baseline)
- **AND** TLM `[mmu]` SHALL pass 59/59 (regression gate for cross-binary baseline)
- **AND** architecture gate `check_plugin_portability.sh 12/12` + `verify_plugin_decision.sh 8/8` + `verify_adr.sh 0 fail` SHALL all PASS

### Requirement: Single-cycle write-enable observation port exposed for testing

The CH_MEM CSR plugin SHALL expose 2 new public observation methods for unit testing of the R3.5 write path:
- `ch_uint<32> last_csr_write_value_ch() const` — returns the ch_signal DAG reference for `CSR_WRITE_VALUE` payload, post-tick readable via `sim->get_signal_value()`
- `ch_uint<1> last_csr_write_en_ch() const` — returns the ch_signal DAG reference for `CSR_WRITE_EN` payload, post-tick readable via `sim->get_signal_value()`

These observation ports SHALL be updated at the end of the `at_stage("execute", LATE)` closure, AFTER the `n->payloads().put(CsrKT::CSR_WRITE_*, ...)` writes. The observation values SHALL reflect the same ch_signal DAG that writeback consumes.

**Rationale**: Single-stage unit tests cannot validate 2-cycle end-to-end writeback (ch_reg requires posedge to commit `next` to `current`). Observation ports allow 1-cycle validation of payload propagation, which is the contract between execute-LATE and writeback-NORMAL closures.

#### Scenario: Observation port captures write enable at 1 cycle post-tick
- **WHEN** `csrrw x1, mstatus, x10` (inst 0x300510F3) executed and `sim->tick()` called once
- **THEN** `sim->get_signal_value(plugin->last_csr_write_en_ch())` SHALL return `1`
- **AND** `sim->get_signal_value(plugin->last_csr_write_value_ch())` SHALL return `0xCAFEBABE`
- **AND** these values SHALL be deterministically set by the execute-LATE closure (no race with writeback)

#### Scenario: Observation port captures mcycle write enable false
- **WHEN** `csrrs x1, mcycle, x10` (inst 0xB0005073) executed and `sim->tick()` called once
- **THEN** `sim->get_signal_value(plugin->last_csr_write_en_ch())` SHALL return `0` (R3 decision: mcycle 永不被 CSRR* 写)
- **AND** `sim->get_signal_value(plugin->last_csr_write_value_ch())` SHALL return `0xCAFEBABE` (rs1 value, irrelevant since write_en=false)
- **AND** `sim->get_signal_value(plugin->last_csr_read_valid_ch())` SHALL return `0` (R3 decision: mcycle 单独路径)

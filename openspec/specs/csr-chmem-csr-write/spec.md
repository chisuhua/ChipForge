# csr-chmem-csr-write Specification

## Purpose
TBD - created by archiving change csr-chmem-writeback-complete. Update Purpose after archive.
## Requirements
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
// R3.5 [GREEN]: write_en = zicsr_valid
//                && funct3 != 000  (排除 ECALL/EBREAK/MRET)
//                && funct3 != 100  (保留)
//                && !(funct3 in {010,011} && rs1_idx == 0)  (csrr 读惯例不写)
//                && !(funct3 in {110,111} && imm5 == 0)    (csrwi/csrri 读惯例不写)
//                && csr_addr != 0xB00  (mcycle 永不被 CSRR* 写)
const bool write_en_bool = static_cast<bool>(
    select(zicsr_valid_ch,
           select(csr_addr_eq_B00, ch_uint<1>(ch_literal<0, 1>{}),
                                       ch_uint<1>(ch_literal<1, 1>{})),
           ch_uint<1>(ch_literal<0, 1>{})));
// 注: R3.5 [GREEN] 实际 select 树走 4 层 (zicsr_valid × !ecall_ebreak_mret × csrr_read_preserve × !mcycle_addr),
//   上方代码为 R3.3 简化示意; 完整 4 层 select 树见 ip/cpu/plugins/csr_chmem.h 实施。
```

`write_en_bool` SHALL be `true` if and only if **all** of the following hold:
- The current instruction is a Zicsr instruction (`opcode == 0x73` AND `funct3 ∈ {001,010,011,101,110,111}`)
- AND `funct3 != 000` (排除 ECALL/EBREAK/MRET — these are trap instructions, NOT CSR writes)
- AND for `funct3 ∈ {010, 011}` (CSRRS/CSRRC), `rs1_idx != 0` (RISC-V spec: read-only when rs1=x0)
- AND for `funct3 ∈ {110, 111}` (CSRRSI/CSRRCI), `imm5 != 0` (RISC-V spec: read-only when uimm=0)
- AND the target CSR address is NOT `mcycle` (`csr_addr != 0xB00`)

The CSR write value SHALL be `select(funct3[2], uimm_5bit_zero_extended, rs1_value)`:
- For CSRRW/CSRRS/CSRRC (`funct3 ∈ {001,010,011}`), the write value is `rs1_value` (32-bit, used as mask for CSRRS/CSRRC)
- For CSRRWI/CSRRSI/CSRRCI (`funct3 ∈ {101,110,111}`), the write value is `imm5` zero-extended to 32-bit (uimm)
- **NOTE**: This still deviates from RISC-V spec for CSRRS/CSRRC semantics (mask expansion: `csr |= rs1` / `csr &= ~rs1`). Full mask expansion deferred to a future change. Current implementation writes the raw value directly, treating all variants as direct write.

#### Scenario: CSRRW writes mstatus and writeback register receives select-tree assignment
- **WHEN** `csrrw x1, mstatus, x10` (inst encoding `0x300510F3`, rs1=x10=0xCAFEBABE) executed
- **THEN** at `at_stage("execute", LATE)` the `csr_keys<T>::CSR_WRITE_ADDR` payload SHALL be `0x300` (mstatus)
- **AND** the `csr_keys<T>::CSR_WRITE_VALUE` payload SHALL be `0xCAFEBABE` (rs1 value)
- **AND** the `csr_keys<T>::CSR_WRITE_EN` payload SHALL be `true`
- **AND** at `at_stage("writeback", NORMAL)` the `mstatus_` ch_reg SHALL participate in a select-tree assignment `mstatus_ <<= select(we_300, write_value, mstatus_)` where `we_300 == true` and the select-tree node SHALL be constructed in the elaboration DAG (verifiable via `to_verilog` output / `sim->get_signal_value` on internal DAG nodes)
- **NOTE**: This change validates the **DAG construction** of the writeback assignment, not the **cross-cycle state propagation** (the latter requires 2-cycle end-to-end e2e, deferred to C.1 integration change per csr-chmem-pipeline-integration archive)

#### Scenario: CSRRS mcycle write enable is false (mcycle read-only)
- **WHEN** `csrrs x1, mcycle, x10` (inst encoding `0xB00520F3`, funct3=010=CSRRS, rd=x1, rs1=x10, csr_addr=0xB00) executed
- **THEN** at `at_stage("execute", LATE)` the `csr_keys<T>::CSR_WRITE_ADDR` payload SHALL be `0xB00` (mcycle)
- **AND** the `csr_keys<T>::CSR_WRITE_VALUE` payload SHALL be `0xCAFEBABE` (rs1 value, but irrelevant since write_en=false)
- **AND** the `csr_keys<T>::CSR_WRITE_EN` payload SHALL be `false` (mcycle 永不被 CSRR* 写 per R3 decision)
- **AND** at `at_stage("writeback", NORMAL)` `mcycle_` ch_reg SHALL NOT receive any `<<=` (preserving R2.5 free-running counter)
- **AND** `mcycle_` SHALL continue to increment per cycle via `mcycle_ <<= mcycle_ + 1` in `at_stage("fetch", NORMAL)` (R2.5)

#### Scenario: ECALL/EBREAK/MRET write enable is false (funct3=000 trap instructions)
- **WHEN** `ecall` (inst encoding `0x00000073`, funct3=000, funct12=0x000) executed
- **THEN** the `csr_keys<T>::CSR_WRITE_EN` payload SHALL be `false` (funct3=000 are trap instructions, not CSR writes)
- **AND** the `csr_keys<T>::CSR_WRITE_ADDR` payload SHALL be `0x000` (from funct12)
- **AND** no CSR register SHALL receive any `<<=` update from writeback
- **AND** `branch-LATE` closure (R2.4) SHALL continue to handle ECALL/EBREAK/MRET trap logic (writes mcause/mepc via branch path, NOT writeback path)

#### Scenario: csrr read-only convention preserved (CSRRS/CSRRC with rs1=x0)
- **WHEN** `csrr x1, mstatus` (CSRRS with rs1=x0, inst encoding `0x300020F3`, funct3=010, rs1_idx=0) executed
- **THEN** the `csr_keys<T>::CSR_WRITE_EN` payload SHALL be `false` (RISC-V spec: CSRRS with rs1=x0 is read-only)
- **AND** the `csr_keys<T>::CSR_WRITE_VALUE` payload SHALL be `0` (rs1=x0)
- **AND** the `csr_keys<T>::CSR_READ_VALUE` payload SHALL be the current mstatus value (read happens regardless)
- **AND** `mstatus_` SHALL NOT be overwritten

#### Scenario: csrrwi read-only convention preserved (CSRRWI with uimm=0)
- **WHEN** `csrrwi x1, mstatus, 0` (inst encoding `0x300020F3` — same encoding as csrr, by RISC-V spec design) executed
- **THEN** the `csr_keys<T>::CSR_WRITE_EN` payload SHALL be `false`
- **AND** the read value SHALL be returned to rd

#### Scenario: Immediate-variant write value uses uimm not rs1
- **WHEN** `csrrwi x1, mstatus, 5` (inst encoding `0x300520F3`, funct3=101=CSRRWI, rd=x1, uimm=5) executed with x10=0xCAFEBABE
- **THEN** the `csr_keys<T>::CSR_WRITE_VALUE` payload SHALL be `5` (uimm zero-extended to 32-bit), NOT `0xCAFEBABE` (rs1 value)
- **AND** the `csr_keys<T>::CSR_WRITE_EN` payload SHALL be `true` (funct3=101, imm5=5 != 0)

#### Scenario: CSR-Write payload propagation across stages
- **WHEN** a Zicsr instruction executed and `sim->tick()` advances through fetch → decode → execute → memory → writeback stages (full 5-stage pipeline)
- **THEN** the `csr_keys<T>::CSR_WRITE_ADDR/VALUE/EN` payload SHALL be visible at the writeback stage node (propagated by `cpu_factory_chmem.h` §2b/§2c stage links)
- **AND** the propagation SHALL NOT lose payload data across stages (Payload<bool>/Payload<T> round-trip preserves value)
- **NOTE**: This requires `cpu_factory_chmem.h` to extend its stage-link whitelist to include `CSR_WRITE_*` keys (R3.5 [GREEN] implementation prerequisite)

#### Scenario: mcause/mepc single-driver invariant maintained
- **WHEN** a CSRRW to `mcause` (csr_addr=0x342) executed at cycle N
- **THEN** `mcause_` SHALL receive exactly one `<<=` per cycle from writeback-NORMAL (the CSR write)
- **AND** `branch-LATE` closure SHALL NOT also write `mcause_` in the same cycle for trap instructions (the writes from branch-LATE for ECALL/EBREAK and from writeback-NORMAL for CSRRW are temporally distinct: branch operates on cycle N's PC, writeback operates on cycle N-1's CSRRW; the CppHDL elaboration must serialize these without conflict)
- **NOTE**: This requires Phase 0 smoke test (Task 0.2) to confirm CppHDL 5.052 behavior on multi-driver ch_reg <<= scenarios. If elaboration fails or produces undefined behavior, the implementation MUST add a `selector` to arbitrate between branch-LATE and writeback-NORMAL writes (branch takes priority for trap instructions: `select(is_trap_instr, branch_write, writeback_write)`)

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
- **WHEN** `csrrw x1, mstatus, x10` (inst `0x300510F3`) executed and `sim->tick()` called once
- **THEN** `sim->get_signal_value(plugin->last_csr_write_en_ch())` SHALL return `1`
- **AND** `sim->get_signal_value(plugin->last_csr_write_value_ch())` SHALL return `0xCAFEBABE`
- **AND** these values SHALL be deterministically set by the execute-LATE closure (no race with writeback)

#### Scenario: Observation port captures mcycle write enable false
- **WHEN** `csrrs x1, mcycle, x10` (inst `0xB00520F3`) executed and `sim->tick()` called once
- **THEN** `sim->get_signal_value(plugin->last_csr_write_en_ch())` SHALL return `0` (R3 decision: mcycle 永不被 CSRR* 写)
- **AND** `sim->get_signal_value(plugin->last_csr_write_value_ch())` SHALL return `0xCAFEBABE` (rs1 value, irrelevant since write_en=false)
- **AND** `sim->get_signal_value(plugin->last_csr_read_valid_ch())` SHALL return `0` (R3 decision: mcycle 单独路径)


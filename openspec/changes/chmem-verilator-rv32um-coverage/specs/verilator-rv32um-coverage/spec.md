# Spec: verilator-rv32um-coverage

> **Capability**: Verilator 路径下 rv32um 测试覆盖  
> **Change**: `chmem-verilator-rv32um-coverage`  
> **Created**: 2026-10-13

## ADDED Requirements

### Requirement: Verilator 仿真路径必须覆盖全部 8 个 rv32um ELF

The system SHALL provide a Catch2 test case that drives the Verilator-compiled 5-stage CH_MEM CPU against each of the 8 vendored `rv32um-p-*` ELF files (mul, mulh, mulhsu, mulhu, div, divu, rem, remu), verifying stdout contains `TOHOST=1` and the ELF name for each.

The test case MUST be discoverable under family tag `[verilator]` so it integrates with the existing `chipforge_tests_chmem` binary and can be invoked via `./build/bin/chipforge_tests_chmem "[verilator]"`.

#### Scenario: All 8 rv32um ELF pass on Verilator runner
- **WHEN** the developer runs `./build/bin/chipforge_tests_chmem "[verilator]"` from the repository root
- **THEN** the `cpu_verilator_sim_tohost1` Catch2 case reports 48 DYNAMIC_SECTION results (40 rv32ui + 8 rv32um), all passing
- **AND** each rv32um ELF section verifies `TOHOST=1` in `cpu_verilator_sim` stdout within the configured cycle cap (`--cycles 2000`)

#### Scenario: rv32um cycle count is recorded in stdout for manual verification
- **WHEN** any rv32um ELF completes on Verilator
- **THEN** the `CYCLES=<n>` field SHALL appear in `cpu_verilator_sim` stdout (existing runner output format documented at `test_cpu_verilator_sim.cpp:12`)
- **AND** the executor MUST manually compare `<n>` against the TLM baseline in `soc/cpu/docs/dse/rv32um-baseline-matrix.csv` (mul=455, mulh/mulhsu/mulhu=504, div=380, divu=381, rem=380, remu=380) within ±5% relative tolerance OR within absolute tolerance of +50 cycles (whichever is larger), recording the comparison in the commit message
- **AND** automated cycle assertion is explicitly deferred to a future DSE follow-up change (per design.md Q2)

#### Scenario: Existing rv32ui coverage remains green
- **WHEN** the Verilator kCases are extended with 8 rv32um entries
- **THEN** the 40 pre-existing rv32ui DYNAMIC_SECTION results MUST still pass
- **AND** no test count regression in the `[verilator]` family (40 → 48, not 40 → 40)

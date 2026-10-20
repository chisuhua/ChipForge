# verilator-rv32um-coverage Specification

## Purpose
TBD - created by archiving change chmem-verilator-rv32um-coverage. Update Purpose after archive.
## Requirements
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
- **AND** the executor SHALL record `<n>` in the commit message for manual inspection
- **AND** the executor SHALL NOT compare `<n>` against the TLM baseline in `soc/cpu/docs/dse/rv32um-baseline-matrix.csv` (mul=455, mulh/mulhsu/mulhu=504, div=380, divu=381, rem=380, remu=380) within ±5% relative tolerance OR within absolute tolerance of +50 cycles: the combinational CH_MEM M-op datapath resolves in a single cycle and is **not cycle-homologous** with the TLM multi-cycle radix-2 FSM (`ip/cpu/arch/riscv/mul_div_fsm.h:144-148` — MUL=1c / MULH=3c / DIV=33c), so such a comparison is **not meaningful** rather than failing
- **AND** automated cycle assertion remains deferred; cycle-parity acceptance for the Verilator path is deferred to change `chmem-mext-fsm-cycle-parity`, which restores a multi-cycle M-op datapath

#### Scenario: Existing rv32ui coverage remains green
- **WHEN** the Verilator kCases are extended with 8 rv32um entries
- **THEN** the 40 pre-existing rv32ui DYNAMIC_SECTION results MUST still pass
- **AND** no test count regression in the `[verilator]` family (40 → 48, not 40 → 40)


# Spec: chmem-rv32um-coverage

> **Capability**: CH_MEM 路径下 rv32um 测试覆盖  
> **Change**: `chmem-verilator-rv32um-coverage`  
> **Created**: 2026-10-13

## ADDED Requirements

### Requirement: CH_MEM Simulator 路径必须覆盖全部 8 个 rv32um ELF

The system SHALL provide a Catch2 test case that runs the CH_MEM (CppHDL) Simulator against each of the 8 vendored `rv32um-p-*` ELF files (mul, mulh, mulhsu, mulhu, div, divu, rem, remu), verifying `DMem[0x1000] (tohost) == 1` for each.

The test case MUST be discoverable under family tag `[chmem]` so it integrates with the existing `chipforge_tests_chmem` binary and can be invoked via `./build/bin/chipforge_tests_chmem "[chmem]"`.

#### Scenario: All 8 rv32um ELF pass on CH_MEM Simulator
- **WHEN** the developer runs `./build/bin/chipforge_tests_chmem "[chmem]"` from the repository root
- **THEN** the `chmem_vendored_elf_tohost1` Catch2 case reports 46 DYNAMIC_SECTION results (38 rv32ui + 8 rv32um), all passing
- **AND** each rv32um ELF section verifies `DMem[0x1000] == 1` within the configured cycle cap (currently `kMaxCycles=1500` in-source constant at `tests/cpu/test_cpu_chmem_vendored_elf.cpp:43`)

#### Scenario: rv32um cycle count is recorded in stdout for manual verification
- **WHEN** any rv32um ELF completes on CH_MEM Simulator
- **THEN** the reported cycle index SHALL be printed via Catch2 `INFO` macro (existing pattern at `test_cpu_chmem_vendored_elf.cpp:150-160`)
- **AND** the executor MUST manually compare the printed cycle index against the TLM baseline in `soc/cpu/docs/dse/rv32um-baseline-matrix.csv` (mul=455, mulh/mulhsu/mulhu=504, div=380, divu=381, rem=380, remu=380) within ±5% relative tolerance OR within absolute tolerance of +50 cycles (whichever is larger), recording the comparison in the commit message
- **AND** automated cycle assertion is explicitly deferred to a future DSE follow-up change (per design.md Q2)

#### Scenario: Existing rv32ui coverage remains green
- **WHEN** the CH_MEM kCases are extended with 8 rv32um entries
- **THEN** the 38 pre-existing rv32ui DYNAMIC_SECTION results MUST still pass
- **AND** no test count regression in the `[chmem]` family (38 → 46, not 38 → 38)

#### Scenario: Elaboration succeeds for all 8 rv32um ELF
- **WHEN** the CH_MEM Simulator is built for any of the 8 rv32um ELF
- **THEN** the elaboration step (`pb->elaborate(ctx)`) MUST return success for each ELF
- **AND** any elaboration failure causes the corresponding DYNAMIC_SECTION to fail immediately, before cycle simulation begins
- **NOTE**: This invariant is already covered by the existing `REQUIRE((pb->elaborate(ctx)).has_value())` check at `test_cpu_chmem_vendored_elf.cpp:141`; no new assertion needed, but behavior MUST hold for rv32um.

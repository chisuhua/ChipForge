# chmem-rv32um-coverage — Delta Spec

> **Change**: `chmem-mext-comb-datapath`
> **Created**: 2026-10-20
> **Base capability**: `openspec/specs/chmem-rv32um-coverage/spec.md`
> **Delta type**: `## MODIFIED Requirements` — 整块拷贝原 requirement（含全部 scenario），**仅改写第 2 个 scenario**（cycle 比对）。
> **为什么改**: 组合式 CH_MEM M-op 1 拍出结果，与 TLM radix-2 多周期 FSM（MUL=1c / MULH=3c / DIV=33c）**不同构**；
> 原 scenario 的 "±5% 或 +50c" 比对在组合式实现下**无意义**（不是"漏比对"，是判据本身不适用）。
> 覆盖类 Requirement（8 ELF 全 PASS / rv32ui 不回归 / elaboration 成功）**保持原样不变**。

## MODIFIED Requirements

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
- **AND** the executor SHALL record the printed cycle index in the commit message for manual inspection
- **AND** the executor SHALL NOT compare the printed cycle index against the TLM baseline in `soc/cpu/docs/dse/rv32um-baseline-matrix.csv` (mul=455, mulh/mulhsu/mulhu=504, div=380, divu=381, rem=380, remu=380) within ±5% relative tolerance OR within absolute tolerance of +50 cycles: the combinational CH_MEM M-op datapath resolves in a single cycle and is **not cycle-homologous** with the TLM multi-cycle radix-2 FSM (`ip/cpu/arch/riscv/mul_div_fsm.h:144-148` — MUL=1c / MULH=3c / DIV=33c), so such a comparison is **not meaningful** rather than failing
- **AND** automated cycle assertion remains deferred; cycle-parity acceptance for the CH_MEM path is deferred to change `chmem-mext-fsm-cycle-parity`, which restores a multi-cycle M-op datapath

#### Scenario: Existing rv32ui coverage remains green
- **WHEN** the CH_MEM kCases are extended with 8 rv32um entries
- **THEN** the 38 pre-existing rv32ui DYNAMIC_SECTION results MUST still pass
- **AND** no test count regression in the `[chmem]` family (38 → 46, not 38 → 38)

#### Scenario: Elaboration succeeds for all 8 rv32um ELF
- **WHEN** the CH_MEM Simulator is built for any of the 8 rv32um ELF
- **THEN** the elaboration step (`pb->elaborate(ctx)`) MUST return success for each ELF
- **AND** any elaboration failure causes the corresponding DYNAMIC_SECTION to fail immediately, before cycle simulation begins
- **NOTE**: This invariant is already covered by the existing `REQUIRE((pb->elaborate(ctx)).has_value())` check at `test_cpu_chmem_vendored_elf.cpp:141`; no new assertion needed, but behavior MUST hold for rv32um.

## Purpose

Document the Dhrystone benchmark harness contract + DMIPS/MHz formula + real gate, to support v0.11.0 launch gate §8 Sprint 1 判据 #3 (DMIPS/MHz ≥ 1.4 硬门禁).

## Requirements

### Requirement: Dhrystone benchmark MUST use standard DMIPS formula

The Dhrystone benchmark in `tests/cpu/integration/test_dhrystone.cpp` MUST use the standard DMIPS/MHz formula:
- `DMIPS/MHz = N * 1e6 / (1757 * cycles)`
- N = iteration count (kDhrystoneRuns = 2000)
- cycles = measured cycle count from `pb->run()` loop

The 1.4 DMIPS/MHz hard gate corresponds to: `cycles ≤ 813,043` (= N * 1e6 / (1757 * 1.4)).

The legacy formula `1757 * N / cycles` (off by 3.087x, since 1757²/1e6 = 3.087) is BANNED.

The Dhrystone harness MUST satisfy:
- Linker script `tests/cpu/manual_elf/link_dhrystone.ld` (base=0x80000000 + .tohost section @ 0x80001000)
- crt0 sp init via `tests/cpu/manual_elf/dhrystone/dhrystone_crt0.S` (set sp=0x8000F000 before main loop)
- Build via `RISCV_GPP -O2 -fno-inline -march=rv32im_zicsr -mabi=ilp32 -nostdlib -static` (Dhrystone 规范要求 -O2)
- Test config: `window_base = 0x80000000` hardcode (not `entry & ~0xFFFF` fallback)
- `tohost_addr = 0x80001000` hardcode (not 0 fallback for no-.tohost-section)
- `kMaxCycles = 5'000'000` cap (not 100M, to fit ctest wall-clock)

#### Scenario: 2000 iter Dhrystone PASS with 1.4 DMIPS/MHz gate (B1+B2+B3 fixed)
- **GIVEN** `link_dhrystone.ld` (base=0x80000000 + .tohost section @ 0x80001000) + crt0 sp init (inline asm `li sp, 0x8000F000`)
- **AND** `test_dhrystone.cpp` window_base=0x80000000 hardcode (no `entry & ~0xFFFF` fallback)
- **AND** `RISCV_GPP -O2 -fno-inline` build flag
- **WHEN** 跑 `[cpu-integration][dhrystone] dhrystone_legacy_tohost_pass` + `dhrystone_fsm_dmips_mhz_minimum_1_4`
- **THEN** both TEST_CASE PASS, cycles < 813,043 (= 1.4 DMIPS/MHz 真 gate), exited=true
- **AND** 公式 `DMIPS/MHz = N*1e6/(1757*cycles)` 计算正确 (无 3.087x 宽)

#### Scenario: CSV baseline MUST 引用 feat-dhrystone-dmips-gate commit hash (N1 AGENTS.md 数字维护)
- **GIVEN** `soc/cpu/docs/dse/dhrystone-baseline-matrix.csv`
- **WHEN** 写入 DMIPS/MHz 真值
- **THEN** notes 字段 MUST 引用本 change commit hash (符合 AGENTS.md §数字维护原则)
- **AND** 旧 0.0351 DMIPS/MHz baseline (pre-df3bcea 伪测量值, due to harness contract violations) MUST 重写为 HEAD 实测真基线

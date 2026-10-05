## ADDED Requirements

### Requirement: L1Cache + MMU Verilator full-chain e2e

The `--enable-mmu --mmu-mode bare --enable-cache` flag combination on `cpu_verilator_sim` SHALL reach `tohost=1` on the vendored `tests/cpu/manual_elf/l1cache_basic.elf` (which exercises 1 cache hit + 1 cache miss path). The cycle count SHALL be `≤ baseline_csv[l1cache_basic][cache_mmu_bare] × 1.5`, where `baseline_csv[l1cache_basic][cache_mmu_bare]` is loaded from `CF_L1CACHE_VERILATOR_BASELINE_CSV` compile definition.

#### Scenario: 三 flag 联动链路可达
- **WHEN** user invokes `cpu_verilator_sim --enable-mmu --mmu-mode bare --enable-cache --elf tests/cpu/manual_elf/l1cache_basic.elf --cycles 2000`
- **THEN** the runner SHALL output `TOHOST=1 PASS` AND the cycle count SHALL be `≤ baseline × 1.5` (manual_elf 接受标准, 不在 5 ELF baseline 表)。

#### Scenario: prerequisite validation
- **WHEN** this scenario is invoked without prior completion of change `verilator-cpu-factory-extensible-params` (Change 1, archive) AND `cache-phase1.5-4way` (archive) AND this change's task group 1.5 (L1Cache CH_MEM wiring 翻转)
- **THEN** the test SHALL FAIL with one of:
  - Change 1 未 archive: `--enable-cache` flag 不识别
  - cache-phase1.5-4way 未 archive: `ip/cache/tlm/l1_cache_chmem.h` 不存在, link error
  - task group 1.5 未完成: `--enable-cache` 路径 throw "L1Cache CH_MEM not implemented"

This cross-cutting dependency is captured as a 3-layer check (CLI flag → source file → factory behavior) to prevent silent failure.

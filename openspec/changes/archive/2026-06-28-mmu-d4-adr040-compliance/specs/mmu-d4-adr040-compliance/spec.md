## ADDED Requirements

### Requirement: Architecture Gate `verify_plugin_decision` passes with `ip/mmu/` present

The project MUST pass `tools/verify_plugin_decision.sh` (Architecture Gate, ADR-043) with exit code 0 when `ip/mmu/` skeleton code (mmu-ip-skeleton, 2026-06-29) is included in `TARGET_DIRS`.

The gate composes three D4 checks (verify_plugin_decision.sh §1-§3) and four ADR-040 checks (check_plugin_portability.sh §1-§4). The skeleton's `ip/mmu/` MUST satisfy:

- **D4 #3 (Bundle 字段类型)**: No `(ch_uint<\d+>|uint(8|16|32|64)_t)` followed by `addr|data|tag|idx|valid|burst_len|is_write` in `ip/mmu/**/*.h|*.cpp`.
- **ADR-040 #1 (at_stage 早返)**: No `return;` statement inside any `pb.at_stage(...)` lambda body in `ip/mmu/tlm/**/*.cpp`.

#### Scenario: `verify_plugin_decision` exits 0
- **WHEN** `ctest --test-dir build -R verify_plugin_decision --output-on-failure` is executed
- **THEN** the script MUST return exit code 0
- **AND** MUST print `=== D4 + ADR-040 检查全部通过 (3+4/3) ===`

#### Scenario: Full `ctest` regression passes
- **WHEN** `ctest --test-dir build --output-on-failure` is executed
- **THEN** 43 of 43 tests MUST pass (including `verify_plugin_decision`)
- **AND** `Total Test time` MUST be ≤ 2 seconds (no per-test regression)

#### Scenario: `ip/mmu/lib/tlb.h` no longer matches D4 #3 grep pattern
- **WHEN** `grep -nE "(ch_uint<\d+>|uint(8|16|32|64)_t) +(addr|data|tag|idx|valid|burst_len|is_write)" ip/mmu/lib/tlb.h` is executed
- **THEN** the command MUST exit with no matches (exit code 1)

#### Scenario: `ip/mmu/tlm/MMUPlugin.cpp` has no `return;` inside at_stage bodies
- **WHEN** `tools/check_plugin_portability.sh` runs the awk detector against `ip/mmu/tlm/MMUPlugin.cpp`
- **THEN** the detector MUST emit no `MMUPlugin.cpp:N:    return;` lines
- **AND** MUST report `[PASS] at_stage 回调内无 'if (cond) return;' 早返`
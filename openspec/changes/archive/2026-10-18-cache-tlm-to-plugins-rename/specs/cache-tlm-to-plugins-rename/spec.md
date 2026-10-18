# cache-tlm-to-plugins-rename Specification

## Purpose

Document the contract for cache-tlm-to-plugins-rename: rename `ip/cache/tlm/` to `ip/cache/plugins/` for ADR-083 v2.0 alignment with `ip/cpu/plugins/`. Pure path-rename refactor, 0 business code semantics changes.

## ADDED Requirements

### Requirement: include-paths-updated-to-plugins

The system SHALL replace `#include "ip/cache/tlm/..."` with `#include "ip/cache/plugins/..."` at 9 call sites:

1. `src/cf_plugin/bridge/l1_cache_bridge.h:27`
2. `src/cf_plugin/bridge/l1_cache_bridge.cpp:26`
3. `src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp:23`
4. `tests/cache/test_l1_cache_bridge.cpp:17`
5. `tests/cache/test_l1_cache_plugin_unit.cpp:34`
6. `tests/cache/test_mmu_cache_integration.cpp:10`
7. `tests/cache/test_l1cache_refill_fsm_chmem.cpp:36` (note: `_chmem.h` suffix preserved)
8. `ip/cache/README.md:124` (code example in markdown)
9. `ip/cache/tlm/L1CachePlugin.cpp:26` (self-include, migrates with git mv)
10. `tests/cache/test_l1_cache_plugin_unit.cpp:24` (comment-only reference)

#### Scenario: cmake build green
- **WHEN** `cmake --build build -j$(nproc)` runs after change
- **THEN** 0 error; 0 warning new (per baseline).

### Requirement: cmake-paths-updated

The system SHALL update `tests/CMakeLists.txt:20` from `${CMAKE_SOURCE_DIR}/ip/cache/tlm/L1CachePlugin.cpp` to `${CMAKE_SOURCE_DIR}/ip/cache/plugins/L1CachePlugin.cpp`.

#### Scenario: cmake configure passes
- **WHEN** `cmake -B build -DCMAKE_BUILD_TYPE=Debug` runs
- **THEN** no error related to L1CachePlugin.cpp path.

### Requirement: verify-adr-grep-pattern

The system SHALL update `tools/verify_adr.sh` directory-existence check (test for `ip/cache/...` invalid ADR pattern) to use `plugins/` instead of `tlm/`.

#### Scenario: verify_adr green
- **WHEN** `bash tools/verify_adr.sh` runs after change
- **THEN** 0 fail.

### Requirement: cache-readme-md-aligned

The system SHALL update `ip/cache/README.md` 4 instances of `ip/cache/tlm/` references (L3 status paragraph, L29 directory structure table, L124 code example, L126 namespace reference `cf::ip::cache::tlm::L1CachePlugin`).

#### Scenario: readme tlm absent
- **WHEN** `grep -n 'tlm' ip/cache/README.md` runs
- **THEN** 0 hits (or only historical-context mentions, not file-path references).

### Requirement: cache-docs-secondary-aligned

The system SHALL update `ip/cache/docs/integration.md` (L103 + L106 grep examples) + `ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md` (5 file path references) to use `plugins/` path.

#### Scenario: cache docs tlm free
- **WHEN** `grep -rn 'ip/cache/tlm' ip/cache/docs/` runs
- **THEN** 0 hits.

### Requirement: cpu-mmu-docs-aligned

The system SHALL update `ip/cpu/docs/{dse_architecture.md, implementation-plan/README.md, implementation-plan/M1-cpu-skeleton.md, research/ooo_forward_compat_gap_analysis.md}` + `ip/mmu/docs/architecture.md` (L77 + L85 + L149 + L159) to use `ip/cache/plugins/` references.

#### Scenario: cross-ip docs aligned
- **WHEN** `grep -rn 'ip/cache/tlm' ip/cpu/docs/ ip/mmu/docs/` runs
- **THEN** 0 hits.

### Requirement: framework-docs-aligned

The system SHALL update `docs/architecture/adr/ADR-040-tlm-hdl-portability-constraints.md:59`, `docs/architecture/adr/ADR-041-bridge-tick-pattern.md:16+:138`, `docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md:265`, `docs/architecture/adr.md`, `docs/architecture/declarative-hybrid-framework.md`, `docs/architecture/interface-design.md`, `docs/architecture/roadmap-evolution.md`, `docs/lessons/phase-1.2-l1cacheplugin.md`, `docs/methodology/plugin-style-design-methodology-v1.md`, `docs/superpowers/plans/2026-06-30-catch2-test-framework.md` — all `ip/cache/tlm/` path references → `ip/cache/plugins/`.

#### Scenario: framework docs tlm-free
- **WHEN** `grep -rn 'ip/cache/tlm' docs/` runs
- **THEN** 0 hits.

### Requirement: rddf-roadmap-aligned

The system SHALL update `.rddf/roadmap/strategy.md` + `.rddf/roadmap/archive/phase-1-tlm-foundation.md` (6 occurrences) to use `ip/cache/plugins/` references.

#### Scenario: rddf rdmap aligned
- **WHEN** `grep -rn 'ip/cache/tlm' .rddf/roadmap/` runs
- **THEN** 0 hits.

### Requirement: openspec-specs-aligned

The system SHALL update `openspec/specs/mmu-cache-idx-vipt/spec.md:31` + `openspec/specs/mmu-vipt-data-flow/spec.md:43+:67` to use `ip/cache/plugins/` references.

#### Scenario: openspec specs tlm-free
- **WHEN** `grep -rn 'ip/cache/tlm' openspec/specs/` runs
- **THEN** 0 hits.

### Requirement: changelog-history-preserved

The system SHALL update ≥7 occurrence in `CHANGELOG.md` (historical release records) BUT preserve historical truth by adding `legacy: ip/cache/tlm/` annotations (i.e., show both old and new path with note explaining the rename).

#### Scenario: changelog both paths visible
- **WHEN** a reader inspects a historical release entry
- **THEN** they can see what the path was at that time AND that it has since been renamed.

### Requirement: regression-baseline-locked

The system SHALL pass regression baseline (locked before change): `[cache] 7 cpp / 28 TEST_CASE PASS`, `[cpu-l1-mmu-demo] 6/6`, `[mmu] 59/59`, `[mmu-verilator] 3/3`, `[riscv-tests] 48/48`, `[framework] 89/89`, `[verilator] 1/1` ([chmem] binary).

#### Scenario: regression 0
- **WHEN** rename commit landed
- **THEN** ctest reports no FAIL new (compared to baseline locked in change tasks.md Phase 0).

### Requirement: archive-exemption-handled

The system SHALL NOT modify any file in `openspec/changes/archive/`. The 58 occurrences of `ip/cache/tlm/` across 32 archive files are exempt via `tools/doc_checker.py:76 DEFAULT_SKIP_PATTERNS` (which already includes `openspec/changes/archive`).

#### Scenario: archive untouched
- **WHEN** `git diff origin/main -- openspec/changes/archive/` runs after change
- **THEN** empty output (no archive modification).

### Requirement: zero-hit-acceptance-with-change-self-exclusion

The system SHALL satisfy: `grep -rn 'ip/cache/tlm' . | grep -v '/build/' | grep -v '/.git/' | grep -v 'openspec/changes/archive' | grep -v 'cache-tlm-to-plugins-rename/' | grep -v '.rddf/improvements/cache-tlm-to-plugins-rename.md'` returns 0 hits, AFTER the rename is complete.

#### Scenario: zero hit
- **WHEN** acceptance check runs
- **THEN** 0 output (excluding change self-references in proposal/spec/tasks/improvement draft).

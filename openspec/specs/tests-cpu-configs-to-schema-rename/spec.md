# tests-cpu-configs-to-schema-rename Specification

## Purpose
TBD - created by archiving change tests-cpu-configs-to-schema-rename. Update Purpose after archive.
## Requirements
### Requirement: directory-rename-atomic
The system SHALL rename `tests/cpu/configs/` to `tests/cpu/schema/` containing `test_schema_m5_19.cpp`. Git mv preserves file history.

#### Scenario: rename with history
- **WHEN** `git mv tests/cpu/configs tests/cpu/schema` executes
- **THEN** `test_schema_m5_19.cpp` relocated; `git log --follow` shows prior history.

### Requirement: docs-superpowers-path-updated
The system SHALL update `docs/superpowers/plans/2026-06-30-catch2-test-framework.md` (1 occurrence) to reference `tests/cpu/schema/` instead of `tests/cpu/configs/`.

#### Scenario: superpowers docs aligned
- **WHEN** `grep -rn 'tests/cpu/configs' docs/superpowers/` runs
- **THEN** 0 hits.

### Requirement: rddf-plan-path-updated
The system SHALL update `.rddf/plans/cpu-pipeline-tick-rca.md` (1 occurrence) to reference `tests/cpu/schema/`.

#### Scenario: rddf plans aligned
- **WHEN** `grep -rn 'tests/cpu/configs' .rddf/plans/` runs
- **THEN** 0 hits.

### Requirement: improvement-drafts-path-updated
The system SHALL update `.rddf/improvements/cache-tlm-to-plugins-rename.md` + `.rddf/improvements/tests-soc-topology-restructure.md` (1 occurrence each, dependency drift sync) to reference `tests/cpu/schema/`.

#### Scenario: improvements drafts aligned
- **WHEN** `grep -rn 'tests/cpu/configs' .rddf/improvements/` runs
- **THEN** 0 hits (or only legacy-tagged historical reference).

### Requirement: archive-exemption-handled
The system SHALL NOT modify any file in `openspec/changes/archive/2026-10-15-cpu-pipeline-tick-rca/`. The 3 occurrences of `tests/cpu/configs/` across 3 archive files are exempt via `tools/doc_checker.py:76 DEFAULT_SKIP_PATTERNS` (which already includes `openspec/changes/archive`).

#### Scenario: archive untouched
- **WHEN** `git diff origin/main -- openspec/changes/archive/` runs
- **THEN** empty output.

### Requirement: regression-baseline-maintained
The system SHALL pass `[cpu-configs] 1 cpp PASS` + `[cpu] 129/129` + `[cpu-integration] 33/36` (3 baseline-known FAIL pre-existing) + `[cpu-l1-mmu-demo] 6/6` + `[soc] 20/20` + `[mmu] 59/59` + `[cache] 25/25` + `[riscv-tests] 48/48`.

#### Scenario: regression 0
- **WHEN** rename commit landed
- **THEN** ctest reports no FAIL new (compared to baseline locked in change tasks.md Phase 0).

### Requirement: zero-hit-acceptance-with-archive-exclusion
The system SHALL satisfy: `grep -rn 'tests/cpu/configs' . | grep -v '/build/' | grep -v '/.git/' | grep -v 'openspec/changes/archive' | grep -v 'tests-cpu-configs-to-schema-rename/' | grep -v '.rddf/improvements/tests-cpu-configs-to-schema-rename.md'` returns 0 hits AFTER the rename is complete.

#### Scenario: zero hit
- **WHEN** acceptance check runs
- **THEN** 0 output (excluding change self-references + archive + improvement drafts).


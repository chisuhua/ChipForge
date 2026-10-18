## ADDED Requirements

### Requirement: tests-soc-subdirs-existence
The system SHALL provide 3 subdirectories under `tests/soc/`: `soc-cpu-l1-mmu-demo/`, `mmu-minimal/`, `l1-cache-minimal/`, each containing `test_*.cpp` files migrated via `git mv` (history preserved).

#### Scenario: subdir structure
- **WHEN** `ls tests/soc/` runs after rename
- **THEN** 3 subdirs + 1 top-level README.md visible (no flat file_*.cpp at top).

### Requirement: subdir-readme-md
The system SHALL provide `README.md` in each new subdirectory explaining the SoC topology and linking to the corresponding `soc/<name>.json`.

#### Scenario: readme cross-ref
- **WHEN** `cat tests/soc/soc-cpu-l1-mmu-demo/README.md` runs
- **THEN** content references `soc/cpu_l1_mmu_demo.json` and lists [soc][cpu-l1-mmu-demo] family tag.

### Requirement: openspec-active-paths-updated
The system SHALL update paths in 4 active openspec changes (`mmu-chmem-pipeline-integration/`, `mmu-chmem-phase-b-c-launch/`, `mmufault-verilator-sv32-e2e-flip/`, `cpu-factory-satp-mapping/`) and 6 spec files to use new subdir paths.

#### Scenario: openspec spec paths correct
- **WHEN** `grep -rn 'tests/soc/test_' openspec/ AGENTS.md CHANGELOG.md .rddf/ | grep -v /archive/` runs
- **THEN** 0 hits.

### Requirement: archive-path-exception-list
The system SHALL maintain `tools/doc_link_check.sh` KNOWN_LEGACY regex `^openspec/changes/archive/.*:.*tests/soc/test_` to skip 23 archive hits.

#### Scenario: doc_link_check green
- **WHEN** `bash tools/doc_link_check.sh` runs after rename
- **THEN** 0 fail.

### Requirement: regression-tests-pass
The system SHALL pass `[soc] 5 cpp + [cpu-l1-mmu-demo] 6/6 + [mmu] 59/59 + [cache] 7/7` in `bash tools/run_chipforge_tests.sh`.

#### Scenario: regression 0
- **WHEN** rename commit landed
- **THEN** ctest reports no FAIL new.

### Requirement: page-table-helpers-h-relative-include
The system SHALL keep `#include "page_table_helpers.h"` working after migration (relative include, resolved at compile time).

#### Scenario: include resolves
- **WHEN** `tests/soc/soc-cpu-l1-mmu-demo/test_cpu_l1_mmu_demo.cpp` is compiled
- **THEN** `page_table_helpers.h` resolves via same-dir relative path.

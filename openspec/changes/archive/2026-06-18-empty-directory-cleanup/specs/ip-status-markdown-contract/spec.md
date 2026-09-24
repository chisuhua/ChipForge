## ADDED Requirements

### Requirement: Zero-Code IPs Have STATUS.md Marker

The 5 IPs with 0 LOC of implementation code (memory, interconnect, peripheral, tilecore, tilecopy) MUST each have a `STATUS.md` file at the IP root directory.

#### Scenario: All 5 zero-code IPs have STATUS.md
- **WHEN** `ls ip/memory/STATUS.md ip/interconnect/STATUS.md ip/peripheral/STATUS.md ip/tilecore/STATUS.md ip/tilecopy/STATUS.md` is executed
- **THEN** all 5 files MUST exist
- **AND** each MUST be non-empty

#### Scenario: STATUS.md contains required sections (PLANNED variant)
- **WHEN** `ip/memory/STATUS.md` (or `ip/interconnect/STATUS.md` or `ip/peripheral/STATUS.md`) is read
- **THEN** it MUST contain a `# STATUS: PLANNED (0 LOC)` heading
- **AND** MUST contain a "## Implementation Roadmap" section with a roadmap link
- **AND** MUST contain a "## 已知限制" section noting that subdirectories will be created on implementation

#### Scenario: STATUS.md contains required sections (INITIAL DESIGN variant for tilecore/tilecopy)
- **WHEN** `ip/tilecore/STATUS.md` (or `ip/tilecopy/STATUS.md`) is read
- **THEN** it MUST contain a `# STATUS: INITIAL DESIGN` heading (NOT "PLANNED" because design docs exist)
- **AND** MUST contain a "## Existing Assets" section listing `docs/architecture.md` size and the `policies/` empty directory
- **AND** MUST contain a "## Implementation Roadmap" section with a Phase 5+ roadmap link
- **AND** MUST contain a "## 已知限制" section

#### Scenario: STATUS.md updated when implementation starts
- **WHEN** a developer adds the first .h file to a previously PLANNED IP
- **THEN** the developer MUST update `STATUS.md` from PLANNED to PARTIAL within the same commit
- **AND** MUST update the LOC count and add an "## Existing Assets" section listing the new files

### Requirement: STATUS.md Template File Exists at Standardized Path

A reusable STATUS.md template MUST be created at `docs/templates/IP_STATUS_TEMPLATE.md`. All IP STATUS.md files MUST conform to this template's section structure (using one of the 2 approved variants: PLANNED or INITIAL DESIGN).

#### Scenario: Template file exists
- **WHEN** `test -f docs/templates/IP_STATUS_TEMPLATE.md` is executed
- **THEN** the file MUST exist and be non-empty
- **AND** MUST contain the 2 approved STATUS variants (PLANNED + INITIAL DESIGN)

#### Scenario: All STATUS.md files reference the template
- **WHEN** any `ip/*/STATUS.md` is read
- **THEN** it MUST reference `../docs/templates/IP_STATUS_TEMPLATE.md` in a comment
- **AND** MUST declare which variant it uses (PLANNED or INITIAL DESIGN)

### Requirement: No Empty Subdirectories for Unimplemented IPs

The 5 zero-code IPs (memory/interconnect/peripheral/tilecore/tilecopy) MUST NOT contain empty `tlm/`, `rtl/`, `test/`, `configs/` subdirectories.

#### Scenario: Subdirectories are absent
- **WHEN** `find ip/memory ip/interconnect ip/peripheral ip/tilecore ip/tilecopy -mindepth 1 -maxdepth 1 -type d \( -name tlm -o -name rtl -o -name test -o -name configs \) 2>/dev/null` is executed
- **THEN** the command MUST produce zero output (no empty `tlm/`/`rtl/`/`test/`/`configs/` subdirectories remain)
- **AND** the dirs that legitimately exist (tilecore/docs, tilecore/policies, tilecore/.test, tilecopy/docs, tilecopy/policies, tilecopy/.test) MUST NOT be affected

#### Scenario: Brace-expansion check matches actual deletion count
- **WHEN** `ls -d ip/memory/tlm ip/memory/rtl ip/memory/test ip/memory/configs ip/interconnect/tlm ip/interconnect/rtl ip/interconnect/test ip/interconnect/configs ip/peripheral/tlm ip/peripheral/rtl ip/peripheral/test ip/peripheral/configs ip/tilecore/tlm ip/tilecore/rtl ip/tilecore/configs ip/tilecopy/tlm ip/tilecopy/rtl ip/tilecopy/configs` is executed
- **THEN** the command MUST fail with "No such file or directory" for exactly 2 paths: `ip/tilecore/test` and `ip/tilecopy/test` (these never existed; tilecore and tilecopy lack the `test/` subdirectory but have `.test/` hidden directory which is preserved)
- **AND** the remaining 16 paths MUST NOT exist either (because they were deleted by task 2.1)

### Requirement: src/cf_plugin/tests/ Directory Removed

The empty `src/cf_plugin/tests/` directory MUST be removed, and the `src/cf_plugin/CMakeLists.txt` MUST explicitly document that tests live at `tests/framework/`.

#### Scenario: Directory does not exist
- **WHEN** `test -d src/cf_plugin/tests` is executed
- **THEN** the test MUST fail (directory absent)

#### Scenario: CMakeLists.txt references test location
- **WHEN** `src/cf_plugin/CMakeLists.txt` is read
- **THEN** it MUST contain a comment line stating "Tests for cf_plugin live at tests/framework/ (not src/cf_plugin/tests/)"
- **AND** the test source paths MUST reference `tests/framework/test_*.cpp`

### Requirement: ip/cpu/test/ Directory Removed

The empty `ip/cpu/test/` directory (README-only) MUST be removed, and `ip/cpu/README.md` MUST document that CPU tests live at `tests/cpu/`.

#### Scenario: ip/cpu/test/ does not exist
- **WHEN** `test -d ip/cpu/test` is executed
- **THEN** the test MUST fail (directory absent)

#### Scenario: ip/cpu/README.md references tests/cpu/
- **WHEN** `ip/cpu/README.md` is read
- **THEN** it MUST contain a "测试位置" or "Testing" section pointing to `../tests/cpu/`

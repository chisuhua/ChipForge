# doc-checker-skip-config Specification

## Purpose
TBD - created by archiving change doc-link-check-baseline-cleanup. Update Purpose after archive.
## Requirements
### Requirement: Default skip patterns for archived and external paths

The `tools/doc_checker.py` tool MUST skip the following path patterns by default when scanning for broken markdown links and ip_structure issues:

- `openspec/changes/archive/**` (historical OpenSpec change snapshots)
- `**/archive/**` (any directory explicitly marked as archived)
- `build/_deps/**` (ExternalProject build artifacts, never source-of-truth)
- `sail-riscv-Linux-x86_64/**` (vendored third-party repository, references to its own internal docs are valid)
- `**/cpp-tlm/**` and `**/CppHDL/**` (resolved framework paths under build/ or external checkouts)

#### Scenario: Archive changes excluded from link check

- **WHEN** `python tools/doc_checker.py --format json` is run against a repo with `openspec/changes/archive/2026-09-22-v06-static-config-result/` containing references to historical paths (e.g., `docs/superpowers/specs/2026-09-22-v0.6-result-error-handling-design.md`)
- **THEN** those historical-path references MUST NOT appear in `checks.links.errors`
- **AND** the archived change itself MUST NOT be flagged in any check

#### Scenario: Build artifacts excluded from link check

- **WHEN** `python tools/doc_checker.py --format json` is run and `build/_deps/install/include/cpp-tlm/AGENTS.md` exists with a reference into the deps tree
- **THEN** that reference MUST NOT appear in `checks.links.errors`
- **AND** the `build/` subtree MUST NOT be scanned for ip_structure either

#### Scenario: Vendored third-party excluded from link check

- **WHEN** `python tools/doc_checker.py --format json` is run against `sail-riscv-Linux-x86_64/share/doc/sail-riscv/ChangeLog.md`
- **THEN** its internal references to `../config/config.json.in` and `./AddingExtensions.md` MUST NOT appear in `checks.links.errors`

### Requirement: Opt-in archive scanning flag

The `tools/doc_checker.py` tool MUST accept a `--include-archive` flag that, when present, disables the default archive-skip behavior (sections to re-enable scanning for historical drift detection).

#### Scenario: Archive scanning enabled via flag

- **WHEN** `python tools/doc_checker.py --include-archive --format json` is run
- **THEN** `openspec/changes/archive/**` and `**/archive/**` MUST be scanned like any other markdown
- **AND** `checks.links.errors` MAY include broken archive links

#### Scenario: Default behavior unchanged without flag

- **WHEN** `python tools/doc_checker.py` is run without `--include-archive`
- **THEN** archive patterns MUST be skipped per Requirement 1
- **AND** exit code MUST reflect only non-archive broken references

### Requirement: IP structure allowlist for known-exempt directories

The `ip_structure_check` in `tools/doc_checker.py` MUST accept a `KNOWN_EXEMPT_TEST_DIRS` configuration allowing specific IPs to omit `test/` subdirectory when documented historical removal decisions exist.

#### Scenario: ip/cpu exempt from missing test/ check

- **WHEN** `python tools/doc_checker.py --format json` is run against a repo where `ip/cpu/test/` does not exist but `ip/cpu/README.md` documents the removal date (`CPU 测试 **不在** `ip/cpu/test/`（该目录已于 2026-06-17 删除）`)
- **THEN** `checks.ip.errors` MUST NOT contain `ip/cpu/: 缺少目录 test/`

#### Scenario: Non-exempt IPs still require test/

- **WHEN** `python tools/doc_checker.py --format json` is run against an IP without a `test/` directory and without a documented removal
- **THEN** `checks.ip.errors` MUST include the missing-test/ error for that IP

### Requirement: Framework path allowlist for documented-but-pending headers

The `framework` check in `tools/doc_checker.py` MUST accept a `FRAMEWORK_PENDING_HEADERS` configuration that marks specific CppHDL/CppTLM headers as known-missing/under-development so reference documents listing them are not flagged.

#### Scenario: Reference documenting future header not flagged

- **WHEN** `python tools/doc_checker.py --format json` is run and a reference document lists `CppHDL/include/axi4/axi4_lite.h` and `CppHDL/include/bundle/clock_reset_bundle.h` as planned/under-development
- **THEN** `checks.framework.errors` MUST NOT include those two paths
- **AND** a comment MUST be added to the allowlist entry explaining the pending status

#### Scenario: Unrelated missing framework path still flagged

- **WHEN** `python tools/doc_checker.py --format json` is run and a reference document lists `CppHDL/include/foo/bar.h` that is NOT in the allowlist
- **THEN** `checks.framework.errors` MUST include the `CppHDL/include/foo/bar.h` entry


# test-location-discipline Specification

## Purpose
Mandates that all IP-related test files (GoogleTest C++) live at `tests/<ip-name>/` (project root), not at `ip/<name>/test/`. Allows cross-IP integration tests to share `tests/` infrastructure, decouples IP test organization from IP source layout, and prevents the historical "test dir scattered per-IP" anti-pattern.
## Requirements
### Requirement: All IP Tests Live in tests/<ip-name>/

All IP-related test files (GoogleTest C++ files) MUST be located at `tests/<ip-name>/` (project root), NOT at `ip/<name>/test/`.

The only exception is when a test specifically tests IP-internal implementation details AND that test must run as part of the IP build (in which case it should be justified in the IP's README and tracked separately).

#### Scenario: No test_*.cpp files under ip/
- **WHEN** `find ip/ -name "test_*.cpp" 2>/dev/null` is executed
- **THEN** the command MUST return 0 results (no IP-internal test files)

#### Scenario: All test_*.cpp files are under tests/
- **WHEN** `find tests/ -name "test_*.cpp" 2>/dev/null` is executed
- **THEN** the result MUST include all IP test files (cache, cpu, bundles, framework, soc, etc.)
- **AND** each IP's test directory MUST be at `tests/<ip-name>/` (e.g., `tests/cache/`, `tests/cpu/`)

### Requirement: Existing IP Test Locations Documented in IP README

Any IP whose tests live at a non-standard location (e.g., `tests/<other-name>/` for cross-IP integration tests) MUST have the location documented in its README.

#### Scenario: Cross-IP integration tests have a documented location
- **WHEN** an integration test exists that tests multiple IPs (e.g., L1Cache + CPU)
- **THEN** each affected IP's README MUST list the integration test location
- **AND** MUST link to the test file path


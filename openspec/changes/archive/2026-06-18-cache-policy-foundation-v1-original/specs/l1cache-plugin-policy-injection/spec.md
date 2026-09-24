## ADDED Requirements

### Requirement: ip/cache/policies/ Directory Exists with Header Files

The `ip/cache/policies/` directory MUST exist and contain at least these 3 header files:
- `replacement_policy.h` (abstract interface)
- `lru_policy.h` (LRU implementation)
- `no_replacement_policy.h` (no-op default)

#### Scenario: All 3 header files exist
- **WHEN** `ls ip/cache/policies/` is executed
- **THEN** it MUST list `replacement_policy.h`, `lru_policy.h`, `no_replacement_policy.h`

#### Scenario: Header files compile standalone
- **WHEN** each header is compiled with `-Wall -Wextra -Wpedantic`
- **THEN** it MUST compile without warnings (Phase 0 quality bar)
- **AND** MUST NOT introduce external dependencies (only `<cstdint>`, `<memory>`, `<string>`, `<stdexcept>`)

### Requirement: ip/cache/policies/ Conforms to ip/README.md Standard Structure

The `policies/` subdirectory MUST follow the `ip/README.md` standard structure (listed in the IP directory contract).

#### Scenario: policies/ is documented in ip/README.md
- **WHEN** `ip/README.md` is read
- **THEN** it MUST mention the `policies/` subdirectory as part of the standard IP structure
- **AND** MUST describe the policies/ subdirectory as "replacement/prefetch/write strategies for caches and similar"

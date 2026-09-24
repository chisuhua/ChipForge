# cpu-hazard-kind-enum Specification

## Purpose
Replaces `HazardPlugin::has_hazard()`'s `bool` return with `enum class HazardKind : std::uint8_t` (`NONE`/`RAW_RS1`/`RAW_RS2`/`WAW`), letting Phase 5+ OoO issue logic distinguish hazard sources (issue port 1/2/retire). ABI-compatible via implicit `bool` conversion from `NONE == 0`.
## Requirements
### Requirement: HazardKind enum discriminates hazard types

The system SHALL provide an `enum class HazardKind : std::uint8_t` with values `NONE`, `RAW_RS1`, `RAW_RS2`, `WAW` to allow Phase 5+ issue logic to distinguish hazard sources.

`HazardPlugin::has_hazard` SHALL return `HazardKind` (replacing previous `bool` return type).

The `has_hazard` method SHALL accept a `tid` parameter of type `std::uint8_t` with default value 0.

The function body SHALL detect:

- `HazardKind::RAW_RS1`: `dec.reads_rs1 && has_raw(dec.rs1_idx, tid)`
- `HazardKind::RAW_RS2`: `dec.reads_rs2 && has_raw(dec.rs2_idx, tid)`
- `HazardKind::WAW`: `dec.writes_rd && has_waw(dec.rd_idx, tid)`
- `HazardKind::NONE`: no hazard detected

#### Scenario: No hazard returns NONE

- **WHEN** a test calls `has_hazard(dec)` with empty DecodePayload
- **THEN** the system SHALL return `HazardKind::NONE`

#### Scenario: RAW on RS1 detected

- **WHEN** register 5 is marked in-flight and `dec.reads_rs1=true, dec.rs1_idx=5`
- **THEN** `has_hazard(dec)` SHALL return `HazardKind::RAW_RS1`

#### Scenario: RAW on RS2 detected

- **WHEN** register 7 is marked in-flight and `dec.reads_rs2=true, dec.rs2_idx=7`
- **THEN** `has_hazard(dec)` SHALL return `HazardKind::RAW_RS2`

#### Scenario: WAW detected

- **WHEN** register 10 is marked in-flight and `dec.writes_rd=true, dec.rd_idx=10`
- **THEN** `has_hazard(dec)` SHALL return `HazardKind::WAW`

#### Scenario: In-tree caller updated

- **WHEN** the `HazardPlugin::build()` callback calls `has_hazard`
- **THEN** the caller SHALL check `h != HazardKind::NONE` and dispatch to the appropriate stall logic

## ADDED Requirements

### Requirement: CPU plugins are templated on N_REGS and N_THREADS

The system SHALL provide template parameters `N_REGS` and `N_THREADS` on `RegFilePlugin<T>`, `HazardPlugin<T>`, and `BranchPredictorPlugin<T>` to support Phase 5+ superscalar and SMT configurations.

Template parameters SHALL have the following defaults that preserve the current ABI:

- `RegFilePlugin<typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>`
- `HazardPlugin<typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>`
- `BranchPredictorPlugin<typename T, std::size_t BTB_SIZE = 16, std::size_t BIMODAL_SZ = 16, std::size_t GSHARE_SZ = 16, std::uint8_t GHR_BITS = 8, std::size_t N_THREADS = 1>`

The system SHALL enforce compile-time constraints via `static_assert`:

- `T` MUST be unsigned (`std::is_unsigned<T>::value`)
- `N_REGS` MUST be in range [1, 128]
- `N_THREADS` MUST be in range [1, 4]
- `N_REGS` MUST be a power of 2 OR equal to 1

Storage layout SHALL be `std::array<array_store<T, N_REGS>, N_THREADS>` for RegFile and `std::array<std::array<bool, N_REGS>, N_THREADS>` for Hazard scoreboard.

All public methods that access per-thread state SHALL accept a `tid` parameter of type `std::uint8_t` with default value 0.

#### Scenario: Default template parameters preserve ABI

- **WHEN** existing code declares `RegFilePlugin<uint32_t>` without explicit template arguments
- **THEN** the system SHALL instantiate `RegFilePlugin<uint32_t, 32, 1>` and the behavior SHALL be identical to the pre-M4G version

#### Scenario: Custom N_REGS compiles

- **WHEN** code declares `RegFilePlugin<uint32_t, 8>` with non-default N_REGS
- **THEN** the system SHALL compile successfully and the instance SHALL have 8 registers

#### Scenario: Multi-thread configuration compiles

- **WHEN** code declares `HazardPlugin<uint32_t, 32, 2>` with N_THREADS=2
- **THEN** the system SHALL compile successfully and the scoreboard SHALL have per-thread isolation

#### Scenario: Invalid N_REGS rejected at compile time

- **WHEN** code declares `RegFilePlugin<uint32_t, 200>` with N_REGS > 128
- **THEN** the compiler SHALL emit a `static_assert` failure error

#### Scenario: Invalid N_THREADS rejected at compile time

- **WHEN** code declares `HazardPlugin<uint32_t, 32, 8>` with N_THREADS > 4
- **THEN** the compiler SHALL emit a `static_assert` failure error

#### Scenario: Per-thread register isolation

- **WHEN** a test writes register 5 with value 100 on thread 0 and value 200 on thread 1
- **THEN** `read_reg(5, 0)` SHALL return 100 and `read_reg(5, 1)` SHALL return 200

#### Scenario: Per-thread scoreboard isolation

- **WHEN** a test marks register 3 in-flight on thread 0
- **THEN** `has_raw(3, 0)` SHALL return true and `has_raw(3, 1)` SHALL return false

### Requirement: BranchPredictor tid parameter threading

The system SHALL provide a `tid` parameter on `BranchPredictorPlugin::predict` and `BranchPredictorPlugin::update` methods to support per-thread global history register (GHR).

The `tid` parameter SHALL be of type `std::uint8_t` with default value 0.

The `global_history_` field SHALL be `std::array<std::uint8_t, N_THREADS>` indexed by `tid`.

Existing test cases that call `predict(pc)` and `update(pc, taken, target)` without explicit `tid` SHALL continue to pass without modification (default `tid=0`).

#### Scenario: Default tid preserves behavior

- **WHEN** a test calls `bp.predict(0x1000)` without explicit tid
- **THEN** the system SHALL use tid=0 and behavior SHALL be identical to pre-M4G

#### Scenario: Per-thread GHR isolation

- **WHEN** a test updates the predictor with `(pc=0x2000, taken=true, target=0x3000, tid=1)`
- **THEN** `global_history_[0]` SHALL remain unchanged and `global_history_[1]` SHALL be updated
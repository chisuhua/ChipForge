## ADDED Requirements

### Requirement: ReplacementPolicy Abstract Interface Exists

The project MUST provide a `ReplacementPolicy` abstract base class at `ip/cache/policies/replacement_policy.h`. The interface MUST allow cache implementations to delegate victim selection, access tracking, and insertion tracking to a pluggable strategy.

The interface MUST include:
- `virtual ~ReplacementPolicy() = default`
- `virtual void on_access(uint32_t set, uint32_t way) = 0`
- `virtual uint32_t select_victim(uint32_t set) = 0`
- `virtual void on_insert(uint32_t set, uint32_t way) = 0`
- `virtual std::string name() const = 0`
- `static std::unique_ptr<ReplacementPolicy> create(const std::string& name)`

#### Scenario: Default factory returns null for unknown name
- **WHEN** `ReplacementPolicy::create("UnknownPolicy")` is called
- **THEN** the function MUST throw `std::runtime_error` with message containing the unknown policy name

#### Scenario: Factory returns LRUPolicy for "LRU"
- **WHEN** `ReplacementPolicy::create("LRU")` is called
- **THEN** the function MUST return a non-null `unique_ptr` to a `LRUPolicy` instance
- **AND** the returned pointer MUST pass dynamic_cast to `LRUPolicy*`

#### Scenario: Factory returns NoReplacementPolicy for "None"
- **WHEN** `ReplacementPolicy::create("None")` is called
- **THEN** the function MUST return a non-null `unique_ptr` to a `NoReplacementPolicy` instance

### Requirement: NoReplacementPolicy Maintains Phase 1.3 Behavior

The `NoReplacementPolicy` implementation MUST preserve the exact behavior of the Phase 1.3 hard-coded L1CachePlugin implementation (direct-mapped, no replacement logic).

#### Scenario: NoReplacementPolicy select_victim returns 0
- **WHEN** `NoReplacementPolicy::select_victim(set)` is called for any set
- **THEN** the function MUST return 0 (the only way in 1-way direct-mapped cache)
- **AND** MUST NOT mutate any state

#### Scenario: NoReplacementPolicy access/insert are no-op
- **WHEN** `NoReplacementPolicy::on_access(set, way)` or `on_insert(set, way)` is called
- **THEN** the function MUST be a no-op (no observable state change)

### Requirement: LRUPolicy Provides LRU Tracking

The `LRUPolicy` implementation MUST provide least-recently-used tracking via access counters, suitable for 1-way L1 (simplified) and extensible to N-way L2.

#### Scenario: LRUPolicy select_victim returns 0 for 1-way
- **WHEN** `LRUPolicy::select_victim(set)` is called in a 1-way cache
- **THEN** the function MUST return 0 (simplified for 1-way)
- **AND** MUST update internal LRU timestamp

#### Scenario: LRUPolicy name returns "LRU"
- **WHEN** `LRUPolicy::name()` is called
- **THEN** the function MUST return the string `"LRU"`

### Requirement: L1CachePlugin Accepts Policy Injection

`L1CachePlugin` constructor MUST accept an optional `unique_ptr<ReplacementPolicy>` parameter, defaulting to `nullptr` (which internally creates a `NoReplacementPolicy`).

#### Scenario: L1CachePlugin default construction preserves behavior
- **WHEN** `L1CachePlugin` is constructed with no policy argument
- **THEN** internally a `NoReplacementPolicy` MUST be created
- **AND** the L1CachePlugin's `lookup` and `refill` stage behavior MUST be identical to Phase 1.3 (existing 4 tests PASS)

#### Scenario: L1CachePlugin with LRU policy
- **WHEN** `L1CachePlugin` is constructed with `make_unique<LRUPolicy>()`
- **THEN** internally the LRU policy MUST be invoked during `lookup` stage's `on_access` call
- **AND** the policy's `name()` MUST be `"LRU"`

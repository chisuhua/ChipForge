# payload-store-get-miss-semantics Specification

## Purpose
TBD - created by archiving change fix-5stage-mux-segv-elaboration. Update Purpose after archive.
## Requirements
### Requirement: PayloadStore::get() MUST differentiate const-read and write accessors

The `cf::plugin::PayloadStore` class in `include/cf/plugin/payload.h` MUST expose
two accessor overloads with distinct miss-handling semantics:

- `const T& get(const Payload<T>& key) const` — when the cell is missing (i.e.
  `cells_.find(&key) == cells_.end()`), MUST throw `std::runtime_error` with
  message `"PayloadStore cell missing: " + key.name() + " (CH_MEM elaboration:
  populate before at_stage accesses)"`. MUST NOT mutate the store on miss.
- `T& get(const Payload<T>& key)` (non-const) — when the cell is missing, MUST
  retain the existing `emplace-on-miss` semantics that default-constructs `T{}`
  and inserts it, so that the syntactic convenience `n(key) = value` (which
  invokes the non-const overload) continues to work without explicit
  pre-population.

In TLM mode (when `CF_PLUGIN_USE_CH_MEM` is NOT defined), the original
`emplace-on-miss` behavior MUST be preserved on BOTH overloads for backward
compatibility.

This change closes the SIGSEGV at `ch::Simulator::initialize()` →
`muximpl::create_instruction` line 13 (`false_value()->id()` dereferences
nullptr), caused by `const T& get()` emplace-on-miss producing a
`ch_uint<N>{}` with `impl_ = nullptr` that propagated through stage linking
and was later dereferenced by mux downstream.

#### Scenario: CH_MEM const-get on missing cell throws

- **WHEN** a test calls `store.get<ch_uint<32>>(some_payload_key)` in CH_MEM
  mode (`CF_PLUGIN_USE_CH_MEM` defined) and `some_payload_key` has never been
  written to `store` via the non-const overload or `put`
- **THEN** the call MUST throw `std::runtime_error` with a message containing
  both `"PayloadStore cell missing"` and `some_payload_key.name()`; the store
  MUST NOT be mutated (no cell inserted on miss)

#### Scenario: CH_MEM write-then-const-read returns the written value

- **WHEN** a test executes `store.get<ch_uint<32>>(key)` (non-const) followed
  by `store.get<ch_uint<32>>(key)` (const) in CH_MEM mode
- **THEN** the first call MUST emplace `T{}` (zero/null default) without
  throwing; the second call MUST return the emplaced cell without throwing;
  the returned reference MUST equal the emplaced value

#### Scenario: TLM mode preserves emplace-on-miss on both overloads

- **WHEN** a test calls `store.get<ch_uint<32>>(key)` (either const or
  non-const) in TLM mode (`CF_PLUGIN_USE_CH_MEM` NOT defined) and `key` has
  never been written
- **THEN** the call MUST NOT throw; it MUST emplace `T{}` and return a
  reference to the emplaced cell, preserving the historical Phase 1 ABI

#### Scenario: Type-mismatch error message format unchanged

- **WHEN** a test writes `store.put<ch_uint<32>>(key)` then reads
  `store.get<ch_uint<64>>(key)` (const, wrong type)
- **THEN** the call MUST throw `std::runtime_error` containing the substring
  `"Payload type mismatch: " + key.name()` (unchanged from pre-fix
  behavior); only the miss-handling semantics changed, not the
  type-mismatch diagnostic

### Requirement: Check 8 statically verifies PayloadStore fail-fast is wired

The `tools/check_plugin_portability.sh` script MUST include a static Check 8
that grep-asserts `include/cf/plugin/payload.h` (under the
`CF_PLUGIN_USE_CH_MEM` `#ifdef` branch) contains the literal string
`"PayloadStore cell missing"` adjacent to a `throw std::runtime_error(` call.
The grep MUST specifically target the unique message substring (not a generic
`throw std::runtime_error` pattern) to avoid false positives from the four
existing `Payload type mismatch` throw sites.

The script header MUST report "8/8" instead of "7/7" total checks, and
`ADR-040 v2.0 移植性检查全部通过` MUST print `8/8` on success.

#### Scenario: check_plugin_portability.sh reports 8/8 when fail-fast is wired

- **WHEN** `bash tools/check_plugin_portability.sh` is executed and
  `include/cf/plugin/payload.h` CH_MEM path contains
  `throw std::runtime_error("PayloadStore cell missing: ...`
- **THEN** the script MUST print `[PASS] PayloadStore CH_MEM get-miss
  fail-fast 已就位` and exit 0; the summary line MUST show
  `(8/8)` not `(7/7)`

#### Scenario: check_plugin_portability.sh fails if fail-fast is removed

- **WHEN** the `throw std::runtime_error("PayloadStore cell missing: ...")`
  literal is removed from `include/cf/plugin/payload.h` CH_MEM branch
- **THEN** running `bash tools/check_plugin_portability.sh` MUST exit
  non-zero with a `[FAIL]` diagnostic naming the missing pattern; this guards
  against silent regression of the SEGV fix


## ADDED Requirements

### Requirement: cpu_factory_chmem::build_cpu() MUST pre-populate stage cells before pb->build()

`cf::cpu::cpu_factory_chmem::build_cpu()` MUST register an EARLY-phase `at_stage` closure on the `PipeBuilder` BEFORE calling `pb->build()`. The EARLY closure MUST populate all Payload cells that downstream NORMAL closures read from each stage.

Each stage MUST be populated with concrete `ch_uint<W>(ch_literal<0, W>{})`
(or `ch_bool(false)` for boolean cells) literal values — NEVER with `T{}`
default-constructed values that produce null `impl_` handles. The populated
cells MUST cover, at minimum:

- **fetch**: `keys<T, XLEN>::PC` (ch_uint<XLEN>, `ch_literal<0, XLEN>{}`)
- **decode**: `keys<T, XLEN>::RS1`, `RS2`, `RD_DATA` (ch_uint<XLEN>); if
  `DECODE` exists in the keys bundle, also `DECODE` (default-constructed
  `RiscvDetail` struct with zeroed fields)
- **execute**: `keys<T, XLEN>::RESULT` (ch_uint<XLEN>)
- **memory**: `keys<T, XLEN>::MEM_ADDR`, `MEM_DATA` (ch_uint<XLEN>)
- **writeback**: `keys<T, XLEN>::RD_DATA` (ch_uint<XLEN>) — re-populated if
  writeback reads it before any decode-stage producer has run
- **branch** (if registered): `keys<T, XLEN>::BRANCH_TARGET` or analogous
  branch outcome cell (ch_uint<XLEN>)

The EARLY closure MUST use `n->operator()(KeyType::X) = ch_uint<W>(...)` (or
`put` for non-`operator()` paths), which invokes the non-const emplace
overload — this is the only write path permitted to mutate the store with
zero-valued cells.

The existing NORMAL-phase closures that read cells (branch_chmem /
hazard_chmem / reg_file_chmem / int_alu_chmem) MUST NOT be modified to add
defensive miss-handling — they rely on the EARLY-phase guarantee.

This requirement closes the second root cause of the 5-stage Simulator
SEGV: even with PayloadStore const-get fail-fast (spec
`payload-store-get-miss-semantics`), if no producer has run yet, the
read-miss would throw at every stage's first cycle. Pre-populating with
zero literals guarantees that reads succeed with deterministic default
values, matching the Phase 1 TLM-mode semantics where T{} defaults were
silently emplaced.

#### Scenario: build_cpu() registers EARLY closure before pb->build()

- **WHEN** `cpu_factory_chmem::build_cpu(pb, configs...)` is invoked and
  `pb->build()` has NOT yet been called
- **THEN** an EARLY-phase `at_stage` closure (or `at_phase(PHASE::EARLY)`)
  MUST be registered on `pb`; that closure MUST populate the cells listed
  in the requirement body with `ch_literal<0, W>{}` (or `false` for bool);
  only AFTER this registration may `pb->build()` be called

#### Scenario: Pre-populated cells use real literals, not null handles

- **WHEN** the EARLY closure runs and reads back any cell it just populated
  via `const T& get(key)`
- **THEN** the returned value MUST equal the literal that was written (e.g.
  `ch_uint<32>(0)` for `RS1`); the underlying `impl_` MUST be non-null
  (verifiable via `ch->is_implicit()` or by `ch->id() != 0` — non-null
  id-allocated handles); the read MUST NOT throw

#### Scenario: Downstream CH_MEM plugin reads succeed without miss-exception

- **WHEN** `branch_chmem`, `hazard_chmem`, `reg_file_chmem`, or
  `int_alu_chmem` reads `n->operator()(KeyType::X)` (non-const write
  accessor) inside their at_stage NORMAL closures, BEFORE any other
  plugin has written to that key for this cycle
- **THEN** the read MUST succeed (returning `T{}` via emplace-on-miss if
  no value is present, or the EARLY-populated literal if present); the
  read MUST NOT throw a `PayloadStore cell missing` exception — the EARLY
  pre-population guarantees all known cells exist

#### Scenario: PoC 5-stage simulator tick completes 10 cycles without SEGV

- **WHEN** `m4_poc_5stage_simulator_tick` runs `pb->build()` + 10
  `Simulator::tick()` calls
- **THEN** all 10 ticks MUST complete without SEGV; the previous failure
  mode (`muximpl::create_instruction` line 13 dereferences nullptr) MUST
  not occur, because all upstream `ch_uint` cells fed into the stage mux
  now have non-null `impl_` from EARLY pre-population

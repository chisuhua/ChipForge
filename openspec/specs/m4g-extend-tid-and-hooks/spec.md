# m4g-extend-tid-and-hooks Specification

## Purpose
Second-stage M4G forward-compat lock: adds `PluginBase::set_tid(uint8_t)` virtual (default no-op) with overrides in `RegFile`/`Hazard`/`BranchPredictor` plugins reading `this->tid_` from per-`at_stage` closures; documents `commit_storages` + `CtrlLink::flush_when` as the OoO commit and mispredict-squash primitives; reserves `COMMIT` as the 6th pipeline stage name. Hard prerequisite for M5-DSE 2-wide superscalar; saves ~200 LOC of Phase 5+ plugin-signature rewrites.
## Requirements
### Requirement: Plugin tid plumbing via set_tid virtual method

`PluginBase` SHALL provide a `virtual void set_tid(uint8_t tid) = 0;` (or `{}` no-op default) method that the CPU factory calls before each `run()` invocation to dispatch per-thread work.

Each plugin that consumes `tid` (RegFilePlugin, HazardPlugin, BranchPredictorPlugin) SHALL override `set_tid` to store the tid in a private member, and the plugin's `at_stage` closure SHALL read `this->tid_` instead of hardcoding `tid=0`.

`cpu_factory::build_cpu` SHALL iterate `for (tid = 0; tid < config.n_threads; ++tid) { plugin.set_tid(tid); plugin.run(); }` to dispatch per-thread cycles.

When `config.n_threads == 1` (default), the behavior SHALL be byte-identical to current code.

#### Scenario: set_tid is called with tid=0 in single-thread mode
- **WHEN** `cpu_factory::build_cpu` runs with `config.n_threads == 1`
- **THEN** the factory SHALL call `plugin.set_tid(0)` once before each `plugin.run()` call
- **AND** the behavior SHALL be identical to M4G baseline (no observable change)

#### Scenario: set_tid is called for each thread in multi-thread mode
- **WHEN** `cpu_factory::build_cpu` runs with `config.n_threads == 4`
- **THEN** the factory SHALL call `plugin.set_tid(0)`, `plugin.set_tid(1)`, `plugin.set_tid(2)`, `plugin.set_tid(3)` in order
- **AND** each call SHALL be followed by a `plugin.run()` invocation

### Requirement: commit_hook documented as OoO commit primitive

`include/cf/plugin/pipe_builder.h` SHALL contain a comment block at the `commit_hooks_` declaration (lines 104-138) documenting:
- `register_commit_hook(fn)` is the OoO commit primitive
- `commit_storages()` (called at end of `run()`) is the per-cycle commit sink
- `CtrlLink::flush_when(condition)` (in `include/cf/plugin/ctrl_link.h`) is the mispredict-squash primitive that pairs with commit_hook for branch recovery

The comment SHALL reference `dse_architecture_v2_design_research.md` §3 E.1 (ROB design) as the Phase 5+ design that consumes these primitives.

#### Scenario: Documentation comment exists at the correct location
- **WHEN** a Phase 5 designer opens `include/cf/plugin/pipe_builder.h` and navigates to the `commit_hooks_` declaration
- **THEN** a comment block SHALL be present explaining OoO/Superscalar usage
- **AND** the comment SHALL reference `CtrlLink::flush_when` as the partner primitive

### Requirement: COMMIT stage naming convention

`ip/cpu/docs/multi_isa_architecture.md §2.4` SHALL contain a 6th stage row named `COMMIT` in the 5-stage pipeline table, with the note: "in-order 隐含; OoO 显式阶段, 用 commit_hook 原语".

The COMMIT stage SHALL be reserved as the OoO commit point. In-order pipelines (current code) SHALL NOT register a `at_stage("commit", ...)` callback.

#### Scenario: COMMIT stage row exists in 5-stage table
- **WHEN** a reader opens `ip/cpu/docs/multi_isa_architecture.md` and navigates to §2.4
- **THEN** the 5-stage table SHALL contain 6 rows (the 5 original + 1 COMMIT row)
- **AND** the COMMIT row SHALL be marked `(无)` in the implementation column (no current code)

#### Scenario: No at_stage("commit", ...) callback registered in current code
- **WHEN** grep searches for `at_stage(\"commit` in `ip/cpu/` source files
- **THEN** zero matches SHALL be returned
- **AND** the stage name is reserved for Phase 5+ use


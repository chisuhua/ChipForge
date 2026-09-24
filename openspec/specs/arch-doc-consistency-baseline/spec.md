# arch-doc-consistency-baseline Specification

## Purpose
Defines the invariant that project artifacts (docs, JSON configs, source) MUST NOT reference non-existent "ghost" classes (e.g., `RiscvIssTlm`, `L1CacheTlm`), enforced by `tools/verify_no_ghost_refs.sh`. Companion rules align docs with the actual Plugin-style architecture (D4) and the Bridge/Plugin tick delegation pattern.
## Requirements
### Requirement: No Ghost Class References in Project Artifacts

The project MUST NOT contain references to classes that do not exist in the codebase. This applies to all documentation (`.md`), JSON configurations (`.json`), and source files (`.h`, `.cpp`).

The following class names are explicitly tracked as ghost references and MUST NOT appear in any project artifact (outside of `openspec/changes/` archive and `CHANGELOG.md` historical entries):

- `RiscvIssTlm`
- `L1CacheTlm`
- `BusMatrixTlm`
- `DramTlm`
- `UartTlm`
- `ClintTlm`
- `PlicTlm`
- `RiscvCoreRtl`

#### Scenario: CI grep check passes
- **WHEN** `tools/verify_no_ghost_refs.sh` is executed on the repository
- **THEN** the script MUST return exit code 0 with output `0 ghost class references found`
- **AND** MUST search recursively in `docs/`, `ip/`, `soc/`, `bundles/`, `include/`, `src/`, `tests/` for the 8 ghost class names
- **AND** MUST exclude `openspec/changes/`, `CHANGELOG.md`, and `.omo/drafts/` (archived decision memory) from the search

#### Scenario: New ghost reference is introduced
- **WHEN** a developer adds a new file containing any of the 8 ghost class names
- **THEN** `tools/verify_no_ghost_refs.sh` MUST return exit code 1
- **AND** MUST print the file path and line number of each violation

### Requirement: Removed Ghost Configuration Artifacts

The project MUST NOT contain the following ghost configuration artifacts:

- `soc/riscv_virt.json` (referenced 7 non-existent classes and unused `impl_mode` field)
- `ip/cpu/cpu_factory.cpp` (12-line stub with no corresponding `.cpp` implementation; only `.h` declaration remains)

#### Scenario: Ghost files do not exist after change
- **WHEN** the change is applied
- **THEN** `test -f /workspace/project/ChipForge/soc/riscv_virt.json` MUST return non-zero
- **AND** `test -f /workspace/project/ChipForge/ip/cpu/cpu_factory.cpp` MUST return non-zero
- **AND** `ip/cpu/cpu_factory.h` MUST still exist (declaration retained for future implementation)

### Requirement: Documentation Reflects Plugin-style Naming Convention

All documentation that previously referenced ghost classes MUST be updated to use the actual existing class names or explicit "not yet implemented" markers.

#### Scenario: Overview SoC section references real configuration
- **WHEN** `docs/architecture/overview.md §"SoC 层是 IP 组合器"` is read
- **THEN** it MUST reference `soc/l1_cache_minimal.json` (a real working example) instead of the non-existent `soc/RiscvVirtSoC.h/cpp`
- **AND** MUST NOT show the 8 ghost class names in any code examples

#### Scenario: ISA-agnostic section marked as future work
- **WHEN** `docs/architecture/overview.md §"ch_stream 接口即 ISA 无关层"` is read
- **THEN** it MUST be replaced with a "Phase 1.4+ Future Work" placeholder section
- **AND** MUST contain the marker `> ⚠️ 推迟到 Phase 1.4+ 实际有第 2 个 CPU IP 时再验证`

### Requirement: Bridge Tick Pattern ADR

The project MUST have an Architecture Decision Record (ADR-041) that explicitly clarifies the boundary between Plugin-style business logic (no `tick()`) and Bridge-style adapter layers (allowed `tick()`).

#### Scenario: ADR-041 exists and is linked
- **WHEN** `docs/architecture/adr/ADR-041-bridge-tick-pattern.md` is read
- **THEN** it MUST contain sections: Context, Decision, Consequences
- **AND** MUST explicitly reference ADR-025 (Plugin base class no tick) and ADR-037 (Plugin as paradigm)
- **AND** MUST specify that `L1CacheTLMBridge` and `L1CacheTLMBridgeAdapter` are the canonical examples of the Bridge pattern

#### Scenario: ADR main index includes ADR-041
- **WHEN** `docs/architecture/adr.md §2.1` is read
- **THEN** it MUST include `ADR-041` in the decision list with status `Accepted`
- **AND** MUST include a link to `docs/architecture/adr/ADR-041-bridge-tick-pattern.md`

### Requirement: code-framework-mapping.md Drift Section Updated

The drift section in `docs/architecture/code-framework-mapping.md §7.4` MUST be updated to reflect the post-change state.

#### Scenario: Drift items moved to resolved section
- **WHEN** the change is complete
- **THEN** `code-framework-mapping.md §7.4` MUST NOT list the 8 ghost class references as "待修复" anymore
- **AND** MUST add a new `§7.6 已修复 (2026-06-17)` section listing the 8 items moved to "已修复"
- **AND** MUST update `§7.5 建议的修正优先级` to reflect that items 1-4 are now complete


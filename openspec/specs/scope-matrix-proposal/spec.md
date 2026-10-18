# scope-matrix-proposal Specification

## Purpose
TBD - created by archiving change scope-matrix-proposal. Update Purpose after archive.
## Requirements
### Requirement: agents-md-scope-matrix-section
The system SHALL provide a `## Build Agent Work Scope Matrix (POC)` (H2) section in `AGENTS.md` containing:
- Scope matrix introduction paragraph (scope | role matrix orthogonal relationship | advisory not enforcement)
- State source schema (`.rddf/state/scope.json`, parallel to `role.json`)
- 5 categories: framework | ip | soc | governance | tools
- Open report format extension: `【角色: <X> | scope: <Y>】` (project-level; distinct from global builder `change:` tail)
- Optional Mermaid visualization diagram
- Title-level: `##` not `#` per repo + global convention (project AGENTS.md already has 1 H1, global opencode AGENTS.md uses `##` for analogous section)

#### Scenario: scope matrix section presence
- **WHEN** `grep -n 'Build Agent Work Scope Matrix' AGENTS.md` runs
- **THEN** ≥1 match (H2 header + section content).

#### Scenario: orthogonal role relationship stated
- **WHEN** reader inspects Work Scope Matrix §0/§1 (intro)
- **THEN** they see explicit statement: "scope 与 role matrix 正交独立; role 控制行为, scope 控制焦点" + "本项目级 AGENTS.md 与全局 `~/.config/opencode/AGENTS.md` 的 role matrix 共存 (per-host scope)".

### Requirement: scope-state-file-initial
The system SHALL initialize `.rddf/state/scope.json` with schema v1:
```json
{
  "schema_version": "scope-v1",
  "current_scope": {"category": "governance", "target": "governance", "sub_target": null},
  "updated_at": "<ISO timestamp>",
  "updated_by": "user",
  "session_id": "<session ID>",
  "history": [{"from": null, "to": "governance/governance", "at": "<ISO>", "by": "user", "trigger": "scope-matrix-proposal initial"}]
}
```

#### Scenario: state file exists with v1 schema
- **WHEN** `cat .rddf/state/scope.json` runs
- **THEN** schema_version: scope-v1 visible; current_scope.category: governance.

### Requirement: scope-targets-enumeration
The system SHALL list 5 scope categories with example targets in the Work Scope Matrix section:

| Category | Targets (示例) |
|----------|-----|
| framework | framework |
| ip | cpu, cache, mmu, memory, peripheral, interconnect, tilecopy, tilecore |
| soc | soc/cpu_l1_mmu_demo, soc/mmu_minimal, soc/l1_cache_minimal |
| governance | governance |
| tools | cpu_sim, verilator_runner |

> **Naming note**: targets mirror the actual physical directory names
> (`soc/cpu_l1_mmu_demo.json` not `soc/cpu-l1-mmu-demo`). The `/` separator
> in proposals/improvements reflects the 5-category namespace, but actual
> filesystem paths use `_` (underscore) in JSON filenames and `/` for
> directories.

#### Scenario: target list discoverable
- **WHEN** reader inspects scope matrix §4 table
- **THEN** they can identify any of the 5 categories and at least 2 example targets per category.

### Requirement: open-report-format-extension
The system SHALL specify the build agent open report format extension:
```
【角色: <X> | scope: <Y>】
  <human-readable line>
```

> **Format note**: Unlike the global `~/.config/opencode/AGENTS.md` builder
> open report format `【角色: builder | change: <名或无>】` (uses `change:`
> tail), this project's scope-matrix uses `scope:` to convey work-focus.
> The two formats coexist (global vs project-level); both are `advisory`
> overlays that do not block agent actions. Format choice is mutually
> exclusive by host file (project AGENTS.md vs global AGENTS.md).

#### Scenario: open report adherence
- **WHEN** build agent outputs opening report in project AGENTS.md context
- **THEN** uses `【角色: <X> | scope: <Y>】` template format.

### Requirement: scope-not-enforcement-clause
The system SHALL include an explicit "scope 是 advisory, 不阻断操作" clause in the Work Scope Matrix section.

#### Scenario: advisory wording visible
- **WHEN** `grep -n "advisory\|不阻断\|scope.*不.*禁" AGENTS.md` runs
- **THEN** at least 1 match in Work Scope Matrix section.

### Requirement: four-architecture-gates-baseline-maintained
The system SHALL pass all 4 architecture gates unchanged after scope-matrix-proposal change:
- check_plugin_portability 12/12 PASS
- verify_plugin_decision 8/8 PASS
- verify_adr 0 fail
- doc_checker 0 new fail

#### Scenario: gates green
- **WHEN** 4 gate scripts run post-archive
- **THEN** all gates return 0 (PASS for checks, 0 new fail for doc_checker).

### Requirement: five-family-regression-zero
The system SHALL pass all 5 family tests (and full TLM regression) with 0 new regression vs baseline `[cache] 25/25 + [cpu] baseline + [cpu-l1-mmu-demo] 6/6 + [soc] 20/20 + [mmu] 59/59 + [riscv-tests] 48/48`.

> **Baseline measurement**: locked in `tasks.md` Phase 0 via
> `bash tools/run_chipforge_tests.sh` before any code change. Expected
> total: `458/459 PASS` (1 pre-existing fail known untouched).

#### Scenario: regression 0
- **WHEN** `bash tools/run_chipforge_tests.sh` runs post-archive
- **THEN** same `458/459 PASS` baseline unchanged (no new fail).

### Requirement: archive-and-sync-complete
The system SHALL complete openspec archive workflow:
- openspec archive `scope-matrix-proposal` succeeds
- `openspec/specs/scope-matrix-proposal/spec.md` D3 spec-delta generated
- `.rddf/roadmap/strategy.md §7` auto-derived via `sync_strategy_status.sh`

#### Scenario: archive atom
- **WHEN** openspec archive executes
- **THEN** change moved to openspec/changes/archive/ + spec auto-updated.

### Requirement: role-json-untouched
The system SHALL NOT modify `.rddf/state/role.json` schema or content (parallel coexistence with scope.json).

#### Scenario: role.json unchanged
- **WHEN** `git diff .rddf/state/role.json` runs after scope-matrix-proposal archive
- **THEN** empty diff (zero modification).

### Requirement: scope-json-tracked-despite-gitignore
The system SHALL commit `.rddf/state/scope.json` to git despite `.gitignore:57 state/` rule (which would normally ignore `state/` paths). Resolution method: `git add -f .rddf/state/scope.json` (force-add despite gitignore pattern).

> **Reason**: per critic 2026-10-19 BLOCKER #3, `.rddf/state/` is gitignored
> for runtime state files (events.jsonl, sessions.json) which are
> machine-generated and would pollute git history. But scope.json is
> hand-maintained SSoT (like role.json) and must be tracked. Use
> `git add -f` to bypass the .gitignore rule for scope.json specifically.

#### Scenario: scope.json tracked
- **WHEN** `git ls-files .rddf/state/scope.json` runs
- **THEN** 1 hit (file tracked).

#### Scenario: role.json still untracked
- **WHEN** `git ls-files .rddf/state/role.json` runs
- **THEN** empty (still untracked, parallel coexistence preserved).

### Requirement: scope-target-mismatch-caveat
The system SHALL state in the scope-matrix section that scope targets mirror physical directory names (`soc/cpu_l1_mmu_demo.json` with `_`, not `-`). This avoids the critic SHOULD_FIX #6 about JSON filename vs proposed hyphenated names.

#### Scenario: target naming caveat visible
- **WHEN** reader inspects Work Scope Matrix §4 targets table
- **THEN** note about `_` vs `-` filename convention visible.


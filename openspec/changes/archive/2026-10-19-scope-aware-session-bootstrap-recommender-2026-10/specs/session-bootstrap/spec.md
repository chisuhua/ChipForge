# scope-aware-session-bootstrap-recommender Specification

## Purpose

Document the contract for `scope-aware-session-bootstrap-recommender`: a session bootstrap helper (governance meta) that:

1. **Fixes 6 active drift** in `.opencode/skills/v0100-bootstrap/SKILL.md` + `tools/bootstrap/sections/gates.sh` + `AGENTS.md` (each drift commands LLM to `cat` non-existent or already-archived files, costing ~5-10 minutes per session to recover)
2. **Adds a `scope_snapshot` custom_section** to the rdd-workflow thin-wrapper bootstrap pipeline, providing 4 deterministic raw-state outputs (scope.json / roadmap directories / git log / 5-gate exit codes) for LLM to consume instead of re-discovering the world

0 business code change. Pure governance helper enhancement. 0 rdd-workflow upstream change (thin-wrapper philosophy preserved).

## ADDED Requirements

### Requirement: skill-md-drift-fixes
The system SHALL fix 6 documented drift in 3 files (per critic session `ses_eac338bdeffeb0qMMKHeNXGaY6` review):

1. `.opencode/skills/v0100-bootstrap/SKILL.md:78` — `objective-v0110-launch.md` → `objective-soc-cpu-v0110-launch.md` (was renamed by commit `766c15b` 2026-10-19)
2. `.opencode/skills/v0100-bootstrap/SKILL.md:79` — `openspec/changes/mfc-cpu-pipeline-multi-cycle-fsm/tasks.md` → archive path or rephrase (was archived 2026-10-16)
3. `.opencode/skills/v0100-bootstrap/SKILL.md:82-85` — Step 5 add `tools/doc_link_check.sh` and `tools/roadmap-r9-check.sh` (5th gate was added by commit `115c7dd` 2026-10-19)
4. `.opencode/skills/v0100-bootstrap/SKILL.md:70` — "不要做" section mfc reference refresh
5. `tools/bootstrap/sections/gates.sh:56` — drop hard-coded `(target: 40/40)` label (zero-drift fix; S3 alternative to CSV parsing)
6. `AGENTS.md:99` — refresh "锁定不 archive" → "已 archive" (mfc was archived 2026-10-16)

#### Scenario: drift-fixes verifiable by grep
- **WHEN** `grep -rn "objective-v0110-launch\.md\|mfc-cpu-pipeline-multi-cycle-fsm/tasks\.md\|target: 40/40\|锁定不 archive" --include="*.md" .rddf/ .opencode/ tools/bootstrap/sections/ AGENTS.md` runs
- **THEN** the output is empty (0 hits, all 6 drift fixed)

#### Scenario: drift-fixes preserve parent change content
- **WHEN** `awk '/^---$/{c++; next} c==1' .opencode/skills/v0100-bootstrap/SKILL.md` runs
- **THEN** the frontmatter still contains all original keys (only inline path references change)

### Requirement: scope-snapshot-section-script
The system SHALL add `tools/bootstrap/sections/scope_snapshot.sh` as a new `custom_section` hook in the rdd-workflow thin-wrapper bootstrap pipeline (`rdd-session-bootstrap`). The script MUST output exactly 4 deterministic raw-state items and exit 0 unconditionally.

#### Scenario: 4 deterministic output items
- **WHEN** `bash tools/bootstrap/sections/scope_snapshot.sh` runs from repo root
- **THEN** output contains exactly 4 sections in order:
  1. `## scope.json` + `cat .rddf/state/scope.json` content (or `(missing), default=governance/governance` if absent)
  2. `## roadmap` + `ls .rddf/roadmap/{phases,objectives,features}/` listing
  3. `## git_log` + `git log -5 --oneline` output
  4. `## gates` + 5 lines mapping gate name → exit code (`verify_adr: 0`, `verify_plugin_decision: 0`, `check_plugin_portability: 0`, `doc_link_check: 0`, `roadmap-r9-check: 0`)

#### Scenario: scope-snapshot-script exits 0 unconditionally
- **WHEN** `bash tools/bootstrap/sections/scope_snapshot.sh; echo "rc=$?"` runs
- **THEN** `rc` equals 0 (zero exit code, gate failures encoded as text not exit code per B2 fix)

#### Scenario: scope-snapshot-script cwd-independent
- **WHEN** `cd /tmp/opencode && bash /workspace/project/ChipForge/tools/bootstrap/sections/scope_snapshot.sh` runs
- **THEN** the script still emits 4 sections (S7 fix: `cd "$(git rev-parse --show-toplevel)"` as first line)

#### Scenario: scope-snapshot-script degrade on missing scope.json
- **WHEN** `mv .rddf/state/scope.json /tmp/opencode/scope.bak && bash tools/bootstrap/sections/scope_snapshot.sh; mv /tmp/opencode/scope.bak .rddf/state/scope.json` runs
- **THEN** section 1 outputs `(missing), default=governance/governance` and exit code is still 0

### Requirement: profile-toml-custom-section-registration
The system SHALL register the `scope_snapshot` script in `.rddf/skill-profiles/session-bootstrap.toml` as a `custom_section` AND insert `"scope_snapshot"` into the `order` array.

#### Scenario: profile-toml-contains-scope-snapshot
- **WHEN** `grep -c "scope_snapshot" .rddf/skill-profiles/session-bootstrap.toml` runs
- **THEN** the count is ≥ 2 (one in `order`, one in `custom_sections`)

#### Scenario: profile-toml-renders-scope-snapshot-in-bootstrap-output
- **WHEN** `bash tools/v0100-bootstrap.sh 2>&1 | head -80` runs
- **THEN** output includes a `## scope_snapshot` section before `## hard_prerequisites` (rdd-session-bootstrap renders custom_sections at expected position)

## REMOVED Requirements

None (no existing capability is removed; this is a pure additive enhancement).

## RENAMED Requirements

None (no existing requirement is renamed).

## ADDED Requirements (continued)

### Requirement: v0100-bootstrap-skill-md-thin-wrapper-invariants
The system SHALL preserve the `v0100-bootstrap` skill thin-wrapper design philosophy after the drift fixes (see `skill-md-drift-fixes` above). Specifically: 0 new logic SHALL be added to SKILL.md body; 0 new rdd-workflow upstream change SHALL be made; line count SHALL stay ≤ 120; H2 section count SHALL stay ≤ 9.

#### Scenario: skill-md-remains-thin-wrapper
- **WHEN** `wc -l .opencode/skills/v0100-bootstrap/SKILL.md` runs
- **THEN** line count stays ≤ 120 (current 80 lines + ~30 drift fixes ≤ 120; thin-wrapper boundary preserved)

#### Scenario: skill-md-no-new-tool-section
- **WHEN** `grep -c "^## " .opencode/skills/v0100-bootstrap/SKILL.md` runs
- **THEN** H2 section count stays ≤ 9 (current 9 H2; delta after drift fix ≤ 9; no new H2 added)

## Cross-References

- **supersedes**: 6 active drift in SKILL.md / gates.sh / AGENTS.md
- **manual_deps**: [] (no blocking dependencies; coordinates with commit `9af06e8` feat scope field, `766c15b` primary scope rename, `115c7dd` R9 check gate)
- **initiative**: governance (per AGENTS.md §Build Agent Work Scope Matrix)
- **superseded by**: P1 governance change (to be drafted after this change archives)
- **upstream compatibility**: rdd-workflow `rdd-session-bootstrap` profile `order` array — verified compatible (engine `parse_profile` accepts arbitrary `custom_sections`; `order` accepts any string)

## Out of Scope (P1 governance change)

- Protocol section (oracle Q2-C hybrid: LLM behavior constraints)
- `tools/skill-drift-check.sh` (S5 complexity)
- Taxonomy mapping table (B1 full solution: scope.json 6 values ↔ primary_scope 6 values ↔ scope 5 values)
- CI integration (S6: `architecture-gates.yml` only runs 4 gates)
- Upstream rdd-workflow v0.2 K1-K3 interface pinning (S8)
- Route #4 (BTB) and #5 (立即下一步段) design defects

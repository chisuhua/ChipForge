# session-bootstrap Specification

## Purpose
TBD - created by archiving change scope-aware-session-bootstrap-recommender-2026-10. Update Purpose after archive.
## Requirements
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


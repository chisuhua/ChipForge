# dse-scope-alignment-v2-locks Specification

## Purpose
Reconciles DSE v1.0 §9 Phase A+B tasks (A.1-A.6 + B.1-B.5) with v2.0-locks §6.3 Oracle review, producing an 11-row disposition table in v2.0-locks §7 with tags (✅ Allowed / 🟡 Needs Redesign / ❌ Removed) and citations. Prevents future readers from re-litigating Oracle verdicts.
## Requirements
### Requirement: v1.0 §9 Tasks Reconciled with v2.0-locks §6.3

`dse_architecture_v2_locks.md` SHALL contain a §7 section that exhaustively maps each Phase A and Phase B task in `dse_architecture.md` v1.0 §9 to its disposition under §6.3 (Oracle 2026-06-17 review).

The §7 table SHALL contain at least 11 rows (6 from Phase A + 5 from Phase B) and each row SHALL be tagged with exactly one of: ✅ Allowed, 🟡 Needs Redesign, ❌ Removed.

Each row SHALL cite the specific §6.3 sub-section that determines its disposition (e.g., "§6.3 row 'D.7'").

#### Scenario: §7 table covers all 11 Phase A+B tasks

- **WHEN** a reader opens `ip/cpu/docs/dse_architecture_v2_locks.md` and navigates to §7
- **THEN** the table SHALL contain at least 11 rows
- **AND** each row SHALL reference a task ID from `dse_architecture.md` v1.0 §9 (A.1-A.6 for Phase A, B.1-B.5 for Phase B)
- **AND** the total ✅ count + 🟡 count + ❌ count SHALL equal 11

#### Scenario: Three §6.3-conflicted tasks are marked ❌

- **WHEN** a reader examines the §7 table for v1.0 §9 tasks A.1, B.2, and B.3
- **THEN** all three SHALL be tagged ❌
- **AND** each ❌ row SHALL cite the §6.3 entry that justifies the removal (D.7 deferral or BranchPredictorFactory deletion)

#### Scenario: §7 table cites §6.3 sub-section for each row

- **WHEN** any cell in the §7 table is examined
- **THEN** it SHALL include a textual reference to a specific §6.3 sub-section (e.g., "§6.3 row 3" or "§6.3 BranchPredictorFactory")
- **AND** the cited §6.3 entry SHALL exist in the v2.0-locks document

### Requirement: v1.0 §9 Section Headers Warn About Oracle Review

`dse_architecture.md` v1.0 §9 SHALL contain explicit warning markers at the beginning of Phase A and Phase B sections that direct readers to the v2.0-locks §7 reconciliation table.

The warning SHALL appear before the first task row in each section (Phase A and Phase B) and SHALL contain:
- The ⚠️ marker
- A reference to `dse_architecture_v2_locks.md` (anchor `#7` for the §7 section)
- A statement that direct implementation of v1.0 §9 would violate the Oracle 2026-06-17 review

The warning SHALL NOT modify or remove any task row in the v1.0 §9 table.

#### Scenario: Phase A header contains ⚠️ warning

- **WHEN** a reader opens `dse_architecture.md` and scrolls to the "Phase A" subsection under §9
- **THEN** the first content lines (before the A.1 table row) SHALL contain the ⚠️ marker
- **AND** SHALL contain a markdown link or path reference to `dse_architecture_v2_locks.md#7`
- **AND** SHALL NOT modify the existing A.1-A.6 task rows in the Phase A table

#### Scenario: Phase B header contains ⚠️ warning

- **WHEN** a reader opens `dse_architecture.md` and scrolls to the "Phase B" subsection under §9
- **THEN** the first content lines (before the B.1 table row) SHALL contain the ⚠️ marker
- **AND** SHALL contain a markdown link or path reference to `dse_architecture_v2_locks.md#7`
- **AND** SHALL NOT modify the existing B.1-B.5 task rows in the Phase B table

### Requirement: status.md M4-DSE Section References Reconciliation Prerequisite

`ip/cpu/docs/status.md` §4.1 ("M4-DSE 子阶段") SHALL contain a prerequisite note that establishes the v2.0-locks §7 reconciliation as a hard prerequisite for any M4.12-M4.19 task implementation.

The note SHALL appear in the §4.1 section body (not as the §4.1 title) and SHALL contain:
- A reference to `dse_architecture_v2_locks.md` (anchor `#7`)
- A statement that M4.12-M4.19 task statuses remain 🔵 Pending and are NOT to be marked complete without first reconciling against the §7 table

The M4.12-M4.19 task rows in §4.1 SHALL remain unchanged in this change (no status renumbering, no deletion).

#### Scenario: status.md §4.1 contains reconciliation prerequisite

- **WHEN** a reader opens `ip/cpu/docs/status.md` and navigates to §4.1
- **THEN** the section body SHALL contain a reference to `dse_architecture_v2_locks.md#7` (via markdown link or path)
- **AND** the existing 8 M4.12-M4.19 task rows SHALL remain present
- **AND** their status markers SHALL remain 🔵 (or any variant indicating "Pending")
- **AND** no task ID SHALL be renumbered or deleted by this change


## ADDED Requirements

### Requirement: IP Catalog Status Table Includes Implementation Scope Column

The IP catalog at `docs/architecture/ip-catalog.md` MUST include an "Implementation Scope" (实现范围) column for every IP entry.

The Implementation Scope MUST state one of:
- **具体实现描述**: For implemented IPs, list the specific features/classes/parameters actually present in code (e.g., "L1 unified direct-mapped 32KB, Phase 1.3 L1CachePlugin + L1CacheTLMBridgeAdapter")
- **未实现声明**: For unimplemented IPs, state "0 LOC, 仅 README" or "骨架 (skeleton only)"

#### Scenario: L1Cache row shows actual implementation
- **WHEN** `docs/architecture/ip-catalog.md` is read
- **THEN** the L1Cache row's "实现范围" column MUST contain "L1 unified direct-mapped 32KB (L1CachePlugin), Phase 1.3 Plugin-style + L1CacheTLMBridgeAdapter cpptlm 桥接"
- **AND** MUST NOT contain "Phase 1.2 L1D" (the previous incorrect status)

#### Scenario: Unimplemented IP rows show zero LOC
- **WHEN** any zero-code IP (memory/interconnect/peripheral/tilecore/tilecopy) row is read
- **THEN** the "实现范围" column MUST contain "0 LOC, 仅 README + .gitkeep"
- **AND** the "实施预计" column MUST contain a roadmap phase link (e.g., "Phase 2+, see soc/cpu/docs/roadmap/phase-2-baremetal.md")

### Requirement: IP Catalog Includes Roadmap Phase Link Column

The IP catalog MUST include a "Implementation Roadmap" (实施预计) column. Every IP MUST have a roadmap link, even if the link is "Phase 6+ TBD".

#### Scenario: All IP rows have non-empty roadmap links
- **WHEN** the table is rendered
- **THEN** every row's "实施预计" column MUST be non-empty
- **AND** the link MUST point to an existing file under `docs/roadmap/phases/` (or "Phase 6+ TBD" if no phase defined)

### Requirement: IP README Template Includes Implementation Status Section

A reusable template MUST be created at `docs/templates/IP_README_TEMPLATE.md`. Every IP README MUST conform to this template.

#### Scenario: Template includes mandatory sections
- **WHEN** `docs/templates/IP_README_TEMPLATE.md` is read
- **THEN** it MUST include sections: "实现状态", "实施预计", "依赖", "测试", "已知限制"
- **AND** the "实现状态" section MUST use one of the 4 status markers (🟢/🟡/🔴/⚫)
- **AND** the "实施预计" section MUST include a roadmap link

#### Scenario: Existing IP READMEs conform to template
- **WHEN** each `ip/<name>/README.md` is read
- **THEN** it MUST contain the "实现状态" section with a status marker
- **AND** MUST contain the "实施预计" section with a roadmap link

## REMOVED Requirements

### Requirement: L1Cache marked as Phase 1.2 L1D
**Reason**: The L1Cache IP is unified (L1I/L1D split does not exist) and was implemented in Phase 1.3, not Phase 1.2. Marking it as "Phase 1.2 L1D" is a factual error.
**Migration**: Use the new status "🟡 TLM 实现中 (Phase 1.3, L1 unified direct-mapped)" per the "IP Catalog Status Table" requirement above.

## ADDED Requirements

### Requirement: IP README Template File Exists

The project MUST provide a reusable template at `docs/templates/IP_README_TEMPLATE.md` for IP authors to follow when creating or updating `ip/<name>/README.md`.

#### Scenario: Template file exists
- **WHEN** `docs/templates/IP_README_TEMPLATE.md` is read
- **THEN** the file MUST exist and be non-empty
- **AND** MUST contain at least these sections: 实现状态, 实施预计, 依赖, 测试, 已知限制

#### Scenario: Template references IP catalog status convention
- **WHEN** the "实现状态" section of the template is read
- **THEN** it MUST reference the 4 status markers (🟢 稳定 / 🟡 设计中/实现中 / 🔴 规划中 / ⚫ 弃用) defined in `docs/architecture/ip-catalog.md §状态图例`
- **AND** MUST require one of these 4 markers to be present in any conforming IP README

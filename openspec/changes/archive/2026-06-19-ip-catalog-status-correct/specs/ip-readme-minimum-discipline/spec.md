# ip-readme-minimum-discipline

## Purpose

定义 IP README 内容的最小集要求：所有 IP README MUST 含"实现状态"与"实施预计"段；零代码 IP 通过 v0.0.5 已建的 `STATUS.md` 满足（不重复要求 README 含同样段）。

## Requirements

### ADDED Requirements

#### REQ-IRMD-001: 现有 IP README 必备段

现有 IP（cpu/cache）的 `ip/<name>/README.md` MUST 包含"实现状态"与"实施预计"段。描述 MUST 与 `docs/architecture/ip-catalog.md` 对应行一致（特别是 L1Cache 的 `Phase 1.3, 16KB` 描述）。

#### REQ-IRMD-002: 零代码 IP 通过 STATUS.md 满足

零代码 IP（memory/interconnect/peripheral/tilecore/tilecopy）MUST 有 `ip/<name>/STATUS.md`（v0.0.5 已建）含"实施预计"段。`ip/<name>/README.md` 不强制要求重复该段，但若含 MUST 与 STATUS.md 一致。

#### REQ-IRMD-003: 不新建 IP_README_TEMPLATE.md

本 change 不创建 `docs/templates/IP_README_TEMPLATE.md`。`docs/templates/IP_STATUS_TEMPLATE.md`（v0.0.5 已建）覆盖零代码 IP 场景；若未来需要统一所有 IP README 结构，应作为独立 change 提案。
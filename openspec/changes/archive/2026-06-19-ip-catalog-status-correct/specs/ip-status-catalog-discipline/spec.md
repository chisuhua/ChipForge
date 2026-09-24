# ip-status-catalog-discipline

## Purpose

建立 IP 状态目录（`docs/architecture/ip-catalog.md`）的强约束：状态标记必须附"实现范围"说明，零代码 IP 必须有 roadmap 链接，禁止"标稳定"或"模糊描述"。

## Requirements

### ADDED Requirements

#### REQ-ISCD-001: 实现范围列

`docs/architecture/ip-catalog.md` IP 索引表 SHALL 增加"实现范围"列，明确每个 IP 已实现什么（具体到 LOC/功能）。8 个 IP 全部填写，禁止空列或占位符（如"TBD"）。

#### REQ-ISCD-002: 实施预计列

`docs/architecture/ip-catalog.md` IP 索引表 SHALL 增加"实施预计"列。零代码 IP 必须指向对应 roadmap phase + `ip/<name>/STATUS.md` 路径（v0.0.5 已建）。

#### REQ-ISCD-003: L1Cache 状态描述

L1Cache 行的状态字段 SHALL 为 `"🟡 TLM 实现中 (Phase 1.3, L1 unified direct-mapped 16KB, L1I/L1D/L2 未拆分)"`。**禁止**使用 "Phase 1.2 L1D"、"Phase 1.3 32KB" 等错误描述。

#### REQ-ISCD-004: 容量数据一致性

`docs/architecture/ip-catalog.md`、`ip/cache/README.md §4 §5`、`ip/cache/tlm/L1CachePlugin.h` 3 处描述 L1Cache 容量时 MUST 一致使用 `16KB`（256 sets × 1 way × 64B = 16384 字节）。`32KB` 仅在描述 8-way 设计目标时出现，且 MUST 标注"非当前实现"。

#### REQ-ISCD-005: 零代码 IP 引用 v0.0.5 STATUS.md

5 个零代码 IP（memory/interconnect/peripheral/tilecore/tilecopy）的"实现范围"列 SHALL 引用 v0.0.5 已建的 `ip/<name>/STATUS.md`（PLANNED 或 INITIAL DESIGN 变体），避免在 catalog 中重复声明其内容。
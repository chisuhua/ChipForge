## Why

`docs/architecture/ip-catalog.md` 当前 IP 状态表与实际代码状态有 3 处不一致：

1. **L1Cache 状态描述错误**: 当前标"🟡 TLM 实现中 (Phase 1.2 L1D)"，但实际是 **Phase 1.3 完成的 L1 unified direct-mapped 16KB**（256 sets × 1 way × 64B = 16384 字节，**不是 32KB**；原 32KB 描述是 8-way 误算，L1CachePlugin.h L18 注释已修正）
2. **表格缺少"实现范围"列**：新人无法分辨"已实现什么"和"规划中什么"
3. **零代码 IP 没有 roadmap 链接**：5 个零代码 IP（memory/interconnect/peripheral/tilecore/tilecopy）的实施预计路径未在表中显式标注

**与 v0.0.5 (empty-directory-cleanup) 的关系**: v0.0.5 已创建 `ip/<name>/STATUS.md`（5 个零代码 IP）+ `docs/templates/IP_STATUS_TEMPLATE.md`，并在 `ip/README.md` 顶部加"STATUS 约定"段。本 change **不重复** v0.0.5 工作，而是补全 ip-catalog.md 的"实现范围"和"实施预计"列，与 STATUS.md 内容保持一致。

**本 change 目标**: 修正 IP 状态表与现实对齐，并建立"状态变更必同步"的可执行机制。

## What Changes

- **重写** `docs/architecture/ip-catalog.md` IP 索引表（增加"实现范围"列 + "实施预计"列）
- **修正** L1Cache 状态：`"🟡 TLM 实现中 (Phase 1.3, L1 unified direct-mapped 16KB, L1I/L1D/L2 未拆分)"`（**注意：16KB 不是 32KB**）
- **重写** `ip/cache/README.md §5` 配置参数表格 `capacity_kb` 默认值 32 → 16
- **同步** `ip/cache/README.md §4` "可插拔策略"表格的容量描述：32KB → 16KB
- **更新** `CHANGELOG.md` 记录 v0.0.3 "ip-catalog-status-correct" + L1Cache 容量注释修正
- **不修改** `ip/README.md`（v0.0.5 STATUS 约定段已含 cache 状态描述）
- **不创建** `docs/templates/IP_README_TEMPLATE.md`（v0.0.5 `IP_STATUS_TEMPLATE.md` 已覆盖零代码 IP）

## Capabilities

### New Capabilities

- `ip-status-catalog-discipline`: 建立 IP 状态目录的强约束：状态标记必须附"实现范围"说明，零代码 IP 必须有 roadmap 链接，禁止"标稳定"或"模糊描述"。
- `ip-readme-minimum-discipline`: 现有 IP README（cache/cpu）必须包含"实现范围"+"实施预计"段；零代码 IP 通过 `ip/<name>/STATUS.md` 满足（v0.0.5 已建）。

### Modified Capabilities

（无现有 spec，无 modified capabilities）

## Impact

- **影响文件**:
  - 修改: `docs/architecture/ip-catalog.md` (~30 行变更)
  - 修改: `ip/cache/README.md §4 §5`（容量描述 32KB→16KB）
  - 修改: `ip/cache/tlm/L1CachePlugin.h` L18 注释（已在 cache-policy-foundation §0.1 落地）
  - 更新: `CHANGELOG.md`
- **依赖与时序**:
  - 本 change 依赖 CHANGE-001 (doc-code-realignment, v0.0.2) 完成 → ✅ 已 archived
  - 本 change 在 v0.0.5 (empty-directory-cleanup) **之后**实施 → v0.0.5 已建 STATUS.md，本 change 引用其内容
  - 本 change 是 CHANGE-003 (cache-policy-foundation) 的前置依赖（cache 状态描述稳定后，policy change 才能修改 ip-cache README）
- **CI 影响**: 增强 `tools/verify_no_ghost_refs.sh` 检查 ip-catalog.md 是否包含"实现范围"列
- **无代码运行时影响**
- **breaking 变更**: 无
- **与 archive/2026-06-18-ip-catalog-status-correct-v1-original 关系**: v1-original 4 项问题已在本版本修复（详见 design §"v1 已知问题修复"）

## Alternatives Considered

### Alternative A: 推迟到 v0.0.6+，等 v0.0.5 STATUS.md 完全落地后再写本 change

**放弃理由**: v0.0.5 STATUS.md 已建且内容稳定，本 change 只需引用其内容。延迟无收益。**不放弃**：本 change 立即落地。
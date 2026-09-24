## Why

`docs/architecture/ip-catalog.md` 当前 IP 状态表与实际代码状态有 3 处不一致：(1) L1Cache 标为 "🟡 TLM 实现中 (Phase 1.2 L1D)"，但实际是 unified direct-mapped 32KB L1 (L1I/L1D/L2 完全未拆分)；(2) 表格缺少"实现范围"列，新人无法分辨"已实现什么"和"规划中什么"；(3) 5 个零代码 IP（memory/interconnect/peripheral/tilecore/tilecopy）没有指向 roadmap 的"实施预计"列。本 change 目标是修正 IP 状态表与现实对齐，并建立"状态变更必同步"的可执行机制。

## What Changes

- **重写** `docs/architecture/ip-catalog.md` IP 索引表（增加"实现范围"列 + "实施预计"列）
- **修正** L1Cache 状态：标"🟡 TLM 实现中 (Phase 1.3, L1 unified direct-mapped 32KB, L1I/L1D/L2 未拆分)"
- **重写** `ip/README.md` 状态约定（增加"实现范围强制要求" + "实施预计必填"）
- **更新** `CHANGELOG.md` 记录 v0.0.3 "ip-catalog-status-correct"

## Capabilities

### New Capabilities

- `ip-status-catalog-discipline`: 建立 IP 状态目录的强约束：状态标记必须附"实现范围"说明，零代码 IP 必须有 roadmap 链接，禁止"标稳定"或"模糊描述"。
- `ip-readme-template`: 创建 `ip/README.md` 模板，要求所有 IP README 包含"实现状态"+"实施预计"段。

### Modified Capabilities

（无现有 spec，无 modified capabilities）

## Impact

- **影响文件**:
  - 修改: `docs/architecture/ip-catalog.md` (~90 行)
  - 修改: `ip/README.md` (标准约定文档)
  - 新增: `docs/templates/IP_README_TEMPLATE.md` (可复用模板)
  - 更新: `CHANGELOG.md`
- **依赖**:
  - 本 change 依赖 CHANGE-001 完成（`riscv_virt.json` 已删除，否则 ip-catalog 引用会保留）
  - 被 CHANGE-003 (cache-policy) 引用（cache 状态修正）
- **CI 影响**: 增强 `tools/verify_no_ghost_refs.sh` 检查 ip-catalog 是否包含"实现范围"列
- **无代码运行时影响**
- **breaking 变更**: 无

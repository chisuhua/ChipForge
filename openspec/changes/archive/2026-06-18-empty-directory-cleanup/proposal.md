## Why

5 个 IP（memory/interconnect/peripheral/tilecore/tilecopy）的 `tlm/`/`rtl/`/`test/`/`configs/` 子目录全部为空（仅 `.gitkeep`），加 `src/cf_plugin/tests/` 空目录 + `ip/cpu/test/` 仅 README 违反"测试在 IP 内"约定——总共 20+ 个空目录噪音污染项目结构。本 change 目标：清理空目录、加 `STATUS.md` 标记、迁移 IP 测试到正确位置，恢复项目结构信号。

## What Changes

- **删除** 5 个 IP 的 18 个 `tlm/rtl/test/configs` 空目录（保留 IP 根 + README；**注意：tilecore/tilecopy 各只有 3 个，无 `test/`**，所以总数 18 而非 20）
- **新增** 5 个 IP 根 `STATUS.md`（memory/interconnect/peripheral 用 PLANNED 变体；tilecore/tilecopy 用 INITIAL DESIGN 变体，承认已有 docs/architecture.md）
- **删除** `src/cf_plugin/tests/` 空目录（测试在 `tests/framework/`，CMakeLists.txt 注释同步）
- **删除** `ip/cpu/test/` 空目录（README 内容移到 `ip/cpu/docs/verification.md`）
- **处理** `ip/cpu/docs/riscv/` 隐藏空目录（删除或保留——见 task 6）
- **更新** `CHANGELOG.md` 记录 v0.0.5

## Capabilities

### New Capabilities

- `ip-status-markdown-contract`: 5 个零代码 IP 必须有 `STATUS.md` 在 IP 根，标明当前状态（0 LOC/PLANNED）+ 实施预计 + roadmap 链接。
- `test-location-discipline`: IP 测试统一在 `tests/<ip-name>//` 而非 `ip/<ip-name>/test/`，违反约定的 IP README 须指明原因。

### Modified Capabilities

（无现有 spec，无 modified capabilities）

## Impact

- **影响文件**:
  - 删除: 5 IP × 18 个 `tlm/rtl/test/configs` 空目录（+18 个 .gitkeep）
  - 新增: 5 个 `STATUS.md` (memory/interconnect/peripheral/tilecore/tilecopy)
  - 新增: `docs/templates/IP_STATUS_TEMPLATE.md`（PLANNED + INITIAL DESIGN 2 个变体）
  - 删除: `src/cf_plugin/tests/` 目录
  - 修改: `src/cf_plugin/CMakeLists.txt` L30 后插入注释（指明测试在 `tests/framework/`）
  - 删除: `ip/cpu/test/` 目录（README 移到 `ip/cpu/docs/verification.md`）
  - 处理: `ip/cpu/docs/riscv/` 隐藏空目录
  - 修改: `ip/cpu/README.md` 加 "测试位置" 段指向 `tests/cpu/`
  - 修改: `ip/README.md` 加 "STATUS 约定" 段
  - 修改: `CHANGELOG.md`
- **依赖**:
  - 本 change 必须在 **CHANGE-001 (doc-code-realignment) 之后**（避免 ip-catalog 表格引用旧子目录）
  - 本 change 与 **CHANGE-002 (ip-catalog-status-correct) 必须并行** —— 因为：
    - CHANGE-002 修正 `ip/README.md` L11 表格中 cache 状态 "Phase 1.2 L1D" → "Phase 1.3 L1 unified"
    - 本 change 清理 `ip/<5 zero-code IP>/` 子目录结构
    - 两者都触及 `ip/README.md`，**必须按以下顺序**：本 change 先在 `ip/README.md` 顶部加 "STATUS 约定" 段，CHANGE-002 再修正表格条目
  - 不依赖 CHANGE-003（独立）
  - **不冲突**: 本 change 与 CHANGE-003 都创建 `ip/cache/policies/`，但**本 change 不删 `ip/cache/`，CHANGE-003 单独处理 cache 子目录**（见设计 Decision 2）
- **CI 影响**: 减少 `find` 噪音；可能需要更新 `tools/verify_*.sh` 中的路径排除
- **runtime 影响**: 无
- **breaking 变更**: 无（删除的是空目录，无代码）

## Alternatives Considered

### Alternative A: 保留空目录 + .gitkeep，仅加 STATUS.md

**放弃理由**: 18 个空目录对 git status / IDE 索引噪音严重；删除是无风险操作（无代码，git history 保留可恢复）

### Alternative B: 迁移 IP 测试到 ip/<name>/test/

**放弃理由**: 测试基础设施（CMakeLists.txt + ctest 集成）已在 `tests/` 根；迁移会触发 CMake 路径变更，影响其他 change 范围

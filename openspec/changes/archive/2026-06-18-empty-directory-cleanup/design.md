## Context

本 change 仅清理项目结构噪音，无运行时影响。

**当前状态**:
- 5 个 IP（memory/interconnect/peripheral/tilecore/tilecopy）的 `tlm/`/`rtl/`/`test/`/`configs/` 中共 **18 个空目录**（实测：memory 4 + interconnect 4 + peripheral 4 + tilecore 3 + tilecopy 3；tilecore/tilecopy 没有 `test/` 子目录）
- 7 个 IP 中只有 cpu 和 cache 有实际代码（cpu 在 `arch/`/`plugins/`/`core/`，违反 `ip/<name>/tlm/` 约定；cache 在 `ip/cache/tlm/`，符合约定）
- `src/cf_plugin/tests/` 是空目录（测试在 `tests/framework/`）
- `ip/cpu/test/` 是 README-only 空目录（测试在 `tests/cpu/`）

**约束**:
- 不破坏已工作的代码路径
- 5 个零代码 IP 的 `tlm/`/`rtl/`/`test/`/`configs/` 未来实施时会重新创建（git 历史保留）
- 现有 CMake 路径不变

**利益相关者**:
- 新人: 项目结构更清晰
- 维护者: `find` 噪音减少
- CI 脚本: 路径排除列表简化

## Goals / Non-Goals

**Goals:**
- 删除 18 个空 IP 子目录 + 18 个 `.gitkeep`（**注意：不是 20 个**——tilecore/tilecopy 没有 `test/` 子目录，5 IP 实际只有 18 个 `tlm/rtl/test/configs` 空目录）
- 5 个零代码 IP 根加 `STATUS.md`（PLANNED/INITIAL DESIGN 变体 + 实施预计 + roadmap 链接）
- 删除 `src/cf_plugin/tests/` + `ip/cpu/test/` 2 个空目录
- 同步更新 `src/cf_plugin/CMakeLists.txt` 注释 + `ip/cpu/README.md` 测试位置段

**Non-Goals:**
- **不**实施任何 IP（推迟到 roadmap 对应 phase）
- **不**迁移 `tests/cpu/` 到 `ip/cpu/test/`（避免 CMake 路径变更）
- **不**改 `tilecore`/`tilecopy` 的 `.test/` 隐藏目录（不属于本次范围）
- **不**改 `ip/cache/policies/`（本 change 与 CHANGE-003 并行，无冲突）

## Decisions

### Decision 1: 删除 vs 保留空子目录

**选择**: **删除 20 个空 IP 子目录**

**理由**:
- 子目录在 git 历史中保留（可恢复）
- 现状下 `git status` / IDE 索引 / `find` 搜索都会噪音
- 未来 IP 实施时，子目录自然创建（`mkdir` 是实施第一步）
- 维护约定 > 当前噪音

**替代方案**:
- ❌ 保留 + 加 STATUS.md 标记: 治标不治本，git 噪音仍存在
- ❌ 改用单一 .gitkeep + .keep 文件: 增加复杂度

### Decision 2: STATUS.md 内容

**选择**: **3 段简洁内容 + 可选 "现有资产" 段**

主模板（零代码 IP 使用）：

```markdown
# STATUS: PLANNED (0 LOC)

This IP is **planned but not yet implemented**. No source code exists.

## Implementation Roadmap
- 实施预计: Phase 2+ (见 docs/roadmap/phases/phase-X.md)
- 依赖: (从 ip/README.md 标准结构)
- 状态: 🔴 规划中

## 已知限制
- 所有子目录 (`tlm/`/`rtl/`/`test/`/`configs/`) 待实施时创建
```

**tilecore / tilecopy 扩展模板**（有设计文档但无源码）：

```markdown
# STATUS: INITIAL DESIGN (0 LOC src, design docs exist)

This IP has **architectural documentation** but no source code yet.

## Existing Assets
- 设计文档: `docs/architecture.md` (27.5 KB / 5.8 KB, 微架构 + 接口)
- 政策框架: `policies/` (空目录, 待实施)

## Implementation Roadmap
- 实施预计: Phase 5+ (见 soc/cpu/docs/roadmap/phase-5-rtl.md)
- 依赖: tilecore 依赖 tilecopy; 两者都依赖 interconnect
- 状态: 🟡 初始设计

## 已知限制
- 所有子目录 (`tlm/`/`rtl/`/`test/`/`configs/`) 待实施时创建
- `.test/` 隐藏目录保留 (历史遗留, 不影响 git status)
```

**理由**:
- 主模板 3 段覆盖状态/roadmap/限制
- tilecore/tilecopy 扩展模板承认"设计先于实现"的现实
- 统一文件结构（都用 `STATUS.md` 在 IP 根），便于未来扫描
- **不超 30 行**（主模板）或 40 行（扩展模板）

**替代方案**:
- ❌ 用 `ip/<name>/README.md` 顶部 banner 代替: 与现有 ip-catalog 重复
- ❌ 详细设计文档: 不在本 change 范围
- ❌ 仅用 PLANNED 状态（忽略设计文档存在）: 误导（把"有 27KB 架构文档"标"0 资产"）

### Decision 3: cpu 测试位置变更策略

**选择**: **删除 `ip/cpu/test/` + `ip/cpu/README.md` 加测试位置说明**

**理由**:
- 所有 CPU 测试在 `tests/cpu/`，迁回 `ip/cpu/test/` 需 CMake 路径变更
- 简单方式：删除空目录 + README 显式说明测试在项目根
- 未来可单独 change 处理迁移

**替代方案**:
- ❌ 迁移所有 CPU 测试: 风险高，超本 change 范围

## Risks / Trade-offs

**[Risk 1]** 删除空目录时误删含隐藏文件 → **Mitigation**: 删除前 `ls -la` 检查；仅删除确认空的
**[Risk 2]** `STATUS.md` 路径在 PR 评审中产生争议 → **Mitigation**: PR description 显式说明
**[Risk 3]** 删除后未来 IP 实施者不知道原约定 → **Mitigation**: `ip/README.md` 标准结构段保留，明确子目录契约
**[Risk 4]** 误删 `tilecore/.test/` 隐藏目录 → **Mitigation**: 本 change **不删** `.test/`，仅删 `tlm/`/`rtl/`/`test/`/`configs/`

## Migration Plan

无运行时迁移。**步骤**:
1. 删除 20 个空 IP 子目录（commit 1，5 IP 一次性 batch）
2. 创建 5 个 STATUS.md（commit 2）
3. 删除 `src/cf_plugin/tests/` + 更新 CMakeLists.txt 注释（commit 3）
4. 删除 `ip/cpu/test/` + 更新 `ip/cpu/README.md`（commit 4）
5. 更新 CHANGELOG.md（commit 5）

**回滚策略**: 5 个 commit 可逐个 revert；空目录删除是可逆操作（git history 保留）

## Open Questions

1. 5 个 STATUS.md 模板还是各自独立？ → **本 change 决定**: 统一模板（保持一致性）
2. 是否在 `ip/README.md` 顶部加 "STATUS 约定" 段？ → **本 change 决定**: **是**（指向模板路径）
3. `tilecore/.test/` 隐藏目录是否也清理？ → **本 change 决定**: **不**（属于其他 change 范围）

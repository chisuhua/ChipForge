## Context

本 change 仅修改 IP 状态目录的展示层与约定层，无运行时影响。

**当前状态 (2026-06-18 审计)**：
- `docs/architecture/ip-catalog.md` 表格列：IP / 状态 / 类型 / 关键参数 / 可独立使用 / 主页 — **缺"实现范围"和"实施预计"**
- L1Cache 当前描述：`🟡 TLM 实现中 (Phase 1.2 L1D)` — **错误**（实际是 Phase 1.3，且 L1Cache 是 unified 不是 L1D-only）
- 5 个零代码 IP（memory/interconnect/peripheral/tilecore/tilecopy）的"实施预计"在 `ip/<name>/STATUS.md`（v0.0.5 已建）但未在 ip-catalog.md 表格显式标注

**v0.0.5 (empty-directory-cleanup) 已落地状态**：
- 5 个 IP 已有 `STATUS.md`（memory/interconnect/peripheral 用 PLANNED；tilecore/tilecopy 用 INITIAL DESIGN）
- `docs/templates/IP_STATUS_TEMPLATE.md` 已建（PLANNED / INITIAL DESIGN / PARTIAL 3 个变体）
- `ip/README.md` 顶部已加 STATUS 约定段
- `ip/cache/test/` 等 18 个空目录已删除

**约束**：
- 状态标记（🟢/🟡/🔴）来自 `ip-catalog.md §状态图例`，不能改语义
- 表格格式必须可由 Markdown 渲染器（GitHub）正确显示
- 5 个零代码 IP 的实施预计引用 v0.0.5 STATUS.md 内容，不重复声明

**利益相关者**：
- 新人：清晰看到"IP 状态 vs 实际实现"
- IP Owner：明确每个 IP 的实施路径
- 维护者：状态变更同步机制

## v1 已知问题修复 (来自 archive/2026-06-18-ip-catalog-status-correct-v1-original)

| v1 Issue | 本 v2 修复 |
|---|---|
| Issue 3: tasks §3.1 引用已删除的 `ip/cache/test/` 等目录 | 改为引用实际存在的 `ip/<name>/README.md` + `ip/<name>/STATUS.md`（v0.0.5 约定） |
| Issue 4: CHANGE-XXX 编号引用 v0.0.5 (empty-directory-cleanup) 缺失 | 显式引用 v0.0.5 已落地内容，明确"本 change 不重复" |
| Issue 8: L1Cache 状态描述 32KB 错误 + Phase 1.2 错误 | 修正为 `Phase 1.3, L1 unified direct-mapped 16KB`（与 L1CachePlugin.h 注释同步） |
| Issue 9: 没引用 v0.0.5 创建的 STATUS.md | 实施预计列内容引用 `ip/<name>/STATUS.md`，避免重复声明 |

## Goals / Non-Goals

**Goals:**
- 表格增加"实现范围"列，明确每个 IP 已实现什么（具体到 LOC/功能）
- 表格增加"实施预计"列，零代码 IP 指向 v0.0.5 STATUS.md + roadmap phase
- 修正 L1Cache 状态描述为 `Phase 1.3, L1 unified direct-mapped 16KB, L1I/L1D/L2 未拆分`
- 5 个零代码 IP 增加 STATUS.md 链接（**不重写 STATUS.md 内容**，仅链接）

**Non-Goals:**
- **不**实现 memory/interconnect/peripheral/tilecore/tilecopy 中任何一个（推迟到对应 phase）
- **不**改 IP 目录物理结构（v0.0.5 已清理空目录）
- **不**重写 `ip/README.md`（v0.0.5 STATUS 约定段已含 7 IP 状态表）
- **不**新建 `docs/templates/IP_README_TEMPLATE.md`（v0.0.5 IP_STATUS_TEMPLATE.md 已覆盖零代码 IP 场景）

## Decisions

### Decision 1: 表格列扩展 vs 重写表格

**选择**: **扩展列**（保留原 6 列 + 新增 2 列）

**理由**:
- 原 6 列（IP/状态/类型/关键参数/可独立使用/主页）已提供分类价值
- 新增 2 列（实现范围/实施预计）补充状态详情
- 8 列在 GitHub Markdown 渲染下仍可读

**替代方案**:
- ❌ 重写为子目录 + 文件树：失去表格的视觉对比优势
- ❌ 单列"状态详情"：列内文字过长，不易扫读

### Decision 2: L1Cache 状态修正措辞

**选择**: `"🟡 TLM 实现中 (Phase 1.3, L1 unified direct-mapped 16KB, L1I/L1D/L2 未拆分)"`

**理由**:
- Phase 1.3 比 Phase 1.2 更准确（实际是 Phase 1.3 完成的 L1CacheTLMBridgeAdapter e2e）
- **"L1 unified direct-mapped 16KB"** 反映 `L1CachePlugin` 实际行为（256 sets × 1 way × 64B = 16384 字节 = 16KB）
- "L1I/L1D/L2 未拆分" 显式说明架构债务

**替代方案**:
- ❌ "🟡 TLM 实现中 (Phase 1.3, L1 unified direct-mapped 32KB, ...)"：**错误**（32KB 是 8-way 误算，已在 L1CachePlugin.h L18 注释修正）
- ❌ "🟡 TLM 实现中 (L1D only)"：错误，L1CachePlugin 是 unified，不分 L1I/L1D
- ❌ "🟡 部分实现"：模糊，违反 ip-status-catalog-discipline

### Decision 3: 零代码 IP 的"实施预计"列内容

**选择**: **直接引用 v0.0.5 STATUS.md + roadmap 路径**

| IP | 实施预计（ip-catalog 新增列） |
|----|------------------------------|
| memory | Phase 2+ (见 `ip/memory/STATUS.md` + `soc/cpu/docs/roadmap/phase-2-baremetal.md`) |
| interconnect | Phase 2+ (见 `ip/interconnect/STATUS.md` + roadmap Phase 2) |
| peripheral | Phase 3+ (见 `ip/peripheral/STATUS.md` + `soc/cpu/docs/roadmap/phase-3-rtos.md`) |
| tilecore | Phase 5+ (见 `ip/tilecore/STATUS.md` + `soc/cpu/docs/roadmap/phase-5-rtl.md`, GPU 形态) |
| tilecopy | Phase 5+ (见 `ip/tilecopy/STATUS.md` + roadmap Phase 5) |

**理由**:
- v0.0.5 STATUS.md 已存在并含"实施预计"字段，**避免重复声明**
- roadmap 已存在阶段定义，引用比自创估算准确
- 0 工期估算（避免假装可估算）

**替代方案**:
- ❌ 给出具体周数估算：风险高，会过期
- ❌ 不填"实施预计"：违反本 change 目标

### Decision 4: 不修改 `ip/README.md` 不新建 README 模板

**选择**: **本 change 仅修改 `docs/architecture/ip-catalog.md` + `ip/cache/README.md`**

**理由**:
- v0.0.5 STATUS 约定段已含 7 IP 状态表，含 L1Cache 描述
- v0.0.5 IP_STATUS_TEMPLATE.md 已覆盖零代码 IP 场景
- 本 change 职责是 catalog 表格补全，**不应重复 v0.0.5 工作**

**替代方案**:
- ❌ 重写 `ip/README.md` 顶部 STATUS 段：与 v0.0.5 重复，可能引入冲突
- ❌ 新建 IP_README_TEMPLATE.md：与 v0.0.5 IP_STATUS_TEMPLATE.md 职责重叠

## Risks / Trade-offs

**[Risk 1]** 表格 8 列在窄屏渲染下可能溢出 → **Mitigation**: GitHub Markdown 支持横向滚动，OK
**[Risk 2]** L1Cache 状态从"Phase 1.2"修正为"Phase 1.3"，可能与其他文档不一致 → **Mitigation**: 同时修正 `ip/cache/README.md §4 §5` 和 `L1CachePlugin.h L18` 注释（3 处同步）
**[Risk 3]** IP 实施预计依赖 roadmap 准确性 → **Mitigation**: roadmap 变更时同步 review ip-catalog 状态
**[Risk 4]** "实施预计"列内容与 STATUS.md 不一致 → **Mitigation**: 本 change 实施预计列内容直接引用 STATUS.md 路径，避免重复声明导致的漂移

## Migration Plan

无运行时迁移。**步骤**:
1. 备份 ip-catalog.md（commit 1）
2. 修正 L1CachePlugin.h L18 注释（32KB → 16KB）— **本 change 之前已落地**（在 cache-policy-foundation §0.1）
3. 修正 `ip/cache/README.md §4 §5`（32KB → 16KB，capacity_kb 默认 32 → 16）（commit 2）
4. 重写 `docs/architecture/ip-catalog.md` 表格（commit 3）
5. 更新 CHANGELOG.md（commit 4）
6. PR 评审

**回滚策略**: 4 个 commit 可逐个 revert。

## Open Questions

1. "实施预计"列内容是否在 ip-catalog.md 表格内联（避免跳转），还是只链接 STATUS.md？ → **本 change 决定**: **链接 + 简短摘要**（避免跳转但保持简洁）
2. tilecore/tilecopy 应该是"🟡 初始设计"还是"🔴 规划中"？ → **本 change 决定**: 维持"🟡 初始设计"（已有 README + architecture.md 27KB+5KB 设计文档，与 v0.0.5 STATUS.md INITIAL DESIGN 变体一致）
## Context

本 change 仅修改 IP 状态目录的展示层与约定层，无运行时影响。

**当前状态**：`docs/architecture/ip-catalog.md` 表格列：IP / 状态 / 类型 / 关键参数 / 可独立使用 / 主页 — 缺"实现范围"和"实施预计"。

**约束**：
- 状态标记（🟢/🟡/🔴）来自 `ip-catalog.md §状态图例`，不能改语义
- 表格格式必须可由 Markdown 渲染器（GitHub）正确显示
- 5 个零代码 IP（memory/interconnect/peripheral/tilecore/tilecopy）需要明确 roadmap 链接

**利益相关者**：
- 新人：清晰看到"IP 状态 vs 实际实现"
- IP Owner：明确每个 IP 的实施路径
- 维护者：状态变更同步机制

## Goals / Non-Goals

**Goals:**
- 表格增加"实现范围"列，明确每个 IP 已实现什么（具体到 LOC/功能）
- 表格增加"实施预计"列，零代码 IP 指向对应 roadmap phase
- 修正 L1Cache 状态描述为"unified direct-mapped 32KB, L1I/L1D/L2 未拆分"
- 5 个零代码 IP 增加 roadmap 链接
- 创建 `ip/README.md` 模板（带"实施预计"段）

**Non-Goals:**
- **不**实现 memory/interconnect/peripheral/tilecore/tilecopy 中任何一个（推迟到对应 phase）
- **不**改 IP 目录物理结构（推迟到 CHANGE-005）
- **不**改 `ip/README.md` 标准约定外的内容

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

**选择**: "🟡 TLM 实现中 (Phase 1.3, L1 unified direct-mapped 32KB, L1I/L1D/L2 未拆分)"

**理由**:
- Phase 1.3 比 Phase 1.2 更准确（实际是 Phase 1.3 完成的 L1CacheTLMBridgeAdapter e2e）
- "L1 unified direct-mapped 32KB" 反映 `L1CachePlugin` 实际行为（256 sets × 64B × 1-way = 16KB; 但参数允许调整）
- "L1I/L1D/L2 未拆分" 显式说明架构债务

**替代方案**:
- ❌ "🟡 TLM 实现中 (L1D only)"：错误，L1CachePlugin 是 unified，不分 L1I/L1D
- ❌ "🟡 部分实现"：模糊，违反 ip-status-catalog-discipline

### Decision 3: 零代码 IP 的"实施预计"列内容

**选择**: **直接引用 roadmap 路径 + phase 编号**

| IP | 实施预计 |
|----|----------|
| memory | Phase 2+ (见 `soc/cpu/docs/roadmap/phase-2-baremetal.md`) |
| interconnect | Phase 2+ (见 roadmap Phase 2) |
| peripheral | Phase 3+ (见 `soc/cpu/docs/roadmap/phase-3-rtos.md`) |
| tilecore | Phase 5+ (见 `soc/cpu/docs/roadmap/phase-5-rtl.md`, GPU 形态) |
| tilecopy | Phase 5+ (见 roadmap Phase 5) |

**理由**:
- roadmap 已存在阶段定义，引用比自创估算准确
- 0 工期估算（避免假装可估算）

**替代方案**:
- ❌ 给出具体周数估算：风险高，会过期
- ❌ 不填"实施预计"：违反本 change 目标

## Risks / Trade-offs

**[Risk 1]** 表格 8 列在窄屏渲染下可能溢出 → **Mitigation**: GitHub Markdown 支持横向滚动，OK
**[Risk 2]** L1Cache 状态从"Phase 1.2"修正为"Phase 1.3"，可能与其他文档不一致 → **Mitigation**: 同时 review `bundles/README.md` + `CHANGELOG.md`，同步修正
**[Risk 3]** IP 实施预计依赖 roadmap 准确性 → **Mitigation**: roadmap 变更时同步 review ip-catalog 状态

## Migration Plan

无运行时迁移。**步骤**:
1. 备份 ip-catalog.md
2. 重写表格（commit 1）
3. 重写 ip/README.md 模板（commit 2）
4. 更新 CHANGELOG.md（commit 3）
5. PR 评审

**回滚策略**: 3 个 commit 可逐个 revert。

## Open Questions

1. `ip/README.md` 模板是否在 `docs/templates/IP_README_TEMPLATE.md` 单独维护，还是直接在 `ip/README.md` 包含？ → **本 change 决定**: 单独维护模板（`docs/templates/IP_README_TEMPLATE.md`），便于各 IP 参考
2. tilecore/tilecopy 应该是"🟡 初始设计"还是"🔴 规划中"？ → **本 change 决定**: 维持"🟡 初始设计"（已有 README + architecture.md 27KB+5KB 设计文档）

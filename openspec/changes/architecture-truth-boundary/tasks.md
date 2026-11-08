---
change: architecture-truth-boundary
priority: P1
initiative: wave7-architecture-governance
adr:
  - ADR-095
  - ADR-083
  - ADR-040
plan: .rddf/plans/architecture-truth-boundary.md
status: in-progress
depends_on: []
---

# tasks — architecture-truth-boundary（位置调整 + 引用改写）

> ★ **AC 权威 = `proposal.md` 的 `## Acceptance Criteria` 段**（与 `rdd-verifier` 提取源一致：`rdd-verifier/SKILL.md:148`）⇒ 本文件 = 执行清单 + 勾选态（编号与 AC 一一对应）。
> ★ 迁移动手：`tools/migrate_arch_docs.py`（两阶段：先记录旧绝对目标 → git mv → 按新基准重算相对路径）。

## 1. 批次方案（★ 实测修正：弱连通分量，不是 SCC）

> ★ ★ **原 proposal 的 7 批划分不成立**：判据应是**弱连通分量**（任何 A→B（两者都移动）MUST 同批，
> 否则中间态必断链）。跨目录环把 `mixed/` 的 5 份与 `archive/` 的 `performance-guide` 连成一个分量。

| 批 | 文件 | 目标 | commit |
|---|---|---|---|
| 1 | plugin-framework · declarative-hybrid-framework · code-framework-mapping · tech-selection · testing-and-dse · performance-guide | mixed/ ×5 · archive/ ×1 | `00e239a` |
| 2 | tlm-coverage-parity-ledger | archive/ | `2c649d7` |
| 3 | tlm-retirement-b1-capacity-plan | archive/ | `b6be804` |
| 4 | m4-dse-cpufactory-real-pr-description | archive/ | `3a2ee8d` |
| 5 | ip-catalog | ip/ | `cac54e6` |

## 2. AC 执行记录

- [x] **AC-101** 建 `docs/architecture/README.md`（目录导航 · 顶层清单由实测派生 · 不复制 adr.md 注册表 · 4 道门禁表）⇒ `c12679f`
- [x] **AC-102** 批量移动 10 份（5 批 · git mv 保留历史）⇒ 顶层 **18 → 9**（含新 README）· mixed/ **5** · archive/ **4** · `ip/ip-catalog.md` 存在
- [ ] **AC-103** ★ **引用基线清单**（★ 原要求「每批移动**之前**」导出）
      ★ ★ **流程偏差（如实登记）**：迁移直接执行，**未在每批前单独导出基线清单**。
      ★ **等价证据**：① 迁移前全仓已 0 broken（`doc_link_check` 基线）；② **每批后**均跑 `doc_link_check` ⇒ 0 broken（见各批 commit message）；
      ③ 迁移后 0 broken ⇒ ★ 「漏改一处」不可能存在（否则必报断链）。
      ★ ⇒ 语义目的已达成，但**流程未按 AC 字面执行** ⇒ 交用户裁决是否接受该等价形式。
- [x] **AC-104** 改写全部引用（仓外 10 处 + adr/ 2 处 + adr.md 7 处 + 顶层 1 处 + 各批内部链接）⇒ `doc_link_check` 0 broken
- [x] **AC-105** `adr.md` 三处订正：源文档字段标「非权威 · 历史派生源」· 「提炼」→「历史派生」· 维护规则 5 权威倒置修正（★ 明确「权威 = `adr/` 自身」）
- [x] **AC-106** 根级文档同步：`AGENTS.md` 1 处（链接 + 反引号文字同步）；★ `.cursorrules` **实测 0 处命中** ⇒ 无需改（原 AC 假设其有引用 ⇒ 实测为空任务）
- [x] **AC-107** `ip-catalog` ↔ `ip/README` 双向链 + 单一维护点（★ ip/README 移除 STATUS 副本，改指针）⇒ `fc7f017`
- [x] **AC-108** `arch-docs.json` 目录级表达（arch_paths 16 → 3 · exclude 4 · 死条目 0 · 权限语义 8 样本实测全对）⇒ `28b01db`
- [x] **AC-116** 批批可绿：5 批均按弱连通分量划分 · 每批一个 commit · 每批 `doc_link_check` 0 broken
- [x] **AC-117** 76 处文档内部相对链接：由 `migrate_arch_docs.py` 的「文件自身移动 ⇒ 按新基准重算」覆盖（批 1 改写 72 处 · 批 4 改写 6 处 · 批 5 改写 18 处深度重算）
- [x] **AC-118** `ip-catalog` owner 交接：移出架构文档区 ⇒ 归 IP owner；维护责任已在 `ip/ip-catalog.md` 的「单一维护点声明」+ 维护约定中登记

## 3. 遗留清扫

| 检查项 | 结果 |
|---|---|
| 本 change 范围内 TODO / 半截实现 / 临时日志 | 无 |
| 被 skip 的测试 / 未替换的 mock / 临时 fallback | 无 |
| 工作区临时文件 | 无 |
| ★ 迁移工具 `tools/migrate_arch_docs.py` 是否保留 | ★ **保留**（供 `architecture-truth-boundary-enforcement` 之后的新增文档下沉场景复用；★ 也可移除 —— 见交付说明） |
| ★ 流程偏差 | ★ **AC-103 基线未按「每批前」执行** ⇒ 已如实登记，交用户裁决 |

## 4. 阶段边界

| 项 | 本 change |
|---|---|
| 位置 + 引用改写 | ✔ |
| 顶层净化（AC-109c） | ✘ ⇒ 姊妹 change |
| 4 道门禁落地（AC-110~113） | ✘ ⇒ 姊妹 change |
| 回滚验证（AC-114） | ✘ ⇒ 姊妹 change |

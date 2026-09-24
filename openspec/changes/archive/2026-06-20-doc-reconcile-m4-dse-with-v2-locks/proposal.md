## Why

`ip/cpu/docs/dse_architecture.md` v1.0 §9 (Phase A+B, 11 任务) 是 M4-DSE 的事实任务清单，但 Oracle 2026-06-17 评审通过 `dse_architecture_v2_locks.md` 后，**v1.0 表格未同步更新**。两文档存在 3 处直接冲突（A.1 = D.7 已推迟；B.2 = BranchPredictorFactory 已删除；B.3 = D.7 推迟）+ 3 处范围模糊（A.2 / B.1 部分 / B.4）。

任何 M4-DSE 实施者按 v1.0 §9 直接开工，都会违反 Oracle 评审结论。在 status.md §4.1 已规划 M4.12-M4.19 任务的当前状态下，必须先做一次**文档裁剪验证**，把 v1.0 §9 的 11 任务按 Oracle 评审重新分类为 ✅ 允许 / 🟡 需重设计 / ❌ 删除，然后才有干净的实施起点。

这是 0 行代码的"前置审计" change，但**它解锁整个 M4-DSE / M5-DSE 队列**（共 18 子任务，估时 2-3 周）。

## What Changes

- 在 `dse_architecture_v2_locks.md` 新增 §7 章节："§9 任务与 Oracle 评审对齐表"，列出 11 任务逐条裁剪结果（每条标注：v1.0 内容 / v2.0-locks 结论 / 状态 ✅🟡❌）
- 在 `dse_architecture.md` v1.0 §9 Phase A 段开头添加 1 段"⚠️ Oracle 评审后裁剪提示"，指向 v2.0-locks §7
- 在 `dse_architecture.md` v1.0 §9 Phase B 段开头添加 1 段"⚠️ B.1/B.2/B.3 需重设计或拆分到 Phase 5+"
- 在 `ip/cpu/docs/status.md` §4.1 顶部添加 1 行 link，引用 v2.0-locks §7 作为 M4.12-M4.19 任务的"前置"
- **不修改** `dse_architecture_v2_locks.md` §1-§6 已有内容（Oracle 评审通过的冻结区）
- **不修改** `dse_architecture.md` v1.0 §1-§8 的设计内容（只动 §9 任务清单的指针）
- **不修改**任何 `.cpp`/`.h` 代码（这是纯文档 change）

## Capabilities

### New Capabilities
- `dse-scope-alignment-v2-locks`: 文档对齐契约 — `dse_architecture.md` v1.0 §9 任务必须经 `dse_architecture_v2_locks.md` §7 裁剪表（Oracle 评审通过状态）映射后才可实施。读者通过文档交叉引用与文件位置可验证对齐。

### Modified Capabilities
（无 — 没有修改任何 `openspec/specs/` 已有 capability 的 REQUIREMENTS）

## Impact

- **影响文件**:
  - 修改: `ip/cpu/docs/dse_architecture_v2_locks.md`（+1 §7 章节，~50 行表格 + 文字）
  - 修改: `ip/cpu/docs/dse_architecture.md`（§9 Phase A/B 段头各 +1 段指针，~10 行）
  - 修改: `ip/cpu/docs/status.md`（§4.1 顶部 +1 行 link，~2 行）
- **依赖与时序**:
  - 本 change 不依赖任何活跃 change（M4G 已归档完成）
  - 本 change 是 **M4-DSE / M5-DSE 任何实施 change 的硬前置**（按 v1.0 §9 直接开工会违反 Oracle 评审）
- **基线影响**:
  - `tools/verify_adr.sh` 仍 PASS（不动 ADR）
  - `tools/verify_no_ghost_refs.sh` 仍 PASS（不动代码）
  - `ctest` 不退化（不动代码，36/36 保持绿）
  - 编译时间影响：0
- **运行时影响**: 零
- **API 影响**: 零
- **breaking 变更**: 零

## Alternatives Considered

### Alternative A: 不做裁剪，直接按 v1.0 §9 开工 M4-DSE

**放弃理由**: 立即触发 3 处 Oracle 冲突（A.1 = D.7 已推迟 / B.2 = Factory 已删 / B.3 = D.7 推迟），需要中途重设计或撤销，浪费 1-2 天。**不放弃**：先做 30 分钟文档对齐，干净起点。

### Alternative B: 把 v1.0 §9 整段删掉，只留 v2.0-locks

**放弃理由**: v1.0 §9 仍含 **6 个 ✅ 允许的任务**（A.3/A.4/A.5/A.6/B.5），删除它们会丢失 M4-DSE 实施细节（文件路径、估时、验收标准）。**不放弃**：保留 v1.0 §9 内容，仅加 ⚠️ 提示，v2.0-locks §7 表格作权威裁剪源。

### Alternative C: 改 v1.0 §9 表格的每一行（标 ✅🟡❌ 状态列）

**放弃理由**: v1.0 §9 是"实施路线图"格式（任务/文件/验收），加状态列会污染表格；状态信息属于 v2.0-locks 的"决策文档"职责。**不放弃**：在 v2.0-locks §7 单独建对齐表，v1.0 §9 仅加指针。

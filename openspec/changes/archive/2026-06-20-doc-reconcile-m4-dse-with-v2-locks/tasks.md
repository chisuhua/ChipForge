# Tasks: doc-reconcile-m4-dse-with-v2-locks

> **总工时**: ~30 分钟 (0 行代码, 3 文档编辑)
> **执行模式**: 主分支直接改 (纯文档, 无代码冲突风险)
> **依赖**: M4G 已完成 ✅ (commit 89892aa, 2026-06-19)
> **Oracle 评审**: 间接引用 `dse_architecture_v2_locks.md` (2026-06-17, bg_df09c224)
> **作为前置**: M4-DSE / M5-DSE 任何实施 change 必须先通过本裁剪

## 1. v2.0-locks §7 裁剪表

- [x] 1.1 在 `ip/cpu/docs/dse_architecture_v2_locks.md` §6 之后插入新 §7 章节，标题"§9 任务与 Oracle 评审对齐表"
- [x] 1.2 §7 表格包含 11 行（v1.0 §9 Phase A 6 任务 + Phase B 5 任务），列：v1.0 任务编号 / 内容摘要 / v2.0-locks §6.3 引用 / 状态 (✅/🟡/❌)
- [x] 1.3 §7 表格的 3 个 ❌ 行（A.1, B.2, B.3）各加 1-2 句引用 §6.3 原文，标注为何被否决
- [x] 1.4 §7 表格的 3 个 🟡 行（A.2, B.1 部分, B.4）各加 1-2 句说明"需重设计"的方向（如：移到 Phase 5+ / 拆分 / 简化）
- [x] 1.5 §7 表格的 5 个 ✅ 行（A.3, A.4, A.5, A.6, B.5）各加 1 句确认来源（E.6 允许 / §1.2 隐含 / 文档未禁止）
- [x] 1.6 §7 末尾加 1 段"权威性声明"：本表是 v1.0 §9 任务的裁剪源；任何 v1.0 §9 实施必须先对照本表

## 2. v1.0 §9 段头 ⚠️ 提示

- [x] 2.1 在 `ip/cpu/docs/dse_architecture.md` §9 Phase A 段开头（"### Phase A — 骨架 (M4-DSE, 1 周)" 前）加 1 段 ⚠️ 提示（5-7 行），引用 v2.0-locks §7 表格
- [x] 2.2 在 `ip/cpu/docs/dse_architecture.md` §9 Phase B 段开头（"### Phase B — 配置真正生效 (M4-DSE, 1 周)" 前）加 1 段 ⚠️ 提示（5-7 行），引用 v2.0-locks §7 表格
- [x] 2.3 ⚠️ 提示包含 3 个要素：(a) Oracle 评审日期 (b) v2.0-locks 链接 (c) 一句话说明"按 v1.0 §9 直接开工会违反 Oracle 评审"
- [x] 2.4 ⚠️ 提示**不修改** v1.0 §9 表格的任何任务行（仅加段头引用）

## 3. status.md §4.1 前置 link

- [x] 3.1 在 `ip/cpu/docs/status.md` §4.1 段头（"### 4.1 M4-DSE 子阶段" 标题后）加 1 行："> **前置**: v1.0 §9 任务需先经 [`dse_architecture_v2_locks.md` §7](../../dse_architecture_v2_locks.md#7-9-任务与-oracle-评审对齐表) 裁剪，本表保留 M4.12-M4.19 编号待实施时重新映射"
- [x] 3.2 M4.12-M4.19 8 个任务状态保持 🔵 待启动（**不**重新分类——本 change 仅加 link，不重编号）

## 4. 验证

- [x] 4.1 验证 `git diff --stat` 报告 3 文件改动，~60 行 markdown（v2.0-locks ~50 + dse_architecture ~10 + status.md ~2）
- [x] 4.2 验证 `grep -c "✅\|🟡\|❌" ip/cpu/docs/dse_architecture_v2_locks.md` 增量 ≥ 11（11 任务每行至少 1 状态符号）
- [x] 4.3 验证 `grep -n "v2.0-locks §7\|dse_architecture_v2_locks.md#7" ip/cpu/docs/dse_architecture.md` ≥ 2（Phase A + Phase B 段头各 1 处）
- [x] 4.4 验证 `grep -n "v2.0-locks" ip/cpu/docs/status.md` ≥ 1（§4.1 前置 link）
- [x] 4.5 验证 `cd build && ctest` 仍 36/36 PASS（0 行代码改动，预期不退化）
- [x] 4.6 验证 `tools/verify_adr.sh` 仍 PASS（不动 ADR）
- [x] 4.7 验证 `tools/verify_no_ghost_refs.sh` 仍 PASS（不动代码）

## 5. git 验收

- [x] 5.1 commit 1：`docs(dse): reconcile v1.0 §9 with v2.0-locks §6.3 Oracle 评审`（3 文件改动）
- [x] 5.2 验证 commit message 包含 2 个引用：`dse_architecture_v2_locks.md` (本表 §7) + `dse_architecture.md` (v1.0 §9 段头)

## 6. PR 准备

- [x] 6.1 PR description 引用 `bg_df09c224` (Oracle 2026-06-17 评审)
- [x] 6.2 PR description 引用本 change 是 M4-DSE / M5-DSE 任何实施 change 的硬前置
- [x] 6.3 PR description 标注：本 change 不引入新 capability（`Modified Capabilities` 为空）

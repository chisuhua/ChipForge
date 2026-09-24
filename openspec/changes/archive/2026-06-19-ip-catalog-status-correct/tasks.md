# Tasks: ip-catalog-status-correct

> **总工时**: ~3h
> **执行模式**: 主分支直接改（变更范围小且仅文档）
> **依赖**: CHANGE-001 (doc-code-realignment, v0.0.2) ✅ archived；v0.0.5 (empty-directory-cleanup) ✅ 落地
> **作为前置**: CHANGE-003 (cache-policy-foundation) 依赖本 change 完成后 cache 状态稳定

## 0. 预执行

- [x] 0.1 archive v1 原稿到 `openspec/changes/archive/2026-06-18-ip-catalog-status-correct-v1-original/`
- [x] 0.2 修正 L1CachePlugin.h L18 注释（32KB → 16KB）— 在 cache-policy-foundation §0.1 已落地

## 1. 同步修正 ip/cache/README.md

- [x] 1.1 修改 `ip/cache/README.md §4` "可插拔策略"表格：将"256 sets × 64-byte cache line = 32KB L1"改为"256 × 1 way × 64B = 16KB L1 (direct-mapped)"
- [x] 1.2 修改 `ip/cache/README.md §5` "配置参数"表格：`capacity_kb` 默认值 32 → 16
- [x] 1.3 验证 `grep -n "16KB\|16 kb" ip/cache/README.md` 至少出现 1 次
- [x] 1.4 验证 `grep -n "32KB" ip/cache/README.md` 0 命中

## 2. 重写 IP 状态表

- [x] 2.1 在 `docs/architecture/ip-catalog.md` 表格增加"实现范围"列（8 个 IP 全部填写）
- [x] 2.2 修正 L1Cache 状态为 `"🟡 TLM 实现中 (Phase 1.3, L1 unified direct-mapped 16KB, L1I/L1D/L2 未拆分)"`
- [x] 2.3 5 个零代码 IP（memory/interconnect/peripheral/tilecore/tilecopy）的"实现范围"列填 `"0 LOC, 仅 STATUS.md (v0.0.5 PLANNED/INITIAL DESIGN)"` — **不要重复 STATUS.md 内容**
- [x] 2.4 在表格增加"实施预计"列，零代码 IP 指向 v0.0.5 STATUS.md 路径（详见 design §Decision 3）
- [x] 2.5 验证 `grep -c "实施预计" docs/architecture/ip-catalog.md` ≥ 1
- [x] 2.6 验证 `grep -c "实现范围" docs/architecture/ip-catalog.md` ≥ 1
- [x] 2.7 验证 `grep -c "32KB" docs/architecture/ip-catalog.md` 0 命中（确保容量错误彻底清除）

## 3. 同步现有 IP 状态文件

- [x] 3.1 检查 5 个 STATUS.md（`ip/memory/STATUS.md`, `ip/interconnect/STATUS.md`, `ip/peripheral/STATUS.md`, `ip/tilecore/STATUS.md`, `ip/tilecopy/STATUS.md`）的"实施预计"字段与 ip-catalog.md 新增列**内容一致**
- [x] 3.2 检查 2 个 README（`ip/cpu/README.md`, `ip/cache/README.md`）的"实现状态"段与 ip-catalog.md L1Cache 行描述一致
- [x] 3.3 **不要修改** `ip/README.md`（v0.0.5 STATUS 约定段已含 7 IP 状态表）
- [x] 3.4 **不要创建** `docs/templates/IP_README_TEMPLATE.md`（v0.0.5 IP_STATUS_TEMPLATE.md 已覆盖）

## 4. 更新验证基线

- [x] 4.1 在 `CHANGELOG.md` 顶部 `## v0.0.3` 段添加条目：
  - L1Cache 状态从 `Phase 1.2 L1D` 修正为 `Phase 1.3, L1 unified direct-mapped 16KB`
  - ip-catalog 表格增加"实现范围"+"实施预计"列
  - 5 个零代码 IP 实施预计链接至 v0.0.5 STATUS.md
- [x] 4.2 验证 `tools/verify_no_ghost_refs.sh` 仍 PASS
- [x] 4.3 验证 `tools/verify_adr.sh` 仍 PASS
- [x] 4.4 验证现有 ctest 不破坏（仅修改文档，无需重 build；基线 16/16 不变）

## 5. PR 准备

- [x] 5.1 `git diff --stat` 报告变更规模（commit `59b6b1a`：2 files, 18+/17-）
- [x] 5.2 PR description 引用（以 commit message 形式落地）：
  - v0.0.5 (empty-directory-cleanup) 已建 STATUS.md + IP_STATUS_TEMPLATE.md
  - CHANGE-001 (doc-code-realignment, v0.0.2) 是本 change 的依赖
  - 本 change 是 CHANGE-003 (cache-policy-foundation) 的前置依赖
- [x] 5.3 PR description 引用 `archive/2026-06-18-ip-catalog-status-correct-v1-original/` 中的 4 项 v1 问题已在本版本修复
- [x] 5.4 Request review（直 commit main，作者自审 + 后续 Oracle 评审）

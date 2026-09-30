## Why

`tools/doc_checker.py` 在 main HEAD (post-#6 merge, commit `25a2daf`) 报 **66 个问题**（62 broken links + 1 IP 结构 + 1 deprecated 残留 + 2 framework paths），全部为**预先存在腐化**（不属任何活跃 change），导致 PR #6 的 `Documentation Health Check` workflow 失败（虽然 smoke-only 不阻塞，但每次 PR docs/ip/ 路径变更都触发 PR 评论噪声）。

**根因** 三层叠加：
1. **doc_checker.py scope 过宽**：扫描 `openspec/changes/archive/`、`soc/cpu/docs/roadmap/archive/`、`build/_deps/install/include/cpp-tlm/`、`sail-riscv-Linux-x86_64/` 等**历史/外部/vendored 路径**，将这些不该修的旧快照误判为 broken
2. **active docs 真实 broken**（15 个）：v0.10 文档结构迁移（`soc/cpu/docs/roadmap/` + `docs/superpowers/` 合并）+ IP README 路径漂移（`ip/cpu/test/` 已删但 doc_checker 不感知）
3. **CppHDL 框架路径**：reference 文档列出 2 个不在 CppHDL 当前 include 路径下的旧 header（axi4_lite.h / clock_reset_bundle.h）

按 v0.10.0 wave5 launch gate（`[cpu-l1-mmu-demo]` 6/6 + `[riscv-tests]` 40/40 PASS 之外），文档健康也是 release 验收项之一（CHANGELOG §Verification）。

## What Changes

### 1. 扩展 doc_checker.py scope 配置（核心修复）

- 新增 `tools/doc_checker.py` 默认跳过配置：
  - `openspec/changes/archive/**` (openspec 已归档 change)
  - `**/archive/**` (通用 archive 标记)
  - `build/_deps/**` (ExternalProject build artifact)
  - `sail-riscv-Linux-x86_64/**` (vendored 第三方)
- 新增 `--include-archive` flag（默认 off）让 archive 扫描成为 opt-in
- `ip_structure_check`: 增加 `ip/<name>/test/` allowlist（`ip/cpu` 因 2026-06-17 删除 test/ 是已知历史决定，README §测试位置 已注明）

### 2. 修 active docs 的 15 个真实 broken links

| # | 文件 | 行 | 当前引用 | 修复 |
|---|------|---|----------|------|
| 1 | `README.md` | 46 | `soc/riscv_virt.json` | 改为 `soc/` 概述 + 删 JSON 引用 |
| 2 | `bundles/README.md` | 7 | `../soc/cpu/docs/roadmap/phase-1-tlm-foundation.md` | 改 `archive/phase-1-tlm-foundation.md` 或 `roadmap/README.md` |
| 3-9 | `docs/architecture/plugin-research/external-plugin-architectures.md` | 155/242/583/592/1016/1169/1172/1175 | `classOf[X]`, `key: Handle[T]`, `clazz`, `file:///workspace/project/CppHDL/...` | 全部为 Scala 代码文本被 markdown 解析为链接，用 `` ` `` 包成 inline code 或加 `<…>` 转义 |
| 10 | `ip/cache/README.md` | 148/166/199 | phase 文档路径 | 改 `soc/cpu/docs/roadmap/...` 或 `archive/...` |
| 11 | `ip/cpu/docs/multi_isa_architecture.md` | 734 | 待查 | 现场修复 |
| 12 | `ip/mmu/README.md` | 150 | 待查 | 现场修复 |
| 13 | `openspec/changes/vexii-riscv-parity-poc/tasks.md` | 33 | `../../specs/vexii-riscv-parity-runner/spec.md` | 创建缺失 spec 或改 `../../openspec/changes/vexii-riscv-parity-poc/proposal.md` |
| 14 | `soc/cpu/docs/architecture.md` | 209 | `roadmap/phase-2-baremetal.md` | 改 `roadmap/archive/phase-2-baremetal.md` |
| 15 | `soc/cpu/docs/roadmap/references/adr-matrix.md` | 4 | `../../../../docs/architecture/adr.md` | 路径深度调整 |
| 16 | `soc/docs/integration-guide.md` | 73 | `../../docs/architecture/interfaces.md` | 现场修复 |
| 17 | `tests/README.md` | 3 | `docs/superpowers/plans/2026-06-30-catch2-test-framework.md` | 改 `docs/superpowers/plans/` 或 `docs/` 路径 |
| 18 | `tests/cpu/manual_elf/README.md` | 58 | `../../docs/implementation-plan/M4-integration.md` | 现场修复

### 3. 修 framework path reference（2）

- 检查 `CppHDL/include/axi4/axi4_lite.h` 与 `CppHDL/include/bundle/clock_reset_bundle.h` 是否已迁移
- 如已迁移：更新 reference 文档至新路径
- 如未迁移：在 `tools/doc_checker.py` framework allowlist 中显式登记为已知 missing（reference 文档先于框架实现）

### 4. 修 deprecated 残留（1）

- `.opencode/skills/openspec-apply-change/SKILL.md:43` 含旧目录名 `implementation/`
- 改为 `openspec/changes/` 或 `src/cf_plugin/` 现状

## Capabilities

### New Capabilities

- `doc-checker-skip-config`: `tools/doc_checker.py` 增加 skip patterns + flags + ip_structure allowlist 机制，让 history/archive/vendored 路径不污染健康评分

### Modified Capabilities

无（不涉及业务 spec 行为变更；仅修复 doc_checker.py 工具配置 + 现有文档链接准确性）

## Impact

### 受影响代码

- `tools/doc_checker.py` (核心增强, ~50 LOC)
- `tools/doc_link_check.sh` (不变)
- 16 个 active docs (links 修正, 小规模)
- `.opencode/skills/openspec-apply-change/SKILL.md` (deprecated 修复)

### 受影响 API / CI

- `.github/workflows/doc_check.yml`: 不变（保持 smoke-only 性质），但修复后应该转 ✅
- PR #6 + 后续 PR: doc_check.yml 不再 FAIL，PR 评论噪声消失

### 受影响 AGENTS.md

- AGENTS.md §CI 细节表 doc_check.yml 行：当前标 "smoke-only"，修复后改为 "smoke + 实测全 PASS"

### 依赖 / 启动时机

- **不依赖**任何 active change
- **优先级**：P3（cleanup 类，不阻塞 v0.10.0 launch gate 但改善 CI 噪声）
- **建议启动时机**：wave5-isa-coverage-and-bp 内任意时间窗（不与 mfc Phase E/G/H 关键路径冲突）

### 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| doc_checker.py skip pattern 配置错误导致真实 broken 漏报 | LOW | 修复前后跑 `python tools/doc_checker.py --format json` 对比 broken count；启用 `--include-archive` 让 archive 仍可显式扫描 |
| active docs 改链接后语义变化 | LOW | 修复以"保留指向原意"为原则（如 `phase-2-baremetal.md` → `archive/phase-2-baremetal.md`，文档已 archive 不变更内容） |
| CppHDL framework path 误判（实际迁移 vs reference 滞后） | LOW | 检查 CppHDL 实际 include + 在 PR body 注明决策依据 |
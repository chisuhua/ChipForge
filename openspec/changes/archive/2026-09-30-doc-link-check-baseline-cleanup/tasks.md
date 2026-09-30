# Tasks: doc-link-check-baseline-cleanup

> **OpenSpec change**: `doc-link-check-baseline-cleanup` (proposed)
> **Initiative**: `tooling-cleanup-and-ci-noise-reduction` (独立 initiative, 不绑定 wave5)
> **Priority**: P3
> **Estimated effort**: 1-2 天 (cleanup 类工作, 单 PR 落地)
> **Risks**: LOW (doc_checker.py 配置层 + docs link 修复, 无业务逻辑变化)

## Phase A: 扩展 `tools/doc_checker.py` skip/allowlist 配置层

- [ ] **A.1**: 读 `tools/doc_checker.py` 主入口（`link_check` / `ip_structure_check` / `framework_check` / `deprecated_check` 4 函数）, 确认现有调用约定
- [ ] **A.2**: 在模块顶部新增 3 个常量（参考 `design.md` D1）:
  - `DEFAULT_SKIP_PATTERNS: list[str]` (6 项: archive/build/sail/cpphdl/cpptlm)
  - `KNOWN_EXEMPT_TEST_DIRS: dict[str, str]` (1 项: cpu + 注释)
  - `FRAMEWORK_PENDING_HEADERS: dict[str, str]` (2 项: axi4_lite + clock_reset_bundle)
- [ ] **A.3**: `_is_skipped(file_path: str) -> bool` helper (path 包含任一 pattern)
- [ ] **A.4**: `link_check` 在 scan loop 入口加 `if _is_skipped(file_path) and not args.include_archive: continue`
- [ ] **A.5**: `ip_structure_check` 在缺失 test/ 判断处加 `if ip_name in KNOWN_EXEMPT_TEST_DIRS: continue`
- [ ] **A.6**: `framework_check` 在 missing 判断处加 `if path in FRAMEWORK_PENDING_HEADERS: continue`
- [ ] **A.7**: CLI parser 新增 `--include-archive` flag (action='store_true', default=False)
- [ ] **A.8**: 跑 `python -m unittest tools.test_doc_checker -v` 验证单测不退化

## Phase B: 修 active docs 的 broken links (D6 最小化原则)

### B.1: README.md (顶层)

- [ ] **B.1.1**: 行 46 — 改 `soc/riscv_virt.json` 为 `soc/` 概述（避免指向非真实存在的 JSON, 不属于本仓库 SoC 范围）

### B.2: bundles/README.md

- [ ] **B.2.1**: 行 7 — 改 `../soc/cpu/docs/roadmap/phase-1-tlm-foundation.md` 为 `../soc/cpu/docs/roadmap/archive/phase-1-tlm-foundation.md`

### B.3: docs/architecture/plugin-research/external-plugin-architectures.md (7 处 Scala 代码文本)

- [ ] **B.3.1**: 行 155 `classOf[X]` → `` `classOf[X]` `` (inline code)
- [ ] **B.3.2**: 行 242 `key: Handle[T]` → `` `key: Handle[T]` ``
- [ ] **B.3.3**: 行 583 `clazz` → `` `clazz` ``
- [ ] **B.3.4**: 行 592 `classOf[X]` → `` `classOf[X]` ``
- [ ] **B.3.5**: 行 1016 `classOf[X]` → `` `classOf[X]` ``
- [ ] **B.3.6**: 行 1169 `file:///workspace/project/CppHDL/include/component.h` → 相对路径或 inline code
- [ ] **B.3.7**: 行 1172 `file:///workspace/project/CppHDL/include/module.h` → 同上
- [ ] **B.3.8**: 行 1175 `file:///workspace/project/CppHDL/include/ch.hpp` → 同上

### B.4: ip/*/README.md (cache / mmu)

- [ ] **B.4.1**: `ip/cache/README.md` 行 148/166/199 — phase 文档路径加 `archive/` 前缀或删
- [ ] **B.4.2**: `ip/mmu/README.md` 行 150 — 现场修复（先 `python tools/doc_checker.py --format text` 看具体 target）

### B.5: ip/cpu/docs/multi_isa_architecture.md

- [ ] **B.5.1**: 行 734 — 现场修复（grep + 看 target）

### B.6: openspec/changes/vexii-riscv-parity-poc/tasks.md

- [ ] **B.6.1**: 行 33 — 创建 `openspec/changes/vexii-riscv-parity-poc/specs/vexii-riscv-parity-runner/spec.md` (新建 spec 内联) 或改 `../../openspec/changes/vexii-riscv-parity-poc/proposal.md` 引用
- [ ] **B.6.2**: 决策记录于 commit body (选创建 vs 改引用)

### B.7: soc/cpu/docs/* (3 处)

- [ ] **B.7.1**: `soc/cpu/docs/architecture.md:209` — `roadmap/phase-2-baremetal.md` → `roadmap/archive/phase-2-baremetal.md`
- [ ] **B.7.2**: `soc/cpu/docs/roadmap/references/adr-matrix.md:4` — `../../../../docs/architecture/adr.md` 路径深度调整（验证目标存在）

### B.8: soc/docs/integration-guide.md

- [ ] **B.8.1**: 行 73 — 改 `../../docs/architecture/interfaces.md` 为实际存在路径或加 allowlist 注释

### B.9: tests/* (2 处)

- [ ] **B.9.1**: `tests/README.md:3` — 改 `docs/superpowers/plans/2026-06-30-catch2-test-framework.md` 为现状 `docs/superpowers/plans/` 路径或 `docs/` 路径
- [ ] **B.9.2**: `tests/cpu/manual_elf/README.md:58` — 改 `../../docs/implementation-plan/M4-integration.md` 为现状 `docs/` 路径

## Phase C: 修 deprecated 残留 (D7)

- [ ] **C.1**: `.opencode/skills/openspec-apply-change/SKILL.md:43` — 旧目录名 `implementation/` 改为 `openspec/changes/` 或 `src/cf_plugin/` 现状

## Phase D: 验证 + commit

- [ ] **D.1**: 跑 `python tools/doc_checker.py --format json` 验证 broken count 大幅下降（≤ 5 目标）
- [ ] **D.2**: 跑 `python tools/doc_checker.py --include-archive --format json` 验证 archive 重新扫描行为正确
- [ ] **D.3**: 跑 `python -m unittest tools.test_doc_checker -v` 单测不退化
- [ ] **D.4**: 跑 `bash tools/verify_plugin_decision.sh` 8/8 PASS
- [ ] **D.5**: 跑 `bash tools/check_plugin_portability.sh` 12/12 PASS
- [ ] **D.6**: 跑 `bash tools/doc_link_check.sh` PASS
- [ ] **D.7**: git diff self-review（README 不变量: AGENTS.md §CI 细节表 doc_check.yml 行更新 "smoke-only" → "smoke + 实测全 PASS"）
- [ ] **D.8**: commit (按 git-master skill SEMANTIC 风格)
  - 建议拆 2-3 commits:
    - `feat(tool): doc_checker skip patterns + --include-archive flag (Phase A)`
    - `fix(docs): repair 16 active docs broken links (Phase B + C)`
    - `docs(agents): doc_check.yml 不变量 sync (Phase D.7)`
- [ ] **D.9**: 推 PR + CI 验证 (Architecture Compliance + Doc Health)
- [ ] **D.10**: PR merge 后 openspec archive 流程（按 `openspec-archive-change` skill）

## 退出标准

- ✅ `python tools/doc_checker.py --format json` 显示 `links / ip / framework / deprecated` 全 ✅ (或 broken count ≤ 5 且全部为合理豁免)
- ✅ `tools/test_doc_checker.py` 单测不退化
- ✅ `verify_plugin_decision.sh` 8/8 PASS
- ✅ `check_plugin_portability.sh` 12/12 PASS
- ✅ `Documentation Health Check` workflow 转 ✅ (smoke-only 但代表 PR 评论噪声消失)
- ✅ AGENTS.md §CI 细节表 doc_check.yml 行更新 "smoke + 实测全 PASS"
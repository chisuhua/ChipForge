## Context

`tools/doc_checker.py` 在 main HEAD (post-#6 merge, commit `25a2daf`) 报告 **66 个问题**：62 broken links + 1 IP 结构 + 1 deprecated 残留 + 2 framework paths。背景调研（详见 `proposal.md` Why 段）显示根因三层叠加：

1. **scope 过宽**：扫描 archive/build/vendored 路径误报
2. **active docs 真实 broken**：文档结构迁移后未清理引用
3. **CppHDL 框架路径**：reference 滞后于实际 header 路径

`tools/doc_checker.py` 当前结构（基于 `tools/test_doc_checker.py` 单测可见）：每个 check 独立实现，没有共享的 skip-patterns 配置层；`ip_structure_check` 用硬编码 IP 列表。本设计在保持现有 check 接口的同时引入 3 层可配置项。

## Goals / Non-Goals

**Goals:**

- 让 `python tools/doc_checker.py` 在干净仓库（除 archive/external/已知 exempt 外）达到 `links / ip / framework / deprecated` 全 ✅
- PR #6 + 后续 PR 的 `Documentation Health Check` workflow 不再 FAIL
- 保持 `tools/doc_checker.py` 现有 CLI 接口（`--format json/text`, `--verbose`）向后兼容
- 新增 `--include-archive` flag 让 archive 扫描成为 opt-in（用于历史 drift 审计）
- 修 16 个 active docs 的真实 broken link（小规模、保留语义）
- 修 1 个 deprecated 残留（`.opencode/skills/openspec-apply-change/SKILL.md` 旧目录名引用）

**Non-Goals:**

- 不重写 `doc_checker.py` 整体架构（仅最小必要配置层增强）
- 不修 archive 目录内 broken links（snapshot 不变原则）
- 不增加 `--no-framework-allowlist` / `--no-ip-exempt` 等反 flag（避免配置膨胀）
- 不引入新工具或 Python 依赖
- 不修改 `tools/doc_link_check.sh`（轻量脚本，仅 cross-reference 校验，与 doc_checker.py 解耦）

## Decisions

### D1. 集中配置在 doc_checker.py 模块顶部常量

将 skip patterns、IP allowlist、framework allowlist 定义为模块顶部 named tuple / dict，3 个 check 函数读取同一配置：

```python
# tools/doc_checker.py (新增 ~30 LOC at module top)
DEFAULT_SKIP_PATTERNS = [
    'openspec/changes/archive',
    '/archive/',
    'build/_deps/',
    'sail-riscv-Linux-x86_64/',
    'cpp-tlm/',
    'CppHDL/',
]

KNOWN_EXEMPT_TEST_DIRS = {
    'cpu': '2026-06-17 删除 (README §测试位置 已注明)',
    # 后续可加 (如 'mmu': '...')
}

FRAMEWORK_PENDING_HEADERS = {
    'CppHDL/include/axi4/axi4_lite.h': 'Phase 5+ RTL integration planned (per docs/roadmap/phases/phase-5-rtl.md)',
    'CppHDL/include/bundle/clock_reset_bundle.h': 'Phase 3+ peripheral planned (per docs/roadmap/phases/phase-3-rtos.md)',
}
```

**Rationale**: 集中可见 + 单测易覆盖（`tools/test_doc_checker.py` 现有结构适合）。**alternatives considered**: YAML 配置文件（拒绝：增加 IO 依赖与解析复杂度，不值得为此 5 行的配置引入新格式）。

### D2. `link_check` 应用 skip via path prefix 过滤

`link_check` 当前扫所有 markdown，输出每个文件的 broken refs。改造：在 scan 阶段对 `file_path` 应用 `DEFAULT_SKIP_PATTERNS` 前缀匹配，匹配的文件直接跳过扫描。

```python
def _is_skipped(file_path: str) -> bool:
    return any(p in file_path for p in DEFAULT_SKIP_PATTERNS)

# scan loop:
for md_file in glob(...):
    if _is_skipped(md_file) and not args.include_archive:
        continue
    errors.extend(_check_file(md_file))
```

**Rationale**: path 前缀匹配是最简且对 gitignore-style 路径足够。**alternatives considered**: gitignore parser（`pathspec`）— 拒绝：依赖 `dulwich` 或自己实现 parser，过重。

### D3. `--include-archive` flag 是 single boolean

`--include-archive` 单 flag，无复杂 policy（不像 ripgrep 的 `--no-ignore`）。当 present 时，所有 DEFAULT_SKIP_PATTERNS 中与 archive 相关的项（`openspec/changes/archive`、`/archive/`）失效；`build/`、`sail-riscv`、`CppHDL/cpp-tlm` 仍 skip（这些永远不该被扫）。

**Rationale**: archive 是历史快照语义（不可改），但**审计**价值有时需要；外部/vendored 是永久非源码，无审计需求。**alternatives considered**: 独立 `--no-build-skip` 等多个 flag — 拒绝，flag 膨胀无收益。

### D4. `ip_structure_check` 用 `KNOWN_EXEMPT_TEST_DIRS` dict

每个 entry 的 key 是 IP 名（`cpu` / `mmu` / ...），value 是注释字符串（说明豁免理由）。check 函数跳过这些 IP 的 `test/` 缺失检查。

**Rationale**: 让豁免决策可见 + 易审计（git blame 到具体豁免 commit）。**alternatives considered**: 在 `tools/doc_checker.py` 顶部用 `# ip/cpu: 2026-06-17 删除` 注释 — 拒绝，散落注释易遗漏。

### D5. `framework_check` 用 `FRAMEWORK_PENDING_HEADERS` dict

每个 entry 的 key 是 CppHDL/CppTLM 相对路径，value 是注释字符串（说明 pending 状态 + 引用源）。

**Rationale**: 与 D4 一致风格。**alternatives considered**: 从 CppHDL repo 的 `STATUS.md` / `CHANGELOG.md` 自动解析 — 拒绝，IO 跨仓库 + 解析格式不稳定，不值得。

### D6. 修 16 个 active docs 时保守最小化

每个文件改动原则：
- 保留指向原意（不删内容，仅改路径）
- archive 类引用加 `archive/` 前缀（如 `phase-2-baremetal.md` → `archive/phase-2-baremetal.md`）
- Scala 代码文本 `classOf[X]` / `clazz` / `key: Handle[T]` 用 `` ` `` 包成 inline code
- `file:///workspace/...` 绝对路径改为相对路径
- 不重构文档结构，仅修连接字符串

### D7. 不修改 `.opencode/skills/` 内 SKILL.md 的 `openspec-propose/openspec-archive-change` 等（除 deprecated 1 处）

这些是 .opencode/ tools/scope，已 force-tracked 但不在 user-facing doc 范围。本次仅修 `.opencode/skills/openspec-apply-change/SKILL.md:43` 的 `implementation/` 旧目录名引用（被 doc_checker 的 deprecated check 检测到）。

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| skip pattern 配错导致真实 broken 漏报 | 修复前后跑 `python tools/doc_checker.py --format json` 对比 broken count；保留 `--include-archive` 让 archive 仍可显式扫描 |
| `KNOWN_EXEMPT_TEST_DIRS` 滥用导致新 IP 也免 test 检查 | 注释强制必填 + 仅豁免有 README 文档说明删除日期的 IP（check 逻辑：豁免 entry 必须 value 非空） |
| `FRAMEWORK_PENDING_HEADERS` 累积成"missing header 坟墓" | 半年一次 code review 清理已实现的 path；本 change entry commit body 注明清理触发条件 |
| 修 active docs 改链接后语义变化（指向 archive 而非现行文档） | archive 文件本身就是历史快照，指向 archive 是承认历史；用户跳转时可看 archive 标记 |
| `doc_checker.py` 改动后单测失败 | `tools/test_doc_checker.py` 是 vendored 单测，本 change 不改 check 主逻辑仅加配置层，单测应全绿；CI 实测验证 |

## Migration Plan

**Phase A (本 change archive 前)**: 单 PR 落地所有修复 + 配置增强

1. 提交 doc_checker.py skip/allowlist 配置（D1+D2+D3+D4+D5）
2. 修复 16 个 active docs 的 broken links（D6）
3. 修复 1 个 deprecated SKILL.md（D7）
4. 跑 `python tools/doc_checker.py --format json` 验证 broken count 大幅下降（目标 ≤ 5）
5. 跑 `python -m unittest tools.test_doc_checker` 验证单测不退化
6. 跑 `bash tools/verify_plugin_decision.sh` + `bash tools/check_plugin_portability.sh` 验证 CI 门禁不退化

**Rollback strategy**: 单 PR revert 即可恢复所有行为；不涉及 schema 迁移 / 配置格式变化

**No phased rollout needed**: 本 change 是 doc_checker.py 配置 + 文档 link 修复，单 commit 即可生效

## Open Questions

1. **`sail-riscv-Linux-x86_64/` 来源**：本地工具下载的测试参考集（vendor 的 RISC-V 黄金模型），未来是否升级到 fetch + cache 模式？（不阻塞本 change，跟进记录在 archive/issues/）
2. **`openspec/changes/archive/` 永久豁免 vs 定期审计**：本 change 选永久豁免 + opt-in `--include-archive`。是否需要季度任务检查 archive 内 broken 状态？（建议：archive 内部 broken 是历史快照正确状态，不需要修）
#!/usr/bin/env bash
# tools/doc_link_check.sh
# 功能描述: 检查文档中的相对路径引用 (markdown 链接 + 反引号包裹的仓库路径), 列出断链
# 作者: ChipForge Build System
# 最后修改日期: 2026-11-05
#
# 用途:
#   - 本地手动运行: ./tools/doc_link_check.sh
#   - CI 集成: 失败时 exit 1 (可作为 architecture-gates.yml 的额外 step)
#
# 范围:
#   - [检查 1] markdown 链接: 扫描 docs/, ip/, src/cf_plugin/ + 根级文档
#                (AGENTS.md / README.md / CONTRIBUTING.md)
#   - [检查 2] 反引号路径: 根级文档 (AGENTS.md / README.md / CONTRIBUTING.md)
#                + docs/architecture/（含子目录）★ 2026-11-08 扩围（ADR-095 §2.5②）
#                —— docs/ 下架构文档含大量历史/规划态路径引用, 全量扫描噪声过高, 故不纳入
#   - 跳过: .omo/ (gitignored, 非用户面向), build/ (构建产物), .git/, 各种 cache 目录
#   - 类型: markdown 相对路径 (.md) + 反引号包裹的仓库相对路径 (带已知扩展名或目录)
#
# 退出码:
#   0 = 0 broken links
#   1 = >= 1 broken links (输出所有断链)
#   2 = usage error
#
# 详见:
#   - .omo/boulder.json work 2026-06-13 文档债审计
#   - commit 080cd28 (37 broken links 修复)
#   - tools/verify_adr.sh (类似的 shell 验证脚本风格)
#   - 2026-11-05: 增加「反引号路径」检查 (AGENTS.md 曾出现 6 处死引用, 如
#     .rddf/roadmap/archive/phase-6-declarative.md / code-test-ownership-report.md)

set -uo pipefail

# ----------------------------------------------------------------------------
# 路径配置
# ----------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

# 根级文档: 用户/agent 的第一入口 —— 死引用危害最大, 纳入两类检查
ROOT_DOCS=(
  "AGENTS.md"
  "README.md"
  "CONTRIBUTING.md"
)

# 扫描路径: 用户面向的 docs/ + 各 IP + cf_plugin README + 根级文档
SCAN_PATHS=(
  "docs"
  "ip"
  "src/cf_plugin"
  "${ROOT_DOCS[@]}"
)

# 检查 2 (反引号路径) 的适用范围 —— 根级文档 + docs/architecture/（含子目录）
# ★ 门禁②（ADR-095 §2.5② · change architecture-truth-boundary-enforcement AC-111）：
#   反引号路径检查原仅覆盖根级 3 份；扩入 docs/architecture/ 后，架构文档里
#   「`src/cf_plugin/tests/xxx`」这类历史死引用会被收进门禁。
#   ★ 只扩 docs/architecture/（不含其它 docs/ 子目录）—— 脚本注释已说明全量 docs/
#     噪声过高；先跑顺这一块，再评估是否继续扩大。
PATH_CHECK_DOCS=("${ROOT_DOCS[@]}")
while IFS= read -r _arch_doc; do
  [[ -n "$_arch_doc" ]] && PATH_CHECK_DOCS+=("$_arch_doc")
done < <(find docs/architecture -name '*.md' -type f 2>/dev/null | sort)

# 检查 2 的忽略清单: 每行一个路径 (允许"故意不存在"的引用), # 开头为注释
PATH_IGNORE_FILE="tools/doc_link_check_ignore.txt"

# 跳过目录 (在 SCAN_PATHS 内的子目录)
SKIP_DIRS=(
  ".git"
  "build"
  "_deps"
  ".code-review-graph"
  ".cache"
  ".agent" ".amazonq" ".augment" ".bob" ".claude" ".cline"
  ".codebuddy" ".codex" ".continue" ".cospec" ".crush"
  ".cursor" ".factory" ".forge" ".gemini" ".iflow" ".junie"
  ".kilocode" ".kiro" ".qoder" ".clinerules" ".cursorrules"
  ".opencode" ".vscode" ".idea"
  "mypy_cache" "pytest_cache" "ruff_cache" "__pycache__"
  "node_modules" ".venv" "venv"
  ".omo"  # gitignored, 非用户面向 (drafts/plans/notepads)
)

# ----------------------------------------------------------------------------
# 工具
# ----------------------------------------------------------------------------
log() { echo "[$(date +%H:%M:%S)] $*"; }

usage() {
  cat <<EOF
Usage: $0 [options]

Options:
  --strict    exit 1 on any broken link (default: exit 1 on broken, 0 if none)
  --quiet     only output broken links (no progress)
  --help      show this message
EOF
}

# ----------------------------------------------------------------------------
# 参数
# ----------------------------------------------------------------------------
QUIET=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --quiet) QUIET=1; shift ;;
    --help)  usage; exit 0 ;;
    *)       echo "Unknown arg: $1"; usage; exit 2 ;;
  esac
done

# ----------------------------------------------------------------------------
# 检查 1: markdown 相对链接
# ----------------------------------------------------------------------------
[[ $QUIET -eq 0 ]] && log "Scanning markdown cross-references in: ${SCAN_PATHS[*]}"

BROKEN_LINES=$(python3 - "${SCAN_PATHS[@]}" <<'PY'
import os, re, sys

# 收集所有 .md 文件 (目录递归 + 单文件)
md_files = []
for path_str in sys.argv[1:]:
    if not os.path.exists(path_str):
        continue
    if os.path.isfile(path_str):
        if path_str.endswith('.md'):
            md_files.append(path_str)
        continue
    for root, dirs, fs in os.walk(path_str):
        # 过滤 SKIP_DIRS (与 bash 端同步)
        skip = {'.git','build','_deps','.code-review-graph','.cache','.agent','.amazonq','.augment','.bob','.claude','.cline','.codebuddy','.codex','.continue','.cospec','.crush','.cursor','.factory','.forge','.gemini','.iflow','.junie','.kilocode','.kiro','.qoder','.clinerules','.cursorrules','.opencode','.vscode','.idea','__pycache__','node_modules','.venv','venv','mypy_cache','pytest_cache','ruff_cache','.omo'}
        dirs[:] = [d for d in dirs if d not in skip]
        for f in fs:
            if f.endswith('.md'):
                md_files.append(os.path.join(root, f))

# 链接正则
link_re = re.compile(r'\[([^\]]+)\]\(([^)]+)\)')

broken_count = 0
for path in sorted(md_files):
    try:
        with open(path) as f:
            content = f.read()
    except (OSError, IOError):
        continue
    for m in link_re.finditer(content):
        link_text, target = m.group(1), m.group(2)
        # 去除 anchor
        if '#' in target:
            target = target.split('#', 1)[0]
        # 跳过非 .md / 非相对路径
        if not target or not target.endswith('.md'):
            continue
        if target.startswith(('http://', 'https://', 'mailto:')):
            continue
        # 解析为绝对路径
        src_dir = os.path.dirname(os.path.abspath(path))
        if os.path.isabs(target):
            resolved = os.path.normpath(target)
        else:
            resolved = os.path.normpath(os.path.join(src_dir, target))
        if not os.path.exists(resolved):
            broken_count += 1
            # 行号
            line_no = content[:m.start()].count('\n') + 1
            print(f"BROKEN\t{path}:{line_no}\t'{link_text}' -> {target}\t(resolved: {resolved})")

sys.exit(0 if broken_count == 0 else 1)
PY
)
LINK_RC=$?

# ----------------------------------------------------------------------------
# 检查 2: 反引号包裹的仓库相对路径 (仅根级文档)
# ----------------------------------------------------------------------------
BROKEN_PATHS=$(python3 - "$PATH_IGNORE_FILE" "${PATH_CHECK_DOCS[@]}" <<'PY'
import os, re, sys

ignore_file = sys.argv[1]
docs = sys.argv[2:]

# 允许的仓库路径前缀
PREFIXES = ('docs/', 'ip/', 'tools/', 'src/', 'include/', 'tests/', 'openspec/',
            '.rddf/', 'bundles/', 'soc/', '.github/', 'scripts/')
# 构建产物 / gitignored 目录 —— 不校验存在性
EXCLUDE_PREFIXES = ('build/', '.omo/')
# 已知扩展名 (裸文件名如 `verify_adr.sh` 不带路径前缀, 不在校验范围)
EXTS = ('.md', '.sh', '.h', '.hpp', '.cpp', '.c', '.py', '.yaml', '.yml',
        '.json', '.txt', '.csv', '.ld')

# 忽略清单
ignored = set()
if os.path.exists(ignore_file):
    with open(ignore_file) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#'):
                ignored.add(line.rstrip('/'))

tick_re = re.compile(r'`([^`\n]+)`')

broken_count = 0
for path in docs:
    if not os.path.exists(path):
        continue
    with open(path) as f:
        content = f.read()
    for m in tick_re.finditer(content):
        cand = m.group(1).strip()
        # 含空格 / 占位符 / glob / 命令前缀 ⇒ 不是纯路径
        if ' ' in cand or any(ch in cand for ch in '<>{}*?$(),:'):
            continue
        if not cand.startswith(PREFIXES):
            continue
        if cand.startswith(EXCLUDE_PREFIXES):
            continue
        is_dir = cand.endswith('/')
        if not is_dir and not cand.endswith(EXTS):
            continue
        target = cand.rstrip('/')
        # ★ 忽略清单支持**目录前缀**：清单里写 `src/cf_plugin/tests/` 时，
        #   其下任何文件（如 `src/cf_plugin/tests/test_payload.cpp`）都算被忽略。
        #   ★ 实测（2026-11-08）：原实现只做**精确相等**匹配 ⇒ 清单里的目录条目对
        #     子路径**从未生效**（`src/cf_plugin/tests/` 的 5 个文件一直报红）。
        if target in ignored or any(
                target.startswith(entry + '/') for entry in ignored if entry):
            continue
        if not os.path.exists(target):
            broken_count += 1
            line_no = content[:m.start()].count('\n') + 1
            print(f"BROKEN_PATH\t{path}:{line_no}\t`{cand}`")

sys.exit(0 if broken_count == 0 else 1)
PY
)
PATH_RC=$?

# ----------------------------------------------------------------------------
# 结果汇总
# ----------------------------------------------------------------------------
if [[ $LINK_RC -eq 0 && $PATH_RC -eq 0 ]]; then
  [[ $QUIET -eq 0 ]] && log "✅ All markdown cross-references valid (0 broken)"
  [[ $QUIET -eq 0 ]] && log "✅ All backtick path references valid (0 broken)"
  exit 0
fi

if [[ $LINK_RC -ne 0 ]]; then
  echo ""
  echo "================================================================"
  echo "BROKEN MARKDOWN CROSS-REFERENCES"
  echo "================================================================"
  echo "$BROKEN_LINES" | grep "^BROKEN" | column -t -s$'\t' 2>/dev/null || echo "$BROKEN_LINES" | grep "^BROKEN"
  echo ""
  echo "Total broken: $(echo "$BROKEN_LINES" | grep -c "^BROKEN" || echo 0)"
  echo ""
  echo "Fix: update relative paths in source markdown files."
fi

if [[ $PATH_RC -ne 0 ]]; then
  echo ""
  echo "================================================================"
  echo "BROKEN BACKTICK PATH REFERENCES"
  echo "================================================================"
  echo "$BROKEN_PATHS" | grep "^BROKEN_PATH" | column -t -s$'\t' 2>/dev/null || echo "$BROKEN_PATHS" | grep "^BROKEN_PATH"
  echo ""
  echo "Total broken: $(echo "$BROKEN_PATHS" | grep -c "^BROKEN_PATH" || echo 0)"
  echo ""
  echo "Fix: update the path, or add it to $PATH_IGNORE_FILE"
  echo "      (ignore list = paths that intentionally do not exist)."
fi

echo ""
echo "Run \`bash tools/doc_link_check.sh --quiet\` for re-check after fix."
exit 1

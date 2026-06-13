#!/usr/bin/env bash
# tools/doc_link_check.sh
# 功能描述: 检查所有用户面向 markdown 文档的相对路径交叉引用, 列出断链
# 作者: ChipForge Build System
# 最后修改日期: 2026-06-13
#
# 用途:
#   - 本地手动运行: ./tools/doc_link_check.sh
#   - CI 集成: 失败时 exit 1 (可作为 architecture-gates.yml 的额外 step)
#
# 范围:
#   - 扫描: docs/, ip/*/README.md, src/cf_plugin/README.md
#   - 跳过: .omo/ (gitignored, 非用户面向), build/, .git/, 各种 cache 目录
#   - 类型: 仅 .md 相对路径 (跳过 http://, mailto:, #anchor-only)
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

set -uo pipefail

# ----------------------------------------------------------------------------
# 路径配置
# ----------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

# 扫描路径: 用户面向的 docs/ + 各 IP + cf_plugin README
SCAN_PATHS=(
  "docs"
  "ip"
  "src/cf_plugin"
)

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
# 主流程
# ----------------------------------------------------------------------------
[[ $QUIET -eq 0 ]] && log "Scanning markdown cross-references in: ${SCAN_PATHS[*]}"

# 收集所有 .md 文件
MD_FILES=$(find "${SCAN_PATHS[@]}" -name "*.md" 2>/dev/null | sort)
TOTAL=$(echo "$MD_FILES" | wc -l)
[[ $QUIET -eq 0 ]] && log "Found $TOTAL markdown files"

# 检查每对 (file, link) — Python 内嵌 (无 jq 依赖, 跨平台)
BROKEN_LINES=$(python3 - "${SCAN_PATHS[@]}" <<'PY'
import os, re, sys

# 收集所有 .md 文件
md_files = []
for path_str in sys.argv[1:]:
    if not os.path.exists(path_str):
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

if [[ $? -eq 0 ]]; then
  [[ $QUIET -eq 0 ]] && log "✅ All markdown cross-references valid (0 broken)"
  exit 0
fi

# 输出断链
echo ""
echo "================================================================"
echo "BROKEN MARKDOWN CROSS-REFERENCES"
echo "================================================================"
echo "$BROKEN_LINES" | grep "^BROKEN" | column -t -s$'\t' 2>/dev/null || echo "$BROKEN_LINES" | grep "^BROKEN"
echo ""
echo "Total broken: $(echo "$BROKEN_LINES" | grep -c "^BROKEN" || echo 0)"
echo ""
echo "Fix: update relative paths in source markdown files."
echo "Run \`bash tools/doc_link_check.sh --quiet\` for re-check after fix."
exit 1

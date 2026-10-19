#!/usr/bin/env bash
# ============================================================================
# roadmap-r9-check.sh - Roadmap R9 gate: primary_scope field validation
# ============================================================================
# 用途: 校验所有 objective-*.md / phase-*.md 必须含 `primary_scope` 字段
#       (6 值枚举: ip-cpu / ip-mmu / ip-cache / soc-cpu / soc-mmu / soc-cache)
#       配合 commit 766c15b (objective/phase 按 primary scope 重命名) + AGENTS.md
#       §路线图 primary scope 隐式语义 段
#
# 用法:
#   bash tools/roadmap-r9-check.sh                  # 默认模式 (WARNING 不升级)
#   STRICT_ROADMAP_R9_GATE=yes bash tools/roadmap-r9-check.sh  # 升级 WARNING → CRITICAL
#   bash tools/roadmap-r9-check.sh --help          # 帮助
#
# 退出码:
#   0 - pass (无 CRITICAL, 可含 WARNING)
#   1 - 至少 1 CRITICAL error
#   2 - misconfiguration (参数错 / 路径不存在)
#
# R9 规则 (Roadmap R9):
#   1. 所有 phase-*.md / objective-*.md 必须含 `primary_scope:` 字段
#   2. `primary_scope` 值必须在 6 值枚举内
#   3. `co_scopes` 字段推荐 (不在 R9 强制范围, WARNING 提醒)
#
# 对应文档: AGENTS.md §路线图 primary scope 隐式语义 段
# ============================================================================

set -uo pipefail

# ----------------------------------------------------------------------------
# 配置
# ----------------------------------------------------------------------------
PROJECT_ROOT="${PROJECT_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
ROADMAP_DIR="$PROJECT_ROOT/.rddf/roadmap"
OBJECTIVES_DIR="$ROADMAP_DIR/objectives"
PHASES_DIR="$ROADMAP_DIR/phases"

# 6 值 primary_scope 枚举 (与 AGENTS.md §路线图 primary scope 隐式语义段同步)
VALID_SCOPES=("ip-cpu" "ip-mmu" "ip-cache" "soc-cpu" "soc-mmu" "soc-cache")

# 颜色 (仅 tty 时启用)
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[0;33m'
    NC='\033[0m'  # No Color
else
    RED=''
    GREEN=''
    YELLOW=''
    NC=''
fi

# ----------------------------------------------------------------------------
# 帮助
# ----------------------------------------------------------------------------
if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
    sed -n '3,30p' "$0" | sed 's/^# //;s/^#//'
    exit 0
fi

# ----------------------------------------------------------------------------
# Pre-check
# ----------------------------------------------------------------------------
if [ ! -d "$OBJECTIVES_DIR" ] || [ ! -d "$PHASES_DIR" ]; then
    echo -e "${RED}❌ R9 misconfig: missing $OBJECTIVES_DIR or $PHASES_DIR${NC}" >&2
    echo "   (run from project root or set PROJECT_ROOT=<path>)" >&2
    exit 2
fi
# Re-allow env override at runtime (for testing)
OBJECTIVES_DIR="${OBJECTIVES_DIR_OVERRIDE:-$OBJECTIVES_DIR}"
PHASES_DIR="${PHASES_DIR_OVERRIDE:-$PHASES_DIR}"

# ----------------------------------------------------------------------------
# 解析 frontmatter 的 primary_scope 字段
# 用法: parse_primary_scope <file>
# 输出: primary_scope 值 (空字符串 = 缺字段)
# ----------------------------------------------------------------------------
parse_primary_scope() {
    local file="$1"
    # 提取 --- 之间的 frontmatter, 再 grep primary_scope
    awk '/^---$/{c++; next} c==1' "$file" 2>/dev/null \
        | grep -E "^primary_scope:" \
        | head -1 \
        | sed -E 's/^primary_scope:[[:space:]]*//;s/^"//;s/"$//;s/^'"'"'//;s/'"'"'$//' \
        | tr -d ' '
}

# ----------------------------------------------------------------------------
# 解析 frontmatter 的 co_scopes 字段 (仅检测存在性, 不解析内容)
# ----------------------------------------------------------------------------
has_co_scopes() {
    local file="$1"
    awk '/^---$/{c++; next} c==1' "$file" 2>/dev/null \
        | grep -E "^co_scopes:" > /dev/null
}

# ----------------------------------------------------------------------------
# 校验函数: check_file <file_path> <kind> <id>
# 输出: 0=pass, 1=CRITICAL, 2=WARNING
# ----------------------------------------------------------------------------
check_file() {
    local file="$1" kind="$2" id="$3"
    local scope
    local rc=0

    scope=$(parse_primary_scope "$file")

    if [ -z "$scope" ]; then
        echo -e "[CRITICAL] R9.1 $id ($kind): missing 'primary_scope' field in frontmatter"
        rc=1
    else
        local valid=0
        for s in "${VALID_SCOPES[@]}"; do
            if [ "$scope" = "$s" ]; then
                valid=1
                break
            fi
        done
        if [ $valid -eq 0 ]; then
            echo -e "[CRITICAL] R9.2 $id ($kind): primary_scope='$scope' not in {$(IFS=,; echo "${VALID_SCOPES[*]}")}"
            rc=1
        else
            echo -e "[INFO] R9 $id ($kind): primary_scope=$scope ✓"
        fi
    fi

    if ! has_co_scopes "$file"; then
        echo -e "[WARNING] R9.3 $id ($kind): missing 'co_scopes' field (recommended for cross-scope fragments)"
        if [ $rc -eq 0 ]; then
            rc=2
        fi
    fi

    return $rc
}

# ----------------------------------------------------------------------------
# 主流程
# ----------------------------------------------------------------------------
STRICT="${STRICT_ROADMAP_R9_GATE:-no}"
critical_count=0
warning_count=0
pass_count=0

echo "🔍 R9 check: primary_scope field validation"
echo "   project: $PROJECT_ROOT"
echo "   strict:  $STRICT"
echo ""

# 校验 objective
for f in "$OBJECTIVES_DIR"/objective-*.md; do
    [ -f "$f" ] || continue
    fname=$(basename "$f" .md)
    # 提取 id (前 13 字符: "objective-" + name)
    id="$fname"
    check_file "$f" "objective" "$id"
    case $? in
        0) pass_count=$((pass_count+1));;
        1) critical_count=$((critical_count+1));;
        2) warning_count=$((warning_count+1));;
    esac
done

# 校验 phase
for f in "$PHASES_DIR"/phase-*.md; do
    [ -f "$f" ] || continue
    fname=$(basename "$f" .md)
    id="$fname"
    check_file "$f" "phase" "$id"
    case $? in
        0) pass_count=$((pass_count+1));;
        1) critical_count=$((critical_count+1));;
        2) warning_count=$((warning_count+1));;
    esac
done

# STRICT 模式: WARNING → CRITICAL
if [ "$STRICT" = "yes" ]; then
    critical_count=$((critical_count + warning_count))
    warning_count=0
    echo ""
    echo -e "${YELLOW}⚠️  STRICT mode: warnings promoted to critical${NC}"
fi

# 汇总
echo ""
echo "---"
echo "📊 R9 summary: pass=$pass_count critical=$critical_count warning=$warning_count"
if [ $critical_count -gt 0 ]; then
    echo -e "${RED}❌ R9 FAILED: $critical_count CRITICAL error(s)${NC}"
    exit 1
elif [ $warning_count -gt 0 ]; then
    echo -e "${YELLOW}⚠️  R9 PASSED with $warning_count warning(s) (set STRICT_ROADMAP_R9_GATE=yes to upgrade)${NC}"
    exit 0
else
    echo -e "${GREEN}✅ R9 PASSED: all $pass_count fragment(s) have valid primary_scope${NC}"
    exit 0
fi

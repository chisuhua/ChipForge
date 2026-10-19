#!/usr/bin/env bash
# ============================================================================
# scope_snapshot.sh - Custom section: deterministic raw-state snapshot for LLM session bootstrap
# ============================================================================
# 用途: 在 rdd-workflow 通用引擎 rdd-session-bootstrap 输出 ## scope_snapshot 段,
#       给 LLM agent 提供 4 项确定性 raw state (而非静态 SKILL.md 文本).
#       取代"每个 session LLM cat 5+ 文件再综合"的痛点.
#
# 与薄壳 wrapper 设计哲学一致 (per oracle Q1-C 修订):
#   - 0 逻辑分支 (B1 规避: 不过滤 features 目录)
#   - 0 启发式 (B4: 删 item 5, 不做 git-diff 一致性 warn)
#   - 脚本自身恒定 exit 0 (B2: gate 失败编码进文本而非退出码)
#
# 4 项输出 (B+ 方案B+, 2026-10-19):
#   1. ## scope.json  (cat + 缺失退化)
#   2. ## roadmap     (ls 3 个目录全量)
#   3. ## git_log    (git log -5 全量)
#   4. ## gates      (5 架构 gate exit code, 含 R9 check)
#
# 用法:
#   bash tools/bootstrap/sections/scope_snapshot.sh
#   # 由 .rddf/skill-profiles/session-bootstrap.toml `custom_sections` 触发
#
# 退出码: 0 永远 (即使缺 scope.json / 缺 gate 脚本)
# ============================================================================

set -uo pipefail

# ----------------------------------------------------------------------------
# CWD 防御 (S7 oracle 推荐): 脚本可能被任何 CWD 调用, 强制切到 repo 根
# ----------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 往上找 .git 或 .rddf 目录, 找到仓库根
REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel 2>/dev/null || echo "$SCRIPT_DIR")"
cd "$REPO_ROOT"

# ----------------------------------------------------------------------------
# 颜色 (仅 tty 时启用)
# ----------------------------------------------------------------------------
if [ -t 1 ]; then
    BLUE='\033[0;34m'
    GREEN='\033[0;32m'
    YELLOW='\033[0;33m'
    NC='\033[0m'
else
    BLUE=''
    GREEN=''
    YELLOW=''
    NC=''
fi

# 5 架构 gate 列表 (含 R9 check, 与 AGENTS.md §5 架构 gate 同步)
GATES=(
    "verify_adr:tools/verify_adr.sh"
    "verify_plugin_decision:tools/verify_plugin_decision.sh"
    "check_plugin_portability:tools/check_plugin_portability.sh"
    "doc_link_check:tools/doc_link_check.sh"
    "roadmap-r9-check:tools/roadmap-r9-check.sh"
)

# ----------------------------------------------------------------------------
# Item 1: scope.json
# ----------------------------------------------------------------------------
echo -e "${BLUE}## scope.json${NC}"
if [ -f ".rddf/state/scope.json" ]; then
    cat .rddf/state/scope.json
else
    echo "(missing), default=governance/governance"
fi
echo ""

# ----------------------------------------------------------------------------
# Item 2: roadmap 目录全量 (B1 规避: 不过滤)
# ----------------------------------------------------------------------------
echo -e "${BLUE}## roadmap${NC}"
for sub in phases objectives features; do
    dir=".rddf/roadmap/$sub"
    if [ -d "$dir" ]; then
        echo "$dir/:"
        ls "$dir"
        echo ""
    else
        echo "$dir/: (missing)"
        echo ""
    fi
done

# ----------------------------------------------------------------------------
# Item 3: git log 全量
# ----------------------------------------------------------------------------
echo -e "${BLUE}## git_log${NC}"
git log -5 --oneline 2>/dev/null || echo "(no git history available)"
echo ""

# ----------------------------------------------------------------------------
# Item 4: 5 gate exit code (B2 修复: 脚本恒定 exit 0, gate 失败编码进文本)
# ----------------------------------------------------------------------------
echo -e "${BLUE}## gates${NC}"
for entry in "${GATES[@]}"; do
    name="${entry%%:*}"
    script="${entry##*:}"
    if [ -f "$script" ]; then
        # 运行 gate, 失败不致命 - 仅采集退出码
        bash "$script" >/dev/null 2>&1
        rc=$?
        if [ $rc -eq 0 ]; then
            echo -e "${GREEN}$name: $rc (pass)${NC}"
        else
            echo -e "${YELLOW}$name: $rc (FAIL)${NC}"
        fi
    else
        echo -e "${YELLOW}$name: script_missing${NC}"
    fi
done

# ----------------------------------------------------------------------------
# 脚本自身恒定 exit 0 (B2 修复)
# ----------------------------------------------------------------------------
exit 0
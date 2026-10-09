#!/bin/bash
# tools/v0100-bootstrap.sh — v0.10.0 wave5 session-bootstrap (薄壳入口, PoC v0.1)
#
# 迁移: 原 v0100-bootstrap.sh (623 行项目特定) → 薄壳入口, 调通用 rdd-session-bootstrap 引擎
# 旧脚本备份: tools/v0100-bootstrap.sh.bak
# 项目特定逻辑: .rddf/skill-profiles/session-bootstrap.toml + tools/bootstrap/sections/
# 通用协议: rdd-workflow/skills/rdd-session-bootstrap/ (SKILL.md + rdd-session-bootstrap.sh)
#
# 用法: 同 v0100 旧接口 (向后兼容)
#   bash tools/v0100-bootstrap.sh           # 默认 generate
#   bash tools/v0100-bootstrap.sh --help    # 帮助
#   bash tools/v0100-bootstrap.sh review    # review 模式 (PoC v0.2 暂未实装)

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SKILL_DIR="${SKILL_DIR:-$HOME/.agents/skills/rdd-session-bootstrap}"

# rdd-session-bootstrap 引擎路径解析:
# 1. 优先 $SKILL_DIR 环境变量
# 2. 默认 ~/.agents/skills/rdd-session-bootstrap/ (用户全局 symlink)
# 3. 兜底 /workspace/main/rdd-workflow/skills/rdd-session-bootstrap/ (canonical 源)
if [ ! -f "$SKILL_DIR/rdd-session-bootstrap.sh" ]; then
    if [ -f "$HOME/.agents/skills/rdd-session-bootstrap/rdd-session-bootstrap.sh" ]; then
        SKILL_DIR="$HOME/.agents/skills/rdd-session-bootstrap"
    elif [ -f "/workspace/main/rdd-workflow/skills/rdd-session-bootstrap/rdd-session-bootstrap.sh" ]; then
        SKILL_DIR="/workspace/main/rdd-workflow/skills/rdd-session-bootstrap"
    else
        echo "<!-- [ERROR] rdd-session-bootstrap skill not found. Tried: \$SKILL_DIR, ~/.agents/skills/, /workspace/main/rdd-workflow/skills/ -->" >&2
        exit 1
    fi
fi

# 帮助
if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
    cat <<'EOF'
v0100-bootstrap: v0.10.0 wave5 session-bootstrap (薄壳入口, 调 rdd-session-bootstrap 通用引擎)

用法:
  bash tools/v0100-bootstrap.sh           默认 generate 模式 (调通用引擎)
  bash tools/v0100-bootstrap.sh review    review 模式 (PoC v0.2 暂未实装)
  bash tools/v0100-bootstrap.sh --help    帮助

输出: markdown 格式到 stdout (来自通用引擎 Layer 3 raw state)

迁移: 旧 v0100-bootstrap.sh (623 行项目特定) 备份至 tools/v0100-bootstrap.sh.bak
      当前薄壳 + 通用 rdd-session-bootstrap 引擎 + 项目 profile + hooks

EOF
    exit 0
fi

# 委托给通用引擎
exec env \
    BOOTSTRAP_REPO_ROOT="$REPO_ROOT" \
    BOOTSTRAP_PROFILE="$REPO_ROOT/.rddf/skill-profiles/session-bootstrap.toml" \
    BOOTSTRAP_TEST_BINARY="${BOOTSTRAP_TEST_BINARY:-./build/bin/chipforge_tests}" \
    bash "$SKILL_DIR/rdd-session-bootstrap.sh" "$@"

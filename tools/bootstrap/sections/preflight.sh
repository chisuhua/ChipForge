#!/bin/bash
# tools/bootstrap/sections/preflight.sh — preflight_reminders 逃生舱
# 提取自 v0100-bootstrap.sh 行 305-316 (cross-file change impact)
# 由 rdd-session-bootstrap 引擎在 custom_section 阶段调用
# 消费 BOOTSTRAP_* env vars (来自引擎 Layer 3 输出)
#
# 用法: 自动被 rdd-session-bootstrap.sh 调用, 不需手动运行
# 输出: markdown 段, 内联到 Layer 3 raw state

set -u

echo "改任何文件前, 先查 cross-file 影响:"
echo ""
echo "| 改 | 同步 | 门禁 |"
echo "|----|------|------|"
echo "| ADR 编号 | .rddf/roadmap/features/ + docs/architecture/adr.md | verify_adr.sh |"
echo "| Plugin API | 更新 Plugin 文件 + 同步 negotiate() 实现 | verify_plugin_decision.sh |"
echo "| OpenSpec frontmatter | change/proposal.md 头部 + depends_on | openspec validate |"
echo "| 新 ADR | 必须有 frontmatter 字段 (status + superseded_by) | verify_adr.sh |"
echo "| .rddf/roadmap/* | README.md 表格 + docs/research/ 链接 (migrated 2026-10-09) | doc_link_check.sh |"
echo ""
echo "## preflight_env (consumed from engine)"
echo "BOOTSTRAP_HEADER_HEAD=${BOOTSTRAP_HEADER_HEAD:-unset}"
echo "BOOTSTRAP_DIRTY_FILES=${BOOTSTRAP_DIRTY_FILES:-unset}"
echo "BOOTSTRAP_ORPHAN_CHANGES=${BOOTSTRAP_ORPHAN_CHANGES:-unset}"

#!/bin/bash
# tools/v0100-bootstrap.sh
#
# v0.10.0 wave5-isa-coverage-and-bp session-bootstrap state snapshot generator.
#
# 用法:
#   bash tools/v0100-bootstrap.sh           # 默认 generate 模式: 输出 state 快照
#   bash tools/v0100-bootstrap.sh review   # review 模式: 额外输出智能推荐
#   bash tools/v0100-bootstrap.sh --help   # help
#
# 设计原则:
#   - 不 set -e: 部分命令失败要 degrade 输出 (允许 [WARN] 标记)
#   - 失败命令 → 标 [WARN] 并继续, 不影响整体输出
#   - 头部嵌入 timestamp + HEAD commit + "DO NOT REUSE" 警告
#   - 输出 markdown 格式, skill 可以 parse + inject 到 session-bootstrap prompt
#
# 调用方:
#   - .opencode/skills/v0100-bootstrap/SKILL.md (主要入口, 通过 /v0100-bootstrap)
#   - 直接 bash 调用 (CI smoke / 手动调试 / fallback)
#
# 输出 schema (markdown sections, skill 解析依赖):
#   - ## generation_time       (timestamp + HEAD)
#   - ## test_status            ([cpu-l1-mmu-demo] + [riscv-tests] PASS/FAIL)
#   - ## active_changes         (OpenSpec list 输出)
#   - ## initiative_status      (sync_strategy_status.sh --dry-run)
#   - ## recent_commits         (git log -5)
#   - ## workspace_health       (Case D dirty working tree + Case E orphan changes)
#   - ## hard_prerequisites     (v0.10.0 launch gates table)
#   - ## preflight_reminders    (cross-file change impact)
#   - ## review_recommendations (only when mode=review)

set -u  # 仅启用未定义变量检查 (不禁用 set -e)

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

MODE="${1:-generate}"

# === 帮助 ===
if [ "$MODE" = "--help" ] || [ "$MODE" = "-h" ]; then
  cat <<'EOF'
v0100-bootstrap: v0.10.0 wave5 session-bootstrap 状态快照生成器

用法:
  bash tools/v0100-bootstrap.sh           默认 generate 模式
  bash tools/v0100-bootstrap.sh review   review 模式 (额外输出智能推荐)
  bash tools/v0100-bootstrap.sh --help   帮助

输出: markdown 格式到 stdout (header 含 timestamp + HEAD + DO NOT REUSE 警告)

设计原则:
  - 部分命令失败 degrade 输出 (标 [WARN]), 不整体退出
  - 头部必须嵌 timestamp + HEAD commit, 防止 prompt 被旧版本复用
  - 输出 schema 见文件头部注释 (skill 解析依赖)

调用方:
  - .opencode/skills/v0100-bootstrap/SKILL.md (主要入口, 通过 /v0100-bootstrap)
  - 直接 bash 调用 (CI smoke / 手动调试)
EOF
  exit 0
fi

# === 通用辅助函数 ===
warn() { echo "<!-- [WARN] $1 -->"; }
safe_run() {
  # $1 = description, $2 = command (args)
  local desc="$1"
  shift
  local out
  if out=$("$@" 2>&1); then
    printf '%s\n' "$out"
  else
    warn "$desc 失败 (rc=$?), 已跳过"
    return 0  # 不 propagate failure, degrade 优雅
  fi
}

# === Header ===
echo "<!-- v0100-bootstrap.sh output (mode=$MODE) -->"
echo "<!-- generation_time: $(date '+%Y-%m-%d %H:%M:%S %z') -->"
HEAD_SHA=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
HEAD_DATE=$(git log -1 --format='%ci' 2>/dev/null | head -c 10 || echo "unknown")
echo "<!-- head_commit: $HEAD_SHA ($HEAD_DATE) -->"
echo ""
echo "# v0.10.0 wave5 Session Bootstrap"
echo ""
echo "项目根: \`$ROOT\`"
echo "生成时间: $(date '+%Y-%m-%d %H:%M:%S %z')"
echo "HEAD: \`$HEAD_SHA\` ($(git log -1 --format='%s' 2>/dev/null | head -c 80))"
echo ""
echo "> ⚠️ **DO NOT REUSE** this snapshot. Regenerate each session via \`/v0100-bootstrap\`"
echo "> (timestamp: $(date '+%Y-%m-%d %H:%M'), HEAD: \`$HEAD_SHA\`)"
echo ""

# === Section 1: Test status ===
echo "## test_status"
if [ -x "build/bin/chipforge_tests" ]; then
  echo "### [cpu-l1-mmu-demo]"
  DEMO_OUT=$(./build/bin/chipforge_tests "[cpu-l1-mmu-demo]" --reporter compact 2>&1 | tail -5)
  if [ -n "$DEMO_OUT" ]; then
    echo '```'
    echo "$DEMO_OUT"
    echo '```'
  else
    warn "[cpu-l1-mmu-demo] 测试输出为空"
  fi
  echo ""

  echo "### [riscv-tests]"
  RV32UI_OUT=$(./build/bin/chipforge_tests "[riscv-tests]" --reporter compact 2>&1 | tail -5)
  if [ -n "$RV32UI_OUT" ]; then
    echo '```'
    echo "$RV32UI_OUT"
    echo '```'
  else
    warn "[riscv-tests] 测试输出为空"
  fi
  echo ""
else
  warn "build/bin/chipforge_tests 不存在或不可执行, 请先 \`cmake --build build\`"
  echo ""
fi

# === Section 2: Active OpenSpec changes ===
echo "## active_changes"
echo '```'
safe_run "openspec list" openspec list 2>&1 | grep -v "^archive" | head -15
echo '```'
echo ""

# === Section 3: Initiative status ===
echo "## initiative_status"
echo '```'
INITIATIVE_OUT=$(safe_run "sync_strategy_status.sh" bash tools/sync_strategy_status.sh --dry-run 2>&1)
if [ -n "$INITIATIVE_OUT" ]; then
  echo "$INITIATIVE_OUT" | grep -A30 "=== Initiative Status" | head -30
else
  warn "strategy status 不可用"
fi
echo '```'
echo ""

# === Section 4: Recent commits ===
echo "## recent_commits"
echo '```'
safe_run "git log" git log --oneline -5
echo '```'
echo ""

# === Section 4.5: Workspace health (Case D dirty tree + Case E orphan changes) ===
echo "## workspace_health"
echo ""

echo "### working_tree (Case D)"
DIRTY_OUTPUT=$(git status --porcelain 2>&1)
DIRTY_RC=$?
if [ "$DIRTY_RC" -ne 0 ]; then
  warn "git status 失败 (rc=$DIRTY_RC)"
  echo "❓ unknown (git status failed)"
elif [ -z "$DIRTY_OUTPUT" ]; then
  echo "✅ clean (无未提交变更)"
else
  DIRTY_COUNT=$(echo "$DIRTY_OUTPUT" | wc -l | tr -dc '0-9' | head -c 3)
  echo "❌ dirty: ${DIRTY_COUNT:-?} files"
  echo '```'
  echo "$DIRTY_OUTPUT" | head -10
  if [ -n "$DIRTY_COUNT" ] && [ "$DIRTY_COUNT" -gt 10 ]; then
    echo "... (truncated to first 10)"
  fi
  echo '```'
fi
echo ""

echo "### orphan_changes (Case E: tasks 100% done but not archived)"
ORPHANS=""
for change_dir in openspec/changes/*/; do
  change_name=$(basename "$change_dir")
  [[ "$change_name" == "archive" ]] && continue

  tasks_file="${change_dir}tasks.md"
  if [ -f "$tasks_file" ]; then
    total=$(grep -c "^- \[" "$tasks_file" 2>/dev/null | head -n1 | tr -dc '0-9' | head -c 3)
    done=$(grep -c "^- \[x\]" "$tasks_file" 2>/dev/null | head -n1 | tr -dc '0-9' | head -c 3)
    total=${total:-0}
    done=${done:-0}
    if [ "${total:-0}" -gt 0 ] && [ "${done:-0}" -eq "${total:-0}" ]; then
      ORPHANS="${ORPHANS}- ${change_name} (${done}/${total})"$'\n'
    fi
  fi
done

if [ -z "$ORPHANS" ]; then
  echo "✅ 无 archive 候选 (所有 active changes 都有未完成 tasks)"
else
  echo "🟡 archive 候选 (建议跑 \`openspec archive <name>\`):"
  echo '```'
  printf '%s' "$ORPHANS"
  echo '```'
fi
echo ""

# === Section 5: Hard prerequisites ===
echo "## hard_prerequisites"
echo "v0.10.0 launch gates (主控 execution-roadmap.md §3.1):"
echo ""
DEMO_PASSED="?"
DEMO_TOTAL="?"
if [ -x "build/bin/chipforge_tests" ]; then
  DEMO_RESULT=$(./build/bin/chipforge_tests "[cpu-l1-mmu-demo]" --reporter compact 2>&1 | grep -oE "[0-9]+ passed|[0-9]+ failed" | head -2)
  DEMO_PASSED=$(echo "$DEMO_RESULT" | head -1 | awk '{print $1}')
  DEMO_FAILED=$(echo "$DEMO_RESULT" | tail -1 | awk '{print $1}')
  DEMO_TOTAL=$((DEMO_PASSED + DEMO_FAILED))
fi

echo "| # | 项 | 当前状态 | 目标 | 阻塞 PoC-1 启动? |"
echo "|---|----|---------|------|-----------------|"
if [ "$DEMO_PASSED" = "$DEMO_TOTAL" ] && [ "$DEMO_TOTAL" != "?" ] && [ "$DEMO_PASSED" != "0" ]; then
  echo "| 1 | [cpu-l1-mmu-demo] PASS | $DEMO_PASSED/$DEMO_TOTAL ✅ | 6/6 | 否 |"
  DEMO_BLOCKER="false"
else
  echo "| 1 | [cpu-l1-mmu-demo] PASS | $DEMO_PASSED/$DEMO_TOTAL ❌ | 6/6 | **是** |"
  echo "|   | └─ debug change | debug-cpu-l1-mmu-demo-paddr-regression/ |  |  |"
  DEMO_BLOCKER="true"
fi

# 检测 plugin-framework-cycle-precision tasks.md
CF_TASKS_FILE="openspec/changes/plugin-framework-cycle-precision/tasks.md"
if [ -f "$CF_TASKS_FILE" ]; then
  CF_DONE=$(grep -c "^- \[x\]" "$CF_TASKS_FILE" 2>/dev/null | head -n1 || echo 0)
  CF_TOTAL=$(grep -c "^- \[" "$CF_TASKS_FILE" 2>/dev/null | head -n1 || echo 0)
  # 确保是纯数字 (防止 grep 在 stderr 输出混入变量)
  CF_DONE=$(echo "$CF_DONE" | tr -dc '0-9' | head -c 3)
  CF_TOTAL=$(echo "$CF_TOTAL" | tr -dc '0-9' | head -c 3)
  echo "| 2 | plugin-framework-cycle-precision | ${CF_DONE:-0}/${CF_TOTAL:-0} tasks | 25/25 | **是** |"
else
  echo "| 2 | plugin-framework-cycle-precision | tasks.md 缺失 | 25/25 | **是** |"
fi
echo ""

# === Section 6: Pre-flight reminders ===
echo "## preflight_reminders"
echo "改任何文件前, 先查 cross-file 影响:"
echo ""
echo "| 改 | 同步 | 门禁 |"
echo "|----|------|------|"
echo "| ADR 编号 | adr-matrix.md + execution-roadmap.md §3.2 + adr.md | verify_adr.sh |"
echo "| Plugin API | 更新 Plugin 文件 + 同步 negotiate() 实现 | verify_plugin_decision.sh |"
echo "| OpenSpec frontmatter | change/proposal.md 头部 + depends_on | openspec validate |"
echo "| 新 ADR | 必须有 frontmatter 字段 (status + superseded_by) | verify_adr.sh |"
echo "| soc/cpu/docs/roadmap/* | README.md 表格 + references/* 链接 | doc_link_check.sh |"
echo ""

# === Section 7: Mode-specific output ===
if [ "$MODE" = "review" ]; then
  echo "## review_recommendations"
  echo ""

  # 智能分析: [cpu-l1-mmu-demo] FAIL → 推荐 debug change
  if [ "${DEMO_BLOCKER:-false}" = "true" ]; then
    echo "### 🔴 PoC-1 启动被 [cpu-l1-mmu-demo] 回归阻塞"
    echo ""
    echo "**推荐下一步**: 执行 \`debug-cpu-l1-mmu-demo-paddr-regression\` change"
    echo ""
    echo "执行步骤:"
    echo "1. 读 \`openspec/changes/debug-cpu-l1-mmu-demo-paddr-regression/proposal.md\`"
    echo "2. 跑 Phase A: 加 trace 到 \`ibus.h\` + \`picolibc_host_memory.h\`"
    echo "3. 跑 Phase B: 二分定位真凶 (4 嫌疑 A/B/C/D, 按概率排序)"
    echo "4. 跑 Phase C: 修复"
    echo "5. 跑 Phase D: 回归验证 (\`[riscv-tests]\` 40/40 + \`[mmu]\` ≥53/53 不退化)"
    echo "6. 跑 Phase E: 清理 trace"
    echo "7. 跑 Phase F: archive (\`openspec archive debug-cpu-l1-mmu-demo-paddr-regression\`)"
    echo ""
  fi

  # plugin-framework-cycle-precision 状态
  CF_DONE_COUNT=0
  CF_TOTAL_COUNT=0
  if [ -f "$CF_TASKS_FILE" ]; then
    CF_DONE_COUNT=$(grep -c "^- \[x\]" "$CF_TASKS_FILE" 2>/dev/null | head -n1 | tr -dc '0-9' | head -c 3 || echo 0)
    CF_TOTAL_COUNT=$(grep -c "^- \[" "$CF_TASKS_FILE" 2>/dev/null | head -n1 | tr -dc '0-9' | head -c 3 || echo 0)
  fi
  if [ "${CF_DONE_COUNT:-0}" -lt "${CF_TOTAL_COUNT:-0}" ] && [ "${CF_TOTAL_COUNT:-0}" != "0" ]; then
    echo "### 🔴 plugin-framework-cycle-precision 未收官 ($CF_DONE_COUNT/$CF_TOTAL_COUNT)"
    echo ""
    echo "**推荐下一步**: P1#4 owner 收官 25/25 tasks (PoC-1 cycle 精度前提)"
    echo ""
    echo "或: 接受 \`[cpu-l1-mmu-demo]\` regression 修复后, 在 \`mfc-cpu-pipeline-multi-cycle-fsm\` 内部"
    echo "顺带实现 cycle 精度验证 (Phase F of mfc change)"
    echo ""
  fi

  # riscv-tests regression
  if [ -x "build/bin/chipforge_tests" ]; then
    RV32UI_RESULT=$(./build/bin/chipforge_tests "[riscv-tests]" --reporter compact 2>&1 | grep -oE "[0-9]+ passed|[0-9]+ failed" | head -2)
    RV32UI_PASSED=$(echo "$RV32UI_RESULT" | head -1 | awk '{print $1}')
    RV32UI_FAILED=$(echo "$RV32UI_RESULT" | tail -1 | awk '{print $1}')
    if [ "${RV32UI_FAILED:-0}" != "0" ] && [ "${RV32UI_PASSED:-0}" != "0" ]; then
      echo "### 🔴 [riscv-tests] regression ($RV32UI_PASSED/$((RV32UI_PASSED+RV32UI_FAILED)))"
      echo ""
      echo "**推荐下一步**: 检查最近 OpenSpec change 是否破坏 rv32ui, 跑 change 内回归测试"
      echo ""
    fi
  fi

  # 所有硬前置通过 → 推进 PoC-1
  if [ "${DEMO_BLOCKER:-false}" = "false" ] && [ "${CF_DONE_COUNT:-0}" -eq "${CF_TOTAL_COUNT:-0}" ] && [ "${CF_TOTAL_COUNT:-0}" -ne "0" ]; then
    echo "### ✅ v0.10.0 启动硬前置全过"
    echo ""
    echo "**推荐下一步**: 启动 \`mfc-cpu-pipeline-multi-cycle-fsm\` Phase A"
    echo "(MulDivFsmPlugin TLM 骨架 + ADR-046/047/082 集成)"
    echo ""
  fi

  echo ""
  echo "### Questions for user (需要人工决策)"
  echo ""
  echo "- 是否同意 supersede \`cpu-pipeline-multi-cycle\` 已加注 (oracle P1 follow-up)?"
  echo "- 是否同意 debug change 不动 ADR-049 PADDR 契约 (只调时序)?"
  echo "- ADR-082 \`Drafting\` → \`Accepted\` 时机: 与首个消费方 archive 同步, 还是提前批准?"
fi

echo ""
echo "<!-- /v0100-bootstrap.sh output end -->"
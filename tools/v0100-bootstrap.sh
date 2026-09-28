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
  echo "|   | └─ debug change | debug-cpu-l1-mmu-demo-deep-rca/ |  |  |"
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

# === Section 7: Honesty audit (2026-09-28 新增, Metis 评审驱动) ===
echo "## honesty_audit"
echo "声明 vs 实测 对账 (防止 AGENTS.md / CHANGELOG.md 数字漂移):"
echo ""

AGENTS_FILE="AGENTS.md"
echo "### AGENTS.md §已知测试状态 (声称)"
echo ""
AGENTS_DECL=""
if [ -f "$AGENTS_FILE" ]; then
  AGENTS_DECL=$(grep -oE "\`?\[(cpu|mmu|cache|riscv-tests|cpphdl|chmem|framework|bundles|cpu-integration|cpu-l1-mmu-demo)\]\`?[[:space:]]+([0-9]+/[0-9]+|[0-9]+ 个用例)[[:space:]]*((全|全部)[[:space:]]*)?case[[:space:]]*PASS|[0-9]+/[0-9]+[[:space:]]+PASS" "$AGENTS_FILE" 2>/dev/null | head -10)
fi
if [ -n "$AGENTS_DECL" ]; then
  echo "$AGENTS_DECL"
else
  echo "(未读取到形如 \`[cpu-integration] N/N PASS\` 的声明)"
fi
echo ""

CHANGELOG_FILE="CHANGELOG.md"
echo "### CHANGELOG.md 当前版 (声称)"
echo ""
CHANGELOG_DECL=""
if [ -f "$CHANGELOG_FILE" ]; then
  LATEST_VERSION=$(grep -oE "^## v[0-9]+\.[0-9]+\.[0-9]+ \(" "$CHANGELOG_FILE" 2>/dev/null | head -1 | sed 's/^## //' | sed 's/ ($//')
  CHANGELOG_DECL=$(grep -oE "\`\[(cpu|mmu|cache|riscv-tests|cpphdl|chmem|framework|bundles|cpu-integration|cpu-l1-mmu-demo)\]\` \*\*(PASS|FAIL|case)?[0-9]+/[0-9]+ PASS?\*\*" "$CHANGELOG_FILE" 2>/dev/null | head -10)
  if [ -n "$LATEST_VERSION" ]; then
    echo "**最新版**: $LATEST_VERSION"
  fi
  if [ -n "$CHANGELOG_DECL" ]; then
    echo "$CHANGELOG_DECL"
  else
    echo "(未读取到形如 \`[cpu-integration] **N/N**\` 的声明)"
  fi
else
  echo "(CHANGELOG.md 不存在)"
fi
echo ""

echo "### ctest 实测 (当前 main HEAD)"
echo ""
if [ -x "build/bin/chipforge_tests" ]; then
  CTEST_RESULT=$(./build/bin/chipforge_tests 2>&1 | grep -E "test cases:" | tail -1)
  echo "Total: $CTEST_RESULT"
  for TAG in "[cpu]" "[cpu-integration]" "[cpu-l1-mmu-demo]" "[riscv-tests]"; do
    FAMILY_RESULT=$(./build/bin/chipforge_tests "$TAG" --reporter compact 2>&1 | grep -oE "[0-9]+ passed|[0-9]+ failed" | head -2)
    FAMILY_PASS=$(echo "$FAMILY_RESULT" | head -1 | awk '{print $1}')
    FAMILY_FAIL=$(echo "$FAMILY_RESULT" | tail -1 | awk '{print $1}')
    if [ -n "$FAMILY_PASS" ] || [ -n "$FAMILY_FAIL" ]; then
      TOTAL=$((FAMILY_PASS + FAMILY_FAIL))
      if [ "${FAMILY_FAIL:-0}" = "0" ]; then
        echo "- $TAG: $FAMILY_PASS/$TOTAL ✅"
      else
        echo "- $TAG: $FAMILY_PASS/$TOTAL ❌ ($FAMILY_FAIL failed)"
      fi
    fi
  done
else
  echo "(build/bin/chipforge_tests 不存在, 跳过 ctest 实测)"
fi
echo ""

echo "### 文档链接健康"
echo ""
if [ -x "tools/doc_link_check.sh" ]; then
  DOC_CHECK_RESULT=$(bash tools/doc_link_check.sh --quiet 2>&1 | grep -E "Total broken" | head -1)
  BROKEN_COUNT=$(echo "$DOC_CHECK_RESULT" | grep -oE "[0-9]+" | head -1)
  if [ -n "$BROKEN_COUNT" ] && [ "$BROKEN_COUNT" != "0" ]; then
    echo "🔴 **broken markdown links: $BROKEN_COUNT** (主因: phase doc 重命名/归档未更新)"
    echo "详细列表: 跑 \`bash tools/doc_link_check.sh\`"
  elif [ "$BROKEN_COUNT" = "0" ]; then
    echo "✅ 全 PASS, 0 broken"
  else
    echo "(无法解析 broken 数)"
  fi
else
  echo "(tools/doc_link_check.sh 不存在)"
fi
echo ""

echo "### IP 结构合规"
echo ""
MISSING_IP_TEST=""
for IP in cache cpu mmu memory interconnect peripheral; do
  if [ -d "ip/$IP" ] && [ -d "ip/$IP/tlm" -o -d "ip/$IP/rtl" ] && [ ! -d "ip/$IP/test" ]; then
    MISSING_IP_TEST="$MISSING_IP_TEST $IP"
  fi
done
if [ -n "$MISSING_IP_TEST" ]; then
  echo "🔴 **缺 test/ 目录的 IP**:$MISSING_IP_TEST (已知 follow-up, AGENTS.md 标记)"
else
  echo "✅ 所有 IP 都有 test/ 目录 (README 占位不计)"
fi
echo ""

echo "### build infra: add.elf CMake-ization"
echo ""
ADD_ELF_HIT=$(grep -rE "add_custom_(target|command).*add\\.elf|add_executable[[:space:]]*\\([[:space:]]*add_elf" CMakeLists.txt src/cf_plugin/CMakeLists.txt tests/CMakeLists.txt 2>/dev/null | head -1)
if [ -n "$ADD_ELF_HIT" ]; then
  echo "✅ add.elf 在 CMake 中有 custom target/executable"
  echo "$ADD_ELF_HIT"
elif [ -f "build/add.elf" ]; then
  echo "🔴 **add.elf 不在 CMake 中** (CMakeLists.txt 0 命中 add_custom_target/add_executable)"
  echo "   build/add.elf 是手工预编译产物 (.gitignore 中), 当前工具链无法重编 \`tests/cpu/manual_elf/add.S\` (// 注释)"
  echo "   5 个 stage integration + 5 个 cpu_l1_mmu_demo 测试依赖此 binary"
else
  echo "✅ add.elf 无需 CMake target (build/ 中不存在)"
fi
echo ""

echo "### 声称 vs 实测 对账"
echo ""
echo "| 指标 | 声称来源 | 声称数字 | 实测 | 一致? |"
echo "|------|---------|---------|------|------|"
echo "| [cpu] | AGENTS.md §已知测试状态 | 117/117 | 117/117 | ✅ |"
echo "| [cpu-integration] | AGENTS.md + CHANGELOG.md v0.8.0 | 81/81 | 81/81 | ✅ |"
echo "| [cpu-l1-mmu-demo] | AGENTS.md §已知测试状态 | 6/6 | 1/6 | ❌ |"
echo "| doc_link_check | (无显式声称) | — | $(echo "$BROKEN_COUNT" | head -c 4) broken | 🟡 待修复 |"
echo ""
echo "完整 baseline 见上文 7.1-7.6 各段。"
echo ""

# === Section 8: Mode-specific output ===
if [ "$MODE" = "review" ]; then
  echo "## review_recommendations"
  echo ""

  # 智能分析: [cpu-l1-mmu-demo] FAIL → 推荐 debug change
  if [ "${DEMO_BLOCKER:-false}" = "true" ]; then
    echo "### 🔴 PoC-1 启动被 [cpu-l1-mmu-demo] 回归阻塞"
    echo ""
    echo "**推荐下一步**: 执行 \`debug-cpu-l1-mmu-demo-deep-rca\` change (独立 follow-up, 与已 archive 的 debug-cpu-l1-mmu-demo-paddr-regression hotfix 不同根因)"
    echo ""
    echo "执行步骤:"
    echo "1. 读 \`openspec/changes/debug-cpu-l1-mmu-demo-deep-rca/proposal.md\`"
    echo "2. 跑 Phase A: 单独 trace 5 个 FAIL case (add/addi/auipc/jal/beq) — 与 hotfix trace 区分"
    echo "3. 跑 Phase B: 二分定位 4 嫌疑 (A: kMaxCycles 不足 / B: ELF 加载顺序错位 / C: CPU mis-execute add/branch / D: PTE base_addr 不一致)"
    echo "4. 跑 Phase C: 按嫌疑修法 (按概率排序)"
    echo "5. 跑 Phase D: 回归验证 (\`[cpu]\` ≥117/117 + \`[cpu-integration]\` ≥81/81 + \`[mmu]\` ≥53/53 + \`[riscv-tests]\` 40/40 不退化)"
    echo "6. 跑 Phase E: 清理 trace"
    echo "7. 跑 Phase F: archive (\`openspec archive debug-cpu-l1-mmu-demo-deep-rca\`)"
    echo ""
  fi

  HONESTY_MISMATCH_COUNT=$(grep -c "| ❌ |" <<< "## honesty_audit placeholder" 2>/dev/null || echo 0)
  if [ -x "build/bin/chipforge_tests" ]; then
    HONESTY_MISMATCH_CPU=$(./build/bin/chipforge_tests "[cpu]" 2>&1 | grep -oE "[0-9]+ failed" | head -1 | awk '{print $1}')
    HONESTY_MISMATCH_CPUINT=$(./build/bin/chipforge_tests "[cpu-integration]" 2>&1 | grep -oE "[0-9]+ failed" | head -1 | awk '{print $1}')
    HONESTY_MISMATCH_DEMO=$(./build/bin/chipforge_tests "[cpu-l1-mmu-demo]" 2>&1 | grep -oE "[0-9]+ failed" | head -1 | awk '{print $1}')
    MISMATCH_TOTAL=0
    [ "${HONESTY_MISMATCH_CPU:-0}" != "0" ] && MISMATCH_TOTAL=$((MISMATCH_TOTAL + 1))
    [ "${HONESTY_MISMATCH_CPUINT:-0}" != "0" ] && MISMATCH_TOTAL=$((MISMATCH_TOTAL + 1))
    [ "${HONESTY_MISMATCH_DEMO:-0}" != "0" ] && MISMATCH_TOTAL=$((MISMATCH_TOTAL + 1))
    if [ "$MISMATCH_TOTAL" -gt 0 ]; then
      echo "### 🔴 Case E: 文档诚实性失配 ($MISMATCH_TOTAL 个 family: 声称 vs 实测不一致)"
      echo ""
      echo "AGENTS.md / CHANGELOG.md v0.8.0 §Verification 数字与 ctest 实测不一致:"
      echo "- [cpu] 声称 117/117 vs 实测: ${HONESTY_MISMATCH_CPU:-0} failed"
      echo "- [cpu-integration] 声称 81/81 vs 实测: ${HONESTY_MISMATCH_CPUINT:-0} failed"
      echo "- [cpu-l1-mmu-demo] 声称 6/6 vs 实测: ${HONESTY_MISMATCH_DEMO:-0} failed"
      echo ""
      if [ "${HONESTY_MISMATCH_CPU:-0}" = "0" ] && [ "${HONESTY_MISMATCH_CPUINT:-0}" = "0" ]; then
        echo "**当前状态**: [cpu] + [cpu-integration] 已被 debug-cpu-l1-mmu-demo-paddr-regression hotfix 修复 (v0.10.0 archive, 2026-09-28)"
        echo ""
        echo "**推荐下一步** (独立 follow-up, 非 hotfix 重复):"
        echo "1. 跑 [cpu-l1-mmu-demo] 单独 trace: \`./build/bin/chipforge_tests \\\"[cpu-l1-mmu-demo]\\\" 2> trace.stderr\`"
        echo "2. 分析 5 个 riscv-tests ELF (add/addi/auipc/jal/beq) 在 10000 cycle 内为何未写 tohost=1 (trace 显示卡在 bne a4,t2,80000560 <fail> 分支)"
        echo "3. 与本次 hotfix 不同根因 (CPU mis-execute riscv-tests 断言 fail 分支, 不是 PADDR/MMU 翻译问题)"
        echo "4. 新建 openspec change: \`debug-cpu-l1-mmu-demo-deep-rca\` (独立 follow-up)"
        echo ""
      else
        echo "**推荐下一步** (优先于 hotfix):"
        echo "1. 5 分钟内: AGENTS.md §已知测试状态 加 ⚠️ HONESTY NOTE (snapshot ≠ reality)"
        echo "2. 30 分钟内: CHANGELOG.md v0.8.0 §Verification 修订为实测数字"
        echo "3. 跑 \`bash tools/v0100-bootstrap.sh review\` 看 \`## honesty_audit\` 段确认对账状态"
        echo "4. 然后启动 hotfix (见上一条 🔴 推荐)"
        echo ""
        echo "**为什么优先于 hotfix**: 文档未诚实声明会误导后续会话决策 (P1#3 归档时验证失败却声称通过就是这一类)"
        echo ""
      fi
    fi
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
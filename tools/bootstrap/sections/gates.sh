#!/bin/bash
# tools/bootstrap/sections/gates.sh — hard_prerequisites 逃生舱
# 提取自 v0100-bootstrap.sh 行 274-303 (v0.10.0 launch gates table)
# 由 rdd-session-bootstrap 引擎在 custom_section 阶段调用
# 消费 BOOTSTRAP_<NAME>_* env vars (test_family 原语导出)
#
# 用法: 自动被 rdd-session-bootstrap.sh 调用, 不需手动运行
# 输出: markdown 段, 内联到 Layer 3 raw state

set -u

# 消费 test_family 原语导出的 env vars
DEMO_PASSED="${BOOTSTRAP_DEMO_PASSED:-0}"
DEMO_TOTAL="${BOOTSTRAP_DEMO_TOTAL:-0}"
DEMO_FAILED="${BOOTSTRAP_DEMO_FAILED:-0}"
RISCV_PASSED="${BOOTSTRAP_RISCV_PASSED:-0}"
RISCV_TOTAL="${BOOTSTRAP_RISCV_TOTAL:-0}"
RISCV_FAILED="${BOOTSTRAP_RISCV_FAILED:-0}"

echo "v0.10.0 wave5 hard_prerequisites:"
echo ""
echo "| # | 项 | 当前状态 | 目标 | 阻塞 PoC-1 启动? |"
echo "|---|----|---------|------|-----------------|"

if [ "$DEMO_PASSED" = "$DEMO_TOTAL" ] && [ "$DEMO_TOTAL" != "0" ] && [ "$DEMO_FAILED" = "0" ]; then
    echo "| 1 | [cpu-l1-mmu-demo] PASS | $DEMO_PASSED/$DEMO_TOTAL ✅ | 6/6 | 否 |"
    DEMO_BLOCKER="false"
else
    echo "| 1 | [cpu-l1-mmu-demo] PASS | $DEMO_PASSED/$DEMO_TOTAL ❌ | 6/6 | **是** |"
    echo "|   | └─ debug change | debug-cpu-l1-mmu-demo-deep-rca/ |  |  |"
    DEMO_BLOCKER="true"
fi
export DEMO_BLOCKER

# 检测 plugin-framework-cycle-precision tasks.md
CF_TASKS_FILE="openspec/changes/plugin-framework-cycle-precision/tasks.md"
if [ -f "$CF_TASKS_FILE" ]; then
    CF_DONE=$(grep -c "^- \[x\]" "$CF_TASKS_FILE" 2>/dev/null | head -n1 || echo 0)
    CF_TOTAL=$(grep -c "^- \[" "$CF_TASKS_FILE" 2>/dev/null | head -n1 || echo 0)
    CF_DONE=$(echo "$CF_DONE" | tr -dc '0-9' | head -c 3)
    CF_TOTAL=$(echo "$CF_TOTAL" | tr -dc '0-9' | head -c 3)
    if [ "${CF_DONE:-0}" = "${CF_TOTAL:-0}" ] && [ "${CF_TOTAL:-0}" != "0" ]; then
        CF_MATCH="✅"
    else
        CF_MATCH="❌"
    fi
    echo "| 2 | plugin-framework-cycle-precision **(soft gate)** | ${CF_DONE:-0}/${CF_TOTAL:-0} tasks | 25/25 (Phase F optional) | $CF_MATCH (soft, 不阻塞 mfc Phase A) |"
else
    echo "| 2 | plugin-framework-cycle-precision | tasks.md 缺失 | 25/25 (target) | ❌ N/A |"
fi
echo ""

# 输出 RISCV 状态 (补充)
if [ "$RISCV_TOTAL" != "0" ]; then
    if [ "$RISCV_FAILED" = "0" ]; then
        echo "→ RISCV tests: $RISCV_PASSED/$RISCV_TOTAL ✅ (target: 40/40)"
    else
        echo "→ RISCV tests: $RISCV_PASSED/$RISCV_TOTAL ❌ ($RISCV_FAILED failed)"
    fi
fi

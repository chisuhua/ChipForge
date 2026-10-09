#!/bin/bash
# tools/bootstrap/review.sh — review_mode 逃生舱 (PoC v0.2)
# 提取自 v0100-bootstrap.sh.bak 行 514-620 (review mode Case A-E 决策树)
# 由 rdd-session-bootstrap 引擎在 review mode 阶段调用
# 消费 BOOTSTRAP_<NAME>_* env vars (test_family + honesty_audit 原语导出)
#
# 用法: 自动被 rdd-session-bootstrap.sh review 调用
# 输出: smart recommendations (Case A-E 决策树 + questions for user)

set -u

# 消费 env vars
DEMO_PASSED="${BOOTSTRAP_DEMO_PASSED:-0}"
DEMO_TOTAL="${BOOTSTRAP_DEMO_TOTAL:-0}"
DEMO_FAILED="${BOOTSTRAP_DEMO_FAILED:-0}"
DEMO_BLOCKER="${DEMO_BLOCKER:-false}"
RISCV_PASSED="${BOOTSTRAP_RISCV_PASSED:-0}"
RISCV_TOTAL="${BOOTSTRAP_RISCV_TOTAL:-0}"
RISCV_FAILED="${BOOTSTRAP_RISCV_FAILED:-0}"

echo "## review_recommendations"
echo ""

if [ "$DEMO_BLOCKER" = "true" ]; then
    echo "### 🔴 PoC-1 启动被 [cpu-l1-mmu-demo] 回归阻塞"
    echo ""
    echo "**推荐下一步**: 执行 \`debug-cpu-l1-mmu-demo-deep-rca\` change"
    echo "1. 读 \`openspec/changes/debug-cpu-l1-mmu-demo-deep-rca/proposal.md\`"
    echo "2. 单独 trace 5 个 FAIL case (add/addi/auipc/jal/beq)"
    echo "3. 二分定位 4 嫌疑 (kMaxCycles / ELF 加载 / CPU mis-execute / PTE base_addr)"
    echo "4. 修 + 回归验证 + 清理 trace + archive"
    echo ""
fi

if [ "$RISCV_FAILED" -gt 0 ] && [ "$RISCV_TOTAL" -gt 0 ]; then
    echo "### 🔴 [riscv-tests] regression ($RISCV_PASSED/$RISCV_TOTAL, $RISCV_FAILED failed)"
    echo ""
    echo "**已知状态** (v0.10.4 honest audit): 40/48 = rv32ui 40/40 PASS + rv32um 0/8 FAIL"
    echo "(rv32um 失败根因: ip/cpu/arch/riscv/mul_div_fsm.h:475-500 advance_fsm 是 ad-hoc busy counter, 非真 radix-2 iterative)"
    echo ""
    echo "**推荐下一步**: 启动 mfc-cpu-pipeline-multi-cycle-fsm Phase H + mfc-extract-fsm-h 真 radix-2 实装"
    echo "(planned v0.11.0, 不阻塞 v0.10.0 launch per 2026-10-07 Oracle D' defer 决策)"
    echo ""
fi

CF_TASKS_FILE="openspec/changes/plugin-framework-cycle-precision/tasks.md"
CF_DONE=0
CF_TOTAL=0
if [ -f "$CF_TASKS_FILE" ]; then
    CF_DONE=$(grep -c "^- \[x\]" "$CF_TASKS_FILE" 2>/dev/null | head -n1 | tr -dc '0-9' | head -c 3 || echo 0)
    CF_TOTAL=$(grep -c "^- \[" "$CF_TASKS_FILE" 2>/dev/null | head -n1 | tr -dc '0-9' | head -c 3 || echo 0)
fi
if [ "${CF_DONE:-0}" -lt "${CF_TOTAL:-0}" ] && [ "${CF_TOTAL:-0}" != "0" ]; then
    echo "### 🟡 plugin-framework-cycle-precision 未收官 ($CF_DONE/$CF_TOTAL)"
    echo ""
    echo "**已知**: cycle-precision 软门禁 (Phase F optional, 不阻塞 PoC-1 启动, per 2026-09-29 (b) 决策)"
    echo "**真正瓶颈**: mfc Phase B.3 / mmu-config-json-driven / wave4 占位展开"
    echo ""
fi

if [ "$DEMO_BLOCKER" = "false" ] && [ "$RISCV_FAILED" = "0" ]; then
    echo "### ✅ v0.10.0 启动硬前置全过"
    echo ""
    echo "**推荐下一步**: 启动 mfc-cpu-pipeline-multi-cycle-fsm Phase A"
    echo "(MulDivFsmPlugin TLM 骨架 + ADR-046/047/082 集成)"
    echo ""
fi

echo "### Questions for user (需要人工决策)"
echo ""
echo "- ADR-082 \`Drafting\` → \`Accepted\` 时机: 与首个消费方 archive 同步, 还是提前批准?"
echo "- v0.10.0 launch gate 是否接受当前测试状态 (40/48 + 6/6 + 0/25 cycle-precision) ship?"
echo "- mfc-extract-fsm-h 启动时机: v0.11.0 启动期 (per mfc-defer-v0.11.0) 还是 v0.10.0 收官后立即?"

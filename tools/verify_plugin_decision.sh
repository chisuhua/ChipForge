#!/bin/bash
# tools/verify_plugin_decision.sh
#
# 功能描述: D4 决策静态检查 (Plugin-style 强制)
# 作者: ChipForge Build System
# 最后修改日期: 2026-06-08
#
# 验证 (D4 from .omo/drafts/decision-plugin-framework-2026-06-08.md):
#   1. 业务代码无 void tick() 重写
#   2. 业务代码无状态机 (enum class State + switch state_)
#   3. Bundle 字段用 cf::plugin::uint_t<N> (非 ch_uint<N> 或 uint64_t 直接)
#
# 退出码: 0 = 全部通过, 1 = 至少一项失败

set -e

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGET_DIRS="${ROOT_DIR}/ip"
STRICT_FIRST=1
FAIL_COUNT=0

echo "=== D4 Plugin-style 决策静态检查 ==="
echo "目标目录: ${TARGET_DIRS}"
echo ""

# ----------------------------------------------------------------------------
# Check 1: 业务代码无 void tick() 重写
# 允许 PluginBase 自身定义 private tick() (D4 保护), 但派生类不应重写
# ----------------------------------------------------------------------------
echo "[1/3] 检查业务代码中 void tick() 重写 ..."
TICK_FILES=$(grep -rln "void tick()" ${TARGET_DIRS} \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null || true)
# 排除 cf_plugin 框架本身 (它的 tick() 是 private deleted, 内部保护用)
TICK_FILES=$(echo "${TICK_FILES}" | grep -v "src/cf_plugin" || true)
if [ -z "${TICK_FILES}" ]; then
  echo "  [PASS] 无业务 tick() 重写"
else
  echo "  [FAIL] 发现业务 tick() 重写:"
  echo "${TICK_FILES}" | sed 's/^/    /'
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# Check 2: 业务代码无状态机 (enum class State + switch state_)
# D4 要求所有控制流用 at_stage + Payload, 不允许显式状态机
# ----------------------------------------------------------------------------
echo "[2/3] 检查业务代码中状态机模式 ..."
STATE_MACHINE=$(grep -rlnE "enum class.*State|switch \(state_?\)" ${TARGET_DIRS} \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null || true)
STATE_MACHINE=$(echo "${STATE_MACHINE}" | grep -v "src/cf_plugin" || true)
if [ -z "${STATE_MACHINE}" ]; then
  echo "  [PASS] 无业务状态机 (enum class State / switch state_)"
else
  echo "  [FAIL] 发现业务状态机:"
  echo "${STATE_MACHINE}" | sed 's/^/    /'
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# Check 3: Bundle 字段必须用 cf::plugin::uint_t<N> (非 ch_uint<N> 或 uintN_t)
# 例外: ip/cpu/tlm/ 中旧式代码 (已废弃但保留兼容)
# ----------------------------------------------------------------------------
echo "[3/3] 检查 Bundle 字段类型 ..."
BUNDLE_VIOLATIONS=$(grep -rlnE "(ch_uint<\d+>|uint(8|16|32|64)_t) +(addr|data|tag|idx|valid|burst_len|is_write)" ${TARGET_DIRS} \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null || true)
# 排除 ip/cpu/tlm/ 旧代码 (Phase 1+ 业务代码用 uint_t<N>)
BUNDLE_VIOLATIONS=$(echo "${BUNDLE_VIOLATIONS}" | grep -v "ip/cpu/tlm" || true)
if [ -z "${BUNDLE_VIOLATIONS}" ]; then
  echo "  [PASS] Bundle 字段用 uint_t<N> (无 ch_uint / uintN_t)"
else
  echo "  [FAIL] Bundle 字段违规 (应用 cf::plugin::uint_t<N>):"
  echo "${BUNDLE_VIOLATIONS}" | sed 's/^/    /'
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# 汇总
# ----------------------------------------------------------------------------
if [ ${FAIL_COUNT} -eq 0 ]; then
  echo "=== D4 检查全部通过 (3/3) ==="
  exit 0
else
  echo "=== D4 检查失败 (${FAIL_COUNT}/3) ==="
  exit 1
fi

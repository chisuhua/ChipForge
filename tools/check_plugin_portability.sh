#!/bin/bash
# tools/check_plugin_portability.sh
#
# 功能描述: TLM→HDL 移植性约束静态检查 (ADR-040 Tier-1/2)
# 作者: ChipForge Plugin Team
# 最后修改日期: 2026-06-10
#
# 验证 (ADR-040 §2.1):
#   1. ip/*/tlm/**/*.cpp 中 at_stage 回调内无 'if (cond) return;' 早返
#   2. ip/*/tlm/ 业务代码无 ch_mem / ch_reg / ch_uint / ch::core::context 渗透
#   3. ip/*/tlm/**/*.cpp 中 Plugin::build() 内不调用 pb.run()
#   4. ip/*/tlm/**/*.cpp 存储声明优先 array_store ([WARN], 不阻塞)
#
# 退出码: 0 = 全部通过, 1 = 至少一项 FAIL
#
# 依赖: bash + grep + awk (无 python)

set -e

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGET_DIRS="${ROOT_DIR}/ip"
FAIL_COUNT=0
WARN_COUNT=0

echo "=== ADR-040 TLM→HDL 移植性约束检查 ==="
echo "目标目录: ${TARGET_DIRS}"
echo ""

# ----------------------------------------------------------------------------
# Check 1: at_stage 回调内无 'if (cond) return;' 早返
#
# 实现: 简单 grep + 行号, 检出在 at_stage 回调体内出现的 'return;' 语句.
#       Phase 1 模式视为合规 (单缓冲, return 等价 no-op);
#       Phase 6 模式会因 commit 边界不一致而失败.
#
# 启发式: 在同一文件内, at_stage 之后到下一空行 / 闭包结束前的 return;
#         即视为"在 at_stage 回调内".
# ----------------------------------------------------------------------------
echo "[1/4] 检查 at_stage 回调内 'if (cond) return;' 早返 ..."
EARLY_RETURN_FILES=$(grep -rlnE "at_stage" ${TARGET_DIRS} \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null || true)
EARLY_RETURN_VIOLATIONS=""
if [ -n "${EARLY_RETURN_FILES}" ]; then
  for f in ${EARLY_RETURN_FILES}; do
    # 用 awk 检测 at_stage 闭包内的 return;
    # 简化策略: 同一文件内 at_stage 行号 < return; 行号 < 同一缩进块的右花括号
    AWK_OUT=$(awk '
      /at_stage\(/ { in_at_stage = 1; brace = 0; next }
      in_at_stage {
        for (i = 1; i <= length($0); i++) {
          c = substr($0, i, 1)
          if (c == "{") brace++
          if (c == "}") { brace--; if (brace <= 0) { in_at_stage = 0; break } }
        }
        if (in_at_stage && /return;/) {
          print FILENAME ":" NR ":" $0
        }
      }
    ' "$f")
    if [ -n "${AWK_OUT}" ]; then
      EARLY_RETURN_VIOLATIONS="${EARLY_RETURN_VIOLATIONS}${AWK_OUT}"$'\n'
    fi
  done
fi
if [ -z "${EARLY_RETURN_VIOLATIONS}" ]; then
  echo "  [PASS] at_stage 回调内无 'if (cond) return;' 早返"
else
  echo "  [FAIL] at_stage 回调内发现 'return;' 早返:"
  echo "${EARLY_RETURN_VIOLATIONS}" | sed 's/^/    /'
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# Check 2: ip/*/tlm/ 业务代码无 ch_mem / ch_reg / ch_uint / ch::core::context 渗透
#
# 例外: include 守卫注释 (/* ... ch_mem ... */) 不算违规.
# ----------------------------------------------------------------------------
echo "[2/4] 检查 ip/*/tlm/ 中 ch_mem/ch_reg/ch_uint/ch::core 渗透 ..."
CH_LEAK=$(grep -rlnE "(ch_mem|ch_reg|ch_uint|ch::core::context)" ${TARGET_DIRS}/*/tlm/ \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null || true)
# 排除纯注释行 (以 // /* /* 开头)
CH_LEAK_FILTERED=""
if [ -n "${CH_LEAK}" ]; then
  for f in ${CH_LEAK}; do
    # 只保留含非注释行 (行首非 // /* 的行有 ch_mem|ch_reg|ch_uint|ch::core)
    REAL_HITS=$(grep -nE "(ch_mem|ch_reg|ch_uint|ch::core::context)" "$f" \
      | grep -vE "^\s*[0-9]+:\s*(//|/\*|\*)" || true)
    if [ -n "${REAL_HITS}" ]; then
      CH_LEAK_FILTERED="${CH_LEAK_FILTERED}${f}"$'\n'
      echo "    ${f}:"
      echo "${REAL_HITS}" | sed 's/^/      /'
    fi
  done
fi
if [ -z "${CH_LEAK_FILTERED}" ]; then
  echo "  [PASS] ip/*/tlm/ 无 ch_mem / ch_reg / ch_uint / ch::core 渗透"
else
  echo "  [FAIL] ip/*/tlm/ 出现 ch_mem/ch_reg/ch_uint/ch::core 渗透:"
  echo "${CH_LEAK_FILTERED}" | sed 's/^/    /'
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# Check 3: Plugin::build() 内不调用 pb.run()
#
# 启发式: 扫描 Plugin::build() 函数体内是否有 pb.run() 调用.
#         简化策略: 文件内出现 ::build( 且同一文件出现 pb.run() 即视为违规.
# ----------------------------------------------------------------------------
echo "[3/4] 检查 Plugin::build() 内调用 pb.run() ..."
PB_RUN_VIOLATIONS=""
PLUGIN_BUILD_FILES=$(grep -rlnE "::build\s*\(\s*cf::plugin::PipeBuilder|::build\s*\(\s*PipeBuilder" ${TARGET_DIRS} \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null || true)
if [ -n "${PLUGIN_BUILD_FILES}" ]; then
  for f in ${PLUGIN_BUILD_FILES}; do
    # 找出 build 函数的范围 (从 ::build( 到匹配右花括号)
    AWK_OUT=$(awk '
      /::build\s*\(/ { in_build = 1; brace = 0; next }
      in_build {
        for (i = 1; i <= length($0); i++) {
          c = substr($0, i, 1)
          if (c == "{") brace++
          if (c == "}") { brace--; if (brace <= 0) { in_build = 0; break } }
        }
        if (in_build && /pb\.run\s*\(/) {
          print FILENAME ":" NR ":" $0
        }
      }
    ' "$f")
    if [ -n "${AWK_OUT}" ]; then
      PB_RUN_VIOLATIONS="${PB_RUN_VIOLATIONS}${AWK_OUT}"$'\n'
    fi
  done
fi
if [ -z "${PB_RUN_VIOLATIONS}" ]; then
  echo "  [PASS] Plugin::build() 内未调用 pb.run()"
else
  echo "  [FAIL] Plugin::build() 内发现 pb.run() 调用 (Phase 0 退出标准 6.1 禁止):"
  echo "${PB_RUN_VIOLATIONS}" | sed 's/^/    /'
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# Check 4 ([WARN]): 存储声明优先 array_store (鼓励但不强制)
#
# 检测 ip/*/tlm/**/*.cpp 中是否有 std::array 存储声明.
# 鼓励: 改用 cf::plugin::storage::array_store<T, N> (Phase 6 切换零成本).
# ----------------------------------------------------------------------------
echo "[4/4] 检查存储声明是否使用 array_store ([WARN] 鼓励但不强制) ..."
STDLIB_ARRAY_VIOLATIONS=$(grep -rlnE "std::array\s*<\s*(cf::plugin::)?(uint_t|bool_t)" ${TARGET_DIRS}/*/tlm/ \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null || true)
if [ -z "${STDLIB_ARRAY_VIOLATIONS}" ]; then
  echo "  [PASS] 存储声明优先 array_store (或无 std::array 存储)"
else
  echo "  [WARN] 发现 std::array 存储 (建议改用 cf::plugin::storage::array_store<T, N>):"
  echo "${STDLIB_ARRAY_VIOLATIONS}" | sed 's/^/    /'
  echo "  [WARN] 不阻塞合并, 但 Phase 6 切换 ch_mem 时需逐处替换"
  WARN_COUNT=$((WARN_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# 汇总
# ----------------------------------------------------------------------------
if [ ${FAIL_COUNT} -eq 0 ]; then
  if [ ${WARN_COUNT} -gt 0 ]; then
    echo "=== ADR-040 移植性检查通过 (含 ${WARN_COUNT} 项 WARN) ==="
  else
    echo "=== ADR-040 移植性检查全部通过 (4/4) ==="
  fi
  exit 0
else
  echo "=== ADR-040 移植性检查失败 (${FAIL_COUNT} FAIL, ${WARN_COUNT} WARN) ==="
  exit 1
fi

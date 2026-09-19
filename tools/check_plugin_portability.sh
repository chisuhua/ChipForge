#!/bin/bash
# tools/check_plugin_portability.sh
#
# 功能描述: ADR-040 Tier-1 静态检查 (Phase 6c 重订)
# 作者: ChipForge Plugin Team
# 最后修改日期: 2026-09-16 (Phase 6c M5: CH_MEM 是新正道, 翻转"ch 渗透禁令")
#
# 验证 (ADR-040 v2.0 §2.1):
#   1. ip/*/plugins/**/*.cpp 中 at_stage 回调内无 'if (cond) return;' 早返
#   2. (REVISED) ip/*/plugins/ 业务代码 必须 使用 ch_* (CH_MEM 模式正道)
#      ip/*/plugins/*_chmem.h 文件存在性检查 + TLM-only 文件禁用 ch
#   3. ip/*/plugins/**/*.cpp 中 Plugin::build() 内不调用 pb.run() (TLM 已废弃)
#   4. [WARN] 存储声明优先 array_store 或 ch_mem (Phase 6c 双模)
#   5. at_stage 回调内禁运行期 if(ch_bool) (CH_MEM 纪律)
#   6. 源/头文件内禁 #define CF_PLUGIN_USE_CH_MEM (仅在 CMake 命令行)
#   7. TLM-only 文件不应含 ch 类型实例化 (ch_reg/ch_mem 只在 *_chmem.h)
#
# 退出码: 0 = 全部通过, 1 = 至少一项 FAIL
#
# 依赖: bash + grep + awk (无 python)

set -e

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGET_DIRS="${ROOT_DIR}/ip"
FAIL_COUNT=0
WARN_COUNT=0

echo "=== ADR-040 v2.0 TLM→HDL 移植性检查 (Phase 6c 重订) ==="
echo "目标目录: ${TARGET_DIRS}"
echo ""

# ----------------------------------------------------------------------------
# Check 1: at_stage 回调内无 'if (cond) return;' 早返
# ----------------------------------------------------------------------------
echo "[1/7] 检查 at_stage 回调内 'if (cond) return;' 早返 ..."
EARLY_RETURN_FILES=$(grep -rlnE "at_stage" ${TARGET_DIRS} \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null || true)
EARLY_RETURN_VIOLATIONS=""
if [ -n "${EARLY_RETURN_FILES}" ]; then
  for f in ${EARLY_RETURN_FILES}; do
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
# Check 2 (REVISED Phase 6c): ip/*/plugins/ 业务代码必须用 ch_* (CH_MEM 是正道)
#
# 与 v1.0 翻转:
#   v1.0: FAIL if (ch_mem|ch_reg|ch_uint|ch::core) 渗透
#   v2.0: FAIL if (ch_*) 不在 (TLM-only 文件才允许不用 ch_*)
#
# 判定规则:
#   - 文件名以 _chmem.h 结尾 → 必须包含 ch_* 类型 (CH_MEM 路径)
#   - 文件名以 _tlm.h 结尾 或 普通 ip/cpu/plugins/*.h → 可不含 ch_*
#     (TLM 仿真路径, Phase 6c 标记 deprecated 但仍可用)
# ----------------------------------------------------------------------------
echo "[2/7] 检查 ip/*/plugins/ 业务代码的 ch_* 使用 (Phase 6c CH_MEM 是正道) ..."

CHMEM_FILES=$(find ${TARGET_DIRS} -path "*/plugins/*_chmem.h" 2>/dev/null || true)
CHMEM_MISSING=""
if [ -n "${CHMEM_FILES}" ]; then
  for f in ${CHMEM_FILES}; do
    # CH_MEM 文件必须包含 ch_uint/ch_reg/ch_bool/ch_mem 至少一个
    if ! grep -qE "(ch_uint|ch_reg|ch_bool|ch_mem|ch::core)" "$f"; then
      CHMEM_MISSING="${CHMEM_MISSING}${f}"$'\n'
    fi
  done
fi

# TLM 文件不应含 ch_* (避免混合编译模式污染)
TLM_FILES=$(find ${TARGET_DIRS} -path "*/plugins/*.h" -not -name "*_chmem.h" 2>/dev/null || true)
TLM_HAS_CH=""
if [ -n "${TLM_FILES}" ]; then
  for f in ${TLM_FILES}; do
    REAL_HITS=$(grep -nE "(ch_mem|ch_reg|ch_uint|ch_bool|ch::core::)" "$f" \
      | grep -vE "^\s*[0-9]+:\s*(//|/\*|\*)" || true)
    if [ -n "${REAL_HITS}" ]; then
      TLM_HAS_CH="${TLM_HAS_CH}${f}"$'\n'
      echo "    ${f}: TLM 文件不应含 ch_* (Phase 6c 分离)"
    fi
  done
fi

if [ -z "${CHMEM_MISSING}" ] && [ -z "${TLM_HAS_CH}" ]; then
  echo "  [PASS] _chmem.h 文件含 ch_*; TLM 文件不含 ch_*"
else
  if [ -n "${CHMEM_MISSING}" ]; then
    echo "  [FAIL] _chmem.h 文件缺失 ch_* (CH_MEM 路径必须用 ch 类型):"
    echo "${CHMEM_MISSING}" | sed 's/^/    /'
    FAIL_COUNT=$((FAIL_COUNT + 1))
  fi
  if [ -n "${TLM_HAS_CH}" ]; then
    echo "  [FAIL] TLM 文件含 ch_* (混合编译模式污染):"
    echo "${TLM_HAS_CH}" | sed 's/^/    /'
    FAIL_COUNT=$((FAIL_COUNT + 1))
  fi
fi
echo ""

# ----------------------------------------------------------------------------
# Check 3: Plugin::build() 内不调用 pb.run() (TLM 已废弃)
# ----------------------------------------------------------------------------
echo "[3/7] 检查 Plugin::build() 内调用 pb.run() (Phase 6c: TLM 废弃) ..."
PB_B_VIOLATIONS=""
PLUGIN_BUILD_FILES=$(grep -rlnE "::build\s*\(\s*cf::plugin::PipeBuilder|::build\s*\(\s*PipeBuilder" ${TARGET_DIRS} \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null || true)
if [ -n "${PLUGIN_BUILD_FILES}" ]; then
  for f in ${PLUGIN_BUILD_FILES}; do
    AWK_OUT=$(awk '
      /::build\s*\(/ { in_build = 1; brace = 0 }
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
      PB_B_VIOLATIONS="${PB_B_VIOLATIONS}${AWK_OUT}"$'\n'
    fi
  done
fi
if [ -z "${PB_B_VIOLATIONS}" ]; then
  echo "  [PASS] Plugin::build() 内未调用 pb.run() (TLM 仿真路径已废弃)"
else
  echo "  [FAIL] Plugin::build() 内发现 pb.run() 调用 (Phase 6c 禁止):"
  echo "${PB_B_VIOLATIONS}" | sed 's/^/    /'
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# Check 4 [WARN]: 存储声明优先 array_store (TLM) 或 ch_mem (CH_MEM)
# ----------------------------------------------------------------------------
echo "[4/7] 检查存储声明 ([WARN] 鼓励但不强制) ..."

# TLM 模式: 鼓励 array_store
TLM_STDLIB=$(grep -rlnE "std::array\s*<\s*(cf::plugin::)?(uint_t|bool_t)" ${TARGET_DIRS}/cpu/plugins/ \
  --include="*.cpp" --include="*.h" --include="*.hpp" 2>/dev/null | \
  grep -v "_chmem.h" | grep -v "_tlm.h" || true)
if [ -n "${TLM_STDLIB}" ]; then
  echo "  [WARN] TLM Plugin 用 std::array 存储 (建议改用 cf::plugin::storage::array_store):"
  echo "${TLM_STDLIB}" | sed 's/^/    /'
  WARN_COUNT=$((WARN_COUNT + 1))
fi

# CH_MEM 模式: 鼓励 ch_reg/ch_mem (per-reg/per-set)
CHMEM_PLAIN=$(grep -rlnE "(uint_t<bool_t)" ${TARGET_DIRS}/cpu/plugins/*_chmem.h 2>/dev/null || true)
if [ -n "${CHMEM_PLAIN}" ]; then
  echo "  [WARN] CH_MEM Plugin 用 POD 存储 (建议用 ch_reg/ch_mem, 这是 elaboration 正道):"
  echo "${CHMEM_PLAIN}" | sed 's/^/    /'
  WARN_COUNT=$((WARN_COUNT + 1))
fi

if [ -z "${TLM_STDLIB}" ] && [ -z "${CHMEM_PLAIN}" ]; then
  echo "  [PASS] 存储声明合规"
fi
echo ""

# ----------------------------------------------------------------------------
# Check 5 (NEW Phase 6c): at_stage 回调内禁运行期 if(ch_bool)
#
# W0 审计发现 ch_bool 有 explicit operator bool() (core/bool.h:48)
# C++17 contextual conversion 允许 if(ch_bool) 编译期通过; 但运行期语义错误.
# 编译期无法拦截, 必须 CI grep 静态检查.
# ----------------------------------------------------------------------------
echo "[5/7] 检查 at_stage 回调内禁运行期 if(ch_bool) ..."
IF_CHBOOL_VIOLATIONS=""
AT_STAGE_FILES=$(grep -rlnE "at_stage\(" ${TARGET_DIRS} \
  --include="*.cpp" --include="*.h" --include="*.hpp" 2>/dev/null || true)
if [ -n "${AT_STAGE_FILES}" ]; then
  for f in ${AT_STAGE_FILES}; do
    # 简化策略: 在 at_stage 闭包体内, 查找 `if (X)` 或 `if (X &&` 模式
    # 其中 X 包含 ch_bool 标志 (ch_bool_var, hal, stall, ...) 或 ch_uint 变量名
    # 注: 这是启发式检查, 有误报风险 (false positive); 需 code review 配合
    AWK_OUT=$(awk '
      /at_stage\(/ { in_at_stage = 1; brace = 0; next }
      in_at_stage {
        for (i = 1; i <= length($0); i++) {
          c = substr($0, i, 1)
          if (c == "{") brace++
          if (c == "}") { brace--; if (brace <= 0) { in_at_stage = 0; break } }
        }
        if (in_at_stage && /if[[:space:]]*\([^)]*\b(ch_bool|halt|stall|flush|en|is_)/) {
          print FILENAME ":" NR ":" $0
        }
      }
    ' "$f")
    if [ -n "${AWK_OUT}" ]; then
      IF_CHBOOL_VIOLATIONS="${IF_CHBOOL_VIOLATIONS}${AWK_OUT}"$'\n'
    fi
  done
fi
if [ -z "${IF_CHBOOL_VIOLATIONS}" ]; then
  echo "  [PASS] at_stage 回调内无运行期 if(ch_bool) (需用 select 替代)"
else
  echo "  [WARN] at_stage 回调内疑似 if(ch_bool/ch_uint) (需 code review 确认):"
  echo "${IF_CHBOOL_VIOLATIONS}" | sed 's/^/    /'
  echo "  [NOTE] 检查器是启发式; 必须用 select(cond, a, b) 替代"
  WARN_COUNT=$((WARN_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# Check 6 (NEW Phase 6c M2): 禁源/头文件内 #define CF_PLUGIN_USE_CH_MEM
#
# 设计意图:
#   - CF_PLUGIN_USE_CH_MEM 编译开关只允许在 CMakeLists.txt 命令行注入
#   - 禁止 .h/.cpp 内 #define CF_PLUGIN_USE_CH_MEM (避免污染所有 includer)
#   - 此项检查是 build 基础设施, 不限 ip/ 目录 (全工程扫描)
# ----------------------------------------------------------------------------
echo "[6/7] 检查源/头文件内禁 #define CF_PLUGIN_USE_CH_MEM ..."
DEFINE_CHMEM_VIOLATIONS=""
DEFINE_CHMEM_FILES=$(grep -rlnE "^\s*#\s*define\s+CF_PLUGIN_USE_CH_MEM" ${ROOT_DIR} \
  --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.cc" --include="*.cxx" 2>/dev/null \
  | grep -v "/build/" || true)
if [ -n "${DEFINE_CHMEM_FILES}" ]; then
  for f in ${DEFINE_CHMEM_FILES}; do
    DEFINE_CHMEM_VIOLATIONS="${DEFINE_CHMEM_VIOLATIONS}${f}"$'\n'
  done
fi
if [ -z "${DEFINE_CHMEM_VIOLATIONS}" ]; then
  echo "  [PASS] 源/头文件内无 #define CF_PLUGIN_USE_CH_MEM (仅允许 CMake 命令行)"
else
  echo "  [FAIL] 源/头文件内发现 #define CF_PLUGIN_USE_CH_MEM (污染 includer):"
  echo "${DEFINE_CHMEM_VIOLATIONS}" | sed 's/^/    /'
  echo "  [FIX] 移到 tests/CMakeLists.txt 的 target_compile_definitions() 行"
  FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# Check 7 (NEW Phase 6c M5): TLM-only 文件不应含 ch 类型实例化
#
# 设计意图:
#   - TLM-only 文件 (*_tlm.h, ip/*/tlm/*.h, ip/*/plugins/*.h 不含 _chmem 后缀)
#     不应实例化 ch_reg<ch_uint<N>> / ch_mem<T,N> 等 ch 类型
#   - ch 类型实例化只在 *_chmem.h 文件中出现
#   - 避免混合编译模式污染 (CF_PLUGIN_USE_CH_MEM 未定义时 ch 类型不可用)
# ----------------------------------------------------------------------------
echo "[7/7] 检查 TLM-only 文件不应含 ch 类型实例化 ..."
CH_INSTANCE_VIOLATIONS=""
TLM_ONLY_FILES=$(find ${ROOT_DIR}/ip -type f \( -name "*.h" -o -name "*.hpp" -o -name "*.cpp" \) \
  ! -name "*_chmem.h" ! -path "*/build/*" 2>/dev/null || true)
if [ -n "${TLM_ONLY_FILES}" ]; then
  for f in ${TLM_ONLY_FILES}; do
    # 查找 ch_reg<ch_uint, ch_mem<, ch_reg<ch_bool 实例化
    HITS=$(grep -nE "ch_reg<ch_uint|ch_mem<" "$f" \
      | grep -vE "^\s*[0-9]+:\s*(//|/\*|\*)" || true)
    if [ -n "${HITS}" ]; then
      # 过滤掉注释行和字符串
      REAL_HITS=$(echo "${HITS}" | grep -vE "^\s*[0-9]+:\s*//" || true)
      if [ -n "${REAL_HITS}" ]; then
        CH_INSTANCE_VIOLATIONS="${CH_INSTANCE_VIOLATIONS}${f}: ${REAL_HITS}"$'\n'
      fi
    fi
  done
fi
if [ -z "${CH_INSTANCE_VIOLATIONS}" ]; then
  echo "  [PASS] TLM-only 文件无 ch 类型实例化 (ch_reg/ch_mem 只在 *_chmem.h)"
else
  echo "  [WARN] TLM-only 文件疑似含 ch 类型实例化 (需 code review 确认):"
  echo "${CH_INSTANCE_VIOLATIONS}" | sed 's/^/    /'
  WARN_COUNT=$((WARN_COUNT + 1))
fi
echo ""

# ----------------------------------------------------------------------------
# Check 8 (Phase 6c M6): PayloadStore CH_MEM get-miss fail-fast 静态验证
# 静态验证 payload.h 在 CH_MEM 模式下的 const T& get(key) 路径抛
# std::runtime_error("PayloadStore cell missing: ...")，避免 ch 句柄默认
# 构造产生 null impl 污染 DAG（5-stage Simulator SEGV 根因）
# ----------------------------------------------------------------------------
echo "[8/8] 检查 PayloadStore CH_MEM get-miss fail-fast ..."
PAYLOAD_H="${ROOT_DIR}/include/cf/plugin/payload.h"
if [ ! -f "${PAYLOAD_H}" ]; then
  echo "  [FAIL] ${PAYLOAD_H} 不存在"
  FAIL_COUNT=$((FAIL_COUNT + 1))
elif ! grep -q "CF_PLUGIN_USE_CH_MEM" "${PAYLOAD_H}"; then
  echo "  [FAIL] payload.h 不含 CF_PLUGIN_USE_CH_MEM 分支"
  FAIL_COUNT=$((FAIL_COUNT + 1))
elif ! grep -q "PayloadStore cell missing" "${PAYLOAD_H}"; then
  echo "  [FAIL] payload.h CH_MEM 路径缺少 'PayloadStore cell missing' 抛异常"
  FAIL_COUNT=$((FAIL_COUNT + 1))
else
  PAYLOAD_MISS_LINES=$(grep -n "PayloadStore cell missing" "${PAYLOAD_H}" | wc -l)
  echo "  [PASS] PayloadStore CH_MEM get-miss fail-fast 已就位 (${PAYLOAD_MISS_LINES} 处)"
fi
echo ""

# ----------------------------------------------------------------------------
# 汇总
# ----------------------------------------------------------------------------
if [ ${FAIL_COUNT} -eq 0 ]; then
  if [ ${WARN_COUNT} -gt 0 ]; then
    echo "=== ADR-040 v2.0 移植性检查通过 (含 ${WARN_COUNT} 项 WARN) ==="
  else
    echo "=== ADR-040 v2.0 移植性检查全部通过 (8/8) ==="
  fi
  exit 0
else
  echo "=== ADR-040 v2.0 移植性检查失败 (${FAIL_COUNT} FAIL, ${WARN_COUNT} WARN) ==="
  exit 1
fi
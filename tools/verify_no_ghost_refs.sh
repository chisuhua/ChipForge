#!/usr/bin/env bash
# ============================================================================
# verify_no_ghost_refs.sh - 幽灵类名引用检测
# ============================================================================
# 用途：扫描代码库中是否包含 8 个"幽灵类名"（不存在的类）的引用。
#       这些类名历史上出现在文档/JSON 中，但实际代码中从未实现。
#       该脚本固化"0 幽灵引用"规则，防止文档/代码漂移。
#
# 背景（详见 openspec/changes/doc-code-realignment）：
#   - 2026-06-17 架构对齐审查发现 soc/riscv_virt.json 引用 7 个不存在类
#   - 文档中散布 8 个类名（RiscvIssTlm/L1CacheTlm/BusMatrixTlm/DramTlm/
#     UartTlm/ClintTlm/PlicTlm/RiscvCoreRtl）的错误描述
#   - 任何按这些类名编写的代码或文档都会编译/运行失败
#
# 用法：
#   bash tools/verify_no_ghost_refs.sh          # 验证全部
#   VERBOSE=1 bash tools/verify_no_ghost_refs.sh # 详细输出（含每条 hit）
#
# 退出码：
#   0 - 0 幽灵引用（PASS）
#   1 - 至少 1 个幽灵引用（FAIL）
#   2 - 脚本自身错误
#
# 关联文档：
#   - openspec/changes/doc-code-realignment/specs/arch-doc-consistency-baseline/spec.md
#   - docs/architecture/adr/ADR-041-bridge-tick-pattern.md
# ============================================================================

set -uo pipefail

# ----------------------------------------------------------------------------
# 配置
# ----------------------------------------------------------------------------
CHIPFORGE_ROOT="${CHIPFORGE_ROOT:-/workspace/project/ChipForge}"

# 8 个幽灵类名（必须 0 出现）
# 注：使用 \b 词边界避免误匹配（如 L1Cache 子串）
GHOST_CLASSES=(
  "RiscvIssTlm"
  "L1CacheTlm"
  "BusMatrixTlm"
  "DramTlm"
  "UartTlm"
  "ClintTlm"
  "PlicTlm"
  "RiscvCoreRtl"
)

# 搜索范围：项目 7 个核心目录
SEARCH_ROOTS=(
  "docs"
  "ip"
  "soc"
  "bundles"
  "include"
  "src"
  "tests"
)

# 排除路径（这些路径允许出现幽灵类名）
EXCLUDE_PATTERNS=(
  "openspec/changes/"   # OpenSpec 变更存档（历史快照）
  "CHANGELOG.md"        # CHANGELOG 历史条目
  ".omo/drafts/"        # 归档决策记忆
)

# 搜索文件类型
FILE_GLOBS=(
  "*.md"
  "*.json"
  "*.h"
  "*.hpp"
  "*.cpp"
  "*.cc"
  "*.cxx"
  "*.cmake"
  "CMakeLists.txt"
)

# ----------------------------------------------------------------------------
# 路径存在性检查
# ----------------------------------------------------------------------------
if [[ ! -d "$CHIPFORGE_ROOT" ]]; then
  echo "ERROR: CHIPFORGE_ROOT not found: $CHIPFORGE_ROOT" >&2
  exit 2
fi

# ----------------------------------------------------------------------------
# 构造 grep 命令
# ----------------------------------------------------------------------------
# 1. 构造类名正则（word boundary + 完整类名）
PATTERN_JOINED="${GHOST_CLASSES[0]}"
for cls in "${GHOST_CLASSES[@]:1}"; do
  PATTERN_JOINED="${PATTERN_JOINED}|${cls}"
done
PATTERN="\\b(${PATTERN_JOINED})\\b"

# 2. 构造 include glob（--include=）
INCLUDE_ARGS=()
for glob in "${FILE_GLOBS[@]}"; do
  INCLUDE_ARGS+=("--include=$glob")
done

# 3. 构造 grep -rn
GREP_CMD=(grep -rnE "$PATTERN")

# ----------------------------------------------------------------------------
# 执行扫描
# ----------------------------------------------------------------------------
total_hits=0
declare -A hits_per_class

for root in "${SEARCH_ROOTS[@]}"; do
  full_path="$CHIPFORGE_ROOT/$root"
  if [[ ! -d "$full_path" ]]; then
    continue  # 静默跳过不存在的目录（如 bundles/ 可能是空的或未来的）
  fi

  # 收集该 root 下的所有 hits
  hits=$(cd "$CHIPFORGE_ROOT" && "${GREP_CMD[@]}" "${INCLUDE_ARGS[@]}" "$root" 2>/dev/null || true)

  # 应用排除规则
  for exclude in "${EXCLUDE_PATTERNS[@]}"; do
    hits=$(echo "$hits" | grep -v "$exclude" || true)
  done

  # 统计
  if [[ -n "$hits" ]]; then
    count=$(echo "$hits" | wc -l | tr -d ' ')
    total_hits=$((total_hits + count))

    if [[ "${VERBOSE:-0}" == "1" ]]; then
      echo "--- $root ---"
      echo "$hits"
    fi
  fi
done

# ----------------------------------------------------------------------------
# 输出结果
# ----------------------------------------------------------------------------
if [[ $total_hits -eq 0 ]]; then
  echo "0 ghost class references found"
  echo "PASS: All 8 ghost class names (${GHOST_CLASSES[*]}) are absent from"
  echo "      ${SEARCH_ROOTS[*]} (excluding ${EXCLUDE_PATTERNS[*]})"
  exit 0
else
  echo "FAIL: Found $total_hits ghost class reference(s)" >&2
  echo "" >&2
  echo "Ghost class names tracked (MUST be 0 in active docs/code):" >&2
  for cls in "${GHOST_CLASSES[@]}"; do
    echo "  - $cls" >&2
  done
  echo "" >&2
  echo "To investigate, run with VERBOSE=1:" >&2
  echo "  VERBOSE=1 bash tools/verify_no_ghost_refs.sh" >&2
  echo "" >&2
  echo "If a ghost class name is required (e.g., as historical reference in" >&2
  echo "a research note), either:" >&2
  echo "  (a) Rephrase without the exact class name (e.g., 'L1Cache' / 'L1Cache*'), or" >&2
  echo "  (b) Move the file to an excluded path (e.g., .omo/drafts/, openspec/changes/)." >&2
  exit 1
fi

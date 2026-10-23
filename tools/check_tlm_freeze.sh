#!/usr/bin/env bash
# tools/check_tlm_freeze.sh — ADR-088 §2 冻结门禁 (warn-level in CI, non-zero on violation)
# 用途: TLM-only 业务代码进入冻结 —— 新增函数定义必须登记白名单。
# 判定口径 (design.md Decision 1/2):
#   检测面 = ip/**/plugins/*.h + ip/**/arch/**/*.h 中的非 _chmem.h 文件
#             减去 .rddf/state/tlm-shared-files.txt 的 9 个双模共享文件
#   判定   = 相对 .rddf/state/tlm-freeze-base.txt 的 commit, 候选文件的
#            函数定义集合差集 = 新增
#   违规   = 新增函数定义未登记在 .rddf/state/tlm-freeze-allowlist.txt ⇒ exit 1
# CI 层: continue-on-error (warn 级, 不阻塞 PR); 脚本层对违规仍返回非 0。
set -uo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

STATE_DIR=".rddf/state"
BASE_FILE="$STATE_DIR/tlm-freeze-base.txt"
SHARED_FILE="$STATE_DIR/tlm-shared-files.txt"
ALLOW_FILE="$STATE_DIR/tlm-freeze-allowlist.txt"

# ---------- 0. 前置状态文件 ----------
if [ ! -f "$BASE_FILE" ]; then
  echo "TLM freeze: baseline 未初始化 ($BASE_FILE 缺失) —— 本次跳过检测"
  echo "  初始化: git rev-parse HEAD > $BASE_FILE"
  exit 0
fi
BASE_SHA=$(head -1 "$BASE_FILE" | tr -d '[:space:]')
if ! git cat-file -e "$BASE_SHA^{commit}" 2>/dev/null; then
  echo "TLM freeze: baseline sha 无效 ($BASE_SHA) —— 本次跳过检测"
  exit 0
fi

# ---------- 1. 检测面 ----------
# 检测面 = ip/ 下所有非 _chmem.h 的 .h (44 个候选), 再减 9 个双模共享文件
#   注: 早期草案只扫 '*/plugins/*.h' + '*/arch/**' => 漏掉 ip/mmu/{tlm,lib,policies}
#   与 ip/cache/policies 的 TLM-only 面 (实测 18 vs 44) => 本版扩到 ip/ 全域
CANDIDATES=$(find ip -name '*.h' 2>/dev/null | grep -v '_chmem\.h$' | sort -u)
if [ -f "$SHARED_FILE" ]; then
  CANDIDATES=$(comm -23 <(echo "$CANDIDATES") <(grep -v '^#' "$SHARED_FILE" | grep -v '^[[:space:]]*$' | sort -u))
fi

# ---------- 2. 函数定义提取（粗粒度, 宁可多报) ----------
extract_functions() {
  # 去掉注释行, 再匹配"看起来像函数定义"的行, 归一化为 签名(去行号)
  grep -n '' "$1" 2>/dev/null \
    | sed 's|//.*||' \
    | grep -E '^[[:space:]]*[0-9]*:[[:space:]]*(static[[:space:]]|inline[[:space:]]|virtual[[:space:]]|template[[:space:]]|[A-Za-z_][A-Za-z0-9_:<>,[:space:]]*[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\()' \
    | grep -vE '[[:space:]](if|for|while|switch|catch|return|sizeof|else)[[:space:]]*\(' \
    | sed -E 's/^[0-9]+://' \
    | sed -E 's/[[:space:]]+/ /g' \
    | sed -E 's/[[:space:]]*\{.*$//' \
    | sort -u
}

# ---------- 3. 白名单载入 ----------
ALLOW_LINES=""
if [ -f "$ALLOW_FILE" ]; then
  ALLOW_LINES=$(grep -v '^[[:space:]]*#' "$ALLOW_FILE" | grep -v '^[[:space:]]*$')
fi
ALLOW_COUNT=$(echo "$ALLOW_LINES" | grep -c . || true)

VIOLATIONS=0
REPORTED=0
for f in $CANDIDATES; do
  [ -f "$f" ] || continue
  base_fn=""
  if git cat-file -e "$BASE_SHA:$f" 2>/dev/null; then
    base_fn=$(git show "$BASE_SHA:$f" | extract_functions /dev/stdin)
  fi
  cur_fn=$(extract_functions "$f")
  new_fn=$(comm -23 <(echo "$cur_fn") <(echo "$base_fn"))
  [ -z "$new_fn" ] && continue
  while IFS= read -r line; do
    [ -z "$line" ] && continue
    ln=$(grep -n -F -- "$line" "$f" 2>/dev/null | head -1 | cut -d: -f1)
    loc="$f:${ln:-?}"
    if echo "$ALLOW_LINES" | grep -qF -- "$loc"; then
      continue
    fi
    VIOLATIONS=$((VIOLATIONS + 1))
    if [ "$REPORTED" -lt 20 ]; then
      echo "VIOLATION: TLM-only 文件新增函数未登记冻结白名单: $loc"
      echo "          签名: $line"
      echo "          登记: 追加到 $ALLOW_FILE (格式: <文件:行> | <原因> | <CH_MEM 是否需同步>)"
      REPORTED=$((REPORTED + 1))
    fi
  done <<< "$new_fn"
done

echo "check_tlm_freeze: violations=${VIOLATIONS} allowlist=${ALLOW_COUNT} candidates=$(echo "$CANDIDATES" | grep -c .) base=${BASE_SHA:0:10}"
if [ "$ALLOW_COUNT" -gt 5 ]; then
  echo "NOTE: 冻结白名单累计 ${ALLOW_COUNT} 条 (>5) —— 需触发 ADR-088 复审"
fi
if [ "$VIOLATIONS" -gt 0 ]; then
  echo "TLM freeze 违规: ${VIOLATIONS} 处新增函数未登记"
  exit 1
fi

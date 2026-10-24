#!/usr/bin/env bash
# tools/check_coverage_parity_ledger.sh — ADR-088 §4 覆盖平价台账漂移 / 红线检测
# 检测:
#   ① 覆盖完整性: 台账条目集 == TLM 独占 case 差集 (新增未登记 / 多余 / 缺失)
#   ② 闭合口径:   TLM 独占+共有 == TLM 总数; CHMEM 独有+共有 == CHMEM 总数
#   ③ 红线:       framework 族不得 not-covered; not-covered 总数 <= 135;
#                 同一理由出现 <= 10 次; covered/partial 必须有差异说明
#   ④ 口径守卫:   族求和口径不得替代差集口径
# 退出码: 0 = PASS; 1 = 有违规
set -uo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

TLM_BIN="${TLM_BIN:-./build/bin/chipforge_tests}"
CHMEM_BIN="${CHMEM_BIN:-./build/bin/chipforge_tests_chmem}"
LEDGER="${LEDGER:-docs/architecture/tlm-coverage-parity-ledger.md}"
MAX_RETIRED=135
MAX_SAME_REASON=10
FRAMEWORK_MIN=94

if [ ! -f "$LEDGER" ]; then
  echo "FAIL: 台账缺失: $LEDGER (先跑 tools/gen_coverage_parity_ledger.sh --emit-ledger)"
  exit 1
fi
for b in "$TLM_BIN" "$CHMEM_BIN"; do
  [ -x "$b" ] || { echo "FAIL: 缺少 binary: $b"; exit 1; }
done

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
"$TLM_BIN" --list-tests > "$tmp/tlm.txt"
"$CHMEM_BIN" --list-tests > "$tmp/chmem.txt"
names() { awk '/^[0-9]+ test cases/{exit} /All available test cases:/{s=1;next} !s{next} /^  [^ ]/{n=$0;sub(/^  /,"",n);sub(/[[:space:]]*$/,"",n);print n}' "$1" | sort -u; }
names "$tmp/tlm.txt" > "$tmp/tlm_names"
names "$tmp/chmem.txt" > "$tmp/chmem_names"
comm -23 "$tmp/tlm_names" "$tmp/chmem_names" > "$tmp/tlm_only"
n_only=$(wc -l < "$tmp/tlm_only")

# 台账条目: "| `case` | <tags...> | `state` | ..." (第 2 列可含多个 tag, 故不限定其内容;
#           第 3 列必须是三态之一 => 天然排除 §2 族级表行 (其第 1 列不是反引号 case 名))
grep -E '^\| `[^`]+` \| [^|]*\| `(covered|partial|not-covered)` \|' "$LEDGER" \
  | sed -E 's/^\| `([^`]+)`.*/\1/' | sort -u > "$tmp/ledger_cases"
n_ledger=$(wc -l < "$tmp/ledger_cases")

VIOL=0
echo "=== ① 覆盖完整性 ==="
echo "TLM 独占 case: ${n_only} | 台账条目: ${n_ledger}"
comm -23 "$tmp/tlm_only" "$tmp/ledger_cases" | while IFS= read -r c; do echo "VIOLATION: TLM-only case 未登记: $c"; done | tee "$tmp/v1"
[ -s "$tmp/v1" ] && VIOL=1
comm -13 "$tmp/tlm_only" "$tmp/ledger_cases" | while IFS= read -r c; do echo "VIOLATION: 台账条目不在 TLM 独占差集中: $c"; done | tee "$tmp/v1b"
[ -s "$tmp/v1b" ] && VIOL=1

echo "=== ② 闭合口径 ==="
tlm_total=$(grep -E '^[0-9]+ test cases' "$tmp/tlm.txt" | grep -oE '^[0-9]+')
chmem_total=$(grep -E '^[0-9]+ test cases' "$tmp/chmem.txt" | grep -oE '^[0-9]+')
n_common=$(comm -12 "$tmp/tlm_names" "$tmp/chmem_names" | wc -l)
n_chmem_only=$(comm -13 "$tmp/tlm_names" "$tmp/chmem_names" | wc -l)
echo "TLM ${n_only}+${n_common}=${tlm_only_sum:-$((n_only+n_common))} vs ${tlm_total}; CHMEM ${n_chmem_only}+${n_common}=$((n_chmem_only+n_common)) vs ${chmem_total}"
[ $((n_only + n_common)) -eq "$tlm_total" ] || { echo "VIOLATION: TLM 口径不闭合"; VIOL=1; }
[ $((n_chmem_only + n_common)) -eq "$chmem_total" ] || { echo "VIOLATION: CHMEM 口径不闭合"; VIOL=1; }

echo "=== ③ 红线 ==="
n_retired=$(grep -cE '^\| `[^`]+` \| [^|]*\| `not-covered` \|' "$LEDGER")
n_fw_retired=$(grep -cE '^\| `[^`]+` \| [^|]*`framework`[^|]*\| `not-covered` \|' "$LEDGER")
n_fw_total=$(grep -cE '^\| `[^`]+` \| [^|]*`framework`[^|]*\| `(covered|partial|not-covered)` \|' "$LEDGER")
echo "not-covered 总数: ${n_retired} (上限 ${MAX_RETIRED}) | framework 族: ${n_fw_total} 条 (not-covered ${n_fw_retired})"
[ "$n_retired" -le "$MAX_RETIRED" ] || { echo "VIOLATION: not-covered 总数超上限"; VIOL=1; }
[ "$n_fw_retired" -eq 0 ] || { echo "VIOLATION: framework 族出现 not-covered"; VIOL=1; }
[ "$n_fw_total" -ge "$FRAMEWORK_MIN" ] || { echo "VIOLATION: framework 族条目不足 ${FRAMEWORK_MIN}"; VIOL=1; }
# covered/partial 必须有差异说明 (第 5 列非空 或 标注继承)
grep -E '^\| `[^`]+` \| [^|]*\| `(covered|partial)` \|' "$LEDGER" \
  | awk -F'|' 'NF>=6 {d=$6; gsub(/[[:space:]]/,"",d); if(d=="") print "VIOLATION: 差异说明缺失: " $2}' \
  | tee "$tmp/v3" | head -5
[ -s "$tmp/v3" ] && VIOL=1

# ③-b/③-c 同一理由 ≤ MAX_SAME_REASON + not-covered 必须有显式理由
#   口径 (依已归档 spec tlm-coverage-parity-ledger-red-lines):
#     - 仅作用于 not-covered 条目 —— covered/partial 的理由列大量相同 (继承族级),
#       属正常形态, 不适用本红线
#     - 理由 = 台账第 5 列 (awk $6; $1 是首个 | 之前的空串) => NF 必为 7
#     - 归一化 = 仅去首尾空白; 分组 = 完全相同的理由字符串 (字面口径)
#     - 违规必须逐条列出该组条目名 (spec Scenario: 「脚本报错并列出违规条目」)
grep -E '^\| `[^`]+` \| [^|]*\| `not-covered` \|' "$LEDGER" \
  | awk -F'|' 'NF>=7 {
      name=$2; reason=$6;
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", name);
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", reason);
      print reason "\t" name;
    }' > "$tmp/nc_reasons"

awk -F'\t' -v max="$MAX_SAME_REASON" -v vf="$tmp/v3b" '
  function viol(msg) { print msg; print msg > vf; bad=1 }
  function viol_missing(msg) { print msg; print msg > vf; bad=1; missing++ }
  {
    r=$1; n=$2;
    if (r == "") { viol_missing("VIOLATION: not-covered 条目缺显式理由: " n); next }
    if (r ~ /^继承/) { viol_missing("VIOLATION: not-covered 条目缺显式理由 (继承标记): " n); next }
    if (!(r in cnt)) { ord[++k]=r }
    cnt[r]++; lst[r]=lst[r] "\n    - " n
  }
  END {
    for (i=1;i<=k;i++) {
      r=ord[i]
      if (cnt[r] > max) {
        viol("VIOLATION: 同一理由出现 " cnt[r] " 次 (上限 " max "): " r)
        print lst[r]
        print lst[r] > vf
      }
    }
    mx=0; for (i=1;i<=k;i++) if (cnt[ord[i]]>mx) mx=cnt[ord[i]]
    dup=0; for (i=1;i<=k;i++) if (cnt[ord[i]]>1) dup++
    printf "  理由分组数 %d | 最大组 %d 次 | 重复(>1)组 %d 个 | 显式理由缺失 %d 条\n", k, mx, dup, missing+0
  }' "$tmp/nc_reasons"
[ -s "$tmp/v3b" ] && VIOL=1

echo "=== ④ 口径守卫 ==="
if grep -qE '按族(求和|汇总)' "$LEDGER" && ! grep -q '禁止按族求和' "$LEDGER"; then
  echo "VIOLATION: 台账疑似使用族求和口径 (须用 --list-tests 差集口径)"
  VIOL=1
fi

if [ "$VIOL" -ne 0 ]; then echo "check_coverage_parity_ledger: FAILED"; exit 1; fi
echo "check_coverage_parity_ledger: PASS (覆盖 ${n_ledger} / 闭合 OK / 红线 OK)"
exit 0

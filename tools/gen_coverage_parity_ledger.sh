#!/usr/bin/env bash
# tools/gen_coverage_parity_ledger.sh — ADR-088 §4 覆盖平价台账生成器
# 用途: 从两个 binary 的 --list-tests 差集生成台账条目骨架 + 重算闭合校验
#       (数字不写死 —— 计数随代码演进漂移, 台账头部标注基线 commit)
set -uo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

TLM_BIN="${TLM_BIN:-./build/bin/chipforge_tests}"
CHMEM_BIN="${CHMEM_BIN:-./build/bin/chipforge_tests_chmem}"
LEDGER="${LEDGER:-docs/architecture/tlm-coverage-parity-ledger.md}"

for b in "$TLM_BIN" "$CHMEM_BIN"; do
  [ -x "$b" ] || { echo "缺少 binary: $b (先 cmake --build build)"; exit 1; }
done

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
"$TLM_BIN" --list-tests > "$tmp/tlm.txt"
"$CHMEM_BIN" --list-tests > "$tmp/chmem.txt"

# 解析: case 名 + tag 列表 (格式: "  name" 后跟 "      [tag]" 行)
parse() {
  awk '
    /^[0-9]+ test cases/ { exit }
    /All available test cases:/ { started=1; next }
    !started { next }
    /^      \[/ { tags=tags $0 " " ; next }
    /^  [^ ]/ { if (name!="") printf "%s\t%s\n", name, tags; name=$0; sub(/^  /,"",name); sub(/[[:space:]]*$/,"",name); tags="" }
    END { if (name!="") printf "%s\t%s\n", name, tags }
  ' "$1"
}

# 注意: case 名可含空格 => 必须按 TAB 切分 (cut -f1), 不可用 awk '{print $1}'
parse "$tmp/tlm.txt" | cut -f1 | sort -u > "$tmp/tlm_names"
parse "$tmp/chmem.txt" | cut -f1 | sort -u > "$tmp/chmem_names"
comm -23 "$tmp/tlm_names" "$tmp/chmem_names" > "$tmp/tlm_only"
comm -12 "$tmp/tlm_names" "$tmp/chmem_names" > "$tmp/common"
comm -13 "$tmp/tlm_names" "$tmp/chmem_names" > "$tmp/chmem_only"

tlm_total=$(grep -E '^[0-9]+ test cases' "$tmp/tlm.txt" | grep -oE '^[0-9]+')
chmem_total=$(grep -E '^[0-9]+ test cases' "$tmp/chmem.txt" | grep -oE '^[0-9]+')
n_tlm_only=$(wc -l < "$tmp/tlm_only")
n_common=$(wc -l < "$tmp/common")
n_chmem_only=$(wc -l < "$tmp/chmem_only")

echo "=== 闭合校验 ==="
echo "TLM 独占 ${n_tlm_only} + 共有 ${n_common} = $((n_tlm_only + n_common))  (TLM binary 总数 ${tlm_total})"
echo "CHMEM 独有 ${n_chmem_only} + 共有 ${n_common} = $((n_chmem_only + n_common))  (CHMEM binary 总数 ${chmem_total})"
ok=1
[ $((n_tlm_only + n_common)) -eq "$tlm_total" ] || { echo "FAIL: TLM 口径不闭合"; ok=0; }
[ $((n_chmem_only + n_common)) -eq "$chmem_total" ] || { echo "FAIL: CHMEM 口径不闭合"; ok=0; }

echo "=== TLM 独占 case 按族分布 ==="
parse "$tmp/tlm.txt" | while IFS=$'\t' read -r name tags; do
  if grep -qxF "$name" "$tmp/tlm_only"; then
    echo "$tags" | grep -oE '\[[^]]+\]' | tr -d '[]' | tr '\n' ','
    echo ""
  fi
done | awk -F, '{for(i=1;i<=NF;i++) if($i!="") c[$i]++} END{for(k in c) printf "  [%s]: %d\n", k, c[k]}' | sort

if [ "${1:-}" = "--emit-ledger" ]; then
  echo "=== 台账条目骨架（${n_tlm_only} 条）==="
  while IFS= read -r name; do
    fam=$(parse "$tmp/tlm.txt" | grep -F "$name" | head -1 | cut -f2 | grep -oE '\[[^]]+\]' | head -1 | tr -d '[]')
    printf '| `%s` | `%s` | _待标注_ | | |\n' "$name" "${fam:-?}"
  done < "$tmp/tlm_only"
fi
[ "$ok" -eq 1 ] || exit 1
exit 0

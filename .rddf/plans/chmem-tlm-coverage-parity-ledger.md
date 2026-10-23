# chmem-tlm-coverage-parity-ledger Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 交付 ADR-088 §4 定义的覆盖平价台账的**内容** —— 全部 463 个 TLM 独占 case 的三态标注（`covered` / `partial` / `not-covered`），配套 2 个可重跑脚本（生成骨架 + 漂移检测）并接入 CI，使「TLM 独占测试在 TLM 删除后静默消失」从不可控变为可审计。

**Architecture:** 台账采用**族级 + 条目级两层**结构：族级给出该族的判定结论（CH_MEM 侧锚点 / 差异说明 / 默认态），条目级逐条给出三态（族内同态条目显式「继承族级」标记 ⇒ 满足 ADR-088 §4 的必填字段要求且不重复 463 段文字）。`gen` 脚本从两个 binary 的 `--list-tests` 差集生成条目骨架并重算闭合校验（数字不写死）；`check` 脚本做漂移检测（新增 TLM-only case 未登记 / 标注缺失 / 批量退役红线 / 族求和口径误用）。

**Tech Stack:** Bash（2 脚本，零新依赖：grep/sed/awk + `--list-tests`）· Markdown（台账）· CMake/C++23 零改动（**本 change 不碰任何 `.h` / `.cpp`**）。

**上游 ADR / SSOT:** ADR-088 §4（台账三态格式 + 必填字段 + 509≠451 口径注记，**权威**）· ADR-088 §3（5 子系统语义裁决表 —— `not-covered` 条目的理由须引用）· ADR-083（Plugin-style 单一 source of truth）· ADR-047（Result 范式：配置错误要静态可发现）· 路线图 `feat-tlm-backend-retirement`（B0 段 + G1.4 判据）。

---

## 前置条件

- `chmem-tlm-backend-freeze`（B0）已归档 ⇒ ADR-088 §4 格式就位 ✅（2026-10-24）
- 两个 binary 已构建（`build/bin/chipforge_tests` / `build/bin/chipforge_tests_chmem`）⇒ `--list-tests` 可用

---

## File Structure

| File | Responsibility |
|---|---|
| `docs/architecture/tlm-coverage-parity-ledger.md` | **新建** 台账（族级 + 条目级两层 + 措辞映射 + B1 待办清单） |
| `tools/gen_coverage_parity_ledger.sh` | **新建** 从两 binary `--list-tests` 生成条目骨架 + 闭合校验 |
| `tools/check_coverage_parity_ledger.sh` | **新建** 漂移检测 + 红线检测 + 口径守卫 |
| `.github/workflows/architecture-gates.yml` | 改 接入 check 脚本（warn 级） |
| `tools/README.md` | 改 登记 2 个新脚本 |

**零生产代码改动**：本 change 的 `git diff --stat` 中 `.h` / `.cpp` 改动数必须 = 0。

---

### Task 1: 基线快照（计数实测，不写死）

- [ ] **Step 1: 记录两个 binary 的 case 总数与差集**

Run:
```bash
./build/bin/chipforge_tests --list-tests > /tmp/opencode/tlm_tests.txt
./build/bin/chipforge_tests_chmem --list-tests > /tmp/opencode/chmem_tests.txt
grep -E '^[0-9]+ test cases' /tmp/opencode/tlm_tests.txt /tmp/opencode/chmem_tests.txt
```
Expected: `493 test cases` / `130 test cases`

- [ ] **Step 2: 记录闭合口径（供台账头部引用）**

TLM 独占 463 + 共有 30 = 493；CH_MEM 独有 100 + 共有 30 = 130（**以脚本重算为准，不写死进脚本**）

- [ ] **Step 3: 记录基线 commit**

Run: `git rev-parse HEAD`
Expected: 一个 40 位 sha（写进台账头部，计数随代码演进漂移的依据）

---

### Task 2: `tools/gen_coverage_parity_ledger.sh`（生成骨架 + 闭合校验）

- [ ] **Step 1: 写脚本**（见下方「脚本全文」）

- [ ] **Step 2: 语法校验 + 可执行位**

Run: `chmod +x tools/gen_coverage_parity_ledger.sh && bash -n tools/gen_coverage_parity_ledger.sh && echo "syntax ok"`
Expected: `syntax ok`

- [ ] **Step 3: 跑出骨架 + 闭合校验**

Run: `bash tools/gen_coverage_parity_ledger.sh | head -30`
Expected: 输出闭合校验行（TLM 独占 + 共有 = TLM 总数；CHMEM 独有 + 共有 = CHMEM 总数）与按族分组的条目清单

---

### Task 3: `tools/check_coverage_parity_ledger.sh`（漂移 + 红线检测）

- [ ] **Step 1: 写脚本**（见下方「脚本全文」）

- [ ] **Step 2: 语法校验**

Run: `bash -n tools/check_coverage_parity_ledger.sh && echo "syntax ok"`
Expected: `syntax ok`

- [ ] **Step 3: 基线自检（台账未建时 ⇒ 报「台账缺失」而非其他错）**

Run: `bash tools/check_coverage_parity_ledger.sh; echo "exit=$?"`
Expected: 明确报出台账文件缺失 + exit 非 0（此时台账还没建，属预期）

---

### Task 4: 族级判定（19 族逐族定三态 + 锚点 + 差异说明）

- [ ] **Step 1: 列出全部 TLM 独占族及 case 数**

Run: `bash tools/gen_coverage_parity_ledger.sh | grep -E '^\[|族' | head -30`
Expected: 族清单（基线 19 族，`[cpu]` 137 最大）

- [ ] **Step 2: 逐族填族级判定表**（台账 §2）—— 每族必须给出：

| 列 | 内容 |
|----|------|
| 族 | tag |
| TLM 独占 case 数 | 实测 |
| 默认态 | `covered` / `partial` / `not-covered` |
| CH_MEM 侧锚点 | 具体 file:line 或测试文件（`covered` 必填） |
| 差异说明 | 语义差异（`covered`/`partial` 必填；`partial` 必须写差异性质） |
| 复核锚 | 族内抽查的 case（≥3 条） |

> **判定依据**（实测，勿凭印象）：
> - `[framework]` 94 → **`covered`（强制，AC 红线）**：框架语义锚在 CH_MEM 侧已有（`[framework]` 共有 case 30 条 + `test_chmem_arith_poc` 等）
> - `[riscv-tests]` 48 → `covered`：CH_MEM 侧有 vendored ELF 47 + Verilator 48 ELF 严格 `tohost==1`
> - `[mmu]` 59 / `[MMUPlugin]` / `[MultiLevelTLB]` / `[TLBUnit]` → `partial`：CH_MEM 有 TLB 命中路径但**无 PTW walk**（ADR-088 §3 已知分歧）
> - `[cpu-integration]` 96 → `partial`：CH_MEM 侧有 5-stage 与 vendored ELF，但 3/5/7/10-stage + dhrystone 载体仍在 TLM
> - `[cache]` 25 → `partial`：CH_MEM 只有 refill FSM，**L1CachePlugin 本体无覆盖**
> - `[cpu]` 137 → `partial`（主体）/`covered`（已迁移子集，逐条区分）
> - `[soc]` 20 / `[bundles]` 9 / `[picolibc]` 7 / `[stall]` 19 / `[mul-div-fsm]` 9 / `[cpu-configs]` 13 / `[isa-xlen]` 9 / `[rv32um]` 8 / `[RiscV]` 7 → 逐族判定

- [ ] **Step 3: `[framework]` 族核对（AC 强制）**

Run: `grep -c 'framework' docs/architecture/tlm-coverage-parity-ledger.md`
Expected: 族级 1 行 + 条目级 94 行；且 94 条**全部** `covered`

---

### Task 5: 生成条目级台账（463 条）

- [ ] **Step 1: 用 gen 脚本产出条目骨架到台账**

Run: `bash tools/gen_coverage_parity_ledger.sh --emit-ledger > /tmp/opencode/ledger_skeleton.md`
Expected: 骨架含 463 条条目行（每行含 case 名 + 族 + 三态占位）

- [ ] **Step 2: 按族级判定填充三态 + 继承标记**

条目行格式（每条 1 行）：

```
| `<case 名>` | `<族>` | `<三态>` | 继承族级 §2.<族号> | 继承族级差异说明 |
```

- [ ] **Step 3: 台账头部写基线 commit + 闭合校验**

- [ ] **Step 4: 台账加「措辞映射」节（ADR-088 三态为权威）**

映射：`已移植 CH_MEM` ⇒ `covered`；`由 ELF 级覆盖` ⇒ `covered`（语义等价时）/ `partial`（不等价时）；`显式退役` ⇒ `not-covered`

- [ ] **Step 5: 台账加「B1 承载力待办清单」节**

按 plugin 归组 `covered`/`partial` 条目 ⇒ 映射到 `feat-tlm-backend-retirement` 的 B1 项（BP / LSU / DBus / L1Cache 本体 / MMU TLB 策略 / PTW walk / 3-7-10 stage / Dhrystone 载体）

- [ ] **Step 6: Defer commit**

---

### Task 6: 校验脚本全量自检（漂移 + 红线 + 口径）

- [ ] **Step 1: 基线自检（台账已建 ⇒ 应 0 违规）**

Run: `bash tools/check_coverage_parity_ledger.sh; echo "exit=$?"`
Expected: `exit=0` + 报告（覆盖 463 / 闭合 OK / 红线 OK）

- [ ] **Step 2: 变异 A — 新增一个 TLM-only case 未登记 ⇒ 必须被检出**

Run: `printf '\nTEST_CASE("zzz_unregistered_probe", "[cpu]") { REQUIRE(true); }\n' >> tests/cpu/test_cpu_factory_isa_xlen.cpp && cmake -S . -B build >/dev/null && cmake --build build -j$(nproc) >/dev/null 2>&1 && bash tools/check_coverage_parity_ledger.sh; echo "exit=$?"`
Expected: 报出 `zzz_unregistered_probe` + **exit 非 0**

- [ ] **Step 3: 还原变异 A**

Run: `git checkout -- tests/cpu/test_cpu_factory_isa_xlen.cpp && cmake -S . -B build >/dev/null && cmake --build build -j$(nproc) >/dev/null 2>&1 && bash tools/check_coverage_parity_ledger.sh; echo "exit=$?"`
Expected: 回到 `exit=0`

- [ ] **Step 4: 变异 B — 台账删掉 3 条 ⇒ 必须被检出**

Run: `cp docs/architecture/tlm-coverage-parity-ledger.md /tmp/opencode/ledger.bak && sed -i '0,/^| `/!b;N;/^| `/d' /dev/null; python3 - <<'EOF'
import re
p='docs/architecture/tlm-coverage-parity-ledger.md'
s=open(p).read()
lines=s.split('\n')
out=[];removed=0
for l in lines:
    if removed<3 and l.startswith('| `') and l.count('|')>=6:
        removed+=1; continue
    out.append(l)
open(p,'w').write('\n'.join(out))
print('removed',removed)
EOF
bash tools/check_coverage_parity_ledger.sh; echo "exit=$?"`
Expected: 报出台账条目数不足 + **exit 非 0**

- [ ] **Step 5: 还原变异 B**

Run: `cp /tmp/opencode/ledger.bak docs/architecture/tlm-coverage-parity-ledger.md && bash tools/check_coverage_parity_ledger.sh; echo "exit=$?"`
Expected: `exit=0`

- [ ] **Step 6: 变异 C — 把某条 `covered` 改成 `not-covered`（触发 framework 红线 / 理由缺失）⇒ 必须被检出**

Run: `sed -i '0,/^| `\([a-z_]*\)` | `framework` | `covered`/s//| `\1` | `framework` | `not-covered`/' docs/architecture/tlm-coverage-parity-ledger.md && bash tools/check_coverage_parity_ledger.sh; echo "exit=$?"`
Expected: 报出 framework 族出现 `not-covered` + **exit 非 0**

- [ ] **Step 7: 还原变异 C + 复跑**

Run: `cp /tmp/opencode/ledger.bak docs/architecture/tlm-coverage-parity-ledger.md && bash tools/check_coverage_parity_ledger.sh; echo "exit=$?"`
Expected: `exit=0`

---

### Task 7: CI 接入 + README 登记

- [ ] **Step 1: gates.yml 接入（`Check TLM freeze` 之后）**

```yaml
      - name: Check coverage parity ledger (check_coverage_parity_ledger.sh, warn-level)
        id: verify_coverage_parity_ledger
        run: |
          echo "=== Running check_coverage_parity_ledger.sh ==="
          bash tools/check_coverage_parity_ledger.sh
        continue-on-error: true
```

- [ ] **Step 2: yaml 校验（**指定 job 名** —— 该 workflow 有 2 个 job）**

Run: `python3 -c "import yaml; d=yaml.safe_load(open('.github/workflows/architecture-gates.yml')); print('yaml ok, steps:', len(d['jobs']['architecture-gates']['steps']))"`
Expected: `yaml ok, steps: 11`（原 10 + 本次 1）

- [ ] **Step 3: README 登记 2 个新脚本**（§1 门禁表）

- [ ] **Step 4: Defer commit**

---

### Task 8: 收口（AC 逐条机械复核 + 门禁 + 零生产代码核对）

- [ ] **Step 1: 8 门禁**

Run:
```bash
bash tools/verify_adr.sh | tail -1
bash tools/verify_plugin_decision.sh | tail -1
bash tools/check_plugin_portability.sh | tail -1
bash tools/doc_link_check.sh | tail -1
bash tools/roadmap-r9-check.sh | tail -1
bash tools/check_xlen_hygiene.sh | tail -1
bash tools/check_tlm_freeze.sh | tail -1
bash tools/check_coverage_parity_ledger.sh | tail -1
```
Expected: 8 项全绿

- [ ] **Step 2: 零生产代码改动核对（AC 硬性）**

Run: `git diff main...HEAD --stat -- '*.h' '*.cpp' '*.hpp' | tail -3`
Expected: **空**（0 行）

- [ ] **Step 3: AC 逐条机械复核**

```bash
bash tools/check_coverage_parity_ledger.sh                    # 覆盖 + 闭合 + 红线
grep -c '^| `' docs/architecture/tlm-coverage-parity-ledger.md   # 463 条目 (+族级行需排除)
grep -c '继承族级' docs/architecture/tlm-coverage-parity-ledger.md # 条目级继承标记
grep -c 'covered' docs/architecture/tlm-coverage-parity-ledger.md
```
Expected: 覆盖 463 / 闭合两式成立 / `[framework]` 94 条全 `covered` / `not-covered` ≤135 / 同理由 ≤10

- [ ] **Step 4: `openspec change validate`**

Run: `openspec change validate chmem-tlm-coverage-parity-ledger`
Expected: valid（0 error）

- [ ] **Step 5: 分 2 批 commit**

```bash
git add tools/gen_coverage_parity_ledger.sh tools/check_coverage_parity_ledger.sh .github/workflows/architecture-gates.yml tools/README.md && git commit -m "ci: 覆盖平价台账生成 + 漂移检测脚本 (ADR-088 §4)" -- tools/gen_coverage_parity_ledger.sh tools/check_coverage_parity_ledger.sh .github/workflows/architecture-gates.yml tools/README.md
git add docs/architecture/tlm-coverage-parity-ledger.md openspec/changes/chmem-tlm-coverage-parity-ledger/ && git commit -m "docs: TLM 覆盖平价台账 (463 条三态标注) + ADR-088 §4 措辞映射 + B1 待办清单" -- docs/architecture/tlm-coverage-parity-ledger.md openspec/changes/chmem-tlm-coverage-parity-ledger/
```

---

## 脚本全文

### `tools/gen_coverage_parity_ledger.sh`

```bash
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
```

### `tools/check_coverage_parity_ledger.sh`

```bash
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

echo "=== ④ 口径守卫 ==="
if grep -qE '按族(求和|汇总)' "$LEDGER" && ! grep -q '禁止按族求和' "$LEDGER"; then
  echo "VIOLATION: 台账疑似使用族求和口径 (须用 --list-tests 差集口径)"
  VIOL=1
fi

if [ "$VIOL" -ne 0 ]; then echo "check_coverage_parity_ledger: FAILED"; exit 1; fi
echo "check_coverage_parity_ledger: PASS (覆盖 ${n_ledger} / 闭合 OK / 红线 OK)"
exit 0
```

---

## 修订记录

- **2026-10-24（初版）**：P1 plan gen（builder）。数字纠正：TLM 独占 451 → **463**（A 新增 13 个 TLM-only case、删 1 个），TLM 总 481 → **493**；闭合 463+30=493 / 100+30=130。
- **2026-10-24（实施期修正 2）**：check 脚本的台账条目正则原本要求第 2 列是**单个**反引号族，而台账第 2 列是**多个 tag**（`` `mmu` `stall` ``）⇒ 只抓到 286/463 ⇒ 放宽为「第 2 列任意 + 第 3 列必须是三态之一」（后者天然排除 §2 族级表行）。
- **2026-10-24（实施期修正）**：脚本落盘实跑暴露 **2 处解析 bug**，plan 内的脚本全文同步修正：
  ① `parse | sed | awk '{print $1}'` —— **case 名可含空格** ⇒ 按空格切分把名字截断（丢 14 个 ⇒ 闭合 449≠493）⇒ 改 `cut -f1`（TAB 分隔）
  ② `parse` 的 awk 在 **tag 行先 flush 名字** ⇒ tags 全空（族分布输出为空）⇒ 改为「tag 行累积、下个 name 行 flush」+ `END` 补最后一个

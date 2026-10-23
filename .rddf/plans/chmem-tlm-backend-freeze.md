# chmem-tlm-backend-freeze Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把「TLM 后端退役」从路线图口号变成**可执行的冻结契约**：上线冻结门禁（新增 TLM-only 业务代码须登记）、清掉 4 个死代码文件、起草 ADR-088（判据先于动作）并给 ADR-083 补 v1.1 终态。本 change **不删任何 TLM-only 业务代码**（那是 B3）。

**Architecture:** 冻结门禁 = 「模式面（`ip/**/plugins/*.h` + `ip/**/arch/**/*.h` 的非 `_chmem.h` 文件）− 9 个双模共享文件（实测 include 闭包）」为检测面，判定内容 = 相对 baseline commit 的**新增函数定义**差集，未登记即非 0 退出；白名单格式（`文件:行 | 原因 | CH_MEM 是否需同步`）与 ADR-088 的台账格式统一。死代码 4 文件 0 includer 直接删。ADR-088 承载 5 项必填（B3 的删除判据就在其中）。

**Tech Stack:** Bash（门禁脚本，零新依赖：git + grep + awk）· Markdown（ADR-088 / ADR-083 v1.1）· CMake/C++23 双 binary 回归（`chipforge_tests` + `chipforge_tests_chmem`）。

**上游 ADR / SSOT:** ADR-083（Plugin-style 单一 source of truth，§2.3/§2.4/§4.3/§5.2/§5.3）· ADR-040 v3.0（§10.1 双后端语义必须一致 / §10.2 差异必须显式契约化）· ADR-046（FSM 豁免）· ADR-087（§D4 护栏形态随 TLM 退役切换 ⇒ ADR-088 第 5 项待办）· 路线图 `feat-tlm-backend-retirement`（B0–B3 编排）+ `objective-ip-cpu-tlm-backend-retirement`（G1/G2/G3 门禁）· 姊妹提案 `chmem-tlm-coverage-parity-ledger`（P1，交付台账**内容**）。

---

## 前置条件（**不可跳过**）

1. **`rv64-isa-declaration-correction` 已合并**（A 的 TLM 侧护栏实装是 ADR-088 §6 登记的实体；且 A 在 `ip/cpu/cpu_factory.h` 新增的 `validate_isa_xlen` 会被本门禁判为「新增函数」⇒ **A 落地后才采集 baseline**，否则假阳性）
2. 工作区干净；当前分支从 `main` 拉出

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `tools/check_tlm_freeze.sh` | **新建** 冻结门禁脚本（检测面 + baseline 差集 + 白名单校验） |
| `.rddf/state/tlm-freeze-base.txt` | **新建** baseline 锚点（commit sha） |
| `.rddf/state/tlm-freeze-allowlist.txt` | **新建** 白名单（`文件:行 | 原因 | CH_MEM 是否需同步`） |
| `docs/architecture/adr/ADR-088-tlm-backend-retirement.md` | **新建** 5 项必填（冻结条款 / 语义裁决表 / 台账格式 / B3 判据 / ADR-087 v1.1 待办） |
| `docs/architecture/adr/ADR-083-plugin-style-single-source-of-truth.md` | 改 v1.1（§2.5 冻结条款 + §5.4 退役时间线 + 修订历史行） |
| `docs/architecture/adr.md` | 改 注册 ADR-088 + 修订历史行 |
| `.github/workflows/architecture-gates.yml` | 改 接入冻结门禁（`continue-on-error: true`） |
| `ip/cpu/plugins/exception.h` | 删 死代码（`ExceptionPlugin`，0 includer） |
| `ip/cpu/plugins/fpu.h` | 删 死代码（`FPUPlugin`，0 includer） |
| `ip/cpu/arch/riscv/fpu.h` | 删 死代码（`RiscvFpuPlugin`，0 includer，与上者互不引用） |
| `ip/cache/plugins/cache_keys.h` | 删 死代码（0 includer） |

### Tests

| File | Responsibility |
|---|---|
| 无新测试文件 | 本 change **不改业务代码** ⇒ 回归靠既有双 binary 全量测试（0 退化判据）；门禁脚本的自检是**变异对照**（见 Task 4） |

---

### 实施实测留证（2026-10-24，builder 执行中发现并已修正的 5 处）

| # | 发现 | 影响 | 处置 |
|---|------|------|------|
| 1 | **检测面定义过窄**：草案的「`ip/**/plugins/*.h` + `ip/**/arch/**/*.h`」实测只有 **18** 个候选 ⇒ 漏掉 `ip/mmu/{tlm,lib,policies}` 与 `ip/cache/policies` 的 TLM-only 面 | 冻结门禁管不住 `ip/mmu/tlm/MMUPlugin.h` 等 | 扩为 `ip/**/*.h`（非 `_chmem.h`）⇒ **44 − 9 共享 = 35** 候选（删 4 死代码后 **31**） |
| 2 | **白名单还原命令失效**：plan 原本用函数名做 `sed` pattern，但登记行写的是**原因文本** ⇒ 删不掉 ⇒ 白名单残留会**错误豁免**后续判定 | 门禁可能被静默绕过 | 还原改为 `git checkout -- <file>` + 按**登记文本**删条目 + 复核 `grep -vc '^#'` = 0（plan Task 4 Step 3 已修正） |
| 3 | **yaml 校验读到错 job**：workflow 有 2 个 job（`toolchain-sanity` 8 steps + `architecture-gates` N steps）⇒ `list(d['jobs'].keys())[0]` 读的是前者 | 自检命令误报 | plan Task 5 Step 2 改为显式指定 `d['jobs']['architecture-gates']`（实测 10 steps） |
| 4 | **ADR 章节自检口径错**：ADR-088 的必填章节是**三级标题** `### N.` ⇒ plan 的 `grep "^## N"` 恒 0 | 自检恒失败 | plan Task 7 Step 5 改为 `grep -c "^### N\."`（实测 5 行全 1） |
| 5 | **死代码 includer 假阳性**：plan 的 basename 口径（`grep "#include.*exception\.h"`）撞上框架的 `cf/plugin/plugin_exception.h` ⇒ 报 8 命中 | 误判死代码有引用 | 改**完整相对路径**口径 ⇒ 4 文件真实命中 **0**；类名口径的 6 处命中经查全是**文档提及**（`ip/cpu/docs/{dse_architecture,implementation-plan/README}.md`）⇒ 同步把 `dse_architecture.md` 中引用已删 `RiscvFpuPlugin` 的伪代码改为「Phase 5+ 推迟，当前无实现」注释 |

> 5 处全部是「照抄即挂」类问题 ⇒ 再次验证「代码块/命令必须逐字实跑一次」这条纪律的价值（与 A 的 change 同源）。

### Task 1: 基线快照 + 前置条件核实

**Files:**
- Test: 无新文件（只跑既有基线）

- [ ] **Step 1: 核实前置条件（A 已合并）**

Run: `git log --oneline --all | grep -c "rv64-isa-declaration-correction" || echo "0"`
Expected: ≥1（若 0 ⇒ **停下报告**：A 未落地时采集 baseline 会产生假阳性）

- [ ] **Step 2: 跑双 binary 基线**

Run: `./build/bin/chipforge_tests 2>&1 | tail -2`
Expected: 记录 `test cases: N | N-1 passed | 1 failed`（1 failed = `test_7stage_riscv.cpp:124` pre-existing，**口径 = 不新增失败**）

Run: `./build/bin/chipforge_tests_chmem --list-tests 2>&1 | grep "test cases"`
Expected: 记录 CH_MEM binary 总 case 数（TLM-only change 不应影响它）

- [ ] **Step 3: 记录 4 个死代码的 0-includer 事实**

Run: `for f in "plugins/exception.h" "plugins/fpu.h" "arch/riscv/fpu.h" "cache_keys.h"; do echo -n "$f: "; grep -rn "#include.*$(basename $f)" ip/ src/ tests/ tools/ | grep -v "^ip/cpu/plugins/$(basename $f):" | wc -l; done`
Expected: 4 行全 `0`

- [ ] **Step 4: Defer commit**

---

### Task 2: 固化判定口径（共享清单 + baseline 锚点 + 白名单格式）

> **判据先于动作**：门禁脚本的行为必须与 ADR-088 §2/§5 的描述一致 ⇒ 先把三份状态文件与清单定下来，脚本与 ADR 都引用它们。

**Files:**
- Create: `.rddf/state/tlm-freeze-base.txt`
- Create: `.rddf/state/tlm-freeze-allowlist.txt`
- Create: `.rddf/state/tlm-shared-files.txt`（9 个双模共享文件，脚本的排除清单）

- [ ] **Step 1: 写 baseline 锚点（固定 sha，不用 `main`）**

Run: `git rev-parse HEAD > .rddf/state/tlm-freeze-base.txt && cat .rddf/state/tlm-freeze-base.txt`
Expected: 一个 40 位 sha

> ⚠️ 用**固定 sha**而非 `main` ⇒ 切分支不影响判定（design R6）；换锚点必须显式改此文件并在 ADR-088 记录。

- [ ] **Step 2: 写双模共享排除清单（9 条，取自 design.md Decision 1）**

` .rddf/state/tlm-shared-files.txt` 内容（每行一个路径）：

```
ip/cpu/tlm/cpu_keys.h
ip/mmu/tlm/mmu_keys.h
ip/mmu/lib/ptw.h
ip/cpu/arch/riscv/decoder_table.h
ip/cpu/arch/riscv/payload_riscv.h
ip/cpu/core/payload_common.h
ip/mmu/lib/memory_interface.h
ip/cpu/arch/riscv/mul.h
ip/cpu/arch/riscv/mul_div_fsm.h
```

- [ ] **Step 3: 写白名单（空 + 格式说明）**

`.rddf/state/tlm-freeze-allowlist.txt` 首行是格式说明，其后每行 3 段：

```
# 格式: <文件:行> | <原因> | <CH_MEM 是否需同步: 是|否|不适用>
# 登记人: <who> | 登记日期: <YYYY-MM-DD>
```

- [ ] **Step 4: 校验三份文件存在且格式正确**

Run: `ls -l .rddf/state/tlm-freeze-base.txt .rddf/state/tlm-shared-files.txt .rddf/state/tlm-freeze-allowlist.txt && wc -l < .rddf/state/tlm-shared-files.txt`
Expected: 3 个文件存在；共享清单 **9** 行

- [ ] **Step 5: Defer commit**

---

### Task 3: 冻结门禁脚本 `tools/check_tlm_freeze.sh`

**Files:**
- Create: `tools/check_tlm_freeze.sh`

- [ ] **Step 1: 写脚本（全量内容，照抄即可）**

```bash
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
exit 0
```

- [ ] **Step 2: 脚本语法与可执行位**

Run: `chmod +x tools/check_tlm_freeze.sh && bash -n tools/check_tlm_freeze.sh && echo "syntax ok"`
Expected: `syntax ok`

- [ ] **Step 3: 基线自检（当前仓库应 0 违规）**

Run: `bash tools/check_tlm_freeze.sh`
Expected: `check_tlm_freeze: violations=0 ...` + exit 0

> ⚠️ 若报出**存量**违规（baseline 之后才新增的函数，如 `rv64-isa-declaration-correction` 的 `validate_isa_xlen`）⇒ 说明 baseline 采取得太晚 ⇒ **重采 baseline**（Task 2 Step 1）后再跑；不要用白名单掩盖。

- [ ] **Step 4: 门禁脚本自检（builder 已实跑留证，2026-10-24）**

builder 用 scratch 状态文件（`/tmp/opencode/bfreeze/state`）把本计划里的脚本**逐字抽出实跑**：

| 场景 | 实测输出 | 结论 |
|------|---------|------|
| 基线（无新增） | `check_tlm_freeze: violations=0 allowlist=0 candidates=35 base=9a730ba022` + exit 0 | ✅ |
| 变异（`branch.h` 追加一个函数） | `VIOLATION: ... ip/cpu/arch/riscv/branch.h:142` + 签名 + 登记提示 + `violations=1` + **exit 1** | ✅ |
| 登记白名单后 | `violations=0 allowlist=1` + exit 0 | ✅ |
| 还原后 | 无 diff + `violations=0` + exit 0 | ✅ |

> 实测同时暴露了**检测面定义缺陷**并已修正：草案的「`ip/**/plugins/*.h` + `ip/**/arch/**/*.h`」实测只有 **18** 个候选 ⇒ 漏掉 `ip/mmu/{tlm,lib,policies}` 与 `ip/cache/policies` 的 TLM-only 面 ⇒ 本版扩到 `ip/**/*.h`（非 `_chmem.h`），实测 **44 − 9 共享 = 35** 个候选。

- [ ] **Step 5: Defer commit**

---

### Task 4: 门禁变异对照自证（证明它真能抓）

**Files:**
- Test: 无新文件（mutation check）

- [ ] **Step 1: 变异 — 在 TLM-only 文件新增一个函数**

Run: `printf '\n// MUTATION: 冻结门禁自检用临时函数\nstatic int tlm_freeze_probe_fn(int x) { return x + 1; }\n' >> ip/cpu/arch/riscv/branch.h && bash tools/check_tlm_freeze.sh; echo "exit=$?"`
Expected: 报出 `VIOLATION: ... ip/cpu/arch/riscv/branch.h:<行>` 且 **exit=1**

- [ ] **Step 2: 变异 — 登记白名单后应放行**

Run: `LN=$(grep -n "tlm_freeze_probe_fn" ip/cpu/arch/riscv/branch.h | cut -d: -f1) && echo "ip/cpu/arch/riscv/branch.h:$LN | 门禁自检临时探针 | 否" >> .rddf/state/tlm-freeze-allowlist.txt && bash tools/check_tlm_freeze.sh; echo "exit=$?"`
Expected: `violations=0` + exit 0

- [ ] **Step 3: 还原（探针 + 白名单条目都要清）**

Run: `python3 - <<'EOF'
import re
p='ip/cpu/arch/riscv/branch.h'
s=open(p).read()
s=re.sub(r'\n// MUTATION: 冻结门禁自检用临时函数\nstatic int tlm_freeze_probe_fn\(int x\) \{ return x \+ 1; \}\n', '\n', s)
open(p,'w').write(s)
EOF
git checkout -- ip/cpu/arch/riscv/branch.h && sed -i '/门禁自检临时探针/d' .rddf/state/tlm-freeze-allowlist.txt && git diff --stat ip/cpu/arch/riscv/branch.h && grep -vc '^#' .rddf/state/tlm-freeze-allowlist.txt`
Expected: `branch.h` 无 diff（还原干净）+ 白名单非注释行数 = 0

> ⚠️ **还原命令的坑**（builder 实测踩到）：白名单登记行写的是**原因文本**（如「门禁自检临时探针」）而**不是**函数名 ⇒ 用函数名做 `sed` pattern **删不掉** ⇒ 白名单残留会让后续判定被错误豁免。还原一律用 `git checkout -- <file>`（文件）+ 按**登记行文本**删白名单条目，并复核 `grep -vc '^#'` = 0。

- [ ] **Step 4: 还原后复跑**

Run: `bash tools/check_tlm_freeze.sh; echo "exit=$?"`
Expected: `violations=0` + exit 0

- [ ] **Step 5: Defer commit**

---

### Task 5: CI 接入（warn 级）+ 工具 README 登记

**Files:**
- Modify: `.github/workflows/architecture-gates.yml`
- Modify: `tools/README.md`

- [ ] **Step 1: gates.yml 接入（在 XLEN hygiene 步骤之后）**

```yaml
      - name: Check TLM freeze (check_tlm_freeze.sh, warn-level per ADR-088 §2)
        id: verify_tlm_freeze
        run: |
          echo "=== Running check_tlm_freeze.sh ==="
          bash tools/check_tlm_freeze.sh
        continue-on-error: true
```

- [ ] **Step 2: yaml 语法校验**

Run: `python3 -c "import yaml; d=yaml.safe_load(open('.github/workflows/architecture-gates.yml')); j=d['jobs']['architecture-gates']; print('yaml ok, steps:', len(j['steps'])); print([s.get('name','') for s in j['steps'] if 'freeze' in s.get('name','')])"`
Expected: `yaml ok, steps: 10`（原 9 + 本次 1）+ 列表含 `Check TLM freeze (check_tlm_freeze.sh, warn-level per ADR-088 §2)`

> ⚠️ 该 workflow 有 **2 个 job**（`toolchain-sanity` 8 steps + `architecture-gates` N steps）⇒ 校验命令必须**指定 job 名**（`d['jobs']['architecture-gates']`），否则读到的是第一个 job 的 step 数。

- [ ] **Step 3: `tools/README.md` 登记**

在该文件 §1「验证脚本 (Architecture Gates)」表追加一行：脚本 `tools/check_tlm_freeze.sh`、用途「ADR-088 §2 TLM-only 代码冻结门禁（新增函数须登记白名单）」、退出码「0=PASS / 1=有未登记的新增函数」、CI 集成「✅ `architecture-gates.yml` step 5（**warn 级**, `continue-on-error: true`）」

- [ ] **Step 4: Defer commit**

---

### Task 6: 删除 4 个死代码文件

**Files:**
- Delete: `ip/cpu/plugins/exception.h`、`ip/cpu/plugins/fpu.h`、`ip/cpu/arch/riscv/fpu.h`、`ip/cache/plugins/cache_keys.h`

- [ ] **Step 1: 删除前终次确认 0 includer**

Run: `for f in exception.h fpu.h cache_keys.h; do echo -n "$f: "; grep -rn "#include.*$f" ip/ src/ tests/ tools/ bundles/ include/ | grep -vE "^(ip/cpu/plugins/(exception|fpu)\.h|ip/cpu/arch/riscv/fpu\.h|ip/cache/plugins/cache_keys\.h):" | wc -l; done`
Expected: 3 行全 `0`

- [ ] **Step 2: 删除 + 双 binary build**

Run: `git rm ip/cpu/plugins/exception.h ip/cpu/plugins/fpu.h ip/cpu/arch/riscv/fpu.h ip/cache/plugins/cache_keys.h && cmake --build build -j$(nproc) 2>&1 | tail -2 && cmake --build build --target chipforge_tests_chmem -j$(nproc) 2>&1 | tail -2`
Expected: 两个 binary 均编译成功

- [ ] **Step 3: 回归（0 退化）**

Run: `./build/bin/chipforge_tests 2>&1 | tail -2`
Expected: 与 Task 1 Step 2 记录的基线**完全一致**（同样 1 个 pre-existing FAIL）

- [ ] **Step 4: 门禁复跑（删文件不应影响判定面）**

Run: `bash tools/check_tlm_freeze.sh; echo "exit=$?"`
Expected: `violations=0` + exit 0

- [ ] **Step 5: Defer commit**

---

### Task 7: 起草 ADR-088（5 项必填）

**Files:**
- Create: `docs/architecture/adr/ADR-088-tlm-backend-retirement.md`

- [ ] **Step 1: 写 ADR 头部 + Context + 决策 §2 冻结条款**

头部格式照 ADR-087（`> **Status**:` / `> **类别**:` / `> **关联 ADR**:` / `> **关联变更**:`），Status = `🚧 Proposed（2026-10-24，TLM 后端退役 B0 阶段）`。

§2 冻结条款必须写满 3 小节（design.md Decision 4 ①）：
- §2 冻结范围：检测面（模式面 − 9 共享文件，**引用 `.rddf/state/tlm-shared-files.txt`**）+ 判定机制（相对 `.rddf/state/tlm-freeze-base.txt` 的新增函数定义差集）+ 违规行为（脚本 exit 1，CI warn 级）
- §2.1 冻结期内的合法动作：存量函数体 bugfix 允许（不拦）；双模共享文件豁免须写进 `tlm-shared-files.txt` 并在本 ADR 记录
- §2.2 解冻条件：仅 B3 删除完成、或架构决策显式解冻（需新 ADR）

- [ ] **Step 2: 写 §3 语义裁决表（5 子系统，表头 + 已知分歧填满，正式表态留空）**

| 子系统 | TLM 侧实现 | CH_MEM 侧实现 | 已知分歧 | B2 正式表态 |
|--------|-----------|---------------|---------|-----------|
| M 扩展 | `ip/cpu/arch/riscv/mul_div_fsm.h`（424 行，radix-2，MUL=1c/MULH=3c/DIV=33c） | `ip/cpu/arch/riscv/int_alu_chmem.h`（19 层 select 树，组合式 1 拍） | cycle 不同构（差 ~286c，±5%/+50c 判据不适用） | *（留 B2）* |
| CSR | `ip/cpu/arch/riscv/csr.h`（43 行，**纯 stub**，setup/build 全空） | `ip/cpu/plugins/csr_chmem.h`（566 行，Zicsr + trap） | TLM 侧无实装可保留 | *（留 B2）* |
| MMU PTW | `ip/mmu/lib/ptw.{h,cpp}`（149+197 行，完整 walk） | `ip/mmu/plugins/MMUPlugin_chmem.h`（无 PTW，miss ⇒ 回退 vaddr） | PTW 未落地前表现为回退 vaddr | *（留 B2）* |
| BP（BTB/Bimodal/GShare） | `ip/cpu/plugins/branch_predictor.h`（319 行） | **完全缺失**（无 `branch_predictor_chmem.h`） | CH_MEM 侧符号数 = 0 | *（留 B2）* |
| MMU exception handler | `ip/cpu/plugins/mmu_exception_handler.h`（69 行，**TLM-only**） | `ip/cpu/plugins/mmu_exception_handler_chmem.h`（118 行，**存在但未注册**） | CH_MEM 下 `cpu_factory_chmem.h:342-349` 被 `#ifndef` 包裹 ⇒ 不注册 | *（留 B2）* |

- [ ] **Step 3: 写 §4 覆盖平价台账格式 + §5 B3 删除判据 + §6 ADR-087 v1.1 待办**

- §4：三态标注 `covered` / `partial` / `not-covered` + 每态必填字段（子系统 / TLM 锚点 / CH_MEM 锚点 / 回归锚点 / 语义差异说明）+ **口径注记**（13 族求和 509 ≠ 451，case 级以两个 binary `--list-tests` 差集为唯一口径）+ 明确「本 ADR 只定义**格式**，内容由姊妹提案 `chmem-tlm-coverage-parity-ledger` 交付」
- §5：B3 删除判据 = 「按 CH_MEM binary 包含闭包反查」+ **禁止按路径名/文件名推断** + 删除前 3 道前置（覆盖平价达成 / 语义裁决完成 / CI 有可回归的自动测试）+ 引用 `.rddf/state/tlm-shared-files.txt` 作为当前共享事实快照
- §6：**显式登记行** —— 「ADR-087 §D4 的 TLM 侧 `static_assert` 护栏（`int_alu.h` / `branch.h`，已由 `rv64-isa-declaration-correction` 落地）随 TLM 删除消失 ⇒ 须切 CH_MEM 侧 CI grep 门禁；本项待办由 architect 出 ADR-087 v1.1」

- [ ] **Step 4: 写 §7 影响 + §8 验证记录 + §9 相关文档**

§8 验证记录写实际跑过的命令与结果（门禁脚本 3 态 + 双 binary build + 全量测试计数）。

- [ ] **Step 5: 自检 — 5 项必填逐条落地**

Run: `for k in 2 3 4 5 6; do echo -n "### $k: "; grep -c "^### $k\." docs/architecture/adr/ADR-088-tlm-backend-retirement.md; done`
Expected: 5 行全 ≥1（章节为**三级标题** `### N. 标题` ⇒ 匹配 `^### N\.`，不是 `^## N`）

Run: `grep -c "留 B2" docs/architecture/adr/ADR-088-tlm-backend-retirement.md` → 期望 ≥5（5 个子系统的正式表态留空标记）
Run: `grep -c "rv64-isa-declaration-correction\|ADR-087" docs/architecture/adr/ADR-088-tlm-backend-retirement.md` → 期望 ≥2（§6 待办登记）

- [ ] **Step 6: Defer commit**

---

### Task 8: ADR-083 v1.1 修订 + 注册表更新

**Files:**
- Modify: `docs/architecture/adr/ADR-083-plugin-style-single-source-of-truth.md`
- Modify: `docs/architecture/adr.md`

- [ ] **Step 1: ADR-083 加 §2.5 冻结条款**

内容（3 句 + 引用）：TLM-only 业务代码进入冻结（见 [ADR-088](adr/ADR-088-tlm-backend-retirement.md) §2）；新增函数定义须登记 `.rddf/state/tlm-freeze-allowlist.txt`；本节不改变 §2.4 的双文件分离约束。

- [ ] **Step 2: ADR-083 加 §5.4 退役时间线**

内容：B0 冻结（`chmem-tlm-backend-freeze`）→ B1 承载力 → B2 语义裁决 → B3 删除（Gate 3 放行）→ 路线图载体 `feat-tlm-backend-retirement`。

- [ ] **Step 3: ADR-083 头部加 v1.1 修订标记 + 修订历史行**

Run: `grep -n "^> \*\*Status\*\*\|^> \*\*版本\*\*" docs/architecture/adr/ADR-083-plugin-style-single-source-of-truth.md | head -3`
Expected: 找到 Status 行（据实际头部文本追加 v1.1 标记）

- [ ] **Step 4: `docs/architecture/adr.md` 注册 ADR-088**

在注册表表尾追加一行（格式照 ADR-087 行）：

```
| ADR-088 | TLM 后端退役 — 冻结契约 + 语义裁决表 + 覆盖平价台账格式 + B3 删除判据（TLM 单后端收敛 B0 阶段） | 架构治理 | 🚧（设计提案 2026-10-24；实装归 change `chmem-tlm-backend-freeze`） | [`adr/ADR-088-tlm-backend-retirement.md`](adr/ADR-088-tlm-backend-retirement.md) |
```

并在「修订历史」表追加一行（格式照 1.5 行）：

```
| 2026-10-24 | 1.6 | **新增 ADR-088**：TLM 后端退役治理锚（冻结条款 + 5 子系统语义裁决表 + 覆盖平价台账三态格式 + B3 删除判据 + ADR-087 v1.1 待办登记）；同步修订 ADR-083 为 v1.1（补 §2.5 冻结条款 + §5.4 退役时间线） |
```

- [ ] **Step 5: 门禁自检（ADR 注册表一致性）**

Run: `bash tools/verify_adr.sh | tail -1`
Expected: `✓ All ✅ ADRs pass verification`（新增 ADR 必须已注册，否则该脚本会报漂移）

Run: `bash tools/doc_link_check.sh | tail -1`
Expected: `0 broken`

- [ ] **Step 6: Defer commit**

---

### Task 9: 全门禁收口 + AC 终验

**Files:**
- Test: 全部门禁

- [ ] **Step 1: 6 门禁 + 冻结门禁**

Run:
```bash
bash tools/verify_adr.sh | tail -1
bash tools/verify_plugin_decision.sh | tail -1
bash tools/check_plugin_portability.sh | tail -1
bash tools/doc_link_check.sh | tail -1
bash tools/roadmap-r9-check.sh | tail -1
bash tools/check_xlen_hygiene.sh | tail -1
bash tools/check_tlm_freeze.sh | tail -1
```
Expected: 7 项全绿（冻结门禁 `violations=0`）

- [ ] **Step 2: AC 逐条机械复核**

```bash
ls -l tools/check_tlm_freeze.sh                                                   # 存在且可执行
for f in ip/cpu/plugins/exception.h ip/cpu/plugins/fpu.h ip/cpu/arch/riscv/fpu.h ip/cache/plugins/cache_keys.h; do [ ! -e "$f" ] && echo "deleted OK: $f"; done
grep -c "## 2\|## 3\|## 4\|## 5\|## 6" docs/architecture/adr/ADR-088-tlm-backend-retirement.md   # 5 项必填
grep -c "ADR-088" docs/architecture/adr.md                                          # 已注册
grep -c "2.5 冻结条款\|5.4 退役时间线" docs/architecture/adr/ADR-083-plugin-style-single-source-of-truth.md  # v1.1 两节
wc -l < .rddf/state/tlm-shared-files.txt                                           # 9
bash tools/check_tlm_freeze.sh                                                     # violations=0
```
Expected: 全部符合

- [ ] **Step 3: 双 binary 全量回归（0 退化）**

Run: `timeout 1500 bash tools/run_chipforge_tests.sh --all 2>&1 | tail -12`
Expected: 与 Task 1 基线一致（TLM entry 仍含 1 个 pre-existing FAIL；CH_MEM + verilator + verify_plugin_decision 全 PASS）

- [ ] **Step 4: `openspec change validate`**

Run: `openspec change validate chmem-tlm-backend-freeze`
Expected: valid（0 error）

- [ ] **Step 5: 分批 commit（3 批，`git commit -- <paths>` 限定 pathspec）**

```bash
git add tools/check_tlm_freeze.sh .github/workflows/architecture-gates.yml tools/README.md
git add -f .rddf/state/tlm-freeze-base.txt .rddf/state/tlm-shared-files.txt .rddf/state/tlm-freeze-allowlist.txt
git commit -m "ci: TLM-only 代码冻结门禁 + 判定口径三件套 (ADR-088 §2)" -- tools/check_tlm_freeze.sh .github/workflows/architecture-gates.yml tools/README.md .rddf/state/tlm-freeze-base.txt .rddf/state/tlm-shared-files.txt .rddf/state/tlm-freeze-allowlist.txt
git add ip/cpu/plugins/exception.h ip/cpu/plugins/fpu.h ip/cpu/arch/riscv/fpu.h ip/cache/plugins/cache_keys.h && git commit -m "chore: 删除 4 个 0-includer 死代码文件 (TLM 退役 B0 前置)" -- ip/cpu/plugins/exception.h ip/cpu/plugins/fpu.h ip/cpu/arch/riscv/fpu.h ip/cache/plugins/cache_keys.h
git add docs/architecture/adr/ADR-088-tlm-backend-retirement.md docs/architecture/adr/ADR-083-plugin-style-single-source-of-truth.md docs/architecture/adr.md openspec/changes/chmem-tlm-backend-freeze/ && git commit -m "docs: ADR-088 治理锚 (5 项必填) + ADR-083 v1.1 冻结条款与退役时间线" -- docs/architecture/adr/ADR-088-tlm-backend-retirement.md docs/architecture/adr/ADR-083-plugin-style-single-source-of-truth.md docs/architecture/adr.md
```
（批 2 的删除路径已被 `git rm` ⇒ **不要带 `git add`**（`git add <已删路径>` 会 exit 128）；批 1 的三份 state 文件若被 `.gitignore` 覆盖，用 `git add -f`）

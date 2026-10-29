#!/usr/bin/env bash
# tools/run_catch2_with_baseline.sh
#
# 功能描述: ★ **baseline-aware** 的 Catch2 运行器 —— 判定「是否有**新**失败」
#           而非「是否有任何失败」
#
# 作者: ChipForge Build System
# 最后修改日期: 2026-10-28
#
# ---------------------------------------------------------------------------
# ★ 为何存在（C1「旧产物顶替」假绿通道排查的产物）
# ---------------------------------------------------------------------------
# ★ `ctest` 的 `chipforge_tests` / `chipforge_tests_chmem` entry 跑的是**全量**
#   Catch2 binary ⇒ ★ 其中含 baseline-known 失败（见
#   `tools/baseline_known_failures.txt`）⇒ ★ **entry 恒红**
# ⇒ ⇒ ★ 「全量回归通过」从来就不存在 ⇒ ★ 退出码**失去意义**。
# ⇒ ⇒ ★ 本脚本：★ 跑 binary ⇒ ★ 解析失败集合 ⇒ ★ 与清单比对
#   ⇒ ★ **只在出现新失败时**返回非 0。
#
# ---------------------------------------------------------------------------
# ★ 判定规则（★ fail-closed）
# ---------------------------------------------------------------------------
#   · 失败集合 **⊆ 清单**                    ⇒ 退出 0（无新回归）
#   · 失败集合 **⊋ 清单**                    ⇒ 退出 1（有新回归）
#   · **解析出的失败 case 数 ≠ Catch2 汇总行里的 failed 数**
#     （★ 例：进程崩溃 / 解析器漏抓 ⇒ 判定不可信）⇒ 退出 1
#   · Catch2 退出码 **∉ {0, 1}**（★ 崩溃 / 未捕获异常）⇒ 退出 1
#   · 清单里有条目本次**未失败** ⇒ 退出 0，但**打印警告**（可能已修复）
#
# ---------------------------------------------------------------------------
# ★★★ **为什么用 console reporter 而不是 XML reporter**（★ 实测定论）
# ---------------------------------------------------------------------------
# ★ Catch2 的 **XML reporter 会为每个 test case 捕获 stdout/stderr**
#   （★ 实现 = 替换 `std::cerr` 的 rdbuf ⇒ 捕获内容进 `<Output>` 节点）
# ⇒ ⇒ ★ **被测代码自己捕获 `std::cerr` 的测试会被破坏**：
#   ★ 实测：`tests/cpu/test_cpu_factory_isa_xlen.cpp` 的 3 个 case 用
#   ★ `CerrCapture` 检查 CpuFactory 的 `[declared-reserved]` warn
#   ★ ⇒ ★ **console reporter 全过 / XML reporter 3 个全挂**
#   ★ （★ 与顺序/污染无关 ⇒ ★ 单跑亦复现 ⇒ ★ **reporter 差异**，非 flaky）
# ⇒ ⇒ ★ 故本脚本**必须**用 console reporter ⇒ ★ 改为**解析 console 输出**
# ⇒ ⇒ ★ 并用 Catch2 自己的汇总行（`test cases: N | M passed | K failed`）
#   ★ 做**自校验**（★ 解析数 ≠ K ⇒ 判定不可信 ⇒ fail-closed）。
#
# ---------------------------------------------------------------------------
# 用法:
#   tools/run_catch2_with_baseline.sh [--known-list <path>] <binary> [args...]
#   例: tools/run_catch2_with_baseline.sh ./build/bin/chipforge_tests
#   例: tools/run_catch2_with_baseline.sh ./build/bin/chipforge_tests "[cache]"
#
# 退出码:
#   0 = 无新回归（可能含 baseline-known 失败）
#   1 = 有新回归 / 判定不可信 / binary 缺失
#   2 = 用法错误

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
KNOWN_LIST="${SCRIPT_DIR}/baseline_known_failures.txt"

# ★ 可选：--known-list <path>（★ 供测试 / CI 定制清单）
while [[ "${1:-}" == "--known-list" ]]; do
    KNOWN_LIST="$2"
    shift 2
done

if [[ $# -lt 1 ]]; then
    echo "ERROR: usage: $0 [--known-list <path>] <binary> [catch2 args...]"
    exit 2
fi

BINARY="$1"
shift

if [[ ! -x "$BINARY" ]]; then
    echo "ERROR: binary not found or not executable: $BINARY"
    echo "  ⇒ ★ 拒绝回退到旧产物（★ C1：旧产物静默顶替 = 假绿通道）"
    exit 2
fi

# ★★ **产物新鲜度 fail-closed**（★ 2026-10-30 补，★ 审查发现）
#   ★ **缺口**：★ 本脚本原先**只查 `-x`**（★ 存在性）⇒ ★ **不查新鲜度**
#     ⇒ ★ 「直接执行 binary」的路径**绕过** `run_chipforge_tests.sh` 的
#       mtime 校验与 ctest 的 `cf_build_all` fixture ⇒ ★ ★ **三层防护链断在底层**
#     ⇒ ★ ⇒ ★ 本轮**已两次**命中：★ 变异脚本改动未生效 ⇒ 编译失败
#       ⇒ ★ **旧 binary 仍输出「All tests passed」** ⇒ ★ **假绿**
#   ★ **判据**：★ binary 的 mtime MUST 晚于全部编译输入
#     （★ 与 `run_chipforge_tests.sh` 的 `--no-build` 路径**同款**）
#   ★ **MUST NOT** 在有未提交源码改动时误报 ⇒ ★ 只看**文件 mtime**
#     （★ git 的 index mtime 不参与）
#   ★ ★ **按 binary 收窄**（★ 避免误报）：★ TLM binary **不**编译
#     `*_chmem.h`（★ CH_MEM-only ⇒ 含 `<ch.hpp>` ⇒ 绝不可能被 TLM 引用）
#     ⇒ ★ 若不排除 ⇒ ★ **改一个 CH_MEM-only 头就会把 TLM binary 判 stale**
#       ⇒ ★ **误报**（★ 本轮实测命中）
#     ⇒ ★ ⇒ ★ **CH_MEM binary 查全部；★ TLM binary 排除 `*_chmem.h`
#       + ★ CH_MEM-only 的测试文件**
#   ★ ★ **CH_MEM-only 测试的排除清单从 `tests/CMakeLists.txt` 的
#     `CHMEM_ONLY_TEST_SOURCES` 自动提取**（★ ★★ **自动同步**
#     ⇒ ★ 不会与 CMakeLists 漂移）
#   ★ ⚠️ **不使用 bash 数组传 find 的括号表达式**（★ 数组元素不参与 shell
#     转义 ⇒ ★ `\(` 变成字面量 ⇒ ★ find 的括号分组失效）★ ⇒ ★ 直接两套命令
_STALE_ROOTS=( "$PROJECT_ROOT/include" "$PROJECT_ROOT/ip" "$PROJECT_ROOT/src"
               "$PROJECT_ROOT/tests" "$PROJECT_ROOT/bundles" )
if [[ "$(basename "$BINARY")" == *chmem* ]]; then
    STALE_COUNT="$(find "${_STALE_ROOTS[@]}" \
        \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -o -name '*.cxx' \) \
        -newer "$BINARY" 2>/dev/null | wc -l)"
else
    _CHMEM_ONLY_FILES="$(sed -n '/CHMEM_ONLY_TEST_SOURCES/,/^)/p' \
        "$PROJECT_ROOT/tests/CMakeLists.txt" 2>/dev/null \
        | grep -oE '[A-Za-z0-9_.-]+\.(cpp|h|hpp)' | sort -u)"
    _STALE_LIST="$(mktemp -t cf_stale.XXXXXX)"
    _KEEP="$(mktemp -t cf_keep.XXXXXX)"
    trap 'rm -f "${CONSOLE_OUT:-}" "$_STALE_LIST" "$_KEEP"' EXIT
    find "${_STALE_ROOTS[@]}" \
        \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -o -name '*.cxx' \) \
        ! -name '*_chmem.h' -newer "$BINARY" 2>/dev/null > "$_STALE_LIST"
    while IFS= read -r _f; do
        _b="$(basename "$_f")"
        if ! grep -qxF "$_b" <<< "$_CHMEM_ONLY_FILES"; then
            echo "$_f" >> "$_KEEP"
        fi
    done < "$_STALE_LIST"
    STALE_COUNT="$(wc -l < "$_KEEP")"
fi

if [[ "${STALE_COUNT:-0}" -gt 0 ]]; then
    echo "ERROR: binary is STALE (newer sources exist) — refusing to run."
    echo "  binary: $BINARY"
    echo "  ⇒ ★ 编译输入比产物新 ⇒ ★ 产物**未反映**当前源码"
    echo "  ⇒ ★ **旧产物顶替 = 假绿通道**（★ C1）⇒ ★ 先 build 再跑"
    echo "  ⇒ ★（★ 与 run_chipforge_tests.sh 的 --no-build 路径同款判据）"
    exit 2
fi

if [[ ! -f "$KNOWN_LIST" ]]; then
    echo "ERROR: baseline-known list not found: $KNOWN_LIST"
    exit 2
fi

# ★ 按 binary 选节（★ `chipforge_tests_chmem` → `[chmem]`；其余 → `[tlm]`）
BINARY_BASENAME="$(basename "$BINARY")"
if [[ "$BINARY_BASENAME" == *chmem* ]]; then
    KNOWN_SECTION="chmem"
else
    KNOWN_SECTION="tlm"
fi

CONSOLE_OUT="$(mktemp -t cf_catch2_baseline.XXXXXX.log)"
trap 'rm -f "$CONSOLE_OUT"' EXIT

# ★ Catch2 需要 CWD 在项目根（TLM binary 用相对路径读 SoC JSON）
cd "$PROJECT_ROOT"

echo "[baseline] running: $BINARY $*"
# ★ **必须用 console reporter**（★ 理由见文件头）
#   ⇒ ★ 输出保留在文件里供解析 ⇒ ★ 同时 tee 到终端（★ 用户可见）
"$BINARY" "$@" 2>&1 | tee "$CONSOLE_OUT"
CATCH2_EXIT="${PIPESTATUS[0]}"

if [[ ! -s "$CONSOLE_OUT" ]]; then
    echo "[baseline] ERROR: 测试输出为空 ⇒ 判定不可解析 ⇒ fail-closed"
    echo "[baseline]        （binary exit=$CATCH2_EXIT）"
    exit 1
fi

# ★ FULL_RUN：★ 无 tag 过滤 ⇒ 全量 ⇒ ★ 此时「清单条目未失败」才可能意味着
#   「已修复」⇒ ★ 子集跑时该提示是噪音 ⇒ ★ 只在全量时打印。
FULL_RUN=0
[[ $# -eq 0 ]] && FULL_RUN=1

python3 -u - "$CONSOLE_OUT" "$KNOWN_LIST" "$CATCH2_EXIT" "$KNOWN_SECTION" "$FULL_RUN" <<'PY'
import re
import sys

console_path, known_path, catch2_exit = sys.argv[1], sys.argv[2], int(sys.argv[3])
section = sys.argv[4]
full_run = sys.argv[5] == "1"

# ── 读清单（★ 只取本 binary 对应节）──
known = set()
in_section = False
with open(known_path, encoding="utf-8") as fh:
    for line in fh:
        raw = line.strip()
        if raw.startswith("[") and raw.endswith("]"):
            in_section = (raw[1:-1] == section)
            continue
        if not in_section:
            continue
        if raw and not raw.startswith("#"):
            known.add(raw)

with open(console_path, encoding="utf-8", errors="replace") as fh:
    console = fh.read()

lines = console.splitlines()

# ── 解析失败块 ──
# ★ Catch2 v3 console reporter 的失败块格式：
#   ─────────(70+ 个 -)─────────
#   <test case 名>            ← 顶格
#   ─────────(70+ 个 -)─────────
#   <file>:<line>
#   ......(dots)......
#   <file>:<line>: FAILED:
# ★ SECTION 名是**缩进**的 ⇒ 只取顶格行 ⇒ 避免把 SECTION 当 case
# ★ 失败块 = 「分隔线 / 名字 / 分隔线」**成对**出现 ⇒ ★ 用成对模式提取
#   （★ 单看分隔线会把紧随其后的 `<file>:<line>` 行误当成 case 名）
# ★ SECTION 名是**缩进**的 ⇒ 只取顶格行
# ★ ★ **块名可能是多行**：★ 失败发生在 SECTION 内时，Catch2 打印
#   「分隔线 / <test case 名> / <SECTION 名（缩进）> / 分隔线」⇒ ★
#   ★ 单行正则会漏抓 ⇒ ★ 这里允许 1~N 行 ⇒ ★ 取**第一行**（顶格）作 case 名。
BLOCK = re.compile(r"^-{70,}[ \t]*\n((?:[^\n]*\n)+?)^-{70,}[ \t]*$", re.M)
# ★ ★ **必须区分 FAILED 块与 WARN 块**：★ Catch2 v3 对 **warning** 也用
#   「分隔线 / 名字 / 分隔线」格式打印（★ 实测：`PTW_Has_AdvanceFromRealMemory_Method`
#   的 warning 块曾被误判为失败）⇒ ★ 判据 = 块体内含 `: FAILED:`。
blocks = list(BLOCK.finditer(console))
failed = []
for i, m in enumerate(blocks):
    name = m.group(1).splitlines()[0].strip()
    if not name or name.startswith(" "):
        continue
    end = blocks[i + 1].start() if i + 1 < len(blocks) else len(console)
    body = console[m.end():end]
    if ": FAILED:" in body:
        failed.append(name)

failed_set = set(failed)

# ── Catch2 汇总行自校验（★ 解析数必须等于 Catch2 自己报的 failed 数）──
# ★★★ **Catch2 有两种汇总形态，MUST 都支持**（★ 实测踩坑）：
#   ① **有失败**：`test cases: 515 | 514 passed | 1 failed`
#   ② **全通过**：`All tests passed (67466 assertions in 515 test cases)`
#      ⇒ ★ 形态 ② **没有** `test cases:` 行 ⇒ ★ 只匹配形态 ① ⇒ ★ 全绿时
#      ★   会误判「找不到汇总行 ⇒ fail-closed」（★ 实测命中）
m_fail = re.search(
    r"test cases:\s*(\d+)\s*\|\s*(\d+)\s+passed\s*\|\s*(\d+)\s+failed",
    console,
)
m_ok = re.search(
    r"All tests passed\s*\((\d+)\s+assertions? in (\d+)\s+test cases?\)",
    console,
)
if m_fail:
    reported_failures = int(m_fail.group(3))
elif m_ok:
    reported_failures = 0
else:
    print("[baseline] ERROR: 未找到 Catch2 汇总行 ⇒ 判定不可信 ⇒ fail-closed")
    print("[baseline]        （binary exit=%d ⇒ 可能崩溃 / 未捕获异常）" % catch2_exit)
    sys.exit(1)

new_failed = sorted(failed_set - known)
known_hit = sorted(failed_set & known)
known_stale = sorted(known - failed_set)   # ★ 清单里有、但本次没失败 ⇒ 可能已修复

print("[baseline] --------------------------------------------------")
print(f"[baseline] failed cases(parsed) : {len(failed_set)}")
print(f"[baseline] reported failures    : {reported_failures}")
print(f"[baseline] baseline-known hit   : {len(known_hit)} {known_hit}")
if known_stale and full_run:
    print("[baseline] WARNING: baseline-known 条目本次未失败 ⇒ 可能已修复 ⇒ 建议清理清单:")
    for name in known_stale:
        print(f"[baseline]          - {name}")
if new_failed:
    print(f"[baseline] NEW failures ({len(new_failed)}) ⇒ ★ 有新回归:")
    for name in new_failed:
        print(f"[baseline]          - {name}")
if reported_failures != len(failed_set):
    print(
        f"[baseline] ERROR: 解析出的失败 case 数 ({len(failed_set)}) ≠ Catch2 汇总的 "
        f"failed 数 ({reported_failures}) ⇒ 判定不可信 ⇒ fail-closed"
    )
print("[baseline] --------------------------------------------------")

# ★ fail-closed 判定
if new_failed:
    sys.exit(1)
if reported_failures != len(failed_set):
    sys.exit(1)
if catch2_exit not in (0, 1):
    print(f"[baseline] ERROR: Catch2 异常退出码 {catch2_exit} ⇒ fail-closed")
    sys.exit(1)
sys.exit(0)
PY
PY_STATUS=$?

if [[ $PY_STATUS -eq 0 ]]; then
    echo "[baseline] RESULT: 无新回归 ✔"
else
    echo "[baseline] RESULT: 有新回归 ✘（详见上方 NEW failures）"
fi
# ★ 输出到**文件**时 bash 是块缓冲 ⇒ ★ 若进程被强杀 ⇒ 判定行会丢
#   ⇒ ★ 显式 flush（★ 实测踩坑：★ 后台任务被进程组清理 ⇒ 判定输出丢失）
sync
exit $PY_STATUS

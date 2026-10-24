#!/usr/bin/env bash
# tools/measure_branchbench.sh — 分支微基准测量与基线漂移检测 (CH_MEM/Verilator 后端)
#
# 用途 (chmem-branch-microbench, 修正版 chmem-branch-microbench-fix):
#   1) 跑 4 档**可控预测率**分支负载 (mode 0/1/2/3) => TOHOST(=校验和) / CYCLES
#   2) **结果正确性**: TOHOST 携带 32-bit 校验和 (低 31 位, LSB 恒 1) =>
#      必须与基线 CSV 的期望值**逐位相等** (结果错 => 立即失败, 绝不产出数值)
#   3) **工作量一致性 (AC-F1-2)**: 常驻 `pc_changes` 计数 (取指地址变化的 cycle 数)
#      4 档必须**逐位相同** => 指令数一致; cycles 与 pc_changes 之差 = 气泡拍数
#   4) --check-baseline: cycles 与基线**精确相等** (无容差) + workload_sha 匹配
#
# ★设计要点 (为什么可测):
#   4 档的 .text **逐字节相同** (唯一差异 = 数据里的 mode 字) ⇒ 静态指令流一致.
#   ★ 但 `.text 相同` **只**保证静态一致 —— 动态指令数一致性由 `pc_changes`
#     常驻计数证明 (见 AC-F1-2), 不由 .text 推断 (原设计的该推论已被反汇编证伪).
#   分支两条路径**等长** + `branch_cond` **无分支** (位运算 select) ⇒ 无论档位,
#   每迭代执行的指令数恒等 ⇒ cycle 差**只**来自分支方向 (气泡), 不混入工作量差.
#
# ★基线列访问 (修正): **按表头名取列** —— 原实现按**位置**取列 ($2/$3),
#   而 notes 字段含逗号 ⇒ 一旦新增列或 notes 含逗号即**静默错位**.
#   分组键 = (mode_name, bp_mode); workload_sha / build_id 作漂移守卫.
#
# 用法:
#   bash tools/measure_branchbench.sh                    # 测量并打印
#   bash tools/measure_branchbench.sh --check-baseline   # 与基线比对, 不一致则 exit 1
#
# 环境变量: BENCH_SIM / BENCH_BASELINE / BENCH_MAX_CYCLES / BENCH_ELF_DIR / BENCH_BP
set -uo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

SIM="${BENCH_SIM:-build/bin/cpu_verilator_sim}"
BASELINE="${BENCH_BASELINE:-soc/cpu/docs/dse/branchbench-baseline-matrix.csv}"
MAX_CYCLES="${BENCH_MAX_CYCLES:-2000000}"
ELF_DIR="${BENCH_ELF_DIR:-build}"
# bp_mode: which branch-predictor configuration the measurement applies to.
# The no-BP reference rows carry "off"; a BP-enabled rerun (前置2 / ②) appends
# rows with "on" (and its own workload_sha/build_id).
BP_MODE="${BENCH_BP:-off}"
CHECK_ONLY=0
[ "${1:-}" = "--check-baseline" ] && CHECK_ONLY=1

MODES=(0 1 2 3)
# ★ 档名与**硬件分支方向**一致 (chmem-branch-microbench-fix 互换重命名):
#   mode0: cond=0 => `bne` 不 taken => always_not_taken
#   mode1: cond=1 => `bne` taken     => always_taken
MODE_NAMES=(always_not_taken always_taken pattern_6of8 pseudo_random)

fail() { echo "::warning file=soc/cpu/docs/dse/branchbench-baseline-matrix.csv::FAIL: $*"; exit 1; }

[ -x "$SIM" ] || fail "缺少可执行 $SIM (先 cmake --build build)"
[ -f "$BASELINE" ] || fail "缺少基线 $BASELINE"

# ---- 按表头名取列 (修正: 原按位置取列会在 notes 含逗号 / 新增列时静默错位) ----
# ★ notes 字段含逗号 ⇒ 必须用**引号感知**的 CSV 解析 (awk -F',' 会把引号内的逗号
#   当分隔符 ⇒ bp_mode 等位于 notes 之后的列全部错位).
csv_field() { # $1 = csv 行, $2 = 1-based 列索引 => 该字段值
  awk -v want="$2" 'BEGIN{FS=""} {
    n=0; field=""; in_q=0
    for (i=1;i<=length($0);i++) {
      c=substr($0,i,1)
      if (in_q) {
        if (c=="\"") {
          if (substr($0,i+1,1)=="\"") { field=field "\""; i++ }
          else in_q=0
        } else field=field c
      } else {
        if (c=="\"") in_q=1
        else if (c==",") { n++; if (n==want) { print field; exit } field="" }
        else field=field c
      }
    }
    n++
    if (n==want) print field
  }' <<<"$1"
}
col_index() { # $1 = 列名 => 1-based 索引 (0 = 不存在)
  head -1 "$BASELINE" | tr -d '\r' | awk -v want="$1" 'BEGIN{FS=""} {
    n=0; field=""; in_q=0
    for (i=1;i<=length($0);i++) {
      c=substr($0,i,1)
      if (in_q) { if (c=="\"") { if (substr($0,i+1,1)=="\"") { field=field "\""; i++ } else in_q=0 } else field=field c }
      else { if (c=="\"") in_q=1; else if (c==",") { n++; if (field==want) { print n; exit } field="" } else field=field c }
    }
    n++
    if (field==want) print n
  }'
}
# 取 (mode_name, bp_mode) 精确匹配的行
base_row() { # $1 = mode_name, $2 = bp_mode
  local ni="$IDX_NAME" bi="$IDX_BPMODE"
  [ -n "$ni" ] && [ -n "$bi" ] || return 0
  local n=0 line
  while IFS= read -r line; do
    n=$((n+1))
    [ "$n" -eq 1 ] && continue
    local nm bm
    nm=$(csv_field "$line" "$ni")
    bm=$(csv_field "$line" "$bi")
    if [ "$nm" = "$1" ] && [ "$bm" = "$2" ]; then printf '%s' "$line"; return 0; fi
  done < "$BASELINE"
}
field() { csv_field "$1" "$2"; }

IDX_NAME=$(col_index mode_name)
IDX_TOHOST=$(col_index expected_tohost)
IDX_CYCLES=$(col_index cycles)
IDX_BPMODE=$(col_index bp_mode)
IDX_WSHA=$(col_index workload_sha)
IDX_BID=$(col_index build_id)
[ -n "$IDX_NAME" ] && [ -n "$IDX_TOHOST" ] && [ -n "$IDX_CYCLES" ] \
  || fail "基线表头缺少必需列 (mode_name / expected_tohost / cycles)"

# ---- workload_sha 守卫: 基线记录的 .text sha 必须与当前 ELF 一致 ----
OBJCOPY="${BENCH_OBJCOPY:-}"
if [ -z "$OBJCOPY" ]; then
  for c in riscv32-unknown-elf-objcopy /workspace/main/opt/riscv/bin/riscv32-unknown-elf-objcopy; do
    command -v "$c" >/dev/null 2>&1 && OBJCOPY="$c" && break
    [ -x "$c" ] && OBJCOPY="$c" && break
  done
fi

for m in "${MODES[@]}"; do
  [ -f "$ELF_DIR/branchbench_m${m}.elf" ] || fail "缺少 $ELF_DIR/branchbench_m${m}.elf (CMake 目标 branchbench_elfs)"
done

# 当前 build 的 .text sha (4 档共用 => 取 mode0)
cur_wsha=""
if [ -n "$OBJCOPY" ] && [ -n "$IDX_WSHA" ]; then
  "$OBJCOPY" -O binary --only-section=.text "$ELF_DIR/branchbench_m0.elf" "$ELF_DIR/.bb_m0.text" 2>/dev/null \
    && cur_wsha=$(sha256sum "$ELF_DIR/.bb_m0.text" | cut -c1-16)
  rm -f "$ELF_DIR/.bb_m0.text"
fi

rc=0
first_pc_changes=""
for i in "${!MODES[@]}"; do
  m="${MODES[$i]}"; name="${MODE_NAMES[$i]}"
  elf="$ELF_DIR/branchbench_m${m}.elf"

  # ★ 捕获 stderr (修正: 原为 2>/dev/null => flag 未生效却测出「无差异」的假绿通道开着)
  # ★ 注意: **不把 runner 退出码当错误** —— branchbench 的 tohost 携带校验和 (≠1),
  #   而 runner 的退出码判据是 tohost==1 => 必然非零. 正确性由宿主侧的校验和比对决定.
  raw=$("$SIM" --elf "$elf" --cycles "$MAX_CYCLES" --stats 2>&1)
  out=$(printf '%s' "$raw" | grep -oE "TOHOST=[0-9]+ CYCLES=[0-9]+[^[:space:]]*" | head -1)
  [ -n "$out" ] || fail "mode $m ($name): 无结果行 —— 输出: $(printf '%s' "$raw" | tail -3)"

  tohost=$(printf '%s' "$out" | grep -oE 'TOHOST=[0-9]+' | cut -d= -f2)
  cycles=$(printf '%s' "$out" | grep -oE 'CYCLES=[0-9]+' | cut -d= -f2)
  status=$(printf '%s' "$out" | grep -oE '(PASS|FAIL|TIMEOUT)' | head -1)
  pc_changes=$(printf '%s' "$raw" | grep -oE 'pc_changes=[0-9]+' | head -1 | cut -d= -f2)

  # ---- 结果正确性 (硬闸): 校验和必须与基线期望值逐位相等 ----
  row=$(base_row "$name" "$BP_MODE")
  if [ -z "$row" ]; then
    if [ "$BP_MODE" = "off" ]; then
      fail "基线 $BASELINE 无 (${name}, bp_mode=${BP_MODE}) 行 (mode $m)"
    else
      # BP 档尚无基线 => 打印但不做比对 (前置2 / ② 追加基线行后生效)
      printf "branchbench: mode=%s %-16s bp=%s tohost=%-10s cycles=%-8s pc_changes=%-8s (无基线行 => 仅打印)\n" \
        "$m" "$name" "$BP_MODE" "$tohost" "$cycles" "${pc_changes:-?}"
      continue
    fi
  fi
  exp_tohost=$(field "$row" "$IDX_TOHOST")
  base_cycles=$(field "$row" "$IDX_CYCLES")

  [ "$tohost" = "$exp_tohost" ] \
    || fail "mode $m ($name): 校验和不符 —— 实测 $tohost != 基线 $exp_tohost (结果错, 不得产出数值)"
  [ "$status" != "TIMEOUT" ] \
    || fail "mode $m ($name): TIMEOUT (cycles>=$MAX_CYCLES) —— 程序未跑完, 不得产出数值"

  # ---- workload_sha 守卫 ----
  if [ -n "$IDX_WSHA" ] && [ -n "$cur_wsha" ]; then
    base_wsha=$(field "$row" "$IDX_WSHA")
    if [ -n "$base_wsha" ] && [ "$base_wsha" != "$cur_wsha" ]; then
      fail "mode $m ($name): workload_sha 不符 —— 基线 $base_wsha != 当前 .text $cur_wsha (基线属于另一份代码, 须显式重录)"
    fi
  fi

  bubbles=""
  if [ -n "$pc_changes" ]; then
    # 工作量一致性: 4 档 pc_changes 必须相同 (AC-F1-2)
    if [ -z "$first_pc_changes" ]; then
      first_pc_changes="$pc_changes"
    elif [ "$pc_changes" != "$first_pc_changes" ]; then
      fail "mode $m ($name): pc_changes=$pc_changes != $first_pc_changes ($(printf '%s' "${MODE_NAMES[0]}")) —— ★ 工作量不一致 (基准失效)"
    fi
    bubbles=$(( cycles - pc_changes ))
  fi

  printf "branchbench: mode=%s %-16s bp=%s tohost=%-10s cycles=%-8s pc_changes=%-8s bubbles=%-6s checksum-ok\n" \
    "$m" "$name" "$BP_MODE" "$tohost" "$cycles" "${pc_changes:-?}" "${bubbles:-?}"

  if [ "$CHECK_ONLY" -eq 1 ] && [ "$base_cycles" != "$cycles" ]; then
    echo "::warning file=soc/cpu/docs/dse/branchbench-baseline-matrix.csv::FAIL: mode $m ($name) 基线漂移: 基线 $base_cycles != 实测 $cycles (精确相等口径; 漂移须显式重录基线 + commit 说明原因)"
    rc=1
  fi
done

if [ "$rc" -ne 0 ]; then exit 1; fi
if [ "$CHECK_ONLY" -eq 1 ]; then
  echo "branchbench: 基线一致 (4 档, bp_mode=$BP_MODE) => PASS"
fi
exit 0

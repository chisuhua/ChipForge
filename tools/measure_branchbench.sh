#!/usr/bin/env bash
# tools/measure_branchbench.sh — 分支微基准测量与基线漂移检测 (CH_MEM/Verilator 后端)
#
# 用途 (chmem-branch-microbench):
#   1) 跑 4 档**可控预测率**分支负载 (mode 0/1/2/3) => TOHOST(=校验和) / CYCLES
#   2) **结果正确性**: TOHOST 携带 32-bit 校验和 (低 31 位, LSB 恒 1) =>
#      必须与基线 CSV 的期望值**逐位相等** (结果错 => 立即失败, 绝不产出数值)
#   3) --check-baseline: cycles 与基线**精确相等** (无容差)
#
# ★设计要点 (为什么可测):
#   4 档的 .text **逐字节相同** (唯一差异 = 数据里的 mode 字) ⇒ 每档执行的
#   指令数完全一致 ⇒ cycle 差**只**来自分支方向 ⇒ Δcycles 可作为 BP 收益的度量。
#   (已实测: mode0/1 = 方向恒定; mode2 = 周期性变化; mode3 = 伪随机)
#
# 用法:
#   bash tools/measure_branchbench.sh                    # 测量并打印
#   bash tools/measure_branchbench.sh --check-baseline   # 与基线比对, 不一致则 exit 1
#
# 环境变量: BENCH_SIM / BENCH_BASELINE / BENCH_MAX_CYCLES / BENCH_ELF_DIR
set -uo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

SIM="${BENCH_SIM:-build/bin/cpu_verilator_sim}"
BASELINE="${BENCH_BASELINE:-soc/cpu/docs/dse/branchbench-baseline-matrix.csv}"
MAX_CYCLES="${BENCH_MAX_CYCLES:-2000000}"
ELF_DIR="${BENCH_ELF_DIR:-build}"
CHECK_ONLY=0
[ "${1:-}" = "--check-baseline" ] && CHECK_ONLY=1

MODES=(0 1 2 3)
MODE_NAMES=(always_taken always_not_taken pattern_6of8 pseudo_random)

fail() { echo "::warning file=soc/cpu/docs/dse/branchbench-baseline-matrix.csv::FAIL: $*"; exit 1; }

[ -x "$SIM" ] || fail "缺少可执行 $SIM (先 cmake --build build)"
for m in "${MODES[@]}"; do
  [ -f "$ELF_DIR/branchbench_m${m}.elf" ] || fail "缺少 $ELF_DIR/branchbench_m${m}.elf (CMake 目标 branchbench_elfs)"
done

rc=0
for i in "${!MODES[@]}"; do
  m="${MODES[$i]}"; name="${MODE_NAMES[$i]}"
  elf="$ELF_DIR/branchbench_m${m}.elf"
  out=$("$SIM" --elf "$elf" --cycles "$MAX_CYCLES" 2>/dev/null | grep -oE "TOHOST=[0-9]+ CYCLES=[0-9]+[^$]*")
  [ -n "$out" ] || fail "mode $m ($name): cpu_verilator_sim 无输出"

  tohost=$(printf '%s' "$out" | grep -oE 'TOHOST=[0-9]+' | cut -d= -f2)
  cycles=$(printf '%s' "$out" | grep -oE 'CYCLES=[0-9]+' | cut -d= -f2)
  status=$(printf '%s' "$out" | grep -oE '(PASS|FAIL|TIMEOUT)' | head -1)

  # ---- 结果正确性 (硬闸): 校验和必须与基线期望值逐位相等 ----
  exp_tohost=$(grep -E "^${name}," "$BASELINE" 2>/dev/null | head -1 | awk -F',' '{print $2}')
  if [ -z "$exp_tohost" ]; then
    fail "基线 $BASELINE 无 ${name} 行 (mode $m)"
  fi
  if [ "$tohost" != "$exp_tohost" ]; then
    fail "mode $m ($name): 校验和不符 —— 实测 TOHOST=$tohost != 基线期望 $exp_tohost (结果错, 不得产出数值)"
  fi
  if [ "$status" = "TIMEOUT" ]; then
    fail "mode $m ($name): TIMEOUT (cycles>=$MAX_CYCLES) —— 程序未跑完, 不得产出数值"
  fi

  base_cycles=$(grep -E "^${name}," "$BASELINE" | head -1 | awk -F',' '{print $3}')
  drift=""
  [ "$CHECK_ONLY" -eq 1 ] && [ "$base_cycles" != "$cycles" ] && drift=" (基线漂移: 基线 $base_cycles != 实测 $cycles)"

  # 注意: runner 的 PASS/FAIL 标签对本基准**无意义** (它的判据是 tohost==1,
  # 而本基准的 tohost 携带校验和 => 必然 !=1) => 判定权在**宿主侧校验和比对**,
  # 故此处只报自己的结论, 避免误读.
  printf "branchbench: mode=%s %-16s tohost=%-10s cycles=%-8s checksum-ok\n" "$m" "$name" "$tohost" "$cycles"
  if [ -n "$drift" ]; then
    echo "::warning file=soc/cpu/docs/dse/branchbench-baseline-matrix.csv::FAIL: mode $m ($name)$drift (精确相等口径; 漂移须显式重录基线 + commit 说明原因)"
    rc=1
  fi
done

if [ "$rc" -ne 0 ]; then exit 1; fi
if [ "$CHECK_ONLY" -eq 1 ]; then
  echo "branchbench: 基线一致 (4 档) => PASS"
fi
exit 0

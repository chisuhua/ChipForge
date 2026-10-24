#!/usr/bin/env bash
# tools/measure_perf.sh — CH_MEM/Verilator 后端性能测量与基线漂移检测
#
# 用途 (chmem-verilator-perf-harness 交付点 A):
#   1) 用 cpu_verilator_sim 跑 Dhrystone (CH_MEM/Verilator 后端) => TOHOST / CYCLES
#   2) 判「程序结果正确」: TOHOST 必须 == 1 (dhrystone.c:283 tohost_pass() 写 1 后
#      死循环 => TOHOST==1 等价于 2000 次迭代全部完成; TIMEOUT 一律判失败, 哪怕进程
#      退出码为 0 —— 实测 cpu_verilator_sim 超时也 exit 0)
#   3) 算 DMIPS/MHz = N*1e6/(1757*cycles)  (N=2000, dhrystone.c:230 Number_Of_Runs)
#   4) --check-baseline: 与基线 CSV 逐位比对 (精确相等, **无容差**)
#
# 口径纪律 (ADR-088 §3): CH_MEM/Verilator 的 cycle 数与 TLM **不可比**
#   (CH_MEM 是全组合通路, 无流水线转发 => 实测 CH_MEM 914544 vs TLM 794478,
#    CH_MEM 反而 **多** 15% —— 与「组合式更快」的直觉相反, 已在 plan 记录)
#
# 用法:
#   bash tools/measure_perf.sh                     # 测量并打印
#   bash tools/measure_perf.sh --check-baseline   # 与基线比对, 不一致则 exit 1
#
# 环境变量 (可覆盖):
#   DH_ELF / CPU_VERILATOR_SIM / PERF_BASELINE / PERF_MAX_CYCLES / DH_RUNS
set -uo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

ELF="${DH_ELF:-build/dhrystone.elf}"
SIM="${CPU_VERILATOR_SIM:-build/bin/cpu_verilator_sim}"
BASELINE="${PERF_BASELINE:-soc/cpu/docs/dse/verilator-perf-baseline-matrix.csv}"
MAX_CYCLES="${PERF_MAX_CYCLES:-5000000}"
RUNS="${DH_RUNS:-2000}"          # dhrystone.c:230 编译期常量
CHECK_ONLY=0
[ "${1:-}" = "--check-baseline" ] && CHECK_ONLY=1

fail() { echo "::warning file=soc/cpu/docs/dse/verilator-perf-baseline-matrix.csv::FAIL: $*"; exit 1; }

[ -x "$SIM" ] || fail "缺少可执行 $SIM (先 cmake --build build)"
[ -f "$ELF" ] || fail "缺少 $ELF (CMake 目标 dhrystone_elf; 它是 **可复现构建产物**, 勿用仓库内手工副本)"

# ---- 1) 跑测量 ----
out=$("$SIM" --elf "$ELF" --cycles "$MAX_CYCLES" 2>/dev/null | grep -oE "TOHOST=[0-9]+ CYCLES=[0-9]+[^$]*")
[ -n "$out" ] || fail "cpu_verilator_sim 无输出 (elf=$ELF cycles=$MAX_CYCLES)"

tohost=$(printf '%s' "$out" | grep -oE 'TOHOST=[0-9]+' | cut -d= -f2)
cycles=$(printf '%s' "$out" | grep -oE 'CYCLES=[0-9]+' | cut -d= -f2)
status=$(printf '%s' "$out" | grep -oE '(PASS|TIMEOUT)' | head -1)

# ---- 2) 结果正确性 (硬闸: TOHOST==1; TIMEOUT 一律失败) ----
if [ "$status" = "TIMEOUT" ]; then
  fail "TIMEOUT (cycles>=$MAX_CYCLES, tohost=$tohost) —— 程序未跑完, 不得产出数值"
fi
if [ "$tohost" != "1" ]; then
  fail "TOHOST=$tohost (期望 1) —— 程序未正常完成 (dhrystone.c:283 tohost_pass() 未执行)"
fi

# ---- 3) DMIPS ----
dmips=$(awk -v n="$RUNS" -v c="$cycles" 'BEGIN{printf "%.5f", n*1e6/(1757*c)}')

echo "perf: elf=$ELF tohost=$tohost cycles=$cycles dmips_per_mhz=$dmips status=${status:-?}"

# ---- 4) 基线比对 (精确相等, 无容差) ----
if [ "$CHECK_ONLY" -eq 1 ]; then
  [ -f "$BASELINE" ] || fail "缺少基线文件 $BASELINE"
  base_cycles=$(grep -E '^dhrystone,' "$BASELINE" | head -1 | awk -F',' '{print $7}')
  base_dmips=$(grep -E '^dhrystone,' "$BASELINE" | head -1 | awk -F',' '{print $5}')
  [ -n "$base_cycles" ] || fail "基线 $BASELINE 无 dhrystone 行"
  if [ "$base_cycles" != "$cycles" ]; then
    fail "基线漂移: cycles 实测 $cycles != 基线 $base_cycles (精确相等口径; 漂移须**显式重录基线 + commit 说明原因**, 不得用容差吸收)"
  fi
  if [ "$base_dmips" != "$dmips" ]; then
    fail "基线漂移: dmips 实测 $dmips != 基线 $base_dmips"
  fi
  echo "perf: 基线一致 (cycles=$cycles dmips=$dmips) => PASS"
fi
exit 0

#!/usr/bin/env bash
# tools/measure_branchbench.sh — 分支微基准测量与基线漂移检测 (CH_MEM/Verilator 后端)
#
# 用途 (chmem-branch-microbench, 修正版 chmem-branch-microbench-fix):
#   1) 跑 4 档**可控预测率**分支负载 (mode 0/1/2/3) => TOHOST(=校验和) / CYCLES
#   2) **结果正确性**: TOHOST 携带 32-bit 校验和 (低 31 位, LSB 恒 1) =>
#      必须与基线 CSV 的期望值**逐位相等** (结果错 => 立即失败, 绝不产出数值)
#   3) **工作量一致性 (AC-F1-2)**: 承重证据 = **静态结构** (档位选择无分支 + 分支两路径等长
#      ⇒ 4 档执行同一指令序列 ⇒ 动态指令数结构上必然相同).
#      常驻 `pc_changes` (PC 轨迹变化点数) 4 档相同 ⇒ **前端活动量**一致 (动态佐证).
#      ⚠️ `pc_changes` **不是**已执行指令数 (采样流含非架构路径上的 PC 值, 机制未确认);
#      `cycles - pc_changes` 的语义**未确认** (实测回边 taken 不计入)
#      ⇒ **MUST NOT** 作为任何结论的依据.
#   4) --check-baseline: cycles 与基线**精确相等** (无容差) + workload_sha 匹配
#
# ★设计要点 (为什么可测):
#   4 档的 .text **逐字节相同** (唯一差异 = 数据里的 mode 字) ⇒ 静态指令流一致.
#   ★ 但 `.text 相同` **只**保证静态一致 —— 动态指令数一致性由 **AC-F1-1 的静态结构论证**
#     证明 (无分支的档位选择 + 等长的两条路径), **不由** .text 推断
#     (原设计的该推论已被反汇编证伪), 也**不由** `pc_changes` 推断.
#   分支两条路径**等长** + `branch_cond` **无分支** (位运算 select) ⇒ 无论档位,
#   每迭代执行的指令数恒等 ⇒ cycle 差**不**来自工作量差异 (方向/重定向机制待查).
#
# ★基线列访问 (修正): **按表头名取列** —— 原实现按**位置**取列 ($2/$3),
#   而 notes 字段含逗号 ⇒ 一旦新增列或 notes 含逗号即**静默错位**.
#   分组键 = (mode_name, bp_mode); workload_sha 作**硬**漂移守卫 (fail-closed); build_id 仅记录.
#
# 用法:
#   bash tools/measure_branchbench.sh                       # 测量并打印 (BP=off)
#   bash tools/measure_branchbench.sh --check-baseline      # 与基线比对, 不一致则 exit 1
#   bash tools/measure_branchbench.sh --check-bp-direction  # ★ BP 方向性判据 (强制 BP=on)
#   bash tools/measure_branchbench.sh --bp=on|off [...]      # 指定 BP 状态 (默认 off)
#   bash tools/measure_branchbench.sh --help
#
# ★ bp_mode 的两条设定途径（等价）：`--bp=on|off` 或环境变量 `BENCH_BP=on|off`
#   （brief v2.2 §5.3 定稿写作 `--bp=on|off`；本脚本两种都支持）。
#   ★ `--check-bp-direction` 会**强制** BP=on（方向性判据只在 BP 开态有意义）。
#
# 环境变量: BENCH_SIM / BENCH_BASELINE / BENCH_MAX_CYCLES / BENCH_ELF_DIR / BENCH_BP
set -uo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

SIM="${BENCH_SIM:-build/bin/cpu_verilator_sim}"
BASELINE="${BENCH_BASELINE:-soc/cpu/docs/dse/branchbench-baseline-matrix.csv}"
MAX_CYCLES="${BENCH_MAX_CYCLES:-2000000}"
ELF_DIR="${BENCH_ELF_DIR:-build}"
# bp_mode: which branch-predictor configuration the measurement applies to.
#   off = BP **关闭**（无 BP 参照行，基线 CSV 的默认行）
#   on  = BP **开启**（前置2 / ② 追加的行，带自己的 workload_sha/build_id）
# ★ 命名统一为 **--no-bp / --btb-entries**（架构简报 v2.2 §5.3 定稿）：
#   BP **默认开** ⇒ 「关闭」用 `--no-bp` ⇒ 不保留 `--enable-bp`（冗余且有歧义）
BP_MODE="${BENCH_BP:-off}"
CHECK_ONLY=0
CHECK_BP_DIR=0

MODES=(0 1 2 3)
# ★ 档名与**硬件分支方向**一致 (chmem-branch-microbench-fix 互换重命名):
#   mode0: cond=0 => `bne` 不 taken => always_not_taken
#   mode1: cond=1 => `bne` taken     => always_taken
MODE_NAMES=(always_not_taken always_taken pattern_6of8 pseudo_random)

# ★ 与基线 CSV 无关的失败**不挂** CSV 文件注解（否则 GitHub annotation 指向错文件）
fail_plain() { echo "FAIL: $*"; exit 1; }
# ★ 基线内容类失败挂 CSV 文件注解（便于定位需重录的行）
fail() { echo "::warning file=soc/cpu/docs/dse/branchbench-baseline-matrix.csv::FAIL: $*"; exit 1; }

usage() { awk '/^# 用法:/{f=1} f && /^set -uo/{exit} f{sub(/^# ?/,""); print}' "$0"; }

# ★★ `BENCH_BP` **原始值**校验 —— MUST 发生在**任何覆盖之前**（critic 八轮 B-1）：
#   上一版把校验放在「flag / bpdir 已覆盖 BP_MODE 之后」⇒ `BENCH_BP=oon --bp=on`
#   与 `BENCH_BP=oon --check-bp-direction` 两条路径把非法值**静默掩盖**成 on ⇒
#   exit 0 PASS（与「env 与 flag 对称 fail-closed」的声称直接矛盾）。
ENV_BP_RAW="${BENCH_BP:-}"
ENV_BP_SET=0
if [ -n "$ENV_BP_RAW" ]; then
  ENV_BP_SET=1
  case "$ENV_BP_RAW" in
    on|off) ;;
    *) fail_plain "BENCH_BP 取值非法: '$ENV_BP_RAW' (只接受 on|off)" ;;
  esac
fi

# ★ 参数解析 **fail-closed** (critic SHOULD_FIX-1/2):
#   ① 未知参数 MUST 报错（否则静默降级为「测量并打印」并 exit 0 ⇒ ctest 假绿通道）；
#   ② **模式只能给一个** —— 否则 `--check-baseline --check-bp-direction` 这类
#      **最容易被误敲**的组合会只跑第一个 ⇒ 给出一个**不含任何方向性判据的 PASS**；
#   ③ `--bp=on|off` 可与模式组合（brief v2.2 §5.3 定稿的用法 `--check-baseline --bp=on`）。
MODE_ARG=""
BP_ARG_SET=0
for arg in "$@"; do
  case "$arg" in
    ""|--measure)
      [ -z "$MODE_ARG" ] || fail_plain "模式重复: '$*' (模式只接受一个)"
      MODE_ARG=measure ;;
    # ★ 有意短路（critic 七轮 NIT-1）: help 立即退出、**忽略**其后的参数
    #   （惯例; 与脚本其余 fail-closed 教义不同 —— 此处显式声明是有意为之）
    -h|--help) usage; exit 0 ;;
    --check-baseline)
      [ -z "$MODE_ARG" ] || fail_plain "模式重复: '$*' (模式只接受一个)"
      MODE_ARG=baseline ;;
    --check-bp-direction)
      [ -z "$MODE_ARG" ] || fail_plain "模式重复: '$*' (模式只接受一个)"
      MODE_ARG=bpdir ;;
    --bp=on|--bp=off)
      # ★ 重复给 --bp ⇒ MUST fail（否则「后者静默生效」⇒ 脚本 `--bp=on ... --bp=off`
      #   会跑成 BP=off 却让人以为验的是 BP=on ⇒ 假绿 —— 与「参数拼错静默降级」同类）
      [ "$BP_ARG_SET" -eq 0 ] || fail_plain "--bp 重复: '$*' (只能给一次)"
      BP_ARG_SET=1
      BP_MODE="${arg#--bp=}" ;;
    *) fail_plain "未知参数: '$arg' (见 --help)" ;;
  esac
done
# ★ env / flag **冲突** MUST fail-closed（critic 八轮 SF-2：原来 flag 静默赢，
#   而「重复 --bp=on --bp=on 同值」都 fail ⇒ 严格度不一致）
if [ "$ENV_BP_SET" -eq 1 ] && [ "$BP_ARG_SET" -eq 1 ] && [ "$ENV_BP_RAW" != "$BP_MODE" ]; then
  fail_plain "BENCH_BP=$ENV_BP_RAW 与 --bp=$BP_MODE 冲突 (两条途径等价, MUST NOT 静默择一)"
fi
case "$MODE_ARG" in
  ""|measure) ;;
  baseline) CHECK_ONLY=1 ;;
  bpdir)
    # ★ BP 方向性判据（critic B-2: 机制指纹 MUST 有**自动化**锚，不能只活在
    #   commit message / 手工表格里）。★ 强制 BP=on（方向性判据只在 BP 开态有意义）。
    # ★ 矛盾输入 MUST fail-closed（**两条途径都查** —— critic 八轮 SF-1：
    #   上一版只查 flag ⇒ `BENCH_BP=off --check-bp-direction` 静默跑成 on）
    if [ "$BP_ARG_SET" -eq 1 ] && [ "$BP_MODE" != "on" ]; then
      fail_plain "--check-bp-direction 强制 BP=on, 与 --bp=$BP_MODE 矛盾 (去掉 --bp 或写 --bp=on)"
    fi
    if [ "$ENV_BP_SET" -eq 1 ] && [ "$ENV_BP_RAW" != "on" ]; then
      fail_plain "--check-bp-direction 强制 BP=on, 与 BENCH_BP=$ENV_BP_RAW 矛盾 (unset 或写 on)"
    fi
    CHECK_BP_DIR=1
    BP_MODE=on
    ;;
esac

# ★★ BP_MODE 取值 MUST 校验（critic 七轮 BLOCKER）—— `--bp=on|off` 走 case 匹配
#   （非法值 fail-closed），但**环境变量渠道**原来不校验 ⇒ `BENCH_BP=oon` 会：
#   ① 4 档全走「无基线行 ⇒ 仅打印」⇒ **0 条比对却报 PASS**；
#   ② sim 只认 `off` ⇒ `oon` 不传 `--no-bp`（实际跑 BP=on）而标签写 `oon`；
#   ③ `eff_bp` 生效值断言两个分支都要求精确 on/off ⇒ **断言被跳过**。
#   ⇒ 与「flag 拼错 fail-closed」必须对称。
case "$BP_MODE" in
  on|off) ;;
  *) fail_plain "BP 模式取值非法: '$BP_MODE' (只接受 on|off; 来自 BENCH_BP 或 --bp=)" ;;
esac

[ -x "$SIM" ] || fail_plain "缺少可执行 $SIM (先 cmake --build build)"
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
# ★ flush_count 列（BP=off 基线的 FLUSH 事件数）—— 用于「ΔFLUSH == Δcycles」判据
IDX_FLUSH=$(col_index flush_count)
[ -n "$IDX_NAME" ] && [ -n "$IDX_TOHOST" ] && [ -n "$IDX_CYCLES" ] \
  || fail "基线表头缺少必需列 (mode_name / expected_tohost / cycles)"

# ---- workload_sha 守卫: 基线记录的 .text sha 必须与当前 ELF 一致 ----
# ★ fail-closed (critic B3): 缺 objcopy ⇒ 无法验证 workload_sha ⇒ **必须失败**
#   (原实现静默跳过守卫 => 「基线属于另一份代码」的假绿通道开着)
OBJCOPY="${BENCH_OBJCOPY:-}"
if [ -z "$OBJCOPY" ]; then
  # ★ 兼容 CI (critic F1): .github 装的是 gcc-riscv64-unknown-elf (只提供 riscv64-* 工具);
  #   提取 .text 段与 arch 无关 ⇒ riscv64-unknown-elf-objcopy 同样可用
  for c in riscv32-unknown-elf-objcopy riscv64-unknown-elf-objcopy \
           /workspace/main/opt/riscv/bin/riscv32-unknown-elf-objcopy; do
    if command -v "$c" >/dev/null 2>&1; then OBJCOPY="$c"; break; fi
    if [ -x "$c" ]; then OBJCOPY="$c"; break; fi
  done
fi
if [ -z "$OBJCOPY" ]; then
  fail_plain "缺少 objcopy (riscv32- 或 riscv64-unknown-elf-objcopy 均可) ⇒ 无法验证 workload_sha (基线归属守卫) —— 设 BENCH_OBJCOPY 或安装工具链 (★ fail-closed: 不得静默跳过)"
fi
if [ -n "$IDX_WSHA" ] && ! "$OBJCOPY" --version >/dev/null 2>&1; then
  fail_plain "BENCH_OBJCOPY=$OBJCOPY 不可执行 ⇒ 无法验证 workload_sha (★ fail-closed)"
fi

for m in "${MODES[@]}"; do
  [ -f "$ELF_DIR/branchbench_m${m}.elf" ] || fail_plain "缺少 $ELF_DIR/branchbench_m${m}.elf (CMake 目标 branchbench_elfs)"
done

# 当前 build 的 .text sha (4 档共用 => 取 mode0)
cur_wsha=""
if [ -n "$IDX_WSHA" ]; then
  if ! "$OBJCOPY" -O binary --only-section=.text "$ELF_DIR/branchbench_m0.elf" "$ELF_DIR/.bb_m0.text" 2>/dev/null; then
    rm -f "$ELF_DIR/.bb_m0.text"
    fail_plain "objcopy 提取 .text 失败 ⇒ 无法计算 workload_sha (★ fail-closed)"
  fi
  cur_wsha=$(sha256sum "$ELF_DIR/.bb_m0.text" | cut -c1-16)
  rm -f "$ELF_DIR/.bb_m0.text"
  [ -n "$cur_wsha" ] || fail_plain "workload_sha 计算为空 (★ fail-closed)"
fi

rc=0
first_pc_changes=""
bp_dir_judges=0
# ★ critic 七轮 SF-1: **实际比对的行数** —— 0 行却报「基线一致 => PASS」是假绿
#   （`--bp=on --check-baseline` 在 CSV 尚无 bp_mode=on 行时的真实现状）
compared_rows=0
for i in "${!MODES[@]}"; do
  m="${MODES[$i]}"; name="${MODE_NAMES[$i]}"
  elf="$ELF_DIR/branchbench_m${m}.elf"

  # ★ 捕获 stderr (修正: 原为 2>/dev/null => flag 未生效却测出「无差异」的假绿通道开着)
  # ★ 注意: **不把 runner 退出码当错误** —— branchbench 的 tohost 携带校验和 (≠1),
  #   而 runner 的退出码判据是 tohost==1 => 必然非零. 正确性由宿主侧的校验和比对决定.
  # ★ B4 修正: **必须**把 BP 状态传给 sim —— 否则 `bp_mode` 标签与实际行为
  #   **脱钩**（脚本可标 "off" 而 sim 跑的是 BP=on）⇒ 假绿通道。
  # ★ BP **默认开** ⇒ "off" 需显式传 `--no-bp`；"on" 不传（默认）。
  if [ "$BP_MODE" = "off" ]; then
    raw=$("$SIM" --elf "$elf" --cycles "$MAX_CYCLES" --stats --no-bp 2>&1)
  else
    raw=$("$SIM" --elf "$elf" --cycles "$MAX_CYCLES" --stats 2>&1)
  fi
  # ★ 生效值断言: runner 在 stderr 回显 `BP=on/off` ⇒ MUST 与请求一致
  #   （否则「flag 未生效却测出无差异」的假绿通道是开着的）
  # ★ critic 七轮 SF-2: **检查模式下缺回显 MUST fail-closed**（原来 `eff_bp` 为空
  #   就静默跳过 ⇒ 与同文件 BP_STATS / pc_changes / dropped 的 fail-closed 教义不一致）
  eff_bp=$(printf '%s' "$raw" | grep -oE 'BP=(on|off)' | head -1 | cut -d= -f2)
  if [ -z "$eff_bp" ]; then
    if [ "$CHECK_ONLY" -eq 1 ] || [ "$CHECK_BP_DIR" -eq 1 ]; then
      fail_plain "mode $m ($name): runner 未回显 BP= 生效值 (★ fail-closed: 无法确认 flag 生效)"
    fi
  else
    if [ "$BP_MODE" = "off" ] && [ "$eff_bp" != "off" ]; then
      fail "mode $m ($name): 请求 bp_mode=off 但 runner 回报 BP=$eff_bp (flag 未生效)"
    fi
    if [ "$BP_MODE" = "on" ] && [ "$eff_bp" != "on" ]; then
      fail "mode $m ($name): 请求 bp_mode=on 但 runner 回报 BP=$eff_bp (flag 未生效)"
    fi
  fi
  out=$(printf '%s' "$raw" | grep -oE "TOHOST=[0-9]+ CYCLES=[0-9]+[^[:space:]]*" | head -1)
  [ -n "$out" ] || fail "mode $m ($name): 无结果行 —— 输出: $(printf '%s' "$raw" | tail -3)"

  tohost=$(printf '%s' "$out" | grep -oE 'TOHOST=[0-9]+' | cut -d= -f2)
  cycles=$(printf '%s' "$out" | grep -oE 'CYCLES=[0-9]+' | cut -d= -f2)
  status=$(printf '%s' "$out" | grep -oE '(PASS|FAIL|TIMEOUT)' | head -1)
  pc_changes=$(printf '%s' "$raw" | grep -oE 'pc_changes=[0-9]+' | head -1 | cut -d= -f2)
  # ★ FLUSH 事件数（DUT 内部 flush 信号的累计值；host 经 --stats 读）
  flush_count=$(printf '%s' "$raw" | grep -oE 'FLUSH_COUNT=[0-9]+' | head -1 | cut -d= -f2)
  dropped=$(printf '%s' "$raw" | grep -oE 'dropped=[0-9]+' | head -1 | cut -d= -f2)
  # ★ fail-closed (critic SHOULD_FIX-6): 缺 dropped 字段 ⇒ 输出格式变了 ⇒ 静默跳过
  #   溢出检查会开假绿通道 ⇒ 与 pc_changes 同等对待
  [ -n "$dropped" ] || fail "mode $m ($name): 输出缺 dropped (★ fail-closed: 无法判断直方图是否溢出)"
  if [ "$dropped" -ne 0 ]; then
    fail "mode $m ($name): 直方图窗口溢出 (dropped=$dropped) ⇒ 计数被丢弃 ⇒ 前端活动量断言不可信 —— 提高 --stats-len (或检查 --stats-base 覆盖范围)"
  fi

  # ---- BP 统计 (critic B-2: 方向性判据的承重数据) ----
  # ★ 只在方向性判据模式解析（critic SHOULD_FIX-3: 把「BP 模式承重数据」的
  #   fail-closed 检查绑到 BP=off 路径上没有必要；将来 runner 改成「BP 关时
  #   不打印 BP_STATS」会**误杀** baseline entry）

  # ---- 结果正确性 (硬闸): 校验和必须与基线期望值逐位相等 ----
  row=$(base_row "$name" "$BP_MODE")
  if [ -z "$row" ]; then
    if [ "$BP_MODE" = "off" ]; then
      fail "基线 $BASELINE 无 (${name}, bp_mode=${BP_MODE}) 行 (mode $m)"
    elif [ "$CHECK_BP_DIR" -eq 0 ]; then
      # BP 档尚无基线 => 打印但不做比对 (前置2 / ② 追加基线行后生效)
      printf "branchbench: mode=%s %-16s bp=%s tohost=%-10s cycles=%-8s pc_changes=%-8s (无基线行 => 仅打印)\n" \
        "$m" "$name" "$BP_MODE" "$tohost" "$cycles" "${pc_changes:-?}"
      continue
    else
      # ★ --check-bp-direction: 无 (name,bp=on) 基线行 ⇒ 跳过基线比对,
      #   但**继续**跑下面的方向性判据（判据不依赖基线行）
      exp_tohost=""
      base_cycles=""
    fi
  else
    exp_tohost=$(field "$row" "$IDX_TOHOST")
    base_cycles=$(field "$row" "$IDX_CYCLES")
    compared_rows=$(( compared_rows + 1 ))
    [ "$tohost" = "$exp_tohost" ] \
      || fail "mode $m ($name): 校验和不符 —— 实测 $tohost != 基线 $exp_tohost (结果错, 不得产出数值)"
  fi
  [ "$status" != "TIMEOUT" ] \
    || fail "mode $m ($name): TIMEOUT (cycles>=$MAX_CYCLES) —— 程序未跑完, 不得产出数值"

  # ---- build_id 记录 (★ critic B3: 原实现赋值后从未使用 => 注释为假;
  #      这里做**记录性输出**, 漂移判定交给 workload_sha (build_id 会随任何重建变化,
  #      硬比对会假红) ----
  if [ -n "$IDX_BID" ] && [ "$CHECK_ONLY" -eq 1 ]; then
    base_bid=$(field "$row" "$IDX_BID")
    [ -n "$base_bid" ] && printf "  (基线 build_id=%s; 本 build 的仿真器 sha 见 git 记录 —— build_id 不作硬门禁, 仅记录)\n" "$base_bid"
  fi

  # ---- workload_sha 守卫 ----
  if [ -n "$IDX_WSHA" ] && [ -n "$cur_wsha" ]; then
    base_wsha=$(field "$row" "$IDX_WSHA")
    if [ -n "$base_wsha" ] && [ "$base_wsha" != "$cur_wsha" ]; then
      fail "mode $m ($name): workload_sha 不符 —— 基线 $base_wsha != 当前 .text $cur_wsha (基线属于另一份代码, 须显式重录)"
    fi
  fi

  # ═══════════════════════════════════════════════════════════════════════════
  # ★ BP 方向性判据 (critic B-2: 机制指纹 MUST 有**自动化**锚)
  #   —— 背景: BP 的 B-2 修正 (PHT 方向学习) / B-1 修正 (饱和判定用 select)
  #      在修复前**没有任何自动锚**: 4 档指纹只活在 commit message 里,
  #      而唯一存在的 branchbench entry 跑的是 bp_mode=off
  #      ⇒ **BP 路径没有任何自动判据**（★ 注意: 本仓 CI **不跑 ctest 也不构建项目**
  #      ⇒ 连 bp_mode=off 的 entry 在 CI 里也恒走 SKIP —— 见 ADR-088 §5.2 判据 4 /
  #      brief AC-9；所以这里的「自动锚」是**本地锚**，由 PR 提交者手动跑兑现）
  #      ⇒ 有人再写出一个 `?:` 折常量 ⇒ 判据全绿而机制被判「不成立」⇒ 误杀 ②。
  #   —— 判据 (全部是**方向性**不等式, 对 S3 的 PC-mux 接入不敏感):
  #      (1) 4 档 hit > 0            ⇒ BTB 查表/写路径活着 (AC-F2-14 命中自证)
  #      (2) BP=on 的校验和 == BP=off 基线 ⇒ BP 不改变架构可见结果
  #          ★ 当前**恒真**（BP 三输出无消费者）⇒ 它是 S3 才激活的承重判据；
  #          ★ 它同时兜住「档名 ↔ 硬件方向错配」（错配 ⇒ 校验和与基线行不符 ⇒ RED）
  #      (3) always_taken: pred >= 0.9×hit ⇒ PHT 学得到 taken
  #          (饱和判定被折常量 ⇒ 计数器 3→0 回绕 ⇒ pred ≈ hit/2 ⇒ RED)
  #      (4) pattern_6of8: pred <= 0.9×hit ⇒ 方向学习生效
  #          (退化成「任何方向都 +1」⇒ pred == hit ⇒ RED)
  # ═══════════════════════════════════════════════════════════════════════════
  if [ "$CHECK_BP_DIR" -eq 1 ]; then
    # ★ fail-closed: 缺 BP_STATS ⇒ 输出格式变了 ⇒ 静默跳过方向性判据会开假绿通道
    bp_stats=$(printf '%s' "$raw" | grep -oE 'BP_STATS hit=[0-9]+ miss=[0-9]+ pred_taken=[0-9]+' | head -1)
    [ -n "$bp_stats" ] || fail_plain "mode $m ($name): 输出缺 BP_STATS (★ fail-closed: 无法做 BP 方向性判据)"
    bp_hit=$(printf '%s' "$bp_stats" | grep -oE 'hit=[0-9]+' | cut -d= -f2)
    bp_pred=$(printf '%s' "$bp_stats" | grep -oE 'pred_taken=[0-9]+' | cut -d= -f2)

    [ "$bp_hit" -gt 0 ] \
      || fail "mode $m ($name): BTB 命中数 = 0 (AC-F2-14 命中自证失败 ⇒ 查表/写路径坏了)"

    off_row=$(base_row "$name" "off")
    [ -n "$off_row" ] \
      || fail "mode $m ($name): 基线无 (${name}, bp_mode=off) 行 ⇒ 无法做 BP=on 的校验和对照"
    off_tohost=$(field "$off_row" "$IDX_TOHOST")
    # ★★ 校验和判据: **当前恒真**（BP 的三个输出 payload 本 change 内**无消费者**
    #   ⇒ BP=on 与 BP=off 必然同校验和）⇒ 它是 **S3 接入 PC-mux 后才激活**的
    #   承重判据（那时若 BP 改变了架构可见结果 ⇒ 这里 RED）。
    [ "$tohost" = "$off_tohost" ] \
      || fail "mode $m ($name): BP=on 校验和 $tohost != BP=off 基线 $off_tohost ⇒ BP 改变了架构可见结果 (结果错, 不得产出数值)"
    if [ -n "$IDX_WSHA" ] && [ -n "$cur_wsha" ]; then
      off_wsha=$(field "$off_row" "$IDX_WSHA")
      [ -z "$off_wsha" ] || [ "$off_wsha" = "$cur_wsha" ] \
        || fail "mode $m ($name): workload_sha 不符 —— BP=off 基线 $off_wsha != 当前 .text $cur_wsha (基线属于另一份代码, 须显式重录)"
    fi

    # ★★ 「ΔFLUSH == Δcycles」判据（critic 十二轮 SF-4）—— ★ **阈值无关**的
    #   机制证据：**每个被消除的 taken 气泡 = 恰好 1 拍** ⇒ 同时钉住
    #   ① `flush_count` 计数器的口径（off-by-one ⇒ ΔFLUSH ≠ Δcycles ⇒ RED）
    #   ② 门控式 flush 的正确性（少 flush / 多 flush ⇒ 两侧不等 ⇒ RED）
    #   ⇒ ★ 此前「ΔFLUSH 与 Δcycles 逐位相等 4/4」只是**一次性手工测量**
    #     ⇒ 任何人在 flush 路径引入偏差都会**静默**让决策级证据失效。
    if [ -z "$flush_count" ]; then
      fail_plain "mode $m ($name): 输出缺 FLUSH_COUNT (★ fail-closed: 无法做 ΔFLUSH==Δcycles 判据)"
    fi
    off_flush=$(field "$off_row" "$IDX_FLUSH")
    off_cycles=$(field "$off_row" "$IDX_CYCLES")
    # ★★ fail-closed: 基线缺 flush_count 列 / 该行缺值 ⇒ **必须失败**
    #   （否则「ΔFLUSH == Δcycles」判据被**静默跳过** ⇒ 决策级证据链失效而 0 测试变红）
    [ -n "$off_flush" ] \
      || fail_plain "mode $m ($name): 基线行缺 flush_count 值 (★ fail-closed: 无法做 ΔFLUSH==Δcycles 判据)"
    [ -n "$off_cycles" ] \
      || fail_plain "mode $m ($name): 基线行缺 cycles 值 (★ fail-closed: 无法做 ΔFLUSH==Δcycles 判据)"
    d_flush=$(( off_flush - flush_count ))
    d_cycles=$(( off_cycles - cycles ))
    [ "$d_flush" -gt 0 ] \
      || fail "mode $m ($name): ΔFLUSH = $off_flush - $flush_count = $d_flush ≤ 0 (★ BP 开启后 flush 事件数 MUST 下降)"
    [ "$d_flush" -eq "$d_cycles" ] \
      || fail "mode $m ($name): ΔFLUSH ($d_flush) != Δcycles ($d_cycles) (★ 每个被消除的 taken 气泡 MUST 恰为 1 拍 ⇒ flush 计数口径或门控有偏)"

    # ★ 这里**不**累加 `compared_rows`（critic 八轮 NIT-3）：该变量只在
    #   `CHECK_ONLY` 收尾被读，而 bpdir 与 CHECK_ONLY 互斥 ⇒ 累加是死代码；
    #   且若将来 (name,on) 基线行存在，line ~324 与本处会**双重计数** ⇒ 误红。
    #   方向性判据的覆盖完整性由 `bp_dir_judges == 2` 保证。
    case "$name" in
      always_taken)
        # (3) PHT 学得到 taken ⇒ 预测率 MUST ≥ 90% (实测 3996/3998 = 99.95%)
        #   ★ 饱和判定被折常量 (?: ⇒ 3→0 回绕) ⇒ 实测 2000/3998 = 50% ⇒ RED
        bp_dir_judges=$(( bp_dir_judges + 1 ))
        min_pred=$(( bp_hit * 9 / 10 ))
        [ "$bp_pred" -ge "$min_pred" ] \
          || fail "mode $m ($name): pred_taken=$bp_pred < 0.9×hit=$min_pred ⇒ PHT 学不到 taken (饱和判定被折常量? 或 PHT 未更新?)"
        ;;
      pattern_6of8)
        # (4) 方向学习生效 ⇒ 该档真实 taken 率仅 2/8 ⇒ 预测 taken 率 MUST ≤ 90%
        #   ★ 实测(正确) 2247/3992 = 56.3% ✓ ; 实测(退化成「任何方向都 +1」)
        #     3991/3992 = 99.97% ⇒ RED
        #   ★ MUST 用**比值 + 余量**, 不可用「pred < hit」这种严格不等式 ——
        #     99.97% < 100% 也会成立 ⇒ 零鉴别力 (同型陷阱: 只差 1 也是「<」)
        bp_dir_judges=$(( bp_dir_judges + 1 ))
        max_pred=$(( bp_hit * 9 / 10 ))
        [ "$bp_pred" -le "$max_pred" ] \
          || fail "mode $m ($name): pred_taken=$bp_pred > 0.9×hit=$max_pred ⇒ 方向学习失效 (退化成「任何方向都 +1」?)"
        ;;
      *)
        # ★ m0 / m3 档**本来就没有**方向性判据（m0 的命中全来自恒 taken 的回边、
        #   m3 的伪随机方向 ⇒ 比值判据不适用）⇒ 此处**不** fail；
        #   ★ 覆盖完整性由**判据计数**（循环结束 MUST == 2）兜住 ——
        #   档名一旦改名 ⇒ 判据静默失效 ⇒ 计数不足 ⇒ fail（critic SHOULD_FIX-2）
        ;;
    esac
    printf "branchbench: mode=%s %-16s bp=%s tohost=%-10s cycles=%-8s bp_hit=%-6s bp_pred_taken=%-6s bp-direction-ok\n" \
      "$m" "$name" "$BP_MODE" "$tohost" "$cycles" "$bp_hit" "$bp_pred"
  fi

  cycles_minus_pc=""
  # ★ fail-closed (critic F2): 缺 pc_changes ⇒ 输出格式变了 ⇒ 静默跳过跨档断言会开假绿通道
  [ -n "$pc_changes" ] || fail_plain "mode $m ($name): 输出缺 pc_changes (★ fail-closed)"
  if [ -n "$pc_changes" ]; then
    # 工作量一致性: 4 档 pc_changes 必须相同 (AC-F1-2)
    # ★★ 仅在 **BP=off** 下断言 (critic B-3 暗雷 ① / 六轮 BLOCKER-1) ——
    #   守卫键 MUST 是 **BP_MODE**, **不可**用 CHECK_BP_DIR:
    #   「BP=on」有两条路径（`--check-bp-direction` 与 `BENCH_BP=on --check-baseline`
    #   / `--bp=on --check-baseline`）⇒ 用 CHECK_BP_DIR 只挡住前者 ⇒ 后者（② 追加
    #   bp=on 基线行后的正规用法）仍会触发 ⇒ **S3 接入 PC-mux 后必然假红**：
    #   不同档的预测正确性不同 ⇒ 采样 PC 轨迹必然不同（m0 恒 not-taken ⇒ 预测必错
    #   ⇒ 采样流多出错路 PC）⇒ 该断言与「BP 是否健康」无关 ⇒ MUST NOT 用它判生死。
    #   ⇒ BP=on 模式只保留「字段存在」检查 (fail-closed), 不做跨档相等断言。
    if [ "$BP_MODE" = "off" ]; then
      if [ -z "$first_pc_changes" ]; then
        first_pc_changes="$pc_changes"
      elif [ "$pc_changes" != "$first_pc_changes" ]; then
        fail "mode $m ($name): pc_changes=$pc_changes != $first_pc_changes ($(printf '%s' "${MODE_NAMES[0]}")) —— ★ 前端活动量跨档不一致 (承重证据 = 静态结构论证; 此项为动态佐证)"
      fi
    fi
    cycles_minus_pc=$(( cycles - pc_changes ))
  fi

  # ★ NIT-5: `cycles-minus-pc` 的语义**未确认** ⇒ 仅在 --check-baseline 下输出
  #   (默认输出不给下游留下可被误引用的未确认量)
  if [ "$CHECK_ONLY" -eq 1 ]; then
    printf "branchbench: mode=%s %-16s bp=%s tohost=%-10s cycles=%-8s pc_changes=%-8s cycles-minus-pc=%-6s checksum-ok\n" \
      "$m" "$name" "$BP_MODE" "$tohost" "$cycles" "${pc_changes:-?}" "${cycles_minus_pc:-?}"
  else
    printf "branchbench: mode=%s %-16s bp=%s tohost=%-10s cycles=%-8s pc_changes=%-8s checksum-ok\n" \
      "$m" "$name" "$BP_MODE" "$tohost" "$cycles" "${pc_changes:-?}"
  fi

  if [ "$CHECK_ONLY" -eq 1 ] && [ "$base_cycles" != "$cycles" ]; then
    echo "::warning file=soc/cpu/docs/dse/branchbench-baseline-matrix.csv::FAIL: mode $m ($name) 基线漂移: 基线 $base_cycles != 实测 $cycles (精确相等口径; 漂移须显式重录基线 + commit 说明原因)"
    rc=1
  fi
done

if [ "$rc" -ne 0 ]; then exit 1; fi
if [ "$CHECK_ONLY" -eq 1 ]; then
  # ★ critic 七轮 SF-1: **0 行比对 MUST NOT 报 PASS** ——
  #   原实现在「该 bp_mode 尚无基线行」时 4 档全走「仅打印」却仍打
  #   「基线一致 (4 档) => PASS」（0 条比对却声称一致 ⇒ 假绿；② 追加基线行前
  #   任何人把它接进 ctest/脚本都会被骗）。
  [ "$compared_rows" -gt 0 ] \
    || fail_plain "0 行比对 (bp_mode=$BP_MODE 无基线行) —— MUST NOT 报 PASS (基线行待 ② 追加)"
  # ★ 注（critic 八轮 NIT-4）: 该「== 4」断言实际上**只兜 on 路径** ——
  #   `BP_MODE=off` 时缺行会先在循环内「基线无 (name, off) 行」硬失败 ⇒ 到不了这里。
  #   保留它是为了 `--bp=on --check-baseline`（② 追加行后部分缺失）的场景。
  [ "$compared_rows" -eq "${#MODES[@]}" ] \
    || fail_plain "只比对了 $compared_rows/${#MODES[@]} 档 (bp_mode=$BP_MODE 部分基线行缺失)"
  echo "branchbench: 基线一致 ($compared_rows 档, bp_mode=$BP_MODE) => PASS"
fi
if [ "$CHECK_BP_DIR" -eq 1 ]; then
  # ★ 判据覆盖数必须 == 2 (critic SHOULD_FIX-2: 档名改名 ⇒ 判据静默失效)
  [ "$bp_dir_judges" -eq 2 ] \
    || fail "方向性判据只应用了 $bp_dir_judges/2 条 ⇒ 判据覆盖不完整 (档名改动后 MUST 同步本段)"
  echo "branchbench: BP 方向性判据通过 (4 档, bp_mode=on, 判据 2/2) => PASS"
fi
exit 0

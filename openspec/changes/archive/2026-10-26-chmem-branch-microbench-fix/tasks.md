---
change: chmem-branch-microbench-fix
priority: P1
initiative: wave6-linux-and-productization
adr:
  - ADR-088
plan: .rddf/plans/chmem-branch-microbench-fix.md
status: in-progress
---

# Tasks — chmem-branch-microbench-fix

> **定位**: B1 ② `chmem-bp-btb-gshare` 的**硬前置**（独立 change）。修正 0c 微基准的**工作量污染** + **档名方向反向**。
> **权威规格**: `docs/architecture/bp-btb-gshare-design-brief.md` §2.4 / §4.1 / §5.5
> **不可触碰**: `ip/**`（零生产代码改动）· ② 的任何代码

## Implementation

- [x] **Task 1** `branch_cond` 无分支化（位运算 select）⇒ `objdump` 验证 **0 条条件分支**
- [x] **Task 2** `branch_sel` 两条路径补齐等长（taken 路径补一条 `j`）⇒ `objdump` 验证两路径指令数相等
- [x] **Task 3** ★ **档名互换重命名**（mode0 ⇄ mode1）+ **改名传播清单**（同 commit）：CSV `mode_name` · `tools/measure_branchbench.sh` 的模式名数组 · 文档旧档名
- [x] **Task 4** ★ **常驻测量设施（仅 host 可导出的部分）**：`cpu_verilator_sim --stats` 输出**`pc_changes`（取指地址变化的 cycle 数）+ 逐静态 PC 取指直方图**（★ 二者均由 **PC 轨迹**导出 ⇒ host 可见；★ **MUST NOT** 用「retired 指令数」—— retired = 取指 − flush，而 flush 是 DUT 内部信号 ⇒ 归前置2；★ **FLUSH 事件数归前置2**）⇒ 取代「临时改 `sim_main.cpp`」的一次性 hack
- [x] **Task 5** 基线重录 + CI/ctest 保持绿 + 证伪留痕（写入本 change 的 proposal + CSV notes，**不改**已归档的 0c 目录）

## Acceptance

- [x] **AC-F1-1** `objdump` 证明 `branch_cond` **0 条条件分支**（纯 `sltiu/seqz/and/or`）+ `branch_sel` 两条路径**指令数相等**（★ 仅证**静态**一致；★ **计数口径**：以 objdump 的函数体为准，**含 `ret`**；★ MUST 报告两段 objdump 片段与各自的条数）
- [x] **AC-F1-2** ★ **工作量一致性判据（★ 修正口径 —— critic B1/B6 复核后）**：★ **承重证据 = AC-F1-1 的静态结构论证**：`branch_cond` **0 条条件分支** + `branch_sel` 两条路径**等长** ⇒ **4 档执行同一条指令序列** ⇒ 动态指令数**结构上必然相同**（无需测量）。★ **动态佐证（非承重）**：`--stats` 的 `pc_changes`（**采样到的 PC 值的变化拍数**）4 档逐位相同 = 86034。⚠️ ★ **`pc_changes` MUST NOT 被称为「已执行/动态指令数」** —— 采样到的 PC 值**含非架构路径上的值**（★ 机制**未确认** ⇒ 不为它命名机制）：实测 m0 的 `bnez` **不 taken**，但其**目标** `0x80000018` 仍被采样 **2000** 次，而 taken 路径的 `0x8000001c` 在 m0 为 **0**；`0x80000068`（`tohost_pass`，架构 0 次）被采样 2000 次；`0x80000128`（循环出口，架构执行 1 次）被采样 2000 次；且 `sum(直方图) == cycles` ⇒ 它是**逐 cycle 采样**，不是取指计数。★ **独立交叉验证（打破循环）**：`cycles − pc_changes` = 0/2000/500/1022，与**由源码语义独立算出的 `branch_sel` taken 数**（`c1=1` ⇒ 2000；`i&7>=6` ⇒ 500；LCG `w&1` ⇒ 1022）**逐一相等**。⚠️ ★ **MUST NOT** 推广为「每条 taken 条件分支额外 1 拍」的**通则** —— **反例**：同循环的**回边**（`0x80000124 bne → 0x800000e8`，目标 ≠ `pc+4`）taken **1999** 次却对 `cycles − pc_changes` 贡献 **0** ⇒ 该差值**不是** taken 总数，语义**未确认** ⇒ **MUST NOT** 作任何结论的依据；⇒ ★ **「每条 taken 额外 1 拍」这一模型的成立性 MUST 由 ② 用「BP 关态的 taken 数」独立验证**。
- [x] **AC-F1-3** ★ **4 档 `.text` sha256 MUST 逐档相同**（新值入册）—— ★ 改写 `branch_cond`/`branch_sel` 后**必须继续成立**（否则档间差异不再只是「data 段 1 个字」⇒ 判据地基失效）+ `bench_mode` 仍是唯一档间差异 + CSV 重录 + ★ **本地** `bash tools/measure_branchbench.sh --check-baseline` **exit 0**（★ CI 该 job 是 **warn-level** ⇒ 以本地为准）
- [x] **AC-F1-4** ★ **档名与硬件方向一致** —— ★ **mode0 与 mode1 是「互换」不是单改名** ⇒ **强制互换重命名**（**删掉「修正注释」这个弱选项**）+ ★ **改名传播清单**（同 commit）。★ **下游指向须同步**：主体 AC-8b-1 / AC-8c / AC-8e 使用的 `always_taken` MUST 指「**重命名后**硬件方向恒 taken 的那一档」（⇒ 重命名前后同名异义 ⇒ 不钉死则 AC 会打在错误的档上 ⇒ 假 RED / 假绿）
- [x] **AC-F1-5** ★ **留痕 + 替换失效断言**：0c 的 AC-2「`.text` sha256 一致」**事实仍成立**，其推论「`.text` 一致 ⇒ 动态指令数相同」**被证伪** ⇒ 写入本 change 的 proposal + CSV notes（**不得**改动归档产物）。★ **CSV notes MUST 替换/删除两条失效断言**：① 「`.text` 一致 ⇒ 指令数一致 ⇒ cycle 差只来自分支方向」（**已被证伪**）② 「预期本档收益最大/最小」（**按档名推断收益** ⇒ 档名反向使其失效）⇒ 否则 CSV 会继续向下游提供**反向证据**。★ 佐证：归档基线自身已记录档名反向（`always_taken` 33.017 c/iter **快于** `always_not_taken` 34.017 ⇒ 名为 always_taken 的档在硬件上从不 taken ⇒ 无气泡 ⇒ 最快）
- [x] **AC-F1-6** ★ **预登记默认出口**（★ 删除「或新增密度可调负载」这个开放式口子）：若跨档 taken 转移数差 **≤ 1 拍/iter**（★ **含恰好 1.0** —— 本基准每 iter 恰 1 条条件分支 ⇒ 跨档差**恰好 = 1.0** ⇒ 严格小于永不触发 ⇒ **必须用 ≤**）⇒ **直接**采用「`always_taken` 单档绝对 Δ + 解析模型」判据 ⇒ ★ **判定权归本 change**（独立执行者）⇒ 结论写入 CSV notes + 本 change 归档产物 ⇒ **不留给 ②**（② 对「判据不可用」有既得利益）
- [x] **AC-F1-7** ★ **改名与脚本改动必须同一 commit** ⇒ 否则 `measure_branchbench.sh` 的 `grep -E "^${name},"` 找不到行 ⇒ CI `branchbench_baseline` 立刻红


## ★★ per-file × per-symbol owner 表（★ 逐条抄进本 change —— 跨 change 引用会被漏读）

| 文件 / 符号 | 本 change | 其它 change | ★ 约定 |
|---|---|---|---|
| `tests/cpu/manual_elf/branchbench/branchbench.c` | ★ **前置1 owner** | 前置2 / 主体 ✗ | 主体 MUST NOT 触碰（硬门禁） |
| `soc/cpu/docs/dse/branchbench-baseline-matrix.csv` | ★ **前置1 建列**（`workload_sha` / `build_id` / `bp_mode=off`）+ 改按表头取列 | 前置2 / 主体 **仅追加行** | 建列归前置1；追加归后两者 |
| `tools/measure_branchbench.sh` | ★ **前置1 owner**（改名 / 按表头取列 / **捕获 stderr**） | 前置2 **仅追加** `--bp=on\|off` flag + 生效值断言；主体 ✗（硬门禁） | ★ **捕获 stderr 归前置1**（否则假绿通道开着）⇒ 前置2 只加 flag |
| `tools/verilator_runner/*` | ★ **前置1 建 `--stats` 框架 + `pc_changes`/PC 直方图**（★ **NOT** retired —— retired 需 flush ⇒ 归前置2） | 前置2 **补 FLUSH 计数**；主体 **补 BP flag + 回显** | ★ **框架归前置1**，增量归后两者，**MUST NOT** 另起一套 |
| `ip/cpu/plugins/branch_predictor_chmem.h` | ✗（禁 `ip/**`） | 前置2 **创建（最小版）**；主体 **重写（完整版）** | ★ 前置2 的 PoC 版 MUST NOT 被主体当作已完成 ⇒ 主体 AC-1 MUST 检查**完整功能**（非文件存在）；★ PoC 残留的**删除/重写责任归主体**；★ `tests/cpu/test_cpu_chmem_bp_poc.cpp` 的 8 case 在主体后 MUST 仍绿（机制锚）⇒ ★ **该 8 case 只能依赖公开接口** |
| `ip/cpu/plugins/ibus_chmem.h`（`update_pc` 的 mux + flush） | ✗ | 前置2 **首次改**（5 臂 mux + FLUSH 门控 + FLUSH 计数）；主体 **二次改**（补完整语义 + 过时注释 #8） | ★ **同函数两次改** ⇒ 前置2 的版本 MUST 可被主体**增量扩展**（MUST NOT 要求主体重写）⇒ ★ 主体 MUST 继承前置2 的**五臂 select 链**结构 |
| `ip/cpu/cpu_factory_chmem.h`（注册 + 开关 + 参数转发） | ✗ | 前置2 **首次注册**（BP 开关）；主体 **补全**（4 参数转发修复 #9 + 完整注册） | ★ 同上 ⇒ 增量扩展 |
| `docs/architecture/adr/ADR-088-*.md` | ✗ | 前置2 写 **§5.3.2 的「机制结论」段**；主体 写 **「绝对值门禁结论」段** | ★ **主体 MUST NOT 覆盖前置2 的机制结论段**（否则「机制不成立」的结论会被主体的「达标」结论抹掉） |
| `docs/architecture/tlm-coverage-parity-ledger.md` | ★ **前置1 MUST NOT 触碰** | 前置2 MUST NOT 触碰；主体 owner | ★ 台账重判只归主体 |
| `ip/cpu/plugins/branch_predictor.{h,cpp}`（TLM 侧） | — | — | ★ **全程保留**（主体 AC-11） |

★ **本 change 的角色**：前置1（建测量地基）⇒ 本表列出的「前置1 owner」项**归本 change**；其余 change 的项**本 change MUST NOT 触碰**。
## ★ 兜底句纪律

任何「上报 architect / 待裁决」句 MUST 同句给出 **① 触发阈值 ② 落点文件 ③ 可判定时限**（「≤ 2 个工作日」或「本 change 内闭环」）；MUST NOT 以「等待回复」结束本 change。

## ★ builder 开工须知（**逐条抄进本 change**，不跨 change 引用）

| # | 须知 |
|---|------|
| 1 | 台账条目行**必须正好 7 列**；**理由列不得含未转义的 `\|`** |
| 2 | 新增 CH_MEM 侧 case 前**先快照 TLM-only 名单**（`comm -23`）再命名 |
| 3 | 台账脚本需**两个 binary** ⇒ **本地跑 + 输出贴 PR**（CI warn 级无强制力） |
| 4 | ★ CSV / 脚本的**改名必须同一 commit** |
| 5 | ★ CSV **按表头名取列**（**强制**；现 notes 含逗号 ⇒ 位置访问必错位）；`--check-baseline` 分组键 = `(workload_sha, bp_mode, build_id)` |
| 6 | `--btb-entries N` 合法域 = 16/32/64/128/256；**越界 fail-fast**（不得静默 clamp） |
| 7 | ★ 本 change 的 diff **不得触碰** `ip/**`（零生产代码改动）⇒ ★ **注意**：须知 #8/#9/#10 都涉及 `ip/**` ⇒ **它们归前置2 / ②**（见各自 tasks.md）⇒ **本 change 不执行** |
| ★ 本 change 专属 | 本 change **MUST NOT** 触碰台账 `docs/architecture/tlm-coverage-parity-ledger.md`（台账重判归 ②） |

## 实测留证

（实施后填写）


## 实测留证（2026-10-24，branch `feature/chmem-bp-btb-gshare`；★ 2026-10-25 按 critic 复核修正口径）

### 构建与静态验证（★ 承重证据）

```
$ riscv32-unknown-elf-g++ -O2 -fno-inline -march=rv32im_zicsr -mabi=ilp32 -nostdlib -static \
    -DBENCH_MODE=$m -T tests/cpu/manual_elf/link_branchbench.ld \
    tests/cpu/manual_elf/branchbench/branchbench_crt0.S \
    tests/cpu/manual_elf/branchbench/branchbench.c -o build/branchbench_m$m.elf      # m=0..3
$ cmake --build build --target branchbench_elfs        # 与手工编译产物逐字节一致
```

**AC-F1-1** —— `branch_cond` 反汇编（**0 条条件分支**）：

```
80000028 <_ZL11branch_condmm>:
  lui a5,0x80001 ; lw a4,4(a5)          # 读 volatile bench_mode
  andi a0,a0,7 ; sltiu a0,a0,6          # c2 = (i&7)>=6
  addi/seqz ×3 ; and/or ×4              # 位运算 select (4 档等价判定)
  ret                                    # ★ 无 bnez/beq/bge —— 0 条条件分支
```

`branch_sel` 反汇编（★ 口径：**含 `bnez`/`ret` 各 5 条**；「路径专有指令」各 3 条）：

```
8000000c: bnez  a0,80000018        <- 分支点
80000010: mv    a5,a1              <- not-taken 路径 (1/3)
80000014: j     80000020           <- not-taken 路径 (2/3)
80000018: mv    a5,a2              <- taken 路径 (1/3)
8000001c: j     80000020           <- taken 路径 (2/3)  ★ 本 `j` 为修正新增 (目标 0x80000020 == pc+4)
# ★ 反例（★ MUST NOT 混淆）: 循环回边 80000124 `bne s2,s6,800000e8` 目标 = 0x800000e8 ≠ pc+4 = 0x80000128
#   ⇒ 其 taken 1999 次但**不计入** cycles−pc_changes ⇒ 「目标==pc+4」**只**是 `branch_sel` 的 `j` 的性质，不是通则
80000020: mv    a0,a5 ; ret        <- 共享尾
```
⇒ ★ **含 `bnez`/`ret` 各 5 条**（`bnez` + `{mv,j}` + `mv` + `ret`）；★ 只数「路径专有指令」则各 3 条 —— **口径 MUST 写明**，否则复算会误判
⇒ ★ **由 AC-F1-1 直接推出**：4 档执行**同一条指令序列** ⇒ **动态指令数结构上必然相同**（这是**证明**，不是测量）

**AC-F1-3** —— 证据（★ 区分两个产物，避免 conflate）：

```
# (a) .text 段（376 B）逐档 **sha256 相同**  => 静态指令流一致
$ objcopy -O binary --only-section=.text build/branchbench_m0.elf .bb_m0.text
$ sha256sum .bb_m0.text ; ...   # m0..m3 全部 = 2916f2a5c7cf95db...

# (b) 加载镜像（objcopy -O binary 全镜像, 4104 B）唯一差异 = 偏移 4101 的 mode 字
$ objcopy -O binary build/branchbench_m0.elf flat_m0.bin ; ... m1
$ cmp -l flat_m0.bin flat_m1.bin   =>  "4101 0 1"
```
★ **MUST NOT** 说「ELF 文件唯一差异 = 1 字节」—— **ELF 文件本身**差 7 字节
（`.rodata` 的 `bench_mode` 字 + `.strtab` 里编译器临时对象名 `cc*.o`，后者**不可控**）
⇒ ELF 字节级**不可复现**，任何「ELF 逐字节比对」都会 flaky ⇒ 判据 MUST 用 **.text 段** 或**加载镜像**。

### 动态测量（`--stats`，CH_MEM/Verilator 后端）

| mode | 档名 | TOHOST | cycles | pc_changes(前端) | cycles−pc_changes | 独立复算 taken |
|---|---|---|---|---|---|---|
| 0 | `always_not_taken` | 103 | 86034 | 86034 | 0 | 0 |
| 1 | `always_taken` | 183 | 88034 | 86034 | 2000 | 2000 |
| 2 | `pattern_6of8` | 147 | 86534 | 86034 | 500 | 500 |
| 3 | `pseudo_random` | 101 | 87056 | 86034 | 1022 | 1022 |

- ★ **独立复算**（★ 打破循环，critic B2）：`branch_sel` 的 taken 数由**源码语义**算出
  （`c1=1` ⇒ 2000/2000；`i&7>=6` ⇒ 500/2000；LCG `w&1` ⇒ 1022/2000；`c0=0` ⇒ 0）
  ⇒ 与 `cycles − pc_changes` **逐一相等**。
  ⚠️ ★ **MUST NOT** 推广成通则 —— **反例**：同循环的**回边**（`0x80000124 bne → 0x800000e8`，目标 ≠ `pc+4`）
  taken **1999** 次却**不计入**该差值 ⇒ 该差值**不是** taken 总数，语义**未确认**。
  ⚠️ **MUST NOT** 把 taken/iter 定义为 `(cycles − pc_changes)/iter` 再去「验证」它 —— 那是恒等式。
- ⚠️ **`pc_changes` 的语义** = **采样到的 PC 值的变化拍数**（★ 机制**未确认** ⇒ 不为它命名机制）
  ⇒ **≠ 动态指令数**（实测：m0 的 `bnez` **不 taken**，但其**目标** `0x80000018` 仍被采样 **2000** 次，
  而 taken 路径的 `0x8000001c` 在 m0 为 **0**；`0x80000068`（`tohost_pass`，架构 0 次）被采样 2000 次；
  `0x80000128`（循环出口，架构执行 1 次）被采样 2000 次；且 `sum(直方图) == cycles`）
  ⇒ 故它只作**动态佐证**（跨档活动量一致），**承重证据是 AC-F1-1 的静态论证**。

### AC-F1-6 判定（跨档信号强度，本 change 的预登记出口）

跨档 taken 分支数（由源码语义独立算出）= 0 / 2000 / 500 / 1022 ⇒ **极差 = 1.000 条/iter**
⇒ **≤ 1 拍/iter 阈值触发**（本基准每 iter 恰 1 条条件分支 ⇒ 该条件**恒真**）
⇒ 跨档判据（AC-8d / AC-8e）**结构性不可用** ⇒ 下游（②）性能判据 MUST 退化为「**单档绝对 Δ（`always_taken`）+ 解析模型**」
⇒ ★ 判定权归本 change；结论已写入 CSV notes + 本文件；**不留给 ②**

### 回归

- `openspec validate` 3 change 全 valid；8 架构门禁全 PASS
- `ctest -R branchbench_baseline` **PASS**
- 全量 `ctest`：**唯一失败 = `tests/cpu/integration/test_7stage_riscv.cpp:124`（已登记 pre-existing）** ⇒ **新增失败 = 0**
- `bash tools/measure_branchbench.sh --check-baseline` **exit 0**

### 证伪留痕（AC-F1-5）

| # | 0c 的说法 | 事实 |
|---|---|---|
| 1 | 「`.text` sha256 一致 ⇒ 指令数相同 ⇒ cycle 差只来自分支方向」 | `.text` 一致这一**事实仍成立**，但该**推论被证伪**：`branch_cond` 当时编译成分支链 ⇒ 各档动态指令数不同。**修正后**：工作量一致由 **AC-F1-1 的静态结构论证**证明（不是由 `.text` 一致推断） |
| 2 | 「预期 `pattern_6of8` 档 BP 收益最大」 | 该断言**按档名推断收益**，而档名原与硬件方向**互换** ⇒ 失效 ⇒ 已从 CSV notes 移除 |
| 3 | `branch_sel` 两条路径指令数相同 | 实测 taken 路径缺 1 条 `j`（3 vs 2）⇒ 不等长 ⇒ 已补 |
| 4 | 档名与硬件方向 | mode0 名为 `always_taken` 但硬件上**从不 taken**，mode1 反之 ⇒ **互换** ⇒ 已互换重命名 |

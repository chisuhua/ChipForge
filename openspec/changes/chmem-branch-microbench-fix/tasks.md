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
- [x] **AC-F1-2** ★ 用 **host 可导出的判据**证明 4 档工作量一致：**`pc_changes`（取指地址变化的 cycle 数）4 档逐位相同**（★ 由常驻 `--stats` 计数器产出；★ 气泡拍 PC 保持不变 ⇒ 不计入 ⇒ 该量 = **实际取到的指令数**（≠ cycles）；★ **MUST NOT** 用「取指数 ≈ cycles」的等价物做判据 —— 那才是循环论证；`pc_changes` 与 `cycles` 在 taken 档**不相等**（86034 vs 88034）正是非循环性的证明）+ 报告 4 档的 **气泡拍数 / iter**（= (cycles − pc_changes) / iterations，即 taken 转移数/iter —— 由 `cycles` 与 `pc_changes` 之差直接给出，**无需另设计数器**）
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
| `tools/verilator_runner/*` | ★ **前置1 建 `--stats` 框架 + retired 计数** | 前置2 **补 FLUSH 计数**；主体 **补 BP flag + 回显** | ★ **框架归前置1**，增量归后两者，**MUST NOT** 另起一套 |
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


## 实测留证（2026-10-24，branch `feature/chmem-bp-btb-gshare`）

### 构建与静态验证

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

`branch_sel` 反汇编（**两条路径各 3 条** + 共享尾 2 条）：

```
8000000c: bnez  a0,80000018        <- 分支点
80000010: mv    a5,a1              <- not-taken 路径 (1/3)
80000014: j     80000020           <- not-taken 路径 (2/3)  [3/3 = 汇合]
80000018: mv    a5,a2              <- taken 路径 (1/3)
8000001c: j     80000020           <- taken 路径 (2/3)  [3/3 = 汇合]  ★ 本 `j` 为修正新增
80000020: mv    a0,a5 ; ret        <- 共享尾
```
⇒ taken/not-taken **各 3 条**（含 `ret` 口径下各 5 条）⇒ 等长 ✓

**AC-F1-3** —— 4 档 `.text` sha256（`objcopy -O binary --only-section=.text`）：

```
m0..m3 全部 = 2916f2a5c7cf95db...  (376 bytes)   ⇒ 逐档相同 ✓
cmp -l bb_m0.bin bb_m1.bin  =>  "4101 0 1"      ⇒ 唯一差异 = 1 字节 (data 段 mode 字) ✓
```

### 动态验证（`--stats`，CH_MEM/Verilator 后端）

| mode | 档名 | TOHOST | cycles | **pc_changes** | bubbles | taken/iter | 气泡/taken |
|---|---|---|---|---|---|---|---|
| 0 | `always_not_taken` | 103 | 86034 | **86034** | 0 | 0.000 | — |
| 1 | `always_taken` | 183 | 88034 | **86034** | 2000 | 1.000 | **1.000** |
| 2 | `pattern_6of8` | 147 | 86534 | **86034** | 500 | 0.250 | **1.000** |
| 3 | `pseudo_random` | 101 | 87056 | **86034** | 1022 | 0.511 | **1.000** |

- **AC-F1-2 ✓**：`pc_changes` 4 档**逐位相同 = 86034** ⇒ 动态指令数（工作量）一致；`cycles` 差异**全部**是气泡 ⇒ 纯方向效应。
  ★ 非循环性证据：taken 档 `cycles(88034) ≠ pc_changes(86034)` ⇒ 该量**不是** cycles 的等价物。
- **AC-F1-6 判定**：跨档 taken/iter 差 = **恰好 1.000** ⇒ **≤ 1 拍/iter 阈值触发** ⇒ 跨档判据（AC-8d/AC-8e）**结构性不可用** ⇒ 下游（②）性能判据 MUST 退化为「**单档绝对 Δ（`always_taken`）+ 解析模型**」⇒ 判定权归本 change，结论已写入 CSV notes + 本文件。
- ★ **解析模型自验证**：气泡/taken = **恰好 1.000**（4 档全同）⇒ 「**每条 taken 控制转移 1 拍气泡**」被实测确认（该模型即 ② 的 AC-8b-1 结构等式的基础）。

### 回归

- `openspec validate` 3 change 全 valid；8 架构门禁全 PASS
- `ctest -R branchbench_baseline` **PASS**（60s）
- 全量 `ctest`：**唯一失败 = `tests/cpu/integration/test_7stage_riscv.cpp:124`（已登记 pre-existing）** ⇒ **新增失败 = 0**
- `bash tools/measure_branchbench.sh --check-baseline` **exit 0**

### 证伪留痕（AC-F1-5）

| # | 0c 的说法 | 事实 |
|---|---|---|
| 1 | 「`.text` sha256 一致 ⇒ 指令数相同 ⇒ cycle 差只来自分支方向」 | `.text` 一致这一**事实仍成立**，但该**推论被证伪**：`branch_cond` 当时编译成分支链（`beqz/beq/beq`）⇒ 各档动态指令数不同（mode0 过 1 个分支 / mode2 过 3 个）⇒ cycle 差**混入工作量差**。**修正后的证明方式** = `pc_changes` 常驻计数（AC-F1-2） |
| 2 | 「预期 `pattern_6of8` 档 BP 收益最大」 | 该断言**按档名推断收益**，而档名原与硬件方向**互换** ⇒ 断言失效 ⇒ 已从 CSV notes 移除 |
| 3 | `branch_sel` 两条路径指令数相同 | 实测 taken 路径缺 1 条 `j`（3 vs 2）⇒ 不等长 ⇒ 已补 `j` |
| 4 | 档名与硬件方向 | mode0 名为 `always_taken` 但硬件上**从不 taken**（`c0=0` ⇒ `bne` 不满足），mode1 反之 ⇒ **互换** ⇒ 已互换重命名 |

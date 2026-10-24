---
initiative: wave6-linux-and-productization
priority: P1
version_target: v0.11.0
status: ready
created_from: docs/architecture/bp-btb-gshare-design-brief.md
created_from_version: v2.2
adr:
  - ADR-088
depends_on: []
---

# chmem-branch-microbench-fix

> ⚠️ **本 change 是 B1 ② `chmem-bp-btb-gshare` 的硬前置**，且**独立于** ②（0c 微基准已归档 ⇒ 修正它需独立留痕）。

## Why

已归档的 `chmem-branch-microbench`（0c）的 AC-2 论证「4 档 `.text` sha256 一致 ⇒ 指令数相同 ⇒ cycle 差只来自分支方向」**被反汇编证伪**：

| 项 | 实测（2026-10-24，`objdump build/branchbench_m0.elf`） |
|---|---|
| `branch_cond` 编译成**分支链** | `beqz` + `beq`×2（**3 个条件分支**）⇒ 4 档走的路径长度不同 |
| 各档**动态指令数**不同 | mode0: 3 条 / mode1: 5 条 / mode2: 10 条 / mode3: 8 条 |
| `branch_sel` 两条路径**不等长** | taken: `bnez`+`mv`+`mv`+`ret` = **4**；not-taken: `bnez`+`mv`+`j`+`mv`+`ret` = **5** |
| ⇒ **0c 基线的 cycle 差混入了工作量差** | 66034 / 68034 / 79534 / 73012 **不能**直接用作 ② 的判据地基 |

⇒ ★ **额外发现（档名反向）**：`branch_sel` 的 `bnez` 在 `cond≠0` 时 taken，而 `branch_cond` 的 mode0 返回 0 ⇒ ★ **mode0「always_taken」实为硬件上的 never-taken**；mode1 反之 ⇒ **mode0 与 mode1 是「互换」** ⇒ 任何按档名推断收益的推理都会**反**。

⇒ **本 change 修两件事**：① 工作量污染（`branch_cond` 无分支化 + `branch_sel` 两路径等长）② 档名与硬件方向一致（互换重命名）。

### ★ 修法（**已验证可行**，2026-10-24）

| 问题 | 修法 | 验证 |
|---|---|---|
| `branch_cond` 生成分支链 | 改**位运算 select**：`eq0&c0 \| eq1&c1 \| eq2&c2 \| eq3&c3` | ✅ 实测编译成 `andi/sltiu/seqz/and/or`，**0 个分支** |
| `branch_sel` 路径不等长 | taken 路径补一条 `j` | ✅ 实测 `bnez/mv/j` + `mv/mv/ret`，**两条路径各 4 条** |

## What Changes

### In Scope（5 项）

1. **`branch_cond` 无分支化** —— `tests/cpu/manual_elf/branchbench/branchbench.c`
2. **`branch_sel` 两路径等长** —— 同文件
3. ★ **档名互换重命名**（mode0 ⇄ mode1）+ **改名传播清单**（同 commit）
4. **基线重录** —— `soc/cpu/docs/dse/branchbench-baseline-matrix.csv`（数值会变；四档**可能趋平**）
5. **常驻测量设施** —— `cpu_verilator_sim --stats`：**PC 轨迹变化点数**（`pc_changes`）+ 逐 cycle PC 直方图
   ★ **不是**「已执行指令数」—— 该量含**投机/错路取指**（实测 `0x80000068`/`0x80000128` 各 2000 次，架构不可达）
   ★ 退休（retired）计数需 flush 信号（DUT 内部，host 不可见）⇒ **归前置2**

### Out of Scope（4 项）

- ❌ **不实现分支预测器**（那是 ② 的范围）
- ❌ **不新增「分支密度可调」负载**（★ AC-F1-6 的默认出口是「改判据」而非「改负载」⇒ 避免「为凑判据改基准」）
- ❌ **不修改已归档的 0c change 产物**（证伪留痕写入**本 change** 的 proposal + CSV notes）
- ❌ **不改 ② 的任何代码**

## Capabilities

- **判据地基可靠**：四档的**动态指令数相同** ⇒ cycle 差**只**来自分支方向
- **档名可信**：档名与硬件分支方向一致 ⇒ 按档名推理收益不会反
- **测量可复核**：事件计数常驻（不再需要「临时改 `sim_main.cpp`」的一次性 hack）

## Impact

- **代码**：仅 `tests/cpu/manual_elf/branchbench/branchbench.c` + `tools/measure_branchbench.sh` + `tools/verilator_runner/`（计数器）+ CMake
- **数据**：`branchbench-baseline-matrix.csv` 重录（★ 数值会变 ⇒ 旧值作废）
- **下游**：② `chmem-bp-btb-gshare` 的 AC-8 判据依赖本 change 的产物

## Acceptance

见 `tasks.md` 的 AC-F1-1 … AC-F1-7（**逐条对应架构简报 §4.1**）。

## References

- 架构方案简报（权威）：`docs/architecture/bp-btb-gshare-design-brief.md` §2.4 / §4.1
- 已归档的 0c：`openspec/changes/archive/2026-10-24-chmem-branch-microbench/`
- 基准 CSV：`soc/cpu/docs/dse/branchbench-baseline-matrix.csv`

## 规格来源（spec preamble 的归档副本）

> Capability topic: `chmem-branch-microbench-fix`（分支微基准的**工作量污染修正** + **档名方向修正**）。
> 上游 = `docs/architecture/bp-btb-gshare-design-brief.md` §2.4 / §4.1。

## ★ 实施结果（2026-10-24，已完成）

| 项 | 结果 |
|---|---|
| `branch_cond` 无分支化 | ✅ 反汇编 **0 条条件分支**（`andi/sltiu/seqz/and/or`） |
| `branch_sel` 两路径等长 | ✅ taken / not-taken **各 3 条**（taken 路径补 1 条 `j`） |
| 档名互换重命名 | ✅ `always_taken` ⇄ `always_not_taken`（CSV / 脚本 / 代码注释 / 下游文档指针） |
| 4 档 `.text` sha256 | ✅ **逐档相同**（`2916f2a5…`，376 B）★ **加载镜像**（全镜像 4104 B）唯一差异 = 偏移 4101 的 mode 字。★ **MUST NOT** 说「ELF 唯一差异 = 1 字节」—— ELF 文件本身差 7 字节（`.strtab` 的临时对象名不可控）⇒ 判据 MUST 用 `.text` 段或加载镜像 |
| 工作量一致性 | ✅ **承重证据 = 静态结构论证**（`branch_cond` 0 条件分支 + `branch_sel` 两路径等长 ⇒ 4 档执行同一指令序列 ⇒ 动态指令数**结构上必然相同**）；动态佐证 = `pc_changes` 4 档逐位相同 = 86034（**前端活动量**，**非**已执行指令数） |
| 重定向代价 | ⚠️ **部分确认**：`cycles − pc_changes` = 0/2000/500/1022，与**由源码语义独立算出的 `branch_sel` taken 数**逐一相等 ⇒ 该条分支**每条 taken 额外 1 拍**。★ **但循环回边**（`0x80000124 bne → 0x800000e8`，taken 1999 次，目标 ≠ `pc+4`）**不计入**该差值 ⇒ **该差值不是 taken 总数**，其语义**未确认**；★ **MUST NOT** 用 `(cycles − pc_changes)/iter` 去「验证」气泡/taken（恒等式）。★ 该模型的**通则性 MUST 由 ② 用「BP 关态的 taken 数」独立验证** |
| 新基线 cycles | 86034 / 88034 / 86534 / 87056（旧 66034/68034/79534/73012 已作废） |
| 回归 | ✅ 8 门禁全 PASS；全量 ctest **新增失败 = 0**（唯一失败为已登记 pre-existing） |

### ★ AC-F1-6 判定（本 change 的**预登记出口**，判定权归本 change）

跨档 **taken 分支数**（由源码语义独立算出）= 0 / 2000 / 500 / 1022 ⇒ **极差 = 1.000 条/iter**（= 1 条条件分支/iter）
⇒ **≤ 1 拍/iter 阈值触发** ⇒ 跨档判据（AC-8d / AC-8e）**结构性不可用**
⇒ ★ **下游（②）的性能判据 MUST 退化为「单档绝对 Δ（`always_taken`）+ 解析模型」**
⇒ ★ 结论已写入 CSV notes + 本 change 归档产物，**不留给 ②**（② 对「判据不可用」有既得利益）。

### ★ 证伪留痕（AC-F1-5，不改动归档产物）

| # | 0c 的说法 | 事实 |
|---|---|---|
| 1 | 「`.text` 一致 ⇒ 指令数相同 ⇒ cycle 差只来自分支方向」 | `.text` 一致**事实仍成立**；推论**被证伪**（`branch_cond` 编译成分支链 ⇒ 各档动态指令数不同）⇒ **修正后的证明 = AC-F1-1 的静态结构论证**（无分支的档位选择 + 等长的两条路径）；`pc_changes` 仅作**前端活动量**的动态佐证 |
| 2 | 「预期 `pattern_6of8` 档 BP 收益最大」 | 按**档名**推断收益，而档名与硬件方向**互换** ⇒ 失效 ⇒ 已从 CSV notes 移除 |
| 3 | `branch_sel` 两路径指令数相同 | 实测 taken 缺 1 条 `j`（3 vs 2）⇒ 已补 |
| 4 | 档名方向 | mode0 名 `always_taken` 实为硬件 never-taken，mode1 反之 ⇒ 已互换 |

★ 证伪记录写入**本 change** 的 proposal 与 CSV notes（**未**修改已归档的 0c 目录）。

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
5. **常驻测量设施** —— 「已执行指令数」+「FLUSH 事件数」两个 host 可见计数器（`cpu_verilator_sim --stats`）⇒ 供本 change 的 AC 与 ② 的 AC-8b 使用

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

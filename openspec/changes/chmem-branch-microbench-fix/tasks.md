---
change: chmem-branch-microbench-fix
priority: P1
initiative: wave6-linux-and-productization
adr:
  - ADR-088
plan: docs/architecture/bp-btb-gshare-design-brief.md
status: in-progress
---

# Tasks — chmem-branch-microbench-fix

> **定位**: B1 ② `chmem-bp-btb-gshare` 的**硬前置**（独立 change）。修正 0c 微基准的**工作量污染** + **档名方向反向**。
> **权威规格**: `docs/architecture/bp-btb-gshare-design-brief.md` §2.4 / §4.1 / §5.5
> **不可触碰**: `ip/**`（零生产代码改动）· ② 的任何代码

## Implementation

- [ ] **Task 1** `branch_cond` 无分支化（位运算 select）⇒ `objdump` 验证 **0 条条件分支**
- [ ] **Task 2** `branch_sel` 两条路径补齐等长（taken 路径补一条 `j`）⇒ `objdump` 验证两路径指令数相等
- [ ] **Task 3** ★ **档名互换重命名**（mode0 ⇄ mode1）+ **改名传播清单**（同 commit）：CSV `mode_name` · `tools/measure_branchbench.sh` 的模式名数组 · 文档旧档名
- [ ] **Task 4** ★ **常驻测量设施**：「已执行指令数」+「FLUSH 事件数」两个 host 可见计数器 + `cpu_verilator_sim --stats` 输出行（⇒ 取代「临时改 `sim_main.cpp`」的一次性 hack）
- [ ] **Task 5** 基线重录 + CI/ctest 保持绿 + 证伪留痕（写入本 change 的 proposal + CSV notes，**不改**已归档的 0c 目录）

## Acceptance

- [ ] **AC-F1-1** `objdump` 证明 `branch_cond` **0 条条件分支**（纯 `sltiu/seqz/and/or`）+ `branch_sel` 两条路径**指令数相等**（★ 仅证**静态**一致）
- [ ] **AC-F1-2** ★ 用**计数器**证明 4 档**动态指令数相等**（★ `.text` 相同 + objdump **无法**证动态一致）+ 报告 4 档的 **taken 分支数/iter**
- [ ] **AC-F1-3** 新 `.text` sha256 入册 + CSV 重录 + CI `branchbench_baseline` 仍绿
- [ ] **AC-F1-4** ★ **档名与硬件方向一致** —— ★ **mode0 与 mode1 是「互换」不是单改名** ⇒ **强制互换重命名**（**删掉「修正注释」这个弱选项**）+ ★ **改名传播清单**（同 commit）
- [ ] **AC-F1-5** ★ **留痕**：0c 的 AC-2「`.text` sha256 一致」**事实仍成立**，其推论「`.text` 一致 ⇒ 动态指令数相同」**被证伪** ⇒ 写入本 change 的 proposal + CSV notes（**不得**改动归档产物）
- [ ] **AC-F1-6** ★ **预登记默认出口**（★ 删除「或新增密度可调负载」这个开放式口子）：若跨档 taken 分支数差 **< 1 拍/iter** ⇒ **直接**采用「`always_taken` 单档绝对 Δ + 解析模型」判据 ⇒ ★ **判定权归本 change**（独立执行者）⇒ 结论写入 CSV notes + 本 change 归档产物 ⇒ **不留给 ②**（② 对「判据不可用」有既得利益）
- [ ] **AC-F1-7** ★ **改名与脚本改动必须同一 commit** ⇒ 否则 `measure_branchbench.sh` 的 `grep -E "^${name},"` 找不到行 ⇒ CI `branchbench_baseline` 立刻红

## ★ builder 开工须知（10 条，**逐条确认**）

| # | 须知 |
|---|------|
| 1 | 台账条目行**必须正好 7 列**；**理由列不得含未转义的 `\|`** |
| 2 | 新增 CH_MEM 侧 case 前**先快照 TLM-only 名单**（`comm -23`）再命名 |
| 3 | 台账脚本需**两个 binary** ⇒ **本地跑 + 输出贴 PR**（CI warn 级无强制力） |
| 4 | ★ CSV / 脚本的**改名必须同一 commit** |
| 5 | ★ CSV **按表头名取列**（**强制**；现 notes 含逗号 ⇒ 位置访问必错位）；`--check-baseline` 分组键 = `(workload_sha, bp_mode, build_id)` |
| 6 | `--btb-entries N` 合法域 = 16/32/64/128/256；**越界 fail-fast**（不得静默 clamp） |
| 7 | ★ 本 change 的 diff **不得触碰** `ip/**`（零生产代码改动） |
| 8 | ★ 修正 `ibus_chmem.h:83-87` 的**过时注释**（「aread/sread 都是 1-cycle」与 aread 0-cycle 的承重前提矛盾） |
| 9 | `build_cpu_with_elaborate`（`cpu_factory_chmem.h:451-458`）**现状就已丢失** `enable_mmu`/`mmu_mode`/`enable_cache` 三个参数 ⇒ 顺带修全部 4 个的转发（或显式登记为 known gap） |
| 10 | 顺手修 `branch_chmem.h:20` 的注释（「JAL/JALR 推迟」）与 `:221-226` 的实装不符 |

## 实测留证

（实施后填写）

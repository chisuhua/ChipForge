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
- [ ] **Task 4** ★ **常驻测量设施（仅 host 可导出的部分）**：「**已执行(retired)指令数**」计数器（★ 由 **PC 轨迹**导出 ⇒ host 可见；★ **FLUSH 事件数归前置2** —— FLUSH 是 DUT 内部 payload，**host 不可见** ⇒ 需改 `ip/cpu/plugins/ibus_chmem.h` ⇒ **违反本 change 的「不得触碰 `ip/**`」边界**）+ `cpu_verilator_sim --stats` 输出行（⇒ 取代「临时改 `sim_main.cpp`」的一次性 hack）
- [ ] **Task 5** 基线重录 + CI/ctest 保持绿 + 证伪留痕（写入本 change 的 proposal + CSV notes，**不改**已归档的 0c 目录）

## Acceptance

- [ ] **AC-F1-1** `objdump` 证明 `branch_cond` **0 条条件分支**（纯 `sltiu/seqz/and/or`）+ `branch_sel` 两条路径**指令数相等**（★ 仅证**静态**一致；★ **计数口径**：以 objdump 的函数体为准，**含 `ret`**；★ MUST 报告两段 objdump 片段与各自的条数）
- [ ] **AC-F1-2** ★ 用 **host 可导出的判据**证明 4 档工作量一致：**除分支 PC 与其两个后继外**，各静态 PC 的**取指次数逐档相同**（★ 由 PC 轨迹导出；★ `.text` 相同 + objdump **无法**证动态一致；★ **MUST NOT** 用「取指数」证明 —— 取指数 ≈ cycles ⇒ 循环论证）+ 报告 4 档的 **taken 转移数/iter**（★ 这是「跨档信号强度」的输入，**不是**「动态指令数相等」的替代品 —— 两者是不同量，MUST NOT 混写）
- [ ] **AC-F1-3** ★ **4 档 `.text` sha256 MUST 逐档相同**（新值入册）—— ★ 改写 `branch_cond`/`branch_sel` 后**必须继续成立**（否则档间差异不再只是「data 段 1 个字」⇒ 判据地基失效）+ `bench_mode` 仍是唯一档间差异 + CSV 重录 + ★ **本地** `bash tools/measure_branchbench.sh --check-baseline` **exit 0**（★ CI 该 job 是 **warn-level** ⇒ 以本地为准）
- [ ] **AC-F1-4** ★ **档名与硬件方向一致** —— ★ **mode0 与 mode1 是「互换」不是单改名** ⇒ **强制互换重命名**（**删掉「修正注释」这个弱选项**）+ ★ **改名传播清单**（同 commit）
- [ ] **AC-F1-5** ★ **留痕 + 替换失效断言**：0c 的 AC-2「`.text` sha256 一致」**事实仍成立**，其推论「`.text` 一致 ⇒ 动态指令数相同」**被证伪** ⇒ 写入本 change 的 proposal + CSV notes（**不得**改动归档产物）。★ **CSV notes MUST 替换/删除两条失效断言**：① 「`.text` 一致 ⇒ 指令数一致 ⇒ cycle 差只来自分支方向」（**已被证伪**）② 「预期本档收益最大/最小」（**按档名推断收益** ⇒ 档名反向使其失效）⇒ 否则 CSV 会继续向下游提供**反向证据**。★ 佐证：归档基线自身已记录档名反向（`always_taken` 33.017 c/iter **快于** `always_not_taken` 34.017 ⇒ 名为 always_taken 的档在硬件上从不 taken ⇒ 无气泡 ⇒ 最快）
- [ ] **AC-F1-6** ★ **预登记默认出口**（★ 删除「或新增密度可调负载」这个开放式口子）：若跨档 taken 转移数差 **≤ 1 拍/iter**（★ **含恰好 1.0** —— 本基准每 iter 恰 1 条条件分支 ⇒ 跨档差**恰好 = 1.0** ⇒ 严格小于永不触发 ⇒ **必须用 ≤**）⇒ **直接**采用「`always_taken` 单档绝对 Δ + 解析模型」判据 ⇒ ★ **判定权归本 change**（独立执行者）⇒ 结论写入 CSV notes + 本 change 归档产物 ⇒ **不留给 ②**（② 对「判据不可用」有既得利益）
- [ ] **AC-F1-7** ★ **改名与脚本改动必须同一 commit** ⇒ 否则 `measure_branchbench.sh` 的 `grep -E "^${name},"` 找不到行 ⇒ CI `branchbench_baseline` 立刻红

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

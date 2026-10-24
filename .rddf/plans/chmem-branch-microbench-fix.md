# ⑤-前置1 chmem-branch-microbench-fix — 执行文件

> **change**: `chmem-branch-microbench-fix`（B1 ② 的**地基修正**）
> **规模**: 小 · **前置**: 无
> **权威规格**: `docs/architecture/bp-btb-gshare-design-brief.md` **v2.2** §2.4 / §4.1
> ★ **AC 清单**: `openspec/changes/chmem-branch-microbench-fix/tasks.md`（AC-F1-1 … AC-F1-7）
> ★ **owner 表**: 见本 change 的 tasks.md（★ 本 change = 前置1 owner：`branchbench.c` / 基线 CSV 建列 / `measure_branchbench.sh` / `--stats` 框架 + retired 计数）

---

## Goal

修正已归档的 0c 微基准（`chmem-branch-microbench`）的**工作量污染** + **档名方向反向**，使 ② 的性能判据**地基可信**。

## Tasks

- [ ] **Task 1** `branch_cond` 无分支化（位运算 select）⇒ `objdump` 验证 **0 条条件分支**
- [ ] **Task 2** `branch_sel` 两条路径补齐等长（taken 路径补一条 `j`）⇒ `objdump` 验证两路径指令数相等（含 `ret`）
- [ ] **Task 3** ★ **档名互换重命名**（mode0 ⇄ mode1）+ 改名传播清单（同 commit：CSV `mode_name` · `measure_branchbench.sh` 的 `MODE_NAMES` · 文档旧档名）
- [ ] **Task 4** ★ **常驻测量设施（仅 host 可导出的部分）**：「**已执行(retired)指令数**」计数器（由 PC 轨迹导出）+ `--stats` 输出行（★ **FLUSH 事件数归前置2** —— host 不可见 ⇒ 须改 `ip/` ⇒ 违反本 change 的「不得触碰 `ip/**`」边界）
- [ ] **Task 5** 基线重录（★ **4 档 `.text` sha256 MUST 逐档相同**）+ 建列（`workload_sha` / `build_id` / `bp_mode=off`）+ 改按表头取列 + **捕获 stderr** + 证伪留痕（★ **MUST 替换 CSV notes 中两条失效断言**）

## ★ 兜底句纪律

任何「上报 architect / 待裁决」句 MUST 同句给出 **① 触发阈值 ② 落点文件 ③ 可判定时限**（「≤ 2 个工作日」或「本 change 内闭环」）；MUST NOT 以「等待回复」结束本 change。

## ★ 不可触碰（owner 表）

`ip/**`（零生产代码改动）· `docs/architecture/tlm-coverage-parity-ledger.md`（主体 owner）· `tools/measure_branchbench.sh` 的 flag 增量（前置2 的范围）· `tools/verilator_runner/*` 的 FLUSH 分量（前置2 的范围）

## 实测留证

（实施后填写）

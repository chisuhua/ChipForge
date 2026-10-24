---
change: chmem-branch-microbench
priority: P1
initiative: wave6-linux-and-productization
adr:
  - ADR-088
  - ADR-040
plan: .rddf/plans/chmem-branch-microbench.md
status: in-progress
---

# Tasks — chmem-branch-microbench

> **交付目标**: 为 B1 ② `chmem-bp-btb-gshare` 提供**可机械判定**的中间验收手段 + **无 BP 参照值**。
> **不实现 BP**（② 的范围）· **不移植 CoreMark**（交付点 B）· **零 `ip/` 改动**。

## Phase 0 备忘

- **P0 决策**: case-1 approve（auto-decision ADR-0049：AC 7 + 7 文件 + CI 门禁 ⇒ 完整 P1–P3）
- **上游**: ADR-088 **v1.2 §5.3.2 P-5**（中间验收改用分支 microbenchmark，CoreMark 允许顺延）· **P-6**（绝对值门禁恢复条件 = ② 落地 ⇒ ② 同时解锁两件事）
- **★实测纠正（实施期，2 处）**：
  ① **golden 不能进二进制** —— 初版把期望校验和做成编译期常量 ⇒ 被折进 `.text` ⇒ 4 档机器码不同 ⇒ cycle 差混入工作量差异（实测 mode2/3 明显更慢）⇒ 改为**宿主侧比对**（程序恒写 `sum|1`，期望值存基线 CSV）⇒ `.text` sha256 四档一致
  ② **模式值必须放数据** —— 编译期常量会被编译器折叠 ⇒ select 被折叠 ⇒ 各档指令流不同 ⇒ 改 `volatile const uint32_t bench_mode`（运行时读，不可折叠）

## Implementation

- [x] Task 1 负载实现（`branchbench.c` + `branchbench_crt0.S` + `link_branchbench.ld` + CMake 4 目标）
- [x] Task 2 测量脚本 + 基线 CSV（4 档无 BP 参照值）
- [x] Task 3 CI 门禁 step + ctest entry
- [x] Task 4 收口（8 门禁 + 全量回归 + 零 `ip/` 改动 + commit）

## Acceptance

- [x] AC-1 4 个 ELF 均 **RX+RW 两段**（`readelf -l`：`LOAD 0x80000000 R E` + `LOAD 0x80001000 RW`）+ `tohost`=0x80001000 + `nm -u` **空** + 同参数两次构建**字节一致**
- [x] AC-2 4 档 `.text` **sha256 完全一致**（实测 `5a7c0b2c4265c156` × 4）⇒ 指令数相同 ⇒ cycle 差只来自分支方向
- [x] AC-3 4 档均跑完（`TOHOST` = 校验和|1，LSB=1 ⇒ done 位成立 ⇒ cycle 计数 = 真实完成时刻）+ **宿主侧校验和与基线逐位相等**（不符 ⇒ exit 1 且不产出数值）
- [x] AC-4 基线 CSV 入库（4 档 = **无 BP 参照值**）+ `--check-baseline` **精确相等**漂移检测（负例实测 exit 1）
- [x] AC-5 ctest entry `branchbench_baseline`（LABELS `verilator;perf;e2e`，TIMEOUT 300，实测 82.16s）+ CI 门禁 step（warn 级，steps 12 → **13**；缺 binary/elf 显式 SKIP）
- [x] AC-6 8 架构门禁全 PASS + 全量回归 0 退化 + **零 `ip/` 改动**
- [x] AC-7 **不依赖 CoreMark** ⇒ ② 的中间验收不被交付点 B 阻塞

### 实测留证（2026-10-24，branch `feature/chmem-branch-microbench`）

| 项 | 实测 |
|---|---|
| 4 档 `.text` sha256 | `5a7c0b2c4265c156`（4 档**完全一致**） |
| 4 档实测（无 BP 参照值） | `always_taken` TOHOST=103 / **66034** cycles · `always_not_taken` 183 / **68034** · `pattern_6of8` 147 / **79534** · `pseudo_random` 101 / **73012** |
| 形态观察 | 方向**恒定**的档（m0/m1）cycle 最少；方向**周期性变化**的 m2 最多；伪随机 m3 居中 ⇒ CH_MEM 侧分支代价与**方向变化次数**相关 ⇒ **正是 BP 可消除的部分** ⇒ 微基准对 BP 敏感 |
| 段布局 | `LOAD 0x80000000 R E` + `LOAD 0x80001000 RW`；`tohost`=0x80001000；`nm -u` 空 |
| 变异 A | 基线期望值篡改（103→999）⇒ `校验和不符 —— 实测 TOHOST=103 != 基线期望 999` + exit 1 ✅ |
| 变异 B | 基线 cycles 篡改（66034→99999）⇒ `基线漂移: 基线 99999 != 实测 66034` + exit 1 ✅ |
| 还原 | 4 档 `checksum-ok` + `基线一致 (4 档) => PASS` + exit 0 ✅ |
| ctest | `branchbench_baseline` PASS（82.16s） |
| CI | `architecture-gates.yml` steps 12 → **13**（新增 `Check CH_MEM branchbench baseline`，warn 级） |
| 8 门禁 | 全绿（verify_adr / verify_plugin_decision 8/8 / check_plugin_portability 13/13 / doc_link_check 0 broken / roadmap-r9-check 11 fragments / check_xlen_hygiene warn=0 / check_tlm_freeze violations=0 / check_coverage_parity_ledger PASS） |
| 零 `ip/` 改动 | `git status` 中 `ip/` 改动数 = **0** |

### 实施期修正（2 处，已回写 plan）

| # | v1 判断 | 实测 |
|---|---------|------|
| 1 | 「golden 做编译期常量」 | **错** —— 折进 `.text` ⇒ 4 档机器码不同 ⇒ 污染对比 ⇒ 改为**宿主侧比对**（程序恒写 `sum\|1`） |
| 2 | 「模式做编译期常量」 | **错** —— select 被折叠 ⇒ 各档指令流不同 ⇒ 改 `volatile` 全局（运行时读） |

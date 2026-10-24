# chmem-branch-microbench — 执行文件（planner 起草，2026-10-24）

> **change**: `chmem-branch-microbench`
> **来源**: ADR-088 **v1.2 §5.3.2 P-5**（architect 批准：交付点 B 的中间验收改用**分支 microbenchmark**，CoreMark 允许顺延 ⇒ ② 不被 CoreMark 移植的不确定性阻塞）
> **规模**: 小-中 · **前置**: 无
> **被依赖**: ② `chmem-bp-btb-gshare`（中间验收 + 增益量化）· 交付点 B `chmem-verilator-coremark-harness`（前置）
> **性质**: **测量资产**（类比 ① 的 Dhrystone harness）—— 提供**可控预测率**的分支负载，使 BP 的收益**可量化**（不依赖 CoreMark 的绝对分数）

---

## Goal

交付一个**分支微基准**：在 CH_MEM/Verilator 后端上以**可控的预测率**运行分支密集循环，产出**可比较的指标**（cycles/iter、flush 周期、分支计数），使 ② 的 BP 验收判据从「CoreMark/MHz ≥1.9（绝对值，ADR 未定）」变为「**同一负载下 BP 引入前后的 cycle 降幅可量化**」。

## Architecture

```
tests/cpu/manual_elf/branchbench/         (新)
  ├─ branchbench.c      (可控预测率的分支循环 + 自校验)
  ├─ branchbench_crt0.S (栈初始化 + tohost 协议, 照抄 dhrystone_crt0.S)
  └─ link_branchbench.ld (PHDRS 双段, 照抄 link_dhrystone.ld —— 修 CH_MEM 段路由)
tools/measure_branchbench.sh             (新: 跑 + 解析 + 与基线比对)
soc/cpu/docs/dse/branchbench-baseline-matrix.csv  (新: 基线)
```

**负载设计（可预测率的关键）**：

| 模式 | 构造 | 期望的 BP 行为 |
|------|------|---------------|
| `always_taken` | 恒 taken 的条件分支 | 理想预测器 ≈100% 命中 ⇒ **上界参照** |
| `always_not_taken` | 恒 not-taken | 同上（另一侧参照） |
| `pattern_6of8` | `if (i & 7) < 6` ⇒ 75% taken，**周期性可预测** | 中等命中 ⇒ 中等收益 |
| `pseudo_random` | LCG 驱动的伪随机 taken | 理想预测器也难命中 ⇒ **下界参照**（BP 收益应最小） |

⇒ 四档负载给出「**预测率 ↑ ⇒ BP 收益 ↑**」的**单调性判据** ⇒ ② 的验收不依赖任何绝对分数：判据 = 「**在 `pattern_6of8` 上，启用 BP 后 cycles/iter 显著低于禁用 BP**（阈值由本 change 的基线实测确定）」+「**四档单调性成立**（`always_taken` 收益最大、`pseudo_random` 最小）」⇒ 单调性破坏 ⇒ BP 实现有 bug。

**自校验**（结果正确性）：循环结束前把「迭代数 + 分支计数 + 累加校验和」写到**固定地址**，host 侧读回比对 ⇒ 避免「cycle 好看但结果错」。

## Tech Stack

C / 汇编（crt0）· RISC-V toolchain（**必须用 `riscv32-unknown-elf-g++`**，与 CMake 目标一致 ⇒ `.text` 布局一致）· Bash 测量脚本 · Verilator（已有）

---

## Tasks

### Task 1: 负载实现

- [ ] **Step 1: 写 `branchbench.c`** —— 四档模式（编译期常量选择，或运行时参数写入固定地址）；每档固定工作量（同样的指令数，仅分支可预测性不同）⇒ **cycle 差只来自分支预测**
- [ ] **Step 2: 写 `branchbench_crt0.S` + `link_branchbench.ld`** —— 照抄 `dhrystone_crt0.S` + `link_dhrystone.ld`（**PHDRS 双段**，否则 CH_MEM 段路由落空 ⇒ IBus 为空）
- [ ] **Step 3: 写 `build_manual_elf.sh` 目标** —— ⚠️ **注意**：dhrystone 走 **CMake** 目标（`src/cf_plugin/CMakeLists.txt:136-146`）⇒ 本 microbench **同样走 CMake**（加进 `ELF_PAIR` 列表），**不新增 shell 路径**（避免两条构建路径 ⇒ 字节不一致，见 `chmem-verilator-perf-harness` 的实施期修正 #1）
- [ ] **Step 4: 构建** —— `readelf -l` 验两段（RX+RW）+ `nm -u` 空 + **两次构建字节一致**

### Task 2: 测量脚本 + 基线

- [ ] **Step 1: `tools/measure_branchbench.sh`** —— 跑 `cpu_verilator_sim` ⇒ 解析 `TOHOST`/`CYCLES` + 读回固定地址的「迭代数/分支计数/校验和」⇒ **结果不匹配 ⇒ 失败**（与 ① 同一纪律：TOHOST==1 且非 TIMEOUT）
- [ ] **Step 2: 四档基线 CSV** —— `soc/cpu/docs/dse/branchbench-baseline-matrix.csv`：每档一行（mode / cycles / cycles_per_iter / branch_count / checksum_ok / notes）
- [ ] **Step 3: 禁用 BP 的基线** —— ② 未实现前，CH_MEM 侧**无 BP** ⇒ 基线即「无 BP 参照值」⇒ ② 实现后用同一脚本重测 ⇒ 差值即 BP 增益
- [ ] **Step 4: ctest entry + CI 门禁** —— 与 ① 同形态（ctest `branchbench_baseline` + CI warn 级 step）；⚠️ CI 不跑 ctest ⇒ **CI step 才是判据 4 的贡献**

### Task 3: 收口

- [ ] **Step 1: 8 门禁** + **Step 2: 全量回归 0 退化** + **Step 3: commit**

---

## Acceptance

- [ ] AC-1: `branchbench.elf` 有 RX+RW 两段 + `tohost`=0x80001000 + `nm -u` 空 + 两次构建字节一致（**可复现**）
- [ ] AC-2: 四档负载均能在 CH_MEM/Verilator 跑完（`TOHOST=1`）+ **自校验通过**（迭代数/分支计数/校验和与参考一致）
- [ ] AC-3: 四档的 **cycles/iter 单调性**成立（`always_taken` ≤ `pattern_6of8` ≤ `pseudo_random`；且 `always_taken` 与 `always_not_taken` 收益均 ≥ `pseudo_random`）⇒ **单调性破坏 ⇒ BP 实现有 bug**（这是 ② 的核心验收锚）
- [ ] AC-4: 测量脚本可**分别测量启用/禁用 BP**（② 实现前 = 无 BP 基线；② 实现后 = 有 BP 值）⇒ 增益 = (无BP - 有BP)/无BP **可量化**
- [ ] AC-5: 基线 CSV 入库（**只含** CH_MEM/Verilator 侧数据）+ ctest entry + CI 门禁 step（warn 级）
- [ ] AC-6: 8 门禁全 PASS + 全量回归 0 退化 + 零 `ip/` 改动（只加测试资产 + 工具脚本 + CSV + CI/CMake）
- [ ] AC-7: **不依赖 CoreMark** ⇒ ② 的中间验收不被交付点 B 阻塞（ADR-088 §5.3.2 P-5）

## 风险与未知

| # | 风险 | 缓解 |
|---|------|------|
| R1 | 「周期数差异」可能来自**工作量差异**而非分支预测 ⇒ 结论无效 | 四档**指令数严格相同**（只改可预测性）⇒ 差值只能来自分支；AC-2 的自校验同时验证工作量一致 |
| R2 | 伪随机档的 LCG 序列可能被 **简单 BP 学到** ⇒ 下界参照失真 | 用「长周期 LCG + 固定 seed」+ 记录分支计数；必要时用 ≥64K 迭代让有限 history 表学不到 ⇒ AC-3 的单调性判据能暴露此问题 |
| R3 | ② 实现后「启用 BP」的开关方式未定（静态 elaboration ⇒ BP 在编译期就在流水线里） | ② 需提供**可关闭 BP 的配置面**（如 `CPUConfig` 字段）⇒ 本 change 的 Task 2 Step 3 先记录「无 BP 基线」；② 须在 plan 中承诺提供该开关（**已登记为 ② 的接口需求**） |
| R4 | microbench 与 Dhrystone 一样是**裁剪版** ⇒ 绝对值不代表真实 workload | notes 标注「**微基准，绝对值不代表真实应用**」⇒ 只用于**相对**比较（BP 前后），不用于绝对门禁 |
| R5 | 与 ① 的构建路径冲突风险 | 走 **CMake `ELF_PAIR`**（与 dhrystone 同路径）⇒ 单一构建路径 |

## 迁移说明

落地为 `.rddf/plans/chmem-branch-microbench.md`；change 名 `chmem-branch-microbench`；建议 **rdd-quick**（测试资产 + 脚本 + CSV，AC 机械可验，零 `ip/` 改动）。
**必须在 ② 之前完成**（② 的 AC 依赖其基线与增益度量）。

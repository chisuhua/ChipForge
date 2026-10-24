# chmem-branch-microbench — 执行文件（planner 起草，2026-10-24）

> ⚠️ **档名已互换重命名**（`chmem-branch-microbench-fix`，2026-10-24）：
> 本文件出现的 `always_taken` / `always_not_taken` 是**旧名**（与硬件方向相反）⇒ 对应关系：
> 旧 `always_taken`(=66034) ⇒ 新 **`always_not_taken`**(=86034)；旧 `always_not_taken`(=68034) ⇒ 新 **`always_taken`**(=88034)。
> ★ 旧 cycles 数值（66034/68034/79534/73012）**已作废**（工作量污染）⇒ 新值见 `soc/cpu/docs/dse/branchbench-baseline-matrix.csv`。
> ★ 本文件**不是**执行依据（0c 已归档）；前置1 的执行依据 = `.rddf/plans/chmem-branch-microbench-fix.md`。

>
> ⚠️ **ARCHIVED（归档件）—— MUST NOT 作为前置1 `chmem-branch-microbench-fix` 的执行依据**
> ★ 本文件是 0c（已归档 change `chmem-branch-microbench`）的旧计划，内含**已被证伪**的判据
> （如「四档单调性 always_taken→pseudo_random」）⇒ ★ 前置1 的执行依据 MUST 以
> `.rddf/plans/chmem-branch-microbench-fix.md` 与架构简报 v2.2 为准。
> ★ 旧基线数值（66034 / 68034 / 79534 / 73012）亦已作废（工作量污染 + 档名反向）⇒ 前置1 MUST 重录。


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

### Task 1: 负载实现（★实施期 2 处纠正，见文末「实施期修正」：golden 与 mode **都不能**做编译期常量）

- [x] **Step 1: 写 `branchbench.c`** —— 四档模式（**mode 走 `volatile` 全局数据**，不可编译期常量）；每档固定工作量（显式 `beq` + 两条路径**等指令数**）⇒ **cycle 差只来自分支方向**
- [x] **Step 2: 写 `branchbench_crt0.S` + `link_branchbench.ld`** —— 照抄 `dhrystone_crt0.S`（栈初始化 + `jal _cstart` + `tail tohost_pass`；**`tohost_pass` 必须 `extern "C"`** —— 否则 C++ 名字修饰 ⇒ crt0 的 `tail tohost_pass` 链接失败，实测踩过）+ `link_branchbench.ld`（**PHDRS 双段**：RX `.text` + RW `tohost`+data；`tohost` 必须是 RW 段**第一个** output section，否则段 vaddr 偏移 ⇒ tohost 落到加载区外 ⇒ 写入被丢弃）
- [x] **Step 3: CMake 目标（★实施期纠正：走 CMake，**不**走 `build_manual_elf.sh`）** —— dhrystone 已有 CMake 目标（`src/cf_plugin/CMakeLists.txt:136-146`）⇒ 沿用**同一构建路径**（同一 `RISCV_GPP` + 同一 flags）⇒ 避免第二条构建路径（双路径 ⇒ 字节不一致，见 `chmem-verilator-perf-harness` 实施期修正 #1）
- [x] **Step 4: 构建** —— `readelf -l` 验两段（RX+RW）+ `nm -u` 空 + **两次构建字节一致** + **4 档 `.text` sha256 一致**（实测 `5a7c0b2c4265c156`）

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


---

## 实施期修正（2026-10-24，交付实测）

| # | plan v1 的设计 | 实测问题 | 修正 |
|---|---------------|---------|------|
| 1 | 期望校验和做**编译期常量**（`-DBENCH_GOLDEN`） | ① 被折进 `.text` ⇒ **4 档机器码不同** ⇒ cycle 差混入工作量差异（实测 mode2/3 明显更慢，正是这个污染）② 比较行为异常（实测 TOHOST 报的是 sum 而非 1） | **期望值移到宿主侧**：程序恒写 `sum \| 1`（LSB 置 1 ⇒ runner 的 done 位恒成立 ⇒ cycle 计数 = 真实完成时刻；tohost 同时携带校验和）⇒ 期望值存**基线 CSV** ⇒ `.text` 与 golden 无关 ⇒ 4 档一致 |
| 2 | 模式做**编译期常量** | 编译器把 select 折叠 ⇒ 各档生成的指令流不同 ⇒ 工作量差异 | 模式改 `volatile const uint32_t bench_mode`（运行时读，**不可折叠**）⇒ 4 档 `.text` sha256 一致（实测 `5a7c0b2c4265c156`） |
| 3 | 走 `build_manual_elf.sh` | dhrystone 已有 CMake 目标 ⇒ 新增 shell 路径会造成**双构建路径**（字节不一致风险） | 改走 CMake（`branchbench_elfs` 目标） |
| 4 | `tohost_pass` 直接定义 | C++ 名字修饰 ⇒ crt0 的 `tail tohost_pass` **链接失败**（undefined reference） | 加 `extern "C"`（与 dhrystone.c 同构） |
| 5 | AC-3 假设「恒 taken 档收益最大」 | 实测形态相反：方向**恒定**的档 cycle **最少**（m0 66034 / m1 68034），方向**变化**的 m2 **最多**（79534）⇒ CH_MEM 的分支代价与**方向变化次数**相关 | AC-3 改为**机制化**判据：「BP 落地后，**方向变化频繁**的档（m2/m3）Δcycles 显著大于**方向恒定**的档（m0/m1）」⇒ 恒定档本就无变化惩罚 ⇒ 收益小 —— 这与「BP 消除变化惩罚」的机制一致 |

### 实测结果（无 BP 参照值）

| 档 | 方向 | 校验和 | cycles |
|---|---|---|---|
| `always_taken` | 恒 taken | 103 | **66034** |
| `always_not_taken` | 恒 not-taken | 183 | **68034** |
| `pattern_6of8` | 6/8 taken（周期） | 147 | **79534** |
| `pseudo_random` | 伪随机 | 101 | **73012** |

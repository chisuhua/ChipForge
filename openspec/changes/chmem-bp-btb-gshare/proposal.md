---
initiative: wave6-linux-and-productization
priority: P1
version_target: v1.0.0
status: ready
created_from: docs/architecture/bp-btb-gshare-design-brief.md
created_from_version: v2.1
adr:
  - ADR-088
  - ADR-040
depends_on:
  - chmem-branch-microbench-fix
  - chmem-bp-btb-gshare-poc
---

# chmem-bp-btb-gshare

> ⚠️ **本 change 的两个前置必须先完成**（架构简报 §4 的裁决）：① `chmem-branch-microbench-fix`（修 0c 微基准的工作量污染 + 档名反向）② `chmem-bp-btb-gshare-poc`（验证「提前一拍预测 ⇒ 消除气泡」机制）。
> ★ **前置未完成前，本 change 的实现 MUST NOT 开工**（判据地基与机制前提都依赖它们）。

## Why

**ADR-088 v1.2 §3 的 BP 行**：CH_MEM 侧必须实装分支预测器（**CH_MEM-only 且必须做；不适用 §5.2 的「不做」出口**）。

### ★ 根因（architect 判决实验，2026-10-24 —— 已**修正** ADR-088 §5.3.1 的表述）

| 实验 | 结果 |
|------|------|
| **E-2**（摘掉 TLM 的 BP 注册） | cycles **794478 → 794478（逐位不变）** ⇒ ★ **TLM 的 BP 是惰性桩**（`branch_predictor.h:126-129` 预测结果被丢弃、`:149-151` flush 恒 false、`ibus.h` 0 处引用）⇒ **「TLM 有 BTB ⇒ 预测命中不 flush」被证伪** |
| **E-3**（逐拍采样 CH_MEM 的 PC） | taken 控制转移 **120065 次**；`914544 − 120065 = 794479 ≈ TLM 794478`（残差 1）⇒ ★ **全部差距被「计数到的重定向」完全吸收** ⇒ 气泡 = **1 拍 / 每次 taken 转移** |
| **E-4**（读 TLM `ibus.h:114-124`） | TLM 是**单遍 tick**（writeback LATE 同 tick 内重定向）⇒ **重定向零延迟** |

⇒ ★ **真根因 = TLM 单遍模型重定向零代价 vs CH_MEM 两级取指每条 taken 转移 1 拍气泡**。
⇒ ★ **BP 若能提前一拍预测 ⇒ 消除这 120065 拍** ⇒ CH_MEM 的绝对值门禁（DMIPS ≥ 1.4）可恢复（ADR-088 §5.3.2 **P-6**）。

### ★ 绝对值门禁的**定量门槛**（决定 ② 的范围）

| 项 | 数值 |
|---|------|
| DMIPS ≥ 1.4 需要的 cycles | ≤ 813083（CH_MEM 现状 914544） |
| **必须净省** | **≥ 101461 cycles** |
| 可消除的气泡总量 | 120065 |
| ⇒ **净命中率门槛** | **≥ 84.5%** |

⇒ ★ 「GShare 延后」**不是低风险决定** —— 它把 1.4 门禁押在「**Bimodal（无全局历史）能否在 Dhrystone 的分支混合上达到 84.5% 净收益率**」这个**未验证**的假设上。
⇒ ★ **裁决 C-2（范围）须由前置2 的实测数据触发复核**：若前置2 报告的**可达净收益率下界 < 84.5%** ⇒ **不得以「GShare 延后」结案** ⇒ 须回 ② 补 GShare（或另立 change）。

#### ★★ oracle 复审发现 2：**`jal` 的方向无来源 ⇒ call 气泡可能不可恢复**（architect 待追认项）

| 项 | 内容 |
|---|---|
| **问题** | `jal`（无条件跳转）**不占 PHT 表项**（简报 §3.3）⇒ 其 PHT 项恒 0 ⇒ 若 `pred_taken = hit && (pht ≥ 2)` ⇒ ★ **`jal` 永不被预测** ⇒ call 气泡不可恢复 |
| **叠加** | `jalr`（含 `ret`）**不建 BTB 项**（间接跳转目标每次不同）⇒ 同样不可预测 ⇒ Dhrystone 的 call/ret 气泡**双重不可恢复** |
| **对 84.5% 门槛的影响** | 若 call+ret 占 120065 次 taken 转移的比例 **> 15.5%** ⇒ ★ **84.5% 门槛在构造上不可达** ⇒ 「GShare 延后」结案不成立 |
| **★ 已采纳的处置** | BTB 表项**增加 `is_unconditional` 位** ⇒ `pred_taken = hit && (is_unconditional \|\| pht ≥ 2)` ⇒ `jal` **可预测**（`jalr` 仍不可预测，因其目标本质上运行时决定） |
| **★ 待 architect 追认** | ① `is_unconditional` 位是否采纳（本 change 已按采纳设计，**需 architect 追认**）② 前置2 **MUST 报告** `jal` / `jalr` / taken 转移数的比例 ⇒ 若 `jalr` 占比使 84.5% 构造上不可达 ⇒ architect 须重裁绝对值门禁路径（**不得**由实施者自裁） |
| 落点 | 本 proposal + 主体 `design.md` §4.1 + 主体 `tasks.md` **AC-17** + 前置2 `tasks.md` **AC-F2-15** |

## What Changes

### In Scope（7 项）

1. **`ip/cpu/plugins/branch_predictor_chmem.h`（新）** —— **BTB（256×64b）+ Bimodal 2-bit PHT（256×2b）**
2. **BTB 布局** —— ★ **62b 打包**：`{tag30 = PC[31:2], target30 = target[31:2], valid(1b), is_unconditional(1b)}`（余 2 bit）⇒ 依据：**CH_MEM 只解 32 位指令**（`decode_chmem.h:82` 的 `instr_format` 无 16 位格式）⇒ `PC[1:0]=0`、target 4 字节对齐 ⇒ 低位恒 0 ⇒ 可安全裁掉。★ **`is_unconditional` 位是必需的**（见下「oracle 复审发现 2」）
3. **逻辑条目数 M** —— 物理 256 固定（编译期）+ **运行期 mask** `& literal(M-1)`，M ∈ {16,32,64,128,256}
4. **fetch NORMAL 预测 + branch LATE 接入** —— ★ **五级 mux 嵌套**（`trap > branch-actual/mispredict > freeze > bp-predict > seq`）
5. **flush 语义改写** —— `br_taken_buf` 分量替换为 `mispredict_buf`，**`hold_z` 保留**
6. **BP 开关面** —— 工厂参数（普通 `bool enable_bp = true` + `optional<uint32_t> btb_entries`）+ CLI `--no-bp` / `--btb-entries N`（★ **不是** `CPUConfig` 字段 —— CH_MEM 工厂不接它）
7. **台账 14 条重判 + ADR-088 §5.3.1 根因修订**（★ 修订**纳入本 change 的归档范围**）

### Out of Scope（6 项）

- ❌ **GShare 全局历史**（13-bit GHR）—— 延后（相对 Bimodal 的增益待前置2 数据）
- ❌ **RAS call/ret 预测** —— 延后（与 BTB 正交，可独立立项）
- ❌ **`jalr` 建 BTB 项** —— ★ 明确**不建**（间接跳转目标随运行时变化 ⇒ 静态缓存必错）
- ❌ **多级 TLB / PTW** —— 与本 change 无关
- ❌ **改 CH_MEM 的取指延迟**（imem 改 `aread`）—— 刻意保持 `pc_lag` 对齐的既有决策
- ❌ **改判据**（DMIPS / CoreMark 阈值）—— ★ 需显式 ADR，**不属本 change**

## Capabilities

- **CH_MEM 侧有 BP 结构且已注册**（ADR-088 §3 BP 行的门禁载体）
- **BP 收益可机械判定**（微基准四档 + 净收益模型）
- **绝对值门禁的恢复路径明确**（净命中率门槛 84.5% 已量化）

## Impact

- **代码**：`ip/cpu/plugins/branch_predictor_chmem.h`（新）· `ip/cpu/plugins/ibus_chmem.h`（PC-mux + flush）· `ip/cpu/cpu_factory_chmem.h`（注册 + 开关）
- **工具**：`tools/verilator_runner/` · `tools/measure_branchbench.sh`
- **数据**：`branchbench-baseline-matrix.csv` 扩充（`bp_mode` / `workload_sha` / `build_id` 列）
- **台账**：14 条重判 + §2 族级 + §7 裁决行 + 计数同步
- **ADR**：`ADR-088 §5.3.1` 根因修订（★ **必须** —— 现有表述已被 E-1/E-2 证伪）
- ★ **B3**：本 change **不解除** B3 的 BP 阻塞（双前置 ②「CoreMark harness 产出实测值」属**交付点 B**）

## Acceptance

见 `tasks.md` 的 AC-1 … AC-15（**逐条对应架构简报 §4.3**）。

## References

- 架构方案简报（权威）：`docs/architecture/bp-btb-gshare-design-brief.md` v2.1
- 执行文件：`.rddf/plans/chmem-bp-btb-gshare.md`（★ 顶部有 **v2.1 覆盖声明**，逐条作废旧 Goal/Task/AC）
- 前置：`chmem-branch-microbench-fix` · `chmem-bp-btb-gshare-poc`
- 机制结论：`tests/cpu/test_cpu_chmem_bp_poc.cpp`

## 规格来源（spec preamble 的归档副本）

> Capability topic: `chmem-bp-btb-gshare`（CH_MEM 后端的分支预测器）。
> 上游 = ADR-088 v1.2 §3 BP 行 + 架构简报 `docs/architecture/bp-btb-gshare-design-brief.md` v2.1。

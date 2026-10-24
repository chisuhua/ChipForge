---
initiative: wave6-linux-and-productization
priority: P1
version_target: v1.0.0
status: ready
created_from: docs/architecture/bp-btb-gshare-design-brief.md
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

⇒ ★ 「GShare 延后」**不是低风险决定** —— 它把 1.4 门禁押在「Bimodal（无全局历史）能否达到 84.5%」这个**未验证**的假设上 ⇒ ★ **若前置2 报告的可达命中率下界 < 84.5% ⇒ 不得以「GShare 延后」结案**。

## What Changes

### In Scope（7 项）

1. **`ip/cpu/plugins/branch_predictor_chmem.h`（新）** —— **BTB（256×64b）+ Bimodal 2-bit PHT（256×2b）**
2. **BTB 布局** —— **全 32b PC 作 tag + 32b target，valid 偷 target 的 `[1:0]`**（RV32 无 C 扩展 ⇒ target 4 字节对齐 ⇒ `[1:0]` 恒 0 ⇒ 恰好 64b ⇒ 天然消除「tag=0 假命中」）
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

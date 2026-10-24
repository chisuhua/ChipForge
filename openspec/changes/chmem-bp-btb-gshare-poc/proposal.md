---
initiative: wave6-linux-and-productization
priority: P1
version_target: v0.11.0
status: ready
created_from: docs/architecture/bp-btb-gshare-design-brief.md
adr:
  - ADR-088
depends_on:
  - chmem-branch-microbench-fix
---

# chmem-bp-btb-gshare-poc

> ⚠️ **本 change 是 B1 ② `chmem-bp-btb-gshare` 的硬前置**：验证「**fetch 期提前一拍预测 ⇒ 消除 taken 转移的气泡**」这个**机制**是否成立。
> ★ **若机制不成立，② 主体不得开工**（否则会造出「测试能绿、性能零变化」的惰性 BP —— 正是 ADR-040 §10.5 反复防的那类假绿）。

## Why

### 根因（architect 判决实验，2026-10-24）

| 实验 | 结果 |
|------|------|
| **E-2**（摘掉 TLM 的 BP 注册） | cycles **794478 → 794478（逐位不变）** ⇒ ★ **TLM 的 BP 是惰性桩**（`branch_predictor.h:126-129` 预测结果被丢弃、`:149-151` flush 恒 false）⇒ **ADR-088 §5.3.1 的根因表述「TLM 有 BTB ⇒ 预测命中不 flush」被证伪** |
| **E-3**（逐拍采样 CH_MEM 的 PC） | taken 控制转移 **120065 次**；`914544 − 120065 = 794479 ≈ TLM 794478`（残差 1）⇒ ★ **CH_MEM 与 TLM 的全部差距被「计数到的重定向」完全吸收** ⇒ 气泡 = **1 拍 / 每次 taken 转移** |
| **E-4**（读 TLM `ibus.h:114-124`） | TLM 是**单遍 tick**（writeback LATE 同 tick 内重定向）⇒ **重定向零延迟** |

⇒ ★ **真根因 = TLM 单遍模型重定向零代价 vs CH_MEM 两级取指每条 taken 转移 1 拍气泡**。
⇒ ★ **要补回这 120065 拍，BP 必须提前一拍预测** ⇒ 机制**可能**成立，但**未验证**。

### 为什么必须先做 PoC

| 风险 | 说明 |
|------|------|
| **机制可能不成立** | CH_MEM 的取指是 1 拍 `sread` 延迟（`ibus_chmem.h:163`）⇒ 若预测点选错 ⇒ **Δcycles ≈ 0** ⇒ 一个「能跑、有 case、但性能零变化」的 BP |
| **可能引入回归** | flush 语义要从「taken」改为「mispredict」⇒ 波及 3 个消费者（`branch_chmem.h:233` / `dmem_chmem.h:129` / `reg_file_chmem.h:222`）⇒ 改错 ⇒ 静默功能错误 |
| **可能重复取指** | 若正确预测后**未抑制该分支自身的 redirect** ⇒ **target 指令被取两次** ⇒ Δcycles ≈ 0 甚至更差 ⇒ **误判「机制不成立」** |
| ★ **绝对值门槛** | DMIPS ≥ 1.4 需净省 ≥ **101461** cycles ⇒ **净命中率门槛 ≥ 84.5%**（= 101461 / 120065）⇒ ★ 若 PoC 报告的**可达命中率下界 < 84.5%** ⇒ **不得以「GShare 延后」结案** |

## What Changes

### In Scope（5 项）

1. **最小 BTB**（`branch_predictor_chmem.h` 的**最小版**：BTB + Bimodal PHT，只为验证机制）
2. **fetch NORMAL 的预测** —— BTB `aread`（0-cycle）+ payload 输出
3. **branch LATE 的 PC-mux 接入** —— ★ **五级嵌套**（`trap > branch-actual/mispredict > freeze > bp-predict > seq`，见架构简报 §3.4.2）
4. **flush 语义改写** —— `br_taken_buf` 分量替换为 `mispredict_buf`，**`hold_z` 分量保留**
5. **测量设施** —— 「已执行指令数」+「FLUSH 事件数」+ **host 侧 PC 序列**（查重复 PC）+ TLM 侧 cycle 数同时输出

### Out of Scope（5 项）

- ❌ **不实现完整 BP**（容量/别名/替换策略/陈旧目标兜底等归 ② 主体）
- ❌ **不实现 GShare / RAS**
- ❌ **不产出 P-6 结论**（P-6 结论归 architect）
- ❌ **不改台账**（台账重判归 ② 主体）
- ❌ **PoC 代码不复用**为 ② 主体（避免「半成品进主体」）

## Capabilities

- **机制已验证**：「fetch 期提前一拍预测 ⇒ 消除气泡」在 CH_MEM 上**确实成立**
- **收益可量化**：单转移净收益（拍）有实测值
- **命中率下界已知**：与 84.5% 门槛对照 ⇒ 决定 GShare 是否必须回 ②

## Impact

- **代码**：`ip/cpu/plugins/branch_predictor_chmem.h`（最小版）· `ip/cpu/plugins/ibus_chmem.h`（PC-mux + flush）· `ip/cpu/cpu_factory_chmem.h`（注册 + 开关）
- **工具**：`tools/verilator_runner/`（`--stats`）· `tools/measure_branchbench.sh`（`--bp=on|off`）
- **下游**：② 主体依赖本 change 的机制结论 + 命中率数据

## Acceptance

见 `tasks.md` 的 AC-F2-1 … AC-F2-11（**逐条对应架构简报 §4.2**）。

## References

- 架构方案简报（权威）：`docs/architecture/bp-btb-gshare-design-brief.md` §2.1 / §2.5 / §3.4.2 / §4.2
- 前置：`chmem-branch-microbench-fix`
- 机制结论：`tests/cpu/test_cpu_chmem_bp_poc.cpp`（8 case / 51 assertions 全绿）

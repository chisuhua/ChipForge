---
change: chmem-bp-btb-gshare-poc
priority: P1
initiative: wave6-linux-and-productization
adr:
  - ADR-088
plan: docs/architecture/bp-btb-gshare-design-brief.md
status: in-progress
depends_on:
  - chmem-branch-microbench-fix
---

# Tasks — chmem-bp-btb-gshare-poc

> **定位**: B1 ② `chmem-bp-btb-gshare` 的**硬前置**（独立 change）。验证「**fetch 期提前一拍预测 ⇒ 消除 taken 转移气泡**」机制是否成立。
> **权威规格**: `docs/architecture/bp-btb-gshare-design-brief.md` §2.1 / §2.5 / §3.4.2 / §4.2
> ★ **本 PoC 的代码不复用为 ② 主体**（避免「半成品进主体」）

## Implementation

- [ ] **Task 1** 最小 BTB（BTB + Bimodal 2-bit PHT）+ fetch NORMAL 的 **0 拍**查表 + payload 输出（`pred_target` / `pred_taken` / `pred_valid`）
- [ ] **Task 2** branch LATE 的 PC-mux 接入（★ **五级嵌套**：`trap > branch-actual/mispredict > freeze > bp-predict > seq`）
- [ ] **Task 3** flush 语义改写（`br_taken_buf` 分量替换为 `mispredict_buf`，**`hold_z` 保留`）+ 三个消费方行为正确
- [ ] **Task 4** 误预测纠错（目标 = `pc_lag + 4`）+ **正确预测时抑制该分支自身 redirect**
- [ ] **Task 5** 测量设施（★ **FLUSH 事件数计数器归本 change** —— FLUSH 是 DUT 内部 payload，**host 不可见** ⇒ 需改 `ip/cpu/plugins/ibus_chmem.h` ⇒ **本 change 是首个可改 `ip/` 的 change**）+ ★ **MUST 复用**前置1 的 `--stats` 框架（**MUST NOT** 另起一套）+ BP 开关面（工厂参数 + CLI flag）

## Acceptance

- [ ] **AC-F2-1** 被测对象与工作负载明确 ★ **必须含一个「恒 taken」定向档**（★ 否则 BTB-only 在 not-taken 密集的负载上净负 ⇒ 机制会被误判「不成立」）
- [ ] **AC-F2-2** ★ 测的量 = **FLUSH 事件数**（★ 对取指延迟语义变更鲁棒）+ **总 cycles** + **host 侧 PC 轨迹**（查重复 PC）+ **BP 内部计数**（N_correct / N_false）+ **TLM 侧 cycle 数** —— **不再用「taken 转移数」作判据**
- [ ] **AC-F2-3** ★ **决策表两条分支各自的后果（终局式，★ v2.2 补阈值 —— 否则「成立/不成立」不可判定 ⇒ 兜底纪律被自己违反）**：
  - ★ **量化判据**：`ΔFLUSH = FLUSH(off) − FLUSH(on)` MUST **≥ 0.8 × FLUSH(off)**（★ 阈值由本 change 预登记，**MUST NOT** 写「显著下降」这种无量词）**且** `Δcycles ≥ 0.9 × ΔFLUSH`（★ 两项**同时**满足 ⇒ 成立；否则 ⇒ 不成立）
  - **成立** ⇒ 进 ② 主体
  - **不成立** ⇒ ① 记录 ⇒ ② 在 **ADR-088 §5.3.2** 落**结论**（绝对值门禁恢复 / **不恢复**）⇒ ③ **不恢复时沿用 §5.3.2 已登记的 P-3 临时口径**（cycle 比率 ≤ 1.15 + notes 标注）⇒ ④ **本 change 关闭**，**不留悬空项**
  - ★ **展开**（原单行内容）：
  - **成立**（FLUSH 事件数显著下降 + cycles 下降 ≈ 下降的 FLUSH 数）⇒ 进 ② 主体
  - **不成立** ⇒ ① 记录 ⇒ ② 在 **ADR-088 §5.3.2** 落**结论**（绝对值门禁恢复 / **不恢复**）⇒ ③ **不恢复时沿用 §5.3.2 已登记的 P-3 临时口径**（cycle 比率 ≤ 1.15 + notes 标注）⇒ ④ **本 change 关闭**，**不留悬空项**
- [ ] **AC-F2-4** ★ **兜底路径三要素**（触发阈值 + 落点文件 + ★ **可判定的时限**：「**≤ 2 个工作日**」）—— 继承 plan R6；★ **MUST NOT** 留 `N` 之类占位符（三要素的第 ③ 项实际为空 ⇒ 这条 AC 正是「等回复」的合法化文本）
- [ ] **AC-F2-5** ★ 显式声明「**本 PoC 不产出 P-6 结论**」（P-6 结论归 architect）
- [ ] **AC-F2-6** PoC 的代码**不复用**为 ② 主体
- [ ] **AC-F2-7** ★ **报告「去重后的热点分支数」** ⇒ 若 > 256 ⇒ 容量裁决提前做
- [ ] **AC-F2-8** ★ **报告「可达净收益率下界」** —— ★ **MUST 在 Dhrystone 上测**（84.5% 是 Dhrystone 门槛 ⇒ branchbench 的定向档不可对照）；**净收益率** := `Δcycles(bp=off − bp=on) / N_taken_total`（★ **不是命中率**；★ 误预测代价 = **1 拍** ⇒ 净收益率 = `2×命中率 − 1` ⇒ 等价命中率门槛 = **92.25%**）；下界 := `1 − (cold + mispredict + jalr_占比) / N_taken`（全部取自 AC-15 计数器 + AC-F2-11 的分项统计）⇒ ★ 若下界 **< 84.5%** ⇒ **不得以「GShare 延后」结案** ⇒ **MUST NOT** 用 raw hit rate 对照 84.5%
- [ ] **AC-F2-9** ★ **测量设施产品化**：「临时改 `sim_main.cpp`」⇒ 变成**常驻输出**（`--stats`）⇒ 否则每个 change 重做一次性 hack，测量结论不可复核
- [ ] **AC-F2-10** ★ **TLM 侧 cycle 数同时输出** ⇒ 让「差距全吸收」交叉验证成为**可重复判据**
- [ ] **AC-F2-11** ★ **测量方法护栏**：`enable_mmu=false`（**bare**）· **排除 trap 周期** · **无条件跳转（jal/jalr/ret）与条件分支分开统计**

- [ ] **AC-F2-12** ★ **预测信号必须是活节点** —— `pred_valid` / `pred_taken` 的 payload cell `impl()->is_const() == false` ⇒ 否则被折成常量 ⇒ Δcycles ≈ 0 而 FLUSH 不变 ⇒ **假阴性**（机制被判「不成立」⇒ 误关 ②）
- [ ] **AC-F2-13** ★ **计数器自证** —— BP 关态下 `FLUSH 事件数 == PC 跳变法测得的 taken 转移数` ⇒ 防「计数器恒 0 ⇒ 『显著下降』真空成立」的**假阳性**
- [ ] **AC-F2-14** ★ **BTB 命中率自证** —— BP 开态下 `BTB 命中次数 > 0` ⇒ 防「索引退化为 0 ⇒ 恒 miss ⇒ Δ=0 且 FLUSH 不变」的**假阴性**
- [ ] **AC-F2-15** ★ **无条件跳转占比报告** —— 报告 `jal` 数 / `jalr` 数 / taken 转移数 三者的比例 ⇒ 供 architect 判定「84.5% 门槛在构造上是否可达」（★ `jalr` 不建 BTB 项 ⇒ 其气泡**不可恢复**）

## ★ 兜底句纪律

任何「上报 architect / 待裁决」句 MUST 同句给出 **① 触发阈值 ② 落点文件 ③ 可判定时限**；MUST NOT 以「等待回复」结束本 change。

## 实测留证

（实施后填写）


## ★ builder 开工须知（★ **逐条抄进本 change**，不跨 change 引用）

| # | 须知 | 归属 |
|---|------|------|
| 1 | ★ 修正 `ibus_chmem.h:83-87` 的**过时注释**（「aread/sread 都是 1-cycle」与 aread 0-cycle 的**承重前提**直接矛盾） | ★ 本 change（首个可改 `ip/` 的 change） |
| 2 | ★ **MUST 复用**前置1 的 `--stats` 框架（**MUST NOT** 另起一套计数器） | ★ 本 change |
| 3 | ★ 本 change **MUST NOT** 触碰台账 `docs/architecture/tlm-coverage-parity-ledger.md`（台账重判归 ②） | ★ 本 change |
| 4 | ★ 本 change **MUST NOT** 触碰 `tools/measure_branchbench.sh` 的**模式名数组与表头解析逻辑**（前置1 的范围）⇒ 本 change **仅追加** BP flag（`--bp=on\|off`）与生效值断言相关行 | ★ 本 change |
| 5 | ★ `measure_branchbench.sh` 的 `2>/dev/null` MUST 改为**捕获 stderr** ⇒ 否则「flag 未生效却测出无差异」的**假绿通道**是开着的 | ★ 本 change |
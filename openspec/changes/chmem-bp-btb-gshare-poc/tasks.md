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
- [ ] **Task 5** 测量设施（已执行指令数 / FLUSH 事件数 / host 侧 PC 序列 / TLM 侧 cycle 数同时输出）+ BP 开关面（工厂参数 + CLI flag）

## Acceptance

- [ ] **AC-F2-1** 被测对象与工作负载明确 ★ **必须含一个「恒 taken」定向档**（★ 否则 BTB-only 在 not-taken 密集的负载上净负 ⇒ 机制会被误判「不成立」）
- [ ] **AC-F2-2** ★ 测的量 = **FLUSH 事件数**（★ 对取指延迟语义变更鲁棒）+ **总 cycles** + **host 侧 PC 序列**（查重复 PC）—— **不再用「taken 转移数」作判据**
- [ ] **AC-F2-3** ★ **决策表两条分支各自的后果（终局式）**：
  - **成立**（FLUSH 事件数显著下降 + cycles 下降 ≈ 下降的 FLUSH 数）⇒ 进 ② 主体
  - **不成立** ⇒ ① 记录 ⇒ ② 在 **ADR-088 §5.3.2** 落**结论**（绝对值门禁恢复 / **不恢复**）⇒ ③ **不恢复时沿用 §5.3.2 已登记的 P-3 临时口径**（cycle 比率 ≤ 1.15 + notes 标注）⇒ ④ **本 change 关闭**，**不留悬空项**
- [ ] **AC-F2-4** ★ **兜底路径三要素**（触发阈值 + 落点文件 + ★ **可判定的时限**：「≤ N 个工作日」或「本 change 内闭环」）—— 继承 plan R6
- [ ] **AC-F2-5** ★ 显式声明「**本 PoC 不产出 P-6 结论**」（P-6 结论归 architect）
- [ ] **AC-F2-6** PoC 的代码**不复用**为 ② 主体
- [ ] **AC-F2-7** ★ **报告「去重后的热点分支数」** ⇒ 若 > 256 ⇒ 容量裁决提前做
- [ ] **AC-F2-8** ★ **报告「可达净命中率下界」** ⇒ 与 **84.5% 门槛**对照 ⇒ ★ **若下界 < 84.5% ⇒ 不得以「GShare 延后」结案**
- [ ] **AC-F2-9** ★ **测量设施产品化**：「临时改 `sim_main.cpp`」⇒ 变成**常驻输出**（`--stats`）⇒ 否则每个 change 重做一次性 hack，测量结论不可复核
- [ ] **AC-F2-10** ★ **TLM 侧 cycle 数同时输出** ⇒ 让「差距全吸收」交叉验证成为**可重复判据**
- [ ] **AC-F2-11** ★ **测量方法护栏**：`enable_mmu=false`（**bare**）· **排除 trap 周期** · **无条件跳转（jal/jalr/ret）与条件分支分开统计**

## ★ 兜底句纪律

任何「上报 architect / 待裁决」句 MUST 同句给出 **① 触发阈值 ② 落点文件 ③ 可判定时限**；MUST NOT 以「等待回复」结束本 change。

## 实测留证

（实施后填写）

---
id: feat-bp-btb-gshare
kind: feature
status: active
scope: cpu
phase_refs: [phase-4, phase-5]
主题: 分支预测 BTB + GShare + RAS (PoC-6 CoreMark ≥1.9) — defer → v1.0.0 (2026-10-19 决策)
---

## 概述
实现动态分支预测器（BTB 4K-entry + GShare + RAS 8-entry），使 v1.0.0 CoreMark/MHz ≥2.3（v0.10.0 CoreMark/MHz ≥1.9 软门禁）。BTB 提供分支目标地址缓存，GShare 用全局历史 XOR PC 索引 PHT 查表，RAS 维护 8-entry call/ret 栈。

> ★★ **参考实现状态更正（planner 2026-10-26，静态核对 `ip/cpu/plugins/branch_predictor.h`）**：
> ① **GShare = 移植**（TLM 参考**已有**：`gshare_predict` / `gshare_update` / `global_history_` / `GHR_BITS=8` / `GSHARE_SZ` 表）⇒ ★ **不是新设计** ⇒ 成本远低于「13-bit 全局历史从零设计」的先前假设（★ 历史文档写的「13-bit」与代码的 `GHR_BITS=8` **不一致** ⇒ 以代码为准）；
> ② **RAS = 新增设计**（TLM 参考**无** RAS / 无 `is_unconditional` 位 / BTB entry 仅 `{tag, target, valid}`）⇒ ⇒ **BTB 62b 编码里的 `is_unconditional` 位是新增字段**，而 `jalr`/`ret` 的模式预测**必须**有 RAS 才能安全做 ⇒ 两者是**捆绑**的；
> ③ **BTB 4K-entry 是 aspirational**（TLM 参考的 `btb_size` 由构造参数决定，CH_MEM PoC 用 256）⇒ 本 feature 的「4K」不是既有事实。

> ⚠️ **后端绑定变更（2026-10-23，`objective-ip-cpu-tlm-backend-retirement` B2 裁决输入）**
>
> BP 当前唯一实现在 TLM 侧（`ip/cpu/plugins/branch_predictor.h` 319 行），**CH_MEM binary 中 `BranchPredictorPlugin` 符号数 = 0** —— 即 PoC-6 的被测对象在 CH_MEM 后端**根本不存在**。
>
> 为避免双份实现（双后端已产生 M 扩展 / CSR / MMU PTW 三处同 ISA 不同语义），BP 的实施目标**改绑 CH_MEM-only**：
> - 本 feature 的 phase-5 段实装**只写 CH_MEM 版**（`branch_predictor_chmem.h`），TLM 版随 B3 一并退役
> - 归属 `feat-tlm-backend-retirement` 的 B1 第 5 项（承载力补齐）
> - **裁决权归 ADR-088 §语义裁决表**（本条目为该裁决的输入，不是最终裁决）

## 跨阶段拆分

### phase-4
- pc_reg mux 接入 BP 预测目标
- bp-btb-coordination-3 items: pc_reg mux / stall / fetch stage 协议（0.5d, mfc Phase C 入口）
- BTB 4K-entry 实装 + GShare 13-bit PHT 初始化
- bp_mode 锁 static（防止 R1 触发降级）
- **后端归属确认**: TLM 侧 BP 仅作**参考实现**维护（feature 函数冻结，见 `feat-tlm-backend-retirement` B0 冻结门禁）；新功能一律只写 CH_MEM 版

### phase-5 (v1.0.0)
- BTB 4K-entry + GShare + RAS 8-entry 完整实装 —— **CH_MEM-only**（`branch_predictor_chmem.h`）
- BTB miss 走 static predict fallback
- GShare misprediction recovery（CtrlLink::flush_when 真实消费）
- CoreMark harness 在 `cpu_verilator_sim` 路径可测（`feat-tlm-backend-retirement` B1 第 1 项同批交付）

> ★★ **v1.0.0 的内容须按「增量分档」重排（planner 2026-10-26，**A5 决策**）**
>
> ★ 前置2 PoC（2026-10-26 完成）已实测：CH_MEM 侧 **Bimodal-only** BP 的净收益率 = **56.7%**
> （Dhrystone Δcycles 68044 / FLUSH(off) 120065）⇒ 距绝对值门禁所需的 **84.51%** 仍差 **27.9 个百分点**
> ⇒ ★ 缺口**不再**是「CH_MEM 没有 BP」，而是「**BP 的覆盖面与方向精度不够**」⇒ ⇒ 三个增量按**实测收益**排序落地：
>
> | 增量 | 内容 | 参考实现 | 成本 | 风险 |
> |---|---|---|---|---|
> | **增量 1** | BTB `is_unconditional` 位 + `jal`/`j` 建项（恒 taken ⇒ 不占 PHT） | ★ **无** ⇒ 新增设计 | 低 | 低（目标静态 ⇒ 无陈旧风险） |
> | **增量 2** | RAS 8-entry ⇒ `jalr`/`ret` 模式预测 | ★ **无** ⇒ 新增设计 | **高** | 中（需 return/branch 区分） |
> | **增量 3** | GShare（GHR XOR 索引 PHT） | ★ **有** ⇒ **移植** | **低** | 低（TLM 已有可对照实现） |
>
> ⇒ ★ **决策规则**：净收益率 ≥ 84.5% ⇒ 绝对值门禁可恢复 ⇒ 后续增量可延后；否则 MUST 拉回下一档
>
> ⇒ ★★ **建议执行序 = 增量 1 → 增量 3（GShare，移植）→ 增量 2（RAS，新增设计）** —— 按**实现成本**而非增量编号
> ⇒ ★ **收益上界（planner 2026-10-26 静态分析 Dhrystone `.text`）**：残余 flush **26.0/iter** 中绝大部分是**无条件跳转**
> （BTB 写使能 = `taken & is_branch` ⇒ `jal`/`ret` 从不建项 ⇒ 从不预测）；分支站点共 55 个 = 无条件 36（`ret` 16 / `jal` 15 / `j` 5）+ 条件 19
> ⇒ 增量 1 覆盖 `jal`/`j`（20/36）⇒ 上界 ≈ **14/iter ≈ 28,000 cycles** ⇒ ★ 与「达 DMIPS 1.4 还需 33,426 拍」**同量级**
> ⇒ ⇒ **增量 1 单独大概率不足以达 1.4**（预计净收益率 56.7% → ~80%）⇒ ⇒ ⇒ **GShare（移植）大概率必须进本期**
>
> ★★★ **增量 1 已实施并实测（2026-10-26，② 主体）** —— ★ **实测与上述估算高度吻合**：
>
> | 配置 | DHrystone cycles | FLUSH_COUNT | 净收益率 | branchbench 4 档 cycles |
> |---|---|---|---|---|
> | BP=off | 914544 | 120065 | — | 86034 / 88034 / 86534 / 87056 |
> | BP=on，增量 1 关（= PoC 行为） | 846500 | 52021 | 56.67% | 84038 / 84040 / 84787 / 85037 |
> | **BP=on，增量 1 开** | **820513** | **26034** | **78.3%** | **78041 / 78043 / 78791 / 79041** |
>
> ⇒ ★ **ΔFLUSH ≡ Δcycles ≡ 25987** ⇒ **78.3%** 与静态上界估算的「~80%」**吻合** ⇒ 估算方法有效
> ⇒ ★★ **仍未达 84.5%** ⇒ ⇒ **GShare（增量 3，移植）MUST 进本期**（AC-16 的强制分支）⇒ 还差 **6.2pp ≈ 7440 cycles**
> ⇒ ★★ **实测更正**：「branchbench 上 `jal` 覆盖的增量收益**结构性为零**」是**错的** —— ★ 静态分析只数了 `jal`，**漏了 `j`**
> （`j` = `jal x0` ⇒ 同 opcode ⇒ 也在覆盖范围内）⇒ ★ **4 档全部改善 ~6000 拍** ⇒ ★★ **branchbench 与 DHrystone 双证**
> ⇒ ★ **判据落点**：`openspec/changes/chmem-bp-btb-gshare` 的 **Task 7 / AC-27**（v2.3 覆盖声明）。

## 验收标准

> ★**口径更正（planner 2026-10-24，依据 ADR-088 v1.2 §5.3.2）**：**CoreMark 绝对分数不是本 feature 的验收判据** —— ① **P-5**：CoreMark 移植（交付点 B `chmem-verilator-coremark-harness`）**允许顺延**，中间验收改用**分支 microbenchmark**（`chmem-branch-microbench`，已归档）⇒ 本 feature **不被 CoreMark 移植阻塞**；② **P-6**：CH_MEM 侧的**绝对值**门禁恢复条件 = **本 feature 自身落地**（差距根因 = CH_MEM 无分支预测 ⇒ 与本 feature 同源）⇒ 绝对值是**本 feature 的副产品**，不是它的前置；③ ADR-088 v1.2 §5.3.1 实测：比率随**分支密度**单调升（add 1.037 / jal 1.078 / beq 1.098 / Dhrystone 1.151）⇒ 判据必须是**机制化**的（方向变化频繁的档收益大），**不是**绝对分数。

- [ ] BTB 4K-entry 实装 + 命中率指标在 **microbenchmark 四档**上可量化（★**替代**原「CoreMark benchmark 命中率 ≥95%」—— CoreMark 属交付点 B）
- [ ] **三档增量分档实测**（★ v2.3 新增，A5 决策）—— 增量 1 / 2 / 3 **可独立开关 + 独立测量**，每档 MUST 落表：Dhrystone 的 `FLUSH_COUNT` / `CYCLES` / `ΔFLUSH` / `Δcycles` / **净收益率** + branchbench 4 档校验和**逐位相等**（零退化）⇒ 判据 = 净收益率 ≥ **84.5%**
- [ ] GShare PHT 实装（★ **移植** TLM 参考的 `gshare_predict`/`gshare_update`/`global_history_`，`GHR_BITS=8`）+ **microbench 中间验收**：方向**变化**频繁的档（`pattern_6of8` / `pseudo_random`）Δcycles **显著大于**方向**恒定**的档（`always_taken` / `always_not_taken`）（★**替代**原「CoreMark/MHz ≥1.9」—— 见上方口径更正 P-5/P-6）
- [ ] RAS 8-entry call/ret 正确率 100% (单元测试) —— ★ **无 TLM 参考实现** ⇒ 新增设计 ⇒ ★ 依赖增量 1 的 `is_unconditional` 位（BTB entry 需同时表达「无条件」与「由 RAS 预测」）
- [ ] 提供**可关闭 BP 的配置面**（否则「启用/禁用 BP 对比」无法做 ⇒ microbench 中间验收不可执行）
- [ ] **BP 落地后重录** `branchbench-baseline-matrix.csv`（现值为**无 BP 参照值**）
- [ ] v1.0.0 CoreMark ≥2.3 (v0.10.0 软门禁过渡) —— ★**归交付点 B** `chmem-verilator-coremark-harness`（本 feature **不宣告**绝对值达标）
- [ ] **HARD 门禁 PoC-14**: FPGA CoreMark/MHz 与 VexiiRiscv single-issue（官方 2.4–2.6）偏差 ≤10% —— 归交付点 B / v1.3.0

## 风险（R1 砍分叉）
- **R1 触发信号**: ★ **重校准（planner 2026-10-26）** —— 原判据「microbench 增益 < 10%」已被实测**证伪其不适用性**：branchbench 4 档的 Δcycles 改善 = **1996~3994 拍（2.3%~4.5%）** ⇒ 百分比门槛对「**消除 taken 气泡**」这个**绝对量级很小**的收益天然不敏感 ⇒ ★ 改用**绝对量 + 净收益率**判据：**净收益率 < 84.5%** ⇒ 绝对值门禁不可恢复 ⇒ MUST 拉回下一档增量（见上方三档表）
- **R1 砍分叉动作**: 停 GShare，启动 Learn 通路 RCA（双 sprint），期间 bp_mode 锁 static
- **★R1 口径补充**：判据须**机制化** —— 方向**变化**频繁的档（m2/m3）Δcycles 显著大于方向**恒定**的档（m0/m1）才算 BP 生效（0c 实测：恒定档本就无「方向变化惩罚」⇒ 收益空间小；若四档降幅均 ≈0 ⇒ BP 无效）
- **R-BACKEND（新增，2026-10-23）**: CH_MEM 侧 BP 承载力未补齐 → PoC-6 无被测对象。缓解：归属 `feat-tlm-backend-retirement` B1 第 5 项；若 B1 未完成而 B3 启动，BP 在 CH_MEM 后端将**完全缺失** ⇒ B3 删除门禁须额外核对 BP CH_MEM 版存在

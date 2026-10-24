---
id: feat-bp-btb-gshare
kind: feature
status: active
scope: cpu
phase_refs: [phase-4, phase-5]
主题: 分支预测 BTB + GShare + RAS (PoC-6 CoreMark ≥1.9) — defer → v1.0.0 (2026-10-19 决策)
---

## 概述
实现动态分支预测器（BTB 4K-entry + GShare 13-bit + RAS 8-entry），使 v1.0.0 CoreMark/MHz ≥2.3（v0.10.0 CoreMark/MHz ≥1.9 软门禁）。BTB 提供分支目标地址缓存，GShare 用 13-bit 全局历史 XOR PC 索引 PHT 查表，RAS 维护 8-entry call/ret 栈。

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
- BTB 4K-entry + GShare 13-bit + RAS 8-entry 完整实装 —— **CH_MEM-only**（`branch_predictor_chmem.h`）
- BTB miss 走 static predict fallback
- GShare misprediction recovery（CtrlLink::flush_when 真实消费）
- CoreMark harness 在 `cpu_verilator_sim` 路径可测（`feat-tlm-backend-retirement` B1 第 1 项同批交付）

## 验收标准

> ★**口径更正（planner 2026-10-24，依据 ADR-088 v1.2 §5.3.2）**：**CoreMark 绝对分数不是本 feature 的验收判据** —— ① **P-5**：CoreMark 移植（交付点 B `chmem-verilator-coremark-harness`）**允许顺延**，中间验收改用**分支 microbenchmark**（`chmem-branch-microbench`，已归档）⇒ 本 feature **不被 CoreMark 移植阻塞**；② **P-6**：CH_MEM 侧的**绝对值**门禁恢复条件 = **本 feature 自身落地**（差距根因 = CH_MEM 无分支预测 ⇒ 与本 feature 同源）⇒ 绝对值是**本 feature 的副产品**，不是它的前置；③ ADR-088 v1.2 §5.3.1 实测：比率随**分支密度**单调升（add 1.037 / jal 1.078 / beq 1.098 / Dhrystone 1.151）⇒ 判据必须是**机制化**的（方向变化频繁的档收益大），**不是**绝对分数。

- [ ] BTB 4K-entry 实装 + 命中率指标在 **microbenchmark 四档**上可量化（★**替代**原「CoreMark benchmark 命中率 ≥95%」—— CoreMark 属交付点 B）
- [ ] GShare 13-bit PHT 实装 + **microbench 中间验收**：方向**变化**频繁的档（`pattern_6of8` / `pseudo_random`）Δcycles **显著大于**方向**恒定**的档（`always_taken` / `always_not_taken`）（★**替代**原「CoreMark/MHz ≥1.9」—— 见上方口径更正 P-5/P-6）
- [ ] RAS 8-entry call/ret 正确率 100% (单元测试)
- [ ] 提供**可关闭 BP 的配置面**（否则「启用/禁用 BP 对比」无法做 ⇒ microbench 中间验收不可执行）
- [ ] **BP 落地后重录** `branchbench-baseline-matrix.csv`（现值为**无 BP 参照值**）
- [ ] v1.0.0 CoreMark ≥2.3 (v0.10.0 软门禁过渡) —— ★**归交付点 B** `chmem-verilator-coremark-harness`（本 feature **不宣告**绝对值达标）
- [ ] **HARD 门禁 PoC-14**: FPGA CoreMark/MHz 与 VexiiRiscv single-issue（官方 2.4–2.6）偏差 ≤10% —— 归交付点 B / v1.3.0

## 风险（R1 砍分叉）
- **R1 触发信号**: BTB 在 **microbenchmark** 上的增益 < 10%（★原判据用 CoreMark 提升，已随 P-5 改为 microbench —— 因 CoreMark 移植属交付点 B 且允许顺延）
- **R1 砍分叉动作**: 停 GShare，启动 Learn 通路 RCA（双 sprint），期间 bp_mode 锁 static
- **★R1 口径补充**：判据须**机制化** —— 方向**变化**频繁的档（m2/m3）Δcycles 显著大于方向**恒定**的档（m0/m1）才算 BP 生效（0c 实测：恒定档本就无「方向变化惩罚」⇒ 收益空间小；若四档降幅均 ≈0 ⇒ BP 无效）
- **R-BACKEND（新增，2026-10-23）**: CH_MEM 侧 BP 承载力未补齐 → PoC-6 无被测对象。缓解：归属 `feat-tlm-backend-retirement` B1 第 5 项；若 B1 未完成而 B3 启动，BP 在 CH_MEM 后端将**完全缺失** ⇒ B3 删除门禁须额外核对 BP CH_MEM 版存在

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
- [ ] BTB 4K-entry 命中率 ≥95% (CoreMark benchmark)
- [ ] GShare 13-bit PHT 实装 + CoreMark/MHz ≥1.9 (PoC-6 软门禁)
- [ ] RAS 8-entry call/ret 正确率 100% (单元测试)
- [ ] v1.0.0 CoreMark ≥2.3 (v0.10.0 软门禁过渡)
- [ ] **HARD 门禁 PoC-14**: FPGA CoreMark/MHz 与 VexiiRiscv single-issue（官方 2.4–2.6）偏差 ≤10%

## 风险（R1 砍分叉）
- **R1 触发信号**: BTB CoreMark 提升 < 10%
- **R1 砍分叉动作**: 停 GShare，启动 Learn 通路 RCA（双 sprint），期间 bp_mode 锁 static
- **R-BACKEND（新增，2026-10-23）**: CH_MEM 侧 BP 承载力未补齐 → PoC-6 无被测对象。缓解：归属 `feat-tlm-backend-retirement` B1 第 5 项；若 B1 未完成而 B3 启动，BP 在 CH_MEM 后端将**完全缺失** ⇒ B3 删除门禁须额外核对 BP CH_MEM 版存在

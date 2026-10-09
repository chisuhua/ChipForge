---
id: feat-bp-btb-gshare
kind: feature
status: active
phase_refs: [phase-4, phase-5]
主题: 分支预测 BTB + GShare + RAS (PoC-6 CoreMark ≥1.9)
---

## 概述
实现动态分支预测器（BTB 4K-entry + GShare 13-bit + RAS 8-entry），使 v1.0.0 CoreMark/MHz ≥2.3（v0.10.0 CoreMark/MHz ≥1.9 软门禁）。BTB 提供分支目标地址缓存，GShare 用 13-bit 全局历史 XOR PC 索引 PHT 查表，RAS 维护 8-entry call/ret 栈。

## 跨阶段拆分

### phase-4
- pc_reg mux 接入 BP 预测目标
- bp-btb-coordination-3 items: pc_reg mux / stall / fetch stage 协议（0.5d, mfc Phase C 入口）
- BTB 4K-entry 实装 + GShare 13-bit PHT 初始化
- bp_mode 锁 static（防止 R1 触发降级）

### phase-5 (v1.0.0)
- BTB 4K-entry + GShare 13-bit + RAS 8-entry 完整实装
- BTB miss 走 static predict fallback
- GShare misprediction recovery（CtrlLink::flush_when 真实消费）

## 验收标准
- [ ] BTB 4K-entry 命中率 ≥95% (CoreMark benchmark)
- [ ] GShare 13-bit PHT 实装 + CoreMark/MHz ≥1.9 (PoC-6 软门禁)
- [ ] RAS 8-entry call/ret 正确率 100% (单元测试)
- [ ] v1.0.0 CoreMark ≥2.3 (v0.10.0 软门禁过渡)
- [ ] **HARD 门禁 PoC-14**: FPGA CoreMark/MHz 与 VexiiRiscv single-issue（官方 2.4–2.6）偏差 ≤10%

## 风险（R1 砍分叉）
- **R1 触发信号**: BTB CoreMark 提升 < 10%
- **R1 砍分叉动作**: 停 GShare，启动 Learn 通路 RCA（双 sprint），期间 bp_mode 锁 static

---
id: feat-mmu-chmem-integration
kind: feature
status: active
phase_refs: [phase-4]
主题: CH_MEM MMU/PTW pipeline 集成 (v0.11.0 owner)
---

## 概述
v0.11.0 launch owner：MMUPlugin CH_MEM 模式完整实装（ibus/dmem/mmu_exception_handler/hazard CH_MEM 化），使 [mmu-verilator] 从 3/3 plumbing-only 升级为 5/5 真 sv32 translation 验证。

## 跨阶段拆分

### phase-4 (v0.10.0-v0.11.0)
- mmu-chmem-pipeline-integration Phase A 已 commit (`a3afcfa`, 2026-10-08): Bare shortcut + shadow 双 bug 修复 → [mmu] 59/59 PASS
- Phase B-E 实装（依赖 bp-btb 协调 3 项协议达成）：
  - B1-B5: mmu_chmem.h 单级 ch_mem TLB + 复用 PtWalkFsmPlugin
  - C1-C6: ibus_chmem.h + dmem_chmem.h sv32 消费
  - D1-D6: mmu_exception_handler_chmem.h combinational network
  - E1-E6: hazard_chmem.h mark/clear mmufault 实装

## 验收标准
- [ ] mmu-chmem-pipeline-integration archive (6/166 → 166/166)
- [ ] [mmu] 59/59 维持 + [cpu-l1-mmu-demo] 7/7 PASS（workaround 移除）
- [ ] [mmu-verilator] 5/5 PASS（真 sv32 translation，非 plumbing only）
- [ ] [verilator] 1/1 + [cpu-integration] 81/81 + [cpu] 19/19 全部 0 退化
- [ ] Architecture Gates 4 个 0 error
- [ ] 间接：[cpu-l1-mmu-demo] 7/7（workaround `cfg.enable_mmu=false` 移除，真 sv32 e2e 翻转）

## 风险（CH_MEM 物理限制）
- CSR plugin 物理不可能（Oracle R11）→ mmufault_clear 走最小 combinational
- 多级 TLB / LRU-RRIP / ASID≠0 / Sv39/48 / 权限检查 / megapage 完整 PPN 拼接：全部 out-of-scope（D1 收窄）

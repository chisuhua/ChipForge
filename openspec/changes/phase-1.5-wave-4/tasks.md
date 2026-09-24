---
initiative: wave4-csr-cache-dse
priority: P2
version_target: v0.9.0
status: placeholder
---

# Tasks — phase-1.5-wave-4 (占位)

> ⚠️ 占位 tasks, 待 wave3 (`wave3-mmu-real-memory-and-cycle`) 全部 archive 后展开

## 0. 前置条件 (阻塞项)

- [ ] 等 wave3-mmu-real-memory-and-cycle archive (3 个 change: mmu-paddr-consume-and-real-memory + plugin-framework-cycle-precision + cpu-pipeline-multi-cycle)

## 1. Open Questions 回答 (wave3 archive 后立即)

- [ ] 1.1 Q1 决策: CSR 寄存器最小集是 `mstatus/mtvec/mepc/mcause` 还是含 `mtval/mie/mip`?
- [ ] 1.2 Q2 决策: trap delivery 是否同时支持**同步异常** (ecall) 与**异步中断** (timer interrupt from CLINT)?
- [ ] 1.3 Q3 决策: branch mispredict recovery 是否引入额外架构 ADR?
- [ ] 1.4 Q4 决策: Sv32 与现有 sv39 PTW walk 并存策略 (mode 选择 by `satp.MODE`)

## 2. Change 拆分决策 (wave3 archive 后)

- [ ] 2.1 把本 umbrella change 拆分为 4 个独立 openspec changes:
  - `cpu-csr-minimal` (CSR 寄存器实装)
  - `cpu-pipeline-exception` (exception trap delivery)
  - `cpu-pipeline-mispredict` (branch recovery)
  - `mmu-sv32-extension` (Sv32 PTW decode + megapage)
- [ ] 2.2 每个子 change 独立走 TDD 5 步
- [ ] 2.3 各自 archive 后, 本 umbrella change 才 archive

## 3. CSR 寄存器实装 (子 change 1)

- [ ] 3.1 新建 `ip/cpu/arch/riscv/csr.h/.cpp` (~250 LOC): mstatus/mtvec/mepc/mcause/mtval/sstatus
- [ ] 3.2 CSR 读写 at_stage 闭包 (csr_read / csr_write 子阶段)
- [ ] 3.3 新建 ADR `cpu-csr-minimal` (~180 LOC)
- [ ] 3.4 新建 `tests/cpu/integration/test_csr_read_write.cpp` (~150 LOC)
- [ ] 3.5 verify pass + commit + archive

## 4. exception path 实装 (子 change 2)

- [ ] 4.1 `ip/cpu/plugins/exception.h` 移除 `throw_when lambda 恒 false` stub (~150 LOC)
- [ ] 4.2 trap delivery: ecall / page fault / illegal instruction → mcause/mepc/mtval + PC 跳 mtvec
- [ ] 4.3 与 P1#3 MMU exception 12/13/15 路由集成 (mmu_exit substage)
- [ ] 4.4 新建 ADR `cpu-exception-delivery` (~200 LOC)
- [ ] 4.5 新建 `tests/cpu/integration/test_trap_delivery.cpp` (~120 LOC)
- [ ] 4.6 verify pass + commit + archive

## 5. branch mispredict recovery (子 change 3)

- [ ] 5.1 `ip/cpu/plugins/branch_predictor.h` 移除 `flush_when lambda 恒 false` stub (~120 LOC)
- [ ] 5.2 mispredict detection: predicted_pc vs actual_pc
- [ ] 5.3 pipeline flush: CtrlLink::flush_when(mispredicted) → 清空 5 stage
- [ ] 5.4 新建 ADR `cpu-branch-mispredict-recovery` (依赖 Q3 决策)
- [ ] 5.5 新建 `tests/cpu/integration/test_mispredict_recovery.cpp` (~100 LOC)
- [ ] 5.6 verify pass + commit + archive

## 6. Sv32 MMU extension (子 change 4)

- [ ] 6.1 `ip/mmu/lib/ptw.cpp` Sv32 PTE decode (RISC-V Privileged Spec §4.4.1)
- [ ] 6.2 megapage (4MB) 大页支持
- [ ] 6.3 mode 选择 by `satp.MODE` (sv32 vs sv39)
- [ ] 6.4 新建 `tests/mmu/test_sv32_ptw_decode.cpp` (~80 LOC)
- [ ] 6.5 verify pass + commit + archive

## 7. 跨子 change 集成验证

- [ ] 7.1 `[riscv-tests]` 扩展 rv32mi-p 子集 (CSR/trap) ≥5 用例 PASS
- [ ] 7.2 `[cpu-integration]` 既有 4 + 新增 ≥3 用例 PASS
- [ ] 7.3 `[mmu]` 50 + 新增 Sv32 测试 PASS
- [ ] 7.4 TLM baseline 0 回归

## 8. CI 门禁 + 文档

- [ ] 8.1 `bash tools/verify_adr.sh` PASS（含 3-4 新 ADR）
- [ ] 8.2 `bash tools/verify_plugin_decision.sh` PASS（D4 合规, exception/mispredict 不引入状态机）
- [ ] 8.3 `bash tools/check_plugin_portability.sh` PASS
- [ ] 8.4 `CHANGELOG.md` v0.6.0 段本 wave 4 完成条目
- [ ] 8.5 `soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md` §8 毕业标准全部 ✅

## 9. Phase 2 kickoff (后续)

- [ ] 9.1 wave4 全部 archive 后, 启动 `phase-2-baremetal-kickoff` openspec change
- [ ] 9.2 riscv-tests RV64GC 接入 + Spike co-simulation + RISCOF 框架

## Acceptance

- [ ] 0 阻塞项解除
- [ ] 1.1-1.4 Open Questions 全部回答
- [ ] 2.1-2.3 Change 拆分完成
- [ ] 3.1-3.5 CSR 子 change archive
- [ ] 4.1-4.6 exception 子 change archive
- [ ] 5.1-5.6 mispredict 子 change archive
- [ ] 6.1-6.5 Sv32 子 change archive
- [ ] 7.1-7.4 集成验证全绿
- [ ] 8.1-8.5 CI 门禁 + 文档同步
- [ ] 9.1-9.2 Phase 2 kickoff 启动

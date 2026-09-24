---
initiative: wave4-csr-cache-dse
priority: P2
version_target: v0.9.0
status: placeholder
depends_on:
  - mmu-paddr-consume-and-real-memory
  - plugin-framework-cycle-precision
  - cpu-pipeline-multi-cycle
---

# phase-1.5-wave-4 — CSR/exception/mispredict 实装 (占位)

> **状态**: 🟡 占位 change, 待 wave3 (`wave3-mmu-real-memory-and-cycle`) 全部 archive 后展开 design.md / tasks.md 细化
> **战略位置**: Phase 1.5 毕业前最后 wave, 进入 Phase 2 (Bare-metal RV64GC) 前置

## Why

Phase 1.5 stage doc §"Wave 4" 规划 (见 `soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md`):

> **Wave 4 (Wave 3 后, 2 周)**: Phase 1.5 → Phase 2 毕业
> - `mmu-sv32-ext` (Sv32 PTW decode + megapage, Sv48 砍掉)
> - `cpu-pipeline-exception` (throw_when 真实消费者: trap delivery + mcause/mepc/mtvec CSR + mstatus/sstatus)
> - `ip-cpu-csr-minimal` (mstatus/mtvec/mepc/mcause/mtval/sstatus CSR 写实装)
> - `cpu-pipeline-mispredict` (flush_when 真实消费者: branch recovery + flush pipeline)
> - `phase-2-baremetal-kickoff` (Phase 2 启动文档 + riscv-tests RV64GC 接入计划)

本 change 是 Wave 4 的 4 个子项合并 (CSR + exception + mispredict + sv32 extension), 因为它们**共享 CSR 基础设施 + flush 基础**。Phase 1.5 wave 间依赖解耦说明 (见 phase-1.5-stall-and-validate.md §3 Wave 间依赖图)：

- `exception` 与 `cycle-precision` 是**正交** —— `throw_when` 的语义正确性在单遍 run 下完全可实现, cycle-precision 只影响时序精确度
- `mispredict` 依赖 exception 的 CSR 基础设施 (非 cycle-precision)

## Pending Open Questions (须 wave3 archive 后回答)

- **Q1**: CSR 寄存器最小集是 `mstatus/mtvec/mepc/mcause` 还是含 `mtval/mie/mip`?
- **Q2**: trap delivery 是否同时支持**同步异常** (ecall) 与**异步中断** (timer interrupt from CLINT)?
- **Q3**: branch mispredict recovery 是否引入额外架构 ADR (类似 ADR-045 CtrlLink)?
- **Q4 (Oracle #4 已回答)**: ~~`mmu-sv32-ext` 与 P1#3 sv32 翻译对齐? (Phase 1.5 当前仅 Sv39)~~ — **Phase 6d 6d.6 已 ship sv32 5 状态 FSM (commit `09c9d23`, `ip/cpu/plugins/mmu_ptw_chmem.h`)**. 本 change 范围限定为 **TLM 侧对齐** (与 sv32 FSM 共存检查, 防止 TLM/stub 与 CH_MEM/FSM 分叉), 不重新实装 Sv32.

## What Changes (待 wave3 后展开)

> ⚠️ 以下为占位大纲, 实际 implementation 待 wave3 archive 后细化

### 1. CSR 寄存器实装 (依赖 Q1 回答)

- **新增** `ip/cpu/arch/riscv/csr.h/.cpp` (~250 LOC):
  - mstatus, mtvec, mepc, mcause, mtval, sstatus 寄存器
  - CSR 读写 at_stage 闭包 (csr_read / csr_write 子阶段)
  - 与现有 `cpu-pipeline-stubs-replace` (v0.1.2) 的 CSR stub 集成

### 2. exception path 实装 (依赖 Q2 回答)

- **修改** `ip/cpu/plugins/exception.h` (~150 LOC):
  - 移除 commit C 的 `throw_when lambda 恒 false` stub
  - 实装 trap delivery: 捕获 ecall / page fault / illegal instruction → 写 mcause/mepc/mtval + PC 跳 mtvec
  - 与 P1#3 MMU exception 12/13/15 路由集成 (mmu_exit substage)

### 3. branch mispredict recovery (依赖 Q3 回答)

- **修改** `ip/cpu/plugins/branch_predictor.h` (~120 LOC):
  - 移除 commit C 的 `flush_when lambda 恒 false` stub
  - 实装 mispredict detection: 比较 predicted_pc vs actual_pc
  - flush pipeline: CtrlLink::flush_when(mispredicted) → 清空 IF/ID/EX/MEM/WB stage

### 4. ~~Sv32 MMU extension~~ (Oracle #4: 已被 Phase 6d 6d.6 覆盖, **本 change 不再实装**)

- **状态**: 已于 Phase 6d v0.5.0 ship (commit `09c9d23`)
  - `ip/cpu/plugins/mmu_ptw_chmem.h` 314 行 (sv32 5 状态 FSM, IDLE/L0_WAIT/L1_WAIT/DONE/FAULT)
  - `tests/cpu/test_mmu_ptw_fsm_chmem.cpp` 3 PoC (24 assertions)
  - CHANGELOG v0.5.0 段 6d.6 条目
- **本 change 范围内**: 仅做 **TLM 侧对齐** (e.g., `ip/mmu/tlm/MMUPlugin.cpp` Sv32 PTE decode 与 Sv32 FSM 共存检查, 防止 TLM 路径走 stub 而 CH_MEM 走 FSM 的不一致)
- **判断依据**: Oracle 2026-09-24 审查 (双审查报告 §1.4) 显式标 "Phase 6d 6d.6 已 ship sv32 5 状态 FSM, wave4 不应重复"

### 5. `phase-2-baremetal-kickoff` 文档

- **新建** `openspec/changes/phase-2-baremetal-kickoff/proposal.md` (单独 change, 在 Wave 4 archive 后启动)

## Capabilities

### New Capabilities

- `cpu-csr-minimal`: 定义 CSR 寄存器读写 contract
- `cpu-exception-delivery`: 定义 trap delivery flow (含 CSR 写入)
- `cpu-branch-mispredict-recovery`: 定义 branch recovery + pipeline flush 机制
- `mmu-sv32-extension`: 定义 Sv32 PTW decode + megapage (vs sv39)

### Modified Capabilities

- `cpu-mmu-exception-routing`: 增强 "MMU exception 12/13/15 路由到 exception path" requirement (与 exception 实装集成)
- `ctrllink-consumption-contract`: 新增 "`flush_when` SHALL trigger pipeline flush when branch mispredicted" requirement

## Impact (估算, 待 wave3 后细化)

- **修改代码**:
  - `ip/cpu/plugins/exception.h` (大改造, ~150 LOC)
  - `ip/cpu/plugins/branch_predictor.h` (大改造, ~120 LOC)
  - `ip/cpu/arch/riscv/csr.h/.cpp` (新建, ~250 LOC)
  - `ip/mmu/lib/ptw.cpp` (+100 LOC: Sv32 + megapage)
- **新增测试**:
  - `tests/cpu/integration/test_csr_read_write.cpp` (~150 LOC)
  - `tests/cpu/integration/test_trap_delivery.cpp` (~120 LOC)
  - `tests/cpu/integration/test_mispredict_recovery.cpp` (~100 LOC)
  - `tests/mmu/test_sv32_ptw_decode.cpp` (~80 LOC)
- **新增文档**:
  - 2-3 个 ADR (CSR layout + exception delivery + mispredict recovery)
  - `openspec/changes/phase-2-baremetal-kickoff/` (后续 change 入口)

## Acceptance (待 wave3 后细化)

- [ ] 4 个子 change 全部 archive
- [ ] CSR 寄存器读写正确 (新测试 PASS)
- [ ] exception trap delivery 真生效 (新测试 PASS, CtrlLink::throw_when 真消费者)
- [ ] mispredict recovery 真生效 (新测试 PASS, CtrlLink::flush_when 真消费者)
- [ ] Sv32 PTW decode + megapage (新测试 PASS)
- [ ] `[riscv-tests]` 含 CSR/trap 子集扩展 (rv32mi-p 至少 5 用例 PASS)
- [ ] Phase 1.5 毕业标准达成 (见 `phase-1.5-stall-and-validate.md` §8)
- [ ] 3 门禁全 PASS
- [ ] CHANGELOG v0.9.0 段本 change 条目
- [ ] `openspec archive phase-1.5-wave-4 -y`

## Risk (待 wave3 后细化)

- **R1 (Q1-Q4 未回答)**: 4 个 Open Questions 须 wave3 archive 后才能定 → 占位期间无法 CI 验证
- **R2 (scope creep)**: 4 子 change 合并容易 scope creep → 建议 wave3 archive 后拆分为独立 openspec changes (本 change 作为 umbrella, 子项各自 archive)
- **R3 (CSR layout 复杂度)**: RISC-V CSR 完整 spec 含 ~80 个寄存器, 本 change 仅 5-6 个最小集 → 与 Q1 决策相关
- **R4 (Phase 2 RV64 切换)**: 本 change 仍 RV32-only, Phase 2 RV64 切换是另一 work stream (Q4 决策)

## 后续 Action (本 change 不 pending 具体任务)

- ⏸ 等 wave3-mmu-real-memory-and-cycle archive (3 个 change: mmu-paddr-consume + cycle-precision + multi-cycle)
- ⏸ 回填 4 个 Open Questions (Q1-Q4) 答案
- ⏸ 把本 umbrella change 拆分为 4 个独立 openspec changes (CSR / exception / mispredict / sv32)
- ⏸ 启动 phase-2-baremetal-kickoff 文档 change (Wave 4 archive 后)

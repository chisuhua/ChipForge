---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: draft
depends_on:
  - mmu-chmem-pipeline-integration  # C.1 stall_ctrl_ skeleton 复用
oracle_session: pending
oracle_date: pending
---

# csr-chmem-pipeline-integration — CH_MEM 模式 RISC-V CSR (Zicsr) 实装 (v0.11.0 owner)

> **Scope**: 本 change 关闭 CH_MEM 模式 CSR (Control and Status Register) 集成层 owner gap。`RiscvCsrPlugin` 当前是 43 行 P2 stub (`ip/cpu/arch/riscv/csr.h`),TLM 路径已注册但无业务逻辑;CH_MEM 路径 (`csr_chmem.h`) **完全不存在**。本 change 实装 Zicsr 指令 (CSRRW/CSRRS/CSRRC + immediate variants) + trap 路径 (ECALL/EBREAK/MRET) + minimum CSR 集合 (mstatus/mtvec/mepc/mcause/mie/mip/mcycle) 以便 MRET/CSR-write 流水线闭环。

## Why

CH_MEM 模式 7-plugin 流水线 (per `cpu_factory_chmem.h`) 当前**不包含 CSR plugin**:
- `cpu_factory_chmem.h:50-58` 显式枚举 7 plugin: Decoder, Hazard, IBus, RegFile, IntAlu, Branch, DMem
- `ip/cpu/STATUS.md:8` 列 plugin 套件含 RiscvCsrPlugin — 但只对 TLM 路径生效
- TLM `cpu_factory.h:504` 调 `pb.register_plugin(std::make_unique<RiscvCsrPlugin>())` — 但这是 P2 stub,setup/build 空函数

**调用依赖 (TLM 端已实装,CH_MEM 缺失)**:
- `ip/cpu/arch/riscv/decode.h:86` 译码 SYSTEM 指令时填 `rv.csr_addr = (op == OpCode::SYSTEM) ? get_funct12(inst) : 0` (CH_MEM 译码在 `decode_chmem.h` 同源 payload)
- `decode.h:117-118` SYSTEM 指令分支处理 (CSRRW/CSRRS/CSRRC + ECALL/EBREAK/MRET)
- D3-A 最小 (per `mmu-chmem-pipeline-integration` HANDOFF.md §1): MRET trap PC 跳转走 FSM 而非 combinational (CSR 指令 CSR-write 同样需 FSM,ch_state_machine DSL 符合 ADR-046 v2.0 豁免)

**C.1 mmu_exception_handler_chmem 现状 (per commit 1429f5a)**:
- stall_ctrl_ 已 setup() 注册到 decode stage
- combinational network skeleton 已注册 (待 Phase B mmu_chmem.h 落地后填 CPU_EXCEPTION_CODE 上游)
- HazardPlugin 端 (commit 06bb1d3) mmufault_pending_ 状态已添加
- **本 change 复用同一 stall_ctrl_ 路径** — mret/ecall/break trap 走相同 hazard_clear 路径

**owner 真空**:
- `ip/cpu/cpu_factory_chmem.h:50-58` 7-plugin list 不含 CSR (CSR 实装 = 第 8 plugin,需 owner 决策)
- 任何 Zicsr 指令 (CSRRW 等) 在 CH_MEM 路径 = no-op decoder + execute ALU 走 default 分支 (per decode.h:117 注释)
- Trap 路径 (ECALL → mepc/mcause CSR write) 完全没有 MMU exception code propagation

## What Changes

### 1. 实装 `csr_chmem.h` (D1 收窄 scope, B 收窄版风格)

- **新文件**:`ip/cpu/arch/riscv/csr_chmem.h` (~250 LOC, 含 ch_state_machine DSL for trap)
- **scope 边界** (D1 收窄, 类比 mmu-chmem-pipeline-integration D1-B):
  - **M-mode CSR 子集 (8 个,启动需求最小集)**:
    - `mstatus` (0x300): 仅 SDR/MIE/TW/TVM 字段,MRET 路径需要
    - `mtvec` (0x305): trap handler base address, ECALL/MRET 路径需要
    - `mepc` (0x341): trap return PC, MRET 需要
    - `mcause` (0x342): trap cause (3/7/11), ECALL/MRET 需要
    - `mie` (0x304): interrupt enable, 读路径 (write 推迟 Phase 5+)
    - `mip` (0x344): interrupt pending, 读路径 (write 推迟)
    - `mscratch` (0x340): trap scratch (可选)
    - `mcycle` (0xB00): cycle counter (read-only)
  - **Zicsr 指令 (4 类 × 2 变体 = 8 指令)**:
    - CSRRW / CSRRWI
    - CSRRS / CSRRSI
    - CSRRC / CSRRCI
  - **Trap 路径 (3 指令)**:
    - ECALL (0x000): trap to mtvec, mcause = 11 (E-call from M-mode)
    - EBREAK (0x001): trap to mtvec, mcause = 3 (Breakpoint)
    - MRET (0x302): PC ← mepc, mstatus.MIE 恢复
  - **多级 page table / LRU/RRIP / FPU / PMP / Smode 全部 out-of-scope** (Phase 5+)
- **D2 字类型**: 在 CH_MEM 模式用 `ch::core::ch_uint<32>` 表达 CSR value(无 64-bit CSR for RV32 PoC)
- **D3 combinational network**:
  - 在 `execute` stage LATE 闭包读 `cpu_keys<T>::CPU_EXCEPTION_CODE` 触发的 mmu_fault 路径(per mmu-chmem-pipeline-integration C.1) + **新增** CSRR* 写副作用(写 mstatus/mtvec/mepc/mcause)
  - ECALL/MRET trap 走 ch_state_machine 状态机(2 状态:IDLE → TRAP_PENDING,符合 ADR-046 v2.0 §2.1.1 豁免)
  - 跨周期 state 由 `ch::core::ch_reg<...>` 维护(elaboration 期固化)

### 2. cpu_factory_chmem.h 第 8 plugin 集成

- **修改**:`ip/cpu/cpu_factory_chmem.h:50-58` 7-plugin list → 8-plugin list
  - 加 `register_plugin(std::make_unique<RiscvCsrPluginChmem<T>>())` 在 RegFile 之后(IntAlu 之前)
  - ADR-048 canonical ordering 维持:CSRR* 写 mstatus CSR 在 execute LATE 之后,writeback 之前(per HazardPlugin ordering)
- **新注释** 替换 "7-plugin 流水线" 为 "8-plugin 流水线 + 启用 Zicsr ext"
- **enable_mmu 注释已有 owner attribution** (per commit 8bb55a9 C.4) — 保持

### 3. 单元测试 `test_csr_chmem_basic`

- **新文件**:`tests/cpu/test_csr_chmem.cpp` (~150 LOC)
- **Family tag**:`[cpu][csr-chmem]`
- **覆盖范围**:
  - Case 1: CSRRW x1, mstatus (0x300) → mstatus 写入, x1 = old mstatus
  - Case 2: CSRRS x1, mcycle (0xB00) → x1 = mcycle (read-only 位不写)
  - Case 3: ECALL → mcause = 11, mepc = ECALL PC + 4
  - Case 4: MRET (after ECALL) → PC = mepc
  - Case 5: EBREAK → mcause = 3
- **回归项**:`[verilator] 1/1 case PASS (5 ELF tohost=1)` 维持(Zicsr ELF 不在 rv32ui-p-* baseline)

## Risk

| 维度 | 评估 |
|------|------|
| 复杂度 | 中 — Zicsr 指令 + trap 路径 + 8 个 CSR + ch_state_machine FSM (~2 周) |
| CppHDL 探索 | 中 — ch_state_machine DSL per ADR-046 v2.0 (mmu-paddr-consume 已用 ch_bool;ch_state_machine 是新增机制) |
| 依赖 | C.1 stall_ctrl_ (per commit 1429f5a) + Phase B MMUPluginChmem (defer 2026-12-01) — CSRR* 独立 phase B,trap 路径需 Phase B 落地 |
| Phase 5+ scope | PMP / Smode / 64-bit CSR 明确 defer,scope 收窄降低 risk |

## Out-of-Scope (Phase 5+ 跟踪)

- PMP (Physical Memory Protection)
- Smode / S-mode CSR (sstatus, sepc, scause, stvec, sip, sie)
- 64-bit CSR (mcycleh, mtime, mtimecmp) — 需 RV64 only
- FPU CSR (fcsr, frm, fflags) — 需 F ext
- Debug CSR (dcsr, dpc, dscratch) — 需 Debug spec
- Cache CSR (perf-related) — 需 cache-coherent 场景
- 270+ 全 CSR 完整表 — 当前 D1 收窄 8 个,够 MRET + interrupt acknowledge 闭环即可

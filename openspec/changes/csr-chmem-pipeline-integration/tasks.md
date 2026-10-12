# csr-chmem-pipeline-integration — tasks.md

> **Status**: draft (2026-10-11)
> **Owner**: P1-4 wave5-isa-coverage-and-bp
> **Version target**: v0.11.0
> **Scope**: 8-plugin 流水线 + Zicsr 指令 (8) + trap 路径 (3 指令) + M-mode CSR 子集 (8 个)

---

## Phase Schedule Table

| Sprint | Window | Phase | Tasks | Owner |
|--------|--------|-------|-------|-------|
| Sprint 5 | 11-12 ~ 11-25 | Phase A (D1 收窄实装) | A.1-A.5 | TBD |
| Sprint 6+ | 11-26 ~ 12-15 | Phase B (cpu_factory 8-plugin 集成) | B.1-B.3 | TBD |
| Sprint 7+ | 12-16 ~ 01-10 | Phase C (测试 + 回归) | C.1-C.3 | TBD |

---

## Phase A — csr_chmem.h 收窄实装 (D1 锁定 scope)

### A.1 [RED] 写 `[cpu][csr-chmem]` 测试 — 5 case Zicsr + trap 基础

- [ ] 新建 `tests/cpu/test_csr_chmem.cpp` (~150 LOC)
- [ ] Family tag: `[cpu][csr-chmem]`
- [ ] 覆盖范围:
  - Case 1: CSRRW x1, mstatus → mstatus 写入, x1 = old mstatus
  - Case 2: CSRRS x1, mcycle → x1 = mcycle (read-only 位不写)
  - Case 3: ECALL → mcause = 11, mepc = PC + 4
  - Case 4: MRET (after ECALL) → PC = mepc
  - Case 5: EBREAK → mcause = 3
- [ ] 断言数: 8-12 assertions
- [ ] 运行命令: `./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`

### A.2 [GREEN] 实装 `RiscvCsrPluginChmem<T>` skeleton

- [ ] 新建 `ip/cpu/arch/riscv/csr_chmem.h` (~120 LOC, 基础类 + setup/build empty)
- [ ] 头守卫 `#ifndef CF_IP_CPU_ARCH_RISCV_CSR_CHMEM_H`
- [ ] `#ifdef CF_PLUGIN_USE_CH_MEM` 包裹
- [ ] D4 合规: 0 业务 tick(), 0 状态机 (FSM 推迟 A.3)
- [ ] 编译通过 + A.1 编译可链接 (test stub 失败但 build 干净)

### A.3 [GREEN] CSR value ch_reg 存储 (8 个 M-mode CSR)

- [ ] 在 `RiscvCsrPluginChmem::build()` 注册 at_stage("writeback", NORMAL) 闭包
- [ ] 闭包读 `payload::keys<T, 32>::CSR_WRITE_ADDR` (新 key, 12-bit) + `CSR_WRITE_VALUE` (32-bit)
- [ ] if (addr == 0x300) ch_reg<32>("mstatus") = value
- [ ] if (addr == 0x305) ch_reg<32>("mtvec") = value
- [ ] if (addr == 0x341) ch_reg<32>("mepc") = value
- [ ] if (addr == 0x342) ch_reg<32>("mcause") = value
- [ ] 读路径: at_stage("execute", LATE) 闭包从 `payload::keys<T, 32>::CSR_READ_REQ` (新 key, bool) 触发,填 `CSR_READ_VALUE` (32-bit) payload
- [ ] 8 个 CSR 完整, mcycle 只读特殊处理
- [ ] A.1 Case 1+2 PASS (CSRRW / CSRRS 基础)

### A.4 [GREEN] ch_state_machine DSL trap 状态机 (ADR-046 v2.0 §2.1.1 豁免)

- [ ] 引入 `ch::core::ch_state_machine<TrapState>` (elaboration 期固化)
- [ ] 2 状态: `IDLE`, `TRAP_PENDING`
- [ ] ECALL/EBREAK 触发 (at_stage("execute", NORMAL) 读 funct12):
  - if (funct12 == 0x000) → trap_cause = 11, trap_pending = 1
  - if (funct12 == 0x001) → trap_cause = 3, trap_pending = 1
  - if (funct12 == 0x302) → mret_pending = 1
- [ ] trap_pending 状态在下一个 cycle 写 mcause/mepc + 触发 mmu_exception_handler 路径(C.1 stall_ctrl_ 复用)
- [ ] mret_pending 在下一个 cycle 写 PC = mepc
- [ ] 加 `CF_PLUGIN_USE_FSM_EXEMPT` 宏标记 (per ADR-046 v2.0 §2.1.1 算术/控制 FSM 豁免)
- [ ] A.1 Case 3+4+5 PASS (ECALL/EBREAK/MRET)

### A.5 [GREEN] 集成回归

- [ ] chipforge_tests_chmem `[cpu]` 不退化
- [ ] chipforge_tests_chmem `[verilator]` 不退化 (rv32ui-p-* ELF 不触发 Zicsr 指令)
- [ ] chipforge_tests_chmem `[chmem]` 不退化
- [ ] bare path byte-identical (CSRR* 指令在 rv32ui-p-* 中不出现,新增 plugin 零影响)

---

## Phase B — cpu_factory_chmem.h 第 8 plugin 集成 (B.1-B.3)

### B.1 [GREEN] 集成 `RiscvCsrPluginChmem<T>` 到 build_cpu()

- [ ] 修改 `ip/cpu/cpu_factory_chmem.h:50-58` 7-plugin list → 8-plugin list
- [ ] 在 RegFile 之后(IntAlu 之前)注册:
  ```cpp
  pb->register_plugin(std::make_unique<RiscvCsrPluginChmem<T>>());
  ```
- [ ] ADR-048 canonical ordering 检查: CSR 写 CSR 在 execute LATE,writeback NORMAL 之间,符合 HazardPlugin stall
- [ ] 注释更新: "7-plugin 流水线" → "8-plugin 流水线 + Zicsr ext 默认启用"

### B.2 [GREEN] 移除 7-plugin 注释,加 owner attribution (类比 C.4)

- [ ] 替换注释 "7-plugin 流水线无 MMU hook" 描述为 8-plugin 完整描述
- [ ] 加 v0.11.0 owner 引用 + change 路径

### B.3 [GREEN] enable_mmu 注释同步(无变化,只是 review)

- [ ] cpu_factory_chmem.h:80-85 enable_mmu 注释已由 commit 8bb55a9 (C.4) 标 owner
- [ ] 回归项: 8-plugin 流水线 build 通过,无链接错误

---

## Phase C — 验证与回归 (C.1-C.3)

### C.1 [GREEN] [cpu-integration] 7-stage riscv 新增 Zicsr 测试

- [ ] 新建 `tests/cpu/integration/test_7stage_zicsr.cpp` (~80 LOC)
- [ ] 使用手动 ELF (add.elf 简单 binary + 内联 CSRRW/MRET) 跑 7-stage pipeline
- [ ] 期望: mstatus 写入, MRET 跳转,tohost=1

### C.2 [GREEN] 全量回归

- [ ] chipforge_tests TLM 0 退化 (`[mmu] 59/59 + [cpu] 128/129 + [cpu-integration] 81/81`)
- [ ] chipforge_tests_chmem 0 退化 (`[chmem] 9/9 + [cpphdl] 6/6 + [verilator] 40/40 + [cpu] 24 cases`)
- [ ] 新增 [cpu][csr-chmem] 5/5 PASS

### C.3 [GREEN] 文档同步

- [ ] AGENTS.md §CH_MEM 模式 更新 7→8 plugin
- [ ] ip/cpu/STATUS.md 移除 "🔴 RISC-V CSR P2 占位" 状态
- [ ] openspec/changes/mmu-chmem-pipeline-integration 链接加此 change (related)

---

## 接受目标

- [cpu][csr-chmem] 5/5 PASS
- 8-plugin 流水线在 chipforge_tests_chmem 全套零退化
- Zicsr 8 指令 + trap 3 指令功能闭环 (CSRRW/CSRRS/CSRRC 写、CSRRWI/CSRRSI/CSRRCI 写立即数、ECALL/EBREAK/MRET 跳转)
- bare path (rv32ui-p-* 无 Zicsr) byte-identical preserved

## 关键依赖与 hard-block

| 依赖 | 状态 | 阻塞? |
|------|------|--------|
| mmu-chmem-pipeline-integration C.1 stall_ctrl_ | ✅ merged (1429f5a) | 复用,无新依赖 |
| mmu-chmem-pipeline-integration Phase B mmu_chmem.h | ⏸ defer 2026-12-01 | trap 路径 mcause/mepc 写需 Phase B 落地 (但 A.4 FSM 状态机可独立测试 mcause=11) |
| chipforge_tests_chmem 二进制 + Verilator 后端 | ✅ ready | 验证环境就绪 |
| ch_state_machine DSL (mmu_ptw_chmem 已有先例) | ✅ available | FSM 模式参考 mmu_ptw_chmem.h |

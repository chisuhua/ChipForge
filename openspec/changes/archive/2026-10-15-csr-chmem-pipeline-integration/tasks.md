# csr-chmem-pipeline-integration — tasks.md

> **Status**: ready (2026-10-14, design.md R1-R3 决策 locked + tasks.md 拆解落地)
> **Owner**: P1-4 wave5-isa-coverage-and-bp
> **Version target**: v0.11.0
> **Scope**: 8-plugin 流水线 + Zicsr 指令 (8) + trap 路径 (3 指令) + M-mode CSR 子集 (8 个)
> **Design decisions**: `design.md` §R1/R2/R3 (Oracle 2026-10-13) — 实施前必读
> **Plan**: `.rddf/plans/csr-chmem-pipeline-integration.md` (TDD 5-step, 估时 2 周)

---

## Phase Schedule Table (2026-10-14 修订: 拆 R1-R3 决策实施)

| Sprint | Window | Phase | Tasks | Owner |
|--------|--------|-------|-------|-------|
| Sprint 5 | 11-12 ~ 11-25 | Phase A (D1 收窄实装 + R1 数据通路) | A.1-A.5, R1.1-R1.4 | TBD |
| Sprint 6 | 11-26 ~ 12-09 | Phase B (R3 CSR 读值 RD_DATA + cpu_factory 集成) | R3.1-R3.5, B.1-B.3 | TBD |
| Sprint 6+ | 12-10 ~ 12-23 | Phase C (R2 trap 重定向 + 测试 + 回归) | R2.1-R2.5, C.1-C.3 | TBD |
| Sprint 7 | 12-24 ~ 01-13 | Phase D-E (架构门禁 + archive) | D, E.1-E.4 | TBD |

**Phase 间硬依赖** (per `design.md` §4):
- R1 (csr_addr 重提取) → R3 (CSR 读值 RD_DATA 通路) → R2 (trap 重定向)
- R1.1 (ibus_chmem.h 改 1 行 写 INSTRUCTION) → R1.2 (csr_chmem.h 重提取)
- R3.1 (bundles/csr_bundles.h 新建) → R3.2 (IntAlu 改 4 行) → R3.3 (csr_chmem.h 写 CSR_READ_VALUE)
- R2.1 (csr_keys 新 TRAP_PENDING/TRAP_TARGET) → R2.2 (csr_chmem.h branch-LATE) → R2.3 (ibus_chmem.h branch-NORMAL select)
- **R2 须 IBus 改 2 处 (R1.1 + R2.3)**, 与 mmu-chmem-pipeline-integration C.2 (ibus_chmem.h bare byte-identical) 需协调合并顺序 — 建议 csr PR 在 mmu C.2 merge 后开

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

### A.3 [GREEN] CSR value ch_reg 存储 (8 个 M-mode CSR) — 2026-10-14 重写对齐 R3 决策

- [ ] 在 `RiscvCsrPluginChmem::build()` 注册 at_stage("writeback", NORMAL) 闭包
- [ ] 闭包读 `csr_keys<T>::CSR_WRITE_ADDR` (12-bit, 新增, R3 决策) + `CSR_WRITE_VALUE` (32-bit, 新增) + `CSR_WRITE_EN` (1-bit, 新增)
- [ ] **禁用 `if (ch_bool)` 模式** (per AGENTS.md §架构约束 #8 + R4 [High] 风险): 用 `select` 树 / write-enable mux 替代 8 个 `if` 分支
  ```cpp
  // 模式 (伪代码): 8 个 ch_reg<32> 各有独立 write-enable 闭包
  //   we = CSR_WRITE_EN & (CSR_WRITE_ADDR == 0x300)
  //   mstatus <<= select(we, CSR_WRITE_VALUE, mstatus)  // ch_reg 赋值须用 <<=
  //   (其他 7 个 CSR 同模式)
  ```
- [ ] 8 个 CSR 完整, mcycle 只读 (write-enable 恒 false, 除 read 路径外不写)
- [ ] 读路径 (CSRRW 读 old value, CSRRS 读 always): at_stage("execute", LATE) 闭包
  - **R1 决策**: csr_addr = `bits<31,20>(cpu_keys<T>::INSTRUCTION)` (不新增 DecodedInst 字段)
  - 填 `csr_keys<T>::CSR_READ_VALUE` (32-bit) + `CSR_READ_VALID` (1-bit) payload (per R3 决策: writeback 阶段 IntAlu 消费)
- [ ] A.1 Case 1+2 PASS (CSRRW / CSRRS 基础) — 需 R3.1-R3.5 (CSR_READ_VALUE 通路) 落地后**才** PASS; 当前 commit `c277799` skeleton 不含 R3, 必 FAIL until R3 落

### A.4 [GREEN] ch_state_machine DSL trap 状态机 (ADR-046 v2.0 §2.1.1 豁免)

- [ ] 引入 `ch::core::ch_state_machine<TrapState>` (elaboration 期固化)
- [ ] 2 状态: `IDLE`, `TRAP_PENDING`
- [ ] ECALL/EBREAK 触发 (at_stage("execute", LATE) 读 funct12 via R1 重提取):
  - if (funct12 == 0x000) → trap_cause = 11, trap_pending = 1
  - if (funct12 == 0x001) → trap_cause = 3, trap_pending = 1
  - if (funct12 == 0x302) → mret_pending = 1
- [ ] trap_pending 状态在下一个 cycle 写 mcause/mepc + 触发 mmu_exception_handler 路径(C.1 stall_ctrl_ 复用)
- [ ] mret_pending 在下一个 cycle 写 PC = mepc
- [ ] 加 `CF_PLUGIN_USE_FSM_EXEMPT` 宏标记 (per ADR-046 v2.0 §2.1.1 算术/控制 FSM 豁免)
- [ ] A.1 Case 3+4+5 PASS (ECALL/EBREAK/MRET) — 需 R2.1-R2.5 (trap PC 重定向) 落地后**才** PASS; 当前 skeleton 不含 R2, 必 FAIL until R2 落

---

## R1 — csr_addr / funct12 信号源 (decision: 方法 A 重提取, 实施前必读 design.md §R1)

### R1.1 [GREEN] ibus_chmem.h 改 1 行: 写统一 INSTRUCTION payload key

- [ ] 修改 `ip/cpu/plugins/ibus_chmem.h` ~line 146-155 (fetch aread 闭包)
- [ ] 在 aread 闭包内, 已有 `instr` 局部变量, 加 1 行:
  ```cpp
  (*node)(cpu_keys<T>::INSTRUCTION) = instr;  // R1 决策: 统一 key, CSR 重提取信号源
  ```
- [ ] `cpu_keys<T>::INSTRUCTION` 须先存在 (查 `bundles/cpu_bundles.h` 或类似); 如不存在, 加 1 行 enum 声明
- [ ] 编译通过, `[verilator]` baseline 5 ELF tohost=1 不退化 (rv32ui-p-* 不读 INSTRUCTION, 加 1 写零影响)
- [ ] 协调: 与 mmu-chmem-pipeline-integration C.2 改同一文件, 建议 mmu C.2 merge 后再开本 task

### R1.2 [GREEN] csr_chmem.h execute-LATE 闭包: 重提取 csr_addr / funct12

- [ ] 修改 `ip/cpu/arch/riscv/csr_chmem.h` 的 `build()` (现 88 LOC skeleton, commit `c277799`)
- [ ] 新增 at_stage("execute", LATE, [...]) 闭包
- [ ] 闭包内:
  - `auto instr = (cpu_keys<T>::INSTRUCTION).value();`
  - `csr_addr = bits<31, 20>(instr);` (12-bit, 0x000-0xFFF)
  - `funct12 = bits<31, 20>(instr);` (SYSTEM 指令 12-bit funct12 与 csr_addr 同字段)
  - `imm5 = bits<19, 15>(instr);` (CSRRWI/CSRRSI/CSRRCI 立即数 5-bit, zero-extend)
- [ ] 闭包**不**写任何 payload (只提取, 留给 R3.3 写 CSR_READ_VALUE)
- [ ] 编译通过, A.1 Case 1-5 仍 FAIL (无 R3/R2 配合)

### R1.3 [GREEN] csr_chmem.h execute-LATE 闭包: Zicsr 指令判定

- [ ] 续 R1.2, 同一闭包内加 Zicsr 指令判定 (用 select 树, 不用 if)
- [ ] 输出 `csr_keys<T>::Zicsr_OP` (3-bit enum: CSRRW/CSRRS/CSRRC/CSRRWI/CSRRSI/CSRRCI/INVALID)
  - `bits<14, 12>(instr)` 为 funct3, 映射:
    - 001 → CSRRW, 010 → CSRRS, 011 → CSRRC
    - 101 → CSRRWI, 110 → CSRRSI, 111 → CSRRCI
    - 其他 → INVALID
- [ ] 输出 `csr_keys<T>::Zicsr_VALID` (1-bit) = (bits<6, 0>(instr) == 0b1110011)  // SYSTEM opcode
- [ ] 输出 `csr_keys<T>::CSR_WRITE_EN` (1-bit) = Zicsr_VALID & !mcycle_readonly (R3 决策: mcycle 不可写)
- [ ] A.1 Case 1-2 仍 FAIL (无 R3 写 CSR_READ_VALUE 通路)

### R1.4 [GREEN] R1 单元验证: 提取值正确性单测

- [ ] 追加 `tests/cpu/test_csr_chmem.cpp` 3 个新 TEST_CASE (~50 LOC):
  - Case 6: `ZicsrAddressReextract_FromInstruction` — 构造假 INSTRUCTION (e.g. 0x30052073 = csrrs x4, mstatus; x10), 调 build, 验证 `csr_addr == 0x300`
  - Case 7: `ZicsrImmediateReextract_FromInstruction` — 构造 CSRRWI (0x30552073 + imm5=5), 验证 `imm5 == 5`
  - Case 8: `NonZicsrSystemInstruction_NoExtract` — 构造 ECALL (0x00000073), 验证 `Zicsr_VALID == false` 但 `funct12 == 0x000` (trap 路径仍可读)
- [ ] 3/3 PASS

---

## R3 — CSR 读值 → rd 写回通路 (decision: 方法 X, 实施前必读 design.md §R3)

### R3.1 [GREEN] 新建 `bundles/csr_bundles.h` (~30 LOC)

- [ ] 新建 `bundles/csr_bundles.h`, namespace `cf::cpu::csr::payload`
- [ ] 定义 `csr_keys<T>` struct, 模板化 T=uint32/uint64:
  - `static inline cf::plugin::Payload<T> CSR_READ_VALUE{"csr.read_value"};` (32-bit, R3 决策)
  - `static inline cf::plugin::Payload<bool> CSR_READ_VALID{"csr.read_valid"};`
  - `static inline cf::plugin::Payload<T> CSR_WRITE_ADDR{"csr.write_addr"};` (12-bit)
  - `static inline cf::plugin::Payload<T> CSR_WRITE_VALUE{"csr.write_value"};` (32-bit)
  - `static inline cf::plugin::Payload<bool> CSR_WRITE_EN{"csr.write_en"};`
  - `static inline cf::plugin::Payload<bool> Zicsr_VALID{"csr.zicsr_valid"};`
  - `static inline cf::plugin::Payload<std::uint8_t> Zicsr_OP{"csr.zicsr_op"};` (3-bit enum)
- [ ] 头守卫 `#ifndef CF_BUNDLES_CSR_BUNDLES_H`
- [ ] `cf::plugin::Payload<T>` 已存在 (per `bundles/mem_bundles.h` 同源)

### R3.2 [GREEN] `IntAlu_chmem` 改 4 行: 消费 CSR_READ_VALUE 覆写 RD_DATA

- [ ] 修改 `ip/cpu/plugins/IntAlu_chmem` (找精确文件, 或 `int_alu_chmem.h`/`alu_chmem.h` 等; 如不存, 标 deferred + 改 4 行阻塞)
- [ ] 在 execute-LATE 闭包 (R3 决策: IntAlu 位置 6, 晚于 CSR 位置 5) 加 4 行:
  ```cpp
  // R3 决策: 消费 CSR_READ_VALUE, 优先于 IntAlu default RD_DATA
  //   (CSR 位置 5 早于 IntAlu 位置 6, IntAlu execute-LATE 在 CSR 之后,
  //    故 IntAlu 可"覆写"CSR_READ_VALUE 到 RD_DATA)
  if (auto* node = ... /* fetch RD_DATA 节点 */) {
    (*node)(cpu_keys<T>::RD_DATA) = select(csr_keys<T>::CSR_READ_VALID, csr_keys<T>::CSR_READ_VALUE, default_rd_data);
  }
  ```
- [ ] 编译通过, `[cpu]` baseline 不退化
- [ ] 协调: 与 R1.1 改同一时段 (执行前确认 `IntAlu_chmem` 文件名)

### R3.3 [GREEN] csr_chmem.h execute-LATE 闭包: 写 CSR_READ_VALUE (旧值)

- [ ] 修改 `csr_chmem.h` (续 R1.3), execute-LATE 闭包内
- [ ] 输出 `csr_keys<T>::CSR_READ_VALUE` = `select(Zicsr_VALID, read_csr_register(csr_addr), 0)` (CSRRW 读 old, CSRRS 读 always)
- [ ] 输出 `csr_keys<T>::CSR_READ_VALID` = `Zicsr_VALID & !is_mcycle(csr_addr)` (R3 决策: mcycle 读但不走 RD_DATA, 单独路径)
- [ ] 读实现: 实例成员 `mstatus_/mtvec_/mepc_/mcause_/mie_/mip_/mscratch_/mcycle_` (8 个 ch_reg<ch_uint<32>>), select 树:
  ```cpp
  // 8 选 1 mux (无 if, 用 select 树)
  //   CSR_READ_VALUE = (csr_addr == 0x300) ? mstatus_ : ... : 0
  //   编码为 8 个 (csr_addr == X) bool 标志 + select 嵌套
  ```
- [ ] 编译通过, A.1 Case 1 (CSRRW) **真 PASS** 验证 R3 决策落地

### R3.4 [GREEN] csr_chmem.h writeback-NORMAL 闭包: 写 CSR 寄存器 (新值)

- [ ] 修改 `csr_chmem.h` 的 at_stage("writeback", NORMAL, [...]) 闭包 (替代 A.3 旧的 if 模式)
- [ ] 闭包内:
  - 8 个独立 write-enable: `we_X = CSR_WRITE_EN & (CSR_WRITE_ADDR == 0xX) & !is_mcycle`
  - 8 个 ch_reg 赋值 (用 `<<=` 而非 `=`):
    ```cpp
    mstatus_ <<= select(we_300, CSR_WRITE_VALUE, mstatus_);
    mtvec_   <<= select(we_305, CSR_WRITE_VALUE, mtvec_);
    // ... 其他 6 个同模式
    mcycle_  <<= select(false, ?, mcycle_);  // mcycle 永不被 CSRR* 写
    ```
- [ ] **强制 select 树, 严禁 if (ch_bool)** (per R4 [High] + AGENTS.md §架构约束 #8)
- [ ] A.1 Case 2 (CSRRS mcycle) PASS — 验证 read-only 行为

### R3.5 [GREEN] R3 单元验证: CSR_READ_VALUE 通路 5 case PASS

- [ ] 跑 `./build/bin/chipforge_tests_chmem "[cpu][csr-chmem]"`
- [ ] A.1 Case 1-2 全部 PASS (CSRRW + CSRRS 基础)
- [ ] 跑 `./build/bin/chipforge_tests "[cpu]"` baseline 不退化
- [ ] 跑 `./build/bin/chipforge_tests_chmem "[verilator]"` baseline 1/1 (48 ELF) 不退化

---

## R2 — MRET / ECALL / EBREAK PC 重定向 (decision: 自建 CtrlLink + 新 key, 实施前必读 design.md §R2)

### R2.1 [GREEN] 扩 `bundles/csr_bundles.h` 加 2 个新 key (R2 决策)

- [ ] 修改 `bundles/csr_bundles.h` (续 R3.1), 加 2 行:
  - `static inline cf::plugin::Payload<T> TRAP_PENDING{"csr.trap_pending"};` (1-bit, ch_bool)
  - `static inline cf::plugin::Payload<T> TRAP_TARGET{"csr.trap_target"};` (32-bit, PC 覆写值)
- [ ] 编译通过, R3.1 既有 keys 不动

### R2.2 [GREEN] csr_chmem.h branch-LATE 闭包: 写 TRAP_PENDING / TRAP_TARGET

- [ ] 修改 `csr_chmem.h` (续 R1.3 + R3.3)
- [ ] 新增 at_stage("branch", LATE, [...]) 闭包 (R2 决策: 晚于 IBus branch-LATE, 故 IBus NORMAL 覆写时 TRAP_PENDING 可见)
- [ ] 闭包内读 `funct12` (R1.2 重提取的) + `INSTRUCTION`:
  - `trap_pending = (funct12 == 0x000) | (funct12 == 0x001) | (funct12 == 0x302)` (select 树, 不用 if)
  - `trap_target = select(funct12 == 0x302, mepc_, select(funct12 == 0x000, mtvec_, select(funct12 == 0x001, mtvec_, 0)))` // ECALL/EBREAK → mtvec, MRET → mepc
  - 同时写 mcause + mepc (在 ECALL/EBREAK 时):
    - `mcause_ <<= select(funct12 == 0x000, ch_literal<11, 32>{}, select(funct12 == 0x001, ch_literal<3, 32>{}, mcause_))`
    - `mepc_ <<= select((funct12 == 0x000) | (funct12 == 0x001), PC_PLUS_4, mepc_)` (PC+4 来源: `cpu_keys<T>::PC` 节点)
- [ ] 写 `csr_keys<T>::TRAP_PENDING` + `TRAP_TARGET` payload
- [ ] 编译通过, A.1 Case 3-5 仍 FAIL (无 IBus NORMAL 消费)

### R2.3 [GREEN] ibus_chmem.h branch-NORMAL 闭包: select 覆写 PC 节点

- [ ] 修改 `ip/cpu/plugins/ibus_chmem.h` (续 R1.1 改 1 行之后, 第二次改)
- [ ] 在 branch-NORMAL 闭包 (现 `ibus_chmem.h` 可能不区分 NORMAL/LATE, 须先看实现) 加 select 覆写:
  ```cpp
  // R2 决策: trap > branch > pc+4
  //   next_pc = TRAP_PENDING ? TRAP_TARGET : (br_taken ? BRANCH_TARGET : pc+4)
  next_pc = select(csr_keys<T>::TRAP_PENDING, csr_keys<T>::TRAP_TARGET,
                   select(br_taken, BRANCH_TARGET, pc_plus_4));
  ```
- [ ] 与 mmu C.2 bare byte-identical 保护: `TRAP_PENDING` 默认 false (无 CSR 写时), select 退化为 `pc+4`, byte-identical
- [ ] 编译通过, A.1 Case 3-5 **真 PASS** 验证 R2 决策落地
- [ ] 协调: 与 mmu C.2 改同一文件 (ibus_chmem.h), **mmu C.2 merge 之前不可合本 task**

### R2.4 [GREEN] R2 单元验证: trap 优先级 5 case

- [ ] 追加 `tests/cpu/test_csr_chmem.cpp` 2 个新 TEST_CASE:
  - Case 9: `TrapPriorityOverBranch` — 同时触发 `mret` 与 `beq` (通过手动构造 pipeline state), 验证 PC 走 mepc 而非 branch target
  - Case 10: `MretPCEqualsMepc` — ECALL → MRET 序列, 验证 MRET 后 PC = mepc (即 ECALL PC+4)
- [ ] 2/2 PASS
- [ ] `[cpu][csr-chmem]` 总 5+3+2 = 10/10 PASS (A.1 5 + R1.4 3 + R2.4 2)

### R2.5 [GREEN] mcycle 自由计数详化 (Q1 pending, 收口)

- [ ] 修改 `csr_chmem.h`, 加 at_stage("fetch", ALWAYS, [...]) 闭包
- [ ] 闭包内 `mcycle_ <<= mcycle_ + ch_literal<1, 32>{};` (carry 自增, 自由运行)
- [ ] 验证: A.1 跑 100 cycle, 读 mcycle 应得 ~100 (允许 ±1 延迟)
- [ ] 加 `tests/cpu/test_csr_chmem.cpp` Case 11 `McycleFreeRunning` (~30 LOC)
- [ ] 11/11 PASS

---

## R4-R7 风险缓解 (合并入 Phase A-B-C, 不单开 phase)

- [ ] **R4 [High] at_stage 内禁用 if(ch_bool)**: A.3 旧 `if (addr == 0x300)` 全删, 改 R3.4 select 树; 强制 CI grep 兜底 (`tools/check_plugin_portability.sh` Check 5 扩 csr_chmem.h)
- [ ] **R5 [Medium] ch_state_machine 生产率风险**: 开工前 owner 读 `mmu_ptw_chmem.h` 全文, 抄 2 状态 FSM 模式; 估时 2 周保留乐观
- [ ] **R6 [Medium] mcycle 自由计数详化**: R2.5 已落
- [ ] **R7 [Low] gate 假阴性 (csr_chmem.h 路径漏洞)**: 把 `ip/cpu/arch/riscv/csr_chmem.h` 移到 `ip/cpu/plugins/csr_chmem.h`, 走 `check_plugin_portability.sh` Check 2 强制检查; **不阻塞本 change, 建议 Sprint 5 启动后第 1 周完成**

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

### A.5 [GREEN] 集成回归 (R1-R3 全部落地后)

- [ ] chipforge_tests_chmem `[cpu][csr-chmem]` 11/11 PASS (A.1 5 + R1.4 3 + R2.4 2 + R2.5 1)
- [ ] chipforge_tests_chmem `[cpu]` baseline 不退化
- [ ] chipforge_tests_chmem `[verilator]` 1/1 baseline (48 ELF, 96 assertions) 不退化
- [ ] chipforge_tests_chmem `[chmem]` 9/9 不退化
- [ ] bare path byte-identical (rv32ui-p-* 不触发 Zicsr 指令 + 不触发 ECALL/EBREAK/MRET,新增 plugin 零影响)
- [ ] **R7 落地**: 把 `ip/cpu/arch/riscv/csr_chmem.h` 移到 `ip/cpu/plugins/csr_chmem.h` (Sprint 5 第 1 周)

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

### C.1 [GREEN] [cpu-integration] 7-stage riscv 新增 Zicsr e2e 集成测试

- [ ] 新建 `tests/cpu/integration/test_7stage_zicsr_e2e.cpp` (~120 LOC)
- [ ] **不能用 add.elf** (R1.3 决策: add.elf 是 R-type/L-type, 无 Zicsr 指令; 2026-10-14 修订: 旧 C.1 假设"内联 CSRRW/MRET"误, manual_elf 现有 4 ELF 都不含 Zicsr)
- [ ] **新建 Zicsr ELF**: `tests/cpu/manual_elf/zicsr_basic.S` (~30 LOC)
  - `csrrs x1, mstatus, x0;` (读 mstatus 到 x1)
  - `csrrw x2, mstatus, x1;` (写 mstatus = x1)
  - `ecall;` (trap)
  - 期望 tohost=1, mstatus 写入, mcause=11
- [ ] `build_manual_elf.sh` 加 zicsr_basic.S → zicsr_basic.elf, 用 `link.ld` (与 add 复用)
- [ ] 7-stage pipeline 跑 zicsr_basic.elf 期望 tohost=1 within 5000 cycles
- [ ] 8-stage pipeline 跑同 ELF 期望 tohost=1 (验证 8-plugin 集成)
- [ ] `[cpu-integration] 91/93` PASS (现有 89 + zicsr 2 新 case)

### C.2 [GREEN] 全量回归 (2026-10-14 baseline 数字对齐)

- [ ] chipforge_tests TLM 0 退化:
  - `[mmu] 59/59` (2026-10-14 实测)
  - `[cpu] 128/129` (1 已知 cpu_sim FAIL, 非本 change 责任)
  - `[cpu-integration] 89/93` (4 已知 cpu_sim FAIL + 1 SIGTERM dhrystone, 非本 change 责任)
  - `[cpu-l1-mmu-demo] 7/7` (or 6/6, 实测确认)
- [ ] chipforge_tests_chmem 0 退化:
  - `[chmem] 9/9` + `[cpphdl] 6/6` + `[verilator] 1/1` (48 ELF, 96 assertions) + `[mmu-verilator] 3/3`
  - 新增 `[cpu][csr-chmem] 11/11`
  - 新增 `[cpu-integration][zicsr] 2/2` (C.1)
- [ ] Architecture gate: `verify_adr 0 fail` / `verify_plugin_decision 8/8` / `check_plugin_portability 12/12`
- [ ] 跨测试 binary 跑 `bash tools/run_chipforge_tests.sh --all` 全 PASS (~167s)

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

---

## Phase D — 架构门禁 (E1 之后, archive 前必跑)

- [ ] 跑 `bash tools/verify_adr.sh` — 必须 0 失败 (ADR 引用一致性)
- [ ] 跑 `bash tools/verify_plugin_decision.sh` — 必须 0 失败 (D4 业务代码 8 项检查)
- [ ] 跑 `bash tools/check_plugin_portability.sh` — 必须 0 失败 (ADR-040 v2.0 12 项检查)
- [ ] 跑 `bash tools/doc_link_check.sh` — 必须 0 失败 (文档死链)
- [ ] 若失败:
  - 是本 PR 引入 → 修复
  - 是历史问题 → 记录但不阻塞 archive
- [ ] 运行命令: 上面 4 个 bash 命令, 顺序跑

---

## Phase E — Archive 准备 (D 之后)

- [ ] **E.1 [GREEN] CHANGELOG.md 同步**
  - [ ] 在 `CHANGELOG.md` v0.11.0 段 (如未建, 则新建) 加 entry: "csr-chmem-pipeline-integration: CH_MEM 模式 Zicsr 集成 (8 CSR + 8 指令 + 3 trap) (Oracle 2026-10-13 R1-R3 decisions)"
  - [ ] 标注 scope-partial 风险 (R4: at_stage 内 if(ch_bool) 陷阱; R7: 假阴性 gate 风险)
- [ ] **E.2 [GREEN] AGENTS.md §CH_MEM 模式 段更新**
  - [ ] 7-plugin 流水线 → 8-plugin 流水线 文字修订
  - [ ] `ip/cpu/STATUS.md` 移除 "🔴 RISC-V CSR P2 占位" 状态 (R1 落 → 占位消除)
- [ ] **E.3 [GREEN] `openspec change validate csr-chmem-pipeline-integration` 0 error**
  - [ ] 验证 `specs/csr-chmem-zicsr/spec.md` 4 个 ADDED Requirements + Scenarios 解析无错
- [ ] **E.4 [GREEN] Archive 触发**
  - [ ] 跑 `openspec archive csr-chmem-pipeline-integration -y` 触发 archive
  - [ ] 验证目录移到 `openspec/changes/archive/2026-11-XX-csr-chmem-pipeline-integration/`
  - [ ] 验证 `openspec/specs/csr-chmem-zicsr/spec.md` 升格
  - [ ] 跑 `bash .rddf/sync_strategy_status.sh` 更新 strategy.md §7
- [ ] **运行命令**:
  - `bash tools/verify_adr.sh && bash tools/verify_plugin_decision.sh && bash tools/check_plugin_portability.sh && bash tools/doc_link_check.sh`
  - `openspec change validate csr-chmem-pipeline-integration`
  - `openspec archive csr-chmem-pipeline-integration -y`

---

## 接受目标 (2026-10-14 修订: 与 AGENTS.md v0.10.4 实测数字对齐)

- [cpu][csr-chmem] 5/5 PASS
- 8-plugin 流水线在 chipforge_tests_chmem 全套零退化
  - TLM 端基线: `[mmu] 59/59 + [cpu] 128/129 (1 已知 cpu_sim FAIL) + [cpu-integration] 89/93 (4 已知 cpu_sim FAIL + 1 SIGTERM dhrystone)`
  - CH_MEM 端基线: `[chmem] 9/9 + [cpphdl] 6/6 + [verilator] 1/1 (48 ELF, 96 assertions) + [mmu-verilator] 3/3`
- Zicsr 8 指令 + trap 3 指令功能闭环 (CSRRW/CSRRS/CSRRC 写、CSRRWI/CSRRSI/CSRRCI 写立即数、ECALL/EBREAK/MRET 跳转)
- bare path (rv32ui-p-* 无 Zicsr) byte-identical preserved

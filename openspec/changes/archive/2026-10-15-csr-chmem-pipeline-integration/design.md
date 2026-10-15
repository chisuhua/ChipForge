# Design — csr-chmem-pipeline-integration (v0.11.0)

> 承载 Oracle 2026-10-13 审查 R1-R3 设计决策 + 实装机制选择。
> 与 proposal §What Changes 对应, tasks.md 任务分配的依据。

## Context

`RiscvCsrPlugin` 当前是 43 行 P2 stub (`ip/cpu/arch/riscv/csr.h`), TLM 路径已注册但无业务逻辑; CH_MEM 路径 (`csr_chmem.h`) **完全不存在**。本 change 实装 Zicsr 指令 (CSRRW/CSRRS/CSRRC + immediate variants) + trap 路径 (ECALL/EBREAK/MRET) + minimum CSR 集合 (mstatus/mtvec/mepc/mcause/mie/mip/mcycle) 以便 MRET/CSR-write 流水线闭环。

**Oracle 审查 (2026-10-13) 发现 3 个 R[Critical] 数据通路缺位, 须在开工前先做机制决策**:

## R1 决策: csr_addr / funct12 信号源

**现状**: `decode_chmem.h` 的 `DecodedInst` (line 70-83) **没有 `csr_addr` / `funct12` 字段**。Step 13 注释 (line 407-418) 明说 RISCV_DETAIL 是 elaboration-time 常量默认值, 不走运行时 payload。proposal §Why:24 引用 `decode.h:86` (TLM 路径) 暗示 CH_MEM 同源 —— **事实错误** (TLM 与 CH_MEM 译码 plugin 是两套)。

**决策 (方案 A)**: 在 `RiscvCsrPluginChmem::at_stage("execute", LATE, [...])` 闭包内**自行**重提取 `bits<31,20>(cpu_keys<T>::INSTRUCTION)` 作为 csr_addr。这样不需扩展 `DecodedInst` 字段, 不动 decoder + 全消费者。

**代价**:
- ✅ 范围最小, 不动现有 plugin
- ✅ 已有先例: `csr_chmem.h:88` skeleton 注释里已写类似重提取模式
- ⚠️ `cpu_keys<T>::INSTRUCTION` 必须前递到 execute stage, 需 `ibus_chmem.h` 写 INSTRUCTION (current `ibus_chmem.h:146-155` 只写 `instr`, 不是统一 key; 须改造)。**R1 阻断 R3 实施**。

**未决策的备选 (方案 B)**: 扩 `DecodedInst` 加 `csr_addr` / `funct12` 字段 —— 动 decoder + 全消费者, scope 膨胀。**不采纳**。

**实施依赖**: R1 → R3 (CSR 读值 RD_DATA 通路)。

## R2 决策: MRET / ECALL PC 重定向机制

**现状**: IBus 的 pc next-mux 是 `select(br_taken, BRANCH_TARGET, pc+4)` 在 branch LATE (`ibus_chmem.h:146-155`)。CSR 按 §2 注册在第 5 位 (RegFile 后), IBus 是第 3 位 —— **elaboration 闭包顺序 = 注册顺序, IBus branch-LATE 采样 BRANCH_TARGET 早于 CSR 任何覆写**, redirect 会丢。

**trap 优先级 (per `objective-v0110-launch.md §8`)**: `trap > stall > branch > BP`。但 `wave5-bp-btb` change 目录**不存在** (幽灵依赖), 该优先级协调项无 owner。

**决策**: CSR plugin **自建** `CtrlLink` + **新增 payload key** `csr_keys<T>::TRAP_TARGET` (ch_uint<32>) + **新增 payload key** `csr_keys<T>::TRAP_PENDING` (ch_bool)。在 `at_stage("branch", LATE, [...])` 闭包 (复用 BranchPlugin 的 branch stage, **后于** IBus) 写:
- `TRAP_PENDING = mcause != 0 (即 trap 触发条件)`
- `TRAP_TARGET = mepc / mtvec (per MRET / ECALL/EBREAK)`

IBus 在 branch NORMAL (而非 LATE) 增加 `select(TRAP_PENDING, TRAP_TARGET, ...)` 覆写 BRANCH_TARGET。

**代价**:
- ✅ IBus 只改 NORMAL 阶段 (elaboration 顺序晚于 LATE), 不会丢
- ⚠️ IBus NORMAL 闭包需消费 `TRAP_PENDING` key, IBus plugin 必须**改一行** (而非不依赖)
- ⚠️ 与 `wave5-bp-btb` pc-mux 优先级协调项: 本 change 显式声明 `trap > bp`, 但 bp-btb change 启动时需协商, 可能产生 PR 冲突

**未决策的备选**: 让 CSR plugin 注册在 IBus 之前 (位置 2, 在 IBus branch-LATE 之前) —— 破坏 ADR-048 canonical ordering (Decoder 必须是位置 1), **不采纳**。

**实施依赖**: R2 须 IBus plugin 改一行 (接受新 payload key); 与 mmu-chmem-pipeline-integration 同一 IBus 修改可能产生合并冲突, 须协调。

## R3 决策: CSR 读值 → rd 写回通路

**现状**: SYSTEM 指令 `writes_rd=true` (`decode_chmem.h:318`)。RD_DATA 由 IntAlu default 分支驱动 (IntAlu plugin 后注册)。CSR plugin (先注册) execute-LATE 覆写会被 IntAlu (后注册) 盖掉 —— A.1 Case 1/2 必 FAIL。

**决策 (方案 X)**: CSR plugin 不覆写 RD_DATA, 而是把 **CSR 读值** (CSRRW 的 old value / CSRRS 不写位的 value) 写到**新增 payload key** `csr_keys<T>::CSR_READ_VALUE` (ch_uint<32>)。writeback 阶段新增 plugin 消费该 key, 优先于 IntAlu 覆写 RD_DATA。

**实施细节**:
- 新增 payload key 定义在 `bundles/csr_bundles.h` (新建)
- writeback 阶段 (factory 第 6 位, 在 CSR 之后) 由 IntAlu plugin 加 4 行代码消费 CSR_READ_VALUE (`select(CSR_READ_VALID, CSR_READ_VALUE, default_RD_DATA)`)
- ADR-048 canonical ordering: writeback 闭包必须**晚于** IntAlu execute-LATE (即注册顺序在 IntAlu 之后)

**代价**:
- ✅ 与 R1 (csr_addr 走 execute-LATE) + R2 (TRAP 走 branch LATE) 隔离, 三个 R 互相正交
- ✅ IntAlu 改动最小 (4 行)
- ⚠️ 新增 payload key + 新建 bundles/csr_bundles.h —— 小幅 scope 扩张 (但属于"必须的 1 个")

**未决策的备选 (方案 Y)**: 让 CSR plugin 注册在 writeback 阶段 (位置 6, 晚于 IntAlu) —— 破坏 canonical ordering (execute 阶段必须 hold CSR 状态), **不采纳**。

**未决策的备选 (方案 Z)**: 让 IntAlu 改 default 分支消费 CSR_READ_VALUE (CSR 写 IntAlu 的 key 而非自建 key) —— 双向耦合, 维护成本高, **不采纳**。

**实施依赖**: R3 须 IntAlu plugin 改 4 行; 与方案 X 配套实施。

## 4. 综合实施顺序

R1 → R3 → R2 (按依赖):
1. **R1 实施前**: `ibus_chmem.h` 须先把 `instr` 写统一 key `INSTRUCTION` (小改)
2. **R1 实施**: `csr_chmem.h` execute-LATE 闭包 `bits<31,20>(INSTRUCTION)` 重提取 csr_addr
3. **R3 实施**: `bundles/csr_bundles.h` 新建 + `IntAlu_chmem` 改 4 行 + `csr_chmem.h` 写 CSR_READ_VALUE
4. **R2 实施**: `csr_keys<T>::TRAP_PENDING / TRAP_TARGET` 新增 + `csr_chmem.h` branch-LATE 闭包 + `ibus_chmem.h` branch-NORMAL 加 select 覆写

**风险**: R2 须 IBus 改 2 处 (一处为 R1 改 INSTRUCTION, 一处为 R2 改 NORMAL select), 与 mmu-chmem-pipeline-integration C.2 改同一文件 —— 需协调合并。

## 5. 决策权衡总结

| 决策 | 选 | 不选 | 关键 tradeoff |
|------|----|----|------|
| R1: csr_addr 信号源 | A (重提取) | B (扩 DecodedInst) | 范围最小, 须 IBus 改 1 行 |
| R2: PC 重定向 | 自建 CtrlLink + 新 key + IBus NORMAL 覆写 | CSR 改注册位置 (破 ADR-048) | 维持 canonical ordering, 须 IBus 改 1 行 + 与 bp-btb 协调 |
| R3: RD_DATA 通路 | X (新 CSR_READ_VALUE key + IntAlu 4 行) | Y (改 CSR 位置) / Z (改 IntAlu 写) | canonical ordering 维持, 须 IntAlu 改 4 行 |

**总代码改动估算**: csr_chmem.h ~250 LOC (proposal 估时 2 周) + ibus_chmem.h 2 行 + IntAlu 4 行 + bundles/csr_bundles.h ~30 LOC + tests/cpu/test_csr_chmem.cpp ~150 LOC = ~440 LOC (vs 单纯 csr_chmem.h 250 LOC, +76% 但分布合理)。

## 6. 已知未决策项 (须开工前再确认)

- **Q1**: `mcycle` 自由计数 (每周期自增 ch_reg) —— tasks.md:52 一句"特殊处理"过简, 需详化 (ch_reg DAG 节点如何 free-running, 需 `at_stage("fetch", ALWAYS, [...])` 闭包 + carry 自增逻辑)
- **Q2**: mscratch 是否实现 —— proposal §What Changes 写"可选", tasks.md 未列; 建议**不实现**, 减少 scope
- **Q3**: 测试 baseline 数字 `89/93` (含 4 已知 cpu_sim FAIL) 是否接受作为 v0.11.0 launch 标准 —— tasks.md:65 写"`[cpu-integration] 81/81`" 已过期
- **Q4**: `wave5-bp-btb` 幽灵依赖 —— 本 change 显式声明 `trap > bp` 优先级, 后续 bp-btb change 启动时需协调 (3-6 个月后问题, 可不阻塞本 change)

## 7. 实施风险矩阵 (per Oracle 7 项)

| 风险 | criticality | 决策后缓解 |
|------|------|------|
| R1 [Critical] | 高 | 方案 A + IBus 1 行改 |
| R2 [Critical] | 高 | 自建 CtrlLink + IBus NORMAL 1 行改 |
| R3 [Critical] | 高 | 方案 X + IntAlu 4 行改 |
| R4 [High] A.3 `if(ch_bool)` | 中 | select/write-enable mux 重写 |
| R5 [Medium] ch_state_machine 生产率 | 中 | mmu_ptw_chmem.h 先例在, 估时 2 周略乐观 |
| R6 [Medium] mcycle 自由计数 | 中 | Q1 须详化 |
| R7 [Low] gate 假阴性 (csr_chmem.h 路径漏洞) | 低 | 把 csr_chmem.h 从 `arch/riscv/` 移到 `ip/cpu/plugins/`, 走 `check_plugin_portability.sh` 强制检查 |

## 8. 与 mmu-chmem-pipeline-integration 协调项

- 同一文件 `ibus_chmem.h` 双方都要改 (mmu C.2 bare byte-identical + csr R1+R2 INSTRUCTION/branch NORMAL 覆写) —— 须同 PR 或显式声明合并顺序
- 同一 stall 通路: mmu C.1 用 CtrlLink, csr R2 自建 CtrlLink —— 须验证 `PipeBuilder` 支持同 stage 多 CtrlLink 合并 (per `mmu_exception_handler_chmem.h:67-70` 注释自证支持, 但未实测)
- 同一 factory 7→8 plugin 注册清单 (`cpu_factory_chmem.h:50-58` 改 7→8)

## 9. Out-of-Scope (确认)

- PMP (Physical Memory Protection) — Phase 5+
- Smode / S-mode CSR — Phase 5+
- 64-bit CSR (mcycleh 等) — RV32 PoC 不需要
- FPU CSR — 需 F ext
- Debug CSR — 需 Debug spec
- Cache CSR — 需 cache-coherent
- 270+ 全 CSR — D1 收窄 8 个, 够 MRET + interrupt acknowledge 闭环

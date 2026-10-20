# Design — chmem-mext-comb-datapath

> **Change**: `chmem-mext-comb-datapath`
> **Created**: 2026-10-20
> **Decision basis**: oracle 咨询 (2026-10-20) + PoC `tests/framework/test_chmem_arith_poc.cpp`

## Context

CH_MEM 数据通路**全组合**（`cpu_factory_chmem.h:431-435`：无 stage pipeline 寄存器；reg 在 cycle N writeback 写、cycle N+1 decode 读）。PC 是唯一时序寄存器。execute stage 的 `RD_DATA` 单写者链：`IntAlu NORMAL → CSR LATE → JAL/JALR link LATE`。

RV32M 集成有两条候选路径。

## D1 — 路径 A（组合式）vs 路径 B（FSM 注册）

**决策：路径 A。**

| 维度 | 路径 A（组合式 IntAlu） | 路径 B（注册 MulDivFsmPlugin） |
|---|---|---|
| 与数据通路结构 | 同构（全组合，1 拍出结果） | 全组合通路里嵌多周期 FSM，需 stall |
| 目标达成 | 直接让 tohost 转绿 | 需先解 CtrlLink API gap |
| 前置依赖 | 无 | `hazard_chmem.h:163-167` 记录的 CtrlLink gap（`halt_when` 只接 ch_bool，需扩展 `ch_reg<ch_bool>` 句柄） |
| 改动面 | 1 文件（int_alu_chmem.h） | int_alu + factory 注册 + stub 真实现 + stall 接线 |
| blast radius | 单写者内加 select 层 | 引入第 4 个 RD_DATA 写者 + 优先级耦合 |

**理由**：
1. 失败测试 `test_cpu_chmem_vendored_elf.cpp` 只断言 `tohost==1`，无 cycle 断言。cycle parity 由 TLM 侧 radix-2 FSM 承担（`mul_div_fsm.h` + CSV baseline），不是 CH_MEM binary 的验收维度。
2. 路径 B 被未修的 CtrlLink API gap 硬阻塞；在 IP 修复 change 里夹带框架级 API 扩展 = 范围失控。
3. `int_alu_chmem.h` 本就是 select 树风格，M-op 只是再加 8 层 mux，与 ADR-040 v2.0 / D4 完全兼容（无 tick、无运行期 `if(ch_bool)`、无状态机）。
4. PoC Part D 已用同一套 `sext`/`zext`/`select` 原语实测 8/8 PASS，迁移是机械性的。

**路径 B 处置**：降级为独立 follow-up change `chmem-mext-fsm-cycle-parity`，前置 = `ctrl-link-chmem-reg-handle`（解 CtrlLink gap）+ stub 占位替换为真 ch_reg 迭代（PoC Part C 已证可行）。

## D2 — M-op 判别策略（防 base ALU 误捕）

现状审计（`int_alu_chmem.h:145-169`）：`is_add`/`is_sub`/`is_srl`/`is_sra` 有 funct7 gate；但 `is_sll`(f3=1)/`is_slt`(2)/`is_sltu`(3)/`is_xor`(4)/`is_or`(6)/`is_and`(7) **无 gate**。M-op funct3 与这些重叠（MULH f3=1 撞 SLL、DIV f3=4 撞 XOR、REMU f3=7 撞 AND），会误捕。

**决策：双保险**：
1. 新 gate `auto is_mext = is_op && (funct7 == 0x01)`；8 个 M-op = `is_mext && f3_N`。
2. 8 层 select **追加在现有 11 层树之后** → `select` 后写覆盖前写 → M-op 无条件赢过 base 误捕。
3. （可选加固）给 6 个无 gate 的 base R-type op 的 `is_op` 侧补 `f7_00`，使 base 与 M 判别互斥，不依赖 select 顺序。**注意**：只对 `is_op` 侧加，不能加到 `is_opimm`（I-type bits[31:25] 是立即数，见 `int_alu_chmem.h:148-150` 教训）。

## D3 — RD_DATA 写者链不受影响

M-op 结果写入同一 `RD_DATA`，经下游 select 优先级链：CSR LATE 覆写（`int_alu_chmem.h:272-289`）、JAL/JALR link 覆写（`cpu_factory_chmem.h:400-408`）。M-op opcode=0x33 与 JAL(0x6F)/JALR(0x67) 不交叉；CSR LATE 按 CSR opcode 判别，不交叉。**无新增写者，无优先级变更。**

## D4 — 符号运算（CppHDL 无 ch_int）

MULH/MULHSU/DIV/REM 用 sign-magnitude：
- `neg32(x) = (~x) + 1`
- `neg_flag(x) = bits<31,31>(x) == 1`
- `abs32(x) = select(neg_flag(x), neg32(x), x)`
- MULH = `bits<63,32>(sext<64>(a) * sext<64>(b))`
- MULHSU = `bits<63,32>(sext<64>(a) * zext<64>(b))`
- MULHU = `bits<63,32>(a * b)`
- DIV = `select(sign, neg32(|a|/|b|), |a|/|b|)`，除零 → 全 1
- REM = `select(neg_flag(a), neg32(|a|%|b|), |a|%|b|)`，除零 → 被除数

以上均为 PoC Part D 已验证的精确表达式，逐字移植。

## D5 — Stub 与 factory 处置

| 对象 | 处置 | 理由 |
|---|---|---|
| `mul_div_fsm_chmem.h` | 保留不改（仅加注释） | 它是 `test_chmem_multi_cycle_fsm.cpp` 的 DSL 机制 fixture；真算术属 cycle-parity change scope |
| `cpu_factory_chmem.h` 注册表 | 不改 | stub 无 at_stage 闭包、输出不进 RD_DATA，注册徒增死节点与误导 |

## Risks

| 风险 | 等级 | 缓解 |
|---|---|---|
| 组合除法 emit Verilog `/`、`%`，真实综合 = 巨型组合除法器 | 中（仅上 FPGA/ASIC 叙事；当前 Verilator-only 为低） | change 文档声明"组合 M-op = 仿真正确性基线；可综合版 = cycle-parity change 的 FSM 除法器" |
| MULH 64-bit 中间宽度理解错会静默截断 | 中 | 直接移植 PoC 已 PASS 的表达式，不重新推导；PoC 已覆盖 0xFFFFFFFF 边界 |
| INT_MIN÷-1 / 除零特例 | 中 | PoC Part D 已含特例分支，移植时保留 |
| DAG node_count 快照 drift | 低 | 实施第一步 grep 全部 `node_count` 断言，基线更新列为显式 task |
| 漏加 gate 仅靠 select 顺序，未来重排会静默破 M-op | 低 | 双保险 + 8 ELF 端到端永久回归锚 |

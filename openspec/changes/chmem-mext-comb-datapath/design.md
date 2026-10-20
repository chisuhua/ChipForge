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

**显式排除项（本次不实现，且无中间态可退）**：

- **多周期 FSM**（路径 B）：见上，前置 = CtrlLink API gap。
- **1-cycle 迭代 FSM 折中**：CH_MEM 数据通路**全组合**（`cpu_factory_chmem.h:431-435` 无 stage pipeline 寄存器），execute 当拍必须产出 `RD_DATA`；N 拍 FSM 的结果到达时该指令早已流过写回点，而 CH_MEM 通路**没有 stall 机制**（无任何东西能把晚到的结果写回正确的 rd）。因此"组合（1 拍）"与"多周期（N 拍）"之间**不存在中间态** —— 要么全组合，要么引入 stall / 写回仲裁机制（属框架级改动，超出本 change scope）。

> **术语统一**：本 design 的"路径 A = 组合式 / 路径 B = FSM"与 `AGENTS.md:215` 的 "path-B (ch_uint 算术) / fallback path-A (32 层 select 真算术)"**命名相反**。跨文档引用一律用无歧义表述：**"组合式路径"（本 change）** 与 **"FSM 路径"（follow-up `chmem-mext-fsm-cycle-parity`）**。

## D2 — M-op 判别策略（防 base ALU 误捕）

现状审计（`int_alu_chmem.h:145-169`）：`is_add`/`is_sub`/`is_srl`/`is_sra` 有 funct7 gate；但 `is_sll`(f3=1)/`is_slt`(2)/`is_sltu`(3)/`is_xor`(4)/`is_or`(6)/`is_and`(7) **无 gate**。M-op funct3 与这些重叠（MULH f3=1 撞 SLL、DIV f3=4 撞 XOR、REMU f3=7 撞 AND），会误捕。

**决策：双保险**：
1. 新 gate `auto is_mext = is_op && (funct7 == 0x01)`；8 个 M-op = `is_mext && f3_N`。
2. 8 层 select **追加在现有 11 层树之后** → `select` 后写覆盖前写 → M-op 无条件赢过 base 误捕。
3. **必选加固**：给 6 个无 gate 的 base R-type op 的 `is_op` 侧补 `f7_00`，使 base 与 M 判别互斥，不依赖 select 顺序。**注意**：只对 `is_op` 侧加，不能加到 `is_opimm`（I-type bits[31:25] 是立即数，见 `int_alu_chmem.h:148-150` 教训）。

## D3 — RD_DATA 写者链不受影响

M-op 结果写入同一 `RD_DATA`，经下游 select 优先级链：CSR LATE 覆写（`int_alu_chmem.h:272-289`）、JAL/JALR link 覆写（`cpu_factory_chmem.h:400-408`）。M-op opcode=0x33 与 JAL(0x6F)/JALR(0x67) 不交叉；CSR LATE 按 CSR opcode 判别，不交叉。**无新增写者，无优先级变更。**

## D4 — 符号运算（CppHDL 无 ch_int）

MULH/MULHSU/DIV/REM 用 sign-magnitude：

- `neg32(x) = (~x) + 1`
- `neg_flag(x) = bits<31,31>(x) == 1`
- `abs32(x) = select(neg_flag(x), neg32(x), x)`

### 8 条 M-op 完整表达式

| M-op | 表达式 | 备注 |
|---|---|---|
| MUL | `bits<31,0>(a * b)` | 无符号 64-bit 积取低 32 位 |
| MULHU | `bits<63,32>(a * b)` | 无符号 64-bit 积取高 32 位 |
| MULH | `bits<63,32>(sext<64>(a) * sext<64>(b))` | 有符号积 |
| MULHSU | `bits<63,32>(sext<64>(a) * zext<64>(b))` | 有符号 × 无符号 |
| DIV | `select(b == 0, all_ones, select(sign, neg32(mag), mag))` | `sign = neg_flag(a) ^ neg_flag(b)`，`mag = abs32(a) / b_safe_mag` |
| DIVU | `select(b == 0, all_ones, a / b_safe)` | 无符号除 |
| REM | `select(b == 0, a, select(neg_flag(a), neg32(mag), mag))` | `mag = abs32(a) % b_safe_mag`，符号跟随被除数 |
| REMU | `select(b == 0, a, a % b_safe)` | 无符号取模 |

### 除零 safe divisor（必选）

所有除/模节点**必须显式 guard**，不能依赖 CppHDL 默认行为（`CppHDL/include/ast/instr_op_arith.h:52-73`：除零写 stderr 并返回 0）：

- 无符号：`b_safe = select(b == 0, 1, b)` → 除/模吃 `b_safe`，输出再 `select(b == 0, …)` 覆盖特例值。
- 有符号：`b_safe_mag = select(abs32(b) == 0, 1, abs32(b))` → `mag = abs32(a) / b_safe_mag`；输出 `select(b == 0, all_ones / a, …)`。

**为什么必须 guard**：vendored ELF 的 `divu` / `remu` **test_8/9/10** 明确以 `rs2 == 0` 驱动（2026-10-20 反汇编核验，见 `specs/chmem-mext-datapath/spec.md` 的特例覆盖映射表）；PoC 的 `chmem_arith_poc_mext_divu`（`tests/framework/test_chmem_arith_poc.cpp:272-280`）/ `_remu`（`:282-290`）是**裸 `a/b` / `a%b`、无 guard**，照抄会得到 0 而非 0xFFFFFFFF → 8 ELF 继续 FAIL。

**边界自洽性**（幅值用无符号 32-bit 表示，`abs32(INT_MIN) = 0x80000000` 装得下，无需 33-bit）：

| 输入 | RISC-V 期望 | 本表达式 | |
|---|---|---|---|
| `div(INT_MIN, -1)` | `0x80000000` | `mag = 0x80000000 / 1`，`sign = 1^1 = 0` → `0x80000000` | ✅ |
| `div(x, 0)` | `0xFFFFFFFF` | 外层 `select(b == 0, all_ones, …)` | ✅ |
| `rem(INT_MIN, -1)` | `0` | `mag = 0x80000000 % 1 = 0` → `neg32(0) = 0` | ✅ |
| `rem(INT_MIN, 0)` | `0x80000000`（被除数） | 外层 `select(b == 0, a, …)` | ✅ |
| `divu(x, 0)` | `0xFFFFFFFF` | 显式 guard | ✅ |
| `remu(x, 0)` | `x`（被除数） | 显式 guard | ✅ |

> **宽度陷阱**：`neg32(0x80000000)` 必须仍为 `0x80000000`。`ch_uint` 的 `operator+` 结果宽度 = `max(lhs, rhs)`（32），但上游 `Neg` op 注释（`CppHDL/include/ast/instr_op_arith.h:48-49`）自陈 `~src + 1` 会走 `add_width = max + 1`。因此 PoC 必须补一条断言钉死 `neg32(0x80000000) == 0x80000000`。

### 与 PoC 的关系（落地顺序的依据）

PoC Part D（`tests/framework/test_chmem_arith_poc.cpp:225-320`）已验证 8 条 M-op 的**正常路径**语义（8/8 PASS），但：

- DIV / REM 的除零与 INT_MIN 分支**从未被驱动**（只跑过 `-14 / 4`）；
- DIVU / REMU **无除零 guard**。

故本 change 的落地顺序必须是"**先在 PoC 把表达式定稿（含 safe divisor + 5 个边界 case 全绿），再移植到 `int_alu_chmem.h`**"，而非逐字照抄 PoC。

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
| INT_MIN÷-1 / 除零特例 | 中 | PoC Part D 的特例分支**从未被实测**（DIV/REM 只跑 `-14 / 4`）；缓解 = tasks §1 先在 PoC 补 5 个边界 case 定稿（含 safe divisor），再移植；vendored ELF 的 div/rem/divu/remu test_7~test_10 构成 e2e 回归锚 |
| DAG 规模变化不体现在 node_count | 低 | `node_count()` = 按 stage 去重的 PipeNode **拓扑计数**（`include/cf/plugin/pipe_builder.h:283`），**不是** lnode/DAG 计数，select 树 11→19 层不改变该值 → **无快照需更新**；RV32I 语义不变由 38 rv32ui ELF 端到端锚定 |
| 漏加 gate 仅靠 select 顺序，未来重排会静默破 M-op | 低 | 双保险 + 8 ELF 端到端永久回归锚 |

---
id: chmem-mext-fsm-cycle-parity
status: draft
initiative: wave5-isa-coverage-and-bp
priority: P2
phase: phase-4
theme: phase-4
version_target: v0.11.0
manual_deps:
  - ctrl-link-chmem-reg-handle (CtrlLink `halt_when` 需接受 `ch_reg<ch_bool>` 句柄 — 框架级 API 扩展，尚未立项)
review_by: 2026-11-15
---

# chmem-mext-fsm-cycle-parity — CH_MEM 路径 RV32UM 多周期 FSM + cycle parity + 可综合除法器

> **Successor to** `openspec/changes/archive/2026-10-20-chmem-mext-comb-datapath/`（组合式 M 扩展已落地，8/8 rv32um ELF 真 PASS）
> **Blocked on** `ctrl-link-chmem-reg-handle`（CtrlLink API gap，见 `ip/cpu/plugins/hazard_chmem.h:163-167`）
> **Naming note**：本 change 的"FSM 路径"= `chmem-mext-comb-datapath` design.md 里的"路径 B"（该 design 曾用 "path-A/path-B" 指代组合式/FSM，与 AGENTS.md 早期措辞相反 —— 跨文档一律用无歧义表述"组合式路径"/"FSM 路径"）。

**主题**: RV32UM M 扩展真实 radix-2 覆盖 (rv32um 8/8)
**优先级**: P2

## 架构依据

### CH_MEM M 扩展现状 = 组合式（1 拍），与 TLM 不同构

`chmem-mext-comb-datapath`（archived 2026-10-20）用组合式 `ch_uint` 算术在 `ip/cpu/arch/riscv/int_alu_chmem.h` 实现了全部 8 条 RV32M op（select 树 11 → 19 层），8 个 rv32um ELF 在 CH_MEM simulator 与 Verilator 双路径全部 `tohost == 1`（真 PASS，非 mask 假绿）。

但 CH_MEM 侧 M-op **1 拍出结果**，而 TLM 侧走真 radix-2 迭代（`ip/cpu/arch/riscv/mul_div_fsm.h:144-148` — MUL=1c / MULH=3c / DIV=33c）：

| ELF | CH_MEM 组合式实测 | TLM radix-2 baseline |
|---|---|---|
| mul / mulh / mulhsu / mulhu | 472 | 455 / 504 / 504 / 504 |
| div / divu / rem / remu | 94 / 95 / 94 / 94 | 380 / 381 / 380 / 380 |

两者**不同构、不可比**。原归档 spec 的 "±5% 或 +50 cycles" 判据已在 `chmem-mext-comb-datapath` 的两个 delta spec（`openspec/specs/{chmem-rv32um-coverage,verilator-rv32um-coverage}/spec.md`）中改判为"**不适用** + 移交本 change" —— 该 MUST 目前处于悬空状态，需要本 change 兑现或重新定义。

### 组合除法不可综合

组合式 M-op 在 Verilog 里直译为 `/` 与 `%`（`CppHDL` codegen），真实综合 = 巨型组合除法器，时序不可行。当前 CH_MEM 路径仅用于仿真/验证（Verilator），可综合版本需要 32 级恢复除法（restoring division）迭代网络。

### 恢复多周期 M-op 缺两块基础设施

1. **CtrlLink 句柄扩展（框架级）**：`ip/cpu/plugins/hazard_chmem.h:163-167` 显式记录 —— `CtrlLink::halt_when()` 只接受 `ch_bool` 信号句柄，**反复调用会累积到 OR 网无法 toggle**；要表达"多周期 busy 状态"必须能接受 `ch_reg<ch_bool>` 句柄（instance state 在 elaboration 期形成持续信号）。该解法已记为独立 change `ctrl-link-chmem-reg-handle`，**本 change 硬依赖它**。
2. **CH_MEM 数据通路无 stage 寄存器**：`ip/cpu/cpu_factory_chmem.h:431-435` 明确 —— 数据通路全组合（reg 在 cycle N writeback 写、cycle N+1 decode 读），PC 是唯一时序寄存器。N 拍 FSM 的结果到达时该指令已流过写回点，**且无 stall 机制**能把晚到的结果写回正确的 rd。因此"组合（1 拍）"与"多周期（N 拍）"之间**不存在中间态**：要么全组合，要么引入 stall + 写回仲裁机制。

### 关联 ADR / Spec

- **ADR-046 v2.0** —— 多周期协议引擎豁免（`chlib::ch_state_machine` DSL）本 change 的机制依据
- **ADR-040 v2.0** —— CH_MEM 是移植正道；`array_store` 双缓冲 commit swap
- **ADR-082** —— CapabilityTable negotiate 集成
- `openspec/specs/chmem-rv32um-coverage/spec.md` —— 本 change 兑现其"cycle 判据不适用 → 移交"的悬空 MUST
- `openspec/specs/verilator-rv32um-coverage/spec.md` —— 同上

## 范围

### In Scope

- **CtrlLink 句柄扩展**（独立 change `ctrl-link-chmem-reg-handle`，本 change 消费）：`halt_when` 接受 `ch_reg<ch_bool>` 句柄
- **`mul_div_fsm_chmem.h` 占位 → 真算法**：把 `result_reg_`/`busy_cycles_reg_` 的 `ch_literal<12>`/`<3>`/`<33>` 换成真 `ch_reg` radix-2 迭代（PoC Part C 已证 ch_reg + ch_uint 算术 + select 组合在 CH_MEM interpreter 下可用）
- **CH_MEM M-op stall + 写回仲裁**：FSM busy 期间冻结 fetch/decode；结果在 WRITE_BACK 拍驱动 `RD_DATA` 并写回正确 rd
- **恢复 cycle parity 判据**：8 个 rv32um ELF 的 CH_MEM cycle 数与 TLM baseline 对齐（±5% 或 +50c 取大）
- **可综合除法器**：32 级恢复除法网络替代 Verilog `/`、`%`
- **组合式路径的去留决策**：明确"组合式 = 仿真正确性基线 / 组合式 = 可综合 fallback"二选一，或并存（组合式供快速验证 + FSM 供可综合）

### Out of Scope (严禁触碰)

- **不改 TLM 路径**：`ip/cpu/arch/riscv/mul_div_fsm.h`、`ip/cpu/cpu_factory.h` 注册表
- **不改 RV32I base op 语义**：`int_alu_chmem.h` 的 11 层 base 树与 6 个 R-type op 的 `f7_00` 加固保持不变
- **不动 MMU / CSR / Branch 子系统**
- **不改 CtrlLink 之外的框架 API**（除 `ctrl-link-chmem-reg-handle` 已含范围）

## Why

1. **spec 悬空**：两个归档 spec 的 cycle 判据被显式改判为"不适用 + 移交本 change"，若本 change 长期不启动，spec 将永久处于"有 MUST 但不可满足"状态。
2. **可综合性缺口**：组合式 `/`、`%` 让 CH_MEM 路径无法上 FPGA/ASIC。v1.2.0（Linux-on-FPGA）与 v1.3.0（芯片选型）路线图节点需要可综合的 M 扩展。
3. **TLM/CH_MEM 双路径行为分歧**：TLM 已真 radix-2 且有 CSV baseline（`soc/cpu/docs/dse/rv32um-baseline-matrix.csv`），CH_MEM 组合式与之不可比 → 双后端一致性无锚点。
4. **DSL 机制已就绪但未被真实使用**：`mul_div_fsm_chmem.h` 的 `ch_state_machine` DSL 至今只是"机制 fixture"（写死占位），从未承载真实算术；本 change 是它的首次真实投产。

## What Changes

### 1. 前置：`ctrl-link-chmem-reg-handle`（框架级，独立 change）

- `cf::plugin::CtrlLink::halt_when()` 增加接受 `ch_reg<ch_bool>` 句柄的重载 → 让"多周期 busy 状态"可作为持续信号进入 stall OR 树
- **必须先 archive**，本 change 才启动（P0 依赖门禁）

### 2. `ip/cpu/plugins/mul_div_fsm_chmem.h`：占位 → 真 radix-2

- `result_reg_` / `busy_cycles_reg_` 的 `ch_literal` 占位替换为真 `ch_reg` 迭代（移位-加法部分积累积 / 32 级恢复除法）
- 复用 PoC Part C 已验证的原语：`ch_reg` 迭代累加、shift-add 乘法、`select` 条件累加
- 保持 D4 合规：多周期引擎走 ADR-046 豁免（`CF_PLUGIN_USE_FSM_EXEMPT`），无业务 `tick()`

### 3. `ip/cpu/cpu_factory_chmem.h`：注册 + stall 接线

- 注册 `MulDivFsmPlugin`（CH_MEM DSL 版），位置在 `IntAlu` 之后
- 打通 `RD_DATA` 写者链：IntAlu NORMAL → M-op FSM WRITE_BACK → CSR LATE → JAL/JALR link LATE（**新增一个写者 → 必须显式定义优先级，否则回到 `int_alu_chmem.h` 引入 M-op 前的同类风险**）
- 接入 `CtrlLink::halt_when(busy_reg)` 冻结 decode/fetch

### 4. `ip/cpu/arch/riscv/int_alu_chmem.h`：组合式路径的处置

- 二选一：(a) 保留组合式作快速验证基线，FSM 路径在其之后覆写；(b) 用 FSM 路径替换组合式 select 层
- **无论哪种，RV32I 路径必须逐位不变**（`is_mext` gate 保持）

### 5. Verilog 可综合性

- FSM 除法器 emit 为 32 级移位-减法网络，Verilog 中**不得出现** `/`、`%` 组合算子（M-op 路径）
- 用 `ch::toVerilog` 输出目检 + Verilator 端到端复验

### 6. 测试与判据恢复

- 单元级：`ch_reg` 迭代算术的白盒语义锚（承接 PoC Part C/D 的表达式）
- 端到端：8 个 rv32um ELF 在 CH_MEM simulator + Verilator 双路径 `tohost == 1` **且** cycle 数与 TLM baseline 对齐
- 恢复归档 spec 的 cycle 判据（把两个 spec 的"不适用"改回可满足的判据）

## Capabilities

- **CH_MEM 多周期 M 扩展**（新增 capability `chmem-mext-fsm-datapath`）：CH_MEM 路径的 RV32M 走真 radix-2 迭代，cycle 数与 TLM 同构可比
- **CtrlLink 持续状态句柄**（新增 capability `ctrllink-continuous-handle`）：`halt_when` 支持 `ch_reg<ch_bool>`，多周期引擎可表达 busy 状态
- **可综合除法器**：CH_MEM M-op 在 Verilog 中不再依赖 `/`、`%`
- **双后端一致性锚**：TLM（radix-2 baseline CSV）与 CH_MEM（FSM）cycle 同构可比

## Impact

### 受影响文件

| 文件 | 动作 | 影响 |
|------|------|------|
| `include/cf/plugin/ctrl_link.h` | 改（前置 change） | `halt_when` 增加 `ch_reg<ch_bool>` 句柄重载 |
| `ip/cpu/plugins/mul_div_fsm_chmem.h` | 改 | 占位 → 真 radix-2 `ch_reg` 迭代；首次真实投产 |
| `ip/cpu/cpu_factory_chmem.h` | 改 | 注册 M-op plugin + stall 接线 + `RD_DATA` 写者链优先级 |
| `ip/cpu/arch/riscv/int_alu_chmem.h` | 改（决策依赖） | 组合式 8 层 M-op select 的保留/替换 |
| `tests/framework/test_chmem_arith_poc.cpp` | 改 | 白盒算术锚从组合式扩到 FSM 迭代式 |
| `tests/cpu/test_cpu_chmem_vendored_elf.cpp` | 改 | 恢复 cycle 判据断言 |
| `openspec/specs/{chmem-rv32um-coverage,verilator-rv32um-coverage}/spec.md` | 改（archive 时） | cycle 判据"不适用 → 可满足" |
| `openspec/specs/chmem-mext-fsm-datapath/spec.md` | 新增 | 多周期 M 扩展 capability |

### 风险

- **R-STALL（高）**：CH_MEM 无 stage 寄存器，引入 stall 需同时解决"冻结 PC"与"晚到结果写回正确 rd"两件事，写回仲裁出错会静默产生错误结果（不 crash、只是数值错）。缓解：写者链优先级显式化 + 端到端 ELF cycle + 数值双锚。
- **R-WRITER（高）**：`RD_DATA` 新增第 4 个写者（IntAlu / M-op FSM / CSR LATE / JAL link）→ 优先级耦合。缓解：单一写者图 + 每个写者的判别互斥证明。
- **R-CTRLLINK（高）**：硬依赖未立项的 `ctrl-link-chmem-reg-handle`；CtrlLink 是框架级 API，改动波及所有 CH_MEM 消费者。缓解：前置 change 独立评审 + 全 `[chmem]` 回归。
- **R-CYCLE（低）**：恢复 cycle 判据后，若 FSM 迭代拍数与 TLM 不完全同构，会出现"接近但不达标"。缓解：以 TLM CSV baseline 为唯一参照，早期先对齐单 ELF 再批量。
- **R-SYNTH（中）**：可综合除法器增加 Verilog 体积与延迟。缓解：32 级网络是本领域常规结构。

### 下游依赖

- `feat-rv32um-m-extension`（phase-4）—— 本 change 兑现其 CH_MEM 侧"真实 radix-2 覆盖"（当前 feat 内该 checklist 仍是 `- [ ]`）
- `feat-mfc-multi-cycle-fsm`（phase-2/4）—— 共享 `ch_state_machine` FSM 框架，ADR-082 negotiate 集成可复用
- `feat-phase-6d-rtl-verification`（phase-4）—— Verilator 端到端与可综合性验证

## 验收标准

- **V-1 周期正确性**：8 个 rv32um ELF 在 CH_MEM simulator 的 cycle 数与 TLM baseline（`soc/cpu/docs/dse/rv32um-baseline-matrix.csv`）对齐，满足 ±5% 相对容差或 +50 cycles 绝对容差（取大）
- **V-2 数值正确性**：8 个 rv32um ELF 在 CH_MEM simulator + Verilator 双路径 `tohost == 1`（严格判定，非 mask 产物）
- **V-3 可综合性**：M-op 路径生成的 Verilog 中不出现 `/` 与 `%` 组合算子（改为 32 级恢复除法网络）
- **V-4 零回归**：现有 `[chmem]` 66 case、`[verilator]` 48 ELF、`[mmu-verilator]` 3 case、`[arith-poc]` 24 case 与 TLM binary 481 case 均无新增 FAIL（`7stage_topology_from_config` pre-existing infra drift 除外）
- **V-5 spec 闭合**：两个归档 spec 的 cycle 判据从"不适用"改回可满足状态，`openspec validate --specs --strict` 对本 change 相关 spec 全 PASS
- **V-6 门禁**：`verify_adr` 0 fail / `verify_plugin_decision` 8/8 / `check_plugin_portability` 12/12 / `roadmap-r9-check` 0 fail
- **V-7 文档一致**：`AGENTS.md`、`CHANGELOG.md` 的 cycle 数与 `[chmem]`/`[verilator]` family 数字与实测一致

## Acceptance

- [ ] **AC-1** 前置 `ctrl-link-chmem-reg-handle` 已 archive（CtrlLink `halt_when` 支持 `ch_reg<ch_bool>` 句柄），且 `[chmem]` 全套无回归
- [ ] **AC-2** `mul_div_fsm_chmem.h` 的 `result_reg_`/`busy_cycles_reg_` 不再是 `ch_literal` 占位；`ch_reg` radix-2 迭代的白盒语义锚（`ch_reg` 迭代累加 / shift-add 乘法 / 32 级恢复除法）PASS
- [ ] **AC-3** `cpu_factory_chmem.h` 注册 M-op plugin 且 stall 接线生效；`RD_DATA` 写者链（IntAlu / M-op FSM / CSR LATE / JAL link）优先级显式化并在 spec 中记录
- [ ] **AC-4** 8 个 rv32um ELF 在 CH_MEM simulator 的 cycle 数与 TLM baseline 对齐（V-1 容差），且 `tohost == 1`
- [ ] **AC-5** 8 个 rv32um ELF 在 Verilator 路径 `tohost == 1`（严格判定）
- [ ] **AC-6** M-op 路径生成的 Verilog 不含 `/` 与 `%` 组合算子（V-3）
- [ ] **AC-7** 零回归：V-4 全部通过
- [ ] **AC-8** 架构门禁全 PASS（V-6）+ `openspec validate --specs --strict` 对相关 spec 全 PASS（V-5）
- [ ] **AC-9** `AGENTS.md` / `CHANGELOG.md` 数字与实测一致（V-7）；`feat-rv32um-m-extension.md` 的 CH_MEM cycle parity checklist 勾选
- [ ] **AC-10** `openspec change validate <name>` 0 error 并 archive

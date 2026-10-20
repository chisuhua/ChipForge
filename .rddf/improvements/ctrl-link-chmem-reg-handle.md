---
id: ctrl-link-chmem-reg-handle
status: draft
initiative: wave5-isa-coverage-and-bp
priority: P1
phase: phase-4
theme: phase-4
version_target: v0.11.0
manual_deps: []
review_by: 2026-11-15
---

# ctrl-link-chmem-reg-handle — CtrlLink 支持"有状态"（跨拍保持）halt/flush 条件

> **被谁阻塞**：`chmem-mext-fsm-cycle-parity`（CH_MEM M 扩展多周期 FSM 的硬前置 —— FSM busy 期间必须能冻结 fetch/decode）。
> **声明位置**：`ip/cpu/plugins/hazard_chmem.h:163-167`（"blocker 2: CtrlLink API gap"，建议 PR 名 `ctrl-link-chmem-reg-handle`）。
> **命名注记**："reg handle" 指 `ch_reg<ch_bool>` 句柄 —— 即"由寄存器支撑、可跨拍保持的条件"。

**主题**: MUL/DIV FSM 化 + ADR-082 negotiate 集成
**优先级**: P1

## 架构依据

### 现状：CtrlLink 只提供"组合条件"一种形态

`include/cf/plugin/ctrl_link.h` 的 CH_MEM 模式契约（文件头 12-25 行的双模语义表）：

| 维度 | TLM 模式 | CH_MEM 模式 |
|---|---|---|
| 条件类型 | `std::function<bool()>` 运行期谓词 | `ch::core::ch_bool` 硬件信号句柄 |
| 评估时机 | 每周期 `should_*()` 求值 | **elaboration 期**连成 OR 合并网络，Simulator 读当前值 |

`halt_when(ch_bool)` 把条件 push 进 `halt_conds_`；`halt_condition()` 用
`ch_bool::operator||`（→ `binary_bool_operation<logical_or_op>`，**建真 OR 门 DAG**）合并；
`PipeBuilder::elaborate`（`pipe_builder.h:425-445`）把合并结果接到 stage stall。

### 5 个具体缺口（2026-10-20 逐条核实）

| # | 缺口 | 证据 | 影响 |
|---|---|---|---|
| **G1** | 注释承诺的 `OR_combined_halt_conds()`（"返回 ch_bool，供 DAG 消费"）**根本未实现** | `ctrl_link.h:96` 提到；全仓无定义 | 想拿 OR 后信号接进 DAG 的调用方无路可走 |
| **G2** | `ch_reg<ch_bool>` **已经是** `ch_bool` 的派生类（`reg.h:32 class ch_reg : public T`）⇒ 寄存器支撑的条件**其实已经能通过 `halt_when` 传进去**（基类上行转换），但**意图被静默丢弃**，文档/类型都不体现"这是有状态条件" | `reg.h:32` + `ctrl_link.h:68` | 读者/调用方无法区分组合条件与跨拍保持条件；易写出"以为生效了"的代码 |
| **G3** | `should_halt()/should_throw()/should_flush()` 用 `c.to_bool()` 读 **C++ 侧缓冲**，**不是** Simulator 的 data_map 当前值 ⇒ 对寄存器支撑的条件**永远读到陈旧/初始值** | `ctrl_link.h:98-124` | 有状态条件的"读回"路径不可信 |
| **G4** | HazardPlugin 的 `mark_mmufault()` / `clear_mmufault()` 是 **C++ 实例状态 toggle**；而 OR 网是 **elaboration 一次成型** ⇒ 运行期 toggle **在结构上无法表达** | `hazard_chmem.h:149-173` | MMU 故障 halt 只能停在"骨架"状态（`mmufault_pending_` 读了但没接进 stall） |
| **G5** | 框架内 5 处用已 deprecated 的 `ch_bool::operator||`（`ctrl_link.h`×3 + `pipe_builder.h`×2），构建期刷 deprecation warning | `ctrl_link.h`/`pipe_builder.h` | 上游 `fa8b745` 已把 `&&`/`\|\|` 漂移收口，框架侧应跟用 `operator\|` |

### 关联 ADR / Spec

- **ADR-046 v2.0** —— 多周期协议引擎豁免（`ch_state_machine` DSL）；多周期引擎的 busy 状态必须能驱动 stall
- **ADR-040 v2.0** —— CH_MEM 是移植正道；`CtrlLink` 是 CH_MEM 侧唯一的 stall/flush 控制面
- **ADR-082** —— CapabilityTable negotiate（`CtrlLink` 与 plugin 能力协商的交汇点）
- 上游 CppHDL `openspec/specs/mem-read-timing/spec.md`（R3/R4）—— 展示了"后端语义差异必须显式契约化"的先例，本 change 同理

## 范围

### In Scope

- **A1 显式有状态条件 API**：`CtrlLink` 增加可区分"有状态"的重载/入口（候选：`halt_when_stateful(const ch_reg<ch_bool>&)`，或统一用一个 `ConditionHandle` 抽象），并文档化 OR 合并对寄存器条件的语义（DAG OR 门引用寄存器节点 ⇒ 随拍更新 ✓，**能力上已可用**，本次是让类型与文档显式）
- **A2 补 `OR_combined_halt_conds()`**：返回 OR 合并后的 `ch_bool`（DAG 节点），供调用方接进自己的 DAG（`ctrl_link.h:96` 已承诺）
- **A3 修正 `should_*()` 语义契约**：明确 `should_halt/should_throw/should_flush` 只对**组合**条件可靠；有状态条件必须走 A2 的 DAG 路径（或提供 simulator 感知的读取）
- **A4 HazardPlugin 的 mmufault 状态改造**：`mark/clear` 实例状态 → **信号驱动**（`ch_reg<ch_bool>`，其 `next` 由 DAG 内的故障检测逻辑驱动），消除"elaboration 一次成型 vs 运行期 toggle"的结构性矛盾
- **A5 清理 deprecated `||`**：框架内 5 处改 `operator|`（1-bit OR），消除构建期 deprecation warning

### Out of Scope (严禁触碰)

- **不改 `ch_state_machine` DSL**（`chlib/state_machine.h`）与任何业务 plugin 的 FSM 逻辑
- **不实现 `chmem-mext-fsm-cycle-parity`**（那是独立 change，本 change 只解除其硬前置）
- **不改 TLM 模式的 CtrlLink 语义**（`std::function<bool()>` 路径不动）
- **不实现 `bypass()`**（`bypass_active` 至今是 PoC stub，`ctrl_link.h:126-128` 自承"PoC: M3+ 实现"）—— 另立 change
- **不碰 CH_MEM dmem 读路径**（归 `chmem-dmem-read-latency`）

## Why

1. **阻塞 v0.11.0 的 CH_MEM 多周期 M 扩展**：`chmem-mext-fsm-cycle-parity` 需要"FSM busy 期间冻结 fetch/decode"。CH_MEM 数据通路全组合（`cpu_factory_chmem.h:431-435` 无 stage 寄存器），**没有 stall 就没法实现多周期** ⇒ 这是该 follow-up 的硬前置。
2. **G4 是结构性矛盾，不是缺一个 API**：只要 halt 条件是 C++ 实例状态，elaboration 一次成型的 OR 网就永远无法响应运行期变化 ⇒ 必须换模型（信号驱动），而不是补一个函数。
3. **G2/G3 是"看似可用实则不可靠"的陷阱**：`ch_reg<ch_bool>` 能传进 `halt_when`（能编译、能跑），但 `should_halt()` 对它返回陈旧值 ⇒ 未显式化的 API 会持续产出"静默失效"的代码。
4. **上游已有同类先例**：CppHDL 把后端语义差异写成显式 capability（`mem-read-timing` R3/R4）并配套 pin 测试 ⇒ 本项目的框架级语义差异也应显式契约化 + 配套测试。
5. **G5 是零风险清理**：5 处 deprecated 调用，构建期已刷 warning。

## What Changes

### 1. `include/cf/plugin/ctrl_link.h`（框架级 API）

- **1.1** 增加显式的有状态条件入口：新增 `halt_when_stateful(...)`（或对 `ch_reg<ch_bool>` 的专门重载），内部仍 push 进 `halt_conds_`（能力已具备，本次是**类型/意图显式化** + 文档）
- **1.2** 实现 `OR_combined_halt_conds()`：返回 OR 合并后的 `ch_bool`（与 `halt_condition()` 同源，但**明确返回 DAG 节点**供调用方消费）
- **1.3** `should_halt()/should_throw()/should_flush()` 的文档契约改写：明确"仅对组合条件可靠；有状态条件请用 A2 的 DAG 路径"，并对有状态条件给出明确的失败/告警语义（不静默返回陈旧值）
- **1.4** 5 处 deprecated `||` → `operator|`（`halt_condition()` / `throw_condition()` / `flush_condition()` + `pipe_builder.h` 的 stall/flush 合并）

### 2. `include/cf/plugin/pipe_builder.h`

- **2.1** stall/flush 合并处改 `operator|`
- **2.2** 若 A1 引入"有状态条件"标记，elaborate 期需保证该条件的 OR 门**引用寄存器节点**（而非其 C++ 侧缓冲快照）—— 加断言/注释防回归

### 3. `ip/cpu/plugins/hazard_chmem.h`

- **3.1** `mark_mmufault()` / `clear_mmufault()` 实例状态 → 信号驱动：用 `ch_reg<ch_bool>` 表达"fault pending"，其 `next` 由 DAG 内的故障检测逻辑驱动
- **3.2** 把 pending 信号 OR 进 `stall_ctrl_->halt_when(...)`（当前是"读了但没接"，见 `hazard_chmem.h:149-173` 的自述）
- **3.3** **Bare 路径 byte-identical 保护**：pending 默认 false，RAW stall 路径行为不变（对齐既有约束 `hazard_chmem.h:155` / `:171`）

### 4. 测试

- **4.1** 框架级单测：`CtrlLink` 接受有状态条件后，OR 合并结果**随寄存器更新**（真 DAG 行为，非 C++ 侧缓冲）
- **4.2** 框架级单测：`OR_combined_halt_conds()` 返回的节点可被消费方 DAG 引用
- **4.3** `should_*()` 对有状态条件的契约测试（明确断言其行为，而不是"碰巧不报错"）
- **4.4** HazardPlugin 侧：`[chmem]` 回归零新增 FAIL（`[chmem]` 73 case / 72 PASS / 1 failed 基线不变）

### 5. 文档 / spec

- **5.1** `openspec/specs/ctrllink-continuous-handle/spec.md`（新 capability）：CtrlLink SHALL 支持有状态条件 + `should_*` 的适用范围 SHALL 显式声明
- **5.2** `AGENTS.md`：`chmem-mext-fsm-cycle-parity` 的前置状态更新（"硬前置已解除"）
- **5.3** `CHANGELOG.md` 新增条目

## Acceptance

- [ ] **AC-1** `CtrlLink` 提供**显式的有状态条件入口**（`halt_when_stateful` 或 `ch_reg<ch_bool>` 专门重载），且 OR 合并结果**随寄存器逐拍更新**（真 DAG 行为，**不得**读 C++ 侧缓冲快照）—— 框架级单测覆盖
- [ ] **AC-2** `OR_combined_halt_conds()` 已实现并返回可被消费方 DAG 引用的 `ch_bool` 节点（补上 `ctrl_link.h:96` 注释里承诺但缺失的 API）
- [ ] **AC-3** `should_halt()/should_throw()/should_flush()` 的适用范围**显式声明**（仅组合条件可靠）；对有状态条件不得静默返回陈旧值 —— 契约测试覆盖
- [ ] **AC-4** 框架内 5 处 deprecated `ch_bool::operator||` 全部改为 `operator|`，构建期**零 deprecation warning**
- [ ] **AC-5** HazardPlugin 的 mmufault pending 改为**信号驱动**（`ch_reg<ch_bool>` + DAG 内故障检测逻辑驱动 next），并真正 OR 进 `stall_ctrl_->halt_when(...)`；**Bare 路径 byte-identical**（pending 默认 false，RAW stall 行为不变）
- [ ] **AC-6** 零回归：`[chmem]` 73 case / 72 PASS / 1 failed 基线不变；TLM 全量无新增 FAIL；`verify_plugin_decision` 8/8 + `check_plugin_portability` 12/12 + `verify_adr` 0 fail
- [ ] **AC-7** 新 capability `ctrllink-continuous-handle` 落 `openspec/specs/`，`openspec validate --specs --strict` PASS
- [ ] **AC-8** `AGENTS.md` 中 `chmem-mext-fsm-cycle-parity` 的硬前置状态更新为"已解除"；`CHANGELOG.md` 新增条目
- [ ] **AC-9** `openspec change validate` 0 error 并 archive

## Capabilities

- **CtrlLink 持续状态句柄**（新增 capability `ctrllink-continuous-handle`）：`halt_when` SHALL 接受跨拍保持的条件（`ch_reg<ch_bool>` 支撑），且 OR 合并结果 SHALL 随该条件的寄存器值逐拍更新
- **`should_*()` 适用范围显式化**：`should_halt/should_throw/should_flush` SHALL 明确只对组合条件可靠；调用方若注册了有状态条件，SHALL 被文档引导到 DAG 消费路径
- **MMU fault halt 可用**（依赖项）：HazardPlugin 的 mmufault pending 变为信号驱动后，CH_MEM 的 MMU fault 才能真正进入 stall 通路（`mmu-chmem-pipeline-integration` Phase C.1/C.5 的物理前置解除）

## Impact

### 受影响文件

| 文件 | 动作 | 影响 |
|------|------|------|
| `include/cf/plugin/ctrl_link.h` | 改 | 新增有状态条件 API + `OR_combined_halt_conds()` + `should_*` 契约 + 5 处 `\|\|`→`\|` |
| `include/cf/plugin/pipe_builder.h` | 改 | 2 处 `\|\|`→`\|`；有状态条件的 OR 门引用保证 |
| `ip/cpu/plugins/hazard_chmem.h` | 改 | mmufault pending 改信号驱动；接进 halt_when |
| `openspec/specs/ctrllink-continuous-handle/spec.md` | 新增 | 新 capability |
| `tests/framework/`（新文件） | 新增 | 框架级单测（4.1-4.3） |

### 风险

- **R-SEM（高）**：把"能跑但不可靠"的 `should_*()` 语义收紧，可能暴露**既有调用方**依赖了陈旧读取的行为 ⇒ 需 grep 全部 `should_halt/should_throw/should_flush` 调用点并逐一确认（TLM 路径不受影响，CH_MEM 路径需查）
- **R-SIG（高）**：mmufault pending 改信号驱动后，**故障检测逻辑必须在 DAG 内可表达**（不能是 C++ 实例状态）⇒ 若 MMU 的 fault 产生路径当前只在 C++ 侧，本 change 需先把它落成信号（可能引出对 `mmu_chmem.h` 的依赖 ⇒ 属 `mmu-chmem-pipeline-integration` Phase B 范围，**需确认可解耦**）
- **R-API（中）**：新增 API 是框架级扩展 ⇒ 影响所有 CH_MEM 消费者；需 `verify_plugin_decision` / `check_plugin_portability` 双门禁 + `[chmem]` 全套回归
- **R-SCOPE（中）**：`bypass()` 仍是 stub，本 change 明确不动 ⇒ 后续若需要 bypass 需另立 change（避免范围蔓延）

### 下游依赖

- `chmem-mext-fsm-cycle-parity`（本 change 是其**硬前置**，解除后该 follow-up 可启动）
- `mmu-chmem-pipeline-integration` Phase C.1/C.5（mmufault halt 接线；R-SIG 需与之对齐）
- `feat-mfc-multi-cycle-fsm`（多周期 FSM 的 stall 机制）

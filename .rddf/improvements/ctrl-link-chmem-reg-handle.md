---
id: ctrl-link-chmem-reg-handle
status: done
initiative: wave5-isa-coverage-and-bp
priority: P1
phase: phase-4
theme: phase-4
version_target: v0.11.0
manual_deps: []
review_by: 2026-11-15
---

# ctrl-link-chmem-reg-handle — CtrlLink 支持"有状态"（跨拍保持）halt/flush 条件

> **✅ 已完成 (2026-10-22)**：openspec change 已归档为 `openspec/changes/archive/2026-10-22-ctrl-link-chmem-reg-handle/`，capability `ctrllink-continuous-handle` 已落 `openspec/specs/ctrllink-continuous-handle/spec.md`（5 个 Requirement）。
> **实测**：框架级单测 9 case / 35 assertions 全 PASS；`[chmem]` family 73 case 不变；CH_MEM binary 总 case 105 → 114（= +9 新增）；TLM 481 case / 67135 assertions 与 baseline **逐 case 一致**；5 架构 gate 全绿。
> **A4 已移出 scope** → 归属 `mmu-chmem-phase-b-c-launch` 的 Phase C.5（理由见下「Out of Scope」）。

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
| **G2** | `ch_reg<ch_bool>` **已经是** `ch_bool` 的派生类（`class ch_reg : public T`）⇒ 寄存器支撑的条件**其实已经能通过 `halt_when` 传进去**（基类上行转换），但**意图被静默丢弃**，文档/类型都不体现"这是有状态条件" | **上游** `build/_deps/install/include/cpp-hdl/core/reg.h:32`（ChipForge 本地无 `include/cf/plugin/reg.h`）+ `ctrl_link.h:68` | 读者/调用方无法区分组合条件与跨拍保持条件；易写出"以为生效了"的代码 |
| **G3** | `should_halt()/should_throw()/should_flush()` 用 `c.to_bool()` 读 **C++ 侧缓冲**，**不是** Simulator 的 data_map 当前值 ⇒ 对寄存器支撑的条件**永远读到陈旧/初始值** | `ctrl_link.h:98-124` | 有状态条件的"读回"路径不可信 |
| **G4** | HazardPlugin 的 `mark_mmufault()` / `clear_mmufault()` 是 **C++ 实例状态 toggle**；而 OR 网是 **elaboration 一次成型** ⇒ 运行期 toggle **在结构上无法表达** | `hazard_chmem.h:149-173` | MMU 故障 halt 只能停在"骨架"状态（`mmufault_pending_` 读了但没接进 stall） |
| **G5** | 框架内用已 deprecated 的 `ch_bool::operator\|\|` —— **编译器权威统计 (2026-10-22, `g++-14 -fsyntax-only -Wall -Wextra` 逐 TU)**: 框架 `include/cf/plugin/` 内共 **4 处** = `ctrl_link.h:150` (`halt_condition`) + `ctrl_link.h:160` (`flush_condition`) + `pipe_builder.h:434` + `pipe_builder.h:435` (elaborate 期 stall/flush 聚合)。**原文 "ctrl_link.h×3 + pipe_builder.h×2" 不准确** (实测 ctrl_link.h ×2、pipe_builder.h ×2)。业务侧 `ip/**/*_chmem.h` 另有 ≥60 `\|\|` + ≥90 `&&` | 上游 `core/operators_logic.h:166-167` 的 `[[deprecated]]`; 框架内实测 4 处 | 上游 `fa8b745` 已把 `&&`/`\|\|` 漂移收口, 框架侧应跟用 `operator\|` |

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
- **A5 清理 deprecated `||`（范围收窄）**：**框架 `include/cf/plugin/` 内全部 4 处**改 `operator|`（1-bit OR）—— `ctrl_link.h:150` + `ctrl_link.h:160` + `pipe_builder.h:434` + `pipe_builder.h:435`（编译器验证：改动后含全部 13 个 framework 头的 probe TU 剩余 deprecated 站点 = **0**）。**业务侧 `ip/**/*_chmem.h` 的 ≥60 `\|\|` + ≥90 `&&`（`decode_chmem.h` 24+36 / `hazard_chmem.h` 17+~10 / `int_alu_chmem.h` 8+~30 / `dmem_chmem.h` 7+~10 / `csr_chmem.h` 4+~10 / `cpu_factory_chmem.h` 1 …）单独立独立 change，排在 `mmu-chmem-phase-b-c-launch` Phase B 之后** —— 强行执行会撑成 30+ 文件的回归工程（每文件需 `[chmem]` 73/73 + `[verilator]` 48 ELF 回归）
- **A6 API 关系定案（新增，防止再造第三版）**：新 `OR_combined_halt_conds()` 与既有 `halt_condition()`（`ctrl_link.h:148`）/ `flush_condition()`（`:157`）的关系必须显式定案 —— **裁决：新 API 为唯一实现，两个旧名改为 inline alias 转发**（保持既有调用点与 `openspec/specs/ctrllink-consumption-contract/spec.md` 的"不修改 CtrlLink 原 API" Requirement 不冲突：`halt_when/throw_when/flush_when/bypass` 4 个方法的签名与行为均不变，只做**增量 API**）

### Out of Scope (严禁触碰)

- **A4 HazardPlugin 的 mmufault 信号驱动改造 —— 移出本 change**（2026-10-22 双审裁决），改挂 `mmu-chmem-phase-b-c-launch` 的 **C.5**。理由：真实驱动源是 C.1/C.5 的 `CPU_EXCEPTION_CODE` 写入（`hazard_chmem.h:157-163` 自陈"统一 key 类型为 `ch_uint<8>` → MmuExceptionHandlerPlugin 写 `pl::CPU_EXCEPTION_CODE` → HazardPlugin 读"），Phase 1 交付的 `ch_reg<ch_bool>` **无 next 驱动源 ⇒ 恒 false 惰性骨架**；惰性窗口实为 Phase 1 → C.5 跨 ~5 周，不是 5 天 ⇒ 正是本 change 要消灭的 G3 同款陷阱（"看起来接上了但永不触发"）。本 change 只在 `hazard_chmem.h:149-173` 更新 gap 注释，指向新归属。**`chmem-mext-fsm-cycle-parity` 的硬前置只需 A1/A2 的 stateful halt 通道，不需要 A4 ⇒ 照常解除**
- **不改 `ch_state_machine` DSL**（`chlib/state_machine.h`）与任何业务 plugin 的 FSM 逻辑
- **不实现 `chmem-mext-fsm-cycle-parity`**（那是独立 change，本 change 只解除其硬前置）
- **不改 TLM 模式的 CtrlLink 语义**（`std::function<bool()>` 路径不动；TLM 消费方 `ibus.h:70` / `mul_div_fsm.h:300` / `hazard.h:217` 走 `#else` 分支，物理不受影响）
- **不实现 `bypass()`**（`bypass_active` 至今是 PoC stub，`ctrl_link.h:126-128` 自承"PoC: M3+ 实现"）—— 另立 change
- **不碰 CH_MEM dmem 读路径**（归 `chmem-dmem-read-latency`）
- **不清全仓 deprecated `&&`/`||`**（见 A5 范围收窄说明）

## Why

1. **阻塞 v0.11.0 的 CH_MEM 多周期 M 扩展**：`chmem-mext-fsm-cycle-parity` 需要"FSM busy 期间冻结 fetch/decode"。CH_MEM 数据通路全组合（`cpu_factory_chmem.h:431-435` 无 stage 寄存器），**没有 stall 就没法实现多周期** ⇒ 这是该 follow-up 的硬前置。
2. **G4 是结构性矛盾，不是缺一个 API**：只要 halt 条件是 C++ 实例状态，elaboration 一次成型的 OR 网就永远无法响应运行期变化 ⇒ 必须换模型（信号驱动），而不是补一个函数。**该模型改造（A4）已移出本 change 改挂 `mmu-chmem-phase-b-c-launch` C.5** —— 因为真实驱动源在 C.1/C.5（`CPU_EXCEPTION_CODE` 写入），本 change 阶段交付信号通道只会得到恒 false 惰性骨架。
3. **G2/G3 是"看似可用实则不可靠"的陷阱**：`ch_reg<ch_bool>` 能传进 `halt_when`（能编译、能跑），但 `should_halt()` 对它返回陈旧值 ⇒ 未显式化的 API 会持续产出"静默失效"的代码。
4. **上游已有同类先例**：CppHDL 把后端语义差异写成显式 capability（`mem-read-timing` R3/R4）并配套 pin 测试 ⇒ 本项目的框架级语义差异也应显式契约化 + 配套测试。
5. **G5 是零风险清理**：`ctrl_link.h` 2 处 deprecated 调用，构建期已刷 warning（**全仓 ~134 站点另立 change**）。

### 0. 前置 baseline（2026-10-22 双审新增）

在动任何代码前，先跑 `bash tools/run_chipforge_tests.sh --all` 记录 baseline，**必须含逐 case cycle 计数**（不只 PASS/FAIL）—— `[cpu-integration]` 存在 3 个 baseline-known FAIL + `node_count==17` infra drift 这类既有噪声，不先锁定基线会把历史 FAIL 误归因到本 change 头上。且 Phase 1 与后续 Phase B 都动 CH_MEM stall 通路（`should_*()` 语义 + `||`→`|`），**行为归因必须靠 commit 边界 + 逐 case cycle 快照**。

## What Changes

### 1. `include/cf/plugin/ctrl_link.h`（框架级 API）

- **1.1** 增加显式的有状态条件入口：新增 `halt_when_stateful(...)`（或对 `ch_reg<ch_bool>` 的专门重载），内部仍 push 进 `halt_conds_`（能力已具备，本次是**类型/意图显式化** + 文档）
- **1.2** 实现 `OR_combined_halt_conds()`：返回 OR 合并后的 `ch_bool`（**DAG 节点**，供调用方消费）。**A6 定案：这是唯一实现；既有 `halt_condition()`（`:148`）/ `flush_condition()`（`:157`）改为 inline alias 转发** —— 保持既有调用点不变，不新增第三版 API
- **1.3** `should_halt()/should_throw()/should_flush()` 的文档契约改写：明确"仅对组合条件可靠；有状态条件请用 A2 的 DAG 路径"，并对有状态条件给出明确的失败/告警语义（不静默返回陈旧值）
- **1.4** **A5（范围收窄）**：deprecated `||` → `operator|` —— 仅 `ctrl_link.h:150`（`halt_condition`）+ `:160`（`flush_condition`）2 处。`pipe_builder.h` 实测 0 处，不动

### 2. `include/cf/plugin/pipe_builder.h`

- **2.1** **确认** `:307` `should_halt()` / `:202` `should_throw()` 两处消费点按 A3 新契约的语义（**实测这是 CH_MEM 运行期 `should_*()` 的全部暴露面** —— CH_MEM 路径无运行期 `should_*()` 调用方，`mmu_exception_handler_chmem.h:89` 是注释掉的；TLM 侧 `#else` 分支物理不受影响）
- **2.2** 若 A1 引入"有状态条件"标记，elaborate 期需保证该条件的 OR 门**引用寄存器节点**（而非其 C++ 侧缓冲快照）—— 加断言/注释防回归

### 3. `ip/cpu/plugins/hazard_chmem.h`（**仅注释更新，无逻辑改动**）

- **3.1** 更新 `:149-173` 的 FP-4 gap 描述：指向新归属（`mmu-chmem-phase-b-c-launch` C.5），并记录"A4 已移出本 change"的原因（惰性骨架风险）
- **3.2** **不做**信号驱动改造（见 Out of Scope A4）—— 本 change **零惰性代码**（AC-5 反向约束）

### 4. 测试

- **4.1** 框架级单测：`CtrlLink` 接受有状态条件后，OR 合并结果**随寄存器更新**（真 DAG 行为，非 C++ 侧缓冲）。**测试形态要求：Simulator tick ≥2 拍，中途改变 `ch_reg` 值，观测 OR 输出随拍翻转** —— 单拍 elaboration 测试证明不了"逐拍更新"
- **4.2** 框架级单测：`OR_combined_halt_conds()` 返回的节点可被消费方 DAG 引用
- **4.3** `should_*()` 对有状态条件的契约测试（明确断言其行为，而不是"碰巧不报错"）
- **4.4** **新 CH_MEM 框架测试必须手动加入 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES`（`:118-145`）** —— 该 list 是**逐条手写**的（GLOB `:6` 只覆盖 TLM target），不加就**静默不编译不运行**
- **4.5** `[chmem]` 回归零新增 FAIL（73 case 基线，**逐 case cycle 计数比对**，不只 PASS/FAIL）

### 5. 文档 / spec

- **5.1** `openspec/specs/ctrllink-continuous-handle/spec.md`（新 capability）：CtrlLink SHALL 支持有状态条件 + `should_*` 的适用范围 SHALL 显式声明
- **5.2** `AGENTS.md`：`chmem-mext-fsm-cycle-parity` 的前置状态更新（"硬前置已解除"）
- **5.3** `CHANGELOG.md` 新增条目

## Acceptance

- [ ] **AC-0** 前置 baseline 已记录（`bash tools/run_chipforge_tests.sh --all`，**含逐 case cycle 计数**），可支撑后续"零回归"判定
- [ ] **AC-1** `CtrlLink` 提供**显式的有状态条件入口**（`halt_when_stateful` 或 `ch_reg<ch_bool>` 专门重载），且 OR 合并结果**随寄存器逐拍更新**（真 DAG 行为，**不得**读 C++ 侧缓冲快照）—— 框架级单测覆盖，**测试须 Simulator tick ≥2 拍并中途改变寄存器值观测 OR 输出翻转**
- [ ] **AC-2** `OR_combined_halt_conds()` 已实现并返回可被消费方 DAG 引用的 `ch_bool` 节点（补上 `ctrl_link.h:96` 注释里承诺但缺失的 API）；**A6 定案已落地：新 API 为唯一实现，既有 `halt_condition()`（`:148`）/ `flush_condition()`（`:157`）为 inline alias 转发，未新增第三版**
- [ ] **AC-3** `should_halt()/should_throw()/should_flush()` 的适用范围**显式声明**（仅组合条件可靠）；对有状态条件不得静默返回陈旧值 —— 契约测试覆盖；`pipe_builder.h:307` / `:202` 两处消费点语义已确认
- [ ] **AC-4** 框架 `include/cf/plugin/` 内 deprecated `ch_bool::operator\|\|` **全部 4 处**（`ctrl_link.h:150,160` + `pipe_builder.h:434,435`）改为 `operator|`，构建期框架范围内**零 deprecation warning**（验证方式：含全部 13 个 framework 头的 probe TU 编译，剩余 deprecated 站点 = 0）。**业务侧 `ip/**/*_chmem.h` 的 ≥60 `\|\|` + ≥90 `&&` 不在本 change AC 内**，须另立 change 并排在 `mmu-chmem-phase-b-c-launch` Phase B 之后
- [ ] **AC-5** **零惰性代码** —— 本 change **不引入**任何"已接上但永不触发"的信号通道（A4 已移出 scope 改挂 `mmu-chmem-phase-b-c-launch` C.5）；`hazard_chmem.h` 仅注释更新，**无逻辑改动**
- [ ] **AC-6** 零回归：`[chmem]` 73 case 基线不变（**逐 case cycle 计数比对**）；TLM 全量无新增 FAIL；`verify_plugin_decision` 8/8 + `check_plugin_portability` 12/12 + `verify_adr` 0 fail
- [ ] **AC-7** 新 capability `ctrllink-continuous-handle` 落 `openspec/specs/`，`openspec validate --specs --strict` PASS
- [ ] **AC-8** `AGENTS.md` 中 `chmem-mext-fsm-cycle-parity` 的硬前置状态更新为"已解除"；`CHANGELOG.md` 新增条目
- [ ] **AC-9** `openspec change validate` 0 error 并 archive
- [ ] **AC-10** 新 CH_MEM 框架测试**已手动加入 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES`（`:118-145`）**，且在 `./build/bin/chipforge_tests_chmem` 中可见可跑

## Capabilities

- **CtrlLink 持续状态句柄**（新增 capability `ctrllink-continuous-handle`）：`halt_when` SHALL 接受跨拍保持的条件（`ch_reg<ch_bool>` 支撑），且 OR 合并结果 SHALL 随该条件的寄存器值逐拍更新
- **`should_*()` 适用范围显式化**：`should_halt/should_throw/should_flush` SHALL 明确只对组合条件可靠；调用方若注册了有状态条件，SHALL 被文档引导到 DAG 消费路径
- **MMU fault halt 可用**（**依赖项，已移出本 change**）：HazardPlugin 的 mmufault pending 变为信号驱动后，CH_MEM 的 MMU fault 才能真正进入 stall 通路 —— 该改造归属 `mmu-chmem-phase-b-c-launch` 的 C.5（驱动源是 C.1/C.5 的 `CPU_EXCEPTION_CODE` 写入），本 change 只交付 stateful halt 通道（A1/A2）

## Impact

### 受影响文件

| 文件 | 动作 | 影响 |
|------|------|------|
| `include/cf/plugin/ctrl_link.h` | 改 | 新增有状态条件 API（A1）+ `OR_combined_halt_conds()` 唯一实现 + 旧 API inline alias 转发（A2/A6）+ `should_*` 契约（A3）+ 2 处 `\|\|`→`\|`（A5） |
| `include/cf/plugin/pipe_builder.h` | 改 | `:434` / `:435` 两处 `halt_condition()` / `flush_condition()` 聚合的 deprecated `\|\|` → `operator\|`；`:307` / `:202` 两处 `should_*()` 消费点按 A3 新契约确认语义 + 有状态条件的 OR 门引用保证 |
| `ip/cpu/plugins/hazard_chmem.h` | 改（仅注释） | 更新 `:149-173` FP-4 gap 描述，指向 C.5 新归属 |
| `openspec/specs/ctrllink-continuous-handle/spec.md` | 新增 | 新 capability |
| `tests/framework/`（新文件） | 新增 | 框架级单测（4.1-4.3） |
| `tests/CMakeLists.txt` | 改 | 新 CH_MEM 测试加入 `CHMEM_TEST_SOURCES`（`:118-145` 手工 list） |

### 风险

- **R-SEM（高）**：把"能跑但不可靠"的 `should_*()` 语义收紧，可能暴露**既有调用方**依赖了陈旧读取的行为 ⇒ **暴露面已定位（2026-10-22 实测）**：CH_MEM 运行期 `should_*()` 调用方**为零**（`mmu_exception_handler_chmem.h:89` 是注释掉的），全部暴露面只有 `pipe_builder.h:307` / `:202` 两处；TLM 消费方（`ibus.h:70` / `mul_div_fsm.h:300` / `hazard.h:217`）走 `#else` 分支**物理不受影响**
- **R-API（中）**：新增 API 是框架级扩展 ⇒ 影响所有 CH_MEM 消费者；需 `verify_plugin_decision` / `check_plugin_portability` 双门禁 + `[chmem]` 全套回归
- **R-SCOPE（中）**：`bypass()` 仍是 stub，本 change 明确不动 ⇒ 后续若需要 bypass 需另立 change（避免范围蔓延）
- **R-LAZY（高，2026-10-22 双审新增）**：若把 A4 留在本 change，`ch_reg<ch_bool>` 无 next 驱动源 ⇒ **恒 false 惰性骨架**，惰性窗口跨 Phase 1 → C.5 ~5 周 ⇒ 正是本 change 要消灭的 G3 同款陷阱 ⇒ **已裁决 A4 移出 scope**
- **R-ATTR（低）**：Phase 1 与后续 Phase B 都动 CH_MEM stall 通路 ⇒ 行为归因靠 commit 边界 + 逐 case cycle 快照（AC-0 / AC-6）

### 下游依赖

- `chmem-mext-fsm-cycle-parity`（本 change 是其**硬前置**，解除后该 follow-up 可启动；只需 A1/A2 的 stateful halt 通道，**不需要 A4**）
- `mmu-chmem-phase-b-c-launch` Phase C.5（**A4 的新归属** —— mmufault halt 接线；R-LAZY 需与之对齐）
- `feat-mfc-multi-cycle-fsm`（多周期 FSM 的 stall 机制）

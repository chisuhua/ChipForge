---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: ready
depends_on: []
supersedes: []
related_to:
  - ADR-040 v2.0 (TLM↔CH_MEM 双模同语义)
  - ADR-046 v2.0 (多周期协议引擎 FSM 豁免 — stateful halt 是多周期引擎的前提)
  - ADR-082 (CapabilityTable negotiate)
  - openspec/specs/ctrllink-consumption-contract/spec.md (既有 capability — 本 change 为**增量** API, 不修改其 "不修改 CtrlLink 原 API" Requirement)
  - .rddf/improvements/ctrl-link-chmem-reg-handle.md (source draft, status=ready)
  - .rddf/improvements/chmem-mext-fsm-cycle-parity.md (下游受益方 — 本 change 是其硬前置)
  - openspec/changes/mmu-chmem-phase-b-c-launch/ (Phase C.5 是 A4 的新归属)
---

# ctrl-link-chmem-reg-handle — CtrlLink 支持"有状态"（跨拍保持）halt 条件

> **scope**: 框架级（`include/cf/plugin/`）+ 1 处业务注释更新 + 框架级单测。**纯框架层，零惰性代码**。
> **触发**: `chmem-mext-fsm-cycle-parity`（CH_MEM M 扩展多周期 FSM）需要"FSM busy 期间冻结 fetch/decode"，而 CH_MEM 数据通路全组合（`cpu_factory_chmem.h:431-435` 无 stage 寄存器）⇒ **没有 stall 机制就没法实现多周期**。当前 CtrlLink 只能接组合条件。
> **目标版本**: v0.11.0。
> **治理路径**: rdd-builder（触及框架级公开头文件 + 新 capability spec ⇒ 非 rdd-quick）。

## Why

### 1.1 当前状态（2026-10-20 / 2026-10-22 双审实测）

`include/cf/plugin/ctrl_link.h` 的 CH_MEM 分支提供"组合条件"一种形态：

| 维度 | TLM 模式 | CH_MEM 模式 |
|---|---|---|
| 条件类型 | `std::function<bool()>` 运行期谓词 | `ch::core::ch_bool` 硬件信号句柄 |
| 评估时机 | 每周期 `should_*()` 求值 | **elaboration 期**连成 OR 合并网络 |

`halt_when(ch_bool)` 把条件 push 进 `halt_conds_`；`halt_condition()`（`:148`）用 `ch_bool::operator||` 合并（建真 OR 门 DAG）；`PipeBuilder::elaborate` 把合并结果接到 stage stall。

### 1.2 五个具体缺口

| # | 缺口 | 证据 | 影响 |
|---|---|---|---|
| **G1** | 注释承诺的 `OR_combined_halt_conds()`（"返回 ch_bool，供 DAG 消费"）**根本未实现** | `ctrl_link.h:96` 提到；全仓无定义 | 想拿 OR 后信号接进 DAG 的调用方无路可走 |
| **G2** | `ch_reg<ch_bool>` **已经是** `ch_bool` 的派生类（上游 `class ch_reg : public T`）⇒ 寄存器支撑的条件**其实已经能通过 `halt_when` 传进去**（基类上行转换），但**意图被静默丢弃** | 上游 `cpp-hdl/core/reg.h:32` + `ctrl_link.h:68` | 调用方无法区分组合条件与跨拍保持条件；易写出"以为生效了"的代码 |
| **G3** | `should_halt()/should_throw()/should_flush()` 用 `c.to_bool()` 读 **C++ 侧缓冲**，**不是** Simulator 的 data_map 当前值 ⇒ 对寄存器支撑的条件**永远读到陈旧/初始值** | `ctrl_link.h:98-124` | 有状态条件的"读回"路径不可信 |
| **G4** | HazardPlugin 的 `mark_mmufault()`/`clear_mmufault()` 是 **C++ 实例状态 toggle**；而 OR 网是 **elaboration 一次成型** ⇒ 运行期 toggle **在结构上无法表达** | `hazard_chmem.h:149-173` | MMU 故障 halt 只能停在"骨架"状态 |
| **G5** | 框架内 deprecated `ch_bool::operator\|\|` —— 实测 `ctrl_link.h` **×2**（`:150` + `:160`），`pipe_builder.h` **×0** | 上游 `core/operators_logic.h:166-167` 的 `[[deprecated]]` | 构建期刷 deprecation warning |

### 1.3 为什么是"显式化"而不是"补一个函数"

G2 已经证明**能力层面可用**（`ch_reg<ch_bool>` 能传进 `halt_when`，OR 门引用寄存器节点 ⇒ 随拍更新）。缺的是**类型与文档的显式契约** —— 正如上游 CppHDL 把后端语义差异写成显式 capability（`mem-read-timing` R3/R4）并配套 pin 测试。

G3 的毒性在于**静默**：调用方写 `halt_when(reg)` 能编译能跑，`should_halt()` 却返回陈旧值。显式化 API + 契约测试是消除这类"静默失效"的唯一手段。

### 1.4 风险分析（为何现在做）

- **阻塞 v0.11.0 的 CH_MEM 多周期 M 扩展**：`chmem-mext-fsm-cycle-parity` 需要"FSM busy 期间冻结 fetch/decode"。CH_MEM 数据通路全组合（`cpu_factory_chmem.h:431-435` 无 stage 寄存器），**没有 stall 就没法实现多周期** ⇒ 这是该 follow-up 的硬前置。
- **本 change 是 `mmu-chmem-phase-b-c-launch` Phase C.5 的物理前置**：C.5 要把 MMU fault 接进 `halt_when`，其驱动源（`CPU_EXCEPTION_CODE` 写入）在 C.1/C.5 wiring ⇒ stateful halt 通道必须先存在。
- **不是 v0.11.0 launch gate 的直接判据**（判据 #3 走 `[cpu-l1-mmu-demo] 7/7` + `[mmu-verilator] 4/4`）—— 紧迫性来自**下游两条链的共同硬前置**，不是版本门禁。

## What Changes

| # | 文件 | 改动 |
|---|---|---|
| 1 | `include/cf/plugin/ctrl_link.h` | **A1** 新增 `halt_when_stateful(ch_reg<ch_bool>)` 显式有状态条件入口 + `halt_when(ch_reg<ch_bool>)` 重载（自动按有状态登记）；**A2** 实现 `OR_combined_halt_conds()` 作为唯一实现，既有 `halt_condition()`（`:148`）/ `flush_condition()`（`:157`）改为 inline alias 转发；**A3** `should_*()` 契约改写（`:98-124`）+ 不可靠查询计数/告警；**A5** `:150`/`:160` deprecated `\|\|` → `operator\|` |
| 2 | `include/cf/plugin/pipe_builder.h` | `:434` / `:435` 两处 deprecated `\|\|` → `operator\|`（框架内 A5 共 4 处）；`:307` `should_halt()` / `:202` `should_throw()` 两处消费点按 A3 新契约确认语义 + 有状态条件的 OR 门引用寄存器节点保证（**TLM `#else` 分支零改动**） |
| 3 | `ip/cpu/plugins/hazard_chmem.h` | **仅注释更新** `:149-173` FP-4 gap 描述，指向新归属（`mmu-chmem-phase-b-c-launch` C.5）。**零逻辑改动** |
| 4 | `openspec/specs/ctrllink-continuous-handle/spec.md` | 新 capability（ADDED Requirements delta） |
| 5 | `tests/framework/test_ctrl_link_stateful.cpp`（新） | 框架级单测 4.1-4.3；**手动加入 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES`**（`:118-145` 手工 list，GLOB 只覆盖 TLM target） |
| 6 | `AGENTS.md` + `CHANGELOG.md` | `chmem-mext-fsm-cycle-parity` 硬前置状态更新为"已解除"；CHANGELOG 条目 |

## Out of Scope

- **A4 HazardPlugin 的 mmufault 信号驱动改造 —— 移出本 change**，改挂 `mmu-chmem-phase-b-c-launch` 的 **C.5**。理由：真实驱动源是 C.1/C.5 的 `CPU_EXCEPTION_CODE` 写入（`hazard_chmem.h:157-163` 自陈），本 change 阶段交付的 `ch_reg<ch_bool>` **无 next 驱动源 ⇒ 恒 false 惰性骨架**，惰性窗口跨 Phase 1 → C.5 ~5 周 ⇒ 正是 G3 同款陷阱（"看起来接上了但永不触发"）。**`chmem-mext-fsm-cycle-parity` 的硬前置只需 A1/A2 的 stateful halt 通道，不需要 A4 ⇒ 照常解除**
- **不清全仓 deprecated `&&`/`||`** —— 实测全仓 **~134 站点**（35 处 `\|\|` + 99 处 `&&`，分布 7 个 CH_MEM 业务文件：`decode_chmem.h` 36+6 / `int_alu_chmem.h` 19+10 / `mmu_ptw_chmem.h` 11+1 / `cpu_factory_chmem.h` 11+1 / `dmem_chmem.h` 11+3 / `csr_chmem.h` 9+3 / `hazard_chmem.h` 2+9）。全清需 30+ 文件各跑 `[chmem]` 73/73 + `[verilator]` 48 ELF 回归 ⇒ **单独立独立 change，排在 `mmu-chmem-phase-b-c-launch` Phase B 之后**
- **不改 `ch_state_machine` DSL** 与任何业务 plugin 的 FSM 逻辑
- **不改 TLM 模式的 CtrlLink 语义**（`std::function<bool()>` 路径不动；TLM 消费方 `ibus.h:70` / `mul_div_fsm.h:300` / `hazard.h:217` 走 `#else` 分支，物理不受影响）
- **不实现 `bypass()`**（`bypass_active` 至今是 PoC stub，`ctrl_link.h:126-128` 自承"PoC: M3+ 实现"）
- **不碰 CH_MEM dmem 读路径**（归 `chmem-dmem-read-latency`）
- **不实现 `chmem-mext-fsm-cycle-parity`**（本 change 只解除其硬前置）

## Design Decision（API 关系定案，防"再造第三版"）

新 `OR_combined_halt_conds()` 与既有 `halt_condition()`（`:148`）/ `flush_condition()`（`:157`）的关系**必须显式定案**，否则会出现三个语义重叠的 API。

**裁决**：**新 API 为唯一实现**；两个旧名改为 inline alias 转发。

- 既有调用点（`pipe_builder.h` elaborate 期合并）零改动 ⇒ 不破坏 `openspec/specs/ctrllink-consumption-contract/spec.md` 的 "不修改 CtrlLink 原 API" Requirement（`halt_when/throw_when/flush_when/bypass` 4 个方法的签名与行为均不变，本 change 是**纯增量**）
- `operator||` → `operator|` 替换后**语义等价**（1-bit OR 门 DAG），零行为变化

## Acceptance Criteria

- [ ] **AC-0** 前置 baseline 已记录（`bash tools/run_chipforge_tests.sh --all`，**含逐 case cycle 计数**），可支撑"零回归"判定
- [ ] **AC-1** `CtrlLink` 提供**显式的有状态条件入口**，且 OR 合并结果**随寄存器逐拍更新**（真 DAG 行为，不得读 C++ 侧缓冲快照）—— **测试须 Simulator tick ≥2 拍并中途改变寄存器值观测 OR 输出翻转**
- [ ] **AC-2** `OR_combined_halt_conds()` 已实现并返回可被消费方 DAG 引用的 `ch_bool` 节点；**旧 `halt_condition()` / `flush_condition()` 为 inline alias 转发，未新增第三版**
- [ ] **AC-3** `should_halt()/should_throw()/should_flush()` 的适用范围**显式声明**（仅组合条件可靠）；对有状态条件不得静默返回陈旧值 —— 契约测试覆盖；`pipe_builder.h:307` / `:202` 两处消费点语义已确认
- [ ] **AC-4** 框架 `include/cf/plugin/` 内 deprecated `ch_bool::operator\|\|` **全部 4 处**（`ctrl_link.h:150,160` + `pipe_builder.h:434,435`）改为 `operator|`，构建期框架范围内零 deprecation warning（验证：含全部 13 个 framework 头的 probe TU 剩余 deprecated 站点 = 0）。**业务侧 `ip/**/*_chmem.h` 的 ≥60 `\|\|` + ≥90 `&&` 不在本 change AC 内**
- [ ] **AC-5** **零惰性代码** —— 不引入任何"已接上但永不触发"的信号通道；`hazard_chmem.h` 仅注释更新，无逻辑改动
- [ ] **AC-6** 零回归：`[chmem]` 73 case 基线不变（逐 case cycle 计数比对）；TLM 全量无新增 FAIL；`verify_plugin_decision` 8/8 + `check_plugin_portability` 12/12 + `verify_adr` 0 fail + `doc_link_check` 0 fail
- [ ] **AC-7** 新 capability `ctrllink-continuous-handle` 落 `openspec/specs/`，`openspec validate --specs --strict` PASS
- [ ] **AC-8** `AGENTS.md` 中 `chmem-mext-fsm-cycle-parity` 硬前置状态更新为"已解除"；`CHANGELOG.md` 新增条目
- [ ] **AC-9** `openspec change validate` 0 error 并 archive
- [ ] **AC-10** 新 CH_MEM 框架测试**已手动加入 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES`**，且在 `./build/bin/chipforge_tests_chmem` 中可见可跑

## References

- `.rddf/improvements/ctrl-link-chmem-reg-handle.md` — source draft（status=ready）
- `openspec/specs/ctrllink-consumption-contract/spec.md` — 既有 CtrlLink capability
- `ip/cpu/plugins/hazard_chmem.h:149-173` — FP-4 gap 声明位置
- `include/cf/plugin/ctrl_link.h` / `include/cf/plugin/pipe_builder.h` — 改动目标
- `build/_deps/install/include/cpp-hdl/core/operators_logic.h:166-167` — upstream deprecation 实证
- `build/_deps/install/include/cpp-hdl/core/reg.h:32` — upstream `ch_reg : public T` 实证

# ctrllink-continuous-handle Specification

## Purpose
TBD - created by archiving change ctrl-link-chmem-reg-handle. Update Purpose after archive.
## Requirements
### Requirement: CtrlLink 提供显式的有状态（跨拍保持）条件入口

`CtrlLink` SHALL 提供一个**类型与意图显式**的入口，用于注册"由寄存器支撑、可跨拍保持"的 halt 条件（`ch_reg<ch_bool>` 句柄），与既有的"组合条件"入口在**类型层面**可区分。

该入口 SHALL 复用既有的 `halt_conds_` 存储与 OR 合并网络（不新建平行存储），使 OR 门在 elaboration 期引用**寄存器节点本身**（而非其 C++ 侧缓冲快照）⇒ OR 合并结果 SHALL 随该寄存器的值**逐拍更新**。

**已有能力的显式化说明**：`ch_reg<ch_bool>` 已经是 `ch_bool` 的派生类（上游 `class ch_reg : public T`），因此它**早已能**通过既有 `halt_when(ch_bool)` 传进去并被 OR 门正确引用。本 Requirement 的价值在于**让"这是有状态条件"在类型与文档上可见**，从而消除 G2/G3 类"看似生效实则静默失效"的写法。

**Where**: `include/cf/plugin/ctrl_link.h`（CH_MEM 分支）。

**既有 API 不变承诺**：本 Requirement 是**纯增量**。`CtrlLink::halt_when` / `throw_when` / `flush_when` / `bypass` 四个方法的**签名与行为均不变**，与 `openspec/specs/ctrllink-consumption-contract/spec.md` 的 "不修改 CtrlLink 原 API" Requirement 一致。

#### Scenario: 有状态条件的 OR 合并结果随寄存器逐拍更新

- **WHEN** 调用方注册一个 `ch_reg<ch_bool>` 支撑的 halt 条件，并在 elaboration 后驱动该寄存器的值变化
- **THEN** `OR_combined_halt_conds()` 返回的 OR 合并结果 SHALL 随该寄存器的值**逐拍**变化
- **AND** 该结果 SHALL NOT 读取 C++ 侧的缓冲快照（快照语义由 `should_*()` 路径承担，见下一 Requirement）
- **AND** 测试 SHALL 以 Simulator **不少于 2 拍** 的 tick 驱动该变化并观测输出翻转 —— 单拍 elaboration 断言不足以证明"逐拍更新"

#### Scenario: 组合条件与有状态条件可共存

- **WHEN** 同一 `CtrlLink` 上同时注册一个组合条件（`ch_bool` 字面量/信号）与一个有状态条件（`ch_reg<ch_bool>`）
- **THEN** OR 合并结果 SHALL 为两者的逻辑或
- **AND** 任一条件为真时合并结果为真

#### Scenario: 把 ch_reg<ch_bool> 直接传给 halt_when 时自动按有状态登记

- **WHEN** 调用方调用 `halt_when(some_ch_reg_of_ch_bool)`（而非 `halt_when_stateful`）
- **THEN** 该条件 SHALL 被登记为**有状态条件**（`stateful_halt_count()` 递增），从而关闭"能编译能跑但意图被静默丢弃"的缺口
- **AND** 既有 `halt_when(ch_bool)` 的签名与行为 SHALL 完全不变（纯增量重载）

#### Scenario: 有状态条件登记数量可观测

- **WHEN** 调用方查询 `stateful_halt_count()` / `has_stateful_conditions()`
- **THEN** 前者 SHALL 等于已登记的有状态条件数量
- **AND** 后者在无有状态条件时 SHALL 为 false

### Requirement: OR 合并结果是唯一的实现路径，旧 API 转发而非并行

`OR_combined_halt_conds()` SHALL 是 CH_MEM 模式下 halt 条件 OR 合并的**唯一实现**；既有 `halt_condition()` SHALL 改为 inline alias 转发到它。

既有 `flush_condition()` SHALL 采用同一模式（inline alias 转发到同源的 OR 合并实现），避免出现三个语义重叠的合并 API。

**动机**：既有 `ctrl_link.h:96` 的注释已承诺 `OR_combined_halt_conds()`（"返回 ch_bool，供 DAG 消费"）但**全仓无定义**；若在补实现时另起一条并行路径，将导致三个语义重叠的 API 与三套 deprecated 调用点。

**Where**: `include/cf/plugin/ctrl_link.h`（CH_MEM 分支）。

#### Scenario: 旧 API 与新 API 返回等价结果

- **WHEN** 同一 `CtrlLink` 上注册若干 halt 条件后，分别调用 `halt_condition()` 与 `OR_combined_halt_conds()`
- **THEN** 两者 SHALL 返回**逻辑等价**的结果
- **AND** `halt_condition()` SHALL NOT 拥有独立的合并实现（源码层面 SHALL 为 inline 转发）

#### Scenario: 空条件集合的退化行为不变

- **WHEN** `CtrlLink` 上没有任何 halt 条件
- **THEN** `OR_combined_halt_conds()` SHALL 返回表示"无 stall"的常量假值
- **AND** 该行为与既有 `halt_condition()` 的空集合行为一致（不产生回归）

### Requirement: should_*() 的适用范围被显式声明且不得静默返回陈旧值

`should_halt()` / `should_throw()` / `should_flush()` SHALL 在其文档契约中**显式声明**：它们仅对**组合条件**可靠。对**有状态条件**（`ch_reg<ch_bool>` 支撑），这三个接口 SHALL NOT 被文档引导为验证路径 —— 调用方 SHALL 被引导到 DAG 消费路径（`OR_combined_halt_conds()`）。

对有状态条件调用 `should_*()` 时，系统 SHALL NOT **静默**返回一个看似有效的陈旧值；实现 SHALL 提供可观测的失败/告警语义（实现形式由实现方选择，但必须可被测试断言）。

**可观测语义的具体形态**（实现约定）：调用 SHALL 递增一个可查询的"不可靠查询"计数器（例如 `unreliable_query_count()`），并 SHALL 在首次触发时 emit 一次告警。调用方据此可断言"确实用错了 API"，而不是只看到一个看似有效的布尔值。

**已知事实**：`should_*()` 读的是 C++ 侧缓冲（`c.to_bool()`），而非 Simulator 的 data_map 当前值 ⇒ 对寄存器支撑的条件**永远读到陈旧/初始值**。本 Requirement 把这一事实从"隐藏陷阱"提升为"显式契约"。

**Where**: `include/cf/plugin/ctrl_link.h`（CH_MEM 分支，`:98-124`）。

#### Scenario: 组合条件的 should_*() 语义不变

- **WHEN** 注册组合条件并驱动其取值变化
- **THEN** `should_halt()` SHALL 反映该组合条件的当前值
- **AND** 该行为与本 change 之前一致（无回归）

#### Scenario: 有状态条件不被文档引导到 should_*()

- **WHEN** 调用方注册了有状态条件
- **THEN** API 文档契约 SHALL 明确指出 `should_*()` 对该条件不可靠
- **AND** 调用方 SHALL 被引导使用 `OR_combined_halt_conds()` 的 DAG 消费路径
- **AND** 该契约 SHALL 有对应测试断言（明确断言行为，而非"碰巧不报错"）

### Requirement: 框架级 deprecated 算子清理范围受控

本 change SHALL 把 `include/cf/plugin/` 内及其直接触碰文件中的 deprecated `ch_bool::operator||` 调用替换为 `operator|`（1-bit OR），且构建期该范围内 SHALL **零 deprecation warning**。

**范围上限**：全仓 deprecated `ch_bool::operator||` / `operator&&` 调用实测约 **134 站点**（35 处 `||` + 99 处 `&&`，分布在 7 个 CH_MEM 业务文件）。该全量清理 SHALL **不属于**本 change —— 强行执行会把本 change 撑成 30+ 文件的回归工程（每文件需 `[chmem]` 73/73 + `[verilator]` 48 ELF 回归）， SHALL 由独立 change 承担并排在 `mmu-chmem-phase-b-c-launch` Phase B 之后。

**Where**: `include/cf/plugin/ctrl_link.h`（`halt_condition()` 与 `flush_condition()` 两处合并循环）。

#### Scenario: 替换后语义等价

- **WHEN** 多个 halt 条件被 OR 合并
- **THEN** 使用 `operator|` 产生的 OR 门 DAG SHALL 与使用 deprecated `operator||` 产生的 DAG **逻辑等价**
- **AND** 既有 `[framework]` 与 `[chmem]` 测试 SHALL 零回归（逐 case cycle 计数不变）

#### Scenario: 范围外站点不被本 change 触碰

- **WHEN** 仓库中存在 CH_MEM 业务文件内的 deprecated `||` / `&&` 调用
- **THEN** 本 change SHALL NOT 修改这些文件
- **AND** 这些清理 SHALL 被记录为独立 change 的范围

### Requirement: 本 change 不引入惰性信号通道

本 change SHALL NOT 引入任何"已注册但永不触发"的信号通道。具体地，本 change SHALL NOT 交付任何**缺少 next 驱动源**的 `ch_reg<ch_bool>` 条件并将其接入 `halt_when` OR 网。

**理由**：`ch_reg<T>` 在无 next 驱动时保持复位值，在 DAG 中表现为恒假常量。若在缺少真实驱动源的阶段交付该通道，会制造与 G3 同款的"看起来接上了但永不触发"陷阱 —— 而消除此类静默失效正是本 capability 的存在理由。

HazardPlugin 的 mmufault 状态改造（`mark_mmufault()` / `clear_mmufault()` 从 C++ 实例状态改为信号驱动）其真实驱动源是 `CPU_EXCEPTION_CODE` 的写入（由 `mmu_exception_handler_chmem` 在 memory stage 产生），该改造 SHALL 归属 `mmu-chmem-phase-b-c-launch` 的 Phase C.5，**不属于**本 change。

#### Scenario: 无惰性注册

- **WHEN** 审查本 change 的全部 `halt_when_stateful` 调用点
- **THEN** 每个调用点 SHALL 有可验证的 next 驱动源
- **AND** 不存在任何仅声明、未被驱动的 `ch_reg<ch_bool>` 条件

#### Scenario: HazardPlugin 的 gap 注释指向新归属

- **WHEN** 审查 `ip/cpu/plugins/hazard_chmem.h` 中关于 CtrlLink API gap 的注释
- **THEN** 该注释 SHALL 说明该改造归属 `mmu-chmem-phase-b-c-launch` 的 Phase C.5
- **AND** 该文件 SHALL NOT 有因本 change 引入的逻辑改动


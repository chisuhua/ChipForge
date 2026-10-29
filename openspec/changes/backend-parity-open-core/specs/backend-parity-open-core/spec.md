## ADDED Requirements

### Requirement: pipe-builder-top-level-port-declaration

插件框架 MUST 提供在 **CH_MEM 模式**下声明**顶层端口**的能力，使业务 IP 可产出带对外接口的网表（而非仅 `clock`/`reset` 两端口的封闭核）。

#### Scenario: 声明的端口出现在网表顶层

- **WHEN** 业务代码通过框架声明一个 `ch_in` 与一个 `ch_out` 并 `elaborate` + `to_verilog`
- **THEN** 生成的网表 `module top (...)` MUST 在顶层端口列表中包含这两个端口
- **AND** MUST NOT 依赖 CppHDL 的 `Component` / `ch_device` 体系

#### Scenario: 无 current context 时 fail-closed

- **WHEN** 在没有 current context 的情况下声明端口
- **THEN** 框架 MUST **返回错误**（★ `Result` + `std::unexpected(PluginError::NullContextPtr)`）⇒ ★ **fail-closed 语义不变**（★ ADR-091 v1.8 **G-1c 修正**：★ 原为 `throw`，★ 违反 ADR-047 Check 10「静态配置头文件禁 throw」）
- **AND** MUST NOT 静默跳过节点创建（★ CppHDL 的 `ch_logic_in` 构造是 `if (ctx) {...}` ⇒ 无 ctx 时 `impl()` 为空且不报错 ⇒ 会造成「端口消失而门禁照绿」的假绿通道）

#### Scenario: 被内部驱动的输入端口不作为顶层端口

- **WHEN** 一个 `ch_in` 被内部逻辑赋值（存在内部 driver）
- **THEN** 该端口 MUST NOT 出现在网表顶层端口列表
- **AND** 框架 MUST 在契约测试中钉住该行为（★ 请求侧 MUST 用 `ch_out`，响应侧 MUST 用 `ch_in`）

#### Scenario: 框架头不得引入 ch.hpp

- **WHEN** 框架头或业务 IP 头文件需要 CppHDL 类型
- **THEN** MUST 只 include **具体头**（`core/io.h` / `core/uint.h` / `core/context.h` 等）
- **AND** MUST NOT include `<ch.hpp>`（★ 它引入 `module.h`，而 `module.h` 的 `build_hierarchical_name` 缺 `inline` ⇒ 任何 2+ TU include 即链接失败）

#### Scenario: TLM 模式不受影响

- **WHEN** 在非 `CF_PLUGIN_USE_CH_MEM` 模式下编译
- **THEN** 端口声明 API MUST NOT 引入 CppHDL 依赖 ⇒ TLM 构建 MUST 保持通过

### Requirement: pipe-builder-backend-injection

插件框架 MUST 支持向仿真器注入**求值后端**，使同一份硬件描述可在解释器与 Verilator 后端下执行。

#### Scenario: 注入后端生效

- **WHEN** 以指定 backend 构造仿真器
- **THEN** `active_backend_name()` MUST 返回该后端的名字

#### Scenario: 空后端保持既有行为

- **WHEN** 以 `nullptr`（或调用既有无参重载）构造仿真器
- **THEN** 行为 MUST 与现状**逐位相同** ⇒ 既有调用方 MUST NOT 需要修改

### Requirement: open-core-bus-interface-contract

CH_MEM 开放核 MUST 通过 **CppHDL 的 `ch_stream`** 暴露取指与访存接口，且 MUST NOT 引入 TLM 兼容依赖。

#### Scenario: 接口载荷是 CH_MEM-only bundle

- **WHEN** 定义总线请求/响应载荷
- **THEN** MUST 是 `bundle_base` 派生 + `CH_BUNDLE_FIELDS_T`，字段类型 MUST 为 `ch::core::ch_uint<N>`
- **AND** MUST NOT 引用 `cf::plugin::uint_t`（★ 保持零 TLM 兼容 ⇒ ADR-092）

#### Scenario: 载荷真名不得是悬空名

- **WHEN** 定义总线载荷类型
- **THEN** 名字 MUST NOT 是 `MemReqBundle` / `MemRespBundle`（★ 这两个名字在代码中**不存在**，实际类型是 `cf::bundles::MemReq` / `MemResp`）
- **AND** MUST NOT 与既有 `bundles/` 中的类型重名（避免歧义）

#### Scenario: 每个端口字段宽度受限

- **WHEN** 定义任一端口字段
- **THEN** 字段位宽 MUST ≤ **64 bit**
- **AND** MUST NOT 整体超宽（★ `VerilatorBackend::build_port_access_table()` 对 `size() > 64` 的端口返回 `PortBitwidthOverflow`；★ bundle 端口按字段展开 ⇒ 总宽可超但**单字段不可超**）

#### Scenario: 核内跨阶段通信不受影响

- **WHEN** 业务 IP 在核内 stage 之间传递数据
- **THEN** MUST 继续使用 `Payload<T>` + `PipeNode`（无握手，同拍数据通路）
- **AND** MUST NOT 因引入 `ch_stream` 而删除或替换 `Payload<T>`（★ 两者**层次不同**：IP 间 / 核↔从设备 = `ch_stream`；核内 stage 间 = `Payload<T>`）

### Requirement: open-core-observation-through-top-level-ports

开放核的可观测量 MUST 通过**顶层端口**暴露，MUST NOT 依赖穿透内部层级的私有读法。

#### Scenario: 观测量从顶层端口读取

- **WHEN** 门禁采集 `tohost` / `pc` / 寄存器 / 内存签名
- **THEN** MUST 从**顶层 `ch_out` 端口**读取
- **AND** MUST NOT 依赖 `--public-flat-rw` 之类的内部层级读法（★ 内部节点名随重构变化 ⇒ 门禁观测路径不稳定）

#### Scenario: 握手成本必须显式承担

- **WHEN** 跨 IP 接口采用带 `valid`/`ready` 的流
- **THEN** 从设备 MUST 可将 `ready` 恒真（退化为无阻塞流）⇒ 单周期同步场景 MUST NOT 被握手拖慢
- **AND** 若 `ready` 可被拉低，则核侧 MUST 有对应 stall 机制
- **AND** 时序 MUST 在「`ready` 恒真」与「`ready` 可拉低」**两档分别**测量 ⇒ MUST NOT 只测一档

### Requirement: open-core-verification-implementation-role

ip 级单元测试 MUST 提供一个**符合接口契约的验证实现**承担从设备角色（响应请求），SoC 级测试 MUST 可用同一接口接入真实 Cache/MMU IP。

#### Scenario: 验证实现承担从设备角色

- **WHEN** ip 级单测驱动开放核
- **THEN** 验证实现 MUST 作为**从设备**（slave）响应 imem/dmem 请求
- **AND** MUST 提供 ELF 加载能力（指令侧的数据来源）

#### Scenario: 与 SoC 接线角色对称

- **WHEN** SoC 级测试接入真实 Cache/MMU IP
- **THEN** 真实 IP MUST 以**同一接口契约**承担从设备角色 ⇒ 替换验证实现即得 SoC 场景
- **AND** 验证实现 MUST NOT 与真实 IP 的接口形态分叉

### Requirement: open-core-equivalence-with-preload-core

开放核与既有预载核 MUST 有机器可判的等价性保证，防止二者分叉为「双副本漂移」。

#### Scenario: 同输入下三终值观测量逐位相等

- **WHEN** 同一 ELF 分别在开放核与预载核上执行
- **THEN** `tohost` / 寄存器 / 内存签名 **三项 MUST 逐位相等**
  （★ **ADR-091 v1.6 H-3 修正**：★ 原写「五观测量逐位相等」⇒ ★ **流水结构不同构**
  ⇒ ★ `总拍数` / `逐拍 PC 流 hash` **结构性不可比** ⇒ ★ 两者 MUST 已登记为
  ★ **ADR-089 例外**（★ scope 限定「开放核 vs 预载核」⇒ 双后端线不受影响））

#### Scenario: 等价性检查具备鉴别力

- **WHEN** 故意修改开放核的一处语义
- **THEN** 等价性检查 MUST 判红
- **AND** MUST NOT 因「两边都没跑」而呈现通过

### Requirement: open-core-dual-backend-must-pass

开放核的同一份测试 MUST 在**解释器后端**与 **Verilator 后端**下都通过。

#### Scenario: 两后端均通过

- **WHEN** 同一份测试 body 分别在两个后端执行
- **THEN** 两个后端 MUST 都 PASS

#### Scenario: 后端缺失不得算通过

- **WHEN** Verilator 工具链不可用或后端初始化被拒（如端口超宽）
- **THEN** 该半边 MUST 显式标记为**跳过/告警**
- **AND** MUST NOT 被计为 PASS ⇒ ★ 门禁的「双后端必过」判据 MUST 区分「跳过」与「通过」

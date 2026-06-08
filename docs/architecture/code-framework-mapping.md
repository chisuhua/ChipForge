# 代码框架映射

> 本文档说明 ChipForge 架构设计概念与底层框架（CppTLM/CppHDL）实际代码的对应关系，帮助开发者快速定位实现。

> ⚠️ **版本敏感性声明**
>
> 本文档基于以下代码版本验证（抽样路径已 `ls` 核对存在）:
> - CppTLM: 仓库当前 HEAD
> - CppHDL: 仓库当前 HEAD
>
> **维护规则**:
> 1. 升级 CppTLM/CppHDL 依赖后,必须重新核对本文档列出的所有头文件路径
> 2. 新增 CppTLM/CppHDL 内部接口引用时,需同步更新对应章节
> 3. 文档审查 checklist 中应包含"框架代码-本文档路径一致性"项
>
> 已知限制:本文档列举的为常用 API 路径,非穷尽列表。具体实现以代码为准。

> 📊 **实现状态图例**
>
> 本文档涉及的概念混合了**已实现代码**与**架构设计目标**。为避免误导读者，特标注以下状态：
>
> - ✅ **[已实现]**：CppTLM/CppHDL 框架层代码中已存在，可在 C++ 中直接引用
> - 🔧 **[框架层已实现]**：框架层接口完整，但 ChipForge 应用层尚未调用或封装
> - 🚧 **[设计阶段]**：仅存在于架构设计文档（`overview.md` / `interface-design.md` / `ip/cpu/docs/multi_isa_architecture.md`），代码尚未实现
> - ❌ **[文档与代码背离]**：架构文档声称已实现，但实际代码不存在（详见第 7 节"实现状态与设计目标对比"）
>
> 默认未标注条目均为 ✅ **[已实现]**。

## 1. 总体架构层次

```
┌─────────────────────────────────────────────────────────────┐
│ ChipForge 应用层                                            │
│ ├── ip/*/tlm/   → TLM 模块实现 (空目录，参见 §7)            │
│ ├── ip/*/rtl/   → RTL 模块实现 (空目录，参见 §7)            │
│ ├── bundles/    → 共享 Bundle 定义 (目录为空，参见 §7)       │
│ └── soc/        → JSON 配置 + 组装                          │
├─────────────────────────────────────────────────────────────┤
│ CppTLM 框架层                                               │
│ ├── SimObject / EventQueue   → 仿真引擎                     │
│ ├── ChStreamModuleBase       → ch_stream 模块               │
│ ├── StreamAdapter            → 自动适配层（已实现 ~800 行） │
│ ├── ChStreamAdapterFactory   → JSON 类型注册中心            │
│ ├── CoherenceDomain / VC     → NoC / 一致性原语（§2.7）     │
│ ├── ModuleFactory            → JSON 配置驱动                │
│ └── Metrics                  → 统计收集                     │
├─────────────────────────────────────────────────────────────┤
│ CppHDL 框架层                                               │
│ ├── Component                → RTL 组件基类                 │
│ ├── ch_module<T>             → 子模块实例化                 │
│ ├── bundle_base<Self>        → Bundle 类型系统              │
│ ├── chlib/*                  → 25 个 HDL 高级组件（§3.5）   │
│ ├── axi4/*                   → AXI4 参考实现（§3.6）        │
│ ├── codegen_verilog.h        → Verilog 代码生成             │
│ └── Simulator                → RTL 仿真引擎                 │
└─────────────────────────────────────────────────────────────┘
```

> **实现边界说明**：上述架构图中，**框架层（CppTLM/CppHDL）**部分全部已实现。ChipForge 应用层仅 `soc/riscv_virt.json` 与 IP 目录骨架（`README.md` / `.gitkeep`）存在，具体 IP 实现 / Bundle 定义 / SoC 装配类尚未构建，详见第 7 节"实现状态与设计目标对比"。

## 2. CppTLM 框架映射

### 2.1 核心仿真引擎

| 文档概念 | CppTLM 代码位置 | 类/接口 | 说明 |
|---------|----------------|---------|------|
| 仿真对象基类 | `CppTLM/include/core/sim_object.hh` | `SimObject` | 所有 TLM 模块继承此类 |
| 事件驱动引擎 | `CppTLM/include/core/event_queue.hh` | `EventQueue` | 基于优先队列的仿真调度 |
| 仿真模块 | `CppTLM/include/core/sim_module.hh` | `SimModule` | SimObject 的模块化扩展 |
| TLM 模块 | `CppTLM/include/core/tlm_module.hh` | `TlmModule` | TLM 层级仿真模块 |

### 2.2 ch_stream 通信体系

> **路径变更说明（2026-06-02）**：`StreamAdapterBase` 基类已从 `framework/stream_adapter.hh` 迁出至 `core/stream_adapter_base.hh`，以打破 core↔framework 循环依赖。具体适配器类（`InputStreamAdapter` / `OutputStreamAdapter` / `StreamAdapter`）仍保留在 `framework/stream_adapter.hh`。

| 文档概念 | CppTLM 代码位置 | 类/接口 | 说明 |
|---------|----------------|---------|------|
| ch_stream 模块基类 | `CppTLM/include/core/chstream_module.hh` | `ChStreamModuleBase` | 支持 ch_stream 通信的模块基类 |
| ch_stream 端口 | `CppTLM/include/core/chstream_port.hh` | `ChStreamInitiatorPort` / `ChStreamTargetPort` | ch_stream 握手端口 |
| ch_stream 注册 | `CppTLM/include/chstream_register.hh` | - | ch_stream 模块注册宏 |
| 适配层基类 | `CppTLM/include/core/stream_adapter_base.hh` | `cpptlm::StreamAdapterBase` | ch_stream ↔ Port 转换（**已迁出 framework/**） |
| 输入适配器 | `CppTLM/include/framework/stream_adapter.hh` | `InputStreamAdapter<T>` | SlavePort → ch_stream |
| 双向适配器 | `CppTLM/include/framework/bidirectional_port_adapter.hh` | `BidirectionalPortAdapter` | 请求/响应双向适配 |
| 双端口适配器 | `CppTLM/include/framework/dual_port_stream_adapter.hh` | `DualPortStreamAdapter` | 双端口流适配 |
| 多端口适配器 | `CppTLM/include/framework/multi_port_stream_adapter.hh` | `MultiPortStreamAdapter` | 多端口聚合 |
| 适配器工厂 | `CppTLM/include/framework/chstream_adapter_factory.hh` | `ChStreamAdapterFactory` | JSON 驱动的类型注册中心（单例），支持 `registerFactory` / `registerAdapter` / `create` / `knows` |
| 调试追踪器 | `CppTLM/include/framework/debug_tracker.hh` | `DebugTracker` | 调试信息追踪 |

### 2.3 端口与连接

| 文档概念 | CppTLM 代码位置 | 类/接口 | 说明 |
|---------|----------------|---------|------|
| 主设备端口 | `CppTLM/include/core/master_port.hh` | `MasterPort` | 发起方接口（框架级） |
| 从设备端口 | `CppTLM/include/core/slave_port.hh` | `SlavePort` | 响应方接口（框架级） |
| 简单端口 | `CppTLM/include/core/simple_port.hh` | `SimplePort` | 轻量端口接口 |
| 端口管理器 | `CppTLM/include/core/port_manager.hh` | `PortManager` | 模块端口注册与管理 |
| 端口类型 | `CppTLM/include/core/port_types.hh` | - | 端口类型定义 |
| 端口兼容性 | `CppTLM/include/core/port_compatibility.hh` | - | 端口类型兼容检查 |
| 端口统计 | `CppTLM/include/core/port_stats.hh` | `PortStats` | 端口流量统计 |
| 事务包 | `CppTLM/include/core/packet.hh` | `Packet` | Port 间传输的数据载体 |
| 连接解析器 | `CppTLM/include/core/connection_resolver.hh` | `ConnectionResolver` | 自动连接解析 |

### 2.4 配置与工厂

| 文档概念 | CppTLM 代码位置 | 类/接口 | 说明 |
|---------|----------------|---------|------|
| 模块工厂 | `CppTLM/include/core/module_factory.hh` | `ModuleFactory` | JSON 配置驱动的模块装配 |
| 拓扑解析器 | `CppTLM/include/core/topology_parser.hh` | `TopologyParser` | JSON 拓扑文件解析 |
| 拓扑节点 | `CppTLM/include/core/topology_node.hh` | `TopologyNode` | 拓扑图中的节点 |
| JSON 包含器 | `CppTLM/include/utils/json_includer.hh` | `JsonIncluder` | 支持 JSON 文件引用 |
| 配置工具 | `CppTLM/include/utils/config_utils.hh` | - | 配置辅助函数 |
| 模块分组 | `CppTLM/include/utils/module_group.hh` | `ModuleGroup` | 模块逻辑分组 |
| 变量解析器 | `CppTLM/include/utils/var_resolver.hh` | `VarResolver` | 配置中变量替换 |
| 参数解析器 | `CppTLM/include/core/param_parser.hh` | `ParamParser` | 模块参数解析 |
| 参数规则 | `CppTLM/include/core/param_rules.hh` | `ParamRules` | 参数校验规则 |
| 插件加载器 | `CppTLM/include/core/plugin_loader.hh` | `PluginLoader` | 动态模块加载 |

### 2.5 统计与性能

| 文档概念 | CppTLM 代码位置 | 类/接口 | 说明 |
|---------|----------------|---------|------|
| 统计框架 | `CppTLM/include/metrics/stats.hh` | `Stats` | 统计数据收集基础 |
| 统计管理器 | `CppTLM/include/metrics/stats_manager.hh` | `StatsManager` | 全局统计管理 |
| 直方图 | `CppTLM/include/metrics/histogram.hh` | `Histogram` | 延迟/分布统计 |
| 指标报告器 | `CppTLM/include/metrics/metrics_reporter.hh` | `MetricsReporter` | 指标格式化输出 |
| 流式报告器 | `CppTLM/include/metrics/streaming_reporter.hh` | `StreamingReporter` | 实时流式统计输出 |
| 事务追踪 | `CppTLM/include/framework/transaction_tracker.hh` | `TransactionTracker` | 事务生命周期追踪 |

### 2.6 Bundle 定义（CppTLM 内置）

| Bundle | CppTLM 代码位置 | 说明 |
|--------|----------------|------|
| NoCFlitBundle | `CppTLM/include/bundles/noc_bundles_tlm.hh` | NoC Flit 数据包 |
| CacheBundle | `CppTLM/include/bundles/cache_bundles_tlm.hh` | 缓存请求/响应 |
| CppHDL 类型 | `CppTLM/include/bundles/cpphdl_types.hh` | CppHDL 类型支持 |
| Bundle 序列化 | `CppTLM/include/bundles/bundle_serialization.hh` | Bundle ↔ Packet 转换 |

### 2.7 NoC 与缓存一致性原语

> **实现状态**：框架层已实现，应用于 ChipForge NoC 仿真（Phase 4.x）。这两个原语是 CppTLM 高层抽象，开发者一般通过 `ModuleFactory` 间接使用。

| 文档概念 | CppTLM 代码位置 | 类/接口 | 说明 |
|---------|----------------|---------|------|
| 一致性域 | `CppTLM/include/core/coherence_domain.hh` | `CoherenceDomain` / `DomainRegistry` / `enum class Protocol { MESI, MOESI }` | Phase 4.2/4.3 缓存一致性模块：snoop fanout、bridge map、home-node 查找 |
| 虚拟通道 | `CppTLM/include/core/virtual_channel.hh` | `InputVC` / `OutputVC`（含嵌套 `Stats`） | VC 缓冲区，提供 `enqueue` / `front` / `pop` / `trySend` 接口、容量、优先级、per-VC 统计 |

## 3. CppHDL 框架映射

### 3.1 组件模型

| 文档概念 | CppHDL 代码位置 | 类/接口 | 说明 |
|---------|----------------|---------|------|
| RTL 组件基类 | `CppHDL/include/component.h` | `Component` | 所有 RTL 组件继承此类 |
| 子模块实例化 | `CppHDL/include/module.h` | `ch_module<T>` | 模板化子模块创建 |
| 顶层设备 | `CppHDL/include/device.h` | `ch_device<T>` | 仿真顶层容器 |
| RTL 仿真器 | `CppHDL/include/simulator.h` | `Simulator` | RTL 仿真引擎 |
| 统一头文件 | `CppHDL/include/ch.hpp` | - | 包含所有 CppHDL 核心功能 |

### 3.2 类型系统

| 文档概念 | CppHDL 代码位置 | 类/接口 | 说明 |
|---------|----------------|---------|------|
| 无符号整数 | `CppHDL/include/core/uint.h` | `ch_uint<N>` | N 位无符号 |
| 布尔 | `CppHDL/include/core/bool.h` | `ch_bool` | 1 位布尔 |
| 类型特性 | `CppHDL/include/core/traits.h` | - | 类型检测与推导 |
| 寄存器 | `CppHDL/include/core/reg.h` | `ch_reg` | 时序寄存器 |
| 存储器 | `CppHDL/include/core/mem.h` | `ch_mem` | 存储器原语 |
| IO 端口 | `CppHDL/include/core/io.h` | `ch_logic_in` / `ch_logic_out` | 输入/输出端口 |
| 字面量 | `CppHDL/include/core/literal.h` | `ch_literal` | 硬件字面值 |
| 运算符 | `CppHDL/include/core/operators.h` | - | 硬件运算符重载（编译期） |
| 模拟数据类型 | `CppHDL/include/core/types.h` | `sdata_type` / `ch::core::constants::*` | 框架内部仿真数据原语与常量 |
| 全局仿真上下文 | `CppHDL/include/core/context.h` | `ch::core::Context` | `thread_local` 仿真状态：管理 lnode 集合、求值阶段 |
| 方向标签 | `CppHDL/include/core/direction.h` | `input_direction` / `output_direction` / `internal_direction` | 端口方向空类型标签 + `is_input_v` / `is_output_v` 特性 |
| 节点构建器 | `CppHDL/include/core/node_builder.h` | `ch::core::node_builder` | 单例工厂类，是构造 lnode 的首选接口（**避免直接操作 lnode**，参见 AGENTS.md 反模式） |
| lnode 前向声明 | `CppHDL/include/core/lnode.h` | - | lnode 类型前向声明 |
| lnode 运行时多态 | `CppHDL/include/core/lnodeimpl.h` | - | lnode 运行时多态支持 |
| 逻辑缓冲 | `CppHDL/include/core/logic_buffer.h` | `LogicBuffer` | 信号传播 + 时序封装 |
| 运行时运算符 | `CppHDL/include/core/operators_runtime.h` | - | 运行时运算符（必须与 `operators.h` 同步维护，参见 AGENTS.md） |
| 求值后端接口 | `CppHDL/include/core/eval_backend.h` | - | 求值后端抽象接口 |
| 解释器后端 | `CppHDL/include/core/interpreter_backend.h` | - | 解释器风格求值后端 |
| Verilator 后端 | `CppHDL/include/core/verilator_backend.h` | - | Verilator 协同仿真后端 |

### 3.3 Bundle 系统

#### 3.3.1 Bundle 基础设施

| 文档概念 | CppHDL 代码位置 | 类/接口 | 说明 |
|---------|----------------|---------|------|
| Bundle 基类 | `CppHDL/include/core/bundle/bundle_base.h` | `bundle_base<Self>` | CRTP 模式 Bundle |
| Bundle 元数据 | `CppHDL/include/core/bundle/bundle_meta.h` | - | Bundle 字段反射 |
| Bundle 布局 | `CppHDL/include/core/bundle/bundle_layout.h` | - | 位宽布局计算 |
| Bundle 特性 | `CppHDL/include/core/bundle/bundle_traits.h` | - | Bundle 类型检测 |
| Bundle 协议 | `CppHDL/include/core/bundle/bundle_protocol.h` | - | 协议方向标注 |
| Bundle 操作 | `CppHDL/include/core/bundle/bundle_operations.h` | - | Bundle 操作符 |
| Bundle 序列化 | `CppHDL/include/core/bundle/bundle_serialization.h` | - | 序列化支持 |
| Bundle 工具 | `CppHDL/include/core/bundle/bundle_utils.h` | - | 工具函数 |
| TLM 转换器 | `CppHDL/include/core/bundle/tlm/tlm_bundle_converter.h` | - | Bundle ↔ TLM 转换 |

#### 3.3.2 标准 Bundle 库

| Bundle | CppHDL 代码位置 | 说明 |
|--------|----------------|------|
| Stream 接口 | `CppHDL/include/bundle/stream_bundle.h` | 流式数据（valid/ready 握手） |
| Flow 接口 | `CppHDL/include/bundle/flow_bundle.h` | 单向流控 |
| 通用 Bundle | `CppHDL/include/bundle/common_bundles.h` | `fifo_bundle<T>` / `interrupt_bundle` / `config_bundle<A,D>` |
| AXI4 接口 | `CppHDL/include/bundle/axi_bundle.h` | AXI4 完整协议 |
| AXI4-Lite | `CppHDL/include/bundle/axi_lite_bundle.h` | 轻量 AXI |
| 时钟复位 | `CppHDL/include/bundle/clock_reset_bundle.h` | 时钟与复位信号 |
| Fragment | `CppHDL/include/bundle/fragment.h` | 分片传输 |
| 统一包含 | `CppHDL/include/bundle.h` | Bundle 系统统一头文件 |

### 3.4 内部机制

| 文档概念 | CppHDL 代码位置 | 说明 |
|---------|----------------|------|
| 逻辑节点 | `CppHDL/include/lnode/` | **位于顶层**（非 `include/core/lnode/`），包含模板实现 `*.tpp`（bool.tpp / uint.tpp / reg.tpp / context.tpp / logic_buffer.tpp / node_builder.tpp）和扩展头 `*.h`（literal_ext.h / node_builder_ext.h / operators_ext.h） |
| AST | `CppHDL/include/ast/` | 抽象语法树：`ast_nodes.h` + 12 个 `instr_*.h` 指令节点（base/clock/io/mem/mux/op/proxy/reg）+ 内部实现 `clockimpl.h` / `memimpl.h` / `resetimpl.h` / `mem_port_impl.h` |
| 位向量 | `CppHDL/include/bv/` | 位级操作支持（`bitvector.h`, `bv.h`, `utils.h`） |
| AXI4 组件 | `CppHDL/include/axi4/` | AXI4 参考实现（互联、协议转换等） |
| 代码生成 | `CppHDL/include/codegen_verilog.h` | Verilog 代码生成 |
| DAG 代码生成 | `CppHDL/include/codegen_dag.h` | DAG 格式代码生成 |
| JIT 编译 | `CppHDL/include/jit/` | JIT 仿真加速（`jit_compiler.h` / `jit_ir.h`） |
| 工具库 | `CppHDL/include/utils/` | 通用工具：`logger.h` / `exceptions.h` / `format_utils.h` / `source_info.h` / `destruction_manager.h` / `converter.h` |

### 3.5 HDL 组件库（chlib）

> **重要补充**：`chlib/` 目录是 CppHDL 的高级组件库，包含 **25 个 .h 文件（5881 行）**的预构建 HDL 原语（FIFO、流水线、仲裁器、FSM、断言等）。此前映射文档完全遗漏此章节，是最大缺口。
>
> 入口头：`CppHDL/include/chlib.h`（58 行 umbrella header，已默认包含大部分组件；`axi4lite.h` / `converter.h` / `fragment` 类被显式注释掉，需按需手动 include）

#### 3.5.1 算术与位操作

| 组件 | CppHDL 代码位置 | 说明 |
|------|----------------|------|
| 基础算术 | `CppHDL/include/chlib/arithmetic.h` | `add<N>`、基本算术函数（`ch_uint<N>` 输入） |
| 超前进位加法器 | `CppHDL/include/chlib/arithmetic_advance.h` | `carry_lookahead_adder<N>`，返回 `CLAResult<N>`（sum + carry_out） |
| 位级操作 | `CppHDL/include/chlib/bitwise.h` | `leading_zero_detector<N>` 等位级原语 |
| 组合逻辑 | `CppHDL/include/chlib/combinational.h` | `priority_encoder<N>` 等组合逻辑原语 |
| 逻辑门 | `CppHDL/include/chlib/logic.h` | `and_gate<N>` 等基本门 |
| 编码转换 | `CppHDL/include/chlib/converter.h` | `binary_to_onehot<N>` 二进制 ↔ one-hot（**未包含在 `chlib.h`**，需手动 include） |
| One-Hot 解码 | `CppHDL/include/chlib/onehot.h` | `onehot_dec<N>` one-hot 解码器 |

#### 3.5.2 存储与寄存器

| 组件 | CppHDL 代码位置 | 说明 |
|------|----------------|------|
| FIFO | `CppHDL/include/chlib/fifo.h` | 完整 FIFO 实现，含 `count_empty` 辅助（chlib 中最大单文件，466 行） |
| 单口 RAM | `CppHDL/include/chlib/memory.h` | `single_port_ram<DATA, ADDR>` RAM 原语 |
| 寄存器 | `CppHDL/include/chlib/sequential.h` | `register_<N>` 使能寄存器（基于 `ch_reg`） |
| 流水线寄存器 | `CppHDL/include/chlib/pipeline.h` | 单级流水线寄存器组件，含数据 + ready/valid 变体 |
| 流流水线 | `CppHDL/include/chlib/stream_pipeline.h` | `stream_m2s_pipe` 增加 1 周期 valid + payload 延迟 |

#### 3.5.3 状态机与控制流

| 组件 | CppHDL 代码位置 | 说明 |
|------|----------------|------|
| 状态机 DSL | `CppHDL/include/chlib/state_machine.h` | `ch_state_machine<S, N>` SpinalHDL 风格 DSL，`.state()` / `.on_active()` / `.transition_to()` |
| 表达式条件 | `CppHDL/include/chlib/if.h` | 表达式风格 `multi_if<T>`（branch_info 列表） |
| 语句块条件 | `CppHDL/include/chlib/if_stmt.h` | 语句块风格 `conditional_block`（上下文管理） |
| 路由/选择 | `CppHDL/include/chlib/switch.h` | switch/routing 逻辑 + `std::common_type` 对 `ch_uint` 的特化 |

#### 3.5.4 仲裁器

| 组件 | CppHDL 代码位置 | 说明 |
|------|----------------|------|
| 优先级选择器 | `CppHDL/include/chlib/selector_arbiter.h` | `PrioritySelectorResult<N>` 优先级选择器 |
| 流仲裁器 | `CppHDL/include/chlib/stream_arbiter.h` | `StreamArbiterLockResult<T, N>` 锁定仲裁器（事务完成前保持锁定） |

#### 3.5.5 流（Stream）操作

| 组件 | CppHDL 代码位置 | 说明 |
|------|----------------|------|
| 流核心 | `CppHDL/include/chlib/stream.h` | chlib 最大文件（562 行），`ch_stream<T>` 核心流操作 |
| 流构建器 | `CppHDL/include/chlib/stream_builder.h` | 流畅 API：`StreamBuilder<T>::queue<depth>()` / `.haltWhen()` / `.map()` / `.throwWhen()` / `.takeWhen()` / `.continueWhen()` |
| 流运算符 | `CppHDL/include/chlib/stream_operators.h` | `operator<<=` 直接连接（SpinalHDL 风格 `sink <<= source`） |
| 流位宽转换 | `CppHDL/include/chlib/stream_width_adapter.h` | `stream_narrow_to_wide<W, N>` / `stream_wide_to_narrow<N, W>` |

#### 3.5.6 总线协议与调试

| 组件 | CppHDL 代码位置 | 说明 |
|------|----------------|------|
| AXI4-Lite 通道 | `CppHDL/include/chlib/axi4lite.h` | `Axi4LiteWriteAddr<N>` 等 AXI4-Lite 通道信号结构（**未包含在 `chlib.h`**，需手动 include） |
| 断言检查 | `CppHDL/include/chlib/assert.h` | `AssertChecker` 仿真时断言组件 |
| UART 追踪 | `CppHDL/include/chlib/simulator_trace.h` | `UartCapture` UART TX 输出捕获 |

### 3.6 AXI4 参考实现

| 组件 | CppHDL 代码位置 | 说明 |
|------|----------------|------|
| AXI4 完整接口 | `CppHDL/include/axi4/axi4_full.h` | AXI4 主/从接口组件 |
| AXI4-Lite 接口 | `CppHDL/include/axi4/axi4_lite.h` | AXI4-Lite 接口组件 |
| AXI4-Lite 矩阵 | `CppHDL/include/axi4/axi4_lite_matrix.h` | Lite 互联矩阵 |
| AXI4-Lite 从设备 | `CppHDL/include/axi4/axi4_lite_slave.h` | Lite 从设备基类 |
| AXI 互联 4×4 | `CppHDL/include/axi4/axi_interconnect_4x4.h` | 4×4 AXI 互联 |
| AXI→AXI-Lite 桥 | `CppHDL/include/axi4/axi_to_axilite.h` | 协议转换桥 |
| 外设库 | `CppHDL/include/axi4/peripherals/` | AXI4 外设参考实现（axi_dma / axi_gpio / axi_i2c / axi_pwm / axi_spi / axi_timer / axi_uart） |

## 4. ChipForge 项目层映射

### 4.1 目录职责

| ChipForge 目录 | 对应框架 | 职责 | 开发者角色 |
|---------------|---------|------|-----------|
| `ip/*/tlm/` | CppTLM | TLM 模块实现（继承 ChStreamModuleBase） | IP 开发者 |
| `ip/*/rtl/` | CppHDL | RTL 模块实现（继承 Component） | IP 开发者 |
| `ip/*/test/` | 两者 | 验证测试（Level A/B/C） | 验证工程师 |
| `ip/*/configs/` | 独立 | JSON 参数配置和 Schema | IP 开发者 |
| `bundles/` | 共享层 | Bundle 定义（基于 CppHDL 类型系统） | 架构师 |
| `soc/` | CppTLM | SoC 顶层配置（JSON 驱动 ModuleFactory） | SoC 集成者 |

### 4.2 典型开发流程映射

```
1. 定义 Bundle（bundles/）
   └── 使用 CppHDL: bundle_base<Self> + ch_uint<N>

2. 实现 TLM 模块（ip/*/tlm/）
   └── 继承 CppTLM: ChStreamModuleBase
   └── 内部使用 ch_stream<MyBundle>
   └── 注册：REGISTER_MODULE("TypeName", ClassName)

3. 实现 RTL 模块（ip/*/rtl/）
   └── 继承 CppHDL: Component
   └── 在 describe() 中实现逻辑
   └── 使用相同 Bundle 类型

4. 编写测试（ip/*/test/）
   └── Level A: 单元测试（直接调用模块方法）
   └── Level B: 集成测试（最小 JSON 拓扑）
   └── Level C: 系统测试（完整 SoC）

5. 配置 SoC（soc/）
   └── JSON 定义模块实例和连接
   └── ModuleFactory 自动装配
```

## 5. 关键接口对照

### 5.1 ch_stream（模块内部）vs Port（框架外部）

| 特征 | ch_stream | Port |
|------|-----------|------|
| 使用者 | IP 开发者 | SoC 集成者/框架 |
| 定义位置 | 模块类内部 | ModuleFactory 自动创建 |
| 数据类型 | 泛型 Bundle | Packet（序列化） |
| 连接方式 | 不可直接连接 | JSON 配置连接 |
| 转换机制 | StreamAdapter 自动映射 | 直接使用 |

### 5.2 CppTLM Bundle vs CppHDL Bundle

| 特征 | CppTLM 中的 Bundle | CppHDL 中的 Bundle |
|------|-------------------|-------------------|
| 基类 | 无固定基类（POD 结构） | `bundle_base<Self>` (CRTP) |
| 字段类型 | C++ 原生类型（uint64_t 等） | `ch_uint<N>` / `ch_bool` |
| 用途 | ch_stream 传输载体 | RTL 信号线组 |
| 共享方式 | 通过 bundle_serialization.hh 序列化 | 直接使用 |
| TLM 互操作 | 原生支持 | 通过 `tlm_bundle_converter.h` 转换 |

## 6. 相关文档

- [项目架构总览](overview.md)
- [接口设计详解](interface-design.md)
- [术语表](../GLOSSARY.md)
- [技术选型](tech-selection.md)

## 7. 实现状态与设计目标对比

> **重要**：本节旨在明确区分**已实现的框架代码**与**架构文档中的设计目标**。`overview.md` 和 `interface-design.md` 中的部分描述属于**设计意图**而非已完成代码，读者应以此节为准。

### 7.1 已实现（框架层 + 工具完备）

| 概念 | 状态 | 证据 |
|------|------|------|
| CppTLM SimObject / EventQueue / ChStreamModuleBase | ✅ 已实现 | `include/core/sim_object.hh` 等 86 条路径全部验证存在 |
| CppTLM StreamAdapter（适配层） | ✅ 已实现 | 5 个适配器头文件共 ~800 行，无 TODO / unimplemented |
| CppTLM ChStreamAdapterFactory | ✅ 已实现 | `framework/chstream_adapter_factory.hh` 单例工厂 |
| CppTLM CoherenceDomain / VirtualChannel | ✅ 已实现 | `core/coherence_domain.hh` / `core/virtual_channel.hh` |
| CppTLM PluginLoader（运行时 SO 加载） | ✅ 已实现 | `core/plugin_loader.hh` + `src/utils/dynamic_loader.cc` |
| CppHDL Component / Simulator / Bundle 系统 | ✅ 已实现 | `component.h` / `simulator.h` / `core/bundle/*` 全部存在 |
| CppHDL chlib 组件库 | ✅ 已实现 | 25 个组件文件、5881 行，预构建 FIFO / 流水线 / 仲裁器 / FSM / AXI4-Lite 等 |
| CppHDL lnode / AST / bv / codegen | ✅ 已实现 | 模板实现 + 13 个 AST 节点 + Verilog/DAG 代码生成 |

### 7.2 框架层已实现，但应用层尚未构建（🔧）

| 概念 | 框架层状态 | 应用层状态 |
|------|----------|----------|
| CppTLM `tlm/*.hh` 标准 IP 库 | ✅ 已实现（12 个文件：arbiter / cache / cpu / crossbar / link / memory / nic / noc_statistics / router / stress_patterns / tlm_stub / traffic_gen） | ❌ ChipForge 未注册或使用 |
| CppHDL `cpu/*` CPU 子系统（RV32I / 流水线 / 缓存 / 分支预测 / 冒险） | ✅ 已实现（独立框架） | ❌ ChipForge 未集成 |
| CppHDL JIT 编译器 | ✅ 已实现（`jit/jit_compiler.h`） | ❌ ChipForge 未启用 |

### 7.3 设计阶段（仅文档存在，🚧）

| 概念 | 文档来源 | 缺失原因 |
|------|---------|---------|
| **PipeNode / PipeLink / PipeBuilder** | `GLOSSARY.md` L16-18, `ip/cpu/README.md` L11-13, `ip/cpu/docs/multi_isa_architecture.md`（1100+ 行设计稿） | 0 行代码，0 头文件，纯设计提案 |
| **声明式 Plugin 模型**（`Plugin` 基类 + `setup()` / `build()` / `at_stage()`） | `ip/cpu/docs/multi_isa_architecture.md` L1055 | 仅有 `PluginLoader`（dlopen SO 加载器），与声明式 Plugin 是无关概念 |
| **IP 级别的 TLM 实现**（`RiscvIssTlm` / `L1CacheTlm` / `BusMatrixTlm` / `DramTlm` / `UartTlm` 等） | `soc/riscv_virt.json` 引用 | `ip/*/tlm/` 目录全部为空（仅 `README.md`） |

### 7.4 文档与代码背离（❌ 必须修正）

| 文档声称 | 实际代码状态 | 修正方向 |
|---------|------------|---------|
| `interface-design.md` L8-22: `bundles/mem_bundles.h` 定义 `MemReqBundle` | `bundles/` 目录为空；`MemReqBundle` 在 CppTLM/CppHDL 中 0 匹配 | 要么实现 Bundle 定义，要么从文档中删除 |
| `interface-design.md` L39-65: `bundles/cache_bundles.h` 定义 `CacheReqBundle` | 不存在 | 同上 |
| `interface-design.md` L68-75: `bundles/impl_mode.h` 定义 `enum class ImplMode {TLM_ONLY, RTL_ONLY, COMPARE, SHADOW}` | `impl_mode.h` 不存在；`ImplMode` 在 CppTLM/CppHDL/ChipForge 全部 0 匹配 | 同上 |
| `interface-design.md` L186-217: `soc/RiscvVirtSoC.h/cpp` 已存在 | 不存在；`soc/` 仅有 `riscv_virt.json` | 要么构建 SoC 装配类，要么删除示例代码 |
| `overview.md` L131-141: 各 IP 使用 `REGISTER_MODULE` 宏 | `REGISTER_MODULE` 在 ChipForge 中 0 匹配 | 同上 |
| `soc/riscv_virt.json` L4: `"impl_mode": "TLM_ONLY"` | 无代码解析此字段 | 删除该字段或实现 `ImplMode` 支持 |

### 7.5 建议的修正优先级

1. **紧急**：明确 `interface-design.md` 与 `overview.md` 的文档性质（设计提案 vs 已完成体系结构）
2. **紧急**：删除或补全 `bundles/` 目录的引用
3. **高**：要么实现 `RiscvVirtSoC.cpp` 入口，要么删除 `soc/riscv_virt.json`（避免悬挂引用）
4. **中**：实现 `bundles/impl_mode.h` 与 `ImplMode` 框架支持（如确实需要 TLM↔RTL 切换）
5. **低**：将 PipeNode / 声明式 Plugin 的设计从 `multi_isa_architecture.md` 拆分到独立 RFC 文档，避免读者误以为已规划

---

> **维护建议**：每次升级 CppTLM/CppHDL 后，应运行 `verify_framework_mapping.sh`（参见仓库根目录）核对所有路径一致性，并人工核查本节"已实现"列表是否仍然成立。

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

## 1. 总体架构层次

```
┌─────────────────────────────────────────────────┐
│ ChipForge 应用层                                │
│ ├── ip/*/tlm/   → TLM 模块实现                  │
│ ├── ip/*/rtl/   → RTL 模块实现                  │
│ ├── bundles/    → 共享 Bundle 定义              │
│ └── soc/        → JSON 配置 + 组装              │
├─────────────────────────────────────────────────┤
│ CppTLM 框架层                                   │
│ ├── SimObject / EventQueue   → 仿真引擎         │
│ ├── ChStreamModuleBase       → ch_stream 模块   │
│ ├── StreamAdapter            → 自动适配层       │
│ ├── ModuleFactory            → JSON 配置驱动    │
│ └── Metrics                  → 统计收集         │
├─────────────────────────────────────────────────┤
│ CppHDL 框架层                                   │
│ ├── Component                → RTL 组件基类     │
│ ├── ch_module<T>             → 子模块实例化     │
│ ├── bundle_base<Self>        → Bundle 类型系统  │
│ └── Simulator                → RTL 仿真引擎     │
└─────────────────────────────────────────────────┘
```

## 2. CppTLM 框架映射

### 2.1 核心仿真引擎

| 文档概念 | CppTLM 代码位置 | 类/接口 | 说明 |
|---------|----------------|---------|------|
| 仿真对象基类 | `CppTLM/include/core/sim_object.hh` | `SimObject` | 所有 TLM 模块继承此类 |
| 事件驱动引擎 | `CppTLM/include/core/event_queue.hh` | `EventQueue` | 基于优先队列的仿真调度 |
| 仿真模块 | `CppTLM/include/core/sim_module.hh` | `SimModule` | SimObject 的模块化扩展 |
| TLM 模块 | `CppTLM/include/core/tlm_module.hh` | `TlmModule` | TLM 层级仿真模块 |

### 2.2 ch_stream 通信体系

| 文档概念 | CppTLM 代码位置 | 类/接口 | 说明 |
|---------|----------------|---------|------|
| ch_stream 模块基类 | `CppTLM/include/core/chstream_module.hh` | `ChStreamModuleBase` | 支持 ch_stream 通信的模块基类 |
| ch_stream 端口 | `CppTLM/include/core/chstream_port.hh` | `ChStreamInitiatorPort` / `ChStreamTargetPort` | ch_stream 握手端口 |
| ch_stream 适配器工厂 | `CppTLM/include/core/chstream_adapter_factory.hh` | `ChStreamAdapterFactory` | 自动创建 StreamAdapter |
| ch_stream 注册 | `CppTLM/include/chstream_register.hh` | - | ch_stream 模块注册宏 |
| 适配层基类 | `CppTLM/include/framework/stream_adapter.hh` | `StreamAdapterBase` | ch_stream ↔ Port 转换 |
| 输入适配器 | `CppTLM/include/framework/stream_adapter.hh` | `InputStreamAdapter<T>` | SlavePort → ch_stream |
| 双向适配器 | `CppTLM/include/framework/bidirectional_port_adapter.hh` | `BidirectionalPortAdapter` | 请求/响应双向适配 |
| 双端口适配器 | `CppTLM/include/framework/dual_port_stream_adapter.hh` | `DualPortStreamAdapter` | 双端口流适配 |
| 多端口适配器 | `CppTLM/include/framework/multi_port_stream_adapter.hh` | `MultiPortStreamAdapter` | 多端口聚合 |
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
| 运算符 | `CppHDL/include/core/operators.h` | - | 硬件运算符重载 |

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
| 逻辑节点 | `CppHDL/include/lnode/` | 电路图表示（bool.tpp, uint.tpp, reg.tpp 等） |
| AST | `CppHDL/include/ast/` | 抽象语法树（ast_nodes.h, instr_*.h） |
| 位向量 | `CppHDL/include/bv/` | 位级操作支持（bitvector.h, bv.h） |
| AXI4 组件 | `CppHDL/include/axi4/` | AXI4 参考实现（互联、协议转换等） |
| 代码生成 | `CppHDL/include/codegen_verilog.h` | Verilog 代码生成 |
| DAG 代码生成 | `CppHDL/include/codegen_dag.h` | DAG 格式代码生成 |
| JIT 编译 | `CppHDL/include/jit/` | JIT 仿真加速 |

### 3.5 AXI4 参考实现

| 组件 | CppHDL 代码位置 | 说明 |
|------|----------------|------|
| AXI4 完整接口 | `CppHDL/include/axi4/axi4_full.h` | AXI4 主/从接口组件 |
| AXI4-Lite 接口 | `CppHDL/include/axi4/axi4_lite.h` | AXI4-Lite 接口组件 |
| AXI4-Lite 矩阵 | `CppHDL/include/axi4/axi4_lite_matrix.h` | Lite 互联矩阵 |
| AXI4-Lite 从设备 | `CppHDL/include/axi4/axi4_lite_slave.h` | Lite 从设备基类 |
| AXI 互联 4×4 | `CppHDL/include/axi4/axi_interconnect_4x4.h` | 4×4 AXI 互联 |
| AXI→AXI-Lite 桥 | `CppHDL/include/axi4/axi_to_axilite.h` | 协议转换桥 |
| 外设库 | `CppHDL/include/axi4/peripherals/` | AXI4 外设参考实现 |

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

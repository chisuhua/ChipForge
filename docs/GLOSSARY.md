# ChipForge 术语表

## 核心架构概念

| 术语 | 英文全称 | 定义 | 首次出处 |
|------|---------|------|--------|
| **Bundle** | - | TLM 与 RTL 共享的数据结构，封装传输信号（payload + 元数据） | interface-design.md |
| **ch_stream\<T\>** | Channel Stream | CppTLM 基础通信接口，实现 valid/ready 握手协议 + 流控 | overview.md |
| **ChStreamModuleBase** | - | 所有 ch_stream 模块的基类，继承自 SimObject，提供 set_stream_adapter() 接口 | overview.md |
| **EventQueue** | - | CppTLM 事件驱动调度器，管理仿真时间推进 | overview.md |
| **ImplMode** | Implementation Mode | 组件运行模式：TLM_ONLY / RTL_ONLY / COMPARE / SHADOW | overview.md |
| **MasterPort / SlavePort** | - | CppTLM 框架级组装接口，由 StreamAdapter 从 ch_stream 自动映射 | interface-design.md |
| **Port** | - | 模块对外组装接口（框架视角），SoC JSON 配置通过 Port 名称连接模块 | interface-design.md |
| **SimObject** | Simulation Object | CppTLM 的基础仿真对象，所有组件的基类 | overview.md |
| **StreamAdapter** | Stream Adapter | ch_stream 到 Port 的双向适配层，由 ModuleFactory 自动创建，用户无需手动编码 | overview.md |
| **PipeNode** | - | 声明式管线节点，承载 Plugin 行为 | multi_isa_architecture.md |
| **PipeLink** | - | PipeNode 间的数据通道声明 | multi_isa_architecture.md |
| **PipeBuilder** | - | 编译期调度生成器，将声明转为 TLM/RTL | multi_isa_architecture.md |

## 接口与数据结构

| 术语 | 定义 | 用途 |
|------|------|------|
| **MemReqBundle** | 内存请求数据包（地址、数据、操作类型、大小） | CPU → Cache → Memory 请求 |
| **MemRespBundle** | 内存响应数据包（数据、状态、延迟信息） | Memory → Cache → CPU 响应 |
| **SnoopBundle** | 一致性侦听数据包（地址、操作、状态） | 多核缓存一致性 |
| **DMI** | Direct Memory Interface，直接内存接口 | TLM 仿真加速，跳过时序模型 |

## 验证与测试

| 术语 | 英文全称 | 定义 |
|------|---------|------|
| **Level A** | Unit Test | 单元测试：IP 内部组件独立验证 |
| **Level B** | Integration Test | 集成测试：相邻 IP 联合验证 |
| **Level C** | System Test | 系统测试：SoC 级端到端验证 |
| **HTIF** | Host-Target Interface | 仿真器与被测程序的通信机制 |
| **RISCOF** | RISC-V Compliance Framework | RISC-V 官方合规测试框架 |
| **Spike** | - | RISC-V 官方参考 ISS（指令集模拟器） |
| **Co-simulation** | 协同仿真 | TLM 与 RTL 同时运行并对比结果的验证模式 |

## 设计空间探索

| 术语 | 英文全称 | 定义 |
|------|---------|------|
| **DSE** | Design Space Exploration | 设计空间探索，通过参数扫描寻找最优配置 |
| **参数扫描** | Parameter Sweep | 遍历指定参数范围组合，收集性能指标 |
| **Pareto 前沿** | Pareto Frontier | 多目标优化中的最优解集合 |
| **可插拔策略** | Pluggable Strategy | 运行时可配置的算法选择（如缓存替换策略） |

## 项目结构

| 术语 | 定义 |
|------|------|
| **IP** | 知识产权核，独立的硬件功能模块 |
| **SoC** | System on Chip，通过 JSON 配置装配 IP 的系统 |
| **CppTLM** | C++ Transaction-Level Modeling 框架 |
| **CppHDL** | C++ Hardware Description Language 框架 |

## 缩写对照

| 缩写 | 全称 | 中文 |
|------|------|------|
| TLM | Transaction-Level Modeling | 事务级建模 |
| RTL | Register-Transfer Level | 寄存器传输级 |
| ISS | Instruction Set Simulator | 指令集模拟器 |
| ISA | Instruction Set Architecture | 指令集架构 |
| IPC | Instructions Per Cycle | 每周期指令数 |
| MSHR | Miss Status Holding Register | 缺失状态保持寄存器 |
| BTB | Branch Target Buffer | 分支目标缓冲器 |
| PMP | Physical Memory Protection | 物理内存保护 |
| MMU | Memory Management Unit | 内存管理单元 |
| PLIC | Platform-Level Interrupt Controller | 平台级中断控制器 |
| CLINT | Core-Local Interruptor | 核心本地中断器 |
| NoC | Network on Chip | 片上网络 |
| QoS | Quality of Service | 服务质量 |

## 架构层级关系

| 层级 | 接口 | 使用者 | 说明 |
|------|------|--------|------|
| 模块设计层 | ch_stream\<Bundle\> | IP 开发者 | 模块内部数据流定义 |
| 适配层 | StreamAdapter | 框架自动 | ch_stream ↔ Port 双向转换 |
| 框架组装层 | Port (MasterPort/SlavePort) | SoC 集成者 | JSON 配置引用的接口 |
| 配置驱动层 | ModuleFactory + JSON | 用户 | 零代码实例化和连接 |

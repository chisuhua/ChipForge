# ChipForge 术语表

## 核心架构概念

| 术语 | 英文全称 | 定义 | 首次出处 |
|------|---------|------|--------|
| **Bundle** | - | TLM 与 RTL 共享的数据结构，封装传输信号（payload + 元数据） | interface-design.md |
| **ch_stream\<T\>** | Channel Stream | ★ **CppHDL** 的 bundle 类型（`bundle/stream_bundle.h`，`bundle_base` 派生 + `as_master/slave_direction()`），valid/ready 握手 + 流控；★ **用于 IP 之间 / 核 ↔ 从设备**（ADR-092 §D4） | overview.md |
| **ChStreamModuleBase** | - | ★ **CppTLM**（legacy）的 stream 模块基类，继承自 SimObject，提供 set_stream_adapter() 接口；★ **与 CppHDL 的 `ch_stream<T>` 是同名不同物** | overview.md |
| **EventQueue** | - | CppTLM 事件驱动调度器，管理仿真时间推进 | overview.md |
| **ImplMode** | Implementation Mode | 组件运行模式：TLM_ONLY / RTL_ONLY / COMPARE / SHADOW | overview.md |
| **MasterPort / SlavePort** | - | CppTLM 框架级组装接口，由 StreamAdapter 自动映射 | interface-design.md |
| **Port** | - | 模块对外组装接口（框架视角），SoC JSON 配置通过 Port 名称连接模块 | interface-design.md |
| **SimObject** | Simulation Object | CppTLM 的基础仿真对象，所有组件的基类 | overview.md |
| **StreamAdapter** | Stream Adapter | ★ CppTLM（legacy）里 Port ↔ stream 的双向适配层，由 ModuleFactory 自动创建，用户无需手动编码 | overview.md |
| **PipeNode** | - | 声明式管线节点，承载 Plugin 行为 | multi_isa_architecture.md |
| **PipeLink** | - | PipeNode 间的数据通道声明 | multi_isa_architecture.md |
| **PipeBuilder** | - | 编译期调度生成器，将声明转为 TLM/RTL | multi_isa_architecture.md |

## 接口与数据结构

| 术语 | 定义 | 用途 |
|------|------|------|
| ★ **`cf::bundles::MemReq`** | 内存请求数据包（address 64 / data 64 / is_write 1 / burst_len 8 / id 8） | CPU → Cache → Memory 请求<br>★ **代码里的实际名**（★ 旧文档写的 `MemReqBundle` **不存在** ⇒ ADR-092 §D7） |
| ★ **`cf::bundles::MemResp`** | 内存响应数据包（data 64 / id 8 / error 1 / last 1） | Memory → Cache → CPU 响应 |
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
| 接口层 | ★ **ch_stream\<Bundle\>**（CppHDL） | IP 开发者 | ★ **IP 之间 / 核 ↔ 从设备**的接口契约（ADR-092 §D4） |
| ★ 核内跨阶段层 | `Payload<T>` + `PipeNode` | IP 开发者 | ★ 核内 stage 间数据流（无握手，同拍通路） |
| 适配层（legacy） | StreamAdapter | 框架自动 | ★ CppTLM Port ↔ stream 双向转换（bridge 专用） |
| 框架组装层 | Port (MasterPort/SlavePort) | SoC 集成者 | JSON 配置引用的接口 |
| 配置驱动层 | ModuleFactory + JSON | 用户 | 零代码实例化和连接 |

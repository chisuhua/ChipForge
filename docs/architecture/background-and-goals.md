# 背景与目标

> 基于 [CppTLM](https://github.com/chisuhua/CppTLM) + [CppHDL](https://github.com/chisuhua/CppHDL)
> 支持 Bare-metal / RTOS / Linux 三级测试框架，可扩展至 GPU 等多芯片形态

---

## 1.1 项目目标

构建一个基于 **CppTLM**（事务级建模）与 **CppHDL**（C++ 硬件描述）的 RISC-V 虚拟验证平台，实现：

- **类 Gem5 的 TLM 建模风格**：以事务为核心驱动，支持松散定时（LT）到近似定时（AT）多粒度仿真
- **TLM 与 RTL 接口一致**：同一套 Bundle 定义，TLM 模型用 `ch_stream<T>`，RTL 模型用 `Signal<T>`，在 SoC 组合层可按模式混插
- **CppHDL 渐进演进**：前期直接 C++ 仿真，后期生成 Verilog 并引入 Verilator
- **三级测试覆盖**：裸机（bare-metal）→ RTOS（FreeRTOS/Zephyr）→ Linux（OpenSBI + Linux Kernel）
- **多芯片可扩展**：`cpu/`, `cache/`, `memory/`, `interconnect/`, `peripheral/` 各为独立组件库，可按产品形态组合成不同 SoC（RISC-V、GPU 等）

## 1.2 核心约束

- 主要语言：**C++17**
- 构建系统：**CMake >= 3.16**
- TLM 框架：**CppTLM**（不依赖 SystemC，使用 `ch_stream<T>` 接口）
- RTL 框架：**CppHDL**（`Component` 基类，支持 C++ 直仿与 Verilog 生成）
- ISS 参考：**Spike**（官方黄金参考模型）

---

## 参考项目调研

### 成熟 RISC-V 实现

| 项目 | 位宽 | 流水线 | HDL | 流片 | 特点 |
|------|------|--------|-----|------|------|
| **VexRiscv** | 32 | 2-5 级顺序 | SpinalHDL | 否 | 插件化，极高可定制性，FPGA 优化 |
| **CVA6** | 64 | 6 级顺序 | SystemVerilog | 是 | 应用级，core-v-verif 验证环境完整 |
| **BOOM v3** | 64 | 10 级乱序 | Chisel | 是（测试） | 高性能乱序，Chipyard 生态 |
| **Rocket Core** | 64 | 5 级顺序 | Chisel | 是 | Berkeley 参考实现，生态完善 |
| **ibex** | 32 | 2-3 级顺序 | SystemVerilog | 是（OpenTitan） | 安全特性，UVM 验证标杆 |
| **CV32E40P** | 32 | 4 级顺序 | SystemVerilog | 是 | PULP DSP 扩展 |

### CppTLM 框架特性

**CppTLM v2.0** 采用四层分层架构：

```
Application Layer   - 用户模块业务逻辑，操作 ch_stream<T>
Framework Layer     - 自动适配与连接管理，ModuleRegistry，生命周期管理
Mapper Layer        - 协议转换（AXI4/CHI/TileLink），Port <-> Stream 转换
Bundle Layer        - Generic Payload 定义，TLM/RTL 共享数据结构
```

核心特性：

- `ch_stream<T>` 统一通信接口，类型安全
- `TransactionTracker` 端到端事务追踪（含 JSON/VCD 导出）
- `ImplMode`：`TLM_ONLY` / `RTL_ONLY` / `COMPARE` / `SHADOW` 四种运行模式
- 纯 TLM 仿真速度目标：> 1000 KIPS；混合模式额外开销 < 10%

### CppHDL 框架特性

**CppHDL** 提供：

- `Component` 基类：通过 `describe()` 方法声明硬件逻辑，使用 `__io()` 宏定义端口，`ch_reg<T>` 定义时序逻辑
- `LogicNode` DAG 系统：数据流图，支持常数传播和死码消除
- `Simulator`：直接 C++ 仿真，`tick(N)` 驱动 N 个时钟周期，支持配置文件加载和 VCD 追踪导出
- `VerilogCodeGen`：AST -> Verilog，支持 DAG 优化后输出
- **与 CppTLM 共享同一套 Bundle 定义**——这是两者集成的核心纽带

### 关键集成点

```
bundles/common_bundles.h
        |
        +-- CppTLM 使用 ch_stream<MemReqBundle>    (TLM 高速仿真)
        |
        +-- CppHDL 使用 Port<MemReqBundle>         (RTL 周期精确 / Verilog 生成)
```

同一份 Bundle 定义，消除 TLM 与 RTL 之间的手工桥接层。

### 验证生态参考

| 工具 | 用途 |
|------|------|
| **Spike** | 官方 ISS，黄金参考，co-simulation |
| **riscv-tests** | 官方 ISA 单元测试 |
| **riscv-arch-test + RISCOF** | 官方合规认证框架 |
| **riscv-dv** | Google/ChipsAlliance 随机指令生成器（Phase 5） |
| **FreeRTOS** | 嵌入式 RTOS，RISC-V 官方移植 |
| **Zephyr** | 完整 RTOS，HWMv2 架构，RISC-V 全系列扩展 |
| **OpenSBI** | M-mode 固件，SBI 接口规范 |
| **Verilator** | Verilog 转 C++ 周期精确仿真（Phase 5） |


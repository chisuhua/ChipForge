# 背景与目标

> 基于 [CppHDL](https://github.com/chisuhua/CppHDL) 作为 Plugin-style 的硬件后端设施
> 支持 Bare-metal / RTOS / Linux 三级测试框架，可扩展至 GPU 等多芯片形态

---

## 1.1 项目目标

构建一个用 **Plugin-style**（D4 决策，ADR-083）设计硬件 IP、用 **CppHDL** 作硬件后端的 RISC-V 虚拟验证平台：

- **Plugin-style 单一 source of truth**（ADR-083）：业务 IP 用 `cf::plugin::PluginBase` + `at_stage()` + `Payload<T>` 编写；同一份代码经由两个编译模式产出不同仿真后端
- **TLM 模式**（默认）：`cf::plugin::uint_t<N>` 解析为 POD，`pb.run()` 每周期执行 → C++ 直仿（~0.6s 全套）
- **CH_MEM 模式**（`-DCF_PLUGIN_USE_CH_MEM`）：`cf::plugin::uint_t<N>` 解析为 `ch::core::ch_uint<N>`，`pb.elaborate(ctx)` 发射 lnode DAG → ① CppHDL `Simulator::tick()` ② `ch::toVerilog(ctx)` → `.v` ③ Verilator 联合仿真
- **三级测试覆盖**：裸机（bare-metal）→ RTOS（FreeRTOS/Zephyr）→ Linux（OpenSBI + Linux Kernel）
- **多芯片可扩展**：`cpu/`, `cache/`, `memory/`, `interconnect/`, `peripheral/` 各为独立 Plugin-style IP 库，可按产品形态组合成不同 SoC（RISC-V、GPU 等）

## 1.2 核心约束

- 主要语言：**C++17**（Plugin-style 业务代码）/ **C++20**（chipforge_tests target，因为链接 CppHDL 头文件）
- 构建系统：**CMake >= 3.16**
- **设计模式**：**Plugin-style**（D4 决策，ADR-037 + ADR-083）——业务 IP 不再分别为 TLM 和 RTL 各写一份
- **硬件后端**：**CppHDL**（CH_MEM 模式必依赖，提供 ch_uint/ch_reg/ch_mem + elaboration + Simulator + toVerilog + Verilator backend）
- **Legacy 仿真内核**：**CppTLM**（保持依赖以兼容 bridge 适配层，业务 IP 不使用 ch_stream/Module/Port/ModuleFactory）
- ISS 参考：**Spike**（官方黄金参考模型）

---

## 参考项目调研

### 成熟 RISC-V 实现

| 项目 | 位宽 | 流水线 | HDL | 流片 | 特点 |
|------|------|--------|-----|------|------|
| **VexRiscv** | 32 | 2-5 级顺序 | SpinalHDL | 否 | **插件化，极高可定制性，FPGA 优化**（Plugin-style 灵感来源） |
| **CVA6** | 64 | 6 级顺序 | SystemVerilog | 是 | 应用级，core-v-verif 验证环境完整 |
| **BOOM v3** | 64 | 10 级乱序 | Chisel | 是（测试） | 高性能乱序，Chipyard 生态 |
| **Rocket Core** | 64 | 5 级顺序 | Chisel | 是 | Berkeley 参考实现，生态完善 |
| **ibex** | 32 | 2-3 级顺序 | SystemVerilog | 是（OpenTitan） | 安全特性，UVM 验证标杆 |
| **CV32E40P** | 32 | 4 级顺序 | SystemVerilog | 是 | PULP DSP 扩展 |

> **ChipForge 与 VexRiscv 的设计同构**：VexRiscv 用 SpinalHDL 的 Plugin Scala 组合实现流水线骨架，ChipForge 用 `cf::plugin` C++ Plugin 组合 + CppHDL elaboration 实现等价效果。差异：VexRiscv 通过 Scala 宏隐式插入 stage reg，ChipForge 通过 `pb.elaborate(ctx)` 显式发射 lnode DAG。

### CppHDL 框架特性（CH_MEM 模式后端）

**CppHDL** 为 Plugin-style CH_MEM 模式提供：

- **硬件类型**：`ch::core::ch_uint<N>` / `ch::core::ch_bool` / `ch::core::ch_reg` / `ch::core::ch_mem`（Plugin 通过 `cf::plugin::uint_t<N>` 类型抽象间接访问，业务代码不感知差异）
- **elaboration DSL**：`ch::core::Context` + `node_builder` 单例，`pb.elaborate(ctx)` 把 at_stage 闭包发射为 lnode DAG
- **仿真器**：`ch::core::Simulator::tick()` 周期精确直仿（C++ 仿真器，无需 EDA 工具）
- **Verilog 代码生成**：`ch::toVerilog(ctx)` AST → `.v` 文件
- **Verilator backend**：`verilator --cc file.v` → linked library → `ch::core::VerilatorBackend` 联合仿真
- **chlib 高级组件**：`chlib::ch_state_machine` 多周期 FSM、`chlib::stream` 流协议、`chlib::fifo`/`chlib::pipeline` 等 25 个 .h 文件（5881 行）

**与 CppTLM 共享同一套 Bundle 类型系统**（基于 `ch_uint<N>` + `bundle_base<Self>`），消除 TLM（Plugin-style POD）与 HDL（Plugin-style CH_MEM）之间的手工桥接层——业务代码直接用 `cf::plugin::uint_t<N>` 即可。

### CppTLM 框架角色（legacy 仿真内核）

**CppTLM v2.0** 仍作为外部依赖保留（`extern/CppTLM` 符号链接），**仅** Bridge 适配层（`src/cf_plugin/bridge/`）依赖：

- Bridge 适配层用于桥接外部 CppTLM-based 系统到 ChipForge
- CppTLM 核心特性（`ch_stream<T>` / `TransactionTracker` / `ModuleFactory`）**业务 IP 不使用**

业务 IP 跨阶段通信由 `cf::plugin::Payload<T>` + `PipeNode` 替代。`ImplMode` enum 已在 doc-code-realignment 2026-06-17 废弃。

### 关键集成点（修订）

```
bundles/<bundle>.h
        |
        +-- Plugin-style TLM 模式: cf::plugin::uint_t<N> = POD
        |    -> pb.run() -> C++ 直仿 (~0.6s)
        |
        +-- Plugin-style CH_MEM 模式: cf::plugin::uint_t<N> = ch::core::ch_uint<N>
        |    -> pb.elaborate(ctx) -> lnode DAG
        |    -> ch::toVerilog(ctx) -> .v 文件
        |    -> verilator --cc -> VerilatorBackend 联合仿真
        |
        +-- Legacy Bridge 适配层: cpptlm::ch_stream<Bundle>
             (仅 src/cf_plugin/bridge/ 使用, 业务 IP 不使用)
```

同一份 Bundle 定义 + 同一份 Plugin-style 业务代码，消除 TLM 与 CH_MEM/Verilog 之间的手工桥接层。

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
| **Verilator** | Verilog 转 C++ 周期精确仿真（CH_MEM 模式 backend） |


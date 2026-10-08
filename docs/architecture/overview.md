# 总体架构设计

> **📌 实现状态快照 (2026-10-08)**
>
> - ✅ **Phase 0 Plugin 脚手架已落地**：`cf::plugin` 6 个头文件（PluginBase / Payload\<T\> / PipeNode / PipeBuilder / CtrlLink / uint_t\<N\>）+ 89/89 框架测试 PASS + 432/432 ctest PASS（TLM binary）
> - ✅ **Plugin-style 单一 source of truth 固化**（ADR-083, 2026-10-08）：业务 IP 不再分别维护 TLM 副本 + RTL 副本；同份代码经由**两个编译模式**产出不同仿真后端
> - ✅ **CH_MEM elaboration 正道**（ADR-040 v2.0）：`cf::plugin::uint_t<N>` 在 `-DCF_PLUGIN_USE_CH_MEM` 下编译为 `ch::core::ch_uint<N>`；业务代码经由 `pb.elaborate(ctx)` 发射 lnode DAG
> - ✅ **Verilator backend 可用**（Phase 6d.5 E8 子集）：`ch::toVerilog(ctx)` → `.v` → `verilator --cc` → 联合仿真；`[verilator]` 1/1 case PASS（5 ELF tohost=1，10 assertions）
> - ✅ **Phase 1.3 全部子任务完成**（2026-06-13）：L1CachePlugin (lookup+refill) + Bridge + Adapter e2e + 5 缓存测试文件 (21 test cases)
> - ✅ **Phase 1.4 完成**（2026-06-13）：L1CachePlugin 设计方法学复盘（[`docs/methodology/plugin-style-design-methodology-v1.md`](../methodology/plugin-style-design-methodology-v1.md)）
> - ✅ **CPU M4/M5 完成**（2026-06-24）：11 Plugin 套件 CpuFactory 真实注册 + DSE 576-config sweep
> - ✅ **MMU 实装 + VIPT 集成 + CPU Pipeline**（2026-09-14）：`mmu-tlb-ptw-impl`（TLB/PTW 算法 + MMUTLMBridge）→ `mmu-cache-integration`（L1Cache 消费 MMU_VADDR VIPT + PIPT fallback）→ `cpu-mmu-integration`（RiscvMMUPlugin 注册 CpuFactory + 3 substage）→ `ptw-walk-bridge-fix`（PTW at_stage 接线 + Bridge 真实 issue_request/read_response）。**317/317 tests PASS**
> - 🚧 **下一里程碑**：`soc-cpu-l1-mmu-demo`（CPU+MMU+L1+Memory 完整 SoC，真 RISC-V 程序 tohost 退出）→ `cache-phase1.5-4way`（VIPT 正式安全 ADR-044 §2.5）→ `mmu-chmem-pipeline-integration`（CH_MEM 真 sv32 translation owner 闭合）。SoC 路线图见 [`soc/cpu/docs/roadmap/`](../../soc/cpu/docs/roadmap/)
>
> **本文档描述目标架构**；具体实现进度以 [`roadmap/roadmap-status.md`](../roadmap/roadmap-status.md) 为准。

## 架构总览

> **设计核心（ADR-083, 2026-10-08）**：ChipForge 用 **Plugin-style** 设计所有硬件模块。**业务 IP 不再分别为 TLM 和 RTL 各写一份**。同一份 Plugin-style 代码经由**两个编译模式**产出不同仿真后端：
> - **TLM 模式**（默认）：`cf::plugin::uint_t<N>` 解析为 POD；`pb.run()` 每周期执行 → C++ 直仿
> - **CH_MEM 模式**（`-DCF_PLUGIN_USE_CH_MEM`，v0.3.0+ 新正道）：`cf::plugin::uint_t<N>` 解析为 `ch::core::ch_uint<N>`；`pb.elaborate(ctx)` 一次性发射 lnode DAG → ① CppHDL Simulator tick 直仿；② `ch::toVerilog(ctx)` 生成 Verilog；③ Verilator 编译+联合仿真

```
+------------------------------------------------------------------+
|                       测试套件层                                  |
|  +------------+ +-------------+ +------------+ +-------------+  |
|  | Bare-metal | |    RTOS     | |   Linux    | |  GPU Tests  |  |
|  |riscv-tests | |FreeRTOS /   | |OpenSBI +   | |  (可扩展)   |  |
|  |riscv-arch  | |Zephyr       | |Linux Kernel| |             |  |
|  +------------+ +-------------+ +------------+ +-------------+  |
+----------------------------+-------------------------------------+
                              | 同一份 Plugin-style 业务代码
+----------------------------v-------------------------------------+
|          Plugin-style 业务 IP (D4 强制, 单一 source of truth)    |
|  ip/{cpu,cache,mmu,memory,...}/plugins/<name>.{h,_chmem.h}       |
|  ----------------------------------------------------------------|
|  · 继承 cf::plugin::PluginBase                                    |
|  · build() 内调 pb.at_stage(name, phase, lambda)                  |
|  · 跨阶段数据流: cf::plugin::Payload<T> + PipeNode               |
|  · 多周期控制: cf::plugin::CtrlLink (halt_when/throw_when/...)    |
|  · 存储抽象: cf::plugin::storage::array_store<T,N>                |
|       (TLM=单缓冲; CH_MEM=双缓冲 commit swap)                    |
|  · 双文件分离: <name>.h (TLM) + <name>_chmem.h (CH_MEM, #ifdef) |
+--------------+----------------+----------------+---------------+
               |                |                |
       (默认编译)        (-DCF_PLUGIN_USE_CH_MEM)
               |                |                |
+--------------v--+ +-----------v-----+ +--------v-------------+
|  TLM 仿真       | |  CppHDL Simulator | |  CppHDL Verilog    |
|  POD 直仿       | |  tick() 直仿     | |  → .v 文件 +        |
|  (pb.run()      | |  (CH_MEM 模式)   | |  Verilator 仿真    |
|   每周期)       | |                  | |  (CH_MEM 模式)     |
|                | |                  | |                    |
|  ~0.6s 全套    | |  CH_MEM binary   | |  cpu_verilator_sim |
|  chipforge_tests| |  ~300s (含 PoC) | |  5 ELF tohost=1   |
+----------------+ +------------------+ +--------------------+
                              |
+-----------------------------v-----------------------------------+
|               CppHDL 设施层 (chipforge 必依赖, 硬件后端)           |
|  ------------------------------------------------------------------ |
|     ch::core::{ch_uint, ch_bool, ch_reg, ch_mem, Context}      |
|     chlib::{ch_state_machine, fifo, stream, axi4lite, ...}     |
|     ch::toVerilog(ctx) → .v 文件                                |
|     VerilatorBackend → 编译 + 联合仿真                          |
|     Simulator::tick() → 周期精确直仿                            |
+----------------------------+-----------------------------------+
                              | (外部依赖, 仅 bridge 适配层用)
+----------------------------v-----------------------------------+
|                CppTLM (legacy 仿真内核)                         |
|  ----------------------------------------------------------------|
|  extern/CppTLM 符号链接保留                                     |
|  src/cf_plugin/bridge/{l1_cache,mmu,...}_bridge.cpp 适配层依赖  |
|  业务 IP 不使用 ch_stream / Module / Port / ModuleFactory      |
+------------------------------------------------------------------+
```

> **架构层级说明**：
> - **业务层**（IP 开发者视角）：写 Plugin-style 代码（`PluginBase` + `at_stage` + `Payload<T>`），源码在两种编译模式下源码相同
> - **编译期分派**：`uint_t<N>` 类型抽象 + `pb.run()` vs `pb.elaborate(ctx)` 调度入口分派到不同后端
> - **设施层**（CppHDL）：提供硬件类型（ch_uint/ch_reg/ch_mem）+ 仿真器（Simulator/Verilator）+ Verilog 代码生成
> - **Legacy 层**（CppTLM）：保留依赖以兼容 bridge 适配层；业务 IP 不再使用

---

## 关键设计原则

### 编译模式与仿真后端切换

Plugin-style 不再使用 `ImplMode` enum（该 enum 在 doc-code-realignment 2026-06-17 已废弃）。取而代之的是**编译期模式开关**与**多后端选择**：

**编译期模式**（CMake 编译选项决定，**不可运行时切换**）：

| 模式 | 编译选项 | `uint_t<N>` 解析为 | 调度入口 | 仿真速度 |
|------|---------|-------------------|---------|---------|
| **TLM**（默认，兼容） | 不加 flag | POD (`uint8_t`/`uint16_t`/`uint32_t`/`uint64_t`) | `pb.run()` 每周期执行 | 最快（~0.6s 全套） |
| **CH_MEM**（v0.3.0+ 新正道，ADR-040 v2.0） | `-DCF_PLUGIN_USE_CH_MEM` | `ch::core::ch_uint<N>` | `pb.elaborate(ctx)` 一次性发射 lnode DAG | 较慢（含 PoC 验证 ~300s） |

**CH_MEM 模式下可选后端**（运行时选择）：

| 后端 | 用途 | 启动方式 |
|------|------|---------|
| **CppHDL Simulator** | 周期精确直仿（C++ 仿真器，无需 EDA 工具） | `ch::core::Simulator::run(ctx, N)` |
| **Verilog 生成** | 输出 `.v` 文件供外部综合/后端流程使用 | `ch::toVerilog(ctx) → file.v` |
| **Verilator 联合仿真** | 周期精确 + 与 C++ 仿真器对比验证 | `verilator --cc file.v → linked library` + VerilatorBackend |

业务代码不感知模式差异——同一份源码经由编译期条件展开绑定到不同类型后端。CI 通过两个独立 binary 验证双模式：

```bash
./build/bin/chipforge_tests            # TLM binary，默认
./build/bin/chipforge_tests_chmem      # CH_MEM binary，需 -DCF_PLUGIN_USE_CH_MEM
ctest --test-dir build -R chipforge_tests_chmem --output-on-failure
```

### IP 独立性与验证环境

所有硬件 IP（cpu / cache / memory / interconnect / peripheral）统一放在 `ip/` 目录下，每个 IP 是**完全独立的可验证单元**：

- 每个 IP 包含 `plugins/`（Plugin-style 业务代码，`<name>.h` + `<name>_chmem.h` 双文件）+ `lib/`（纯 C++ 算法，可选）+ `policies/`（可插拔策略，可选）+ `docs/`（设计文档）+ `configs/`（JSON 测试拓扑）+ `rtl/`（预留：未来 Phase 7+ 真实 RTL 实体）
- 每个 IP 拥有独立的验证环境，由以下组件构成：
  - **驱动模块**（如 TrafficGen）：在 `tests/{module}/` 中实现（测试在仓库根 tests/，不在 ip/{module}/test/）
  - **JSON 最小测试拓扑**：在 `ip/{module}/configs/` 中定义，仅包含待测 IP + 驱动 + 监控
  - **参考模型对比**：与 Spike 等 ISS 结果进行比对

> **注**（ADR-083, 2026-10-08）：早期文档说"每个 IP 包含 `tlm/`、`rtl/` 两个子目录"——这是 Phase 1 早期假设。在 Plugin-style 架构下，业务代码不维护 TLM/RTL 双副本，而是用双文件分离（`<name>.h` + `<name>_chmem.h`）覆盖两种模式。历史命名 `tlm/` 子目录保留为 Plugin-style in TLM mode。
- TLM 模式（默认编译）用于高速功能验证和设计空间探索
- CH_MEM 模式（`-DCF_PLUGIN_USE_CH_MEM`）用于周期精确验证 + Verilog 生成 + Verilator 后端
- 新芯片形态（如 GPU）直接复用已有 IP 库

#### IP 级三层验证模式

```
Level A（单元测试）：纯逻辑验证，无框架依赖，毫秒级反馈
    +-- 直接操作 Bundle 数据结构，验证算法正确性

Level B（集成测试）：ch_stream 握手验证，含 EventQueue
    +-- 驱动模块 tick()，验证 valid/ready 协议和数据流

Level C（端到端测试）：JSON 配置驱动完整拓扑
    +-- TrafficGenTlm -> 被测 IP -> MemoryTlm，统计驱动验证
```

#### IP 独立验证配置示例

```json
{
  "name": "cache_ip_test",
  "modules": [
    {"name": "tg", "type": "TrafficGenTLM",
     "params": {"pattern": "HOTSPOT", "num_requests": 5000}},
    {"name": "cache", "type": "CacheTLM",
     "params": {"size": 32768, "replacement_policy": "LRU"}},
    {"name": "mem", "type": "MemoryTLM", "params": {"latency_ns": 50}}
  ],
  "connections": [
    {"src": "tg", "dst": "cache", "latency": 0},
    {"src": "cache", "dst": "mem", "latency": 1}
  ]
}
```

### Bundle 与跨阶段 PayloadStore 是核心纽带

业务 Bundle（如 `MemReqBundle`、`CacheReqBundle`）定义在 `bundles/`，基于 CppHDL 类型系统（`ch_uint<N>`、`ch_bool`、`bundle_base<Self>`）构建，但不依赖任何仿真引擎——这一性质保证 CH_MEM 模式下 Bundle 字段可直接映射为硬件信号。

**Plugin-style 跨阶段通信**：业务 IP 不再使用 CppTLM `ch_stream<T>` 做模块内部数据流（详见 ADR-083）。取而代之的是 `cf::plugin::Payload<T>` + `PipeNode`：

- **TLM 模式**：`Payload<T>` 持 POD 值，`PipeNode::operator()` 返回值，`pb.run()` 每周期执行闭包 → 跨阶段值传递
- **CH_MEM 模式**：`Payload<T>` 持 `ch::core::ch_uint<N>` 句柄，`pb.elaborate(ctx)` 一次性把阶段间数据流绑定为 lnode → elaboration DAG 自然形成 stage 之间的连接

**为什么不用 `ch_stream<T>`**：D4 决策（2026-06-08）以来，`ch_stream<T>` 强握手语义对单周期同步流水线场景过度；`PipeBuilder::build()` + `Payload<T>` + `CtrlLink` 已验证可替代（ADR-037 v2.0 + ADR-040 v2.0 + L1Cache/MMU/CPU 实测）。

> **历史背景**（Phase 1 早期）：本节之前描述了"CppHDL `__input(Bundle)` / `__output(Bundle)` 定义 RTL 端口"——这是 Phase 5 RTL 实体模型描述方式。在 Plugin-style + CH_MEM 架构下，**不再需要手工桥接**：业务代码用 `Payload<T>` 即可，CH_MEM 模式经 `pb.elaborate(ctx)` 自动产生 stage 间连接。

### SoC 层是 IP 组合器

`soc/` 目录仅负责把 `ip/` 目录下的独立 IP **组合连接**为产品形态：

- 不包含业务逻辑，仅做 IP 实例化和连接
- 不同产品形态只需新建一个 SoC JSON 配置文件
- 当前 Phase 1.3+ 已落地 2 个 L1Cache 验证配置（`l1_cache_minimal.json` + `l1_cache_adapter_e2e.json`），作为新 SoC 配置的参考模板
- 完整的 RISC-V virt SoC 装配（CPU + 多级 Cache + 总线 + 内存 + 外设）推迟到 Phase 2+ 实施

**当前已工作的 SoC 配置示例**（`soc/l1_cache_minimal.json`）：

```json
{
  "name": "l1_cache_minimal",
  "description": "Minimal L1 cache SoC - L1CachePlugin + PicolibcHostMemory",
  "modules": [
    {"name": "cpu",     "type": "TrafficGenPlugin", "params": {"isa": "rv64gc"}},
    {"name": "l1_cache", "type": "L1CachePlugin",   "params": {"size_kb": 32, "assoc": 8}}
  ],
  "connections": [
    {"src": "cpu.ibus", "dst": "l1_cache.cpu_port"},
    {"src": "cpu.dbus", "dst": "l1_cache.cpu_port"}
  ]
}
```

**关于 JSON 装配的现状说明**：

- 当前 `soc/*.json` 是 SoC 装配的目标格式（计划中），但 `soc/` 目录下当前 2 个 L1Cache 配置的实际可工作流程是 CMake/ctest 直接调用对应的 C++ 单元测试（见 `soc/README.md`）
- **业务 IP 全部以 Plugin 风格实现**（`cf::plugin::PluginBase`），不依赖 ModuleFactory 反射机制（参见 ADR-037 + ADR-083）
- `ModuleFactory` + `REGISTER_MODULE` 是 CppTLM 提供的标准装配机制——**仅 Bridge 适配层**（`src/cf_plugin/bridge/`）使用 CppTLM 接口以桥接外部 CppTLM-based 系统；**业务 IP 不使用** ModuleFactory 反射机制

### CppHDL 设施演进（CH_MEM 模式后端）

Plugin-style 在 CH_MEM 模式下经由 CppHDL 设施演进：

```
Phase 0-2: cf::plugin::PluginBase + pb.run() (TLM POD 仿真, ~0.6s 全套)
Phase 6c:  cf::plugin + CH_MEM 编译开关 (-DCF_PLUGIN_USE_CH_MEM)
                -> pb.elaborate(ctx) 发射 lnode DAG
                -> ch::core::Simulator::tick() 直仿 (~300s 全套含 PoC)
Phase 6d:  ch::toVerilog(ctx) 输出 .v 文件
                -> verilator --cc 编译
                -> VerilatorBackend 联合仿真 ([verilator] 1/1 PASS, 5 ELF)
Phase 7+  : 未来扩展 (外部综合/FPGA / 多核 / 形式化验证)
```

业务代码不变——所有演进都发生在 `cf::plugin::uint_t<N>` / `PipeBuilder::elaborate()` 抽象层之下。这正是 Plugin-style 单一 source of truth（ADR-083）的核心收益。

### 多芯片可扩展性

```
chipforge/
+-- ip/
|   +-- cpu/          <- RISC-V core 或 ARM core 或 GPU shader core
|   +-- cache/        <- 通用 L1/L2/LLC（含可插拔替换策略）
|   +-- memory/       <- DRAM/HBM 控制器
|   +-- interconnect/ <- AXI bus 或 GPU NoC
|   +-- peripheral/   <- UART/PLIC 或 GPU display 控制器
+-- soc/
    +-- RiscvVirtSoC       <- RISC-V 产品形态（JSON 配置）
    +-- RiscvEmbedSoC      <- RISC-V 嵌入式形态
    +-- GpuSoC             <- GPU 产品形态（复用 ip/ 下组件）
```

### PayloadStore 接口即 ISA 无关层

> **核心改动（ADR-083, 2026-10-08）**：本节之前描述"ch_stream<Bundle> 接口即 ISA 无关层"——这是 Phase 1 早期假设。在 Plugin-style 架构下，IP 间通信介质是 `cf::plugin::Payload<T>` + `PipeNode`（而非 `ch_stream<T>`）。

**当前状态（Phase 6c/6d, 2026-10-08）**：

- ✅ **业务 IP 跨阶段通信通过 `Payload<T>` + `PipeNode`**（TLM 模式 + CH_MEM 模式同一套 API）
- ✅ CPU IP 实装：M4/M5 11 个 Plugin 套件（CpuFactory 注册 → `pb.at_stage(...)`），加上 DSE 576-config sweep
- ✅ MMU IP 实装：MMUPlugin + RiscvMMUPlugin（`ip/mmu/tlm/`），CH_MEM 版待 mmu-chmem-pipeline-integration owner 闭合
- ✅ L1Cache IP 实装：L1CachePlugin（TLM 完整 + CH_MEM refill_fsm stub）
- ✅ Memory IP：PicolibcHostMemory（PoC 替代，待 MemoryPlugin 全面实装）

**设计原则**：

- 模块间仅通过 `Payload<T>` 数据结构通信（编译期类型检查 + 运行期 typeid 二次校验）
- Cache、Memory、Interconnect 等 IP 完全 ISA 无关（仅暴露 `Payload<T>` 接口）
- CH_MEM 模式下，`Payload<T>` 句柄展开为 lnode → elaboration DAG 自然形成连接
- 新增 ISA 支持仅需实现一个新的 CPU Plugin，无需修改其余组件

### 可插拔策略模式（Policy Pattern）

每个硬件模块应将**可变算法**与**固定骨架**分离，支持通过 JSON 配置切换不同策略实现：

```
组件 = 骨架（固定硬件结构） + 策略（可替换算法）

例如：
L1Cache (Plugin 风格) = 缓存骨架 + 替换策略 + 预取策略
MMU TLB (Plugin 风格, mmu-ip-skeleton 2026-06-29) = TLB 骨架 + 替换策略 + 各级配置
BusMatrix (Plugin 风格) = 总线骨架 + 仲裁策略
NoC Router (Plugin 风格) = 路由器骨架 + 路由算法
CPU Pipeline (Plugin 风格) = 流水线骨架 + 分支预测策略
```

#### 策略接口规范

```cpp
// cache/policies/replacement_policy.h
class ReplacementPolicy {
public:
    virtual ~ReplacementPolicy() = default;
    virtual void on_access(uint32_t set, uint32_t way) = 0;
    virtual uint32_t select_victim(uint32_t set) = 0;
    virtual void on_insert(uint32_t set, uint32_t way) = 0;
    virtual std::string name() const = 0;

    // 工厂方法：根据 JSON 配置名称创建策略实例
    static std::unique_ptr<ReplacementPolicy> create(const std::string& name);
};

// 具体策略实现
class LRUPolicy    : public ReplacementPolicy { /* 最近最少使用 */ };
class PLRUPolicy   : public ReplacementPolicy { /* 伪 LRU（树型，低开销）*/ };
class RandomPolicy : public ReplacementPolicy { /* 纯随机（无状态）*/ };
class FIFOPolicy   : public ReplacementPolicy { /* 先进先出 */ };
class RRIPPolicy   : public ReplacementPolicy { /* Re-reference Interval Prediction */ };
```

#### 策略应用领域

| 组件 | 策略维度 | 可选实现 |
|------|---------|---------|
| **L1/L2 Cache** | 替换策略 | LRU, PLRU, Random, FIFO, RRIP, NRU — **✅ Phase 1.4 落地 (NoReplacementPolicy + LRUPolicy 接口契约，详见 `ip/cache/policies/`)** |
| **L1/L2 Cache** | 预取策略 | None, Stride, NextLine, Indirect, AMPM |
| **L1/L2 Cache** | 写策略 | WriteThrough, WriteBack, WriteAllocate |
| **BusMatrix** | 仲裁策略 | RoundRobin, Priority, WeightedFair |
| **NoC Router** | 路由算法 | XY, YX, WestFirst, Adaptive, Minimal |
| **CPU Pipeline** | 分支预测 | Static, BTB, GShare, TAGE, Perceptron |

#### JSON 配置驱动策略选择

```json
{
  "name": "l1_cache",
  "type": "CacheTLM",
  "params": {
    "size": 32768,
    "assoc": 8,
    "line_size": 64,
    "replacement_policy": "PLRU",
    "prefetcher": {"type": "stride", "degree": 2},
    "write_policy": "WriteBack"
  }
}
```

---

## 工程目录结构

### 已建目录

```
chipforge/
+-- CMakeLists.txt
+-- cmake/
|   +-- modules/                    # CMake 辅助模块
|
+-- configs/                        # * JSON 配置层（DSE + SoC 形态）
|   +-- soc/
|   |   +-- riscv_virt.json         # RV64GC virt SoC 配置
|   |   +-- riscv_embed.json        # RV32IMC embed SoC 配置
|   |   +-- gpu_compute.json        # GPU 计算 SoC 配置（Phase 5+）
|   +-- sweep/                      # 设计空间探索参数扫描
|   |   +-- cache_sweep.json        # 缓存参数扫描定义
|   |   +-- noc_sweep.json          # NoC 拓扑参数扫描
|   +-- policies/                   # 可插拔策略注册表
|       +-- replacement_policies.json
|       +-- prefetch_policies.json
|
+-- bundles/                        # * 共享 Bundle（Plugin-style Payload + CppHDL 类型系统）
|   +-- mem_bundles.h               # MemReqBundle, MemRespBundle (基于 ch_uint<N>)
|   +-- cache_bundles.h             # CacheReqBundle, CacheRespBundle
|   +-- tlb_bundles_extension.h     # TLB-specific Bundle 扩展
|                                   # 注: impl_mode.h 已废弃（2026-06-17 doc-code-realignment）
|
+-- include/cf/plugin/              # * Plugin 框架核心头 (D4 决策, ADR-037)
|   +-- plugin_base.h               # PluginBase + 禁止 tick() (delete)
|   +-- pipe_builder.h              # PipeBuilder + at_stage() + Phase + CtrlLink
|   +-- pipe_node.h                 # PipeNode + PayloadStore
|   +-- payload.h                   # Payload<T> + PayloadStore + get/has/operator()
|   +-- ctrl_link.h                 # CtrlLink: halt_when/throw_when/flush_when/bypass
|   +-- uint_t.h                    # uint_t<N>/bool_t 类型抽象 (TLM=POD, CH_MEM=ch_uint<N>)
|   +-- storage/                    # array_store<T,N> + commit_hook (TLM 单缓冲, CH_MEM 双缓冲)
|   +-- capability_table.h          # ADR-082 negotiate() capability 协商
|
+-- src/cf_plugin/                  # * Plugin 框架实现 + Bridge 适配层
|   +-- bridge/                     # Bridge 适配层 (L1Cache/MMU/...) — 依赖 CppTLM 接口
|                                   # 注: 业务 IP 不在 src/cf_plugin/, 在 ip/<area>/plugins/
|
+-- ip/                             # * IP 组件库（Plugin-style, 单一 source of truth, ADR-083）
|   +-- cpu/                        # CPU IP — Plugin-style 套件 (M4/M5 完成)
|   |   +-- plugins/                # 业务代码: <name>.h + <name>_chmem.h 双文件
|   |   |   +-- ibus.{h,_chmem.h}   # 11 个 Plugin: ibus/dbus/decode/branch/...
|   |   |   +-- dbus.{h,_chmem.h}
|   |   |   +-- decode_chmem.h
|   |   |   +-- branch_chmem.h
|   |   |   +-- hazard.{h,_chmem.h}
|   |   |   +-- mul_div_fsm_chmem.h
|   |   |   +-- reg_file.{h,_chmem.h}
|   |   |   +-- mmu.{h,cpp}
|   |   |   +-- mmu_ptw_chmem.h
|   |   |   +-- mmu_exception_handler.{h,_chmem.h}
|   |   |   +-- stage_link.h
|   |   +-- cpu_factory.h           # CpuFactory (TLM mode)
|   |   +-- cpu_factory_chmem.h     # CpuFactoryChmem (CH_MEM mode)
|   |   +-- arch/                   # RISC-V 算法库 (lib/, 不依赖 cf::plugin)
|   |   +-- core/                   # Payload Key 集中定义
|   |   +-- docs/                   # RISC-V 架构文档
|   |   +-- rtl/                    # 预留: 未来 Phase 7+ 真实 RTL 实体 (暂空)
|   |   +-- configs/                # CPU IP 测试配置
|   |
|   +-- cache/                      # Cache IP — Plugin-style L1CachePlugin
|   |   +-- tlm/                    # 历史命名: Plugin-style in TLM mode
|   |   |   +-- L1CachePlugin.{h,cpp}
|   |   |   +-- cache_keys.h
|   |   |   +-- l1_cache_refill_fsm_chmem.h   # CH_MEM refill FSM stub
|   |   +-- policies/               # 可插拔策略实现 (LRU/PLRU/Random/FIFO/RRIP)
|   |   +-- configs/                # JSON 测试拓扑
|   |   +-- rtl/                    # 预留: 未来 Phase 7+ 真实 RTL 实体 (暂空)
|   |
|   +-- mmu/                        # MMU IP — Plugin-style MMUPlugin + RiscvMMUPlugin
|   |   +-- tlm/                    # 历史命名: Plugin-style in TLM mode
|   |   |   +-- MMUPlugin.{h,cpp}
|   |   |   +-- mmu_keys.h
|   |   +-- lib/                    # 纯 C++ 算法 (TLB/PTW/MultiLevelTLB)
|   |   +-- policies/               # 替换策略
|   |   +-- configs/                # JSON 测试拓扑
|   |   +-- rtl/                    # 预留 (暂空)
|   |
|   +-- memory/                     # Memory IP — PicolibcHostMemory PoC
|   |   +-- tlm/                    # Plugin-style in TLM mode
|   |   +-- rtl/                    # 预留 (暂空)
|   |   +-- configs/
|   |
|   +-- interconnect/               # Interconnect IP — 占位, 待实装
|   +-- peripheral/                 # Peripheral IP — 占位, 待实装 (Phase 3+)
|   +-- tilecopy/, tilecore/        # 实验性 IP
|
+-- soc/                            # * SoC 组合层（用 IP 装配产品形态）
|   +-- MemoryMap.h                 # 统一内存地图
|   +-- RiscvVirtSoC 等具体 SoC 类 （Phase 2+ 实施）
|
+-- extern/                         # 外部依赖符号链接
|   +-- CppTLM/                     # legacy 仿真内核 (仅 bridge 适配层依赖)
|   +-- CppHDL/                     # 硬件后端 (CH_MEM 模式必依赖)
|   +-- Catch2/                    # 测试框架 v3.7.0
|
+-- metrics/                        # * 统计收集框架 (规划中, Phase 2 创建)
|   +-- statistics.h                # Scalar/Distribution/Vector/Formula
|   +-- stat_group.h                # 层次化统计组
|   +-- stat_manager.h              # 全局统计管理器
|   +-- stat_exporter.h             # JSON/CSV/gem5 格式导出
|
+-- verification/                   # 验证基础设施（跨 IP 共用）(规划中, Phase 3 创建)
|   +-- ScoreBoard.h/cpp            # TLM vs CH_MEM/Verilator 执行迹对比
|   +-- SpikeBridge.h/cpp           # Spike co-simulation 接口
|   +-- CoverageCollector.h/cpp     # 功能覆盖率收集
|
+-- sw/                             # 软件镜像（git submodules）(规划中, Phase 2 创建)
|   +-- baremetal/
|   |   +-- riscv-tests/            # 官方 ISA 测试
|   |   +-- riscv-arch-test/        # 合规测试
|   |   +-- custom/                 # 自定义功能测试
|   +-- rtos/
|   |   +-- freertos/               # FreeRTOS + 应用
|   |   +-- zephyr/                 # Zephyr 应用 + BSP
|   +-- linux/
|       +-- opensbi/                # OpenSBI 固件
|       +-- u-boot/                 # U-Boot 引导器
|       +-- linux/                  # Linux Kernel
|       +-- buildroot/              # 根文件系统
|
+-- tools/                          # * DSE 工具链
|   +-- dse/                        (规划中, Phase 2 创建)
|   |   +-- sweep_driver.py         # 参数扫描驱动（并行执行）
|   |   +-- pareto_analyzer.py      # Pareto 前沿计算
|   |   +-- sensitivity_plot.py     # 敏感性分析可视化
|   +-- config_gen/                 (规划中, Phase 2 创建)
|       +-- topology_generator.py   # 拓扑配置生成工具
|
+-- scripts/                        (规划中, Phase 2 创建)
    +-- run_tests.py                # 测试驱动脚本
    +-- run_dse.py                  # 设计空间探索入口脚本
    +-- analyze_results.py          # DSE 结果分析
    +-- gen_report.py               # 覆盖率报告生成
```


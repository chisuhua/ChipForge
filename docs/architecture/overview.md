# 总体架构设计

## 架构总览

```
+------------------------------------------------------------------+
|                       测试套件层                                  |
|  +------------+ +-------------+ +------------+ +-------------+  |
|  | Bare-metal | |    RTOS     | |   Linux    | |  GPU Tests  |  |
|  |riscv-tests | |FreeRTOS /   | |OpenSBI +   | |  (可扩展)   |  |
|  |riscv-arch  | |Zephyr       | |Linux Kernel| |             |  |
|  +------------+ +-------------+ +------------+ +-------------+  |
+----------------------------+-------------------------------------+
                             | 统一平台接口
+----------------------------v-------------------------------------+
|                      SoC 组合层                                   |
|  RiscvVirtSoC (RV64GC)  RiscvEmbedSoC (RV32IMC)  GpuSoC ...     |
|  按产品形态装配组件，ImplMode 决定 TLM/RTL 混插比例               |
+---------+------------------------------------------+-------------+
          | ch_stream<T>                              | Port<T>
+---------v----------------------------------+    +---v-------------------------------+
|     CppTLM 组件层           |    |      CppHDL 组件层             |
|  高速功能建模               |    |  周期精确 RTL 建模             |
|  -------------------------  |    |  ----------------------------- |
|  cpu/tlm/  RiscvIssTlm     |    |  cpu/rtl/  RiscvCoreRtl        |
|  cache/tlm/ L1/L2CacheTlm  |    |  cache/rtl/ L1CacheRtl         |
|  memory/tlm/ DramTlm       |    |  memory/rtl/ DramCtrlRtl       |
|  interconnect/tlm/ BusTlm  |    |  interconnect/rtl/ CrossbarRtl |
|  peripheral/tlm/ Uart/Plic |    |  peripheral/rtl/ UartRtl       |
+---------+----------------------------------+    +---+-------------------------------+
          |                                           |
          +------------------+------------------------+
                             | 共享 Bundle 定义
+----------------------------v-------------------------------------+
|                     共享 Bundle 层                                |
|  bundles/  MemReqBundle  CacheBundle  NoCBundle  IntBundle        |
|            ImplMode      TxnTracker   MemoryMap                   |
+------------------------------------------------------------------+
```

### 组件模式切换

SoC 组合层在构建时传入 `ImplMode`，相同的 Bundle 接口保证组件可以无缝替换：

```
ImplMode::TLM_ONLY   - 全 TLM，最高仿真速度（功能验证、RTOS、Linux）
ImplMode::RTL_ONLY   - 全 RTL，周期精确（RTL 调试、时序分析）
ImplMode::COMPARE    - TLM + RTL 并行，ScoreBoard 自动对比（协同验证）
ImplMode::SHADOW     - RTL 跟踪 TLM，用于 RTL 调试
```

---

## 关键设计原则

### IP 独立性与验证环境

所有硬件 IP（cpu / cache / memory / interconnect / peripheral）统一放在 `ip/` 目录下，每个 IP 是**完全独立的可验证单元**：

- 每个 IP 包含 `tlm/`、`rtl/`、`test/`、`configs/` 四个子目录
- 每个 IP 拥有独立的 CppTLM 验证环境（TrafficGen 驱动 + JSON 最小测试拓扑）
- TLM 实现用于高速验证和设计空间探索，RTL 实现用于周期精确验证
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
    {"name": "tg", "type": "TrafficGenTlm",
     "params": {"pattern": "HOTSPOT", "num_requests": 5000}},
    {"name": "cache", "type": "L1CacheTlm",
     "params": {"size_kb": 32, "replacement_policy": "LRU"}},
    {"name": "mem", "type": "DramTlm", "params": {"latency_ns": 50}}
  ],
  "connections": [
    {"src": "tg", "dst": "cache", "latency": 0},
    {"src": "cache", "dst": "mem", "latency": 1}
  ]
}
```

### Bundle 是核心纽带

`bundles/` 目录的数据结构定义**同时被 CppTLM 和 CppHDL 引用**：

- CppTLM 中：`ch_stream<MemReqBundle>` 传递事务
- CppHDL 中：`Port<MemReqBundle>` 定义 RTL 端口
- 无需手工桥接层，接口天然一致

### SoC 层是 IP 组合器

`soc/` 目录仅负责把 `ip/` 目录下的独立 IP **组合连接**为产品形态：

- 不包含业务逻辑，仅做 IP 实例化和连接
- 根据 `ImplMode` 参数决定各 IP 用 TLM 还是 RTL 实现
- 不同产品形态只需新建一个 SoC JSON 配置文件
- 通过 CppTLM 的 `ModuleFactory` + JSON 配置实现零代码装配

```json
{
  "name": "RiscvVirtSoC",
  "impl_mode": "TLM_ONLY",
  "modules": [
    {"name": "cpu_0", "type": "RiscvIssTlm", "params": {"isa": "rv64gc"}},
    {"name": "l1i",   "type": "L1CacheTlm", "params": {"size_kb": 32, "assoc": 8}},
    {"name": "l1d",   "type": "L1CacheTlm", "params": {"size_kb": 32, "assoc": 8}},
    {"name": "l2",    "type": "L2CacheTlm", "params": {"size_kb": 512}},
    {"name": "bus",   "type": "BusMatrixTlm"},
    {"name": "dram",  "type": "DramTlm", "params": {"size_mb": 512}},
    {"name": "uart",  "type": "UartTlm"},
    {"name": "clint", "type": "ClintTlm"},
    {"name": "plic",  "type": "PlicTlm"}
  ],
  "connections": [
    {"src": "cpu_0.ibus", "dst": "l1i.cpu_port"},
    {"src": "cpu_0.dbus", "dst": "l1d.cpu_port"},
    {"src": "l1i.mem_port", "dst": "bus.port.0"},
    {"src": "l1d.mem_port", "dst": "bus.port.1"},
    {"src": "bus.port.2", "dst": "l2.cpu_port"},
    {"src": "l2.mem_port", "dst": "dram.port"},
    {"src": "bus.port.3", "dst": "uart.port"},
    {"src": "bus.port.4", "dst": "clint.port"},
    {"src": "bus.port.5", "dst": "plic.port"}
  ]
}
```

### CppHDL 渐进演进

```
Phase 1-4：Component -> Simulator::run()    （C++ 直接仿真，无需 EDA 工具）
Phase 5  ：Component -> VerilogCodeGen      （输出 Verilog）
                    -> Verilator 编译       （周期精确 C++ 仿真）
                    -> 与 TLM 执行迹对比   （协同验证）
```

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

### ch_stream 接口即 ISA 无关层

ISA 无关性**无需抽象基类**，而是通过 `ch_stream<Bundle>` 接口天然实现：

- RISC-V Core 暴露 `ch_stream<MemReqBundle>` 指令/数据总线接口
- ARM Core 暴露相同的 `ch_stream<MemReqBundle>` 接口
- GPU Shader Core 暴露相同的 `ch_stream<MemReqBundle>` 接口

在 SoC 组合层，任何暴露相同 Bundle 接口的 CPU IP 都可以通过 JSON 配置互换：

```json
{
  "modules": [
    {"name": "cpu_0", "type": "RiscvIssTlm", "params": {"isa": "rv64gc"}},
    {"name": "l1_cache", "type": "L1CacheTlm", "params": {"size_kb": 32}}
  ],
  "connections": [
    {"src": "cpu_0.dbus", "dst": "l1_cache.cpu_port", "latency": 0}
  ]
}
```

切换为 ARM 核心只需更改一行配置：
```json
    {"name": "cpu_0", "type": "ArmIssTlm", "params": {"profile": "cortex-a55"}}
```

**设计原则**：
- 模块间仅通过 Bundle 数据结构通信（`ch_stream<T>` 握手协议）
- Cache、Memory、Interconnect 等 IP 完全 ISA 无关
- 测试框架按 ISA 类型自动选择测试套件，但 IP 验证环境可跨 ISA 复用
- 新增 ISA 支持仅需实现一个新的 CPU IP（继承 `ChStreamModuleBase`），无需修改其余组件

### 可插拔策略模式（Policy Pattern）

每个硬件模块应将**可变算法**与**固定骨架**分离，支持通过 JSON 配置切换不同策略实现：

```
组件 = 骨架（固定硬件结构） + 策略（可替换算法）

例如：
L1CacheTlm = 缓存骨架 + 替换策略 + 预取策略
BusMatrixTlm = 总线骨架 + 仲裁策略
NoCTlm = 路由器骨架 + 路由算法
RiscvIssTlm = 流水线骨架 + 分支预测策略
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
| **L1/L2 Cache** | 替换策略 | LRU, PLRU, Random, FIFO, RRIP, NRU |
| **L1/L2 Cache** | 预取策略 | None, Stride, NextLine, Indirect, AMPM |
| **L1/L2 Cache** | 写策略 | WriteThrough, WriteBack, WriteAllocate |
| **BusMatrix** | 仲裁策略 | RoundRobin, Priority, WeightedFair |
| **NoC Router** | 路由算法 | XY, YX, WestFirst, Adaptive, Minimal |
| **CPU Pipeline** | 分支预测 | Static, BTB, GShare, TAGE, Perceptron |

#### JSON 配置驱动策略选择

```json
{
  "name": "l1_cache",
  "type": "L1CacheTlm",
  "params": {
    "size_kb": 32,
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
+-- bundles/                        # * 共享 Bundle（TLM/RTL 共用）
|   +-- mem_bundles.h               # MemReqBundle, MemRespBundle
|   +-- cache_bundles.h             # CacheReqBundle, CacheRespBundle
|   +-- noc_bundles.h               # NoCFlitBundle
|   +-- int_bundles.h               # IntBundle（PLIC/CLINT 中断）
|   +-- impl_mode.h                 # ImplMode: TLM/RTL/COMPARE/SHADOW
|
+-- ip/                             # * IP 组件库（每个 IP 独立验证）
|   +-- cpu/                        # CPU IP
|   |   +-- tlm/
|   |   |   +-- RiscvIssTlm.h/cpp   # RISC-V ISS（Spike 封装）
|   |   |   +-- ArmIssTlm.h/cpp     # ARM ISS（未来扩展）
|   |   |   +-- TrafficGenTlm.h/cpp # 通用流量驱动器
|   |   +-- rtl/
|   |   |   +-- RiscvCoreRtl.h/cpp  # CppHDL RISC-V 核心
|   |   |   +-- Pipeline.h/cpp      # 流水线 RTL 模型
|   |   +-- test/                   # CPU IP 独立验证环境
|   |   |   +-- test_riscv_iss.cc   # ISS 单元测试
|   |   |   +-- test_cpu_e2e.cc     # CPU 端到端测试
|   |   +-- configs/                # CPU IP 测试配置
|   |       +-- cpu_minimal_test.json
|   |
|   +-- cache/                      # Cache IP
|   |   +-- policies/               # 可插拔策略实现
|   |   |   +-- replacement_policy.h
|   |   |   +-- lru_policy.h/cpp
|   |   |   +-- plru_policy.h/cpp
|   |   |   +-- random_policy.h/cpp
|   |   |   +-- prefetch_policy.h/cpp
|   |   +-- tlm/
|   |   |   +-- L1CacheTlm.h/cpp    # L1 TLM 模型
|   |   |   +-- L2CacheTlm.h/cpp    # L2 TLM 模型
|   |   +-- rtl/
|   |   |   +-- L1CacheRtl.h/cpp    # CppHDL L1 Cache
|   |   +-- test/                   # Cache IP 独立验证环境
|   |   |   +-- test_cache_unit.cc  # 策略单元测试
|   |   |   +-- test_cache_stream.cc # ch_stream 集成测试
|   |   |   +-- test_cache_e2e.cc   # TrafficGen->Cache->Mem E2E
|   |   +-- configs/
|   |       +-- cache_minimal.json  # 最小测试拓扑
|   |       +-- cache_sweep.json    # 缓存 DSE 配置
|   |
|   +-- memory/                     # Memory IP
|   |   +-- tlm/
|   |   |   +-- DramTlm.h/cpp       # DRAM 模型（含 DMI 加速）
|   |   |   +-- RomTlm.h/cpp        # ROM / Flash 模型
|   |   +-- rtl/
|   |   |   +-- DramCtrlRtl.h/cpp   # DDR 控制器 RTL
|   |   +-- test/
|   |   |   +-- test_memory_e2e.cc
|   |   +-- configs/
|   |       +-- memory_test.json
|   |
|   +-- interconnect/               # Interconnect IP
|   |   +-- tlm/
|   |   |   +-- BusMatrixTlm.h/cpp  # AXI/TileLink 总线
|   |   |   +-- CrossbarTlm.h/cpp   # Crossbar 交叉开关
|   |   |   +-- NoCRouterTlm.h/cpp  # Mesh/Ring NoC 路由器
|   |   +-- rtl/
|   |   |   +-- CrossbarRtl.h/cpp
|   |   +-- test/
|   |   |   +-- test_bus_e2e.cc
|   |   |   +-- test_noc_mesh.cc
|   |   +-- configs/
|   |       +-- bus_test.json
|   |       +-- mesh_4x4.json
|   |
|   +-- peripheral/                 # Peripheral IP
|       +-- tlm/
|       |   +-- UartTlm.h/cpp       # NS16550A UART
|       |   +-- ClintTlm.h/cpp      # CLINT（mtime/mtimecmp）
|       |   +-- PlicTlm.h/cpp       # PLIC（多优先级中断）
|       |   +-- VirtioBlockTlm.h/cpp # VirtIO Block
|       |   +-- VirtioNetTlm.h/cpp  # VirtIO Net
|       +-- rtl/
|       |   +-- UartRtl.h/cpp
|       +-- test/
|       |   +-- test_uart.cc
|       |   +-- test_plic.cc
|       +-- configs/
|           +-- peripheral_test.json
|
+-- soc/                            # * SoC 组合层（用 IP 装配产品形态）
|   +-- MemoryMap.h                 # 统一内存地图
|   +-- RiscvVirtSoC.h/cpp          # RV64GC virt（用于 Linux）
|   +-- RiscvEmbedSoC.h/cpp         # RV32IMC embed（用于 RTOS）
|   +-- MultiCoreSoC.h/cpp          # SMP 多核形态
|   +-- GpuSoC.h/cpp               # GPU 形态（Phase 5+）
|
+-- metrics/                        # * 统计收集框架
|   +-- statistics.h                # Scalar/Distribution/Vector/Formula
|   +-- stat_group.h                # 层次化统计组
|   +-- stat_manager.h              # 全局统计管理器
|   +-- stat_exporter.h             # JSON/CSV/gem5 格式导出
|
+-- verification/                   # 验证基础设施（跨 IP 共用）
|   +-- ScoreBoard.h/cpp            # TLM vs RTL 执行迹对比
|   +-- SpikeBridge.h/cpp           # Spike co-simulation 接口
|   +-- CoverageCollector.h/cpp     # 功能覆盖率收集
|
+-- sw/                             # 软件镜像（git submodules）
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
|   +-- dse/
|   |   +-- sweep_driver.py         # 参数扫描驱动（并行执行）
|   |   +-- pareto_analyzer.py      # Pareto 前沿计算
|   |   +-- sensitivity_plot.py     # 敏感性分析可视化
|   +-- config_gen/
|       +-- topology_generator.py   # 拓扑配置生成工具
|
+-- scripts/
    +-- run_tests.py                # 测试驱动脚本
    +-- run_dse.py                  # 设计空间探索入口脚本
    +-- analyze_results.py          # DSE 结果分析
    +-- gen_report.py               # 覆盖率报告生成
```


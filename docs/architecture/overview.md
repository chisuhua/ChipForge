# 总体架构设计

> **📌 实现状态快照 (2026-06-13)**
>
> - ✅ **Phase 0 Plugin 脚手架已落地**：`cf::plugin` 5 个头文件（PluginBase / Payload\<T\> / PipeNode / PipeBuilder / CtrlLink）+ 51/51 单元测试 PASS + 16/16 ctest PASS
> - ✅ **框架层已就位**：CppTLM (TLM 建模) + CppHDL (RTL/lnode DAG) 集成完成，`cpptlm_core` / `cpphdl` 目标可达
> - ✅ **应用层 Phase 1.3 全部子任务完成**（2026-06-13, 含 1.3d-extras）：`bundles/mem_bundles.h` 6 个 Bundle (MemReq/MemResp/CacheReq/CacheResp/L1CachePluginBundle/IntBundle, D4 合规) + `ip/cache/tlm/L1CachePlugin.{h,cpp}` (lookup + refill 两阶段, 256 sets × 64B direct-mapped, 4/4 单元测试) + `src/cf_plugin/bridge/l1_cache_bridge.{h,cpp}` (L1CacheTLMBridge, 2/2 测试) + `src/cf_plugin/bridge/l1_cache_bridge_adapter.{h,cpp}` (cpptlm ModuleFactory 兼容层 + ch_stream 4 字段窄桥注册, 5/5 e2e + 5/5 instantiateAll) + `soc/l1_cache_minimal.json` (静态验证 spec) + `soc/l1_cache_adapter_e2e.json` (full JSON instantiateAll spec) + `ip/cache/configs/params_schema.json` (JSON Schema draft-07)
> - 🚧 **下一里程碑 (Phase 1.4)**：`cpptlm::CacheTLM` baseline 对比 (PA-7) + baseline 决策草案 (PA-9)；Phase 2+ 应用层（CPU / memory / interconnect / peripheral）待建设
>
> **本文档描述目标架构**；具体实现进度以 [`roadmap/roadmap-status.md`](../roadmap/roadmap-status.md) 为准。

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
|  （Phase 2+ 实施）RV64GC virt / RV32IMC embed / GPU SoC ...      |
|  按产品形态装配组件，ImplMode 决定 TLM/RTL 混插比例               |
+---------+------------------------------------------+-------------+
          | ch_stream<T>                              | Port<T>
          | （模块内部设计接口）                      | （RTL 组件端口）
          | StreamAdapter 自动映射为 Port              |
+---------v----------------------------------+    +---v-------------------------------+
|     CppTLM 组件层           |    |      CppHDL 组件层             |
|  高速功能建模               |    |  周期精确 RTL 建模             |
|  -------------------------  |    |  ----------------------------- |
|  cpu/tlm/  CPUTLM          |    |  cpu/rtl/  （Phase 5 实施）    |
|  cache/tlm/ CacheTLM       |    |  cache/rtl/ （Phase 5 实施）    |
|  memory/tlm/ MemoryTLM     |    |  memory/rtl/ （Phase 5 实施）  |
|  interconnect/tlm/ CrossbarTLM | | interconnect/rtl/ （Phase 5）|
|  peripheral/tlm/ （Phase 3+ 实施）| | peripheral/rtl/（Phase 5+）|
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

> **架构层级说明：**
> - **模块设计层**（IP 开发者视角）：使用 `ch_stream<Bundle>` 定义数据流
> - **框架组装层**（SoC 集成者视角）：使用 Port 连接 + JSON 配置
> - **StreamAdapter**：自动桥接两层，IP 开发者和 SoC 集成者各自独立工作
>
> ```
> IP 开发者 ──→ ch_stream<T> ──→ [StreamAdapter] ──→ Port ──→ SoC 集成者
>            （模块内部）      （自动转换，透明）   （JSON配置引用）
> ```

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
- 每个 IP 拥有独立的验证环境，由以下组件构成：
  - **驱动模块**（如 TrafficGen）：在 `ip/{module}/test/` 中实现，负责生成激励
  - **JSON 最小测试拓扑**：在 `ip/{module}/configs/` 中定义，仅包含待测 IP + 驱动 + 监控
  - **参考模型对比**：与 Spike 等 ISS 结果进行比对

> 注：TrafficGen 等验证工具由 ChipForge 项目实现，CppTLM 仅提供框架支持。
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

### Bundle 是核心纽带

`ChipForge/bundles/` 目录定义所有业务 Bundle（如 MemReqBundle、CacheReqBundle），这些 Bundle 基于 CppHDL 的类型系统（`ch_uint<N>`、`ch_bool`、`bundle_base<Self>`）构建，但不依赖任何一个框架的仿真引擎。CppTLM 通过序列化/反序列化将 Bundle 转换为内部 Packet 进行传输；CppHDL 直接操作 Bundle 信号。

- CppTLM 中：模块设计时通过 `ch_stream<MemReqBundle>` 定义数据流接口。框架通过 StreamAdapter 自动将其映射为 Port，SoC 级 JSON 配置通过 Port 名称完成连接。模块开发者只需关注 ch_stream 层面的数据流设计。
- CppHDL 中：`__input(Bundle)` / `__output(Bundle)` 定义 RTL 端口（直接操作 Bundle 信号）
- 无需手工桥接层，接口天然一致

### SoC 层是 IP 组合器

`soc/` 目录仅负责把 `ip/` 目录下的独立 IP **组合连接**为产品形态：

- 不包含业务逻辑，仅做 IP 实例化和连接
- 不同产品形态只需新建一个 SoC JSON 配置文件
- 当前 Phase 1.3 已落地 2 个 L1Cache 验证配置（`l1_cache_minimal.json` + `l1_cache_adapter_e2e.json`），作为新 SoC 配置的参考模板
- 完整的 RISC-V virt SoC 装配（CPU + 多级 Cache + 总线 + 内存 + 外设）推迟到 Phase 2+ 实施

**当前已工作的 SoC 配置示例**（`soc/l1_cache_minimal.json`）：

```json
{
  "name": "l1_cache_minimal",
  "description": "Minimal L1 cache SoC - L1CachePlugin + CppTLM CacheTLM",
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
- ModuleFactory + `REGISTER_MODULE` 宏机制是 CppTLM 提供的标准装配机制，将随 Phase 2 RISC-V virt SoC 实施时启用
- 业务 IP 全部以 Plugin 风格实现（`cf::plugin::PluginBase`），不依赖 ModuleFactory 反射机制（参见 ADR-037 + ADR-041）

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

> ⚠️ 推迟到 Phase 1.4+ 实际有第 2 个 CPU IP 时再验证

**当前 Phase 1.3 状态**：

- 仅有 1 个 CPU IP（`ip/cpu/` 处于 M4 Plugin + Stageable 设计中，尚未提供对外可工作的 TLM 模型）
- 唯一对外可工作的"CPU 流量"是 `tests/` 中的 TrafficGen / Driver（不属于 IP 层）
- 因此 `ch_stream<Bundle>` 接口的"ISA 无关层"性质当前**没有第 2 个 CPU IP 来验证**

**设计原则（待 Phase 1.4+ 实施时再验证）**：

- 模块间仅通过 Bundle 数据结构通信（`ch_stream<T>` 握手协议）
- Cache、Memory、Interconnect 等 IP 完全 ISA 无关（仅暴露 `ch_stream<MemReqBundle>` 接口）
- 新增 ISA 支持仅需实现一个新的 CPU IP，无需修改其余组件

### 可插拔策略模式（Policy Pattern）

每个硬件模块应将**可变算法**与**固定骨架**分离，支持通过 JSON 配置切换不同策略实现：

```
组件 = 骨架（固定硬件结构） + 策略（可替换算法）

例如：
L1Cache (Plugin 风格) = 缓存骨架 + 替换策略 + 预取策略
MMU TLB (Plugin 风格, mmu-ip-skeleton 2026-06-29) = TLB 骨架 + 替换策略 + 各级配置
CrossbarTLM = 总线骨架 + 仲裁策略
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
+-- bundles/                        # * 共享 Bundle（TLM/RTL 共用）
|   +-- mem_bundles.h               # MemReqBundle, MemRespBundle
|   +-- cache_bundles.h             # CacheReqBundle, CacheRespBundle
|   +-- noc_bundles.h               # NoCFlitBundle
|   +-- int_bundles.h               # IntBundle（PLIC/CLINT 中断）
|   +-- impl_mode.h                 # ImplMode: TLM/RTL/COMPARE/SHADOW
|
+-- ip/                             # * IP 组件库（每个 IP 独立验证）
|   +-- cpu/                        # CPU IP
|   |   +-- tlm/                    # （RISC-V ISS 推迟到 Phase 2 实施）
|   |   +-- rtl/                    # （Phase 5 实施）
|   |   +-- test/                   # CPU IP 独立验证环境
|   |   +-- configs/                # CPU IP 测试配置
|   |
|   +-- cache/                      # Cache IP
|   |   +-- policies/               # 可插拔策略实现
|   |   |   +-- replacement_policy.h
|   |   |   +-- lru_policy.h/cpp
|   |   |   +-- plru_policy.h/cpp
|   |   |   +-- random_policy.h/cpp
|   |   |   +-- prefetch_policy.h/cpp
|   |   +-- tlm/                    # CppTLM 通用 CacheTLM 模板（无 L1/L2 特化）
|   |   +-- rtl/                    # （Phase 5 实施）
|   |   +-- test/                   # Cache IP 独立验证环境
|   |   +-- configs/
|   |       +-- cache_minimal.json  # 最小测试拓扑
|   |       +-- cache_sweep.json    # 缓存 DSE 配置
|   |
|   +-- memory/                     # Memory IP
|   |   +-- tlm/                    # CppTLM 通用 MemoryTLM 模板
|   |   +-- rtl/                    # （Phase 5 实施）
|   |   +-- test/
|   |   +-- configs/
|   |
|   +-- interconnect/               # Interconnect IP
|   |   +-- tlm/                    # CrossbarTLM（4 端口）
|   |   +-- rtl/                    # （Phase 5 实施）
|   |   +-- test/
|   |   +-- configs/
|   |
|   +-- peripheral/                 # Peripheral IP
|       +-- tlm/                    # （Phase 3+ 实施：UART/CLINT/PLIC/VirtIO）
|       +-- rtl/                    # （Phase 5+ 实施）
|       +-- test/
|       +-- configs/
|
+-- soc/                            # * SoC 组合层（用 IP 装配产品形态）
|   +-- MemoryMap.h                 # 统一内存地图
|   +-- RiscvVirtSoC 等具体 SoC 类 （Phase 2+ 实施）
|
+-- metrics/                        # * 统计收集框架 (规划中, Phase 2 创建)
|   +-- statistics.h                # Scalar/Distribution/Vector/Formula
|   +-- stat_group.h                # 层次化统计组
|   +-- stat_manager.h              # 全局统计管理器
|   +-- stat_exporter.h             # JSON/CSV/gem5 格式导出
|
+-- verification/                   # 验证基础设施（跨 IP 共用）(规划中, Phase 3 创建)
|   +-- ScoreBoard.h/cpp            # TLM vs RTL 执行迹对比
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


# 声明式 CppTLM/CppHDL 混合电路设计架构

| 字段 | 值 |
|------|-----|
| 版本号 | 2.0.1 |
| 日期 | 2026-06-08 |
| 状态 | **Active (混合：已实现 + Phase 1 提案)** |
| 适用范围 | ChipForge 全局（任意 IP 通用） |
| 修订依据 | 架构审查（2026-06-08）的 P0–P1 修复建议 |

> ⚠️ **重要：本文档混合了"已实现框架"与"Phase 1 设计提案"**
>
> 本文档**包含两类内容**，读者必须注意区分：
>
> - **✅ 已实现部分**（约 35%）：描述 CppTLM/CppHDL 框架层**已存在**的 API 与机制。任何 C++ 代码示例均可直接编译。
> - **🚧 Phase 1 提案部分**（约 65%）：描述**尚未实现**的 Plugin / PipeNode / ImplMode / 验证策略等高级抽象。这些是 ChipForge 项目的**下一步实现目标**，代码示例为**设计草稿**，按其直接编译会失败。
>
> **请在阅读每一节时，查看该节开头的状态标记**。所有"Phase 1 提案"章节在文档目录中以 🚧 标记。
>
> **如需查阅"框架层已实现 API 与代码路径"的权威映射**，请参考 [`code-framework-mapping.md`](./code-framework-mapping.md)。

---

## 目录

- [1. 概述与设计哲学](#1-概述与设计哲学)
- [2. CppTLM 事件驱动引擎](#2-cpptlm-事件驱动引擎) ✅
- [3. CppHDL 硬件描述引擎](#3-cpphdl-硬件描述引擎) ✅
- [4. 声明式 Plugin 模型](#4-声明式-plugin-模型) 🚧
  - [4.1–4.7 全部为设计提案](#4-声明式-plugin-模型)
  - [4.8 与 ChStreamModuleBase 体系的关系](#48-与-chstreammodulebase-体系的关系v2201-新增)
- [5. Bundle 分层与协议策略](#5-bundle-分层与协议策略) ✅ (部分)
- [6. 混合仿真：模块级 TLM/RTL 细粒度配置](#6-混合仿真模块级-tlmrtl-细粒度配置) 🚧
  - [6.8 通用 TLM↔RTL 边界桥接：限制声明](#68-通用-tlmrtl-边界桥接限制声明v2201-新增)
- [7. PipeNode / PipeLink / PipeBuilder 通用骨架](#7-pipenodepipelinkpipebuilder-通用骨架) 🚧
- [8. SoC 级混合组装](#8-soc-级混合组装) ✅
- [9. 验证策略](#9-验证策略) 🚧
- [10. 完整示例：L1 Cache IP 双模式实现](#10-完整示例l1-cache-ip-双模式实现) 🚧
- [11. 附录](#11-附录)
- [12. 实现路线图（Phase 映射）](#12-实现路线图phase-映射) ✅ 新增
  - [12.0 与零债务原则的协调](#120-与-cpptlmcpphdl-零债务原则的协调v2201-新增)
  - [12.2 Phase 1 拆分方案](#122-phase-1-拆分方案v2201-重构)（1a/1b/1c）

---

## 1. 概述与设计哲学

### 1.1 项目目标

ChipForge 旨在提供一套**统一的 TLM/RTL 混合仿真与生成框架**，让任意 IP（Cache、DMA、Interconnect、Memory Controller、Peripheral 等）能够：

| 能力 | 状态 |
|------|------|
| 框架层：TLM 高速行为仿真 | ✅ 已实现（CppTLM） |
| 框架层：RTL 周期精确仿真与 Verilog 生成 | ✅ 已实现（CppHDL） |
| 框架层：Bundle 接口契约 + ch_stream 通信 | ✅ 已实现 |
| 应用层：声明式 Plugin 模型（同一份描述生成 TLM 调度表与 RTL AST） | 🚧 Phase 1 提案 |
| 应用层：模块级 ImplMode 选择（同一 SoC 中混合 TLM/RTL） | 🚧 Phase 1 提案 |
| 应用层：JSON 驱动的 IP 装配与拓扑 | ✅ 框架层已实现；⚠️ 唯一示例 `soc/riscv_virt.json` 不可运行（见 §6.2） |
| 应用层：完整的 L1 Cache IP 双模式参考实现 | 🚧 Phase 1 提案 |

### 1.2 三层框架分工

```
┌────────────────────────────────────────────────┐
│  Layer 3 — ChipForge：应用层                    │
│    IP 实例（ip/cache/、ip/cpu/ 等）             │
│    JSON 拓扑（soc/*.json）+ ModuleFactory 装配 │
│    Plugin / PipeBuilder（Phase 1 提案）         │
├────────────────────────────────────────────────┤
│  Layer 2 — CppHDL：硬件描述与生成引擎            │
│    ch_uint<N> / ch_bool / ch_reg / ch_mem       │
│    Component / describe() / VerilogCodeGen      │
│    lnode DAG → Verilog / JIT 仿真              │
├────────────────────────────────────────────────┤
│  Layer 1 — CppTLM：事件驱动仿真引擎              │
│    EventQueue / SimObject / SimModule           │
│    ch_stream / ChStreamModuleBase / Adapter     │
│    StreamAdapter 类型擦除 + 协议转换             │
└────────────────────────────────────────────────┘
```

**已落地的数据流**：

```
TLM 模式（CppTLM）：
  ch_stream<Bundle>  →  StreamAdapter  →  MasterPort / SlavePort  →  EventQueue tick()
                                                                   ↓
                                              SimObject 内部状态推进

RTL 模式（CppHDL）：
  Component::describe()  →  LogicNode DAG  →  VerilogCodeGen  →  .v 文件
                                              Simulator (JIT) → 周期精确仿真
```

> **删除说明**：原文档中描述"同一份 Plugin → build_tlm()/build_rtl()" 的图是 🚧 阶段 1 提案内容（见 [第 4 节](#4-声明式-plugin-模型)）。

### 1.3 与传统 tick-based 模型对比

| 维度 | 传统 tick-based（SystemC / Gem5） | ChipForge 现状 |
|------|------------------------------------|----------------|
| 单元抽象 | 组件（Module）持有状态 + `tick()` | ✅ 框架层：`SimObject::tick()` 由 `EventQueue` 调用；应用层 IP 仍可自定义 `tick()` |
| 调度顺序 | 用户在 `tick()` 内手写 | ✅ `EventQueue` 按时间优先级确定性调度 |
| 多组件协作 | 显式信号 + 互斥锁 / 事件 | ✅ `ch_stream<Bundle>` + `StreamAdapter` 自动桥接 |
| TLM/RTL 复用 | 需各写一份 | ✅ CppTLM 写 TLM，CppHDL 写 RTL，但**两套代码**（Plugin 模型未实现） |
| 可推理性 | 调度顺序难追溯 | ✅ 框架层调度顺序确定 |
| 流水线深度变更 | 重写 tick 调度 | 🚧 JSON 配置切换（仅对 Plugin 化的 IP，Phase 1 提案） |

---

## 2. CppTLM 事件驱动引擎 ✅

> **本章描述 CppTLM 已实现的框架层 API**，所有代码示例可直接编译。

### 2.1 类层次

```
SimObject               ← 仿真对象基类，参与 tick 调度
   ↑
SimModule               ← 模块基类，可包含子对象
   ↑
TlmModule               ← TLM 模块，具备 Port/Stats 能力
   ↑
ChStreamModuleBase      ← 声明式 ch_stream 模块基类
```

> **澄清**：本层次结构中的派生类**不会自动注入**用户的 `ch_stream<Bundle>` 端口。`ChStreamModuleBase` 的实际 API 是**接收 `StreamAdapter` 注入**（见 2.4）。这与 SpinalHDL 的"在类内声明端口"模式不同。

### 2.2 EventQueue 调度模型

`EventQueue` 是 CppTLM 的核心调度器：

- 基于优先队列的时间驱动事件调度
- `eq.run(cycles)` 推进仿真至指定周期数
- 每周期对所有已注册 `SimObject` 调用 `tick()`
- 支持任意时间偏移的事件入队（用于异步事件、延迟回调）

```cpp
#include "core/event_queue.hh"
#include "tlm/cache_tlm.hh"
#include "tlm/memory_tlm.hh"
#include "framework/chstream_adapter_factory.hh"

int main() {
    cpptlm::EventQueue eq;
    auto* cache = eq.createModule<cpptlm::CacheTLM>("cache");
    auto* dram  = eq.createModule<cpptlm::MemoryTLM>("dram");
    eq.connect("cache.mem_port", "dram.cpu_port");
    eq.run(/*cycles=*/100000);
}
```

### 2.3 ChStreamModuleBase：实际 API

`ChStreamModuleBase` 的实际 API（[`core/chstream_module.hh`](../../CppTLM/include/core/chstream_module.hh)）：

```cpp
// include/core/chstream_module.hh
class ChStreamModuleBase : public SimObject {
public:
    ChStreamModuleBase(const std::string& n, EventQueue* eq)
        : SimObject(n, eq) {}
    virtual ~ChStreamModuleBase() = default;

    // ModuleFactory 在 instantiateAll 阶段注入适配器
    virtual void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) = 0;
    virtual void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) {
        if (adapters) set_stream_adapter(adapters[0]);
    }
    virtual unsigned num_ports() const { return 1; }
    virtual tlm_stats::StatGroup* get_stats_group() { return nullptr; }
    virtual std::string get_stats_path() const { return getName(); }
};
```

**关键事实**：

- `ChStreamModuleBase` **不持有** `ch_stream<Bundle>` 成员变量
- **没有** `register_input(name, stream_)` 或 `register_output(...)` 方法
- 端口注册通过 `ChStreamAdapterFactory` + `set_stream_adapter()` 完成
- IP 派生类应**重写** `set_stream_adapter()` 以保存适配器指针，并在 `tick()` 中通过适配器收发数据

> **本节修正了原文档中虚构的 API**（原文档描述 `ch_stream<Bundle> in_req_;` 成员 + `register_input` 调用均不存在）。

### 2.4 StreamAdapter 自动桥接

模块内部使用 `ch_stream<Bundle>`（**仅在适配器内部**，非模块成员），SoC 集成者只看到 `MasterPort` / `SlavePort`。两层之间由 `StreamAdapter` 自动桥接。

```
┌─────────────────────────────────────────┐
│  模块内部（CppTLM 适配器）               │
│    StreamAdapter::process_request_input │
│    → 调用模块虚方法 process(req)        │
│    → 通过 ch_stream<MemRespBundle> 输出  │
└────────────┬────────────────────────────┘
             │ (StreamAdapter 类型擦除为 Packet)
┌────────────▼────────────────────────────┐
│  框架组装层（SoC 集成者）                │
│    MasterPort req_port                  │
│    SlavePort  resp_port                 │
│    JSON 配置通过 Port 名称完成连接       │
└─────────────────────────────────────────┘
```

`StreamAdapter` 解决三件事：

1. **类型擦除**：将 `ch_stream<T>` 转为通用 `Packet` 序列
2. **协议转换**：valid/ready/cancel 三态握手 → Port 级 send/recv 接口
3. **跨模块互通**：TLM 模块之间通过 Port 直连（无需关心对方实现）

> **澄清**：原文档声称"TLM ch_stream 与 RTL Signal<Bundle> 之间的桥梁"。**这是错误的**——StreamAdapter 仅桥接 TLM 端口，**不连接 TLM 与 RTL**。跨 TLM↔RTL 模式通信由 `HybridCacheWrapper` 等**具体桥接组件**实现（见 2.6）。

### 2.5 ModuleFactory + JSON 零代码装配（8 步流程）

> ✅ **本节内容均已实现**（路径已修正：`stats.hh` 在 `metrics/`，`port_stats.hh` 在 `core/`）。

| 步骤 | 描述 |
|------|------|
| 1 | JSON 加载 → 解析 `modules` + `connections` |
| 2 | 类型注册查找 → `ModuleFactory::create(type, params)` |
| 3 | 模块实例化 → `EventQueue` 注册 `SimObject` |
| 4 | 端口反射 → 从 `ChStreamModuleBase` 收集 input/output |
| 5 | 创建 `StreamAdapter` → 每个 ch_stream 端口包装为 Port |
| 6 | 应用 `connections` → 按 (src, dst) 名称查找 Port 并连接 |
| 7 | 应用 `latency` → 在连接上插入延迟元素 |
| 8 | Build & Reset → 触发 `build()`，进入仿真循环 |

新增 IP 只需：实现模块类 + 使用 `REGISTER_CHSTREAM` 批量注册（见 8.1）+ 加 JSON 节点。

### 2.6 统计收集体系

| 工具 | 路径 | 用途 |
|------|------|------|
| `tlm_stats`（`Stats` / `StatsManager` / `Histogram`） | `CppTLM/include/metrics/stats.hh` 等 | Scalar / Average / Distribution / Formula / StatGroup |
| `TransactionTracker` | `CppTLM/include/framework/transaction_tracker.hh` | 端到端事务跟踪、延迟分布 |
| `DebugTracker` | `CppTLM/include/framework/debug_tracker.hh` | 差异记录、状态快照 |
| `PortStats` | `CppTLM/include/core/port_stats.hh` | 端口级传输次数 / 字节数 |

> **路径修正**：原文档将 `stats.hh` 和 `port_stats.hh` 路径列为 `framework/`，实际分别在 `metrics/` 和 `core/`。

### 2.7 TLM 标准 IP 库（`tlm/*.hh`）

> ✅ **已实现**，共 12 个标准 IP 模板，ChipForge 可直接复用。

| 类 | 路径 | 用途 |
|----|------|------|
| `cpptlm::CacheTLM` | `CppTLM/include/tlm/cache_tlm.hh` | 通用缓存模板（**非 L1 特化**） |
| `cpptlm::MemoryTLM` | `CppTLM/include/tlm/memory_tlm.hh` | 内存模型（**非 DRAM 控制器**） |
| `cpptlm::CrossbarTLM` | `CppTLM/include/tlm/crossbar_tlm.hh` | 4 端口交叉开关（**非通用 AXI 互联**） |
| `cpptlm::CPUTLM` | `CppTLM/include/tlm/cpu_tlm.hh` | TLM CPU 模型（**非 RiscvIssTlm**） |
| `cpptlm::TrafficGenTLM` | `CppTLM/include/tlm/traffic_gen_tlm.hh` | 流量生成器 |
| `cpptlm::ArbiterTLM<N>` | `CppTLM/include/tlm/arbiter_tlm.hh` | N 路仲裁器 |
| `cpptlm::tlm::RouterTLM` | `CppTLM/include/tlm/router_tlm.hh` | 5 端口路由器（双向） |
| `cpptlm::tlm::NICTLM` | `CppTLM/include/tlm/nic_tlm.hh` | 网络接口（双端口非对称） |
| `cpptlm::tlm::LinkTLM` | `CppTLM/include/tlm/link_tlm.hh` | NoC 链路 |
| `cpptlm::rtl::HybridCacheWrapper` | `CppTLM/include/rtl/hybrid_cache_wrapper.hh` | **TLM↔RTL 桥接**（`CacheTLM` + `Component` 协同） |
| `noc_statistics` | `CppTLM/include/tlm/noc_statistics.hh` | NoC 统计 |
| `stress_patterns` | `CppTLM/include/tlm/stress_patterns.hh` | 压力测试模式 |

> **重要修正**：
> - 原文档中 `L1CacheTlm` / `L1CacheRtl` / `CpuTlm` / `DramTlm` / `AxiCrossbar` / `UartTlm` 等类名**均不存在**。
> - `UartTlm` 完全不存在于 CppTLM，需要在 `ip/peripheral/` 自行实现。

### 2.8 代码位置表（已修正路径）

| 功能 | 类名 | 路径 |
|------|------|------|
| 事件调度 | `EventQueue` | `CppTLM/include/core/event_queue.hh` |
| 仿真对象 | `SimObject` | `CppTLM/include/core/sim_object.hh` |
| 仿真模块 | `SimModule` / `TlmModule` | `CppTLM/include/core/sim_module.hh` / `CppTLM/include/core/tlm_module.hh` |
| ch_stream 模块 | `ChStreamModuleBase` | `CppTLM/include/core/chstream_module.hh` |
| 适配层基类 | `cpptlm::StreamAdapterBase` | `CppTLM/include/core/stream_adapter_base.hh` ⚠️ 已迁出 framework/ |
| 适配器工厂 | `ChStreamAdapterFactory` | `CppTLM/include/framework/chstream_adapter_factory.hh` |
| 模块工厂 | `ModuleFactory` | `CppTLM/include/core/module_factory.hh` |
| 统计 | `Stats` / `StatsManager` | `CppTLM/include/metrics/stats.hh` 等 |
| 事务追踪 | `TransactionTracker` | `CppTLM/include/framework/transaction_tracker.hh` |
| 调试追踪 | `DebugTracker` | `CppTLM/include/framework/debug_tracker.hh` |
| 端口统计 | `PortStats` | `CppTLM/include/core/port_stats.hh` ⚠️ 不在 framework/ |

---

## 3. CppHDL 硬件描述引擎 ✅

> **本章描述 CppHDL 已实现的框架层 API**，代码示例已用真实宏名重写。

### 3.1 类型系统

| 类型 | 路径 | 说明 |
|------|------|------|
| `ch_uint<N>` | `CppHDL/include/core/uint.h` | N 位无符号整数（综合期常量） |
| `ch_bool` | `CppHDL/include/core/bool.h` | 1 位布尔 |
| `ch_reg<T>` | `CppHDL/include/core/reg.h` | 时序寄存器（每周期更新） |
| `ch_mem<T, Depth>` | `CppHDL/include/core/mem.h` | 同步存储器（SRAM 推断） |
| `ch_logic_in<T>` / `ch_logic_out<T>` | `CppHDL/include/core/io.h` | IO 端口（旧 API） |
| `ch_in<T>` / `ch_out<T>` | `CppHDL/include/core/io.h:300-301` | IO 端口（`port<T, Dir>` 别名） |

> **删除声明**：原文档中 `ch_int<N>` **未在 CppHDL 中实现**（仅 1 个文件有零散提及，非完整类型）。如有需要需扩展类型系统。

### 3.2 Bundle 系统：`bundle_base<Self>` CRTP 模式 ✅

```cpp
#include "core/bundle/bundle_base.h"

struct MemReqBundle : public bundle_base<MemReqBundle> {
    ch_uint<64>  address;
    ch_uint<512> data;
    ch_uint<8>   burst_len;
    ch_bool      is_write;
    CH_BUNDLE_FIELDS_T(address, data, burst_len, is_write)
    //  ↑ 实际定义在 core/bundle/bundle_meta.h:23-32（含 _1 到 _5+ 重载）
};
```

`CH_BUNDLE_FIELDS_T(...)` 宏族负责：

- 自动累计字段位宽 → 决定 Bundle 总位宽
- 生成字段访问的 `ch_uint` / POD 双视图
- 注册到 BundleMapper 用于 Bundle 间转换

### 3.3 Component 派生：实际 API ✅

> **本节代码示例已用真实宏名重写**（原文档中 `__input` / `__output` / `__posedge_if` / `ch_select` 均不存在）。

```cpp
#include "component.h"
#include "core/io.h"

class L1CacheRtl : public Component {
public:
    L1CacheRtl(Component* parent, const std::string& name)
        : Component(parent, name) {
        create_ports();
    }

    // 真实端口声明（__io 宏 + __in/__out 别名，定义在 core/io.h:318,337-338）
    void create_ports() override {
        __io(
            (__in(CacheReqBundle))   cpu_req,
            (__out(CacheRespBundle)) cpu_resp,
            (__out(MemReqBundle))    mem_req,
            (__in(MemRespBundle))    mem_resp
        );
        // ↑  __io 宏定义一个 io_type struct 并生成 io() 访问器
    }

    void describe() override {
        auto& io_inst = io();    // io() 返回 io_type&（无参，定义在 core/io.h:323）
        auto  req     = io_inst.cpu_req;
        auto  idx     = req.address(11, 4);   // 实际是 (hi, lo) 形式
        auto  tag     = req.address(31, 12);
        auto  tag_match = (cache_tag_[idx] == tag) & valid_[idx];

        // 组合逻辑
        ch_uint<512> hit_data = cache_data_[idx];

        // 时序逻辑（ch_reg + 条件赋值；无 __posedge_if 宏）
        ch_reg<ch_uint<512>> r_data;
        // ... r_data <= 时序更新通过 ch_reg 的 set/enable API

        // 输出
        io_inst.cpu_resp.data = hit_data;
        io_inst.cpu_resp.resp = tag_match ? OK : MISS;   // C++ 三元运算符（无 ch_select）
    }

private:
    ch_mem<ch_uint<20>,  256> cache_tag_;
    ch_mem<ch_uint<512>, 256> cache_data_;
    ch_mem<ch_bool,      256> valid_;
};
```

**本节修正对照表**：

| 原文档（错误） | 实际 API |
|---------------|---------|
| `__input(CacheReqBundle) cpu_req;` | `__in(CacheReqBundle) cpu_req;`（在 `__io(...)` 块中） |
| `__output(CacheRespBundle) cpu_resp;` | `__out(CacheRespBundle) cpu_resp;` |
| `__posedge_if(clk_, cond) { data <= req.data; }` | 使用 `ch_reg<T>` + `set/enable` API 实现条件寄存器更新 |
| `ch_select(cond, A, B)` | C++ 内置 `cond ? A : B` 三元运算符 |
| `auto req = io(cpu_req);` | `auto& io_inst = io(); auto& req = io_inst.cpu_req;` |

### 3.4 LogicNode DAG → 后端

`describe()` 中的每条赋值被记录为 `LogicNode`（前向声明 `core/lnode.h`，模板实现位于顶层 `lnode/` 目录的 `*.tpp` 文件，构建入口 `core/node_builder.h`），构成有向无环图（DAG）。后端遍历 DAG 产生：

- **Verilog 生成**：`VerilogCodeGen` 输出可综合 `.v` 文件
- **JIT 仿真**：`Simulator` 直接对 DAG 求值，用于 RTL_ONLY / COMPARE 模式

> **路径修正**：原文档将 `LogicNode` 路径列为 `core/logic_node.h`，实际是 `core/lnode.h` + `core/lnodeimpl.h`。

### 3.5 代码位置表（已修正路径）

| 功能 | 类名 | 路径 |
|------|------|------|
| RTL 组件 | `ch::Component` | `CppHDL/include/component.h` |
| 硬件整数 | `ch_uint<N>` | `CppHDL/include/core/uint.h` |
| 时序寄存器 | `ch_reg<T>` | `CppHDL/include/core/reg.h` |
| 存储器 | `ch_mem<T,D>` | `CppHDL/include/core/mem.h` |
| Bundle 基类 | `bundle_base<Self>` | `CppHDL/include/core/bundle/bundle_base.h` |
| 端口宏 | `__io` / `__in` / `__out` | `CppHDL/include/core/io.h:318,337-338` |
| 逻辑节点 | `lnode` / `lnodeimpl` | `CppHDL/include/core/lnode.h`（前向声明） + `CppHDL/include/lnode/`（模板实现：`.tpp`/`.h`） |
| 节点构建器 | `node_builder` | `CppHDL/include/core/node_builder.h` |
| Verilog 生成 | `VerilogCodeGen` | `CppHDL/include/codegen_verilog.h` |
| 仿真器 | `Simulator` | `CppHDL/include/simulator.h` |

---

## 4. 声明式 Plugin 模型 🚧

> **本章为 Phase 1 设计提案**。所有描述的 `Plugin` / `Phase` / `at_stage` / `declare_substage` / `CtrlLink` 等 API **目前 100% 不存在于代码中**。本节是 ChipForge 项目的下一步实现目标。
>
> **唯一相关的代码**：`CppTLM/include/core/plugin_loader.hh`（运行时 `.so` 加载器，**与声明式 Plugin 模型无关**）。

### 4.1 设计动机

传统 SystemC / Gem5 模型中，每组件实现 `tick()` 自行驱动状态。Plugin 数量增加后调度顺序难以推理。声明式 Plugin 模型希望：

- 同一份 Plugin 描述既能生成 TLM 调度表，也能生成 RTL 硬件 AST
- 调度由框架确定性决定，Plugin 仅声明「在哪个阶段做什么」
- 流水线深度变更通过 JSON 配置，无需重写 Plugin

### 4.2 Plugin 基类（设计草稿）

> ⚠️ 以下代码**不可编译**。这是设计意图，不反映当前代码。

```cpp
// 设计草稿 — Phase 1 提案
enum class Phase { EARLY = 0, NORMAL = 1, LATE = 2 };

class Plugin {
public:
    explicit Plugin(const std::string& name);
    virtual ~Plugin() = default;
    virtual void setup(PipeBuilder& b) {}
    virtual void build(PipeBuilder& b) = 0;
    const std::string& name() const;
    // 注：基类没有 tick()。这是约束，不是疏忽。
};
```

### 4.3 at_stage() API（设计草稿）

> ⚠️ 设计意图：`PipeBuilder` / `at_stage` / `Phase` 枚举**均未实现**。

```cpp
// 设计草稿
void Plugin::build(PipeBuilder& b) {
    b.at_stage("lookup", Phase::NORMAL, [&b]() {
        auto* n = b.node_of_logic_stage("lookup");
        if (!n->is_firing()) return;
        // ... 业务逻辑
    });
}
```

### 4.4 declare_substage()（设计草稿）

> ⚠️ 设计意图。

```cpp
// 设计草稿
class DmaBurstPlugin : public Plugin {
    void setup(PipeBuilder& b) override {
        b.declare_substage("process", "burst_s1", 1);
        b.declare_substage("burst_s1", "burst_s2", 1);
    }
};
```

### 4.5 CtrlLink 声明式控制（设计草稿）

> ⚠️ 设计意图。

| API（设计） | 作用 |
|------------|------|
| `halt_when(cond)` | 阻塞下游 ready |
| `flush_when(cond)` | 清空寄存器 |
| `throw_when(cond)` | 注入 cancel |
| `bypass(key, src)` | 旁路转发 |

### 4.6 通用 IP Plugin 示例（设计草稿，全部不可编译）

> ⚠️ 以下示例是设计草稿，用于说明概念。**实际代码中 `PipeBuilder` / `Payload<T>` / `pl::CACHE_REQ` 等符号均不存在**。

```cpp
// 设计草稿 — Phase 1 提案
class CacheTagLookupPlugin : public Plugin {
public:
    void build(PipeBuilder& b) override {
        b.at_stage("lookup", Phase::NORMAL, [&b]() {
            auto* n = b.node_of_logic_stage("lookup");
            if (!n->is_firing()) return;
            const auto& req = (*n)(pl::CACHE_REQ);
            (*n)(pl::TAG_HIT) = tag_array_lookup(req.addr);
        });
    }
};
```

### 4.7 实现状态与路线图

详见 [第 12 节 实现路线图](#12-实现路线图phase-映射)。简述：Plugin 模型是 ChipForge Phase 1（4–8 周）的核心目标，目前仅有 `ip/cpu/docs/multi_isa_architecture.md` 的设计草案。

### 4.8 与 `ChStreamModuleBase` 体系的关系（v2.0.1 新增）

> **本节澄清 §4 提出的 Plugin 模型与 §2 描述的 `ChStreamModuleBase` 现有体系之间的关系。**

`ChStreamModuleBase`（§2.3）与 `Plugin`（§4.2）是两种**正交**的抽象层级，目前的草案关系如下：

| 维度 | `ChStreamModuleBase`（已实现） | `Plugin`（Phase 1 提案） |
|------|--------------------------------|--------------------------|
| 基类实例 | TLM 模块实例（`CacheTLM`、`MemoryTLM` 等） | 横切关注点单元（`CacheTagLookupPlugin`、`DMABurstPlugin` 等） |
| 调度单元 | `tick()` 由 `EventQueue` 调用 | **无 `tick()`**；通过 `at_stage(phase, fn)` 注册回调 |
| 通信机制 | `StreamAdapter` 注入 + `ch_stream<Bundle>` 内部传递 | `Payload<T>` 类型安全 Key + `PipeNode::operator()` 跨阶段共享 |
| 时间模型 | 离散事件 / 周期 | 逻辑阶段 + Phase 顺序（EARLY → NORMAL → LATE） |
| 组合方式 | ModuleFactory JSON 注册 + REGISTER_CHSTREAM | `PipeBuilder::register_plugin(unique_ptr<Plugin>)` |

**关系约束（草案，Phase 1a 设计阶段）**：

1. **Plugin 不替代 SimObject/TlmModule/ChStreamModuleBase**：Plugin 是**横切关注点**（cross-cutting concern），必须挂载到具体 `ChStreamModuleBase` 子类上才能生效
2. **Plugin 实例的生命周期短于模块**：单次 `build()` 内创建并固化，模块持续运行
3. **Plugin 间的通信通过 `PipeBuilder` 共享的 `PipeNode`**：不直接调用彼此方法
4. **第一阶段（Phase 1a）Plugin 仅在 TLM 模式生效**：RTL 模式继续使用现有 `Component::describe()` DAG

**与 §8.5 职责分离的关系**：§8.5 描述的是"TLM 模块开发者"与"Plugin 开发者"的分工——这暗示两者是不同的角色：
- **TLM 模块开发者**：实现 `ChStreamModuleBase` 子类（已有模式）
- **Plugin 开发者**：实现 `Plugin` 子类（在 `setup()` 中声明能力，在 `build()` 中按阶段执行）
- **SoC 集成者**：在 JSON 中配置 `modules` 数组，同时配置 `plugins` 数组（Phase 1c 引入）

> **开放问题（待 Phase 1a 设计阶段回答）**：
> 1. Plugin 如何与 `ChStreamModuleBase` 子类的 `tick()` 协作？Plugin 是在 `tick()` 之前/之后/之间触发？
> 2. 一个 `ChStreamModuleBase` 子类是否可挂载多个 Plugin？反之，一个 Plugin 是否可挂载到多个模块？
> 3. Plugin 如何访问模块私有状态——通过 `PipeBuilder` 注入访问器，还是模块主动暴露？

---

## 5. Bundle 分层与协议策略 ✅ (部分)

> **本章节大部分内容是已实现代码的描述**。仅 5.5（Mapper 模板）和 5.7（运行时 `Symbolic<T>` 抽象）为未实现。5.4 中的 `ch::ch_flow<T>` 与 `ch::ch_fragment<T>` 已实现（v2.0.1 修正）。

### 5.1 Bundle 三层架构（已实现）

```
┌─────────────────────────────────────────────────────┐
│  Layer 3: Mapper                                    │
│    BundleMapper<InternalPOD, ExternalChUint>        │
│    流水线内 Payload  ↔  IP 对外 Bundle              │
├─────────────────────────────────────────────────────┤
│  Layer 2: Protocol                                  │
│    ch_stream<T>  /  ch_logic_in<T> / ch_logic_out<T> │
├─────────────────────────────────────────────────────┤
│  Layer 1: Bundle                                    │
│    内部 POD Bundle (uint64_t/bool)  ← TLM 高速      │
│    外部 ch_uint Bundle (ch_uint<N>)  ← RTL 接口     │
└─────────────────────────────────────────────────────┘
```

### 5.2 内部 Payload Bundle（POD-style）

> ✅ 已实现：内部 Payload 可直接使用 POD 字段以获得最大性能。

```cpp
struct CacheReqPOD {
    uint64_t addr;
    uint64_t data_lo;
    uint64_t data_hi;
    uint8_t  burst_len;
    bool     is_write;
};
```

### 5.3 外部接口 Bundle（ch_uint + ch_stream）

> ✅ 已实现：Bundle 通过 `bundle_base<Self>` CRTP 派生，对外接口使用 ch_uint Bundle。

```cpp
#include "core/bundle/bundle_base.h"

struct CacheReqBundle : public bundle_base<CacheReqBundle> {
    ch_uint<64>  addr;
    ch_uint<512> data;
    ch_uint<8>   burst_len;
    ch_bool      is_write;
    CH_BUNDLE_FIELDS_T(addr, data, burst_len, is_write)
};

// 实际定义在 CppHDL/include/bundle/stream_bundle.h:19
template <typename T> struct ch_stream : public bundle_base<ch_stream<T>> { ... };
using CacheReqStream  = ch_stream<CacheReqBundle>;
using CacheRespStream = ch_stream<CacheRespBundle>;
```

### 5.4 协议类型归属

| 协议 | 来源 | 状态 | 用途 |
|------|------|------|------|
| `ch_stream<T>` | CppHDL `bundle/stream_bundle.h:19` | ✅ 已实现 | 握手数据流（valid/ready/cancel），流水线间首选 |
| `ch_logic_in<T>` / `ch_logic_out<T>` | CppHDL `core/io.h` | ✅ 已实现 | 单元级 IO 端口 |
| `ch::ch_flow<T>` | CppHDL `bundle/flow_bundle.h:21`（命名空间 `ch::`） | ✅ 已实现 | 单向流控（payload + valid，无 ready）；无反压场景 |
| `ch::ch_fragment<T>` | CppHDL `bundle/fragment.h:16`（命名空间 `ch::`） | ✅ 已实现 | 多拍突发（data_beat + last + first）；分片传输 |

> **v2.0.1 修正**：v2.0 中将 `ch_flow<T>` / `Fragment<T>` 标为"未实现"是错误的（v2.0 修订时未先验证代码）。两份头文件实际已完整实现，使用时需注意：
> - 完整路径为 `ch::ch_flow<T>` / `ch::ch_fragment<T>`（位于 `ch` 命名空间）
> - 必须包含 `bundle/flow_bundle.h` / `bundle/fragment.h` 或 umbrella `bundle.h`
> - 原文档中称"无 Fragment<T> 类型"的叙述源于类型名前缀缺失（实际为 `ch_fragment<T>`）

### 5.5 BundleMapper（设计草稿）

> ⚠️ 内部 POD ↔ 外部 ch_uint 的 Mapper 模板**目前未实现**。原文档中的代码为设计草稿。

```cpp
// 设计草稿 — Phase 1 提案
template<>
struct BundleMapper<CacheReqPOD, CacheReqBundle> {
    static CacheReqBundle to_external(const CacheReqPOD& p) { /* ... */ }
    static CacheReqPOD    to_internal(const CacheReqBundle& b) { /* ... */ }
};
```

### 5.6 编译期切换策略（已实现）

> ✅ 已实现：通过 `ch_uint<N>` 的 trait 推导，TLM 模式下字段是 POD，RTL 模式下是硬件类型。

```cpp
#ifdef RTL_MODE
  template<int N> using uint_t = ch_uint<N>;
  using bool_t = ch_bool;
#else
  template<int N> using uint_t =
    std::conditional_t<(N<=32), uint32_t, uint64_t>;
  using bool_t = bool;
#endif
```

### 5.7 运行时适配路径（设计草稿）

> 🚧 Phase 1 提案：`Symbolic<T>` 抽象与跨模块类型擦除**未实现**。当前基线是编译期切换（5.6）。

---

## 6. 混合仿真：模块级 TLM/RTL 细粒度配置 🚧

> **本章为 Phase 1 设计提案**。`ImplMode` 枚举、`impl_mode_override` JSON 字段、`CompareDriver`、SHADOW 模式等**全部未实现**。
>
> 当前仅 `cpptlm::rtl::HybridCacheWrapper`（`CppTLM/include/rtl/hybrid_cache_wrapper.hh`）支持 TLM+RTL 同实例并存的特殊场景。

### 6.1 ImplMode 四种模式（设计提案）

> ⚠️ 不可编译。设计意图。

```cpp
// 设计草稿
enum class ImplMode {
    TLM_ONLY = 0,   // 全 TLM
    RTL_ONLY = 1,   // 全 RTL
    COMPARE  = 2,   // TLM + RTL 并行对比
    SHADOW   = 3    // RTL 主路径，TLM 旁路观察
};
```

| 模式 | 使用场景 | 仿真速度（待基准测试） |
|------|---------|---------|
| TLM_ONLY | 功能验证、系统软件调试、大规模回归 | TBD（参考值：10–100 MIPS，需基准测试验证） |
| RTL_ONLY | RTL 调试、时序分析、综合前验证 | TBD（参考值：0.01–0.1 MIPS） |
| COMPARE | 协同验证、模型一致性检查 | TBD（参考值：0.005–0.05 MIPS） |
| SHADOW | RTL 调试辅助 | TBD（参考值：0.05–0.5 MIPS） |

> **删除声明**：所有"参考值"无任何基准测试支撑，应标为 TBD。

### 6.2 模块级 impl_mode_override（设计提案）

> ⚠️ **整文件不可用（v2.0.1 重大修正）**：
>
> `soc/riscv_virt.json`（仓库唯一示例 SoC 配置）的 **7 个模块类型引用 + 1 个 `impl_mode` 字段（共 8 项）都是悬挂引用**，无法运行。当前不可用的清单如下：
>
> | JSON 引用类型 | 实际状态 | 备注 |
> |--------------|---------|------|
> | `RiscvIssTlm` | ❌ 类不存在 | CppTLM 框架内无 ISS 模型 |
> | `L1CacheTlm` | ❌ 名称错误 | 应为 `CacheTLM`（通用缓存模板，非 L1 特化） |
> | `BusMatrixTlm` | ❌ 名称错误 | 应为 `CrossbarTLM`（4 端口，非通用总线矩阵） |
> | `DramTlm` | ❌ 名称错误 | 应为 `MemoryTLM`（通用内存模型，非 DRAM 控制器） |
> | `UartTlm` | ❌ 类不存在 | 需在 `ip/peripheral/` 自行实现 |
> | `ClintTlm` | ❌ 类不存在 | 需自行实现 |
> | `PlicTlm` | ❌ 类不存在 | 需自行实现 |
> | `"impl_mode": "TLM_ONLY"` | ❌ 字段未被 ModuleFactory 解析 | 整 ImplMode 机制未实现 |
>
> **修复路径**（三选一）：
> 1. **删除 `riscv_virt.json`**：避免悬挂引用误导读者
> 2. **重写 JSON**：使用 §8.2 示例中的正确类名（`CPUTLM` / `CacheTLM` / `MemoryTLM` / `CrossbarTLM`），并删除 `impl_mode` 字段
> 3. **实现缺失的 IP**：在 `ip/cpu/`、`ip/cache/`、`ip/peripheral/` 中补齐 RiscvIssTlm / UartTlm / ClintTlm / PlicTlm，并加入 `REGISTER_CHSTREAM` 注册表
>
> 当前推荐路径 (2)：保留最小可用示例，详见 §8.2。

```json
{
  "modules": [
    { "name": "cpu",   "type": "CPUTLM",    "params": {"impl_mode_override": "TLM_ONLY"} },
    { "name": "cache", "type": "CacheTLM",  "params": {"impl_mode_override": "TLM_ONLY"} }
  ]
}
```

### 6.3 TLM 模块开发范式（已实现）

> ✅ 已实现：使用 CppTLM 标准 IP 模板的代码范式。

```cpp
#include "tlm/cache_tlm.hh"

class MyCacheTlm : public cpptlm::CacheTLM {
public:
    MyCacheTlm(const std::string& n, cpptlm::EventQueue* eq,
               const nlohmann::json& params)
        : CacheTLM(n, eq) {
        // 继承自 CacheTLM 的端口自动注册
    }

    // 通过重写 set_stream_adapter 接收注入的适配器
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    // 接收请求 / 发送响应（在 tick 中由 EventQueue 调用）
    void tick() override {
        // 通过 adapter_ 收发 Packet
    }

private:
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
};
```

> **重要修正**：
> - 原文档中 `class L1CacheTlm : public ChStreamModuleBase { ch_stream<CacheReqBundle> cpu_req_; ... }` 模式**完全虚构**。
> - 实际 IP 类继承自标准模板（如 `CacheTLM`），通过 `set_stream_adapter()` 接收适配器，没有 `ch_stream<Bundle>` 成员变量。

### 6.4 RTL 模块开发范式（已实现）

> ✅ 已实现：使用 CppHDL Component + `__io` 宏的代码范式（已重写）。

```cpp
#include "component.h"
#include "core/io.h"

class L1CacheRtl : public Component {
public:
    L1CacheRtl(Component* parent, const std::string& name)
        : Component(parent, name) {
        create_ports();
    }

    void create_ports() override {
        __io(
            (__in(CacheReqBundle))   cpu_req,
            (__out(CacheRespBundle)) cpu_resp,
            (__out(MemReqBundle))    mem_req,
            (__in(MemRespBundle))    mem_resp
        );
    }

    void describe() override {
        auto& io_inst = io();
        auto  req     = io_inst.cpu_req;
        auto  idx     = req.address(11, 4);
        auto  tag     = req.address(31, 12);
        auto  hit     = (cache_tag_[idx] == tag) & valid_[idx];

        io_inst.cpu_resp.data = cache_data_[idx];
        io_inst.cpu_resp.resp = hit ? RESP_OK : RESP_MISS;

        // 时序写入：使用 ch_reg 而非 __posedge_if
        ch_reg<ch_uint<512>> r_data;
        // ... r_data <= cache_data_[idx] when hit & req.is_write
    }

private:
    ch_mem<ch_uint<20>,  256> cache_tag_;
    ch_mem<ch_uint<512>, 256> cache_data_;
    ch_mem<ch_bool,      256> valid_;
};
```

### 6.5 跨 TLM↔RTL 桥接（已实现 + 局部）

> **当前现状**：仅 `cpptlm::rtl::HybridCacheWrapper` 提供 CacheTLM + L1CacheRtl 的协同仿真（详见 `CppTLM/include/rtl/hybrid_cache_wrapper.hh`）。**通用 TLM↔RTL 边界桥接**详见 [§6.8 限制声明](#68-通用-tlmrtl-边界桥接限制声明v2201-新增)。

### 6.6 COMPARE / SHADOW 模式（设计提案）

> ⚠️ 全部为 Phase 1 提案。`CompareDriver` / `ScoreBoard` / `CycleScoreBoard` 等类**未实现**。

### 6.7 性能影响（待基准测试验证）

> 所有性能数字为设计参考值，未经任何基准测试验证。Phase 1 实施时应通过标准 benchmark（如 Dhrystone、RISC-V 指令流）实测后填入。

### 6.8 通用 TLM↔RTL 边界桥接：限制声明（v2.0.1 新增）

> **本节显式声明**：在 Phase 1 完成前，**通用 TLM↔RTL 边界桥接没有可用的通用方案**。

#### 6.8.1 现状

| 范围 | 当前能力 | 状态 |
|------|----------|------|
| 单 IP 协同（CacheTLM + L1CacheRtl） | `cpptlm::rtl::HybridCacheWrapper`（`CppTLM/include/rtl/hybrid_cache_wrapper.hh`） | ✅ 已实现，**特例** |
| 任意 TLM IP + 任意 RTL IP 跨边界 | 无通用桥接器 | ❌ 未实现 |
| 同一 IP 内同时存在 TLM 与 RTL 视图 | 仅 `HybridCacheWrapper` | ⚠️ 特例 |
| 跨 IP 边界（TLM 端 ↔ RTL 端）的通用同步协议 | 无 | ❌ 未实现 |
| 跨 TLM↔RTL 的时序对齐（如 RTL 单周期延迟 vs TLM 多事务） | 无 | ❌ 未实现 |

#### 6.8.2 为什么"通用桥接"不是 Phase 1 的可交付物

跨 TLM↔RTL 边界的**根本难点**：

1. **时序模型差异**：TLM 模式按事务（transaction）推进；RTL 模式按周期（cycle）推进。两者在边界处的转换需精确的"事务→周期"映射
2. **数据表示差异**：TLM Bundle 是 `uint64_t`/`bool` POD；RTL Bundle 是 `ch_uint<N>`/`ch_bool` 综合期类型。需要在边界执行 `BundleMapper`（§5.5，**未实现**）
3. **握手协议差异**：TLM 用 valid/ready/cancel 三态；RTL 用 valid/ready 两态 + 时序延迟
4. **取消语义差异**：TLM 支持事务级 cancel；RTL 需通过 `throwWhen()` 注入 cancel

**Phase 1a 范围限制声明**：

- ✅ Phase 1a/b：仅完成 Plugin/PipeBuilder 核心机制（与 `HybridCacheWrapper` 协同工作时可用）
- ❌ Phase 1c 之前的任意 TLM↔RTL 通用桥接：**不在范围内**
- 🚧 Phase 2：通用 `TLMRTLBridge<ReqBundle, RespBundle>` 模板提案（在 12.x 路线图中）

#### 6.8.3 用户如何在此期间处理 TLM↔RTL 协同需求

| 用户场景 | 推荐方案 |
|----------|---------|
| Cache 一致性对比（最常见） | 使用 `HybridCacheWrapper` 模式，参考 `CppTLM/include/rtl/hybrid_cache_wrapper.hh` 与 `hybrid_cache_component.hh` 复制模式 |
| 非 Cache 场景（如 DMA、Interconnect） | Phase 1 内**无法**实现，需自行编写桥接 wrapper；或将整个 IP 全部采用 TLM 或全部 RTL |
| SoC 级混合（TLM CPU + RTL 外设） | 当前无通用方案；建议：CPU 用 TLM，外设全部用 TLM（保留仿真速度优势），Phase 2 后再混合 |
| 模型一致性检查（COMPARE/SHADOW） | **不可用**（§6.6 全部未实现） |

#### 6.8.4 验证此声明

本节约束可通过以下方式验证：

- 全仓搜索 `TLMRTLBridge` / `TlmRtlBridge` / `HybridBridge`（应 0 命中，仅 `HybridCacheWrapper` 与 `hybrid_cache_component.*` 命中）
- 全仓搜索 `CompareDriver` / `ScoreBoard`（应 0 命中）

---

## 7. PipeNode / PipeLink / PipeBuilder 通用骨架 🚧

> **本章为 Phase 1 设计提案**。`PipeNode` / `PipeLink` / `PipeArbitration` / `PipeBuilder` 类**100% 不存在于代码中**。本节是设计意图。
>
> **唯一相关的设计文档**：`ip/cpu/docs/multi_isa_architecture.md`（1100+ 行设计稿）和 `docs/GLOSSARY.md` L16-18。

### 7.1 PipeNode（设计草稿）

```cpp
// 设计草稿 — Phase 1 提案
class PipeNode {
public:
    explicit PipeNode(const std::string& name);

    template<typename T> T& operator()(const Payload<T>& key);
    template<typename T> const T& operator()(const Payload<T>& key) const;

    PipeArbitration& arb();
    bool is_firing()    const;   // valid && ready && !cancel
    bool is_moving()    const;   // valid && ready
    bool is_blocked()   const;   // valid && !ready
    bool is_canceling() const;   // cancel && valid
};
```

### 7.2 PipeLink 三种类型（设计草稿）

| Link 类型 | 行为 | 典型用途 |
|-----------|------|---------|
| `StageLink` | 流水线寄存器 | 标准流水级 |
| `CtrlLink` | 寄存器 + 控制 API | 反压、刷新 |
| `DirectLink` | 组合直连 | 同拍组合扩展 |

### 7.3 PipeBuilder（设计草稿）

```cpp
// 设计草稿 — Phase 1 提案
class PipeBuilder {
public:
    explicit PipeBuilder(ImplMode mode);
    PipeNode*   create_node(const std::string& name);
    StageLink*  create_stage_link (PipeNode* up, PipeNode* down);
    CtrlLink*   create_ctrl_link  (PipeNode* up, PipeNode* down);
    DirectLink* create_direct_link(PipeNode* up, PipeNode* down);
    void register_plugin(std::unique_ptr<Plugin> p);
    void at_stage(const std::string& logic_stage, Phase phase,
                  std::function<void()> logic);
    void declare_substage(const std::string& parent,
                          const std::string& sub, int depth);
    PipeNode* node_of_logic_stage(const std::string& name);
    CtrlLink* ctrl_link_of(const std::string& up, const std::string& down);
    void build();
    void tick();
};
```

### 7.4–7.5 逻辑阶段映射 / 流水线配置（设计草稿）

> ⚠️ JSON `pipeline_stages` 字段**当前不被 CppTLM 解析**。整个流水线拓扑是设计意图。

### 7.6 实现状态

详见 [第 12 节](#12-实现路线图phase-映射)。PipeNode/PipeLink/PipeBuilder 是 Phase 1 核心目标。

---

## 8. SoC 级混合组装 ✅

> **本章已实现部分描述 SoC JSON 装配流程**。Plugin 化的 SoC 组装是 Phase 1 提案。

### 8.1 模块注册机制（已实现）

> **重要修正**：原文档描述的 `REGISTER_MODULE(L1CacheTlm)` 每类单独注册宏**不存在**。实际是 `REGISTER_CHSTREAM` 一次性批量注册宏（定义在 `CppTLM/include/chstream_register.hh:29-64`）。

```cpp
// CppTLM/include/chstream_register.hh（实际代码）
#include "chstream_register.hh"

// 一次性注册所有标准模块
REGISTER_CHSTREAM;
// ↑ 展开为：ModuleFactory::registerObject<CacheTLM>("CacheTLM"); ...
//   以及对应的 ChStreamAdapterFactory::get().registerAdapter<CacheTLM, ...>(...)
//   （完整列表见 chstream_register.hh:29-64）
```

**已注册的标准模块**：

| 类名 | 路径 | 注册名 |
|------|------|--------|
| `cpptlm::CacheTLM` | `CppTLM/include/tlm/cache_tlm.hh` | `"CacheTLM"` |
| `cpptlm::MemoryTLM` | `CppTLM/include/tlm/memory_tlm.hh` | `"MemoryTLM"` |
| `cpptlm::CrossbarTLM` | `CppTLM/include/tlm/crossbar_tlm.hh` | `"CrossbarTLM"` |
| `cpptlm::CPUTLM` | `CppTLM/include/tlm/cpu_tlm.hh` | `"CPUTLM"` |
| `cpptlm::TrafficGenTLM` | `CppTLM/include/tlm/traffic_gen_tlm.hh` | `"TrafficGenTLM"` |
| `cpptlm::ArbiterTLM<2>` / `<4>` | `CppTLM/include/tlm/arbiter_tlm.hh` | `"ArbiterTLM2"` / `"ArbiterTLM4"` |
| `cpptlm::tlm::RouterTLM` | `CppTLM/include/tlm/router_tlm.hh` | `"RouterTLM"` |
| `cpptlm::tlm::NICTLM` | `CppTLM/include/tlm/nic_tlm.hh` | `"NICTLM"` |
| `cpptlm::tlm::LinkTLM` | `CppTLM/include/tlm/link_tlm.hh` | `"LinkTLM"` |
| `cpptlm::rtl::HybridCacheWrapper` | `CppTLM/include/rtl/hybrid_cache_wrapper.hh` | `"HybridCacheWrapper"` |

> **澄清**：原文档列出的 `L1CacheTlm` / `L1CacheRtl` / `CpuTlm` / `DramTlm` / `AxiCrossbar` / `UartTlm` 等类**均不存在**于已注册表中。如需这些 IP，需在 `ip/<name>/` 实现并加入 `chstream_register.hh` 的 `REGISTER_CHSTREAM` 列表。

### 8.2 JSON 拓扑配置（已实现）

```json
{
  "name": "ExampleSoC",
  "modules": [
    { "name": "cpu",  "type": "CPUTLM",    "params": {"cores": 1} },
    { "name": "l1d",  "type": "CacheTLM",  "params": {"size": 32768} },
    { "name": "l1i",  "type": "CacheTLM",  "params": {"size": 32768} },
    { "name": "ic",   "type": "CrossbarTLM","params": {"masters": 2, "slaves": 2} },
    { "name": "dram", "type": "MemoryTLM", "params": {"size": "1G"} }
  ],
  "connections": [
    { "src": "cpu.dbus",     "dst": "l1d.cpu_port",   "latency": 0 },
    { "src": "cpu.ibus",     "dst": "l1i.cpu_port",   "latency": 0 },
    { "src": "l1d.mem_port", "dst": "ic.master[0]",   "latency": 1 },
    { "src": "l1i.mem_port", "dst": "ic.master[1]",   "latency": 1 },
    { "src": "ic.slave[0]",  "dst": "dram.port",      "latency": 4 }
  ]
}
```

> **修正要点**：
> - 实际类名是 `CPUTLM` / `CacheTLM` / `MemoryTLM` / `CrossbarTLM`，**非** `CpuTlm` / `L1CacheTlm` / `DramTlm` / `AxiCrossbar`
> - `UartTlm` 不存在（无法在 JSON 中引用）

### 8.3 自动 StreamAdapter 创建流程（已实现）

> ✅ 已实现 8 步装配流程（详见 2.5）。

### 8.4 混合仿真拓扑示例（部分实现）

> ⚠️ 严格来说，**模块级 `impl_mode` JSON 字段当前不被 CppTLM 解析**。当前仅 `HybridCacheWrapper`（`"HybridCacheWrapper"`）支持 TLM+RTL 同实例。

```json
{
  "name": "MixedSoC_DebugCache",
  "modules": [
    { "name": "cpu",  "type": "CPUTLM",            "params": {} },
    { "name": "l1",   "type": "HybridCacheWrapper","params": {} },
    { "name": "l2",   "type": "CacheTLM",          "params": {} },
    { "name": "mem",  "type": "MemoryTLM",         "params": {} }
  ],
  "connections": [
    { "src": "cpu.dbus",     "dst": "l1.cpu_port",  "latency": 0 },
    { "src": "l1.mem_port",  "dst": "l2.cpu_port",  "latency": 1 },
    { "src": "l2.mem_port",  "dst": "mem.port",     "latency": 2 }
  ]
}
```

### 8.5 职责分离 ✅

| 角色 | 关心 | 不关心 |
|------|------|--------|
| **IP 开发者** | Bundle 定义、模块类实现、单元测试 | 物理拓扑、其它 IP |
| **SoC 集成者** | JSON 拓扑、连线、IP 选择、性能权衡 | IP 内部实现 |
| **框架** | 调度、握手、桥接、统计 | 具体业务逻辑 |

---

## 9. 验证策略 🚧

> **本章为 Phase 1 设计提案**。`ScoreBoard` / `CompareDriver` / `CycleScoreBoard` / `TimingScoreBoard` / DSE 集成**全部未实现**。
>
> 当前可用的验证手段：
> - 直接调用 `ModuleFactory::create()` 与 `EventQueue::run()` 编写 C++ 测试
> - 使用 `tlm_stats` 与 `TransactionTracker` 收集指标
> - 使用 `DebugTracker` 做差异记录

### 9.1 三级测试金字塔（设计草稿）

```
        ┌─────────────────────────┐
        │  C: COMPARE 模式         │   TLM ↔ RTL 逐 Node 对比  🚧
        ├─────────────────────────┤
        │  B: 集成测试             │   完整 IP 级功能验证      ✅
        ├─────────────────────────┤
        │  A: Plugin 单元测试      │   单 Plugin + 最小流水线  🚧
        └─────────────────────────┘
```

### 9.2–9.7 全部为设计提案

> ⚠️ Section 9.2 至 9.7 描述的 Level A / Level B / Level C 测试框架、ScoreBoard、CompareDriver、DSE 集成**全部未实现**。当前 ChipForge 的 IP 验证通过直接编写 C++ 单元测试 + GoogleTest 完成（详见 `ip/*/test/` 目录骨架）。

---

## 10. 完整示例：L1 Cache IP 双模式实现 🚧

> **本章为 Phase 1 设计提案**。所有示例文件路径均**不存在**于 ChipForge 仓库：
> - `ip/cache/bundles/cache_bundles.h` ❌（`ChipForge/bundles/` 仅含 `.gitkeep`）
> - `ip/cache/tlm/l1_cache_tlm.h` ❌（`ip/cache/tlm/` 目录为空）
> - `ip/cache/rtl/l1_cache_rtl.h` ❌（`ip/cache/rtl/` 目录为空）
> - `ip/cache/plugins/*` ❌（`ip/cache/plugins/` 目录不存在）
>
> **当前可直接复用**：`cpptlm::CacheTLM`（`CppTLM/include/tlm/cache_tlm.hh`）作为 TLM 行为模型，通过 `REGISTER_CHSTREAM` 已注册；RTL Cache 可基于 `CppHDL/include/axi4/axi4_lite.h` 的参考实现或独立编写。

### 10.1 Bundle 定义（设计草稿）

```cpp
// 设计草稿 — Phase 1 提案
struct CacheReqBundle : public bundle_base<CacheReqBundle> {
    ch_uint<64>  addr;
    ch_uint<512> data;
    ch_bool      is_write;
    ch_uint<8>   id;
    CH_BUNDLE_FIELDS_T(addr, data, is_write, id)
};
```

### 10.2 TLM 实现（设计草稿）

> ⚠️ 该文件不存在。如需实现，应继承 `cpptlm::CacheTLM`（已注册的标准模板）并配置参数，而非从零编写。

### 10.3 RTL 实现（设计草稿，已修正宏名）

> ⚠️ 该文件不存在。代码示例已用真实 API 重写（`__in` / `__out` / `__io`）。

```cpp
// 设计草稿 — 实际宏名已修正
class L1CacheRtl : public Component {
public:
    L1CacheRtl(Component* parent, const std::string& name)
        : Component(parent, name) { create_ports(); }

    void create_ports() override {
        __io(
            (__in(CacheReqBundle))   cpu_req,
            (__out(CacheRespBundle)) cpu_resp
        );
    }

    void describe() override {
        auto& io_inst = io();
        auto  req     = io_inst.cpu_req;
        auto  idx     = req.address(11, 4);
        auto  tag     = req.address(31, 12);
        auto  hit     = (tags_[idx] == tag) & valid_[idx];

        io_inst.cpu_resp.data = data_[idx];
        io_inst.cpu_resp.resp = hit ? RESP_OK : RESP_MISS;
        io_inst.cpu_resp.id   = req.id;

        ch_reg<ch_uint<512>> r_data;
        // ... r_data <= data_[idx] when hit & req.is_write
    }

private:
    ch_mem<ch_uint<20>,  256> tags_;
    ch_mem<ch_bool,      256> valid_;
    ch_mem<ch_uint<512>, 256> data_;
};
```

### 10.4 Plugin 实现（设计草稿）

> ⚠️ 全部为 Phase 1 提案。Plugin / `pl::CACHE_REQ` / `pl::TAG_HIT` 等符号均未实现。

### 10.5–10.7 JSON / 测试 / COMPARE（设计草稿）

> ⚠️ 全部为 Phase 1 提案。JSON `pipeline_stages` 字段、`ScoreBoard`、`CompareDriver` 均未实现。

---

## 11. 附录

### 11.1 ADR 设计决策记录（含实际执行状态）

| ADR | 决策 | 实际执行状态 |
|-----|------|-------------|
| **ADR-1** | Plugin 声明式模型（无 tick） | 🚧 **未执行**：`Plugin` 基类不存在；`PluginLoader`（dlopen）是无关概念 |
| **ADR-2** | 逻辑阶段名标识（Plugin 与物理深度解耦） | 🚧 **未执行**：依赖 `PipeBuilder`（不存在） |
| **ADR-3** | Phase 子阶段执行顺序（EARLY/NORMAL/LATE） | 🚧 **未执行**：`Phase` 枚举不存在 |
| **ADR-4** | CtrlLink 声明式控制（halt/flush/throw/bypass） | 🚧 **未执行**：`CtrlLink` 不存在 |
| **ADR-5** | Bundle 三层分层（POD + ch_uint + Mapper） | ⚠️ **部分执行**：Bundle 系统已实现，Mapper 模板未实现 |
| **ADR-6** | 统一目录结构（取消 tlm/rtl 分离） | ❌ **未执行**：ChipForge 当前仍是 `ip/*/tlm/` 与 `ip/*/rtl/` 分离 |
| **ADR-7** | 模块级 ImplMode 选择 | 🚧 **未执行**：`ImplMode` 枚举不存在；JSON `impl_mode` 字段未被解析 |

### 11.2 CppTLM / CppHDL 完整代码映射表（已修正全部路径）

> **声明**：本表为 CppTLM/CppHDL **已实现** API 的权威映射。所有路径均通过 `ls` 验证存在。

| 维度 | 概念 / 类 | 路径 | 备注 |
|------|----------|------|------|
| TLM 调度 | `EventQueue` | `CppTLM/include/core/event_queue.hh` | ✅ |
| TLM 仿真对象 | `SimObject` | `CppTLM/include/core/sim_object.hh` | ✅ |
| TLM 模块 | `SimModule` / `TlmModule` | `CppTLM/include/core/sim_module.hh` / `CppTLM/include/core/tlm_module.hh` | ✅ |
| TLM ch_stream 模块 | `ChStreamModuleBase` | `CppTLM/include/core/chstream_module.hh` | ✅ |
| TLM 适配器基类 | `cpptlm::StreamAdapterBase` | `CppTLM/include/core/stream_adapter_base.hh` | ✅ ⚠️ 已从 framework/ 迁出 |
| TLM 适配器工厂 | `ChStreamAdapterFactory` | `CppTLM/include/framework/chstream_adapter_factory.hh` | ✅ |
| TLM 工厂 | `ModuleFactory` | `CppTLM/include/core/module_factory.hh` | ✅ |
| TLM 统计 | `Stats` / `StatsManager` / `Histogram` | `CppTLM/include/metrics/stats.hh` 等 | ✅ ⚠️ 不在 framework/ |
| TLM 事务追踪 | `TransactionTracker` | `CppTLM/include/framework/transaction_tracker.hh` | ✅ |
| TLM 调试追踪 | `DebugTracker` | `CppTLM/include/framework/debug_tracker.hh` | ✅ |
| TLM 端口统计 | `PortStats` | `CppTLM/include/core/port_stats.hh` | ✅ ⚠️ 不在 framework/ |
| TLM 缓存 IP | `cpptlm::CacheTLM` | `CppTLM/include/tlm/cache_tlm.hh` | ✅ |
| TLM 内存 IP | `cpptlm::MemoryTLM` | `CppTLM/include/tlm/memory_tlm.hh` | ✅ |
| TLM 交叉开关 | `cpptlm::CrossbarTLM` | `CppTLM/include/tlm/crossbar_tlm.hh` | ✅ |
| TLM CPU | `cpptlm::CPUTLM` | `CppTLM/include/tlm/cpu_tlm.hh` | ✅ |
| TLM 路由器 | `cpptlm::tlm::RouterTLM` | `CppTLM/include/tlm/router_tlm.hh` | ✅ |
| TLM NIC | `cpptlm::tlm::NICTLM` | `CppTLM/include/tlm/nic_tlm.hh` | ✅ |
| TLM 链路 | `cpptlm::tlm::LinkTLM` | `CppTLM/include/tlm/link_tlm.hh` | ✅ |
| TLM 仲裁器 | `cpptlm::ArbiterTLM<N>` | `CppTLM/include/tlm/arbiter_tlm.hh` | ✅ |
| TLM 流量生成 | `cpptlm::TrafficGenTLM` | `CppTLM/include/tlm/traffic_gen_tlm.hh` | ✅ |
| TLM↔RTL 桥接 | `cpptlm::rtl::HybridCacheWrapper` | `CppTLM/include/rtl/hybrid_cache_wrapper.hh` | ✅ |
| RTL 组件 | `ch::Component` | `CppHDL/include/component.h` | ✅ |
| RTL 硬件整数 | `ch_uint<N>` | `CppHDL/include/core/uint.h` | ✅ |
| RTL 布尔 | `ch_bool` | `CppHDL/include/core/bool.h` | ✅ |
| RTL 寄存器 | `ch_reg<T>` | `CppHDL/include/core/reg.h` | ✅ |
| RTL 存储器 | `ch_mem<T,D>` | `CppHDL/include/core/mem.h` | ✅ |
| RTL 端口（新） | `__io` / `__in` / `__out` | `CppHDL/include/core/io.h:318,337-338` | ✅ |
| RTL 端口（旧） | `ch_logic_in` / `ch_logic_out` | `CppHDL/include/core/io.h` | ✅ |
| RTL Bundle 基类 | `bundle_base<Self>` | `CppHDL/include/core/bundle/bundle_base.h` | ✅ |
| RTL 流 | `ch_stream<T>` | `CppHDL/include/bundle/stream_bundle.h:19` | ✅ |
| RTL 节点 | `lnode` / `lnodeimpl` | `CppHDL/include/core/lnode.h` + `core/lnodeimpl.h`（前向声明）；`CppHDL/include/lnode/*.tpp`（模板实现） | ✅ ⚠️ 非 logic_node.h |
| RTL 节点构建器 | `node_builder` | `CppHDL/include/core/node_builder.h` | ✅ |
| RTL Verilog 生成 | `VerilogCodeGen` | `CppHDL/include/codegen_verilog.h` | ✅ |
| RTL 仿真 | `Simulator` | `CppHDL/include/simulator.h` | ✅ |
| 🚧 Plugin 模型 | `Plugin` | （未实现） | 🚧 Phase 1 提案 |
| 🚧 PipeNode / PipeBuilder | `PipeNode` / `PipeBuilder` | （未实现） | 🚧 Phase 1 提案 |
| 🚧 ImplMode | `enum ImplMode` | （未实现） | 🚧 Phase 1 提案 |

### 11.3 参考文档链接

#### 项目内文档

- [docs/architecture/overview.md](./overview.md) — 顶层架构概览
- [docs/architecture/interface-design.md](./interface-design.md) — 接口设计
- [docs/architecture/tech-selection.md](./tech-selection.md) — 技术选型
- [docs/architecture/testing-and-dse.md](./testing-and-dse.md) — 验证与 DSE
- [docs/architecture/code-framework-mapping.md](./code-framework-mapping.md) — **代码与框架映射（已实现 API 权威清单）**
- [docs/architecture/error-handling.md](./error-handling.md) — 错误处理
- [docs/architecture/performance-guide.md](./performance-guide.md) — 性能指南
- [docs/architecture/background-and-goals.md](./background-and-goals.md) — 项目背景与目标
- [docs/GLOSSARY.md](../GLOSSARY.md) — 术语表
- [ip/cpu/docs/multi_isa_architecture.md](../../ip/cpu/docs/multi_isa_architecture.md) — CPU 多 ISA Plugin 设计提案（**Phase 1 草案**）

#### 外部参考

- [CppHDL GitHub](https://github.com/gtcasl/cash) — 内部 CppHDL
- [CppTLM GitHub](https://github.com/) — 内部 CppTLM
- [Verilator](https://www.veripool.org/verilator/) — RTL 协同仿真后端（CppHDL 支持）
- [SystemC TLM 2.0](https://www.accellera.org/downloads/standards/systemc) — TLM 标准参考
- [Chisel / FIRRTL](https://www.chisel-lang.org/) — 生成式硬件描述语言参考（**非 CppHDL 依赖**）

---

## 12. 实现路线图（Phase 映射）

> **本章由评审过程新增**（P3 修订要求），明确每个设计项的落地阶段与负责人。

### 12.0 与 CppTLM/CppHDL "零债务原则"的协调（v2.0.1 新增）

> **本节显式声明**：v2.0 文档中约 65% 章节标为 🚧 设计提案，**这本身是一种技术债**。本节给出与 CppTLM AGENTS.md "零债务原则"的调和策略。

#### 12.0.1 冲突识别

CppTLM AGENTS.md 明确：
> **零债务原则**：每个 Phase 完成即编译通过、测试覆盖、文档同步。**禁止遗留 TODO、跳过测试、未归档的技术债**

但 v2.0 文档中：

| 章节 | 状态 | 隐含的技术债 |
|------|------|--------------|
| §4 Plugin 模型（4.1–4.7） | 🚧 设计 | 调度算法、生命周期、与 ChStreamModuleBase 关系均未实现 |
| §5.5 BundleMapper | 🚧 设计 | POD↔ch_uint 转换模板缺失 |
| §5.7 Symbolic<T> 运行时抽象 | 🚧 设计 | 跨模块类型擦除缺失 |
| §6 混合仿真 4 种 ImplMode | 🚧 设计 | 整 ImplMode 机制缺失 |
| §7 PipeNode/PipeBuilder | 🚧 设计 | 0 行代码、0 头文件 |
| §9 验证策略（9.2–9.7） | 🚧 设计 | ScoreBoard / CompareDriver / DSE 集成缺失 |
| §10 L1 Cache 双模式实现 | 🚧 设计 | 引用文件全部不存在（`ip/cache/{bundles,tlm,rtl,plugins}/` 目录全空） |

这些章节合计约 1100 行设计草案，构成**未实现的规格说明**——本身就是技术债。

#### 12.0.2 调和策略

**短期（v2.0.x 维护期）**：
1. 任何新的 🚧 设计章节必须有显式 ADR + Owner + Phase 归属
2. 任何从仓库移除的"已实现"标签必须经过路径 + 编译验证（参见 v2.0 错误地将 `ch_flow` 标为"未实现"）
3. 每个 Phase 完成时，文档必须升级主版本号（v2.0 → v3.0）

**中期（v2.1/v2.2 演进期）**：
1. §4 Plugin 模型分阶段实施（详见 §12.2 拆分方案）
2. §10 L1 Cache 双模式作为"参考实现"分阶段落地（不阻塞主线）

**长期（v3.0 重组期）**：
1. 当 Phase 1c 完成（首个端到端 Plugin化 IP）时，§4 / §7 / §9 / §10 应转为 ✅ 状态
2. 若 Phase 1c 在 6 个月后仍未启动，应将相关章节归档至 `docs-archived/`（参见 CppTLM AGENTS.md 的归档约定）

#### 12.0.3 责任归属（明确缺失）

| 设计草案 | 当前 Owner | 状态 |
|----------|-----------|------|
| §4 Plugin 模型 | **未指定** | 阻塞 Phase 1 启动 |
| §5.5 BundleMapper | **未指定** | 可独立实施 |
| §5.7 Symbolic<T> 运行时抽象 | **未指定** | 可与 §5.5 并行 |
| §6 ImplMode | **未指定** | 阻塞 Phase 1c |
| §7 PipeNode/PipeBuilder | **未指定** | 阻塞 Phase 1 启动 |
| §9 验证策略 | **未指定** | 可与 §6 并行 |
| §10 L1 Cache 双模式 | **未指定** | Phase 1c 依赖 |

**v2.0.1 行动项**：在 Phase 1 启动前，§12.0.3 必须为每个 🚧 章节指定 Owner；否则 Phase 1 启动会议应**冻结**所有相关章节的设计。

### 12.1 已实现项 ✅（可直接使用）

| 模块 | 路径 | 状态 |
|------|------|------|
| CppTLM 仿真引擎（EventQueue / SimObject / ChStreamModuleBase） | `CppTLM/include/core/` | ✅ v2.0+ |
| CppHDL 硬件描述（Component / ch_uint / ch_reg / Bundle） | `CppHDL/include/` | ✅ v1.0+ |
| StreamAdapter 适配层（Input/Output/MultiPort/DualPort/Bidirectional） | `CppTLM/include/framework/` | ✅ v2.0+ |
| ChStreamAdapterFactory 类型注册 | `CppTLM/include/framework/chstream_adapter_factory.hh` | ✅ 2026-04-14 |
| ModuleFactory JSON 装配 | `CppTLM/include/core/module_factory.hh` | ✅ |
| TLM 标准 IP 库（Cache / Memory / Crossbar / CPU / Router / NIC / Link） | `CppTLM/include/tlm/` | ✅ |
| HybridCacheWrapper TLM↔RTL 桥接 | `CppTLM/include/rtl/hybrid_cache_wrapper.hh` | ✅ |
| VerilogCodeGen 与 Simulator | `CppHDL/include/codegen_verilog.h` / `simulator.h` | ✅ |
| lnode DAG 与 NodeBuilder | `CppHDL/include/core/lnode.h` + `core/lnodeimpl.h` + `lnode/*.tpp` + `core/node_builder.h` | ✅ |
| PluginLoader（dlopen SO 加载） | `CppTLM/include/core/plugin_loader.hh` | ✅ |

### 12.2 Phase 1 拆分方案（v2.0.1 重构）

> **v2.0.1 重大重构**：v2.0 中将所有 10 项 Phase 1 任务集中在 4–8 周窗口内，**严重低估工作量**。本节将 Phase 1 拆分为三个独立可交付的子阶段（1a/1b/1c），每个子阶段均有明确的可交付物与退出标准。

#### 12.2.1 Phase 1a — Plugin/Pipe 核心机制（4–6 周）

> **目标**：完成 Plugin/PipeBuilder 内部 API，**不**与现有 `ChStreamModuleBase` 集成。

| 任务 | 依赖 | 风险 | 退出标准 |
|------|------|------|----------|
| **实现 `Plugin` 基类**（`setup` + `build`，无 tick） | 框架层稳定 | 中：需要明确 Plugin 生命周期 | 单元测试覆盖 `Plugin` 的 setup/build 时序 |
| **实现 `Phase` 枚举** | Plugin 基础 | 低 | 编译通过 |
| **实现 `Payload<T>` 类型安全 Key** | Plugin 基础 | 低 | 编译通过 |
| **实现 `PipeBuilder` 核心 API**（`create_node` / `at_stage` / `declare_substage`） | Plugin + Phase | 中：调度算法设计 | 最小 PipeBuilder 单测 + 调度确定性测试 |
| **实现 `PipeNode` / `PipeLink` 三种类型**（`StageLink` / `CtrlLink` / `DirectLink`） | PipeBuilder | 中 | PipeNode 状态机单测 |
| **实现 `CtrlLink` 四种控制 API**（`halt_when` / `flush_when` / `throw_when` / `bypass`） | PipeNode | 中：与 `halt_when` / `throw_when` 等交互复杂 | CtrlLink 状态机 + 反压场景测试 |

**Phase 1a 退出标准**（必须全部满足才能进入 1b）：
- [ ] 所有单元测试通过（覆盖率 ≥ 80%）
- [ ] `Plugin` + `PipeBuilder` 的 API 文档（Doxygen）完整
- [ ] 调度确定性证明（在 `docs/architecture/plugin-scheduling-correctness.md` 中给出）
- [ ] 零 TODO 残留（与 CppTLM 零债务原则一致）

#### 12.2.2 Phase 1b — JSON 装配 + 验证基础设施（4–6 周）

> **目标**：扩展 ModuleFactory 接受 Plugin/Pipeline 配置；建立 COMPARE/SHADOW 验证基础设施。

| 任务 | 依赖 | 风险 | 退出标准 |
|------|------|------|----------|
| **实现 `BundleMapper` 模板** | Bundle 系统已实现 | 低 | POD↔ch_uint 双向转换单测 |
| **实现模块级 `impl_mode_override` JSON 解析** | ModuleFactory | 中：需修改拓扑解析器 | JSON 含 `impl_mode_override` 字段的解析测试 |
| **实现 `CompareDriver` 与 `ScoreBoard` 基类** | 框架层稳定 | 中 | ScoreBoard 差异检测单测 |

**Phase 1b 退出标准**：
- [ ] ModuleFactory 可解析含 `pipeline_stages` 字段的 JSON
- [ ] ScoreBoard 可对比两个 Packet 流的差异
- [ ] JSON Schema 校验脚本通过

#### 12.2.3 Phase 1c — 端到端 Plugin 化 IP（4–8 周）

> **目标**：将现有 `CacheTLM` 改造为 Plugin 化 IP，验证 1a+1b 集成效果。

| 任务 | 依赖 | 风险 | 退出标准 |
|------|------|------|----------|
| **实现第一个 Plugin 化 IP**（建议：基于现有 `CacheTLM`） | Phase 1a + 1b | 高：端到端验证 | CacheTLM 跑通 Plugin 化 SoC demo；与原 CacheTLM 行为一致 |

**Phase 1c 退出标准**：
- [ ] 端到端 SoC demo 跑通（Catch2 集成测试）
- [ ] 与 v2.0 行为对比测试通过（同一测试套件下输出相同统计）
- [ ] L1 Cache 双模式参考实现（如未在本期完成，进 Phase 2）

#### 12.2.4 Phase 1 总时间窗口

| 子阶段 | 时间窗口 | 累计 | 关键风险 |
|--------|----------|------|----------|
| Phase 1a | 4–6 周 | 4–6 周 | 调度算法设计 |
| Phase 1b | 4–6 周 | 8–12 周 | ModuleFactory 修改需向后兼容 |
| Phase 1c | 4–8 周 | 12–20 周 | 端到端集成 |
| **总计** | **12–20 周** | | **（vs v2.0 估计的 4–8 周）** |

**重要警告**：v2.0 中"Phase 1（4–8 周）"是不切实际的。Phase 1a 单独就需要 4–6 周。**应将版本目标从 ChipForge 0.2 调整为 0.2.0（1a 完成）/ 0.2.1（1c 完成）**。

### 12.3 Phase 2 目标（8–16 周，目标版本 ChipForge 0.3）

| 任务 | 依赖 |
|------|------|
| 完整 L1 Cache IP 双模式参考实现 | Phase 1c 完成 |
| `ip/cache/` 目录填充（tlm/rtl/plugins/tests/configs） | Phase 1c |
| 通用 TLM↔RTL 桥接（扩展 `HybridCacheWrapper` 模式） | Phase 1c |
| 性能基准测试套件（Dhrystone / RISC-V 指令流） | Phase 1c |
| DSE 集成（参数扫描 + Pareto 分析） | Phase 1c |
| 单元/集成/COMPARE 测试金字塔落地 | Phase 1c |
| `ch_int<N>` 类型扩展（如需要） | 用户反馈 |
| 修复文档 ADR-6 目录分离问题 | 全栈 |

### 12.4 不在当前路线图中（需用户/产品决策）

| 设计项 | 备注 |
|--------|------|
| SpinalHDL 风格 API | CppHDL 已独立设计，未承诺兼容 |
| 运行时 Symbolic<T> 抽象 | 当前编译期切换足够；运行时支持需明确驱动场景 |

> **v2.0.1 移除**：`ch_flow<T>` / `Fragment<T>`（即 `ch::ch_flow<T>` / `ch::ch_fragment<T>`）已实现并归入 5.4 节，不再列为路线图项。

### 12.5 文档维护规则

1. 每次升级 CppTLM/CppHDL 后，**必须重新核对**本文档的 11.2 路径表与所有代码示例
2. Phase 1 完成后，本文档 v2.x 需升级为 v3.0，将 ✅ 与 🚧 标记按实现状态调整
3. 文档审查 checklist 应包含「所有代码示例可编译」项
4. 评审过程产出的 P0–P3 修订建议已在 v2.0 应用，原始评审过程产物参见 Git 仓库历史与 `docs/architecture/` 目录

---

*文档结束。本版本（v2.0.1）已应用架构审查（2026-06-08）的全部 P0–P1 修复；v2.0 中的 P0–P3 修订建议保留为历史基线（v2.0 → v2.0.1 增量修改详见各章节"v2.0.1"标注）。*

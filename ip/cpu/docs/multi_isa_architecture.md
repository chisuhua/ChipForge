# 多 ISA CPU 架构设计文档

| 字段 | 值 |
|------|-----|
| 版本号 | 2.0 |
| 日期 | 2026-06-04 |
| 状态 | Active |
| 适用范围 | ChipForge IP/CPU 子系统 |

> **⚠️ Deprecated Notice**
>
> 本文档 v2.0 是对旧版（v1.x）多 ISA 架构方案的**完全重写**。
> 旧版方案中的 `ExecContext`、`Plugin::tick()`、`tlm/` 与 `rtl/` 目录分离等设计已**全部废弃**。
> 历史版本可通过 `git log -- ip/cpu/docs/multi_isa_architecture.md` 查阅。
> 项目处于纯规划阶段（无任何已发布产物），新方案不考虑兼容性。

---

## 目录

1. [概述与设计哲学](#1-概述与设计哲学)
2. [框架核心：PipeNode / PipeLink / PipeBuilder](#2-框架核心pipenode--pipelink--pipebuilder)
3. [Plugin 声明式模型](#3-plugin-声明式模型)
4. [Bundle 分层与双模式策略](#4-bundle-分层与双模式策略)
5. [多 ISA 支持](#5-多-isa-支持)
6. [可配置流水线](#6-可配置流水线)
7. [目录结构](#7-目录结构)
8. [验证策略](#8-验证策略)
9. [关键设计决策记录（ADR）](#9-关键设计决策记录adr)
10. [参考文档](#10-参考文档)

---

## 1. 概述与设计哲学

### 1.1 项目目标

ChipForge IP/CPU 子系统是一个**多 ISA、可配置、TLM/RTL 统一**的 CPU 建模框架，核心目标包括：

| 目标 | 描述 |
|------|------|
| **多 ISA 支持** | 同一套流水线骨架支持 RISC-V / ARM / 自定义 ISA，通过 Plugin 切换 |
| **DSE（设计空间探索）** | 通过 JSON 配置快速生成不同流水线深度（3/5/7/10+ 级） |
| **TLM/RTL 统一** | 同一份 Plugin 代码，TLM 模式下高速仿真，RTL 模式下生成硬件 |
| **声明式开发** | Plugin 编写 = 声明逻辑绑定到阶段，无需手写调度循环 |

### 1.2 核心设计哲学：「生成式」范式

**关键洞察**：CppTLM 和 CppHDL 都是**生成器**。

| 模式 | 输入 | 生成产物 | 运行方式 |
|------|------|----------|----------|
| TLM (`CppTLM::build()`) | Plugin 声明 | **运行时调度表**（lambda 列表） | 解释执行调度表 |
| RTL (`CppHDL::describe()`) | Plugin 声明 | **硬件 AST** | Verilator/Verilog 生成 |

**统一抽象**：Plugin 开发者只描述「在某阶段做什么」，**绝不关心**调度细节、tick 时序、双模式切换。
框架（PipeBuilder）负责将声明编译为目标产物。

```
┌──────────────────────────────────────────────────┐
│            Plugin (声明式逻辑描述)                │
│   builder->at_stage("execute", lambda)            │
└─────────────────┬────────────────────────────────┘
                  │
        ┌─────────┴────────┐
        ▼                  ▼
   PipeBuilder         PipeBuilder
   .build_tlm()        .build_rtl()
        │                  │
        ▼                  ▼
   调度表 (lambdas)    硬件 AST (ch_module)
        │                  │
        ▼                  ▼
   TLM 仿真            Verilog 生成
```

**Plugin 不写 `tick()`**——这是本架构与旧版的根本区别。

### 1.3 与 SpinalHDL Pipeline API 的关系

本框架直接借鉴 [SpinalHDL `spinal.lib.misc.pipeline`](https://spinalhdl.github.io/SpinalDoc-RTD/master/SpinalHDL/Libraries/Pipeline/index.html) 新版 API：

| SpinalHDL 概念 | ChipForge 对应 | 备注 |
|----------------|----------------|------|
| `Node` | `PipeNode` | 流水线节点，承载 Payload |
| `StageLink` | `StageLink` | 流水线寄存器（valid/ready 握手 + 数据寄存） |
| `CtrlLink` | `CtrlLink` | CPU 控制（halt/throw/flush/bypass） |
| `S2MLink` / `M2SLink` | `DirectLink` | 组合直连（无寄存） |
| `Stageable[T]` | `Payload<T>` | 类型安全 Key |
| `Plugin.during build new Area` | `Plugin::build()` | 声明逻辑 |

**关键差异**：SpinalHDL 仅生成硬件；ChipForge 同时生成 TLM 调度表与 RTL 硬件。

### 1.4 方案选型对比

我们对比了主流多 ISA CPU 框架后采用 **方案 A：共享骨架 + ISA Plugin 子目录**。

| 方案 | 代表 | 优点 | 缺点 | 是否采用 |
|------|------|------|------|----------|
| **A：共享骨架 + ISA 子目录** | SpinalHDL VexiiRiscv | 流水线复用，ISA 隔离清晰 | 需精心设计 ISA 无关 Payload | ✅ **采纳** |
| B：ISA 完全独立 | Gem5 (`arch/`) | ISA 间零耦合 | 流水线代码大量重复 | ❌ |
| C：解释器 + JIT | QEMU TCG | 跨 ISA 极快 | 无流水线建模，无法 DSE | ❌ |
| D：每 ISA 独立 Generator | Chipyard (Rocket/BOOM) | 各自最优化 | 无统一框架，难比较 | ❌ |

---

## 2. 框架核心：PipeNode / PipeLink / PipeBuilder

### 2.1 PipeNode：流水线节点

`PipeNode` 是流水线骨架的最小单位，对标 SpinalHDL `Node`。

```cpp
// core/pipe_node.h

class PipeNode {
public:
    PipeNode(const std::string& name);

    // ---------- Payload 访问（类型安全，按 Key 索引）----------
    template<typename T>
    T& operator()(const Payload<T>& key);

    template<typename T>
    const T& operator()(const Payload<T>& key) const;

    template<typename T>
    bool has(const Payload<T>& key) const;

    // ---------- 仲裁信号（PipeArbitration）----------
    PipeArbitration& arb();

    // 派生状态（只读）
    bool is_firing() const;   // valid && ready && !cancel
    bool is_moving() const;   // valid && ready
    bool is_blocked() const;  // valid && !ready
    bool is_canceling() const;// cancel && valid

    // ---------- 元信息 ----------
    const std::string& name() const;

private:
    std::string name_;
    PayloadMap payloads_;     // Key -> 类型擦除存储
    PipeArbitration arb_;
};
```

**PipeArbitration** 持有三态握手信号：

```cpp
// core/pipe_arbitration.h

struct PipeArbitration {
    Signal<bool> valid;   // 上游产生有效数据
    Signal<bool> ready;   // 下游可接受
    Signal<bool> cancel;  // 取消（异常/分支预测错误）
};
```

派生状态语义（与 SpinalHDL 一致）：

| 状态 | 表达式 | 含义 |
|------|--------|------|
| `is_firing` | `valid && ready && !cancel` | 数据本拍真正流出 |
| `is_moving` | `valid && ready` | 数据本拍前移（无论是否被 cancel） |
| `is_blocked` | `valid && !ready` | 阻塞中 |
| `is_canceling` | `valid && cancel` | 数据将被丢弃 |

### 2.2 PipeLink：节点之间的连接

```cpp
// core/pipe_link.h

class PipeLink {
public:
    virtual ~PipeLink() = default;
    virtual void elaborate(PipeBuilder& b) = 0;  // build 阶段调用

    PipeNode* upstream() const;
    PipeNode* downstream() const;
};
```

三种 Link 类型：

#### 2.2.1 StageLink — 流水线寄存器

```cpp
class StageLink : public PipeLink {
public:
    StageLink(PipeNode* up, PipeNode* down);
    // valid/ready 握手 + 全部 Payload 注册并打一拍
};
```

#### 2.2.2 CtrlLink — CPU 控制（声明式）

```cpp
class CtrlLink : public PipeLink {
public:
    CtrlLink(PipeNode* up, PipeNode* down);

    // 声明式控制 API（在 Plugin::build() 中调用）
    CtrlLink& halt_when(std::function<bool()> cond);    // 阻塞下游 ready
    CtrlLink& throw_when(std::function<bool()> cond);   // 注入 cancel
    CtrlLink& flush_when(std::function<bool()> cond);   // 清空寄存器
    CtrlLink& bypass(const PayloadKeyBase& key,
                     std::function<std::any()> src);    // 旁路覆盖某 Payload
};
```

#### 2.2.3 DirectLink — 组合直连

```cpp
class DirectLink : public PipeLink {
public:
    DirectLink(PipeNode* up, PipeNode* down);
    // 无寄存，valid/ready 直通，Payload 引用透传
};
```

### 2.3 Payload\<T\>：类型安全 Key

```cpp
// core/payload.h

template<typename T>
class Payload {
public:
    explicit Payload(const std::string& name);
    const std::string& name() const;
    using value_type = T;
    // 不可拷贝、不可赋值；仅作 Key
};

// 用法（全局单例 Key）
namespace pl {
    inline const Payload<uint32_t>  PC{"PC"};
    inline const Payload<uint32_t>  INSTRUCTION{"INSTRUCTION"};
    inline const Payload<DecodePayload>  DECODE{"DECODE"};
}
```

**Key 与 Value 完全分离**：Key 是全局静态对象（描述符），Value 存储于 `PipeNode::payloads_`。

### 2.4 PipeBuilder：统一管理者

```cpp
// core/pipe_builder.h

enum class ImplMode { TLM, RTL, COMPARE };

class PipeBuilder {
public:
    explicit PipeBuilder(ImplMode mode);

    // ---------- 拓扑构建（由 JSON 加载器 / Plugin 调用）----------
    PipeNode* create_node(const std::string& name);
    StageLink* create_stage_link(PipeNode* up, PipeNode* down);
    CtrlLink*  create_ctrl_link (PipeNode* up, PipeNode* down);
    DirectLink* create_direct_link(PipeNode* up, PipeNode* down);

    // ---------- Plugin 注册（顺序即优先级）----------
    void register_plugin(std::unique_ptr<Plugin> p);

    // ---------- Plugin::build() 中调用的声明 API ----------
    void at_stage(const std::string& logic_stage,
                  Phase phase,
                  std::function<void()> logic);

    void declare_substage(const std::string& parent_logic_stage,
                          const std::string& sub_name,
                          int depth);

    PipeNode* node_of_logic_stage(const std::string& logic_stage);

    // ---------- 编译 ----------
    void build();           // setup + build 全部 Plugin，根据 mode 生成产物
    void tick();            // TLM 模式：执行调度表（一拍）；RTL 模式：no-op
    ChModule  emit_rtl();   // RTL 模式：返回硬件模块

private:
    ImplMode mode_;
    std::vector<std::unique_ptr<PipeNode>> nodes_;
    std::vector<std::unique_ptr<PipeLink>> links_;
    std::vector<std::unique_ptr<Plugin>>   plugins_;
    std::map<std::string, PipeNode*>       logic_stage_map_;
    std::vector<ScheduledLogic>            schedule_;  // TLM 调度表
};
```

**逻辑阶段名 → 物理 PipeNode 映射**：

| 逻辑阶段名 | 物理 Node（5 级） | 物理 Node（3 级） | 物理 Node（7 级超标量） | 说明 |
|------------|-------------------|-------------------|--------------------------|------|
| `fetch`    | `IF`              | `IF`              | `IF1` → `IF2`            | 取指 |
| `decode`   | `ID`              | `IF`              | `ID` → `RENAME`          | 译码 |
| `execute`  | `EX`              | `EXMEM`           | `ISSUE` → `EX1` → `EX2`  | 执行 |
| `memory`   | `MEM`             | `EXMEM`           | `MEM`                    | 访存 |
| `writeback`| `WB`              | `WB`              | `RETIRE`                 | 写回 |
| `commit`   | `WB`（隐含）       | `WB`（隐含）       | `RETIRE`                | (无) — M4G-extend 命名约定: in-order 隐含; OoO 显式阶段, 用 `commit_hook` 原语 (见 `include/cf/plugin/pipe_builder.h:138-143`) |

> **M4G-extend G.X Gap C (M4G-extend-tid-and-hooks, 2026-06-21)**: 新增第 6 行 `commit` 阶段, 命名锁定以避免 Phase 5+ 在 `at_stage("retire")` vs `at_stage("commit")` 之间碎片化. 当前 in-order 代码不应注册 `at_stage("commit", ...)` 回调 — 仅 OoO (Phase 5+) 显式使用. 推迟的 7 个 OoO 缺口 (ROB / IQ / PRF / LSQ / Rename / MUL-latency / Cache-latency) 留给 Phase 5+.

映射由 JSON 配置 `pipeline_stages` 字段决定（见 §6）。

---

## 3. Plugin 声明式模型

### 3.1 Plugin 基类

```cpp
// core/plugin.h

enum class Phase {
    EARLY  = 0,   // 冒险检测、前置依赖、重定向请求
    NORMAL = 1,   // 算术运算、主体逻辑
    LATE   = 2    // 写回准备、提交
};

class Plugin {
public:
    explicit Plugin(const std::string& name);
    virtual ~Plugin() = default;

    // 阶段一：声明依赖（其他 Plugin / Payload / Link）
    virtual void setup(PipeBuilder& b) {}

    // 阶段二：声明逻辑（at_stage / declare_substage / CtrlLink 配置）
    virtual void build(PipeBuilder& b) = 0;

    const std::string& name() const;
    // 注意：基类没有 tick() 方法。这是约束，不是疏忽。
};
```

### 3.2 `at_stage()` API

```cpp
// 在 Plugin::build() 中：
b.at_stage("execute", Phase::NORMAL, [this, &b]() {
    PipeNode* n = b.node_of_logic_stage("execute");
    if (!n->is_firing()) return;
    auto& d = (*n)(pl::DECODE);
    if (d.op == OpCode::ADD) {
        (*n)(pl::RESULT) = (*n)(pl::RS1) + (*n)(pl::RS2);
    }
});
```

**执行顺序规则**（同一 logic_stage 内）：

```
Phase::EARLY  (Plugin 注册顺序)
   → Phase::NORMAL (Plugin 注册顺序)
     → Phase::LATE   (Plugin 注册顺序)
```

跨 logic_stage 之间：按物理 Node 拓扑顺序（PipeBuilder 拓扑排序后调度）。

### 3.3 子阶段声明

Plugin 可在 `setup()` 中向流水线**追加**物理 Node：

```cpp
class FpuPlugin : public Plugin {
    void setup(PipeBuilder& b) override {
        // 在 "execute" 逻辑阶段下声明一个 3 级深的子流水
        b.declare_substage("execute", "fpu_pipe", /*depth=*/3);
    }
    void build(PipeBuilder& b) override {
        b.at_stage("fpu_pipe", Phase::NORMAL, /* fpu lambda */);
    }
};
```

PipeBuilder 在拓扑构建末尾应用所有 `declare_substage`，再做映射校验。

### 3.4 CtrlLink 声明式控制

```cpp
class HazardPlugin : public Plugin {
    void build(PipeBuilder& b) override {
        auto* ctrl_id_ex = /* CtrlLink between ID and EX */;
        ctrl_id_ex->halt_when([&b]() {
            auto* id = b.node_of_logic_stage("decode");
            auto* ex = b.node_of_logic_stage("execute");
            return raw_hazard(id, ex);
        });
    }
};
```

```cpp
class BranchPlugin : public Plugin {
    void build(PipeBuilder& b) override {
        auto* ctrl_if = /* CtrlLink before IF */;
        ctrl_if->flush_when([&b]() {
            return mispredicted(b.node_of_logic_stage("execute"));
        });
    }
};
```

### 3.5 调度由 PipeBuilder 包办

```cpp
PipeBuilder pb(ImplMode::TLM);
pb.register_plugin(std::make_unique<RiscvDecodePlugin>());
pb.register_plugin(std::make_unique<HazardPlugin>());
pb.register_plugin(std::make_unique<IntAluPlugin>());
pb.register_plugin(std::make_unique<MulPlugin>());
pb.build();

while (!done) pb.tick();   // 用户唯一关心的循环
```

**用户不可重写 `PipeBuilder::tick()`**。所有逻辑均通过 `at_stage()` 注册。

### 3.6 完整示例：MulPlugin（声明式，无 tick）

```cpp
// arch/riscv/plugins/mul_plugin.h

#include "core/plugin.h"
#include "arch/riscv/payload_riscv.h"

class MulPlugin : public Plugin {
public:
    MulPlugin() : Plugin("MulPlugin") {}

    // setup: 声明在 "execute" 下追加 2 级子流水
    void setup(PipeBuilder& b) override {
        b.declare_substage("execute", "mul_s1", 1);
        b.declare_substage("execute", "mul_s2", 1);
    }

    void build(PipeBuilder& b) override {
        // 第 1 拍：Booth 编码
        b.at_stage("mul_s1", Phase::NORMAL, [&b]() {
            auto* n = b.node_of_logic_stage("mul_s1");
            if (!n->is_firing()) return;
            const auto& dec = (*n)(pl::DECODE);
            if (dec.op != OpCode::MUL) return;

            (*n)(pl::MUL_BOOTH) = booth_encode((*n)(pl::RS2));
            (*n)(pl::MUL_A)     = (*n)(pl::RS1);
        });

        // 第 2 拍：累加 + 写回 RESULT
        b.at_stage("mul_s2", Phase::NORMAL, [&b]() {
            auto* n = b.node_of_logic_stage("mul_s2");
            if (!n->is_firing()) return;
            const auto& dec = (*n)(pl::DECODE);
            if (dec.op != OpCode::MUL) return;

            uint64_t prod = booth_accumulate((*n)(pl::MUL_A),
                                             (*n)(pl::MUL_BOOTH));
            (*n)(pl::RESULT) = static_cast<uint32_t>(prod);
        });
        // 整段代码无 tick()，无调度，无时序细节
    }
};
```

---

## 4. Bundle 分层与双模式策略

### 4.1 三层架构

```
┌─────────────────────────────────────────────────────┐
│  Layer 3: Mapper                                    │
│    BundleMapper<InternalPOD, ExternalChUint>        │
│    流水线内 Payload  ↔  CPU 对外 Bundle             │
├─────────────────────────────────────────────────────┤
│  Layer 2: Protocol                                  │
│    ch_stream<T>  /  ch_flow<T>  /  Fragment<T>      │
├─────────────────────────────────────────────────────┤
│  Layer 1: Bundle                                    │
│    内部 POD Bundle (uint32_t/bool)  ← TLM 高速      │
│    外部 ch_uint Bundle (ch_uint<N>)  ← RTL 接口     │
└─────────────────────────────────────────────────────┘
```

### 4.2 流水线内部 Payload Bundle（POD-style）

为 TLM 模式获得最大性能，**流水线寄存器内的 Payload 一律使用 POD 字段**。

```cpp
// arch/riscv/payload_riscv.h（ISA 无关部分实际位于 core/payload_common.h）

// ISA 无关 —— 通用 Plugin 可读
struct DecodePayload {
    uint8_t   rs1_idx;    // 0..31
    uint8_t   rs2_idx;
    uint8_t   rd_idx;
    bool      uses_rs1;
    bool      uses_rs2;
    bool      writes_rd;
    bool      is_branch;
    bool      is_load;
    bool      is_store;
    OpClass   op_class;   // ALU / MUL / MEM / BRANCH / SYS
};

// ISA 特有 —— 仅 RISC-V Plugin 内部使用
struct RiscvDecodeDetail {
    uint32_t  funct3;
    uint32_t  funct7;
    int32_t   imm;
    uint8_t   csr_idx;
    bool      is_compressed;
};
```

### 4.3 CPU 对外接口 Bundle（ch_uint + ch_stream）

```cpp
// core/bundles/mem_bundle.h

#include <cash/cash.h>
using namespace ch::core;

struct MemReqBundle {
    ch_uint<32> addr;
    ch_uint<32> wdata;
    ch_uint<4>  wstrb;
    ch_bool     write;
    ch_uint<3>  size;
};

struct MemRespBundle {
    ch_uint<32> rdata;
    ch_uint<2>  resp;   // OKAY / SLVERR / DECERR
};

// 协议层
using MemReqStream  = ch_stream<MemReqBundle>;
using MemRespStream = ch_stream<MemRespBundle>;
```

**对比**：

| 维度 | 内部 Payload Bundle | 外部接口 Bundle |
|------|---------------------|------------------|
| 字段类型 | `uint32_t`, `bool`, `enum` | `ch_uint<N>`, `ch_bool` |
| 协议 | 无（裸 struct） | `ch_stream<T>` / `ch_flow<T>` / `Fragment<T>` |
| TLM 性能 | ⭐⭐⭐⭐⭐ 直接读写 | ⭐⭐⭐ 需 Mapper |
| RTL 综合 | 编译期切换 ch_uint | 直接综合 |
| 用途 | Plugin 间数据传递 | CPU ↔ 外设、CPU ↔ 总线 |

### 4.4 BundleMapper

```cpp
// core/bundle_mapper.h

template<typename InternalPOD, typename ExternalChUint>
struct BundleMapper {
    static ExternalChUint to_external(const InternalPOD& in);
    static InternalPOD   to_internal(const ExternalChUint& ex);
};

// 特化示例
template<>
struct BundleMapper<MemReqPOD, MemReqBundle> {
    static MemReqBundle to_external(const MemReqPOD& p) {
        MemReqBundle b;
        b.addr  = p.addr;
        b.wdata = p.wdata;
        b.wstrb = p.wstrb;
        b.write = p.write;
        b.size  = p.size;
        return b;
    }
    static MemReqPOD to_internal(const MemReqBundle& b) {
        return { b.addr.as_uint(), b.wdata.as_uint(),
                 (uint8_t)b.wstrb.as_uint(), b.write.as_bool(),
                 (uint8_t)b.size.as_uint() };
    }
};
```

### 4.5 RTL 模式策略：编译期切换

```cpp
// core/payload_common.h

#ifdef RTL_MODE
  template<int N> using uint_t = ch_uint<N>;
  using bool_t  = ch_bool;
#else
  template<int N> using uint_t = std::conditional_t<(N<=32), uint32_t, uint64_t>;
  using bool_t  = bool;
#endif

struct DecodePayload {
    uint_t<5>  rs1_idx;
    uint_t<5>  rs2_idx;
    uint_t<5>  rd_idx;
    bool_t     uses_rs1;
    bool_t     uses_rs2;
    bool_t     writes_rd;
    bool_t     is_branch;
    bool_t     is_load;
    bool_t     is_store;
    uint_t<4>  op_class;
};
```

**Plugin 代码完全不感知模式差异**：`(*n)(pl::DECODE).rs1_idx` 在 TLM 是 `uint32_t`，在 RTL 是 `ch_uint<5>`，操作语法一致。

### 4.6 与 SpinalHDL Fragment 的对比

| 概念 | SpinalHDL | ChipForge |
|------|-----------|-----------|
| 单拍数据 | `Flow[T]` | `ch_flow<T>` |
| 握手数据 | `Stream[T]` | `ch_stream<T>` |
| 多拍突发 | `Fragment[T]` (`last` 标志) | `Fragment<T>`（同义） |
| Bundle 字段类型 | `Bits` / `UInt` | `ch_uint<N>` / POD |

---

## 5. 多 ISA 支持

### 5.1 无 ExecContext

旧版方案中的 `ExecContext` 抽象（封装 ISA 译码与执行的运行时上下文）**已废弃**。
原因：`ExecContext` 与 Plugin 职责重叠，引入额外间接层；新方案中 ISA 逻辑直接作为 Plugin 的 `at_stage()` 声明绑定到 Node。

### 5.2 双 Payload 共存

**ISA 无关 Payload**（`DecodePayload`）

- 由 ISA Decode Plugin 填充
- 通用 Plugin（HazardPlugin / BranchPredictor / RegFile / IBus / DBus）只读取这一份
- 字段：寄存器号、读写标志、指令类别

**ISA 特有 Payload**（`RiscvDecodeDetail` / `ArmDecodeDetail`）

- 仅同 ISA 内的 Plugin（IntAlu / Mul / Branch / Csr / Lsu）使用
- 字段：funct3/funct7/imm/csr_idx 等

```cpp
class RiscvDecodePlugin : public Plugin {
    void build(PipeBuilder& b) override {
        b.at_stage("decode", Phase::NORMAL, [&b]() {
            auto* n = b.node_of_logic_stage("decode");
            if (!n->is_firing()) return;
            uint32_t inst = (*n)(pl::INSTRUCTION);

            // 同时填充两份 Payload
            (*n)(pl::DECODE)        = riscv_decode_common(inst);
            (*n)(pl::RISCV_DETAIL)  = riscv_decode_detail(inst);
        });
    }
};
```

### 5.3 ISA 切换 = 更换 Plugin Registry 工厂函数

```cpp
// cpu_factory.h

std::unique_ptr<PipeBuilder>
build_cpu(const json& config, ImplMode mode) {
    auto pb = std::make_unique<PipeBuilder>(mode);

    // 公共 Plugin
    pb->register_plugin(std::make_unique<HazardPlugin>());
    pb->register_plugin(std::make_unique<RegFilePlugin>());
    pb->register_plugin(std::make_unique<IBusPlugin>());
    pb->register_plugin(std::make_unique<DBusPlugin>());

    // ISA 特有 Plugin（由配置决定）
    const std::string& isa = config["isa"];
    if (isa == "riscv") {
        pb->register_plugin(std::make_unique<RiscvDecodePlugin>());
        pb->register_plugin(std::make_unique<RiscvIntAluPlugin>());
        pb->register_plugin(std::make_unique<RiscvBranchPlugin>());
        if (config.value("ext_m", false))
            pb->register_plugin(std::make_unique<RiscvMulPlugin>());
        if (config.value("ext_zicsr", false))
            pb->register_plugin(std::make_unique<RiscvCsrPlugin>());
    } else if (isa == "arm") {
        pb->register_plugin(std::make_unique<ArmDecodePlugin>());
        // ...
    }
    pb->build();
    return pb;
}
```

### 5.4 新增 ISA Checklist

新增一个 ISA（例如 OpenRISC）需要完成以下清单：

- [ ] 在 `arch/openrisc/` 创建子目录
- [ ] 定义 `OpenRiscDecodeDetail`（Bundle）
- [ ] 实现 `OpenRiscDecodePlugin`（同时填充通用 `DecodePayload`）
- [ ] 实现核心执行 Plugin：`OpenRiscIntAluPlugin` / `OpenRiscBranchPlugin` / `OpenRiscLsuPlugin`
- [ ] 可选扩展 Plugin：MUL / FPU / CSR
- [ ] 在 `cpu_factory.h` 添加 `isa == "openrisc"` 分支
- [ ] 编写 `arch/openrisc/tests/` 单元测试
- [ ] 添加 `configs/openrisc_default.json`

### 5.5 Plugin 分类表

| 类别 | Plugin | 路径 | 说明 |
|------|--------|------|------|
| **ISA 无关** | `HazardPlugin` | `plugins/hazard.h` | 数据冒险检测，仅读 `DecodePayload` |
| ISA 无关 | `BranchPredictorPlugin` | `plugins/branch_predictor.h` | BTB / Bimodal / GShare |
| ISA 无关 | `RegFilePlugin` | `plugins/reg_file.h` | 通用寄存器堆（参数化宽度） |
| ISA 无关 | `IBusPlugin` | `plugins/ibus.h` | 取指总线（CPU 对外） |
| ISA 无关 | `DBusPlugin` | `plugins/dbus.h` | 数据总线（CPU 对外） |
| **ISA 特有** | `RiscvDecodePlugin` | `arch/riscv/decode.h` | 译码并填充两份 Payload |
| ISA 特有 | `RiscvIntAluPlugin` | `arch/riscv/int_alu.h` | RV32I 整数运算 |
| ISA 特有 | `RiscvMulPlugin` | `arch/riscv/mul.h` | RV32M（可声明子流水） |
| ISA 特有 | `RiscvBranchPlugin` | `arch/riscv/branch.h` | RV32I 分支 |
| ISA 特有 | `RiscvCsrPlugin` | `arch/riscv/csr.h` | Zicsr |
| ISA 特有 | `RiscvLsuPlugin` | `arch/riscv/lsu.h` | RV32I 载入存储 |

---


## 6. 可配置流水线

### 6.1 JSON 配置 Schema

> **三种 ISA 表示法的关系 (2026-06-17 校核)**:
> - **`isa` 字符串前缀** (`rv32i`/`rv32im`/`rv64gc`) 是 [`cpu_params_schema.json`](../../configs/cpu_params_schema.json) 的唯一权威字段,也是所有 `configs/cpu_*.json` 实例文件实际使用的形式
> - **`isa_extensions` 数组** (本节描述) 是设计意图视图,与字符串前缀语义等价 (运行时归并)
> - **`ext_*` boolean 字段** 是 DSE 工具 ([`../dse_architecture.md` §4.2](../dse_architecture.md)) 为笛卡尔积扫描引入的 flat 形式,运行时与字符串前缀互验
> 三者冗余但等价,所有变更应同时反映到 `cpu_params_schema.json`

```json
{
  "$schema": "./cpu_params_schema.json",
  "arch": "scalar_inorder",
  "isa": "riscv",
  "isa_extensions": ["i", "m", "zicsr"],
  "xlen": 32,

  "pipeline_stages": [
    { "name": "IF",    "logic_stages": ["fetch"] },
    { "name": "ID",    "logic_stages": ["decode"] },
    { "name": "EX",    "logic_stages": ["execute"] },
    { "name": "MEM",   "logic_stages": ["memory"] },
    { "name": "WB",    "logic_stages": ["writeback"] }
  ],

  "branch_predictor": {
    "type": "bimodal",
    "btb_entries": 64,
    "bht_entries": 256
  },

  "plugins": [
    "HazardPlugin",
    "BranchPredictorPlugin",
    "RegFilePlugin",
    "IBusPlugin",
    "DBusPlugin",
    "RiscvDecodePlugin",
    "RiscvIntAluPlugin",
    "RiscvMulPlugin",
    "RiscvBranchPlugin",
    "RiscvCsrPlugin",
    "RiscvLsuPlugin"
  ]
}
```

### 6.2 逻辑阶段 → 物理 Node 映射规则表

| 配置 | 物理 Node 数 | 映射 |
|------|--------------|------|
| **3 级（嵌入式）** | 3 | `IF`={fetch,decode}, `EXMEM`={execute,memory}, `WB`={writeback} |
| **5 级（默认）** | 5 | `IF`={fetch}, `ID`={decode}, `EX`={execute}, `MEM`={memory}, `WB`={writeback} |
| **7 级（超标量）** | 7 | `IF1`,`IF2`,`ID`,`RENAME`,`ISSUE`,`EX`,`MEM`,`RETIRE` |
| **10+ 级（深流水）** | ≥10 | 自由声明，Plugin 通过 `declare_substage()` 进一步分裂 EX |

**校验规则**（PipeBuilder::build() 中执行）：

1. 每个 `logic_stage` 必须出现在恰好一个物理 Node 的 `logic_stages` 列表
2. Plugin 的 `at_stage("X", ...)` 中 `X` 必须是已声明的 logic_stage（或 declare_substage 生成的）
3. `declare_substage(parent, sub, depth)` 中 `parent` 必须是已存在的 logic_stage
4. 子流水插入位置：在 parent 物理 Node 之后追加 `depth` 个 Node，`sub` 名称指向最后一个 Node

### 6.3 Plugin declare_substage 示例

```cpp
class FpuPlugin : public Plugin {
    void setup(PipeBuilder& b) override {
        // 在 "execute" 后追加 3 级 FPU 子流水
        b.declare_substage("execute", "fpu_s1", 1);
        b.declare_substage("fpu_s1",   "fpu_s2", 1);
        b.declare_substage("fpu_s2",   "fpu_s3", 1);
    }
    void build(PipeBuilder& b) override {
        b.at_stage("fpu_s1", Phase::NORMAL, [&b](){ /* 解码操作数 */ });
        b.at_stage("fpu_s2", Phase::NORMAL, [&b](){ /* 尾数运算 */ });
        b.at_stage("fpu_s3", Phase::NORMAL, [&b](){ /* 规格化、舍入 */ });
    }
};
```

### 6.4 配置校验

```cpp
// 简化伪码
void PipeBuilder::build() {
    // 1. 应用 JSON 拓扑
    apply_pipeline_stages_from_json();

    // 2. setup 全部 Plugin（处理 declare_substage）
    for (auto& p : plugins_) p->setup(*this);
    finalize_substages();

    // 3. build 全部 Plugin（处理 at_stage / CtrlLink）
    for (auto& p : plugins_) p->build(*this);

    // 4. 校验：每个 at_stage 的 logic_stage 必须可解析
    validate_logic_stage_mapping();

    // 5. 拓扑排序 + 生成调度表 / 硬件 AST
    if (mode_ == ImplMode::TLM)  generate_schedule();
    if (mode_ == ImplMode::RTL)  generate_hardware();
}
```

### 6.5 配置实例

#### 默认 5 级（`configs/cpu_default.json`）

```json
{
  "arch": "scalar_inorder", "isa": "riscv", "xlen": 32,
  "isa_extensions": ["i", "m", "zicsr"],
  "pipeline_stages": [
    { "name":"IF","logic_stages":["fetch"] },
    { "name":"ID","logic_stages":["decode"] },
    { "name":"EX","logic_stages":["execute"] },
    { "name":"MEM","logic_stages":["memory"] },
    { "name":"WB","logic_stages":["writeback"] }
  ]
}
```

#### 嵌入式 3 级（`configs/cpu_embedded.json`）

```json
{
  "arch": "scalar_inorder", "isa": "riscv", "xlen": 32,
  "isa_extensions": ["i"],
  "pipeline_stages": [
    { "name":"IF",    "logic_stages":["fetch","decode"] },
    { "name":"EXMEM", "logic_stages":["execute","memory"] },
    { "name":"WB",    "logic_stages":["writeback"] }
  ]
}
```

#### 超标量 7 级（`configs/cpu_superscalar.json`）

```json
{
  "arch": "scalar_ooo", "isa": "riscv", "xlen": 64,
  "isa_extensions": ["i","m","a","f","d","zicsr"],
  "pipeline_stages": [
    { "name":"IF1",    "logic_stages":["fetch_p1"] },
    { "name":"IF2",    "logic_stages":["fetch_p2"] },
    { "name":"ID",     "logic_stages":["decode"] },
    { "name":"RENAME", "logic_stages":["rename"] },
    { "name":"ISSUE",  "logic_stages":["issue"] },
    { "name":"EX",     "logic_stages":["execute"] },
    { "name":"RETIRE", "logic_stages":["memory","writeback"] }
  ]
}
```

---

## 7. 目录结构

```
ip/cpu/
├── core/                          # 框架核心（ISA 无关、Plugin 无关）
│   ├── pipe_node.h
│   ├── pipe_link.h                # StageLink / CtrlLink / DirectLink
│   ├── pipe_arbitration.h
│   ├── pipe_builder.h
│   ├── plugin.h                   # Plugin 基类、Phase 枚举
│   ├── payload.h                  # Payload<T> Key
│   ├── payload_common.h           # uint_t<N> / bool_t 模式切换 + 通用 Key
│   ├── bundle_mapper.h
│   └── bundles/
│       ├── mem_bundle.h           # MemReqBundle / MemRespBundle (ch_uint)
│       └── intr_bundle.h
│
├── plugins/                       # ISA 无关 Plugin
│   ├── hazard.h
│   ├── branch_predictor.h
│   ├── reg_file.h
│   ├── ibus.h
│   └── dbus.h
│
├── arch/                          # ISA 特有
│   ├── riscv/
│   │   ├── decoder_table.h        # 译码表（funct3/funct7 → OpCode）
│   │   ├── payload_riscv.h        # RiscvDecodeDetail
│   │   ├── decode.h               # RiscvDecodePlugin
│   │   ├── int_alu.h
│   │   ├── mul.h
│   │   ├── branch.h
│   │   ├── csr.h
│   │   ├── lsu.h
│   │   └── tests/
│   │       ├── test_decode.cpp
│   │       ├── test_int_alu.cpp
│   │       └── test_riscv_tests.cpp     # 跑 riscv-tests
│   └── arm/                       # 预留：与 riscv 对称
│       └── (TBD)
│
├── configs/                       # 流水线配置
│   ├── cpu_default.json           # 5 级 RV32IM_Zicsr
│   ├── cpu_embedded.json          # 3 级 RV32I
│   ├── cpu_superscalar.json       # 7 级 RV64IMAFD
│   └── cpu_params_schema.json     # JSON Schema 校验
│
├── cpu_factory.h                  # 统一入口：build_cpu(config, ImplMode)
│
└── tests/                         # 跨 ISA / 框架级测试
    ├── unit/
    │   ├── test_pipe_node.cpp
    │   ├── test_pipe_builder.cpp
    │   ├── test_payload.cpp
    │   └── test_ctrl_link.cpp
    ├── integration/
    │   ├── test_5stage_riscv.cpp
    │   └── test_3stage_riscv.cpp
    └── compare/
        └── test_tlm_rtl_compare.cpp # COMPARE 模式
```

**取消的目录**（与旧版对比）：

| 旧目录 | 状态 | 替代案 |
|--------|------|----------|
| `ip/cpu/tlm/` | ❌ 取消 | 通过 `ImplMode::TLM` 由同一份代码生成 |
| `ip/cpu/rtl/` | ❌ 取消 | 通过 `ImplMode::RTL` 由同一份代码生成 |
| `core/exec_context.h` | ❌ 取消 | ISA 逻辑直接以 Plugin 形式存在 |

---

## 8. 验证策略

### 8.1 三级测试金字塔

```
        ┌──────────────────────┐
        │  C: COMPARE          │   TLM ↔ RTL 逐 Node 对比
        │  (跨模式一致性)       │
        ├──────────────────────┤
        │  B: 集成测试          │   完整流水线 + riscv-tests
        │  (riscv-tests)       │
        ├──────────────────────┤
        │  A: Plugin 单元测试   │   单 Node 最小流水线 + Mock
        │  (单 Plugin)         │
        └──────────────────────┘
```

### 8.2 Level A：Plugin 单元测试

**目标**：验证单个 Plugin 在最小流水线（1~2 个 Node）内的行为正确性。

```cpp
// arch/riscv/tests/test_int_alu.cpp

TEST(RiscvIntAlu, AddBasic) {
    PipeBuilder pb(ImplMode::TLM);
    auto* ex = pb.create_node("EX");
    pb.bind_logic_stage("execute", ex);

    pb.register_plugin(std::make_unique<RiscvIntAluPlugin>());
    pb.build();

    DecodePayload d{}; d.op_class = OpClass::ALU; d.writes_rd = true;
    RiscvDecodeDetail rd{}; rd.funct3 = 0; rd.funct7 = 0;  // ADD
    (*ex)(pl::DECODE) = d;
    (*ex)(pl::RISCV_DETAIL) = rd;
    (*ex)(pl::RS1) = 3;
    (*ex)(pl::RS2) = 4;
    ex->arb().valid = true; ex->arb().ready = true;

    pb.tick();
    EXPECT_EQ((*ex)(pl::RESULT), 7u);
}
```

### 8.3 Level B：集成测试

**目标**：跑完整 RV32 程序（riscv-tests），确认 ISA 实现正确。

```cpp
TEST(Integration, RV32I_AddInst) {
    auto pb = build_cpu(load_json("configs/cpu_default.json"), ImplMode::TLM);
    load_elf(*pb, "rv32ui-p-add.elf");
    run_until_tohost(*pb);
    EXPECT_EQ(read_tohost(*pb), 1);   // 1 = pass
}
```

### 8.4 Level C：COMPARE 模式

**目标**：在 TLM 与 RTL 模式下并行运行同一程序，**逐 Node 比较 Payload**，找出实现分歧。

```cpp
TEST(Compare, RV32I_AllPayloadsMatch) {
    auto tlm = build_cpu(cfg, ImplMode::TLM);
    auto rtl = build_cpu(cfg, ImplMode::RTL);
    CompareDriver drv(tlm.get(), rtl.get());
    drv.load_elf("rv32ui-p-add.elf");
    drv.run_steps(10000);
    EXPECT_TRUE(drv.all_payloads_match());
}
```

`PipeBuilder::ImplMode::COMPARE` 内置 dual-run：每 tick 运行一次 TLM 调度 + 一次 RTL 仿真，
比较所有 Node 上所有 Payload Key 的值，发现差异立即报错。

### 8.5 测试目录组织

| 路径 | 级别 | 内容 |
|------|------|------|
| `arch/riscv/tests/` | A + B | RISC-V 单 Plugin 测试 + RISC-V 集成测试 |
| `tests/unit/` | A | 框架核心（PipeNode/PipeBuilder/Payload/CtrlLink）单元测试 |
| `tests/integration/` | B | 跨 Plugin 完整流水线测试 |
| `tests/compare/` | C | TLM vs RTL 对比 |

---

## 9. 关键设计决策记录（ADR）

### ADR-1：取消 ExecContext

- **Context**：旧版 v1.x 引入 `ExecContext` 作为 ISA 译码 / 执行的运行时上下文容器，封装当前指令、寄存器视图、副作用接口。
- **Decision**：完全取消 `ExecContext`。ISA 译码与执行逻辑直接以 Plugin 的 `at_stage()` lambda 声明绑定到 PipeNode。
- **Rationale**：
  1. `ExecContext` 与 Plugin 职责高度重叠（都是「指令在某阶段做什么」）
  2. 多一层抽象 = 多一份模板代码 + 多一处调试断点
  3. 直接读写 PipeNode 的 Payload 更贴合「生成式」范式
- **Consequences**：
  - ✅ ISA 实现更直观，无需理解上下文生命周期
  - ✅ 减少一个核心 header（`exec_context.h` 删除）
  - ⚠️ ISA Plugin 需直接调用 `(*n)(pl::XXX)` 访问 Payload，需熟悉 Key 表

### ADR-2：Plugin 声明式模型（无 tick）

- **Context**：传统 SystemC / Gem5 模型中，每个组件实现 `tick()` 自行驱动状态。Plugin 数量增加后调度顺序难以推理。
- **Decision**：`Plugin` 基类**只有** `setup()` 和 `build()`，**没有** `tick()`。一切逻辑通过 `builder->at_stage(stage, phase, lambda)` 声明。框架（`PipeBuilder::tick()`）统一调度。
- **Rationale**：
  1. 用户无法重写调度顺序 → 调度由拓扑 + 注册顺序确定性决定
  2. Plugin 即「逻辑描述」，不再持有时序状态
  3. 同一份描述可生成 TLM 调度表，也可生成 RTL 硬件 AST
- **Consequences**：
  - ✅ TLM/RTL 双模式天然统一
  - ✅ 调度顺序完全可预测
  - ⚠️ 跨 tick 状态需通过 Payload / Plugin 成员变量显式表达

### ADR-3：逻辑阶段名标识

- **Context**：流水线深度可配置（3/5/7/10+ 级）。如果 Plugin 直接绑定到物理 Node 名（如 "EX"），换一个深度就要重写 Plugin。
- **Decision**：Plugin 绑定到**逻辑阶段名**（`"decode"` / `"execute"` / `"memory"` ...）。`PipeBuilder` 根据 JSON 配置 `pipeline_stages` 将逻辑阶段映射到一个或多个物理 PipeNode。
- **Rationale**：
  1. Plugin 实现与流水线深度解耦
  2. 配置变更（如合并 EX+MEM）无需修改 Plugin 代码
- **Consequences**：
  - ✅ 同一 Plugin 在 3 级 / 5 级 / 7 级流水线下都可工作
  - ⚠️ 引入「逻辑 → 物理」一层间接，需要校验和清晰文档

### ADR-4：Phase 子阶段执行顺序

- **Context**：同一逻辑阶段内可能有多个 Plugin（HazardPlugin、IntAluPlugin、MulPlugin），需要明确执行顺序。
- **Decision**：在 logic_stage 内引入 `Phase::EARLY / NORMAL / LATE` 三个子阶段，
  执行顺序 = `(Phase, Plugin 注册顺序)` 字典序。
- **Rationale**：
  1. 冒险检测必须在运算前（EARLY），写回准备必须在运算后（LATE）
  2. 三档足够覆盖典型需求；过细的优先级数字反而难维护
  3. Plugin 注册顺序作为打破并列的二级 Key，开发者易控制
- **Consequences**：
  - ✅ 顺序声明式可见，框架强制
  - ✅ 用户在 `cpu_factory.h` 控制 Plugin 注册顺序即可控制执行顺序
  - ⚠️ 跨 ISA 切换 Plugin 集合时需重新审视注册顺序

### ADR-5：CtrlLink 声明式控制

- **Context**：CPU 控制（halt / throw / flush / bypass）传统上由组件命令式驱动，难以追溯。
- **Decision**：在 `CtrlLink` 上提供声明式 API：`halt_when(cond)` / `throw_when(cond)` / `flush_when(cond)` / `bypass(key, src)`。Plugin 在 `build()` 中**声明**条件，框架在每拍组合所有声明并求值。
- **Rationale**：
  1. 多个 Plugin 可独立声明 halt 条件，最终框架做 OR 合并
  2. 声明式表达 = 与硬件描述一致 = 易综合
- **Consequences**：
  - ✅ CPU 控制流可视化（一个 CtrlLink 上挂哪些条件一目了然）
  - ✅ TLM/RTL 双模式统一实现
  - ⚠️ Plugin 之间通过 CtrlLink 隐式协作，需文档明确职责

### ADR-6：双 Payload 共存

- **Context**：通用 Plugin（HazardPlugin）需读寄存器号；ISA 特有 Plugin（RiscvIntAlu）需 funct3/funct7。两类信息粒度不同。
- **Decision**：在同一 PipeNode 上同时存放 **ISA 无关 Payload**（`DecodePayload`）和 **ISA 特有 Payload**（`RiscvDecodeDetail`）。Decode Plugin 一次填充两份。
- **Rationale**：
  1. 通用 Plugin 完全不依赖 ISA 字段 → 跨 ISA 复用
  2. ISA Plugin 直接访问特有字段，无需向上转型
- **Consequences**：
  - ✅ HazardPlugin / BranchPredictor / RegFile 跨 ISA 零修改复用
  - ⚠️ Decode Plugin 需要维护两份字段 → 通过宏 / 表驱动减负
  - ⚠️ Payload 总量略多 → POD 字段成本可忽略

### ADR-7：统一目录结构

- **Context**：旧版 `ip/cpu/tlm/` 与 `ip/cpu/rtl/` 分离，导致 Plugin 实现重复。
- **Decision**：取消 `tlm/` 与 `rtl/` 分离，改为以**职责**组织目录：
  - `core/`（框架）
  - `plugins/`（ISA 无关）
  - `arch/<isa>/`（ISA 特有）
  - `configs/`（JSON）
  - `cpu_factory.h`（统一入口，`ImplMode` 参数选模式）
  - `tests/`（测试）
- **Rationale**：
  1. 双模式由编译期 / 运行期开关切换，不应反映在目录上
  2. 按职责组织 = 与「生成式」范式呼应
- **Consequences**：
  - ✅ 同一份 Plugin 代码服务两种模式
  - ✅ 新人 onboarding 路径清晰：core → plugins → arch
  - ⚠️ 需要 CI 同时跑 TLM 与 RTL 模式，避免一种模式悄悄破坏

---

## 10. 参考文档

### 10.1 项目内文档

- [ip/cpu/README.md](../README.md) — IP/CPU 总览
- [ip/cpu/docs/README.md](./README.md) — 文档索引
- [ip/cpu/docs/riscv/VexRiscvArch.md](./riscv/VexRiscvArch.md) — VexRiscv 架构参考
- [ip/cpu/docs/riscv/VexRiscvOnCppTLM.md](./riscv/VexRiscvOnCppTLM.md) — VexRiscv 在 CppTLM 的映射
- [ip/cpu/configs/cpu_params_schema.json](../configs/cpu_params_schema.json) — JSON Schema
- [docs/architecture/overview.md](../../../docs/architecture/overview.md) — 顶层架构
- [docs/architecture/interface-design.md](../../../docs/architecture/interface-design.md) — 接口设计
- [docs/architecture/tech-selection.md](../../../docs/architecture/tech-selection.md) — 技术选型
- [docs/architecture/testing-and-dse.md](../../../docs/architecture/testing-and-dse.md) — 验证与 DSE
- [docs/GLOSSARY.md](../../../docs/GLOSSARY.md) — 术语表

### 10.2 外部参考

- [SpinalHDL Pipeline API](https://spinalhdl.github.io/SpinalDoc-RTD/master/SpinalHDL/Libraries/Pipeline/index.html) — 本框架的核心灵感
- [VexiiRiscv](https://github.com/SpinalHDL/VexiiRiscv) — Plugin + Pipeline 范式参考实现
- [SpinalHDL Stream / Flow / Fragment](https://spinalhdl.github.io/SpinalDoc-RTD/master/SpinalHDL/Libraries/stream.html)
- [Cash HDL (CppHDL)](https://github.com/gtcasl/cash) — RTL 生成器
- [CppTLM](https://github.com/microsoft/CppTLM) — TLM 仿真框架（v2.0.3）
- [RISC-V Specification](https://riscv.org/technical/specifications/)
- [riscv-tests](https://github.com/riscv-software-src/riscv-tests) — 集成测试套件
- [Gem5](https://www.gem5.org/) — 多 ISA 对比参考
- [QEMU TCG](https://wiki.qemu.org/Documentation/TCG) — 解释器对比
- [Chipyard](https://chipyard.readthedocs.io/) — Generator 范式对比

---

## 11. 附录: 实施状态标注 (2026-06-17 实证校核)

> 本节**由审计添加**, 用于区分"设计意图 (本文 §1-9)" vs "已落地实施 (2026-06-17 实证)"。
> 详细 DSE 实施方案见 [`dse_architecture.md`](dse_architecture.md) v1.0。

### 11.1 已落实的部分

| § | 设计意图 | 落地状态 (2026-06-17) |
|---|---------|----------------------|
| §3.2 Plugin 调度顺序 (EARLY/NORMAL/LATE) | 设计意图 | ✅ Phase enum + PipeBuilder::build() 顺序调 setup → build 已实现 |
| §4.2 Pipeline 内部 Payload Bundle (POD-style) | 设计意图 | ✅ `core/payload_common.h` 实现 8+3 通用 Key |
| §5.1 取消 ExecContext | 设计意图 | ✅ 无 ExecContext 类, ISA 逻辑全部 Plugin 化 |
| §5.2 双 Payload 共存 (通用 + ISA 特有) | 设计意图 | ✅ `payload_common.h::DecodePayload` + `payload_riscv.h::RiscvDecodeDetail` |
| §6.1 JSON 配置 Schema | 设计意图 | ✅ `cpu_params_schema.json` 存在 |
| §7 目录结构 | 设计意图 | ✅ core/plugins/arch/configs/ 目录结构符合 |

### 11.2 设计超前于实现的部分 (待 M4-DSE / M5-DSE 实施)

| § | 设计意图 | 当前差距 (2026-06-17) |
|---|---------|----------------------|
| §5.3 ISA 切换 = 更换 Plugin Registry | CpuFactory 应 dispatch `isa == "riscv"` / `"arm"` | ❌ **CpuFactory::build_cpu() 是空 stub**, 三 register_*_plugins 方法仅 `(void)pb; (void)sizeof(U);`, 0 个 plugin 被注册 |
| §6.2 3/5/7/10 级映射 | pipeline_stages 字段控制拓扑 | ❌ `CpuFactory` stub 未消费 `pipeline_stages`; `declare_substage` 当前 `depth` 参数被注释, 无法合并 Node |
| §6.3 Plugin declare_substage (mul 子流水) | FpuPlugin 用 declare_substage 声明子流水 | ⚠️ `BranchPredictorPlugin` 等未用 declare_substage; 仅 `L1CachePlugin` 调过 1 次 |
| §6.4 配置校验 | JSON 校验后再 build | ⚠️ JSON 解析未实现 (CpuFactory 接受 `CPUConfig` struct, 不接受 JSON 字符串) |
| §6.5 三种配置实例 (default/embedded/superscalar) | superscalar/deep_pipeline 示例 | ⚠️ superscalar.json 缺失 (待 M5.18 新建) |

### 11.3 当前真正可调的维度

**仅以下 5 个 `CPUConfig` 字段在改造后生效** (见 [`dse_architecture.md` §2.1](dse_architecture.md)):

1. `isa ∈ {rv32i, rv32im, rv32imac, rv64i, rv64gc}` (字符串, factory dispatch)
2. `pipeline_stages ∈ {3, 5, 7, 10}` (拓扑展开)
3. `branch_predictor ∈ {static, bimodal, gshare, tournament}` (4 选 1)
4. `btb_entries ∈ {16, 32, 64, 128, 256}` (编译期 5 选 1)
5. `ext_m / ext_a / ext_f / ext_d / ext_zicsr / ext_zifencei` (6 个 ISA 扩展开关, 新增)

### 11.4 明确推迟 (Phase 5+)

| 维度 | 推迟理由 |
|------|----------|
| 乱序 (OoO) / ROB / RS / Rename / Wakeup | Phase 5+, 无抽象 |
| 超标量 / 多发射 / Issue Queue | Phase 5+ |
| 跨 ISA (非 RISC-V) | `payload_common.h:115,117` 静态断言卡 XLEN=32/64 |
| L2/L3 cache / 真实 MMU/PMP / FPU | `ip/memory/` 仅 README, plugin 占位 |
| 多核 / SMT | Phase 6+ |
| RTL_ONLY / COMPARE | `cf::plugin::ImplMode` 枚举未在 PipeBuilder 实现 |

---

*文档结束。*

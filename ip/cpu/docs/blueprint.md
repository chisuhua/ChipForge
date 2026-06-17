# ip/cu CPU 静态架构蓝图 (Blueprint)

> **本文件位置**: `ip/cpu/docs/blueprint.md`
> **状态**: 🟢 Accepted (从 `cpu_implementation_guide_v2.0.md` 拆分而来, 内容未改)
> **版本**: v2.0 (2026-06-15 决策)
> **作用**: CPU 终态**长什么样**的长期参考。**改动少**, 关注点: 微架构、目录结构、Plugin 套件、cf_plugin 扩展点。

> **本文件不包含**实施规划 (M1-M5 阶段任务)、任务状态、议题 1-8 的"为什么这样选"论证。 这些见:
> - 总体实施规划: [`implementation-plan/README.md`](implementation-plan/README.md)
> - 各阶段实施规划: [`implementation-plan/M1..M5`](implementation-plan/)
> - 任务状态看板: [`status.md`](status.md)
> - 决策快照 + 入口: [`cpu_implementation_guide_v2.0.md`](cpu_implementation_guide_v2.0.md)

---

## 1. 核心定位: ISA 与 CPU 架构解耦

**本阶段 (Phase 1.5) 的核心目标**:

> **用声明式电路方法学 (Plugin-style) 设计指令集 (ISA) 和 CPU 架构相互独立的 RISC-V 核, 为以后 CPU 架构探索 (DSE) 打下基础。**

这意味着:

- **ISA 层 (riscv/)** = 纯 ISA 实现, 不绑定特定 CPU 架构 (流水线深度/分支预测策略等)
- **CPU 架构层 (configs/)** = 流水线配置 (3/5/7 级), 决定 CPU 内部结构
- **耦合点 (CpuFactory)** = 通过 Plugin 集合 + 流水线配置 JSON 实例化 CPU

**为什么重要**:

| 不解耦 (旧) | 解耦 (v2.0) |
|-------------|-------------|
| CPU 实现 = 单一 ISA + 单一流水线 (改 ISA 全部重写) | CPU 实现 = 流水线骨架 + ISA Plugin 集合 (改 ISA 换 Plugin, 不改骨架) |
| 流水线深度硬编码 (3 级 → 5 级需重写) | 流水线深度由 JSON 决定, Plugin 不绑物理 Node, 绑 logical_stage |
| 5 级流水 RV64IFD = 5 级流水 RV32IM 两次实现 | 5 级流水 = JSON 配置, RV64IFD vs RV32IM 仅 Plugin 集合不同 |
| CPU 架构 DSE = 改源代码 | CPU 架构 DSE = 改 JSON 配置 (DSE 的本质) |

**这与 multi_isa_architecture.md v2.0 §1.1 "项目目标" 完美对齐**:

> ChipForge IP/CPU 子系统是一个**多 ISA、可配置、TLM/RTL 统一**的 CPU 建模框架
> - **多 ISA 支持**: 同一套流水线骨架支持 RISC-V / ARM / 自定义 ISA
> - **DSE (设计空间探索)**: 通过 JSON 配置快速生成不同流水线深度 (3/5/7/10+ 级)
> - **TLM/RTL 统一**: 同一份 Plugin 代码, TLM 高速仿真, RTL 生成硬件
> - **声明式开发**: Plugin 编写 = 声明逻辑绑定到阶段, 无需手写调度循环

---

## 2. 核心原则

1. **D4 Plugin-style Day 1 决策**: 业务代码**不**重写 `void tick()`, 不持有状态机, Bundle 字段用 `uint_t<N>`, 阶段用 `at_stage()`, 跨阶段用 `Payload<T>`。这是范式, 不是工具, 违反需 ADR
2. **方法学复用 (Phase 1.4 已验证)**: L1CachePlugin 6 维度方法学 (D1-D6) 是基线, CPU Plugin 偏离需 ADR 说明
3. **声明式开发**: Plugin 编写 = 声明阶段绑定, 调度由 PipeBuilder
4. **ISA 与 CPU 架构解耦**: ISA 特有 (RiscvDecodePlugin) vs ISA 无关 (HazardPlugin) 双 Payload; 流水线深度由 JSON 决定, 不绑物理 Node
5. **类型安全**: 跨阶段用 `Payload<T>` Key, 不裸指针
6. **Plugin 即范式**: 11 个 Plugin 是**最小可工作集**, 不是完整集 (Phase 5+ 可扩展: 浮点/向量/超标量)
7. **多 ISA 隔离**: 本期仅 RISC-V, 但目录结构与 CpuFactory 接口为未来 ARM 预留
8. **TLM/RTL 双模式**: 本期仅 `ImplMode::TLM`, 但 Plugin 写法 (用 `cf::plugin::uint_t<N>` 而非裸 `uint32_t`) 保证 Phase 5 切 RTL 零业务修改

---

## 3. 目录结构 (严格按 multi_isa v2.0 §7)

```
ip/cpu/
├── core/                          # 框架核心（ISA 无关、Plugin 无关）— 议题 1 选 C: 复用 cf_plugin
│   ├── pipe_node.h                # 复用 cf::plugin::PipeNode (Phase 0)
│   ├── pipe_link.h                # 需扩展: StageLink / CtrlLink / DirectLink
│   ├── pipe_arbitration.h         # 需扩展: valid/ready/cancel 三态
│   ├── pipe_builder.h             # 复用 cf::plugin::PipeBuilder + 扩展 at_stage / declare_substage
│   ├── plugin.h                   # 复用 cf::plugin::PluginBase + 扩展 Phase 枚举
│   ├── payload.h                  # 复用 cf::plugin::Payload<T> (Phase 0)
│   ├── payload_common.h           # 需新建: uint_t<N> / bool_t 模式切换 + 通用 DecodePayload
│   ├── bundle_mapper.h            # 推迟 Phase 5+ (RTL 阶段才需)
│   └── bundles/
│       ├── membundle.h            # 推迟 Phase 5+ (本期用 cf::bundles::MemReq/MemResp POD)
│       └── intrbundle.h           # 推迟 Phase 5+
│
├── plugins/                       # ISA 无关 Plugin (议题 2 选 B: 5 个核心优先)
│   ├── hazard.h                   # P0: 数据冒险检测 (用 DecodePayload)
│   ├── branch_predictor.h         # P1: BTB / Bimodal / GShare
│   ├── reg_file.h                 # P0: 通用寄存器堆 (xlen 参数化, 议题 3 选 B+C 用 array_store)
│   ├── ibus.h                     # P0: 取指总线 (CPU 对外)
│   ├── dbus.h                     # P0: 数据总线 (CPU 对外)
│   ├── fpu.h                      # 推迟 Phase 5+ (F/D 扩展, 在 plugins/ 中预留位置)
│   ├── mmu.h                      # 推迟 Phase 5+ (MMU, 暂不实现)
│   └── exception.h                # 推迟 Phase 5+ (异常处理)
│
├── arch/                          # ISA 特有
│   └── riscv/                     # 本期实施 (议题 7 选 A: 仅 riscv)
│       ├── decoder_table.h        # P0: 译码表 (funct3/funct7 → OpCode)
│       ├── payload_riscv.h        # P0: RiscvDecodeDetail (funct3/funct7/imm/csr_idx)
│       ├── decode.h               # P0: RiscvDecodePlugin (同时填通用 + ISA Payload)
│       ├── int_alu.h              # P0: RiscvIntAluPlugin (RV32I/RV64I 整数)
│       ├── mul.h                  # P1: RiscvMulPlugin (M 扩展, 3 级子流水)
│       ├── branch.h               # P1: RiscvBranchPlugin
│       ├── csr.h                  # P2: RiscvCsrPlugin (Zicsr)
│       ├── lsu.h                  # P1: RiscvLsuPlugin (load/store)
│       ├── fpu.h                  # 推迟 Phase 5+ (F/D 扩展)
│       └── tests/                 # arch 级单元 + 集成测试
│           ├── test_decode.cpp
│           ├── test_int_alu.cpp
│           ├── test_mul.cpp
│           ├── test_branch.cpp
│           ├── test_lsu.cpp
│           ├── test_csr.cpp
│           └── test_riscv_integration.cpp
│
├── configs/                       # 流水线配置 (议题 4 选 B: multi_isa v2.0 §6.1 标准字段)
│   ├── cpu_default.json           # 5 级 RV32IM_Zicsr (multi_isa v2.0 §6.5 字段)
│   ├── cpu_embedded.json          # 3 级 RV32I
│   ├── cpu_superscalar.json       # 7 级 RV64IMAFD (Phase 5+, 字段先定义)
│   └── cpu_params_schema.json     # JSON Schema 校验 (按 multi_isa v2.0 §6.1)
│
├── cpu_factory.h                  # 统一入口 (议题 5 选 B: 内置 PluginOrder 列表)
│
└── tests/                         # 跨 ISA / 框架级测试 (议题 8 选 B: 手工编译 ELF)
    ├── unit/
    │   ├── test_pipe_node.cpp
    │   ├── test_pipe_builder.cpp
    │   ├── test_payload.cpp
    │   └── test_ctrl_link.cpp
    ├── integration/
    │   ├── test_5stage_riscv.cpp
    │   └── test_3stage_riscv.cpp
    └── manual_elf/                # 手工编译最小 ELF (议题 8 选 B)
        ├── add.S
        ├── link.ld
        └── README.md              # 编译 + Spike 对比脚本
```

### 3.1 与 multi_isa v2.0 §7 差异 (我们接受但标注)

- `ip/cpu/tlm/` 与 `ip/cpu/rtl/` 目录**取消**, 由 `ImplMode` 编译期切换 (multi_isa v2.0 §7 决定)
- 旧 `docs/riscv/VexRiscvArch.md` 与 `VexRiscvOnCppTLM.md` **已 git rm** (2026-06-13 19:43), 留 `riscv/` 目录待新内容
- 旧 `configs/cpu_default.json` 字段需按 multi_isa v2.0 §6.1 重写 (议题 4 选 B)
- 本期插件套件仅 5+1 (RegFile/Hazard/BP/IBus/DBus + Decode), FPU/MMU/Exception 推迟

---

## 4. 核心 Plugin 套件 (5+6 Plugin 拆分, 议题 2 选 B)

### 4.1 ISA 无关 Plugin (5 个, 议题 2 选 B: 核心优先)

| Plugin | 职责 | 输入 Payload | 输出 Payload | 优先级 | M 里程碑 |
|--------|------|--------------|--------------|--------|---------|
| `RegFilePlugin` | 通用寄存器堆 (xlen 参数化) | pl::RS1, RS2 (读), pl::RD_IDX, RD_DATA (写) | 无 (写回 regfile_) | **P0** | M2 |
| `HazardPlugin` | RAW/WAW/WAR 数据冒险检测 | pl::DECODE (rs_idx + uses_*) | CtrlLink.halt_when(cond) | **P0** | M2 |
| `BranchPredictorPlugin` | BTB / Bimodal / GShare | pl::DECODE.is_branch | PC redirect (CtrlLink.bypass) | P1 | M2 (stub) |
| `IBusPlugin` | 取指总线接口 (CPU 对外) | pl::PC | MemReqBundle (外发) + pl::INSTRUCTION (回收) | **P0** | M2 |
| `DBusPlugin` | 数据总线接口 | pl::LSU_REQ | MemReqBundle (外发) + pl::LSU_RESP (回收) | **P0** | M2 |

### 4.2 RISC-V ISA 特有 Plugin (6 个, 议题 2 选 B: 核心优先)

| Plugin | 职责 | 输入 Payload | 输出 Payload | 优先级 | M 里程碑 |
|--------|------|--------------|--------------|--------|---------|
| `RiscvDecodePlugin` | 译码并填两份 Payload | pl::INSTRUCTION | pl::DECODE (通用) + pl::RISCV_DETAIL (ISA 特有) | **P0** | M3 |
| `RiscvIntAluPlugin` | RV32I/RV64I 整数运算 | pl::DECODE, pl::RS1, pl::RS2 | pl::RESULT | **P0** | M3 |
| `RiscvBranchPlugin` | 分支跳转 + 链接 | pl::DECODE, pl::RS1, pl::IMM | pl::BRANCH_TARGET, pl::RD_DATA | P1 | M3 |
| `RiscvMulPlugin` | M 扩展乘除法 (3 级子流水) | pl::RS1, pl::RS2 | pl::RESULT (3 拍后) | P1 | M3 |
| `RiscvLsuPlugin` | load/store (含地址生成) | pl::DECODE, pl::RS1, pl::IMM | pl::LSU_REQ → MemReqBundle | P1 | M3 |
| `RiscvCsrPlugin` | Zicsr 读写 | pl::DECODE, pl::RS1, pl::IMM | pl::RESULT, pl::CSR_WRITTEN | P2 | M3 (stub) |

### 4.3 ISA 无关与 ISA 特有 Plugin 的关系 (multi_isa v2.0 §5.2 双 Payload)

```
RiscvDecodePlugin  (arch/riscv/decode.h)
  ↓ 同时填两份
  ├── pl::DECODE      (通用, HazardPlugin / BranchPredictor / RegFile / IBus / DBus 读)
  └── pl::RISCV_DETAIL (ISA 特有, IntAlu / Mul / Branch / Lsu / Csr 读)
```

**关键**: 通用 Plugin **只**读 `pl::DECODE`, **不**依赖 ISA 字段; ISA Plugin **直接**访问 `pl::RISCV_DETAIL`, 无需转型。 这是 multi_isa v2.0 §5.2 双 Payload 共存的设计, 让 HazardPlugin 跨 ISA 零修改复用。

---

## 5. CpuFactory — 流水线组装入口 (议题 5 选 B)

> **⚠️ 2026-06-17 实证状态**: 当前 `ip/cpu/cpu_factory.h` 的 `build_cpu()` 是 **空 stub** — 三个 `register_*_plugins` 私有方法仅 `(void)pb; (void)sizeof(U);`, **零个 plugin 被注册**。本文档描述的是**设计意图**, 真实落实路径见 [`dse_architecture.md`](dse_architecture.md) §7 (M4-DSE 实施后 `build_cpu` 真实实现)。
>
> 详细实施状态: [`dse_architecture.md` §1.1](../dse_architecture.md) (审计结果) + [`status.md` §4.1](../status.md) (M4-DSE 子任务清单)

```cpp
// ip/cpu/cpu_factory.h
// 议题 5 选 B: CpuFactory 内置 PluginOrder 列表, 集中管理
// 当前 (M4 之前): STUB — 11 个 plugin 一个都没注册
// M4-DSE 实施后 (见 dse_architecture.md §7): 真实 register 所有 plugin + 拓扑展开 + BTB/MUL 编译期 switch

#pragma once

#include <memory>
#include <nlohmann/json.hpp>

// 复用 cf_plugin (议题 1 选 C)
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/payload.h"

// ISA 无关 Plugin (P0)
#include "ip/cpu/plugins/reg_file.h"
#include "ip/cpu/plugins/hazard.h"
#include "ip/cpu/plugins/branch_predictor.h"
#include "ip/cpu/plugins/ibus.h"
#include "ip/cpu/plugins/dbus.h"

// ISA 特有 Plugin (P0)
#include "arch/riscv/decode.h"
#include "arch/riscv/int_alu.h"

namespace cf {
namespace cpu {

class CpuFactory {
 public:
  // 议题 5 选 B: 集中管理 Plugin 注册顺序, 单一真相源
  // 顺序 = (Phase, Plugin 注册顺序) 字典序, 由 multi_isa v2.0 §3.2 定义
  // 注册顺序: EARLY 阶段先, NORMAL 后, LATE 最后
  // 同 Phase 内: Plugin 注册顺序 (CpuFactory 集中管理)

  static std::unique_ptr<cf::plugin::PipeBuilder> build_cpu(
      const nlohmann::json& config,
      cf::plugin::ImplMode mode = cf::plugin::ImplMode::TLM) {
    auto pb = std::make_unique<cf::plugin::PipeBuilder>(mode);

    // --- ISA 无关 Plugin (P0 必选) ---
    pb->register_plugin(std::make_unique<RegFilePlugin>(
        config.value("xlen", 32)));
    pb->register_plugin(std::make_unique<HazardPlugin>());
    pb->register_plugin(std::make_unique<IBusPlugin>());
    pb->register_plugin(std::make_unique<DBusPlugin>());

    // --- ISA 特有 Plugin (由 config["isa"] 决定) ---
    // 议题 7 选 A: 仅 riscv, 不创建 arch/arm/ 目录
    // 未来扩展: 只需添加 else if (isa == "arm") { ... } 即可
    const std::string& isa = config.at("isa");
    if (isa == "riscv") {
      // RISC-V 扩展
      pb->register_plugin(std::make_unique<RiscvDecodePlugin>());

      // P0: RV32I/RV64I 整数运算
      pb->register_plugin(std::make_unique<RiscvIntAluPlugin>());

      // P1: M 扩展 (可选)
      if (config.value("ext_m", false)) {
        pb->register_plugin(std::make_unique<RiscvMulPlugin>());
      }

      // P1: 分支 (RV32I 必有)
      pb->register_plugin(std::make_unique<RiscvBranchPlugin>());

      // P1: load/store
      pb->register_plugin(std::make_unique<RiscvLsuPlugin>());

      // P1: 分支预测
      pb->register_plugin(std::make_unique<BranchPredictorPlugin>(
          config.value("branch_predictor",
                       nlohmann::json{{"type", "bimodal"}, {"btb_entries", 64}})));

      // P2: Zicsr (可选)
      if (config.value("ext_zicsr", false)) {
        pb->register_plugin(std::make_unique<RiscvCsrPlugin>());
      }
    }
    // 预留: else if (isa == "arm") { ... } (Phase 5+)

    pb->build();  // 校验 + 生成调度表
    return pb;
  }
};

}  // namespace cpu
}  // namespace cf
```

### 5.1 PluginOrder 设计原则 (议题 5 选 B)

- **CpuFactory 是 PluginOrder 单一真相源**: 调度顺序的真相 (source of truth) 在 CpuFactory 的 `build_cpu()` 静态方法中
- **同 Phase 内顺序 = CpuFactory 注册顺序**: 用户修改 CpuFactory 即控制执行顺序
- **跨 Phase 顺序 = EARLY → NORMAL → LATE**: 由 PipeBuilder 强制 (multi_isa v2.0 §3.2 + §4)
- **JSON `plugins[]` 数组不控制顺序** (议题 5 排除选项 C): JSON 仅决定 "哪些 Plugin 被实例化", 不决定 "执行顺序"。 避免多源真相

### 5.2 5 级流水线 (默认配置, `configs/cpu_default.json`)

```
IF (fetch) → ID (decode) → EX (execute) → MEM (memory) → WB (writeback)
  │            │              │              │             │
  ├─IBusPlugin─┤              │              ├─DBusPlugin──┤
  │            ├─DecodePlugin─┤              │             ├─RegFilePlugin
  │            │              ├─IntAlu/Mul──┤             │
  │            ├─HazardPlugin─┤              │             │
  │            ├─BranchPredic─┤              │             │
```

### 5.3 3 级嵌入式流水线 (`configs/cpu_embedded.json`)

```
IF (fetch+decode) → EXMEM (execute+memory) → WB (writeback)
```

---

## 6. 复用 cf_plugin + 扩展 (议题 1 选 C)

### 6.1 复用 vs 扩展决策表

| multi_isa v2.0 §2-4 类 | cf_plugin 现状 (Phase 0) | 决策 | 实施 |
|------------------------|---------------------------|------|------|
| **PipeNode** | ✅ cf::plugin::PipeNode (Phase 0) | **复用** | 直接 include |
| **Plugin** | ✅ cf::plugin::PluginBase (Phase 0) | **复用** | 直接 include |
| **Payload\<T\>** | ✅ cf::plugin::Payload\<T\> (Phase 0) | **复用** | 直接 include |
| **PipeBuilder** | ✅ cf::plugin::PipeBuilder (Phase 0) | **复用 + 扩展** | 复用基类, 扩展 `at_stage` / `declare_substage` / `tick` |
| **PayloadCommon (uint_t\<N\> / bool_t)** | ❌ cf_plugin 无独立文件 | **新建** | `ip/cpu/core/payload_common.h` (但 cf::plugin::uint_t 已存在, 仅需包装) |
| **PipeLink (StageLink / CtrlLink / DirectLink)** | ⚠️ cf::plugin::CtrlLink (Phase 0) | **扩展** | CtrlLink 复用, 新增 StageLink (cb::plugin 缺) + DirectLink |
| **PipeArbitration (valid/ready/cancel)** | ❌ cf_plugin 无 | **新建** | `ip/cpu/core/pipe_arbitration.h` |
| **BundleMapper** | ❌ cf_plugin 无 | **推迟 Phase 5+** | RTL 阶段才需 |
| **MemReqBundle / MemRespBundle (ch_uint)** | ❌ cf_plugin 无 (但有 POD 版本 in `bundles/mem_bundles.h`) | **推迟 Phase 5+** | 本期用 POD `cf::bundles::MemReq` |

### 6.2 cf_plugin 扩展点 (议题 1 选 C: 复用 + 扩展)

#### 6.2.1 PipeBuilder 扩展: `at_stage` / `declare_substage`

**现状**: cf::plugin::PipeBuilder (Phase 0) 提供 `at_stage` 但无 `declare_substage`。

**扩展**: 在 `ip/cpu/core/pipe_builder_ext.h` 提供:

```cpp
// 扩展接口 (在 cf::plugin::PipeBuilder 基础上)
class PipeBuilderExt {
 public:
  // multi_isa v2.0 §3.2 at_stage
  void at_stage(const std::string& logic_stage,
                Phase phase,
                std::function<void()> logic);

  // multi_isa v2.0 §3.3 declare_substage (新增)
  PipeNode* declare_substage(const std::string& parent_logic_stage,
                             const std::string& sub_name,
                             int depth);

  // multi_isa v2.0 §2.4 build() / tick()
  void build();      // 校验 + 生成调度表
  void tick();       // TLM 模式: 执行调度表
};
```

**实施方式**: 选 2 个之一:
- (A) **直接修改 cf::plugin::PipeBuilder** (在 `include/cf/plugin/pipe_builder.h` 加 `at_stage` / `declare_substage`)
- (B) **新建 `ip/cpu/core/pipe_builder.h`** 包装 cf::plugin::PipeBuilder

推荐 (A): cf_plugin 是 framework, ip/cpu 复用它, 但也扩展它。 修改 `include/cf/plugin/pipe_builder.h` 在 `setup` + `build` 之外增加 `at_stage` / `declare_substage` API。

#### 6.2.2 PipeLink 扩展: StageLink + DirectLink

**现状**: cf::plugin::CtrlLink (Phase 0) 已实现 `halt_when` / `throw_when` / `flush_when` / `bypass` 4 个控制 API (参见 ADR-033)。

**扩展**: 新增 `StageLink` (valid/ready 握手 + Payload 寄存一拍) + `DirectLink` (组合直连, 无寄存)。

**实施**: 在 `include/cf/plugin/pipe_link.h` 新增:

```cpp
class StageLink : public PipeLink {
 public:
  StageLink(PipeNode* up, PipeNode* down);
  // 继承 cf::plugin::CtrlLink 的 4 控制 API
};

class DirectLink : public PipeLink {
 public:
  DirectLink(PipeNode* up, PipeNode* down);
  // 无寄存, valid/ready 直通, Payload 引用透传
};
```

#### 6.2.3 PipeArbitration 新建

**新建**: `ip/cpu/core/pipe_arbitration.h` (独立文件, 避免与 cf_plugin 头文件耦合)

```cpp
struct PipeArbitration {
  bool valid;   // 上游产生有效数据
  bool ready;   // 下游可接受
  bool cancel;  // 取消 (异常/分支预测错误)
  // 派生状态 (与 SpinalHDL 一致):
  bool is_firing()   const { return valid && ready && !cancel; }
  bool is_moving()   const { return valid && ready; }
  bool is_blocked()  const { return valid && !ready; }
  bool is_canceling() const { return valid && cancel; }
};
```

集成到 PipeNode: PipeNode 持有 `PipeArbitration arb_` 成员。

#### 6.2.4 PayloadCommon 新建

**新建**: `ip/cpu/core/payload_common.h`

```cpp
// 跨 ISA 通用 Payload (HazardPlugin / BranchPredictor / RegFile 读)
struct DecodePayload {
  uint8_t  rs1_idx;    // 0..31
  uint8_t  rs2_idx;
  uint8_t  rd_idx;
  bool     uses_rs1;
  bool     uses_rs2;
  bool     writes_rd;
  bool     is_branch;
  bool     is_load;
  bool     is_store;
  uint8_t  op_class;   // ALU / MUL / MEM / BRANCH / SYS
};

// 通用 Payload Key (全局单例)
namespace pl {
  inline Payload<uint32_t>        PC{"PC"};
  inline Payload<uint32_t>        INSTRUCTION{"INSTRUCTION"};
  inline Payload<uint32_t>        RS1{"RS1"};
  inline Payload<uint32_t>        RS2{"RS2"};
  inline Payload<uint32_t>        RD_DATA{"RD_DATA"};
  inline Payload<uint8_t>         RD_IDX{"RD_IDX"};
  inline Payload<DecodePayload>   DECODE{"DECODE"};
  inline Payload<uint32_t>        RESULT{"RESULT"};
}
```

---

## 7. 方法学复用: L1CachePlugin 6 维度 (Phase 1.4 已验证)

### 7.1 L1CachePlugin 6 维度复盘结果 (Phase 1.4, 2026-06-13)

| 维度 | L1Cache 结论 | 复用策略 (本期 CPU) |
|------|-------------|---------------------|
| **D1 可读性** | 5/8 B1 接受, 3 个 B2 摩擦 | 复用: setup() + build() 职责分离, Payload Key 命名 `"cpu."` prefix |
| **D2 范式合规** | 5/5 D4 条款 + 4/4 ADR-040 Tier B1 接受 | 复用: 无 tick(), 无状态机, `uint_t<N>` 字段 |
| **D3 TLM↔RTL** | 2 B2 (harness 缺, COMPARE 待 Phase 5) | 推迟: CPU 本期仅 TLM, Phase 5+ RTL |
| **D4 阶段调度** | 2 B1 + 1 B2 (array_store 不全) | 复用: `at_stage(stage, phase, lambda)` |
| **D5 Payload 通信** | 1 B2 (Key 数量限制) | 复用: 双 Payload (通用 + ISA 特有) |
| **D6 测试便利** | 4/4 单元 + 5/5 e2e + 5/5 instantiateAll | 复用: 单元 + build_cpu e2e + 手工 ELF |

### 7.2 CPU 端预防 B2 摩擦 (来自 L1Cache 经验)

| B2 摩擦 (L1Cache) | CPU 端预防 |
|------------------|-----------|
| helper API 内部泄漏 (7 个 public helper) | friend class 隔离 (L1Cache 1.4 §2 模式) |
| array_store 抽象不完整 | 议题 3 选 B+C: 复用 cf_plugin storage.h 抽象 |
| 早返陷阱 (at_stage lambda 内 if 早返) | 统一约定 + lessons 文档化 (复用 `docs/lessons/phase-1.2-l1cacheplugin.md` §2.3) |
| Payload Key 数量限制 | 双 Payload (通用 + ISA 特有) — multi_isa v2.0 §5.2 |

### 7.3 CPU 特有风险与缓解

| 风险 | 缓解 |
|------|------|
| Plugin 数量多 (11 vs L1Cache 1) → 调度顺序确定性 | 议题 5 选 B: CpuFactory 集中注册顺序 |
| 流水线深度可配置 (3/5/7) → Plugin 跨深度复用 | 用 logical_stage 名 (multi_isa v2.0 §3) |
| RegFilePlugin 多 xlen (32/64) | 模板参数化, 编译期选 |
| MulPlugin 3 级子流水 vs 5 级合并 | declare_substage (议题 1 选 C: 复用 cf_plugin + 扩展) |
| CsrPlugin 270+ CSR 字段 | 表格驱动, 不在 at_stage 闭包内 if/else |
| picolibc 内存区域 64KB 限制 (议题 6 选 C) | 手工编译小 ELF, 远 < 64KB |

---

## 8. 与 VexRiscv 的关系 (议题 1 备注: 解决 VexRiscv 思路)

### 8.1 VexRiscv 简介

VexRiscv 是 SpinalHDL 生态最成功的开源 RISC-V CPU:
- 40+ 复杂 Plugin (Cache, MMU, CSR, Debug, Interrupt, MMU)
- 单一流水线骨架, Plugin 自由组合
- 支持 RV32I/M/A/C/F/D + Linux
- 用 Scala 编写, 编译为 Verilog/CHISEL

### 8.2 VexRiscv 思路借鉴 (不直接移植)

| VexRiscv 思路 | 我们借鉴方式 | 不借鉴原因 |
|--------------|-------------|-----------|
| 单一流水线骨架 + Plugin | ✅ 借用: multi_isa v2.0 §2 PipeNode/Link/Builder | 已是我们的核心 |
| Plugin 分类 (核心/扩展/可选) | ✅ 借用: 我们 §4.1 P0/P1/P2/P3 分类 | 已是我们的结构 |
| 流水线深度可配置 (3/5/7/10+) | ✅ 借用: multi_isa v2.0 §6 JSON | 已是我们的设计 |
| 跨 Plugin 服务接口 (DbusAccessService) | ❌ 不借鉴 | Phase 6 复杂度, 本期不引入 |
| 数据库驱动译码 (JDB) | ⚠️ 部分借鉴: JSON 译码表 | 我们更简单 (Phase 1 不需要 Scala 宏) |
| Debug Plugin | ❌ 不借鉴 | Phase 5+ 范围 |
| Multi-core 一致性 | ❌ 不借鉴 | Phase 6 范围 |
| Interrupt Controller (PLIC/CLINT) | ❌ 不借鉴 | Phase 3 范围 |

### 8.3 解决 VexRiscv 思路的具体行动 (议题 1 备注)

**"在实施过程中, 同时解决 VexRiscv 思路"** = 我们**借鉴 VexRiscv 的设计哲学**, 但**不照搬其实现**:

1. **参考 VexRiscv Plugin 分类思路** → 我们 §4 已按 P0/P1/P2/P3 分类
2. **解决 VexRiscv 在 C++ 17 中的对应** → 我们用 multi_isa v2.0 §2-4 描述的 C++ 抽象 (PipeNode/Link/Builder/Plugin/Payload)
3. **不引入 SpinalHDL 依赖** → 纯 CppTLM + C++ 17
4. **不直接移植 Scala 代码** → 我们写自己的 C++ 17 Plugin

**这一解决思路** 反映在:
- CpuFactory 集中管理 PluginOrder (§5.1)
- Plugin 套件按 P0/P1/P2/P3 拆分 (§4)
- 复用 cf_plugin + 扩展 (议题 1 选 C, §6)
- 联调路径 picolibc 绕过 MemoryTLM (议题 6 选 C, 见 `implementation-plan/README.md`)

---

## 相关文档

- **决策入口**: [`cpu_implementation_guide_v2.0.md`](cpu_implementation_guide_v2.0.md) — F1-F6 决议 + 议题 1-8 完整记录
- **总体实施规划**: [`implementation-plan/README.md`](implementation-plan/README.md)
- **各阶段实施规划**: [`implementation-plan/M1..M5`](implementation-plan/)
- **任务状态看板**: [`status.md`](status.md)
- **权威设计**: [`multi_isa_architecture.md`](multi_isa_architecture.md) v2.0
- **框架层**: [`docs/architecture/plugin-framework.md`](../../docs/architecture/plugin-framework.md)
- **方法学**: [`docs/methodology/plugin-style-design-methodology-v1.md`](../../docs/methodology/plugin-style-design-methodology-v1.md)
- **L1Cache 经验**: [`docs/lessons/phase-1.2-l1cacheplugin.md`](../../docs/lessons/phase-1.2-l1cacheplugin.md)

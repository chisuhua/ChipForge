# 架构设计决策注册表（Architecture Decision Register）

| 字段 | 值 |
|------|-----|
| 版本号 | 1.0 |
| 创建日期 | 2026-06-07 |
| 状态 | Active |
| 源文档 | [`code-framework-mapping.md`](./code-framework-mapping.md) + [`declarative-hybrid-framework.md`](./declarative-hybrid-framework.md) |
| 配套脚本 | [`tools/verify_adr.sh`](../../tools/verify_adr.sh) |

> **本文档定位**
>
> 本文档**提炼**两份架构文档中的所有设计决策点，每条决策附带**可机械执行的验证命令**。通过运行 `tools/verify_adr.sh`，可立即判断**当前代码实现是否仍与架构设计对齐**——任何"漂移"都会被报告为验证失败。
>
> **使用方式**：
> 1. 修改 CppTLM/CppHDL/ChipForge 代码后，运行 `tools/verify_adr.sh`
> 2. 任何 ✅ 决策的验证失败 = **代码漂移**，必须立即修复
> 3. 任何 🚧 决策的状态变化 = **进度更新**，需同步本文档
> 4. CI 集成：将脚本加入 PR 检查流水线
>
> **⚠️ 重要**：本脚本与配套技能 [`verify-architecture`](../../.opencode/skills/verify-architecture/SKILL.md) 共同构成完整的架构验证体系：
> - **`verify_adr.sh` 仅覆盖机械漂移**（文件路径、类名、宏定义、注册表）
> - **`verify-architecture` 技能补充设计意图审查**（抽象层级、命名、依赖方向、性能假设、非目标合规等）
> - **重大决策需同时使用两者**——仅靠脚本会漏掉"设计已不对"的语义漂移

---

## 目录

- [1. 状态图例与维护规则](#1-状态图例与维护规则)
- [2. 决策主表（快速总览）](#2-决策主表快速总览)
- [3. 详细决策记录](#3-详细决策记录)
  - [A. 框架架构](#a-框架架构-3-条)
  - [B. CppTLM 模块/接口](#b-cpptlm-模块接口-5-条)
  - [C. CppHDL 模块/接口](#c-cpphdl-模块接口-5-条)
  - [D. 注册与发现](#d-注册与发现-3-条)
  - [E. 端口与信号](#e-端口与信号-3-条)
  - [F. Bundle 与协议](#f-bundle-与协议-4-条)
  - [G. 声明式 Plugin 模型（Phase 1）](#g-声明式-plugin-模型phase-1-5-条)
  - [H. 流水线抽象（Phase 1）](#h-流水线抽象phase-1-4-条)
  - [I. 验证框架（Phase 1）](#i-验证框架phase-1-3-条)
  - [J. 目录与组织](#j-目录与组织-2-条)
  - [K. 范式决策](#k-范式决策-1-条)
- [4. 漂移检测规则](#4-漂移检测规则)
- [5. 验证脚本说明](#5-验证脚本说明)
- [6. 变更日志](#6-变更日志)

---

## 1. 状态图例与维护规则

### 1.1 状态图例

| 标记 | 含义 | 验证期望 |
|------|------|----------|
| ✅ **已实现** | 代码已存在，文档与代码一致 | 验证必须通过 |
| ⚠️ **部分实现** | 核心 API 存在，但辅助功能缺失 | 验证核心部分必须通过 |
| 🚧 **Phase 1 提案** | 仅设计意图，代码未实现 | 验证预期**失败**（缺失 = 符合预期） |
| ❌ **已弃用** | 决策被新方案替代 | 验证预期**失败**（不应当存在） |
| 🔄 **待评估** | 决策悬而未决 | 无强制验证 |

### 1.2 维护规则

1. **每次修改 CppTLM/CppHDL/ChipForge 后**，必须运行 `tools/verify_adr.sh`
2. **所有 ✅ 决策的验证必须通过**；任何失败即为"代码漂移"
3. **🚧 决策的实现状态变化**时，需同时更新本文档与对应源文档
4. **新增架构决策**时，分配新 ID 并填入对应分类
5. **本文档与源文档的优先级**：以源文档（`code-framework-mapping.md` / `declarative-hybrid-framework.md`）为准；本文档为衍生跟踪表
6. **每月审计**：完整运行一次验证脚本，确认所有 ✅ 状态决策仍可验证

---

## 2. 决策主表（快速总览）

> **阅读方式**：从左到右依次为 ID、标题、类别、当前状态、关键验证路径。**最终列**为脚本中该 ADR 的标识符。

### 2.1 已实现决策（✅）— 23 条

| ID | 标题 | 类别 | 验证路径 |
|----|------|------|----------|
| ADR-001 | 三层框架分工 | 架构 | `CppTLM/` + `CppHDL/` + `ip/` 目录均存在 |
| ADR-002 | CppTLM 事件驱动调度 | 架构 | `core/event_queue.hh` |
| ADR-003 | CppHDL LogicNode DAG + 多后端 | 架构 | `core/lnode.h` + `codegen_verilog.h` + `simulator.h` |
| ADR-004 | SimObject 类层次 | TLM | `core/{sim_object,sim_module,tlm_module,chstream_module}.hh` |
| ADR-005 | StreamAdapter 类型擦除 | TLM | `core/stream_adapter_base.hh` + `framework/stream_adapter.hh` |
| ADR-006 | ChStreamAdapterFactory | TLM | `framework/chstream_adapter_factory.hh` |
| ADR-008 | HybridCacheWrapper TLM↔RTL | TLM | `rtl/hybrid_cache_wrapper.hh` |
| ADR-009 | ch_uint<N> 硬件整数 | HDL | `core/uint.h` |
| ADR-010 | ch_reg<T> 时序寄存器 | HDL | `core/reg.h` |
| ADR-011 | ch_mem<T,D> SRAM | HDL | `core/mem.h` |
| ADR-012 | Component::describe() | HDL | `component.h` |
| ADR-013 | Component::create_ports() | HDL | `component.h` |
| ADR-014 | lnode 节点构建 | HDL | `core/{lnode,lnodeimpl,node_builder}.h` |
| ADR-015 | ModuleFactory JSON 装配 | 注册 | `core/module_factory.hh` |
| ADR-016 | REGISTER_CHSTREAM 批量注册 | 注册 | `chstream_register.hh` |
| ADR-017 | PluginLoader dlopen | 注册 | `core/plugin_loader.hh` |
| ADR-018 | __io 端口宏 | 端口 | `core/io.h:318` |
| ADR-019 | __in / __out 端口宏 | 端口 | `core/io.h:337-338` |
| ADR-020 | ch_logic_in/out 旧 API | 端口 | `core/io.h` |
| ADR-021 | bundle_base<Self> CRTP | Bundle | `core/bundle/bundle_base.h` |
| ADR-022 | CH_BUNDLE_FIELDS_T 宏族 | Bundle | `core/bundle/bundle_meta.h:23-32` |
| ADR-023 | ch_stream<T> 协议 | Bundle | `bundle/stream_bundle.h:19` |
| ADR-038 | chstream_register 集中入口 | 目录 | `chstream_register.hh` |

### 2.2 部分实现决策（⚠️）— 1 条

| ID | 标题 | 类别 | 核心已实现 | 缺失部分 |
|----|------|------|------------|----------|
| ADR-024 | Bundle 三层分层 | Bundle | Bundle + Protocol | Mapper 模板未实现 |

### 2.3 Phase 1 提案决策（🚧）— 15 条

| ID | 标题 | 类别 | 状态 |
|----|------|------|------|
| ADR-025 | Plugin 基类无 tick | Plugin | 🚧 |
| ADR-026 | at_stage() 逻辑阶段名 | Plugin | 🚧 |
| ADR-027 | Phase 子阶段顺序 | Plugin | 🚧 |
| ADR-028 | declare_substage() | Plugin | 🚧 |
| ADR-029 | 模块级 ImplMode | Plugin | 🚧 |
| ADR-030 | PipeNode 三态握手 | Pipe | 🚧 |
| ADR-031 | StageLink / CtrlLink / DirectLink | Pipe | 🚧 |
| ADR-032 | PipeBuilder 统一编译器 | Pipe | 🚧 |
| ADR-033 | CtrlLink 四种控制 API | Pipe | 🚧 |
| ADR-034 | ScoreBoard 三种变体 | 验证 | 🚧 |
| ADR-035 | CompareDriver TLM↔RTL 驱动 | 验证 | 🚧 |
| ADR-036 | 三级测试金字塔 | 验证 | 🚧 |
| ADR-007 | StreamAdapter 跨 TLM↔RTL 通用桥接 | TLM | 🚧（仅 `HybridCacheWrapper` 局部） |
| ADR-039 | 统一目录结构 | 目录 | 🚧（当前仍 tlm/rtl 分离） |

### 2.4 统计

| 类别 | ✅ | ⚠️ | 🚧 | 合计 |
|------|----|----|-----|------|
| 框架架构 (A) | 3 | 0 | 0 | 3 |
| TLM 模块/接口 (B) | 4 | 0 | 1 | 5 |
| HDL 模块/接口 (C) | 6 | 0 | 0 | 6 |
| 注册与发现 (D) | 3 | 0 | 0 | 3 |
| 端口与信号 (E) | 3 | 0 | 0 | 3 |
| Bundle 与协议 (F) | 3 | 1 | 0 | 4 |
| 声明式 Plugin (G) | 0 | 0 | 5 | 5 |
| 流水线抽象 (H) | 0 | 0 | 4 | 4 |
| 验证框架 (I) | 0 | 0 | 3 | 3 |
| 目录与组织 (J) | 1 | 0 | 1 | 2 |
| **合计** | **23** | **1** | **14** | **38** |

**实现率**：24/38 = **63%**（含部分实现）

---

## 3. 详细决策记录

### A. 框架架构（3 条）

---

#### ADR-001：三层框架分工（ChipForge / CppHDL / CppTLM）

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `declarative-hybrid-framework.md` §1.2 |
| 决策 | ChipForge 仓库通过 `CppTLM/` + `CppHDL/` 符号链接组织代码；应用层 IP 位于 `ip/<name>/`；SoC 顶层位于 `soc/` |
| 理由 | 三层分工明确各层职责：ChipForge = 应用、CppHDL = 硬件 IR、CppTLM = 仿真内核 |
| 后果 | ✅ 职责清晰；✅ 可独立升级 CppTLM/CppHDL 子模块；⚠️ 需要符号链接或子模块管理 |

**验证命令**：
```bash
# 验证三个层级目录均存在
test -d /workspace/project/ChipForge/CppTLM && \
test -d /workspace/project/ChipForge/CppHDL && \
test -d /workspace/project/ChipForge/ip
```

**代码锚点**：`/workspace/project/ChipForge/CppTLM` (symlink), `CppHDL` (symlink), `ip/`, `soc/`, `bundles/`

---

#### ADR-002：CppTLM 事件驱动 + 优先队列调度

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `declarative-hybrid-framework.md` §2.2 |
| 决策 | `EventQueue` 是 CppTLM 核心调度器，基于优先队列调度事件 |
| 理由 | 周期精确仿真需要确定性、可回放的调度；优先队列 + tick 入口是最简方案 |
| 后果 | ✅ 调度顺序确定；✅ 易调试；⚠️ 与 Verilog 时序严格同步需注意边界 |

**验证命令**：
```bash
# 验证 EventQueue 类定义
grep -q "class EventQueue" /workspace/project/CppTLM/include/core/event_queue.hh
# 验证 createModule 与 run 方法
grep -qE "createModule|run\(" /workspace/project/CppTLM/include/core/event_queue.hh
```

**代码锚点**：`CppTLM/include/core/event_queue.hh`

---

#### ADR-003：CppHDL LogicNode DAG + 多后端

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `declarative-hybrid-framework.md` §3.4 |
| 决策 | `describe()` 中的每条赋值被记录为 `LogicNode`，构成 DAG；后端遍历 DAG 产生 Verilog 或 JIT 仿真 |
| 理由 | 单一 IR 支持多种后端，避免为每种目标维护独立代码生成 |
| 后果 | ✅ Verilog 与 JIT 仿真行为一致；✅ 易于新增后端（如 SMT / FIRRTL）；⚠️ DAG 序列化需保证确定性 |

**验证命令**：
```bash
# 验证三个组件：lnode 定义、Verilog 后端、JIT 仿真器
test -e /workspace/project/CppHDL/include/core/lnode.h && \
test -e /workspace/project/CppHDL/include/codegen_verilog.h && \
test -e /workspace/project/CppHDL/include/simulator.h
```

**代码锚点**：`CppHDL/include/core/lnode.h`, `core/lnodeimpl.h`, `core/node_builder.h`, `codegen_verilog.h`, `simulator.h`

---

### B. CppTLM 模块/接口（5 条）

---

#### ADR-004：SimObject 类层次

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `declarative-hybrid-framework.md` §2.1 |
| 决策 | 类层次：**`SimModule` / `TLMModule` / `ChStreamModuleBase` 三个类均直接继承 `SimObject`**，不构成线性链 |
| 理由 | 分层抽象允许基础类（`SimObject`）单独使用；`TLMModule` 提供 fragment 重组钩子；`ChStreamModuleBase` 按需启用 ch_stream 通信 |
| 后果 | ✅ 灵活复用；✅ ChStreamModuleBase 自动获得 ch_stream 识别能力；⚠️ 类命名（`TLMModule` 全大写）与典型驼峰约定不同，需注意 |

**实际继承关系**（2026-06-07 验证）：
```
SimObject
  ├── SimModule
  ├── TLMModule        ← 全大写，直接继承 SimObject（不经 SimModule）
  └── ChStreamModuleBase
```

**验证命令**：
```bash
grep -q "class SimObject" /workspace/project/CppTLM/include/core/sim_object.hh && \
grep -q "class SimModule.*: public SimObject" /workspace/project/CppTLM/include/core/sim_module.hh && \
grep -q "class TLMModule.*: public SimObject" /workspace/project/CppTLM/include/core/tlm_module.hh && \
grep -q "class ChStreamModuleBase.*: public SimObject" /workspace/project/CppTLM/include/core/chstream_module.hh
```

**代码锚点**：`CppTLM/include/core/{sim_object,sim_module,tlm_module,chstream_module}.hh`

> **修正记录**：原 `declarative-hybrid-framework.md` §2.1 写为线性链 `SimObject → SimModule → TlmModule → ChStreamModuleBase`，**不准确**。实际三个派生类直接继承 `SimObject`，无传递关系。

---

#### ADR-005：StreamAdapter 类型擦除 + 协议转换

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `declarative-hybrid-framework.md` §2.4 + `code-framework-mapping.md` §2.2 |
| 决策 | `StreamAdapter` 将 `ch_stream<Bundle>` 转为通用 `Packet` 序列；valid/ready/cancel 三态握手 → Port 级 send/recv |
| 理由 | 解耦 IP 内部接口（ch_stream）与框架外部接口（Packet Port），允许独立演进 |
| 后果 | ✅ 内部接口可优化（POD）而不影响外部；✅ 跨模块通过 Packet 透明传输；⚠️ 类型擦除需序列化所有 Bundle |

**验证命令**：
```bash
# 验证基类（注意：基类已从 framework/ 迁到 core/）
test -e /workspace/project/CppTLM/include/core/stream_adapter_base.hh
grep -q "class StreamAdapterBase" /workspace/project/CppTLM/include/core/stream_adapter_base.hh

# 验证具体适配器类
grep -qE "class (InputStreamAdapter|OutputStreamAdapter|StreamAdapter)\b" \
  /workspace/project/CppTLM/include/framework/stream_adapter.hh

# 验证多端口、双端口、双向适配器
for adapter in multi_port_stream_adapter dual_port_stream_adapter bidirectional_port_adapter; do
  test -e /workspace/project/CppTLM/include/framework/${adapter}.hh
done
```

**代码锚点**：
- 基类：`CppTLM/include/core/stream_adapter_base.hh` ⚠️ **注意路径已迁出 framework/**
- 适配器：`CppTLM/include/framework/stream_adapter.hh` 等

---

#### ADR-006：ChStreamAdapterFactory 类型注册中心

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `code-framework-mapping.md` §2.2（新增于 2026 评审）|
| 决策 | `ChStreamAdapterFactory` 是单例工厂，支持 `registerFactory` / `registerAdapter` / `registerMultiPortAdapter` / `registerDualPortAdapter` / `registerBidirectionalPortAdapter` / `create` / `knows` |
| 理由 | 通过统一注册中心实现 ch_stream → Packet 的批量配置，避免每模块手写适配器 |
| 后果 | ✅ JSON 配置驱动装配；✅ ModuleFactory 注入时按类型查找；⚠️ 类型注册与适配器注册需保持同步 |

**验证命令**：
```bash
FACTORY=/workspace/project/CppTLM/include/framework/chstream_adapter_factory.hh
test -e "$FACTORY" && \
grep -q "class ChStreamAdapterFactory" "$FACTORY" && \
for method in registerFactory registerAdapter registerMultiPortAdapter registerDualPortAdapter registerBidirectionalPortAdapter create knows; do
  grep -qE "(\\b${method}\\b|::${method}\\b)" "$FACTORY" || { echo "MISSING: $method"; exit 1; }
done
```

**代码锚点**：`CppTLM/include/framework/chstream_adapter_factory.hh`

---

#### ADR-007：StreamAdapter 跨 TLM↔RTL 通用桥接

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 1 提案（仅 `HybridCacheWrapper` 提供 Cache 局部桥接） |
| 来源 | `declarative-hybrid-framework.md` §6.5 |
| 决策 | 设计意图：通用 TLM↔RTL 桥接由 StreamAdapter 内部多态分支处理 |
| 理由 | 框架层抽象，避免每个 IP 单独实现桥接 |
| 后果 | 🚧 未实现；当前仅 `HybridCacheWrapper`（`rtl/hybrid_cache_wrapper.hh`）支持 CacheTLM + Cache RTL 协同 |

**验证命令**（当前预期失败）：
```bash
# 当前不存在通用 TLM↔RTL 桥接（仅 HybridCacheWrapper）
ls /workspace/project/CppTLM/include/rtl/  # 应仅含 hybrid_cache_wrapper.hh
# 不应有"通用桥接"文件
[[ ! -e /workspace/project/CppTLM/include/framework/tlm_rtl_bridge.hh ]] || {
  echo "DRIFT: 通用 TLM↔RTL 桥接已实现但 ADR-007 仍标 🚧"
  exit 1
}
```

**代码锚点（当前）**：`CppTLM/include/rtl/hybrid_cache_wrapper.hh`

---

#### ADR-008：HybridCacheWrapper TLM↔RTL 协同仿真

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `chstream_register.hh:65` |
| 决策 | `cpptlm::rtl::HybridCacheWrapper` 包装 CacheTLM + Cache RTL，TLM 与 RTL 并行运行 |
| 理由 | 提供 COMPARE 模式的参考实现，验证 TLM↔RTL 桥接可行性 |
| 后果 | ✅ Cache 模块可作 COMPARE 模式测试用例；⚠️ 不通用，仅 Cache |

**验证命令**：
```bash
test -e /workspace/project/CppTLM/include/rtl/hybrid_cache_wrapper.hh && \
grep -q "HybridCacheWrapper" /workspace/project/CppTLM/include/chstream_register.hh
```

**代码锚点**：`CppTLM/include/rtl/hybrid_cache_wrapper.hh`, `chstream_register.hh`

---

### C. CppHDL 模块/接口（5 条）

---

#### ADR-009：ch_uint<N> 模板综合期常量

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `declarative-hybrid-framework.md` §3.1, `code-framework-mapping.md` §3.2 |
| 决策 | `ch_uint<N>` 是 N 位无符号整数，模板参数为综合期常量 |
| 理由 | C++ 模板参数综合期求值，允许 Verilog 生成时静态确定位宽 |
| 后果 | ✅ 位宽在编译期确定；✅ Verilog 生成零开销；⚠️ 不支持运行时位宽（设计选择） |

**验证命令**：
```bash
grep -qE "class ch_uint|using ch_uint|template.*ch_uint" /workspace/project/CppHDL/include/core/uint.h
# 验证位宽实现
grep -qE "static constexpr.*N|WIDTH" /workspace/project/CppHDL/include/core/uint.h
```

**代码锚点**：`CppHDL/include/core/uint.h`

---

#### ADR-010：ch_reg<T> 时序寄存器

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `declarative-hybrid-framework.md` §3.1, `code-framework-mapping.md` §3.2 |
| 决策 | `ch_reg<T>` 每周期更新一次 |
| 理由 | 显式时序元素，避免隐式 latch 推断 |
| 后果 | ✅ 综合就绪；✅ 行为可预测；⚠️ 复位语义需额外建模 |

**验证命令**：
```bash
grep -qE "class ch_reg" /workspace/project/CppHDL/include/core/reg.h
```

**代码锚点**：`CppHDL/include/core/reg.h`

---

#### ADR-011：ch_mem<T,D> SRAM 推断

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `declarative-hybrid-framework.md` §3.1, `code-framework-mapping.md` §3.2 |
| 决策 | `ch_mem<T, Depth>` 是同步存储器（SRAM 推断） |
| 理由 | 与 Verilog `reg [W-1:0] [D-1:0]` 一一对应 |
| 后果 | ✅ 综合器友好；✅ 行为确定；⚠️ 不支持异步 RAM（设计选择） |

**验证命令**：
```bash
grep -qE "class ch_mem" /workspace/project/CppHDL/include/core/mem.h
# 验证双模板参数（数据类型 + 深度）
grep -qE "template.*typename T.*size_t|template.*typename T.*unsigned" /workspace/project/CppHDL/include/core/mem.h
```

**代码锚点**：`CppHDL/include/core/mem.h`

---

#### ADR-012：Component::describe() 纯虚入口

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `declarative-hybrid-framework.md` §3.3 |
| 决策 | `Component::describe()` 是纯虚方法，是硬件逻辑描述的唯一入口 |
| 理由 | 强制用户实现 `describe()`，避免空组件；与 SpinalHDL 风格一致 |
| 后果 | ✅ 编译期保证；✅ 行为可分析；⚠️ 端口必须在 `create_ports()` 中先声明 |

**验证命令**：
```bash
grep -qE "virtual void describe\(\) = 0" /workspace/project/CppHDL/include/component.h
# 验证 create_ports 是可选的
grep -qE "virtual void create_ports" /workspace/project/CppHDL/include/component.h
```

**代码锚点**：`CppHDL/include/component.h:30`

---

#### ADR-013：Component::create_ports() 端口声明入口

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `declarative-hybrid-framework.md` §3.3 |
| 决策 | `Component::create_ports()` 是虚方法（默认空），端口通过 `__io(...)` 宏在 `create_ports()` 中声明 |
| 理由 | 分离端口声明（`create_ports`）与逻辑描述（`describe`），便于 IR 后端区分处理 |
| 后果 | ✅ 端口可在 describe 前完成；✅ IR 工具可独立分析端口；⚠️ 用户需记得重写 `create_ports()` |

**验证命令**：
```bash
grep -qE "virtual void create_ports" /workspace/project/CppHDL/include/component.h
# 验证 __io 宏
grep -qE "^#define __io\(" /workspace/project/CppHDL/include/core/io.h
```

**代码锚点**：`CppHDL/include/component.h`, `core/io.h:318`

---

#### ADR-014：lnode 节点构建（位于 `core/`）

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `code-framework-mapping.md` §3.2（评审修订）|
| 决策 | `lnode` / `lnodeimpl` / `node_builder` 位于 `core/`，模板实现 `*.tpp` 位于顶层 `lnode/` |
| 理由 | 解耦运行时多态（`lnodeimpl`）与综合期类型（`lnode`）；避免直接操作 lnode（AGENTS.md 反模式） |
| 后果 | ✅ `node_builder` 单例工厂统一构造；✅ 模板实现分离；⚠️ 跨目录包含需注意 include 顺序 |

**验证命令**：
```bash
# 验证三个核心文件
for f in lnode.h lnodeimpl.h node_builder.h; do
  test -e /workspace/project/CppHDL/include/core/$f
done
# 验证模板实现位置
test -d /workspace/project/CppHDL/include/lnode
ls /workspace/project/CppHDL/include/lnode/*.tpp | wc -l  # 应 ≥ 6
```

**代码锚点**：
- `CppHDL/include/core/{lnode,lnodeimpl,node_builder}.h`
- `CppHDL/include/lnode/*.tpp`

---

### D. 注册与发现（3 条）

---

#### ADR-015：ModuleFactory JSON 拓扑装配

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `declarative-hybrid-framework.md` §2.5 |
| 决策 | `ModuleFactory` 从 JSON 拓扑装配模块，支持 `modules` + `connections` 字段 |
| 理由 | JSON 配置驱动 SoC 组装，零硬编码连线 |
| 后果 | ✅ SoC 配置外部化；✅ 易于做参数扫描；⚠️ JSON Schema 校验需独立实现 |

**验证命令**：
```bash
test -e /workspace/project/CppTLM/include/core/module_factory.hh && \
grep -qE "class ModuleFactory" /workspace/project/CppTLM/include/core/module_factory.hh
# 验证 create 与 registerObject 方法
grep -qE "(create|registerObject)" /workspace/project/CppTLM/include/core/module_factory.hh
```

**代码锚点**：`CppTLM/include/core/module_factory.hh`

---

#### ADR-016：REGISTER_CHSTREAM 批量注册宏

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `chstream_register.hh:29-64`, `declarative-hybrid-framework.md` §8.1（评审修正）|
| 决策 | `REGISTER_CHSTREAM` 是一个宏，一次性注册所有标准模块（CacheTLM / MemoryTLM / CrossbarTLM / CPUTLM 等）|
| 理由 | 集中注册避免散落；`REGISTER_ALL = REGISTER_OBJECT + REGISTER_CHSTREAM` 提供完整注册 |
| 后果 | ✅ 一键启用；✅ 框架升级时注册表自动同步；⚠️ 用户新增 IP 需修改 `chstream_register.hh` |

**验证命令**：
```bash
REG=/workspace/project/CppTLM/include/chstream_register.hh
test -e "$REG" && \
grep -q "^#define REGISTER_CHSTREAM" "$REG" && \
for mod in CacheTLM MemoryTLM CrossbarTLM CPUTLM TrafficGenTLM; do
  grep -q "registerObject<${mod}>" "$REG" || { echo "MISSING: $mod"; exit 1; }
done
# 验证 REGISTER_ALL 组合宏
grep -q "^#define REGISTER_ALL" "$REG"
```

**代码锚点**：`CppTLM/include/chstream_register.hh:29, 66`

**已注册模块清单**（应在 `REGISTER_CHSTREAM` 中存在）：

| 类名 | 注册名 |
|------|--------|
| `cpptlm::CacheTLM` | `"CacheTLM"` |
| `cpptlm::MemoryTLM` | `"MemoryTLM"` |
| `cpptlm::CrossbarTLM` | `"CrossbarTLM"` |
| `cpptlm::CPUTLM` | `"CPUTLM"` |
| `cpptlm::TrafficGenTLM` | `"TrafficGenTLM"` |
| `cpptlm::ArbiterTLM<2>` / `<4>` | `"ArbiterTLM2"` / `"ArbiterTLM4"` |
| `cpptlm::tlm::RouterTLM` | `"RouterTLM"` |
| `cpptlm::tlm::NICTLM` | `"NICTLM"` |
| `cpptlm::tlm::LinkTLM` | `"LinkTLM"` |
| `cpptlm::rtl::HybridCacheWrapper` | `"HybridCacheWrapper"` |

---

#### ADR-017：PluginLoader dlopen 运行时 SO 加载

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `code-framework-mapping.md` §7.1, `declarative-hybrid-framework.md` §4.7 |
| 决策 | `PluginLoader` 是 `dlopen` 包装，用于运行时加载共享库（**与声明式 Plugin 模型无关**） |
| 理由 | 允许 SoC 配置加载第三方 IP 模块 |
| 后果 | ✅ 运行时扩展；⚠️ 与 ADR-025（声明式 Plugin）正交，不可混淆 |

**验证命令**：
```bash
test -e /workspace/project/CppTLM/include/core/plugin_loader.hh && \
grep -qE "class PluginLoader" /workspace/project/CppTLM/include/core/plugin_loader.hh
# 验证 dlopen 调用
grep -rE "dlopen" /workspace/project/CppTLM/src/ 2>/dev/null | head -1
```

**代码锚点**：`CppTLM/include/core/plugin_loader.hh`, `src/utils/dynamic_loader.cc`

---

### E. 端口与信号（3 条）

---

#### ADR-018：__io 宏定义 io_type struct + io() 访问器

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `CppHDL/include/core/io.h:318` |
| 决策 | `__io(...)` 宏定义一个嵌套 `io_type` struct，并生成 `io()` 访问器返回 `io_type&` |
| 理由 | 允许用户在类内以声明式风格声明端口，类型安全 |
| 后果 | ✅ 端口类型在编译期确定；✅ `io()` 访问模式统一；⚠️ 不支持运行时端口 |

**验证命令**：
```bash
grep -A 8 "^#define __io" /workspace/project/CppHDL/include/core/io.h | head -10
# 验证 io() 访问器存在
grep -qE "io_type\s*&\s*io\(\)" /workspace/project/CppHDL/include/core/io.h
```

**代码锚点**：`CppHDL/include/core/io.h:318-325`

---

#### ADR-019：__in / __out 端口声明宏

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `CppHDL/include/core/io.h:337-338` |
| 决策 | `__in(T)` 与 `__out(T)` 是端口声明宏，等价于 `ch::core::ch_in<T>` / `ch::core::ch_out<T>` |
| 理由 | 提供简洁的端口声明语法，替代完整的 `port<T, input_direction>` 写法 |
| 后果 | ✅ 端口声明简洁；⚠️ 必须在 `__io(...)` 块中使用 |

**验证命令**：
```bash
# 验证两个宏定义
grep -qE "^#define __in\(" /workspace/project/CppHDL/include/core/io.h
grep -qE "^#define __out\(" /workspace/project/CppHDL/include/core/io.h
# 验证底层类型
grep -qE "using ch_in.*=.*port<T, input_direction>" /workspace/project/CppHDL/include/core/io.h
grep -qE "using ch_out.*=.*port<T, output_direction>" /workspace/project/CppHDL/include/core/io.h
```

**代码锚点**：`CppHDL/include/core/io.h:300-301, 337-338`

---

#### ADR-020：ch_logic_in/out 旧 API 保留

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `code-framework-mapping.md` §3.2 |
| 决策 | `ch_logic_in<T>` / `ch_logic_out<T>` 旧 API 保留，与新 `ch_in<T>` / `ch_out<T>` 并存 |
| 理由 | 向后兼容旧代码，平滑迁移 |
| 后果 | ✅ 旧代码不破坏；⚠️ API 表面增大，新用户可能困惑 |

**验证命令**：
```bash
grep -qE "class ch_logic_in|using ch_logic_in" /workspace/project/CppHDL/include/core/io.h
grep -qE "class ch_logic_out|using ch_logic_out" /workspace/project/CppHDL/include/core/io.h
```

**代码锚点**：`CppHDL/include/core/io.h`

---

### F. Bundle 与协议（4 条）

---

#### ADR-021：bundle_base<Self> CRTP 模式

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `declarative-hybrid-framework.md` §3.2, `code-framework-mapping.md` §3.3.1 |
| 决策 | `bundle_base<Self>` 使用 CRTP 模式，自动生成序列化/反序列化、宽度计算等元函数 |
| 理由 | 编译期派生类类型信息，零运行时开销 |
| 后果 | ✅ 派生 Bundle 自动获得元函数；✅ 综合期宽度计算；⚠️ 模板错误信息可能晦涩 |

**验证命令**：
```bash
grep -qE "template.*typename Derived.*class bundle_base" /workspace/project/CppHDL/include/core/bundle/bundle_base.h
# 验证继承逻辑_buffer
grep -qE "bundle_base.*: public logic_buffer" /workspace/project/CppHDL/include/core/bundle/bundle_base.h
```

**代码锚点**：`CppHDL/include/core/bundle/bundle_base.h:24`

---

#### ADR-022：CH_BUNDLE_FIELDS_T 宏族（arity 变体 _1 到 _5+）

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `declarative-hybrid-framework.md` §3.2, `CppHDL/include/core/bundle/bundle_meta.h:23-32` |
| 决策 | `CH_BUNDLE_FIELDS_T` 是一组宏（`_1` / `_2` / `_3` / `_4` / `_5` / `,...` 变体），按字段数选择重载 |
| 理由 | 宏递归展开，避免可变参模板的复杂错误信息 |
| 后果 | ✅ 字段数 1-20 都有对应重载；⚠️ 字段超过最大重载数需扩展宏族 |

**验证命令**：
```bash
META=/workspace/project/CppHDL/include/core/bundle/bundle_meta.h
for i in 1 2 3 4 5; do
  grep -qE "^#define CH_BUNDLE_FIELDS_T_${i}\b" "$META" || {
    echo "MISSING: CH_BUNDLE_FIELDS_T_${i}"; exit 1
  }
done
# 验证展开宏
grep -qE "^#define CH_BUNDLE_FIELDS_T\(" "$META"
```

**代码锚点**：`CppHDL/include/core/bundle/bundle_meta.h:23-32`

---

#### ADR-023：ch_stream<T> 协议层

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `CppHDL/include/bundle/stream_bundle.h:19` |
| 决策 | `ch_stream<T>` 是流式数据契约（valid/ready/cancel 三态握手），`Stream<T>` 是 `ch_stream<T>` 的别名 |
| 理由 | 与硬件流控协议一一对应；`Stream<T>` 别名符合 SpinalHDL 习惯 |
| 后果 | ✅ 综合期接口；⚠️ 仅流式，不支持突发（Fragment 提案未实现） |

**验证命令**：
```bash
test -e /workspace/project/CppHDL/include/bundle/stream_bundle.h && \
grep -qE "struct ch_stream.*: public bundle_base<ch_stream" /workspace/project/CppHDL/include/bundle/stream_bundle.h
grep -qE "using Stream\s*=\s*ch_stream" /workspace/project/CppHDL/include/bundle/stream_bundle.h
# 验证 forward declaration 在 flow_bundle.h
grep -qE "struct ch_stream" /workspace/project/CppHDL/include/bundle/flow_bundle.h
```

**代码锚点**：`CppHDL/include/bundle/stream_bundle.h:19, 67`

---

#### ADR-024：Bundle 三层分层（设计：POD + ch_uint + Mapper）

| 字段 | 值 |
|------|-----|
| 状态 | ⚠️ 部分实现（Bundle + Protocol 已实现，Mapper 模板未实现） |
| 来源 | `declarative-hybrid-framework.md` §5.1 + ADR-5 |
| 决策 | 设计意图：内部 POD Bundle（高速）+ 外部 ch_uint Bundle（综合就绪）+ Mapper（互转） |
| 理由 | 内层 POD 获得 TLM 速度；外层 ch_uint 获得 RTL 综合就绪；Mapper 单点维护转换 |
| 后果 | ⚠️ 框架已支持 POD 与 ch_uint 两种写法；🚧 `BundleMapper<InternalPOD, ExternalChUint>` 模板未实现 |

**验证命令**（核心部分必须通过）：
```bash
# 验证 Bundle 基础 + ch_stream（核心已实现）
test -e /workspace/project/CppHDL/include/core/bundle/bundle_base.h && \
test -e /workspace/project/CppHDL/include/bundle/stream_bundle.h

# 验证 BundleMapper 模板（预期不存在 = 部分实现）
[[ ! -e /workspace/project/CppHDL/include/core/bundle/bundle_mapper.h ]] || {
  echo "DRIFT: BundleMapper 已实现但 ADR-024 仍标 ⚠️"
  exit 1
}
```

**代码锚点**：
- 已实现：`core/bundle/bundle_base.h`, `bundle/stream_bundle.h`
- 未实现：`core/bundle/bundle_mapper.h`（预期缺失）

---

### G. 声明式 Plugin 模型（Phase 1）— 5 条

---

#### ADR-025：Plugin 基类无 tick

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 0 P0 #1 |
| 来源 | `declarative-hybrid-framework.md` §4.1 + ADR-1 |
| 决策 | `Plugin` 基类只有 `setup(PipeBuilder&)` 与 `build(PipeBuilder&)`，**没有** `tick()` |
| 理由 | 调度由框架确定性决定，Plugin 不持有时序状态 |
| 后果 | 🚧 未实现；当前 `PluginLoader`（ADR-017）是无关的 dlopen 加载器 |

**验证命令**（预期失败 = 符合预期）：
```bash
# Plugin 基类应不存在
[[ ! -e /workspace/project/ChipForge/core/plugin.h ]] && \
[[ ! -e /workspace/project/CppTLM/include/core/plugin.h ]] && \
[[ ! -e /workspace/project/CppHDL/include/core/plugin.h ]]
# 任何位置都不应有 class Plugin
! grep -rqE "^class Plugin\s" /workspace/project/CppTLM/include /workspace/project/CppHDL/include /workspace/project/ChipForge 2>/dev/null | grep -v "PluginLoader" | head -1
```

**代码锚点（预期缺失）**：`core/plugin.h`（在 ChipForge / CppTLM / CppHDL 中均不应存在）

---

#### ADR-026：at_stage() 逻辑阶段名绑定

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 0 P0 #4 |
| 来源 | `declarative-hybrid-framework.md` §4.3 + ADR-2 |
| 决策 | `PipeBuilder::at_stage(stage, phase, lambda)` 将 Plugin 绑定到逻辑阶段名（`"lookup"` 等）|
| 理由 | Plugin 实现与物理流水线深度解耦，配置变更无需修改 Plugin 代码 |
| 后果 | 🚧 未实现；当前无法做"逻辑名 → 物理 Node"映射 |

**验证命令**（预期失败）：
```bash
# at_stage 方法应不存在
! grep -rqE "at_stage\s*\(" /workspace/project/CppTLM/include /workspace/project/CppHDL/include /workspace/project/ChipForge 2>/dev/null | head -1
```

**代码锚点（预期缺失）**：`PipeBuilder::at_stage`

---

#### ADR-027：Phase 子阶段 EARLY/NORMAL/LATE

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 0 P0 #4 |
| 来源 | `declarative-hybrid-framework.md` §4.3 + ADR-3 |
| 决策 | `enum class Phase { EARLY, NORMAL, LATE }` 控制同逻辑阶段内多 Plugin 的执行顺序 |
| 理由 | 依赖检测（EARLY）→ 主体逻辑（NORMAL）→ 写回准备（LATE），三档足够典型需求 |
| 后果 | 🚧 未实现；当前同阶段 Plugin 调度顺序未定义 |

**验证命令**（预期失败）：
```bash
! grep -rqE "enum class Phase\s*\{[^}]*EARLY" /workspace/project/CppTLM/include /workspace/project/CppHDL/include /workspace/project/ChipForge 2>/dev/null | head -1
```

**代码锚点（预期缺失）**：`enum class Phase`

---

#### ADR-028：declare_substage() 动态扩展子流水线

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 0 P0 #4 (最小) + Phase 6 (完整) |
| 来源 | `declarative-hybrid-framework.md` §4.4 |
| 决策 | `PipeBuilder::declare_substage(parent, sub, depth)` 在流水线中追加子 Node |
| 理由 | 允许 Plugin 在 `setup()` 阶段动态扩展物理流水线深度 |
| 后果 | 🚧 未实现；当前流水线深度完全由 JSON 静态决定 |

**验证命令**（预期失败）：
```bash
! grep -rqE "declare_substage\s*\(" /workspace/project/CppTLM/include /workspace/project/CppHDL/include /workspace/project/ChipForge 2>/dev/null | head -1
```

**代码锚点（预期缺失）**：`PipeBuilder::declare_substage`

---

#### ADR-029：模块级 ImplMode（TLM_ONLY / RTL_ONLY / COMPARE / SHADOW）

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 6 |
| 来源 | `declarative-hybrid-framework.md` §6 + ADR-7 |
| 决策 | 每个 IP 实例可在 JSON 中独立指定 `impl_mode_override`，框架按模块创建对应实现并自动桥接 |
| 理由 | 焦点调试 / 渐进 RTL 化 / 回归矩阵的工程需求 |
| 后果 | 🚧 未实现；当前 SoC JSON 的 `impl_mode` 字段是悬挂引用（无人解析） |

**验证命令**（预期失败）：
```bash
# ImplMode 枚举应不存在
! grep -rqE "enum class ImplMode" /workspace/project/CppTLM/include /workspace/project/CppHDL/include /workspace/project/ChipForge 2>/dev/null | head -1
# ModuleFactory 不应解析 impl_mode 字段
! grep -qE "impl_mode_override" /workspace/project/CppTLM/include/core/module_factory.hh
# SoC JSON 中的 impl_mode 字段无人消费
! grep -rqE "impl_mode" /workspace/project/CppTLM/src 2>/dev/null | head -1
```

**代码锚点（预期缺失）**：`enum class ImplMode`, JSON 字段 `impl_mode_override`

---

### H. 流水线抽象（Phase 1）— 4 条

---

#### ADR-030：PipeNode 三态握手（valid/ready/cancel）

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 0 P0 #3 |
| 来源 | `declarative-hybrid-framework.md` §7.1 |
| 决策 | `PipeNode` 提供 `is_firing()` / `is_moving()` / `is_blocked()` / `is_canceling()` 状态查询 |
| 理由 | 三态握手（valid/ready/cancel）支持流水线正常、阻塞、取消三种场景 |
| 后果 | 🚧 未实现；当前 `ch_stream` 已支持三态握手，但无显式 `PipeNode` 包装 |

**验证命令**（预期失败）：
```bash
! grep -rqE "class PipeNode" /workspace/project/CppTLM/include /workspace/project/CppHDL/include /workspace/project/ChipForge 2>/dev/null | head -1
! grep -rqE "is_firing\(\)|is_canceling\(\)" /workspace/project/CppTLM/include /workspace/project/CppHDL/include /workspace/project/ChipForge 2>/dev/null | head -1
```

**代码锚点（预期缺失）**：`class PipeNode`

---

#### ADR-031：StageLink / CtrlLink / DirectLink 三种类型

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 0 P0 #5 |
| 来源 | `declarative-hybrid-framework.md` §7.2 |
| 决策 | `StageLink`（标准流水级）/ `CtrlLink`（带控制 API）/ `DirectLink`（组合直连）|
| 理由 | 不同流水线场景需要不同的 Link 抽象 |
| 后果 | 🚧 未实现；当前无 `PipeLink` 抽象 |

**验证命令**（预期失败）：
```bash
! grep -rqE "class (StageLink|CtrlLink|DirectLink)" /workspace/project/CppTLM/include /workspace/project/CppHDL/include /workspace/project/ChipForge 2>/dev/null | head -1
```

**代码锚点（预期缺失）**：`class StageLink / CtrlLink / DirectLink`

---

#### ADR-032：PipeBuilder 统一编译器

| 字段 | 值 |
|------|------|
| 状态 | 🚧 Phase 0 P0 #4 |
| 来源 | `declarative-hybrid-framework.md` §7.3 |
| 决策 | `PipeBuilder` 统一编译入口：create_node / at_stage / declare_substage / build / tick |
| 理由 | 单一入口，简化用户心智 |
| 后果 | 🚧 未实现；当前无 `PipeBuilder` 类 |

**验证命令**（预期失败）：
```bash
! grep -rqE "class PipeBuilder" /workspace/project/CppTLM/include /workspace/project/CppHDL/include /workspace/project/ChipForge 2>/dev/null | head -1
```

**代码锚点（预期缺失）**：`class PipeBuilder`

---

#### ADR-033：CtrlLink 四种控制 API（halt/flush/throw/bypass）

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 0 P0 #5 |
| 来源 | `declarative-hybrid-framework.md` §4.5 |
| 决策 | `CtrlLink::halt_when` / `flush_when` / `throw_when` / `bypass` 四种对象方法 |
| 理由 | 多 Plugin 独立声明同一条件，框架做 OR 合并 |
| 后果 | 🚧 CtrlLink 对象方法未实现；⚠️ CppHDL chlib 已有 `stream_halt_when` / `stream_throw_when` 自由函数（位于 `chlib/stream.h:58,92`），命名与 Plugin 提案有冲突风险 |

**重要发现**（2026-06-07 验证）：CppHDL chlib 提供的 `stream_halt_when` / `stream_throw_when` 是**自由函数**（非对象方法），位于 `chlib/stream.h`。这与 Plugin 模型的 `CtrlLink::halt_when` 对象方法设计**功能重叠但形式不同**。Phase 1 实施时需明确两者关系：
- 选项 A：CtrlLink 复用 chlib 自由函数
- 选项 B：CtrlLink 改名为 `link_halt_when` 以避免命名冲突
- 选项 C：将 chlib 自由函数迁移到 chstream 流（保留 chlib 现有 API）

**验证命令**（预期失败 — CtrlLink 方法不存在）：
```bash
# CtrlLink 对象方法（.halt_when / ->halt_when / ::halt_when）不存在
! grep -rqE "(\.|->|::)\s*(halt_when|flush_when|throw_when)\s*\(" \
  /workspace/project/CppTLM/include /workspace/project/ChipForge \
  --include="*.h" --include="*.hh" --include="*.hpp" 2>/dev/null | head -1
# 注：故意排除 chlib 自由函数 stream_*_when
```

**代码锚点（现有相关实现）**：
- `CppHDL/include/chlib/stream.h:58` — `stream_throw_when(input_stream, condition)`
- `CppHDL/include/chlib/stream.h:92` — `stream_halt_when(input_stream, halt)`
- `CppHDL/include/chlib/stream_builder.h:70,113` — 流畅 API 调用 `stream_halt_when` / `stream_throw_when`

---

### I. 验证框架（Phase 1）— 3 条

---

#### ADR-034：ScoreBoard 三种变体

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 6 |
| 来源 | `declarative-hybrid-framework.md` §9.5 |
| 决策 | `TransactionScoreBoard` / `CycleScoreBoard` / `TimingScoreBoard` |
| 理由 | 事务级、周期级、时序级三种对比粒度 |
| 后果 | 🚧 未实现；当前无 ScoreBoard 抽象 |

**验证命令**（预期失败）：
```bash
! grep -rqE "class (TransactionScoreBoard|CycleScoreBoard|TimingScoreBoard)" /workspace/project/CppTLM/include /workspace/project/CppHDL/include /workspace/project/ChipForge 2>/dev/null | head -1
```

**代码锚点（预期缺失）**：`class TransactionScoreBoard` 等

---

#### ADR-035：CompareDriver TLM↔RTL 协同驱动

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 6 |
| 来源 | `declarative-hybrid-framework.md` §6.6, §9.4 |
| 决策 | `CompareDriver` 同步 TLM 与 RTL 实例，注入相同输入，比较输出 |
| 理由 | COMPARE 模式基础设施 |
| 后果 | 🚧 未实现；当前无 CompareDriver |

**验证命令**（预期失败）：
```bash
! grep -rqE "class CompareDriver" /workspace/project/CppTLM/include /workspace/project/CppHDL/include /workspace/project/ChipForge 2>/dev/null | head -1
```

**代码锚点（预期缺失）**：`class CompareDriver`

---

#### ADR-036：三级测试金字塔

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 1+ (随业务展开) |
| 来源 | `declarative-hybrid-framework.md` §9.1 |
| 决策 | Level A（Plugin 单元）/ Level B（IP 集成）/ Level C（COMPARE 协同）|
| 理由 | 自底向上的验证策略 |
| 后果 | 🚧 未实现；当前 IP 测试通过直接 C++ 单元测试（GoogleTest）完成 |

**验证命令**（预期失败）：
```bash
# 测试金字塔命名约定不存在
! grep -rqE "Level [ABC].*Test|level_a_test" /workspace/project/ChipForge 2>/dev/null | head -1
# ScoreBoard/CompareDriver 不存在
! grep -rqE "class (ScoreBoard|CompareDriver)" /workspace/project/CppTLM/include /workspace/project/CppHDL/include /workspace/project/ChipForge 2>/dev/null | head -1
```

**代码锚点（预期缺失）**：`Level A/B/C` 测试框架

---

### K. 范式决策（1 条）

---

#### ADR-037：Plugin 作为设计范式（不是工具）

**状态**: Accepted (2026-06-08)
**决策者**: User + Prometheus
**背景**: 见 `.omo/drafts/decision-plugin-framework-2026-06-08.md`
**关联**: 重塑路线图（Phase 0/1/6 重新定位）

**决策内容**:
- **D1**: 路线图前插入 Phase 0 = Plugin 最小**脚手架**（2-3 周）
- **D2**: Phase 1 Hello World = L1CachePlugin（真实 Plugin，不是占位）
- **D4**: 业务逻辑强制采用 **Plugin-style** 设计（无 `tick()`、无状态机、Bundle 字段用 `uint_t<N>`）
- **D5**: Phase 6 = 完整 PipeBuilder 框架 + RTL 生成（12-20 周）
- **D6-D9**: 4 项命名冲突解决方案

**影响**:
- 重塑 Phase 1-5 实施路径（业务逻辑必须 Plugin-style）
- 推迟 v2.0.1 §12.2 的 Phase 1a/1b/1c 到 Phase 6
- 路线图新增 Phase 0（在 Phase 1 前）和 Phase 6（在 Phase 5 后）

**影响 ADR**:
- ADR-025~036 状态从 "🚧 未实施" → "Phase 0/6 范围"
- ADR-026 (`at_stage()` 逻辑阶段名) → Phase 0 P0 #4
- ADR-027 (`Phase {EARLY,NORMAL,LATE}`) → Phase 0 P0 #4
- ADR-028 (`declare_substage()`) → Phase 0 P0 #4 (最小实现) + Phase 6 (完整)
- ADR-029 (模块级 `ImplMode`) → Phase 6
- ADR-030 (`PipeNode` 三态握手) → Phase 0 P0 #3
- ADR-031 (`StageLink/CtrlLink/DirectLink`) → Phase 0 P0 #5
- ADR-032 (`PipeBuilder` 统一编译器) → Phase 0 P0 #4
- ADR-033 (`CtrlLink` 四种控制 API) → Phase 0 P0 #5
- ADR-034 (`ScoreBoard`) → Phase 6
- ADR-035 (`CompareDriver`) → Phase 6
- ADR-036 (三级测试金字塔) → Phase 1+ (随业务展开)

**不可逆性**: D4（Plugin-style 强制）不可逆 —— 一旦 Phase 1 业务逻辑用 Plugin-style，事后改回 tick() 几乎全重写

**重新审视指引**: 见决策记录 §8（每季度或在 Phase 0/1/6 关键节点）

---

### J. 目录与组织（2 条）

---

#### ADR-038：chstream_register.hh 集中注册入口

| 字段 | 值 |
|------|-----|
| 状态 | ✅ 已实现 |
| 来源 | `CppTLM/include/chstream_register.hh` |
| 决策 | 所有标准 TLM 模块在 `chstream_register.hh` 集中注册，配套 `REGISTER_CHSTREAM` 宏 |
| 理由 | 避免散落注册；升级 CppTLM 时注册表自动同步 |
| 后果 | ✅ 集中可见；✅ 一键启用；⚠️ 用户新增 IP 需修改此文件 |

**验证命令**：
```bash
test -e /workspace/project/CppTLM/include/chstream_register.hh && \
grep -qE "REGISTER_CHSTREAM" /workspace/project/CppTLM/include/chstream_register.hh
```

**代码锚点**：`CppTLM/include/chstream_register.hh`

---

#### ADR-039：统一目录结构（取消 tlm/rtl 分离）

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 1 提案（**未执行**：当前仍 tlm/rtl 分离）|
| 来源 | `declarative-hybrid-framework.md` ADR-6 |
| 决策 | 取消 `ip/<x>/tlm/` 与 `ip/<x>/rtl/` 分离，按职责组织（`bundles/` / `plugins/` / `tlm/` / `rtl/` / `configs/` / `tests/`）|
| 理由 | 双模式由编译期 / 运行期开关切换，不应反映在顶层目录上 |
| 后果 | 🚧 **未执行**：当前 ChipForge 仍是 `ip/cache/{tlm,rtl,configs,test}/` 分离 |

**验证命令**（当前预期失败）：
```bash
# 当前 tlm/rtl 仍分离（说明 ADR-6 未执行）
[[ -d /workspace/project/ChipForge/ip/cache/tlm ]] && \
[[ -d /workspace/project/ChipForge/ip/cache/rtl ]] && \
  echo "DRIFT: 目录仍分离 — ADR-039 仍为 🚧 状态"
```

**代码锚点（当前）**：`ip/cache/{tlm,rtl,configs,test}/`, `ip/cpu/{tlm,rtl,configs,test}/`, `ip/interconnect/...`, `ip/memory/...`, `ip/peripheral/...`

---

## 4. 漂移检测规则

### 4.1 漂移定义（两层）

#### 4.1.1 机械漂移（Mechanical Drift）

**机械漂移** = 代码实现与本文档声明的架构在**结构层面**不一致。可被 `verify_adr.sh` 自动检测。

可能原因：
1. CppTLM/CppHDL 内部重构（路径变更、类重命名）
2. 新增 / 废弃 API 但本文档未更新
3. 文档撰写时未完全对照代码

#### 4.1.2 设计意图漂移（Design-Intent Drift）

**设计意图漂移** = 代码与文档在结构上一致，但**设计本身已不再合理**。**无法用机械命令检测**，需要 AI / 人工审查。

可能原因：
1. 架构的隐式非目标被新代码违反
2. 抽象层级对新场景过高或过低
3. 命名约定不一致或易混淆
4. 性能假设不再成立
5. 有更好的替代方案出现

**检测方法**：使用配套技能 [`verify-architecture`](../../.opencode/skills/verify-architecture/SKILL.md)（Phase 2/3）

### 4.2 漂移分类

| 严重度 | 类型 | 含义 | 修复 SLA |
|--------|------|------|----------|
| **Critical** | 机械 | ✅ 决策验证失败 | 24 小时内修复文档或回滚代码 |
| **Major** | 机械 | ⚠️ 决策核心部分失败 | 1 周内修复 |
| **Minor** | 机械 | 🚧 决策**已实现但文档仍标 🚧** | 1 个月内更新文档 |
| **False Positive** | 机械 | 验证命令有误 | 立即修正命令 |
| **Architectural** | 设计意图 | 设计不再合理（非目标违反/复杂度失控/替代方案出现） | 下一季度审查处理 |
| **Strategic** | 设计意图 | 战略层面需重新评估 | 重大版本前必处理 |

### 4.3 漂移处理流程

```
1. 运行 tools/verify_adr.sh（机械层）
2. 任何 ✅ ADR 验证失败 → Critical 漂移
3. 立即定位：代码还是文档出错？
   ├─ 代码错 → 回滚或修正代码，恢复架构承诺
   └─ 文档错 → 更新本文档（修改 ADR 状态或验证命令）
4. 在 CHANGELOG 记录漂移
5. 若为 API 重构，更新对应源文档（code-framework-mapping.md / declarative-hybrid-framework.md）

↓

6. 每月/每季度：触发 verify-architecture 技能（设计意图层）
7. 11 项检查清单 + 5 个战略性问题
8. 输出：重大设计问题清单 + 行动建议
9. 必要时新增/废弃 ADR
```

### 4.4 漂移预警

**潜在漂移点**（近期高风险）：
- `CppTLM/include/framework/stream_adapter.hh` 中的 `StreamAdapterBase` 已于 2026-06-02 迁出至 `core/stream_adapter_base.hh`，本文档 ADR-005 已更新
- 新增 TLM 标准模块时，需同步更新 ADR-016 的"已注册模块清单"
- 升级 CppHDL 时，需核对 ADR-009/010/011 的模板签名
- **设计意图层面**：`CtrlLink::halt_when` 与 CppHDL chlib `stream_halt_when` 自由函数命名冲突（ADR-033）

---

## 5. 验证脚本说明

### 5.1 脚本位置

`tools/verify_adr.sh`

### 5.2 使用方式

```bash
# 在仓库根目录运行
cd /workspace/project/ChipForge
bash tools/verify_adr.sh

# 详细输出模式
VERBOSE=1 bash tools/verify_adr.sh

# 仅运行特定类别
bash tools/verify_adr.sh --category=A    # 框架架构
bash tools/verify_adr.sh --category=B    # TLM 模块
bash tools/verify_adr.sh --only=ADR-001  # 单条决策
```

### 5.3 输出格式

```
[ADR-001] ✅ PASS  三层框架分工
[ADR-002] ✅ PASS  CppTLM 事件驱动调度
[ADR-005] ⚠️  STALE_PATH  StreamAdapter 类型擦除
  → framework/stream_adapter.hh:61 references class StreamAdapterBase
    but actual definition is at core/stream_adapter_base.hh
[ADR-025] 🚧 EXPECTED_MISSING  Plugin 基类无 tick (符合 Phase 1 提案)

Summary:
  ✅ 23 PASS
  ⚠️  1 STALE (建议更新文档)
  🚧 14 EXPECTED_MISSING (Phase 1 提案，缺失符合预期)
  ❌ 0 FAILED
```

### 5.4 退出码

| 退出码 | 含义 |
|--------|------|
| 0 | 所有 ✅ 决策通过验证；无 Critical 漂移 |
| 1 | 至少一个 ✅ 决策验证失败（Critical 漂移） |
| 2 | 脚本自身错误（命令不存在等） |

### 5.5 CI 集成建议

在 `.github/workflows/ci.yml` 中添加：

```yaml
- name: Verify ADR alignment
  run: bash tools/verify_adr.sh
- name: Fail on Critical drift
  if: failure()
  run: |
    echo "::error::Architecture drift detected. Run 'bash tools/verify_adr.sh --verbose' for details."
```

---

## 6. 变更日志

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-06-07 | 1.0 | 初始版本：38 条 ADR，24 已实现 + 14 Phase 1 提案 |
| | | 配套 `tools/verify_adr.sh` 同步发布 |
| | | 与 `code-framework-mapping.md` v2.0 / `declarative-hybrid-framework.md` v2.0 对齐 |
| 2026-06-08 | 1.1 | 新增 ADR-037（Plugin 作为设计范式）；ADR-025~036 状态映射到 Phase 0/1+/6；原 ADR-037 编号顺延为 ADR-039 |

---

*本文档由架构评审过程自动生成，所有 ADR 与验证命令均经过至少一次直接代码核查。下次审计建议时间：2026-07-07（一个月后）。*

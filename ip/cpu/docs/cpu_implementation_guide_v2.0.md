# ip/cpu 实施指南 v2.0 — 声明式电路方法学 + RISC-V CPU 模块开发

> **版本**: v2.0 (Accepted)
> **状态**: 🟢 **Accepted** — 用户 2026-06-15 确认 F1-F6 全部决议 + 议题 1-8 全部选择
> **决策 ID**: DECISION-2026-06-15-02 (v2.0 完整版, 取代 v2.0 Proposed 草稿)
> **取代关系**: 全面取代 v1.0 通用 RISC-V TLM 指南 + v1.1 Phase 2 bare-metal 适配版 (均已废)
> **核心目标**: 用声明式电路方法学 (Plugin-style) 设计 ISA 与 CPU 架构相互独立的 RISC-V 核, 为以后 CPU 架构探索 (DSE) 打下基础
> **基础文档**:
> - `ip/cpu/docs/multi_isa_architecture.md` v2.0 (1164 行, 权威设计) — Phase 1.4 验证 L1CachePlugin 方法学的可复用基线
> - `docs/architecture/plugin-framework.md` v1.0 (861 行, Plugin 范式)
> - `docs/architecture/declarative-hybrid-framework.md` v2.1.0 (1241 行, TLM/RTL 混合架构)
> - `docs/methodology/plugin-style-design-methodology-v1.md` (352 行, L1CachePlugin 6 维度方法学)
> - `docs/lessons/phase-1.2-l1cacheplugin.md` (353 行, 行级踩坑清单)
> - `bundles/README.md` (D4 Plugin-style 强制)
> - `bundles/mem_bundles.h` (已实施 6 个 Bundle: MemReq/MemResp/CacheReq/CacheResp/...)

---

## 目录

1. [核心定位: ISA 与 CPU 架构解耦](#1-核心定位-isa-与-cpu-架构解耦)
2. [核心原则](#2-核心原则)
3. [用户确认的决议 (F1-F6) + 议题选择 (议题 1-8)](#3-用户确认的决议-f1-f6--议题选择-议题-1-8)
4. [实施范围与不在范围](#4-实施范围与不在范围)
5. [目录结构 (严格按 multi_isa v2.0 §7)](#5-目录结构-严格按-multi_isa-v2.0-7)
6. [核心 Plugin 套件 (5+6 Plugin 拆分)](#6-核心-plugin-套件-56-plugin-拆分)
7. [CpuFactory — 流水线组装入口](#7-cpufactory--流水线组装入口)
8. [复用现有 cf_plugin + 扩展 (议题 1 选 C)](#8-复用现有-cf_plugin--扩展-议题-1-选-c)
9. [Plugin 拆分粒度 (议题 2 选 B)](#9-plugin-拆分粒度-议题-2-选-b)
10. [RegFilePlugin array_store 抽象 (议题 3 选 B+C)](#10-regfileplugin-array_store-抽象-议题-3-选-bc)
11. [JSON 字段名 (议题 4 选 B)](#11-json-字段名-议题-4-选-b)
12. [Plugin 注册顺序 (议题 5 选 B)](#12-plugin-注册顺序-议题-5-选-b)
13. [联调路径: picolibc 内存区域绕过 MemoryTLM stub (议题 6 选 C)](#13-联调路径-picolibc-内存区域绕过-memorytlm-stub-议题-6-选-c)
14. [ISA 切换机制: 仅 riscv (议题 7 选 A)](#14-isa-切换机制-仅-riscv-议题-7-选-a)
15. [验证范围: build_cpu + 手工 ELF (议题 8 选 B)](#15-验证范围-build_cpu--手工-elf-议题-8-选-b)
16. [方法学复用: L1CachePlugin 6 维度 (Phase 1.4 已验证)](#16-方法学复用-l1cacheplugin-6-维度-phase-14-已验证)
17. [与 VexRiscv 的关系 (议题 1 备注: 解决 VexRiscv 思路)](#17-与-vexriscv-的关系-议题-1-备注-解决-vexriscv-思路)
18. [三级测试金字塔 (复用 multi_isa §8)](#18-三级测试金字塔-复用-multi_isa-8)
19. [风险与边界](#19-风险与边界)
20. [实施里程碑 (M1-M5)](#20-实施里程碑-m1-m5)
21. [联调路径 (CPU 完成后)](#21-联调路径-cpu-完成后)
22. [下一步 (用户授权后启动 M1)](#22-下一步-用户授权后启动-m1)

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

## 3. 用户确认的决议 (F1-F6) + 议题选择 (议题 1-8)

### 3.1 决议草案 (F1-F6) — 全部 Accepted

| # | 决议 | 状态 | 关键内容 |
|---|------|------|---------|
| **F1** | 范围 = ip/cpu 模块实施 (RISC-V RV32I/RV64I + M + Zicsr + Zifencei), 5/3 级流水线, TLM_ONLY | 🟢 **Accepted** | 排除 FPU/Vector/Multi-core/7级 (Phase 5+) |
| **F2** | 完全复用 multi_isa_architecture.md v2.0 的目录结构与 Plugin 分类 | 🟢 **Accepted** | 与权威设计 1:1 对齐 |
| **F3** | 复用 L1CachePlugin Phase 1.4 6 维度方法学 (B1 接受) | 🟢 **Accepted** | D1-D6 全维度复盘已沉淀在 methodology 文档 |
| **F4** | riscv-tests/riscv-arch-test/Spike/Python 全部推迟 (本期仅 CppTLM) | 🟢 **Accepted** | Phase 1.5 仅 CppTLM 声明式电路, 测试套件下一阶段 |
| **F5** | Phase 1 仅 TLM_ONLY, RTL/COMPARE 推迟到 Phase 5 | 🟢 **Accepted** | `ImplMode::TLM` 是本期末唯一值 |
| **F6** | RISC-V 与 ARM ISA 隔离 (`arch/riscv/` + 预留 `arch/arm/`), 为多 ISA 集成打基础 | 🟢 **Accepted** | 本期仅 riscv, arm/ 不创建 |

### 3.2 议题 1-8 选择 — 全部用户回复

| # | 议题 | 用户选择 | 选项 A | 选项 B | **选项 C (用户选)** |
|---|------|---------|--------|--------|----------------------|
| **1** | core/ 框架层实施深度 | **C** | 完整 1:1 | 最小可用 | **复用现有 cf_plugin + 扩展** |
| **2** | Plugin 拆分粒度 | **B** | 11 全部 | **5 个核心优先** | 5+6 stub |
| **3** | RegFilePlugin 物理实现 | **B+C** (同一个东西) | 纯 lambda | **array_store (cf_plugin 已有 storage.h)** | **复用 cf_plugin storage.h** |
| **4** | JSON 字段名 | **B** | 沿用旧字段 | **multi_isa v2.0 §6.1 标准** | 兼容两套 |
| **5** | Plugin 注册顺序 | **B** | 严格 multi_isa 顺序 | **CpuFactory 内置 PluginOrder 列表** | JSON `plugins[]` 数组 |
| **6** | 联调路径 | **C** | 实施 MemoryTLM | L1Cache 2KB 限制 | **picolibc 内存区域绕过 MemoryTLM stub** |
| **7** | ARM 目录 | **A** | **不创建, 仅 riscv** | 创建 arch/arm/ stub | — |
| **8** | 验证范围 | **B** | build_cpu + riscv-tests 工具链 | **build_cpu + 手工编译最小 ELF** | 单 Plugin 单元测试 |

### 3.3 议题 1 备注 — 解决 VexRiscv 思路 (用户主动提出)

> 用户在议题 1 选择 C (复用 cf_plugin) 后, 主动备注:
> "**再实施过程中, 同时解决 VexRiscv 的思路**"

含义:
- 复用 cf_plugin Phase 0 (PluginBase / Payload / PipeNode / PipeBuilder / CtrlLink) 5 个头文件
- 解决 VexRiscv 思路 = 在实施 CPU Plugin 过程中, 借鉴 VexRiscv Plugin 设计模式, 解决 multi_isa v2.0 §2-4 描述的 PipeLink / DirectLink / declare_substage 等尚未实现的 API
- VexRiscv 的 40+ 复杂 Plugin (Cache, MMU, CSR, Debug, Interrupt) 共享同一流水线骨架, 我们参考其**Plugin 分类** (核心/扩展/可选) 但不直接移植其 Scala 代码
- 实施过程中如发现 cf_plugin 缺 API (例如 declare_substage, PipeLink), 应**扩展 cf_plugin** 而非在 ip/cpu 内部重新实现

---

## 4. 实施范围与不在范围

### 4.1 范围

| 项 | 范围 | 备注 |
|----|------|------|
| **ISA** | RISC-V RV32I/RV64I + M + Zicsr + Zifencei | 5 个扩展, 11 个 Plugin 套件 |
| **流水线** | 3 级 (embedded) / 5 级 (default) | 7 级超标量 Phase 5+ |
| **IP 形态** | TLM_ONLY (ImplMode::TLM) | RTL/COMPARE Phase 5+ |
| **多 ISA 集成** | 仅 RISC-V, 但目录接口预留 ARM | 不创建 arch/arm/ 目录 |
| **联调** | CPU + L1CachePlugin + picolibc 内存 | 绕过 MemoryTLM stub |
| **CPU 架构 DSE** | 改 JSON 配置切换 3/5 级流水 | 为以后 DSE 工具链打基础 |
| **验证** | 单元测试 + build_cpu 集成 + 手工编译 ELF | 不引入 riscv-tests 工具链 |

### 4.2 不在范围 (明确推迟)

| 项 | 推迟到 | 理由 |
|----|--------|------|
| FPU (F/D 扩展) | Phase 5+ | 单精度/双精度浮点, 需要 FPU Plugin, 复杂度高 |
| Vector (V 扩展) | Phase 6+ | 向量寄存器 + lane 操作, 需重新设计 Payload |
| Multi-core | Phase 6+ | 多核一致性 + Cache coherence, 需大量新增 |
| 7 级超标量 | Phase 5+ | 7 级流水 + 重命名/发射/ROB 复杂, DSE 阶段 |
| RTL_ONLY / COMPARE | Phase 5+ | 本期末仅 TLM, Phase 5 实施 CppHDL RTL |
| riscv-tests 工具链 | Phase 5+ | 依赖完整 CPU + 工具链, 本期不引入 |
| Spike / sail_riscv 对比 | Phase 5+ | 行为验证, 本期不引入 |
| MemoryTLM | Phase 2.5+ | 当前 stub, 联调阶段 picolibc 绕过 |
| arch/arm/ 目录 | Phase 5+ | 本期仅 RISC-V, 目录结构预留 |
| BundleMapper (POD↔ch_uint) | Phase 5+ | RTL 阶段才需, 推迟 |

---

## 5. 目录结构 (严格按 multi_isa v2.0 §7)

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

### 5.1 与 multi_isa v2.0 §7 差异 (我们接受但标注)

- `ip/cpu/tlm/` 与 `ip/cpu/rtl/` 目录**取消**, 由 `ImplMode` 编译期切换 (multi_isa v2.0 §7 决定)
- 旧 `docs/riscv/VexRiscvArch.md` 与 `VexRiscvOnCppTLM.md` **已 git rm** (2026-06-13 19:43), 留 `riscv/` 目录待新内容
- 旧 `configs/cpu_default.json` 字段需按 multi_isa v2.0 §6.1 重写 (议题 4 选 B)
- 本期插件套件仅 5+1 (RegFile/Hazard/BP/IBus/DBus + Decode), FPU/MMU/Exception 推迟

---

## 6. 核心 Plugin 套件 (5+6 Plugin 拆分, 议题 2 选 B)

### 6.1 ISA 无关 Plugin (5 个, 议题 2 选 B: 核心优先)

| Plugin | 职责 | 输入 Payload | 输出 Payload | 优先级 | M 里程碑 |
|--------|------|--------------|--------------|--------|---------|
| `RegFilePlugin` | 通用寄存器堆 (xlen 参数化) | pl::RS1, RS2 (读), pl::RD_IDX, RD_DATA (写) | 无 (写回 regfile_) | **P0** | M2 |
| `HazardPlugin` | RAW/WAW/WAR 数据冒险检测 | pl::DECODE (rs_idx + uses_*) | CtrlLink.halt_when(cond) | **P0** | M2 |
| `BranchPredictorPlugin` | BTB / Bimodal / GShare | pl::DECODE.is_branch | PC redirect (CtrlLink.bypass) | P1 | M2 (stub) |
| `IBusPlugin` | 取指总线接口 (CPU 对外) | pl::PC | MemReqBundle (外发) + pl::INSTRUCTION (回收) | **P0** | M2 |
| `DBusPlugin` | 数据总线接口 | pl::LSU_REQ | MemReqBundle (外发) + pl::LSU_RESP (回收) | **P0** | M2 |

### 6.2 RISC-V ISA 特有 Plugin (6 个, 议题 2 选 B: 核心优先)

| Plugin | 职责 | 输入 Payload | 输出 Payload | 优先级 | M 里程碑 |
|--------|------|--------------|--------------|--------|---------|
| `RiscvDecodePlugin` | 译码并填两份 Payload | pl::INSTRUCTION | pl::DECODE (通用) + pl::RISCV_DETAIL (ISA 特有) | **P0** | M3 |
| `RiscvIntAluPlugin` | RV32I/RV64I 整数运算 | pl::DECODE, pl::RS1, pl::RS2 | pl::RESULT | **P0** | M3 |
| `RiscvBranchPlugin` | 分支跳转 + 链接 | pl::DECODE, pl::RS1, pl::IMM | pl::BRANCH_TARGET, pl::RD_DATA | P1 | M3 |
| `RiscvMulPlugin` | M 扩展乘除法 (3 级子流水) | pl::RS1, pl::RS2 | pl::RESULT (3 拍后) | P1 | M3 |
| `RiscvLsuPlugin` | load/store (含地址生成) | pl::DECODE, pl::RS1, pl::IMM | pl::LSU_REQ → MemReqBundle | P1 | M3 |
| `RiscvCsrPlugin` | Zicsr 读写 | pl::DECODE, pl::RS1, pl::IMM | pl::RESULT, pl::CSR_WRITTEN | P2 | M3 (stub) |

### 6.3 ISA 无关与 ISA 特有 Plugin 的关系 (multi_isa v2.0 §5.2 双 Payload)

```
RiscvDecodePlugin  (arch/riscv/decode.h)
  ↓ 同时填两份
  ├── pl::DECODE      (通用, HazardPlugin / BranchPredictor / RegFile / IBus / DBus 读)
  └── pl::RISCV_DETAIL (ISA 特有, IntAlu / Mul / Branch / Lsu / Csr 读)
```

**关键**: 通用 Plugin **只**读 `pl::DECODE`, **不**依赖 ISA 字段; ISA Plugin **直接**访问 `pl::RISCV_DETAIL`, 无需转型。 这是 multi_isa v2.0 §5.2 双 Payload 共存的设计, 让 HazardPlugin 跨 ISA 零修改复用。

---

## 7. CpuFactory — 流水线组装入口 (议题 5 选 B)

```cpp
// ip/cpu/cpu_factory.h
// 议题 5 选 B: CpuFactory 内置 PluginOrder 列表, 集中管理

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

### 7.1 PluginOrder 设计原则 (议题 5 选 B)

- **CpuFactory 是 PluginOrder 单一真相源**: 调度顺序的真相 (source of truth) 在 CpuFactory 的 `build_cpu()` 静态方法中
- **同 Phase 内顺序 = CpuFactory 注册顺序**: 用户修改 CpuFactory 即控制执行顺序
- **跨 Phase 顺序 = EARLY → NORMAL → LATE**: 由 PipeBuilder 强制 (multi_isa v2.0 §3.2 + §4)
- **JSON `plugins[]` 数组不控制顺序** (议题 5 排除选项 C): JSON 仅决定 "哪些 Plugin 被实例化", 不决定 "执行顺序"。 避免多源真相

### 7.2 5 级流水线 (默认配置, `configs/cpu_default.json`)

```
IF (fetch) → ID (decode) → EX (execute) → MEM (memory) → WB (writeback)
  │            │              │              │             │
  ├─IBusPlugin─┤              │              ├─DBusPlugin──┤
  │            ├─DecodePlugin─┤              │             ├─RegFilePlugin
  │            │              ├─IntAlu/Mul──┤             │
  │            ├─HazardPlugin─┤              │             │
  │            ├─BranchPredic─┤              │             │
```

### 7.3 3 级嵌入式流水线 (`configs/cpu_embedded.json`)

```
IF (fetch+decode) → EXMEM (execute+memory) → WB (writeback)
```

---

## 8. 复用现有 cf_plugin + 扩展 (议题 1 选 C)

### 8.1 复用 vs 扩展决策表

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

### 8.2 cf_plugin 扩展点 (议题 1 选 C: 复用 + 扩展)

#### 8.2.1 PipeBuilder 扩展: `at_stage` / `declare_substage`

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

#### 8.2.2 PipeLink 扩展: StageLink + DirectLink

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

#### 8.2.3 PipeArbitration 新建

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

#### 8.2.4 PayloadCommon 新建

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

### 8.3 解决 VexRiscv 思路 (议题 1 备注: 用户主动提出)

**VexRiscv** 是 SpinalHDL 生态最成功的开源 RISC-V CPU, 40+ 复杂 Plugin (Cache, MMU, CSR, Debug, Interrupt) 共享同一流水线骨架。 我们的解决思路 (不直接移植 Scala 代码):

1. **Plugin 分类借鉴**: VexRiscv 把 Plugin 分为核心 (I/M/A/C) / 扩展 (F/D/V) / 可选 (Debug, Interrupt)。 我们的 §6 也是这个分类
2. **流水线骨架借鉴**: VexRiscv 用 `Node` + `StageLink` + `CtrlLink` 三类基本元素。 multi_isa v2.0 §2 + 我们 §8.2.2 沿用
3. **两级 Plugin 抽象借鉴**: VexRiscv 有 `DbusAccessService` 等跨 Plugin 服务接口, 通过 Service 注入。 我们本期不引入 (Phase 6 复杂度), 但预留扩展点
4. **数据库驱动译码借鉴**: VexRiscv 用 Scala 宏生成 `JDB` 译码表, 类似我们用 JSON `decoder_table.h` 表驱动 (但我们的更简单)
5. **调试接口借鉴**: VexRiscv 提供 Debug Plugin, 暴露内部状态。 我们本期不引入

**具体实施**: 复用 VexRiscv 的**思路** (Plugin 分类, 骨架抽象), 但**实现**遵循 multi_isa v2.0 §2-4 描述的 C++ 17 PipeNode/Link/Builder/Plugin 抽象。 不依赖 SpinalHDL, 也不直接照搬 VexRiscv Scala 代码。

---

## 9. Plugin 拆分粒度 (议题 2 选 B)

### 9.1 5 个核心 P0 优先实施, 6 个 P1+ 推迟

| 优先级 | Plugin | 实施时机 |
|--------|--------|----------|
| **P0 (本期 M3 必做)** | RiscvDecodePlugin, RiscvIntAluPlugin | M3 |
| P0 (本期 M2 必做) | RegFilePlugin, HazardPlugin, IBusPlugin, DBusPlugin | M2 |
| P1 (本期 M3 必做) | RiscvBranchPlugin, RiscvLsuPlugin, RiscvMulPlugin, BranchPredictorPlugin | M3 |
| P2 (本期 M3 stub) | RiscvCsrPlugin | M3 (目录 + .h 占位, .cpp 写 `// TODO: M3+`) |
| P3+ (Phase 5+ 推迟) | RiscvFpuPlugin, MmuPlugin, ExceptionPlugin | 暂不创建 |

### 9.2 推迟的 Plugin 占位策略

- **目录占位**: 所有 Plugin 都有 `ip/cpu/plugins/*.h` (P0/P1/P2/P3 都创建)
- **实现占位**: P2/P3 仅有 `.h` 头文件声明, `.cpp` 写 `// TODO: M3+` 或 `throw std::runtime_error("Plugin not implemented");`
- **不在 CpuFactory 注册**: P2/P3 Plugin 不在 `build_cpu()` 中注册, 用户配置 `ext_zicsr=true` 也不会生效 (本期)

### 9.3 推迟 Plugin 的 ADR

需要在 `.omo/drafts/` 起草 ADR-XXX (Plugin 推迟决策):

- ADR 内容: 为什么 RiscvFpuPlugin / MmuPlugin / ExceptionPlugin 推迟, 推迟到 Phase 5+ 的具体条件
- 引用: multi_isa v2.0 §1.1 项目目标 (本期仅 RV32I/RV64I + M + Zicsr + Zifencei)
- 范围: 推迟的 Plugin 在 `ip/cpu/plugins/fpu.h` 中声明, 但 `.cpp` 写明推迟原因

---

## 10. RegFilePlugin array_store 抽象 (议题 3 选 B+C)

### 10.1 议题 3 注释: "B 和 C 是一个东西"

用户指出: **选项 B (引用 L1CachePlugin 的 array_store) 和 选项 C (复用 cf_plugin storage.h)** 是同一物。 这是 Phase 1.4 1.4 §2.4 决策: cf_plugin Phase 0 已有 `include/cf/plugin/storage.h` 提供 `array_store<T,N>`, L1CachePlugin `ip/cache/tlm/L1CachePlugin.h:147-149` 引用 cf_plugin storage.h, 这是 L1CachePlugin 6 维度方法学 D1 + D2 共同支持的"复用现有抽象"。

### 10.2 RegFilePlugin 物理实现

```cpp
// ip/cpu/plugins/reg_file.h (议题 3 选 B+C: 复用 array_store 抽象)
#pragma once

#include <array>
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/storage.h"   // 议题 3: array_store<T,N> 复用

namespace cf {
namespace cpu {

// 通用寄存器堆 (xlen 参数化)
template <typename XLEN_T>
class RegFileStorage {
 public:
  // 32 元素寄存器堆 (x0 始终为 0, x1-x31 可读写)
  // 议题 3: 用 array_store 抽象 (与 L1CachePlugin 一致)
  cf::plugin::array_store<XLEN_T, 32> regs_;
};

class RegFilePlugin : public cf::plugin::PluginBase {
 public:
  RegFilePlugin() : PluginBase("RegFilePlugin") {}

  void build(cf::plugin::PipeBuilder& b) override {
    // 写回阶段: 把 RD_DATA 写入 RD_IDX 寄存器
    b.at_stage("writeback", cf::plugin::Phase::NORMAL, [this, &b]() {
      auto* n = b.node_of_logic_stage("writeback");
      if (!n->is_firing()) return;
      const auto& rd_idx = (*n)(pl::RD_IDX);
      const auto& rd_data = (*n)(pl::RD_DATA);
      if (rd_idx == 0) return;  // x0 始终为 0
      storage_.regs_[rd_idx] = rd_data;
    });
  }

 private:
  RegFileStorage<uint32_t> storage_;  // 默认 RV32, RV64 由 template 特化
};

}  // namespace cpu
}  // namespace cf
```

### 10.3 L1CachePlugin 1.4 教训复用

L1CachePlugin 1.4 §2.4 经验: `array_store<T,N>` 抽象**不完整** (Phase 1 退化), 但本期 CPU 端复用是合适的:

- L1Cache line_data_bits=8 (Phase 0 退化), RegFile xlen=32/64 (不退化)
- L1Cache 1 个 array, RegFile 32 个 element, 都是模板参数化
- L1Cache 在 at_stage 内用 `read_data(set)` / `write_set(set, ...)`, RegFile 用 `storage_.regs_[idx]` (更直接)

**Phase 6 升级路径** (不在本期): 当 `array_store` Phase 6 升级 (TBD) 时, RegFilePlugin 业务代码不需修改, 只需更新 storage.h 头文件。

---

## 11. JSON 字段名 (议题 4 选 B)

### 11.1 multi_isa v2.0 §6.1 标准 JSON Schema

完全按 multi_isa v2.0 §6.1 字段, 替换旧 `configs/cpu_default.json` (字段: `isa`/`pipeline_stages`/`clock_freq_mhz`/...):

```json
// ip/cpu/configs/cpu_default.json (议题 4 选 B: multi_isa v2.0 §6.1 标准字段)
{
  "$schema": "./cpu_params_schema.json",
  "arch": "scalar_inorder",
  "isa": "riscv",
  "isa_extensions": ["i", "m", "zicsr", "zifencei"],
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

  "csr_modes": ["m"],   // 仅 M-mode (Phase 1)
  "enable_mmu": false,
  "enable_pmp": false
}
```

```json
// ip/cpu/configs/cpu_embedded.json (3 级 RV32I)
{
  "$schema": "./cpu_params_schema.json",
  "arch": "scalar_inorder",
  "isa": "riscv",
  "isa_extensions": ["i"],
  "xlen": 32,
  "pipeline_stages": [
    { "name": "IF",    "logic_stages": ["fetch", "decode"] },
    { "name": "EXMEM", "logic_stages": ["execute", "memory"] },
    { "name": "WB",    "logic_stages": ["writeback"] }
  ]
}
```

```json
// ip/cpu/configs/cpu_superscalar.json (7 级 RV64IMAFD, Phase 5+ 字段先定义)
{
  "$schema": "./cpu_params_schema.json",
  "arch": "scalar_ooo",
  "isa": "riscv",
  "isa_extensions": ["i", "m", "a", "f", "d", "zicsr"],
  "xlen": 64,
  "pipeline_stages": [
    { "name": "IF1",    "logic_stages": ["fetch_p1"] },
    { "name": "IF2",    "logic_stages": ["fetch_p2"] },
    { "name": "ID",     "logic_stages": ["decode"] },
    { "name": "RENAME", "logic_stages": ["rename"] },
    { "name": "ISSUE",  "logic_stages": ["issue"] },
    { "name": "EX",     "logic_stages": ["execute"] },
    { "name": "RETIRE", "logic_stages": ["memory", "writeback"] }
  ]
}
```

### 11.2 cpu_params_schema.json 修订

`ip/cpu/configs/cpu_params_schema.json` (已存在, 3 KB) 需按 multi_isa v2.0 §6.1 修订, 添加字段:
- `arch` (scalar_inorder / scalar_ooo)
- `isa_extensions[]`
- `xlen` (32 / 64)
- `pipeline_stages[].name` + `logic_stages[]`
- `branch_predictor.type` + `btb_entries` + `bht_entries`
- `csr_modes[]`
- `enable_mmu` / `enable_pmp`

### 11.3 旧 JSON 字段处理

旧 `cpu_default.json` 字段 (`clock_freq_mhz` / `enable_pmp` / `enable_mmu` / `mmu_mode` / `branch_predictor` enum / `btb_entries` enum / `icache_latency_cycles` / `dcache_latency_cycles`):

- `clock_freq_mhz` + `icache_latency_cycles` + `dcache_latency_cycles`: 推迟 (本期末不实施 ISS 时序, 仅 cycle-approximate)
- `enable_pmp` + `enable_mmu` + `mmu_mode`: 保留 (CSR 字段, 即使禁用, 字段也需定义)
- `branch_predictor` + `btb_entries`: 改为对象结构 (multi_isa v2.0 §6.1)

---

## 12. Plugin 注册顺序 (议题 5 选 B)

### 12.1 CpuFactory 内置 PluginOrder 列表

**CpuFactory::build_cpu() 是 PluginOrder 单一真相源** (议题 5 选 B), 不在 JSON `plugins[]` 数组指定 (排除选项 C)。

**注册顺序设计原则** (multi_isa v2.0 §3.2 + §3.5):

1. **同 Phase 内**: 严格按 CpuFactory 代码顺序注册 → PipeBuilder 用注册顺序打破并列
2. **跨 Phase**: EARLY → NORMAL → LATE (由 `Phase` 枚举强制)
3. **跨 logic_stage**: 按物理 Node 拓扑顺序 (由 PipeBuilder 拓扑排序)

### 12.2 推荐 PluginOrder (CpuFactory 代码顺序)

```cpp
// 议序 12.2 推荐 CpuFactory 内置 PluginOrder
// (与 §7.1 一致)

// ISA 无关 (5 个 P0)
// 1. RegFilePlugin (写回逻辑, 优先级最高, 所有写回都依赖)
// 2. HazardPlugin (RAW/WAW/WAR 冒险检测, 必须在运算前)
// 3. IBusPlugin / DBusPlugin (总线接口, 必须先注册才能被其他 Plugin 引用)
// 4. BranchPredictorPlugin (P1, 提供 PC redirect, 必须在分支运算前)

// ISA 特有 (6 个)
// 5. RiscvDecodePlugin (P0, 填两份 Payload, 必须在所有 ISA Plugin 前)
// 6. RiscvIntAluPlugin (P0, 整数运算)
// 7. RiscvMulPlugin (P1, M 扩展, 可声明子流水)
// 8. RiscvBranchPlugin (P1, 分支跳转)
// 9. RiscvLsuPlugin (P1, load/store)
// 10. RiscvCsrPlugin (P2, CSR 读写)
```

### 12.3 与 multi_isa v2.0 §5.5 列表顺序对比

multi_isa v2.0 §5.5 表格顺序 (供参考):
- HazardPlugin → BranchPredictorPlugin → RegFilePlugin → IBusPlugin → DBusPlugin → RiscvDecodePlugin → RiscvIntAluPlugin → RiscvMulPlugin → RiscvBranchPlugin → RiscvCsrPlugin → RiscvLsuPlugin

我们推荐顺序与之**略有不同** (RegFilePlugin 提前), 因为:
- RegFilePlugin 在 writeback 阶段, 优先级在所有 Plugin 之后
- 但 RegFilePlugin 业务代码**不**调 at_stage 写 (它读 pl::RD_DATA, 写自己的 storage_)
- 因此**注册顺序**影响不大, 但**代码可读性** = "RegFilePlugin 第一行出现" 更符合"寄存器是 CPU 核心"直觉

**最终决定**: 按我们推荐顺序, 但需在 CpuFactory 注释中说明与 multi_isa v2.0 §5.5 差异, 解释理由 (代码可读性 vs multi_isa 1:1 顺序)。

---

## 13. 联调路径: picolibc 内存区域绕过 MemoryTLM stub (议题 6 选 C)

### 13.1 当前状态

`ip/memory/` 仅 `.gitkeep` + 规划 .md, **无任何 .h/.cpp 实现**。 MemoryTLM 是 stub, Phase 2.5+ 实施。

但本期末 CPU 模块完成后, 我们想跑通 add/sub/and/or/sll 等基础 RISC-V ELF 验证 CPU 正确性。 议题 6 选 C: **绕过 MemoryTLM, 用 picolibc 提供的内存区域作为主存**。

### 13.2 picolibc 内存区域作为主存

picolibc 1.8.6-2 (Phase 2 任务 1 已装) 提供了:
- `crt0.o` / `libcrt0.a` (静态库, 含 `_start` 入口)
- `libc.a` (基础 C 运行时)
- `libcrt0-hosted.a` (hosted 模式, 模拟 main return 退出)
- 链接器脚本: 默认 `. = 0x80000000` + `.data/.bss` 在主存区域

我们可以**直接用 picolibc 作为主存**:
1. riscv64-unknown-elf-gcc 编译 RISC-V C 程序
2. 链接时指定 picolibc 静态库 + 自定义链接脚本
3. 加载到 ChipForge TLM 平台时, **`.text` + `.data` + `.bss` 区域** 都被 picolibc 自动初始化
4. CPU 跑 ELF 时, 取指/访存**直接命中 picolibc 初始化后的内存** (无需 MemoryTLM)

### 13.3 集成步骤 (议题 6 选 C 实施)

```cpp
// ip/cpu/cpu_factory.h 中增加 host_memory 模式
// 议题 6 选 C: picolibc 内存区域作为主存

#include "bundles/mem_bundles.h"  // 复用现有 MemReq/MemResp POD

// picolibc 内存区域 (静态分配, 类似 OS 引导后的 RAM 镜像)
class PicolibcHostMemory {
 public:
  static constexpr size_t MEM_SIZE = 64 * 1024;  // 64KB
  uint8_t ram[MEM_SIZE];

  // 处理 MemReq: 读/写 picolibc RAM
  // CPU 的 IBus/DBus 命中此处, 无需 MemoryTLM
  void handle_request(const cf::bundles::MemReq& req,
                     cf::bundles::MemResp& resp) {
    // 简化版: 直接读写, 无 cache, 无 MMU
    if (req.is_write) {
      std::memcpy(&ram[req.address], &req.data, sizeof(req.data));
    } else {
      std::memcpy(&resp.data, &ram[req.address], sizeof(resp.data));
    }
    resp.id = req.id;
    resp.error = false;
  }
};
```

**风险**: picolibc RAM 容量 64KB, riscv-tests ELF 可能 > 64KB (本期不引入 riscv-tests, 议题 8 选 B: 手工编译最小 ELF, 远小于 64KB)。 议题 6 选 C 风险可控。

### 13.4 联调架构 (CPU + picolibc 内存, 议题 6 选 C)

```
soc/cpu_picolibc demo.json
├── modules:
│   ├── cpu    (build_cpu(cpus/cpu_default.json, TLM))
│   └── host_mem (PicolibcHostMemory 静态分配 64KB RAM)
└── connections:
    ├── cpu.IBus → host_mem (req)
    ├── host_mem → cpu.IBus (resp)
    ├── cpu.DBus → host_mem (req)
    └── host_mem → cpu.DBus (resp)
```

---

## 14. ISA 切换机制: 仅 riscv (议题 7 选 A)

### 14.1 决议: 不创建 arch/arm/ 目录

议题 7 选 A: Phase 1 **只实施 riscv**, **不创建** `arch/arm/` 目录。

**理由**:
- Phase 1 目标是 RISC-V (用户明确)
- `arch/` 目录结构预留为未来 ARM / OpenRISC / 自定义 ISA 接入点
- 不创建 `arch/arm/` 目录 = 不留未实施代码 (避免范围蔓延)
- CpuFactory 中 `if (isa == "riscv")` 分支可扩展, 但本期不实施

### 14.2 未来扩展点 (Phase 5+ 文档化, 不实施)

```cpp
// 未来扩展: 在 CpuFactory 添加 (Phase 5+)
// } else if (isa == "arm") {
//   pb->register_plugin(std::make_unique<ArmDecodePlugin>());
//   pb->register_plugin(std::make_unique<ArmIntAluPlugin>());
//   pb->register_plugin(std::make_unique<ArmBranchPlugin>());
//   ...
// }
```

### 14.3 multi_isa_architecture.md v2.0 §5.4 新增 ISA Checklist

新增 ISA 需要:
- 在 `arch/<isa>/` 创建子目录
- 定义 `<Isa>DecodeDetail` (Bundle)
- 实现 `<Isa>DecodePlugin`
- 实现核心执行 Plugin: `<Isa>IntAluPlugin` / `<Isa>BranchPlugin` / `<Isa>LsuPlugin`
- 可选扩展: MUL / FPU / CSR
- 在 `cpu_factory.h` 添加 `isa == "<isa>"` 分支
- 编写 `arch/<isa>/tests/` 单元测试
- 添加 `configs/<isa>_default.json`

(本期仅 riscv, 但**文档完整**, 未来 ARM 接入按此清单)

---

## 15. 验证范围: build_cpu + 手工 ELF (议题 8 选 B)

### 15.1 议题 8 选 B: 不依赖 riscv-tests 工具链

议题 8 选项:
- A: build_cpu + 跑 riscv-tests (需 riscv-tests 工具链, 跨范围)
- **B: build_cpu + 跑手工编译最小 ELF** (推荐)
- C: 仅单 Plugin 单元测试, integration 推迟

**议题 8 选 B 实施**: 复用 Phase 2 任务 11-13 已验证的"手工编译最小 ELF"路径:

```bash
# 编译 add.elf (RV32I add 指令验证)
riscv64-unknown-elf-gcc -march=rv32imac -mabi=ilp32 -static \
  --sysroot=/usr/lib/picolibc/riscv64-unknown-elf/ \
  -specs=picolibc.specs \
  -o /tmp/add.elf add.S

# 跑 CPU TLM 平台 (CpuFactory build_cpu + load_elf + run)
./build/chipforge_cpu --elf=/tmp/add.elf --cycles=1000
# 监控 pl::TOHOST 写入 1 → 退出码 0 → PASS
```

### 15.2 集成测试 (`test_5stage_riscv.cpp`)

```cpp
// ip/cpu/tests/integration/test_5stage_riscv.cpp
// 议题 8 选 B: build_cpu + 手工编译 ELF

#include "ip/cpu/cpu_factory.h"
#include "bundles/mem_bundles.h"

TEST(Integration, RV32I_AddInst) {
  // 1. 加载 5 级 JSON 配置
  auto config = load_json("ip/cpu/configs/cpu_default.json");

  // 2. 编译 CPU (议题 5 选 B: CpuFactory 集中 PluginOrder)
  auto pb = cf::cpu::CpuFactory::build_cpu(config);

  // 3. 加载手工编译的 add.elf (议题 6 选 C: picolibc 内存)
  auto* host_mem = pb->template get_module<cf::cpu::PicolibcHostMemory>("host_mem");
  load_elf_to_memory(*host_mem, "tests/manual_elf/add.elf");

  // 4. 跑 CPU 直到 tohost 写入
  while (!tohost_written(*pb)) {
    pb->tick();
  }

  // 5. 验证 tohost = 1 (PASS)
  EXPECT_EQ(read_tohost(*pb), 1u);
}
```

### 15.3 验证深度 (议题 8 选 B)

| 测试层级 | 数量 | 工具 | 目的 |
|----------|------|------|------|
| Plugin 单元测试 (Level A) | 11 个 (5 ISA 无关 + 6 ISA 特有) | gtest + cf_plugin testing | 单 Plugin 行为正确性 |
| 框架级测试 (Level A) | 4 个 (PipeNode/PipeBuilder/Payload/CtrlLink) | gtest | 复用 multi_isa §8.2 |
| 集成测试 (Level B) | 2 个 (3 级 + 5 级 RV32I) | gtest + 手工 ELF | 流水线整体 + 5/3 级 DSE |
| 手工 ELF 验证 (Level B) | 4-6 个 (add/sub/and/or/sll/srli) | hand-compiled | 基础指令正确性 |
| **不引入** | riscv-tests / Spike / sail_riscv / Python | — | 推迟 Phase 5+ |

### 15.4 与 Phase 1.4 L1CachePlugin 测试模式对比

L1CachePlugin 测试 pattern (Phase 1.4 §D6):
- 4/4 单元测试 (issue_request / refill / hit-after-refill / D4 runtime)
- 5/5 e2e (ModuleFactory / Adapter ctor / Bridge hold / tick→pb.run / 1000+ tx)
- 5/5 instantiateAll (full JSON e2e)

我们 CPU 端复用**同样**模式:
- Plugin 单元测试 → cf_plugin testing 模式
- build_cpu() e2e → instantiationAll 模式
- 手工 ELF 验证 → Phase 2 任务 11-13 模式

---

## 16. 方法学复用: L1CachePlugin 6 维度 (Phase 1.4 已验证)

### 16.1 L1CachePlugin 6 维度复盘结果 (Phase 1.4, 2026-06-13)

| 维度 | L1Cache 结论 | 复用策略 (本期 CPU) |
|------|-------------|---------------------|
| **D1 可读性** | 5/8 B1 接受, 3 个 B2 摩擦 | 复用: setup() + build() 职责分离, Payload Key 命名 `"cpu."` prefix |
| **D2 范式合规** | 5/5 D4 条款 + 4/4 ADR-040 Tier B1 接受 | 复用: 无 tick(), 无状态机, `uint_t<N>` 字段 |
| **D3 TLM↔RTL** | 2 B2 (harness 缺, COMPARE 待 Phase 5) | 推迟: CPU 本期仅 TLM, Phase 5+ RTL |
| **D4 阶段调度** | 2 B1 + 1 B2 (array_store 不全) | 复用: `at_stage(stage, phase, lambda)` |
| **D5 Payload 通信** | 1 B2 (Key 数量限制) | 复用: 双 Payload (通用 + ISA 特有) |
| **D6 测试便利** | 4/4 单元 + 5/5 e2e + 5/5 instantiateAll | 复用: 单元 + build_cpu e2e + 手工 ELF |

### 16.2 CPU 端预防 B2 摩擦 (来自 L1Cache 经验)

| B2 摩擦 (L1Cache) | CPU 端预防 |
|------------------|-----------|
| helper API 内部泄漏 (7 个 public helper) | friend class 隔离 (L1Cache 1.4 §2 模式) |
| array_store 抽象不完整 | 议题 3 选 B+C: 复用 cf_plugin storage.h 抽象 |
| 早返陷阱 (at_stage lambda 内 if 早返) | 统一约定 + lessons 文档化 (复用 `docs/lessons/phase-1.2-l1cacheplugin.md` §2.3) |
| Payload Key 数量限制 | 双 Payload (通用 + ISA 特有) — multi_isa v2.0 §5.2 |

### 16.3 CPU 特有风险与缓解

| 风险 | 缓解 |
|------|------|
| Plugin 数量多 (11 vs L1Cache 1) → 调度顺序确定性 | 议题 5 选 B: CpuFactory 集中注册顺序 |
| 流水线深度可配置 (3/5/7) → Plugin 跨深度复用 | 用 logical_stage 名 (multi_isa v2.0 §3) |
| RegFilePlugin 多 xlen (32/64) | 模板参数化, 编译期选 |
| MulPlugin 3 级子流水 vs 5 级合并 | declare_substage (议题 1 选 C: 复用 cf_plugin + 扩展) |
| CsrPlugin 270+ CSR 字段 | 表格驱动, 不在 at_stage 闭包内 if/else |
| picolibc 内存区域 64KB 限制 (议题 6 选 C) | 手工编译小 ELF, 远 < 64KB |

---

## 17. 与 VexRiscv 的关系 (议题 1 备注: 解决 VexRiscv 思路)

### 17.1 VexRiscv 简介

VexRiscv 是 SpinalHDL 生态最成功的开源 RISC-V CPU:
- 40+ 复杂 Plugin (Cache, MMU, CSR, Debug, Interrupt, MMU)
- 单一流水线骨架, Plugin 自由组合
- 支持 RV32I/M/A/C/F/D + Linux
- 用 Scala 编写, 编译为 Verilog/CHISEL

### 17.2 VexRiscv 思路借鉴 (不直接移植)

| VexRiscv 思路 | 我们借鉴方式 | 不借鉴原因 |
|--------------|-------------|-----------|
| 单一流水线骨架 + Plugin | ✅ 借用: multi_isa v2.0 §2 PipeNode/Link/Builder | 已是我们的核心 |
| Plugin 分类 (核心/扩展/可选) | ✅ 借用: 我们 §6.1 P0/P1/P2/P3 分类 | 已是我们的结构 |
| 流水线深度可配置 (3/5/7/10+) | ✅ 借用: multi_isa v2.0 §6 JSON | 已是我们的设计 |
| 跨 Plugin 服务接口 (DbusAccessService) | ❌ 不借鉴 | Phase 6 复杂度, 本期不引入 |
| 数据库驱动译码 (JDB) | ⚠️ 部分借鉴: JSON 译码表 | 我们更简单 (Phase 1 不需要 Scala 宏) |
| Debug Plugin | ❌ 不借鉴 | Phase 5+ 范围 |
| Multi-core 一致性 | ❌ 不借鉴 | Phase 6 范围 |
| Interrupt Controller (PLIC/CLINT) | ❌ 不借鉴 | Phase 3 范围 |

### 17.3 解决 VexRiscv 思路的具体行动 (议题 1 备注)

**"在实施过程中, 同时解决 VexRiscv 的思路"** = 我们**借鉴 VexRiscv 的设计哲学**, 但**不照搬其实现**:

1. **参考 VexRiscv Plugin 分类思路** → 我们 §6 已按 P0/P1/P2/P3 分类
2. **解决 VexRiscv 在 C++ 17 中的对应** → 我们用 multi_isa v2.0 §2-4 描述的 C++ 抽象 (PipeNode/Link/Builder/Plugin/Payload)
3. **不引入 SpinalHDL 依赖** → 纯 CppTLM + C++ 17
4. **不直接移植 Scala 代码** → 我们写自己的 C++ 17 Plugin

**这一解决思路** 反映在:
- CpuFactory 集中管理 PluginOrder (§7.1)
- Plugin 套件按 P0/P1/P2/P3 拆分 (§6)
- 复用 cf_plugin + 扩展 (议题 1 选 C, §8)
- 联调路径 picolibc 绕过 MemoryTLM (议题 6 选 C, §13)

---

## 18. 三级测试金字塔 (复用 multi_isa §8)

### 18.1 Level A — Plugin 单元测试 (本期必做)

**目标**: 验证单个 Plugin 在最小流水线 (1~2 个 Node) 内的行为正确性。

```cpp
// arch/riscv/tests/test_int_alu.cpp

TEST(RiscvIntAlu, AddBasic) {
  cf::plugin::PipeBuilder pb(cf::plugin::ImplMode::TLM);
  auto* ex = pb.create_node("EX");
  pb.bind_logic_stage("execute", ex);

  pb.register_plugin(std::make_unique<RiscvIntAluPlugin>());
  pb.build();

  DecodePayload d{};
  d.op_class = OpClass::ALU;
  d.writes_rd = true;
  RiscvDecodeDetail rd{};
  rd.funct3 = 0;  // ADD
  rd.funct7 = 0;
  (*ex)(pl::DECODE) = d;
  (*ex)(pl::RISCV_DETAIL) = rd;
  (*ex)(pl::RS1) = 3;
  (*ex)(pl::RS2) = 4;
  ex->arb().valid = true;
  ex->arb().ready = true;

  pb.tick();
  EXPECT_EQ((*ex)(pl::RESULT), 7u);
}
```

**覆盖率目标**: 11 个 Plugin × 单指令验证 (4-6 用例/Plugin) = 50-66 单元测试。

### 18.2 Level B — 集成测试 (CPU 模块完成时)

```cpp
// tests/integration/test_5stage_riscv.cpp
// 议题 8 选 B: 手工编译最小 ELF

TEST(Integration, RV32I_5Stage_Add) {
  auto config = load_json("ip/cpu/configs/cpu_default.json");
  auto pb = cf::cpu::CpuFactory::build_cpu(config);
  load_elf_to_picolibc(*pb, "tests/manual_elf/add.elf");
  run_until_tohost(*pb);
  EXPECT_EQ(read_tohost(*pb), 1u);  // PASS
}

TEST(Integration, RV32I_3Stage_Add) {
  auto config = load_json("ip/cpu/configs/cpu_embedded.json");
  auto pb = cf::cpu::CpuFactory::build_cpu(config);
  load_elf_to_picolibc(*pb, "tests/manual_elf/add.elf");
  run_until_tohost(*pb);
  EXPECT_EQ(read_tohost(*pb), 1u);  // PASS
}
```

### 18.3 Level C — COMPARE 模式 (Phase 5+ 推迟)

- TLM_ONLY (本期) → 推迟 RTL_ONLY/COMPARE 到 Phase 5
- multi_isa v2.0 §8.4 描述完整, 但 Phase 5 才实施

---

## 19. 风险与边界

### 19.1 复用 L1CachePlugin 6 维度教训 (从 Phase 1.4 复盘)

| B2 摩擦 (来自 L1Cache) | CPU 端预防策略 |
|----------------------|---------------|
| helper API 内部泄漏 (7 个 public helper) | **friend class 隔离** (L1Cache 1.4 §2 模式) |
| array_store 抽象不完整 | 议题 3 选 B+C: 复用 cf_plugin storage.h 抽象 |
| 早返陷阱 (at_stage lambda 内 if 早返) | 统一约定 + lessons 文档化 |
| Payload Key 数量限制 | 双 Payload (通用 + ISA 特有) |

### 19.2 CPU 特有风险

| 风险 | 缓解 |
|------|------|
| Plugin 数量多 (11 vs L1Cache 1) → 调度顺序确定性 | 议题 5 选 B: CpuFactory 集中管理 |
| 流水线深度可配置 (3/5/7) → Plugin 跨深度复用 | 用 logical_stage 名 (multi_isa v2.0 §3) |
| RegFilePlugin 多 xlen (32/64) | 模板参数化, 编译期选 |
| MulPlugin 3 级子流水 vs 5 级合并 | declare_substage (议题 1 选 C 扩展 cf_plugin) |
| CsrPlugin 270+ CSR 字段 | 表格驱动, 不在 at_stage 闭包内 if/else |
| picolibc 内存 64KB 限制 | 手工编译小 ELF (议题 8 选 B) |
| cf_plugin 缺 at_stage / declare_substage | 议题 1 选 C: 复用 + 扩展 (§8.2.1) |
| cf_plugin 缺 PipeLink (StageLink / DirectLink) | 议题 1 选 C: 复用 + 扩展 (§8.2.2) |

### 19.3 VexRiscv 解决思路风险 (议题 1 备注)

- 借鉴思路但**不**直接移植 → 自主实现 C++ 17 抽象
- 避免 Scala 宏生成 → 用 JSON 表驱动 (更简单)
- 避免 SpinalHDL 依赖 → 纯 CppTLM + C++ 17

---

## 20. 实施里程碑 (M1-M5)

| 阶段 | 内容 | 验收 | 估时 |
|------|------|------|------|
| **M1** | **核心框架层** (议题 1 选 C: 复用 cf_plugin + 扩展)<br>• 扩展 `cf::plugin::PipeBuilder` 增加 `at_stage` / `declare_substage`<br>• 新增 `cf::plugin::PipeLink` (StageLink / DirectLink)<br>• 新增 `cf::plugin::PipeArbitration`<br>• 新增 `ip/cpu/core/payload_common.h` (DecodePayload + 通用 Key) | 4/4 框架级单元测试 PASS<br>• test_pipe_node<br>• test_pipe_builder<br>• test_payload<br>• test_ctrl_link | **3-4 d** |
| **M2** | **ISA 无关 Plugin** (议题 2 选 B: 5 个核心 P0)<br>• `ip/cpu/plugins/reg_file.h` (议题 3 选 B+C: array_store)<br>• `ip/cpu/plugins/hazard.h`<br>• `ip/cpu/plugins/branch_predictor.h` (P1)<br>• `ip/cpu/plugins/ibus.h`<br>• `ip/cpu/plugins/dbus.h`<br>• `ip/cpu/plugins/fpu.h` (P3+ 占位)<br>• `ip/cpu/plugins/mmu.h` (P3+ 占位)<br>• `ip/cpu/plugins/exception.h` (P3+ 占位) | 5/5 P0 单元测试 PASS<br>• P3+ 占位 (.h 声明, .cpp TODO) | **2-3 d** |
| **M3** | **RISC-V ISA 特有 Plugin** (议题 2 选 B: 6 个 P0/P1/P2)<br>• `arch/riscv/decoder_table.h` (P0)<br>• `arch/riscv/payload_riscv.h` (P0)<br>• `arch/riscv/decode.h` (P0)<br>• `arch/riscv/int_alu.h` (P0)<br>• `arch/riscv/mul.h` (P1)<br>• `arch/riscv/branch.h` (P1)<br>• `arch/riscv/lsu.h` (P1)<br>• `arch/riscv/csr.h` (P2 stub)<br>• `arch/riscv/fpu.h` (P3+ 占位) | 6 × 单元测试 PASS<br>• RV32I 译码正确性<br>• RV32I 整数运算 | **4-5 d** |
| **M4** | **CpuFactory + JSON 配置 + 集成测试** (议题 4 选 B + 议题 5 选 B + 议题 8 选 B)<br>• `ip/cpu/cpu_factory.h` (集中 PluginOrder)<br>• 修订 `configs/cpu_default.json` (multi_isa v2.0 §6.1 字段)<br>• 修订 `configs/cpu_embedded.json`<br>• 新增 `configs/cpu_params_schema.json` (修订版)<br>• `tests/integration/test_5stage_riscv.cpp`<br>• `tests/integration/test_3stage_riscv.cpp`<br>• `tests/manual_elf/add.S` + `link.ld` + 编译脚本<br>• 议题 6 选 C: `PicolibcHostMemory` 静态 RAM | build_cpu() 跑通<br>• 5 级 + 3 级 end-to-end 跑通 `add.elf`<br>• tohost = 1 (PASS) | **2-3 d** |
| **M5** | **联调 + 文档收尾** (议题 6 选 C)<br>• CPU + L1CachePlugin + PicolibcHostMemory 联调<br>• 跑通 4-6 个基础 RISC-V ELF (add/sub/and/or/sll/srli)<br>• 修订 `ip/cpu/README.md` + `ip/cpu/docs/README.md`<br>• ADR-XXX: Plugin 推迟决策<br>• `git commit` + tag | 4-6 ELF 全 PASS<br>• 16/16 ctest 不退化<br>• D4 + ADR-040 3+4/3 PASS | **1-2 d** |
| **总计** | | | **12-17 d** |

---

## 21. 联调路径 (CPU 完成后)

### 21.1 联调前置

- ✅ L1CachePlugin (已落地, 4/4 单元测试, Phase 1.2)
- ✅ CpuFactory (M4 实施)
- ✅ 11 个 RISC-V Plugin (M2 + M3 实施)
- ✅ PicolibcHostMemory (议题 6 选 C: 绕过 MemoryTLM stub)
- ❌ MemoryTLM (当前 stub, 推迟 Phase 2.5+)

### 21.2 联调架构 (M5 实施)

```
soc/cpu_l1_picolibc demo.json
├── modules:
│   ├── cpu      (build_cpu(cpus/cpu_default.json, TLM))
│   ├── l1       (L1CacheTLMBridge + L1CachePlugin, 256 sets × 8B/line)
│   └── host_mem (PicolibcHostMemory, 64KB 静态 RAM)
└── connections:
    ├── cpu.IBus.req → l1.req_in
    ├── l1.req_out → cpu.IBus.resp
    ├── cpu.DBus.req → l1.req_in
    ├── l1 → host_mem (on miss)
    └── host_mem → l1 (resp)
```

### 21.3 联调验证 (M5 验收)

| 测试 ELF | 验证指令 | 期望结果 |
|----------|----------|---------|
| add.elf | ADD (RV32I) | tohost=1 PASS |
| sub.elf | SUB (RV32I) | tohost=1 PASS |
| and.elf | AND (RV32I) | tohost=1 PASS |
| or.elf | OR (RV32I) | tohost=1 PASS |
| sll.elf | SLL (RV32I) | tohost=1 PASS |
| srli.elf | SRLI (RV32I) | tohost=1 PASS |
| mul.elf | MUL (RV32M) | tohost=1 PASS |

### 21.4 不在 M5 范围 (推迟)

- riscv-tests / Spike Diff / RISCOF (Phase 5+)
- MemoryTLM 实施 (Phase 2.5+)
- RTL 协同验证 (Phase 5+)
- Multi-core (Phase 6+)

---

## 22. 下一步 (用户授权后启动 M1)

### 22.1 当前状态 (2026-06-15)

- ✅ v1.0 通用 RISC-V TLM 指南已识别为错误 (5 项调研)
- ✅ v1.1 适配版草案已 commit `ff21424` + tag `phase-2-v1.1-baseline-2026-06-15` (已废, 偏离 cpu 实施目标)
- ✅ v2.0 ip/cpu 实施指南草案已交付 (用户 2026-06-15 接受 F1-F6 + 议题 1-8)
- ✅ **本文档** (v2.0 Accepted 完整版) 已生成
- ⏸️ 等待: git commit 授权 + 启动 M1

### 22.2 M1 启动前置检查清单

| 项 | 状态 |
|----|------|
| **v2.0 文档** 用户授权 commit | 🟡 待用户授权 |
| **git tag** `phase-1.5-cpu-v2.0-baseline-2026-06-15` | 🟡 待用户授权 |
| **8 文件 staged** (v2.0 doc + 修订的 cf_plugin 扩展点) | 🟡 待 M1 启动前 |

### 22.3 M1 启动步骤 (用户授权后)

1. **git commit** v2.0 文档 + tag `phase-1.5-cpu-v2.0-baseline-2026-06-15`
2. **M1 启动**: 扩展 `cf::plugin::PipeBuilder` 增加 `at_stage` / `declare_substage` (议题 1 选 C)
3. **M1 验证**: 4/4 框架级单元测试 PASS
4. **进入 M2**: ISA 无关 5 个 Plugin
5. **进入 M3**: RISC-V ISA 6 个 Plugin
6. **进入 M4**: CpuFactory + JSON + 集成测试
7. **进入 M5**: 联调 + 文档收尾 + 最终 commit

### 22.4 文档状态: Accepted

- **状态**: 🟢 **Accepted** — 用户 2026-06-15 接受 F1-F6 + 议题 1-8
- **关联**: DECISION-2026-06-15-02 (v2.0 完整版)
- **取代**: v1.0 通用 RISC-V TLM 指南 + v1.1 适配版 (均已废)
- **位置**: `ip/cpu/docs/cpu_implementation_guide_v2.0.md` (本文件)

---

*本指南基于 4 项调查 (ip/ 目录布局 / multi_isa 1164 行 / plugin-framework 861 行 / L1CachePlugin Phase 1.4 方法学) + 用户 2026-06-15 对 6 项决议 + 8 个议题的全部选择起草。 后续实施严格按 §20 M1-M5 里程碑执行。*

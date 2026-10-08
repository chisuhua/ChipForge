# ADR-083：Plugin-style 作为 ChipForge 硬件模块的单一 source of truth

| 字段 | 值 |
|------|-----|
| 状态 | ✅ Accepted (2026-10-08) |
| 来源 | overview.md / AGENTS.md 文档漂移修正 + Plugin-style 范式固化 |
| 关联 ADR | **ADR-037**（Plugin 作为设计范式，v2.0 Phase 6c M5 落地）/ **ADR-040 v2.0**（TLM→HDL 移植性约束，CH_MEM 是新正道）/ ADR-025（Plugin 基类无 tick）/ ADR-046（多周期 FSM 豁免） |
| 关联文档 | `docs/architecture/overview.md` §1 架构总览 / `AGENTS.md` 顶部简介 / `docs/methodology/plugin-style-design-methodology-v1.md` v2 双模方法学 |

---

## 1. 背景

### 1.1 文档/代码漂移

overview.md (2026-08-14 快照) §"架构总览" 描述了一个**双层架构**：

```
+-----------------------+    +-----------------------+
|   CppTLM 组件层       |    |   CppHDL 组件层       |
|   ip/*/tlm/ CPUTLM    |    |   ip/*/rtl/ (Phase 5) |
+-----------------------+    +-----------------------+
```

并把 `ch_stream<T>` 描述为"IP 开发者视角"的数据流接口。该图与实际代码状态已经**漂移**：

- `ip/*/tlm/` 目录只有 `L1CachePlugin.cpp`（Plugin-style in TLM mode）和 `ip/mmu/tlm/MMUPlugin.cpp`（Plugin-style），**没有**继承 `ChStreamModuleBase` 的 TLM 模块
- `ip/*/rtl/` 目录除 README 外全空
- 跨阶段数据流用的是 `cf::plugin::Payload<T>` + `PipeNode` 而非 `ch_stream<T>`
- D4 决策（2026-06-08）以来，业务 IP 一律用 Plugin-style

### 1.2 三层分工的再认识

ADR-001（2026-06-07）定义了"三层框架分工"（ChipForge 应用 + CppHDL 硬件 IR + CppTLM 仿真内核）。这是**仓库符号链接层面的分工**，并不意味着 ChipForge 业务 IP 同时使用 CppTLM 和 CppHDL 的 API。

实际关系是：

```
┌────────────────────────────────────────────────────────────┐
│ ChipForge 应用层 (Plugin-style 业务代码)                   │
│   ip/{cpu,cache,mmu}/plugins/*.{h,_chmem.h}                │
│   bind 到 cf::plugin::PluginBase + PipeBuilder + Payload<T>│
└─────────────────────────────┬──────────────────────────────┘
                              │ uint_t<N>/bool_t 抽象
                              ▼
┌────────────────────────────────────────────────────────────┐
│ CppHDL 设施层 (chipforge 的硬件后端, v0.3.0 起是唯一后端) │
│   - TLM 模式: cf::plugin POD 直接走 C++ 直仿               │
│   - CH_MEM 模式: lnode DAG → Verilog → Verilator 仿真    │
│   - Bundle 类型系统 + chlib + toVerilog + Verilator backend│
└────────────────────────────────────────────────────────────┘
                              │
                              │ (外部依赖, 不被 Plugin 使用)
                              ▼
┌────────────────────────────────────────────────────────────┐
│ CppTLM 仿真内核 (legacy, 仅 bridge 适配层用)               │
│   - ch_stream / Module / Port / ModuleFactory             │
│   - Bridge 适配层 (src/cf_plugin/bridge/) 仅用于外部集成   │
└────────────────────────────────────────────────────────────┘
```

### 1.3 D4 决策已有但文档未显式记录

- **D4 决策**（`.omo/drafts/decision-plugin-framework-2026-06-08.md`）：Plugin-style 替代 `tick()` 风格
- **ADR-025**（Plugin 基类无 `tick`）：`PluginBase::tick() = delete`
- **ADR-037 v2.0**（Plugin 作为设计范式）：Plugin 是范式而非工具
- **ADR-040 v2.0**（CH_MEM 是新正道）：TLM 模式是 deprecated（Phase 6c 起）

但**没有一条 ADR 整合说明**：

1. Plugin-style 是 ChipForge IP 的**单一 source of truth**——不再分别为 TLM 和 RTL 写两套实现
2. CppHDL 是 Plugin-style 的**底层设施**（提供 ch 类型、lnode DAG、toVerilog、Verilator backend）
3. CppTLM 是**legacy 仿真内核**，Plugin 不再使用

本 ADR 补齐此空缺。

## 2. 决策

### 2.1 单一 source of truth：Plugin-style

**所有 ChipForge 业务 IP 必须用 Plugin-style 编写**（继承 `cf::plugin::PluginBase`，在 `build()` 里调 `pb.at_stage(name, phase, lambda)`）。

不再为同一 IP 分别维护 TLM 副本和 RTL 副本。同一份业务代码经由**两个编译模式**产出两种仿真后端：

| 模式 | 编译开关 | `cf::plugin::uint_t<N>` | 调度入口 | 仿真后端 |
|------|---------|-------------------------|---------|---------|
| **TLM**（兼容） | 默认 | POD (`uint8_t`/`uint16_t`/`uint32_t`/`uint64_t`) | `pb.run()` 每周期执行 | C++ 直仿（`pb.run()` 内联） |
| **CH_MEM**（v0.3.0+ 新正道） | `-DCF_PLUGIN_USE_CH_MEM` | `ch::core::ch_uint<N>` | `pb.elaborate(ctx)` 一次性发射 lnode DAG | ① CppHDL `Simulator::tick()` 直仿；② `ch::toVerilog(ctx)` 生成 `.v`；③ Verilator 编译+仿真 |

### 2.2 CppHDL 设施层（CH_MEM 模式后端）

CppHDL 在 CH_MEM 模式下为 Plugin-style 提供：

| CppHDL 设施 | Plugin-style 用法 |
|------------|------------------|
| `ch::core::ch_uint<N>` | 替换 POD 整数（编译期可见） |
| `ch::core::ch_bool` | 替换 POD bool |
| `ch::core::ch_reg<N>` | 同步寄存器 |
| `ch::core::ch_mem<T, N>` | SRAM 后端（`array_store` CH_MEM 模式自动切换） |
| `chlib::ch_state_machine<S, N>` | 多周期协议引擎（ADR-046 豁免） |
| `ch::core::Context` + `node_builder` | 仿真全局状态（`thread_local`） |
| `ch::core::elaborate(ctx)` + lnode DAG | `pb.elaborate(ctx)` 内部调用 |
| `ch::toVerilog(ctx)` | CH_MEM 模式输出 RTL |
| `ch::core::VerilatorBackend` | 编译 Verilog + 联合仿真 |

业务代码不直接 import `<ch.hpp>`——而是通过 `cf::plugin::uint_t<N>` / `cf::plugin::bool_t` 类型抽象访问，业务代码在两种模式下源码相同。

### 2.3 CppTLM 角色调整为 legacy

**CppTLM 仓库仍作为外部依赖**（`extern/CppTLM` 符号链接）保留：

- 已实现的 L1Cache/MMU/CPU Bridge 适配层（`src/cf_plugin/bridge/`）继续依赖 CppTLM 接口
- 任何外部 CppTLM-based 系统集成仍经由 Bridge
- CI 仍构建 CppTLM（`build/_deps/install/`），保证 Bridge 编译通过

**业务 IP 不使用 CppTLM**：

- ❌ 不继承 `cpptlm::ChStreamModuleBase` / `cpptlm::SimObject`
- ❌ 不调 `REGISTER_MODULE("TypeName", ClassName)`
- ❌ 不在 CH_MEM 业务代码里 `#include <chstream_*.hh>` / `<module_factory.hh>`
- ✅ 业务代码仅依赖 `cf::plugin::*` + `cf::plugin::storage::*` + `bundles/*.h`（共享 Bundle 类型定义）

CI 防护（`tools/verify_plugin_decision.sh` + `tools/check_plugin_portability.sh` Check 2/7）已强制此规则。

### 2.4 双文件分离约束（已 lock）

业务 IP 强制双文件分离（ADR-040 v2.0 + Check 2/7）：

- `ip/<area>/<name>/<name>.h` —— TLM 模式（永远编译）
- `ip/<area>/<name>/<name>_chmem.h` —— CH_MEM 模式（`#ifdef CF_PLUGIN_USE_CH_MEM` 守护）

CH_MEM 文件**必须**包含 `ch_*` 引用（`ch_uint`/`ch_reg`/`ch_mem`/`ch_bool`/`ch::core`）；TLM 文件**不得**包含。这保证：

1. TLM 模式编译时 `uint_t<N>` 解析为 POD，零 CppHDL 渗透
2. CH_MEM 模式编译时 `uint_t<N>` 解析为 `ch::core::ch_uint<N>`，可被 `pb.elaborate(ctx)` 发射
3. CI grep 静态检查（`check_plugin_portability.sh` Check 2/7）可机械验证

## 3. 架构总览（修正后）

```
+-------------------------------------------------------------+
|   测试套件层 (Catch2 v3.7.0)                                |
|   tests/{framework,cpu,cache,mmu,bundles,soc}               |
+-----------------------------+-------------------------------+
                              | 同一份 Plugin-style 业务代码
+-----------------------------v-------------------------------+
|   Plugin-style 业务 IP (单一 source of truth, D4 强制)     |
|   ip/{cpu,cache,mmu,memory}/plugins/<name>.{h,_chmem.h}     |
|   ├─ 业务代码用 cf::plugin::uint_t<N> / bool_t             |
|   ├─ build() 内调 pb.at_stage(name, phase, lambda)         |
|   └─ 双文件分离: <name>.h (TLM) + <name>_chmem.h (CH_MEM)  |
+-----------------------------+-------------------------------+
                              | 编译期条件展开 (uint_t<N> 抽象)
+--------------+--------------+--------------+---------------+
|              |              |              |               |
+--------------+ +------------+ +-------------+ +-------------+
| TLM 编译     | | CppHDL     | | CppHDL      | | CppHDL      |
| POD 直仿     | | Simulator  | | toVerilog   | | Verilator   |
| (pb.run())   | | tick()     | | → .v 文件    | | Backend      |
|              | |            | |              | |              |
| 默认编译     | | 需 -DCF_PLUGIN_USE_CH_MEM                  |
| ~0.6s 反馈   | | CH_MEM 模式编译后才可用                    |
+--------------+ +------------+ +-------------+ +-------------+
                              |
+-----------------------------v-------------------------------+
|   CppHDL 设施层 (chipforge 的硬件后端, 必依赖)              |
|   ch::core::{ch_uint, ch_bool, ch_reg, ch_mem, Context}    |
|   chlib::{ch_state_machine, fifo, stream, axi4lite}        |
|   ch::toVerilog / VerilatorBackend / Simulator             |
+-------------------------------------------------------------+
                              |
+-----------------------------v-----------------------------+
|   CppTLM (legacy 仿真内核, 仅 Bridge 适配层依赖)          |
|   extern/CppTLM 符号链接保留                               |
|   src/cf_plugin/bridge/{l1_cache_bridge,mmu_bridge}.cpp    |
+-------------------------------------------------------------+
```

### 3.1 与 IPC 跨 IP 通信的关系

- **Plugin 风格**: 跨阶段数据流 = `cf::plugin::Payload<T>` (key-value store, 编译期类型检查 + 运行期 typeid 二次校验)
- **CH_MEM 模式**: Payload<T> 在 `pb.elaborate(ctx)` 时展开为 lnode 引用
- **CH_MEM 多周期**: 经由 `cf::plugin::CtrlLink::halt_when/throw_when/flush_when/bypass` (ADR-033) 控制流水线暂停/异常/冲刷
- **CH_MEM 存储**: `cf::plugin::storage::array_store<T, N>` 双缓冲 commit swap (TLM 模式是 no-op)

## 4. 拒绝方案

### 4.1 拒绝"分别维护 TLM 副本 + RTL 副本"

- **理由**：违反 DRY；版本同步成本高；接口漂移已发生（L1Cache `MMUPlugin.cpp` 在 TLM 路径有 MMU bug 但 CH_MEM 路径无法复现）
- **替代**：Plugin-style 单一 source + 双模式编译

### 4.2 拒绝"CppTLM ch_stream 作为 IP 间通信"

- **理由**：D4 决策已否（2026-06-08）；ch_stream 强握手语义对单周期同步场景过度；`PipeBuilder::build()` + `Payload<T>` 已验证可替代
- **替代**：`cf::plugin::Payload<T>` + `PipeNode`

### 4.3 拒绝"TLM 模式作为长期模式"

- **理由**：ADR-040 v2.0 已 lock CH_MEM 是新正道；TLM 模式保留仅作为兼容层（Phase 1-2 已发布的代码不需要重写）
- **替代**：所有新 IP 必须有 `<name>_chmem.h` 双文件

## 5. 影响

### 5.1 文档修订

| 文档 | 修订内容 |
|------|---------|
| `docs/architecture/overview.md` | §"架构总览" ASCII 图改为 Plugin-style 单一层 + CppHDL 后端三选项；删除"CppTLM 组件层"/"CppHDL 组件层"双层描述；§"ch_stream 接口" 改为 "PayloadStore 是 IP 间通信"；§"工程目录结构" 删除 `ip/*/tlm/` + `ip/*/rtl/` 描述 |
| `AGENTS.md` | §1 顶部简介改为 "本项目用 Plugin-style 设计硬件 IP，CppHDL 提供 elaboration + Verilog + Verilator 设施"；§"## 代码组织 > ip/{name}/ 标准结构" 修订 tlm/ vs rtl/ 描述 |
| `docs/architecture/background-and-goals.md` | §1 项目简介 + §3 CppTLM 框架特性 + §5 CppHDL 框架特性 全部修订 |
| `docs/architecture/tech-selection.md` | §2 CppTLM 选型 + §3 CppHDL 选型 合并为 "CppHDL 设施选型"（CppTLM 段降到"外部依赖，bridge 适配层用"） |
| `docs/architecture/code-framework-mapping.md` | §1 总图 + §4 项目层映射 修订为"Plugin-style 是项目层；CppHDL 是设施层；CppTLM 是 legacy" |
| `docs/architecture/adr.md` | 注册 ADR-083 |

### 5.2 代码无需修改

- `cf::plugin::*` API 已有（Phase 0 + Phase 6c 双模式已就绪）
- 双文件分离约束已有（ADR-040 v2.0 + Check 2/7）
- `ch::toVerilog` / Verilator backend 已有（Phase 6d.5 E8 子集 PASS）

### 5.3 CI 验证

- `tools/verify_adr.sh` → ADR-083 注册（路径检查 `docs/architecture/adr/ADR-083-*.md` 存在）
- `tools/verify_plugin_decision.sh` → D4 Plugin-style 强制（已有，无变化）
- `tools/check_plugin_portability.sh` → ADR-040 v2.0 双文件分离 + Check 5/8（已有，无变化）

## 6. 验证记录

- **2026-10-08**：ADR 起草，本仓库 HEAD commit `a8f27b3` 实测基线 432/432 + 48/48 + 6/6 + 53/53 PASS 维持
- **CI 状态**：`architecture-gates.yml` 3 脚本 (verify_adr / verify_plugin_decision / check_plugin_portability) 0 失败

## 7. 相关文档

- [ADR-037](ADR-037-plugin-as-design-paradigm.md)：Plugin 作为设计范式（v2.0 Phase 6c M5 落地）
- [ADR-040](ADR-040-tlm-hdl-portability-constraints.md)：TLM→HDL 移植性约束 v2.0（CH_MEM 是新正道）
- [ADR-046](ADR-046-multi-cycle-fsm-exemption.md)：多周期 FSM 豁免
- [docs/methodology/plugin-style-design-methodology-v1.md](../../methodology/plugin-style-design-methodology-v1.md) v2：Plugin 声明式电路设计方法学（TLM + CH_MEM 双模）
- [docs/architecture/plugin-framework.md](../plugin-framework.md)：Plugin 框架架构
- [docs/lessons/phase-6c-elaboration-substrate.md](../../lessons/phase-6c-elaboration-substrate.md)：Phase 6c 行级教训

---

*ADR 起草日期：2026-10-08*
*ADR 起草者：Sisyphus session（基于 overview.md / AGENTS.md 文档漂移审计）*
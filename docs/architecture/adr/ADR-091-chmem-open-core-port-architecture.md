# ADR-091 — CH_MEM 开放核形态与端口架构（ip/cpu/core）

> **Status**: ✅ **Accepted（v1.2，2026-10-28，architect 主持；v1.0 = D0 决策链 D0-①~D0-⑨ 全部裁决；v1.1 = 增补 §D3a（D-J 接口契约 + 层次区分 + 握手成本）+ §D3a.3 真名；★ **v1.3 = G-1e（驱动赋值 MUST 在 ctx_swap 保护下 —— S1 实现期实测发现的第 5 条假绿通道）+ §D5.1.1 G-1d 判据补正（`is_assignable` 检不出 ⇒ 改三项判据）**；v1.2 = §D5.1 G-1 的 4 条实测约束（G-1a/b/c/d）+ PoC 证据（裸 ctx 端口机制已验证可行，2 case / 6 assertions 实测全过））**
> **上游**: ADR-088 v1.4 §10（目标态：一个模型 CH_MEM + 两个后端 + 一道差分门禁）· ADR-089 v1.4（双后端差分 parity 门禁）· ADR-083（Plugin-style 单一 source of truth）· ADR-040 v3.0（TLM→HDL 移植性约束）
> **下游**: `ip/cpu/core/`（新开放核实现）· `include/cf/plugin/pipe_builder.h`（端口声明 + 后端注入）· `ip/cpu/configs/cpu_config_chmem.h`（配置面复用）· `.rddf/roadmap/features/feat-backend-parity-gate.md`（阶段 A 载体）
> **编号说明**: `ADR-090` 已被 `openspec/changes/vexii-riscv-parity-poc` 预留（VexiiRiscv 数值 SSOT），故本 ADR 取 **ADR-091**。
> **执行文件（待 planner 落）**: `.rddf/plans/`（★ 归 builder 起草；本 ADR 只定契约）

---

## Context

### 1. 现状：CH_MEM CPU 是**封闭核** —— 顶层只有 2 个端口

| 证据 | 数据 | 取证 |
|---|---|---|
| 生成网表的顶层端口 | **只有 2 个**：`default_clock` / `default_reset` | 实测生成 `top.v`（`rv32ui-p-add`） |
| 根因 | ELF 在 **elaboration 期预载**进 `ibus_chmem.h` 的 `ch_mem<ch_uint<32>, kMemWords> imem_` + `dmem` | `ip/cpu/plugins/ibus_chmem.h:53-61, 84`；`ip/cpu/cpu_factory_chmem.h:276` |
| 插件框架的产出方式 | `PipeBuilder::to_verilog` 直接 `ch::toVerilog(filename, ctx_)` ⇒ **整个 ctx 变成 top** | `include/cf/plugin/pipe_builder.h:464-467` |
| 框架层无端口概念 | `grep -c "ch_in\|ch_out\|port<" include/cf/plugin/pipe_builder.h` = **0** | 实测 |

⇒ ★ **CPU 核无法与外部内存/设备通信 ⇒ SoC 互联不存在**。

### 2. 由此派生的 4 个真实阻塞

| # | 阻塞 | 后果 |
|---|---|---|
| **B-1** | `VerilatorBackend` 的 `port_access_` **只认顶层端口**（`type_input`/`type_output` 节点） | ⇒ 对 2 端口的核**几乎无用**；且现有 5 个观测量只能靠 `--public-flat-rw` 穿透内部层级读 `top__DOT___reg_N` / `top__DOT__dmem[0x400]`（**非契约路径**，内部节点名一变即断） |
| **B-2** | 观测走内部 hack | ⇒ 门禁的观测路径**不稳定**（内部重构即打断门禁） |
| **B-3** | 无对外接口 | ⇒ 无法接 Cache / MMU IP ⇒ SoC 级验证不存在 |
| **B-4** | 每个 ELF 一份网表（网表随 ELF 变） | ⇒ 开放项 **O-4/O-5**（每 corpus 一次编译 / ELF 移出 elaboration 期）**无解**<br>★ **O-5 的归属正是「architect 评估」（契约 §8）⇒ 本 ADR 即该评估的产出**（见 D1「形态」） |

### 3. ★ 但上游 CppHDL **已有**「带端口的核 + 多后端」的完整设施（2026-10-27 调研）

| 事实 | 取证 | 意义 |
|---|---|---|
| `ch::VerilatorBackend` **已实装**（非脚手架） | `include/core/verilator_backend.h` + `src/core/verilator_backend.cpp`（**952 行**） | 可直接用 |
| ★ `try_initialize(ctx*, data_map_t&)` **只要 context + data_map** | `src/core/verilator_backend.cpp:301-304` | ⇒ **不需要 `ch_device` / `Component` 体系** |
| ★ `Simulator(ctx*, bool trace_on)` **裸 ctx 即可构造** | `src/simulator.cpp:24-26` | ⇒ ChipForge 现有 `create_simulator()` 已是同范式 |
| ★ `generate_verilog(ctx)` 从**裸 ctx** 产 `top.v` | `src/core/verilator_backend.cpp:416-429` | ⇒ `PipeBuilder`（持裸 ctx）可直接喂 backend |
| 顶层端口 = ctx 里的 `type_input`/`type_output` 节点 | `src/codegen_verilog.cpp:669-679` | ⇒ **不需要 CppHDL 的 Component 体系**；只需在 ctx 里放端口节点 |
| `ch_in<T>` = `port<T, input_direction>` | `include/core/io.h:295-296` | 现成 API |
| `ch_stream` 有 `as_master_direction()` / `as_slave_direction()` | `include/bundle/stream_bundle.h:32-41` | ⇒ **bundle 端口按字段拆成逐字段端口** |
| 上游有双后端测试夹具 | `tests/dual_backend_fixture.h`（`DUAL_BACKEND_TEST_PAIR` 宏） | ⇒ 「同一份测试 body 跑两个后端」有现成范式 |
| 上游有 `ch_stream` 走 Verilator 的实证 | `tests/test_verilator_stream_e2e.cpp`（`[verilator][e2e][stream]`） | ⇒ `ch_stream` 可进 Verilator codegen |

### 4. 现状：两套 bundle 体系并存需求

| 现状 | 证据 |
|---|---|
| `bundles/mem_bundles.h` 是**纯 POD**（`cf::plugin::uint_t<N>`） | `bundles/mem_bundles.h:17`（自述「Phase 1.1 (TLM 模式): POD struct, 无 bundle_base 继承」） |
| `MemReq` 总宽 **144 bit** | address(64) + data(64) + is_write(1) + burst_len(8) + id(8) |
| CppHDL 的 `ch_stream<T>` 要求 T 是 bundle 类型 | `include/bundle/stream_bundle.h:19`（`bundle_base` 派生 + `CH_BUNDLE_FIELDS_T`） |
| `VerilatorBackend` **拒绝 >64-bit 端口** | `src/core/verilator_backend.cpp:631-643`（`PortBitwidthOverflow`） |

⇒ ★ **144-bit 的 `MemReq` 若整体作为一个端口 ⇒ 直接触发 `PortBitwidthOverflow`**；
⇒ ★ **但 `as_master_direction()` 按字段拆开 ⇒ 每个字段 ≤64 bit ⇒ 该限制自然消解**（**但这是设计约束，不是巧合** ⇒ MUST 写进契约，见 D4）。

### 5. 现状：CH_MEM 上的「DSE」与文档描述的不是同一件事

| 事实 | 取证 | 含义 |
|---|---|---|
| BP 表深度是**编译期常量** | `ip/cpu/plugins/branch_predictor_chmem.h:150-155`：`kBtbEntries=256` / `kPhtEntries=256` / `kGshareEntries=16` | |
| `btb_entries` 参数只是**运行期逻辑条目数**（`logical_entries_`） | 同上 `:187-203`（越界 fail-fast，合法域 = 精确集合 {16,32,64,128,256}） | ⇒ ★ **扫 `btb_entries` 不改物理表深 ⇒ 面积/时序不变 ⇒ 不是真正的设计维度** |
| 流水线级数 / 乘法延迟 / Cache 延迟 | CH_MEM 为**全组合通路**，无 stage 寄存器、无多周期乘法、Cache 未实装 | ⇒ ★ **不可配** |
| 现有 DSE 文档描述 7 维 576-config 空间 | `ip/cpu/docs/dse_architecture.md` §8 / `tools/dse/sweep_driver.py:34-42` | ⇒ 那是 **TLM 后端**的空间 |

⇒ ★★ **CH_MEM 上真正可扫的维度 = 「存储深度 / 表容量」类**（`kBtbEntries` / `kPhtEntries` / `kGshareEntries` / `kMemWords` / `kXlenBits`）⇒ 与文档描述的「微架构 DSE」**不是同一件事** ⇒ MUST 正名（MUST NOT 让读者期待流水级数/超标/多周期）。

---

## Decision

### D1 ★ 核形态：**新增开放核**（不复用预载核，不改 `_chmem.h`）

| 项 | 决策 |
|---|---|
| **形态** | ★ **开放核**（P1）：对外暴露 `ch_stream` 端口，取指/访存由外部供给<br>★ **本 ADR 同时是开放项 O-5 的架构解**（契约 §8：「让 Verilator 侧免掉『每 ELF 一次编译』：**把 ELF 从 elaboration 期移出**（imem 初始化改运行期加载）」，归属「architect 评估」）⇒ 开放核的 ELF 走**端口**而非 elaboration 期预载 ⇒ 网表不再随 ELF 变 ⇒ **O-4/O-5 一并解开** |
| **实现位置** | ★ **`ip/cpu/core/`**（新目录）—— 形态差异**由目录表达**，不用版本号 |
| **预载核** | ★ **原样不动**（`ip/cpu/plugins/*_chmem.h` + `cpu_factory_chmem.h`）⇒ 现有 52 case parity 基线**零风险** |
| **退役** | 预载核的移除**待开放核通过全部测试后另行裁决**（★ 归 ADR-088 阶段 C，**不在本 ADR**） |
| **命名** | ★ **不用 `_v2.h`** —— `v2` 是版本号语义，与「长期共存的两形态」矛盾 ⇒ 用语义化命名（`ibus_port_chmem.h` / `dmem_port_chmem.h` / `cpu_core_chmem.h`） |

⇒ **文件布局**：

```
ip/cpu/core/
├── cpu_core_chmem.h          # 开放核装配（对标 cpu_factory_chmem.h）
├── bus_iface_chmem.h         # ★ CH_MEM 版 MemReq/MemResp bundle（纯 ch::core::ch_uint）
├── ibus_port_chmem.h         # 取指：外部端口驱动（替代 ibus_chmem.h 的预载源）
├── dmem_port_chmem.h         # 访存：外部端口驱动
└── verify_mem_model_chmem.h  # ★ 验证实现（内存模型 + ELF 加载）—— ip 级单测用
```

★ **复用（形态无关，直接共享）**：`ip/cpu/arch/riscv/decoder_table.h`（ISA 解码表）· `ip/cpu/arch/riscv/int_alu_chmem.h`（ALU 语义）· `ip/cpu/core/payload_common.h` + `ip/cpu/arch/riscv/payload_riscv.h`（★ **payload key 契约，必须共享，否则新旧核语义漂移**）。

### D2 ★ 验证实现的角色：**slave（内存模型）**，通过标准端口响应请求

> 用户裁决 D0-①：**读法 A**。

```
┌────────────────────────┐
│  开放核（ip/cpu/core）   │
│  imem_req/rsp · dmem_req/rsp │
└───────────┬────────────┘
            │ ch_stream<MemReq> / ch_stream<MemResp>
            ▼
┌────────────────────────────────────────────┐
│  从设备（slave）                             │
│  ip 级单测 ⇒ verify_mem_model_chmem.h（验证实现）│
│  SoC 级   ⇒ 真实 Cache / MMU IP              │
└────────────────────────────────────────────┘
```

⇒ ★ **两种场景角色对称**（都是 slave）⇒ 替换实现即得 SoC。
⇒ ★ **验证实现 MUST 提供 ELF 加载**（imem 侧）+ 响应 dmem 读写请求。

### D3 ★ 端口契约：`ch_stream<CpuBusReq>`（标准 bundle）

| 项 | 决策 |
|---|---|
| 端口形态 | ★ `ch::core::ch_stream<CpuBusReq>` / `ch_stream<CpuBusResp>`（**CppHDL 原生 bundle**）<br>★ **真名 = `CpuBusReq` / `CpuBusResp`**（D-B-2 裁决）⇒ ★ **不用 `MemReqBundle`** —— 该名字在代码里**不存在**（实际是 `cf::bundles::MemReq`）⇒ 沿用它会**继承悬空引用** |
| 方向 | imem/dmem 请求 = 核为 **master**；响应 = 核为 **slave**（按 `as_master_direction()` / `as_slave_direction()`） |
| 观测 | ★ **必须走顶层 `ch_out`**（上游 `rv32i_pipeline_top.h` 铁律：「观测 MUST 走顶层 `ch_out`（verilator 只能读顶层端口）」）⇒ `tohost` / `pc` / `regs` / `memsig` 全部从顶层端口读 ⇒ **退休 `--public-flat-rw` 穿透内部层级的读法** |
| 旧 `bundles/mem_bundles.h` | ★ **不动**（POD 保留给预载核 + `src/cf_plugin/bridge/` + TLM 业务代码） |

### D3a ★ D-J：`ch_stream` 的层次区分与接口契约（★ 增补，2026-10-28）

> 依据 **ADR-092 §D4**（`ch_stream` 层次区分）+ **ADR-083 v1.2 §4.2**（拒绝范围收窄）。

#### D3a.1 ★ 层次区分（★ MUST NOT 误删 `Payload<T>`）

| 机制 | 作用域 | 语义 | 决策 |
|---|---|---|---|
| `Payload<T>` + `PipeNode` | ★ **核内 stage 之间** | 「跨 PipeNode 隔离（每个 PipeNode 有自己的 PayloadStore）」（`include/cf/plugin/payload.h:10`）⇒ 无握手，同拍数据通路 | ★ **保留不动** |
| `ch_stream<T>` | ★ **IP 之间 / 核 ↔ 从设备** | `valid`/`ready`/`fire()`/`isStall()` ⇒ **有握手**的接口协议 | ★ **采用** |

⇒ ★ **两者层次不同，不是竞争关系** ⇒ ★ **MUST NOT** 以「采用 `ch_stream`」为理由删除 `Payload<T>`。

#### D3a.2 ★ 握手成本（★ MUST NOT 默认「握手免费」）

ADR-083 §4.2 当初拒绝的理由之一是「强握手语义对单周期同步场景过度」⇒ ★ 跨 IP 接口采用 `ch_stream` 时，该成本**由接口契约显式承担**：

| 项 | 决策 |
|---|---|
| `ready` 语义 | ★ **从设备可恒真**（`ready` 恒 1 ⇒ 退化为无阻塞流）⇒ ★ 单周期同步场景**不被握手拖慢** |
| `valid` 语义 | ★ 请求方**必须**在 `valid=1` 时保持 `payload` 稳定（⇒ 契约测试 MUST 覆盖） |
| 背压 | ★ 若从设备无法在本拍响应 ⇒ MUST 拉低 `ready` ⇒ ★ **CPU 侧 MUST 有 stall 机制**（★ 与 ADR-091 D8 S3 的外部驱动数据通路耦合） |
| 判据 | ★ 握手成本 MUST 以「`ready` 恒真 vs 可拉低」两档**分别**测时序（★ 不得只测一档） |

#### D3a.3 ★ 接口契约（`CpuBusReq` / `CpuBusResp`）

| 项 | 决策 |
|---|---|
| 载荷类型 | ★ `CpuBusReq` / `CpuBusResp`（**新文件** `ip/cpu/core/bus_iface_chmem.h`，纯 `ch::core::ch_uint` + `bundle_base` 派生 + `CH_BUNDLE_FIELDS_T`） |
| 字段宽度 | ★ **每字段 ≤ 64 bit**（ADR-091 H-1；★ `address`(64) / `data`(64) 恰好卡在上限 ⇒ **禁止再加宽**） |
| 方向 | 请求 = 核为 **master**（`as_master_direction()`）· 响应 = 核为 **slave**（`as_slave_direction()`）⇒ ★ 逐字段展开 ⇒ 144-bit 总宽**不触发** `PortBitwidthOverflow` |
| 与老核 bundle | ★ **无耦合**（老核的 `cf::bundles::MemReq` 保持 POD 不动 ⇒ ADR-092 §D-E 推迟到 Phase-2） |
| 命名一致性 | ★ **MUST NOT** 复用 `MemReqBundle`（悬空引用）；★ 老核 JSON 里的 `"bundle": "MemReqBundle"` 修正归 ADR-092 §D7 / 本节配套文档同步 |

### D4 ★ 两套 bundle 并存（新核用新 bundle，老核代码不动）

> 用户裁决 D0-⑨ + D0-③。

| bundle | 位置 | 字段类型 | 消费者 |
|---|---|---|---|
| ★ **新（CH_MEM-only）** | `ip/cpu/core/bus_iface_chmem.h` | `ch::core::ch_uint<N>` + `bundle_base` 派生 + `CH_BUNDLE_FIELDS_T` | 新开放核 |
| 旧（POD，双模） | `bundles/mem_bundles.h` | `cf::plugin::uint_t<N>` | 预载核 + bridge + TLM 业务代码 |

⇒ ★★ **项目内部自证**：新核的 bundle 形态 = ★ **`bundles/README.md` 自己写的 Phase 5 计划**（不是本 ADR 的发明）：

| `bundles/README.md` 原文 | 行 | 与本 ADR 的关系 |
|---|---|---|
| 「Phase 5 (RTL) \| BundleMapper 转换为 `ch_uint<N>` + `bundle_base<T>` 派生」 | `:103` | ⇒ 新核 = 执行该计划 |
| 「RTL 模式 (Phase 5) 引入 `as_master/slave_direction()`」 | `:21` | ⇒ 新核端口的方向语义 = 该计划 |
| 「Bundle 形态分阶段：Phase 1: POD / **Phase 5: `ch_uint<N>` + `bundle_base`** / Phase 6: 自动 codegen」 | `:22` | ⇒ 形态演进表 |

⇒ ⇒ **Phase 5 计划因「预载核」而一直未触发**；本 ADR 让**新核**先行落地 Phase 5 形态，★ **老核的 Phase 1 形态保留 ⇒ 两阶段并存**（⇒ 老核退役时归阶段 C 收口）。

⇒ ★ **老核 bundle 的单模化（`bundle_base` 化）归 ADR-092 Phase-2**（★ 本 ADR D5 的 G-3 门禁之一）⇒ ★ **本 ADR 期间老核 bundle 保持 POD 不动**。

⇒ ★ **「移除 TLM 兼容」在**新核范围内**生效，**不在框架层生效**：
- `include/cf/plugin/uint_t.h` 的模态分支 **保持不动**（预载核需要）
- `bundles/` **保持不动**（老核 + bridge 需要）
- ★ 框架层与 `bundles/` 的真正清理 **归 ADR-088 阶段 C**（老核退役时）

⇒ **代价（诚实登记）**：两套 bundle 并存 ⇒ 长期漂移风险 ⇒ 缓解见 D6 的等价性自检。

### D5 ★ 插件框架层的 3 项缺口（★ 全部是**新增**，不改现有语义）

| # | 缺口 | 落点 | 规模 |
|---|---|---|---|
| **G-1** | **端口声明能力**：`PipeBuilder` 无 `ch_in`/`ch_out` API | `include/cf/plugin/pipe_builder.h` 新增（★ 4 条实测约束见 **D5.1**） | 1~2 人日 |
| **G-5a** | **后端注入**：`create_simulator()` 无后端参数 | 新增 `create_simulator(std::unique_ptr<ch::IEvalBackend>)`（★ `nullptr` = 保持现有默认行为 ⇒ 向后兼容） | 0.5 人日 |
| **G-4** | CH_MEM 版 bundle 定义 | `ip/cpu/core/bus_iface_chmem.h`（新）⇒ ★ 真名 `CpuBusReq`/`CpuBusResp`（D3a.3） | 1~2 人日 |

⇒ ★ **框架层仍不引入「配置面」**（G-2）：配置归 `ip/cpu` 层（复用已落地的 `CpuConfigChmem`，见 `.rddf/improvements/chmem-cpu-json-config.md`）。
⇒ ★ **框架层仍不引入「DSE 维度抽象」**（G-3）：归 `ip/cpu/core/`（D7）。

### D5.1 ★ G-1 的 4 条**实测**约束（★ 2026-10-28 PoC 得出，非推断）

> **PoC 证据**：`tests/framework/test_pipe_builder_ports.cpp`（2 case / 6 assertions，**实测全过**）
> ⇒ 验证「裸 ctx + `ch_in`/`ch_out` ⇒ 网表顶层端口」可行 ⇒ ★ **不需要 CppHDL 的 Component / `ch_device` 体系**。

| # | 约束 | 依据（实测） | 违反后果 |
|---|---|---|---|
| **G-1a** | ★ 端口 API MUST 置于 **`#ifdef CF_PLUGIN_USE_CH_MEM`** 内 | 框架头现有 CppHDL 依赖**全部**条件化（`uint_t.h:25` · `payload.h:34` · `ctrl_link.h:39`）⇒ TLM 模式不得引入 | TLM 模式编译失败 |
| **G-1b** | ★ 框架头 **MUST NOT include `<ch.hpp>`** ⇒ 只 include 具体头（`core/io.h` · `core/uint.h` · `core/context.h`） | ★ `ch.hpp:10` include `module.h`，而 ★ `module.h:13` 的 `build_hierarchical_name` **缺 `inline`** ⇒ **任何 2+ TU include `<ch.hpp>` 即链接失败**（multiple definition）★ 实测触发 | ★ **新核多文件必然踩雷**（每个头都含 `ch.hpp` ⇒ 链接失败） |
| **G-1c** | ★ 端口创建 MUST **fail-closed 断言 ctx 非空** | ★ `ch_logic_in` 构造是 `if (ctx) { ctx->create_input(...); }` ⇒ ★ **无 current context 时静默不建节点**（`impl() == nullptr`）★ 实测确认 | ★ **假绿通道**：端口「消失」而不报错 ⇒ 网表少端口 ⇒ 门禁照绿 |
| **G-1d** | ★ 端口**方向必须严格**：请求用 `ch_out`，响应用 `ch_in`；且 ★ `ch_in` **MUST NOT** 被内部逻辑赋值 | ★ 有内部 driver 的 `ch_in` **不是**顶层端口（上游 `verilator_backend.cpp:617` 的 skip：`inputimpl::driver() != nullptr` ⇒ 不进 `port_access_`；codegen 侧同理） | 端口**不进** `port_access_` ⇒ Verilator 后端**读不到** ⇒ 观测量采集失败 |
| **G-1e** | ★ **端口的驱动赋值 MUST 在 `ctx_swap` 保护下做** | ★ 实测：赋值在保护外 ⇒ `[ERROR] No active context for literal creation` ⇒ `ch_logic_out::operator=` 报「output_node_ or src_lnode is null」⇒ ★ **output 端口进了网表但无 driver ⇒ 值未定义/X** | ★ **假绿通道**：端口「在但无值」⇒ 仿真读到 X ⇒ 门禁照绿 |

⇒ ★★ **G-1a/b 是「框架头纪律」**（与 ADR-040 的 CH_MEM-only 纪律同族）；★ **G-1c/d/e 是「假绿通道」**（与 ADR-089 的 fail-closed 纪律同族）⇒ ★ MUST 逐条机检。

#### D5.1.1 ★ G-1d 的判据补正（★ 实测）

★ `ch_in` 的「值赋值不可用」**无法**用 `std::is_assignable` 检出 —— 其 `operator=` 是**模板** + `static_assert` 在**函数体内**（`io.h:239-243`）⇒ 签名匹配 ⇒ `is_assignable_v` 报 **true** ⇒ ★ 真正的阻止发生在**实例化时**（编译错误）⇒ ★ 本仓库的判据改为三项：

| 判据 | 形式 |
|---|---|
| ① `ch_out` 可赋值 | 编译期 `static_assert(is_assignable_v<ch_out&, ch_uint>)` |
| ② `ch_in` 值赋值编译失败 | 由 CppHDL 自身的实例化期 `static_assert` 保证（★ 本仓库不写会失败的编译） |
| ③ ★ **行为判据**：`ch_in` 被内部驱动（`inputimpl::set_driver`）⇒ **MUST NOT** 出现在网表顶层端口 | 测试 case E（★ 实测通过） |

#### D5.1.1 ★ PoC 实测的网表形态（★ 开放核的接口目标形态）

```verilog
module top (
    input default_clock,
    input default_reset,
    input [31:0] poc_in,      ← ★ 新增顶层 input (响应侧)
    output [31:0] poc_out     ← ★ 新增顶层 output (请求侧)
);
```

⇒ ★ 从「2 端口封闭核」变为「**N 端口开放核**」⇒ ★ 这是 ADR-091 §B-1/B-2/B-3 三个阻塞的**共同解锁点**。

### D6 ★ 硬约束（★ MUST，全部为 fail-closed）

| # | 约束 | 依据 |
|---|---|---|
| **H-1** | ★ **每个端口字段 MUST ≤ 64 bit**（`PortBitwidthOverflow`）⇒ `MemReq.address`(64) / `MemReq.data`(64) **恰好卡在上限** ⇒ **禁止再加宽**（如 128-bit 地址）⇒ 需加宽时 MUST 拆字段或走解释器后端 | `verilator_backend.cpp:631-643` |
| **H-2** | ★ **观测 MUST 走顶层 `ch_out`** ⇒ MUST NOT 依赖 `--public-flat-rw` 读内部层级 | 上游 `rv32i_pipeline_top.h` 注释 |
| **H-3** | ★ **开放核与预载核 MUST 有机器可判的等价性自检**：同一 ELF 分别跑两核，**5 项观测量逐位相等** ⇒ 否则两核就是 ADR-083 §4.1 拒绝的「双副本漂移」 | ADR-083 §4.1 + ADR-089 观测量集 |
| **H-4** | ★ **新核 MUST NOT 引入 TLM 依赖**（不引 `cf::plugin::uint_t`、不引 `cpptlm`）⇒ `ip/cpu/core/**` 全部文件 MUST 为 CH_MEM-only | ADR-040 v3.0 + ADR-088 §10 |
| **H-5** | ★ **每个顶层端口字段 MUST 在两个后端上都能读到** ⇒ 门禁 MUST 以「顶层端口」为唯一观测量来源 | ADR-089 D2 |

### D7 ★ DSE 的定位与可扫维度清单

| 项 | 决策 |
|---|---|
| 命名 | ★ **正名为「表容量 / 存储深度的参数敏感度分析」**（而非「设计空间探索」）⇒ MUST NOT 让读者期待流水级数 / 超标 / 多周期乘法 / Cache 延迟（★ 这些在 CH_MEM 上**不可配**） |
| 可扫维度（★ 全部是 `static constexpr`，改它 ⇒ 面积/时序真变） | `kBtbEntries`（256）· `kPhtEntries`（256）· `kGshareEntries`（16）· `kMemWords`（16K，⚠️ 节点爆炸风险，ADR-085-D17 已警示）· `kXlenBits`（32） |
| 不可扫维度 | 流水线级数（CH_MEM 固定 5 级全组合）· 乘法延迟（组合式）· Cache 延迟（Cache 未实装）· `xlen=64`（ADR-087 fail-fast：isa=rv32im 要求 xlen=32 ⇒ ⚠️ `tools/dse/sweep_driver.py:38` 的默认空间含 `xlen:[32,64]` ⇒ **约 288/576 config 注定失败**） |
| sweep 引擎 | 归 P5（`tools/dse/sweep_driver.py` 加 `--backend chmem`），**不在本 ADR 范围** |

### D8 ★ 分阶段实施路径

| 阶段 | 内容 | 前置 | 备注 |
|---|---|---|---|
| **S1** | 框架层 G-1（端口声明）+ G-5a（后端注入） | — | ★ 独立可验收 |
| **S2** | `ip/cpu/core/bus_iface_chmem.h`（G-4）+ `cpu_core_chmem.h` 装配骨架 | S1 | |
| **S3** | `ibus_port_chmem.h` / `dmem_port_chmem.h`（外部驱动数据通路）+ 顶层观测端口（H-2） | S2 | |
| **S4** | `verify_mem_model_chmem.h`（验证实现：ELF 加载 + 内存模型） | S3 | ★ ip 级单测的落点 |
| **S5** | ★ **双后端测试**（D0-c：解释器 + Verilator **都必过**）+ H-3 等价性自检 | S4 | ★ 判据 = 上游 `DUAL_BACKEND_TEST_PAIR` 范式 |
| **S6** | SoC 级接线（接真实 Cache/MMU IP） | S5 + MMU/Cache CH_MEM 实装 | ★ **依赖外部实装**（MMU PTW 未落地、Cache 主插件不存在）⇒ **不在近期窗口** |

⇒ ★ **S1~S5 覆盖 D0 的全部范围**（ip 级单测 + 开放核 + 双后端）⇒ ★ **S6（SoC）留待 MMU/Cache CH_MEM 实装后**。

### D9 ★ 明确**不**在本 ADR 范围内

- ✗ **预载核的移除 / TLM 兼容的全局清理** ⇒ 归 ADR-088 阶段 C（★ 5 条硬前置未满足）
- ✗ **`bundles/mem_bundles.h` 的 bundle 化** ⇒ 归阶段 C（本 ADR 用新 bundle，见 D4）
- ✗ **框架层 `uint_t.h` 模态分支的删除** ⇒ 归阶段 C
- ✗ **`check_plugin_portability.sh` 的 Check 2/7 改单模语义** ⇒ 归 ADR-088 硬前置⑤（★ 防「真空 PASS」）
- ✗ **MMU PTW / L1Cache 的 CH_MEM 实装** ⇒ 归 `mmu-chmem-phase-b-c-launch` / `verilator-l1cache-e2e-coverage`
- ✗ **DSE sweep 引擎换后端** ⇒ 归 P5
- ~~✗ **`ch_stream` 表述纠偏**~~ ⇒ ✅ **已完成（ADR-083 v1.2，2026-10-28）**：§1.2 架构图 + §4.2 拒绝范围 + Status/来源/关联 + §5.1 文档修订表<br>★ **余下**：`AGENTS.md` + `docs/architecture/*` 12 文件 + `docs/GLOSSARY.md` 的**实际文本同步** + ★ `MemReqBundle` 悬空引用修正（10 文档 + 4 JSON）⇒ 落点 = ADR-092 §D7 + 本 ADR D3a.3

---

## Consequences

### 正面

1. ★ **B-1/B-2/B-3/B-4 四个阻塞一次性解开**：顶层端口 ⇒ `VerilatorBackend` 可用 + 观测走契约 + SoC 互联有路 + O-4/O-5 有解
2. ★ **零回归**：预载核不动 ⇒ 现有 52 case parity 基线**不需重跑**（★ 这是「不原地改 `_chmem.h`」的直接收益）
3. ★ **D0-c（双后端必过）落地**：上游 `DUAL_BACKEND_TEST_PAIR` 范式可直接借鉴；`VerilatorBackend` 只需 `ctx` ⇒ **不引入 CppHDL 的 Component 体系**
4. ★ **H-3 等价性自检**把「两核语义漂移」变成机器可判 ⇒ 规避 ADR-083 §4.1 拒绝的「双副本漂移」
5. ★ **DSE 正名**消除「以为能扫微架构、实际不能」的期望落空 ⇒ 「绿≠对」的第二类风险被堵

### 负面 / 成本

1. ⚠️ **两套 bundle 并存**（D4）⇒ 长期漂移风险
   ⇒ 缓解：H-3 等价性自检 + 新核 bundle 的 `CH_BUNDLE_FIELDS_T` 契约测试
2. ⚠️ **两套 CPU 核共存**（预载 + 开放）⇒ 维护面翻倍
   ⇒ 缓解：★ **共享 `arch/riscv/`（ISA 语义）+ `core/payload_common.h`（payload 契约）** ⇒ 真正分叉的只有「数据通路 + 装配」
   ⇒ 退役时机归阶段 C（D9）
3. ⚠️ **框架层新增 2 个 API**（G-1 / G-5a）⇒ 触及 D4 架构门禁（`check_plugin_portability.sh`）
   ⇒ 缓解：★ **纯新增**（不改现有语义）⇒ `create_simulator(nullptr)` 保持原行为
4. ⚠️ **S1~S5 估算 6~10 人日**（不含 S6）⇒ 超出当前 v0.11.0 窗口，与 A4 / BP 增量 3 争人力（roadmap R5）
5. ⚠️ **H-1 的 64-bit 天花板** ⇒ 地址/数据宽度被锁死在 64 bit

---

## Alternatives considered

| 备选 | 结论 | 理由 |
|---|---|---|
| **A. 维持封闭核**（不改） | ❌ 拒绝 | B-1~B-4 四个阻塞全部保留 ⇒ `VerilatorBackend` 用不上、SoC 不存在、O-5 无解 ⇒ D0 的目标全部落空 |
| **B. 原地改 `_chmem.h` 改成开放核** | ❌ 拒绝 | ★ 预载核的 52 case parity 基线**全部作废**（须重跑 ≈50 min + 重录基线）⇒ 且失去「两形态可对照」的能力 |
| **C. 新建顶层 IP 目录（如 `ip/cpu-open/`）** | ❌ 拒绝 | 违反 `ip/{name}/` 规范（AGENTS.md）；跨 IP 复用 `arch/riscv/` 需跨目录引用；scope 矩阵要改 |
| **D. 命名用 `_v2.h`** | ❌ 拒绝 | ★ `v2` 是**版本号**语义，与「两形态长期共存」矛盾 ⇒ 命名会暗示替代关系（而实际是共存）⇒ 形态差异应由**目录**表达 |
| **E. 现在就全面删 TLM 兼容**（`uint_t.h` 模态 + `bundles/` bundle 化 + bridge） | ❌ 拒绝 | ★ **= 提前启动 ADR-088 阶段 C**，而其 5 条硬前置未满足（尤其⑤「门禁体系自身改版」未做 ⇒ `check_plugin_portability.sh` Check 2/7 会「**真空 PASS**」= ADR-088 点名的最坏假绿形态） |
| **F. 用 ChipForge 自己的 POD bundle（`bundles/mem_bundles.h`）作端口** | ❌ 拒绝 | ★ ① 不是 CppHDL bundle ⇒ `ch_stream<T>` 不接受 ② POD 无 `bundle_base` ⇒ 无法走 `as_master_direction()` ③ 与「标准接口」目标矛盾（用户裁决 D0-③ 明示「借鉴 CppHDL 设计 CH_MEM bundle」） |
| **G. 引入 CppHDL 的 `Component`/`ch_device` 体系** | ❌ 拒绝 | ★ 实测 `try_initialize(ctx*, data_map_t&)` 只要 ctx + data_map（`verilator_backend.cpp:301-304`）⇒ `PipeBuilder`（持裸 ctx）可直接喂 ⇒ **引入组件体系是纯额外成本** |
| **H. 沿用 `tools/dse/sweep_driver.py` 的 7 维 576-config 空间** | ❌ 拒绝 | ★ 那是 **TLM 后端**的空间；CH_MEM 上 6/7 维不可配，且默认空间含注定失败的 `xlen:[32,64]`（ADR-087 fail-fast）⇒ 已由 D7 正名 + 收窄 |

---

## Verification

- [ ] **V-1** ★ **端口契约生效**：新核生成的网表顶层端口**含** `imem_req`/`imem_rsp`/`dmem_req`/`dmem_rsp` 的逐字段端口 + `tohost`/`pc` 等观测端口 ⇒ **且每个字段 ≤ 64 bit**（H-1）
- [ ] **V-2** ★ **双后端必过**（D0-c）：★ **同一份测试 body** 在**解释器后端**与 **Verilator 后端**下**都必须 PASS** ⇒ 判据 = 上游 `DUAL_BACKEND_TEST_PAIR` 范式（`tests/dual_backend_fixture.h`）⇒ ★ Verilator 侧 **WARN-skip 不算 PASS**（MUST 显式 SKIP 而非静默跳过）
- [ ] **V-3** ★ **等价性自检**（H-3）：同一 ELF 跑**开放核**与**预载核** ⇒ **5 项观测量（tohost / regs / memsig / cycles / pchash）逐位相等** ⇒ ★ 变异对照：★ 故意改开放核的 1 处语义 ⇒ 该自检 MUST RED（证明它有鉴别力，不是「两边都没跑就绿」）
- [ ] **V-4** ★ **H-2 生效**：门禁的 5 个观测量**全部**从**顶层端口**读取 ⇒ ★ `--public-flat-rw` 读内部层级的路径**退休**（新核路径上）
- [ ] **V-5** ★ **H-4 生效**：`ip/cpu/core/**` 全部文件**零** `cf::plugin::uint_t` / `cpptlm` 引用（★ 建议加 CI grep 检查）
- [ ] **V-6** ★ **零回归**：★ 现有 52 case parity **基线不变**（预载核不动 ⇒ 报告应与 `openspec/changes/backend-parity-corpus/first-run-evidence.md` 的 A3 首跑**逐位一致**）⇒ ★ D4 架构门禁（`check_plugin_portability.sh` / `verify_plugin_decision.sh`）全绿
- [ ] **V-7** ★ **DSE 正名生效**：`ip/cpu/docs/dse_architecture.md` 与 `tools/dse/README.md` 的表述与 D7 一致（★ 「表容量/存储深度的参数敏感度分析」；★ 明确列出**不可配**维度）⇒ ⚠️ **不得**仍以「7 维 576-config」描述 CH_MEM 路径

---

## Related

- **ADR-088 v1.4 §10**（目标态与三段迁移路径）—— 本 ADR 是 §10 阶段 C 的**先行准备**，但**不**替代阶段 C 的 5 条硬前置
- **ADR-089 v1.4**（双后端差分 parity 门禁）—— 观测量集与 H-3 等价性自检的判据来源
- **ADR-083 v1.2**（Plugin-style 单一 source of truth）—— ★ §4.1「拒绝双副本」是 H-3 的由来；★★ **v1.2 已完成表述纠偏**（§1.2 架构图 `ch_stream` 归类 + §4.2 拒绝范围收窄）⇒ ★ 本 ADR D3a 的直接依据
- **ADR-092**（TLM 参考实现的冻结与演进停止）—— ★ §D4（`ch_stream` 层次区分）· §D5（Phase-1 门禁）· §D6（Phase-2 准入门禁）· §D-E（老核 bundle 单模化推迟到 Phase-2）
- **ADR-040 v3.0**（TLM→HDL 移植性约束）—— H-4 的依据
- **ADR-085-D17**（CH_MEM 容量上限 / 节点爆炸）—— `kMemWords` 维度的风险约束
- **ADR-087**（ISA 声明真实性）—— `xlen=64` fail-fast 的依据
- `docs/architecture/parity-gate-contract.md` **v1.8** —— 观测量与门禁契约（★ v1.7 补 `--cpu-config` 落点；★ v1.8 S-15 改 **per-case 超时推导**，修「全局单一超时 ⇒ branchbench 假红」）
- `.rddf/improvements/chmem-cpu-json-config.md` —— 配置面（`CpuConfigChmem`）已落地，本 ADR 复用
- `.rddf/roadmap/features/feat-backend-parity-gate.md` —— 阶段 A 载体
- `openspec/changes/backend-parity-corpus/first-run-evidence.md` —— V-6 的基线来源
- 上游：`/workspace/project/CppHDL/include/core/verilator_backend.h` · `src/core/verilator_backend.cpp` · `src/codegen_verilog.cpp` · `include/bundle/stream_bundle.h` · `tests/dual_backend_fixture.h` · `examples/riscv-mini/src/rv32i_pipeline_top.h`

# ADR-091 — CH_MEM 开放核形态与端口架构（ip/cpu/core）

> **Status**: ✅ **Accepted（v1.2，2026-10-28，architect 主持；v1.0 = D0 决策链 D0-①~D0-⑨ 全部裁决；v1.1 = 增补 §D3a（D-J 接口契约 + 层次区分 + 握手成本）+ §D3a.3 真名；★ **v1.9 = C2 逐条变异对照复核（★ 存量 G-1a~G-1k 共 11 条 ⇒ ★ **无一被证伪** ⇒ ★ 但 ★ **机检覆盖度极不均衡**（★ 强 7 / 中 1 / 弱 3）⇒ ★ 增「G-1 的机检覆盖矩阵」+ ★ **更正 G-1b 的后果描述**（★ 实测 = **命名冲突**，★ 非 ADR 原写的 multiple definition））**；
★ **v1.11 = A2 收尾审查 + `memsig` 观测来源裁决（2026-10-29，用户裁决 Q1/Q4 + architect 主持）：★ 新增 §D3a.6「`memsig` 观测来源 = **从设备自算**」（★ 节点名沿用 `mem_sig`/`mem_sig_updates` ⇒ ★ **parity runner 零改动**；★ 折叠算法 MUST 与预载核 `dmem_chmem.h:244-270` **逐位同款**；★ **从设备 MUST 提供**（强契约）⇒ ★ ADR-089 例外白名单**零新增**）+ ★ **新增硬约束 G-1o**（★ **顶层端口名 MUST NOT 与内部 `ch_reg`/`ch_node` 名同名** ⇒ ★ 否则 CppHDL 唯一化把端口改名（`open_pc` → `open_pc_1`）⇒ ★ **两后端端口名不一致** ⇒ ★ 破 H-5）+ ★ Verification **V-11~V-15**（5 条）；
★ **v1.10 = A2（分支重定向 + flush，S3.2b）实施期收敛：★ 新增 §D3a.5「分支反馈契约」（3 个核内 payload key + **1 拍对齐** + fail-open）+ ★ **新增硬约束 G-1n**（★ **stage 首现序 MUST 等于流水顺序** —— `canonical_stage_order()` 按首现序 ⇒ ★ 装配/测试注册顺序错 ⇒ **跨 stage payload 可见性反转** ⇒ 静默失效）+ ★ Verification **V-10**（4 个变异对照）；
★ **v1.8 = A1（访存端口化）实施期实测收敛：★ 新增 **2 条**硬约束（G-1l「累加式 select 需上游先写」+ G-1m「`ch_bool` 禁用于 `?:`」）+ ★ **G-1c 修正**（fail-closed 由 `throw` 改为 `std::unexpected` ⇒ 合 ADR-047 Check 10）+ ★ **访存时序模型澄清**（★ `ch_reg` 的 next 是组合表达式 ⇒ 端口呈现的是**上一拍末**算出的地址）+ ★ **诚实标注 4 条被变异对照证伪的怀疑**（★ `&&`/`||` 失效 / `==` 走指针比较 / 跨宽构造须 `zext` / ctx 名必须唯一 ⇒ ★ **均不成立** ⇒ ★ **不作为硬约束**）⇒ ★ 推导与证据见 `.rddf/plans/backend-parity-open-core.md` §K17；
★ **v1.7 = A1 实施期发现并裁决「写语义」缺口：★ `CpuBusReq` MUST 增 `size`（3 bit，访问字节宽度）⇒ 写由**从设备**按 byte-strobe RMW ⇒ ⇒ ★ CPU 侧不依赖读响应 ⇒ 写**无延迟**（★ 代价远小于「CPU 侧 RMW ⇒ stall/forwarding」）；★ 同时补齐 `burst_len` 字段登记**；★ **v1.6 = A0 访存时序裁决（★ 2026-10-28 用户裁决 + architect 主持）：★ 访存端口 = 「地址提前 1 拍发出 + 数据侧组合消费 + 写请求同拍发出」⇒ 读/写数据时点与预载核同拍等价 ⇒ ★ 不需要 stall / forwarding；★ ★ 连带修正 H-3（5 观测量逐位相等 ⇒ **3 终值逐位相等 + 2 轨迹观测量走 ADR-089 例外白名单**）+ 新增硬约束 G-1k（访存地址 MUST 提前 1 拍）**；★ **推导与证据见 `.rddf/plans/backend-parity-open-core.md` §K13~K15（★ 正文只留决策）**；★ **v1.5 = G-1h/G-1i/G-1j（S3 实现期实测发现的第 8/9/10 条假绿通道：★ `elaborate` 不做 ctx_swap · ★ plugin 成员初始化器创建字面量 · ★ 漏调 `build()` ⇒ 全部逻辑静默不执行）+ ★ 框架级加固（`elaborate` 自建 ctx_swap + 自动 build）**；v1.4 = G-1f/G-1g（S2 实现期实测发现的第 6/7 条假绿通道：bundle 字段禁带字面量初始化器 + 方向设置须在 ctx 保护内；★ 连带发现老核 `bundles/mem_bundles.h` 有同样隐患）；v1.3 = G-1e（驱动赋值 MUST 在 ctx_swap 保护下 —— S1 实测发现的第 5 条假绿通道）+ §D5.1.1 G-1d 判据补正（`is_assignable` 检不出 ⇒ 改三项判据）；v1.2 = §D5.1 G-1 的 4 条实测约束（G-1a/b/c/d）+ PoC 证据（裸 ctx 端口机制已验证可行，2 case / 6 assertions 实测全过））**
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
| 背压 | ★ **v1.6 降级**（★ A0 裁决连带）：★ 访存正确性**不依赖** stall（★ 写**无延迟** ⇒ 无 load-after-store 问题 ⇒ 见 §D3a.4）⇒ ★ stall 仅在「外部从设备**非组合响应**（降频）」时才需要 ⇒ ★ 该场景 MUST 显式登记为**已知边界** |
| 判据 | ★ **v1.6 改三档**：① **组合响应**（★ A0 裁决的**基线档**）② `ready` 恒真 + 1 拍响应 ③ `ready` 可拉低（背压）⇒ ★ **① MUST 过**；★ ②③ MUST 显式标注为**未实现/已知边界**（★ 不得只测一档） |

#### D3a.3 ★ 接口契约（`CpuBusReq` / `CpuBusResp`）

| 项 | 决策 |
|---|---|
| 载荷类型 | ★ `CpuBusReq` / `CpuBusResp`（**新文件** `ip/cpu/core/bus_iface_chmem.h`，纯 `ch::core::ch_uint` + `bundle_base` 派生 + `CH_BUNDLE_FIELDS_T`） |
| 字段宽度 | ★ **每字段 ≤ 64 bit**（ADR-091 H-1；★ `address`(64) / `data`(64) 恰好卡在上限 ⇒ **禁止再加宽**） |
| ★ **字段清单（v1.7 补齐）** | ★ `address`(64) · `data`(64) · `burst_len`(8) · `id`(8) · `is_write`(1) · ★★ **`size`(3，v1.7 新增)** ⇒ ★ `size` = 访问字节数（1/2/4/8）⇒ ★ **写语义的载体**（见 §D3a.4） |
| 方向 | 请求 = 核为 **master**（`as_master_direction()`）· 响应 = 核为 **slave**（`as_slave_direction()`）⇒ ★ 逐字段展开 ⇒ 144-bit 总宽**不触发** `PortBitwidthOverflow` |
| 与老核 bundle | ★ **无耦合**（老核的 `cf::bundles::MemReq` 保持 POD 不动 ⇒ ADR-092 §D-E 推迟到 Phase-2） |
| 命名一致性 | ★ **MUST NOT** 复用 `MemReqBundle`（悬空引用）；★ 老核 JSON 里的 `"bundle": "MemReqBundle"` 修正归 ADR-092 §D7 / 本节配套文档同步 |

#### D3a.4 ★ 访存端口化的时序方案（★ **A0 裁决**，2026-10-28，用户裁决 + architect 主持）

> ★ **推导与证据**（预载核三侧时序的实测事实表 · 三条推导 · 代价分析）⇒ **`.rddf/plans/backend-parity-open-core.md` §K13~K15** ⇒ ★ 本节只留**决策与后果**。

| 项 | 决策 |
|---|---|
| ★ **读语义** | ★ **组合消费** `dmem_rsp_data`（★ **MUST NOT** 引入额外 1 拍 reg）⇒ ★ 与预载核 `aread`（0 拍）**同拍消费** |
| ★ **写语义** | ★★ **写由「从设备」按 byte-strobe RMW 完成**（★ **CPU 侧不做 RMW**）⇒ ★ **从设备**按 `address` 低 2 位（byte offset）+ `size` 推出 mask，合并后写回 ⇒ ★⇒ **写不依赖读响应** ⇒ ★ **写生效的绝对拍 = 与预载核一致**（★ 写**无延迟**）⇒ ⇒ ★ **无需 stall，也无需 forwarding**（★ 理由见 plan §K16） |
| ★ **地址可见性** | ★ **访存地址 MUST 提前 1 拍发出**（相对数据消费点）⇒ 需 1 个地址 `ch_reg` ⇒ ⇒ **新增硬约束 G-1k** |
| ★ **从设备义务** | ★ 从设备 MUST 是**组合 slave**（组合地响应读 + 组合地执行写）⇒ ★ Verilator 侧生成组合 `assign`（★ 上游 `c27543d` 已把 `aread` 对齐成 0 拍组合 assign ⇒ 可行） |
| ★ **stall / forwarding** | ★ **不需要**（★ 写无延迟 ⇒ 无 load-after-store 问题）⇒ ★ §D3a.2 的 stall 要求**降级**为「仅当从设备非组合响应时」 |
| ★ ★ **时序模型澄清**（★ v1.8 实测） | ★ `ch_reg` 的 next 是**组合表达式** ⇒ ★ 每次 tick 的 sequential 阶段用「**tick N-1 末的 DAG 状态**」重新求值 ⇒ ★ **端口在拍 N 呈现的是「拍 N-1 末算出的地址」** ⇒ ★ host 在 tick N 后读到它 ⇒ 在 tick N+1 前供数 ⇒ ⇒ ★ **消费拍 N+1 的数据对应地址拍 N**（★ 延迟 1 拍）⇒ ⇒ ★ 这正是 **G-1k** 的成立机制 |
| ★ ★ **代价** | ★ **流水结构不同构** ⇒ 预载核是**全组合 5 级**（`cpu_factory_chmem.h:431-435` 无 stage 寄存器）而新核**带访存级寄存器** ⇒ ⇒ ★★ **`cycles`（拍数）与 `pchash`（per-cycle ordered FNV-1a over PC）在「开放核 vs 预载核」之间结构性不可比** ⇒ ⇒ ★ **H-3 判据 MUST 修正**（见 §D6）⇒ ★ 与 ADR-088 §5.3.3 记录的 M-op「cycle 不同构不可比」**同族、有先例** |

⇒ ★★ **对 H-3 的直接影响**：★ **5 观测量逐位相等** ⇒ 修正为 **3 个终值观测量（`tohost`/`regs`/`memsig`）逐位相等 + 2 个轨迹观测量（`cycles`/`pchash`）走 ADR-089 例外白名单** ⇒ ★ 详见 §D6。

#### D3a.5 ★ 分支反馈契约（★ **A2 裁决**，2026-10-28 —— S3.2b）

> ★ **推导与证据**（3 个 1 拍延迟的逐拍推导 · 4 个变异对照 · 注册顺序踩坑）⇒ **`.rddf/plans/backend-parity-open-core.md` §K20** ⇒ ★ 本节只留**契约**。

| 项 | 决策 |
|---|---|
| ★ **载体** | ★ **核内 payload**（`ip/cpu/core/branch_iface_chmem.h`）⇒ ★ **MUST NOT** 走顶层端口（★ 分支反馈是**核内 stage 间信号** ⇒ ★ 顶层端口语义 = 对外接口 + 观测，见 H-2） |
| ★ **key 清单** | ★ `BRANCH_TAKEN`（`ch_bool`，★ **已被 flush 掩码** = `taken & !flush`）· `BRANCH_TARGET`（`ch_uint<32>`）· `FLUSH`（`ch_bool`） |
| ★ **与预载核** | ★ **同名同义，但独立定义** ⇒ ★ **MUST NOT** include `branch_chmem.h` / `ibus_chmem.h`（★ 二者含 `<ch.hpp>` ⇒ G-1b 链接失败） |
| ★ **pc 重定向位置** | ★ **`branch` stage 的 `LATE` phase**（★ 与预载核同款）⇒ ★ **MUST NOT** 放 `fetch LATE`（★ `canonical_stage_order()` 按首现序 ⇒ 会排在 `branch NORMAL` **之前** ⇒ 读不到本拍 branch 结果 ⇒ 多 1 拍错路） |
| ★ **pc mux 形态** | ★ `pc <<= select(BRANCH_TAKEN, BRANCH_TARGET, pc + 4)` ⇒ ★ **无 BP 预测臂**（★ BP 归 `feat-bp-btb-gshare`） |
| ★ ★ **`FLUSH` 的 1 拍对齐** | ★ **`FLUSH(N) = BRANCH_TAKEN(N-1)`**（★ 生产者 MUST 打 1 拍）⇒ ★ 理由：★ `taken(N)=1` ⇔ `pc_reg(N)` 是错路（本应跳 target）⇒ 其指令在拍 N+1 才发布为 `INSTRUCTION` ⇒ ★ `FLUSH` MUST 在拍 N+1 有效 |
| ★ **store 抑制链** | ★ `we = is_store ? (flush ? 0 : 1) : 0`（★ **纯嵌套 `select`**）⇒ ★ `dmem` 的 `we_reg` 本身打 1 拍 ⇒ ★ **`FLUSH(N)=1` ⇒ 该 store 的写请求在拍 N+1 发出且 `we=0`** |
| ★ **fail-open** | ★ 读不到 `FLUSH` / `BRANCH_TAKEN` ⇒ ★ 视为 0（★ 退化顺序取指 / 不抑制）⇒ ★ 理由：★ A2/A4 分阶段装配 ⇒ ★ 只装单侧是合法形态 ⇒ ★ 抛异常会让合法装配失败 ⇒ ★ 风险由 **A6 的 H-3 等价性自检**兜底（终值 `memsig` 会不等） |
| ★ ★ **布尔取反纪律** | ★ **MUST NOT** 用 `ch_uint<1>(ch_bool)` 做取反 ⇒ ★ 该构造读 C++ 侧值 ⇒ 节点**折成常量** ⇒ ★ 抑制静默失效 ⇒ ★ MUST 用 `select(cond, 0, 1)`（★ G-1m 同族） |

#### D3a.6 ★ `memsig` 的观测来源（★ **memsig 裁决**，2026-10-29，用户裁决 Q1/Q4 + architect 主持）

> ★ **推导与证据**（预载核折叠算法逐位拆解 · `resolve_node_id` 采集机制核验 · 5 个备选方案的代价对比 · 端口名二义的发现过程）⇒ **`.rddf/plans/backend-parity-open-core.md` §K21** ⇒ ★ 本节只留**契约**。

| 项 | 决策 |
|---|---|
| ★ **归属** | ★ **`memsig` 由「从设备」自算**（★ 用户裁决 Q1=A）⇒ ★ 节点名 MUST 沿用 **`mem_sig`** / **`mem_sig_updates`**（★ 与预载核 `dmem_chmem.h:85-86` **同名**）⇒ ★ **parity runner 零改动**（★ `resolve_node_id` 按**节点名**查 `ctx.get_nodes()` ⇒ ★ **不限归属** ⇒ ★ 从设备的节点同样可读） |
| ★ **为何不能由核侧算** | ★ 核**不持有内存** ⇒ ★ 核算不出「RMW 后的最终字」⇒ ★ 预载核能算是因为它的 `dmem` **持有**内存 ⇒ ★ 开放核的对应角色 = **从设备**（★ 架构上同构） |
| ★ **折叠算法** | ★ MUST 与预载核 `dmem_chmem.h:244-270` **逐位同款**：`mix = word_addr*31 + store_wdata + sig_upd*0x9E3779B1`；`sig_next = is_store_active ? (sig*16777619) ^ mix : sig`；`sig_upd += is_store_active` ⇒ ★ `store_wdata` = **RMW 后的最终字**（★ sub-word store 合并后的字面值）⇒ ★ **只有持有内存者能算** ⇒ ★ 从设备 MUST 做 byte-strobe RMW（★ 与 §D3a.4「写由从设备 RMW」一致 ⇒ **无额外成本**） |
| ★ ★ **从设备义务（强契约）** | ★ **MUST 提供**（★ 用户裁决 Q4=A）⇒ ★ 任何作为 DUT 一部分的从设备 MUST 暴露 `mem_sig`/`mem_sig_updates` ⇒ ★ 后果：**ADR-089 例外白名单零新增**（★ `exceptions.json` 现仅 1 条**合成**自检条目 ⇒ ★ `memsig` **未被豁免** ⇒ ★ **必须实现**，不能靠白名单绕过） |
| ★ **折叠顺序** | ★ 按**写请求到达顺序**折叠 ⇒ ★ 与预载核「写发生顺序」同序 ⇒ ★ 同一拍多写时 MUST 按请求内顺序（★ 由 **S4 验证实现**保证） |
| ★ **H-3 中的地位** | ★ 保留在 H-3 的「**3 个终值观测量**（`tohost`/`regs`/`memsig`）逐位相等」内 ⇒ ★ 它验证的正是「**从设备 RMW 与预载核 RMW 逐位相同**」⇒ ★ 逻辑自洽 ⇒ ★ **不改 H-3** |
| ★ **与 `tohost`/`regs` 的差别** | ★ `tohost`/`regs` 由**核**产生（★ 核持有寄存器堆）⇒ 走**顶层 `ch_out`**（H-2）⇒ ★ `memsig` 由**从设备**产生 ⇒ 走**内部节点名**（★ 预载核同款）⇒ ★ **两条观测通路并存**，MUST NOT 混淆 |

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
| **G-1b** | ★ 框架头 **MUST NOT include `<ch.hpp>`** ⇒ 只 include 具体头（`core/io.h` · `core/uint.h` · `core/context.h`） | ★ `ch.hpp:10` include `module.h`，而 ★ `module.h:13` 的 `build_hierarchical_name` **缺 `inline`**（★ **上游与 install 头均如此**）⇒ ★ **2+ TU include 即 multiple definition**；★ ★ **v1.9 实测更正**：★ 该变异的**首个**失败**不是** multiple definition ⇒ ★ 而是 **29 个 `reference to 'detail' is ambiguous`**（★ `<ch.hpp>` 引入的 `ch::detail` 与其它 `detail` **命名冲突**）⇒ ★ ⇒ ★ **后果以「编译期命名冲突」为准**（★ 约束本身不变） | ★ **新核多文件必然踩雷**（★ 编译失败 ⇒ 旧产物顶替 ⇒ ★ C1 假绿通道） |
| **G-1c** | ★ 端口创建 MUST **fail-closed 拒绝 ctx 为空**（★ **v1.8 修正**：★ 由 `throw` 改为 **`return std::unexpected(PluginError::NullContextPtr)`** ⇒ ADR-047 Check 10「静态配置头文件禁 throw」） | ★ `ch_logic_in` 构造是 `if (ctx) { ctx->create_input(...); }` ⇒ ★ **无 current context 时静默不建节点**（`impl() == nullptr`）★ 实测确认 | ★ **假绿通道**：端口「消失」而不报错 ⇒ 网表少端口 ⇒ 门禁照绿 |
| **G-1d** | ★ 端口**方向必须严格**：请求用 `ch_out`，响应用 `ch_in`；且 ★ `ch_in` **MUST NOT** 被内部逻辑赋值 | ★ 有内部 driver 的 `ch_in` **不是**顶层端口（上游 `verilator_backend.cpp:617` 的 skip：`inputimpl::driver() != nullptr` ⇒ 不进 `port_access_`；codegen 侧同理） | 端口**不进** `port_access_` ⇒ Verilator 后端**读不到** ⇒ 观测量采集失败 |
| **G-1e** | ★ **端口的驱动赋值 MUST 在 `ctx_swap` 保护下做** | ★ 实测：赋值在保护外 ⇒ `[ERROR] No active context for literal creation` ⇒ `ch_logic_out::operator=` 报「output_node_ or src_lnode is null」⇒ ★ **output 端口进了网表但无 driver ⇒ 值未定义/X** | ★ **假绿通道**：端口「在但无值」⇒ 仿真读到 X ⇒ 门禁照绿 |
| **G-1f** | ★ **bundle 字段 MUST NOT 带字面量初始化器** | ★ `AddrT address{0};` 在**默认构造时**就创建字面量节点 ⇒ 无 active context 时报「No active context for literal creation」⇒ 值退化为默认且**不报错** ⇒ 上游 `bundle/common_bundles.h` 的 `fifo_bundle` 同款做法（`T data;` 无初始化器） | ★ **假绿通道**（字段值静默为默认）· ★ **连带**：★ **老核 `bundles/mem_bundles.h` 有同样隐患**（`uint_t<64> address{0}`）⇒ 在 CH_MEM 下 include 它即报错 ⇒ ⇒ **新核 MUST NOT include 老 bundle**（⇒ 「不重名」纪律只能文档级，非机检 ⇒ 已诚实标注） |
| **G-1g** | ★ **`as_master_direction()` / `as_slave_direction()` MUST 在 `ctx_swap` 保护内调用** | ★ 内部 `make_output`/`make_input` 会创建节点 ⇒ 无 active context 时报错 | 方向设置失败 ⇒ 端口方向错 ⇒ Verilator 后端读不到 |
| **G-1h** | ★ **`PipeBuilder::elaborate(ctx&)` MUST 在 `ctx_swap` 保护下调用**（★ **v1.5 已做框架级加固**：`elaborate` 内部自建 `ctx_swap`） | ★ `elaborate` 此前**不做** current context 切换 ⇒ stage 闭包内创建的 `ch_*` 节点**静默失败**（值 0 / null impl）⇒ ★ 既有 runner 均自行 `ctx_swap` 后调用（`parity_sim_runner.cpp:328` · `cpu_verilator_sim.cpp:272`）⇒ ★ 是**隐式约定**，无文档无门禁 | ★ 假绿通道（端口/寄存器全为 0 而不报错） |
| **G-1i** | ★ **plugin 的成员初始化器 MUST NOT 创建 `ch_*` 字面量** | ★ `ch_uint<N> m = ch_uint<N>(ch_literal<0,N>{})` 在**plugin 构造期**（`make_unique`）即创建字面量节点 ⇒ ★ 构造期**通常无 active context** ⇒ 静默失败 ⇒ ★ 实测：PC 初值恒 0 ⇒ **全部下游读 0** | ★ 假绿通道（plugin 存在、端口在、但功能全无）⇒ ⇒ **MUST 用纯 C++ 值 + 惰性转换**（如 `uint32_t initial_pc_` + `get_pc()` 内 `ch_reg(uint32_t)`） |
| **G-1j** | ★ **`PipeBuilder::build()` MUST 在 `elaborate()` 之前调用**（★ **v1.5 已做框架级加固**：`elaborate` 检测未 build ⇒ 自动 build） | ★ `build()` 跑 plugin 的 `setup()`/`build()`（★ **注册 `at_stage` 闭包**），`elaborate()` 只**执行**已注册的闭包 ⇒ ★ 漏调 `build()` ⇒ stage 从未注册 ⇒ **全部逻辑静默不执行**（★ 网表能生成、端口能在，但功能全无）⇒ ★ 既有 runner 靠 `CpuFactoryChmem::build_cpu()` 内部调 `build()` ⇒ ★ 又是**隐式约定** | ★★ **最严重的假绿通道** |
| **G-1n** | ★ **stage 的「首现序」MUST 等于流水顺序** ⇒ ★ 装配方（含测试）MUST 按流水顺序**注册 plugin**（★ `setup()` 在 `build()` 时按 `plugins_` 顺序调用）⇒ ★ **MUST NOT** 在 plugin 注册**之前**直接 `at_stage` | ★ `canonical_stage_order()` 只看 stage 名的**首现序** ⇒ ★ 若提供者先注册 `branch`/`memory` ⇒ ★ `fetch` 被排到**最后** ⇒ ★ `memory NORMAL` 先于 `fetch NORMAL` 执行 ⇒ ★ 跨 stage payload **读不到**（`has()` = false）★ **实测命中** | ★ **跨 stage payload 可见性反转** ⇒ ★ 静默走 fail-open（端口在、语义为零 ⇒ 最坏假绿） |
| **G-1o** | ★ **顶层端口名 MUST NOT 与内部 `ch_reg` / `ch_node` 名同名** ⇒ ★ 且 ★ 端口名 MUST 与内部节点名**可区分**（★ 建议内部 reg 用 `_reg` 后缀，如 `open_pc_reg`） | ★ CppHDL codegen 按 `eval_list` 顺序建 `name_counts`（`codegen_verilog.cpp:64-76`）⇒ ★ **内部 node 先占名** ⇒ ★ 重名者加 `_N` 后缀 ⇒ ★ **实测**：顶层端口 `open_pc` 被改名 `open_pc_1`（`open_core_ports_top.v:11`）⇒ ★ 同时 `ctx` 里**两个节点同名** ⇒ ★ `resolve_node_id(ctx, "pc")` 取**首个**匹配 ⇒ ★ 观测可能读到 `ch_reg` 而非端口 | ★ **两后端端口名不一致**（Simulator 侧 `open_pc` / Verilator 侧 `open_pc_1`）⇒ ★ 破 H-5（★ Verilator 侧按**生成成员名**读 ⇒ 读到 `open_pc_1` ⇒ ★ 按名匹配失败 ⇒ **静默读不到** ⇒ 最坏假绿） |
| **G-1l** | ★ **payload 的「累加式 select」（`X = select(cond, v, X)`）MUST 由上游 stage 先写 `X`** ⇒ 否则 false 分支在首次赋值前被读 ⇒ `PayloadStore::get`（可变版）缺失时 emplace **null impl** ⇒ `Simulator::initialize` **段错误** | ★ gdb 栈坐实：`muximpl::create_instruction` → `lnodeimpl::id(nullptr)` ⇒ `Simulator::initialize` 段错误 | ★ **段错误**（★ 预载核不崩是因为 EX 阶段总是先写 `RD_DATA`） |
| **G-1m** | ★ **`ch_bool` MUST NOT 用于 C++ 条件上下文**（`?:` / `if`）⇒ ★ MUST 用**节点级 `select()`** | ★ `ch_bool::operator bool` 是 **explicit** ⇒ ★ 在 `?:` / `if` 里走 **contextual conversion** ⇒ ★ **静默取 false** ⇒ ★ **变异对照 RED**（`select()` → `?:` ⇒ 2 处 FAILED：★ **写使能恒 0**（写请求发不出去 ⇒ 端口在、值全 0 ⇒ 最坏假绿）+ ★ **符号扩展失效**） | ★ **静默失效**（★ 无编译错误、无运行错误 ⇒ 只表现为「值不对」） |
| **G-1k** | ★ **访存地址 MUST 提前 1 拍发出**（相对数据消费点）⇒ 需 1 个地址 `ch_reg` | ★ 端口形态下外部从设备是「拍边界驱动」⇒ 拍 N 才知道拍 N 的读地址 ⇒ ★ **无法在拍 N 组合给出数据** ⇒ ⇒ 地址 MUST 提前 1 拍可见（★ §D3a.4 A0 裁决） | ★ 访存读退化为 1 拍 ⇒ **与预载核不等价** ⇒ ⇒ `memsig`/`regs` 的 H-3 判定随之失效 |

#### ★ v1.9 ★ G-1 的**机检覆盖矩阵**（★ C2 逐条变异对照复核，2026-10-28）

> ★ **复核方法**：★ 对每条约束施加「**撤掉该写法**」的变异 ⇒ ★ 跑对应判据
> ⇒ ★ **判据必须 RED** 才算该约束「被机检覆盖」（★ ADR-091 §D6 **V-9** 纪律）。
> ★ **变异细节与输出** ⇒ `.rddf/plans/backend-parity-open-core.md` §K19。

| # | 约束 | 机检形态 | 变异结果 | 覆盖度 |
|---|---|---|---|---|
| **G-1a** | 端口 API 条件化 | ★ **编译期**（TLM target） | ★ **268 errors**（`'ch' was not declared`） | ✅ **强** |
| **G-1b** | 禁 include `<ch.hpp>` | ★ **编译期** | ★ **29 errors**（★ `detail` 命名冲突） | ✅ **强** |
| **G-1c** | fail-closed 拒绝空 ctx | ★ 单测（`test_pipe_builder_ports.cpp`） | ★ **RED** | ✅ **强** |
| **G-1d** | 端口方向严格 | ★ **编译期类型系统**（★ `ch_in`/`ch_out` 不可互换）+ 行为判据 case E | ★ 类型层面阻断 | ✅ **强** |
| **G-1f** | bundle 无字面量初始化器 | ★ 单测（★ **C2 新增**） | ★ **RED**（★ `{0}` 与编译期字面量**两种**形式） | ✅ **强** |
| **G-1h** | `elaborate()` 自建 `ctx_swap` | ★ 单测（★ **C2 新增**） | ★ **RED** | ✅ **强** |
| **G-1j** | `build()` 在 `elaborate()` 之前 | ★ 单测（★ **C2 新增**） | ★ **RED**（★ 2 处） | ✅ **强** |
| **G-1i** | 成员初始化器不建 `ch_*` 字面量 | ★ **机制锚**单测（★ C2 新增：★ 钉住「无 ctx ⇒ null impl」） | ★ 变异「某 plugin 在成员里建字面量」**未被抓住** | ⚠️ **中**（★ 覆盖**机制**，★ 不覆盖「**谁**违反」） |
| **G-1e** | 驱动赋值在 `ctx_swap` 下 | ✗ 无 | —— | ⚠️ **弱**（★ 仅文档级） |
| **G-1g** | 方向设置在 `ctx_swap` 下 | ✗ 无 | —— | ⚠️ **弱**（★ 仅文档级） |
| **G-1k** | 访存地址提前 1 拍 | ✗ 无 | —— | ⚠️ **弱**（★ 需 **A6** 的「开放核 vs 预载核」时序对比才可判定） |

⇒ ★★ **复核结论（★ 诚实标注）**：
★ **11 条存量约束无一被证伪**（★ 与 v1.8 的 8 条候选中 **4 条被证伪**形成对照
⇒ ★ 说明 S1/S3a 时期的登记**确实有实测支撑**）⇒ ★
★ **但机检覆盖度极不均衡**：★ **强 7 / 中 1 / 弱 3** ⇒ ★
★ ⇒ ★ 「**MUST 逐条机检**」的表述**名不副实**（★ 3 条**没有任何机检**）⇒ ★
★ ⇒ ★ **降级表述**：★ 本节矩阵 = **各条的真实覆盖度**（★ 不得再宣称「逐条机检」）。

⇒ ★★ **G-1a/b 是「框架头纪律」**（与 ADR-040 的 CH_MEM-only 纪律同族）；★ **G-1c/d/e/f/g/h/i/j 是「假绿通道」**（与 ADR-089 的 fail-closed 纪律同族）；★ **G-1k 是「等价性前提」**（★ 违反 ⇒ H-3 的终值判定失效）⇒ ★ ★ **v1.9 更正**：★ 原表述「MUST 逐条机检」**名不副实** ⇒ ★ 实际覆盖度见上方
★ **机检覆盖矩阵**（★ 强 7 / 中 1 / **弱 3** ⇒ ★ **G-1e / G-1g / G-1k 无任何机检**）
⇒ ★ ⇒ ★ **本节各条 MUST 至少有一条机检**（★ 弱覆盖的 3 条 **MUST 补机检**或显式标注）。

#### D5.1.2 ★ v1.5 的**框架级加固**（★ G-1h / G-1j ⇒ 已在 `include/cf/plugin/pipe_builder.h` 落地）

| 加固 | 内容 | 性质 |
|---|---|---|
| **G-1h** | ★ `elaborate(ctx&)` 内部**自建 `ctx_swap`** | ★ **幂等纯增益**（既有 runner 自行 swap ⇒ 嵌套无副作用）⇒ ★ 消除「stage 闭包内建节点静默失败」 |
| **G-1j** | ★ `elaborate()` 检测「未 `build()`」⇒ **自动 build** | ★ `build()` 幂等（只跑 plugin 的 `setup`/`build` ⇒ 注册 `at_stage`）⇒ ★ 消除「漏调 `build()` ⇒ 全部逻辑静默不执行」 |

⇒ ★ **验收**：★ **零退化** —— TLM binary **509 case / 508 passed / 1 failed**（唯一失败 = baseline-known）· ★ CH_MEM 框架子集 **24 case / 105 assertions 全绿** · ★ `[chmem]` 58 case 中 57 PASS（唯一失败 = `[verilator]` 48-ELF 端到端 **SIGTERM 超时**，★ 非功能失败）。

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
| **H-3** | ★ **开放核与预载核 MUST 有机器可判的等价性自检**：同一 ELF 分别跑两核 ⇒ ★★ **v1.6 修正：3 个终值观测量（`tohost`/`regs`/`memsig`）逐位相等**（★ 这才是「语义等价」的机器可判形式 ⇒ 满足 ADR-083 §4.1「防双副本漂移」的本意）⇒ ★ **2 个轨迹观测量（`cycles`/`pchash`）MUST 走 ADR-089 例外白名单**（★ 结构性不可比 —— 流水结构不同构，见 §D3a.4）⇒ ★ 例外 scope MUST 限定为「开放核 vs 预载核」对比语料，★ **MUST NOT 扩散到双后端 parity 语料**（★ 双后端线 MUST 保持逐拍相等）⇒ ★ **白名单扩张权归 architect**（★ 本条即 2026-10-28 architect 扩张） | ADR-083 §4.1 + ADR-089 §D3 观测量集与例外白名单 |
| **H-4** | ★ **新核 MUST NOT 引入 TLM 依赖**（不引 `cf::plugin::uint_t`、不引 `cpptlm`）⇒ `ip/cpu/core/**` 全部文件 MUST 为 CH_MEM-only | ADR-040 v3.0 + ADR-088 §10 |
| **H-5** | ★ **每个顶层端口字段 MUST 在两个后端上都能读到** ⇒ 门禁 MUST 以「顶层端口」为唯一观测量来源 | ADR-089 D2 |

### ★ 正文长度自检（★ 诚实标注 —— 不在本次顺手重构）

★ 本 ADR 已 **> 250 行**（★ 正文 = 决策本身）⇒ ⇒ ★ **推导与证据已外移**（§D3a.4 的推导 → plan §K13~K15）⇒ ★ **建议后续拆分**：★ 「核形态决策」与「端口契约 + 实测约束（G-1 系列）」可拆为 **ADR-091 + 1 篇主题文档** ⇒ ★ **归后续独立 change**（★ 不在本次顺手改写 —— 存量文档不追溯）。

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
6. ⚠️ **「从设备 MUST 提供 `mem_sig`」是强契约**（★ §D3a.6，用户裁决 Q4=A）⇒ ★ 第三方 SoC 接入开放核时，其 memory model / Cache IP MUST 实现该折叠算法 ⇒ ★ **接入门槛**：MUST 复刻预载核 `dmem_chmem.h:244-270` 的折叠（★ 约 5 行）⇒ 缓解：★ 折叠算法已固化在 §D3a.6 + plan §K21 ⇒ ★ 可直接复制
7. ⚠️ **端口名与内部节点名解耦是强制约束**（★ G-1o）⇒ ★ 新增顶层端口时 MUST 同步检查内部 `ch_reg` 名不冲突 ⇒ ★ 否则 CppHDL 静默改名 ⇒ ★ 两后端端口名不一致 ⇒ ★ 观测按名匹配静默失败

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
- [ ] **V-3** ★ **等价性自检**（H-3，★ **v1.6 修正**）：同一 ELF 跑**开放核**与**预载核** ⇒ ★ **3 个终值观测量（`tohost`/`regs`/`memsig`）逐位相等** ⇒ ★ **2 个轨迹观测量（`cycles`/`pchash`）MUST 已登记为 ADR-089 例外**（★ scope 限定「开放核 vs 预载核」⇒ 双后端线不受影响）⇒ ★ 变异对照：★ 故意改开放核的 1 处语义 ⇒ 该自检 MUST RED（证明它有鉴别力，不是「两边都没跑就绿」）
- [ ] **V-4** ★ **H-2 生效**：门禁的 5 个观测量**全部**从**顶层端口**读取 ⇒ ★ `--public-flat-rw` 读内部层级的路径**退休**（新核路径上）
- [ ] **V-5** ★ **H-4 生效**：`ip/cpu/core/**` 全部文件**零** `cf::plugin::uint_t` / `cpptlm` 引用（★ 建议加 CI grep 检查）
- [ ] **V-6** ★ **零回归**：★ 现有 52 case parity **基线不变**（预载核不动 ⇒ 报告应与 `openspec/changes/backend-parity-corpus/first-run-evidence.md` 的 A3 首跑**逐位一致**）⇒ ★ D4 架构门禁（`check_plugin_portability.sh` / `verify_plugin_decision.sh`）全绿
- [ ] **V-10** ★ **分支反馈语义有鉴别力**（v1.10）：★ 4 个变异 MUST 各自 RED —— ① `FLUSH` 恒 0 ② 去掉 store 抑制（`we = is_store`）③ pc 不重定向（顺序取指）④ 提供者注册在 ibus **之前**（G-1n）⇒ ★ **实测**：4/4 RED（★ 分别 2/1/1/1 个 case 失败）
- [ ] **V-11** ★ **`memsig` 观测来源落地**（v1.11）：★ 开放核的 `memsig` MUST 由**从设备**提供 ⇒ ★ `mem_sig` / `mem_sig_updates` 节点 MUST 存在于网表 ⇒ ★ `parity_sim_runner` MUST 在**不传** `--allow-incomplete` 时读到它（★ 证明「按节点名查、不限归属」成立）⇒ ★ 变异对照：★ 从设备**不提供** `mem_sig` ⇒ MUST 报「missing observation 'memsig'」并退出非 0（★ fail-closed）
- [ ] **V-12** ★ **折叠算法逐位同款**（v1.11）：★ 从设备的折叠算法 MUST 与预载核 `dmem_chmem.h:244-270` **逐位一致**（★ 同一写序列 ⇒ 同一 `mem_sig` / `mem_sig_updates`）⇒ ★ 变异对照：★ 改一处常量（★ 如 `0x9E3779B1` → `0x9E3779B2`）⇒ MUST RED
- [ ] **V-13** ★ **从设备强契约**（v1.11）：★ 任一作为 DUT 一部分的从设备 MUST 暴露 `mem_sig`/`mem_sig_updates` ⇒ ★ **MUST NOT** 新增 ADR-089 例外白名单条目来绕过 ⇒ ★ 白名单条目数 MUST 保持 1（★ 那 1 条是**合成**自检条目，★ 不计真实例外）
- [ ] **V-14** ★ **端口名解耦**（v1.11，★ G-1o）：★ 生成网表的顶层端口名 MUST **等于**契约名（★ 如 `open_pc` / `open_flush`）⇒ ★ **MUST NOT** 出现 `_N` 后缀改名 ⇒ ★ 变异对照：★ 把内部 `ch_reg` 改回与端口同名 ⇒ MUST RED（★ 由「端口存在」断言 + 「端口名精确匹配」断言共同捕获）
- [ ] **V-15** ★ **端口断言精确化**（v1.11）：★ 顶层端口断言 MUST 用**精确名匹配** ⇒ ★ **MUST NOT** 用子串匹配（★ 子串匹配会让改名后的端口「通过」⇒ ★ 假绿）⇒ ★ 变异对照：★ 断言一个**不完整**的名字（★ 如 `open_pc` 对 `open_pc_1`）⇒ MUST RED
- [ ] **V-9** ★ **变异对照纪律**（v1.8）：★ 每条「实测约束」MUST 有**变异对照**（★ 撤掉该写法 ⇒ 测试 MUST RED）⇒ ★ 否则「约束」只是**猜测**，会被后人当铁律继承 ⇒ ★ ★ **本轮实测**：★ 8 条候选里 **4 条被证伪**（★ `&&`/`||` 失效 / `==` 走指针比较 / 跨宽构造须 `zext` / ctx 名必须唯一 ⇒ ★ 全部**不成立**）⇒ ⇒ ★ **只有 2 条（G-1l / G-1m）配登记为硬约束** ⇒ ★ 证据见 plan §K17
- [ ] **V-8** ★ **写语义完备**（v1.7）：★ `CpuBusReq` MUST 含 `size`（3 bit）⇒ ★ `size` MUST 表达 1/2/4/8 四种字节宽度 ⇒ ★ **sub-word store 的 byte-strobe 语义 MUST 可表达** ⇒ ★ 契约测试 MUST 覆盖 `size` 的 4 种取值 ⇒ ★ **MUST NOT** 让 CPU 侧承担 RMW（★ 那会引入读响应依赖 ⇒ 写延迟 ⇒ stall/forwarding）
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

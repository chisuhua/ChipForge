# ADR-091 开放核实施路径（S1~S5）

> **change**: `backend-parity-open-core`（待立项）
> **本文件**: ADR-091 §D8「S1~S5」的可执行路径（repo 内 SSOT）
> **架构授权**: **ADR-091 v1.2**（CH_MEM 开放核形态与端口架构，✅ Accepted 2026-10-28）
> **契约**: **ADR-092 v1.1**（TLM 参考实现的冻结与演进停止）· ADR-083 v1.2（`ch_stream` 归类纠偏）· ADR-089 v1.4（parity 门禁）
> **规模**: 中（★ **6~10 人日**，★ **超出当前 v0.11.0 窗口** ⇒ 与 A4 / BP 增量 3 争人力，见 §风险 R-5）
> **前置**: ✅ ADR-091 已 Accepted · ✅ 配置面已落地（`CpuConfigChmem`，P2）· ★ **G-1 机制已 PoC 验证**（见 §1）
> **分支**: ★ **当前在 `main`**（P2 + ADR + 文档同步共 15 commit 领先 `origin/main`；★ `feature/chmem-bp-btb-gshare` 停在 `3b7c392` ⇒ ★ **本 change 沿用 `main`**，★ **不新开分支** —— 理由见 §1.3）
>
> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan phase-by-phase. Steps use checkbox (`- [ ]`) syntax for tracking.

---

## Goal

按 **ADR-091 §D8** 落地 **CH_MEM 开放核**：新增 `ip/cpu/core/`，对外暴露 `ch_stream<CpuBusReq>` 端口（取指/访存由外部供给），并让 **解释器后端 + Verilator 后端都必过**（D0-c）。

⇒ ★ **不删预载核**（`ip/cpu/plugins/*_chmem.h` + `cpu_factory_chmem.h` 保持不动）⇒ ★ 现有 **52 case parity 基线零风险**。
⇒ ★ **不删 TLM 代码**（ADR-092 Phase-1：冻结保留）⇒ ★ 686 case 保留、Check 7 不真空 PASS。

### 阶段划分（ADR-091 §D8）+ 本 plan 的 Phase 对应

| ADR-091 阶段 | 内容 | 本 plan Phase | 前置 |
|---|---|---|---|
| **S1** | 框架层 G-1（端口声明）+ G-5a（后端注入） | ★ **Phase 1** | — |
| **S2** | `bus_iface_chmem.h`（G-4）+ `cpu_core_chmem.h` 装配骨架 | Phase 2 | S1 |
| **S3** | `ibus_port_chmem.h` / `dmem_port_chmem.h` + 顶层观测端口（H-2） | Phase 3 | S2 |
| **S4** | `verify_mem_model_chmem.h`（验证实现：ELF 加载 + 内存模型） | Phase 4 | S3 |
| **S5** | ★ **双后端必过**（D0-c）+ **H-3 等价性自检** | Phase 5 | S4 |
| **S6** | SoC 接线（接真 Cache/MMU） | ★ **不在本 plan** | 依赖 MMU/Cache CH_MEM 实装 |

---

## 1. 当前状态快照（2026-10-28 实测，事实）

### 1.1 ★ G-1 机制已 PoC 验证（★ 本 plan 的技术前提）

`tests/framework/test_pipe_builder_ports.cpp`（**本轮新建，2 case / 6 assertions 实测全过**）：

| 验证项 | 结果 | 实测输出 |
|---|---|---|
| **A** 裸 ctx 建 `ch_in`/`ch_out` ⇒ 进网表**顶层端口** | ✅ 通过 | `Created node ID 2 (poc_in) of poc_in (input, 32 bits)` + 网表 `input [31:0] poc_in` |
| **B** 无 current context ⇒ `impl() == nullptr` | ✅ 通过 | ⇒ **静默不建节点 ⇒ 假绿通道** |

⇒ ★★ **不需要 CppHDL 的 Component / `ch_device` 体系**（上游 `VerilatorBackend::try_initialize(ctx*, data_map_t&)` 只要 ctx + data_map）。

### 1.2 ★ PoC 得出的 4 条 G-1 约束（已落 **ADR-091 v1.2 §D5.1**）

| # | 约束 | 违反后果 |
|---|---|---|
| **G-1a** | 端口 API MUST 置于 `#ifdef CF_PLUGIN_USE_CH_MEM` 内 | TLM 模式编译失败 |
| **G-1b** | ★ 框架头 **MUST NOT include `<ch.hpp>`**（`ch.hpp:10` → `module.h:13` 的 `build_hierarchical_name` **缺 `inline`** ⇒ 2+ TU 即链接失败） | ★ **新核多文件必然踩雷** |
| **G-1c** | 端口创建 MUST **fail-closed 断言 ctx 非空** | ★ 假绿通道（端口消失而不报错） |
| **G-1d** | 端口方向严格：`ch_in` MUST NOT 被内部赋值 | 端口不进 `port_access_` ⇒ Verilator 读不到 |

### 1.3 ★ 分支现状（★ 决策：**沿用 `main`**）

```
* main                                  597aeb4+15  [origin/main: ahead 15]
  feature/chmem-bp-btb-gshare          3b7c392      （P2 验收证据落档）
```

⇒ ★ P2（9 commit）+ ADR（4 commit）+ 文档同步（11 commit）**全在 `main`** ⇒ ★ `main` 是**事实上的开发主线** ⇒ ★ 本 change **不新开分支**（避免又一次分支漂移）。
⇒ ⚠️ **代价**：`main` 领先 `origin/main` 15 commit ⇒ ★ 推送时机归用户。

### 1.4 GShare 状态澄清（★ 避免误解）

★ **`main` 的 `cpu_factory_chmem.h:501` 明确写「不含 `bp_gshare` —— GShare 增量 3 未签批（净负收益, ADR-088 §5.3.3 C-3 不满足）」** ⇒ ★ **GShare 已被架构裁决「结案为本期不做」并从主线移除** ⇒ ★ **不是丢失，是有意退役** ⇒ ★ 备份 patch 仍在 `/tmp/opencode/gshare_only_now.patch`（历史留档）。
⇒ ★⇒ **本 change 与 GShare 无耦合**（新核在 `ip/cpu/core/`，不改 `branch_predictor_chmem.h`）。

---

## 2. Phase 1 — S1：框架层 G-1（端口声明）+ G-5a（后端注入）

> ★ **本 Phase 独立可验收**，且是后续全部阶段的前提。

- [ ] **1.1** [RED] `tests/framework/test_pipe_builder_ports.cpp` 扩为契约测试：
  - case C：`PipeBuilder::declare_in/declare_out` 产出的端口**出现在网表顶层端口列表**
  - case D：★ **ctx 为 null 时 fail-closed**（MUST 抛异常，MUST NOT 静默不建节点 ⇒ G-1c）
  - case E：★ **`ch_in` 被内部赋值时** ⇒ MUST NOT 出现在顶层端口（G-1d）
  - case F：`create_simulator(backend)` 注入 `InterpreterBackend` / `VerilatorBackend` 后 `active_backend_name()` 正确（G-5a）
- [ ] **1.2** [GREEN] `include/cf/plugin/pipe_builder.h` 增 `declare_in<T>` / `declare_out<T>`：
  - ★ **置 `#ifdef CF_PLUGIN_USE_CH_MEM` 内**（G-1a）
  - ★ **只 include 具体头**（`core/io.h` · `core/uint.h`），**MUST NOT include `<ch.hpp>`**（G-1b）
  - ★ **`ctx_` 为 null ⇒ throw**（G-1c）
  - 创建时 MUST 在 **`ctx_swap(ctx_)`** 保护下（否则落到错误的 current context ⇒ 上游 `device.h:13-19` 记录过 node-id aliasing 坑）
- [ ] **1.3** [GREEN] `include/cf/plugin/pipe_builder.h` 增 `create_simulator(std::unique_ptr<ch::IEvalBackend>)` 重载（G-5a）：
  - ★ **`nullptr` ⇒ 保持现有行为**（向后兼容，★ 现有调用方零改动）
  - ★ `#ifdef CF_PLUGIN_USE_CH_MEM` 内
- [ ] **1.4** [GREEN] `tools/parity_runner/parity_sim_runner.cpp` + `tools/verilator_runner/cpu_verilator_sim.cpp` ★ **可选**加 `--backend interpreter|verilator`（★ **不强制** —— 现有两进程架构继续有效；本项仅用于 Phase 5 的单进程双后端对照）
- [ ] **1.5** 门禁：`verify_plugin_decision.sh` / `check_plugin_portability.sh` / `doc_link_check.sh` 全绿

### 2.1 Phase 1 的验证判据

| # | 判据 | 形式 |
|---|---|---|
| V1.1 | 端口进网表顶层 | 测试 case C |
| V1.2 | ★ ctx null ⇒ fail-closed | 测试 case D（★ MUST 有变异对照：去掉断言 ⇒ RED） |
| V1.3 | ★ 内部驱动的 `ch_in` 不进顶层 | 测试 case E |
| V1.4 | 后端注入生效 | 测试 case F（`active_backend_name()`） |
| V1.5 | ★ **零回归** | TLM binary **507 case / 506 passed / 1 failed**（唯一失败 = `test_7stage_riscv.cpp:124` 的 baseline-known FAIL）⇒ 计数 MUST 不变 |

---

## 3. Phase 2 — S2：`bus_iface_chmem.h`（G-4）+ 装配骨架

- [ ] **2.1** [RED] `tests/cpu/schema/test_cpu_bus_iface.cpp`（新）：`CpuBusReq` / `CpuBusResp` 字段与宽度断言（★ 每字段 ≤64 bit ⇒ H-1）
- [ ] **2.2** [GREEN] 新建 `ip/cpu/core/bus_iface_chmem.h`：
  - `CpuBusReq` / `CpuBusResp`：`bundle_base` 派生 + `CH_BUNDLE_FIELDS_T` + **纯 `ch::core::ch_uint`**（★ 零 `cf::plugin::uint_t` ⇒ H-4）
  - ★ **真名 MUST NOT 是 `MemReqBundle`**（该名字在代码中不存在 ⇒ ADR-092 §D7）
  - ★ 字段宽度：`address` 64 / `data` 64 / `is_write` 1 / `id` 8 ⇒ ★ **每字段 ≤64 bit**（H-1）
- [ ] **2.3** [GREEN] 新建 `ip/cpu/core/cpu_core_chmem.h`：装配骨架（复用 `ip/cpu/arch/riscv/` + `ip/cpu/core/payload_common.h`；★ **不动** `ip/cpu/plugins/*_chmem.h`）
- [ ] **2.4** 门禁 + 表格检查全绿

## 4. Phase 3 — S3：端口驱动数据通路 + 顶层观测端口

- [ ] **3.1** [RED] 测试：开放核在**无 ELF 预载**下能通过 `ch_stream` 端口取到指令
- [ ] **3.2** [GREEN] `ip/cpu/core/ibus_port_chmem.h`：取指走端口（★ 复用 `ibus_chmem.h` 的两级取指 + flush 语义）
- [ ] **3.3** [GREEN] `ip/cpu/core/dmem_port_chmem.h`：访存走端口
- [ ] **3.4** [GREEN] 顶层**观测端口**（H-2）：`tohost` / `pc` / `regs` / `memsig` ⇒ ★ **MUST 走顶层 `ch_out`**（★ 这是「退休 `--public-flat-rw` 内部层级读法」的前提）
- [ ] **3.5** [GREEN] 握手成本处理（D3a.2）：`ready` 恒真 ⇒ 无阻塞流；`ready` 可拉低 ⇒ CPU 侧 MUST 有 stall 机制
- [ ] **3.6** 门禁 + 零回归

## 5. Phase 4 — S4：验证实现（`verify_mem_model_chmem.h`）

- [ ] **4.1** [RED] 测试：验证实现加载 ELF 并正确响应 imem/dmem 请求
- [ ] **4.2** [GREEN] 新建 `ip/cpu/core/verify_mem_model_chmem.h`：内存模型 + ELF 加载（★ 可参考 TLM 侧 `ip/cpu/picolibc_host_memory.h` 的**语义**，★ 但 MUST NOT include 它 —— 它在 ADR-092 的冻结集里）
- [ ] **4.3** 门禁 + 零回归

## 6. Phase 5 — S5：双后端必过（D0-c）+ 等价性自检（H-3）

- [ ] **5.1** [RED] 同一份测试 body 在 **解释器后端**与 **Verilator 后端**下都必过（★ 借鉴上游 `tests/dual_backend_fixture.h` 的 `DUAL_BACKEND_TEST_PAIR` 范式；★ **Verilator 侧 WARN-skip 不算 PASS**）
- [ ] **5.2** [GREEN] 单进程双后端对照（用 Phase 1 的 `create_simulator(backend)`）
- [ ] **5.3** [RED+GREEN] ★ **H-3 等价性自检**：同一 ELF 跑**开放核**与**预载核** ⇒ **5 观测量（tohost / regs / memsig / cycles / pchash）逐位相等** ⇒ ★ **变异对照**：故意改开放核 1 处语义 ⇒ 该自检 MUST RED
- [ ] **5.4** 门禁 + 零回归 + ★ **52 case parity 基线逐位不变**

---

## 7. 执行期实测知识（重要，勿再踩）

### K1 — ★ 框架头 **MUST NOT** include `<ch.hpp>`

`ch.hpp:10` include `module.h`，而 `module.h:13` 的 `build_hierarchical_name` **缺 `inline`** ⇒ ★ **任何 2+ TU include `<ch.hpp>` 即链接失败**（multiple definition）★ **本轮实测踩到**。
⇒ ★ 框架头与新核的所有头 ⇒ **只 include 具体头**（`core/io.h` · `core/uint.h` · `core/context.h` · `core/reg.h` · `core/mem.h` · `codegen_verilog.h`）。

### K2 — ★ `ch_in` 无 current context ⇒ **静默不建节点**

`ch_logic_in` 构造是 `if (ctx) { ctx->create_input(...); }` ⇒ ★ 无 ctx 时 `impl() == nullptr`，**不报错** ⇒ ⇒ ★ **假绿通道** ⇒ G-1c 的 fail-closed 断言是必需的。
★ **同类静默失败**：任何 `ch_*` 节点在错误 context 下构造都会「消失」⇒ ★ **凡建节点处 MUST 断言**。

### K3 — ★ 有内部 driver 的 `ch_in` **不是**顶层端口

上游 `verilator_backend.cpp:617` 的 skip 逻辑：`inputimpl::driver() != nullptr` ⇒ 不进 `port_access_` ⇒ codegen 侧同样不发射为顶层 `input`。
⇒ ★ 端口方向必须严格：**请求侧用 `ch_out`，响应侧用 `ch_in`**；★ `ch_in` MUST NOT 被内部逻辑赋值。

### K4 — ★ `tests/CMakeLists.txt` 的两个 binary 收集方式不同

| binary | 收集方式 | 新增测试 |
|---|---|---|
| `chipforge_tests`（TLM） | ★ `file(GLOB_RECURSE ...)` ⇒ **自动收集** | 无需改 CMake（★ 但**需重新 `cmake -S . -B build`** ⇒ GLOB 在 configure 时展开） |
| `chipforge_tests_chmem`（CH_MEM） | ★ `CHMEM_TEST_SOURCES` **显式列表** | **必须显式加源** |

### K5 — ★ 顶层端口 = ctx 里的 `type_input` / `type_output` 节点

`ch::toVerilog` 的端口声明来自 ctx 的节点类型（`codegen_verilog.cpp:669-679`）⇒ ★ **与 Component / `ch_device` 无关** ⇒ ★ ChipForge 无需引入 CppHDL 组件体系。

### K6 — ★ `VerilatorBackend` 的 64-bit 端口上限

`build_port_access_table()` 对 `size() > 64` 的 `type_input`/`type_output` 返回 `PortBitwidthOverflow`。
⇒ ★ `ch_stream` 的 **bundle 端口按字段展开**（`as_master_direction()` ⇒ `make_output(payload, valid)` + `make_input(ready)`）⇒ ★ **总宽 144-bit 的 `CpuBusReq` 不会触发** ⇒ 但 ★ **每字段 MUST ≤64 bit**（H-1）。

### K8 — ★ `ch_bool` / `ch_uint` 的**字面量构造**也需要 active context

`ch_uint<32>(ch_literal<0,32>{})` / `ch_bool(ch_literal<1,1>{})` 在**无 current context** 时
⇒ `[ERROR] No active context for literal creation` ⇒ ★ **值为默认值且不报错**（实测：构造出的
`ch_bool` 读出 `false` 而非 `true`）⇒ ⇒ ★ **一切 `ch_*` 字面量构造 MUST 在 `ctx_swap` 保护内做**
（★ 与 G-1e 同族：端口驱动赋值也是同一根因）。

### K9 — ★ `CH_BUNDLE_FIELDS_T` 宏要求作用域内 `using ch::core::bundle_base;`

宏内部用的是**非限定名** `bundle_base<Self>` ⇒ MUST 在使用宏的命名空间里
`using ch::core::bundle_base;`（★ 上游 `bundle/common_bundles.h:13` 同款做法）
⇒ 否则报「expected nested-name-specifier before 'bundle_base'」。

### K10 — ★ 类模板 + 默认参数：**类型位置**必须写 `<>`

`CpuBusReq` 是类模板（宽度参数化）⇒ ★ CTAD 只在**变量声明**处生效
⇒ ★ 在**类型位置**（`is_base_of_v<...>` / `decltype(...::field)` / 函数签名）必须写 `CpuBusReq<>`。

### K11 — ★ bundle 头的 include **顺序**要紧

`core/bundle/bundle_base.h` 依赖 `ch_width_v`（定义在 `core/traits.h`）
⇒ ★ `traits.h` MUST 在 `bundle_base.h` **之前**；且 ★ `CH_BUNDLE_FIELDS_T` 宏在
`core/bundle/bundle_meta.h` ⇒ 该头也要 include（★ `ch.hpp` 正是靠这个顺序让 bundle 可用 —— 而我们
**不能** include `ch.hpp` ⇒ 必须显式补齐）。

### K12 — ★ `ch_bool` 的 `operator bool` 是 explicit

⇒ Catch2 的 `static_cast<bool>` **不适用**（编译错误）⇒ 用 `bool(x.operator bool())`
或 `x == ch_bool(ch_literal<1,1>{})` 做比较。

### K13 — ★ 预载核的三侧访存时序**不对称**（★ A0 裁决的实测依据）

| 侧 | 预载核实现 | 时延 | 实测依据 |
|---|---|---|---|
| **取指读** | `imem.sread(word_addr)` | **1 拍** | `ip/cpu/plugins/ibus_chmem.h:100-103` |
| **访存读** | `mem.aread(word_addr)` | **0 拍**（组合） | `ip/cpu/plugins/dmem_chmem.h:185` |
| **访存写** | `mem.write(word_addr, data, en)` | **同拍生效** | `ip/cpu/plugins/dmem_chmem.h:237`（★ 无 `swrite`/`awrite` 之分 ⇒ 组合写） |

⇒ ★ 上游 `c27543d` 把 `aread` 对齐成 **0 拍组合 assign**（Simulator 与生成 Verilog 同为 0-cycle）⇒
⇒ ★ **`imem` 必须用 `sread`**（否则 `INSTRUCTION` 与 `pc_lag` 的 1 拍对齐被破坏）⇒
⇒ ★ **`dmem` 用 `aread`**（0 拍组合）⇒ ★ **写是同拍生效**。

### K14 — ★ 端口形态下「0 拍读」为何**不可直接达成** ⇒ 三条推导

1. ★ **端口形态 = 拍边界驱动**：外部从设备通过顶层 `ch_in` 端口供数 ⇒ 值在**拍边界**确定 ⇒
   ⇒ 外部在拍 N 才拿到拍 N 的读地址 ⇒ ★ **无法在拍 N 组合给出该地址的数据**。
2. ★ **唯一出路 = 地址提前 1 拍可见**：CPU 侧用 1 个 `ch_reg` 把访存地址打一拍 ⇒
   ⇒ 端口在拍 N-1 就呈现地址 A ⇒ 外部在拍 N 组合算数据 ⇒ CPU 在拍 N **组合消费**
   ⇒ ★★ **与预载核同拍消费 ✔**（⇒ 硬约束 **G-1k**）。
3. ★ **写侧同拍生效**：地址拍 N-1 发出 ⇒ 外部拍 N 收到写请求 ⇒ 拍 N 执行写
   ⇒ ★★ **写生效的绝对拍 = 拍 N = 与预载核一致 ✔**
   ⇒ ⇒ **无 load-after-store 问题 ⇒ 不需要 stall，也不需要 forwarding**。

⇒ ★★ **A0 裁决落地**（ADR-091 §D3a.4）：访存端口 = ★「**地址提前 1 拍发出 + 数据侧组合消费 + 写请求同拍发出**」。

⇒ ★ **从设备的强约束**：★ MUST 是**组合 slave**（组合地响应读 + 组合地执行写）⇒
★ Verilator 侧生成组合 `assign`（★ 上游 `c27543d` 已把 `aread` 对齐成 0 拍组合 assign ⇒ 可行）⇒
⇒ ★ 若从设备是「拍边界 slave」⇒ 退化为 §D3a.2 的**第 ② 档**（`ready` 恒真 + 1 拍响应）。

### K15 — ★ 「流水结构不同构」⇒ `cycles` / `pchash` **结构性不可比**

| 维度 | 预载核 | 开放核 | 后果 |
|---|---|---|---|
| 流水结构 | ★ **全组合 5 级**（`ip/cpu/cpu_factory_chmem.h:431-435` 无 stage 寄存器）⇒ 访存地址同拍产生同拍消费 | ★ **带访存级寄存器**（地址打 1 拍）⇒ 消费点晚 1 拍 ⇒ 后续 stage 全部延后 | ★★ **不同构** |
| `cycles`（tick count to done） | — | — | ★★ **必然不等** |
| `pchash`（per-cycle ordered FNV-1a over PC） | — | — | ★★ **必然不等**（PC 序列长度与内容都变） |
| `memsig`（折叠签名） | — | — | ★ **仍相等**（★ 只观测「写了哪一字 + 写了什么」的**集合/内容** ⇒ 与时序无关） |
| `regs` / `tohost`（终值） | — | — | ★ **仍相等** |

⇒ ★★ **H-3 判据修正**（ADR-091 §D6）：★ **3 终值逐位相等 + 2 轨迹走 ADR-089 例外白名单**。
⇒ ★ **例外 scope MUST 限定**：「开放核 vs 预载核」对比语料（★ **需新 corpus**）⇒
★ **双后端 parity 语料的 `cycles`/`pchash` MUST 保持逐拍相等**（★ 那是 A 阶段 52 case 基线，★ 不得被污染）。
⇒ ★ **先例**：ADR-088 §5.3.3 的 M-op「组合式 1 拍 vs TLM radix-2 多周期 ⇒ cycle 不同构不可比」= **同族问题**。

### K16 — ★ 为何「写语义」必须由**从设备**承担（★ A1 实施期发现的契约缺口）

★ **缺口**：`CpuBusReq` 原字段清单 = `address`(64) / `data`(64) / `burst_len`(8) / `id`(8) / `is_write`(1)
⇒ ★★ **没有「访问字节宽度」字段** ⇒ ⇒ sub-word store（`sb` / `sh`）的语义**无法表达**。

| 方案 | 写语义 | 依赖 | 代价 | 等价性 |
|---|---|---|---|---|
| ★ **A. 从设备做 byte-strobe RMW**（★ **采纳**） | 请求带 `address`(字节地址) + `data` + `size`(1/2/4/8) ⇒ 从设备按 `address` 低 2 位 + `size` 推 mask，合并后写回 | ★ **不依赖读响应** | ★ **契约增 1 个 3-bit 字段** | ★ **写与预载核同拍生效** ✔ ⇒ 无 load-after-store ⇒ ★ **无需 stall / forwarding** ✔ |
| B. CPU 侧做 RMW | CPU 必须先拿到「当前字」才能算合并值（★ 预载核靠 `aread` 拿到） | ★ **依赖读响应** | ★ 写晚 1 拍 ⇒ 需 **stall 或 forwarding** 整套机制 | ★ 写延迟 ⇒ cycles 再 +1 ⇒ 与预载核不等价 |

⇒ ⇒ ★★ **裁决 A**：★ 代价对比 = **1 个 3-bit 字段** vs **一整套 stall/forwarding 机制 + 额外 1 拍延迟** ⇒ ★ 明显更小
⇒ ★ 且 AXI 的 byte-strobe 语义是**行业惯例** ⇒ ★ 与「未来接 AXI4 适配」的方向一致。

⇒ ★ **读侧不需要 `size`**：★ 从设备**总是返回整个对齐字**（低 32 位有效）⇒
⇒ ★ CPU 侧按 `address` 低 2 位做 byte/half 提取（★ 与预载核 `dmem_chmem.h` 的 byte select 树**同款**）
⇒ ⇒ ★ 读语义与预载核**逐位同构**。

⇒ ★ **连带改动**：★ `CpuBusReq` 增 `size`（3 bit）⇒ ★ `ip/cpu/core/bus_iface_chmem.h`
+ ★ `tests/cpu/schema/test_cpu_bus_iface.cpp` MUST 同步 ⇒ ★ **H-1 仍满足**（3 ≤ 64）。

### K7 — ★ `ch_uint` 需全限定 `ch::core::ch_uint`

CppHDL 的 `ch_uint` **不在全局命名空间** ⇒ ★ 测试与实现里一律写 `ch::core::ch_uint<N>`（★ `ch::core::ch_bool` 同理）。

---

## 8. 范围边界（严格遵守）

| ✗ 不做 | 理由 |
|---|---|
| ✗ 改 `ip/cpu/plugins/*_chmem.h`（预载核） | ★ 现有 52 case parity 基线全部建立在它上面 ⇒ 改它就要重跑基线 |
| ✗ 改 `ip/cpu/cpu_factory_chmem.h` | ★ 同上（★ 且它含 P2 的配置面接入） |
| ✗ 删 TLM 代码 / 改 `uint_t.h` 模态 | ★ ADR-092 Phase-2（★ 门禁 G-1/G-2/G-3 未齐备） |
| ✗ 改 `bundles/mem_bundles.h` | ★ ADR-092 Phase-2（★ 老核 bundle 保持 POD） |
| ✗ 引入 CppHDL 的 Component / `ch_device` 体系 | ★ 实测不需要（K5）⇒ 引入是纯额外成本 |
| ✗ 改 `check_plugin_portability.sh` 的 Check 2/7 | ★ ADR-092 Phase-2（★ 现在双模仍在 ⇒ Check 全部有效） |
| ✗ 碰 GShare | ★ 已按 ADR-088 §5.3.3 C-3 结案为「本期不做」 |
| ✗ SoC 接线（S6） | ★ 依赖 MMU PTW / L1Cache 的 CH_MEM 实装（★ 两者都不存在） |

---

## 9. 风险

| # | 风险 | 触发 | 缓解 |
|---|---|---|---|
| **R-1** | ★ **工作量超窗口**（6~10 人日 vs v0.11.0 余量） | 与 A4 / BP 增量 3 争人力（roadmap R5） | ★ **Phase 1 独立可验收** ⇒ 可先交 Phase 1（框架层，收益独立）再决定续期；★ **S6 不在本 plan** |
| **R-2** | 端口方向搞反（G-1d） | 实现期 | 测试 case E 钉住 |
| **R-3** | ctx 作用域错（G-1c 同族） | 实现期 | `ctx_swap` 保护 + 断言 |
| **R-4** | ★ **多 TU include `ch.hpp` 链接失败**（K1） | 新核多文件 | ★ 头文件纪律（只 include 具体头）+ 门禁 grep |
| **R-5** | 预载核被误改 | 实施期手滑 | ★ 实施只碰 `ip/cpu/core/` + `include/cf/plugin/pipe_builder.h` ⇒ 收尾 diff 复核 |
| **R-6** | ★ parity 基线漂移 | 动到预载核或门禁 | ★ 每次 Phase 收尾跑 **52 case parity**（≈50 min）核对 |

---

## 10. 复现命令

```bash
# Phase 1 验证
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j4 --target chipforge_tests
./build/bin/chipforge_tests "[ports]"                       # ★ G-1 契约测试
python3 tools/parity/parity_gate.py --self-test             # 13/13
bash tools/verify_plugin_decision.sh && bash tools/check_plugin_portability.sh

# 零回归核对
./build/bin/chipforge_tests                                # 507 case / 506 passed / 1 baseline-known FAIL

# Phase 5 等价性自检（52 case）
python3 tools/parity/parity_gate.py --layer night --jobs 4 --report build/parity/report-night.json
```

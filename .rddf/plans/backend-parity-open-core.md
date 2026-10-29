# ADR-091 开放核实施路径（S1~S5）

> **change**: `backend-parity-open-core`（★ **已立项** ⇒ `openspec/changes/backend-parity-open-core/`）
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
  ⇒ ★ **v1.11 修正**（★ 2026-10-29）：★ `memsig` 由**从设备**产生（ADR-091 v1.11 §D3a.6）
    ⇒ ★ 但**载体仍是顶层 `ch_out`**（★ 名 `mem_sig` / `mem_sig_updates`）
    ⇒ ★ 理由（★ 实测，见 §K23）：★ `resolve_node_id` 遍历 `ctx.get_nodes()`
      ⇒ ★ **顶层端口节点也在其中** ⇒ ★ 解释器侧 runner **零改动**
      ⇒ ★ 且 Verilator 后端走 `port_access_`（★ **契约路径** ⇒ H-2）
      ⇒ ★ 而**内部 `ch_reg`** 只能靠 `--public-flat-rw` 穿透（★ **非契约路径**）
- [ ] **3.5** [GREEN] 握手成本处理（D3a.2）：`ready` 恒真 ⇒ 无阻塞流；`ready` 可拉低 ⇒ CPU 侧 MUST 有 stall 机制
- [ ] **3.6** 门禁 + 零回归

## 5. Phase 4 — S4：验证实现（`verify_mem_model_chmem.h`）

- [ ] **4.1** [RED] 测试：验证实现加载 ELF 并正确响应 imem/dmem 请求
- [ ] **4.2** [GREEN] 新建 `ip/cpu/core/verify_mem_model_chmem.h`：内存模型 + ELF 加载（★ 可参考 TLM 侧 `ip/cpu/picolibc_host_memory.h` 的**语义**，★ 但 MUST NOT include 它 —— 它在 ADR-092 的冻结集里）
- [ ] **4.3** 门禁 + 零回归

## 6. Phase 5 — S5：双后端必过（D0-c）+ 等价性自检（H-3）

- [ ] **5.1** [RED] 同一份测试 body 在 **解释器后端**与 **Verilator 后端**下都必过（★ 借鉴上游 `tests/dual_backend_fixture.h` 的 `DUAL_BACKEND_TEST_PAIR` 范式；★ **Verilator 侧 WARN-skip 不算 PASS**）
- [ ] **5.2** [GREEN] 单进程双后端对照（用 Phase 1 的 `create_simulator(backend)`）
- [ ] **5.3** [RED+GREEN] ★ **H-3 等价性自检**：同一 ELF 跑**开放核**与**预载核** ⇒ ★ **3 个终值观测量（`tohost` / `regs` / `memsig`）逐位相等** ⇒ ★ **2 个轨迹观测量（`cycles` / `pchash`）MUST 已登记为 ADR-089 例外**（★ scope 限定「开放核 vs 预载核」⇒ 双后端线不受影响）⇒ ★ **变异对照**：故意改开放核 1 处语义 ⇒ 该自检 MUST RED
  ⇒ ★ **v1.11 修正**（★ 2026-10-29）：★ 本条原文写「5 观测量逐位相等」⇒ ★ **已被 ADR-091 v1.6 修正**（★ 「流水结构不同构」⇒ `cycles` / `pchash` **结构性不可比**）⇒ ★ 本 plan 未同步 ⇒ ★ 此处补正
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

### K17 — ★ A1（访存端口化）实施期的实测约束裁决（★ **5 条变异对照**）

> ★ **ADR-091 v1.8 正文只留决策**（G-1l / G-1m 两条硬约束 + G-1c 修正 + 时序澄清）
> ⇒ ★ **本节承载全部推导、证据与「被证伪的怀疑」**。

#### K17.1 ★ 候选约束清单与**裁决表**（★ 8 条候选 ⇒ **只有 2 条**配登记为硬约束）

| # | 候选「实测约束」 | 判据 | 裁决 |
|---|---|---|---|
| ★ **G-1l** | payload「累加式 select」（`X = select(cond, v, X)`）**需上游 stage 先写 `X`** | ★ **gdb 栈**：`muximpl::create_instruction` → `lnodeimpl::id(nullptr)` ⇒ `Simulator::initialize` 段错误 | ✅ **登记为硬约束** |
| ★ **G-1m** | **`ch_bool` 禁用于 `?:` / `if`**（⇒ MUST 用节点级 `select()`） | ★ **变异对照 RED**：把 `select()` 换回 `?:` ⇒ 62 assertions ⇒ 2 处 FAILED（★ **写使能恒 0** + ★ **符号扩展失效**） | ✅ **登记为硬约束** |
| G-1n（候选） | `ch_bool` 的 `&&` / `\|\|` 失效 | ★ **变异对照**：改回 `&&` / `\|\|` ⇒ **92 assertions 全绿** | ❌ **证伪 ⇒ 撤回** |
| G-1o（候选） | `ch_uint` 的 `==` 走 `logic_buffer::operator==`（**节点指针比较**）⇒ 须显式限定调用 | ★ **变异对照**：改回 `a == b` ⇒ **92 assertions 全绿** | ❌ **证伪 ⇒ 撤回**（★ 显式限定调用仍保留，但降级为**防御性写法**） |
| G-1p（候选） | 跨宽构造 `ch_uint<64>(ch_uint<32>)` 只支持截断 ⇒ 须用 `zext<64>` | ★ **变异对照**：改用跨宽构造 ⇒ **92 assertions 全绿** | ⚠️ **降级**（★ 原始 null impl 日志真实，但**已被后续修复覆盖** ⇒ 当前形态等效） |
| G-1q（候选） | ctx 名必须唯一（否则节点 id aliasing） | ★ **变异对照**：全部场景用**同名 ctx** ⇒ **92 assertions 全绿** | ⚠️ **降级**为防御性写法（★ 该现象来自 **S3a** 的历史观测，本轮**未复现**） |
| G-1r（候选） | `ch_reg<ch_uint<1>> <<= ch_bool` 与 `<<= ch_uint<1>` 路径不同 | ★ 未做对照实验 | ⚠️ **降级**（★ 实现本就显式转换 ⇒ 无需约束） |
| G-1s（候选） | `bits<>` / `select()` 结果须显式转 `ch_uint` | ★ 未做对照实验 | ⚠️ **降级**为防御性写法（★ 显式化只提升可读性） |

⇒ ★★ **裁决纪律**（★ 已升为 ADR-091 §D6 **V-9**）：
★ 每条「实测约束」MUST 有**变异对照**（撤掉该写法 ⇒ 测试 MUST RED）⇒
★ 否则它只是**猜测**，会被后人当铁律继承 ⇒ ★ 本轮 **8 条候选里 4 条被证伪**。

#### K17.2 ★ ★ **真实的假失败根因 = 测试指令编码错误**（★ 最贵的误诊）

★ 症状：`lbu` 场景期望 `0x000000FF`（零扩展）实得 `0xFFFFFFFF`（符号扩展）。
★ 期间先后**误判**为 3 个不同根因（★ 每次都「修」了实现）：

| 轮次 | 当时的假设 | 实际 |
|---|---|---|
| 1 | ★ `ch_bool` 的 `\|\|` 失效 ⇒ 符号扩展标志恒 0 | ❌ |
| 2 | ★ `ch_uint` 的 `==` 走指针比较 ⇒ `funct3` 判定恒真 | ❌ |
| 3 | ★ `?:` 的 contextual conversion | ✅ **成立**（但**只**解释「符号扩展失效」，**不**解释「`lbu` 被当 `lb`」） |

⇒ ★★ **真正的根因**：★ 我写的 `lbu` 指令编码 `0x00030183` 把 `0x3` 放到了 **rs1** 字段（bits[19:15]），
⇒ ★ **funct3（bits[14:12]）实际为 0** ⇒ ★ `lbu` 被解码成 `lb` ⇒ ★ 符号扩展**正确**施加 ⇒ ★ 假失败。

★ **RISC-V I 型编码纪律**：`funct3[14:12] | rd[11:7] | rs1[19:15] | imm | opcode[6:0]`
⇒ ★ `lbu` 正确编码 = `4<<12 | 3<<7 | 0x03` = **`0x00004183`**（★ 原值 `0x00030183` 差一个字段位置）。

⇒ ★★★ **教训（可迁项）**：★ **假失败排查的 FIRST 检查项 = 输入编码 / 期望值本身**
⇒ ★ **不是**实现。⇒ ★ ★ 本轮**为此浪费了 4 轮**（★ 每轮都改实现 + 加诊断端口）
⇒ ★ ⇒ ★ **流程改进**：★ 每次断言失败 ⇒ ★ **先手算期望值**（★ 不看代码）⇒ ★ 再看实现。

#### K17.3 ★ `ch_reg` 的时序模型（★ ADR-091 §D3a.4 的「时序澄清」依据）

★ `ch_reg` 的 next 是**组合表达式** ⇒ ★ 每次 tick 的 sequential 阶段用
★ 「**tick N-1 末的 DAG 状态**」重新求值 ⇒ ⇒

```
tick N 的 sequential :  addr_reg_ ← expr(tick N-1 末的状态)
tick N 的组合        :  端口呈现 addr_reg_（= 拍 N-1 末算出的地址）
host 读端口         :  在 tick N 之后读到 ⇒ 在 tick N+1 之前供数
⇒ 消费拍 N+1 的数据  对应  地址拍 N        ⇒ 延迟 1 拍 ✔
```

⇒ ★★ 这正是 **G-1k**（访存地址 MUST 提前 1 拍可见）的成立机制
⇒ ★ 也是 S5「验证实现 MUST 提前 1 拍供数」的**机制依据**。

#### K17.4 ★ `Result<P>` 的**悬垂引用**陷阱（★ 本轮自引入 ⇒ 已修）

★ `declare_in/out` 改成 `Result<Port>`（ADR-047）后，★ 惯用解包写法会引入悬垂：

```cpp
template <typename P> P& must(Result<P> r) { REQUIRE(r.has_value()); return *r; }
auto& req_addr = must(pb.declare_out<U64>("x"));   // ❌ r 是临时 ⇒ *r 悬垂
```

⇒ ★ full-expression 结束后 `Result<P>` 临时对象析构 ⇒ ★ `port` 的 `impl_` 失效
⇒ ★ **端口「消失」**（★ 最坏假绿：网表少端口 ⇒ 门禁照绿）。

⇒ ★ **正确写法**：★ `must` **按值返回** + 调用方用**值语义**（★ C++17 copy elision
⇒ ★ 不触发 `port` 的 deleted 拷贝赋值）：

```cpp
template <typename P> P must(Result<P> r) { REQUIRE(r.has_value()); return *r; }
auto req_addr = must(pb.declare_out<U64>("x"));      // ✅
```

#### K17.5 ★ ADR-047 Check 10 与 `declare_in/out` 的 `throw`（★ S1 遗留缺陷）

★ S1 引入的 `declare_in/out` 用了 `throw PluginException(...)` ⇒ ★ **违反 ADR-047 Check 10**
⇒ ★ **门禁红**（★ `check_plugin_portability.sh` [10/12] FAIL）⇒ ★ **本轮修复**：
★ 改 `Result<Port>` + `std::unexpected(PluginError::NullContextPtr)` ⇒ ★ fail-closed 语义不变
⇒ ★ 调用面 = ★ **3 个测试文件**（★ 生产代码零调用）⇒ ★ 破坏面可控。

### K18 — ★ C1：「旧产物顶替」假绿通道的系统性排查（★ 3 个通道 + 4 条实测）

> ★ **触发**：★ 本轮实测发现 TLM binary **自 ADR-091 S1 起编译失败**
> ⇒ ★ 此前多轮报告的「TLM 509 case / 508 passed / 1 failed」全部是**旧产物**的数字
> ⇒ ★ ⇒ ★ **「回归通过」这个信号本身不可信** ⇒ ★ ⇒ ★ C1 立项。

#### K18.1 ★★★ **三个「旧产物顶替」通道**（★ 逐个封堵）

| # | 通道 | 机制 | 封堵 |
|---|---|---|---|
| ★ **C1-①** | ★ **框架代码在 `#ifdef` 内声明，两种模式只编一种** | ★ `built_` 声明被置在 `#ifdef CF_PLUGIN_USE_CH_MEM` 内 ⇒ ★ **TLM 模式编译失败** ⇒ ★ 旧 binary 静默顶替 | ★ 成员移出 `#ifdef`（★ `built_` 是**框架状态**，与后端无关）⇒ ★ **真实基线 509 → 515** |
| ★ **C1-②** | ★ **两个测试 binary 的源文件集合**不对称**（TLM = GLOB / CH_MEM = 显式列表） | ★ CH_MEM-only 测试（用 `declare_in/out` / `elaborate()` / `create_simulator()`）被 TLM 的 `GLOB_RECURSE` 收集 ⇒ ★ **TLM 111 个编译错误** | ★ 新增 `CHMEM_ONLY_TEST_SOURCES` + **CMake 断言**（★ 排除集合 ⊆ CH_MEM 源集合）⇒ ★ 防「只排一边」 |
| ★ **C1-③** | ★ **ctest 本身不构建项目** | ★ 源码编译失败 ⇒ ★ ctest **照跑旧产物** ⇒ ★ 报告「通过」 | ★ 新增 **build fixture**（★ `cf_build_all`）⇒ ★ 编译失败 ⇒ 依赖测试 **Not Run** ⇒ ctest 整体 FAIL ⇒ **fail-closed** |

#### K18.2 ★★ **「回归通过」信号此前根本不存在**（★ 恒红）

★ `ctest` 的 `chipforge_tests` entry 跑**全量** binary ⇒ ★ 其中含 baseline-known
失败 ⇒ ★ **entry 恒红** ⇒ ★ 「全量回归通过」**从来就不存在** ⇒ ★ 任何依赖
退出码的判断都在误判。

⇒ ★ **修法**：★ `tools/run_catch2_with_baseline.sh`（★ baseline-aware）
⇒ ★ 判定「是否有**新**失败」⇒ ★ 清单 = `tools/baseline_known_failures.txt`
⇒ ★ **按 binary 分节**（★ `[tlm]` / `[chmem]`）⇒ ★ 每条附「根因 + owner + 收敛期限」
⇒ ★ ★ **纪律**：★ 清单不得靠扩张来消红（★ 同 ADR-089 例外白名单）。

#### K18.3 ★★★ **★ Catch2 的 XML reporter 会破坏「被测代码捕获 std::cerr」的测试**（★ 最有价值的可迁项）

★ **实测对照**（★ 同一 binary、同一 filter、同一时刻）：

| reporter | `tests/cpu/test_cpu_factory_isa_xlen.cpp` 的 3 个 case |
|---|---|
| **console** | ★ **全过** |
| **XML** | ★ **3 个全挂** |

★ **机制**：★ Catch2 的 XML reporter 会为**每个 test case** 捕获 stdout/stderr
（★ 实现 = 替换 `std::cerr` 的 rdbuf ⇒ 捕获内容进 `<Output>` 节点）
⇒ ⇒ ★ 被测代码自己的 `CerrCapture`（★ 卸掉 Catch2 的捕获 buffer）
⇒ ⇒ ★ 被测代码的 warn 写进了 **Catch2 的 buffer** ⇒ ⇒ ★ `cap.str()` 为空
⇒ ⇒ ★ 断言 `find("[declared-reserved]") != npos` 失败。

★ ★ **⇒ 两条可迁结论**：
  ① ★ **不要用 XML reporter 做「失败用例名单」的唯一来源**（★ 它会改变被测行为）
  ② ★ ★ **依赖 `std::cerr` 捕获的测试是脆弱测试** ⇒ ★ 它们的通过**依赖 reporter
  实现细节** ⇒ ★ **存量 follow-up**（★ 见 §K18.5 L2）

#### K18.4 ★★★ **Catch2 console 输出的三个坑**（★ 解析器必须处理）

| # | 坑 | 现象 | 处理 |
|---|---|---|---|
| ★ ① | ★ **两种汇总形态** | ★ 有失败 = `test cases: N \| M passed \| K failed`；★ 全通过 = `All tests passed (A assertions in N test cases)` ⇒ ★ **后者没有 `test cases:` 行** | ★ 两种都匹配（★ 只匹配前者 ⇒ ★ 全绿时误判 fail-closed） |
| ★ ② | ★ **warning 也用「分隔线 / 名字 / 分隔线」格式** | ★ `PTW_Has_AdvanceFromRealMemory_Method` 的 warning 块被误判为失败 | ★ 按块体内的 **`: FAILED:`** 标记区分 |
| ★ ③ | ★ 失败块后紧跟 `<file>:<line>` 行 | ★ 单看分隔线会把它误当 case 名 | ★ 用**成对分隔线**模式提取 |

#### K18.5 ★★ **C1 的遗留项（★ 诚实标注，未实现）**

| # | 遗留 | 归属 |
|---|---|---|
| ★ **L1** | ★ `cmake --build <dir>`（**all**）会触发 **ExternalProject re-configure** ⇒ ★ 该独立 cmake 步骤**找不到编译器** ⇒ ★ 失败（★ 既存环境脆性，★ 与本轮改动无关）⇒ ★ 故 fixture 与回归脚本**只 build 测试 target** | ★ 独立 change（★ 给 ExternalProject 传 `CMAKE_CXX_COMPILER` / `CXX`） |
| ★ **L2** | ★ `test_cpu_factory_isa_xlen.cpp` 的 3 个 case **依赖 reporter 实现**（★ XML 下失败）⇒ ★ 脆弱测试 | ★ 独立 change（★ 让 CpuFactory 的 warn 可注入 ⇒ 不依赖全局 `std::cerr`） |
| ★ **L3** | ★ CI（`architecture-gates.yml`）**不构建项目、不跑测试** ⇒ ★ C1 的 build fixture **只在本地 ctest 生效** | ★ 归 **A4 `backend-parity-ci`**（B2） |
| ★ **L4** | ★ **`[mmu-verilator]` 的 `mmu_bare_plumbing_tohost1_baseline_5_elf` 是顺序敏感 flake**（★ 单跑 3/3 PASS ⇒ ★ 全量跑 FAIL，`tohost == 0`）⇒ ★ 根因**未最终定位** | ★ 独立 change（★ owner: ip/mmu）⇒ ★ 已登记进 baseline 清单（★ 附「根因未定位」标注） |

### K19 — ★ C2：★ 存量 G-1a~G-1k 的**逐条变异对照复核**（★ 11 条）

> ★ **ADR-091 v1.9 正文只留覆盖度矩阵**（★ 决策层）⇒ ★ **本节承载变异细节与输出**。

#### K19.1 ★ **复核方法**

★ 对每条约束施加「**撤掉该写法**」的变异 ⇒ ★ 跑对应判据 ⇒ ★ **判据必须 RED**
⇒ ★ 才算该约束「**被机检覆盖**」（★ ADR-091 §D6 **V-9** 纪律）。
★ ★ **注意**：★ 变异必须**干净**（★ 只改被测的那一处）⇒ ★ 本轮有 2 次
★ 首版变异**无效**（★ ① 误删 `#ifdef` 的配对 `#endif` ⇒ 报「unterminated #ifndef」
★ ② 正则用 `[^/]*` 吞掉前导空格 ⇒ ★ 缩进判据失效）⇒ ★ **已重做并复测**。

#### K19.2 ★ 逐条结果（★ 11 条）

| # | 变异手法 | 判据 | 结果 |
|---|---|---|---|
| ★ **G-1a** | 删掉 `pipe_builder.h` 中含 `declare_in/out` 的 `#ifdef CF_PLUGIN_USE_CH_MEM`（★ 行 331/557） | 编 **TLM target** | ★ **rc=2 / 268 errors** ⇒ 首个 = `'ch' was not declared in this scope`（★ `core/io.h` 的 include 本身也在条件化块内）⇒ ★ CH_MEM target 不受影响（rc=0） |
| ★ **G-1b** | 在 `pipe_builder.h` 加 `#include <ch.hpp>` | 编 **TLM target**（★ CH_MEM target 带 `-Wl,--allow-multiple-definition` ⇒ **会掩盖**链接错误 ⇒ 必须用 TLM） | ★ **rc=2 / 29 errors** ⇒ 全部是 `reference to 'detail' is ambiguous` ⇒ ★ ★ **不是** ADR 原写的 multiple definition ⇒ ★ **后果描述须更正**（★ `module.h:13` 缺 `inline` 仍然成立，★ 但首个失败是**命名冲突**） |
| ★ **G-1c** | 删掉 `declare_in/out` 的 `if (!ctx_) return unexpected(...)` | `[ports]` 的 null-ctx case | ★ **RED**（9 case ⇒ 1 failed） |
| ★ **G-1d** | —— | —— | ★ **编译期类型系统**保证（★ `ch_in`/`ch_out` 不可互换）⇒ ★ 运行期另有行为判据（case E：★ 被内部驱动的 `ch_in` 不进顶层） |
| ★ **G-1f** | ① `AddrT address{0};` ② `AddrT address{ch::core::ch_literal<0,64>{}};` | ★ **C2 新增**的 `[bus-iface]` 机检 case | ★ **两次都 RED**（★ 判据 = 「**有 ctx** 下默认构造 bundle ⇒ 字段 impl 应为 null」） |
| ★ **G-1h** | 删掉 `elaborate()` 内部的 `ch::core::ctx_swap ctx_guard(&ctx);` | ★ **C2 新增**的 G-1h case | ★ **RED**（9 case ⇒ 1 failed） |
| ★ **G-1i** | 在 `dmem_port_chmem.h` 的**成员初始化器**里创建 `ch_uint` 字面量 | ★ **C2 新增**的 G-1i case | ★ **全绿** ⇒ ★ ⇒ ★ 该 case 只钉住**机制**（★ 无 ctx ⇒ null impl），★ **抓不到「谁违反」** ⇒ ★ 覆盖度 = **中** |
| ★ **G-1j** | 删掉 `elaborate()` 里的「未 build ⇒ 自动 build」 | ★ **C2 新增**的 G-1j case | ★ **RED**（9 case ⇒ **2 failed**） |
| ★ **G-1e / G-1g** | —— | —— | ★ **无机检** ⇒ ★ 变异只能观察到**上游错误日志**（★ 非抛异常）⇒ ★ 难以断言 ⇒ ★ 覆盖度 = **弱** |
| ★ **G-1k** | 去掉地址 `ch_reg`（地址直接组合输出） | —— | ★ **不可观测** ⇒ ★ host 供数时序不变 ⇒ ★ **当前测试无法区分** ⇒ ★ 判据需 **A6** 的「开放核 vs 预载核」时序对比 |

#### K19.3 ★ **G-1i 的静态机检尝试与放弃**（★ ★ 避免引入假红）

★ 想法：★ 在 `tools/check_plugin_portability.sh` 加一条静态检查，扫「类成员声明行含
`ch_literal<`」⇒ ★ 形态：`grep -rnE "^ {0,4}[^ /].*ch_literal<" ip/ --include="*.h"`。

★ ★ **实测结果：★ 该判据误报严重** ⇒ ★ 命中 11 个文件 / 上百行 ⇒ ★ 因为
★ **namespace 内的自由函数体**语句缩进也可能 ≤ 4（★ 例
`ip/cache/plugins/l1_cache_refill_fsm_chmem.h:167`）⇒ ★ ⇒ ★ **放弃该方案**
⇒ ★ ★ **理由**：★ **门禁的假红比漏检更坏**（★ 会让人养成「忽略门禁」的习惯）
⇒ ★ ⇒ ★ G-1i 保留「**机制锚**」单测（★ 钉住「无 ctx ⇒ null impl」）⇒ ★
★ ★ **覆盖度诚实标注为「中」**（★ 覆盖机制，不覆盖「谁违反」）。

#### K19.4 ★ ★ **C2 的副产物：★ C1 的 CMake 断言只做了单向**（★ 已修）

★ C2 新增的 G-1h/i/j 三个 case 用了 CH_MEM-only API（★ `elaborate()` / `ch_literal`）
⇒ ★ 而它们被追加在 `test_pipe_builder_ports.cpp` 的 **`#ifdef` 之外**
⇒ ★ ⇒ ★ **TLM target 编译失败**（★ 11 errors）⇒ ★ ★ **正是 C1-② 的同一通道**
⇒ ★ ⇒ ★ C1 的断言只检查了「CH_MEM-only ⊆ CH_MEM 源集合」（★ **单向**）
⇒ ★ ⇒ ★ **补双向断言**（★ CH_MEM-only **MUST NOT** 留在 TLM 源集合）
⇒ ★ ⇒ ★ 并把 `test_pipe_builder_ports.cpp` **整体**移出 TLM 源集合
（★ ★ 它本质是 CH_MEM-only：★ 在 TLM 下只贡献 1 个 case，★ 却是用 CH_MEM API 的）。

★ ★ **TLM 基线随之变化**：★ 515 → **514 case**（★ 67466 → 67463 assertions）。

### K20 — ★ A2（分支重定向 + flush）的逐拍推导与变异对照

> ★ **ADR-091 v1.10 正文只留契约**（§D3a.5 + G-1n + V-10）⇒ ★ 本节承载推导与证据。

#### K20.1 ★ 三个 1 拍延迟的逐拍推导（★ 抑制链）

| 量 | 定义 | 说明 |
|---|---|---|
| `pc_reg(N)` | 拍 N 的取指地址 | ★ `req_addr(N) = pc_reg(N)` |
| `INSTRUCTION(N)` | `instr_reg(N)` = 地址 `pc_reg(N-1)` 的指令 | ★ 与 `PC(N) = pc_lag(N) = pc_reg(N-1)` **对齐** |
| `BRANCH_TAKEN(N)` | 地址 `pc_reg(N-1)` 的指令的分支结果 | ★ 生产者 = branch（A4） |
| `pc_reg(N+1)` | `select(taken(N), target, pc_reg(N)+4)` | ★ **taken(N)=1 ⇔ `pc_reg(N)` 是错路** |
| `FLUSH(N)` | `flush_reg(N)` = `taken(N-1)` | ★ ★ **与 `INSTRUCTION(N)` 对齐**（二者都对应 `pc_reg(N-1)`） |
| `we_reg(N+1)` | `is_store(N) ? !FLUSH(N) : 0` | ★ dmem 的 `we_reg` **本身打 1 拍** |
| ⇒ | ★ **`FLUSH(N)=1` ⇒ 该 store 的写请求在拍 N+1 发出且 `we=0`** | ★ 抑制成立 |

★ ★ **读取口径陷阱**（★ 本轮实测踩过 3 次）：
★ `sim.tick()` **之后**读端口 ⇒ ★ 读到的是**下一拍**的值
（★ `ch_reg` 的 next 在 tick 的 sequential 阶段已应用）
⇒ ★ 即 `trace[i]` = 拍 `i+1` 的值 ⇒ ★ 断言 MUST 按此口径写。

#### K20.2 ★ 4 个变异对照（★ 全部 RED ⇒ V-10 满足）

| # | 变异 | 结果 |
|---|---|---|
| M1 | `flush_reg <<= 0`（`FLUSH` 恒 0） | ★ **2 case 失败** |
| M2 | `we = is_store`（去掉 flush 抑制） | ★ **1 case 失败** |
| M3 | `pc <<= pc + 4`（不重定向） | ★ **1 case 失败** |
| M4 | ★ 提供者 plugin 注册在 ibus **之前**（G-1n） | ★ **1 case 失败** |

⇒ ★ 恢复后 **339 assertions / 6 cases 全绿**。

#### K20.3 ★ G-1n 的发现过程（★ 最贵的一次误判）

★ 症状：★ `flush` 端口输出**正确**（序列 0,1,0,0），但 `dmem_req_we` **恒 1**（抑制未生效）。

★ 诊断链（★ 3 步才定位）：

| 步 | 假设 | 结果 |
|---|---|---|
| 1 | ★ `ch_uint<1>(ch_bool)` 取反折成常量（G-1m 同族） | ❌ 改成 `select` 后仍失败 |
| 2 | ★ 混宽 `operator&(ch_bool, ch_uint<1>)` 语义不明 | ❌ 改成纯嵌套 `select` 后仍失败 |
| 3 | ★ 加诊断打印 `has(FLUSH)` | ★ **`hasFLUSH=0`** ⇒ ★ dmem 根本读不到 |

★ 再追：★ `stage_names()` = `branch memory fetch branch memory`
⇒ ★ 说明注册顺序是「提供者 → ibus → dmem」
⇒ ★ `canonical_stage_order()` 按**首现序** ⇒ `[branch, memory, fetch]`
⇒ ★ `memory NORMAL`(dmem) **先于** `fetch NORMAL`(ibus) 执行 ⇒ ★ `FLUSH` 未写。

★ ★ **真正根因**：★ `PipeBuilder::build()` 才按 `plugins_` 顺序调用 `setup()`
⇒ ★ 测试里**直接**调 `pb.at_stage(...)` 的提供者会**先于所有 plugin** 注册
⇒ ★ ⇒ ★ **提供者 MUST 做成 plugin，且注册在 ibus/dmem 之后**
⇒ ★ 首现序 = `fetch, branch, memory` = **流水顺序** ✔

⇒ ★★ **升为硬约束 G-1n**（★ 装配骨架 A4 同样受约束：★ **MUST 按流水顺序注册 plugin**）。

★ ★ **通用教训**：★ 「跨 stage payload 读不到」的**第一诊断动作** = ★
★ **打印 `has(key)` + `stage_names()`**（★ 而不是怀疑类型/运算语义）。

### K21 — ★ `memsig` 观测来源的推导与证据（★ A2 收尾审查）

> ★ **ADR-091 v1.11 正文只留契约**（§D3a.6 + G-1o + V-11~V-15）⇒ ★ 本节承载推导与证据。

#### K21.1 ★ 预载核折叠算法逐位拆解（★ `dmem_chmem.h:244-270`）

```
sig_val    = mem_sig(N)                     // 32-bit，当前签名
sig_upd    = mem_sig_updates(N)             // 32-bit，已发生写次数
mix        = word_addr * 31 + store_wdata + sig_upd * 0x9E3779B1
sig_next   = is_store_active ? ((sig_val * 16777619) ^ mix) : sig_val
sig_upd'   = is_store_active ? sig_upd + 1  : sig_upd
```

★ 三个**不可省略**的细节（★ 抄错任一项 ⇒ 与预载核不等 ⇒ H-3 RED）：

| 细节 | 说明 |
|---|---|
| ★ `store_wdata` = **RMW 后的最终字** | ★ sub-word store 合并后的**字面值**（★ 不是原始 store 数据）⇒ ★ **只有持有内存者能算** |
| ★ `mix` 纳入**写次数** | ★ 否则「同一 (addr,data) 但发生在不同时刻」的多笔写 ⇒ 折叠值相同 ⇒ **区分度不足** |
| ★ 运算顺序 | ★ `sig * 16777619` **先乘后 XOR**（★ 不是 `sig ^ (mix * 16777619)`）⇒ ★ 32-bit 截断语义逐位一致 |

★ ★ **`store_wdata` 的依赖性**是本裁决的**决定性依据**：
★ `store_wdata` 需要「写之前的当前内存内容」⇒ ★ 核**不持有内存** ⇒ ★ **核算不出** ⇒ ★ 只能由从设备算。

#### K21.2 ★ 采集机制核验（★ 为何「从设备自算」⇒ runner 零改动）

★ `parity_sim_runner.cpp:249-255`：

```cpp
std::optional<std::uint32_t> resolve_node_id(ch::core::context& ctx,
                                             const std::string& name) {
  for (const auto& node : ctx.get_nodes()) {
    if (node && node->name() == name) return node->id();
  }
  return std::nullopt;
}
```

★ 三个事实 ⇒ 结论：

| 事实 | 依据 |
|---|---|
| ★ 按**节点名**查，**不限归属**（★ 不区分「核内」还是「从设备」）⇒ ★ 从设备的节点**同样可读** | ★ 上面的实现（★ 遍历 `ctx.get_nodes()` 全体） |
| ★ 名字 `mem_sig` / `mem_sig_updates` 与预载核**一致** ⇒ ★ **同名即可对照** | ★ `dmem_chmem.h:112,120` |
| ★ Verilator 侧同款（★ 也遍历 `ctx.get_nodes()` 取名）⇒ ★ **两后端采集口径一致** | ★ `cpu_verilator_sim.cpp:288-290` |

⇒ ★★ **结论**：★ 只要从设备把 `mem_sig` / `mem_sig_updates` 放进网表 ⇒ ★ **parity runner 一行不改**。

#### K21.3 ★ 5 个备选方案的代价对比（★ 用户裁决 Q1/Q4）

| 备选 | 结论 | 理由 |
|---|---|---|
| **A. 从设备自算 + 沿用 `mem_sig`/`mem_sig_updates`** | ✅ **采纳** | ★ runner 零改动 + 与预载核逐位可对照 + 白名单零新增 |
| **B. 核侧算（从设备提供「当前字」给核，核再折叠）** | ❌ 拒绝 | ★ 违反 §D3a.4「CPU 侧不做 RMW」⇒ ★ 需新增「读当前字」通路 ⇒ ★ **重新引入写延迟** ⇒ ★ 与「写无延迟」冲突 |
| **C. host 侧算（全内存 dump）** | ❌ 拒绝 | ★ `Simulator::get_memory()` 是**空 stub**（★ 被未启用的 `ENABLE_MEMORY_API` 包住）⇒ ★ 无读出通道 ⇒ ★ ADR-089 §D3 ③ 已把它列为「待上游」的增强项 |
| **D. 从设备算但换节点名（如 `open_mem_sig`）** | ❌ 拒绝 | ★ runner 按名查 ⇒ ★ 换名 ⇒ **runner 要改** ⇒ ★ 且失去与预载核的**逐位对照**（★ 同名才有对照价值） |
| **E. 从设备义务 = 可选（缺失则登记白名单）** | ❌ 拒绝 | ★ 用户裁决 Q4=A ⇒ ★ 强契约 ⇒ ★ 「缺失」与「豁免」混淆 ⇒ ★ **掩盖真缺陷** ⇒ ★ 且需在 ADR-089 新增白名单条目（★ 扩张权归 architect） |

★ ★ **Q4 选 A 的关键收益**：★ `exceptions.json` 现仅 1 条**合成**自检条目
⇒ ★ `memsig` **未被豁免** ⇒ ★ **必须实现** ⇒ ★ 强契约把「实现」变成**义务**而非「选项」。

#### K21.4 ★ 端口名二义的发现过程（★ A2 收尾审查 —— 第 4 个假绿通道）

★★ **触发**：★ 审查「`memsig` 归从设备后，parity runner 怎么按名读观测」⇒ ★ 顺带查「开放核的顶层观测端口名是否稳定」。

★ **发现 1（假绿源）**：★ `top_port_declared()` 用**子串匹配**：

```cpp
const bool is_decl = line.find("input") != npos || line.find("output") != npos;
if (is_decl && line.find(name) != npos) return true;   // ★ 子串！
```

★ 实测生成物（`open_core_ports_top.v:11`）⇒ ★ 顶层端口**实际叫 `open_pc_1`**（不是 `open_pc`）
⇒ ★ 但断言 `top_port_declared(vpath, "open_pc")` **通过**（★ `"output [31:0] open_pc_1"` 含子串 `"open_pc"`）
⇒ ★★ **假绿**：★ 端口被改名这件事被测试**掩盖**。

★ **发现 2（契约级）**：★ 顶层端口**真的**被改名了 ⇒ ★ 为什么？

| 环节 | 事实 |
|---|---|
| codegen 建名表 | ★ `codegen_verilog.cpp:64-76` ⇒ 按 `eval_list` 顺序建 `name_counts` ⇒ **内部 node 先占名** ⇒ 重名者加 `_N` 后缀 |
| 内部 `ch_reg` 名 | ★ `ibus_port_chmem.h:162` = `"open_pc"` · `:178` = `"open_flush"` ⇒ ★ **与顶层端口同名** |
| ⇒ 结果 | ★ 顶层端口被改名 `open_pc_1` / `open_flush_1` ⇒ ★ **两后端端口名不一致**（★ Simulator 侧 `open_pc` / Verilator 侧 `open_pc_1`） |

★ **发现 3（二义）**：★ `ctx` 里**两个节点同名**（内部 `ch_reg` + 顶层端口）
⇒ ★ `resolve_node_id(ctx, "pc")` 取**首个**匹配 ⇒ ★ **观测可能读到 `ch_reg` 而非端口**。

★ **发现 4（覆盖缺口）**：★ `test_cpu_open_core_branch.cpp` include 了 `codegen_verilog.h`
但★ **没调 `toVerilog`** ⇒ ★ A2 新增的 `flush_reg` + `open_flush` 端口**从未经过 codegen**。

★ ★ **裁决（升为硬约束 G-1o）**：★ **顶层端口名 MUST NOT 与内部 `ch_reg`/`ch_node` 名同名**
⇒ ★ 内部 reg 用 `_reg` 后缀（`open_pc_reg` / `open_flush_reg`）⇒ ★ 端口名保持契约名。

★ ★ **对照 `dmem`**（★ 无此病）：★ 内部 reg 名 `open_dmem_addr` / `open_dmem_data` / …
⇒ ★ 与端口名 `dmem_req_addr` / `open_rd_data` **不同名** ⇒ ★ 生成的端口名**全干净** ✔
⇒ ★ 证实「**只有 ibus 侧撞名**」⇒ ★ 修法就是内部 reg 加后缀。

★ ★ **通用教训**（★ 与 §K20.3 同族）：★
★ 「顶层端口名」类断言 MUST 用**精确名匹配** ⇒ ★ 子串匹配会掩盖改名。
★ ⇒ ★ **G-1o + V-14 + V-15** 三条配套：★ 约束 + 变异对照 + 断言精确化。

### K22 — ★ A2.5（端口名解耦 + 断言精确化）的变异对照

> ★ **ADR-091 v1.11 正文只留契约**（G-1o + V-14 + V-15）⇒ ★ 本节承载推导与证据。
> ★ **前置**：§K21.4 的「端口名二义」发现过程。

#### K22.1 ★ 修复内容（★ 3 处改动）

| # | 位置 | 改动 |
|---|---|---|
| 1 | `ip/cpu/core/ibus_port_chmem.h` | ★ 内部 `ch_reg` 名 `open_pc` → **`open_pc_reg`** · `open_flush` → **`open_flush_reg`**（★ G-1o）⇒ ★ 顶层端口名保持契约名 |
| 2 | `tests/cpu/open_core_verilog_helper.h`（★ **新建**） | ★ `top_port_declared` 改**精确名匹配**（★ 取端口声明行的**最后一个标识符 token**）⇒ ★ 共享实现（★ 原先 ibus/dmem 各有一份**相同的子串匹配**⇒ ★ 改一处漏一处 ⇒ ★ 漂移风险） |
| 3 | `tests/cpu/test_cpu_open_core_branch.cpp` | ★ **补 `toVerilog`**（★ A2 之前 `branch` 测试从未生成 Verilog ⇒ ★ 新增的 `flush_reg` + `open_flush` 端口**从未过 codegen**）⇒ ★ 加正向断言（`open_pc`/`open_flush` 精确存在）+ ★ **反向断言**（`open_pc_1`/`open_flush_1` MUST 不存在） |

★ ★ **修复后的实测**（★ 生成物 `/tmp/opencode/open_core_branch_top.v`）：

```
module top (
    ...
    output [31:0] open_pc,        ← ★ 契约名（无 `_1` 后缀）
    output  open_flush            ← ★ 契约名
);
```

⇒ ★ 内部 reg 名为 `open_pc_reg` / `open_flush_reg`（★ `_` 前缀的 `_open_pc_reg` 是
★ 顶层 port 的内部 wire 镜像，★ 非冲突）。

#### K22.2 ★ 4 个变异对照（★ 全部实测）

| # | 变异 | 结果 | 判据 |
|---|---|---|---|
| **M-V14** | ★ 内部 `ch_reg` 改回与端口同名（`open_pc` / `open_flush`）⇒ ★ helper 保持精确 | ★ **2 case 失败** | ⇒ **V-14 满足**（★ 精确匹配能捕获端口改名） |
| **M-V15** | ★ helper 单独降级回子串匹配（★ 实现已修） | ★ 全绿（357） | ★ 预期（★ 单改 helper 不触发缺陷） |
| **M-V15+V14** | ★ 子串匹配 + 内部 reg 撞名 | ★ **1 case 失败** | ★★★ **意外** ⇒ ★ 捕获来自**反向断言** ⇒ ★ 反向断言有独立价值 |
| **M-V15b** | ★ 子串匹配 + **去反向断言** + 内部 reg 撞名 | ★ ★ **全绿（355）= 假绿复现** | ⇒ ★ **精确匹配是必需的** ⇒ **V-15 满足** |

★ ★★ **关键结论（★ 由 M-V15+V14 与 M-V15b 的对照得出）**：

| 断言组合 | 能否捕获「端口被改名」 |
|---|---|
| ★ 子串匹配 + 无反向断言 | ❌ **不能**（★ M-V15b 实测全绿 ⇒ ★ **假绿**） |
| ★ 子串匹配 + **有**反向断言 | ✔ 能（★ M-V15+V14 实测 1 case 失败） |
| ★ **精确匹配**（正向）+ 无反向断言 | ✔ 能（★ M-V14 实测 2 case 失败） |
| ★ **精确匹配 + 反向断言**（★ 当前实现） | ✔ 能（★ 双保险） |

⇒ ★⇒ ★ **V-15 的准确表述**：★ 正向断言 MUST **精确匹配**（★ 这是**主断言**）
⇒ ★ 反向断言是**第二道护栏**（★ 它连「子串匹配」的漏洞都能堵 ⇒ ★ 独立价值）
⇒ ★ 二者**并存**（★ 当前实现）⇒ ★ **最坏情况**：helper 被降级 + 反向断言被删 ⇒ ★ 才假绿（★ M-V15b）

★★ ★ **诚实标注**：★ **V-15 在 ADR v1.11 的原表述有歧义**
（★ 「断言一个不完整的名字 ⇒ MUST RED」★ 未说明是「正向断言」还是「反向断言」）
⇒ ★ **实测后的准确表述见 §K22.2 表格** ⇒ ★ **ADR 的 V-15 措辞待下一轮 architect 澄清**
（★ 本轮**不改**已签批的 ADR 决策文字 ⇒ 归 backlog）。

#### K22.3 ★ 一个操作失误（★ 记入教训）

★ 变异 M-V15b 收尾时**误用 `git checkout -- <file>`** 恢复被变异文件
⇒ ★ **连带丢掉了该文件本轮尚未提交的全部改动**（★ `test_cpu_open_core_branch.cpp`
★ 的 `toVerilog` + 端口断言）⇒ ★ 需**重做**。

⇒ ★ **教训**：★ 变异对照的收尾**必须用 `cp <gold>` 覆盖**，★ **MUST NOT** 用
★ `git checkout`（★ 它按 **HEAD** 恢复 ⇒ ★ 会丢掉**未提交**的改动 ⇒ ★
★ 而本轮的改动**本来就还没提交** ⇒ ★ 必丢）。

### K23 — ★ 观测量的「载体」实测（★ A2.6 台账同步时发现 ⇒ 决定 A3 的实现前置）

> ★ **触发**：★ A2.6 台账同步时核对「`tohost`/`regs`/`memsig` 走顶层 `ch_out`」
>   ⇒ ★ 读 `tools/verilator_runner/sim_main.cpp` ⇒ ★ **发现 5 个观测量的实际读法**。

#### K23.1 ★ Verilator 后端当前**全部**靠 `--public-flat-rw` 穿透（★ H-2 的实证）

| 观测量 | Verilator 侧读法 | 性质 |
|---|---|---|
| `mem_sig` | `vtop->rootp->top__DOT__mem_sig` | ★ **穿透内部层级** |
| `mem_sig_updates` | `vtop->rootp->top__DOT__mem_sig_updates` | ★ **穿透内部层级** |
| `regs[1..31]` | `r->top__DOT___reg_N` | ★ **穿透内部层级** |
| `tohost` | `rootp->top__DOT__dmem[tohost_word]` | ★ **穿透 + 硬编码数组路径** |
| `pc` | `pc_head[8]`（同款穿透） | ★ **穿透内部层级** |

⇒ ★★ **这就是 ADR-091 H-2 的动机**（「MUST NOT 依赖 `--public-flat-rw` 读内部层级」
⇒ ★ 内部节点名一变即断 ⇒ ADR-091 §Context B-1）。

#### K23.2 ★ 关键实测：`resolve_node_id` **顶层端口节点也在** `ctx.get_nodes()` 里

★ `parity_sim_runner.cpp:249-255`：

```cpp
for (const auto& node : ctx.get_nodes()) {
  if (node && node->name() == name) return node->id();
}
```

★ ★ **顶层 `ch_out` 节点也是 `ctx.get_nodes()` 的成员** ⇒ ★ ⇒ ★ **载体 = 顶层 `ch_out`**
★ 同时满足两侧：

| 载体 | 解释器后端 | Verilator 后端 |
|---|---|---|
| ★ **顶层 `ch_out`**（★ 名字 = `mem_sig`） | ✔ `resolve_node_id` 直接命中 ⇒ ★ **runner 零改动** | ✔ `port_access_`（★ **契约路径** ⇒ H-2） |
| ★ 内部 `ch_reg`（名字 = `mem_sig`） | ✔ 命中 | ✘ **不在 `port_access_`** ⇒ ★ 只能 `--public-flat-rw` 穿透 ⇒ ★ **违背 H-2 的目标** |

⇒ ★★ ⇒ ★ **「顶层 `ch_out`」是唯一自洽解** ⇒ ★ §D3a.6 的「节点名沿用 `mem_sig`」
★ **无需推翻**，★ 但 ★ **必须补齐「载体 MUST 是顶层 `ch_out`」这一半**
（★ ADR-091 v1.11 §D3a.6 只写了「节点名」，★ 未写「载体」）
⇒ ★ **澄清属 architect 职权** ⇒ ★ 本轮**不改**已签批 ADR ⇒ ★ 归**待裁决清单**。

#### K23.3 ★ 待 architect 裁决的清单（★ A3 的前置）

| # | 待裁决项 | 为什么阻塞 A3 |
|---|---|---|
| **①** | ★ **`memsig` 的载体** MUST 是顶层 `ch_out`（★ 补齐 §D3a.6） | ★ 决定 A5 从设备怎么接签名 ⇒ ★ 决定 A3/A4 的端口清单 |
| **②** | ★ **`tohost` 的归属** = **从设备**（★ 开放核**不持有内存** ⇒ 核侧无法 `aread`）⇒ ★ 预载核的 `tohost_probe_data_proxy` 来自 dmem 的 `mem.aread`（`dmem_chmem.h:278`）⇒ ★ **开放核无对应物** | ★ 决定 `tohost` 是核侧端口还是从设备端口 |
| **③** | ★ **`regs[32]` 的载体**：★ 预载核是**内部** `ch_reg`（`reg_file_chmem.h:169` 显式命名 `reg_0..reg_31`）⇒ ★ 开放核若走顶层端口 ⇒ ★ **两侧结构不同构** ⇒ ★ H-3 对比时两侧读法不同 | ★ 决定 A4 的 regfile plugin 要不要额外接 32 个端口 |
| **④** | ★ **`pchash` 的契约名**：★ 预载核的 pc 节点名 = `pc`（内部）⇒ ★ 开放核现有顶层端口名 = `open_pc` ⇒ ★ **命名不一致** ⇒ ★ 采集侧要么改名要么特判 | ★ 决定 A3 是否要改 `open_pc` 的契约名（★ 已进 ADR/tasks ⇒ 改动面大） |

★ ★ **A3 的实际可做范围（★ 按「不预设未裁决契约」原则）**：
★ **仅 `pchash` 的载体形态**可先行（★ 但它撞 ④ 的命名问题）⇒ ★
★ `tohost` / `regs` / `memsig` **全部依赖上述裁决** ⇒ ★ **A3 应在 architect 裁决之后实施**。

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

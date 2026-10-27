---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v1.0.0
status: proposed
created_from: docs/architecture/adr/ADR-091-chmem-open-core-port-architecture.md
adr:
  - ADR-091
  - ADR-083
  - ADR-092
  - ADR-089
depends_on:
  - backend-parity-corpus
plan: .rddf/plans/backend-parity-open-core.md
---

# backend-parity-open-core

> **ADR-091 §D8 S1~S5**：新增 **CH_MEM 开放核**（`ip/cpu/core/`），对外暴露 `ch_stream<CpuBusReq>` 端口，
> 让 **解释器后端 + Verilator 后端都必过**（用户裁决 D0-c）。
>
> ★ **架构授权**：**ADR-091 v1.2**（✅ Accepted 2026-10-28）· **ADR-092 v1.1**（TLM 冻结）· ADR-083 v1.2（`ch_stream` 归类）
> ★ **执行路径**：`.rddf/plans/backend-parity-open-core.md`

## Why

### 1. 当前 CPU 是**封闭核** —— 顶层只有 2 个端口（实测）

| 证据 | 数据 |
|---|---|
| 生成网表的顶层端口 | **只有** `default_clock` / `default_reset` |
| 根因 | ELF 在 **elaboration 期预载**进 `ibus_chmem.h` 的 `ch_mem`（`cpu_factory_chmem.h:276`） |
| 框架层无端口概念 | `grep -c "ch_in\|ch_out\|port<" include/cf/plugin/pipe_builder.h` = **0** |

⇒ ★ **4 个真实阻塞**（ADR-091 §B-1~B-4）：

| # | 阻塞 | 后果 |
|---|---|---|
| B-1 | `VerilatorBackend` 的 `port_access_` **只认顶层端口** | 对 2 端口的核几乎无用 |
| B-2 | 观测走内部 hack（`--public-flat-rw` 读 `top__DOT___reg_N`） | 门禁观测路径**不稳定**（内部重构即打断） |
| B-3 | 无对外接口 | **SoC 互联不存在** |
| B-4 | 每 ELF 一份网表 | 开放项 O-4/O-5 无解 |

### 2. 用户诉求（2026-10-27/28）

- 「ip/cpu 是如何生成支持 Verilator 后端仿真的 cpu 核的」⇒ ★ 现状：**能跑，但核是封闭的**
- 「考虑用 json 来配置，而不是命令行」⇒ ✅ **已落地**（P2 `CpuConfigChmem`）⇒ ★ 本 change 复用它
- 「我希望 cpu_factory_chmem 构建的核和 Cache/MMU ip 可以有一致的接口 bundle 契约」⇒ ★ **本 change 的 D3/D3a**
- 「采用 CppHDL 的 `verilator_backend.h`，通过 `sim->set_backend()` 方式来支持多后端」⇒ ★ **实测可行**（见 §3）

### 3. ★ 上游调研结论（2026-10-28，实测）

| 事实 | 取证 | 意义 |
|---|---|---|
| `ch::VerilatorBackend` **已实装**（952 行） | `include/core/verilator_backend.h` + `src/core/verilator_backend.cpp` | 可直接用 |
| ★ `try_initialize(ctx*, data_map_t&)` **只要 ctx + data_map** | `verilator_backend.cpp:301-304` | ⇒ **不需要 `ch_device` / Component 体系** |
| ★ `Simulator(ctx*, bool)` **裸 ctx 即可** | `simulator.cpp:24-26` | ⇒ ChipForge 现有 `create_simulator()` 已是同范式 |
| ★ 顶层端口 = ctx 里的 `type_input`/`type_output` 节点 | `codegen_verilog.cpp:669-679` | ⇒ **框架层只需新增「端口声明」API** |
| ★ **裸 ctx 建端口已 PoC 验证** | `tests/framework/test_pipe_builder_ports.cpp`（2 case / 6 assertions **实测全过**） | ⇒ ★ **G-1 机制成立** |

## What Changes

### 1. 框架层（`include/cf/plugin/pipe_builder.h`）

- ★ **G-1**：`declare_in<T>` / `declare_out<T>`（★ 4 条实测约束见 ADR-091 v1.2 §D5.1）
- ★ **G-5a**：`create_simulator(std::unique_ptr<ch::IEvalBackend>)` 重载（★ `nullptr` ⇒ 保持现行为 ⇒ 向后兼容）

### 2. 新核（`ip/cpu/core/`，★ 全新目录）

| 文件 | 内容 |
|---|---|
| `bus_iface_chmem.h` | ★ `CpuBusReq` / `CpuBusResp`（`bundle_base` 派生 + `CH_BUNDLE_FIELDS_T` + 纯 `ch::core::ch_uint`） |
| `cpu_core_chmem.h` | 开放核装配（复用 `ip/cpu/arch/riscv/` + `ip/cpu/core/payload_common.h`） |
| `ibus_port_chmem.h` / `dmem_port_chmem.h` | 取指/访存走端口 |
| `verify_mem_model_chmem.h` | 验证实现（内存模型 + ELF 加载）—— ip 级单测用 |

### 3. ★ 不动的东西（★ 零回归的关键）

| ✗ 不动 | 理由 |
|---|---|
| `ip/cpu/plugins/*_chmem.h` + `cpu_factory_chmem.h` | ★ 现有 **52 case parity 基线**建立在它上面 |
| `ip/cpu/configs/*.json`（TLM 4 份） | TLM 侧仍在用 |
| `bundles/mem_bundles.h` | ★ ADR-092 Phase-2 才单模化 |
| `include/cf/plugin/uint_t.h` 模态分支 | ★ ADR-092 Phase-2 |
| TLM 代码（54 文件） | ★ ADR-092 Phase-1：**冻结保留**为设计参考 |
| GShare | ★ 已按 ADR-088 §5.3.3 C-3 结案为「本期不做」 |

## Acceptance

- [ ] **AC-1** ★ **G-1 端口契约**：端口进网表**顶层端口列表**；★ ctx null ⇒ **fail-closed**；★ 内部驱动的 `ch_in` **不进**顶层
- [ ] **AC-2** ★ **G-5a 后端注入**：`create_simulator(backend)` ⇒ `active_backend_name()` 正确；★ `nullptr` ⇒ 现行为不变
- [ ] **AC-3** ★ **`CpuBusReq`/`CpuBusResp`** 字段齐备 + **每字段 ≤64 bit**（H-1）+ 零 `cf::plugin::uint_t` 依赖（H-4）
- [ ] **AC-4** ★ **开放核能通过端口取指/访存**（无 ELF 预载）
- [ ] **AC-5** ★ **顶层观测端口**（H-2）：`tohost`/`pc`/`regs`/`memsig` ⇒ ★ 退休 `--public-flat-rw` 内部层级读法
- [ ] **AC-6** ★ **验证实现**：加载 ELF + 响应 imem/dmem 请求
- [ ] **AC-7** ★ **双后端必过**（D0-c）：同一份测试 body 在解释器 + Verilator 下**都 PASS**（★ Verilator 侧 WARN-skip 不算 PASS）
- [ ] **AC-8** ★ **H-3 等价性自检**：同一 ELF 跑开放核 vs 预载核 ⇒ **5 观测量逐位相等**；★ **变异对照** ⇒ 故意改 1 处 ⇒ MUST RED
- [ ] **AC-9** ★ **零回归**：TLM binary **507 case / 506 passed / 1 baseline-known FAIL**（计数不变）+ ★ **52 case parity 逐位不变**
- [ ] **AC-10** ★ **框架头纪律**：★ 框架头与新核头**零** `<ch.hpp>` include（★ `module.h:13` 缺 `inline` ⇒ 2+ TU 即链接失败）
- [ ] **AC-11** ★ **八门禁全绿**

## 阶段边界

| 阶段 | 内容 | 本 change |
|---|---|---|
| **S1** | G-1 + G-5a | ✅ |
| **S2** | bundle + 装配骨架 | ✅ |
| **S3** | 端口数据通路 + 观测端口 | ✅ |
| **S4** | 验证实现 | ✅ |
| **S5** | 双后端必过 + 等价性自检 | ✅ |
| **S6** | SoC 接线（接真 Cache/MMU） | ✗ ★ 依赖 MMU PTW / L1Cache 的 CH_MEM 实装（★ 两者都不存在） |

## 相关

- **ADR-091 v1.2**（CH_MEM 开放核形态与端口架构）—— ★ 本 change 的架构授权（含 §D5.1 的 4 条实测约束）
- **ADR-092 v1.1**（TLM 参考实现的冻结与演进停止）—— ★ Phase-1/Phase-2 边界
- **ADR-083 v1.2**（`ch_stream` 归类纠偏）—— ★ 层次区分（IP 间 `ch_stream` / 核内 `Payload<T>`）
- **ADR-089 v1.4**（双后端差分 parity 门禁）—— ★ 5 观测量定义
- `.rddf/plans/backend-parity-open-core.md` —— ★ 执行路径（含 7 条实测知识 K1~K7）

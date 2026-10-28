---
change: backend-parity-open-core
priority: P1
initiative: wave5-isa-coverage-and-bp
adr:
  - ADR-091
  - ADR-083
  - ADR-092
  - ADR-089
plan: .rddf/plans/backend-parity-open-core.md
status: proposed
depends_on:
  - backend-parity-corpus
---

# tasks — backend-parity-open-core（ADR-091 S1~S5）

> 每条任务末尾 `[ADR-091 §Dx]` = 它落地的决策单元。
> ★ **执行细节**（TDD 5-step / 实测知识 K1~K7）见 `.rddf/plans/backend-parity-open-core.md`。

## 0. 前置（✅ 已完成）

- [x] 0.1 ★ **G-1 机制 PoC 验证**：`tests/framework/test_pipe_builder_ports.cpp`（2 case / 6 assertions **实测全过**）⇒ 裸 ctx 建 `ch_in`/`ch_out` ⇒ 进网表顶层端口；★ 无 current context ⇒ `impl()==nullptr`（**静默不建节点 ⇒ 假绿通道**）[ADR-091 §D5.1]
- [x] 0.2 ★ **ADR-091 升 v1.2**：§D5.1 落 G-1a/b/c/d 四条实测约束 + PoC 证据
- [x] 0.3 ★ **plan 落盘**：`.rddf/plans/backend-parity-open-core.md`（含 K1~K7 实测知识）

## 1. Phase 1 — S1：框架层 G-1（端口声明）+ G-5a（后端注入）

- [x] 1.1 [RED] 扩 `tests/framework/test_pipe_builder_ports.cpp`：case C（端口进网表顶层）/ case D（ctx null ⇒ fail-closed）/ case E（内部驱动的 `ch_in` 不进顶层）/ case F（后端注入 ⇒ `active_backend_name()`）[ADR-091 §D5, §D5.1]
- [x] 1.2 [GREEN] `include/cf/plugin/pipe_builder.h` 增 `declare_in<T>` / `declare_out<T>`：★ `#ifdef CF_PLUGIN_USE_CH_MEM` 内 · ★ **只 include 具体头**（MUST NOT `<ch.hpp>`）· ★ ctx null ⇒ throw · ★ `ctx_swap(ctx_)` 保护 [ADR-091 §D5.1 G-1a/b/c]
- [x] 1.3 [GREEN] `include/cf/plugin/pipe_builder.h` 增 `create_simulator(std::unique_ptr<ch::IEvalBackend>)` 重载（★ `nullptr` ⇒ 现行为不变 ⇒ 向后兼容）[ADR-091 §D5 G-5a]
- [x] 1.4 [GREEN] ★ 头文件纪律：框架头 **零** `<ch.hpp>` include（★ `module.h:13` 缺 `inline` ⇒ 2+ TU 链接失败）
  ⇒ 验证：`simulator.h` 不引入 `module.h`/`ch.hpp` ⇒ `pipe_builder.h` 的现有 include 链**本就安全**
  ⇒ ★ 新增的 `declare_in`/`declare_out` 复用 `simulator.h` 间接引入的 `core/io.h` ⇒ **零新 include**
  [ADR-091 §D5.1 G-1b]
- [ ] ~~1.6 runner `--backend` flag~~ ⇒ ★ **推迟到 Phase 5**（★ 本 Phase 不需要：G-5a 的价值是单进程双后端对照，而对照在 Phase 5 才出现 ⇒ ★ 提前加是无用户的 flag）
- [x] 1.5 ★ **零回归**：TLM binary **507 case / 506 passed / 1 baseline-known FAIL**（计数不变）+ 八门禁全绿

## 2. Phase 2 — S2：`bus_iface_chmem.h`（G-4）+ 装配骨架

- [x] 2.1 [RED] `tests/cpu/schema/test_cpu_bus_iface.cpp`（新）：`CpuBusReq`/`CpuBusResp` 字段与宽度断言（★ 每字段 ≤64 bit）[ADR-091 §D6 H-1]
- [x] 2.2 [GREEN] 新建 `ip/cpu/core/bus_iface_chmem.h`：`bundle_base` 派生 + `CH_BUNDLE_FIELDS_T` + 纯 `ch::core::ch_uint`（★ 零 `cf::plugin::uint_t`）· ★ 真名 **MUST NOT** `MemReqBundle`（该名代码中不存在）[ADR-091 §D3a.3, §D6 H-1/H-4]
- [ ] 2.3 [GREEN] 新建 `ip/cpu/core/cpu_core_chmem.h`：装配骨架（复用 `ip/cpu/arch/riscv/` + `ip/cpu/core/payload_common.h`）[ADR-091 §D1]
  ⇒ ★ **推迟到 Phase 3**（★ 装配骨架需要 Phase 3 的端口数据通路才有意义 ⇒ 单独提交一个空骨架是纯形式）
- [x] 2.4 零回归 + 八门禁

## 3. Phase 3 — S3：端口数据通路 + 顶层观测端口

- [x] 3.1 [RED] 测试：开放核在**无 ELF 预载**下能通过端口取到指令 [ADR-091 §D2]
  ⇒ `tests/cpu/test_cpu_open_core_ibus.cpp`（2 case / 24 assertions **实测全过**）
- [x] 3.2 [GREEN] `ip/cpu/core/ibus_port_chmem.h`：取指走端口（★ **取指侧完成**）
  ⇒ ★ 与预载核**共享** `ip/cpu/core/payload_common.h`（payload 契约）⇒ 语义不分叉
  ⇒ ★ **时序等价性**：`sread`（1 拍）≡ 请求当拍 → 响应下拍（也是 1 拍）⇒ ★ **H-3 等价性自检有前提**
  ⇒ ★ 端口**逐字段**（★ CppHDL 的 `ch_stream` 语义即 payload/valid/ready 三元组）
- [ ] 3.2b [GREEN] ★ **分支重定向**（`pc` 由 branch 驱动）⇒ 本版本只做**顺序取指**（★ 诚实标注）
- [ ] 3.3 [GREEN] `ip/cpu/core/dmem_port_chmem.h`：访存走端口
- [x] 3.4 [GREEN] 顶层**观测端口**（取指侧）：★ `open_pc` 走顶层 `ch_out` [ADR-091 §D6 H-2]
  ⇒ ★ `tohost`/`regs`/`memsig` 随 dmem 侧与完整装配落地
- [x] 3.5 [GREEN] 握手成本**第一档**：`ready` 恒真 ⇒ 无阻塞流 [ADR-091 §D3a.2]
  ⇒ ★ **「`ready` 可拉低」（背压）+ CPU 侧 stall 机制未实现** ⇒ ★ **诚实标注**
- [x] 3.6 零回归 + 八门禁
  ⇒ ★ **框架级加固**（ADR-091 v1.5 §D5.1.2）：`elaborate()` 自建 `ctx_swap`（G-1h）+ 自动 `build()`（G-1j）
  ⇒ ★ 零退化：TLM **509 case / 508 passed / 1 failed**（baseline-known）· CH_MEM 框架子集 **24/24 全绿**

## 4. Phase 4 — S4：验证实现（`verify_mem_model_chmem.h`）

- [ ] 4.1 [RED] 测试：验证实现加载 ELF 并正确响应 imem/dmem 请求 [ADR-091 §D2]
- [ ] 4.2 [GREEN] 新建 `ip/cpu/core/verify_mem_model_chmem.h`：内存模型 + ELF 加载（★ 参考 TLM 侧 `picolibc_host_memory.h` 的**语义**，★ MUST NOT include 它 —— 它在 ADR-092 冻结集里）[ADR-092 §D1.1]
- [ ] 4.3 零回归 + 八门禁

## 5. Phase 5 — S5：双后端必过（D0-c）+ 等价性自检（H-3）

- [ ] 5.1 [RED+GREEN] 同一份测试 body 在**解释器后端**与 **Verilator 后端**下**都必过**（★ 借鉴上游 `DUAL_BACKEND_TEST_PAIR` 范式；★ **Verilator 侧 WARN-skip 不算 PASS**）[ADR-091 §D8 S5]
- [ ] 5.2 [RED+GREEN] ★ **H-3 等价性自检**：同一 ELF 跑**开放核**与**预载核** ⇒ **5 观测量（tohost/regs/memsig/cycles/pchash）逐位相等**；★ **变异对照** ⇒ 故意改开放核 1 处 ⇒ MUST RED [ADR-091 §D6 H-3]
- [ ] 5.3 ★ **52 case parity 逐位不变** + 八门禁全绿

## 6. 范围外（★ 明确不做）

- [ ] ✗ 不改 `ip/cpu/plugins/*_chmem.h` + `cpu_factory_chmem.h`（★ 52 case 基线）
- [ ] ✗ 不删 TLM 代码（★ ADR-092 Phase-1 冻结保留）
- [ ] ✗ 不改 `bundles/mem_bundles.h` / `uint_t.h` 模态（★ ADR-092 Phase-2）
- [ ] ✗ 不引入 CppHDL Component / `ch_device` 体系（★ 实测不需要）
- [ ] ✗ 不做 SoC 接线（S6）—— ★ 依赖 MMU PTW / L1Cache 的 CH_MEM 实装（两者都不存在）
- [ ] ✗ 不碰 GShare（★ 已按 ADR-088 §5.3.3 C-3 结案）

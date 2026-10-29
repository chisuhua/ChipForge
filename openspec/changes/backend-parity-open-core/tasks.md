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
  ⇒ ★★ **v1.12 裁决后的细化**（★ ADR-091 v1.12 §D3a.7 / §D3a.8）：
    · ★ **全部 plugin 均为新建**（★ 不改 `ip/cpu/plugins/*_chmem.h` ⇒ ADR-092 冻结集零风险）
    · ★ **两层范式**：★ 层 1 = `PluginBase` + `at_stage` + `Payload`（★ 核内 stage 间）
      · ★ 层 2 = `ch::Component`（★ 可复用 IP 单元：★ regfile 等）
      · ★ **桥接 plugin** = 层 2 → 层 1 的唯一通道（★ 读 `io()` ⇒ 写 `Payload`）
    · ★ ★ **框架缺口 #4（G-6）**：★ `PipeBuilder` 暴露只读 ctx accessor
      （★ `ctx_` 当前 private ⇒ 无法把 ctx 传给 `Component::build(external_ctx)`
      ⇒ ★ 破 G-1p ⇒ 组件输出端口从 `data_map` 消失 ⇒ **静默读不到**）
    · ★ ★ **装配 MUST 按流水顺序注册 plugin**（★ G-1n：★ `setup()` 在 `build()` 时
      按 `plugins_` 顺序调 ⇒ ★ 注册顺序 = stage 首现序）
    · ★ ★ **端口名 MUST NOT 与内部 `ch_reg` 同名**（★ G-1o）
    · ★ **Component MUST 共用 ctx**（★ G-1p）⇒ ★ **MUST NOT** `build(nullptr)`
    · ★ **MUST NOT** 用 `ch_module<T>`（★ 需 active parent Component ⇒ ChipHDL 无此机制）
    · ★ **MUST NOT** 用 `ch_stream` 做单元接口（★ 越层：★ ADR-092 §D4 定其为 IP 间接口）
  ⇒ ★ **推迟到 Phase 3**（★ 装配骨架需要 Phase 3 的端口数据通路才有意义 ⇒ 单独提交一个空骨架是纯形式）
- [x] 2.3b [RED+GREEN] ★ **可复用单元：regfile**（★ ADR-091 v1.12 §D3a.7 两层范式 · §D3a.8 register re-naming）
  ⇒ ★ **层 2（`ch::Component` 子类）**：★ `io()` struct（★ `rs1_addr`/`rs1_data`/`rs2_*`/`rd_addr`/`rd_data`/`rd_we`）
    ⇒ ★ 上游先例：`include/cpu/pipeline/rv32i_regfile.h`
  ⇒ ★ ★ **MUST 用 `std::vector` + 显式命名 `reg_0..reg_31`**（★ 实证：`reg_file_chmem.h:160-172`
    ⇒ ★ `std::array` 默认构造泄漏 32 对孤儿节点 ⇒ 撞名 ⇒ Verilator `Duplicate declaration of signal '_reg_N'`）
  ⇒ ★ **契约名 `reg_N` MUST NOT 改**（★ runner 按 `reg_<i>` 采集）
  ⇒ ★ **桥接 plugin**（★ 层 1）：★ 在 `setup()` 里 `build(pb_ctx)` ⇒ ★ 在 `at_stage` 闭包读 `io()` ⇒ 写 `Payload`
  ⇒ ★ **实测**：★ `ip/cpu/core/regfile_chmem.h`（★ 层 2 Component）
    + `ip/cpu/core/regfile_bridge_chmem.h`（★ 层 1 桥接 plugin）
    + `tests/cpu/test_cpu_open_core_regfile.cpp`（★ **4 case**）
    ⇒ ★ ★ **注**：★ 勾选时记的是「3 case / 75 assertions」⇒ ★ 后增
      「regs 顶层端口契约」case ⇒ ★ 现为 **4 case**
      ⇒ ★ `[open-core]` 整体 **18 case / 812 assertions**（★ 与 AGENTS.md 一致）
  ⇒ ★ **变异对照 4/4 RED**（★ plan §K25.1）：★ ① `build(nullptr)`（★ 破 G-1p）→ **2 case**
    ② reg 名用默认名（★ 破 §D3a.8）→ 1 ③ 桥接不发布 payload → 1 ④ 允许写 x0 → 1
  ⇒ ★★ **6 个契约陷阱**（★ plan §K25.2）：★ `namespace` alias 不能带模板参数 ·
    ★ 端口 MUST 用 `unique_ptr` 持有（★ `port::operator=` 被 delete）·
    ★ `ch_bool` 须走 `port::impl()` · ★ 门禁 Check 1 是**纯文本 grep**
    （★ 注释里也不能写「条件早返」字面形式）· ★ 索引 MUST 是**编译期常量**
    （★ 否则常量折叠 ⇒ 假绿 ⇒ 模板递归展开）
- [x] 2.4 零回归 + 八门禁

## 3. Phase 3 — S3：端口数据通路 + 顶层观测端口

- [x] 3.1 [RED] 测试：开放核在**无 ELF 预载**下能通过端口取到指令 [ADR-091 §D2]
  ⇒ `tests/cpu/test_cpu_open_core_ibus.cpp`（2 case / 24 assertions **实测全过**）
- [x] 3.2 [GREEN] `ip/cpu/core/ibus_port_chmem.h`：取指走端口（★ **取指侧完成**）
  ⇒ ★ 与预载核**共享** `ip/cpu/core/payload_common.h`（payload 契约）⇒ 语义不分叉
  ⇒ ★ **时序等价性**：`sread`（1 拍）≡ 请求当拍 → 响应下拍（也是 1 拍）⇒ ★ **H-3 等价性自检有前提**
  ⇒ ★ 端口**逐字段**（★ CppHDL 的 `ch_stream` 语义即 payload/valid/ready 三元组）
- [x] 3.2b [GREEN] ★ **分支重定向 + flush**（★ ADR-091 v1.10 §D3a.5）
  ⇒ ★ **实现**：`ip/cpu/core/branch_iface_chmem.h`（3 个核内 payload key）+
    `ibus_port_chmem.h` 的 `branch LATE`（pc mux + `FLUSH` 打拍）+
    `dmem_port_chmem.h` 的 store 抑制（`we = is_store ? !flush : 0`）
  ⇒ ★ **测试**：`tests/cpu/test_cpu_open_core_branch.cpp`（★ **6 case / 339 assertions 全过**）
  ⇒ ★ **pc mux**：★ `pc <<= select(BRANCH_TAKEN, BRANCH_TARGET, pc+4)`（★ 无 BP 预测臂）
  ⇒ ★ **`FLUSH` 1 拍对齐**：★ `FLUSH(N) = BRANCH_TAKEN(N-1)` ⇒ ★ 与 `INSTRUCTION(N)` 对齐
  ⇒ ★ **变异对照 4/4 RED**（★ `FLUSH` 恒 0 / 去掉抑制 / 不重定向 / 注册顺序倒置）
  ⇒ ★★ **新增硬约束 G-1n**：★ **stage 首现序 MUST 等于流水顺序**
    ⇒ ★ 装配方（含测试）MUST 按流水顺序注册 plugin（★ `setup()` 在 `build()` 时才跑）
    ⇒ ★ 否则跨 stage payload **读不到** ⇒ 静默走 fail-open（★ 本轮踩坑）
  ⇒ ★★ **诚实标注（仍未实现）**：
    · ✗ **分支预测**（★ BP / BTB / GShare ⇒ 归 `feat-bp-btb-gshare`）
    · ✗ **多拍错路**（★ 当前只 flush **1 拍**错路 ⇒ ★ 与预载核的两级取指同深度）
    · ✗ **异常 / 中断重定向**（★ 归 `mmu-chmem-phase-b-c-launch` 的异常通路）
- [x] 3.2c [GREEN] ★ **A2.5 收尾：端口名解耦 + 断言精确化**（★ ADR-091 v1.11 G-1o / V-14 / V-15）
  ⇒ ★ **G-1o 修复**：★ 内部 `ch_reg` 名 `open_pc` → `open_pc_reg` · `open_flush` → `open_flush_reg`
    ⇒ ★ **顶层端口名保持契约名**（★ 生成物实测：`open_pc` / `open_flush`，★ **无 `_1` 后缀**）
  ⇒ ★ **断言精确化**：★ 新建 `tests/cpu/open_core_verilog_helper.h`（★ 共享 helper）
    ⇒ ★ `top_port_declared` 改**精确名匹配**（★ 原先 ibus/dmem 各一份**相同的子串匹配**⇒ 假绿）
  ⇒ ★ **覆盖缺口修复**：★ `test_cpu_open_core_branch.cpp` **补 `toVerilog`**
    ⇒ ★ A2 之前 `branch` 测试从未生成 Verilog ⇒ 新增的 `flush_reg` + `open_flush` 端口**从未过 codegen**
  ⇒ ★ **反向断言**：★ `open_pc_1` / `open_flush_1` MUST **不在**顶层（★ 第二道护栏）
  ⇒ ★ **变异对照 4 组全测**（★ plan §K22.2）：
    · M-V14（内部 reg 撞名）→ **2 case 失败** ✔
    · M-V15+V14（子串匹配 + 撞名）→ **1 case 失败**（★ 捕获来自反向断言）
    · M-V15b（子串匹配 + 去反向断言 + 撞名）→ ★ **全绿 = 假绿复现** ⇒ 证明精确匹配必需
  ⇒ ★ **诚实标注**：★ ADR v1.11 的 **V-15 措辞有歧义**（★ 实测后的准确表述见 plan §K22.2）
    ⇒ ★ 本轮**不改**已签批的 ADR 决策文字 ⇒ 归 backlog（下一轮 architect 澄清）
- [x] 3.4b [RED+GREEN] ★ **顶层观测端口补全**：`tohost` / `regs[32]` / `memsig` / `pchash`
  ⇒ ★★ **观测契约已裁决**（★ ADR-091 **v1.12 §D3a.9**，★ 2026-10-29）
    ⇒ ★ **载体通则**：★ 每个顶层观测量 MUST 是**顶层 `ch_out`**（★ H-2）
      ⇒ ★ **MUST NOT** 是内部 `ch_reg`（★ 不在 Verilator `port_access_` ⇒ 只能穿透）
      ⇒ ★ **MUST NOT** 依赖 `--public-flat-rw` 读内部层级
  · ★★ **`memsig` / `mem_sig_updates`**：★ 归**从设备**（§D3a.6）★ 顶层 `ch_out`
    ⇒ ★ 节点名不变 ⇒ ★ **parity runner 零改动** ⇒ ★ **已完成**（★ A5-3）
  · ★★ **`tohost`**：★ 归**从设备**（★ 开放核**不持有内存** ⇒ 核侧无法 `aread`
    ⇒ 预载核的 `tohost_probe_data_proxy` 来自 dmem 的 `mem.aread`
    ⇒ 开放核**无对应物**）★ 顶层 `ch_out`
    ⇒ ★ ★ **契约名 = `tohost_probe_data_proxy`**（★ 与预载核**同名** ⇒ runner 零改动）
    ⇒ ★ **已完成**（★ A5-4）⇒ ★ ★ **注**：★ §D3a.9 原文写「名 `tohost`」
      ⇒ ★ **MUST 修正**（★ 证据 plan §K28.1）⇒ ★ **提请 architect**
  · ★★ **`regs[32]`**：★ 归**核**★ 顶层 `ch_out`（★ 名 `reg_1`..`reg_31`
    ⇒ ★ `reg_0` 为 x0 恒 0 ⇒ 沿用 runner 的 1..31 口径）
    ⇒ ★ 内部 `ch_reg` 名 `open_reg_<i>`（★ G-1o：不撞名）⇒ ★ **已完成**（★ A3-1）
  · ★★ **`pchash` / `pc`**：★ 归**核**★ 顶层 `ch_out` ⇒ ★ **契约名 `open_pc` MUST NOT 改**
    ⇒ ★ **parity runner 加 1 处 fallback**（★ 先 `pc` ⇒ 未命中则 `open_pc`）⇒ ★ **归 A6**
    ⇒ ★ ⇒ ★ **本子项未完成**（★ 归 A6）
  ⇒ ★ **Verilator 侧**须分别处理（★ 预载核走穿透 / 开放核走 `port_access_`）⇒ ★ **归 A6**
    ⇒ ★ ★ **注**：★ 「采集零改动」**只对解释器侧成立**（★ `sim_main.cpp:96-126`
      全是穿透读法）⇒ ★ A6 的 6c 是**实质性新代码**（★ plan §K29.3 ⑧）
- [x] 3.3 [GREEN] `ip/cpu/core/dmem_port_chmem.h`：访存走端口
  ⇒ ★ **实现**：`ip/cpu/core/dmem_port_chmem.h` + `tests/cpu/test_cpu_open_core_dmem.cpp`
    （★ **4 case / 92 assertions 实测全过**）⇒ ★ **8 个访存端口 + 1 个观测端口进网表顶层**
  ⇒ ★ **时序形态**（★ ADR-091 v1.6 §D3a.4）：★ 请求侧**打 1 拍** ⇒ ★ 数据侧**组合消费**
    ⇒ ⇒ ★ 消费拍 N+1 的数据对应地址拍 N（★ 「外部提前 1 拍供数」的机制依据，见 plan §K17.3）
  ⇒ ★ **写语义完备**（★ ADR-091 v1.7）：★ 端口带 `size`（3 bit）⇒ ★ **从设备**做 byte-strobe RMW
    ⇒ ★ CPU 侧**不做** RMW ⇒ ★ 写**无延迟** ⇒ ★ 无 load-after-store ⇒ ★ **无需 stall / forwarding**
  ⇒ ★ **读语义与预载核逐位同构**（★ byte select 树同款：lb/lbu/lh/lhu/lw 的符号扩展与零扩展）
  ⇒ ★★ **诚实标注（本版本**未**实现）**：
    · ✗ **背压（`ready` 可拉低）+ CPU 侧 stall** ⇒ 只实现「`ready` 恒真」的无阻塞流（★ ADR-091 §D3a.2 第 ③ 档未实现）
    · ✗ **flush 抑制**（★ 分支重定向未实现 ⇒ 错路 store 不存在 ⇒ 无 flush 语义）
    · ✗ **MMU 翻译**（★ 地址取 `KT::RESULT`（vaddr）直接作字节地址 ⇒ **Bare 模式**）
    · ✗ **`memsig` 观测**（★ ★ 开放核**不持有内存** ⇒ 核侧算不出「最终字」⇒ 观测来源 MUST 由**从设备 / host**承担 ⇒ 归 **S5 决策**）
- [x] 3.4 [GREEN] 顶层**观测端口**（取指侧）：★ `open_pc` 走顶层 `ch_out` [ADR-091 §D6 H-2]
  ⇒ ★ **v1.11 修正**（★ 2026-10-29）：★ `memsig` 归**从设备**（§D3a.6）⇒ ★ 但**载体仍是
    顶层 `ch_out`**（★ 名字 `mem_sig` / `mem_sig_updates`）⇒ ★ 理由：★ `resolve_node_id`
    遍历 `ctx.get_nodes()` ⇒ ★ **顶层端口节点也在其中** ⇒ ★ 解释器侧 runner 零改动
    + ★ Verilator 后端走 `port_access_`（★ 契约路径 ⇒ H-2）
  ⇒ ★ **`tohost` / `regs` / `pchash`** 见 **3.4b**
- [x] 3.5 [GREEN] 握手成本**第一档**：`ready` 恒真 ⇒ 无阻塞流 [ADR-091 §D3a.2]
  ⇒ ★ **「`ready` 可拉低」（背压）+ CPU 侧 stall 机制未实现** ⇒ ★ **诚实标注**
- [x] 3.7 [GREEN] ★ **ADR-047 合规修正**（★ S1 遗留缺陷，本轮发现）：★ `declare_in/out` 的
  `throw PluginException` ⇒ 违反 ADR-047 Check 10 ⇒ **门禁红** ⇒ ★ 改 `Result<Port>` +
  `std::unexpected(PluginError::NullContextPtr)` ⇒ ★ **fail-closed 语义不变**（★ ADR-091 §D5.1 **G-1c 修正**）
  ⇒ ★ 调用面 = **3 个测试文件**（★ 生产代码零调用）
- [x] 3.8 ★ **变异对照纪律**（★ ADR-091 v1.8 §D6 **V-9**）：★ 本轮 8 条候选「实测约束」
  ⇒ ★ **变异对照裁决** ⇒ ★ **只有 2 条（G-1l / G-1m）** 配登记为硬约束 ⇒ ★ **4 条被证伪**
  （★ `&&`/`||` 失效 / `==` 走指针比较 / 跨宽构造须 `zext` / ctx 名必须唯一 ⇒ ★ 全部不成立）
  ⇒ ★ 证据见 `.rddf/plans/backend-parity-open-core.md` §K17
  ⇒ ★ ★ **真实假失败根因 = 测试指令编码错误**（★ `funct3` 未落在 bits[14:12] ⇒ `lbu` 被解码成 `lb`）
  ⇒ ★ ⇒ ★ **教训**：★ 假失败排查 FIRST 检查项 = **输入编码 / 期望值本身**（★ 不是实现）
- [x] 3.6 零回归 + 八门禁
  ⇒ ★ **框架级加固**（ADR-091 v1.5 §D5.1.2）：`elaborate()` 自建 `ctx_swap`（G-1h）+ 自动 `build()`（G-1j）
  ⇒ ★ 零退化：TLM **509 case / 508 passed / 1 failed**（baseline-known）· CH_MEM 框架子集 **24/24 全绿**

## 3.5 ★ C1 — 「旧产物顶替」假绿通道排查（★ 2026-10-28，★ 独立于 S1~S5 主线）

- [x] C1.1 ★ **排查三个通道**：① 框架成员误置 `#ifdef`（TLM 编译失败）② CH_MEM-only 测试进 TLM 的 `GLOB_RECURSE`（TLM 111 错误）③ **ctest 不构建项目**（编译失败 ⇒ 照跑旧产物）⇒ ★ 逐个封堵
- [x] C1.2 ★ **真实基线修正**：★ TLM **509 → 515 case**（★ 旧数字是**旧产物**）⇒ ★ 唯一失败仍是 baseline-known `7stage_topology_from_config`
- [x] C1.3 ★ **回归脚本三约束**：`tools/run_chipforge_tests.sh` ⇒ ★ ① **默认先 build**（编译失败即退出）② **产物新鲜度 fail-closed**（跳过 build 时校验 mtime）③ **baseline-aware 判定**
- [x] C1.4 ★ **baseline-aware 运行器**：`tools/run_catch2_with_baseline.sh` + `tools/baseline_known_failures.txt`（★ 按 binary 分节 + 每条附根因/owner/收敛期限）⇒ ★ **「全量回归通过」信号恢复**（★ 原先 ctest entry **恒红** ⇒ 信号从来不存在）
- [x] C1.5 ★ **ctest build fixture**：`cf_build_all` ⇒ ★ 编译失败 ⇒ 依赖测试 **Not Run** ⇒ ctest 整体 FAIL ⇒ ★ **fail-closed**（★ 实测：注入编译错误 ⇒ `0% tests passed, 2 tests failed`）
- [x] C1.6 ★ **实测知识沉淀**：plan §K18（★ 含「Catch2 XML reporter 会破坏依赖 `std::cerr` 捕获的测试」等 4 条可迁结论）
- [ ] ★ **L1** `cmake --build <dir>`（all）触发 ExternalProject re-configure ⇒ 找不到编译器 ⇒ 失败（★ 既存环境脆性）⇒ 独立 change
- [ ] ★ **L2** `test_cpu_factory_isa_xlen.cpp` 的 3 个 case **依赖 reporter 实现**（★ XML 下失败）⇒ 脆弱测试 ⇒ 独立 change
- [ ] ★ **L3** CI 不构建项目、不跑测试 ⇒ ★ C1 的 build fixture 只在本地 ctest 生效 ⇒ 归 **A4 `backend-parity-ci`**
- [ ] ★ **L4** `[mmu-verilator]` 的 `mmu_bare_plumbing_tohost1_baseline_5_elf` 是**顺序敏感 flake**（★ 单跑 3/3 PASS ⇒ ★ 全量跑 FAIL，`tohost == 0`）⇒ ★ 根因**未最终定位** ⇒ ★ 已登记进 baseline 清单（★ 附「根因未定位」标注）⇒ 独立 change（owner: ip/mmu）

## 3.6 ★ C2 — 存量 G-1a~G-1k 的**逐条变异对照复核**（★ 2026-10-28）

- [x] C2.1 ★ **11 条存量约束逐条施加变异**（★ 撤掉该写法 ⇒ 判据 MUST RED）⇒ ★ **无一被证伪**（★ 与 v1.8 的 8 条候选中 **4 条被证伪**形成对照 ⇒ ★ 说明 S1/S3a 时期的登记确有实测支撑）
- [x] C2.2 ★ **补 4 个机检 case**（★ G-1f / G-1h / G-1i / G-1j）⇒ ★ 覆盖度 = ★ **强 7 / 中 1 / 弱 3**
- [x] C2.3 ★ **G-1b 后果描述更正**：★ 实测首个失败 = **29 个 `reference to 'detail' is ambiguous`**（★ 命名冲突）★ **非** ADR 原写的 multiple definition（★ `module.h:13` 缺 `inline` 仍成立）
- [x] C2.4 ★ **「MUST 逐条机检」表述更正**（★ 名不副实 ⇒ ★ 改为**机检覆盖度矩阵**，★ 不得再宣称「逐条机检」）
- [x] C2.5 ★ **副产物**：★ C1 的 CMake 断言改**双向**（★ CH_MEM-only **MUST NOT** 留在 TLM 源集合）+ `test_pipe_builder_ports.cpp` **整体**移出 TLM ⇒ ★ TLM 基线 **515 → 514 case**
- [ ] ★ **W1** **G-1e / G-1g** 无机检（★ 弱）⇒ ★ 补机检或显式标注为「仅文档级」
- [ ] ★ **W2** **G-1k** 的机检**依赖 A6** 的「开放核 vs 预载核」时序对比
- [ ] ★ **W3** **G-1i** 的静态机检（★ 已评估并**放弃** ⇒ ★ grep 缩进启发式**误报严重** ⇒ ★ 避免门禁假红 ⇒ ★ 保留「机制锚」单测）

## 4. Phase 4 — S4：验证实现（`verify_mem_model_chmem.h`）

- [x] 4.1 [RED] 测试：验证实现加载 ELF 并正确响应 imem/dmem 请求 [ADR-091 §D2]
  ⇒ ★ **部分完成**：★ **imem/dmem 响应**已验（★ A5-1）⇒ ★ **ELF 加载部分**待 A5-2
- [x] 4.2 [GREEN] 新建 `ip/cpu/core/verify_mem_model_chmem.h`：内存模型 + ELF 加载（★ 参考 TLM 侧 `picolibc_host_memory.h` 的**语义**，★ MUST NOT include 它 —— 它在 ADR-092 冻结集里）[ADR-092 §D1.1]
  ⇒ ★ **A5-1 完成**：★ 内存（`ch_mem`）+ **组合 slave**（★ `aread` 0 拍）
    + 按 `size` 的 **byte-strobe RMW**（★ ADR-091 v1.7：★ CPU 侧不做 RMW）
  ⇒ ★ **架构定位**：★ 从设备属**核外**（★ DUT 外部边界）⇒ ★ **不套层 1/层 2**
    ⇒ ★ 直接做 plugin（★ 理由见 plan §K26.1）
  ⇒ ★ ★ **诚实标注（本版本未实现）**：
    · ✗ **ELF 加载**（★ 只提供 `set_elf_image()` 钩子 ⇒ ★ **不解析**）⇒ ★ 归 **A5-2**
    · ✓ **`mem_sig` / `mem_sig_updates`**（★ ADR-091 v1.11 §D3a.6）⇒ ★ **A5-3 已实现**
      ⇒ ★ 折叠 **逐位同款**（★ `mix = word_addr*31 + store_wdata + sig_upd*0x9E3779B1`
        · `sig = (sig*16777619) ^ mix` · `sig_upd += 写使能`）
      ⇒ ★ `store_wdata` = **RMW 后的最终字**（★ `merged`）⇒ ★ **只有持有内存者能算**
      ⇒ ★ 载体 = **顶层 `ch_out`**（★ 契约名 `mem_sig` / `mem_sig_updates`）
        ⇒ ★ 内部 `ch_reg` 名 `open_mem_sig`（★ G-1o：不撞名）
      ⇒ ★ ★ **逐位同款的直接证据**：★ C++ 侧 reference + 影子内存**逐拍对照**
        （★ 8 次写 × 混合 1/2/4 字节）⇒ 全等
      ⇒ ★ 变异对照 **4 个有效 RED**（★ mix 常数 / 写次数 / 乘数 / ★ `store_wdata` 用原始数据）
        + **2 个无效变异**（★ 「必须显式取值」在开放核侧**不复现** ⇒ ★ 按 V-9 纪律
        **不登记为硬约束**，★ 见 plan §K27.3）
    · ✓ **`tohost` 探针**（★ ADR-091 v1.12 §D3a.9）⇒ ★ **A5-4 已实现**
      ⇒ ★ 归**从设备**（★ 开放核**不持有内存** ⇒ 核侧无法 `aread`）
      ⇒ ★ 字索引 **0x400**（★ 字节 0x1000）★ API `set_tohost_probe_word()`（★ 同预载核）
      ⇒ ★ 载体 = **顶层 `ch_out`**；★ ★★ **契约名 = `tohost_probe_data_proxy`**
        （★ 与预载核**内部** proxy 节点名**完全一致** ⇒ ★ **parity runner 零改动**）
        ⇒ ★ ★ **注**：★ §D3a.9 原文写「名 `tohost`」⇒ ★ **MUST 修正**
          （★ 证据见 plan §K28.1）⇒ ★ **提请 architect 落 ADR**
        ⇒ ★ 内部 proxy MUST 用**别的**名字（★ `tohost_probe_internal` ⇒ **G-1o**）
      ⇒ ★ 变异对照 **3 个全 RED**（★ 探针地址错 / 内部 proxy 撞名 / 不驱动）
  ⇒ ★ 变异对照 **3 个有效 RED**（★ 写使能忽略 → 2 case · RMW 退化 → 1 · byte_off 忽略 → 1）
    + 1 个**无效变异**（★ `aread`→`sread` ⇒ **编译失败** ⇒ ★ 诚实记录，见 plan §K26.3）
- [ ] ★ **A4-0** [裁决] ★ **decode/alu/csr/branch 的 reuse-vs-rewrite**（★ **A4-2 不可启动**）
  ⇒ ★★ **三处契约矛盾**：★ ADR-091 D1「复用 `ip/cpu/arch/riscv/int_alu_chmem.h`（ALU 语义）」
    ⇒ ★ ★★ 但**实测**：★ 该文件 include `<ch.hpp>` + `plugin_base.h` + `pipe_builder.h`
      ⇒ ★ 它**不是纯算法头**，★ 而是**完整 plugin**（★ `RiscvIntAluPlugin : PluginBase`）
      ⇒ ★ ⇒ ★ **绝不能**被开放核 include（★ G-1b：★ `<ch.hpp>` ⇒ 多 TU 链接失败）
    ⇒ ★ 与 tasks 2.3 的「**全部 plugin 均为新建**」**矛盾**
    ⇒ ★ **工作量差 3~5 人日**（★ reuse ≈0.5 vs rewrite ≈3~5 含语义锚测试）
  ⇒ ★ ⇒ ★ **裁决前 A4-2 不可启动**（★ 证据见 plan §K29.3 ⑥）
- [ ] ★ **A5-0** [裁决] ★ **ELF 加载路线**（★ **A5-2 不可启动**）
  ⇒ ★ 路线 1 = **elaboration init vector**（★ `ch_mem` 构造时填初值）⇒ ★ 简单
    ⇒ ★ ★★ 但 **O-5 未解**（★ 网表仍随 ELF 变 ⇒ Verilator 需逐 ELF 重编译）
  ⇒ ★ 路线 2 = **运行期 data_map 注入** ⇒ ★ O-5 真解
    ⇒ ★ ★★ 但上游 `Simulator::get_memory/set_memory` 是**空 stub**
      （★ `CppHDL/src/simulator.cpp:162-167`）⇒ ★ 需上游工作或新增机制
  ⇒ ★ ⇒ ★ 连带：★ ADR-091 D1「O-4/O-5 一并解开」**可能需降级为诚实标注**
  ⇒ ★ **证据见 plan §K29.3 ⑨**
- [ ] ★ **A5-3** [RED+GREEN] ★ **核 ↔ 从设备集成测试**（★ **真实缺口**）
  ⇒ ★★★ **缺口**：★ `test_cpu_open_core_mem_model.cpp` 的 `MemRig` **直接驱动从设备端口**
    ⇒ ★ 而 ibus/dmem 测试**由 host 直接喂 `rsp_data`** ⇒ ★ ★ **两侧各自绿，但从未连过**
  ⇒ ★ ⇒ ★ 本条 = ibus + dmem + verify_mem_model **三者对接**
    ⇒ ★ 跑手工编码的 `add` + `sw` 序列 ⇒ ★ 直至 `mem_sig_updates > 0`
  ⇒ ★ ★ **注**：★ commit `17e9a4c` 写的「ibus/dmem 只断言端口声明、从不读值」
    ⇒ ★★ **该标注不属实**（★ 实测 ibus `:169,170,189` **逐拍读值**）
    ⇒ ★ ⇒ ★ **真实缺口是本条**（★ 公开更正见 plan §K29.1）
- [ ] ★ **B5** [backlog] ★ `chmem_vendored_elf_tohost1` **拆分 / 子进程化**
  ⇒ ★ 47 个 DYNAMIC_SECTION 在**同一进程内累积 47 个 CPU 网表**（★ CppHDL 不释放节点）
    ⇒ ★ 单次跑 **> 25 分钟** ⇒ ★ 全量 CH_MEM 回归**无法一次跑完**
  ⇒ ★ 现仅登记在 AGENTS.md ⇒ ★ **本条补进 tasks**（★ 审查发现）
- [x] 4.3 零回归 + 八门禁（★ **2026-10-30 实测**：★ 六门禁全 PASS
  + ★ CH_MEM **215 case / 4735 assertions** 无新回归 + ★ TLM 无新回归）

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

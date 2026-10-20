---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: draft
depends_on: []
supersedes: []
related_to:
  - ADR-040 v2.0 (TLM↔CH_MEM 双模同语义 — 本 change 的核心是消除双后端行为分歧)
  - ADR-083 (Plugin-style 单一 source of truth)
  - feat-phase-6d-rtl-verification (phase-4, 10 个 pre-existing rv32ui load/store FAIL 的既有 owner)
---

# chmem-dmem-read-latency — CH_MEM dmem 适配上游 `aread` 1-cycle 滞后 (10 个 rv32ui load/store FAIL 归位)

> **范围 (scope)**：`ip/cpu/plugins/dmem_chmem.h` 的读路径 + CH_MEM 数据通路的 load 时序。**不改上游 CppHDL。**
> **触发 (trigger)**：`chmem-mext-comb-datapath` 验收 P0 —— 10 个 rv32ui load/store ELF 长期 FAIL 且**无台账**。
> **本 change 只做归位 + 根因固化 + 方案决策材料**；实施分后续 change（见 §What Changes 的 A/B 方案）。

## Why

### 1.1 症状（2026-10-20 实测）

`[chmem] chmem_vendored_elf_tohost1` 的 39 个 rv32ui 中 **10 个 FAIL**，全部是含 load/store 的用例：

| ELF | tohost | cycles |
|---|---|---|
| `rv32ui-p-lb` / `rv32ui-p-lbu` | 11 | 102 |
| `rv32ui-p-lh` / `rv32ui-p-lhu` | 5 | 62 / 64 |
| `rv32ui-p-lw` / `rv32ui-p-sb` / `rv32ui-p-sh` / `rv32ui-p-sw` / `rv32ui-p-ld_st` / `rv32ui-p-st_ld` | 3 | 57 / 51 / 51 / 53 / 55 / 47 |

tohost = `test_num | 1`（riscv-tests harness fail 协议）⇒ 失败发生在各测试的**第一个 load 依赖检查**。
**其余 29 个不含 load 的 rv32ui 全 PASS** ⇒ 失败面精确等于"读内存"。

**2026-10-20 回退对照实验**（把 `int_alu_chmem.h` 回退到改前版本重建）证明这 10 个 FAIL 与 M 扩展 change 无关，是 pre-existing。

### 1.2 根因（源码级 + 实测，已闭环）

**上游契约（by-design，非 bug）** —— CppHDL `openspec/specs/mem-read-timing/spec.md`：

- **R3**：`type_mem_read_port`（**sync 与 async 两类**）SHALL 归入 `sequential_instr_list_`
  （`src/simulator_init.cpp:116-121`）
- **R4**：drift 明确记录 —— "Simulator 中代理在下一个 posedge 才更新（有效 **1-cycle 滞后**）"，
  且设计意图为"**由下游 register 提供 1-cycle 依赖，如 ChipForge CPU `reg_file <<=`**"，
  "该差异 SHALL NOT 触发任何回归 fail"

**实测（`tests/cpu/test_cpu_chmem_mem_read_drift.cpp`，本次新增的 drift pin）**：
`ch_mem::aread` 在 Simulator 中读到的是**上一拍地址**的内容：

| tick | 当前地址 (`areg`) | 观测读值 | Verilog `assign` 语义期望 |
|---|---|---|---|
| 0 | 1 | **0** | 1 |
| 1 | 2 | **1** | 2 |
| 2 | 3 | **2** | 3 |

（Verilog 侧 `print_mem_read_port` 对 `mem_port_type::async_read` 发射连续 `assign proxy = mem[addr];`，0-cycle）

**⇒ 违约方是 ChipForge，不是上游。** 原因：上游假设"下游 regfile 的 1-cycle 依赖可吸收该滞后"，
这在 **legacy/TLM** dmem 成立；但 **CH_MEM 数据通路全组合**（`cpu_factory_chmem.h:431-435`
无 stage 寄存器），regfile 在**同一次 `eval_sequential()`** 内锁存，且按 node id 顺序
（RegFile plugin 在 DMem plugin 之前注册 ⇒ 节点 id 更小 ⇒ **先锁存**）**先于**读端口更新
⇒ regfile 捕获到的是**上一拍**的读值 ⇒ **"load 的结果 = 上一个访存地址的内容"**。

**旁证**：`[verilator]` 路径同一批 ELF **全 PASS**（48 ELF / 96 assertions）——
同插件、同 DAG，仅后端不同 ⇒ 差异精确落在 interpreter 的读端口相位分类上。

### 1.3 为什么此前没被发现

上游 codegen 注释（`src/codegen_verilog.cpp` `print_mem_read_port`）写明
"ChipForge dmem tolerates both drifts because downstream `reg_file` provides the required 1-cycle latency"
—— 该断言对 **legacy dmem** 成立，被沿用到 **CH_MEM dmem**（Phase 6d 新写）时**未重新验证**。
CH_MEM dmem 从未有过任何 load 语义测试（见 `chmem-alu-unit-semantics` 补的 ALU 语义锚同批）。

## What Changes

### 本 change（归位 + 固化 + 决策材料）

| # | 改动 | 说明 |
|---|---|---|
| 1 | `tests/cpu/test_cpu_chmem_mem_read_drift.cpp` | **新增**：drift pin 回归锚，钉死"aread 滞后 1 拍"（上游若改回 combinational 会 FAIL → 提醒同步适配） |
| 2 | `AGENTS.md` known-issues | 登记该 known issue（含根因 + owner + 方案 A/B 指针），消除"事实存在但无台账" |
| 3 | `openspec/specs/chmem-dmem-read-latency/spec.md` | 新 capability：CH_MEM dmem 读路径 SHALL 与生成 Verilog 同构（load 结果正确） |

### 后续实施方案（本 change 只定方向，不实施）

**方案 A（推荐，backend 同构，CHForge 侧）**：把 dmem 的读端口从 `aread`（Verilog 0-cycle）
换成 `sread`（Verilog `always_ff`，1-cycle）⇒ **两个后端都是 1-cycle load 延迟**（与上游 R4
"由下游 register 提供 1-cycle 依赖"的模型一致）。代价：CH_MEM 数据通路需相应处理 load 的
额外一拍 —— ① hazard RAW stall 需覆盖 load-use；② `sb`/`sh` 的 RMW 读也要走 sread，
于是**写通路也要配一拍**（实质是给 CH_MEM dmem 一级流水）⇒ 属**数据通路级**改动，需独立设计。

**方案 B（上游协调，需上游 change）**：请上游把 **async** 读端口按
`port_type() == ch::core::mem_port_type::async_read` 单独归入 combinational 指令表
（`src/simulator_init.cpp` 一处），恢复 Verilog↔Simulator parity。
代价：上游 R3 是 **hard requirement**（明文要求 sync+async 都归 sequential），
且上游已有 `2026-10-12-followup-mem-read-drift-hardening` 选择了"文档化 + 代码级缓解"路线
⇒ 需上游重新决策，周期不可控。

**方案 C（上游协调，最小）**：请上游提供**显式的 backend 语义开关**（如 `aread` 提供
`aread_combinational()` 变体），ChipForge 侧按后端选择。代价：上游 API 扩展 + ChipForge 侧
条件编译，跨后端行为分叉需在 spec 中显式声明。

### Non-Goals

- **不改上游 CppHDL 仓库**（本 change 只提方案，不改代码）
- **不改 legacy/TLM dmem 路径**（legacy 的 regfile 确实吸收了该滞后）
- **不在本 change 内实施 A/B/C 任一方案**（需独立设计与评审）

## Impact

- **受益**：10 个 rv32ui load/store ELF 从"无台账的长期红点"变为有 owner、有根因、有方案的可追踪项
- **回归面**：本 change 只加 1 个 drift pin 测试（PASS）+ 文档 + spec，**零业务代码改动**
- **风险**：drift pin 测试是"钉死已知缺陷"型测试 —— 上游若修复 aread 相位，该测试会 FAIL
  （这是**期望行为**：提醒同步更新 dmem 适配与本文档）
- **度量**：`[chmem]` family 66→67 case；本 change 不改 `[chmem]` 的 10 个 FAIL 状态

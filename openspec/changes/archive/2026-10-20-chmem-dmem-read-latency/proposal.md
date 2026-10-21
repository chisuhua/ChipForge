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

**失败面的独立佐证（反汇编统计，2026-10-21 实测）**：对全部 47 个 vendored ELF
逐个 `objdump -d` 统计 load / store 指令条数，FAIL 集合与"含 load"集合**逐个精确重合**：

| ELF 分组 | 数量 | load 指令数 | 结果 |
|---|---|---|---|
| FAIL 组（`lb`/`lbu`/`lh`/`lhu`/`lw`/`sb`/`sh`/`sw`/`ld_st`/`st_ld`） | 10 | 19～193 | 全 FAIL |
| PASS 组（其余 rv32ui） | 29 | **全为 0** | 全 PASS |
| rv32um（8 个） | 8 | 全为 0 | 全 PASS |

⇒ **凡含 load 的 ELF 必 FAIL，不含 load 的必 PASS**，无例外。该统计与"load 结果 = 上一拍
地址内容"的根因预测一致，且**独立于** tohost 观测（不依赖被测路径本身的判定逻辑）。

**tohost 观测值的可信性（2026-10-21 复核）**：`tohost` 判定链路本身也走 `aread`
（`dmem_chmem.h:216-220` 的 `tohost_probe`），理论上同样受 1-cycle 滞后影响。
但该链路**不削弱**本证据，理由有二：

1. **地址固定 ⇒ 滞后只影响"读到新值晚一拍"，不改变稳态值。** `tohost_probe`
   读的是**常量** word offset（`0x400` = `0x80001000`），而 Verilator 侧
   （`tools/verilator_runner/sim_main.cpp:91`）直接读 `dmem[]` 数组、**绕开读端口**。
   两后端对"最终写入的 tohost 值"因此一致。
2. **观测值与 harness 语义自洽。** riscv-tests 的 `fail` 路径为
   `ori gp,gp,1` + `sw gp,0(tohost)`（`gp` = 当前 test 编号），故
   `tohost = test_num | 1`。实测反查：`lb` → tohost=11 ⇒ `gp`=10 ⇒ test_10；
   `lh` → tohost=5 ⇒ `gp`=4 ⇒ test_4；`lw`/`sw`/`sb`/`sh`/`ld_st`/`st_ld` → tohost=3 ⇒ `gp`=2。
   逐条与反汇编中的 `li gp,N` 位置吻合 ⇒ 观测值确为 harness 真实写入值。

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
无 stage 寄存器），regfile 在**同一次 `eval_sequential()`** 内锁存，且**先于**读端口更新
⇒ regfile 捕获到的是**上一拍**的读值 ⇒ **"load 的结果 = 上一个访存地址的内容"**。

**顺序成因（已用受控实验确证，非推断）**：`eval_sequential()` 按
`sequential_instr_list_` 的插入顺序求值（`src/simulator.cpp:186-226`），该列表由
`eval_list_`（`context::get_eval_list()`，`src/core/context.cpp:131`）按节点创建序
拓扑排序后筛选 `type_reg` / `type_mem_read_port` / `type_mem_write_port` 得到
（`src/simulator_init.cpp:116-121`）。**受控实验**（同一 CppHDL，两种创建顺序，
仅交换"读端口"与"消费寄存器"的创建先后）：

| 创建顺序 | 消费寄存器捕获到的值 | 结论 |
|---|---|---|
| 读端口先创建（node id 小） | `mem[areg-1]`（**当拍**） | 消费方读到新值 ⇒ 无滞后 |
| 读端口后创建（node id 大） | `mem[areg-2]`（**上一拍**） | 消费方读到旧值 ⇒ 滞后传导 |

CH_MEM CPU 属**后者**：RegFile plugin 先注册（`cpu_factory_chmem.h:225`
"顺序: Decoder → Hazard → IBus → RegFile → IntAlu → Branch → DMem"）⇒ regfile
节点 id 更小（实测 `reg_0`=702 … `reg_31`≈763）⇒ **先于** dmem 读端口
（实测 `load_read`=1707）锁存 ⇒ 滞后被传导进 regfile。**这是"全组合数据通路"
与"上游按 R3 把读端口归 sequential"两个条件叠加的必然结果**，缺一不成立。

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

### 方案选型决策所需的补充证据（2026-10-21 实测，归档后补记）

以下 3 项为选型 A/B/C 的**决定性输入**，在归档后实测补记，供后续实施 change 直接引用，
避免重新踩坑：

**(1) Simulator 侧 `aread` 与 `sread` 行为完全相同**（同 CppHDL，同一 DAG，实测逐拍对比）：

| 读端口 | tick 0 | tick 1 | tick 2 | tick 3 |
|---|---|---|---|---|
| `aread` | obs=0 | obs=1 | obs=2 | obs=3 |
| `sread` | obs=0 | obs=1 | obs=2 | obs=3 |

⇒ **两者在 Simulator 中都是 1-cycle 滞后**（与 `ip/cpu/plugins/ibus_chmem.h:83` 的既有注释
"ch_mem aread/sread both have 1-cycle latency in the simulator" 一致）。
**因此方案 A 并不会改变 Simulator 的读值时序** —— 它改变的是**生成 Verilog 的一侧**
（`aread`→`assign proxy = mem[addr]` 0-cycle；`sread`→`always_ff` 1-cycle），
从而让**两个后端的 load 延迟都变成 1 拍**（后端同构）。
**推论**：方案 A 实施后，Simulator 侧的读值**仍是滞后 1 拍**，
必须靠**数据通路侧吸收**（如 hazard RAW stall 覆盖 load-use）才能让 10 个 ELF 转绿；
**若期望"Simulator 侧读到当拍值"，方案 A 做不到**，只能走 B/C。

**(2) 创建顺序决定滞后是否传导**（受控实验，仅交换读端口与消费寄存器的创建先后）：

| 读端口创建顺序 | 消费寄存器捕获值 |
|---|---|
| 先创建（node id 小） | `mem[areg-1]`（**当拍**读值） |
| 后创建（node id 大） | `mem[areg-2]`（**上一拍**读值） |

CH_MEM CPU 属后者（regfile id 702 < `load_read` 1707）⇒ 滞后传导进 regfile。
**该结论是"根因 = 顺序"的直接证据**，也说明**若能调整注册顺序即可规避**，
但那会改动 Phase 6d.4 的全组合数据通路设计前提（`cpu_factory_chmem.h:431-435`
"fully combinational (no stage pipeline registers)"），属设计变更而非局部补丁。

**(3) 失败面独立佐证**：47 个 vendored ELF 逐个 `objdump` 统计 load 数 ⇒ FAIL 集合与
"含 load"集合**逐个精确重合**（FAIL 组 19~193；PASS 组 29 个 + rv32um 8 个**全为 0**）⇒
失败面独立于 tohost 观测被确证为"读内存"。

**选型建议（2026-10-21 实测后修订 —— 取代此前"优先推进方案 A"的建议）**：

> ⚠️ **原建议"优先推进方案 A"已被实测证伪并撤回。** 理由：把 dmem 的读端口从
> `aread` 换成 `sread` **单独**实施，实测 **`[chmem]` 从 10 个 FAIL 恶化到 11 个**
> （新增 `test_cpu_decoded_inst_migration.cpp:342` 失败 —— 该测试按名探
> `load_read_data_proxy` 节点，而 `sread` 不产生该 proxy 名），**且那 10 个
> load/store ELF 依然全红**（`sread` 在 Simulator 中与 `aread` 行为完全相同，
> 都滞后 1 拍；它只改 Verilog 侧）。方案 A **必须**伴随 CH_MEM 数据通路的
> 一级流水改造（load-use stall + RMW 写通路）才能生效 ⇒ 成本远高于初判。

**修订后的推荐：走方案 B（上游改），且已有实测支撑** —— 2026-10-21 在上游
CppHDL 仓库实测了"async read port 归组合 + 数据流边"的变体：

| 指标 | 结果 |
|---|---|
| ChipForge `[chmem]` | 10 failed → **0 failed**（73 case 全 PASS） |
| ChipForge vendored ELF | 47 个**全 PASS**（16410 assertions） |
| ChipForge `[verilator]` | 4 case / 122 assertions **不变**（PASS） |
| ChipForge TLM | **不受影响**（未动 TLM 路径） |
| ChipForge 业务代码改动 | **零**（无需 load-use stall / RMW 写通路） |
| 上游侧净破坏面 | **1 个断言**（R4 竞态钉住值从"写前值"反转为"写后值" —— 属显式契约变更） |

⇒ **方案 B 是唯一"零 ChipForge 数据通路改动"的解法**，且它让 Simulator 追上
Verilog（双后端 parity 变好）。**方案 A 在上游可全力配合的前提下应放弃**
（成本高、收益相同）；**方案 C（上游加显式 backend 开关）在"上游只能小步改"
的场景下仍是备选**。

**上游已立项**：`CppHDL` 仓库 `openspec/changes/sim-mem-read-timing-align-2026-10/`
（commit `cc41a21` 立项 + `bd56b4c` 补入实测决策证据），含 spec delta
（`mem-read-timing` RENAMED ×2 + MODIFIED ×2）与 8 个 task。
**ChipForge 侧的跟踪项 = 等上游落地后升级依赖、重跑 `[chmem]` 全套、
并把 drift pin 测试从"钉死已知缺陷"改为"确认 skew 已消除"**。

**无论选哪个，都需独立实施 change**（本 change 的 Non-Goals 已明确不实施）。


### Non-Goals

- **不改上游 CppHDL 仓库**（本 change 只提方案，不改代码）
- **不改 legacy/TLM dmem 路径**（legacy 的 regfile 确实吸收了该滞后）
- **不在本 change 内实施 A/B/C 任一方案**（需独立设计与评审）

## Impact

- **受益**：10 个 rv32ui load/store ELF 从"无台账的长期红点"变为有 owner、有根因、有方案的可追踪项
- **回归面**：本 change 只加 1 个 drift pin 测试（PASS）+ 文档 + spec，**零业务代码改动**
- **风险**：drift pin 测试是"钉死已知缺陷"型测试 —— 上游若修复 aread 相位，该测试会 FAIL
  （这是**期望行为**：提醒同步更新 dmem 适配与本文档）
- **度量**：`[chmem]` family 72→73 case（实测：改前 72 case / 71 PASS，改后 73 case / 72 PASS；新增 1 个 `[mem-drift]` case）；本 change 不改 `[chmem]` 的 10 个 FAIL 状态

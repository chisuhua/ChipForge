# Parity 门禁实现契约（A1 · S-1…S-16）

> **Status**: ✅ **v1.4（2026-10-26，builder；v1.4 = A2b 比对器落盘 + S-15 缓存条款实测更正 + §7 覆盖矩阵填实）**
> **上游**: `ADR-089` v1.2（契约本体：D2 观测量 / D3 对齐 / D4 corpus）· `ADR-088` v1.4 §10（目标态与三段路径）
> **下游**: `A2a backend-parity-observability` · `A2b backend-parity-harness` · `A3 backend-parity-corpus` · `A4 backend-parity-ci`
> **粘住机制**: 本文件以 **S-编号** 为唯一引用单元 ⇒ ★ **下游每个 change 的 `tasks.md` 每条 MUST 引 ≥1 个 S 编号**；`G-A2` MUST 含「S ⇒ 实现/测试」覆盖矩阵（§7）

---

## 0. 阅读约定

| 记号 | 含义 |
|---|---|
| **MUST / MUST NOT** | 硬约束。违反 = 契约违规（G-A2 判 FAIL） |
| **S-N** | 可被 `tasks.md` / 测试 / 报告引用的**契约单元编号** ⇒ ★ 新增或改语义 = 扩契约 = **必须经 architect** |
| **归属** | 该条由哪个 change 落地实现 |
| **验收方式** | `机检`（脚本可判）· `变异对照`（必须能证伪）· `人工`（architect 签） |

★ 本文档**不含代码**（A1 零代码：`ip/**` `src/**` `tests/**` `tools/**` 零改动）。§2 的代码事实是**只读引用**，A2a 实现时 MUST 与之保持一致；若实测发现事实错，MUST **先回改本契约**（走 architect），不得就地绕过。

---

## 1. 术语与两侧边界

| 术语 | 定义 |
|---|---|
| **DUT** | 同一个 `PipeBuilder`（同插件集 / 同配置 / 同 ELF）发射出的 lnode DAG。★ **两侧必须是同一份 DAG**（`ADR-089` D2 前置条件） |
| **Simulator 侧** | `pb->create_simulator()` → `Simulator::tick()`（解释执行 DAG，`CppHDL/src/simulator.cpp:267`） |
| **Verilator 侧** | `ch::toVerilog(ctx)` → `verilator --cc --public-flat-rw` → `Vtop`（`tools/verilator_runner/cpu_verilator_sim.cpp` + `sim_main.cpp`） |
| **一拍** | 两侧各推进一个完整时钟周期（含 posedge 与 negedge 各一次求值） |
| **采样点** | 一拍结束、状态稳定后的那个瞬间（S-3 精确定义） |
| **corpus** | 差分执行的输入集合（ELF + branchbench 档），★ **单一清单文件**驱动（S-13） |
| **例外** | 已知的、有机制解释的跨后端分歧；★ **登记制**，不得以容差吸收（S-11/S-12） |

---

## 2. 采集接口事实基线（只读引用；A2a MUST 遵循）

### 2.1 Simulator 侧

| # | 事实 | 证据 |
|---|---|---|
| S2-1 | `Simulator::get_value_by_name(name)` **每次调用都是 O(nodes) 线性扫描**（遍历 `ctx_->get_nodes()` 比 `name()`） | `CppHDL/src/simulator.cpp:455-470` |
| S2-2 | `Simulator::get_signal_id_by_name(name)` **只查 trace 注册表** `signals_`；而 trace **默认关**（`Simulator(ctx, trace_on=false)`，`signals_` 仅在 `collect_signals()` 后填充）⇒ ★ **不可用作通用解析器** | `CppHDL/include/simulator.h:557-568`、`include/simulator.h:9` |
| S2-3 | `data_map()` 返回 `const ch::data_map_t&` = `unordered_map<uint32_t, sdata_type>`（node_id → 值） | `CppHDL/include/simulator.h:477`、`CppHDL/include/ast/instr_base.h:15` |
| S2-4 | `sdata_type::operator uint64_t()` 是 **explicit** ⇒ 读法 = `static_cast<uint64_t>(slot)` | `CppHDL/include/core/types.h:96` |
| S2-5 | `Simulator::get_memory(addr,size)` 是**空 stub**（`return {}`），且被 `#ifdef ENABLE_MEMORY_API` 包住（构建未启用）⇒ ★ **无 host 侧内存读出通道** | `CppHDL/src/simulator.cpp:161-168`、`include/simulator.h:581-582` |
| S2-6 | `Simulator::tick()` = `eval_combinational()` → (`eval()` ×2：posedge + negedge) → `eval_combinational()` | `CppHDL/src/simulator.cpp:267-345` |
| S2-7 | JIT 存在但**默认关**（`jit_enabled_` 仅在 `CH_JIT_ENABLED` 时 true） | `CppHDL/include/simulator.h:645-654` |
| S2-8 | 寄存器堆节点名 = `reg_0` … `reg_31`（逐寄存器显式命名，防默认名撞名） | `ip/cpu/plugins/reg_file_chmem.h:162-170` |
| S2-9 | PC 寄存器节点名 = `pc` | `ip/cpu/plugins/ibus_chmem.h:125-129` |
| S2-10 | `tohost` 探针节点名 = `tohost_probe_data_proxy`（固定 word 偏移的 `aread` 探针） | `ip/cpu/plugins/dmem_chmem.h:214-222` |
| S2-11 | 现有 ELF 测试：**每拍** `get_value_by_name("tohost_probe_data_proxy")` + 自数 `cycles`；`kMaxCycles = 1500`；★ **不调 `sim->reset()`**（reset 会用 `rst_val` 覆盖 PC 初值 `0x80000000`） | `tests/cpu/test_cpu_chmem_vendored_elf.cpp:43,157-172` |
| S2-12 | 跨阶段 payload key 命名：`cpu.pc` / `cpu.mem_addr` / `cpu.mem_data` / `cpu.mem_size` / `cpu.rd_data` / `cpu.rd_idx` … | `ip/cpu/core/payload_common.h:118-170` |
| ★ S2-13 | **实测吞吐 ≈ 9.1 拍/s**（`rv32ui-p-add`：478 拍 / **52.8 s** / DAG **2485** 节点）⇒ `--no-bp` 与默认 BP 无显著差异（200 拍：21 s vs 24 s）⇒ 瓶颈是解释器本身 | A2a increment 1 实测（`tools/parity_runner`） |
| ★ S2-14 | ★ **ADR-089 早前引用的「575 拍/s」高估约 60×** ⇒ 一切「按拍数估时长」的预算 MUST 以本行为准 | 同上 |
| ★ S2-15 | ★ **`ch_reg` 继承自 `ch_uint`** ⇒ 直接写 `reg * X` 会走 **ch_uint 的编译期值**路径（恒 0）⇒ 表达式退化成常量 ⇒ 该 reg 被 codegen 裁掉（网表里根本没有它）⇒ ★ **读 reg 必须显式取值** `ch_uint<N> v(reg);`（同 `ibus_chmem.h` 的 `flush_count` 写法） | `CppHDL/include/core/reg.h:32-60`、`lnode/reg.tpp` |
| ★ S2-16 | ★ **codegen 只发射 eval list 里的节点**（`verilogwriter` 用 `ctx_->get_eval_list()`）⇒ 「DAG 里有」≠「网表里有」⇒ 排查用 `parity_sim_runner --dump-nodes <substr>`（打印节点 + 是否在 eval list） | `CppHDL/src/codegen_verilog.cpp:65-82` |

### 2.2 Verilator 侧

| # | 事实 | 证据 |
|---|---|---|
| V2-1 | host 读通道全部经 `--public-flat-rw` + `rootp->top__DOT__*`：`dmem[word]`（`top__DOT__dmem`）、`top__DOT___pc`、`top__DOT__flush_count`、`top__DOT__bp_{hit,miss,taken}_count` | `tools/verilator_runner/sim_main.cpp:75-110` |
| V2-2 | `kToHostWord = 0x400`（= `(0x80001000 - 0x80000000)/4`） | `tools/verilator_runner/sim_main.cpp:55` |
| V2-3 | 时钟循环 = `clk=1; eval(); clk=0; eval();` ⇒ **采样点在 `clk=0` 的 `eval()` 之后** | `tools/verilator_runner/sim_main.cpp:136-141`、`197-205` |
| V2-4 | `cf_run_sim` **先打 1 拍 reset pulse**（`reset=1` → `clk=1;eval` → `reset=0;clk=0;eval`） | `tools/verilator_runner/sim_main.cpp:117-127` |
| V2-5 | `--stats` 的 `PC_HIST` 是**直方图**（按 4-byte slot 计数，`dropped` 计入窗口外）⇒ ★ **丢顺序** | `tools/verilator_runner/sim_main.cpp:190-222`；`cpu_verilator_sim.cpp:108-110` |
| V2-6 | 每次调用都 `generate_verilog()` + `verilator --cc` + `g++` 编译（**无按 config 缓存**） | `tools/verilator_runner/cpu_verilator_sim.cpp`（编译流程段） |
| V2-7 | CLI 全集：`--elf` `--verilog` `--work-dir` `--cycles` `--enable-mmu` `--mmu-mode` `--enable-cache` `--stats` `--stats-base` `--stats-len` `--no-bp` `--no-bp-gshare` `--no-bp-uncond` `--btb-entries` | `tools/verilator_runner/cpu_verilator_sim.cpp:129-186` |
| V2-8 | branchbench 跑批：`--elf build/branchbench_m{0..3}.elf --cycles 2000000 [--no-bp] --stats`，判定靠 **tohost 校验和**（≠1）而非退出码 | `tools/measure_branchbench.sh:39-56,255-290` |
| V2-9 | 信号名 → Verilog 名带前缀下划线：`pc` → `top__DOT___pc`、`reg_N` → `top__DOT___reg_N`（★ **已实测确认**：生成 Verilog 声明 `logic [31:0] _reg_30;`，Verilator 头 `IData/*31:0*/ top__DOT___reg_30`） | `sim_main.cpp:80` + 2026-10-26 实测 |

### 2.3 ★ 初始状态与相位（本节最重要的事实结论；2026-10-26 实测更正）

| 侧 | 一拍的结构 | 采样点 | 初始状态从哪来 |
|---|---|---|---|
| Simulator | `tick()` = comb → (posedge, negedge) → comb | `tick()` 返回后 | ★ **构造期初始化**（`ch_reg` ctor 的 init 值；`Simulator::reset()` 只是**把非 literal 槽位清零**，**不**加载 init） |
| Verilator | `clk=1;eval(); clk=0;eval();` | 第二次 `eval()` 后 | ★ **复位边沿**（`else pc <= 32'h80000000;` —— 生成的 Verilog 里 init 值**只在 reset 分支**赋） |

⇒ ★★ **两侧一拍同构、采样相位 offset = 0**（3 个 ELF 实测：`pchash` 逐位相等，`pc_head` 首位即 `0x80000004`）。
⇒ ★★ **但"初始状态"的加载路径不同**，且**不可统一成同一字面模式**：
- ✗ **不能两边都不 reset** —— 生成的 Verilog 没有 `initial`，跳过复位脉冲 ⇒ `pc` 从 **0** 起（实测：`x30 = 0x1000` 而非 `0x80001000`）⇒ ★ **`--reset-pulse 0` 不是 parity 兼容配置**（本轮已加 stderr 警告）
- ✗ **不能两边都调 `Simulator::reset()`** —— 它把槽位**清零**，会丢掉 `pc` 的 init `0x80000000`（既有 47-ELF harness 就是因此**故意不 reset**）
- ✓ **正确口径 = `init_semantics = ctor-init`**：Simulator 走"不 reset ⇒ 保留 ctor init"；Verilator 走"1 拍 reset ⇒ 复位分支加载**同一批** ctor init 值"。两条路径**语义等价**，MUST 以 `pchash` + `regs` 逐位相等作为等价证据（本轮 3/3 ELF 已验证）。
⇒ ★ **`--reset-pulse 0` 只能作诊断用途**，报告里 MUST 标 `reset_mode = "pulse0(diagnostic)"`，MUST NOT 与 Simulator 结果比对。

---|---|---|---|
| Simulator | `tick()` = comb → (posedge, negedge) → comb | `tick()` 返回后 | ★ **不打 reset**（既有 harness 有意跳过，S2-11） |
| Verilator | `clk=1;eval; clk=0;eval` | 第二个 `eval()` 后 | ★ **打 1 拍 reset pulse**（V2-4） |

⇒ ★★ **两侧"一拍"同构**（各含一次 posedge + 一次 negedge 求值）⇒ 采样相位**原理上 offset = 0**。
⇒ ★★ **但 `reset` 处理不同** ⇒ 初始状态不同 ⇒ ★ **这是一处真实分歧源，必须在 A2a 消除**：契约要求 `reset_mode = none`（两侧一致），Verilator 侧 MUST 以**新增可选开关**（如 `--reset-pulse=0`）实现，**MUST NOT 改动既有默认行为**（否则打翻 48/48 基线）。
⇒ 实测校准值与 reset 统一后的 47 ELF 重基线结果，MUST 回写 §2.3 + S-3（开放项 O-1 / C-2）。

---

## 3. 契约条目

### S-1 观测量命名与语义冻结

**要求**
- 5 个观测量 id **固定**为：`tohost` / `regs` / `memsig` / `cycles` / `pchash`；报告、白名单、脚本参数一律用这 5 个 id。
- 每个 id 的语义 = `ADR-089` D2 的定义，**MUST NOT** 在实现里改语义（例：把 `pchash` 换成直方图 hash）。
- 新增/删除观测量 ⇒ **扩契约**（architect 审）。

**验收方式**：机检（报告 schema 里 5 个 key 必须齐备，多出未声明 key ⇒ 拒收）。
**归属**：A2b（校验）+ A2a（产出）。

---

### S-2 确定性前置校验（先于任何观测量比对）

**要求**
- 两侧 elaborate 完成后 MUST 各自导出并比对：① **DAG node_count** ② **顶层信号名名单的排序后 hash**。
- 二者任一不等 ⇒ **不得继续比对**（`ADR-089` D2 前置条件）。
- 判定分类：本契约归 **`ERROR` + `error_kind = determinism_mismatch`**（门禁/环境问题，而**不是**后端语义分歧）⇒ ★ 见开放项 **C-1**（`ADR-089` 原文用词是 FAIL，本契约按三态语义细化，待 architect 确认）。
- 名单 hash 的取法（排序、分隔符、截断）MUST 与 A2a 实现一起写进报告，便于复现。

**验收方式**：变异对照（人为改一个配置参数 ⇒ MUST 报 `determinism_mismatch`，且**不**产生任何 per-observation 比对结果）。
**归属**：A2a（导出）+ A2b（比对）。

---

### S-3 采样相位、初始状态与对齐（★ 禁自适应）

**要求**
- Simulator 采样点 = `sim->tick()` 返回后；Verilator 采样点 = `clk=0` 的 `eval()` 之后（V2-3）⇒ **offset MUST 为 0**。
- ★ **MUST NOT** 在比对器里实现「找最佳 offset」「LCS 对齐」之类自适应逻辑 —— 那会把真实的 **1 拍延迟分歧**（`c27543d` 那一类）静默吞掉。
- ★★ **初始状态口径 = `init_semantics = ctor-init`**：
  - Simulator：**不调用** `Simulator::reset()`（它只清零，会丢 `pc` 的 init `0x80000000`）
  - Verilator：**打 1 拍 reset 脉冲**（生成的 Verilog 只在复位分支加载 init 值）
  - ⇒ 两者**语义等价**，但**字面模式不同** ⇒ 报告 MUST 分别记录 `reset_mode = "sim:ctor-init"` / `"vl:reset-pulse-1"`
- ★ **`--reset-pulse 0` MUST NOT 用于 parity 比对**（实测：`pc` 从 0 起、`x30` 变 `0x1000`）⇒ 仅诊断用途，报告标 `pulse0(diagnostic)`。
- 若实测发现恒定 offset `k ≠ 0` ⇒ MUST 记录到本节 + 走 architect，**MUST NOT** 就地补偿。

**验收方式**：变异对照（把 Simulator 侧某机制延迟 +1 拍 ⇒ MUST 报差异，而不是被判成 offset）· 等价性证据（3 个 ELF 的 `pchash`/`regs` 逐位相等）· 人工（相位结论签字）。
**归属**：A2a。

### S-4 观测量 ① `tohost`

**要求**
- Simulator：解析节点 `tohost_probe_data_proxy`（S2-10）**一次**，之后每拍直读 `data_map()`。
- Verilator：读 `top__DOT__dmem[0x400]`（V2-1/V2-2）。
- 取值语义 = 32-bit 无符号；**done 判定** = `(tohost & 1) != 0`，**pass 判定** = `tohost == 1`（与既有 harness 一致，S2-11）。
- 节点/信号缺失 ⇒ `ERROR`（**MUST NOT** 按 0 处理）。

**验收方式**：机检（两侧同一 ELF 的 `tohost` 末值相等）；变异对照（缺信号 ⇒ ERROR）。
**归属**：A2a。

---

### S-5 观测量 ② `regs`（x1..x31）

**要求**
- 采集 `x1..x31`（**x0 不采**：恒 0，比它只会掩盖「写 x0」的真 bug —— 该 bug 由 ⑤/② 其他寄存器体现）。
- 序列化：按 `x1 → x31` 升序，每个寄存器取 **32-bit 小端** 4 字节，拼成 124 字节后取 `sha256`。
- Simulator：解析节点 `reg_1` … `reg_31`（S2-8）一次，之后直读 `data_map()`（**MUST NOT** 每拍 `get_value_by_name`，S2-1）。
- Verilator：经 `--public-flat-rw` 读对应顶层信号；★ 实际信号名 MUST 由 A2a 实测后**回写 §2.2**（V2-9，开放项 O-2）。缺失任一寄存器 ⇒ `ERROR`。

**验收方式**：机检（同 ELF 两侧 `regs` hash 相等）；变异对照（改 1 个寄存器最终值 ⇒ hash 变）。
**归属**：A2a。

---

### S-6 观测量 ③ `memsig`（DUT 自算内存签名）

**要求**
- ★ **MUST 是 DUT 内自算**，**MUST NOT** 依赖 host 全内存 dump（Simulator 侧无此通道，S2-5）。
- **落法（已实装）**：`ip/cpu/plugins/dmem_chmem.h` 在**写路径旁**挂一个只读累加器（复用该处已有的
  `word_addr` / `store_wdata` / `is_store_active`，**不新增 payload**），导出两个 named reg：
  - `mem_sig`（32-bit 滚动签名）· `mem_sig_updates`（32-bit 写次数）
  - ★ 两侧读的是**同一 DAG 的两个 named reg** ⇒ 天然可比（无需任何 host 镜像）
  - ★ 生成的 Verilog 里对应 `logic [31:0] mem_sig;` + `assign _mem_sig = mem_sig;`
    ⇒ host 经 `--public-flat-rw` 读 `top__DOT__mem_sig` / `top__DOT__mem_sig_updates`
- **签名函数（MUST 固定，A2a 定稿）**（32-bit 模运算）：
  ```
  mix    = word_addr * 31 + store_wdata + sig_updates * 0x9E3779B1
  sig'   = sig * 16777619 ^ mix          （sig' 仅在 is_store_active 时生效）
  ```
  ★ `word_addr` 是**字**地址；`store_wdata` 是 **sub-word RMW 之后的最终字面值**；
  ★ 计入 `sig_updates`（写次数）是为区分「同一 (addr,data) 但发生在不同时刻」。
  ★ 条件与 `mem.write()` 的写使能**同源**（`is_store_active`）⇒ 被 flush 掉的 store 不计（它本就没进内存）。
- **自校验（fail-closed，MUST）**：若「`tohost` 已被写过（≠0）但 `mem_sig_updates == 0`」
  ⇒ 签名是**假绿** ⇒ 门禁 MUST 判 **ERROR(2)**，**MUST NOT** 接受「两边都是 0」。
  ★ 两侧都 MUST 实现该自校验（Simulator runner + Verilator parity 入口）。
- 备选（增强项，不在本期）：guest 侧校验和（需改 ELF harness ⇒ 成本高）。

**验收方式**
- 变异对照 ①：把 `mix` 里的 `store_wdata` 取反 ⇒ `memsig` MUST 变
  （实测 `a55f52c5` → `8fcc53ba` ⇒ 签名对写数据**敏感**）
- 变异对照 ②：把 `sig_upd` 强制恒 0 ⇒ 两侧 MUST 判 **ERROR(2)** 并打印
  `mem_sig_updates == 0 ... false green`（实测两侧 rc=2 且消息一致）
- 机检：同 ELF 两侧 `memsig` / `memsig_updates` 相等
- ★ **零 cycle 影响**：加探针前后同 ELF 的 `cycles` MUST 不变
  （实测 `rv32ui-p-add` 478 / `rv32um-p-mul` 472 / `rv32ui-p-simple` 38 全部不变）

**归属**：A2a。

### S-7 观测量 ④ `cycles`（总拍数，标量）

**要求**
- 语义 = **从第一拍推进到 done 判定那一拍（含）为止的拍数**；两侧循环结构 MUST 同构：

```
cycles = 0
loop:
  step_one_cycle()          # Simulator: tick() | Verilator: clk1/eval; clk0/eval
  cycles += 1
  sample()                  # 所有观测量在同一采样点取值
  if (tohost & 1) break
  if cycles >= max_cycles: -> TIMEOUT (ERROR，不是 FAIL)
```

- ★ **MUST NOT** 用「④ + 末态」替代 ⑤（`ADR-089` D2 判据强度排序）。

**验收方式**：机检（两侧 `cycles` 相等，或在白名单登记的例外范围内）。
**归属**：A2a（采集）+ A2b（比对）。

---

### S-8 观测量 ⑤ `pchash`（逐拍 PC 流）

**要求**
- **每拍**采样 PC 值一次，按序喂滚动 hash（`h = fnv1a64(h, pc_u32)`）；报告 `pchash` + `pc_samples`。
- ★ **MUST NOT** 用直方图（V2-5：`PC_HIST` 丢顺序）作为 ⑤；直方图最多作为诊断附件。
- Simulator：节点 `pc`（S2-9）；Verilator：`top__DOT___pc`（V2-1）。
- **可诊断性**：报告 MUST 记录**首个分歧拍**（index + 两侧 PC 值 + 两侧累计 hash），MUST NOT 只给一个不等的 hash。
- ★ 与 `ADR-089` D3 的关系：**「逐拍相等」的判定全部落在本项**（④ 只是首/末标量）。

**验收方式**：变异对照（两侧任一机制差 1 拍 ⇒ `pchash` MUST 不同 + 首分歧拍可定位）；机检（同 ELF 两侧 `pchash` 相等）。
**归属**：A2a（采集）+ A2b（比对）。

---

### S-9 判定规则与"禁降级"

**要求**
- PASS 条件（全部成立）：S-2 确定性一致 · 5 观测量两侧相等（或命中合规例外）· 无 `ERROR`。
- 判据强度：`pchash` > `cycles` + `regs`/`memsig` > 仅 `tohost` ⇒ ★ **MUST NOT** 因某项不可采而"降级通过"（不可采 = `ERROR`）。
- 例外命中 MUST 精确匹配 `(observation, corpus 作用域, mechanism)`；`corpus_scope: ["*"]` 的条目 MUST 附 `justification`（S-11）。

**验收方式**：机检（构造 3 组输入：全等 ⇒ PASS；单观测不等 ⇒ FAIL；观测缺失 ⇒ ERROR）。
**归属**：A2b。

---

### S-10 三态退出码与错误分类

**要求**
- `0 = PASS` · `1 = FAIL`（**后端分歧**）· `2 = ERROR`（**门禁自身/环境故障**）。
- `ERROR` 涵盖：TIMEOUT、runner 进程崩溃（segfault/abort）、观测量缺失、确定性不符（S-2）、白名单格式非法、语料集合不一致（S-13）。
- ★ **MUST NOT** 把 `ERROR` 计入「既存后端分歧清单」（`ADR-089` V-1）。
- 报告 MUST 同时给 `verdict` 与 `error_kind`（见 S-14）。

**验收方式**：机检（4 类故障各造 1 例 ⇒ 退出码 2 且 `error_kind` 正确）。
**归属**：A2b。

---

### S-11 例外白名单文件格式

**要求**
- 单一受控文件（A2b 落盘）：`tests/cpu/parity/exceptions.json`；门禁只认这一个路径。
- 每条字段（**5 必填 + 3 元数据**，**未知字段一律拒收** —— 防偷偷加 `tolerance`）：

| 字段 | 类型 | 说明 |
|---|---|---|
| `id` | string | 唯一，形如 `EX-0001` |
| `observation` | enum | `tohost`/`regs`/`memsig`/`cycles`/`pchash` |
| `corpus_scope` | string[] | ELF 名或档名；`["*"]` MUST 同时给 `justification` |
| `mechanism` | string | **机制说明**（哪条语义不同、为什么） |
| `upstream_status` | enum | `upstream-pending` / `by-design`；`upstream-pending` MUST 给 `issue_url` |
| `mutation_control` | string | **变异对照**：能证伪「例外被误当回归」的命令或断言 id |
| `sunset` | string | **收敛期限**（ISO date，≤ 1 个发布窗口） |
| `owner` | string | 责任人 |

- ★ **MUST NOT** 出现容差类字段（`tolerance` / `eps` / `max_delta`）。
- **常驻合成例外**：固定 `id = "synthetic-self-test"` + `synthetic: true`，用于持续验证白名单机制不随真实例外清零而腐化；★ **不计入**真实例外计数。

**验收方式**：机检（schema 校验 + 未知字段拒收）；变异对照（删任一必填字段 ⇒ 门禁 FAIL；改 `sunset` 为过去日期 ⇒ 门禁 FAIL）。
**归属**：A2b。

---

### S-12 白名单扩张权与规模护栏

**要求**
- 白名单**扩张** MUST 经 **architect 审**（`ADR-089` D3，2026-10-26 用户裁决）；实施者**不得**自行新增条目。
- 门禁 MUST 校验：含白名单变更的 commit message 引 `ADR-089`（机检）。
  ★ **v1.4 状态（诚实标注）**：规模护栏 / 过期失效 / 扩张权（architect 审）**已实现**；**「commit message 引裁决编号」的机检尚未实现**（需 git hook 级检查）⇒ 登记为 A4 的待办，★ **MUST NOT 假装已覆盖**。
- 真实例外条目数 **> 10**（`N = 10`）⇒ 报告顶部 **强制提示 architect 复审**；★ **MUST NOT** 以「超过 N ⇒ 退出码非 0」作护栏（会逼出假登记）。
- `sunset` 已过期 ⇒ 该条目**自动失效** ⇒ 门禁 FAIL，并标注「升级为真 bug 类」。

**验收方式**：机检（commit message 格式、条目计数、过期判定）；人工（architect 签）。
**归属**：A2b + 流程。

---

### S-13 语料清单的单一来源

**要求**
- 清单文件：`tests/cpu/parity/corpus.txt`，每行 `name<TAB>kind<TAB>relpath<TAB>layer`，`#` 起注释；`kind ∈ {elf, branchbench}` · `layer ∈ {pr, night}`。★ **归属细化（v1.4）**：**A2b 落「格式 + 种子清单」**（47 ELF + 4 branchbench = **51 case**）· **A3 落「扩到 48**（补 `rv32ui-p-simple`）**+ 首跑」**。
- ★ 两侧 harness MUST **都从这一个文件**读取；MUST NOT 各自维护 C++ 数组。
- 门禁 MUST 断言两侧 harness 对清单的覆盖，且 **v1.4 起按方向性判定**：
  - **清单 ⊄ 某侧 `kCases`** ⇒ `ERROR` / `corpus_mismatch`（该 case 不会被那一侧跑 ⇒ 差分缺一侧 ⇒ **真漏检**）
  - **某侧 `kCases` ⊄ 清单** ⇒ `WARNING`（**覆盖超集**：对清单内 case 无差分风险；当前事实 = `rv32ui-p-simple`
    只在 Verilator 侧 ⇒ 由 A3 收口）
- 首批：两侧交集 **47** ELF + branchbench **4** 档；★ `rv32ui-p-simple` MUST 在清单内（Simulator 侧 `kCases` 需补 1 行 —— A3 落）。
- 新增 corpus MUST 先有「5 观测量可采」的证据，否则登记为「该观测量对该 corpus 豁免」（走 S-11，禁止静默跳过）。

**验收方式**：机检（三方集合相等）；变异对照（清单删 1 行 ⇒ 门禁 ERROR）。
**归属**：A3（落盘）+ A2b（断言）。

---

### S-14 报告格式

**要求**
- 产物：`build/parity/report.json`（machine）+ stdout 摘要（human）。
- **必备字段**：
  `schema_version` / `verdict`(PASS|FAIL|ERROR) / `error_kind` / `timestamp` / `commit` / `reset_mode` / `corpus_source` / `determinism{node_count,names_hash}` / `per_case[]{name, observations{tohost,regs,memsig,cycles,pchash}, first_divergence, verdict}` / `exceptions_applied[]` / `timings{sim_ms,vl_build_ms,vl_run_ms}`。
- 每个 case 的 5 个观测量 **MUST 全部出现**；缺失 = `ERROR`（**MUST NOT** 省略或填 0）。
- `first_divergence` MUST 给 `{metric, cycle_index, sim_value, vl_value}`（可诊断性硬要求）。

**验收方式**：机检（schema 校验 + 字段齐备性）。
**归属**：A2b。

---

### S-15 运行时预算与分层

**要求**
- **PR 层**：ELF 子集 **≤ 8**（MUST 含 `add`/`sub`/`lw`/`sw`/`beq` 类 + 1 个 rv32um）+ branchbench **0 档**。
- **夜间层**：**51 case = 47 ELF + 4 branchbench 档**（以 S-13 清单为准，全部覆盖）。
- ★★ **Verilog 构建缓存（v1.4 更正 —— 原「按 config 缓存」被实测推翻）**：生成的 Verilog **把 ELF 的
  指令内存烤进了网表**（实测 `imem[32] = 32'h200193;`；两个 ELF 的网表差 640 行）⇒ **每个 ELF 的网表都不同** ⇒
  ★ **跨 ELF 无可复用** ⇒ 现实约束是「**每 corpus 一次编译**」。
  ⇒ 时长预算据此重算：**PR 层 8 case ≈ 10 min**（Simulator ≈7 min + Verilator ≈3 min）·
  **夜间 51 case ≈ 1 h**（Simulator ≈45 min + Verilator ≈17 min）。
  ★ 要真正省掉 Verilator 侧编译，必须把 ELF 从 **elaboration 期**移出（imem 初始化改为运行期加载）
  ⇒ 属 DUT/上游级改动 ⇒ 登记为开放项 **O-5**（不在本期）。
- ★ 分层（v1.4 落实）：清单 `layer` 列 + `--layer pr|night|all` 驱动；PR 层的 case 集合 MUST ⊆ 清单。
- ★★ **branchbench 在 Simulator 侧的 cap MUST ≤ 20,000 拍**（实测 ≈ 9.1 拍/s ⇒ 20k ≈ 37 min/档）；`--cycles 2,000,000`（Verilator 侧现状默认）在 Simulator 侧 ≈ 61 小时/档，**不可行** ⇒ 若某档无法在 cap 内到达 done，MUST 登记为「该 corpus 仅 Verilator 侧可跑」的显式豁免（走 S-11），**MUST NOT** 静默跳过。
- 时长阈值：★ **本期不设未实测的硬阈值**；A4 MUST 先把实测 p95 写进报告 `timings`，再据此定阈值。

**验收方式**：机检（分层集合 ⊆ S-13 清单；`--layer` 只执行对应层）× 人工（A4 定时长阈值）。
**归属**：A2b（分层驱动）· A4（接线 + 阈值）。

---

### S-16 契约"粘住"机制

**要求**
- S 编号是**唯一引用单元**；下游每个 change 的 `tasks.md` **每条任务 MUST 引 ≥1 个 S 编号**。
- `G-A2` MUST 交付 **「S ⇒ 实现/测试」覆盖矩阵**（§7 模板）：每个 S 至少 1 个实现落点，或显式 `N/A + 理由`。
- 契约变更（新增 S / 改语义 / 改判定强度）MUST 经 **architect**；纯笔误/措辞修正可由 builder 直接改并在 commit message 注明。

**验收方式**：机检（A2b/A3/A4 的 tasks.md 每条含 `S-\d+`；矩阵无空行）；人工（architect 签）。
**归属**：全部 + G-A2。

---

## 4. 语料清单格式（S-13 细化）

```text
# tests/cpu/parity/corpus.txt
# name<TAB>kind<TAB>relpath   （# 起注释；空行忽略）
rv32ui-p-add	elf	tests/cpu/riscv_tests/elf/rv32ui-p-add
...
rv32ui-p-simple	elf	tests/cpu/riscv_tests/elf/rv32ui-p-simple
rv32um-p-mul	elf	tests/cpu/riscv_tests/elf/rv32um-p-mul
branchbench_m0	branchbench	build/branchbench_m0.elf
```

- `relpath` 相对**仓库根**（两侧 harness 都以 `CMAKE_SOURCE_DIR` 为基准解析）。
- branchbench 的 `--cycles` cap MUST 由清单外的配置给出（默认 2,000,000，与 `measure_branchbench.sh` 一致）；★ 若 Simulator 侧实测单档 > 10 min ⇒ 该档默认归夜间层。

---

## 5. 例外白名单（S-11 细化）

```json
{
  "schema_version": "parity-exceptions-v1",
  "exceptions": [
    {
      "id": "EX-0001",
      "observation": "cycles",
      "corpus_scope": ["rv32ui-p-lw"],
      "mechanism": "<哪条语义不同、为什么不同>",
      "upstream_status": "upstream-pending",
      "issue_url": "<上游 issue>",
      "mutation_control": "bash tools/parity/run.sh --mutation MEM_READ_LAG+1",
      "sunset": "2026-12-31",
      "owner": "<owner>"
    },
    {
      "id": "synthetic-self-test",
      "synthetic": true,
      "observation": "regs",
      "corpus_scope": ["__synthetic__"],
      "mechanism": "常驻 fixture：验证白名单机制本身不会随真实例外清零而腐化",
      "upstream_status": "by-design",
      "mutation_control": "bash tools/parity/run.sh --self-test-exceptions",
      "sunset": "2099-12-31",
      "owner": "parity-gate"
    }
  ]
}
```

---

## 6. 报告格式（S-14 细化）

```json
{
  "schema_version": "parity-report-v1",
  "verdict": "FAIL",
  "error_kind": null,
  "timestamp": "2026-10-26T10:00:00Z",
  "commit": "<sha>",
  "reset_mode": "none",
  "corpus_source": "tests/cpu/parity/corpus.txt",
  "determinism": { "node_count": 12345, "names_hash": "…" },
  "per_case": [
    {
      "name": "rv32ui-p-add",
      "observations": { "tohost": 1, "regs": "…", "memsig": "…", "memsig_updates": 1, "cycles": 455, "pchash": "…" },
      "first_divergence": { "metric": "cycles", "cycle_index": 0, "sim_value": 455, "vl_value": 456 },
      "verdict": "FAIL"
    }
  ],
  "exceptions_applied": ["EX-0001"],
  "timings": { "sim_ms": 0, "vl_build_ms": 0, "vl_run_ms": 0 }
}
```

---

## 7. 「S ⇒ 实现/测试」覆盖矩阵（G-A2 必交；2026-10-26 填实）

> ★ 本表是 **G-A2** 的核心交付物：每个 S 至少 1 个实现落点，或显式 `N/A + 理由`。

| S | 归属 | 实现落点 | 测试 / 变异对照 | 状态 |
|---|---|---|---|---|
| S-1 | A2b | `tools/parity/parity_gate.py` 的 `OBSERVATIONS`（5 个 id 冻结） | `--self-test`：全等 PASS / 缺失 ERROR | ✅ |
| S-2 | A2a+A2b | Simulator runner 导出 `determinism` · `cpu_verilator_sim::generate_verilog()` 从 **DAG** 导出 · `judge()` 前置校验 | 两侧实测 2511 / `67205f7d…` 相等；`--self-test` 的 `determinism_mismatch` | ✅ |
| S-3 | A2a | runner 强制 `--reset none`（fail-fast）· VL `--reset-pulse`（默认 1；0 仅诊断） | 3 ELF `pchash`/`pc_head` 逐位相等；`--reset-pulse 0` 反例（`x30=0x1000`） | ✅ |
| S-4 | A2a | Sim：节点 `tohost_probe_data_proxy` · VL：`top__DOT__dmem[0x400]` | 5/5 corpus `tohost` 相等 | ✅ |
| S-5 | A2a | Sim：节点 `reg_1..reg_31` · VL：`read_reg()`（`top__DOT___reg_N`，实测确认） | 31 个寄存器原始值 diff 为空 | ✅ |
| S-6 | A2a | `ip/cpu/plugins/dmem_chmem.h` 探针 → `mem_sig` / `mem_sig_updates`；两侧各自读取 + 假绿自校验 | 变异 ①数据取反（`a55f52c5`→`8fcc53ba`）②计数恒 0 ⇒ 两侧 rc=2；零 cycle 影响；5/5 相等 | ✅ |
| S-7 | A2a | 两侧同构循环（`step → cycles+=1 → sample → break on done`） | 5/5 `cycles` 相等 | ✅ |
| S-8 | A2a | 两侧逐拍 FNV-1a over PC + `pc_samples` | 5/5 `pchash` 相等；相位 offset = 0 | ✅ |
| S-9 | A2b | `judge()`（纯函数：强度序比对 + `first_divergence` + 禁降级） | `--self-test` 9 例 | ✅ |
| S-10 | A2b | `main()` 三态返回；VL parity 路径 TIMEOUT → 2 | 负向 N1~N5（均 rc=2）；branchbench 两侧 rc=2 | ✅ |
| S-11 | A2b | `load_exceptions()`（未知/容差字段拒收 · 5+3 字段 · `issue_url` · `justification` · 过期失效 · 合成例外必存在） | 负向 N1/N2/N3 + `--self-test`（过期 / 作用域 / 豁免） | ✅ |
| S-12 | A2b | `load_exceptions()` 规模软限（>10 ⇒ WARNING，不阻断） | 过期 → FAIL（self-test） | ⚠️ 规模护栏 ✅；**commit message 引 `ADR-089` 的机检未实现** ⇒ A4 |
| S-13 | A2b（种子）/ A3（扩充） | `tests/cpu/parity/corpus.txt`（51 case + `layer`）+ `check_corpus_against_harnesses()`（方向性判定） | 负向 N4（清单缺覆盖 ⇒ ERROR）/ N5（格式坏 ⇒ ERROR）；`rv32ui-p-simple` ⇒ WARNING | ✅（48 的收口归 A3） |
| S-14 | A2b | `build/parity/report.json`（S-14 全字段 + 逐 case `determinism`） | 结构校验（顶层 / per_case / 观测量键齐备） | ✅ |
| S-15 | A2b | `--layer pr\|night\|all` + 清单 `layer` 列 | PR 层 8 case 实测 **≈10 min**；★ 缓存条款已按实测更正 | ✅ |
| S-16 | 全部 | 三个 change 的 `tasks.md` 均引 `[S-x]` + 本矩阵 | — | ✅ |

## 8. 开放项（★ 需 architect 确认 / 需 A2a 实测）

| id | 事项 | 现状 | 谁定 |
|---|---|---|---|
| **C-1** | 确定性不符归 `FAIL` 还是 `ERROR` | ✅ **已裁决（2026-10-26，Oracle 建议 + 用户采纳）：归 `ERROR(2)` + `error_kind = determinism_mismatch`** ⇒ `ADR-089` 升 v1.3 同步 D2/V-1/V-2 三处措辞；A3 的「既存分歧清单」只收 exit 1 | ✅ 已闭合 |
| **C-2** | 两侧初始状态如何统一 | ★★ **已裁决并实测修正（2026-10-26）**：原方案「两侧都 `reset_mode=none`」**被实测推翻** —— 生成的 Verilog 只在复位分支加载 init（`else pc <= 32'h80000000;`），跳过脉冲则 `pc` 从 0 起（`x30=0x1000`）。正确口径 = **`init_semantics = ctor-init`**：Simulator 不 reset（保 ctor init）/ Verilator 打 1 拍 reset（加载同一批 init）⇒ 3/3 ELF 的 `pchash`/`regs`/31 个寄存器逐位相等 | ✅ 已闭合 |
| **O-1** | PC 采样实测 offset | ✅ **已实测 = 0**：`add`/`mul`/`simple` 三个 ELF 的 `pchash` 与 `pc_head`（首位 `0x80000004`）**逐位相等** | ✅ 已闭合 |
| **O-2** | Verilator 侧 `reg_1..31` 的实际信号名 | ✅ **已实测 = `top__DOT___reg_N`**（生成 Verilog 声明 `logic [31:0] _reg_30;` / Verilator 头 `IData/*31:0*/`）⇒ 31 个寄存器逐位读取成功 | ✅ 已闭合 |
| **O-3** | `memsig` 探针的触发条件（哪个 stage / 哪个 payload 组）与 hash 常数 | 语义已定（S-6），机制细节待实现 | A2a |
| **O-4** | Verilog 构建缓存形态 | ✅ **已闭合（2026-10-26）**：实测网表**随 ELF 变化** ⇒ 跨 ELF 缓存不成立 ⇒ 每 corpus 一次编译 | ✅ 已闭合 |
| **O-5** | 让 Verilator 侧免掉「每 ELF 一次编译」：把 ELF 从 elaboration 期移出（imem 初始化改运行期加载） | 需改 DUT/上游；★ 收益 = 夜间层省 ≈17 min | architect 评估（不在阶段 A 范围） |

---

## 9. 相关

- `ADR-089` v1.2（契约本体）· `ADR-088` v1.4 §10（目标态 / 阶段 C 5 硬前置）
- `.rddf/improvements/backend-parity-gate-spec.md`（本 change 的立项草案，AC-1~AC-8）
- `.rddf/roadmap/objectives/objective-backend-parity-and-single-model.md`（阶段 A 里程碑 / G-A0~G-A4）
- `.rddf/roadmap/features/feat-backend-parity-gate.md`（阶段 A feature 载体）

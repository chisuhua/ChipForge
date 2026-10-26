# Parity 门禁实现契约（A1 · S-1…S-16）

> **Status**: ✅ **v1.1（2026-10-26，builder；v1.1 = C-1/C-2 裁决落定 + A2a increment 1 实测吞吐入契约）**
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
| V2-9 | 信号名 → Verilog 名带前缀下划线（`pc` → `top__DOT___pc`）⇒ `reg_1` 很可能为 `top__DOT___reg_1`（★ **未实测**，见 O-2） | `sim_main.cpp:80` |

### 2.3 ★ 相位差（本节最重要的事实结论）

| 侧 | 一拍的结构 | 采样点 | reset |
|---|---|---|---|
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

### S-3 采样相位与对齐（★ 禁自适应）

**要求**
- Simulator 采样点 = `sim->tick()` 返回后；Verilator 采样点 = `clk=0` 的 `eval()` 之后（V2-3）。
- **offset MUST 为 0**（不做位移对齐）。★ **MUST NOT** 在比对器里实现「找最佳 offset」「最长公共子序列对齐」之类自适应逻辑 —— 那会把真实的 **1 拍延迟分歧**（`c27543d` 那一类）静默吞掉。
- `reset_mode` MUST 两侧一致，默认 **`none`**（Simulator 既有行为为准）；Verilator 侧经**新增开关**实现，默认行为不变。
- 若实测发现恒定 offset `k ≠ 0` ⇒ MUST 记录到本节 + 走 architect，**MUST NOT** 就地补偿。

**验收方式**：变异对照（把 Simulator 侧某机制延迟 +1 拍 ⇒ MUST 报差异，而不是被判成 offset）；人工（相位结论签字）。
**归属**：A2a（实测 + 重基线）。

---

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
- 落法（首选，= `ADR-089` D2 ③ 的"DUT 自算签名"）：在 CH_MEM 侧新增**只读探针**（观测内存写通道的 `addr` / `data` / 写使能），每次**对内存的写操作恰好计入一次**，滚动更新一个 32/64-bit 寄存器，导出为 named reg `mem_sig`；同拍再导出一个计数器 `mem_sig_updates`。
- ★ 两侧读的是**同一 DAG 的两个 named reg** ⇒ 天然可比（不需要任何 host 侧内存镜像）。
- **自校验（fail-closed）**：若 `mem_sig_updates == 0` 而 DUT 实际发生过写（由 `tohost` 写入或 dmem 侧计数交叉验证）⇒ `ERROR`（防止"签名恒 0 全绿"的假绿）。
- `memsig` 的滚动 hash 函数 MUST 在契约/实现里固定（建议 `h = h*1099511628211 ^ (addr*31 + data)`，A2a 定稿后回写本节）。
- 备选（增强项，不在本期）：guest 侧校验和（需改 ELF harness ⇒ `ADR-089` D2 已登记为"待上游/增强"）。

**验收方式**：变异对照（改 1 字节 store 数据 ⇒ `memsig` MUST 变；删除写 ⇒ `mem_sig_updates` MUST 变）；机检（`mem_sig_updates` 与 dmem 侧写次数对账）。
**归属**：A2a。

---

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
- 真实例外条目数 **> 10**（`N = 10`）⇒ 报告顶部 **强制提示 architect 复审**；★ **MUST NOT** 以「超过 N ⇒ 退出码非 0」作护栏（会逼出假登记）。
- `sunset` 已过期 ⇒ 该条目**自动失效** ⇒ 门禁 FAIL，并标注「升级为真 bug 类」。

**验收方式**：机检（commit message 格式、条目计数、过期判定）；人工（architect 签）。
**归属**：A2b + 流程。

---

### S-13 语料清单的单一来源

**要求**
- 清单文件（A3 落盘）：`tests/cpu/parity/corpus.txt`，每行 `name<TAB>kind<TAB>relpath`，`#` 起注释；`kind ∈ {elf, branchbench}`。
- ★ 两侧 harness MUST **都从这一个文件**读取；MUST NOT 各自维护 C++ 数组。
- 门禁 MUST 断言「Simulator 侧实际执行集合 == Verilator 侧实际执行集合 == 清单集合」，任一不等 ⇒ `ERROR`。
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
- **夜间层**：49 corpus（47 ELF + 4 档分支微基准 = 51 − ?，以 S-13 清单为准全部覆盖）。
- ★ Verilog 构建产物 MUST **按 config 缓存**（key = 插件集 + 参数 hash，目录 `build/parity/vl_cache/<key>/`）；同一 config 的 47 ELF MUST 只编译一次（V2-6 现状每 ELF 都重编译，直接采纳会爆预算）。
- ★★ **branchbench 在 Simulator 侧的 cap MUST ≤ 20,000 拍**（实测 ≈ 9.1 拍/s ⇒ 20k ≈ 37 min/档）；`--cycles 2,000,000`（Verilator 侧现状默认）在 Simulator 侧 ≈ 61 小时/档，**不可行** ⇒ 若某档无法在 cap 内到达 done，MUST 登记为「该 corpus 仅 Verilator 侧可跑」的显式豁免（走 S-11），**MUST NOT** 静默跳过。
- 时长阈值：★ **本期不设未实测的硬阈值**；A4 MUST 先把实测 p95 写进报告 `timings`，再据此定阈值。

**验收方式**：机检（缓存命中率 = N-1/N；分层集合 ⊆ S-13 清单）；人工（A4 定阈值）。
**归属**：A4。

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
      "observations": { "tohost": 1, "regs": "…", "memsig": "…", "cycles": 455, "pchash": "…" },
      "first_divergence": { "metric": "cycles", "cycle_index": 0, "sim_value": 455, "vl_value": 456 },
      "verdict": "FAIL"
    }
  ],
  "exceptions_applied": ["EX-0001"],
  "timings": { "sim_ms": 0, "vl_build_ms": 0, "vl_run_ms": 0 }
}
```

---

## 7. 「S ⇒ 实现/测试」覆盖矩阵（模板，G-A2 必交）

| S | 归属 change | 实现位置（A2a/A2b/A3/A4 填） | 测试 / 变异对照 | 状态 |
|---|---|---|---|---|
| S-1 | A2a/A2b |  |  | ☐ |
| S-2 | A2a/A2b |  |  | ☐ |
| S-3 | A2a |  |  | ☐ |
| S-4 | A2a |  |  | ☐ |
| S-5 | A2a |  |  | ☐ |
| S-6 | A2a |  |  | ☐ |
| S-7 | A2a/A2b |  |  | ☐ |
| S-8 | A2a/A2b |  |  | ☐ |
| S-9 | A2b |  |  | ☐ |
| S-10 | A2b |  |  | ☐ |
| S-11 | A2b |  |  | ☐ |
| S-12 | A2b + 流程 |  |  | ☐ |
| S-13 | A3/A2b |  |  | ☐ |
| S-14 | A2b |  |  | ☐ |
| S-15 | A4 |  |  | ☐ |
| S-16 | 全部 |  |  | ☐ |

---

## 8. 开放项（★ 需 architect 确认 / 需 A2a 实测）

| id | 事项 | 现状 | 谁定 |
|---|---|---|---|
| **C-1** | 确定性不符归 `FAIL` 还是 `ERROR` | ✅ **已裁决（2026-10-26，Oracle 建议 + 用户采纳）：归 `ERROR(2)` + `error_kind = determinism_mismatch`** ⇒ `ADR-089` 升 v1.3 同步 D2/V-1/V-2 三处措辞；A3 的「既存分歧清单」只收 exit 1 | ✅ 已闭合 |
| **C-2** | `reset_mode = none` 需要 Verilator 侧新增开关（默认行为不变） | ✅ **已裁决（2026-10-26 用户）：`reset_mode = none`**；★ 且 increment 1 实测发现 `rv32ui-p-add` 在「Simulator 无 reset」与「Verilator 打 1 拍 pulse」下 **cycles 同为 478** ⇒ reset pulse 当前未扰动计数，但**仍按 S-3 统一为 none**（不依赖"恰好没炸"） | ✅ 已闭合（Verilator 开关待 A2a inc-2 落） |
| **O-1** | PC 采样实测 offset | 🟡 **部分证据（inc-1）**：同 ELF 两侧 `cycles` 同为 **478**（Simulator 478 / Verilator 478）⇒ 至少总拍数无偏移；`pchash` 比对待 Verilator 侧导出（inc-2） | A2a inc-2 |
| **O-2** | Verilator 侧 `reg_1..31` 的实际信号名 | 推测 `top__DOT___reg_1` | A2a |
| **O-3** | `memsig` 探针的触发条件（哪个 stage / 哪个 payload 组）与 hash 常数 | 语义已定（S-6），机制细节待实现 | A2a |
| **O-4** | Verilog 构建缓存形态（复用 `--work-dir` vs hash 目录） | 未定 | A4 |

---

## 9. 相关

- `ADR-089` v1.2（契约本体）· `ADR-088` v1.4 §10（目标态 / 阶段 C 5 硬前置）
- `.rddf/improvements/backend-parity-gate-spec.md`（本 change 的立项草案，AC-1~AC-8）
- `.rddf/roadmap/objectives/objective-backend-parity-and-single-model.md`（阶段 A 里程碑 / G-A0~G-A4）
- `.rddf/roadmap/features/feat-backend-parity-gate.md`（阶段 A feature 载体）

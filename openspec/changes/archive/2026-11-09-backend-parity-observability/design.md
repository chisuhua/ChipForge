# backend-parity-observability — 设计

> 契约正文（`docs/architecture/parity-gate-contract.md`）是唯一引用来源；本文件记录**实现选择**与**实测证据**。

---

## 1. inc-1：Simulator 侧通用 runner（已落）

### 1.1 为什么新建 target 而不是复用 `cpu_sim`

`tools/cpu_sim` 是 **TLM** CLI（`CpuFactory<T>::build_cpu` + `pb.run()`）。CH_MEM 路径要 `CpuFactoryChmem` + `pb.elaborate(ctx)` + `create_simulator()` + `Simulator::tick()`，**编译开关不同**（`-DCF_PLUGIN_USE_CH_MEM`）⇒ 只能独立 target。

### 1.2 CLI 面镜像 `cpu_verilator_sim`

| 参数 | 语义 |
|---|---|
| `--elf` | 必填 |
| `--cycles` | 默认 2,000,000（与 Verilator 侧一致） |
| `--reset none` | ★ 唯一支持值（S-3）；其它值 **fail-fast**，不静默忽略 |
| `--enable-mmu` / `--mmu-mode` / `--enable-cache` | 与 Verilator 侧同义转发 |
| `--no-bp` / `--btb-entries` / `--no-bp-uncond` / `--no-bp-gshare` | 与 Verilator 侧同义（`--btb-entries` 默认 **256**，与 Verilator 侧一致） |
| `--json <path>` | 报告片段落盘 |
| `--allow-incomplete` | ★ 脚手架开关：`memsig` 未实现时放行（**默认关闭 ⇒ 缺失即 exit 2**） |

### 1.3 采集实现（按 S-2 §2.1 的纪律）

```
resolve_node_id(ctx, name)   // 一次性扫 ctx.get_nodes()
read_slot(sim->data_map(), id)  // 每拍直读，无 O(nodes) 扫描
```

- 禁止 `get_value_by_name`（每拍 O(nodes)）
- 禁止 `get_signal_id_by_name`（只认 trace 注册表，trace 默认关）

### 1.4 stdout 卫生（一个不得不做的 workaround）

CppHDL 每创建节点都打 `CHINFO`，而 `include/utils/logger.h` **没有运行期级别阈值** ⇒ 会把 stdout 冲成日志墙。做法：在 build/run 期间把 **fd 1 重定向到 fd 2**，输出 JSON 前恢复 ⇒ 机器可读输出在 stdout、库日志在 stderr。

> ⚠️ 已知代价：这是**进程内重定向**，若将来有并发写 stdout 的线程会串。当前 runner 单线程，可接受。

---

## 2. 实测证据（inc-1 + inc-2 + inc-3）

### 2.0 ★ 5/5 corpus 全观测量逐位相等（最终状态）

| corpus | determinism | cycles | tohost | regs | pchash | memsig | memsig_updates |
|---|---|---|---|---|---|---|---|
| `rv32ui-p-add` | ✅ 2511 / `67205f7d…` | 478 / 478 | 1 / 1 | ✅ | ✅ | `7c01` / `7c01` | 1 / 1 |
| `rv32um-p-mul` | ✅ | 472 / 472 | 1 / 1 | ✅ | ✅ | `7c01` / `7c01` | 1 / 1 |
| `rv32ui-p-simple` | ✅ | 38 / 38 | 1 / 1 | ✅ | ✅ | `7c01` / `7c01` | 1 / 1 |
| `branchbench_m0` | ✅ | 3000 / 3000 | 0 / 0 | ✅ | ✅ | `a55f52c5` / `a55f52c5` | 9 / 9 |
| `branchbench_m2` | ✅ | 3000 / 3000 | 0 / 0 | ✅ | ✅ | `a55f52c5` / `a55f52c5` | 9 / 9 |

- ★ `memsig` 在 3 个 ELF 上同为 `7c01`：**这三个测试各只写一次**（写 `tohost=1`）⇒
  签名只折叠了那一次写（word `0x400`、data `1`）⇒ 这是**预期**的，不是 bug；
  branchbench（**9 次写**）才体现多写序列 ⇒ `a55f52c5`。
- ★ 退出码两侧统一：ELF PASS ⇒ `0/0`；branchbench 在 3000 拍内未写 tohost ⇒ **两侧都 2**（TIMEOUT = ERROR，契约 S-10）。

### ★ 2.1 吞吐结论（推翻 ADR 的 575 拍/s）

- `--no-bp` 200 拍 = **21 s**；默认 BP 200 拍 = **24 s** ⇒ **BP 不是瓶颈**，解释器本身是。
- ⇒ `branchbench --cycles 2000000` 在 Simulator 侧 ≈ **61 小时/档** ⇒ 不可行。
- ⇒ 契约 S-15 已改：Simulator 侧 branchbench cap **≤ 20k 拍**，否则登记「仅 Verilator 侧可跑」豁免。

### 2.2 ★★ 初始状态语义（本轮最重要的发现，推翻了原方案）

本轮先按契约草案试了 `--reset-pulse 0`（两边都不 reset），结果是：
- `cycles` **仍然相等**（478）—— 因为 dmem 窗口对 base 不敏感
- 但 `pc_head = 0x4, 0x8, …`（丢了 `0x80000000` 基址）、`x30 = 0x1000`（应为 `0x80001000`）⇒ **`regs`/`pchash` 全不等**

查生成 Verilog 得到根因：
```verilog
always_ff @(posedge default_clock or posedge default_reset) begin
    if (!default_reset) pc <= mux_select_206;
    else                pc <= 32'h80000000;   // ★ init 值只在复位分支赋
end
```
⇒ 生成的 Verilog **没有 `initial`**，跳过复位脉冲 ⇒ `pc` 从 0 起。
而 Simulator 侧 `Simulator::reset()` 的实现是「**把非 literal 槽位全部清零**」（`simulator.cpp:422-441`），**不加载 init**
⇒ 既有的 47-ELF harness 因此**故意不调 reset**（`test_cpu_chmem_vendored_elf.cpp:157-161` 的注释说法"加载 rst_val"其实不准确）。

**定案（已回写契约 S-3 / §2.3 / C-2）**：`init_semantics = ctor-init`
- Simulator：不 reset（保留 ctor init）
- Verilator：打 **1 拍** reset（复位分支加载**同一批** ctor init 值）
- 两条路径字面不同、**语义等价** ⇒ 用 `pchash` + `regs` 逐位相等作等价证据（3/3 ELF 已验证）
- `--reset-pulse 0` 降级为**诊断**，并加 stderr 警告

★ 方法论教训：`cycles` 相等**不足以**证明等价（478 vs 478 掩盖了完全不同的初始状态）—— 这正好实证了 ADR-089 D2 的「④ 弱于 ⑤」判据排序。

## 3. inc-2：Verilator 侧观测量导出（✅ 已落）

- 改 `tools/verilator_runner/sim_main.cpp`：新增 `cf_read_reg()`（`reg_1..31`，实测信号名待定）、逐拍 PC 滚动 hash、以及 `--json` 输出
- `--reset-pulse=0`：新增开关，**默认行为不变**（保护 48/48 基线）
- ★ 信号名 MUST 实测：`pc` 在 Verilog 里是 `top__DOT___pc`（前缀下划线）⇒ `reg_1` 很可能是 `top__DOT___reg_1`；名称缺失 MUST **编译期/运行期硬失败**，不得静默读 0

## 4. inc-3：`memsig` DUT 探针（✅ 已落）

### 4.1 落点

`ip/cpu/plugins/dmem_chmem.h` 的写路径旁（`mem.write(...)` 之后）：复用该处**已有**的
`word_addr` / `store_wdata` / `is_store_active` 三个表达式，**不新增 payload**、**不改 datapath**。

```
mix  = word_addr * 31 + store_wdata + sig_updates * 0x9E3779B1
sig' = sig * 16777619 ^ mix        （仅在 is_store_active 时生效）
```
⇒ 导出 `mem_sig` / `mem_sig_updates` 两个 named reg ⇒ 两侧读**同一 DAG 的两个 reg**。

### 4.2 ★ 两个必须记住的坑（都踩过）

| 坑 | 现象 | 根因 | 正确写法 |
|---|---|---|---|
| **reg 被 codegen 裁掉** | 网表里**根本没有** `mem_sig`（`grep` 0 命中），但 Simulator 侧能按名解析 | `ch_reg` **继承自 `ch_uint`** ⇒ 直接 `reg * X` 走 **ch_uint 的编译期值**路径（恒 0）⇒ 表达式退化成常量 ⇒ reg 无驱动逻辑 ⇒ 不在 eval list ⇒ codegen 不发射 | 显式取值：`ch_uint<32> v(reg);`（同 `ibus_chmem.h` 的 `flush_count`） |
| **改了头文件但 runner 没重建** | 重建 `parity_sim_runner` 后 `cpu_verilator_sim` 仍用旧头文件 ⇒ 误判「探针没生效」 | Verilator 侧 Verilog 由 `cpu_verilator_sim`（CMake target）生成，**不是**运行期编译 | 改 `ip/**` 头文件后必须**同时**重建 `cpu_verilator_sim` |

⇒ ★ 为此给 runner 加了诊断项 `--dump-nodes <substr>`：打印 DAG 节点 + **是否在 eval list**
（因为 codegen 只发射 eval list 里的节点 ⇒ 「DAG 里有」≠「网表里有」）。

### 4.3 变异对照（契约 S-6 的验收形式）

| 变异 | 期望 | 实测 |
|---|---|---|
| `mix` 里的 `store_wdata` 取反 | `memsig` MUST 变（证明对写数据敏感） | `a55f52c5` → `8fcc53ba` ✅ |
| `sig_upd` 强制恒 0 | 两侧 MUST 判「假绿」ERROR（rc=2 + 消息） | 两侧 rc=2，消息一致 ✅ |

### 4.4 零 cycle 影响

加探针前后 `rv32ui-p-add` 478 / `rv32um-p-mul` 472 / `rv32ui-p-simple` 38 —— **全部不变**
⇒ 探针只观测、不进 datapath。DAG `node_count` 2485 → **2511**（+26，两侧同步变化 ⇒ 确定性判据仍相等）。

### 4.5 附带修正

- Verilator parity 路径的 **TIMEOUT 退出码** 1 → **2**（此前与 Simulator 侧不一致；契约 S-10 规定 TIMEOUT = ERROR）
- 两侧都补上 ③ 的**假绿自校验**（`tohost != 0` 但 `mem_sig_updates == 0` ⇒ ERROR）

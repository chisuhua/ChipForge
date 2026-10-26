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

## 2. 实测证据（inc-1）

| 项 | Simulator | Verilator |
|---|---|---|
| `rv32ui-p-add` `cycles` | **478** | **478** ✅ |
| `tohost` | 1 | 1 ✅ |
| DAG `node_count` | 2485 | —（待 inc-2） |
| `names_hash` | `d2b048bdbed141ff` | —（待 inc-2） |
| `regs` hash | `72c91fd2b209cdff` | —（待 inc-2） |
| `pchash` | `830b6af580cdf64e`（478 samples） | —（待 inc-2） |
| 时长 | **52.8 s / 478 拍 ⇒ ≈ 9.1 拍/s** | 秒级（含 Verilog 生成+编译） |

### ★ 2.1 吞吐结论（推翻 ADR 的 575 拍/s）

- `--no-bp` 200 拍 = **21 s**；默认 BP 200 拍 = **24 s** ⇒ **BP 不是瓶颈**，解释器本身是。
- ⇒ `branchbench --cycles 2000000` 在 Simulator 侧 ≈ **61 小时/档** ⇒ 不可行。
- ⇒ 契约 S-15 已改：Simulator 侧 branchbench cap **≤ 20k 拍**，否则登记「仅 Verilator 侧可跑」豁免。

### 2.2 reset 相位的意外发现

Verilator 侧默认打 1 拍 `reset` pulse（`sim_main.cpp:117-127`），Simulator 侧不打 —— 但 `rv32ui-p-add` 两侧 **cycles 同为 478**。说明当前 reset pulse **不扰动计数**.

★ 但**仍按 S-3 统一为 `reset_mode=none`**（inc-2 给 Verilator 加开关）：不把"恰好没炸"当成契约。

---

## 3. inc-2：Verilator 侧观测量导出

- 改 `tools/verilator_runner/sim_main.cpp`：新增 `cf_read_reg()`（`reg_1..31`，实测信号名待定）、逐拍 PC 滚动 hash、以及 `--json` 输出
- `--reset-pulse=0`：新增开关，**默认行为不变**（保护 48/48 基线）
- ★ 信号名 MUST 实测：`pc` 在 Verilog 里是 `top__DOT___pc`（前缀下划线）⇒ `reg_1` 很可能是 `top__DOT___reg_1`；名称缺失 MUST **编译期/运行期硬失败**，不得静默读 0

## 4. inc-3：`memsig` DUT 探针

- 落点：`ip/cpu/plugins/dmem_chmem.h`（`word_addr` / `store_wdata` / `is_store_active` 三个表达式**已在该处存在**，无需新增 payload）
- 产物：两个 named reg ⇒ `mem_sig`（滚动 hash）+ `mem_sig_updates`（计数，用于自校验：写发生过但计数为 0 ⇒ ERROR）
- ★ 选择理由：① 两侧读**同一 DAG 的两个 reg** ⇒ 天然可比 ② 不需要 host 内存通道（Simulator 侧没有）③ 不需要重 vendor 47 个 ELF（guest 校验和方案的成本）
- ⚠️ 风险：`mem_sig` 是 **32/64-bit 状态**，会让 Verilog 多一条 hash 组合链 ⇒ 需测对 47 ELF 的 cycle 数**零影响**（若有影响，说明它进了关键路径，须改为纯观测寄存器）

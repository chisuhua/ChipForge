# parity-gate-spec — 设计（契约推导与事实基线）

> **本文件是 `docs/architecture/parity-gate-contract.md` 的推导记录**：说明 S-1…S-16 为什么这么定、哪些是实测事实、哪些是待实测假设。
> ★ 契约正文是**唯一可引用来源**；本文件不作为引用目标（避免两处漂移）。

---

## 1. 设计目标

| 目标 | 判据 |
|---|---|
| 让 A2a/A2b 无从「自选口径」 | 每条约束都有 S 编号 + 验收方式（机检/变异对照/人工） |
| 让假绿无处藏身 | 5 观测量各自 fail-closed；三态退出码；变异对照 ≥2 例；常驻合成例外 |
| 让契约不腐化 | S 编号 + 下游 tasks 必引 + G-A2 覆盖矩阵（S-16） |
| 让未知变显式 | §8 开放项（C-1/C-2 待 architect，O-1~O-4 待实测） |

---

## 2. 代码事实基线（决定契约写法的硬约束）

### 2.1 三条"反直觉事实"（直接改写了 ADR 的实现口径）

| # | 事实 | 证据 | 对契约的影响 |
|---|---|---|---|
| F-1 | `Simulator::get_memory()` 是空 stub，且 `ENABLE_MEMORY_API` 未启用 | `CppHDL/src/simulator.cpp:161-168`、`include/simulator.h:581-582` | ③ **必须** DUT 自算签名（S-6）；host dump 只能作为「待上游」增强项 |
| F-2 | `get_signal_id_by_name()` 只查 trace 注册表 `signals_`，trace 默认关 | `include/simulator.h:557-568`、`:9` | ② / ③ / ⑤ 的 Simulator 侧解析 MUST 用 **一次性 `ctx()->get_nodes()` 扫名字**（S-5/S-6/S-8），**不是** ADR v1.1 写的那个 API |
| F-3 | Verilator `--stats` 的 `PC_HIST` 是**直方图**，丢顺序 | `sim_main.cpp:190-222` | ⑤ **必须**逐拍有序滚动 hash（S-8），直方图最多做诊断附件 |

### 2.2 相位与 reset（★ 本次新发现的真实分歧源）

| 侧 | 一拍结构 | 采样点 | reset |
|---|---|---|---|
| Simulator | `eval_combinational()` → `eval()`×2 → `eval_combinational()`（`simulator.cpp:267-345`） | `tick()` 返回后 | **不打**（既有 harness 有意跳过，`test_cpu_chmem_vendored_elf.cpp:157-161`：reset 会用 `rst_val` 覆盖 PC 初值 `0x80000000`） |
| Verilator | `clk=1;eval(); clk=0;eval();`（`sim_main.cpp:136-141`） | 第二次 `eval()` 后 | **打 1 拍 reset pulse**（`sim_main.cpp:117-127`） |

**推导**：
- 「一拍」两侧**同构**（各含 posedge + negedge 求值）⇒ 采样相位**原理 offset = 0**（待 O-1 实测确认）。
- ★ 但 **reset 处理不同 ⇒ 初始状态不同**。这是**真实分歧源**，若不统一，47 个 ELF 会从第一拍起就整体错位，而错位会被误判成「后端 1 拍延迟 bug」。
- **统一方向选 `none`**：Simulator 侧要改 reset 语义得改上游 `rst_val` 机制（代价大且会破坏 48/48 基线）；Verilator 侧只需**新增可选开关**（默认行为不变）⇒ 代价非对称，选 `none`（S-3）。

### 2.3 corpus 事实

| 侧 | 清单载体 | 条数 |
|---|---|---|
| Simulator | `tests/cpu/test_cpu_chmem_vendored_elf.cpp` 的 `kCases`（C++ 数组） | **47**（39 rv32ui + 8 rv32um，**无** `rv32ui-p-simple`） |
| Verilator | `tests/cpu/test_cpu_verilator_sim.cpp` 的 `kCases`（C++ 数组） | **48**（多 `rv32ui-p-simple`） |

⇒ 两份手维护数组 ⇒ S-13 要求抽成**单一清单文件** + 三方集合断言；`rv32ui-p-simple` 补进 Simulator 侧（该 ELF 已在 `tests/cpu/riscv_tests/elf/`）。

---

## 3. 关键落法选择（含被否方案）

| 决策 | 采纳 | 被否 | 理由 |
|---|---|---|---|
| ③ 内存签名的载体 | **DUT 内只读探针 + named reg**（`mem_sig` / `mem_sig_updates`） | ① host 全内存 dump ② guest 侧校验和 ELF | ① Simulator 无通道（F-1）；② 需重 vendor 47 个 ELF，成本高且会改坏既有基线 |
| ⑤ PC 观测量 | **逐拍有序滚动 hash + 首分歧拍定位** | `PC_HIST` 直方图 | 直方图丢顺序（F-3）⇒ 抓不到「一侧省 2 拍、别处多 2 拍」 |
| 相位对齐 | **offset 固定 0；实测异常则回改契约** | 比对器内自适应找最佳 offset | 自适应会把 `c27543d` 那类真实 1 拍延迟分歧吞掉 |
| 观测量缺失 | **ERROR（退出码 2）** | 跳过该项 / 记 0 | 跳过 = 假绿；记 0 会把「没采到」伪装成「两边都是 0」 |
| 白名单 schema | **未知字段拒收** | 宽容解析 | 防偷偷加 `tolerance`（ADR-089 明令禁容差） |
| 例外规模护栏 | **>10 条 ⇒ 提示复审**（不阻断） | 超过 N 条 ⇒ 门禁红 | 门禁红会逼出「假登记」 |

---

## 4. 契约条目 ↔ ADR-089 条款映射

| S | ADR-089 条款 |
|---|---|
| S-1 | D2 观测量集 |
| S-2 | D2 **前置条件**（同输入 + 确定性判据） |
| S-3 | D2 ⑤ 采样相位 + D3 逐拍相等 |
| S-4 / S-5 / S-6 / S-7 / S-8 | D2 ①②③④⑤ 逐条落地 |
| S-9 | D2 判据强度排序 + D3 例外登记制 |
| S-10 | V-1 三态语义 |
| S-11 / S-12 | D3 例外 5 字段 + 扩张权 + 规模护栏 + 常驻合成例外 |
| S-13 | D4 corpus（两侧交集 47 + 单一清单） |
| S-14 | D2 观测量齐备 + V-1 可诊断性 |
| S-15 | Consequences §4（Verilog 构建缓存）+ §6（分层 / 快车道） |
| S-16 | D6 + G-A2（契约粘住） |

---

## 5. 开放项与处置

| id | 处置 |
|---|---|
| **C-1** 确定性不符归 FAIL/ERROR | 契约按三态语义归 `ERROR`；**交 architect 确认**（行为一致，仅分类不同） |
| **C-2** `reset_mode=none` 需 Verilator 新增开关 | 契约已定为必须；**交 architect 确认**后由 A2a 落 |
| **O-1** PC 相位实测 offset | A2a 用 `rv32ui-p-add` 实测并回写 §2.3 |
| **O-2** Verilog 侧 `reg_N` 实际信号名 | A2a 生成 Verilog 后 grep 实测并回写 §2.2 |
| **O-3** `memsig` 探针触发条件 / hash 常数 | A2a 定稿后回写 S-6 |
| **O-4** Verilog 构建缓存形态 | A4 定稿 |

★ 纪律：开放项 MUST 显式列出并带 owner；**MUST NOT** 用「实现时再看」隐性兜底。

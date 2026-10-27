# ADR-089 — 双后端差分 parity 门禁（同一模型 × 两后端）

> **Status**: ✅ **Accepted（v1.4，2026-10-27，architect 主持；v1.2 = A1 代码事实更正 · v1.3 = C-1 归类裁决 + 吞吐实测更正 · ★ **v1.4 = A3 首跑结果：既存后端分歧清单 = 空**）**
> **上游**: ADR-083（Plugin-style 单一 source of truth）· ADR-040 v2.0（CH_MEM 是新正道）· ADR-088（TLM 后端退役；本 ADR 是其 **B3 删除的前置契约**）
> **下游**: `.rddf/roadmap/features/feat-backend-parity-gate.md`（阶段 A）· `.rddf/roadmap/features/feat-tlm-backend-retirement.md`（阶段 B/C）· `objective-ip-cpu-tlm-backend-retirement`（B3 门禁）
> **执行文件（待 planner 落）**: `.rddf/plans/`（阶段 A 的 change 计划，★ 归 builder 起草）

---

## Context

### 1. 现状：项目有两个仿真后端，但**从不互比**

| 产物 | 跑哪个后端 | 比较对象 |
|---|---|---|
| vendored ELF 47（`tests/cpu/test_cpu_chmem_vendored_elf.cpp`） | **仅 CppHDL Simulator**（`create_simulator()`） | 只判 `tohost == 1` |
| Verilator 48 ELF（`tools/verilator_runner/cpu_verilator_sim.cpp`） | **仅 Verilator**（`to_verilog` → Verilator 编译 → 联合仿真） | 只判 `tohost == 1` |
| 覆盖平价台账（463 条，`docs/architecture/tlm-coverage-parity-ledger.md`） | **不跑任何仿真** | 数「有没有对应 case 名」 |
| `[mem-drift]` 锚（`tests/cpu/test_cpu_chmem_mem_read_drift.cpp`） | Simulator | ★ **唯一的跨后端断言** —— 但只钉了 `ch_mem::aread` 一个机制的 0-cycle 对齐 |

⇒ ★ **两个后端跑的是不同 corpus、不同 oracle，从不互比** ⇒ 一个「两边都 `tohost == 1` 但内部状态不同」的模型**今天完全不可见**。

### 2. 现状：台账是**计数代理**，且已在漂

- 台账比对的是**两个不同的模型**：TLM-mode elaboration（`uint_t<N>` = POD、`pb.run()` 每周期执行、`array_store::commit()` 是 no-op）与 CH_MEM-mode elaboration（`uint_t<N>` = `ch_uint<N>`、`pb.elaborate(ctx)` 发射 DAG）⇒ 二者**插件集 / 语义 / 存储提交语义都不同** ⇒ 分歧来自**建模语义**（无界、不可机检）。
- 漂移实测：台账头部声明 partial **292** vs 条目级实测 **282**；`per_thread_ghr_isolation` 一行简报记 `not-covered` / 台账记 `partial` ⇒ ★ 一个需要人工维护 463 行计数的代理指标**已经开始骗自己**。
- 双后端**同 ISA 不同语义**已实证（`feat-tlm-backend-retirement` 的子系统对照表）：M 扩展（TLM radix-2 多周期 vs CH_MEM 组合式 1 拍）· CSR（TLM 纯 stub vs CH_MEM 实装 566 行）· MMU PTW（TLM 完整 walk vs CH_MEM 回退 vaddr）· BP（TLM 319 行 vs CH_MEM 当时为 0）⇒ ★ 这类差异**不是 bug，是两种设计的合法产物** ⇒ 用「一致性」去卡它是**错误的度量**。

### 3. 但差分执行比的是**同一个 DAG** ⇒ 分歧类有界

```
差分执行:  同一 elaboration  ┬→ CppHDL Simulator
                          └→ 生成 Verilog → Verilator
          ↑ 同一插件集 · 同一配置 · 同一 ELF · 同一 DAG
          ⇒ 分歧只可能来自【后端】⇒ 小且有界
```

⇒ ★ **bug 类包含关系**：Simulator ≡ Verilator 的分歧 = **后端 bug**（小类）；TLM ≡ CH_MEM 的分歧 = **建模语义差异**（无界类）。
⇒ ★ **历史证据**：2026-10-21 上游 CppHDL `c27543d` 修的 `ch_mem::aread` 1 拍滞后，导致 10 个 rv32ui load/store ELF 失败 —— 那**不是建模错误**，而是**后端时序不一致**（Simulator 1 拍 vs 生成 Verilog 0 拍）⇒ ★ 这正是差分执行会**第一优先**抓到的一类，而台账对它**完全无感**。
⇒ ★ 概念早已存在（上游归档 change `dual-backend-mem-read-timing-parity-2026-10`），**只是从未做成门禁**。

---

## Decision

### D1 ★ 目标态：**单模型（CH_MEM）+ 双后端（CppHDL Simulator / Verilator）**

| 项 | 目标态 |
|---|---|
| 模型 | **只有 CH_MEM 一种**（`pb.elaborate(ctx)` 发射 lnode DAG）⇒ `cf::plugin::uint_t<N>` 恒等于 `ch::core::ch_uint<N>` ⇒ **编译期模态开关消失** |
| 后端 | **两个**：① CppHDL Simulator（解释执行 DAG）② Verilator（`ch::toVerilog(ctx)` → Verilog → Verilator） |
| TLM 模式 | **随 B3 删除**（`pb.run()` 快环 / `array_store` TLM no-op / `src/cf_plugin/bridge/` / TLM-only 业务文件）—— ★ **时点绑 B3，见 D6** |
| **parity 的定义** | ★ **同一输入的确定性双后端差分执行**（本 ADR 的核心） |

### D2 ★ parity 观测量集与比较强度

| # | 观测量 | 强度 | 说明 |
|---|---|---|---|
| ① | `tohost` | bit-exact | 现状保留（最弱判据） |
| ② | 最终寄存器堆（`x1..x31`） | **bit-exact** | 最强功能判据，成本低；★ Verilator 侧经 `--public-flat-rw` 读 `reg_0..31`（`reg_file_chmem.h:166-169` 显式命名）；★ Simulator 侧 ⇒ **一次性**遍历 `ctx()->get_nodes()` 按 `node->name()` 解 id，再 `static_cast<uint64_t>(data_map().at(id))` 直读（`sdata_type::operator uint64_t()` 是 **explicit**，`types.h:96`；`data_map_t` = `unordered_map<uint32_t, sdata_type>`，`ast/instr_base.h:15`） ⇒ ★★ **MUST NOT** 用 `get_signal_id_by_name`（它只查 **trace 注册表** `signals_`（`simulator.h:557`），而 trace **默认关**（`Simulator(ctx, trace_on=false)`）⇒ 不可作通用解析器）；★ **MUST NOT** 每拍 `get_value_by_name`（`simulator.cpp:455-470` 实测 O(nodes) 线性扫描） |
| ③ | ★ **DUT 自算内存签名（mailbox）** | **bit-exact** | ★ **首选落法 = DUT 侧自算签名写 mailbox**（branchbench 的 tohost 已是 32-bit 校验和形态 ⇒ 同款）⇒ ★ **不是** host 全内存 dump（★ **Simulator 侧 `Simulator::get_memory()` 是空 stub** 且被未启用的 `ENABLE_MEMORY_API` 包住 ⇒ 无 host 读出通道）⇒ host dump 留作**增强项**（待上游实装或 dmem 探针阵列）⇒ 白名单登记「host dump = 待上游」 |
| ④ | **总 cycle 数（标量）** | **bit-exact** | ★ **措辞更正**：标量无「逐拍」可言 ⇒ ★ **「逐拍相等」的语义全部归 ⑤**；④ 只做「总拍数相等」 |
| ⑤ | ★ **逐拍采样 PC 流 hash** | **bit-exact** | ★ **逐拍判据的载体**；★ 轨迹定义（采样点 = fetch / commit？采样率？）**归 A1 细则**，且 MUST 钉死采样相位（Simulator = `tick()` 返回后；Verilator = `clk 1→0` eval 对之后） |

⇒ ★★ **判据强度排序**（★ 防止实现时把强判据降级成弱判据）：**⑤ 逐拍 PC 流 > ④ 总拍数 + ②③ 末态 > 仅末态** ⇒ ★ 「总拍数相等 + 末态相等」**弱于**「逐拍流相等」（总拍数相同可掩盖中间轨迹分歧：一侧浪费 2 拍又在别处省 2 拍）⇒ ★ **MUST NOT** 用「④+末态」替代 ⑤。
⇒ ★ **「逐拍相等」在机制上可达**：Simulator 的 `tick()` = 时序求值 + 组合收敛，与生成 Verilog 的 `always_ff @(posedge)` + 组合 settle 是**同一个时钟同步抽象**（CppHDL 核心契约）⇒ 2026-10-21 上游 `c27543d`（`aread` 1 拍滞后）是该契约的一次**违约并被修复** ⇒ ★ 「逐拍相等」是**上游契约的回归断言**，不是不可达的理想。

⇒ ★ **fail-closed 纪律**：任一观测量**缺失 / 不可采集 / 被截断** ⇒ **判 FAIL**，**MUST NOT** 跳过或降级（沿用项目既有 `dropped` / `FLUSH_COUNT` fail-closed 风格）。
⇒ ★★ **前置条件（v1.1 更正 —— 措辞不可实现 ⇒ 改为「同输入 + 确定性」）**：★ 「同一次 elaboration 的产物」**字面不可实现**（Simulator 在测试进程、Verilog 在 runner 进程 ⇒ 必然是**两次 elaboration**）⇒ 真实约束 = ★ **同一输入**（同插件集 / 同配置 / 同 ELF）的**确定性 elaboration** ⇒ ★ **确定性判据（先于任何观测量比对）**：两侧的 **DAG node_count + 顶层信号名名单 hash** MUST 一致 ⇒ 否则 **门禁判红**（★ 三态语义下归 **ERROR**，`error_kind = determinism_mismatch`；★ **MUST NOT** 计入「后端分歧清单」 —— 2026-10-26 Oracle 裁决：这是**配置/上游漂移**，不是后端语义分歧；若判 FAIL(1) 恰好制造本条要防的那个误报）。★ 依据：上游发射顺序漂移会导致按名采集静默错位 ⇒ 配置漂移会被误报成「后端分歧」。

### D3 ★ 对齐规则（Q1 裁决）：**逐拍相等为默认 + 例外白名单**

- ★ **默认**：④ 的 cycle 数 MUST **逐拍相等** ⇒ 任何偏差都是**待解释的分歧**，不是「可接受的噪声」。
- ★ **例外机制**：组合 DAG 与生成 Verilog 的调度语义（`always_ff` vs DAG 求值顺序）**本质上可能不同** ⇒ 允许登记**显式例外**，但每条例外 MUST 同时具备 **5 项**：
  1. **机制说明**（哪个机制的哪条语义不同、为什么不同）
  2. **上游状态**（是上游 CppHDL 未对齐，还是本项目有意为之 + 上游 issue 链接）
  3. **变异对照**（一个能证伪「例外被误当回归」的锚）
  4. ★ **收敛期限**（≤ 1 个发布窗口；到期未收敛 ⇒ ★ **自动升级为「真 bug」类** ⇒ 阻塞阶段 C）
  5. ★ **corpus 作用域**（该例外作用于哪些 ELF / 哪几档 ⇒ ★ 否则一条 `aread` 例外会静默盖住全部 corpus 的同类偏差）
  ⇒ ★ **例外清单 MUST 是受控清单**（单一文件 + 门禁引用），**MUST NOT** 以「容差」吸收（容差 = 假绿通道）
  ⇒ ★★ **扩张权**：★ **白名单的扩张 MUST 经 architect 审**，**实施者不得自行扩张**（★ 2026-10-26 用户裁决）⇒ 门禁脚本对「白名单变更」MUST 要求 commit message 引裁决编号
  ⇒ ★★ **规模护栏**：★ 例外条目数 > **N**（N 由 A1 细则定，初始建议 10）⇒ **强制 architect 复审**（★ 仿项目既有模式：`ADR-088 §2.1` 的「白名单 > 5 条 ⇒ 提示 ADR 复审」）⇒ ★ **MUST NOT** 以「条目数硬上限 ⇒ 门禁红」作为护栏（★ 会逼出假登记）
  ⇒ ★ **常驻合成例外条目**：★ 门禁 MUST 自带 **1 条合成例外**（用于持续验证白名单机制本身不会随清零而腐化）⇒ 走**常驻 fixture**，不计入真实例外计数。
- ★ **先例**：上游 `c27543d` 把 `aread` 对齐为 0-cycle vs 0-cycle ⇒ ★ **默认路径是「上游补齐对齐」**，例外是**待收敛项**而非终态。

### D4 ★ corpus（Q2 裁决）：**vendored 47 + branchbench 4 档**；Dhrystone **后置**

| corpus | 规模 | 为何入选 / 后置 |
|---|---|---|
| ★ **vendored ELF 48（两侧交集）** | ★ **48** | ★ **实测（A3 已收口）**：Simulator 侧 `test_cpu_chmem_vendored_elf.cpp` 的 `kCases` = 40 `rv32ui-p-*` + 8 `rv32um-p-*` = **48**（★ **已补 `rv32ui-p-simple`**）；Verilator 侧 `test_cpu_verilator_sim.cpp` = **48** ⇒ ★ **两侧现已完全对齐（48 == 48，交集 48，零漂移）**<br>★ **`rv32ui-p-simple` 的处置 = 补进 Simulator 侧 `kCases`（1 行）**（★ 该 ELF **已在** `tests/cpu/riscv_tests/elf/`）⇒ **不是**排除、**不是**例外 ⇒ ★ **A3 已落地**<br>★ ★ **两侧 corpus MUST 抽成单一清单文件**（两 harness 各自读）⇒ ★ 门禁 MUST 断言「两侧实际执行集合 == 清单」（集合不对称 ⇒ FAIL）⇒ ★ 否则两份手维护的 C++ 数组必然再漂<br>★ **落地**：`tests/cpu/parity/corpus.txt` = **52 case = 48 ELF + 4 branchbench** |
| ★ branchbench 4 档 | 4 | 可控预测率负载（always_not_taken / always_taken / pattern_6of8 / pseudo_random）⇒ 覆盖方向与 flush 语义 ⇒ **首批**<br>★ ⚠️ **Simulator 侧无 branchbench runner**（`tools/measure_branchbench.sh` 硬绑 `cpu_verilator_sim`）⇒ ★ **须新增**（估 ~1 工作日）+ 运行时表征（~0.5~1 工作日）<br>★ ⚠️ **时长风险**：★ **v1.4 吞吐实测更正**：Simulator **≈ 9~10 拍/s**（★ **不是** 575 拍/s —— 原值高估 ≈60×；实测 `rv32ui-p-add` 478 拍 / 52.8 s，branchbench 20k 拍 ≈ 37 min）⇒ branchbench 完整跑（~86k 拍）≈ **61 h/档** ⇒ ★ **4 档 × BP on/off 不可进 PR 级门禁** ⇒ ★ **cap 重定为 20k**（⇒ 该 4 档为 **partial**：承重判据 = `regs`+`memsig`+`pchash`，`tohost` 不可采）⇒ 归**夜间层**<br>★ **落地**：`tools/parity_runner/parity_sim_runner.cpp`（Simulator 侧通用 ELF runner）+ `--reset none` |
| Dhrystone | 1 | 单 case 极长（CH_MEM 侧 ≈ 26.5 min）⇒ ★ **后置**（门禁稳定后接入，作为性能相关的第 5 个观测量载体） |
| ★ 未来 | CoreMark 等 | 随交付点 B 接入 |

⇒ ★ corpus 的**两侧产物** MUST 可采集上述 5 个观测量 ⇒ 若某 corpus 暂不支持某观测量 ⇒ 该观测量对该 corpus **显式豁免**（登记在例外清单里，**MUST NOT** 静默跳过）。

#### ★★ D4a **A3 首跑结果（2026-10-27，v1.4 新增 —— 阶段 A 的核心结论）**

| 项 | 结果 |
|---|---|
| 语料 | **52 case = 48 ELF + 4 branchbench**（单一清单 `tests/cpu/parity/corpus.txt`） |
| 跑批 | `--layer night --jobs 4` ⇒ ≈ **50 min**（4 核；Simulator 侧 9~10 拍/s 是瓶颈） |
| 判定 | **52/52 PASS** ⇒ ★ **既存后端分歧清单 = 空**（真实例外数 **0**） |
| 观测量 | 5 项**逐位相等**（含 `pchash` = **逐拍 PC 流 hash** ⇒ 最强判据） |

⇒ ★★ **「今天两边都 `tohost==1` 但内部状态不同」这一担忧，被实测证伪**：
两后端在 ALU / load-store / M 扩展 / 分支密集负载上**逐位一致** ⇒ ★ **阶段 A 的照妖镜没有照出分歧**。

⇒ ★★ **但首跑同时暴露了 4 个门禁自身的真缺陷**（都已修）：
① **partial 假绿**（两侧都被 cap 截断 ⇒ `tohost` 两侧恒 0 ⇒ 「相等」被误当通过证据）
② **Verilator 产物串扰**（`--work-dir`/`--verilog` 全局共享 ⇒ 并行时后一个 case 覆盖前一个的网表 ⇒ **静默错测**）
③ **per-case cap 缺失**（ELF ~1k 拍 vs branchbench ~86k 拍 ⇒ 单一全局 cap 必二选一失败）
④ **分层语义反了**（`--layer night` 只选 `night` 层 ⇒ 漏 PR 层 8 条 ⇒ 夜间层比 PR 层还小）

⇒ ★ **2 项未覆盖（诚实标注）**：
① branchbench 4 档**只跑到 20k 拍**（未跑到 done ⇒ 完整校验和 `tohost` 未验证）⇒ 归开放项 **O-5**
② **BP 相关路径未覆盖**（GShare 增量 3 的 WIP 已 stash ⇒ 本次跑的是 Bimodal + uncond BTB 状态）
⇒ ★ 增量 3 定稿后 **MUST 重跑**（BP 逻辑变更 ⇒ 后端行为可能变）。

### D5 ★ 台账重新定性（Q4 裁决）：从「迁移追踪器」→「**CH_MEM 覆盖边界规格**」

| 阶段 | 台账的角色 | 维护方式 |
|---|---|---|
| 现在 → 阶段 B | **迁移追踪器**：463 条 TLM 独占 case 的三态标注（covered / partial / not-covered）⇒ 证明 B3 的迁移完整性 | ★ 人工维护（现状）⇒ **已在漂**（292 vs 282）⇒ 只作 B3 的**工作量清单**，**不再**承担「parity」语义 |
| 阶段 B 起 | ★ **CH_MEM 覆盖边界规格**：哪些行为 CH_MEM **故意不覆盖** + 显式理由（如 `jalr` 多地址 / TID 线程隔离 / `reset` 语义 / `custom_btb_size` 的 M 域） | ★ **冻结**（只增不改）⇒ 它的价值在**冻结**而非维护 |
| 阶段 C 后 | 保留为**规格文档**（不是活跃台账） | 冻结 |

⇒ ★ **parity 语义完全由本 ADR 的差分门禁承担** ⇒ 台账**不再**被引用为「一致性证据」⇒ ★ 这消灭了「人工维护计数当一致性证据」这一失真源。

### D6 ★ 迁移路径与时序：阶段 A **MUST 先于** 阶段 C

> ★ **Q 标签映射（v1.1 更正）**：★ 本 ADR 早期版本在 D4 / D5 / D6 上标注的「Q1 / Q2 / Q3 / Q4 裁决」**与 2026-10-26 用户会话里的 Q1~Q5 编号不对应**（用户的 Q2 = 「A1 不立即开工」、Q3 = 「例外扩张权归 architect」、Q4 = 「跑批合并」）⇒ ★ **本 ADR 一律改用「裁决内容」而非「Q 编号」引用** ⇒ 对应关系：
> | 本 ADR 条款 | 2026-10-26 用户裁决 |
> |---|---|
> | D2 观测量集 + D3 对齐规则（逐拍相等默认 + 例外登记制） | **Q1**（默认逐拍相等 + 例外须显式登记）|
> | D3 例外**扩张权归 architect** | **Q3** |
> | D6 阶段 A 的**开工前置 = 双审通过** | **Q2**（不立即开工）|
> | D6 阶段 A 的 **A3 跑批与 BP 增量 3 合并**（落 roadmap） | **Q4** |
> | D5 台账重新定性 | **Q4 裁决**（★ 用户会话第 4 条是「跑批合并」⇒ ★ 此处指**裁决包**的第 4 条「台账重新定性」）⇒ ★ **命名歧义已消除：今后一律写「台账重新定性（ADR-089 D5）」，不用 Q 编号** |

```
阶段 A（v0.11.0 窗口；★ 开工前置 = G-A0 双审通过）——不动 TLM 模式；新增【差分门禁】
   ├─ A1  parity-gate-spec            （零代码契约；★ 契约条目编号化 S-1…S-n）
   ├─ A2a backend-parity-observability（Simulator 侧通用 ELF runner + 两侧 5 观测量采集 + 样例 JSON 证据）
   ├─ A2b backend-parity-harness      （比对器 + fail-closed + 变异自检 + 白名单校验脚本）
   ├─ A3  backend-parity-corpus       （语料接入 + ★ 首跑产出「既存后端分歧清单」）
   └─ A4  backend-parity-ci           （门禁接线 + CI build 能力 + 分层 corpus ⇒ = ADR-088 判据 7 的落点）
   ★ 独立于 TLM 退役；★ 立刻能抓到今天不可见的后端分歧

阶段 B（与阶段 C 并行）——台账重新定性（D5）
   └─ 迁移完成度改由【阶段 A 门禁 + B3 删除闭包反查】证明，不再靠人工计数
   ★ 顺序判据：计数修正 → check_coverage_parity_ledger.sh PASS → **再**落冻结位
   ★ 冻结时拆两层：not-covered+理由 ⇒ 规格正文；covered/partial ⇒ 历史附件

阶段 C（B3 落地时）——删 TLM 模式
   ├─ uint_t 模态消失 · pb.run() 快环消失 · bridge/ 与 TLM-only 文件删除
   └─ ★ 5 条硬前置（缺一不得删）：
       ① 阶段 A 差分门禁已上线且全绿
       ② CI 具备 build 能力（ADR-088 §5.2 判据 4 的可判定化）
       ③ TLM 参考实现的语义已抽成规格
       ④ 台账已冻结为覆盖边界规格（not-covered 项已显式登记）
       ⑤ ★★ **门禁体系自身改版**（v1.1 新增 —— **治理层自噬**）：
          `check_plugin_portability.sh` 的 Check 2 / 5 / 6 / 7 / 13 全部**建立在双模前提上**
          ⇒ 删 TLM 模态后：★ **Check 2 / 7 会「真空 PASS」**（TLM 文件集合为空 ⇒ 最坏的假绿形态）
          ⇒ MUST 改写为单模语义（Check 5 仍有效；Check 13 随模态消失而**显式退役**）
          ⇒ ★ 空集合 MUST 触发脚本**显式报告「该检查已退役」**，MUST NOT 静默 PASS
          ⇒ `check_tlm_freeze.sh` + `.rddf/state/tlm-freeze-*` 显式退役
          ⇒ ADR-040 升 v4.0（移除 TLM 分支语义）· ADR-083 / AGENTS.md 双模表述清除
          ⇒ ★ **验收形式 = 变异自检**：注入一个旧 Check 5 能抓的 `if(ch_bool)` ⇒ 新检查 MUST 仍 RED
```

⇒ ★ **B3 的删除判据（ADR-088 §5.2）追加的是「判据 6（差分门禁全绿）」与「判据 7（CI build 能力）」两条**（★ **不是**「第 4 条」—— §5.2 的第 4 条是既有的「CI 有可回归的自动测试」）。
⇒ ★ **阶段 A 的 5 个里程碑的工期**：A1 ≈ 0.5~1 人日 · A2a ≈ 2~3 人日（★ 新增 Simulator 侧通用 ELF runner ~1 人日 + 两侧 5 观测量采集 + 样例 JSON）· A2b ≈ 1.5~2 人日 · A3 ≈ 2~3 人日（★ 含运行时表征与 cap 重定）· A4 ≈ 1~2 人日 ⇒ ★ **合计 ≈ 7~11 人日**（★ 早期估计「A2 = 1 行」严重低估，已按双审修正）。

### D7 ★ 明确**不**在本 ADR 范围内

- ✗ TLM 模式删除的**具体时点**（归 ADR-088 §5.2 / B3，归 roadmap phase-5）
- ✗ CI 流水线重构（归 roadmap；本 ADR 只把「CI build 能力」列为阶段 C 的硬前置）
- ✗ parity 门禁的**实现**（归 builder；本 ADR 只定契约）
- ✗ 观测量 ②③⑤ 的**采集接口设计**（归 A2a，须服从本 ADR 的 fail-closed 纪律；★ ③ 的**首选落法**已在 D2 定为 DUT 自算签名，host dump 留作增强项）
- ✗ **PC 轨迹的定义**（采样点 / 采样率 / 相位）——归 A1 细则
- ✗ **elaboration 确定性的实现**（node_count / 信号名单 hash 的取法）——归 A2a

---

### 实现注记：CH_MEM 配置面穿透（P2，2026-10-27）

> 这是**实现事实记录**，不是新决策（配置面本身不在本 ADR 的决策范围内）。

门禁与两个 runner 现支持 `--cpu-config <path>`，把 **CH_MEM CPU 的 JSON 配置**（`CpuConfigChmem`，
见 `ip/cpu/configs/cpu_config_chmem.h`）转发给两侧。

- ★ **两侧 MUST 用同一份配置** —— 否则比的是「两种配置」而非「两个后端」⇒ 判据失去意义
- ★ **`--cpu-config` 与配置类 flag（`--enable-mmu` / `--no-bp` / `--btb-entries` …）互斥**（同时给 ⇒ rc=2）
  —— 静默覆盖 = 假绿通道
- ★ 报告新增顶层 `cpu_config` 字段 ⇒ 事后可判断「这次跑的是哪套配置」；为空串 = 走各 runner 默认
  （与 P2 之前逐位一致）
- ★ **实测**：PR 层 8 case 逐位等价 · 全量 52 case **52/52 PASS**（determinism 指纹与 A3 基线逐位相同）

---

## Consequences

### 正面

1. ★ **补上一个今天存在的验证空洞**：两后端互比从 0 提升为门禁。
2. ★ **消灭「人工计数当一致性证据」**：parity 语义从代理指标（463 行计数）换成机器可判的差分执行 ⇒ 台账漂移不再影响结论。
3. ★ **把 bug 类收敛到有界集合**：差分执行只暴露**后端**分歧（调度 / 延迟 / 复位语义），不要求两个**不同设计**互相一致。
4. ★ **为 B3 提供唯一可信的替代证据**：删掉 463 个 TLM-only case 后，「没有自动机制能发现语义退化」（`feat-tlm-backend-retirement` 实测结论）这一盲区由差分门禁补上。
5. ★ **长期收益（阶段 C）**：`uint_t` 模态消失 ⇒ 「全绿但语义为零」的双实现漂移族（ADR-040 §10.5/§10.6、host `pack_entry` vs ch 写路径等）**整族消失** ⇒ 今天为抓这类漂移而存在的部分锚（`[keys]` 4 case + `[alu][semantics]` 6 case）的一半将变成历史 ⇒ 测试预算可还给真实功能覆盖。

### 负面 / 成本

1. ⚠️ **阶段 C 会删掉唯一的亚秒级回归环**：实测 TLM binary **432 case ≈ 0.6s** vs CH_MEM Simulator **vendored 47 ≈ 1594s（~34s/ELF）** vs Verilator（dhrystone 端到端含 Verilog 生成 + Verilator 编译）**18.85s** ⇒ ⇒ **CI build 能力必须先到位**（D6 硬前置 ②）。
2. ⚠️ **差分门禁的运行成本**：每 corpus 需两后端各跑一次 ⇒ Verilator 侧每次都要 Verilog 生成 + 编译 ⇒ 门禁时长须预算（★ 用 vendored 47 + branchbench 4 档起步）。
   ★★ **2026-10-26 实测更正（A2a increment 1）**：**本文档早前引用的「Simulator ≈ 575 拍/s」高估约 60×** —— `rv32ui-p-add`（478 拍 / DAG 2485 节点）实测 **52.8 s** ⇒ **≈ 9.1 拍/s**，且 `--no-bp` 与默认 BP **无显著差异**（200 拍：21 s vs 24 s）⇒ 瓶颈是解释器本身，不是 BP。
   ⇒ ★ **直接后果**：`branchbench --cycles 2000000` 在 Simulator 侧**根本不可行**（≈ 61 小时/档）⇒ Simulator 侧 branchbench 的 cap MUST 重定为 **≤ 20k 拍（≈ 37 min/档）**，否则该 corpus 只能在 Verilator 侧跑并登记豁免（见契约 S-15）。
3. ⚠️ **例外白名单有被滥用的风险** ⇒ 纪律：每条例外须带 **5 项**（机制说明 / 上游状态+issue 链接 / 变异对照 / **收敛期限** / **corpus 作用域**）；★ 白名单 MUST 由 **architect** 审（不得由实施者自行扩张）；★ 条目数 > N ⇒ 强制 architect 复审；★ 常驻 1 条合成例外防机制腐化。
4. ⚠️ **「Verilator 每次都要生成 + 编译」是现状的坏成本模型**（`cpu_verilator_sim.cpp:202-260` 每次调用都 `generate_verilog()` + `verilator --cc` + `g++`）⇒ ★ **MUST NOT** 把该现状写成契约约束 ⇒ ★ **harness MUST 按 config 缓存 Verilog 构建产物**（同 config 的 47 ELF 只 build 一次）⇒ ★ 否则门禁时长预算直接爆炸。
5. ⚠️ **elaboration 确定性不是免费性质** ⇒ Verilator 侧按名采集（`--public-flat-rw`）⇒ 上游若改了信号名/发射顺序 ⇒ **静默错位** ⇒ ★ 确定性判据（D2）必须先于观测量比对。
6. ⚠️ **CI build 能力是必要但不充分** —— 它解决「有没有保护」，不解决「开发者本地快环」⇒ ★ **补强方案 = 单元级快车道**：CH_MEM Simulator 的**单元级**测试（`[keys]` / `[alu][semantics]` / `[mmu-chmem-basic]` 这类不跑 ELF 的）本身比 ELF 端到端快几个量级（★ 1594s 的大头是 47 个 ELF 的 context + elaborate + 仿真）⇒ ★ **PR 级快环 = CH_MEM 单元级 + 小子集 ELF 差分**；全量差分进夜间 ⇒ ★ 该方案**不依赖 TLM 存活**。
7. ★ **待测项（不进任何承诺）**：CppHDL Simulator 自带 JIT 通道（`simulator.h:90-97` `set_jit_enabled` / `try_jit_compile`）⇒ 若可用可大幅压缩 Simulator 时长 ⇒ 阶段 C 前值得花 ~0.5 人日实测（★ 现状未知 ⇒ **MUST NOT** 写进工期承诺）。
8. ⚠️ **cycle 逐拍相等可能要求上游 CppHDL 多轮对齐**（像 `c27543d` 那样）⇒ 阶段 A 的工期不确定 ⇒ ★ **兜底**：例外白名单（带收敛计划）⇒ **MUST NOT** 用容差替代。

---

## Alternatives considered

| 备选 | 结论 | 理由 |
|---|---|---|
| **A. 保留 TLM 作为 parity 第三条腿**（TLM ≡ CH_MEM ≡ Verilator 三方对拍） | ❌ **拒绝** | 比的是**两个不同模型**（建模语义差异无界）⇒ 与「双后端零对照」的实测冲突（M 扩展 cycle 语义**不可比**，AGENTS.md 自认）⇒ 且 TLM 侧的 BP 是**惰性桩**（E-1/E-2 实测：摘掉 TLM BP 注册后 cycles 逐位不变）⇒ 它**不是**可信的语义参照 |
| **B. 维持台账计数作为 parity 机制**（status quo） | ❌ **拒绝** | 已在漂（292 vs 282、#11 冲突）⇒ 人工维护的计数不可作为一致性证据；且对「两后端是否一致」**完全无感** |
| **C. 只比 `tohost == 1`** | ❌ **拒绝** | 最弱判据：两边都过但内部状态不同 ⇒ 不可见 ⇒ 与现状无实质差别 |
| **D. cycle 严格相等、零例外** | ⚠️ **不采纳为唯一形态** | 组合 DAG 与 `always_ff` 调度语义本质上可能不同 ⇒ 零例外会让门禁长期红 ⇒ 迫使团队用容差绕过 ⇒ **更坏**。⇒ 采用 D3 的「默认严格 + 受控例外（带机制说明/上游状态/变异对照）」 |
| **E. 只做 Verilator，弃 Simulator** | ❌ **拒绝** | Simulator 是**零编译成本**的快速后端（无需 Verilog 生成 + Verilator 编译）⇒ 是差分执行的**廉价一侧** ⇒ 弃它等于把差分成本翻倍 |

---

## Verification

- [ ] **V-1** parity 门禁在 **两侧交集 47 ELF** + branchbench 4 档上运行：②③⑤ bit-exact、**⑤ 逐拍 PC 流相等**、④ 总 cycle 数相等（例外白名单为空或每条合规）
  - ★ **三态语义（v1.1 补）**：★ 任一侧 **TIMEOUT / runner 进程崩溃（segfault 等）⇒ 门禁判红**（fail-closed；★ 三态语义下归 **ERROR(2)**）⇒ ★ **MUST NOT** 计入「后端分歧清单」（那是**门禁自身故障**，不是后端语义差异）⇒ 门禁 MUST 区分「FAIL（分歧）」与「ERROR（门禁自身故障）」两种退出码
  - ★ **全绿的语义（v1.1 澄清）**：★ 「白名单非空时全绿」= ★ **已知分歧已被登记**，★ **MUST NOT** 理解为「两后端已等价」⇒ 阶段 C 前置①「差分门禁全绿」**允许**带未收敛的已知分歧进入（★ 但每条 MUST 有收敛期限；★ 期限到期 ⇒ 自动升级为「真 bug」类 ⇒ 阻塞阶段 C）
- [ ] **V-2** 门禁 **fail-closed 验证**：★ **每个观测量各做一次** fail-closed 用例（**≥5 例**，不是「关闭一个」即可）⇒ 门禁 MUST 判红（★ 归 **ERROR(2)**，而非跳过）⇒ ★ 外加：确定性判据（node_count + 信号名单 hash）不符 ⇒ MUST FAIL（**先于**任何观测量比对）
- [ ] **V-3** 门禁**变异对照**：★ 至少 2 例 —— ① 把 Simulator 侧某机制的时序改 1 拍（如复现 `aread` 1 拍滞后）② 改 1 字节内存写（⇒ 内存签名不同）⇒ 门禁 MUST RED（证明门禁有鉴别力，而非「两边都过就绿」）
- [ ] **V-4** 例外白名单机制生效：★ 登记一条**合规**例外 ⇒ 门禁仍 PASS 且该例外在报告中可见 ⇒ ★ 登记一条**缺字段**（5 项任缺）的例外 ⇒ 门禁 MUST FAIL ⇒ ★ 登记一条**过期**（收敛期限已到）的例外 ⇒ 门禁 MUST FAIL（并升级为真 bug 类）⇒ ★ **常驻合成例外**（验证机制本身不随清零而腐化）⇒ 门禁 MUST PASS 且不计入真实例外计数
- [ ] **V-5** 阶段 C 前置检查：★ 前置 ①②④ **可由脚本判定**；★ 前置 ③（TLM 参考语义抽成规格）为 **architect 判断题** ⇒ ★ **判定物 = 逐子系统规格清单**（BP GShare ✓ 已抽到 `ip/cpu/docs/bp-btb-gshare-design-brief.md` §3.1/§7 D10 · **M 扩展 radix-2 语义** / **CSR stub 语义** / **MMU PTW walk 语义** 逐项列出）+ architect 签认 ⇒ ★ 前置 ⑤（门禁体系改版）以**变异自检**判定（见 D6）
- [ ] **V-6** 台账冻结：★ **顺序 = 计数修正 → `check_coverage_parity_ledger.sh` PASS → 再落冻结位** ⇒ ★ **G-B 的判据 MUST 含「脚本新增『头部计数 == 条目级实测』等式校验」作为交付物**（★ 现状该脚本**无此检测能力** ⇒ 292 vs 282 才会在「脚本 PASS」下长期存活 ⇒ 「脚本仍 PASS」**不构成证据**）⇒ ★ 冻结时**拆两层**：not-covered+理由 ⇒ 规格正文；covered/partial ⇒ 历史附件（★ 由阶段 B 主体负责）

---

## Related

- `docs/architecture/adr/ADR-088-tlm-backend-retirement.md`（TLM 退役治理锚；本 ADR 是其 B3 前置契约）
- `docs/architecture/adr/ADR-083-plugin-style-single-source-of-truth.md`（单源真相；本 ADR 把「一致性」的度量方式定死）
- `docs/architecture/adr/ADR-040-tlm-hdl-portability-constraints.md`（CH_MEM 正道 + 移植性约束；§10.5/§10.6 的漂移族在阶段 C 整族消失）
- `docs/architecture/tlm-coverage-parity-ledger.md`（台账；★ 阶段 B 起重新定性为覆盖边界规格）
- `docs/architecture/tlm-retirement-b1-capacity-plan.md`（B1 承载力盘点）
- `ip/cpu/docs/bp-btb-gshare-design-brief.md`（BP 规格 v2.3；§3.1 / §7 D10 是「TLM 参考语义抽成规格」的样板）
- `.rddf/roadmap/features/feat-backend-parity-gate.md`（阶段 A，★ planner 落）
- `.rddf/roadmap/features/feat-tlm-backend-retirement.md`（阶段 B/C，★ planner 落）
- `.rddf/roadmap/objectives/objective-ip-cpu-tlm-backend-retirement.md`（B3 门禁）

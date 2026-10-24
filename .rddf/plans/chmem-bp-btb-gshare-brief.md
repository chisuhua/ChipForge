# ② chmem-bp-btb-gshare — 实施边界简报（planner → builder 交接物）

> ⚠️ **档名互换重命名（`chmem-branch-microbench-fix`, 2026-10-24）**：本文的 `always_taken`/`always_not_taken` 为**旧名**（与硬件方向相反）⇒ 已互换；旧 cycles 数值已作废（工作量污染）。


> **change**: `chmem-bp-btb-gshare`（B1 执行图 §5.1 顺序 1 = **最高杠杆**）
> **简报日期**: 2026-10-24 · **作者**: planner · **接收方**: builder
> **配套**: 执行文件 `.rddf/plans/chmem-bp-btb-gshare.md`（40 open / 11 AC）
> **上游裁决**: ADR-088 v1.2 §3 BP 行（**CH_MEM-only 且必须做**；不适用 §5.2「不做」出口）· §5.2 BP 专项删除前置（双前置）· §5.3.2 **P-5**（中间验收）· §5.3.2 **P-6**（绝对值门禁恢复条件 = **本 change 自身**）· §5.3.1（真根因 = CH_MEM 无分支预测）

---

## 0. 为什么这份简报必须先于实施

本 change 有 **5 个「照抄即挂」点**（下面 §1–§5 逐个给出**已核实的真实接口**与**正确做法**）。执行文件里的措辞不足以让 agent 不踩坑 ⇒ 本简报把「实现者必须知道、但 plan 里没写」的事实补齐。

---

## 1. ★BP 开关面设计（本 change 最硬的接口需求）

### 1.1 需求来源

- 0c `chmem-branch-microbench` 的 **AC-8 中间验收** = 「启用 BP vs 禁用 BP」的 Δcycles ⇒ **必须有可关闭 BP 的开关**，否则验收不可执行。
- ADR-088 v1.2 §5.3.2 **P-6**：绝对值 1.4 门禁的恢复条件 = 本 change 落地 ⇒ BP 的**收益**必须能被**归因**（若不能区分启用/禁用，收益无法归因 ⇒ 绝对值门禁无法恢复）。

### 1.2 ★关键约束：**CH_MEM 工厂不接 `CPUConfig`**

实测（`ip/cpu/cpu_factory_chmem.h:95-107`）：

```cpp
static std::unique_ptr<PipeBuilder> build_cpu(
    ch::core::context* elaboration_ctx,
    T* memory = nullptr,
    T initial_pc = T{0x80000000},
    bool preload_elf = false,
    const std::vector<uint8_t>& elf_image = {},
    std::optional<bool> enable_mmu = std::nullopt,
    std::optional<std::string> mmu_mode = std::nullopt,
    std::optional<bool> enable_cache = std::nullopt);
```

⇒ **ADR-088 §9.1 NG1/NG2/NG3 显式声明**：CH_MEM 工厂**不读 cfg、不引 `CpuConfig` 依赖**。

⇒ ★**推论：BP 开关面不能靠 `CPUConfig` 字段**（`ip/cpu/configs/cpu_params_schema.json` 的 `branch_predictor` / `btb_entries` 字段对 CH_MEM **无效**）。这与 plan 里「如 `CPUConfig` 字段」的措辞**不符** —— 实施时**照抄会挂**。

### 1.3 推荐方案（planner 建议，builder 可调整但须在 Task 1 记录理由）

**双层开关**：

| 层 | 入口 | 用途 |
|---|---|---|
| **L1 工厂参数** | `build_cpu(..., std::optional<bool> enable_bp = std::nullopt, std::optional<std::uint32_t> btb_entries = std::nullopt)` | 库级 API（供 `cpu_verilator_sim` 与单元测试调用） |
| **L2 CLI flag** | `cpu_verilator_sim --enable-bp` / `--no-bp` / `--btb-entries N` | 测量脚本的外部入口（`measure_branchbench.sh` 需要它） |

**语义**：★**默认必须是「启用」**（否则「BP 没生效」会被误当成「BP 无效」），但**必须能显式关闭**用于对照。

⇒ ⚠️ **半双态陷阱**：`std::optional<bool>` 的 `nullopt` 语义在 `enable_mmu` / `enable_cache` 上表示「不读 cfg、走默认」。若 BP 用同样写法，**「未指定」与「显式关闭」会撞** ⇒ 建议 BP 开关用**普通 `bool enable_bp = true`**（无三态需求），避免语义歧义。

### 1.4 CLI 侧的既有先例（照抄即可）

`tools/verilator_runner/cpu_verilator_sim.cpp` 已有 `--enable-mmu` / `--mmu-mode` / `--enable-cache` 三个 flag，模式为：

```cpp
} else if (a == "--enable-mmu") { opts.enable_mmu = true; }
...
opts.enable_mmu, opts.mmu_mode, opts.enable_cache);   // 转发给 build_cpu
```

⇒ BP flag **完全同构**：加 `Options` 字段 → `parse_args` 分支 → `--help` 文本 → 转发给 `build_cpu`。

### 1.5 验收要求

- [ ] `cpu_verilator_sim --enable-bp` 与 `--no-bp`（或等价）能产出**不同的 cycle 数**（且**校验和相同** ⇒ 语义不变）
- [ ] `tools/measure_branchbench.sh` 支持**启用/禁用两组**测量并**同时**写入基线 CSV（两行：`bp_enabled` / `bp_disabled`）⇒ 0c 的 CSV 现有 4 行是**无 BP 参照值** ⇒ ② 落地后须**扩充**而非替换（**保留无 BP 行作历史锚**）

---

## 2. ★14 条台账重判清单

### 2.1 精确清单（实测自 `docs/architecture/tlm-coverage-parity-ledger.md`）

**10 条 `not-covered`（ADR-088 §3 BP 行已按 B2 裁决降级，§7 表已登记）**：

| # | 条目名 | 现状 | ② 落地后的目标态 |
|---|--------|------|-----------------|
| 1 | `btb_entry` | `not-covered` | `covered`（CH_MEM 有 BTB 结构 + 单测锚） |
| 2 | `custom_btb_size` | `not-covered` | `covered`（运行时可配 BTB 尺寸 ⇒ 需**可配置面**，见 §1） |
| 3 | `btb_runtime_16` | `not-covered` | `covered`（同上） |
| 4 | `btb_runtime_64` | `not-covered` | `covered`（同上） |
| 5 | `btb_runtime_256` | `not-covered` | `covered`（同上） |
| 6 | `no_predict_initially` | `not-covered` | `covered`（冷启动不预测语义有载体） |
| 7 | `predict_after_update` | `not-covered` | `covered`（预测器更新路径有载体） |
| 8 | `d2_branch_predictor_multi_thread` | `not-covered` | ⚠️ **见 §2.3** |
| 9 | `d4_branch_predictor_tid_param` | `not-covered` | ⚠️ **见 §2.3** |
| 10 | `branch_predictor_set_tid_stores_tid` | `not-covered` | ⚠️ **见 §2.3** |

**4 条 `partial`**：`not_taken_returns_zero` · `multiple_addresses` · `reset` · `per_thread_ghr_isolation`

### 2.2 ★重判的**硬口径**

| 规则 | 内容 |
|------|------|
| **条目态继承族级 §2** | 改**条目**必须**同改族级**判定行 ⇒ 台账 §2（46 族）也要改 |
| **not-covered 必须显式手写理由** | 不得为「继承」标记（门禁 `check_coverage_parity_ledger.sh` 会查「理由列为空 or 以『继承』开头」⇒ VIOLATION） |
| **同一理由 ≤10 次** | 门禁机检（`awk -F'|'` 取第 5 列，归一化仅去首尾空白，分组 = 完全相同字符串）⇒ ★**若 10 条 BP 条目共用一个理由 ⇒ 恰好卡在 10（≤10 合规）**，但**再加一条就违规** ⇒ **建议按 4 组差异化理由写**（ADR-088 §7 表已示范：最大组 5 条） |
| **红线总数** | `not-covered` 总数 ≤135（当前 27）· `[framework]` 族 **0** 条 `not-covered` |

### 2.3 ★3 条 TID 条目的**特殊处置**（易踩坑）

`d2_branch_predictor_multi_thread` / `d4_branch_predictor_tid_param` / `branch_predictor_set_tid_stores_tid` —— 这 3 条依赖**框架级多线程 TID 机制**，而 **CH_MEM 侧无此机制**（TLM 侧有 `N_THREADS` 模板参数，见 `branch_predictor.h:83`）。

⇒ **不得**为让它们变绿而**虚构一个 TID 机制**。正确处置（三选一，**须在 tasks.md 记录理由**）：
- **(a)** 保持 `not-covered`，理由写「CH_MEM 无框架级多线程 TID 机制 ⇒ **框架级缺口**，非 BP 缺口 ⇒ 归属 framework scope」⇒ **诚实**，但会让 `not-covered` 保持 3 条
- **(b)** 若 CH_MEM 侧 BP 实现**恰好**支持 TID 隔离（如按 `pc` 高位分区）⇒ 升 `covered` + 有 case 证明
- **(c)** 升 `partial` + 理由「单线程路径已覆盖，多线程隔离语义未覆盖」⇒ ⚠️ **注意**：这会让**同一理由**的条数增加 ⇒ 须计入 §2.2 的 ≤10 红线

⇒ ★**planner 建议 (a)**：这 3 条是**框架级缺口**，把它们混进 BP change 会造成「BP 已完成但 3 条仍未覆盖」的**语义模糊** ⇒ 建议 (a) + 在台账注明「framework scope，**不阻塞 B3**」（ADR-088 §5.2 判据 2 允许「明确 `not-covered` 且归属『不做』」⇒ ⚠️ **但 BP 不适用「不做」出口** ⇒ 这 3 条**必须**有一个明确的**归属声明**，不能只写「不做」）

---

## 3. ★BTB 4K + PHT 2^13×2bit 的体积 / 编译耗时预算

### 3.1 存储量估算

| 结构 | 规格 | 估算 |
|------|------|------|
| **BTB** | 4096 entry × (tag + target + valid) | tag 32b + target 32b + valid 1b ≈ 65b ⇒ **≈ 33 KB**（`ch_mem` 承载） |
| **PHT** | 2^13 = 8192 entry × 2bit | **≈ 2 KB** |
| **GHR** | 13 bit | 2 B |
| **RAS** | 8 entry × (target + valid) | ≈ 68 B |
| **合计** | | **≈ 35 KB** |

### 3.2 ★必须量化的三条判据（plan 的 Task 5.5）

| # | 判据 | 为什么 | 怎么量 |
|---|------|--------|---------|
| **B-1** | 生成 Verilog 的**行数 / 文件大小** | 35 KB 的 `ch_mem` 若展开成寄存器数组 ⇒ Verilog 巨大 ⇒ **Verilator 编译时间爆炸**（`cpu_verilator_sim` 每次跑都要 Verilate） | `pb->to_verilog(ctx)` 后 `wc -l` + `du -h` ⇒ 与**当前基线**对比（须先测当前值作对照） |
| **B-2** | **Verilator 编译耗时** | 同上 —— 现有 ctest `verilator_perf_baseline` 实测 **110.86s**（4 档 82.16s + perf）⇒ 若 BP 让 Verilate 时间翻倍 ⇒ CI/本地回归都受影响 | `time` 包住 `cpu_verilator_sim`（首次含 Verilate）⇒ 与当前基线对比 |
| **B-3** | **C++ 侧仿真耗时** | 35 KB `ch_mem` 的初始化 + 每拍访问成本 | 跑 4 档 microbench ⇒ 与 0c 无 BP 参照值对比（**cycles 应下降，但墙钟可能上升**）⇒ 两个维度都要报 |

⇒ ★**若 B-1/B-2 超阈值 ⇒ 缩容量**（BTB 4096 → 1024/2048，PHT 2^13 → 2^11）⇒ **但缩容量须同步改 §2 的台账条目**（`custom_btb_size` / `btb_runtime_*` 依赖「尺寸可配」⇒ 缩的是**默认**不是**可配性** ⇒ 台账不受影响）。

### 3.3 ★`ch_mem` 承载的**已知风险**（plan 未提，务必先做 PoC）

`ch_mem<ch_uint<32>, N>` 是 **N 个 32-bit word 的固定大小数组**。BTB 4096 entry 若用**每 entry 1 word** 存储 ⇒ 需要 **4096 words = 16 KB**；若每 entry 2-3 words ⇒ 32-48 KB ⇒ ⚠️ **`ch_mem` 的 N 是编译期模板参数** ⇒ **体积在编译期固定** ⇒ 不能运行时调 BTB 尺寸！

⇒ ★**这与台账条目 `custom_btb_size` / `btb_runtime_{16,64,256}`（「运行时可配」）冲突**：
- **方案 1**：BTB 用**固定容量 + 运行时逻辑条目数**（容量取最大，运行时用 `btb_entries` 限制有效范围）⇒ 可配性成立 ⇒ **推荐**
- **方案 2**：模板参数化 ⇒ 运行时不可配 ⇒ 这 3 条台账条目**无法升级为 `covered`** ⇒ 与 §2.1 的目标态冲突

⇒ ★**建议方案 1**，并在 Task 1 的 PoC 中**先验证** `ch_mem` 的模板参数是否真能按最大容量实例化 + 运行时限制有效条目。

---

## 4. ★microbench 前后对比协议（0c → ②）

### 4.1 0c 的无 BP 参照基线（**已归档，不可改**）

`docs/architecture/.../branchbench-baseline-matrix.csv`（实际路径 `soc/cpu/docs/dse/branchbench-baseline-matrix.csv`）：

| 档 | 方向 | 校验和 | **无 BP cycles** |
|---|---|---|---|
| `always_taken` | 恒 taken | 103 | **66034** |
| `always_not_taken` | 恒 not-taken | 183 | **68034** |
| `pattern_6of8` | 6/8 taken（周期） | 147 | **79534** |
| `pseudo_random` | 伪随机 | 101 | **73012** |

★`.text` sha256 **4 档完全一致**（`5a7c0b2c4265c156`）⇒ 指令流相同 ⇒ cycle 差只来自分支方向。

### 4.2 ★判据方向（0c 实测纠正过的，**不要写反**）

| 判据 | 内容 |
|------|------|
| ★**正确** | **方向变化频繁**的档（`pattern_6of8` / `pseudo_random`）Δcycles **显著大于**方向**恒定**的档（`always_taken` / `always_not_taken`） |
| ✗ **错误** | 「恒 taken 收益最大」—— 0c 实测证明**方向相反**（恒定档本就无「方向变化惩罚」⇒ 收益空间小） |
| **反证判据** | 若 **4 档降幅均 ≈0** ⇒ **BP 无效**（不是「档位选得不好」） |

### 4.3 ★协议（测量脚本须支持）

```
bash tools/measure_branchbench.sh --check-baseline           # 无 BP 参照（现有行为，4 档）
bash tools/measure_branchbench.sh --check-baseline --bp=on   # BP 启用
bash tools/measure_branchbench.sh --check-baseline --bp=off  # BP 禁用（= 无 BP 参照）
```

⇒ **CSV 结构**：现有 4 行**保留**（无 BP 参照）+ **新增 4 行**（BP 启用）⇒ 8 行。CSV 须加一列 `bp_mode`（`off`/`on`）。

⚠️ **精确相等口径**（无容差）⇒ BP 启用后的 cycles **必然与无 BP 不同** ⇒ **不能**用同一个「基线漂移」判据 ⇒ 须**按 `bp_mode` 分组**比对。

### 4.4 ★「无 BP 参照」在 ② 落地后的处置

- 0c 的 CSV 是**无 BP 参照值** ⇒ ② 落地后**扩充**（加 `bp_mode` 列 + BP 启用行）⇒ **不替换**（保留历史锚）
- ⚠️ **若 builder 选择替换** ⇒ 必须在 commit message 里说明「无 BP 参照值丢失」⇒ **不建议**

---

## 5. ★GShare `ch_reg` 可行性 PoC（风险已从「架构缺口」降为「diligence」，但仍须先验）

### 5.1 已就位的条件

| 条件 | 状态 | 出处 |
|------|------|------|
| `ch_reg` 承载跨拍状态 | ✅ 可用 | 上游 CppHDL `ch::core::ch_reg<T>`（`include/cf/plugin/ctrl_link.h:79-91` 已用它做有状态 halt 条件） |
| 有状态 halt 通道 | ✅ 已就位 | `CtrlLink::halt_when_stateful(const ch_reg<ch_bool>&)`（`ctrl_link.h:91`）+ `halt_when(ch_reg<ch_bool>)` 重载（`:101`）+ `OR_combined_halt_conds()`（`:203`）⇒ capability `ctrllink-continuous-handle` |
| `at_stage` 闭包执行时机 | ✅ 天然满足 | `elaborate()` 期执行一次 ⇒ 「`ch_reg <<=` 须在 Simulator 创建前调用」的约束**天然满足**（`ctrl-link-chmem-reg-handle` 已交付） |

### 5.2 ★PoC 要验的三件事（Task 1 Step 1，**限时闭环**）

| # | 待验 | 失败时的处置 |
|---|------|------------|
| **P-1** | `ch_reg<ch_uint<13>>`（GHR）能否在 `at_stage` 闭包里 `<<=` 更新，且**下一拍**读出的是新值 | 若不能 ⇒ 降级为**组合式 GHR**（PHT 索引用上一拍值）⇒ **须记录降级 + 性能影响** |
| **P-2** | BTB 的 **tag/target/valid** 三字段能否装进**一个** `ch_mem` word（32b）还是需 2-3 words ⇒ 影响 §3.3 的容量 | 若需多 word ⇒ 容量预算重算（B-1/B-2） |
| **P-3** | **PHT 的 2-bit 饱和计数器**在 `ch_mem` 里怎么存（2 entry/word 打包？还是用 `ch_mem<ch_uint<2>, N>`？）⇒ `ch_mem` 的 T 是 `ch_uint<N>` ⇒ **2-bit 类型可行**，但**索引算术**（`pc ^ ghr`）需在 CH_MEM 侧算 | 若 2-bit 不可行 ⇒ 用 32-bit 存储（PHT 体积 8192×4 = 32 KB ⇒ **超预算** ⇒ 须缩 PHT 至 2^11） |

### 5.3 ★PoC 的判定纪律（防「兜底句挂起」）

plan 的 R6 已定：**PoC 不可行 ⇒ 触发条件 = 连续 2 次失败 ⇒ 落点 = ADR-088 §3 追加一行 + 本 change Task 记录 ⇒ 时限 = 本 change 内必须闭环，不得挂起等回复**。

⇒ **planner 补充**：若 P-3 失败导致 PHT 必须缩容，**不得**默默改容量 ⇒ 须走 §3.2 的「缩容量 ⇒ 同步台账」路径。

---

## 6. 实施顺序建议（planner 给 builder 的落点）

| Task | 内容 | ★本简报对应 |
|------|------|-------------|
| **Task 1** | 事实复核 + TLM 版语义提取 | ★**先做 §5.2 的 3 个 PoC（P-1/P-2/P-3）+ §3.3 的 `ch_mem` 容量验证**（这是 Task 1 的**硬前置**，不做不许进 Task 2） |
| **Task 2** | BTB 实现 | §3（容量预算 B-1/B-2/B-3） |
| **Task 3** | GShare 实现 | §5（PoC 结论） |
| **Task 4** | RAS 实现 | — |
| **Task 5** | misprediction recovery（CtrlLink flush 真实消费） | ⚠️ `flush_when` 的**有状态**版本？实测 `ctrl_link.h:110` 的 `flush_when(Condition)` 与 `halt_when_stateful` 不同 —— **`flush_when` 是否支持 `ch_reg` 需核实**（若不支持 ⇒ misprediction flush 信号须走**组合**路径） |
| **Task 5.5** | 容量与体积预算 | §3.2 的 B-1/B-2/B-3 三条判据 |
| **Task 6** | 工厂注册 | ★**§1.3 的双层开关面**（`build_cpu` 参数 + CLI flag） |
| **Task 7** | 收口 | §2（14 条台账重判 + 族级 §2 同步）· §4（microbench 前后对比） |

---

## 7. 本简报**不**决定的事（留给 builder 在 Task 1 记录理由）

- BTB/PHT 的**具体编码布局**（word 打包方式）⇒ §5.2 P-2/P-3 的 PoC 结论决定
- 预测**接入点**（fetch 阶段改 PC mux？还是只提供信号由 CPU 侧消费？）⇒ 须核对 ADR-048 canonical ordering（`check_canonical_ordering()` 运行时断言）
- misprediction 的**恢复策略**（flush 整条流水线？只 flush fetch？）⇒ 依赖 CH_MEM 的**全组合**特性（无 stage 寄存器 ⇒ 「flush」语义与 TLM 不同 ⇒ ★**这是 ② 最可能返工的一处**，建议 Task 1 先做**语义对齐分析**）
- 是否实现 **Bimodal**（TLM 版有 3 种模式：`static` / `bimodal` / `gshare`）⇒ ② 的 plan 只列了 BTB+GShare+RAS ⇒ 若只做 GShare ⇒ `CPUConfig` 的 `branch_predictor` 枚举里的 `bimodal`/`tournament` 在 CH_MEM 侧**无对应** ⇒ 须在台账或文档中说明（**不要**为此虚构实现）

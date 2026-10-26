# ② 主体 — 增量分档执行路径（repo SSOT）

> **change**: `chmem-bp-btb-gshare`（② 主体）
> **本文件**: 主体「三档增量 → 决策点 → 收口」的**可执行路径**（repo 内 SSOT）
> **主执行文件**: `.rddf/plans/chmem-bp-btb-gshare.md`（Task 2f / 2g / 2h）· **规格权威**: `openspec/changes/chmem-bp-btb-gshare/{design,tasks}.md` 的 §0 v2.3 / v2.3.1 覆盖声明
> **取代**: `/home/ubuntu/.opencode/plan/02b-chmem-bp-btb-gshare-increments.md`（外部草稿 v1）⇒ ★ 冲突以本文件为准
> **规模**: 中-大（增量 3 + 收口 + 裁决）· **前置**: ★ **已归档**（`2026-10-26-chmem-branch-microbench-fix` + `2026-10-26-chmem-bp-btb-gshare-poc`）⇒ **开工门禁已解除**
> **上游裁决**: ADR-088 v1.2 §5.3 · **A1~A7 待 architect 签批**（`.rddf/improvements/bp-spec-rewrite-and-doc-hygiene.md` 顶部「裁决摘要」）· ★ **本文件新增 D8 / D9 / D10 待签批（§9）**
> **修订依据**: v2 = 2026-10-26 builder 审查（2 BLOCKER + 6 SHOULD + 3 NIT）· **v3 = 同轮 oracle + metis 双审**（2 处事实错误 + 1 处编译阻断 + 1 处不可判定的锚）· **v4 = 路径 0 执行期实测**（增量 3 现状 = 双负载负贡献 + 双 target 缺口）⇒ 逐条落点见 **§11**

---

## Goal

按 **A5 决策（2026-10-26 用户裁定 = 选项③）**完成 ② 主体的**三档增量分档**：

> 先扩 `is_unconditional` + `jal`/`j` 覆盖并量收益，再定 GShare

⇒ **增量 1 已完成**（净收益率 56.7% → 78.3%，`cycles` 914544 → **820513**）⇒ ★ **仍未达绝对值门禁** ⇒ **增量 3（GShare，移植）MUST 进本期** ⇒ 本文件 = 增量 3 落地 + 量收益 + 收口 + 裁决的**可执行路径**。

★ **判据（2026-10-26 v3，D9 待签批）**：增量分档的**推进判据 = Dhrystone `cycles ≤ 813074`（整数比较）= DMIPS/MHz ≥ 1.4**；净收益率降为**报告项**（分母口径仍归 A6）⇒ ★ **AC-16 的勾选仍 MUST 等 A6**，但**推进不再被 A6 阻塞**。

---

## 1. 当前状态快照（2026-10-26 实测，事实）

| 维度 | 实测状态 |
|---|---|
| 分支 | `feature/chmem-bp-btb-gshare` · **37 commits 领先 main**（未 push） |
| 已落地 | 前置1 + 前置2 **已归档** · 增量 1 已提交（`2703ca8`）· plan 同步（`8f49cce`）· 规格侧（`a5c6dc3` / `9fd14dc` / `758a52b`） |
| 工作树 | **4 文件未提交 = 增量 3（GShare）实现 + 括号修正**（`branch_predictor_chmem.h` / `cpu_factory_chmem.h` / `cpu_verilator_sim.cpp`，**+160/−17** 后追加 `:539` 括号修正）⇒ ★ **已 build（0 error）/ 已回归** ⇒ 实测见 **§1.3** |
| ★ 已修确定性缺陷 | ★★ `branch_predictor_chmem.h:539-541` 的 `}` 提前闭合 `update()` ⇒ GShare 块变类作用域裸语句块 ⇒ **必然编译失败** ⇒ **2026-10-26 已修**（`update()` 现在包住 GShare 块，块内缩进 +2）⇒ build **0 error** |
| 门禁 | **8/8 PASS**（`verify_adr` · `verify_plugin_decision` · `check_plugin_portability` · `doc_link_check` · `roadmap-r9-check` · `check_xlen_hygiene` · `check_tlm_freeze` · `check_coverage_parity_ledger`） |
| openspec | `openspec change validate chmem-bp-btb-gshare` ⇒ **valid** |
| 后台 ctest（增量 1 二进制） | ★ **已收尾**（`EXIT=8`，总 **3022.58s**）：`chipforge_tests_chmem` **2855.99s PASS** · `cpu_verilator_sim_add` 18.15s PASS · `branchbench_baseline` 70.79s PASS · `branchbench_bp_direction` 58.71s PASS · `verilator_perf_baseline` **FAIL（预期内）** |
| perf 基线 | ★ **陈旧**：实测 `cycles=820513` / `dmips_per_mhz=1.38731` vs 基线 `846500` |
| ★ 新事实 | `chipforge_tests_chmem` 实测 **2856s**（AGENTS.md 记 833s）⇒ **3.4×** ⇒ 收口 / 全量回归按 **~50min** 排 |

### 1.1 增量 1 的实测（双负载，2026-10-26）

| 配置 | DHrystone cycles | FLUSH_COUNT | 净收益率（分母 = FLUSH(off)，**报告项**） | branchbench 4 档 cycles |
|---|---|---|---|---|
| BP=off | 914544 | 120065 | — | 86034 / 88034 / 86534 / 87056 |
| BP=on，增量 1 关（= PoC 行为） | 846500 | 52021 | 56.67% | 84038 / 84040 / 84787 / 85037 |
| **BP=on，增量 1 开** | **820513** | **26034** | **78.3%** | **78041 / 78043 / 78791 / 79041** |

⇒ **ΔFLUSH ≡ Δcycles ≡ 25987** · 校验和**逐位不变**（103/183/147/101）· vendored ELF 47/47 · 单元锚 14 case / 205 assertions
⇒ ★★ **实测更正（已落 3 处文档）**：「branchbench 上 `jal` 覆盖收益**结构性为零**」是**错的** —— 静态分析**只数了 `jal`，漏了 `j`**（`j` = `jal x0` ⇒ 同 opcode）⇒ 判据 = **branchbench + DHrystone 双证**
⇒ ★ **AC-27 的双分母当前不可生成**：实测 `--stats` 只输出 `BP_STATS hit= miss= pred_taken=`（`tools/verilator_runner/cpu_verilator_sim.cpp:381`），且 plugin 侧 `taken_count_` 计的是 **pred_taken**（`branch_predictor_chmem.h:733`）⇒ ★ **actual-taken（`N_taken_total`）计数器全仓不存在** ⇒ 双分母需要**新代码**（见路径 1.2 Step 1b），不是填表动作

### 1.2 距绝对值门禁的**可判定**差额（v3 复算）

| 量 | 值 | 推导 / 校验 |
|---|---|---|
| 门禁 | `DMIPS/MHz ≥ 1.4` | `tools/measure_perf.sh:55` 的 `dmips = RUNS×1e6/(1757×cycles)`，`RUNS=2000`（`:29`） |
| 等价 cycles 阈值 | **813074** | `2e9/(1757×1.4) = 813074.2337` ⇒ **整数阈值 813074**（★ `dmips@813074 = 1.400000`，`dmips@813109 = 1.399940` ⇒ ★ **MUST 用整数比较，不用 dmips 打印值**） |
| 实测 cycles | 820513 | 2026-10-26（增量 1 定稿） |
| ★ 增量 3 须再省 | **≥ 7439 拍（0.91%）** | `820513 − 813074`；★ 与 `tasks.md:52/61` 的「≈7440」一致 |
| ★ 三口径数值重合 | `914544 − 101470 = 813074` | AC-16 的「差距闭合率」精确口径 `101470/120066 = 84.5119%` ⇒ 闭合后 cycles **恰为 813074** ⇒ ★ **「84.5% 净收益率」与「DMIPS 1.4」在数值上是同一条线** ⇒ **A6 的分母之争不影响推进判定**（只影响报告口径） |

### 1.3 ★★ 增量 3 现状实测（**修复前基线**，2026-10-26 路径 0 结果）

| 步骤 | 结果 | 说明 |
|---|---|---|
| 0.2 build | ✅ **0 error** | ★★ **MUST build 两个 target**：`chipforge_tests_chmem` **和** `cpu_verilator_sim` ⇒ 只 build 前者会**静默用旧 runner** 跑 0.4/0.5（本次已踩：旧 runner 回显无 `bp_gshare=`、`--no-bp-gshare` 直接报错退出） |
| 0.3 单测 | ✅ `[bp][plugin]` 14 case / 205 assertions · `[bp][pcmux]` 12 case / 143 assertions | 与增量 1 基线**逐位一致** ⇒ ★ 但 R15–R19 未写 ⇒ 对 GShare 机制**零覆盖**（旧锚全绿 ≠ GShare 正确） |
| 0.4 `--check-baseline` | ✅ PASS | 4 档 `bp=off` 与基线一致 ⇒ BP=off 路径零退化 |
| 0.4 `--check-bp-direction` | ❌ **FAIL** | `mode 2 pattern_6of8: pred_taken=9737 > 0.9×hit=8989 ⇒ 方向学习失效`（GShare off 时该档 = `8243/9988 = 0.825` ⇒ PASS）⇒ ★ **GShare 把 `pred_taken` 推到 ≈ `hit`** |
| Dhrystone | ❌ **净负收益** | `gshare=on` **822514** vs `gshare=off` **820513** ⇒ **−2001 拍（−0.24%）** ⇒ 距门禁 813074 还差 **9440 拍** |
| 0.5 vendored ELF | ⏳ **须用新 runner 重跑**（旧 runner 的结果无效；单 case ≈ **26m34s**） | 新 runner 的跑批已启动 |

⇒ ★★ **结论（BLOCKER-1 的实证）**：当前 GShare 实装对**两个负载都是负贡献**（Dhrystone −2001 拍；branchbench 方向学习判据直接 RED）⇒ 与「更新表项 ≠ 预测表项 ⇒ 计数器被污染成随机 true ⇒ `gshare_taken` ≈ 恒真」的机制预测**一致** ⇒ ★ **1.1 的索引对齐 + `ghr_en` 收窄是增量 3 的入场券，不是优化项**。

---

## 2. 路径 0：解除当前阻塞（**先做**）

| 步 | 动作 | 命令 / 完成判据 | 归属 |
|---|---|---|---|
| 0.0 | ★★ **备份增量 3 的唯一副本** | 已做：`git diff > /tmp/opencode/gshare_inc3.patch`（328 行）+ `git stash create` = `f0cd333` ⇒ ★ **MUST NOT** 直接 `git stash` / `git checkout -- .` | builder |
| 0.0b | ★ **本文件与 plan 的版本化**（两者均为未跟踪/未提交时的唯一副本） | `git add .rddf/plans/chmem-bp-btb-gshare-increments.md .rddf/plans/chmem-bp-btb-gshare.md` ⇒ ★ **提交前须用户确认**；★ patch 另存一份到仓库外持久位置 | builder |
| 0.1 | ~~等后台 ctest 结束~~ ✅ **已完成** | 日志尾 `EXIT=8`（4 passed / 1 预期 FAIL） | builder |
| 0.2 | ✅ **已完成** —— 修括号 + build **两个 target** | 首修：`branch_predictor_chmem.h:539-541` 的 `}` 移到 GShare 块之后（块内联进 `update()`，缩进 +2）⇒ `cmake --build build --target chipforge_tests_chmem -j$(nproc)` **0 error** + `cmake --build build --target cpu_verilator_sim -j$(nproc)` **0 error** ⇒ ★★ **两个 target 都 MUST build**（只 build 前者 ⇒ 0.4/0.5 静默用旧 runner，本次已踩） | builder |
| 0.3 | ✅ **已完成** | `[bp][plugin]` **14/205** · `[bp][pcmux]` **12/143**（★ 与增量 1 基线一致 ⇒ 旧锚不覆盖 GShare） | builder |
| 0.4 | `--check-baseline` ✅ PASS / `--check-bp-direction` ❌ **FAIL（mode 2）** | FAIL 是**真回归**（不是基线陈旧）：`pred_taken=9737 > 0.9×hit=8989` ⇒ ★ **MUST 由 1.1 修好后重跑转绿**，MUST NOT 当「误红」放过 | builder |
| 0.5 | ⏳ **新 runner 重跑中**（旧 runner 结果无效；单 case ≈ 26m34s） | 判据：47/47（16410 assertions） | builder |

---

## 3. 路径 1：增量 3（GShare）落地 ⇒ ★ 决策点 ★

| 步 | 动作 | 判据 | 归属 |
|---|---|---|---|
| 1.0 | ★★ **D8 + D10 签批**（★ 两者都是 1.1 的前置，见 §9） | `tasks.md` AC-12 + AC-25 改写（D8）；GShare 索引口径定案（D10） | architect |
| 1.1 | ★★ **GShare 机制补齐（三件套，缺一则增量 3 无意义）**：① **索引时序对齐**（`ghr_snap_`）② **GHR 驱动源收窄**（去掉 `jal`，见 §3.1）③ **观测口**（3 个新 payload，见 §3.2） | 见 §3.2 的 R15–R19；M16–M20 **每个 MUST RED** | builder |
| 1.2 | **量收益（4 组合 + 归因护栏）** | Step 1b 先补 `N_taken_total` 导出；Step 2 `./build/bin/cpu_verilator_sim --elf build/dhrystone.elf --cycles 5000000 --stats`（叠加 `--no-bp` / `--no-bp-uncond` / `--no-bp-gshare`）+ branchbench 4 档 ⇒ 落表（uncond × gshare，**含双分母列 + 同 build `--no-bp` 对照行**） | builder |
| 1.3 | ★ **决策点（D9）** | `cycles ≤ 813074` ⇒ **达标** ⇒ 走 1.5 + 路径 4.1；**不达标** ⇒ 1.4；★ **裁决表 MUST 4 行齐全**（on/off × uncond/gshare），缺行 ⇒ **不裁决**（防非 BP 漂移误判） | architect 裁决 + builder 执行 |
| 1.4 | （条件性）**增量 2 = RAS 8-entry** | `jalr`/`ret` 覆盖 ⇒ ★ **TLM 无参考实现 ⇒ 全新设计** ⇒ **只在 1.3 不达标时做** | builder |
| 1.5 | 落文档 | `tasks.md` Task 7 Step 4 表 + 本文件 §1 + feature fragment + objective §2.1 ⇒ 数字逐位对齐；★ **勿再复发「branchbench 结构性为零」类错判** | builder |

### 3.1 ★★ GShare 移植口径（v3 修订）

**移植来源**：TLM 参考 `ip/cpu/plugins/branch_predictor.{h,cpp}` 的 `gshare_predict` / `gshare_update` / `global_history_`（`GSHARE_SZ=16` / `GHR_BITS=8` / 索引 `(pc ^ ghr) & (kGshareSize-1)`）⇒ ★ **mask 在 XOR 之后 ⇒ 有效历史位数 = 4 bit**（★ 历史文档的「13-bit」与代码不符 ⇒ 以代码为准）。

**★ 索引时序错位（承重要求，D10）**：

| 环节 | 位置 | 用的 PC | 用的 GHR |
|---|---|---|---|
| 预测（fetch NORMAL） | `branch_predictor_chmem.h:208` | `BP_PC` = `pc_reg` **本拍**（`ibus_chmem.h:260`） | `ghr_val` = **本拍**值（`:660-663`） |
| 更新（branch NORMAL） | `branch_predictor_chmem.h:212` | `KT::PC` = `pc_lag` = **上一拍** `pc_reg`（`ibus_chmem.h:249`） | `ghr_cur` = **本拍**值（`:559-562`） |
| GHR 移位 | `branch_predictor_chmem.h:592-593` | — | `ghr_next = (ghr_cur << 1)` OR `taken` |

⇒ `ibus_chmem.h:149-150` 亲口写明「本拍 `update_pc` 用到的预测记录 = **上一拍** fetch 产出的；被解析的指令 = `pc_lag` = 上一拍 fetch 的那条」⇒ **同一条分支的预测与更新相差 1 拍** ⇒ GHR 在此期间已移位 ⇒ **更新的表项 ≠ 预测读的表项** ⇒ GShare 计数器在被读的那一项上学不到（甚至被污染成随机 true ⇒ 负收益）。

⇒ ★★ **论证口径更正（v3，oracle 审查）**：★ **不得**用「TLM 是同拍调用 ⇒ 现实现偏离参考」作理由 —— 实测 TLM 参考的 predict 挂 **fetch NORMAL**（`branch_predictor.h:121-131`）、update 挂 **execute LATE**（`:134-144`），且 predict 结果在 TLM 流水线里**未被消费**（`:127-129`）⇒ TLM 的闭环性只在**单元测试口径**下成立。★ **正确理由 = 「读写同一表项」是 GShare 兑现收益的**内在要求**（读改写不闭环 ⇒ 训练永远落在别的项上）⇒ 与参考是否同拍无关。

⇒ ★ **修法（MUST）**：`ghr_snap_`（`ch_reg<ch_uint<8>>`）在 fetch NORMAL 写 `<本拍 ghr>`；update 侧索引改读 `ghr_snap_` 的**当前值**（= 上一拍 GHR = 预测该分支时的 GHR）。★ 该模式与既有 `pred_*_buf_`（`ibus_chmem.h:146-168`，同为「fetch 写、下一拍 branch 读」的 1 拍快照）**同构**，已由 R11–R14 验证。

⇒ ★ **GHR 驱动源 MUST 收窄为条件分支**（v3 新增）：`ghr_en = is_branch & !flushed`（★ **去掉 `jal_en`**）。理由：① TLM 参考的 `update()` 只在 `OpClass::BRANCH` 时被调用（`branch_predictor.h:138`）⇒ `jal`/`jalr` **从不**进 GHR；② 现实现把 jal 恒移入 1 ⇒ **无信息量的恒定比特**稀释本就只有 4 bit 的有效历史；③ ★ **会破坏 1.2 的 2×2 正交性**（`enable_uncond_` 开关同时改变 GHR 构成 ⇒ 「gshare 的收益」混着「GHR 构成变化」）⇒ 归因被污染。

⇒ 其余两处**实施层口径差异已登记**（非缺陷）：GShare PHT 写使能 = `is_branch & !flushed`（与 Bimodal 同款）；GShare PHT 初值 0（TLM 为 `WEAK_NOT_TAKEN=1`）⇒ ★ 「忠实移植」注释须限定「**除初值外**」（0 初值与 AC-8b-1 的 W=2 暖机推导一致）。

### 3.2 ★★ 单元锚清单（R15–R19 / M16–M20，v3 补齐定义）

> ★ v2 只引用了 R15–R17 / M16–M18 的**名字**（遗产自外部草稿），repo 内**无定义** ⇒ builder 会自由发明。此处补齐。落点文件 = `tests/cpu/test_cpu_chmem_bp_plugin.cpp`（与 R11–R14 同文件）。

**③ 观测口（R15–R19 的前置，必须先落）**：三个新 payload（**不**改 private 访问层级；★ 现有 `get_gshare()`/`get_ghr()` 在 `private:` 之后 ⇒ 测试不可达）

| payload | 写入位置 | 类型 | 语义 |
|---|---|---|---|
| `BP_GSHARE_IDX` | fetch NORMAL | `uint_t<8>`（值域 0..15） | 本拍**预测**索引 |
| `BP_GSHARE_UPD_IDX` | branch NORMAL | `uint_t<8>`（值域 0..15） | 本拍**更新**索引 |
| `BP_GHR` | fetch NORMAL | `uint_t<8>` | 本拍 GHR 当前值 |

**锚清单（落 `tests/cpu/test_cpu_chmem_bp_plugin.cpp`）**：

| 编号 | 断什么 | 判据形式（可判定） | 变异（MUST RED） |
|---|---|---|---|
| R15 | 索引 = `pc ^ ghr`（断**值**） | `BP_GSHARE_IDX` 逐拍 == `pc ^ BP_GHR` 的期望值 | M16 = 索引去 XOR（改纯 pc） |
| R16 | GHR 跨拍保持 + 左移 + 最低位 = taken | `BP_GHR` 逐拍序列（如 0 → 1 → 3 → 7 的 taken 序列） | M17 = `ghr_en` 恒 false |
| R17 | `enable_gshare_=false` ⇒ 方向恒 false | 关闭态的 `pred_taken` 与「Bimodal-only 参照」逐位相同 | M18 = 开关恒 true |
| ★ R18 | ★★ **读改写闭环**：`upd_idx(t) == pred_idx(t−1)` | ★ 恒 PC（`fetch_pc=0` ⇒ 用 `br_pc_`）+ 每拍 `is_branch=1 & taken=1`（GHR 每拍必移位）⇒ 逐拍断言 `BP_GSHARE_UPD_IDX(t) == BP_GSHARE_IDX(t−1)`（★ 现有夹具即可构造：`Phase{end_tick,taken,flushed,fetch_pc,is_branch,is_jal}`） | ★ M19 = update 索引改用 `get_ghr()` 当前值（**= 现实现**） |
| ★ R19 | ★ **GHR 驱动源 = 条件分支**（`jal`/`j` 不移位） | 驱动 `is_branch=0 & is_jal=1` 的段 ⇒ `BP_GHR` 逐拍**不变** | ★ M20 = `ghr_en = (is_branch OR jal_en) & !flushed`（**= 现实现**） |

★ **为什么放弃 v2 的 R18 写法**（「X 连续 taken 3 次 ⇒ 计数器 ≥2」）：① 索引 mask=15 ⇒ GHR 低 4 位 4 拍内饱和 ⇒ 稳态下修复版与 M19 的索引**重合** ⇒ **M19 不 RED**；② 瞬态期每次预测索引都是「先读后写」⇒ 计数器恒 0 ⇒ **修复版也不 ≥2** ⇒ 该判据与「M19 MUST RED」**不可同时成立**。⇒ 改「逐拍索引等价」后两个方向都可判定。

---

## 4. 路径 2：architect 裁决

| 步 | 事项 | 落点 / 时限 | 归属 |
|---|---|---|---|
| 2.1 | **A6**：净收益率的分母口径 | ADR-088 §5.3.2 + 简报 §5；★ **只阻塞 AC-16 勾选**（推进判据已与 cycles 数值重合，见 §1.2）；触发 = 进归档流程；时限 = 归档前 | architect |
| 2.2 | A1 / A3 / A4 签批 | 实施侧取值已被 PoC 的 R8 / R11 锁死；时限 = 归档前 | architect |
| 2.3 | A7：简报整体回写 v2.3 | 简报 + `adr.md`；`python3 tools/check_md_tables.py docs/architecture/*.md` ⇒ 全 OK | architect |
| 2.4 | A2：AC-F2-3 分量 1 阈值重校准 | 与 A6 同批落 | architect |
| 2.5 | ★ **D8 / D9 / D10 签批 + 抄进裁决摘要** | 见 §9；★ **归口**：`.rddf/improvements/bp-spec-rewrite-and-doc-hygiene.md` 的裁决摘要归 **planner/architect** 维护（builder 不越界改）⇒ ★ 不抄进去 ⇒ architect 签 A1~A7 时**看不到** D8~D10 | architect |
| 2.6 | ★ **AC-27 的勾选态处置** | `tasks.md:117` 现为 `[x]`，但其 fail-closed 条款（双分母取小）**未满足**（计数器不存在）⇒ 由 architect/verifier 决定**退回未勾选**或加「判据句待 D9」挂起标注 ⇒ 时限 = 1.2 完成前 | architect |

---

## 5. 路径 3：② 主体收口（路径 1 决策后）

| 步 | 事项 | 判据 | 归属 |
|---|---|---|---|
| 3.1 | **trap/freeze 占位臂**（AC-22） | grep pattern MUST 匹配 `select(\s*trap_redirect`（注释不命中）+ 占位臂精确形态（4 具名信号 + 常 false/0 + trap 在最顶层）+ 「占位臂惰性」单元锚 ⇒ ★ **MUST NOT** 写假 trap 项过 grep | builder |
| 3.2 | **台账 14 条逐条重判** | 先落裁决包的 **AC-A9**（简报 §5.1 #11 `not-covered` vs 台账 `:270` `partial` 对齐）→ 条目态 + §2 族级判定行 + **计数同步**（头部 partial **292 → 282**，= 裁决包 **AC-A10**）⇒ `tools/check_coverage_parity_ledger.sh` PASS | builder |
| 3.3 | **ADR-088 落两个结论** | §5.3.1 根因修订（`FLUSH(off)=120065` vs TLM 差距 `120066`；完美预测上限 794479 vs TLM 794478）+ §5.3.2 的净收益率结论与绝对值门禁结论（**分两件事**） | architect |
| 3.4 | 勾选 AC | ★ **AC-16 须等 2.1**；AC-22/23/24/25/26/27/28/29 逐条核 ⇒ ★ **AC-25 依赖 D8 已签批**（否则「禁 GShare」与已落地的增量 3 直接冲突）；★ **AC-27 依赖 1.2 的双分母真实落表** | builder |
| 3.5 | 收口 | ★ **前置 = 4.2 → 4.1 已完成**（否则「全量 ctest 0 退化」不可达）⇒ 8 门禁 + 全量 ctest（预留 ~50min）+ `openspec archive chmem-bp-btb-gshare -y --skip-specs` | builder |

---

## 6. 路径 4：基线与门禁

| 步 | 事项 | 判据 | 归属 |
|---|---|---|---|
| 4.2 | perf CSV 加列（缺口 #13） | ★ **一并加 `bp_mode`**（branchbench CSV 有、perf CSV 缺 ⇒ 否则增量 2 落地即再次陈旧）；★ 同时把 `tools/measure_perf.sh:62-69` 的**位置取列**（`$5`/`$7`）改为**按表头名取列**（须知 #5 强制）；缺列 ⇒ fail-closed；★ **次序：4.2 先于 4.1**（schema 先行，否则基线要重录两次） | builder |
| 4.1 | ★ **perf 基线重录** | ★ **判据去同义反复**：`git diff <CSV>` **仅含预期的数值单元格** + commit 说明重录原因 + **另一次干净运行**下 `--check-baseline` PASS（★ 不得以「重录后立刻 check 通过」当成判据）；★ 等增量 3 定稿后做 ⇒ **次序硬约束：1.3 → 4.2 → 4.1 → 3.5** | builder |
| 4.1b | ★ **branchbench CSV 追加 `bp_mode=on` 行**（0.4 误红的出口） | 增量 3 定稿后追加 `bp_mode=on` 基线行（按 `(workload_sha, bp_mode, build_id)` 分组键）⇒ 之后 `--check-bp-direction` 才有可比基线 | builder |
| 4.3 | `tools/check_md_tables.py` 接进门禁（现为孤儿工具） | 并入 `tools/doc_link_check.sh` 或 `verify_adr.sh` ⇒ AC-9 门禁清单 8 → **9 条**；★ **须早于/同 commit 于** 1.5/3.x 的手写表格 | builder |
| 4.4 | ★ **critic 复审**（机制实现 + 增量 1/3 从未过 critic） | 4 段式；★ 用 6 字段上下文（`intent` / `focus` 必填）；★ 要求 critic 独立复算 `813074` 与复核 R18 的可判定性 | critic |
| 4.5 | ★ **变异证据入库** | 把 `/tmp/opencode/pcmux_mut/run3.sh` 的变异配方落进 `tests/cpu/test_cpu_chmem_bp_plugin.cpp` 注释或本文件 ⇒ ★ **MUST NOT** 只留 `/tmp` 引用 | builder |

★ **双 ownership 归口（v3）**：4.2 / 4.3 与 `.rddf/improvements/bp-spec-rewrite-and-doc-hygiene.md` 的 What Changes #3/#5（`AC-A5` / `AC-A7`）是**同一批改动** ⇒ ★ **CSV schema 与工具改列归该 improvement change 先落，主体只追加行** ⇒ 二选一，不许两边都改（避免同函数冲突）。

---

## 7. 路径 5：并行线与退役链（★ 不阻塞 BP 主线）

| 步 | 事项 |
|---|---|
| 5.1 | `mmu-chmem-pipeline-integration`（144 未完成）· `mmu-chmem-phase-b-c-launch`（23）· `phase-1.5-wave-4`（51）· `vexii-riscv-parity-poc`（74） |
| 5.2 | B2 语义裁决（ADR-088 §3 五子系统逐条表态）→ **G3.2 DMIPS 判据改绑 Verilator 后端并重测** → B3 删除（CppTLM 链接归零 · TLM-only 文件归零 · `run_chipforge_tests.sh` 默认入口改指 CH_MEM）⇒ 前置：**CI 具备 build 能力**（现 CI 不构建 ⇒ 门禁 step 恒 SKIP ⇒ 零保护） |

---

## 8. 关键路径与决策树

```
路径 0（0.0 备份 → 0.0b 版本化 → 0.2 修 :539 括号 → build → 0.3 单测 → 0.4/0.5 回归）
   └─► 路径 1（1.0 D8+D10 签批 → 1.1 三件套(对齐/收窄/观测口) + R15–R19/M16–M20
                → 1.2 量收益(含 N_taken_total 导出 + 4 行归因) → ★1.3 决策点★）
          ├─ cycles ≤ 813074 ──► 1.5 ──► 4.2 → 4.1 → 4.1b ──► 路径 3 ──► 4.4 critic ──► 归档
          └─ cycles > 813074 ──► 1.4（RAS · 全新设计）──────────────────► 同上

路径 2（A6 / A1-A4 / A7 / D8-D10）★ 并行；D8 卡 Check 14，A6 卡 AC-16，D9 卡 1.3，D10 卡 1.1
路径 4   ★ 次序：4.3 早于手写表格；4.2 → 4.1 → 4.1b；4.1 夹在 1.3 之后、3.5 之前
路径 5   ★ 完全独立
```

---

## 9. ★ 待 architect 签批（D8 / D9 / D10）

> ★ **命名说明（v3）**：原 v2 用 A8/A9/A10 ⇒ 与裁决包的验收项 **`AC-A8` / `AC-A9` / `AC-A10`**（零代码改动 / 简报与台账对齐 / 头部计数 292→282）**撞车** ⇒ 改用 **D 前缀**。★ 签批面 MUST **并入** `.rddf/improvements/bp-spec-rewrite-and-doc-hygiene.md` 的裁决摘要表（否则 architect 签 A1~A7 时看不到）。

| 编号 | 事项 | 建议裁决 | 触发 / 落点 / 时限 | 不裁决的后果 |
|---|---|---|---|---|
| **D8** | AC-12 / AC-25 的「不做什么」机检 scope 与 A5 冲突 | 禁令对象从「GShare/GHR/RAS 标识」改为「**未落地的增量**」⇒ 当前只禁 **RAS**；机检形式（脚本化、非 tree-wide）不变；★ **allowlist 纪律**：白名单 = `check_plugin_portability.sh` Check 14 内的**具名变量**，修改它 MUST 与对应增量落地**同 commit** 且 commit message 引裁决编号 ⇒ 防「自己改白名单 = 自证」退化 | 触发 = Check 14 实现前 / 落点 = `tasks.md` AC-12 + AC-25 + `check_plugin_portability.sh` / 时限 = **Check 14 实现前，且不晚于 3.4 勾选 AC-25** | ★ 按 AC-25 字面实施 Check 14 ⇒ **本 change 自己红**（PR 阻塞门禁） |
| **D9** | 增量分档的推进判据用哪个量纲 | **`Dhrystone cycles ≤ 813074`**（整数比较）= DMIPS 1.4；★ 与 AC-16 的 84.5%（精确 `101470/120066`）**数值重合**（`914544 − 101470 = 813074`）⇒ 分母之争不影响推进判定；净收益率降为报告项 | 触发 = 1.2 量收益完成 / 落点 = 本文件 §1.2 + `tasks.md` Task 7 Step 6 **+ AC-27 的判据句** / 时限 = 增量 3 定稿前 | ★ 判据悬在 A6 的「不同量纲 84.5%」⇒ 决策点**不可判定** |
| **D10** | GShare 索引口径 + GHR 驱动源 | 索引用**预测期** GHR（`ghr_snap_` 1 拍快照）；`ghr_en = is_branch & !flushed`（**不含 jal**，与 TLM `branch_predictor.h:138` 一致）；★ 理由用「**读写同一表项是 GShare 的内在要求**」，**不得**用「TLM 同拍」（TLM 是 fetch NORMAL vs execute LATE，且 predict 结果未消费） | 触发 = 1.1 开工前 / 落点 = 本文件 §3.1 + 主 plan Task 2f Step 2b/4b / 时限 = 增量 3 定稿前 | ★ 增量 3 收益被机制性压低 + 2×2 矩阵非正交 ⇒ 归因污染 ⇒ **误触发 1.4（RAS，最贵）** |

---

## 10. 三条纪律提醒

1. ★ **GShare 代码当前不可编译**（`:539-541` 的 `}` 错位）+ 未验证 ⇒ **不可声称已实现**；备份见 0.0，**不要** `git checkout -- .` 或裸 `git stash`。
2. ★ **perf 基线 846500 已陈旧**（实测 820513 / DMIPS 1.38731）⇒ 全量 ctest 必 FAIL 1 项（预期内）⇒ 次序 `1.3 → 4.2 → 4.1 → 4.1b → 3.5`。
3. ★ **A6 未裁决前不勾 AC-16**；★ 但增量分档的**推进**不依赖 A6（D9：与 cycles 数值重合）⇒ 实施者**不自改门禁值**。

---

## 11. 修订记录

### v1 → v2（2026-10-26 builder 审查）

| # | 级别 | 问题 | 落点 |
|---|---|---|---|
| 1 | BLOCKER | GShare 更新索引用更新期 GHR ⇒ 读改写不闭环；v1 的锚断不到 | §3.1 + 1.1 + D10 |
| 2 | BLOCKER | AC-12 / AC-25 要求 Check 14 禁 GShare —— 与 A5 自相矛盾 | 1.0 前移 + D8 |
| 3 | SHOULD | 0.2 的 `git stash` 回退会毁掉唯一副本 | 0.0 |
| 4 | SHOULD | 1.3 判据量纲待 A6 ⇒ 不可判定 | 1.3 改 cycles + §1.2 |
| 5 | SHOULD | 量收益表缺 `N_taken_total` 分母 | §1.1 + 1.2 |
| 6 | SHOULD | 仓库 plan 头部与 AC-1 是 v2.1 陈旧文 | 主 plan 已改 |
| 7 | SHOULD | 变异模板在 `/tmp` 不可复现 | 4.5 |
| 8 | SHOULD | 未记全量套件 3.4× 变慢 | §1 + 3.5 |
| 9-11 | NIT | 0.4 误当已验 / 3.5←4.1 顺序未画 / 双轨草稿 | 0.4 + §8 + 头部 |

### v2 → v3（2026-10-26 oracle + metis 双审）

| # | 来源 | 发现 | v3 落点 |
|---|---|---|---|
| 1 | oracle | ★ **阈值算错 35 拍**：`2e9/(1757×1.4) = 813074.23` ⇒ 整数阈值 **813074**（`dmips@813109 = 1.39994 < 1.4`，会把 813080~813109 误判达标） | §1.2 / Goal / §8 / 主 plan 4 处 |
| 2 | oracle | ★ **`branch_predictor_chmem.h:539-541` 的 `}` 提前闭合 `update()`** ⇒ **必然编译失败**（v2 写成「编译不过再修」的或然措辞） | §1 + 0.2「已知首修项」 |
| 3 | oracle | ★ **「TLM 是同拍 ⇒ 现实现偏离参考」论证不实**（TLM 是 fetch NORMAL vs execute LATE，且 predict 未消费）⇒ 改用「内在要求」 | §3.1 + D10 |
| 4 | oracle | ★ **`ghr_en` 含 jal 是未登记偏差** + 破坏 2×2 正交性（TLM 只对 BRANCH 更新） | §3.1 + R19/M20 |
| 5 | metis | ★★ **R18 原写法不可判定**（GHR 4 拍饱和 ⇒ M19 不 RED；瞬态期修复版也不 ≥2）⇒ 改「逐拍索引等价」 | §3.2 R18 |
| 6 | metis | ★ **观测口不可达**（`get_gshare()`/`get_ghr()` 在 `private:` 之后）⇒ 先落 3 个 payload | §3.2 ③ |
| 7 | metis | ★ **`N_taken_total` 计数器全仓不存在**（`BP_STATS` 只有 hit/miss/pred_taken；`taken_count_` 计 pred_taken）⇒ 双分母要新代码；AC-27 却已 `[x]` | §1.1 + 1.2 Step 1b + 2.6 |
| 8 | metis | ★ **A8/A9/A10 与 `AC-A8/A9/A10` 撞车** ⇒ 改 D 前缀 + 要求抄进裁决摘要 | §9 + 2.5 |
| 9 | metis | R15–R17 / M16–M18 **只被引用未定义**（遗产自外部草稿） | §3.2 定义表 |
| 10 | metis | 0.3 钉死 14/205 + 12/143（R15–R19 落地即失效） | 0.3 改「以 `--list-tests` 为准」 |
| 11 | metis | 4.1 判据同义反复（重录后 check 恒 PASS） | 4.1 改「CSV diff 仅预期单元格 + 干净复跑」 |
| 12 | metis | 4.2 缺 `bp_mode` 列 ⇒ 增量 2 落地即再陈旧；且 4.2 须先于 4.1 | 4.2 + §8 |
| 13 | metis | 4.2/4.3 与 improvement 的 `AC-A5`/`AC-A7` 双 ownership | §6 归口段 |
| 14 | metis | 0.4 的 `--check-bp-direction` 误红无出口（CSV 只有 `bp_mode=off` 行） | 0.4 + 4.1b |
| 15 | metis | 0.0 备份不覆盖未跟踪文件（本文件本身 untracked） | 0.0b |
| 16 | metis | D8 的「触发」与「时限」自指循环（都是「Check 14 实现前」） | §9 D8 增阶段上界 |
| 17 | metis | 「见 §6 时间预算」悬空引用 | 已改为 §1 |

### v3 → v4（2026-10-26 路径 0 执行期发现）

| # | 级别 | 发现 | v4 落点 |
|---|---|---|---|
| 1 | BLOCKER（计划缺口） | 路径 0.2 只写 build `chipforge_tests_chmem` ⇒ **`cpu_verilator_sim` 是独立 target**，0.4/0.5 会**静默用旧 runner**（旧 runner 回显无 `bp_gshare=`、`--no-bp-gshare` 直接报错退出）⇒ 0.4/0.5 的「PASS」对增量 3 **无效** | 0.2 + §1.3 |
| 2 | ★ 实证 | 用**新 runner** 实测：Dhrystone `gshare=on` **822514** vs `gshare=off` **820513**（**−2001 拍**）；`--check-bp-direction` 在 `pattern_6of8` 档 **FAIL**（`pred_taken=9737 > 0.9×hit=8989`，GShare off 时 0.825 PASS）⇒ **GShare 现状 = 双负载负贡献** | §1.3 + 0.4 |
| 3 | 确认 | `:539-541` 括号为**唯一**编译阻断（修后 0 error）；0.3 旧锚 14/205 + 12/143 与增量 1 逐位一致 ⇒ 旧锚对 GShare **零覆盖** | §1.3 + 0.3 |

---

## 12. 引用

- ② 主体 change：`openspec/changes/chmem-bp-btb-gshare/{proposal,design,tasks}.md`（★ `design.md` §0 = v2.3/v2.3.1 覆盖声明，**master**）
- 主执行文件：`.rddf/plans/chmem-bp-btb-gshare.md`（Task 2f/2g/2h = 增量分档）
- 规格（待 A7 回写 v2.3）：`ip/cpu/docs/bp-btb-gshare-design-brief.md`
- ADR：`docs/architecture/adr/ADR-088-tlm-backend-retirement.md`（§5.3.1 根因 / §5.3.2 裁决 / P-6）
- 裁决包：`.rddf/improvements/bp-spec-rewrite-and-doc-hygiene.md`（A1~A7 + `AC-A1`~`AC-A10`；★ D8~D10 待并入）
- 路线图：`.rddf/roadmap/features/feat-bp-btb-gshare.md` · `.rddf/roadmap/objectives/objective-ip-cpu-tlm-backend-retirement.md` §2.1
- 工具：`tools/measure_branchbench.sh` · `tools/measure_perf.sh` · `tools/check_md_tables.py` · `tools/check_coverage_parity_ledger.sh`
- 备份（本 session）：`/tmp/opencode/gshare_inc3.patch` · stash `f0cd333`

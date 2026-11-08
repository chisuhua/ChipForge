# ADR-094（层次化装配契约）—— 决策台账

> ★ 本文件 = **决策台账**（过程日志载体）⇒ **不进 ADR 正文**。
> ★ 记录对象 = 「层次化 Plugin 架构 v2 / 架构文档修订方案」的**逐条议题讨论**与**用户裁决**。
> ★ 配套审稿记录 = [`ADR-094-hierarchical-assembly-contract-review.md`](ADR-094-hierarchical-assembly-contract-review.md)（三轮 critic 的 finding 与处置）。
> ★ 落盘人 = architect（2026-11-07）。

---

## 1. 用途与读法

| 项 | 说明 |
|---|---|
| ★ **读法** | §2 是**活表**（议题状态随讨论推进）；§3 起是**逐议题结论**（每条含「分析 → 外部裁决 → 用户裁决 → 闭合条件」四段）；★ **一个议题 = 一节**，新增议题按编号续写。 |
| ★ **落笔边界** | 本台账**不是** ADR ⇒ 台账里的「结论」只有**经用户裁决 + 落进 ADR + critic 通过**后才成为契约。★ 未裁决项一律显式标「未决」。 |
| ★ **两类条目** | ★ **签认项** = 证据已把选项收敛到一个，差用户签字；★ **裁决项** = 有两条以上互斥路径。★ 证据可闭合的**不列为待裁决**（避免把可由事实判定的事推给用户）。 |

---

## 2. 议题表（★ 更新至 2026-11-07，含方向变更带来的作废/新增/变形）

| # | 组 | 议题 | 类别 | 状态 | 阻塞落笔？ |
|---|---|---|---|---|---|
| **01** | A 签认 | ★ **顶层端口的 owner（归属组件）选谁** | 签认→裁决 | ★ **已决（2026-11-07）**：采纳 **(b) + 契约登记簿(i)** | — |
| 02 | A 签认 | §D3a.7 接口载体改写**取代 v1.12 裁决 B** | 签认 | 未决（**当前待议**） | ★ 是 |
| 03 | B 文档 | ADR-091 拆分落点（★ **已含子决策：登记簿契约落点**） | 裁决 | 未决 | ★ 是 |
| 04 | B 文档 | ADR-091 ↔ ADR-094 职责切分 + **G-1o 扩面表述重写**（★ 因 #01 变形） | 裁决 | 未决 | ★ 是 |
| 05 | B 文档 | 连带改动最小集（adr.md / parity-gate-contract / ADR-089 / ADR-092 / ADR-083 / ADR-093） | 裁决 | 未决 | ★ 是 |
| 06 | C 契约 | `attach_unit` 是否进框架 API（可选 vs MUST） | 裁决 | 未决 | ★ 是 |
| 07 | C 契约 | 「外部驱动输入集」的声明形态（单元声明 / 桥接声明 / 合取） | 裁决 | 未决 | ★ 是 |
| 08 | C 契约 | 端口纪律：r.11 标量端口 + r.12 单元前缀（★ **因 #01 降级为可选**） | 裁决 | 未决 | ★ 是 |
| 09 | C 契约 | r.8 禁止单元间 sibling 直连（保守约束） | 裁决 | 未决 | ★ 是 |
| 10 | D 粒度 | 首批成层清单 | 裁决 | 未决（→ tasks，**不进 ADR**） | 否 |
| 11 | D 粒度 | regfile 观测面形态 | 裁决 | 未决（→ tasks，**不进 ADR**） | 否 |
| 12 | D 粒度 | 「交付物 = 每实例一份 module，不支持复用」的措辞强度 | 裁决 | 未决 | ★ 是 |
| 13 | E 框架 | `PipeBuilder::build()` 幂等 + `built_` 置位时机（★ **因 #01 强化**：迁移期重复登记） | 裁决 | 未决 | 否（→ builder） |
| 14 | E 框架 | 门禁 Check 清单与强度（★ **因 #01 扩展**：端口必须登记 + root-scope 内禁 `<<=`） | 裁决 | 未决 | 否（→ builder） |
| 15 | E 框架 | `ch_state_machine` 在层 2 是否禁用（★ 需先做一次实证抽查） | 裁决 | 未决 | ★ 是 |
| 16 | F 上游 | 上游回执范围（U-1 / U-2 / U-4 / **U-5 新增**） | 裁决 | 未决 | 否 |
| ~~17~~ | F 上游 | ~~上游解耦（第三路）~~ | — | ★ **已作废**（#01 采纳登记簿 ⇒ 不需要上游改） | — |
| 18 | G 流程 | 落笔后是否立即切 builder 实施 + change 拆分 | 裁决 | 未决 | 否 |
| ★ **24** | **E 框架** | ★ **上游语义回归面**：`simulator_init.cpp:95` 指令表分流 + `io.h` 4 处方向守卫触发面（#01 派生 · architect 复核新增） | 裁决 | ★ **新增（2026-11-07）** | ★ 是 |
| ★ **25** | **C 契约** | ★ **无登记簿一侧（预载核）的解析路径 + 退役条件**（#01/#21 派生 · architect 复核新增） | 裁决 | ★ **新增（2026-11-07）** | ★ 是 |
| ★ **21** | **C 契约** | ★ **契约登记簿契约本体**（登记点唯一 / manifest 格式 / 两道闸 / allowlist 治理） | 裁决 | ★ **新增**（#01 派生） | ★ 是 |
| ★ **22** | **E 框架** | ★ **C-1 三态边界 + `kAmbiguous` 上移 + 两阶段迁移顺序** | 裁决 | ★ **新增**（#01 派生） | ★ 是 |
| ★ **23** | **C 契约** | ★ **root-scope 窄口径纪律 + root 名冻结** | 裁决 | ★ **新增**（#01 派生） | ★ 是 |

---

## 3. ★ #01 议题讨论结论（顶层端口的 owner 选谁）

### 3.1 议题表述

顶层端口（`pb.declare_in/out` 创建的那些 = parity 契约端口 `reg_1..reg_31` / `open_pc` / `mem_sig` / `mem_sig_updates` / `tohost_probe_data_proxy`）在 ctx 里的 **owner（归属组件）** 是谁？

| 分支 | 内容 |
|---|---|
| **(a)** | 无归属（`owner == nullptr`）—— 现状 |
| **(b)** | 归 **root 组件**（ChipForge 新增的组件树根） |
| **(c)** | 改造上游：节点挂 owner 但名字**不加**路径前缀（owner/命名解耦） |

★ owner 同时决定两件事 ⇒ 不是实现细节：
1. **节点名**：`CppHDL/src/core/lnodeimpl.cpp:33-37` —— owner 非空 ⇒ `name_ = parent_->name() + "." + name`（**无条件前缀**）；
2. **层次化 module 归属**：`CppHDL/src/hierarchy_analysis.cpp:184-198` —— owner==nullptr ⇒ 归 root module；否则归最近成层祖先。

### 3.2 决定性证据（★ 均为实测/读码）

| # | 证据 | 结论 |
|---|---|---|
| **E1** | 契约名解析 = **精确相等**：`tools/parity_runner/parity_sim_runner.cpp:249-255`（`node->name() == name`） | ★ 任何前缀 ⇒ **直接读不到** |
| **E2** | 两侧现在都无前缀：预载核**完全不用** Component（`ip/cpu/cpu_factory_chmem.h` 无 `ch_device`/`Component` 引用）⇒ 其观测节点在 PipeBuilder elaboration 作用域创建 ⇒ owner==nullptr；开放核观测端口走 `pb.declare_out` ⇒ owner==nullptr | ★ 契约名在**两侧同时**成立 |
| **E3** | ⇒ **(b) 会单侧破坏 parity**：开放核侧变 `cpu.reg_1`、预载核侧仍是 `reg_1` ⇒ 精确匹配落空 ⇒ H-3 的 3 个终值观测量读不到 | ⇒ 要走 (b) 必须**同时**改预载核或**改解析机制**（后者 = 下面的 X 轴） |
| **E4** | **(b) 没有「空名前缀」后门**：`build_hierarchical_name`（`include/component.h:266-280`）对空/unnamed 父路径**会跳前缀**，但**节点名不走它** —— `lnodeimpl` 无条件加前缀 ⇒ root 取空名也会得到 `.reg_1` ⇒ sanitize 后 `_reg_1` | ★ **顺带发现上游两处拼接行为不一致** ⇒ 上游回执候选 **U-5** |
| **E5** | ★ **上游自己的做法**（本次实测）：`ch_device<Dut>` 的 top 端口节点名 = `top.out_data` ⇒ Verilog 端口名 = **`top_out_data`** | ★ 上游**接受**顶层端口带 `top_` 前缀（上游没有「按名解析的跨实现契约」需求）⇒ ⇒ **ChipForge 的「裸名契约」才是偏离方**（方向修正：不是上游偏离 ChipForge） |

### 3.3 oracle 的独立裁决（★ 用户要求「分析后再给 oracle 判断」）

| # | 裁决要点 |
|---|---|
| ★ **X 轴重构成立** | 真正的分岔不是 a/b/c，而是「**契约身份要不要与 ctx 节点名解耦**」：**X=否** ⇒ 只有 (a) 可行；**X=是** ⇒ (a)/(b) 都可行 ⇒ ★ 选 (b) 可消掉「告警洪流」与「私有前缀约定」两条债务。★ **但应拆成两轴**：解析机制 × 契约字符串形态（裸名 vs ctx 全路径名） |
| ★ **第四解**（oracle 挖出） | (b) + X=否 + 契约名改 ctx 全路径名 ⇒ 不改上游/不改解析/无洪流 ⇒ ★ **残留债务更重**（契约名携带层次路径 ⇒ 组件改名/嵌套加深 = 契约字符串批量失效）+ 需 bump 已 Accepted 的 ADR-091 §D3a.9 ⇒ 排第 3 |
| ★ **(b) 不破坏 S-2 determinism** | 两个 runner 都走 `CpuFactoryChmem::build_cpu`（`parity_sim_runner.cpp:332` / `cpu_verilator_sim.cpp:276`）⇒ 同一 factory ⇒ 名字集合天然相同 ⇒ ★ (b) 的真实代价**只有契约解析一处** |
| ★ **实现候选裁决** | ★ 选 **(i) 声明期登记**（框架登记「契约名 → 节点」+ manifest）；★ **否决 (ii)**（双侧各出 manifest ⇒ 结构性削弱 `names_hash` 这个「两侧 elaboration 漂移探测仪」）；★ **否决 (iii)**（后缀回退：**形式 fail-closed、语义 fail-open** —— 把「组件改名/端口挪层」这类本应响亮失败的重构吸收为静默继续） |
| ★ **C-1 三态的新边界** | `kOk`/`kMissing` 保留；★ **`kAmbiguous` 从 runner 上移到创建期**（上游 `NodeNamePolicy::kRejectDuplicates` 创建点 fail-fast）+ **登记期**（同一契约名登记两次 ⇒ `elaborate()` 失败）⇒ runner 侧不再有「扫描取首个」的可歧义路径 |
| ★ **防漏登记两道机械闸** | ① `elaborate()` 收尾扫全部顶层 `type_output` ⇒ 每个 MUST ∈ 登记簿 ∪ 显式 allowlist；② gate 侧 manifest 键集 == 契约文件键集 ⇒ diff ⇒ ERROR |
| ★ **第五解** | 上游 `NodeNamePolicy::kRejectDuplicates` + `NodeOwnershipSuppression`（`include/core/context.h:58-63`）⇒ (a) 也有上游-native 路径 ⇒ ★ **(a)/(b) 实现成本几乎相同** ⇒ 选型依据**纯粹落在架构方向**，不落在实现难度 |

### 3.4 我的独立复核（★ 含 1 处纠正 + 1 处补充 + 1 处不同意）

| # | 内容 |
|---|---|
| ✔ | **核实通过**：oracle 的「(b) 不破坏 `names_hash`」（两侧同 factory）；「`kRejectDuplicates` 创建期 fail-fast」（`context.h:51-56` / `context.cpp:194-237` / `lnode/context.tpp:36`）；「`NodeOwnershipSuppression` 存在」（`context.h:58-63`） |
| ★ **纠正** | oracle 称「ChipForge 侧 `declare_in` 全部匿名 ⇒ 开 `kRejectDuplicates` 会立刻炸」——**不成立**：`declare_in<T>(name)` / `declare_out<T>(name)` **传名建节点**（`include/cf/plugin/pipe_builder.h:539/563`）⇒ 匿名名是上游 `ch_logic_in` 默认构造的行为 ⇒ ⇒ 该风险显著降低 |
| ★ **补充（决定 (b) 的改名成本落在哪）** | Verilator 侧观测读取是**硬编码**的 `r->top__DOT___reg_N`（`tools/verilator_runner/sim_main.cpp:93-100`）⇒ 那是**预载核内部** `_reg_N` 信号；开放核的观测是**顶层端口** ⇒ ★ **两侧当前就用了不同访问路径** ⇒ ★ A6（开放核 Verilator 侧）本来就要重写读取路径 ⇒ (b) 的改名成本**落在那一处**，不新增成本面 |
| ★ **不同意（补充为必要组成）** | oracle 把两道机械闸写成「配套」⇒ ★ 我主张它们是 **X=是 的必要组成**（缺一 ⇒ R10「漏登记 ⇒ 观测静默缺失」= 最坏假绿）⇒ 必须写进**硬约束**，不进 tasks 可选项 |

### 3.5 ★ 用户裁决（2026-11-07）

| # | 裁决 |
|---|---|
| ★ **D-01** | ★ **采纳 (b) + 契约登记簿(i)** ⇒ ★ **推翻** v2 方案阶段的「Q1 = nullptr 分支」排除法闭案（闭案前提 = 「契约名 == ctx 节点名」不可变；★ 在「远期无债务」目标下，**该前提本身才是要改的对象**） |
| ★ **D-02** | ★ **迁移策略 (i) 两阶段**：**C-1（`resolve_node_id` 三态化）原样先行**；★ **登记簿随 ADR-094 的 builder 阶段落地**；★ 两阶段之间**顺序前提写死**（见 §3.8 SF-5） |
| ★ **D-03** | ★ **批准 critic 第 3 轮**（预算 2 轮已用满，用户显式追加）—— 理由：架构方向实质变更，不宜跳过审查 |

### 3.6 裁决的**具体内容**（★ 落笔时按此写）

| 项 | 内容 |
|---|---|
| ★ 归属 | 顶层端口**归 root 组件** ⇒ 组件树完整 ⇒ **无「归属空洞」告警** ⇒ 不训练「忽略告警」的习惯 |
| ★ 契约绑定 | **契约身份从 ctx 节点名解耦**：框架登记簿「契约名 → {node id, raw 名, 是否有 driver}」⇒ ★ **两个 runner 消费同一份 manifest** |
| ★ 契约名 | ★ **一个字不用改**（`reg_1..reg_31` / `open_pc` / `mem_sig` / `mem_sig_updates` / `tohost_probe_data_proxy`）⇒ 它们变成**登记簿的 key = 逻辑契约名** |
| ★ 私有约定 | ★ 「顶层端口不许带前缀」**删除** ⇒ ★ 与上游 `ch_device` 惯例（`top_out_data`）**终于一致** ⇒ 「D3 债务」消失 |
| ★ 上游 | ★ **零改动** ⇒ 不欠上游的债 ⇒ (c) 降级为未来项（★ #17 已作废） |
| ★ 三条债务 | ★ **D1**（契约耦合）· **D2**（告警洪流）· **D3**（私有前缀约定）**同时消** |

### 3.7 ★ 可行性核验（★ **不需要上游改动**）

| # | 内容 |
|---|---|
| ★ 实现路径 | `Component::node_owner()`（`CppHDL/src/component.cpp:16-26`）= 「suppressed ⇒ nullptr；**`scope_` 非空 ⇒ `scope_.back()`；否则回落 `current_`**」⇒ ★ `Component::set_current(root)`（public static setter，`include/component.h:104`）即可让**运行时动态创建**的顶层端口归属 root ⇒ 上游 `__io` 的「编译期静态 struct」限制**不构成障碍**（ChipForge 顶层端口是运行时动态声明的：`declare_out("reg_"+i)` 共 31 个） |
| ★ 合规性 | 上游 **ADR-059 D1 原文**即 `scope_.empty() ? current_ : scope_.back()`（`docs/adr/ADR-059-node-ownership-and-component-naming.md:57-63`）⇒ ★ **`current_` 回落是上游 ADR 明文允许的路径**；★ critic 第 3 轮判「不违反 D1 意图」 |
| ★ 窄口径纪律（→ 议题 #23） | root-scope RAII **只包住单次节点创建调用**；★ **不得**覆盖连接操作 —— `include/core/io.h:370/395/420/442` 的 4 处方向守卫用 `lnodeimpl::current_component()`（= `node_owner()`）⇒ 覆盖到 `<<=` 会改变守卫的比较对象 |
| ★ 正面副作用 | 若插件在自己的 `describe()` 内声明端口（`scope_` 非空）⇒ 端口归属**该插件**而非 root ⇒ ★ 「顶层端口」不可能被误建成组件内部端口 |
| ★ 已知既有缺陷（同批必修） | `ip/cpu/core/regfile_bridge_chmem.h:124` 裸 `make_unique<RegFileComponent>(nullptr,"regfile")` ⇒ 作用域帧残留（`include/component.h:140-156` 明写）⇒ 实测：该单元之后创建的节点 owner=`regfile`、名字带 `regfile.` 前缀 ⇒ ★ 在 (b) 下从「命名事故」升级为「**模块归属污染**」⇒ **两案的共同 blocker** |

### 3.8 ★ critic 第 3 轮（Δ7~Δ13 增量）

| 项 | 结果 |
|---|---|
| ★ 结论 | ★ **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · **0 BLOCKER** · 5 SHOULD_FIX · 2 NIT · `scope_exceeded: false` |
| ★ 落点裁决 | ★ **Δ13 分流成立**：登记簿契约落 `docs/architecture/parity-gate-contract.md`（已 own 解析语义 S-2/S-4/S-5 与 ERROR 分类）+ ADR-089（门禁 ERROR/退出码语义）；★ ADR-094 只留引用 ⇒ 不越层 |

| # | SHOULD_FIX / NIT | 闭合（方案 v3.1） |
|---|---|---|
| ★ **SF-1** | 闸①「顶层 `type_output`」定义未钉死 ⇒ 逃逸口（`owner==nullptr` 的输出在分析层被映射为 root，**语义上就是**顶层端口） | ★ 闸①扫描 = 「`type_output` **且**（`owner==root` ∨ `owner==nullptr`）」；clock/reset 与「owner ∈ 子组件」的端口**显式排除**（各附理由） |
| ★ **SF-2** | allowlist 无治理 ⇒ 它**就是**漏登记的后门 | ★ allowlist 落 `parity-gate-contract.md`、**变更走 architect 审** ⇒ MUST NOT 随手编辑 |
| ★ **SF-3** | 登记簿「记全部」与闸②「键集相等」口径冲突 ⇒ 闸②要么恒红要么被架空 | ★ **选 (a)**：登记簿内部记全部，**manifest 导出面只过滤「观测绑定」** ⇒ 闸②比「manifest 观测键集 == 契约文件观测键集」 |
| ★ **SF-4** | Δ11.1 引「ADR-094 G-1s」与 Δ13 分流矛盾 | ★ **只定义一处**（`parity-gate-contract.md` + ADR-089）⇒ ★ **不占用 ADR-091 的 `G-1*` 命名空间**（NIT-1）⇒ ADR-091 §D3a.9 与 ADR-094 均改为引用该处 |
| ★ **SF-5** | ★★ **Δ10 未钉落地批次 ⇒ C-1 有全量变红风险**（`resolve_node_id` 是全名精确比较 ⇒ 若先切 owner=root 再跑 C-1 阶段 1 ⇒ 全量 `kMissing` ⇒ **门禁恒红**） | ★ Δ10 补：「**owner=root 切换与登记簿同批（阶段 2）落地；阶段 1 维持 `owner == nullptr`**」 |
| ★ **NIT-2** | 归属切换（Δ8）与登记（Δ9.1）是 `declare_in/out` 内**同一处**改动 ⇒ 分两批会出现「归属已切、登记未至」的中间态窗口 | ★ **合并为一个 tasks 项**（归属切换 + 登记 + manifest 三件事同批） |
| ★ 验证建议（交 build/verifier） | 闸①补丁后跑 PoC：在 `node_ownership_suppressed()` 激活期**裸建**一个 `ch_out`（绕过 `declare_in/out`）⇒ 确认 `elaborate()` 收尾**报错**而非静默通过 | ⇒ 归 tasks（**推断，未验证**） |

### 3.9 ★ 遗留风险（#01 派生）

| # | 风险 | 触发条件 | 最早可观测信号 | 处置 |
|---|---|---|---|---|
| R6 | root 的名字成为**产物的一部分** ⇒ 改名 ⇒ 全部 Verilog 端口名变（`reg_1`→`cpu_reg_1`）⇒ 基线 churn | 任何人 rename root | baseline 出现「无功能变更的批量 hash 更新」commit | ★ **root 名冻结为契约常量**（议题 #23） |
| R7 | `set_current(root)` 是 **thread-local 全局** ⇒ root-scope 覆盖过宽会把插件节点误归 root | root-scope 内做了连接或跨语句 | 层次化 golden 的模块划分异常 | ★ RAII **只包单次建节点调用**（议题 #23） |
| R8 | 扁平产物 Verilog 端口名变化 ⇒ 开放核 Verilator 侧读取路径失效（现状硬编码 `top__DOT___reg_N`） | owner=root 切换落地 | Verilator 侧读到的观测全为 0/垃圾 | ★ A6 读取路径**改为 manifest 驱动**（`sanitize(raw 名)`），与登记簿同批 |
| R9 | 跨设计节点名不同（开放核带前缀 / 预载核不带）⇒ 误以为破坏 S-2 | — | — | ★ **不破坏**：determinism 是**同设计跨后端**比对（`tools/parity/parity_gate.py:275-283`） |
| R10 | ★ **登记漏了 ⇒ 观测静默缺失**（最坏假绿） | 插件绕开 `declare_in/out` 自建 `ch_out` | 无闸则**不可上线** | ★ 由 SF-1/SF-3 闭合后的**两道闸**封死（硬约束，非 tasks 可选项） |
| R11 | `kRejectDuplicates` 启用面过大 ⇒ 启动即炸 | 全局启用 | 首个 case elaboration 即抛 duplicate-name | ★ 仅 parity ctx opt-in；★ ChipForge `declare_in/out` 传名建节点 ⇒ 匿名名非常态（**注**：oracle 曾称 ChipForge 侧 `declare_in` 全部匿名，经核实**不成立**） |
| R12 | 两阶段迁移双轨 ⇒ 两条解析路径并存 ⇒ 结果漂移 | 阶段 1 与阶段 2 交叠 | runner 判据分叉 | ★ 阶段推进以「ADR-094 的 builder 阶段完成」为准；★ **同一运行内只允许一条路径**（议题 #22） |

### 3.10 与其它议题的联动（★ 已反映进 §2 议题表）

| 议题 | 联动 |
|---|---|
| #03 | ★ 新增子决策：登记簿契约落点（critic 已背书） |
| #04 | ★ G-1o 的**判定对象**从「契约名 vs 节点名」变为「**ctx 节点名** vs 顶层端口名」⇒ 该条表述**要重写** |
| #08 | ★ r.12（单元端口前缀）**不再是契约必需**（契约已解耦）⇒ 裁决**降级为可选** |
| #13 | ★ `build()` 幂等从「注释承诺」升级为**迁移期前置**（setup 重跑 ⇒ 重复登记 ⇒ 登记簿重复键必须被框架拒绝） |
| #14 | ★ 新增两条 Check：「顶层端口 MUST 经 `declare_in/out` 登记」+「root-scope 内禁 `<<=`」 |
| #16 | ★ 回执清单更新：★ **U-5（上游两处全路径拼接行为不一致）** 新增；★ 其余维持 U-1 / U-2 / U-4 |
| #17 | ★ **作废**（登记簿方向下不需要上游改） |
| #21 / #22 / #23 | ★ **新增**（#01 直接派生） |

---

## 4. 决策日志（★ 用户裁决汇总）

| # | 日期 | 议题 | 裁决 |
|---|---|---|---|
| ★ **D-22** | **2026-11-08** | ★ **T13 首批成层单元（D-17 的修订）** | ★ **改判：首批成层单元 = `RegFileComponent`**（★ **取代 D-17 的「首批 = ALU」**）★ 理由三条：① `RegFileComponent` **已存在**（`ip/cpu/core/regfile_chmem.h:93`）② 它的装配形态（`regfile_bridge_chmem.h:124` 裸 `make_unique`）正是 ADR-094 Context 第 3 条点名的现存缺陷 + **V-40 的变异靶** ⇒ ①-D 无论如何都要改它 ③ 新建 ALU Component 是**净新增代码**，**无法验证「既有缺陷是否被修」** ⇒ ★ **代价**：ALU 推迟到后续批次（「纯组合、端口干净」的验证价值保留位置）；★ **连带**：D-17 的「第二批 = regfile 观测面显式化 + regfile 成层」拆开 —— **regfile 成层**提前到首批，**观测面显式化**（内部 `ch_reg` ⇒ 顶层 `ch_out`）留在后续（★ 那也是 `identity` provider 的**退役条件**，D-3.9「载体统一」+ 使用计数 = 0） |
| ★ **D-01** | 2026-11-07 | #01 顶层端口 owner | ★ **采纳 (b) + 契约登记簿(i)**；★ 推翻 v2 的「Q1 = nullptr 分支」闭案 |
| ★ **D-02** | 2026-11-07 | 迁移策略 | ★ **(i) 两阶段**：C-1 原样先行 + 登记簿随 ADR-094 的 builder 阶段落地；★ 顺序前提写死（阶段 1 维持 `owner == nullptr`） |
| ★ **D-03** | 2026-11-07 | 治理 | ★ **批准 critic 第 3 轮**（预算追加） |
| ★ **D-07** | 2026-11-07 | **T1 root 的身份 / 形态 / 命名契约** | ★ **采纳推荐（A / A / A / A）**：① root 组件形态 = **A**（`PipeBuilder` 持有并暴露 `root_component()`；★ root 必须「每 ctx 恰一个」⇒ 框架持有最不易被漏登记）② 名字常量 = **`cpu`**（★ 不用 `top`：上游 `ch_device` 的 top 组件已占用该名 ⇒ 独立 DUT（V-30）与集成网表**同名** ⇒ 拼接撞名（上游 C2 轴②）；★ 不用 `open_core`：不含 SoC 语境 ⇒ 未来 SoC 级 root 需再改名 = 又一次契约变更）③ 名字**是**对外契约（进 Verilog 名 ⇒ 改名 = 契约变更，需 ADR + 重基线）④ root 与层次化 emitter 的 `root` 参数 = **同一对象**（否则「登记簿归属」与「层次化归属」可能不一致 ⇒ 新二义面） |
| ★ **D-08** | 2026-11-07 | **T2 登记簿的登记点与 manifest 产出形态** | ★ **采纳推荐（①~⑤ 全采纳）**：① **登记点** = `declare_in/out` **自动登记**（默认且唯一）★ + `register_observation()` 为**受控例外通道**（须带来源说明 + 进闸① allowlist 面）；★ **输入端口同样登记**（输入也是契约面；★ 闸①仍只扫 `type_output`）② **manifest 产出** = **框架**在 `elaborate()` **收尾**写 `<build>/contract_manifest.json`，★ **每侧各生成**（`node_id` 是 ctx 私有编号 ⇒ 两侧编号空间不保证相同 ⇒ MUST NOT 共享一份）③ **字段** = `{ node_id, raw_name, has_driver, source }`（value-only）；★ **先不带** `sanitized_name`（★ 触发条件：某观测非顶层端口且 Verilator 侧必须按 Verilog 名直读 ⇒ MUST 增补）④ **消费方** = Simulator 侧 runner（替代 `resolve_node_id` 线性扫描）/ Verilator 侧 runner（开放核观测走顶层端口 ⇒ 按 node id 读，★ = R8 的解法）/ gate（闸②比**键集**）⑤ **生效阶段** = **S-17 全部规则自阶段 2 起生效**（★ 阶段 1 不产出 manifest ⇒ 仍走精确名扫描 ⇒ 门禁不红） |
| ★ **D-09** | 2026-11-07 | **T3 长期架构自审后的修订版**（★ 用户要求「不留架构债务」） | ★ **方向确认 + 6 处技术性修订已落**：① **闸①的「期望键集 + allowlist」MUST 由调用方注入**（框架只扫+比+fail ⇒ MUST NOT 内置 parity 概念 ⇒ SoC 级门禁可另配）② **失败 MUST typed end-to-end**（`PluginError::ContractRegistryMismatch` → runner `error_kind` → gate 分类；★ MUST NOT 只非零退出）③ **`error_kind` 允许取值清单登记在 S-10**（★ 三处只消费本表 ⇒ 新增码先改本表）④ **契约清单 = 独立的「逻辑契约名清单」**（★ **S-17.1**，★ 位于契约文档内 ⇒ 不新增文件 ⇒ 不改 `arch_paths` 治理配置）★ 且区分「**节点级契约名**（进 manifest）」与「**报告层观测量 id**」（★ 派生量不进 manifest ⇒ 否则闸②恒红）⑤ **某侧 manifest 缺失 MUST 报错**（`manifest_missing`）⇒ 防闸②「有 manifest 才查」的**静默退化**⑥ **allowlist 条目 MUST 带 owner + 退役条件**，且内容 MUST 出现在报告里（可审计）★ 另修正 D-07：**root 名 = 本设计的契约常量**（多 root 场景下各自成常量且互不同名）；★ 修正 D-08：`source` MUST 是**受控枚举**（非自由文本） |
| ★ **D-10** | 2026-11-07 | **T15 逻辑契约名统一（消除 runner fallback）** | ★ **采纳 A**：★ **逻辑契约名统一为 `pc`**（两侧**节点名**不同：预载核 `pc` / 开放核 `cpu.open_pc`；★ 逻辑名唯一）⇒ ★ 由**契约登记簿**承载绑定 ⇒ ★ ⇒ ★ **作废** ADR-091 §D3a.9 的「parity runner 加 1 处 fallback（先试 `pc` 再试 `open_pc`）」裁决 ⇒ ★ 契约清单 `node_keys` 用 `pc`；★ ADR-094 D-3.3 同步；★ V-14 加「逻辑名层 / 节点名层」两层注；★ **代码改动（runner 删 fallback）归 A6 / tasks**（builder 面） |
| ★ **D-11** | 2026-11-07 | **T4 阶段 1 的前置审计（同名普查）** | ★ **采纳推荐（①~⑤ 全采纳）**：① **普查形式** = 阶段 1 的**第一个 task**（★ 一次性、只读诊断 + 报告；★ **不**做常态化门禁 ⇒ 常态化由创建期 `kRejectDuplicates` 覆盖）② ★ **普查不通过 ⇒ fail-closed：先修存量同名，再切阶段 1**（★ **MUST NOT** 用 allowlist 绕过 ⇒ 同名 ⇒ `resolve_node_id` 取首个 ⇒ **读到错节点 ⇒ 假绿** ⇒ 阶段 1 的 `kAmbiguous` 正是在**暴露**该隐患）③ **普查范围** = **现网 52 case + CH_MEM binary 全量 case** 的 ctx（★ 两侧 runner 的 ctx 构造路径不同 ⇒ 只查一侧会漏）④ ★ **`kAmbiguous` 归类** = 新增 **`observation_ambiguous:<key>`**（**ERROR(2)**，登记进 **S-10** 的 `error_kind` 清单）；★ `kMissing` 复用既有 `observation_missing:<key>`（★ **不**新增第三类）；★ 阶段 2 后 `kAmbiguous` 由创建期/登记期**消除**（稳态）⑤ **落点** = 普查数据落 **plan §K35**（推导与证据面）；★ 一句结论进 tasks 前置；★ 新增 **Verification V-39**（含变异：注入同名节点 ⇒ 普查 MUST 报出 ⇒ RED） |
| ★ **D-12** | 2026-11-07 | **T5 阶段 2 的 root-scope 覆盖面与 R-A 缓解** | ★ **方向确认 + 4 处精确化已落**：① **覆盖面** = **保持全局**（`build()` + `elaborate()` 全程，★ **不细分** ⇒ 方向守卫要求 `current module = root` 在所有连接时刻成立）② ★ **R-A 的正解 = 不另立探针**，★ **把上游「owner ∈ subtree(root)」判据前移到框架构建收尾（扁平路径也跑）** ⇒ 新增 **r.14**（★ 触发点 = `build()` 收尾 **与** `elaborate()` 收尾各一次；★ 判据 = 除 clock/reset 外每节点 owner ∈ {root} ∪ {已登记单元}；★ 排除 ctx 级 clock/reset（上游 ADR-059 D10）与 `node_ownership_suppressed()` 期建的节点；★ 失败 = fail-closed + `error_kind = node_ownership_escape`；★ **自阶段 2 生效**、与 owner 切换**同批**）⇒ ★ **单一语义、无第二套规则**③ **技术实现** = `build()` 入口 `set_current(root)` + **RAII 包裹整个构建期**且 **MUST 恢复先前 `current_`**（嵌套安全）④ **维度边界** = 归属（r.14）/ 创建时机（G-1h·G-1i）/ 方向（G-1d）/ 接口载体（r.11）⇒ 四维度互不重叠 ⇒ r.14 可作硬约束 |
| ★ **D-13** | 2026-11-07 | **T6 预载核侧的表达与退役条件** | ★ **方向确认 + 长期架构修订（provider 模型）已落**：① ★ **「fallback = 第二条解析路径」的原表述被推翻** ⇒ ★ 改为**绑定提供者模型**：契约解析 = 「**逻辑契约名 → 节点**」的绑定；★ 绑定来源**可插拔** = `manifest`（运行时表）/ `identity`（★ 用**同一份** `node_keys` 精确匹配 ⇒ **零第二份清单** ⇒ 无漂移面）⇒ ★★ **两侧语义一致**（★ 差异**仅在绑定来源** ⇒ **不构成第二套解析语义**）② **退役条件** = **载体统一**（预载核观测也走顶层 `ch_out` ⇒ 自然获得 `declare_out` ⇒ 自然切到 `manifest` provider）★ + **机械判据** = `identity` 使用计数 = 0（报告可见 ⇒ 可监督）③ ★ **manifest 提供义务 MUST 按「检查语境」声明**（★ 修正我先前的**单一列表**缺口）⇒ `parity`（同一设计 × 两后端）两侧 `manifest`；★ `h3`（开放核 vs 预载核）开放核 `manifest` / 预载核 `identity` ⇒ ★ **消除 H-3 语境「预载核缺 manifest ⇒ 恒红」**④ **报告 MUST 标注每个观测的解析来源**（`manifest` / `identity`）⇒ fallback 覆盖面可见 ⇒ 临时态有终止证据
| ★ **D-14** | 2026-11-07 | **T7 V-35 / V-36 的判定口径与基线捕获** | ★ **方向确认 + 3 处「静默恒绿 / 僵尸判据」修订已落**：① **时机** = 阶段 1 结束 / 阶段 2 开工前（★ 切换后 `owner==nullptr` 形态不存在 ⇒ 基线**无法事后补**）★ 机器基线 = **临时产物**（构建目录 / CI artifact），★ **结论落 plan §K36** ⇒ 阶段 2 验收后**整体废弃**（★ 不进仓库长期维护面）② **形式** = **逐拍全观测量 hash 链**（FNV，与既有 `pchash` **同构造** ⇒ 零新范式）+ 终值逐位数组 + 环境标识；★ **迁移期产物不进长期报告 schema**（★ 否则多一个只服务迁移期的永久字段）③ **V-35 判定** = hash 链逐 case 相等（硬）+ 终值逐位相等（第二判据）+ **变异对照**（人为注入一处观测差异 ⇒ hash 链 MUST 不同 ⇒ RED）④ ★★ **V-36 判定（升级为双硬判据）** = ① **无新增误报**（阶段 2 触发集 ⊆ 阶段 1）② **阶段 2 触发集为空**（★ 「减少」是**预期收益** ⇒ 报告展示但不参与判定；★ 合法例外走 **REG-4 allowlist**）⇒ ★ 比「只看子集」强，且逃生口已设计⑤ **捕获** = 抓日志 + **4 个 overload 前缀**作**稳定匹配键**（★ 不匹配全文 ⇒ 上游改文案尾部不影响）+ ★★ **抓取器自检**（★ 一轮**零匹配** ⇒ MUST ERROR ⇒ ★ 否则「抓取器失效」表现为「零误报」= **恒绿假绿通道**）⑥ ★ **V-35 / V-36 显式标注为「迁移期判据」，阶段 2 验收后**退役** ⇒ 防僵尸判据 |
| ★ **D-15** | 2026-11-07 | **T8 「同一运行只允许一条解析路径」的强制点** | ★ **方向确认 + 1 处内部矛盾已修正 + 2 处精确化已落**：★★ **修正内部矛盾**：★ 原 ①「provider 由语境决定」与 ④「框架不认识 parity 概念」**冲突**（★ 框架若读 `contexts` 就认识了）⇒ ★ 改为：**框架只提供「provider 槽位」，★ 「装哪个」由调用方按语境决定** ⇒ ★ 框架不读 `contexts` ⇒ 与 **r.13** 同原则★ **单一槽位（按 ctx 划分）**：★ 每 ctx 恰一个 provider ⇒ ★ **重复安装 MUST 立即失败** ⇒ ★⇒ **结构上装不出双轨**（★ 运行期不可能双 provider ⇒ 「单路径」由**结构**保证，**不靠运行期检查**）★ **冲突检测点 = 安装期自检**（不是运行期）：① `manifest` provider 自检（键的 node 存在 + raw 名一致）② `identity` provider 自检（每个 `node_keys` 精确匹配到**唯一**节点）⇒ ★ 失败 ⇒ `ERROR(2)` + `provider_conflict`（★ 点名逻辑名 + 两个候选节点名）★ **`manifest_missing`（能力退化）vs `provider_conflict`（数据将错）分两类**，同为 ERROR(2) 但 kind MUST 不同；★ 「声明了 provider 却未安装」归入 `manifest_missing`（无须第三类）★ **通用总则 GN-1** 落门禁契约面（单点定义）：★ 「**判定机制自身失效 MUST 响亮失败，MUST NOT 表现为『无问题』**」⇒ 覆盖 V-36 抓取器自检 + provider 安装期自检 + 未来同类 ⇒ ADR 只引用 |
| ★ **D-16** | 2026-11-07 | **T9 关联 change 名 + AC 映射回填机制** | ★ **方向确认 + 2 处「两张皮 / 协调债务」修订已落**：★★ **change 数从 3 改为 2**（★ 自审发现：★ 若把「阶段 2 同批」拆到两个 change ⇒ **协调债务** ⇒ ★ 确立原则「★ **change 边界 = 交付批次边界**」）⇒ ① `chmem-hierarchical-assembly`（★ 框架地基 **+** 观测登记：root/root-scope/r.14/provider 槽位/登记簿/manifest/两闸/C-1 三态化/阶段 1 普查/迁移期基线 —— 全部在一批，因 D-02 规定阶段 2 必须同批）② `chmem-hierarchical-unit-landing`（★ 首批成层单元 + 层次化等价性判据）★★ **AC 映射的权威 = change 的 `tasks.md`**（★ openspec 的 AC 机制）⇒ ★ ADR 侧映射行是**生成镜像**，MUST 由工具产出、MUST NOT 人工维护 ⇒ ★ verifier MUST 机检「映射表 == tasks 的 AC 清单」（无孤儿/重复/遗漏）⇒ ★ 消除「ADR 与 change 两张皮」★ **plan 归属** = 同一份 `.rddf/plans/backend-parity-open-core.md`（K34 推导 / K35 阶段 1 普查 / K36 迁移期基线结论）⇒ ★ 不新建 plan 文件★ **Status 表述增强** = 「决策已全部闭合并经四轮审（0 BLOCKER）；★ 仅剩治理收口（立项 + AC 回填）⇒ 完成标志第 4 条未满足 ⇒ 未转 Accepted」★ 且明示「**不阻塞实施**、但不得据此宣称已 Accepted」 |
| ★ **D-17** | 2026-11-07 | **T13 实施面四项** | ★ **方向确认 + 3 处精确化已落**：① **首批成层 = ALU**（纯组合、端口干净）★ ★ **第二批 = 「regfile 观测面显式化」+「regfile 成层」同批**（★ 观测面问题的**退役条件** = 第二批完成）② **regfile 观测面 = 单元内显式化 31 个 `ch_out`**（父层读**单元端口** ⇒ 无 `__x_N` ⇒ 接口干净；★ 扁平下**零新增顶层端口**）；★ 附：成层交付形态下父层 MUST 连接或**空连接** ⇒ 上游 ADR-061 决策② A3 已规定 ⇒ 不新增规则（ADR-091 §D3a.9 已加实施化澄清）③ ★ **`build()` 幂等语义精确定义** = **同一状态** ⇒ 成功早退；★ **状态已变**（插件集指纹不同）⇒ **fail-closed**（`PluginError::BuildStateConflict`）⇒ ★ **MUST NOT** 静默早退并忽略新 plugin（★ 静默忽略 = 隐蔽假绿）；★ `built_` 改为**成功收尾时**置（★ 依据已核实：`elaborate()` `pipe_builder.h:429` 自动 build + `built_` 早置 ⇒ build 失败后**不重试** ⇒ 半构建状态跑闭包 ⇒ **fail-open**）④ **门禁 5 条** + **强度原则**（★ 「违反即静默失效」⇒ 硬；★ 「违反即需人工判断」⇒ WARN + 报告）：① 硬 ② ★ **升级为「层 2 基类的编译期校验」为主**（★ 上游 `port<T,Dir>` 无 bundle 拒绝 ⇒ 不能改上游 ⇒ 校验落 ChipForge 自己的基类 ⇒ **新单元自动受约束**）★ 门禁 grep 只查「绕过基类」的单元 ③ 源码 WARN + 运行期硬失败（闸①）④ WARN（先看噪声）⑤ 硬（G-1q 机检：层 2 单元 MUST NOT 引用 `Payload`）⇒ ★ **新增 r.16 + V-42** |
| ★ **D-18** | 2026-11-07 | **T14 落地顺序与切 builder 时机** | ★ **采纳推荐（①~③）+ 1 处与 D-16 的不一致已修正**：① ★ **change ① 内部 4 批**：①-A 框架地基（root/root-scope/r.14/r.15/r.16/r.10）→ ①-B 观测登记（S-17.1 + 登记簿 + manifest + 两闸 + provider 标注）→ ①-C 阶段 1（三态化 + 前置同名普查）→ ①-D 阶段 2（★ **不可拆**：owner 切换 + 登记上线 + manifest 上线 + r.14 生效 + 迁移期基线）★ change ②（首批成层单元）集成在 ①-D 之后、★ **开发可并行**★★ **修正与 D-16 的不一致**：★ 若 4 批留在同一 change 内而**只在末尾一次性验收** ⇒ 中间批次无独立验收 ⇒ 回滚成本高 ⇒ 债务 ⇒ ⇒ ★ 确立：**★ change 边界 = 不可拆的交付单元边界**（阶段 2）；★ **批内 MUST 分轮验收**（★ 每批的 AC 组单独过 verifier）⇒ 既不拆 change，也不放弃分轮验收② ★ **切 builder 时机** = ★ 讨论结束即切 builder；★ ★ **硬前置**：★ **立项（planner）与实施（builder）跨会话并行** ⇒ ★ 否则「ADR 转正 ↔ 实施」**死锁**（ADR 转正被立项卡住，实施被 ADR 卡住）③ ★ **收口动作** = ADR-094 Status 保持 `Proposed v0.1`（决策已闭、治理未收口 ⇒ 完成标志第 4 条未满足 ⇒ **不阻塞实施**）· 审稿记录 §7f · 本台账 §9 交接总览 · T10/T11/T12 带过 · ★ **commit 需用户点头**（★ 按文件粒度分 5 个） |
| ★ **D-19** | 2026-11-07 | **T10 ADR-046:33 的订正** | ★ **只加 dated 附注**（★ 登记「过时陈述」的根因 = 旧 `current_state()` 访问器 + 上游 ADR-050 改名 + `state_reg->next` 硬件推进 + 2026-11-07 实测 6 case/52 assertions 全绿）⇒ ★ **不**改其决策内容，★ 建议其**下一次修订**正式订正 |
| ★ **D-20** | 2026-11-07 | **T11 上游回执范围** | ★ **提 U-1 + U-5**（U-2 并入 U-1 同篇顺带一句）★ **U-3 不提**（低价值）★ ★ **U-4 不提**（★ 前提已被 D-01「owner=root」消解 ⇒ 提了也是无效回执） |
| ★ **D-21** | 2026-11-07 | **T12 `adr.md` §2.4 统计漂移** | ★ **显式化漂移 + 登记 backlog**（★ 在 §2.4 加口径注：统计截止 ADR-086；★ ADR-087 起未纳入；★ **在此之前本表不得被引用为「全部 ADR 的分布」**）★ **不**在收口轮顺手补登（★ 逐条归类 + 验证路径核实需独立一轮 ⇒ 顺手改易引入新错账） |

---

## 5. 当前未决清单与下一步

| # | 议题 | 类别 |
|---|---|---|
| ★ **02** | §D3a.7 接口载体改写**取代 v1.12 裁决 B** | ★ 签认（**当前待议**） |
| 21 | 契约登记簿契约本体 | 裁决（#01 派生，**最大的新增决策面**） |
| 22 | C-1 三态边界 + `kAmbiguous` 上移 + 两阶段顺序 | 裁决（#01 派生） |
| 23 | root-scope 窄口径 + root 名冻结 | 裁决（#01 派生） |
| 03 · 04 · 05 · 06 · 07 · 08 · 09 · 12 · 15 | 文档与契约面 | 裁决 |
| 10 · 11 · 13 · 14 · 16 · 18 | 顺序 / 实施面 / 上游 / 流程 | 裁决（可延后到 builder/planner） |

★ ★ **落笔前置**：全部「阻塞落笔」项闭合并经 critic 通过后，才写 **ADR-091 v1.16 + ADR-094 + `parity-gate-contract.md` + ADR-089 + `adr.md`**。

---

## 6. ★ #01 的 architect 复核与接受记录（2026-11-07）

> ★ 本节 = architect 对 D-01 的**独立复核**（证据逐条重验）+ **接受结论与绑定条件**。
> ★ 复核人 = architect（2026-11-07）· 依据 = 上游 HEAD `dbc9801` + ChipForge 当前工作区。

### 6.1 证据重验结果（★ 全部核实通过，含 1 处台账口径细化）

| # | 台账断言 | 复核结果 | 证据 |
|---|---|---|---|
| E1 | 契约名解析 = 精确相等 | ✔ 成立 | `tools/parity_runner/parity_sim_runner.cpp:249-255`：`node->name() == name` |
| E2 | 预载核侧 owner == nullptr | ✔ 成立 | `ip/cpu/cpu_factory_chmem.h` **零** `Component`/`ch_device` 引用；观测节点 = 插件内 `ch_reg`（`ip/cpu/plugins/reg_file_chmem.h:169`、`dmem_chmem.h:85-86`）⇒ 无组件作用域 ⇒ owner == nullptr |
| E3 | (b) 单侧破坏 parity | ✔ 成立 | 开放核侧节点名带 root 前缀 ⇒ 与预载核侧裸名不一致 ⇒ 精确匹配落空 |
| E4 | 节点名**无条件**加 owner 前缀 | ✔ 成立 | `CppHDL/src/core/lnodeimpl.cpp:33-37`：`parent_ != nullptr ⇒ name_ = parent_->name() + "." + name`；**不经过** `build_hierarchical_name`（后者只拼**组件名**，且对空父路径会跳前缀 ⇒ 组件侧有「空名前缀」后门，**节点侧没有** ⇒ 两处拼接行为不一致 ⇒ U-5 成立） |
| E5 | 上游 `ch_device` 的顶层端口名带 `top_` 前缀 | ✔ 成立（**代码路径复核**，非重跑实测） | `include/device.h` 造 top（名 `top`）⇒ 节点名 `top.<port>` ⇒ `include/codegen_naming.h:40-44` sanitize（`.`→`_`）⇒ Verilog 名 `top_<port>` ⇒ ★ **上游接受带前缀的顶层端口** ⇒ 「裸名契约」是 ChipForge 侧约定 ⇒ D-01 的方向修正成立 |
| E6 | `set_current(root)` 可让运行时创建的顶层端口归属 root | ✔ 成立 | `CppHDL/src/component.cpp:16-26`：`node_owner()` = suppressed ⇒ nullptr；`scope_` 非空 ⇒ `scope_.back()`；**否则回落 `current_`**；`include/component.h:96-97` `current()/set_current()` 为 public static |
| E7 | 上游 ADR-059 D1 允许 `current_` 回落 | ✔ 成立 | `CppHDL/docs/adr/ADR-059-node-ownership-and-component-naming.md:57-63` 原文：`scope_.empty() ? current_ : scope_.back()` |
| E8 | `NodeOwnershipSuppression` / `kRejectDuplicates` 存在且创建点 fail-fast | ✔ 成立 | `include/core/context.h:51-63`；`include/lnode/context.tpp:35-42`（创建点检查 ⇒ 冲突即抛） |
| ★ **E9** | ★ **台账未记录**：`simulator_init.cpp:95` 按 `!node->get_parent()` 分流指令表 | ★ **成立 ⇒ 新增风险** | `CppHDL/src/simulator_init.cpp:94-100`：`type_input` 节点 ⇒ `!get_parent()` ⇒ `input_instr_list_`，**否则** ⇒ `combinational_instr_list_` ⇒ ★ **(b) 下所有顶层 input 迁移到组合表** ⇒ **解释器侧求值顺序改变**（`src/simulator.cpp:252-260`：先 input 表后组合表）⇒ 上游架构主题文档 §4 已预警「归属真源变更 ⇒ 指令表分流可能变 ⇒ 仿真行为不得假定逐位不变」⇒ ⇒ **必须新增回归判据**（议题 #24） |
| ★ **E10** | ★ **台账未记录**：`io.h` 4 处方向守卫以 `current_component()`（= `node_owner()`）为比较对象 | ★ **成立 ⇒ 修正 #23** | `CppHDL/include/core/io.h:370 / 395 / 420 / 442`：`out<<=in` / `in<<=in` / `out<<=out` 要求 **receiver/driver 属于 current module**；`in<<=out` 要求 **receiver 不属于 current module**（= 子模块输入被外部驱动）⇒ ★ **(b) 下「current module」必须是 root 才是语义正确的比较对象** ⇒ ⇒ **root-scope 只包单次建节点的「窄口径」（#23 草案）会让守卫继续与「泄漏的单元 / null」比较 ⇒ 守卫语义仍错** ⇒ ⇒ ★ **#23 必须改为：root-scope 覆盖整个网表构建期（`build()` + `elaborate()`）** |
| ★ **E11** | ★ **台账未记录**：Verilator 后端**不消费** `input_instr_list_` | ★ **成立（利好）** | `CppHDL/src/core/verilator_backend.cpp:1049-1064`：`eval_combinational` 忽略两个表 ⇒ 按 `sync_inputs_to_vtop()` 驱动 ⇒ ★ **E9 的分流变化只影响解释器路径**，Verilator 侧不受影响 ⇒ 两后端对比的可比性不因 (b) 而变 |
| ★ **E12** | ★ **台账未记录**：预载核侧**无法**进登记簿 | ★ **成立 ⇒ 新增议题 #25** | 预载核观测节点是**插件内部 `ch_reg`**（`reg_file_chmem.h:169` · `dmem_chmem.h:85-86`），**不经** `pb.declare_out` ⇒ ★ **登记簿对预载核侧为空** ⇒ manifest 不能是唯一解析路径 ⇒ 必须定义「无登记簿一侧」的 fallback 与退役条件 |

### 6.2 ★ 接受结论

★ **接受 D-01（采纳 (b) + 契约登记簿(i)）** —— 理由（architect 独立判断）：

1. ★ **登记簿在 (a) 下同样必需** ⇒ (b) 不是「新造机制」，而是**把必需的机制推广到集成路径**：
   - 独立单元测试（V-26 / `ch_device<RegFileComponent>`）在 (a) 下同样会出现 `top.<port>` 前缀 ⇒ 契约名照样断 ⇒ ★ **(a) 也必须有「逻辑契约名 → 节点」的绑定层**；
   - 「顶层端口必须 owner==nullptr」是一条**私有约定**（上游惯例相反，E5）⇒ 靠纪律维持 ⇒ 迟早漂移。
2. ★ **(b) 的收益是可验证的、且成本有界**：消除归属空洞（D2）⇒ 消除「训练忽略告警」；消除私有前缀约定（D3）⇒ 与上游一致；契约身份与节点名解耦（D1）⇒ 后续改名/挪层不再批量改契约字符串。
3. ★ **实现成本不落在「难度」上**（E6/E7/E8：上游已有 `set_current` 回落路径 + 抑制器 + 严格名策略）⇒ ★ 选型依据**纯粹是架构方向**（远期无债务），不因实现便宜而选。
4. ★ **(b) 不破坏 S-2 determinism**：两侧同 factory ⇒ 同 ctx ⇒ 名字集合同源（`tools/parity/parity_gate.py:275-283` 是**同设计跨后端**比对）。

### 6.3 ★ 接受 D-01 的 4 项**绑定条件**（★ 缺任一项 ⇒ D-01 不可实施）

| # | 条件 | 依据 | 落点 |
|---|---|---|---|
| ★ **C1** | ★ **root-scope MUST 覆盖整个网表构建期**（`pb.build()` + `pb.elaborate()`），**不是**「只包单次建节点」⇒ ★ **修正议题 #23 的窄口径草案** | E10：窄口径 ⇒ ① 方向守卫仍与泄漏单元/null 比较（守卫语义仍错）② **非 root 创建的插件节点仍 owner==nullptr ⇒ 归属空洞照旧 ⇒ D2 未消除** ⇒ ★ 窄口径下 (b) 的两个主要收益都不成立 | 议题 #23（改写）+ ADR-094 r.1/r.6 |
| ★ **C2** | ★ **新增上游语义回归判据**：`simulator_init.cpp:95` 指令表分流（解释器侧求值顺序）+ `io.h` 4 处方向守卫触发面 | E9 + E10（上游主题文档 §4 明写「归属真源变更 ⇒ 仿真行为不得假定逐位不变 ⇒ 须逐项回归」） | ★ **议题 #24** ⇒ ADR-094 Verification（V-35 / V-36） |
| ★ **C3** | ★ **作用域帧泄漏 MUST 与 owner 切换同批落地**（`ip/cpu/core/regfile_bridge_chmem.h:124` 裸 `make_unique` ⇒ 帧残留 ⇒ 该单元之后的节点 owner=`regfile`、名字带 `regfile.` 前缀） | `include/component.h:140-156` 明写 + 探针实测 ⇒ (b) 下从「命名事故」升级为「**模块归属污染**」⇒ ★ 两案共同 blocker | 议题 #13 + tasks（builder） |
| ★ **C4** | ★ **必须先定「无登记簿一侧」的解析路径**（预载核侧登记簿为空 ⇒ manifest 不能是唯一路径） | E12 ⇒ ★ 缺此项 ⇒ D-01 的**可实施性不完整**（迁移期必然出现「一侧有簿一侧无簿」的混态） | ★ **议题 #25** ⇒ #21 / #22 的子决策 |

### 6.4 ★ 对台账的 3 处修正（★ 公开更正）

| # | 台账原文 | 修正 | 依据 |
|---|---|---|---|
| ★ **M1** | §3.7「窄口径纪律」：root-scope **只包住单次节点创建调用**，不得覆盖连接操作 | ★ **该纪律与 (b) 的目标自相矛盾** ⇒ 改为「root-scope 覆盖**整个构建期**」；理由 = 方向守卫（E10）需要「current module = root」才是正确比较对象 | E10 |
| ★ **M2** | §3.6「三条债务 D1/D2/D3 **同时消**」 | ★ **在窄口径下 D2 不成立**（非 root 创建的插件节点仍归属空洞 ⇒ 告警洪流照旧）⇒ ★ 「同时消」只在 **C1（全局 root-scope）** 下成立 | E10 + 节点创建路径盘点 |
| ★ **M3** | §3.9 R10「由两道闸封死」+ §3.5 D-01「三条债务同时消」 | ★ 保留，但须补：**闸①的扫描对象**必须含「owner==nullptr 的 `type_output`」（= 语义上的顶层端口）——★ 第 3 轮 critic SF-1 已指出，本记录确认（与台账 §3.8 的闭合一致） | critic 第 3 轮 SF-1 |

### 6.5 ★ 新增议题（★ 阻塞落笔）

| # | 议题 | 为何必须 | 建议方向 |
|---|---|---|---|
| ★ **24** | **上游语义回归面**：`simulator_init.cpp:95` 指令表分流 + `io.h` 4 处方向守卫触发面 | E9/E10：归属真源变更会改**解释器侧求值顺序**与**守卫触发面**；上游明写「不得假定逐位不变」⇒ 必须逐项回归 | 新增 **V-35**（指令表分流回归：两后端在同一 ctx 下的 input/combinational 分流集合一致 + 顶层 input 仍在 input 表）+ **V-36**（方向守卫触发面回归：owner 切换后 4 处守卫的触发/静默集合与基线一致，**不得**新增误报） |
| ★ **25** | **无登记簿一侧（预载核）的解析路径 + 退役条件** | E12：预载核观测节点是插件内 `ch_reg` ⇒ 登记簿为空 ⇒ manifest 不能是唯一路径 | manifest 解析 =「**登记簿命中优先；未命中 ⇒ 回退精确名**」+ 回退路径 MUST 显式登记为**临时态**（带 owner + 退役条件 = 预载核退役时一并删除）⇒ 迁移期**两侧的解析语义必须显式写在契约里**（否则「一侧有簿一侧无簿」会被误读为 bug） |

### 6.6 ★ 复核后的整体状态

- ★ **D-01 = 已接受**（带 4 项绑定条件 C1~C4）；★ **D-02 = 接受**（两阶段；阶段 1 维持 `owner == nullptr` ⇒ 门禁不红）；★ **D-03 = 已执行**（critic 第 3 轮 APPROVE_WITH_CONDITIONS / 0 BLOCKER）。
- ★ **落笔仍被阻塞**：§5 的落笔前置清单未清空（#02 / #03 / #04 / #05 / #06 / #07 / #08 / #09 / #12 / #15 / #21 / #22 / #23 / ★#24 / ★#25）。
- ★ **未落笔**：ADR-091 仍 v1.15；ADR-094 不存在。★ 未 commit。

---

## 7. ★ 用户裁决（2026-11-07 第二轮）与 #15 实证抽查

### 7.1 裁决

| # | 议题 | 裁决 |
|---|---|---|
| ★ **D-04** | 批次 1 / 2 / 3 的建议取值 | ★ **全部采纳**（含 **C1「root-scope 覆盖整个构建期」的修正** 与 **#25「无登记簿一侧解析路径」的新增**） |
| ★ **D-05** | #15 `ch_state_machine` 在层 2 是否禁用 | ★ **先做实证抽查**（critic 第 3 轮附注的建议）⇒ ★ **结果见 7.2** |
| ★ **D-06** | #03 ADR-091 拆分落点 | ★ **同意 = D（不拆，接受超长）**；登记簿契约落 `parity-gate-contract.md` + ADR-089 |

### 7.2 ★ #15 实证抽查（★ 实测，2026-11-07）

**问题**：在役多周期插件（ADR-046 豁免清单内，用 `chlib::ch_state_machine`）在 **CppHDL Simulator（解释器）后端**能否真正推进状态机？若能 ⇒ 「层 2 禁用 `ch_state_machine`」的依据不成立。

**做法**：
1. 增量构建 `chipforge_tests_chmem`（`cmake --build build --target chipforge_tests_chmem` ⇒ exit 0；★ 注：全量 `all` 目标失败于**外部依赖** `cpptlm_external` 的 configure —— 它写死 `/usr/bin/g++-14`（本机只有 `g++-13`）⇒ **与本次改动无关**，属既有环境问题）。
2. 跑 `[fsm]` 标签（走 `tools/run_catch2_with_baseline.sh`，baseline-aware）。

**结果（★ 实测）**：

```
All tests passed (52 assertions in 6 test cases)
[baseline] RESULT: 无新回归 ✔
```

★ 覆盖的 6 个 case（`--list-tests "[fsm]"`）：

| case | 覆盖的跨拍语义 |
|---|---|
| `mmu_ptw_fsm_sv32_walk_success` | ★ **5 状态 FSM 跨拍推进**：IDLE(0) → L0_WAIT(1) → L1_WAIT(2) → DONE(3) → IDLE(0)，并断言 `result_ppn == 0x12345` |
| `mmu_ptw_fsm_sv32_reserved_encoding_fault` | 保留编码 ⇒ fault 分支 |
| `mmu_ptw_fsm_sv32_invalid_pte_fault` | 非法 PTE ⇒ fault 分支 |
| `l1cache_refill_fsm_hit` | refill FSM：命中路径 |
| `l1cache_refill_fsm_miss_refill` | ★ **miss ⇒ 多拍 refill 后回 IDLE** |
| `l1cache_refill_fsm_refill_race` | refill 与竞争条件的交互 |

★ 运行日志明确：`JIT compiler not available, using interpreter` ⇒ ★ **解释器路径**。

**机制层证据（读码）**：

- 上游 `chlib::ch_state_machine::build()`（`CppHDL/include/chlib/state_machine.h:453-491`）把转移组合成 `next_state`，并执行 **`state_reg->next = next_state;`**（`:488`）⇒ ★ 状态是**真实的 `ch_reg`** ⇒ ★ 解释器每拍按 `ch_reg` 的 next 逻辑推进 ⇒ **天然可推进**。
- ★ ADR-046:33 的依据是**旧 `current_state()` 访问器**（永远返回 `entry_state`）⇒ ★ 上游 **ADR-050** 已把它**改名为 `entry_state()`** 并明写「它从不返回运行时状态；运行时请用 `current_state_uint()` / `is_in()`」（`state_machine.h:395-403`）⇒ ⇒ ★ **ADR-046:33 的「不能在 CppHDL simulator 内推进状态机」是**过时陈述**（把「访问器不返回运行时状态」误读为「硬件状态不推进」）。

### 7.3 ★ #15 的裁决结论（★ 由实测得出）

★ **`ch_state_machine` 在层 2 MUST NOT 被一刀切禁用** ⇒ ★ **ADR-094 D-5 的措辞改为**：

| 项 | 结论 |
|---|---|
| ★ 允许性 | ★ **允许**层 2 单元内部使用 `chlib::ch_state_machine`（★ 双后端实测可推进；Verilog 侧经 `state_reg->next` 发射） |
| ★ 边界 | ★ ① 状态机 MUST **完全封装在单元内部**（不跨单元、不读 `Payload`）② 单元接口仍 MUST 是**标量端口** ③ ★ **周期成本 MUST 显式登记**（多周期单元进入 parity/H-3 语境时，其拍数差异 MUST 走 ADR-089 例外白名单，**MUST NOT** 静默假定逐位相等） |
| ★ 连带登记（backlog） | ★ **ADR-046:33 的技术陈述已过时** ⇒ 建议在 ADR-046 的**下一次修订**中订正（★ **不在本 change 顺手改** —— 存量 Accepted ADR 的订正属独立变更） |
| ★ 对 D-5 的影响 | ★ 原草案的「禁用」依据（我据 ADR-046:33 推导）**不成立** ⇒ ★ **D-5 不得引用 ADR-046:33 作为禁用理由**；改为引用本节实测 + 上述三条边界 |

### 7.4 ★ 落笔前置状态

★ 全部**阻塞落笔**项已闭：★ #02（签认）· #03（D）· #04 · #05 · #06 · #07 · #08 · #09 · #12 · #21 · #22 · #23（含 C1）· #24 · #25 · #15（实测结论）⇒ ★ **进入「成形」步骤**（写 ADR-091 v1.16 + ADR-094 + `parity-gate-contract.md` + ADR-089 + `adr.md`）。
★ 剩余治理动作：★ **文本级 oracle → critic 复审**（critic 预算已用满 3 轮 ⇒ **需用户追加批准**方可开第 4 轮）。




---

## 8. ★ 议题 T1~T14 讨论（2026-11-07 第二轮 · 按依赖与关键程度排序）

| # | 议题 | 状态 | 结论 / 依赖 |
|---|---|---|---|
| **T1** | root 的身份 / 形态 / 命名契约 | ★ **已决（D-07）** | root 由 `PipeBuilder` 持有；名字常量 = **`cpu`**；对外契约 = 是；与 emitter `root` = 同一对象 |
| **T2** | 登记簿的登记点与 manifest 产出形态 | ★ **已决（D-08）** | 登记点 = 自动 + 受控例外；manifest = 框架在 `elaborate()` 收尾、每侧各生成、字段 `{node_id, raw_name, has_driver, source}`；★ **S-17 自阶段 2 生效** |
| T3 | 两道闸的实现位置与失败语义 | ★ **已决（D-09 · 长期架构修订版）** | 闸① = 框架内 `elaborate()` 收尾 + 期望键集注入 + typed 失败；闸② = gate 侧比 `node_keys` 键集 + 缺失即 ERROR；契约清单 = S-17.1；无降级开关 |
| T4 | 阶段 1 的前置审计（R-B 同名普查） | ★ **已决（D-11）** | 阶段 1 前置 = 一次性同名普查（52 case + CH_MEM 全量）；★ 不通过 ⇒ fail-closed 先修存量；★ `kAmbiguous` ⇒ `observation_ambiguous:<key>`（ERROR(2)） |
| T5 | 阶段 2 的 root-scope 覆盖面与 R-A 缓解 | ★ **已决（D-12）** | root-scope 全局不细分；★ R-A 正解 = **r.14 归属守卫前移**（扁平路径也跑）|
| T6 | 预载核侧的表达与退役条件 | ★ **已决（D-13 · provider 模型）** | 两侧语义一致；绑定来源可插拔（`manifest`/`identity`）；★ 义务按语境声明 |
| T7 | V-35 / V-36 的判定口径与基线捕获 | ★ **已决（D-14）** | 基线 = 切换前捕获的临时产物；★ V-36 双硬判据 + 抓取器自检；★ 两条均为**迁移期判据**，阶段 2 后退役 |
| T8 | 「同一运行只允许一条解析路径」的强制点 | ★ **已决（D-15）** | provider 槽位按 ctx 单一（装不出双轨）+ 安装期自检（`provider_conflict`）+ 通用总则 GN-1 |
| T9 | 关联 change 名 + AC 映射回填机制 | ★ **已决（D-16）** | ★ **2 个 change**（边界 = 交付批次边界）；★ AC 权威 = `tasks.md`，ADR 侧为生成镜像 + 机检相等 |
| T10 | ADR-046:33 的订正时机与归属 | 待议 | 独立 |
| T11 | 上游回执范围（U-1/U-2/U-4/U-5） | 待议 | 独立 |
| T12 | `adr.md` §2.4 统计表存量漂移 | 待议 | 独立 |
| T13 | 实施面四项（成层清单 / regfile 观测面 / `build()` 幂等 / 门禁 Check） | ★ **已决（D-17）** | 首批 = ALU；第二批 = 观测面显式化 + regfile 成层；★ `build()` 幂等语义精化 + `built_` 成功收尾置；★ 门禁 5 条 + 强度原则 + 层 2 基类编译期校验 |
| T14 | change 拆分与切 builder 时机 | ★ **待议（当前）** | 依赖 T9 ✔ · T13 ✔ |
| ★ **T15** | ★ **逻辑契约名统一（消除 runner 的 `pc`/`open_pc` fallback）** | ★ **已决（D-10 · A）** | 逻辑契约名 = `pc`；★ runner fallback 作废 |


---

## 9. ★ 交接总览（议题 → 裁决 → 文档落点）

### 9.1 落点索引（★ 实施者按此定位「哪条约束在哪篇文档」）

| 约束族 | 文档 · 位置 |
|---|---|
| **装配契约 G-1r（r.1~r.16）** | `ADR-094` §Decision **D-1**（★ r.13 期望键集注入 · r.14 归属守卫前移 · r.15 provider 槽位 · r.16 层 2 基类） |
| **粒度 / 命名 / 端口纪律 / 交付定位 / chlib 边界** | `ADR-094` §Decision **D-2 / D-3 / D-4 / D-5** |
| **迁移顺序（两阶段 + 批次）** | `ADR-094` **D-6**（★ 批内分轮验收 + 立项/实施并行 ⇒ 细则在本台账 §9.2） |
| **Verification V-27~V-42** | `ADR-094` §Verification（★ V-35/V-36 = **迁移期判据**，阶段 2 后退役） |
| **登记簿契约本体（REG-1~REG-6 + S-17.1 清单 + GN-1 + error_kind 清单）** | `docs/architecture/parity-gate-contract.md` **S-17 / S-17.1 / S-10**（★ **单一真源**；ADR-094 只引用） |
| **两层范式 / 接口载体 / 顶层端口形态 / G-1o 扩面 / G-1b 换根 / §D3a.9 观测载体** | `ADR-091` **v1.16**（§D3a.7 · §D5.1 · G-1o · G-1b · §D3a.9 · §D9） |
| **例外白名单 scope / ch_stream 管辖域 / 编号撞名 / FSM 陈述过时** | `ADR-089` §D3 注 · `ADR-092` §D4 注 · `ADR-083` §4.2 注 · `ADR-093` Status 注 · `ADR-046` Status 注 |

### 9.2 实施批次与治理规则（★ D-16 / D-18）

| 批 | 内容 | 出口判据 | 备注 |
|---|---|---|---|
| ①-A | 框架地基：`root_component()` / `create_child` / root-scope（RAII）· r.15（**槽位单例半**）· r.16 · r.10 · ★ **r.14 的代码落盘（不启用）** | **V-41a / V-42 / V-43 / V-34** | ★ `build()` 幂等语义与 `built_` 置位时机在此批；★ ★ **r.14 的「生效 + 判据 V-40」MUST 在 ①-D**（★ 原表把 V-40 列在此 ⇒ 阶段 1 期间 `owner == nullptr` ⇒ 该判据**必然全量 RED** ⇒ 实施者会在 ①-A 就要求归属守卫生效 ⇒ 门禁恒红） |
| ①-A′ | ★ **RegFile 前置适配**（★ 无独立 V · 是 ①-D 的**硬前置**）：`ip/cpu/core/regfile_bridge_chmem.h` 的裸 `make_unique` ⇒ 改 `root.create_child` | — | ★ 该行是 ADR-094 Context 第 3 条点名的现存缺陷 + **V-40 的变异靶** ⇒ 不改则 ①-D 的归属守卫**必然 RED** |
| ①-B | 观测登记：S-17.1 清单 · 登记簿 · manifest · provider 安装 · 闸①② · 报告 provider 标注 | V-37 / V-38 / **V-41b** | ★ 闸①在**扁平路径**上也能拦漏登记；★ V-41 拆两半：①-A 只验「槽位单例 + 重复安装立即失败」，manifest / identity 的**安装期自检**归此批 |
| ①-C | 阶段 1：C-1 三态化 + 前置同名普查（52 case + CH_MEM 全量）· ★ **V-35 / V-36 基线捕获** | V-39 · **V-35 / V-36（捕获）** | ★ 普查**不通过 ⇒ 先修存量**，不切；★ ★ **基线捕获 MUST 在本批出口**（★ ADR-094:157「切换后**无法事后补**」）⇒ ①-D 只做**比对 + 退役** |
| ①-D | ★ **阶段 2（不可拆）**：owner 切换 + 登记上线 + manifest 上线 + r.14 **生效** + ★ **三条同批改动**（runner 精确名解析 ⇒ manifest 驱动 · Verilator `top__DOT___reg_N` 硬编码 · regfile 裸 `make_unique`） | **V-40** · **V-35 / V-36（比对 + 退役）** | ★ 基线 = **临时产物**（构建目录 / CI artifact）；★ 结论落 plan §K36；★ 两条判据验收后**退役**；★ 验收失败 MUST **整批回退** |
| ② | ★ **首批成层单元（`RegFileComponent` · D-22 裁决）** + 层次化等价性判据 | V-27 / V-27b / V-28 / V-29 / V-30 / **V-32** / V-33 | ★ 开发可与 ① 并行，**集成**在 ①-D 之后；★ ★ **D-22 取代 D-17 的「首批 = ALU」**（★ `RegFileComponent` 已存在 + 其装配形态是 V-40 的变异靶 + 新建 ALU 是净新增代码无法验证既有缺陷是否被修）⇒ ALU 推迟到后续批次 |

★ ★ **v0.2 补 2 处归属遗漏（★ 违反 ADR-094:13「无遗漏」）**：★ **V-32**（每实例端口名逐字不同 + 单元每个 `ch_in` 均有 driver）与 **V-34**（2 TU 链接探针）原先在批次表里**无归属** ⇒ ★ 现分别归 ②（V-32）与 ①-A（V-34）。

★ ★ **「无重复」判定口径（v0.2 补）**：★ 同一 V 被两个 change 各覆盖一次是**合法**的（**框架侧** / **落地侧** / **回归侧**）⇒ ★ 机检 MUST 按「**同一 V 的同一侧面**」判定，★ NOT 按 V 计数。

★ **治理硬前置**：★ **立项（planner）与实施（builder）跨会话并行** ⇒ 否则死锁；★ ★ **批内 MUST 分轮验收**（每批的 AC 组单独过 verifier）。

### 9.3 交接清单（★ 给 planner / builder）

- **planner**：★ 立项 **2 个 change**（`chmem-hierarchical-assembly` · `chmem-hierarchical-unit-landing`）⇒ ★ **边界 = 交付批次边界**（★ 阶段 2 同批 ⇒ ①② 合成一个）⇒ ★ 起草 `tasks.md` 的 **AC 清单**（★ **AC 权威**）⇒ ★ 回填 ADR-094 的 AC 映射（★ **生成镜像**，MUST 机检「映射表 == tasks 的 AC 清单」）⇒ ★ 落 **plan §K34**（推导与证据）/ §K35（阶段 1 普查）/ §K36（迁移期基线结论）。
- **builder**：按 §9.2 的 5 批推进；★ 实施面清单 = **10 处代码注释订正** + **框架改动**（root/attach/provider/基类/build 幂等）+ **门禁 5 条**（★ 强度原则：违反即静默失效 ⇒ 硬；需人工判断 ⇒ WARN）+ **runner 删 fallback**（A6）★ **零新增顶层端口**（★ 扁平形态下观测口本就是顶层）。
- **architect**：★ **上游回执**（U-1 + U-5，★ U-2 并入）★ `adr.md` §2.4 补登 backlog（下次注册表整理）★ ADR-046 下次修订正式订正 §1.2。
  - ★ ★ **长期 backlog（本套契约最实质的残留风险 · oracle 终审点名）**：★ **r.14 的「创建时机维度」** —— ★ r.14 的判据是「owner ∈ {root} ∪ {已登记单元}」，★ **不查「该节点是否该在这个时刻/场合创建」** ⇒ ⇒ 全局 root-scope 下，★ `at_stage` 闭包内**误建**的节点被归 root ⇒ **合法化为不可见**（★ 闸①只扫 `type_output` ⇒ 中间类型的幽灵节点 invisible）⇒ ★ 当前缓解 = tasks 级探针（owner 分布诊断）+ REG-4 allowlist ⇒ ★ **机制级封死**（如 r.14 增「创建时机维度」）会显著加复杂度 ⇒ ⇒ ★ **登记为长期项**（★ 不在本次收口解决；★ 触发条件 = 幽灵节点被实际观测到 ⇒ 立即升级为 ADR 修订）。

### 9.4 ★ commit 计划（★ 按文件粒度 · **待用户点头**）

| # | 内容 | 文件 |
|---|---|---|
| 1 | `docs(adr): 新 ADR-094 —— CH_MEM 开放核的层次化装配契约与端口纪律` | `docs/architecture/adr/ADR-094-chmem-hierarchical-assembly-contract.md` |
| 2 | `docs(adr): ADR-091 升 v1.16 —— 层次化框架对齐（4 处订正 + G-1b 换根 + G-1o 扩面 + G-1r 索引）` | `ADR-091-*.md` |
| 3 | `docs(architecture): 契约与注册表同步 —— S-17 登记簿 / S-17.1 清单 / S-10 error_kind + GN-1 / ADR-089·083·092·093·046 注记 / adr.md` | `parity-gate-contract.md` · `ADR-089` · `ADR-083` · `ADR-092` · `ADR-093` · `ADR-046` · `adr.md` |
| 4 | `docs(review): ADR-094 决策台账 + 四轮审稿记录` | `.rddf/reviews/ADR-094-decision-ledger.md` · `.rddf/reviews/ADR-094-hierarchical-assembly-contract-review.md` |
| 5 | `chore(state): role 切 architect` | `.rddf/state/role.json` |

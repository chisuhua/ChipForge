# 层次化 Plugin 架构 —— 入口 · 当前态 · 结构事实 · 需求 · 残留

> ★ **本文件 = 架构主题文档**：只承载**当前态快照** · **下游需求** · **目标形态的结构事实** · **已知残留**。
> ★ **决策不在这里** ⇒ 决策在 **[`ADR-094`](adr/ADR-094-chmem-hierarchical-assembly-contract.md)**（装配契约 / 粒度 / 端口纪律 / 交付定位 / `chlib` 边界 / 迁移顺序）与 **[`ADR-091`](adr/ADR-091-chmem-open-core-port-architecture.md) v1.16**（两层范式 / 硬约束 / 观测量载体）⇒ ★ 本文件只**引用**，MUST NOT 重述条款（★ 避免两处全文 ⇒ 必然漂移）。
> ★ **过程日志**（议题 / 裁决 / 审稿）⇒ `.rddf/reviews/ADR-094-decision-ledger.md`（台账）· `.rddf/reviews/ADR-094-hierarchical-assembly-contract-review.md`（审稿记录）。
> ★ ★ **本文在体系中的位置（2026-11-07 建 · ★ 2026-11-08 语义修订）**：★ 本文件是**层次化 Plugin 的「目标架构导航入口」**（★ §0）⇒ ★ 读者**从本文出发**即可定位**全部真相**（★ ★ **真相 = 架构决策文档 + 架构文档**（★ **目标态**）—— ★ ★ **代码不是真相，代码是朝目标实施的过程态** ⇒ ★ 详见 §0.1 的核心纪律）⇒ ★ 每条真相带**可复核锚点** + **时效标记**；但 ★ **本文件不是权威入口** —— ★ **权威分布** = **决策** → ADR-094 / ADR-091 · ★ **门禁面契约** → `parity-gate-contract.md` · ★ **上游目标态** → CppHDL ADR-058~062（★ **独立仓库** ⇒ 须锚定 commit）⇒ ★ 本文件 §1~§11 只承载**实施态快照 + 结构事实 + 需求 + 残留**，★ **MUST NOT** 与上述权威文档重述同一结论。

- Date: 2026-11-07
- Author: architect
- 状态: 🚧 **Draft（当前态快照 · 2026-11-07 实测）** ⇒ ★ **ADR-094 的治理收口（立项 + AC 回填）完成后**转正
- 上游对照: `CppHDL/docs/architecture/2026-11-02-hierarchical-rtl-output.md`（★ 上游版主题文档 · 2026-11-06 转正）
- 关联: ADR-094 · ADR-091 v1.16 · `parity-gate-contract.md`（S-10 / S-17 / S-17.1）· `.rddf/plans/backend-parity-open-core.md` §K17~§K33（★ K34~K36 待起草）

---

## 0. ★ 导航层（★ **入口** · 2026-11-07 新增 · ★ **2026-11-08 语义修订**）

> ★ **本节 = 入口的唯一职责**：★ 「要查层次化 Plugin 的**目标架构** ⇒ 去哪篇」。★ **本节不含任何决策 / 结论**（★ 结论一律在 §0.3 指向的权威文档里）⇒ ★ 本节**可长期稳定**，而 §1~§11 随实施演进 ⇒ ★ 两者**更新频率不同 ⇒ 分离**（★ 防「高频内容拖累低频内容 ⇒ 整篇腐化」）。
>
> ★ ★ ★ **2026-11-08 语义修订（★ 公开更正）**：★ **上一版（2026-11-07）把「实现 / 运行时 / 证据」也列为「真相」⇒ ★ 该判定错误** ⇒ ★ **代码不是真相** —— ★ **代码是「朝理想架构目标实施」的过程态** ⇒ ★ ⇒ ★ 本版按此重划：★ **真相区 = 架构决策文档 + 架构文档**（★ **目标态**）· ★ **实施区 = 代码 / 测试 / 运行时产物 / 证据链**（★ **过程态**）⇒ ★ 本版据此重写 §0.1 / §0.3 / §0.4 / §0.5。

### 0.1 ★ 真相只有**两类**（★ 目标态）

| 类 | ★ 真相是什么 | 载体 | ★ 权威形态 | ★ 锚点形态 | ★ 漂移方式（★ 决定该盯什么） |
|---|---|---|---|---|---|
| ① **架构决策文档** | 「**该怎么做**」的决策（★ 含取舍与理由） | **ADR** | ★ **单一权威文档** | 文档 + **Status / 版本** + Parent 版本 | 决策漂移 ⇒ ★ 须走**取代流程**（★ 不得单方面改） |
| ② **架构文档** | 「**架构长什么样**」的描述与契约 | 主题文档 / 总览 / 接口 / 门禁契约 / 上游指南 | ★ **单一权威文档**（★ 按「面」各一） | 文档 + **章节锚点** | 描述漂移 ⇒ ★ 须改对应文档（★ 不改则两处失配） |

> ★ ★ **实施态 ≠ 真相（★ 本节的核心纪律）**：★ **代码 / 测试 / 运行时产物 / 证据链** = ★ **朝目标态实施的过程态** ⇒ ★ 它们回答「**离目标还有多远**」，★ **不回答「目标是什么」** ⇒ ★ ⇒ ★ **目标态绝不可被现状污染**（★ 否则「现状即合理」⇒ ★ 这是上一版把实现列为真相的**真正危害**）⇒ ★ ★ **本入口只导航真相区**（① ②）⇒ ★ **实施面在哪、gap 谁跟** ⇒ ★ 见 **§0.4**。

### 0.2 ★ 按意图的阅读路径（★ 「我想做 X ⇒ 读哪几篇」）

| 意图 | 读这几篇（★ 按顺序） | 读完得到 |
|---|---|---|
| ★ **第一次接触层次化 Plugin** | 本文 **§3**（目标形态结构事实）→ ★ **ADR-094 §Decision**（装配契约 r.1~r.16）→ 本文 **§1**（★ 实施态现状，★ 了解离目标多远） | 目标架构 + 权威出处 + 当前差距 |
| ★ **要写一个层 2 单元** | ★ **上游 `HIERARCHICAL-RTL-AUTHORING-GUIDE.md`**（★ **7 条纪律 D1~D7** · 粒度 · 三种写法风格）→ ★ **ADR-094 r.1~r.16**（装配契约）· **r.11 / r.16**（标量端口 + 层 2 基类）→ 本文 **§5**（命名规则）→ **§4**（归属↔语义耦合 ★ 承重） | 怎么建、怎么命名、哪些消费点会被归属变更影响 |
| ★ **要改「契约名 ↔ 节点名」或观测** | ★ **`parity-gate-contract.md` S-17 / S-17.1 / S-10** → ★ **ADR-094 D-3**（端口归属 / 契约身份 / 命名纪律） | 登记簿 / manifest / provider / error_kind 的单一真源 |
| ★ **要判断「某条规则归谁」** | 本文 **§0.5 目标态权威分布** → 对应权威文档 | 避免误改错文档 |
| ★ **要查上游机制**（归属 / 粒度 / 端口策略 / 等价性） | ★ 上游 **ADR-058~062**（★ 决策）→ 上游主题文档 + 2 份指南（★ 描述） | 上游侧的目标架构与决策依据 |
| ★ ★ **要查「实施到哪了 / 离目标多远」** | ★ **§0.4 实施区**（★ 代码 / 测试 / 判据脚本 / 证据链在哪）→ ★ 台账 **§9.2**（5 批 + 出口判据）→ change 的 **`tasks.md`**（★ AC 权威） | ★ **进度与 gap**（★ 注意：★ 这是**过程态**，★ 不是「目标是什么」） |
| ★ **要查未决 / 已知边界** | 本文 **§7**（残留 13 条） | 什么**没做**、什么**未核** |

### 0.3 ★ 真相全景表（★ **只含架构决策文档 + 架构文档** · ★ 每条带**锚点** + **时效**）

> ★ **读法**：★ 「**锚点**」= 读者据此**自己复核**该条真相的**具体位置**（路径 / 版本 / Status / 章节）⇒ ★ **MUST NOT** 代替读者下结论；★ 「**时效**」= 该锚点**什么时候核对过**（★ dated = 会过期的快照 ⇒ ★ 以权威文档头部为准）。

#### A. 架构决策文档（★ 本仓）

| 真相 | 锚点（★ 可复核） | 时效 |
|---|---|---|
| ★ **装配契约**（G-1r r.1~r.16 · 粒度 · 端口归属 / 契约身份 / 命名纪律 · `chlib` 边界 · 两阶段迁移） | `docs/architecture/adr/ADR-094-chmem-hierarchical-assembly-contract.md`（★ §Decision 表 · D-3 · D-6 两阶段迁移表 · §Verification V-27~V-43） | ★ 🚧 **Proposed v0.1**（★ 转正前置 = 治理收口：立项 + AC 回填） |
| ★ **两层范式 / 硬约束 / 观测量载体** | `docs/architecture/adr/ADR-091-chmem-open-core-port-architecture.md`（★ §D5.1 G-1a~G-1q · §D3a.7 接口载体 · §D3a.9 观测量载体 · §D5.1.3 = G-1r 索引） | ✅ Accepted **v1.16**（2026-11-07） |
| ★ **治理注记**（★ 各自主文档的决策**不变**，只加管辖域 / 时效注记） | `docs/architecture/adr/ADR-046`（★ FSM 豁免 · dated 附注）· `ADR-083`（★ 单一 source of truth · §4.2 管辖域）· `ADR-089`（★ 双后端门禁 · §D3 scope）· `ADR-092`（★ TLM 冻结 · §D4 管辖域）· `ADR-093`（★ TLM 移除 · 编号撞名澄清） | ✅ dated 附注（★ 2026-11-07） |
| ★ **决策注册表**（★ 全部 ADR 的索引） | `docs/architecture/adr.md`（★ ADR-091 行 · ADR-094 行 · ★ `tools/verify_adr.sh` 机检其漂移） | ✅ 活（★ 机检） |

#### B. 架构决策文档（★ 上游 · ★ **独立仓库 ⇒ 最易静默漂移**）

| 真相 | 锚点（★ 可复核） | 时效 |
|---|---|---|
| ★ **上游层次化决策**（★ ADR-094 的 Parent） | `CppHDL/docs/adr/ADR-058` **v2.17**（层次化 RTL 输出）· `ADR-059` **v2.3.2**（归属与命名）· `ADR-060` **v1.15**（端口策略与复用）· `ADR-061` **v1.6.4**（粒度由用户显式控制；★ Status 行 v1.6.4 · 版本史最新 v1.6.5，★ 决策零变更）· `ADR-062` **v1.4**（单元接口类型）—— ★ **全部 Accepted** | ★ **2026-11-07 核对** · ★ ★ **锚定 commit `824e2d0`**（★ 「核对过的」commit，★ **不是**「当前 HEAD」——★ 上游 HEAD 在落笔 20 分钟内推进 3 次 ⇒ ★ 「写当前 HEAD」是**注定过期的写法**；★ ★ **但层次化实现与 5 ADR 自该 commit 起 `git diff --stat` 为空 ⇒ 锚点语义仍成立**）⇒ ★ 残留 ⑬ |

#### C. 架构文档（★ 本仓）

| 真相 | 锚点（★ 可复核） | 时效 |
|---|---|---|
| ★ **本主题文档**（★ 当前态快照 / 结构事实 / 需求 / 残留） | `docs/architecture/hierarchical-plugin-architecture.md`（★ §3 目标形态 · §5 命名规则 · §4 归属↔语义耦合） | 🚧 Draft（★ §1 = dated 实施态快照） |
| ★ **门禁面契约**（登记簿 / 逻辑契约名清单 / error_kind / 通用总则） | `docs/architecture/parity-gate-contract.md`（★ **S-17** · **S-17.1**（机器可读 JSON）· **S-10** · **GN-1**） | ✅ **v1.8**（★ S-17 / S-17.1 **自 ADR-094 阶段 2 起生效**） |
| ★ **框架层架构**（★ Plugin 范式 + ★ §4.2 `ch::Component` 与 Plugin 的关系） | `docs/architecture/mixed/plugin-framework.md`（★ **非权威 · 仅作背景**）（★ §4「与 CppTLM / CppHDL 关系」· §4.2 Component） | ⚠️ **v1.2 · 2026-06-09 · §4.2 已过时**（★ 仍写「Plugin 仅在 TLM 模式生效 / 同一 IP 可有 TLM 版 + RTL 版」⇒ ★ 与 ADR-091 v1.16 两层范式 + ADR-083 单一 source of truth **冲突**）⇒ ★ **以 ADR-091 v1.16 / ADR-094 为准**（★ ★ 该文件**不在 `arch_paths`** ⇒ ★ architect 不改它 ⇒ ★ 残留 ⑮） |
| ★ **框架代码映射**（★ 架构概念 ↔ CppHDL 代码位置） | `docs/architecture/mixed/code-framework-mapping.md`（★ **非权威 · 仅作背景**）（★ **§3.1 组件模型表** = `Component`（★ RTL 组件基类）· `ch_module<T>`（子模块实例化）· `ch_device<T>`（顶层设备）· `ch.hpp`） | ✅（★ ★ **锚点是代码路径 ⇒ 随上游漂移** ⇒ 须按 M-6/M-7 复核；★ 该文自带「版本敏感性声明」） |
| ★ **总体架构 / 接口形态**（★ 描述性总览） | `docs/architecture/overview.md` · `docs/architecture/interface-design.md`（★ 两者含 v1.16 补注；★ `interface-design.md` §「层 2 单元接口」= 目标形态示范） | ✅ |
| ★ **版本节点架构演进 SSOT**（★ **层次化的版本落点在此**） | `docs/architecture/roadmap-evolution.md`（★ §4.5 **v0.11.0 目标表**含层次化装配行 · §8 **跨版本对比表**含层次化行） | ✅（★ 2026-11-08 已补） |

#### D. 架构文档（★ 上游）

| 真相 | 锚点（★ 可复核） | 时效 |
|---|---|---|
| ★ **上游主题文档**（★ 含等价性 helper 等收口记录） | `CppHDL/docs/architecture/2026-11-02-hierarchical-rtl-output.md` | ✅ 已转正（★ 同 commit 锚 `824e2d0`） |
| ★ **层次化 RTL 写作指南**（★ 用户视角） | `CppHDL/docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md`（★ §1 **7 条纪律 D1~D7** · §2 粒度（ADR-061）· §3 三种写法风格） | ✅ Active |
| ★ **组件生命周期指南**（★ `ch_device` vs `ch_module`） | `CppHDL/docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md` | ✅ Active |

> ★ **排除说明（★ 为什么有些架构文档不在表内）**：★ `docs/architecture/` 下其余文档与本主题**无直接耦合** ⇒ 不列（★ 避免稀释信号）：★ `performance-guide.md` / `testing-and-dse.md` 的「层次化」命中 = ★ **同词异义**（★ 「StatGroup 层次化统计组」/「层次化统计收集」⇒ ★ 与 Plugin 层次化无关）· ★ `tech-selection.md` · `ip-catalog.md` · `background-and-goals.md` · `error-handling.md` · `tlm-*.md`（★ 命中数 0）· ★ `m4-dse-cpufactory-real-pr-description.md`（★ 命中 0）· ★ `plugin-research/external-plugin-architectures.md`（★ **研究态**：★ 主题是 SpinalHDL 等**外部**架构 ⇒ 非本仓目标架构；★ 且 `arch-docs.json` 的 `exclude_paths` 已排除）· ★ ★ **`declarative-hybrid-framework.md`（★ 混合态 ⇒ 移出）**：★ 该文头部自声明「⚠️ **本文档混合了"实现框架"与"Phase 1 设计提案"**」⇒ ★ ★ **提案不是「已定的目标」⇒ 混入真相区会让未决内容冒充目标态** ⇒ ★ 故只作**背景参考**，不进真相区。

### 0.4 ★ 目标态 ⇄ 实施态（★ **真相区 vs 实施区**）

| | ★ **真相区（目标态）** | ★ **实施区（过程态）** |
|---|---|---|
| ★ 是什么 | ★ **理想架构目标**：该怎么设计 | ★ **现状**：现在长什么样 |
| ★ 载体 | ★ **架构决策文档 + 架构文档**（★ §0.3 A~D） | ★ **代码 / 测试 / 判据脚本 / 证据链**（★ 下表） |
| ★ 回答 | ★ 「**目标是什么**」 | ★ 「**离目标有多远**」 |
| ★ 漂移方式 | ★ 决策漂移（须取代流程）/ 描述漂移（须改文档） | ★ 实施推进 ⇒ ★ **代码变多、真相不变** |
| ★ 谁维护 | ★ architect（★ 目标态变更须走决策流程） | ★ builder（★ 实施面） |
| ★ **本入口是否导航** | ★ ✅ **导航**（★ §0.3） | ★ ★ **不导航为真相**（★ 只在此给指针 ⇒ 避免「现状即合理」误读） |

★ **实施区指针（★ 只给位置，不展开锚点 —— ★ 它不是真相）**：

| 实施面 | 位置 | 状态 / 意义 |
|---|---|---|
| ★ 框架装配面 | `include/cf/plugin/pipe_builder.h`（★ ctx 构造 · `elaborate()` · `declare_in/out`） | ★ 顶层端口创建点 owner == nullptr（★ 阶段 2 后切换） |
| ★ 层 2 唯一实例 + 桥接 | `ip/cpu/core/regfile_chmem.h` · `regfile_bridge_chmem.h` | ★ 现状 = 裸 `make_unique`（★ **帧残留** ⇒ ★ 与 ADR-094 r.2 的**差距**） |
| ★ 观测解析（门禁侧） | `tools/parity_runner/parity_sim_runner.cpp` · `tools/verilator_runner/` | ★ 现状 = 精确名相等（★ 阶段 1 前） |
| ★ 判据脚本 | `tools/run_catch2_with_baseline.sh` + `tools/baseline_known_failures.txt` · `tools/parity/*.py` · `tools/verify_adr.sh` · `tools/check_plugin_portability.sh` | ★ **判据的语义**在契约文档（★ 真相区）⇒ ★ 此处只是**实现** |
| ★ 验证面 | `tests/cpu/test_cpu_open_core_regfile.cpp` 等 | ★ 随代码演进 |
| ★ 证据链 | `.rddf/plans/backend-parity-open-core.md` §K17~§K33 · `.rddf/reviews/ADR-094-*.md` · 探针（★ **已失效** ⇒ 残留 ⑫） | ★ dated（★ 逐条带 file:line + 变异对照） |

> ★ ★ **gap 的跟踪归属**：★ 「目标 vs 现状」的**差距**不由本入口维护 ⇒ ★ 归 **台账 §9.2**（5 批 + 出口判据）+ ★ change 的 **`tasks.md`**（★ AC 权威）⇒ ★ ★ **本入口只保证「目标态的唯一真源可定位」，★ 不保证「实施进度」**（★ 后者会变 ⇒ 不属真相）。

### 0.5 ★ 目标态权威分布（★ 「唯一权威」= 同一主题只允许一处说了算）

| 目标态面 | ★ 唯一权威 | ★ 其余文档的角色 |
|---|---|---|
| 装配契约（怎么建 / 怎么成层 / 怎么登记） | **ADR-094** | ADR-091 §D5.1.3 只留索引；本文件只留结构事实 |
| 两层范式 / 硬约束 / 观测量载体 | **ADR-091 v1.16** | ADR-094 只承接「装配与端口纪律」 |
| 门禁面契约（登记簿 / 清单 / error_kind / 判据语义） | **`parity-gate-contract.md`** | ADR-094 D-3 只定方向 + 引用 |
| 上游机制（目标态） | **CppHDL ADR-058~062** + 上游 2 份指南 | 本仓 ADR 只承接与澄清 |
| ★ **当前态快照（★ 非目标态）** | ★ **实施面（`tasks.md` / plan）** | ★ 本文件 §1 = **截至 2026-11-07 的 dated 实施态快照**（★ 实施前的事实）⇒ ★ **不由 builder 持续维护**（★ 防职责冲突 ⇒ 防腐化） |

### 0.6 ★ 导航表的维护规则（★ 防腐化）

| 规则 | 内容 |
|---|---|
| ★ **M-1 单一真源** | ★ 本表是**指针表** ⇒ **MUST NOT** 在此重述任何权威文档的结论（★ 重述 = 必然漂移） |
| ★ **M-2 变更触发** | ★ 新增 / 删除 / 改名 / **状态翻转**（★ 如 ADR-094 转 Accepted、change 立项）**任一相关文档** ⇒ MUST 同步本表（★ ★ **两张表分别对待**：★ **真相区文档（§0.3）** 的变更 ⇒ 同步 §0.3；★ **实施态条目（§0.4）** 的变更 ⇒ 同步 §0.4 ⇒ ★ ★ **实施态变更不触发 §0.3 改动**（★ 与 M-8 一致）★ 判据：★ 该文档是否被 §0.2 任一阅读路径或 §0.5 任一权威面触及）· ★ ★ **无触发机制** ⇒ ★ **改名 / 状态变更方 SHOULD 反向 grep 本文件**（★ `grep -rn "<旧名>" docs/ .rddf/ openspec/`）⇒ ★ 最低成本纪律 |
| ★ **M-3 反向可达** | ★ 任一相关文档 SHOULD 在其「Related / 关联」段回指本文件 ⇒ ★ **读者从任一入口进入 ⇒ 都能找到本文件**（★ 当前仅 3 处回指 ⇒ ★ 缺口见 §7 残留 ⑨） |
| ★ **M-4 失效检测（★ 诚实口径 · 无门禁兜底）** | ★ ⚠️ **本表条目当前零机检覆盖** —— ★ `tools/doc_link_check.sh` 的**检查 2（反引号路径）仅覆盖根级 3 文档**（`AGENTS.md` / `README.md` / `CONTRIBUTING.md`）· ★ **检查 1 只认 markdown 链接语法**（★ 形如「方括号文本 + 紧接圆括号路径」的 `.md` 链接 ⇒ ★ 本表条目**不是**该形态 ⇒ 不被覆盖）· ★ `SCAN_PATHS` **不含** `.rddf/` ⇒ ⇒ ★ 本表条目**既不被正向检查、也不被反向检查覆盖** ⇒ ★ **失效检测 = 纯人工**（★ 复查本表时逐条 `ls` 一次）⇒ ★ 机检方案见 §7 残留 ⑩ |
| ★ **M-5 更新归属** | ★ 本节（§0）由 **architect** 维护（★ 低频）；★ §1~§11 的**结论性内容**归各权威文档 ⇒ ★ ★ **本文件不再承载需高频刷新的状态** ⇒ ★ **§0.3 状态列 = dated 快照**（★ 见该列表头）⇒ ★ **architect MUST NOT 为追平 builder / planner 的状态事件而维护本表**（★ 那正是 §0 引言要防的「高频内容拖累低频内容」）⇒ ★ **状态以各权威文档头部为准** |
| ★ **M-6 锚点维护（★ 真相入口的生命线）** | ★ §0.3 每条真相 MUST 带**可复核锚点**（路径 + 版本 / Status / 章节）⇒ ★ **锚点失效 = 该条真相不可核实** ⇒ ★ ★ **锚点 MUST 与被指对象同步更新**（★ 文档改名 / 版本推进 / 上游 commit 变 / 章节移位 ⇒ 同步改锚点）⇒ ★ 「时效」列的 dated 值 = **上次核对日期** ⇒ ★ **dated 越久 ⇒ 越不可信** |
| ★ **M-7 上游漂移守卫（★ 独立仓库专项）** | ★ 上游 CppHDL 是**独立仓库** ⇒ ★ 我方文档引用上游时 **MUST 带 commit 锚点**（★ 单写「全 Accepted」= **不可核实**）⇒ ★ ★ **上游 HEAD 变化时 MUST 复核受影响的锚点**（★ 触发 = 上游 rebase / 新 commit 改写层次化实现）⇒ ★ ★ **已实证一次漂移**：★ 台账记录的 `dbc9801` → ★ 实际 HEAD `824e2d0`（★ 2026-11-07 实测）⇒ ★ 该漂移**未影响任何决策**（★ 差异落在上游**非层次化面**：planner 文档面 + 1 行 `.gitignore`）⇒ 但**机制缺口成立** ⇒ 残留 ⑬ |
| ★ ★ **M-8 真相区 / 实施区不混（★ 本版新增 · ★ 守住 §0.1 的核心纪律）** | ★ **本入口 MUST NOT 把实施态（代码 / 测试 / 运行时产物 / 证据链）列为真相** ⇒ ★ **实现状态只允许出现在 §0.4 的实施区指针表**（★ 且只给位置，不展开）⇒ ★ ★ **理由**：★ 目标态一旦被现状污染 ⇒ ★ 「现状即合理」⇒ ★ 约束失效（★ 上一版的错误正是此）⇒ ★ ⇒ ★ **新增代码 / 测试 / 证据 ⇒ 一律落 §0.4，不落 §0.3** |

---

## 1. ★ 实施态快照（★ **当前实现** · 2026-11-07 实测 · ★ **非目标态**）

> ★ ★ **本节 = 实施态**（★ 「现在长什么样」），★ **不是目标态**（★ 「该长什么样」）⇒ ★ 目标态在 **§0.3 真相区**（★ 架构决策文档 + 架构文档）⇒ ★ ★ **本节的用途 = 描述「目标 vs 现状」的差距**（★ 差距的跟踪归属见 §0.4：★ 台账 §9.2 + change `tasks.md`）⇒ ★ ⇒ ★ **本节内容 MUST NOT 被当作目标态引用**（★ 否则「现状即合理」⇒ 约束失效）。

### 1.1 两条 RTL 出口（★ 上游侧已就绪，★ ChipForge 侧只用第一条）

| 入口 | 产物 | 状态 |
|---|---|---|
| ★ **扁平** `ch::toVerilog(filename, ctx)`（`CppHDL/include/codegen_verilog.h:43`） | **单个** `module top` | ✅ 上游**逐字节未变**；★ **ChipForge 的 parity 门禁今天只走这条** |
| ★ **层次化** `ch::toVerilogHierarchical(out_path, root, options)`（`CppHDL/include/hierarchy_verilog.h:129`） | **单文件多 module**（子在前、根在后；默认全 inline ⇒ 与扁平**逐字节一致**） | ✅ 上游已落地并归档；★ **ChipForge 侧尚未使用** |

★ 上游另有 `try_initialize_with_verilog(verilog_path, ports, data_map)`（`CppHDL/include/core/verilator_backend.h:121`）可仿真层次化产物，★ 但 **ChipForge 的 Verilator 路径是自建的**（`tools/verilator_runner/cpu_verilator_sim.cpp:295/317-322`：扁平 `top.v` + `verilator --cc --top-module top` 单文件）⇒ ★ **层次化产物进不了 ChipForge 现有门禁路径**（详见 §7 残留 ①）。

### 1.2 ★ ChipForge 侧层 2（`ch::Component`）的**当前态**

| 项 | 当前态 | 证据 |
|---|---|---|
| 层 2 实例 | ★ **仅 1 个** = `RegFileComponent`（`ip/cpu/core/regfile_chmem.h:93`） | 读码 |
| 层 2 接口 | ★ **逐字段标量端口**（`__io` 的 `ch_in`/`ch_out`，7 个；名字带 `regfile_` 前缀） | `regfile_chmem.h:103-117` |
| 桥接 | ★ `RegFileBridgePlugin`（`ip/cpu/core/regfile_bridge_chmem.h`）在 `setup(pb)` 内建单元 + 驱动输入 + 发布 payload + 驱动顶层观测端口 | `regfile_bridge_chmem.h:124` |
| ★ **装配形态** | ★ **裸 `make_unique<RegFileComponent>(nullptr,"regfile")` + `build(pb.ctx())`** ⇒ **无 `add_child`** ⇒ **作用域帧残留**（★ 实测：该单元**之后**创建的节点 owner = 该单元、名字带 `regfile.` 前缀） | `regfile_bridge_chmem.h:124` · `CppHDL/include/component.h:140-156`（明写限制）· 探针 `/tmp/opencode/probe_scope.cpp` |
| 顶层端口 | ★ `PipeBuilder::declare_in/out`（`include/cf/plugin/pipe_builder.h:538/550`）⇒ ★ 创建点 **owner == nullptr**（★ 无组件作用域）⇒ **节点名无前缀** | 读码 + 探针 |
| ctx 归属 | ★ **宿主持有**（`PipeBuilder(ch::core::context* ctx)`，`pipe_builder.h:476`）⇒ ★ 门禁侧 `ch::core::context ctx("parity_sim_runner")`（`tools/parity_runner/parity_sim_runner.cpp:327`）/ `("cpu_verilator_sim")`（`tools/verilator_runner/cpu_verilator_sim.cpp:271`）⇒ ★ 这就是 **G-1p**（Component MUST 共用宿主 ctx）的落点 | 读码 |
| 观测解析 | ★ **精确名相等**（`node->name() == name`，`parity_sim_runner.cpp:249-255`）⇒ ★ 契约名 = ctx 节点名（★ 无解耦层） | 读码 |

### 1.3 ★ 上游已消解的四个缺陷 ⇒ **ChipForge 侧尚未消解**（对照表）

| # | 上游缺陷（2026-11-02 实测） | 上游消解方式（当前态） | ★ **ChipForge 侧现状** |
|---|---|---|---|
| ① | 成员节点归到父组件 | 归属判定的唯一实现 = `Component::node_owner()`（`CppHDL/src/component.cpp:16-26`）= 最内层「构造 / build 作用域」 | ★ **归属规则可用**（上游机制）★ 但 ★ **ChipForge 不用 `create_child` ⇒ 帧残留 ⇒ 归属错**（1.2） |
| ② | 成员 `ch_module` 挂祖父 | 同上 ⇒ 组件树登记与 build 解耦（`add_child` MUST 先于 `build(ctx)`） | ★ **未登记** ⇒ 上游层次化分析会 **FATAL**（`CppHDL/src/hierarchy_analysis.cpp:211-227`） |
| ③ | 深度 ≥3 前缀重复 | 全路径拼接点收敛为**一处** = `build_hierarchical_name(parent, local_name)`（`CppHDL/include/component.h:266`） | ★ **可用**（上游机制）；★ 但节点名拼接**另有一处**（`lnodeimpl.cpp:33-37`）⇒ ★ **两处行为不一致**（组件侧空父路径会跳前缀、节点侧无条件加） |
| ④ | 成员 `ch_module` 因 `context()==nullptr` 早退 | 构造期 ctx 与 build 期 ctx 分离（`construction_ctx_`）⇒ 无 active parent 处的装配**合法** | ★ **可用**（上游机制）；★ ChipForge 不用 `ch_module` ⇒ 不受影响 |

### 1.4 已排除的岔路（★ 只列指针）

★ ADR-094 §Options 的 **A~F** 六行（维持全扁平 / 全成层交付 / **默认 inline + 显式声明（采纳）** / 每单元 `ch_device` / 顶层端口维持无 owner / 改上游解耦 owner-命名）⇒ ★ **理由与取舍在 ADR-094**，本文件不重述。

---

## 2. 下游需求清单（★ ChipForge 侧对框架/上游的需求）

| # | 需求 | 证据 | 影响落点 |
|---|---|---|---|
| **N1** | ★ 层 2 单元 MUST 能成为**独立 Verilog module**（交付 / 可单独综合 / 可复用） | 上游 ADR-058（层次化输出） | ADR-094 D-1/D-2 |
| **N2** | ★ 装配点**可无 active parent** ⇒ 「成层判定」与「装配点」**解耦**；★ 但组件 MUST 登记进组件树（否则上游 FATAL） | 上游 ADR-061 决策⑥ · `hierarchy_analysis.cpp:211-227` | ADR-094 r.2 / r.7 |
| **N3** | ★ Component MUST 共用**宿主的 ctx**（子 ctx ⇒ node-id aliasing ⇒ 组件 output 端口从 simulator `data_map` 消失 ⇒ 观测静默读不到） | 上游 `CppHDL/include/device.h:12-19`（上游自己踩过） | ADR-091 **G-1p** · ADR-094 r.3 |
| **N4** | ★ 顶层端口名 MUST NOT 与内部 `ch_reg` / `ch_node` 同名（★ **v1.16 扩面**：判定对象 = **ctx 节点名** vs **顶层端口名**） | ★ `CppHDL/include/codegen_naming.h:72-88`（★ **`build_verilog_name_table` = 全局去重的真实实现**：★ 同 base 名 ⇒ 加 `_N` ⇒ 端口被改名 ⇒ 两后端端口名不一致）· `CppHDL/src/codegen_verilog.cpp:71-79`（★ 扁平端口收集） | ADR-091 **G-1o** · ADR-094 D-3.6 |
| **N5** | ★ **节点名唯一性** MUST 有 **opt-in** 的创建期 fail-fast（解释器侧不改名 / Verilator 侧改名 ⇒ 按名解析会**静默采到错节点**） | 上游 `NodeNamePolicy::kRejectDuplicates`（`CppHDL/include/core/context.h:51-55`） | ADR-091 **G-1o** / V-25 · ADR-094 D-6 阶段 1 |
| **N6** | ★ **公共头 ODR 安全**（★ 上游**已修**：`build_hierarchical_name` 移到 `component.h:266` 且 `inline`）⇒ ★ ChipForge 的 G-1b **换根**（ODR 根消失 ⇒ 理由 = include 面最小化 + `ch::detail` 命名二义） | `CppHDL/include/component.h:266` | ADR-091 **G-1b** · ADR-094 **V-34** |
| **N7** | ★ 观测 MUST 按**逻辑契约名**解析（★ 不再要求「契约名 == ctx 节点名」）⇒ ⇒ 契约身份与节点名**解耦** | 上游 `ch_device` 的顶层端口**本身就带 `top_` 前缀**（`top.<port>` → `top_<port>`）⇒ ★ 「裸名」是 ChipForge 侧约定 | ADR-094 **D-3.1~D-3.9** · 契约文档 **S-17 / S-17.1** |
| **N8** | ★ 层 2 单元接口 MUST 是**逐字段标量端口**（★ bundle / `ch_stream` 的成员是 `type_proxy`、**不是端口** ⇒ 被上游接口白名单排除；且 `ch_in<ch_stream<…>>` 退化成**整宽 blob 的单个 `type_input`** ⇒ 语义空壳） | `CppHDL/src/hierarchy_port_check.h:60-90` · 实测探针 | ADR-094 **r.11 / r.16** · D-5 |

---

## 3. ★ 理想目标态（★ 目标形态的结构事实 · 验证形态 · 版本落点）

### 3.1 语义定义（★ 权威在 ADR-094 D-1/D-2）

- **模块边界 = 所有权边界**；RTL module 树 ≡ **被显式声明为成层**的组件子树（★ 默认 inline）。
- **装配**与**成层判定解耦**：装配点可无 active parent；★ 目标形态 = 单元经 `root.create_child` 登记并截帧（★ 权威 = **ADR-094 r.2**）。
- ★ **归属**：★ **决策真源 = 上游 ADR-059**（节点归属与组件命名）；★ **现行机制实现 = 上游 `Component::node_owner()`**（★ ★ **实现不是真源** —— ★ 真源是决策文档，★ 实现只是其机制载体）⇒ ★ ChipForge 的 root-scope 覆盖**整个构建期**（`build()` + `elaborate()`）⇒ 插件级节点归 root、单元内部节点归单元。

### 3.2 模块关系（★ 结构图）

> ★ ★ **本图 = 目标形态**（★ **不含**实施态时点语汇 ⇒ ★ 「哪条路径今天已接入」见 **§1.1**）。

```
        ┌──────────────────────── 宿主（runner / factory）────────────────────────┐
        │  ch::core::context ctx（★ 宿主持有 ⇒ G-1p 的落点）                        │
        │        │                                                                  │
        │        │  PipeBuilder(ctx)                                                │
        │        ▼                                                                  │
        │  ┌──────────────────────────── 根组件（root Component）──────────────────┐ │
        │  │  ★ 实例名 = 契约常量 `cpu` · 构造包 ConstructionFrame · 共用同一 ctx    │ │
        │  │  children: [ unit_1 … unit_n ]                                        │ │
        │  └───────┬───────────────────────────────┬───────────────────────────────┘ │
        │          │ create_child（★ 登记 + 截帧） │                                │
        │          ▼                               ▼                                │
        │   ┌─────────────┐                ┌─────────────┐                          │
        │   │  层 2 单元   │                │  层 2 单元   │   ★ ch::Component      │
        │   │ __io 标量端口 │                │ __io 标量端口 │   ★ 可 CH_HIERARCHY_LAYER │
        │   └──────┬──────┘                └──────┬──────┘                          │
        │          │ 端口直连（组合节点）            │                                │
        │          ▼                               ▼                                │
        │   ┌────────────────────────────────────────────────────────────┐            │
        │   │  桥接 plugin（层 2 → 层 1 的唯一通道）                        │            │
        │   │   ① unit holder ② port connector ③ output publisher           │            │
        │   └───────┬────────────────────────────────────┬───────────────┘            │
        │           │ 层 1 通道（Payload<T> + PipeNode）  │ 顶层端口（declare_in/out）  │
        │           ▼                                    ▼                            │
        │      stage 网（at_stage / canonical_stage_order）   顶层 ch_out（观测载体）    │
        └──────────────────────────────────────────────────────────────────────────────┘
                     │                                    │
                     ▼                                    ▼
        ┌────────────────────────┐          ┌──────────────────────────────┐
        │ 扁平 RTL（★ 门禁路径）    │          │ 层次化 RTL（★ 交付路径）        │
        │ ch::toVerilog → module top│          │ toVerilogHierarchical → 多 module│
        └────────────────────────┘          └──────────────────────────────┘
```

### 3.3 关键数据结构（★ 契约本体在门禁文档，★ 这里只列「有什么、归谁」）

| 结构 | 归属 | 落点 |
|---|---|---|
| **组件树**（root + units） | 框架（`PipeBuilder` 持 root；`create_child` 登记） | ADR-094 r.1 / r.2 |
| **契约登记簿**（逻辑契约名 → 节点绑定） | 框架产出（`elaborate()` 收尾）⇒ 门禁消费 | ADR-094 D-3.2 / D-3.10 · 契约文档 **S-17 / S-17.1**（★ 字段集在 REG-2，此处不重述） |
| **绑定提供者**（`manifest` / `identity`） | 框架提供实现；★ **调用方**按语境决定装哪个 | ADR-094 r.15 / D-3.9 / D-3.11 · 契约 **REG-5** |
| **两道机械闸**（闸① 框架内 / 闸② gate 侧） | 框架 + 门禁 | ADR-094 D-3.7 · 契约 **REG-3** |
| **ctx 节点表** | 上游（单一 ctx ⇒ 单一节点表 ⇒ 无 node-id aliasing） | ADR-091 G-1p |

### 3.4 装配时序（★ **指针** —— ★ 不重述条款）

| 阶段 | 目标态不变量（一句话） | ★ 权威 |
|---|---|---|
| ★ **阶段 1** | ★ 顶层端口与插件级节点的 owner 仍为 `nullptr`（★ 单元内部节点归单元）⇒ ★ 观测按**精确名**解析（三态化）⇒ ★ **不产 manifest / 不走闸②** | ★ **ADR-094 D-6 阶段 1** |
| ★ **阶段 2** | ★ owner 切换（root-scope 覆盖整个构建期）+ 登记簿与 manifest 上线 + **归属守卫**生效 —— ★ **不可拆的一批** | ★ **ADR-094 D-6 阶段 2** |

> ★ **两阶段的具体步骤与前置（★ 含同名普查的前置条件）⇒ 一律以 ADR-094 D-6 为准**；★ 本文件**不重述条款**（★ 避免两处全文 ⇒ 必然漂移）。

### 3.5 ★ 验证形态（★ 目标态的「怎么证明对」）

> ★ **目标态的验证 = 多维互补，★ 缺一不可**（★ 各抓不同类缺陷 ⇒ ★ 不可互相替代；★ ★ **互补证据 = ②③④**（★ 变换 / 后端 / 可交付性）· ★ **① 是装配规则自身的门禁**（★ 框架侧））：

| 层 | 判什么 | ★ 机制锚 / 权威 | ★ 抓不到的（★ 判别力边界） |
|---|---|---|---|
| ① **装配契约** | 装配 / 成层 / 登记 / 命名 是否**符合规则** | ★ **判据全集 = ADR-094 V-27~V-43**；★ 其中**等价性判据（V-28 / V-29）的机制锚见 ②** · **迁移期判据（V-35 / V-36）阶段 2 验收后退役**；★ 其余 = 框架侧机检 | ★ 不判「网表行为对不对」· ★ **迁移安全已在 ① 全集内（V-35 / V-36）⇒ 不存在第五个正确性维度** |
| ② ★ **层次化 ≡ 扁平** | ★ **「层次化变换」本身**有没有引入行为分叉（★ 单后端 · 两份产物比较） | ★ ★ **上游 `ch::hier_eq`**（`CppHDL/include/hierarchy_equivalence.h`）—— ★ namespace `ch::hier_eq` · ★ **7 项闭合公共面**：`DrivenInput` / `Observation` / `ProductSet` / `Result` / `emit` / `compare` / `make_observation` · ★ **fail-closed**（★ 错误前缀如 `observation-not-visible` / `empty-observations` / `emit-failed`） | ★ ★ **抓不到「扁平/层次化共享的发射底座算错」**（★ 两产物同源 ⇒ 一起错）⇒ ★ 上游明写该 helper **MUST NOT** 替代双后端 parity |
| ③ **双后端 parity** | ★ **两套执行后端**不一致（★ 异源 ⇒ 抓「后端算错」） | ★ **ADR-089**（★ 同一模型 × CppHDL Simulator / Verilator 差分执行）· 门禁面契约 `parity-gate-contract.md` | ★ ★ **抓不到「变换算错」**（★ 同源 ⇒ 一起对）⇒ ★ ★ **故 ② 与 ③ 互补**：★ ② 抓「变换」，③ 抓「后端」 |
| ④ ★ **可交付性** | ★ 层 2 单元**能否独立复用 / 独立验证** + ★ 产物**是否可用**（★ 可综合 / 可被工具链接受） | ★ **V-30**（★ `ch_device<RegFileComponent>` 独立复用形态）· ★ **V-33**（★ 产物 lint / 工具链 exit 0） | ★ 不判「行为对不对」（★ ⇒ 那是 ② ③） |

⇒ ★ ★ **结论（★ 目标态的验证形态）**：★ **层次化交付 SHOULD 同时具备 ② 与 ③ 的证据**（★ 缺 ② ⇒ 「层次化变换错了」无人发现；★ 缺 ③ ⇒ 「后端算错了」无人发现）⇒ ★ ① 是**装配规则**自身的门禁（★ 框架侧）· ④ 是**可交付性**门禁。
> ★ ★ ★ **★ 诚实标注（本条是**目标态要求**，★ 不是已登记的交付门禁）**：★ ★ **③ 的「层次化产物适用面」未在 `ADR-089` / `parity-gate-contract.md` 登记**（★ 读码实测：ADR-089 全文**无**层次化适用面声明；★ 契约文档 grep「层次化 / hierarch」**零命中**）⇒ ★ ⇒ ★ 故本条**故意用 SHOULD 而非 MUST**：★ ★ **MUST 的门禁地位需先在 ADR-089 / 契约文档登记适用面后才成立**（★ 归 architect 的 backlog ⇒ 见 §7 残留 ⑭）。★ ★ 同理，★ ② 的依据（★ ADR-094 V-28 / V-29）**已登记** ⇒ ★ ② 侧无此缺口。
⇒ ★ ★ **机制锚点缺口（★ 本节新发现）**：★ 上游 `ch::hier_eq` 于 2026-11-06 落地，★ 但 ★ **ChipForge 侧全仓 0 引用**（★ `grep -rn "hier_eq" docs/ .rddf/ ip/` 零命中）⇒ ★ ★ ADR-094 **V-28 / V-29**（层次化等价性判据）⇒ ★ **判据存在而机制锚未接** ⇒ ⇒ ★ **方向已定**（★ 本节 ② 已把 `ch::hier_eq` 定为等价性判据的**机制锚**）⇒ ★ ★ **缺的是 ADR-094 侧的登记**（★ V-28 / V-29 补一句机制锚注 ⇒ ★ **属 ADR-094 范围**（★ V-28 / V-29 是其判据）⇒ ★ **无需另立 ADR**）⇒ ★ ★ **另需登记**：★ ③ 的「层次化产物适用面」在 `ADR-089` / 契约文档**尚无登记** ⇒ ★ ⇒ ★ 故上文结论句用 **SHOULD 而非 MUST** ⇒ ★ 归 backlog（★ 见 §7 残留 ⑭）。

### 3.6 ★ 版本落点与延迟项（★ 目标态何时落地）

| 项 | 内容 | ★ 权威 |
|---|---|---|
| ★ **版本落点**（★ **建议** —— ★ architect 按批次依赖推定） | ★ **v0.11.0 窗口** = ADR-094 装配契约 + 阶段 1（框架地基 / 观测登记 / 三态化）⇒ ★ **v1.0.0** = 阶段 2（owner 切换）+ 首批成层单元（ALU） | ★ ★ **已写入** `docs/architecture/roadmap-evolution.md` §4.5 + §8（★ 2026-11-08 architect 补，★ 两处均标「待 objective 登记」）⇒ ★ ★ **待 planner 登记进** `objective-soc-cpu-v0110-launch.md` ⇒ ★ ★ **本行是建议、★ 非已决归属**（★ ADR-094 与台账只有**批次**、★ 无版本映射 ⇒ ★ 不自引为「权威」） |
| ★ **落点口径** | ★ v0.11.0 行是**「窗口内目标」而非 archive 硬门禁**（★ 沿用既有口径）⇒ ★ 不因层次化未完成而单方面加硬 gate | ★ `.rddf/roadmap/strategy.md`（★ 版本节点表的既有口径，★ 该文件**不在 `arch_paths`** ⇒ ★ architect 不可写 ⇒ ★ 登记归 planner） |
| ★ **已知延迟项**（★ 目标态的「不做/未决」面） | ★ 多 root / 多核形态（★ 跨 ctx 多 root ⇒ 违 G-1p ⇒ 另立 ADR）· ★ FSM 私有 ctx 的归属形态未核 · ★ r.14 的「创建时机维度」缺口 | ★ **§7 残留 ② / ③ / ④**（★ 不在本文重述） |

> ★ ★ **本节不含实施批次 / 退出判据 / AC**（★ 那些属台账 §9.2 + change `tasks.md`）⇒ ★ 本节只回答「**目标态落在哪个版本**」。

---

## 4. 归属 ↔ 语义的耦合（★ **承重** · 实施期必查）

★ **归属不是纯命名属性** ⇒ 它被用于**语义分流** ⇒ ★ 「改归属 ⇒ 改行为」：

| 消费点 | 用法 | ★ owner 变更的后果 |
|---|---|---|
| `CppHDL/src/simulator_init.cpp:94-100` | `!node->get_parent()` ⇒ 分到 `input_instr_list_` / `combinational_instr_list_` | ★ **顶层 `type_input` 会从 input 表迁到 combinational 表** ⇒ ★ **解释器侧求值顺序改变** ⇒ ⇒ V-35（迁移期语义回归）必需（★ Verilator 后端**不消费**这两张表 ⇒ 后端侧不受影响） |
| `CppHDL/include/core/io.h:370/395/420/442` | 4 处方向守卫以 `current_component()`（= `node_owner()`）为比较对象 | ★ root-scope 覆盖连接操作 ⇒ 「current module = root」⇒ ★ 守卫语义**由错变对** ⇒ ★ 触发集**必然变化** ⇒ V-36 判「**无新增** + 阶段 2 触发集**为空**」 |
| `CppHDL/src/core/lnodeimpl.cpp:30-35` | `name_ = parent_->name() + "." + name`（★ **无条件**） | ★ owner 非空 ⇒ **节点名带前缀** ⇒ ⇒ 契约名不能靠节点名（⇒ 登记簿解耦，N7） |
| `CppHDL/src/codegen_verilog.cpp:71-109` | 扁平端口规则：`type_output` **无条件**全部进顶层；`type_input` 有 driver 则排除 | ★ 组件 `ch_out` **确实污染**扁平顶层 ⇒ ⇒ 「顶层端口不污染」的旧表述**是错的**（ADR-091 v1.16 已订正） |
| `CppHDL/src/hierarchy_analysis.cpp:211-239` | 前置校验：owner MUST 在 root 子树内；已登记 MUST 已 build | ★ 未登记 / 未 build ⇒ **FATAL** ⇒ ⇒ 现有装配形态**会被拒绝** ⇒ ⇒ 归属守卫前移（r.14） |

★ ★ **一处「看得见但不在主 ctx」的例外（★ 本轮新发现）**：★ `chlib::ch_state_machine` 的私有 ctx —— `include/cf/plugin/multi_cycle_fsm.h:93` 与 `ip/cpu/plugins/mul_div_fsm_chmem.h:108` 会 `new ch::core::context("multi_cycle_fsm_ctx")` ⇒ ⇒ ★ **FSM 的状态不在宿主 ctx 内** ⇒ ⇒ 层次化分析**看不见它** ⇒ ★ ⚠️ **残留 ④**：★ 「层 2 单元使用 FSM 的归属形态」**未核** —— ★ **形态 A（`FsmBase` fallback）**：私有 ctx ⇒ **状态连宿主 ctx 都不进**；★ **形态 B（注入宿主 ctx）**：owner 归属（root vs 单元）**未定义**；★ 两种形态**均未实测** ⇒ ★ 故**不能**断言「层 2 单元用 FSM ⇒ 状态不在该单元的 module 里」。

---

## 5. 命名规则（★ **目标态（阶段 2 起）**；★ 与 §1.2 的「**实施态**（节点名无前缀）」**不矛盾** —— 两者是**两个阶段**的形态）

| 对象 | 规则 | 例 |
|---|---|---|
| **root 实例名** | ★ 契约常量 **`cpu`**（★ 改名 = 契约变更：需 ADR + 重基线） | `cpu` |
| **逻辑契约名** | ★ **清单唯一真源** = 契约文档 **S-17.1 `node_keys`**（★ 35 项）⇒ ★ 报告 / 白名单 / 脚本参数一律用它 | `reg_1` · `pc` · `mem_sig` |
| **ctx 节点名** | ★ owner 非空 ⇒ 带路径前缀（`lnodeimpl.cpp:30-35`）⇒ ★ **与逻辑契约名解耦** | `cpu.reg_1` |
| **Verilog 端口名** | ctx 节点名的 sanitize（`.`→`_`，`CppHDL/include/codegen_naming.h:40-44`） | `cpu_reg_1` |
| **单元端口名** | ★ **推荐**带单元局部名前缀（避免全局去重加 `_N`）⇒ ★ 非 MUST（契约已解耦） | `regfile_rs1_addr` |
| **隐式端口** | ★ 只出现在**非顶层 module**（`__if_` / `__x_` / `__up_`） | `__if_payload` |

---

## 6. 与既有文档的关系

| 文档 | 关系 |
|---|---|
| **ADR-094** | ★ **决策权威**（装配契约 / 粒度 / 端口纪律 / 交付定位 / `chlib` 边界 / 迁移顺序）⇒ 本文件只记结构事实 |
| **ADR-091 v1.16** | ★ 两层范式 / 硬约束 G-1a~G-1q / 观测量载体（§D3a.9） |
| `parity-gate-contract.md` | ★ **门禁面单一真源**（S-10 error_kind + GN-1 · S-17 登记簿 · S-17.1 清单） |
| `docs/architecture/interface-design.md` / `overview.md` | ★ 描述性总览 ⇒ ★ 本文件是**层次化 Plugin 侧**的专门主题文档 ⇒ 两者用**指针**互链（★ 不复制条款） |
| `.rddf/plans/backend-parity-open-core.md` §K17~§K33 | ★ **推导与证据**（★ **K34~K36 待 builder 起草** ⇒ 台账 §9.2 的 ①-D 结论落点） |
| `openspec/changes/backend-parity-open-core` · `parity-observation-ambiguity-fail-closed` | ★ 实施面（★ 归 builder / planner） |

---

## 7. 已知残留（★ 本方案**不做 / 未决**）

| # | 残留 | 状态 / 归属 |
|---|---|---|
| ① | ★ **层次化产物进不了 ChipForge 现有 parity 门禁**（自建单文件 Verilator 路径 + `top__DOT__*` 穿透读）⇒ ★ 层次化定位 = **交付 / 可单独综合**（★ ADR-094 D-4 明确：★ **不作**门禁输入） | ⚠️ 已知边界 |
| ② | ★★ **r.14 的「创建时机维度」缺口**（★ oracle 终审点名的**最实质残留风险**）：★ r.14 判据 = 「owner ∈ {root} ∪ {已登记单元}」，★ **不查「该节点是否该在这个时刻创建」** ⇒ ★ 全局 root-scope 下，`at_stage` 闭包内**误建**的节点归 root ⇒ **合法化为不可见**（★ 闸①只扫 `type_output` ⇒ 中间类型节点 invisible） | ⚠️ 已登记长期 backlog（★ 触发条件 = 幽灵节点被实际观测到 ⇒ 升级 ADR 修订） |
| ③ | ★ **多 root / 多核 SoC 路径未给**：★ 「多 root」≠「多 ctx」⇒ ★ 多核 = **一 root + 多单元**；★ **跨 ctx 多 root 不在本契约范围**（违 G-1p）⇒ 需**另立 ADR** | ⚠️ 已知边界 |
| ④ | ★ **FSM 私有 ctx**（§4 末）⇒ ★ **形态 A（FsmBase fallback）**：私有 ctx ⇒ 状态连宿主 ctx 都不进；★ **形态 B（注入宿主 ctx）**：owner 归属未定义；★ 两种形态**均未实测**。★ ★ **当下版补充证据（oracle 终审读码）**：★ `ip/` 业务侧**全仓无 `set_ch_context` 注入** ⇒ ★ 当前业务路径**确实**走形态 A；★ 且 `mul_div_fsm_chmem.h:213-224` 的输出经 `ctx_swap` 读**私有 ctx** ⇒ ★ **它如何被宿主 eval list 接纳未点破** ⇒ ★ **builder 核查建议（≤30 min，①-A 前）**：★ mul_div 是否进 parity 观测面 · 私有 ctx 节点如何进宿主 DAG | ⚠️ **未实测**（★ 已登记） |
| ⑤ | ★ **端口命名契约无静态护栏**（★ 上游**已收口其自身**的护栏；★ **ChipForge 侧**层次化 Plugin 单元仍**从零开始**、端口命名**仍无静态护栏**）⇒ ★ 依赖 ADR-094 r.16（层 2 基类编译期校验）+ 门禁 Check | ⚠️ 部分覆盖 |
| ⑥ | ★ **层次化路径在既有语料上零覆盖**（★ 上游**已收口其自身**的等价性语料，但★ **仅限 5 个专用 DUT**；★ **ChipForge 侧**仍**从零开始**、既有语料零覆盖） | ⚠️ 已知边界 |
| ⑦ | ★ ADR-046 §1.2 的「`ch_state_machine` 不能在 simulator 内推进」为**过时陈述**（★ 已附 dated 注 + 2026-11-07 实测）⇒ ★ 正式订正待其下次修订 | ⚠️ 已登记 |
| ⑧ | ★ `docs/architecture/adr.md` §2.4 统计**截止 ADR-086**（★ ADR-087 起未纳入）⇒ ★ 口径已显式登记，★ 补登归 backlog | ⚠️ 已登记 |
| ⑨ | ★ **反向可达性不完整**（★ 2026-11-07 补录 · 入口化后暴露）：★ 当前**仅 3 处**回指本文件（ADR-094 §Related · `overview.md` · `interface-design.md`）⇒ ★ **ADR-091 自身未回指**（★ 其 Related 列了 7 个 ADR + 契约文档，唯独未列 ADR-094 与本文件）· ★ **`parity-gate-contract.md` 未回指** ⇒ ★ 读者从「两层范式权威」或「门禁契约权威」这两个最可能入口进入 ⇒ **走到死路** ⇒ ★ 违反 §0.6 **M-3** | ✅ **已闭合**（2026-11-07：★ ADR-091 §Related 补 ADR-094 + 本文件两条 · ★ 契约文档头部补导航入口一行 ⇒ ★ 回指方 **3 → 5**） |
| ⑩ | ★ **导航表无反向完备性门禁**：★ `tools/doc_link_check.sh` 只查**正向**断链（文档内引用的路径是否存在）⇒ ★ **无法发现「相关文档未被入口收录」或「入口表条目已失效但未被引用」** ⇒ ★ ★ **且 §0 全景表条目本身也不被覆盖**（★ `doc_link_check.sh` 检查 2 仅根级 3 文档 · 检查 1 只查 markdown 链接 · `SCAN_PATHS` 不含 `.rddf/`）⇒ ★ ⇒ 本表**正向与反向均无机检** ⇒ ★ 当前缓解 = §0.6 **M-2/M-4** 的人工纪律（★ 无机检）⇒ ★ 候选 = 在 `doc_link_check.sh` 增加「§0 全景表条目 ⇒ 路径必须存在」的单向校验（★ **低成本**，★ 不需反向可达判定） | ⚠️ **未建**（★ 归 builder：★ architect 不写 `tools/`） |
| ⑪ | ★ **层 2 唯一实例的实现目录无文档落点**：★ `ip/cpu/core/` 下 **8 个头文件、0 个 `.md`**（★ 唯一层 2 实例 `RegFileComponent` + 其桥接 plugin 都在这里）⇒ ★ ★ **§0 导航层能指到代码，但代码侧无任何自述文档** ⇒ ★ 「装配形态为何是裸 `make_unique`」「为何裸 `make_unique` 是错的」这两条知识**只存在于本文件 §1.2（★ **实施态**）与 ADR-094（★ **目标态**）** ⇒ ★ **实现侧读者无就近落点** ⇒ ★ 候选 = `ip/cpu/core/README.md`（★ ★ **归 builder**：★ `ip/cpu/core/` 不在 architect 的 `arch_paths` 内 ⇒ ★ architect 不写） | ⚠️ **未建**（★ 归 builder） |
| ⑫ | ★ **探针 A-1 的脚本已失效**（★ ★ 属**实施区 / 证据侧** ⇒ ★ **不影响真相区**；★ 但 §9 引用它 ⇒ 故须诚实标注）（★ 2026-11-07 实测：`/tmp/opencode/probe_scope.cpp` **不存在**）⇒ ★ **证据不可复算**（★ 结论仍可由三条独立依据复核 ⇒ 见 §9）⇒ ★ ★ **MUST 随 ①-A 把探针固化入仓**（★ 归 builder：★ architect 不写 `tests/`）⇒ ★ 对照：★ **上游的探针已入仓**（`CppHDL/docs/architecture/2026-11-02-hierarchical-rtl-output.md` §9）⇒ ★ 我方是**明确落后项** | ⚠️ **已失效**（★ 归 builder） |
| ⑬ | ★ **上游 commit 锚点漂移（★ 已实证一次 · ★ 2026-11-08 定性升级）**：★ ★ **上游 ADR-058~062 是「真相区」的一部分** ⇒ ★ ⇒ ★ **上游 ADR 被改写 = 真相变了**（★ 不只是「锚点过期」）⇒ ★ ★ 故 **M-7 的守卫是真相区的完整性守卫**，★ 非仅引用卫生 ⇒ ★ ★ 台账 §6 记录的复核依据 = 上游 HEAD `dbc9801`（2026-11-06）⇒ ★ **2026-11-07 实测实际 HEAD = `824e2d0`** ⇒ ★ 差异落在上游 planner 文档面（`improvement v4` 等）⇒ ★ **未影响任何决策**（★ 本仓 ADR 的 Parent 行锚的是 **ADR 版本号** `v2.17` / `v2.3.2` / `v1.15` / `v1.6.4` / `v1.4` ⇒ **不受影响**）⇒ ★ 但 ★ **机制缺口成立**：★ 「上游 = 独立仓库 + 无版本闸」⇒ ★ 任何依赖上游实现细节的锚点都会静默漂移 ⇒ ★ 已在 §0.5 **M-7** 立守卫（引用上游 MUST 带 commit）⇒ ★ ★ **残余缺口 = 守卫无触发器**（★ 上游 HEAD 变了，谁来提醒？）⇒ ★ 候选 = CI 定期比对上游 HEAD 并在漂移时报出（★ 归 builder）⇒ ★ ★ **★ 二次实证（同一落笔会话内）**：★ oracle 审查期间上游 HEAD **又推进 2 次**（`824e2d0` → `afdcab9` → `6c43d05` → `9f2986f`）⇒ ★ ⇒ ★ **「上游 HEAD 持续漂移」不是理论风险，是每 20 分钟发生一次的事实** ⇒ ★ ⇒ ★ **锚点必须写「核对过的 commit」而非「当前 HEAD」**（★ 已在 §0.3 ④ 改正）⇒ ★ **仍需触发器** | ⚠️ **已实证两次**（★ 守卫已立 · 触发器未建） |
| ⑭ | ★ **`ch::hier_eq` 机制锚未接**（★ 2026-11-08 新发现）：★ 上游 `ch::hier_eq`（7 项闭合公共面 · fail-closed）于 2026-11-06 落地，★ 但 ★ **ChipForge 侧全仓 0 引用**（★ `grep -rn "hier_eq" docs/ .rddf/ ip/` 零命中）⇒ ★ ★ ADR-094 **V-28 / V-29**（层次化等价性判据）⇒ ★ **判据存在而机制锚未接** ⇒ ★ ⇒ ★ **方向已定**（★ §3.5 ② 已把 `ch::hier_eq` 定为等价性判据的**机制锚**）⇒ ★ ★ **缺的是 ADR-094 侧的登记**（★ V-28 / V-29 补一句机制锚注 ⇒ ★ **属 ADR-094 范围** ⇒ ★ **无需另立 ADR**）⇒ ★ ★ **另需登记**：★ ③ 的「层次化产物适用面」在 `ADR-089` / 契约文档**尚无登记** ⇒ ★ ⇒ ★ 故 §3.5 的结论句用 **SHOULD 而非 MUST** | ⚠️ **部分已定 · 待登记**（★ 归 architect） |
| ⑮ | ★ **`plugin-framework.md` §4.2 已过时但 architect 不可写**：★ 该文（★ v1.2 · 2026-06-09）仍写「Plugin 仅在 TLM 模式生效 / 同一 IP 可同时有 TLM 版和 RTL 版」⇒ ★ 与 ADR-091 v1.16 **两层范式** + ADR-083 **单一 source of truth** 冲突 ⇒ ★ 但该文件**不在 `arch_paths`**（★ `arch-docs.json` 实测）⇒ ★ ⇒ ★ 处置二选一：★ ① **扩 `arch_paths`**（★ 治理变更）★ ② 走 owner 加 dated 注（★ **当前唯一可行**）⇒ ★ ★ **局部改写治标不治本**（★ 该文整体处于 Phase 0/1 历史态）⇒ ★ 整体重写归 backlog；★ ★ **已做的最大处置 = 导航面警语**（★ §0.3-C 行内标明「§4.2 已过时 · 以 ADR-091 v1.16 / ADR-094 为准」）⇒ ★ 剩「文件内 dated 注」归 owner | ⚠️ **待决**（★ 治理面） |
| ⑯ | ★ **`tech-selection.md` §8 演进计划无 TLM 退役行**：★ §4 的 CppTLM 角色描述仍准，★ 但 §8 的演进计划止于「Phase 7+」⇒ ★ 未反映 ADR-088 / 092 / 093 的退役路线 ⇒ ★ 该文件**不在 `arch_paths`** ⇒ 归 owner | ⚠️ **待 owner** |
| ⑰ | ★ **`code-framework-mapping.md` §3.1 组件模型表缺层次化符号**：★ 该表 = `Component` / `ch_module<T>` / `ch_device<T>` / `ch.hpp`，★ 缺 `create_child` / `__io` / `toVerilogHierarchical` / `hier_eq` ⇒ ★ 且 `ch_module` 被 ADR-091 **G-1b** 禁用于 ChipForge 层 2 ⇒ ★ 该文自带「版本敏感性声明」⇒ ★ 归 owner 按其声明补行 | ⚠️ **待 owner** |
| ⑱ | ★ **`error-handling.md` §3 + `background-and-goals.md` 的 dated 注待补**：★ 前者 §3 仍用旧名 `MemRespBundle`（★ 真名 `cf::bundles::MemResp`，★ `overview.md:167` 已更正）+ §4.3 COMPARE 模式叙事随 ADR-088 退役；★ 后者仍写「TLM 模式（默认）」无冻结注 ⇒ ★ 两份**均不在 `arch_paths`** ⇒ 归 owner | ⚠️ **待 owner** |
| ⑲ | ★ ★ **`adr.md` §2.4 统计口径与 §2.1 主表脱节（★ 治理决策 · 2026-11-08 实测，比原残留 ⑧ 更深）**：★ §2.4 统计表（**48 条** / A~M 十三类）与 §2.1 主表（★ **实测 41 行** / 另一套类别词表「Plugin / 验证基建 / 架构治理 / IP 架构…」）★ **是两套体系** ⇒ ★ ★ **按主表派生统计当前走不通**（★ 词表不同）⇒ ★ ★ **补登 ADR-087~094 会固化第二套分类** ⇒ ★ ⇒ ★ **architect 不单方面补登**（★ 三个选项 + 代价见 `.rddf/reviews/architecture-docs-governance-recommendations.md` §4 选项 3）· ★ ★ **另实测到两层更深的账实不符**：★ ① `adr.md` **自身**的节标题与行数对不上（★ §2.1 标题「40 条」/ **实测 41 行** · §2.3 标题「9 条」/ **实测 18 行**）⇒ ★ **统计与标题两处都不自洽**（★ ★ **§2.1~§2.3 三节标题合计 = 41 + 1 + 18 = 60** vs **§2.4 统计合计 = 48** ⇒ ★ **差 12**；★ 根因 = 两套口径并存：★ §2.4 按 §3 的 A~M 类别且截止 ADR-086 · §2 主表按状态列全部 60 条）；★ ② §2.4 类别表有 **M 类（IP 架构）** 但 §3 详细记录**只有 A~L 十二节**（★ 无 M 节 · 目录 TOC 亦无）⇒ ★ **类别体系断链** | ⚠️ **待决策**（★ 治理面） |
| ⑳ | ★ ★ **`arch_paths` 治理盲区（★ 治理决策 · 2026-11-08 实测）**：★ 本仓 **18 份**架构文档中**仅 7 份**在 `arch_paths` 内；★ 其余 **10 份既不在范围内也不在 `exclude_paths`**（★ 唯一被 `exclude_paths` 覆盖的是 `tlm-coverage-parity-ledger.md`） ⇒ ★ 治理盲区（★ 原残留 ⑮~⑱ 只挂了与层次化相关的 5 份）⇒ ★ ★ **且 `arch-docs.json` 无任何执行脚本** ⇒ ★ 「architect 可写范围」是**无闸的约定**（★ 越权靠人审）⇒ ★ ★ 本轮已做：★ 删死路径 `ip/cpu/docs/adr`（★ 目录不存在）+ `description` 写明缺口；★ ★ **扩不扩** ⇒ 三个选项 + 代价见治理建议文档 §4 选项 2 | ⚠️ **待决策**（★ 治理面） |

---

## 8. 落点表（★ 本文内容 → 目标文档）

| 本文内容 | 落点 |
|---|---|
| 决策（契约 / 粒度 / 端口 / 边界 / 顺序） | **ADR-094** · **ADR-091 v1.16** |
| 门禁面契约（登记簿 / 清单 / error_kind / 总则） | `parity-gate-contract.md` **S-10 / S-17 / S-17.1** |
| 推导与证据（探针 / 实测 / 选项推演） | `.rddf/plans/backend-parity-open-core.md` §K34~§K36（★ builder 起草） |
| 实施清单（tasks / 注释 / 门禁 Check / 框架改动） | `openspec/changes/*/tasks.md`（★ builder / planner） |
| 议题 / 裁决 / 审稿过程 | `.rddf/reviews/ADR-094-*.md` |

---

## 9. 附录 A —— 可复算的探针

★ 三条探针覆盖本文最承重的三个事实（★ 均可复现）：

| # | 探针 | 期望（当前态） |
|---|---|---|
| **A-1** | ★ **作用域帧残留**：`make_unique<Probe>(nullptr,"unitA")` 后建一个节点 ⇒ 读 `node->get_parent()->name()` 与 `node->name()` | ★ owner = `unitA`；节点名 = `unitA.<name>` ⇒ ★ **帧未截断**（对照：`create_child` ⇒ owner = root、名字 = `root.<name>`） |
| **A-2** | ★ **节点名无条件前缀**：`lnodeimpl` 的 `parent_ != nullptr ⇒ name_ = parent_->name() + "." + name` | ★ 与「组件侧 `build_hierarchical_name` 会跳空父路径前缀」**不一致** ⇒ ★ 上游回执候选（U-5） |
| **A-3** | ★ **FSM 私有 ctx**：`multi_cycle_fsm.h:93` / `mul_div_fsm_chmem.h:108` 的 `new ch::core::context(...)` | ★ **FSM 状态不在宿主 ctx** ⇒ ★ 层次化分析看不见（残留 ④） |

★ ★ **探针脚本的可复算性（★ 2026-11-07 实测复核）**：★ A-1 曾在会话临时路径 `/tmp/opencode/probe_scope.cpp` 实测通过，★ 但 **该路径已失效（2026-11-07 复核：`test -f` 判定不存在）** ⇒ ★ ⇒ **A-1 的脚本当前不可复算**；★ ★ **但 A-1 的结论仍可复核**（三条独立依据：★ ① `ip/cpu/core/regfile_bridge_chmem.h:124` 的裸 `make_unique` 读码 · ② 上游 `CppHDL/include/component.h:140-156` 明写「直接构造 ⇒ 帧保留到析构」· ③ 上游 `CppHDL/src/component.cpp` 的 `node_owner` 实现）⇒ ★ **结论有效、脚本待补** ⇒ ★ **MUST 随 ①-A 固化入仓**（★ 理由 = 上游主题文档 §9 的教训 —— **临时目录的探针不可复算** ⇒ 探针 MUST 入仓才能作为判据）⇒ ★ 归 builder（★ 残留 ⑫）。
★ **A-2 / A-3 为读码结论，未跑探针**（★ 诚实标注）⇒ ★ A-3（FSM 私有 ctx）**至今未实测**（★ 残留 ④）。

---

## 10. 实施路径（★ 指针）

★ 5 批 + 出口判据 + 治理硬前置（★ 立项与实施**跨会话并行**、★ 批内**分轮验收**）⇒ **完整表见 `.rddf/reviews/ADR-094-decision-ledger.md` §9.2**。

| 批 | 内容 | 出口判据 |
|---|---|---|
| ①-A | 框架地基（root / root-scope / r.14 / r.15 / r.16 / r.10） | V-40 / V-41 / V-42 / **V-43**（★ 与台账 §9.2 一致） |
| ①-B | 观测登记（S-17.1 清单 / 登记簿 / manifest / provider / 两闸 / 报告标注） | V-37 / V-38 |
| ①-C | 阶段 1（三态化 + 前置同名普查） | V-39 |
| ①-D | ★ **阶段 2（不可拆）**（owner 切换 + 登记上线 + manifest 上线 + r.14 + 迁移期基线） | V-35 / V-36（★ 验收后退役） |
| ② | 首批成层单元（**ALU**）+ 层次化等价性判据 | V-27 / V-27b / V-28~V-31 / V-33 |

---

## 11. 治理状态

| 项 | 状态 |
|---|---|
| ADR-094 | 🚧 **Proposed v0.1** ⇒ ★ 决策已全部闭合并经 **critic 五轮 + oracle 两轮文本审**（0 BLOCKER）⇒ ★ 仅剩**治理收口**（立项 + AC 回填）⇒ ⇒ 未转 Accepted（★ 完成标志第 4 条未满足；★ **不阻塞实施**） |
| ADR-091 | ✅ **Accepted v1.16**（4 处订正 + G-1b 换根 + G-1o 扩面 + G-1r 索引） |
| 门禁契约 | ✅ `parity-gate-contract.md` S-10 / S-17 / S-17.1 已落（★ **自阶段 2 起生效**） |
| 审稿记录 | `.rddf/reviews/ADR-094-hierarchical-assembly-contract-review.md`（★ 五轮 critic + oracle 终审（ADR-094 侧）★ + **§7h 导航层 / §7i 真相升格：oracle + critic 两轮（本文档侧）**★ + **§7j 语义修订（★ 用户裁决 · oracle 轮）**） |
| 决策台账 | `.rddf/reviews/ADR-094-decision-ledger.md`（17 议题 · D-01~D-21 · §9 交接总览） |
| ★ 本文档 | 🚧 **Draft**（★ §1 = **实施态**快照 2026-11-07 实测；★ §0 = **目标态**导航入口：★ 真相 = **架构决策文档 + 架构文档**）⇒ ★ **ADR-094 治理收口（立项 + AC 回填）完成后**转正 |
| ★ **治理后梳理总结** | `.rddf/reviews/architecture-docs-governance-summary.md`（★ 2026-11-08 · ★ **治理后的现状快照**：★ 全量台账 18 份 + ADR 目录 · 索引层现状 · 「真相=目标态」落实矩阵 · 机检覆盖矩阵 · ★ **数字一致性核（治理后的账实）** · 剩余缺口与待决项）|
| ★ **治理建议** | `.rddf/reviews/architecture-docs-governance-recommendations.md`（★ 2026-11-08 · `docs/architecture/` 文档治理诊断 + 建议：★ 8 类分类法 · 索引层分工 · 元数据模板 · 6 项可机检门禁 · **3 项待治理决策**）★ ★ **落 `.rddf/reviews/`**（★ 建议是**待决策材料** ⇒ 不进真相区） |
| ★ 下一步 | ★ **planner 立项**（2 个 change）★ + ★ **builder 实施**（5 批）★ —— ★ **跨会话并行**（否则死锁）★ + ★ **builder 补 3 项**（残留 ⑩ 导航表校验脚本 · 残留 ⑪ `ip/cpu/core/README.md` · FSM 私有 ctx 核查） |

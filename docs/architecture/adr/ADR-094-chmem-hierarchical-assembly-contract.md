# ADR-094 — CH_MEM 开放核的层次化装配契约与端口纪律

> **Status**: 🚧 **Proposed v0.1（2026-11-07 · architect）** ⇒ ★ **决策已全部闭合并经 oracle → critic 四轮审（0 BLOCKER）**（★ 用户裁决 D-01~D-21；★ 审查轮次 = **critic 五轮 + oracle 两轮文本审**，0 BLOCKER）；★ **仅剩治理收口**（「关联 change 立项」+「AC 映射回填」）⇒ ★ **「ADR 完成的标志」第 4 条未满足 ⇒ 未转 Accepted**；★ ★ **不阻塞实施**（★ 实施者可据此开工；★ 但不得据此宣称「ADR 已 Accepted」）。
> ★ **落笔边界**：本 ADR 只承载**规则**（装配 / 粒度 / 端口与命名 / 交付定位 / chlib 边界）。
> ★ **推导与证据**（逐条 file:line、探针记录、选项推演、上游 ADR 逐条对照）⇒ `.rddf/reviews/ADR-094-decision-ledger.md` §3 / §6 / §7 · `.rddf/plans/backend-parity-open-core.md` §K34（**待 builder 落**）。
> ★ **过程日志**（评审轮次 / finding 清单）⇒ `.rddf/reviews/ADR-094-hierarchical-assembly-contract-review.md`。

- **Date**: 2026-11-07
- **Author**: architect
- **Parent（上游，全部 Accepted）**: ADR-058 v2.17（层次化 RTL 输出）· ADR-059 v2.3.2（归属与命名）· ADR-060 v1.15（端口策略与复用）· ADR-061 v1.6.4（粒度由用户显式控制）· ADR-062 v1.4（单元接口类型）
- **本仓关联**: **ADR-091 v1.16**（核形态 / 两层范式）· ADR-092 §D4（`ch_stream` 层次区分）· ADR-083 §4.2（拒绝范围）· ADR-089 §D3（判定与例外白名单）· ADR-046（多周期 FSM 豁免）
- **关联 change**: ★ **2 个**（★ **change 边界 = 交付批次边界** ⇒ ★ D-02 的「阶段 2 同批」**不可跨 change** ⇒ 框架与登记合为一个）：① `chmem-hierarchical-assembly`（★ **框架地基 + 观测登记** = root/root-scope/attribution 守卫 r.14 + provider 槽位 r.15 + 登记簿 + manifest + 两道闸 + C-1 三态化 + 阶段 1 普查 + 迁移期基线）② `chmem-hierarchical-unit-landing`（★ **首批成层单元** + 层次化等价性判据 V-27~V-31）⇒ ★ **architect 不创建 openspec change**（★ planner 立项）
- **AC 映射**: ★ **权威 = change 的 `tasks.md` 的 AC 清单**（★ openspec 的 AC 机制）⇒ ★ 本 ADR 的「AC 映射」行是**生成镜像**，★ MUST 由工具产出、**MUST NOT** 人工维护（★ 否则与 `tasks.md` 两张皮 ⇒ 漂移）⇒ ★ verifier MUST 机检「**映射表 == tasks 的 AC 清单**」（★ 无孤儿、无重复、无遗漏）⇒ ★ 本 ADR 覆盖 **V-27~V-43（含 V-27b ⇒ 共 18 个条目）**；★ V-1~V-26 属 ADR-091 自己的 change

---

## Context

| # | 事实 | 证据 |
|---|---|---|
| 1 | ★ 上游层次化 RTL **已落地并归档**（5 个 ADR 全 Accepted；`CH_HIERARCHY_LAYER` / `ch_layer` / `toVerilogHierarchical` / `backend_port_set` / `try_initialize_with_verilog` 均可用） | `CppHDL/include/component.h:36` · `include/module.h:162` · `include/hierarchy_verilog.h:129` · `include/core/verilator_backend.h:121` |
| 2 | ★ emitter 按**组件树**发现层级；**未登记进树的 owner ⇒ FATAL**；登记但未 `build()` ⇒ FATAL | `CppHDL/src/hierarchy_analysis.cpp:211-239` |
| 3 | ★ ChipForge 现装配形态 = 桥接 plugin 内**裸 `make_unique` + `build(ctx)`**，**无 `add_child`** ⇒ 命中事实 2；且 `Component` 构造的**作用域帧残留到析构**（实测：其后创建的节点 owner = 该单元、名字带 `<unit>.` 前缀） | `ip/cpu/core/regfile_bridge_chmem.h:124` · `CppHDL/include/component.h:140-156` |
| 4 | ★ 节点名 = `owner->name() + "." + name`（**无条件**）；Verilog 名再由 `sanitize`（`.`→`_`）派生 ⇒ **owner 非空 ⇔ 契约名被前缀破坏** | `CppHDL/src/core/lnodeimpl.cpp:33-37` · `include/codegen_naming.h:40-44` |
| 5 | ★ 契约名解析是**精确相等**（`node->name() == name`）⇒ 任何前缀 ⇒ 读不到 | `tools/parity_runner/parity_sim_runner.cpp:249-255` |
| 6 | ★ 扁平产物：`type_output` **无条件**全部进顶层；有 driver 的 `type_input` 不进顶层 | `CppHDL/src/codegen_verilog.cpp:71-109` |
| 7 | ★ 上游 `ch_device` 的顶层端口名**本身就带 `top_` 前缀**（`top.<port>` → `top_<port>`）⇒ 「裸名」是 ChipForge 侧约定，非上游惯例 | `CppHDL/include/device.h` · `include/codegen_naming.h:40-44` |
| 8 | ★ 上游 `node_owner()` = 抑制器 ? nullptr : (`scope_` 非空 ? `scope_.back()` : **`current_`**)；`set_current()` 是 public static | `CppHDL/src/component.cpp:16-26` · `include/component.h:96-97` · 上游 ADR-059 D1 原文 `docs/adr/ADR-059-…:57-63` |
| 9 | ★ 解释器的**指令表分流**依赖 `!node->get_parent()`；★ Verilator 后端**不消费**这两张表 | `CppHDL/src/simulator_init.cpp:94-100` · `src/core/verilator_backend.cpp:1049-1064` |
| 10 | ★ 预载核的观测节点是**插件内 `ch_reg`**（不经 `declare_out`）⇒ **无法进登记簿** | `ip/cpu/plugins/reg_file_chmem.h:169` · `dmem_chmem.h:85-86` |

⇒ ★ 结论：**装配形态与端口归属都必须显式定契约**；否则「层次化发射」要么被 FATAL 拒绝，要么**静默**把契约名/模块归属弄错（最坏假绿）。

---

## Decision

> ★ **单一决策**：层 2 单元**如何被装配、如何成层、如何暴露接口与观测**。

### D-1 ★ 装配契约（硬约束 **G-1r**）

| # | 规则 |
|---|---|
| **r.1** | ★ `PipeBuilder` MUST 持有**恰一个** root `Component` 并暴露只读访问器 `root_component()`（★ 框架持有 ⇒ 「每 ctx 恰一个」最不易被漏登记）；root 与 `pb.ctx()` **同一 ctx**；root 构造 MUST 包在 `Component::ConstructionFrame` 内（先例 `CppHDL/include/device.h:24-27`）；root MUST NOT 声明成层；`describe()` SHOULD 为空且 **MUST NOT 创建 `ch_out`**；root MUST 在 ctx 就绪后 `build(ctx)`；★ root-scope MUST 以 **RAII** 施加且 MUST **恢复先前的 `current_`**（⇒ 嵌套 / 多 `PipeBuilder` 场景安全）⇒ ★ root 的**实例名 = 契约常量 `cpu`**（见 **D-3.5**）⇒ ★ **该 root 与层次化 emitter 的 `root` 参数 MUST 是同一对象**（否则「登记簿归属」与「层次化归属」可能不一致 ⇒ 新二义面） |
| **r.2** | 单元 MUST 经 **`root.create_child<T>(name)`** 创建（**MUST NOT** 裸 `make_unique`）；调用方 MUST 检查返回值非空（上游失败返回 `nullptr`）。★ 时序前提：`create_child` 返回后作用域帧**已截断** ⇒ 其后创建的节点不再归属该单元 |
| **r.3** | 单元 MUST 以 `pb.ctx()` 调 `build()`（**MUST NOT** `build(nullptr)`）⇒ 即 **G-1p** 的装配侧表述，条款不在此重述 |
| **r.4** | **若**引入 `attach_unit` 便捷 API（**可选**，非 MUST），其前置检查 MUST **fail-closed**：null / 空名 / 同名重复 attach / ctx 为空 / 未 `build` |
| **r.5** | 每个单元 MUST 声明**外部驱动输入集**；`setup()` 结束后该集合内每项 MUST 满足 `impl() != nullptr && impl()->driver() != nullptr`。★ 集合外允许悬空（上游 ADR-060 §2.2 的冒泡是合法形态）但 MUST 登记 |
| **r.6** | 顶层端口 MUST 经 `PipeBuilder::declare_in/out` 创建，且创建点 MUST 处于 **root-scope**（`Component::set_current(root)` 生效、`scope_depth() == 0`）（★ root-scope 口径 = **D-6：覆盖整个构建期**，非「单次建节点」）⇒ **归属 root**；MUST NOT 在组件作用域内创建（否则归该组件） |
| **r.7** | **一个 ctx 恰一个 root**（★ 与 D-3.5 的「多核 = 一 root + 多单元」一致；★ **跨 ctx 多 root 不在本契约范围** ⇒ 违 G-1p ⇒ 另立 ADR）；生命期 **ctx > root > unit**；同一单元二次 `build(不同 ctx)` 已被上游拒绝（★ 详见 ADR-091 §D5.1.3） |
| **r.8** | 跨单元连接 MUST 落在 **root scope**（桥接 / `Payload`）；**MUST NOT** 组件之间 sibling 直连（例外路径 = 经 root scope 或经 `Payload`） |
| **r.9** | 单元 `describe()` MUST NOT 读 `Payload`（= **G-1q**，不重述）；MUST NOT 用 `is_layer()` / `granularity()` **改变可观测行为**（端口集合 / 状态更新）——仅允许用于诊断与断言 |
| **r.10** | ★ **`PipeBuilder::build()` 的幂等语义 MUST 精确定义**：★ **同一状态**重复调用 ⇒ **成功早退**（不重跑 `negotiate/setup/build`）；★ **状态已变**（★ 插件集指纹不同 ⇒ 如新增 plugin 后再 build）⇒ MUST **fail-closed**（新增 `PluginError::BuildStateConflict`）⇒ ★ **MUST NOT** 静默早退并忽略新 plugin（★ 静默忽略 = 隐蔽的假绿）；★ `built_` MUST 在**成功收尾时**置（★ 现状在 `negotiate` **之前**置 ⇒ ★ `elaborate()` 的自动 build（`pipe_builder.h:429`）在 build 失败后**不重试** ⇒ 在**半构建状态**上跑闭包 ⇒ **fail-open**）；★ 重名 attach MUST 拒绝（上游只检测「同一 child 出现两次」，不检测同名不同对象） |
| **r.11** | 层 2 单元 `__io` 成员 MUST 是**标量** `ch_in` / `ch_out`（`ch_uint<N>` / `ch_bool`）；**MUST NOT** 用 bundle / `ch_stream` / `ch_mem` / `ch_reg` 作端口。★ 理由：bundle 成员是 `type_proxy`，**不是端口**；且 `ch_in<ch_stream<…>>` 会退化成**整宽 blob 的单个 `type_input`**，与字段节点不是同一节点 ⇒ **语义空壳**（实测） |
| **r.12** | （**推荐**，非 MUST）单元端口名 SHOULD 带单元局部名前缀（如 `regfile_`）⇒ 避免 ctx 全局去重给同名端口加 `_N` 后缀。★ 因 D-3 已把契约身份与节点名解耦，此项**不再是契约必需** |
| **r.13** | ★ **闸①的「期望键集 + allowlist」MUST 由调用方注入**（★ 框架 MUST 只做「扫 + 比 + fail-closed」⇒ MUST NOT 内置 parity 概念 ⇒ 将来 SoC 级门禁可另配一套）⇒ ★ 失败 MUST **typed end-to-end**：`PluginError::ContractRegistryMismatch`（框架侧 typed 枚举）⇒ runner 报告 `error_kind` ⇒ gate 分类 ⇒ ★ MUST NOT 只「进程非零退出」（否则门禁只能给通用错误 ⇒ 日后加分类要改三处） |
| **r.14** | ★ **归属守卫前移**（★ 把上游层次化前置校验①「节点的 owner MUST 在 root 的子树里」—— `CppHDL/src/hierarchy_analysis.cpp:211-227` —— **前移**到 ChipForge 框架，★ **扁平路径也跑**）：★ **触发点** = 框架**构建收尾**（★ `build()` 收尾 **与** `elaborate()` 收尾**各一次** ⇒ 覆盖「只在 elaborate 期建节点」的情形）；★ **判据** = ctx 中**除 clock/reset 外**的每个节点，其 owner MUST ∈ `{root}` ∪ `{已登记单元}`；★ **排除项** = ① ctx 级 clock/reset（★ 上游 ADR-059 D10：MUST 无归属，`context.cpp:107-115`）② `node_ownership_suppressed()` 生效期间建的节点（★ `context.cpp:510-513`）；★ **失败** = **fail-closed**（新增 `PluginError` 码 ⇒ 门禁 `ERROR(2)` + `error_kind = node_ownership_escape`）；★ ★ **本条自阶段 2 生效**（★ 阶段 1 期间 `owner == nullptr` ⇒ 本条判据会**全量误报** ⇒ MUST 与 owner 切换同批启用）；★ ★ **理由**：★ 上游那条守卫**只在层次化路径跑**（★ `make_flat_module_view` 完全不查归属 ⇒ `codegen_verilog.cpp:61-109`）⇒ ⇒ **ChipForge 的主路径（扁平）此前对「作用域帧泄漏」完全不可见** ⇒ 本条把**同一判据**前移 ⇒ **单一语义、无第二套规则**（★ 与 G-1h/G-1i 的「创建时机」维度、G-1d 的「方向」维度互不重叠） |
| **r.15** | ★ **provider 槽位按 ctx 划分、每 ctx 恰一个** ⇒ ★ **重复安装 MUST 立即失败** ⇒ ★⇒ **结构上装不出双轨**（★ 运行期不可能同时有两个 provider）；★ ★ **「装哪个 provider」由调用方按语境决定**（★ 框架 MUST NOT 读 `contexts`、MUST NOT 认识 parity 概念 ⇒ 与 **r.13** 同原则）⇒ ★ ★ **冲突检测点 = provider 安装期自检**（★ 不是运行期）：① `manifest` provider MUST 自检「每个键的 node 存在、且其 raw 名与 manifest 一致」② `identity` provider MUST 自检「每个 `node_keys` 精确匹配到**唯一**节点（唯一性由 `kAmbiguous` 保护）」⇒ ★ 任一自检不通过 ⇒ **fail-closed**（`ERROR(2)` + `error_kind = provider_conflict`，★ 消息点名**逻辑名 + 两个候选节点名**）⇒ ★ ⇒ 「同一运行只允许一条解析路径」由**结构**保证，而非由运行期检查保证 |
| **r.16** | ★ **层 2 单元 MUST 继承 ChipForge 提供的「层 2 基类」**（★ 由 change ① 引入）⇒ ★ 该基类 MUST 在**编译期**校验 `io_type` 的每个成员为**标量** `ch_in`/`ch_out`（`ch_uint<N>` / `ch_bool`）⇒ ★ ⇒ **r.11 由「文字纪律」升级为「编译期纪律」**；★ ★ **为何落在 ChipForge 侧而非上游**：★ 上游 `port<T,Dir>` **无** bundle 类型的编译期拒绝（★ `CppHDL/include/core/io.h:211/256` 只做类型转发）⇒ ⇒ **不能改上游** ⇒ ⇒ 校验 MUST 落在 ChipForge 自己的基类 ⇒ ★ 收益 = **新单元自动受约束**（★ 不必依赖门禁 grep 逐个抓） |

### D-2 ★ 粒度原则

- 默认 `Granularity::RespectDeclarations`（**默认 inline**）。
- 成层 MUST 用**显式声明**：类级 `CH_HIERARCHY_LAYER("name")` 或实例级 `ch_layer<T>` / `ch_inline<T>`。
- **`ForceLayer` MUST NOT 作为交付形态**（未声明者回退 `typeid` + `CHWARN` ⇒ 跨编译器/ABI 不可复算）；仅用于对照测试。
- **`ForceInline` MUST 仅用于**「与扁平逐字节对照」的判据（V-29），且 **MUST NOT 重新 elaborate**。

### D-3 ★ 端口归属、契约身份与命名纪律

| # | 规则 |
|---|---|
| **D-3.1** | ★ **顶层端口归 root 组件**（`owner = root`）⇒ 组件树完整 ⇒ **无「归属空洞」告警** |
| **D-3.2** | ★ **契约身份与 ctx 节点名解耦**（★ **契约本体 = [`parity-gate-contract.md` **S-17**](../parity-gate-contract.md)**，★ 本 ADR 只定方向与**架构侧形态**：★ 登记点 = 「**默认自动**（`declare_in/out`）+ **受控例外**（`register_observation()`）」；★ **细则 = S-17 REG-1**，★ 本行不重述） |
| **D-3.3** | ★ **契约名一个字不改**（★ **例外 = T15 统一**：逻辑契约名 `pc` —— 旧 `open_pc` 作废 ⇒ ★ 节点名仍可为 `cpu.open_pc`）⇒ 清单：`reg_1..reg_31` / `pc` / `mem_sig` / `mem_sig_updates` / `tohost_probe_data_proxy` ⇒ 它们成为登记簿的 **key = 逻辑契约名**（★ `pc` 的逻辑名统一 = **T15 = A**（2026-11-07）⇒ 两侧**节点名**不同（`pc` vs `cpu.open_pc`）但**逻辑名**唯一 ⇒ ★ **删除** runner 的 `pc`/`open_pc` fallback） |
| **D-3.4** | ★ **删除**「顶层端口不许带前缀」的私有约定 ⇒ 与上游 `ch_device` 惯例（`top_<port>`）一致 |
| **D-3.5** | ★ **root 名冻结为契约常量 = `cpu`**（★ 2026-11-07 裁决 D-07）：★ 改名 = **契约变更**（需 ADR + 重基线），因为它进 Verilog 端口名；★ **不用 `top`**（上游 `ch_device` 的 top 组件已占用该名 ⇒ 独立 DUT（V-30）与集成网表同名 ⇒ 拼接撞名 —— 上游 C2 轴②「内部 module 名当前无机制」）；★ **不用 `open_core`**（不含 SoC 语境 ⇒ 未来 SoC 级 root 需再改名 = 又一次契约变更）；★ 该常量登记于 **S-17 REG-2**；★ ★ **多 root 场景**（多核 SoC）下每个 root 的名字**各自**成为契约常量，且 MUST NOT 与其它设计的 root 名相同（★ 拼接撞名面 ⇒ 上游 C2 轴②「内部 module 名当前无机制」）；★ ★ **但「多 root」≠「多 ctx」**：★ **多核形态 = 一 root + 多单元**（★ **跨 ctx 的多 root 不在本契约范围** ⇒ 违 **G-1p**（禁多 ctx ⇒ node-id aliasing）⇒ 需**另立 ADR**）|
| **D-3.6** | ★ **G-1o 扩面**（本体在 ADR-091 §D5.1，v1.16 行内修订）：判定对象从「契约名 vs 内部节点名」改为「**ctx 节点名** vs **顶层端口名**」——任何可能进任一 module 端口表的节点 MUST NOT 与之同名（命名权威 = 同一张 ctx 级全局表） |
| **D-3.7** | ★ **两道机械闸**（★ 硬约束性 + 判据 = **S-17 REG-3**；★ 本 ADR 只定「闸是硬约束、非 tasks 可选项」这一定性） |
| **D-3.8** | ★ allowlist **变更 MUST 经 architect 审**（★ 落点与治理 = **S-17 REG-4**） |
| **D-3.9** | ★ **绑定提供者模型**（★ 语义：两侧**一致**的「逻辑契约名 → 节点」；★ 差异**仅在绑定来源** ⇒ ★ 槽位按 ctx 单一 + **安装期自检** = **r.15**；★ **细则 = S-17 REG-5 / REG-6**，★ 本行不重述）（★ 契约解析 = 「逻辑契约名 → 节点」的绑定；★ 绑定来源**可插拔**：`manifest`（运行时表）/ `identity`（★ 用**同一份** `node_keys` 做精确名匹配 ⇒ ★ **零第二份清单**）⇒ ★ **两侧语义一致**（★ 差异**仅在绑定来源** ⇒ **不构成第二套解析语义**）；★ `identity` provider 的**退役条件** = **载体统一**（预载核观测也走顶层 `ch_out`）★ + 机械判据 = 使用计数 = 0（★ 细则 = **S-17 REG-5 / REG-6**） |
| **D-3.10** | ★ **manifest 的架构侧约束**（★ 契约细则 = **S-17 REG-1/REG-2**；★ 本 ADR 只定这三条**架构事实**）：★ ① **每侧各生成** —— `node_id` 是 **ctx 私有编号**（上游 ADR-058 §2.2 注）⇒ 两个 runner 各自的 ctx 的编号空间**不保证相同** ⇒ ★ **MUST NOT** 用一份共享 manifest（会给出错误的 `node_id`）；★ ② **产出者 = 框架**、**时机 = `elaborate()` 收尾**（此时端口节点 id 已定、ctx 已完整）⇒ 与闸①**同一时刻、同一处** ⇒ 不可能漂移；★ **MUST NOT** 让 runner 反扫 ctx 自建 manifest（⇒ 与框架真相脱钩 ⇒ 表错则两错相抵）；★ ③ **字段集** = `{ node_id, raw_name, has_driver, source }`（`source` ∈ {`declare`, `register_observation`}）⇒ **value-only**；★ **先不带** `sanitized_name`（★ 触发条件：当某观测**不是**顶层端口、且 Verilator 侧**必须**按 Verilog 名直读时 ⇒ MUST 增补该字段）；★ ④ ★ **键清单的单一真源 = `parity-gate-contract.md` **S-17.1**」**（★ 此前键名散落 ADR-091 / runner / 契约文档三处 ⇒ 漂移面）⇒ ★ 清单区分「**节点级契约名**（进 manifest）」与「**报告层观测量 id**」（★ 派生量，★ NOT 进 manifest）⇒ ★ 闸②只比前者 |
| **D-3.11** | ★ **manifest 提供义务 MUST 按「检查语境」声明**（★ **由调用方**据此安装 provider ⇒ ★ 框架不读本表）（★ **不是**单一列表）⇒ ★ `parity`（**同一设计** × 两后端）⇒ 两侧 runner **都**提供 manifest；★ `h3`（**开放核 vs 预载核** ⇒ 两个不同设计）⇒ 仅开放核侧提供、预载核侧走 `identity` provider ⇒ ★ **义务按语境声明 ⇒ 不会把「结构上产不出 manifest 的一侧」误判为缺失 ⇒ 门禁恒红**（★ 语境清单 = **S-17.1 `contexts`**） |

### D-4 ★ 交付定位（边界声明，非新决策）

- 层次化产物 = **交付 / 可单独综合 / 单元级复用**；**不是** parity 门禁的输入（门禁仍消费**扁平** `top.v`）。
- module 复用 = **对齐上游**「每实例一份定义」（`<type>_<序号>`）⇒ 本 ADR **不另立裁决**。
- ★ 回滚路径：**整组丢弃层次化产物**即可 —— 门禁消费扁平 `top.v` 的路径**不变**（门禁面零风险）。
- ★ ★ **阶段 2（①-D）的回滚**：★ 因 ①-D **不可拆**（owner 切换 + 登记 + manifest + r.14 同批）⇒ ★ 验收失败时 MUST **整批回退**（★ git 级 revert 该批；★ **MUST NOT** 只回退其中一项 ⇒ 会留下 owner/解析路径的**混合态**）⇒ ★ **阶段 1 状态**（`owner == nullptr` + C-1 三态化）**保持可用** ⇒ ★ 回退后门禁仍可跑（★ 但**退化**为精确名解析 ⇒ 失去登记簿的两道闸）。

### D-5 ★ `chlib` / `ch_stream` 边界

- ★ **层 2 装配域与核内连接**：**MUST NOT** 引入 `ch_stream` 作连接机制（层 2 接口 = 标量端口；层 1 = `Payload` / 顶层端口 / `CtrlLink`）。
- ★ **ADR-091 §D3a.3 的核↔从设备 `ch_stream<CpuBusReq/CpuBusResp>` 契约不变**（物理实现 = 逐字段标量端口）。
- ★ `chlib` **允许**在层 2 单元**内部**用于行为实现（如 `chlib/fifo.h`），但：① 引入的**周期成本 MUST 显式登记**（★ 落点 = ADR-089 例外白名单的**机制说明字段**，与 ③ 同一处） ② 单元接口仍 MUST 标量端口 ③ **`chlib::ch_state_machine` 允许**（★ 实测：解释器下状态**可推进**——`state_reg->next` 是真实 `ch_reg` next 逻辑；★ 上游 ADR-046 的旧陈述把「`current_state()` 访问器不返回运行时状态」误读为「硬件状态不推进」，该访问器在上游 ADR-050 已改名 `entry_state()`）⇒ ★ **但状态机 MUST 完全封装在单元内部**，且其拍数差异进入 parity/H-3 语境时 MUST 走 ADR-089 例外白名单，**MUST NOT** 静默假定逐位相等。
- ★ `Payload` / 顶层端口 / `CtrlLink` 三种机制**不变**。

### D-6 ★ 迁移顺序（两阶段，顺序写死）

> ★ **实施批次（①-A 框架地基 → ①-B 观测登记 → ①-C 阶段 1 → ①-D 阶段 2（**不可拆**）→ ② 首批成层单元）** 与 **「立项（planner）与实施（builder）跨会话并行」** 的治理规则 ⇒ **归 `.rddf/reviews/ADR-094-decision-ledger.md` §9**（★ 顺序类内容不进 ADR 正文）。

| 阶段 | 内容 | 不变量 |
|---|---|---|
| **阶段 1** | C-1（`resolve_node_id` **三态化**：`kOk` / `kMissing` / `kAmbiguous`）｜★ ★ **阶段 1 MUST 以「同名普查」为前置**（★ **一次性、只读**；★ 范围 = **现网 52 case + CH_MEM binary 全量 case** 的 ctx；★ 报告落 **plan §K35**）⇒ ★ ★ **普查不通过 ⇒ fail-closed：先修存量同名，再切阶段 1**（★ **MUST NOT** 用 allowlist 绕过 ⇒ 同名 ⇒ `resolve_node_id` 取首个 ⇒ **读到错节点 ⇒ 假绿**）；★ 阶段 1 期间 `kAmbiguous` ⇒ 门禁报 **`observation_ambiguous:<key>`**（**ERROR(2)**）⇒ ★ **阶段 2 后 `kAmbiguous` 由创建期（`NodeNamePolicy::kRejectDuplicates`）+ 登记期消除**（稳态） | ★ **维持 `owner == nullptr`** ⇒ 否则全量 `kMissing` ⇒ **门禁恒红**；★ ★ **阶段 1 MUST NOT 产出 manifest**（★ 登记簿 / manifest / 两闸在 ①-B **已建成未上线**；★ **激活在 ①-D**）⇒ ★ 解析仍走**精确名**扫描 ⇒ ★ **S-17 的规则只在阶段 2 起生效**（否则实施者会在阶段 1 就要求 manifest ⇒ 门禁恒红） |
| **阶段 2** | ★ ★ **前置澄清（★ 消除实施者的误读空间）**：★ **①-A 只交付 root 对象与 RAII 机制**（★ **root-scope 在 ①-A 不全局化**、owner 仍 `== nullptr`）⇒ ★ **全局化 = owner 切换，随 ①-D 启用**（★ 否则 ①-A 之后即全局化 ⇒ 阶段 1 全量 `kMissing` ⇒ **门禁恒红**）｜**同批**：owner 切换（root-scope 覆盖**整个构建期**：`build()` + `elaborate()`）+ 登记簿 + manifest + root-scope 全局化 + ★ **r.14 归属守卫**（★ 与 owner 切换**必须同批** ⇒ 否则「帧泄漏」在新口径下不可见）★ + ★ **迁移期判据 V-35 / V-36 的基线捕获**（★ 基线 MUST 在**切换前**捕获 ⇒ 机器产物为**临时**（构建目录 / CI artifact）★ 结论落 **plan §K36** ⇒ 阶段 2 验收后二者**整体退役**） | ★ 阶段 1 与阶段 2 **不得交叠**；★ 同一运行内只允许一条解析路径 |

---

## Options considered

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| **A** | 维持全扁平（不建组件树） | ❌ 否决 | 单元不可独立交付；V-30（不经过 `PipeBuilder` 也能驱动）无法成立；上游层次化能力用不上 |
| **B** | 交付形态 = 全成层（`ForceLayer` 默认） | ❌ 否决 | 粒度开关的定位是「逃生门 + 对比测试」⇒ 交付形态随全局开关变化 ⇒ 产物不可复现；未声明者回退 `typeid` + `CHWARN` |
| **C** | **默认 inline + 显式声明**（采纳） | ✅ 采纳 | 与上游 ADR-061 决策②/④、ADR-062 §2.1 一致 ⇒ 粒度只由声明决定 ⇒ **可复现** |
| **D** | 每个单元一个 `ch_device`（自带 ctx） | ❌ 否决 | 多 ctx ⇒ node-id aliasing（上游 `include/device.h:12-19` 明载）⇒ 违反 G-1p；且 `ch_device` 只能有一个 top |
| **E** | 顶层端口维持 `owner == nullptr`（保裸名契约） | ❌ 否决 | ★ 「顶层端口必须无 owner」是**私有约定**（上游 `ch_device` 惯例相反）；且独立单元测试（`ch_device<Comp>`）同样会出现 `top_` 前缀 ⇒ **契约耦合没有消失，只是被推迟** |
| **F** | 改上游让节点「挂 owner 但不加前缀」 | ❌ 本轮否决 | 需上游变更（owner/命名解耦）；★ 登记簿方案下**不需要** ⇒ 记为未来项 |

---

## Consequences

**正面**
- 层次化发射**可执行**（组件树完整 ⇒ 不被 FATAL 拒绝）；单元可独立交付 / 单独综合。
- 契约身份**与节点名解耦** ⇒ 组件改名 / 端口挪层 / 嵌套加深**不再批量失效契约字符串**。
- 消除「归属空洞」告警洪流（★ 前置 = root-scope 覆盖整个构建期）⇒ 不训练「忽略告警」的习惯。
- 与上游惯例一致（`top_<port>`）⇒ 不再维护私有前缀约定。

**负面 / 成本**
- 多一层桥接 + 两套生命周期（`Component` vs `PluginBase` 共存）。
- **新增登记簿 + manifest + 两道闸**（框架与门禁各一处）。
- ★ **扁平产物的 Verilog 端口名会变**（`reg_1` → `<root>_reg_1`）⇒ A6 的 Verilator 读取路径 MUST 改为 manifest 驱动；已有 golden 需重基线。
- ★ **解释器指令表分流会变**（顶层 `type_input` 从 input 表迁至 combinational 表）⇒ MUST 逐项回归（V-35）。

**风险**
- ★ **R10**：漏登记 ⇒ 观测静默缺失（最坏假绿）⇒ 由 D-3.7 两道闸封死（**硬约束**）。
- ★ **R6**：root 改名 ⇒ 全部端口名变 ⇒ 由 D-3.5 冻结 root 名。
- ★ **R8**：Verilator 侧读取路径失效 ⇒ MUST 与 owner 切换**同批**改 manifest 驱动。
- ★ **R12**：两阶段双轨 ⇒ 由 D-6 的顺序不变量约束。

---

## Verification（可勾选 · **前瞻判据**）

> ★ 每条 MUST 带**变异对照**（去掉该项 ⇒ MUST RED）与**判别力说明**。

- [ ] **V-27** 层次化发射成功；`warnings` **⊆ 归属空洞类别**，且 **0 条 undriven-bubble**；若宣称「交付接口干净」⇒ **0 个 `Synthesized`**。★ 变异：删一条实例连接 ⇒ RED。
- [ ] **V-27b** `port_provenance` 精确匹配（顶层 input=`Bubbled` · 顶层 output=`InlineFolded` · clock/reset=`ClockReset` · 单元自身=`Self`）。
- [ ] **V-28** 层次化 vs 扁平**逐拍等价**；★ ★ **机制锚（★ 2026-11-08 补记 · architect）**：★ 上游 `ch::hier_eq`（★ `CppHDL/include/hierarchy_equivalence.h`）—— ★ namespace `ch::hier_eq` · ★ **7 项闭合公共面**（`DrivenInput` / `Observation` / `ProductSet` / `Result` / `emit` / `compare` / `make_observation`）· ★ **fail-closed**（★ 错误前缀如 `observation-not-visible` / `empty-observations` / `emit-failed`）⇒ ★ **V-28 / V-29 的实现 MUST 以该 helper 为机制锚**（★ **MUST NOT** 自建等价比对器；★ ★ **逃生阀（★ 补记）**：★ 若 helper 的闭合公共面**未覆盖**某个必要场景 ⇒ ★ **回上游扩展该 helper**，★ **MUST NOT** 在本仓自建等价比对器）★ ★ **判别力边界（★ 上游明写）**：★ 该 helper **MUST NOT** 替代双后端 parity（★ ★ 上游原话：★ 「与既有双后端 parity 是**互补**关系」+「★ **MUST NOT** 被读成『做了它就够了』」⇒ ★ 它只覆盖单后端「层次化 vs 扁平」等价 ⇒ 抓「变换算错」；★ 两产物同源 ⇒ **抓不到「共享发射底座算错」**）⇒ ★ **前置断言 module 数 ≥ 2**（否则全 inline 下两产物相同 = 恒绿）。★ 变异：反转一个端口 / 删一条连接 ⇒ RED。
- [ ] **V-29** `GranularityScope(ForceInline)` 下产物与扁平**逐字节一致**（同一 ctx、同一 elaborate，**不重新 build**）。
- [ ] **V-30** `ch_device<RegFileComponent>` 独立复用 + `ch_device<Wrapper>`（Wrapper 内 `ch_layer`，覆盖父路径命名）。
- [ ] **V-31** 负向三变体：① 漏 `add_child` ⇒ FATAL ② **ctor 传 `nullptr` + 事后 `add_child`** ⇒ RED ③ 登记但不 `build` ⇒ FATAL。
- [ ] **V-32** 每实例端口名逐字不同 + 单元每个 `ch_in` 均有 driver（按 r.5 的集合）。
- [ ] **V-33** `verilator --lint-only --top-module top <file>` **exit 0**，stderr 无 `%Error`。
- [ ] **V-34** 2 TU 链接探针（★ **删除 G-1b 的前置**；当前 G-1b 保留、仅换理由）。
- [ ] **V-35** ★ **归属切换的语义回归**（★ **迁移期判据**，★ 阶段 2 验收后**退役** ⇒ 不得留成僵尸判据）：★ **基线捕获时机** = **阶段 1 结束 / 阶段 2 开工前**（★ 切换后「`owner == nullptr`」的形态**已不存在** ⇒ 基线**无法事后补**）；★ **基线内容** = 逐 case 的**逐拍全观测量 hash 链**（★ FNV，★ 与既有 `pchash` **同构造** ⇒ 零新范式）+ **终值逐位数组**（可定位差异）+ 环境标识（commit / cpu 配置 / 语料集）；★ **判定** = 切换前后 hash 链**逐 case 相等**（硬）★ + **终值逐位相等**（第二判据，★ 便于定位）；★ ★ **迁移期产物不进长期报告 schema**（★ 否则多一个只服务迁移期的永久字段 ⇒ 债务）；★ **变异对照**：★ 在基线捕获后**人为注入一处观测差异** ⇒ hash 链 MUST 不同 ⇒ RED（★ 证明比判机制本身有判别力）。
- [ ] **V-36** ★ **方向守卫触发面回归**（★ **迁移期判据**，★ 阶段 2 验收后**退役**）：★ **捕获** = 抓运行日志（★ 上游 `CHREQUIRE` **只打日志、不抛** ⇒ `log_level::error` + 前缀 `Requirement failed [<cond>]:`）⇒ 按 **4 个 overload 前缀**（`out <<= in` / `in <<= in` / `out <<= out` / `in <<= out`）作**稳定匹配键**归类（★ 不匹配全文 ⇒ 上游改文案尾部不影响）；★ **判定（双判据，均硬）** = ① **无新增误报**（阶段 2 触发集 ⊆ 阶段 1 触发集）② **阶段 2 触发集为空**（★ 「减少」是**预期收益**（比较对象被修正）⇒ 报告展示减少量但**不**参与判定；★ 若实施时出现合法触发 ⇒ 走 **S-17 REG-4 allowlist**（architect 审）⇒ ★ **不是**放宽判据）；★ ★ **抓取器自检（★ 防静默失效）**：★ 若一轮捕获**零匹配**（或捕获器版本与基线不符）⇒ MUST **ERROR**（★ 否则「抓取器失效」会表现为「零误报」= **恒绿假绿通道**）；★ **变异对照**：★ 在阶段 2 侧人为制造一次越界连接 ⇒ 触发集 MUST 变大 ⇒ RED。
- [ ] **V-37** ★ **登记簿两道闸 + 退化可观测**：★ ★ **某侧 manifest 缺失 ⇒ `ERROR(2)` + `manifest_missing`**（★ 防「闸②退化为『有 manifest 才查』」的静默退化）；① 绕过 `declare_in/out` 裸建顶层 `ch_out` ⇒ `elaborate()` **失败**（变异：摘掉闸① ⇒ 静默通过 ⇒ RED）② manifest 观测键集 ≠ 契约文件 ⇒ gate **ERROR(2)**。
- [ ] **V-38** ★ **绑定提供者一致**：同一 case 中两侧解析出的**逻辑契约名集合**一致（★ 开放核走 `manifest` provider、预载核走 `identity` provider）；★ 报告 MUST 标注每个观测的 provider（★ 变异：删 `identity` provider ⇒ 预载核侧 `kMissing` ⇒ RED；★ 变异：伪造一个 provider 标注 ⇒ 报告审计 RED）。
- [ ] **V-39** ★ **阶段 1 前置同名普查**：现网 52 case **+** CH_MEM binary 全量 case 的 ctx 中，**同名节点组数 = 0**（或已逐组定位并修复）⇒ 判据 = 普查报告落 **plan §K35**；★ **变异对照**：★ 注入一个同名节点 ⇒ 普查 MUST 报出该组 ⇒ RED（★ 否则普查是恒绿护栏）；★ 归属 = 阶段 1 的**前置 task**（不阻塞 ADR 收口，但**阻塞阶段 1 开工**）。

- [ ] **V-40** ★ **归属守卫（r.14）生效**：★ 框架构建收尾时，ctx 中**除 clock/reset 外**的每个节点 owner MUST ∈ `{root}` ∪ `{已登记单元}` ⇒ ★ **变异对照**：★ 把某单元改回**裸 `make_unique`**（不 `create_child` ⇒ 作用域帧残留）⇒ MUST **RED**（★ 命中现存缺陷形态 `ip/cpu/core/regfile_bridge_chmem.h:124`）；★ **反向**：★ 把 `register_observation()` 用的顶层端口移进组件作用域内创建 ⇒ MUST RED（★ owner 变成该组件 ⇒ 逃逸）。
- [ ] **V-41** ★ **provider 安装期自检**：★ 装 `manifest` provider 时，MUST 校验每个键的 node 存在且 raw 名一致；★ 装 `identity` provider 时，MUST 校验每个 `node_keys` 精确匹配到**唯一**节点 ⇒ ★ **变异对照**：★ 伪造一条 manifest（把某键的 node 指向另一个同名节点）⇒ 安装 MUST 失败（`provider_conflict`）⇒ RED；★ **正向**：★ 同一 ctx 重复安装第二个 provider ⇒ MUST 立即失败 ⇒ RED。
- [ ] **V-43** ★ **`build()` 幂等语义（r.10）生效**：★ 同一状态重复调用 `build()` ⇒ **成功早退**（★ 不重跑 `negotiate/setup/build` ⇒ 不产生重复登记键）；★ **状态已变**（插件集指纹不同）⇒ MUST **fail-closed**（`PluginError::BuildStateConflict`）；★ `built_` MUST 只在**成功收尾**时置（★ 否则 `elaborate()` 的自动 build 在失败后**不重试** ⇒ 半构建状态跑闭包 ⇒ fail-open）。★ **变异对照**：★ build 失败后再 `elaborate()` ⇒ MUST **不**执行 stage 闭包 ⇒ RED；★ build 成功后新增 plugin 再 build ⇒ MUST 失败（**非**静默忽略）⇒ RED。
- [ ] **V-42** ★ **层 2 基类的编译期校验**：★ 层 2 单元的 `io_type` 成员 MUST 为标量 `ch_in`/`ch_out` ⇒ ★ **变异对照**：★ 把某成员改成 bundle / `ch_stream` 类型 ⇒ MUST **编译失败** ⇒ RED；★ **正向**：★ 绕过基类、直接继承 `ch::Component` 的单元 ⇒ 门禁 grep MUST 报出 ⇒ RED（★ 否则基类可被绕过 ⇒ 编译期纪律形同虚设）。

---

## Related

- **ADR-091 v1.16**：两层范式 / G-1q / G-1o（扩面）/ §D3a.9 观测量载体；本 ADR 承接其**装配与端口纪律**。
- ★ **架构主题文档**：[`docs/architecture/hierarchical-plugin-architecture.md`](../hierarchical-plugin-architecture.md) —— ★ **当前态快照 / 结构事实 / 下游需求 / 已知残留**（★ 决策在 ADR ⇒ 本文件只引用，不重述条款）
- **ADR-092 §D4**：`ch_stream` 的层次区分（本 ADR 仅做**管辖边界澄清**，不推翻）。
- **ADR-083 §4.2**：拒绝 **CppTLM** `ch_stream` 作 IP 间通信（本 ADR 保留其原意）。
- **ADR-089 §D3**：判定与例外白名单（本 ADR 的多周期单元拍数差异须登记于此）。
- **上游 ADR-058 / 059 / 060 / 061 / 062**（全 Accepted）—— 本 ADR 的 Parent。
- **审稿记录**：`.rddf/reviews/ADR-094-hierarchical-assembly-contract-review.md`（oracle / critic 各轮）。
- **决策台账**：`.rddf/reviews/ADR-094-decision-ledger.md`（议题 01~25 + 用户裁决 D-01~D-21）。
- ★ **待登记（backlog）**：ADR-046:33 的「`ch_state_machine` 不能在 CppHDL simulator 内推进状态机」为**过时陈述**（上游 ADR-050 已改名 `current_state()` → `entry_state()`，且 `state_reg->next` 的硬件推进在解释器下实测有效）⇒ 建议在 ADR-046 的下一次修订中订正（**不在本 change 顺手改**）。

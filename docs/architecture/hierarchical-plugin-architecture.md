# 层次化 Plugin 架构 —— 入口 · 当前态 · 结构事实 · 需求 · 残留

> ★ **本文件 = 架构主题文档**：只承载**当前态快照** · **下游需求** · **目标形态的结构事实** · **已知残留**。
> ★ **决策不在这里** ⇒ 决策在 **[`ADR-094`](adr/ADR-094-chmem-hierarchical-assembly-contract.md)**（装配契约 / 粒度 / 端口纪律 / 交付定位 / `chlib` 边界 / 迁移顺序）与 **[`ADR-091`](adr/ADR-091-chmem-open-core-port-architecture.md) v1.16**（两层范式 / 硬约束 / 观测量载体）⇒ ★ 本文件只**引用**，MUST NOT 重述条款（★ 避免两处全文 ⇒ 必然漂移）。
> ★ **过程日志**（议题 / 裁决 / 审稿）⇒ `.rddf/reviews/ADR-094-decision-ledger.md`（台账）· `.rddf/reviews/ADR-094-hierarchical-assembly-contract-review.md`（审稿记录）。
> ★ ★ **本文在体系中的位置（2026-11-07 补 · 入口层）**：★ 本文件是**层次化 Plugin 主题的导航入口**（★ §0）⇒ ★ 读者**从本文出发**即可定位全部相关文档；但 ★ **本文件不是权威入口** —— ★ **权威分布** = **决策** → ADR-094 / ADR-091 · ★ **门禁面** → `parity-gate-contract.md` · ★ **推导证据** → plan / 台账 · ★ **实施面** → openspec change 的 `tasks.md` · ★ **上游机制** → CppHDL ADR-058~062 ⇒ ★ 本文件 §1~§11 只承载**当前态快照 + 结构事实 + 需求 + 残留**，★ **MUST NOT** 与上述权威文档重述同一结论。

- Date: 2026-11-07
- Author: architect
- 状态: 🚧 **Draft（当前态快照 · 2026-11-07 实测）** ⇒ ★ **ADR-094 的治理收口（立项 + AC 回填）完成后**转正
- 上游对照: `CppHDL/docs/architecture/2026-11-02-hierarchical-rtl-output.md`（★ 上游版主题文档 · 2026-11-06 转正）
- 关联: ADR-094 · ADR-091 v1.16 · `parity-gate-contract.md`（S-10 / S-17 / S-17.1）· `.rddf/plans/backend-parity-open-core.md` §K17~§K33（★ K34~K36 待起草）

---

## 0. ★ 导航层（★ **入口** · 2026-11-07 新增）

> ★ **本节 = 入口的唯一职责**：★ 「要查层次化 Plugin 的某件事 ⇒ 去哪篇」。★ **本节不含任何决策 / 结论**（★ 结论一律在 §0.3 指向的权威文档里）⇒ ★ 本节**可长期稳定**，而 §1~§11 随实施演进 ⇒ ★ 两者**更新频率不同 ⇒ 分离**（★ 防「高频内容拖累低频内容 ⇒ 整篇腐化」）。

### 0.1 ★ 按意图的阅读路径（★ 「我想做 X ⇒ 读哪几篇」）

| 意图 | 读这几篇（★ 按顺序） | 读完得到 |
|---|---|---|
| ★ **第一次接触层次化 Plugin** | 本文 **§1**（当前态）→ **§3**（目标形态结构事实）→ ★ **ADR-094 §Decision**（装配契约 r.1~r.16） | 现状 + 目标形态 + 规则的权威出处 |
| ★ **要写一个层 2 单元** | ★ **上游 `HIERARCHICAL-RTL-AUTHORING-GUIDE.md`**（★ **7 条纪律 D1~D7** · 粒度 · 三种写法风格 ⇒ 面向「将来不返工」）→ ★ **ADR-094 r.1~r.16**（装配契约）· **r.11 / r.16**（标量端口 + 层 2 基类）→ 本文 **§5**（命名规则）→ **§4**（归属↔语义耦合 ★ 承重） | 怎么建、怎么命名、哪些消费点会被归属变更影响 |
| ★ **要改「契约名 ↔ 节点名」或观测** | ★ **`parity-gate-contract.md` S-17 / S-17.1 / S-10** → ★ **ADR-094 D-3**（端口归属 / 契约身份 / 命名纪律） | 登记簿 / manifest / provider / error_kind 的单一真源 |
| ★ **要实施（builder）** | ★ 台账 **§9.2**（5 批 + 出口判据）→ plan **§K17~§K33**（推导证据）→ 对应 change 的 **`tasks.md`**（★ AC 权威） | 批次边界、判据、验收清单 |
| ★ **要查上游机制**（归属 / 粒度 / 端口策略 / 等价性） | ★ 上游 **ADR-058~062**（全 Accepted）→ 上游主题文档 `CppHDL/docs/architecture/2026-11-02-hierarchical-rtl-output.md` | 上游侧的实现事实与决策依据 |
| ★ **要判断「某条规则归谁」** | 本文 **§0.3 权威分布表** → 对应权威文档 | 避免误改错文档 |
| ★ **要查现有层 2 实例** | ★ `ip/cpu/core/regfile_chmem.h` · `regfile_bridge_chmem.h`（★ 唯一实例）→ ★ `tests/cpu/test_cpu_open_core_regfile.cpp` | 当前唯一的层 2 装配形态与其验证 |
| ★ **要查未决 / 已知边界** | 本文 **§7**（残留 11 条） | 什么**没做**、什么**未核** |
| ★ **门禁红了怎么排查** | ★ **`parity-gate-contract.md` S-10**（`error_kind` 允许取值清单）→ **S-17 REG-3**（两道机械闸）→ **S-17 REG-4**（allowlist 治理）→ 本文 **§4**（归属↔语义耦合） | 按 `error_kind` 定位失败类别 ⇒ 对应闸与判据 |

### 0.2 ★ 文档全景表（★ 按「哪一层」分类 · ★ 覆盖全部相关文档）

| 层 | 文档 | 承载什么 | 权威性 | ★ **状态（★ dated · ★ 活状态以该权威文档头部为准）** |
|---|---|---|---|---|
| ★ **决策** | `docs/architecture/adr/ADR-094-chmem-hierarchical-assembly-contract.md` | ★ **装配契约（G-1r r.1~r.16）** · 粒度 · 端口归属 / 契约身份 / 命名纪律 · `chlib` 边界 · 两阶段迁移 | ★ **权威** | 🚧 Proposed v0.1 |
| ★ **决策** | `docs/architecture/adr/ADR-091-chmem-open-core-port-architecture.md` | 两层范式 · 硬约束 G-1a~G-1q · 观测量载体（§D3a.9）· §D5.1.3 G-1r 索引 | ★ **权威**（★ 装配部分让位 ADR-094） | ✅ Accepted v1.16 |
| ★ **门禁契约** | `docs/architecture/parity-gate-contract.md` | ★ S-17 登记簿 / S-17.1 逻辑契约名清单 / S-10 error_kind / GN-1 | ★ **权威**（门禁面） | ✅ v1.8 |
| ★ **注册表** | `docs/architecture/adr.md` | ADR-091 行（v1.16）· ADR-094 行（★ 层次化主题的**决策注册入口**） | 索引 | ✅ |
| ★ **描述性总览** | `docs/architecture/overview.md` · `interface-design.md` | 总体架构 / 接口形态（含 v1.16 补注） | 描述（★ 非规则） | — |
| ★ **推导证据** | `.rddf/plans/backend-parity-open-core.md` | ★ **§K17~§K33**（★ 实测约束与变异对照） | 证据（★ 非规则） | ★ **K34~K36 待 builder 起草** |
| ★ **过程日志** | `.rddf/reviews/ADR-094-decision-ledger.md` | 17 议题 · 裁决 D-01~D-21 · §9 交接总览 | ★ **过程**（★ 不引用为规则） | ✅ |
| ★ **过程日志** | `.rddf/reviews/ADR-094-hierarchical-assembly-contract-review.md` | 五轮 critic + oracle 终审的 finding 与处置 | ★ **过程** | ✅ |
| ★ **实施面** | `openspec/changes/backend-parity-open-core/` | 开放核 change（★ `tasks.md` = **AC 权威**） | 实施 | proposed |
| ★ **实施面** | `openspec/changes/parity-observation-ambiguity-fail-closed/` | C-1 三态化（★ 阶段 1 前置） | 实施 | proposed |
| ★ **实施面** | `openspec/changes/chmem-hierarchical-assembly/` · `chmem-hierarchical-unit-landing/` | ★ ADR-094 的 2 个 change（★ **尚未立项** ⇒ 归 planner） | 实施 | ★ **待立项** |
| ★ **实现面** | `ip/cpu/core/regfile_chmem.h` · `regfile_bridge_chmem.h` | ★ **层 2 唯一实例** + 其桥接 plugin | 代码 | ★ 现状 = 裸 `make_unique`（帧残留）· ★ **该目录 0 个 `.md`** ⇒ 残留 ⑪ |
| ★ **验证面** | `tests/cpu/test_cpu_open_core_regfile.cpp` | 层 2 实例的验证 | 代码 | ✅ |
| ★ **治理面** | `docs/architecture/adr/ADR-046/083/089/092/093` | 5 处 dated 注记 / 管辖域注记 / 编号撞名澄清 | ★ **注记**（★ 各自主文档的决策不变） | ✅ |
| ★ **上游** | `CppHDL/docs/adr/ADR-058~062` | 层次化输出 / 归属命名 / 端口策略 / 粒度 / 单元接口类型 | ★ **上游权威** | ✅ 全 Accepted |
| ★ **上游** | `CppHDL/docs/architecture/2026-11-02-hierarchical-rtl-output.md` | 上游版主题文档（含等价性 helper 等收口记录） | 上游描述 | ✅ 已转正 |
| ★ **上游** | `CppHDL/docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md` | ★ **层次化 RTL 写作指南（用户视角）**：§1 **7 条纪律 D1~D7**（★ **D1** 子模块接口用 `ch_in`/`ch_out` 标量、禁 bundle 成员跨层；★ **D2** 子模块 `ch_out` 无人消费 ⇒ 污染扁平顶层端口表；★ **D4** 子模块每个输入 MUST 被显式驱动 —— ★ ★ **本行与本仓条款的对应关系仅供定位**（★ D1↔r.11 · D2↔ADR-091 v1.16 订正③ · D4↔r.5）⇒ ★ **以两份文档各自正文为准**，★ 本行不承载规则）+ §2 粒度（**ADR-061**）+ §3 三种写法风格 | 上游指南 | ✅ Active |
| ★ **上游** | `CppHDL/docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md` | `ch_device<T>`（独立模式）vs `ch_module<T>`（层次模式）生命周期与用法 ⇒ ★ 关联 **V-26 / V-30**（层 2 独立复用形态） | 上游指南 | ✅ Active |

### 0.3 ★ 权威分布表（★ 「同一主题只允许一处说了算」）

| 主题面 | ★ 唯一权威 | ★ 其余文档的角色 |
|---|---|---|
| 装配契约（怎么建 / 怎么成层 / 怎么登记） | **ADR-094** | ADR-091 §D5.1.3 只留索引；本文只留结构事实 |
| 两层范式 / 硬约束 / 观测量载体 | **ADR-091 v1.16** | ADR-094 只承接「装配与端口纪律」 |
| 门禁面契约（登记簿 / 清单 / error_kind） | **`parity-gate-contract.md`** | ADR-094 D-3 只定方向 + 引用 |
| 推导与证据 | **plan §K17~§K33** + 台账 | ADR / 主题文档只引用结论 |
| 验收判据（AC） | ★ **change 的 `tasks.md`** | ADR 侧 = **生成镜像**（MUST 由工具产出） |
| 上游机制 | **CppHDL ADR-058~062** | 本仓 ADR 只承接与澄清 |
| ★ **当前态快照** | ★ **权威（实施中）= 实施面**（`tasks.md` / plan）⇒ ★ **实施批次落地后即以实施面为准** | ★ 本文件 §1 = **截至 2026-11-07 的 dated 快照**（★ 实施前的事实）⇒ ★ **不由 builder 持续维护**（★ 防职责冲突 ⇒ 防腐化） |

### 0.4 ★ 导航表的维护规则（★ 防腐化）

| 规则 | 内容 |
|---|---|
| ★ **M-1 单一真源** | ★ 本表是**指针表** ⇒ **MUST NOT** 在此重述任何权威文档的结论（★ 重述 = 必然漂移） |
| ★ **M-2 变更触发** | ★ 新增 / 删除 / 改名 / **状态翻转**（★ 如 ADR-094 转 Accepted、change 立项）**任一相关文档** ⇒ MUST 同步本表（★ 判据：★ 该文档是否被 §0.1 任一阅读路径或 §0.3 任一权威面触及）· ★ ★ **无触发机制** ⇒ ★ **改名 / 状态变更方 SHOULD 反向 grep 本文件**（★ `grep -rn "<旧名>" docs/ .rddf/ openspec/`）⇒ ★ 最低成本纪律 |
| ★ **M-3 反向可达** | ★ 任一相关文档 SHOULD 在其「Related / 关联」段回指本文件 ⇒ ★ **读者从任一入口进入 ⇒ 都能找到本文件**（★ 当前仅 3 处回指 ⇒ ★ 缺口见 §7 残留 ⑨） |
| ★ **M-4 失效检测（★ 诚实口径 · 无门禁兜底）** | ★ ⚠️ **本表条目当前零机检覆盖** —— ★ `tools/doc_link_check.sh` 的**检查 2（反引号路径）仅覆盖根级 3 文档**（`AGENTS.md` / `README.md` / `CONTRIBUTING.md`）· ★ **检查 1 只认 markdown 链接语法**（★ 形如「方括号文本 + 紧接圆括号路径」的 `.md` 链接 ⇒ ★ 本表条目**不是**该形态 ⇒ 不被覆盖） · ★ `SCAN_PATHS` **不含** `.rddf/` ⇒ ⇒ ★ 本表条目**既不被正向检查、也不被反向检查覆盖** ⇒ ★ **失效检测 = 纯人工**（★ 复查本表时逐条 `ls` 一次）⇒ ★ 机检方案见 §7 残留 ⑩ |
| ★ **M-5 更新归属** | ★ 本节（§0）由 **architect** 维护（★ 低频）；★ §1~§11 的**结论性内容**归各权威文档 ⇒ ★ ★ **本文件不再承载需高频刷新的状态** ⇒ ★ **§0.2 状态列 = dated 快照**（★ 见该列表头）⇒ ★ **architect MUST NOT 为追平 builder / planner 的状态事件而维护本表**（★ 那正是 §0 引言要防的「高频内容拖累低频内容」）⇒ ★ **状态以各权威文档头部为准** |

---

## 1. 当前态快照（**2026-11-07 实测**）

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
| **N4** | ★ 顶层端口名 MUST NOT 与内部 `ch_reg` / `ch_node` 同名（★ **v1.16 扩面**：判定对象 = **ctx 节点名** vs **顶层端口名**） | `CppHDL/src/codegen_verilog.cpp:64-76`（全局去重 ⇒ 重名者加 `_N` ⇒ 端口被改名 ⇒ 两后端端口名不一致） | ADR-091 **G-1o** · ADR-094 D-3.6 |
| **N5** | ★ **节点名唯一性** MUST 有 **opt-in** 的创建期 fail-fast（解释器侧不改名 / Verilator 侧改名 ⇒ 按名解析会**静默采到错节点**） | 上游 `NodeNamePolicy::kRejectDuplicates`（`CppHDL/include/core/context.h:51-55`，已实现） | ADR-091 **G-1o** / V-25 · ADR-094 D-6 阶段 1 |
| **N6** | ★ **公共头 ODR 安全**（★ 上游**已修**：`build_hierarchical_name` 移到 `component.h:266` 且 `inline`）⇒ ★ ChipForge 的 G-1b **换根**（ODR 根消失 ⇒ 理由 = include 面最小化 + `ch::detail` 命名二义） | `CppHDL/include/component.h:266` | ADR-091 **G-1b** · ADR-094 **V-34** |
| **N7** | ★ 观测 MUST 按**逻辑契约名**解析（★ 不再要求「契约名 == ctx 节点名」）⇒ ⇒ 契约身份与节点名**解耦** | 上游 `ch_device` 的顶层端口**本身就带 `top_` 前缀**（`top.<port>` → `top_<port>`）⇒ ★ 「裸名」是 ChipForge 侧约定 | ADR-094 **D-3.1~D-3.9** · 契约文档 **S-17 / S-17.1** |
| **N8** | ★ 层 2 单元接口 MUST 是**逐字段标量端口**（★ bundle / `ch_stream` 的成员是 `type_proxy`、**不是端口** ⇒ 被上游接口白名单排除；且 `ch_in<ch_stream<…>>` 退化成**整宽 blob 的单个 `type_input`** ⇒ 语义空壳） | `CppHDL/src/hierarchy_port_check.h:60-90` · 实测探针 | ADR-094 **r.11 / r.16** · D-5 |

---

## 3. 目标形态的结构事实

### 3.1 语义定义（★ 权威在 ADR-094 D-1/D-2）

- **模块边界 = 所有权边界**；RTL module 树 ≡ **被显式声明为成层**的组件子树（★ 默认 inline）。
- **装配**与**成层判定解耦**：装配点可无 active parent；★ 目标形态 = 单元经 `root.create_child` 登记并截帧（★ 权威 = **ADR-094 r.2**）。
- **归属**唯一真源 = 上游 `Component::node_owner()`；★ ChipForge 的 root-scope 覆盖**整个构建期**（`build()` + `elaborate()`）⇒ 插件级节点归 root、单元内部节点归单元。

### 3.2 模块关系（★ 结构图）

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
        │ 扁平 RTL（今天的门禁路径）│          │ 层次化 RTL（交付路径 · 尚未接入）│
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

### 3.4 装配时序（★ 阶段 1 / 阶段 2）

```
阶段 1（owner 仍 == nullptr）              阶段 2（不可拆的一批）
★ 注：★ 此处「owner == nullptr」仅指**顶层端口与插件级节点**（root-scope 未启用）⇒ ★ **不含**单元内部节点（单元内部节点归单元）
─────────────────────────────            ─────────────────────────────
① 宿主建 ctx → PipeBuilder(ctx)           ① owner 切换（root-scope 全局化）
② plugin setup()：declare_in/out 建端口     ② 登记簿上线（自动 + 例外通道）
③ plugin setup()：层 2 单元 create_child    ③ manifest 产出 + provider 安装
   + build(ctx)（★ 共用宿主 ctx）          ④ 闸①（框架内）+ 闸②（gate 侧）
④ 阶段 1 前置：同名普查（★ 不通过 ⇒ 先修）   ⑤ r.14 归属守卫生效
⑤ C-1 三态化（精确名解析）                ⑥ 迁移期基线捕获 + V-35/V-36 比对
★ 不产 manifest ⇒ 不走闸②                ★ 两条判据验收后退役
```

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

## 5. 命名规则（★ **目标态（阶段 2 起）**；★ 与 §1.2 的「当前态（节点名无前缀）」**不矛盾** —— 两者是**两个阶段**的形态）

| 对象 | 规则 | 例 |
|---|---|---|
| **root 实例名** | ★ 契约常量 **`cpu`**（★ 改名 = 契约变更：需 ADR + 重基线） | `cpu` |
| **逻辑契约名** | ★ **清单唯一真源** = 契约文档 **S-17.1 `node_keys`**（★ 35 项）⇒ ★ 报告 / 白名单 / 脚本参数一律用它 | `reg_1` · `pc` · `mem_sig` |
| **ctx 节点名** | ★ owner 非空 ⇒ 带路径前缀（`lnodeimpl.cpp:33-37`）⇒ ★ **与逻辑契约名解耦** | `cpu.reg_1` |
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
| ⑨ | ★ **反向可达性不完整**（★ 2026-11-07 补录 · 入口化后暴露）：★ 当前**仅 3 处**回指本文件（ADR-094 §Related · `overview.md` · `interface-design.md`）⇒ ★ **ADR-091 自身未回指**（★ 其 Related 列了 7 个 ADR + 契约文档，唯独未列 ADR-094 与本文件）· ★ **`parity-gate-contract.md` 未回指** ⇒ ★ 读者从「两层范式权威」或「门禁契约权威」这两个最可能入口进入 ⇒ **走到死路** ⇒ ★ 违反 §0.4 **M-3** | ✅ **已闭合**（2026-11-07：★ ADR-091 §Related 补 ADR-094 + 本文件两条 · ★ 契约文档头部补导航入口一行 ⇒ ★ 回指方 **3 → 5**） |
| ⑪ | ★ **层 2 唯一实例的实现目录无文档落点**：★ `ip/cpu/core/` 下 **8 个头文件、0 个 `.md`**（★ 唯一层 2 实例 `RegFileComponent` + 其桥接 plugin 都在这里）⇒ ★ ★ **§0 导航层能指到代码，但代码侧无任何自述文档** ⇒ ★ 「装配形态为何是裸 `make_unique`」「为何裸 `make_unique` 是错的」这两条知识**只存在于本文件 §1.2 与 ADR-094** ⇒ ★ **实现侧读者无就近落点** ⇒ ★ 候选 = `ip/cpu/core/README.md`（★ ★ **归 builder**：★ `ip/cpu/core/` 不在 architect 的 `arch_paths` 内 ⇒ ★ architect 不写） | ⚠️ **未建**（★ 归 builder） |
| ⑩ | ★ **导航表无反向完备性门禁**：★ `tools/doc_link_check.sh` 只查**正向**断链（文档内引用的路径是否存在）⇒ ★ **无法发现「相关文档未被入口收录」或「入口表条目已失效但未被引用」** ⇒ ★ ★ **且 §0 全景表条目本身也不被覆盖**（★ `doc_link_check.sh` 检查 2 仅根级 3 文档 · 检查 1 只查 markdown 链接 · `SCAN_PATHS` 不含 `.rddf/`）⇒ ★ ⇒ 本表**正向与反向均无机检** ⇒ ★ 当前缓解 = §0.4 **M-2/M-4** 的人工纪律（★ 无机检）⇒ ★ 候选 = 在 `doc_link_check.sh` 增加「§0 全景表条目 ⇒ 路径必须存在」的单向校验（★ **低成本**，★ 不需反向可达判定） | ⚠️ **未建**（★ 归 builder：★ architect 不写 `tools/`） |

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

★ 探针脚本位置：★ A-1 已在会话中实测（★ 临时路径 `/tmp/opencode/probe_scope.cpp`）⇒ ★ **随 ①-A 固化入仓**（★ 理由：上游主题文档 §9 的教训 —— **临时目录的探针不可复算** ⇒ 探针 MUST 入仓才能作为判据）⇒ ★ **A-2 / A-3 为读码结论，未跑探针**（★ 诚实标注）。

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
| 审稿记录 | `.rddf/reviews/ADR-094-hierarchical-assembly-contract-review.md`（★ 五轮 critic + oracle 终审（ADR-094 侧）★ + **§7h：导航层 oracle + critic 一轮（本文档侧）**） |
| 决策台账 | `.rddf/reviews/ADR-094-decision-ledger.md`（17 议题 · D-01~D-21 · §9 交接总览） |
| ★ 本文档 | 🚧 **Draft**（★ 当前态快照 2026-11-07 实测）⇒ ★ **ADR-094 治理收口（立项 + AC 回填）完成后**转正 |
| ★ 下一步 | ★ **planner 立项**（2 个 change）★ + ★ **builder 实施**（5 批）★ —— ★ **跨会话并行**（否则死锁）★ + ★ **builder 补 3 项**（残留 ⑩ 导航表校验脚本 · 残留 ⑪ `ip/cpu/core/README.md` · FSM 私有 ctx 核查） |

# ADR-094（层次化装配契约）—— 架构文档修订方案 · 双轮审稿记录

> ★ 本文件 = **审稿报告**（过程日志载体）⇒ **不进 ADR 正文**（ADR-094 尚未落笔）。
> ★ 记录对象 = 「架构文档修订方案 v1 → v2」的 oracle 产出与 critic 两轮审查。
> ★ 落盘人 = architect（2026-11-06）。
> ★ ★ **配套台账** = [`ADR-094-decision-ledger.md`](ADR-094-decision-ledger.md)（议题表 + 逐议题结论 + 用户裁决日志）—— ★ **第 3 轮 critic（2026-11-07，用户批准追加预算）的 finding 与处置见本文件 §7c**。

## 1. 审查对象与范围

| 项 | 内容 |
|---|---|
| ★ **对象** | 「架构文档修订方案」——针对 **ADR-091 v1.16**（订正 4 处 + G-1b 换根 + 新增 G-1r 索引 + §D9 边界）与 **新建 ADR-094**（装配契约 / 粒度 / 命名端口纪律 / 交付定位 / chlib-stream 边界） |
| ★ **拟改文档** | `docs/architecture/adr/ADR-091-*.md`（v1.16）· **新建** `docs/architecture/adr/ADR-094-chmem-hierarchical-assembly-contract.md` · `docs/architecture/adr.md`（注册表）· `docs/architecture/parity-gate-contract.md` · ADR-089（例外白名单 scope 注） |
| ★ **不在 architect 写面** | `.rddf/plans/**` · `openspec/changes/*/tasks.md` · `ip/cpu/core/*.h` 注释 · `include/cf/plugin/pipe_builder.h` · `tools/check_plugin_portability.sh` ⇒ 只出落点清单 |
| ★ **落笔状态** | ★ **尚未落笔**（ADR-094 不存在；ADR-091 仍 v1.15 / 606 行）⇒ 本记录只覆盖**方案**的审查 |
| ★ **上游基线** | CppHDL HEAD `dbc9801`（2026-11-06）· 层次化 5 ADR **全部 Accepted** · 层次化 change **全部归档** |

## 2. 审查轮次与结论

| 轮 | 审查者 | 结论 | `mergeable` | BLOCKER | scope_exceeded |
|---|---|---|---|---|---|
| ★ **1** | critic（`arch` + `plan`） | ★ **REJECT** | `no` | ★ **2**（B1、B2） | `false` |
| ★ **2** | critic（复审，只验闭合） | ★ **APPROVE_WITH_CONDITIONS** | ★ `yes` | ★ **0** | `false` |

★ 评审预算：同一 change ≤2 轮 critic ⇒ ★ **已用满 2 轮** ⇒ 后续若还有 BLOCKER ⇒ **不得再发起第 3 轮**，须用户批准。

## 3. 第 1 轮 BLOCKER 与处置

### B1 —— Q1 被 r.6 事实性裁决，却仍列为「待裁决」⇒ 骑墙

- **位置**：方案 r.6（MUST 条款）vs 待裁决清单 Q1 vs 自认矛盾 N-2。
- **冲突**：r.6「顶层端口 MUST 在 `scope_depth()==0` 且 `node_owner()==nullptr` 时创建」**就是 Q1 的 nullptr 分支** ⇒ 读者无法判断 Q1 是否已闭。
- **处置（已修，Δ1）**：★ **Q1 改为「证据闭案 + 用户签认项」** ⇒ 闭案结论 = **nullptr 分支**：
  - 依据（排除法，非选型）：owner 非空 ⇒ 节点名被 `<owner>.` 前缀、sanitize 后变 `<owner>_` ⇒ **契约名破坏**（`CppHDL/src/core/lnodeimpl.cpp:29-35` + `CppHDL/include/codegen_naming.h:44-52`；探针①实测）⇒ 与已 Accepted 的契约名 `reg_1..reg_31` / `open_pc` / `tohost_probe_data_proxy`（ADR-091 §D3a.9）**不相容**；
  - owner==nullptr 在层次化端口表构造中**机制可用**（`CppHDL/src/hierarchy_analysis.cpp:236-290`）；
  - 「归 root」分支 = **被既有契约排除**（非悬案）；「改造上游第三路」= **明确不在本轮范围**。
- **复审判定**：★ **闭合**（非替用户拍板：排除法 + 保留为签认项）。

### B2 —— D-5「不引入 `ch_stream`」无 scope ⇒ 与已 Accepted ADR + 在役代码冲突；补救措辞是假事实

- **位置**：D-5 vs 冲突清单第 2 条（拟写「上游能力保留、**本仓不实例化**」）。
- **冲突**：ADR-091 §D3a.3 的端口契约**就是** `ch_stream<CpuBusReq/CpuBusResp>`（ADR-091:132/:150）；ADR-092 §D4（Accepted）裁 `ch_stream` = IP 间/核↔从设备 ⇒ 采用；`ip/cpu/core/bus_iface_chmem.h:11-13` 在役声明「核对外暴露 `ch_stream<CpuBusReq>`」。⇒ 「本仓不实例化」= **假事实**。
- **处置（已修，Δ2）**：
  - D-5 加 scope：「**层 2 单元装配域与核内连接**不引入 `ch_stream` 作连接机制；★ **§D3a.3 的核↔从设备 `ch_stream<CpuBusReq/CpuBusResp>` 契约不变**（物理实现 = 逐字段标量端口，`ip/cpu/core/ibus_port_chmem.h:21-30`）」；
  - ADR-092 §D4 处置 = **不 bump 版本**、**不写「本仓不实例化」** ⇒ 改为「**适用范围注记**：§D4 裁定的用途保持不变，**层 2 装配域**由 ADR-094 D-5 管辖」；
  - ADR-083 §4.2 处置 = 保留其对 **CppTLM** `ch_stream` 的拒绝原意，仅加 scope 注。
- **复审判定**：★ **闭合**。

## 4. 第 1 轮 SHOULD_FIX / NIT 的处置（8 条，全部已修）

| # | finding | 处置 |
|---|---|---|
| SF-1 | ADR-093 的 `G-1a/G-1b` 是**它自己 §D2 本地定义**的 Phase-2 启动门禁，与 ADR-091 框架层 G-1b **只是撞名** ⇒ 「连带改 ADR-093」属误读 | ★ **删该连带改**；改为 ADR-094 元数据加一句「G-1r 与 ADR-093 局部 G-x 编号无关」 |
| SF-2 | 「S-9a × H-3 交集在任何文档都未登记」= **过度声称**（该交集**已登记**在 ADR-091 H-3 v1.13 A-5） | ★ 改为「承重判据已登记；**未落地的是** H-3 自带待办『MUST 在 ADR-089 / parity-gate-contract 登记』这一半」⇒ 行动只补后半 |
| SF-3 | 「上游 ADR-046:33」引用错位（该裁决在**本仓** `docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md:33`；上游 ADR-046 是 rv32i-canonical-home） | ★ 理由改引本仓 ADR-046:33（其转引上游 `state_machine.h:183-185`）；ADR-094 **不引**上游 ADR-046 |
| SF-4 | 「接口载体」规则在 ADR-091 §D3a.7 与 ADR-094 r.11 两处全文 ⇒ 必然漂移；G-1o 扩面修正案与本体分家 | ★ ADR-091 §D3a.7 该行改「**一句结论 + 指针**」；**G-1o 扩面在 ADR-091 原地 bump**，ADR-094 D-3 只引用不重述 |
| SF-5 | A.1 实质逆转 v1.12「用户裁决 B」，未登记取代关系 | ★ v1.16 版本史登记「取代 v1.12 §D3a.7 裁决 B」；待裁决清单增**签认项** |
| SF-6 | ADR-094 缺显式回滚路径 | ★ Consequences 补：「回滚 = 整组丢弃层次化产物；门禁消费扁平 `top.v` 的路径**不变**（门禁面零风险）」 |
| NIT-1 | r.6 的 fail-closed 机制口径需对齐 ADR-047 Check 10（禁 throw） | ★ r.6 补「MUST 以 `Result`/`std::unexpected` 表达（MUST NOT throw）」 |
| NIT-2 | r.1/r.6 依赖「`create_child` 返回后作用域帧已截断」的时序，方案未明写 | ★ r.2 补该时序前提（探针①实测支撑） |

## 5. 第 2 轮残留 SHOULD_FIX（★ 记录后放行，进 backlog）

| # | 内容 | 落点 |
|---|---|---|
| ★ **SF-A** | v1.16 版本史只登记「§D3a.7 取代裁决 B」，★ **未登记 G-1o 行内扩面**（G-1o 是 v1.11 签批的硬约束；按该 ADR 自身惯例，行内扩面应登记） | ADR-091 v1.16 版本史段 —— 落笔时补 |
| ★ **SF-B** | 「完整规则**只在** ADR-094 r.11」的表述与 ADR-091 §D3a.7 **G-1q 行**（v1.14 签批，仍含层 2 输入规则全文）并存 ⇒ 权威归属表述不准（结论一致、无二义，不阻断） | ADR-094 r.11 / 方案措辞 —— 落笔时改：r.11 侧引用 G-1q，或明确「G-1q 行 = 层 2 输入规则本体，r.11 = 接口载体」 |

## 6. critic 第 2 轮附注（交 build/verifier，★ 推断未验证）

★ Q6 的依据（`chlib::ch_state_machine` 在 CppHDL simulator 内不能推进状态机 ⇒ 解释器后端必挂）若在 simulator 下**普遍成立** ⇒ 在役多周期插件（ADR-046 豁免清单内）的双后端行为值得一次**实证抽查**；若它们实际能过 Simulator 后端 ⇒ Q6「禁用」的依据强度需在用户裁决前**重新标定**。⇒ 归 Q6 的裁决输入核查。

## 7. 落笔前必须由用户裁决 / 签认的清单（★ 不得由 architect 代决）

| # | 类别 | 内容 | 影响 |
|---|---|---|---|
| 1 | ★ **签认** | root 归属模型 = **nullptr 分支**（Δ1 证据闭案） | ADR-094 r.6 / D-3 / V-27 / V-29 / 全部端口名契约 |
| 2 | ★ **签认** | §D3a.7 接口载体改写**取代 v1.12 裁决 B** | ADR-091 v1.16 版本史 + ADR-094 r.11 |
| 3 | 裁决 | Q2 regfile 观测面形态（31 独立顶层 `ch_out` / 打包宽端口 / 内存导出） | H-1（≤64bit）· runner 兼容 · 每端口一条 CHWARN 的代价 ⇒ **只进 tasks** |
| 4 | 裁决 | Q3 首批成层清单 | ⇒ **只进 tasks**，不进 ADR |
| 5 | 裁决 | Q4 ADR-091 拆分落点（A 迁 ADR-094 / B 迁 `interface-design.md` / C 改 `arch-docs.json` / D 不拆） | 文档体系 + 可写路径 |
| 6 | 裁决 | Q6 `chlib::ch_state_machine` 在层 2 是否禁用（D-5 草案 = 禁用） | ADR-094 D-5 措辞 + ADR-046 关系 |
| 7 | 裁决 | Q7 `PipeBuilder::build()` 本轮是否加早退 + 改 `built_` 置位时机 | 框架行为变更 + 既有 runner 回归风险 |
| 8 | 裁决 | Q9 G-1b 是否接受「保留禁令、换掉理由」（删除需多 TU 链接探针 + 命名二义探针） | G-1b + ADR-093 撞名注记 |
| 9 | 裁决 | Q8 向上游提哪几条回执（U-1 `ch_layer` 在无 active parent 处不可达 / U-2 U-4 前置已过时 / U-3 白名单谓词在 `src/` 不可复用 / U-4 必产 N 条归属空洞 CHWARN） | 跨仓协作 |

## 7b. 决策日志（★ 用户裁决，2026-11-07）

| # | 议题 | 裁决 | 影响 |
|---|---|---|---|
| ★ **D-01** | 顶层端口的 owner（#01） | ★ **采纳 (b) + 登记簿(i)** ⇒ **推翻**本文件 §7-1 的「证据闭案 = nullptr 分支」 | ★ 「顶层端口 owner == nullptr」不再是目标态；★ 契约身份**从 ctx 节点名解耦**（登记簿）；★ 「顶层端口不许带前缀」这条私有约定**可删除** ⇒ 与上游 `ch_device` 惯例（`top_out_data`）一致 |
| ★ **D-02** | 迁移策略 | ★ **(i) 两阶段**：C-1（`resolve_node_id` 三态化）**原样先行**；登记簿随 ADR-094 的 builder 阶段落地 | ★ C-1 的三态边界**语义不变**（`kAmbiguous` 的实现位置从 runner 上移到创建期 + 登记期，属 ADR-094 落地后的演进） |
| ★ **D-03** | 治理 | ★ **批准 critic 第 3 轮**（预算 2 轮已用满，用户显式追加） | ★ ADR-094 内容因 D-01 实质变化 ⇒ 走第 3 轮审查 |

### D-01 的可行性核验（★ 本轮实测/读码，**不需要上游改动**）
★ `Component::node_owner()`（`CppHDL/src/component.cpp:16-26`）的判定链 = 「`node_ownership_suppressed()` ⇒ nullptr；**`scope_` 非空 ⇒ `scope_.back()`；否则回落 `current_`**」⇒ ★ **`Component::set_current(root)`（public static，`include/component.h:104`）即可让运行时动态创建的顶层端口归属 root** ⇒ 上游 `__io` 的「编译期静态 struct」限制**不构成障碍**（顶层端口是运行时动态声明的）。
⇒ ★ 反向也有上游-native 路径：`NodeOwnershipSuppression`（`include/core/context.h:58-63`，D10 用于 `ctx::init` 期间）⇒ **(a) 也可实现** ⇒ ★ **(a)/(b) 的实现成本几乎相同** ⇒ 选型依据**纯粹落在架构方向（债务）上**，不落在实现难度上。

### D-01 带来的新增事实（★ 必须写进方案 v3）
1. ★ **扁平产物的 Verilog 端口名会变**：owner=root ⇒ 节点名 `cpu.reg_1` ⇒ sanitize 后 `cpu_reg_1`（`include/codegen_naming.h:44-52`）⇒ ★ 影响面 = ① 开放核的 Verilator 侧读取路径（现状**硬编码** `r->top__DOT___reg_N`，`tools/verilator_runner/sim_main.cpp:93-100` ⇒ 那是**预载核内部**信号；开放核的观测是顶层端口 ⇒ A6 本就要重写读取路径）② 开放核测试的按名断言 ③ `names_hash`（**两侧同步变化 ⇒ 仍相等**，因 determinism 是**同设计跨后端**比对，`tools/parity/parity_gate.py:275-283`）。
2. ★ **跨设计的节点名不同**（开放核带 root 前缀、预载核不带）⇒ ★ 不影响 S-2（determinism 不跨设计比对）。
3. ★ **已 Accepted 的 ADR-091 §D3a.9 契约名（`reg_1..reg_31` / `open_pc` / `mem_sig` / `tohost_probe_data_proxy`）一个字不用改** ⇒ 它们变成**登记簿的 key**（逻辑契约名），与 ctx 节点名**解耦**。
4. ★ **新风险 R6**：root 的名字成为**产物的一部分** ⇒ 改名 ⇒ 全部 Verilog 端口名变 ⇒ 基线 churn（是 oracle R4 的加强版：R4 是「单元改名」，R6 是「根改名」）。
5. ★ **新风险 R7**：`set_current(root)` 是 **thread-local 全局** ⇒ root-scope RAII **必须只包住端口创建**（`declare_in/out` 内部）⇒ 否则插件在自己 `describe()` 之外创建的节点会被误归 root。
6. ★ **登记簿缺口的封堵必须是硬约束**（不是 tasks 可选项）：★ ① `elaborate()` 收尾扫全部顶层 `type_output` ⇒ 每个 MUST ∈ 登记簿 ∪ 显式 allowlist；★ ② gate 侧 manifest 键集 == 契约文件键集 ⇒ diff ⇒ ERROR。

## 7c. 第 3 轮审查（★ 用户 2026-11-07 批准追加预算）

| 项 | 值 |
|---|---|
| ★ 对象 | ★ **方案 v3 增量**（Δ7~Δ13：方向变更后的部分）★ v1/v2 已通过条款**未重开** |
| ★ 结论 | ★ **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · **0 BLOCKER** · 5 SHOULD_FIX · 2 NIT · `scope_exceeded: false` |
| ★ 关键裁决 | ★ **Δ8 `set_current(root)` 不违反上游 ADR-059 D1 意图**（D1 原文 `scope_.empty() ? current_ : scope_.back()`）⇒ 是**使用**既有机制而非另立归属规则；★ **Δ13 落点分流成立**（登记簿契约落 `parity-gate-contract.md` + ADR-089，ADR-094 只留引用） |

### 5 条 SHOULD_FIX 的闭合（★ 已应用于方案 v3.1）

| # | finding | 闭合 |
|---|---|---|
| ★ **SF-1** | 闸①「顶层 `type_output`」定义未钉死 ⇒ R10 留逃逸口（`owner==nullptr` 的输出在分析层被映射为 root，语义上就是顶层端口） | ★ 闸①扫描对象 = 「`type_output` **且**（`owner == root` ∨ `owner == nullptr`）」；`type_clock`/`type_reset` 与 `owner ∈ 子组件` 的端口**显式排除**（各附理由：clock/reset 是时序基础设施非观测；子组件端口属该 module 的接口面，不是顶层端口） |
| ★ **SF-2** | allowlist 无治理 ⇒ 它就是漏登记的后门 | ★ allowlist **落 `parity-gate-contract.md`**（与契约文件同仓同审）⇒ 变更**走 architect 审**（对齐既有「扩契约需 architect 审」门槛）⇒ **MUST NOT** 随手编辑 |
| ★ **SF-3** | Δ9.3（登记簿记全部）与 Δ9.5（闸②要求键集相等）口径冲突 ⇒ 闸②要么恒红要么被架空 | ★ **选 (a)**：登记簿**内部**记全部（`declare_in/out` 全登记），但 **manifest 导出面只过滤出「观测绑定」** ⇒ 闸②比对的是「manifest 观测键集 == 契约文件观测键集」⇒ 口径自洽 |
| ★ **SF-4** | Δ11.1 引「ADR-094 G-1s」与 Δ13 分流矛盾 | ★ **G-1s 只定义一处** = `parity-gate-contract.md`（+ ADR-089 记 ERROR 语义）⇒ ★ **ADR-091 §D3a.9 与 ADR-094 均改为引用该处**；★ 且（见 NIT-1）**不占用 ADR-091 的 `G-1*` 命名空间** |
| ★ **SF-5** | ★ **Δ10 未钉 Δ8 的落地批次 ⇒ C-1 有全量变红风险**：`resolve_node_id` 是全名精确比较（`parity_sim_runner.cpp:249-255`）⇒ 若先落 owner=root 再跑 C-1 阶段 1 ⇒ 端口名带前缀而契约名仍是裸名 ⇒ **全量 `kMissing` ⇒ 门禁恒红** | ★ Δ10 补：「**owner=root 切换与登记簿同批（阶段 2）落地；阶段 1 维持 `owner == nullptr`**」⇒ ★ 两阶段之间的**顺序前提被写死** |

### 2 条 NIT 的闭合
| # | finding | 闭合 |
|---|---|---|
| NIT-1 | `G-1*` 是 ADR-091 的硬约束命名空间；登记簿本体若落 `parity-gate-contract.md` ⇒ 命名空间错位 | ★ **不新增 `G-1s`** ⇒ 登记簿契约用 `parity-gate-contract.md` 自身的编号段；★ ADR-091 只留一行**引用**（不占用 G-1 编号） |
| NIT-2 | Δ8 的 RAII 与 Δ9.1 的登记点是 `declare_in/out` 内**同一处**改动 ⇒ 分两批会出现「归属已切、登记未至」的中间态窗口 | ★ **合并为一个 tasks 项**（归属切换 + 登记 + manifest 三件事同批） |

### critic 交 build/verifier 的验证建议（★ 推断未验证）
★ 闸①定义补丁后跑一个 PoC：在 `node_ownership_suppressed()` 激活期**裸建**一个 `ch_out`（绕过 `declare_in/out`）⇒ 确认 `elaborate()` 收尾按新定义（owner==nullptr ∨ owner==root）**扫到并报错**，而非静默通过。

## 8. 当前状态

- ★ **方案 v2 = 可落笔状态**（critic 第 2 轮 `mergeable: yes`，0 BLOCKER）。
- ★ **ADR-094 尚未落笔** ⇒ 「ADR 完成的标志」5 条**未满足** ⇒ **不得**标记为已决（Status 保持 `Proposed`）。
- ★ 下一步 = 收齐 §7 的 9 项（其中 1/2 为签认、3~9 为裁决）⇒ 落笔 ADR-091 v1.16 + ADR-094 + 注册表 + 契约文档 ⇒ 补齐 SF-A / SF-B ⇒ 收口时把本记录誊入 `.rddf/reviews/<adr>.md` 指针。

---

## 7d. ★ oracle 文本级审查（2026-11-07 · 落笔后）

> ★ 对象 = 落笔后的 **ADR-094（新建）+ ADR-091 v1.16 + `parity-gate-contract.md` S-17/S-1/S-9a + ADR-089 + `adr.md`**。

| 项 | 结果 |
|---|---|
| ★ 结论 | ★ **PASS-WITH-NOTES** · ★ **0 BLOCKER**（按 arch 四类逐项判：无冲突未取代 / 无缺备选-理由-回滚 / 无文内自相矛盾 / 无未决写成已决） |
| ★ 四要素 | ✅ 齐备（元数据含「关联 change 待立项 / AC 占位」的诚实标注 · 6 行选项表 · 独立后果/代价含回滚 · Verification 12 条前瞻判据） |
| ★ 内部一致性 | ✅ 4 项特别核对全过（r.6×事实4 ⇒ 自洽导向登记簿 · r.12×D-3.2 一致 · 接口载体无双居所 · G-1o 扩面与 D-3.6 同一权威） |
| ★ 与既有 ADR | ✅ 无未处理冲突（ADR-092/083 作用域不重叠 ⇒ 注记足够、不 bump；ADR-046 处置含双依据 + 明确路径；H-2/H-5 未被稀释） |
| ★ 忠实性 | ✅ D-01~D-06 逐条对照台账，全部落实（含 C1 全局 root-scope 与 #25 fallback） |

### 7d.1 处置（★ 已修，落笔文件）

| # | finding | 处置 |
|---|---|---|
| **1** | 事实 2 行号 `hierarchy_analysis.cpp:130-158` 实为边遍历/容错段；两条 FATAL 在 **:211-239** | ★ **已修**（ADR-094 改为 `:211-239`；★ 已复核上游源码确认） |
| **2** | D-5 ①「周期成本 MUST 显式登记」**无落点** | ★ **已修**（补「落点 = ADR-089 例外白名单的机制说明字段，与 ③ 同一处」） |
| **3** | r.6 措辞可回引已否决的 #23 窄口径 | ★ **已修**（r.6 补「root-scope 口径 = D-6：覆盖整个构建期，非单次建节点」） |
| **4** | ADR-093 撞名澄清缺失（其 Status 行无归属锚点） | ★ **已修**（ADR-093 Status 行下加编号澄清：其 `G-1a/G-1b/G-3` 是本文 §D2 局部门禁，与 ADR-091 §D5.1 仅撞名） |
| **6** | S-1 注的示例 `<root>_reg_1` 是 sanitized 形态 | ★ **已修**（补「raw 节点名 `<root>.reg_1` ⇒ Verilog 名 `<root>_reg_1`」） |
| **8** | ADR-091 G-1p 的 `component.cpp:107-118` 是**存量**错误（实为移动构造；`external_ctx` 分支在 `:271-282`） | ★ **已修**（同批改正；已复核上游源码） |
| **5** | S-17 REG-4 比 ADR-094 D-3.8 更严（多「commit message MUST 引裁决/审稿编号」） | ★ **接受**（更严方向安全，不改） |
| **7** | `adr.md` §2.4 统计表漂移（ADR-087 起未纳入类别统计） | ★ **记 backlog**（存量簿记问题，不在本 change 顺手修） |

### 7d.2 ★ 超出文本审查的风险（★ 归阶段 tasks，不是 ADR 文本问题）

| # | 风险 | 处置建议（→ tasks） |
|---|---|---|
| ★ **R-A** | **C1 全局 root-scope 的固有副作用**：阶段 2 后「current module = root」覆盖 `build()+elaborate()` 全程 ⇒ 插件在 `at_stage` 闭包内**误建**的节点也归 root，且**不触发任何登记/告警**（闸①只扫 `type_output`）⇒ 「全局归属」会把这类归属错误**合法化**为不可见 | ★ 阶段 2 tasks 增一条探针：**elaborate 期新建节点的 owner 分布**诊断（期望：仅 root / 单元自身两类） |
| ★ **R-B** | **阶段 1 先行的隐含前提未显式验证**：`kAmbiguous` 若命中存量同名节点 ⇒ 现网 52 case 直接打红。`reg_file_chmem.h:160-163` 的撞名已修，但「**整个存量 ctx 无同名节点**」**无全量审计证据**（D-6 只论证了 `kMissing` 侧） | ★ 阶段 1 tasks 增一条前置探针：对现网 52 case 的 ctx 做一次**同名节点普查** |

---

## 7e. ★ critic 第 4 轮（★ 用户 2026-11-07 批准追加预算）—— 文本级

| 项 | 结果 |
|---|---|
| ★ 结论 | ★ **APPROVE_WITH_CONDITIONS** · `mergeable: **yes**` · ★ **0 BLOCKER** · 4 SHOULD_FIX + 3 NIT · `scope_exceeded: false` |
| ★ 可否转 Accepted | ★ **可以**（★ 4 条 SHOULD_FIX 全为注记/引用级，不动决策内容；★ 其中 SF-1 / SF-4 建议**先闭合再转**） |
| ★ focus 核对 | ① 接口载体**无双居所**（ADR-091 `:231` 只留结论 + 指针，r.11 承载全文）② G-1o 扩面 ↔ D-3.6 **同一权威**（两处均写「命名权威 = 同一张 ctx 级全局表」）③ r.6 × Context 事实 4 **自洽**（owner=root ⇒ 名字带前缀 ⇒ 由登记簿解耦）④ S-17 ↔ D-3 内容**逐字一致但重复承载**（⇒ SF-1） |
| ★ 未决检查 | ✅ r.4「可选 API」· r.12「推荐非 MUST」· D-5 ③「周期成本登记」（落点已补）· V-35/V-36 可机检 · 「关联 change 待立项 / AC 占位」诚实标注 |

### 7e.1 处置

| # | finding | 处置 |
|---|---|---|
| **SF-1** | D-3.2 / D-3.7 / D-3.8 / D-3.9 全文承载 S-17 REG-1/2/3/4/5 ⇒ **权威双居所**（与第 3 轮 SF-4 / NIT-1 闭合口径不一致） | ★ **已闭合**：ADR-094 D-3 的登记簿四行改为**引用 S-17**（保留 D-3.1/3.3/3.4/3.5/3.6 这些 ADR 侧决策） |
| **SF-2** | ADR-091 §D3a.9「`resolve_node_id` 兼容」行以「runner 零改动」为决策理由 ⇒ v1.16 阶段 2 后已不成立 | ★ **已闭合**：§D3a.9 加 v1.16 注（「零改动」仅 v1.13 语境成立；★ **现行解析语义以 S-17 为准**；★ 裁决本身不变）+ **V-19** 加同款注 |
| **SF-3** | ADR-092 §D4 / ADR-083 §4.2 的适用范围注记**经批准但未执行** | ★ **已闭合**：两篇各加一条「管辖域不重叠、本条不被推翻」的注记（**不 bump 版本**） |
| **SF-4** | D-3.5 冻结「root 名 = 契约常量」但**全篇未给基准值**（S-1/S-17 均以 `<root>` 占位） | ★ **未闭合 ⇒ 转入决策议题 T1**（★ 值属决策，★ 不由 architect 自行选定） |
| **NIT-1** | ADR-089 新增注记含 MUST 但无版本行登记 | ★ **已闭合**：ADR-089 Status 行补 dated 附注（★ 声明「执行的是 ADR-091 v1.13 已签批的登记义务，非新决策；版本号不变」） |
| **NIT-2** | REG-5「报告中标注」vs D-3.9「写进契约」措辞分岔 | ★ 随 SF-1 收敛（ADR-094 改为引用 S-17 ⇒ 分岔消失） |
| **NIT-3** | `component.cpp:252-262`（二次 build 拒绝）两处引用 | ★ **已闭合**：ADR-094 r.7 改为指向 ADR-091 §D5.1.3（单一出处） |

★ **复验**：`verify_adr.sh` ✅（33 通过 / 0 STALE / 0 FAILED）· `doc_link_check.sh` ✅ 0 broken。


---

## 7f. ★ 议题讨论轮（2026-11-07 · T1~T17）—— 处置总览

> ★ 第 4 轮 critic 之后进入**逐题讨论**（用户驱动，一题一裁）⇒ 每题先做**长期架构自审**（★ 用户明确要求「不要遗留架构债务」）⇒ 再落文档。

| 轮 | 议题 | 裁决 | 落点 | ★ 自审发现并已修正 |
|---|---|---|---|---|
| T1 | root 的身份/形态/命名契约 | ★ **D-07** | ADR-094 r.1 / D-3.5 · S-17 REG-2 · ADR-091 §D5.1.3 | ★ root 名应为**本设计**的常量（多 root 场景各自成常量） |
| T2 | 登记簿登记点与 manifest 形态 | ★ **D-08** | ADR-094 D-3.2 / D-3.10 · S-17 REG-1/REG-2 | ★ 期望键集 MUST **注入**（框架不内置 parity 概念）· ★ 失败 typed end-to-end |
| T3 | 两道闸的实现位置与失败语义 | ★ **D-09** | ADR-094 r.13 · S-17 REG-3 · **S-10 error_kind 清单** | ★ 清单单一真源 + 「某侧 manifest 缺失 ⇒ ERROR」（防静默退化）· ★ allowlist 须带退役条件 |
| T4 | 阶段 1 前置同名普查 | ★ **D-11** | ADR-094 D-6 阶段 1 / **V-39** · S-10 `observation_ambiguous` | ★ `kAmbiguous` MUST 与 `observation_missing` 语义可区分 |
| T5 | root-scope 覆盖面与 R-A | ★ **D-12** | ADR-094 r.1 / **r.14** / D-6 阶段 2 / **V-40** · S-10 `node_ownership_escape` | ★ R-A 的正解 = **前移上游归属判据**（扁平路径也跑）⇒ 单一语义、无第二套规则 |
| T6 | 预载核侧表达与退役条件 | ★ **D-13** | ADR-094 D-3.9 / **D-3.11** · S-17 **REG-5/REG-6** + `contexts` | ★★ 「fallback = 第二条路径」被推翻 ⇒ **绑定提供者模型**（两侧语义一致）· ★ manifest 义务**按语境**声明（修掉单一列表 ⇒ H-3 恒红） |
| T7 | V-35/V-36 口径与基线捕获 | ★ **D-14** | ADR-094 **V-35 / V-36** / D-6 阶段 2 | ★ V-36 升级**双硬判据** + **抓取器自检**（防「抓取器失效 = 恒绿」）· ★ 两条标注**迁移期判据**，阶段 2 后退役 |
| T8 | 单路径强制点 | ★ **D-15** | ADR-094 **r.15** / D-3.9 / D-3.11 / **V-41** · S-10 `provider_conflict` + **GN-1 通用总则** | ★★ 修正**内部矛盾**（provider 由语境决定 vs 框架不认识 parity）⇒ **槽位按 ctx 单一 + 调用方决定装哪个** |
| T9 | change 名 + AC 映射 | ★ **D-16** | ADR-094 Status / 关联 change / AC 映射 | ★★ **change 数 3 → 2**（★ 边界 = **交付批次边界** ⇒ 阶段 2 同批不可跨 change）· ★★ **AC 权威 = `tasks.md`**（ADR 侧为生成镜像 + 机检相等） |
| T13 | 实施面四项 | ★ **D-17** | ADR-094 r.10 / **r.16** / **V-42** · ADR-091 §D3a.9 | ★ `build()` 幂等语义精化（**状态已变 ⇒ fail-closed**）+ `built_` 成功收尾置（★ 已核实 `elaborate()` `pipe_builder.h:429` 的 fail-open）· ★ **r.11 升级为编译期纪律**（层 2 基类） |
| T14 | 落地顺序与切 builder 时机 | ★ **D-18** | 台账 §9 | ★★ 修正**与 D-16 的不一致**（批次若无独立验收轮次 ⇒ 债务）⇒ **批内 MUST 分轮验收**；★ **立项与实施跨会话并行**（否则死锁） |
| T10 | ADR-046:33 订正 | ★ **D-19** | ADR-046 dated 附注 | ★ 只**附注**事实 + 实测，★ 不改其决策内容（★ 下次修订正式订正） |
| T11 | 上游回执范围 | ★ **D-20** | 台账 §9 | ★ **U-4 不提**（★ 前提已被 D-01 消解 ⇒ 提了也是无效回执）；★ 提 **U-1 + U-5**（U-2 并入）；★ U-3 不提 |
| T12 | `adr.md` §2.4 漂移 | ★ **D-21** | `adr.md` §2.4 口径注 | ★ **显式化漂移 + 登记 backlog**（★ 不在收口轮顺手补登 ⇒ 易引入新错账） |

★ **四轮 critic**（2 + 1 + 1）+ **17 议题**（T1~T14 + T10~T12 + T15）⇒ ★ 用户裁决 **D-01~D-21** ⇒ ★ 全部落进 **ADR-094 + ADR-091 v1.16 + `parity-gate-contract.md` S-17/S-10 + ADR-089/083/092/093/046 + `adr.md`**。
★ **未决**：★ 无（★ 架构类议题全部闭合）⇒ ★ 余下 = **治理收口**（立项 + AC 回填 ⇒ Status 转正）与**实施面**（change ①/②）。

---

## 7g. ★ 落笔后终审（2026-11-07 · oracle 文本级 + critic 第 5 轮）

> ★ 用户指令：「请 @oracle → @critic 进行审查，通过后提交」。

### 7g.1 oracle 终审（文本级）

| 项 | 结果 |
|---|---|
| ★ 结论 | ★ **PASS-WITH-NOTES** · ★ **0 BLOCKER** · ★ 14 处 `file:line` 抽查**全部通过** · ★ 判「**可 commit**」 |
| ★ 一致性核验 | ① r.6↔D-3.1↔D-3.2↔事实 4 **自洽** ② r.14↔r.1/r.6↔D-6↔V-40 **一致**（★ 时间线有含糊点 ⇒ 已修）③ r.15↔D-3.9↔D-3.11↔REG-5↔S-17.1↔V-41 **逐字一致**（★ 但 D-3.9 仍全文承载 REG-5 ⇒ 声称闭合未闭合 ⇒ 已修）④ r.16+r.11↔V-42↔D-5↔ADR-091 §D3a.7 **无双居所** ⑤ 时间线自洽（★ S-17 正文缺生效时间标注 ⇒ 已修）⑥ S-10 error_kind **无孤儿/无缺失** ⑦ §Options 覆盖充分 ⑧ ★ **ADR-091 §D3a.9 / V-19 的括号残留旧机制描述**（「manifest 命中优先 + 回退精确名」）⇒ 与 D-13 推翻的 fallback 表述冲突 ⇒ 已修 |
| ★ 缺口 | ★ **缺阶段 2 回滚**（已修）· ★ 「①-A 之后 root-scope 口径」未钉死（已修）· ★ r.14 生效理由措辞（已修）· ★ REG-5 退役口径过弱（已修「全 corpus」）· ★ D-3.5↔r.7 多 root 路径含糊（已修） |

### 7g.2 critic 第 5 轮（终审）

| 项 | 结果 |
|---|---|
| ★ 结论 | ★ **APPROVE_WITH_CONDITIONS** · `mergeable: **yes**` · ★ **0 BLOCKER** · ★ **明确「可 commit」** · `scope_exceeded: false` |
| ★ focus 闭合度 | ① D-3.2/D-3.9 收敛后**「规则无处可寻」不存在**（★ 目标条款逐条在 S-17 可寻）② S-17 生效阶段标注**无双居所**（★ 「阶段 1 恒红」误读通道已消除）③ 未决/回滚/同批清单/既有 ADR 残留冲突 **均合规**（★ Status 行是**反向诚实标注** ⇒ 不构成「未决写成已决」） |
| ★ SHOULD_FIX（4） | ★ ① D-6 阶段 1 理由「登记簿尚未落地」过时 ⇒ 改「①-B 建、①-D 激活」★ ② ★ **r.10 无对应 Verification**（★ 会被 AC↔V 相等机检判遗漏）⇒ **新增 V-43** ★ ③ D-3.3「契约名一个字不改」与 T15 统一自相抵牾 ⇒ 标注**例外 = T15** ★ ④ ADR-091 V-25 行内枚举五契约名 = `node_keys` 的**第二份清单** ⇒ 改为引用 S-17.1 |
| ★ NIT（3） | V-14 示例补「逻辑契约名」标记 · 「G-1r r.1~r.12」区间过时 ⇒ 改 r.1~r.16 · `adr.md` 「待 critic 审后转正」过时 ⇒ 改「转正前置 = 治理收口」 |

★ **处置**：★ 上列 **4 SHOULD_FIX + 3 NIT 全部已修**（★ 新增 **V-43** ⇒ AC 覆盖数由 17 ⇒ **18 个条目**）⇒ ★ 门禁复验：`verify_adr.sh` ✅（33/0/0）· `doc_link_check.sh` ✅ 0 broken · S-17.1 JSON 机器校验合法。

### 7g.3 ★ 审查轮次总账

| 轮 | 审查者 | 对象 | 结论 | BLOCKER |
|---|---|---|---|---|
| 1 | critic | 方案 v1 | REJECT | 2 |
| 2 | critic | 方案 v2（复审） | APPROVE_WITH_CONDITIONS | 0 |
| 3 | critic | 方案 v3（Δ7~Δ13，★ 用户批准追加预算） | APPROVE_WITH_CONDITIONS | 0 |
| 4 | critic | **文本**（v0.1 首版） | APPROVE_WITH_CONDITIONS | 0 |
| 5 | critic | **文本**（终审 · 含 D-01~D-21 与全部修复） | ★ **APPROVE_WITH_CONDITIONS · 可 commit** | ★ **0** |
| — | oracle | 文本终审（独立） | ★ **PASS-WITH-NOTES · 可 commit** | ★ **0** |

---

## 7h. ★ 导航层（入口）化（2026-11-07 · oracle + critic 一轮）

> ★ 用户问：「`docs/architecture/hierarchical-plugin-architecture.md` 可以作为层次化 plugin **所有文档的入口**吗？」

### 7h.1 architect 的定位判定（★ 结论先行）

| 入口类型 | 判定 | 依据 |
|---|---|---|
| ★ **导航入口**（读者找路的起点） | ★ **可以** | 它是唯一横跨「决策 / 契约 / 推导 / 实施 / 实现 / 验证 / 上游」的主题文档 |
| ★ **权威入口**（真相所在） | ★ **不能，也本就不该** | ★ 权威分布 = ADR-094（装配）· ADR-091 v1.16（两层范式）· `parity-gate-contract.md`（门禁面）· plan / 台账（推导证据）· change `tasks.md`（AC）· 上游 ADR-058~062 ⇒ ★ 主题文档首行即写「决策不在这里」⇒ ★ 若兼任权威入口 ⇒ **自我矛盾**（与 ADR-094 讲同一件事时必然漂移） |

### 7h.2 ★ 三类缺口（★ 全部实测，非推测）

| # | 缺口 | 实测依据 |
|---|---|---|
| ① | ★ **覆盖面不够**：§6「与既有文档的关系」仅 6 行 ⇒ 漏上游 Parent（5 ADR）/ 实现面（层 2 唯一实例）/ 验证面 / 治理面（5 处 dated 注记） | 全仓 + 上游扫描 |
| ② | ★ **反向可达性不完整** ⇒ 「入口」名不副实：★ 仅 3 处回指 ⇒ ★ **ADR-091 自身未回指**（其 Related 列了 7 个 ADR + 契约文档，唯独未列 ADR-094 与主题文档）· ★ **`parity-gate-contract.md` 未回指** ⇒ ★ 读者从最可能的两个入口进入 ⇒ **走到死路** | `grep -rn "hierarchical-plugin-architecture"` |
| ③ | ★ **硬断链**：§6/§8/头部写 `plan §K17~§K36` ⇒ ★ 实测 plan **最大只到 K33** | `grep "^### K3[4-6]"` 零命中 |

### 7h.3 ★ 更深的长期问题（★ 比上面三条更重要）

「入口文档」的**内在风险 = 随时间腐化**（文档增删 / 改名 / 状态翻转 ⇒ 导航表过时）⇒ ★ 且 `tools/doc_link_check.sh` **既不查反向可达、连本表条目的正向都不覆盖**（检查 2 仅根级 3 文档 · 检查 1 只认 markdown 链接形态 · `SCAN_PATHS` 不含 `.rddf/`）⇒ ★ **导航表腐化不会被现有门禁发现**。
★ 更关键：§1 是**当前态快照** ⇒ ①-A~①-D 落地后**立即过时** ⇒ ★ 高频易变内容（§1）与低频稳定内容（导航）混在同一文件 ⇒ ★ §1 要 builder 持续更新，但它是 architect 拥有的文档 ⇒ ⇒ **职责冲突 + 必然腐化**。
⇒ ★ 解法：★ §0 导航层独立成节（低频 · architect 维护）· ★ §1 降级为 **dated 快照**（权威明确交给实施面）· ★ §0.4 M-1~M-5 写死防腐化纪律 · ★ §7 残留 ⑩ 登记「无机检」缺口 + 候选方案 + owner。

### 7h.4 审查结论

| 轮 | 审查者 | 结论 | BLOCKER |
|---|---|---|---|
| oracle | 文本级 | ★ **PASS-WITH-NOTES** | ★ **0** |
| critic | arch + plan 档 | ★ **APPROVE_WITH_CONDITIONS · mergeable: yes** | ★ **0** |

★ **oracle 的 finding（2 SHOULD_FIX + 5 NIT）已全修**：
- ★ **SHOULD_FIX-1（M-4 假安全感）**：★ oracle 读码发现 `doc_link_check.sh` 的 `PATH_CHECK_DOCS` **仅根级 3 文档** · 检查 1 只匹配 markdown 链接形态 · `SCAN_PATHS` **不含** `.rddf/` ⇒ ★ architect 原写的「MUST 与 `doc_link_check.sh` 同步修」= **假安全感** ⇒ ★ 改写为**诚实口径**（明写「本表条目当前零机检覆盖」+ 失效检测 = 纯人工）+ 连带顺 §7 残留 ⑩ 的措辞（★ 避免两处口径打架）
- ★ **SHOULD_FIX-2**：§0.2 补上游 `HIERARCHICAL-RTL-AUTHORING-GUIDE.md`（★ 其 D1 = r.11 的上游同源纪律 · D2 = **ADR-091 v1.16 订正③ 的上游依据**）+ `COMPONENT-LIFECYCLE-GUIDE.md`；§0.1 路径 2 追加上游写作指南
- ★ NIT ×5：§0.1 补「门禁红了怎么排查」路径 · `adr.md` 行删「§2.4 统计口径」（★ 那不是层次化内容）· §0.3「当前态快照」行改「权威（实施中）」· M-2 补「改名方 SHOULD 反向 grep」· §0.2 上游行的 D1/D2/D4 映射降险（★ 明示「仅供定位 · 以各文档正文为准」）

★ **critic 的 finding（1 SHOULD_FIX + 2 NIT）已全修**：
- ★ **SHOULD_FIX（M-5 vs §0.2 状态列冲突 · ★ 真问题）**：★ §0.2 状态列承载的恰是**最高频**状态（ADR-094 Proposed · K34~K36 待起草 · 3 个 change proposed/待立项）⇒ ★ 而 M-5 说「本文件不再承载需高频刷新的状态」+ M-2 触发面只含「新增/删除/改名」⇒ ★ ⇒ architect 要么被迫追 builder 事件（★ 正是 §0 引言要防的「高频内容拖累低频内容」），要么状态列静默腐化 ⇒ ★ **三条同改**：★ ① 状态列表头降级为「dated · 活状态以该权威文档头部为准」★ ② M-2 触发面扩到「**状态翻转**」★ ③ M-5 明写「architect MUST NOT 为追平 builder/planner 状态事件而维护本表」
- ★ NIT-1：上游 authoring guide 行的条款映射降险（★ 明示「仅供定位」）
- ★ NIT-2：文件标题体现「入口」

### 7h.5 ★ 门禁事件（★ 真实事件，非工具误报）

★ 中途 `doc_link_check.sh` 报 **1 broken**：★ architect 在 M-4 里写的**说明性示例**（markdown 链接形态的字面示例）**被检查 1 的正则当成了真链接** ⇒ ★ 已改写为纯描述（★ 不给字面示例）⇒ ★ 复跑 ✅ 0 broken。
⇒ ★ **教训**：★ 在本仓写「链接形态示例」时 MUST 避开检查 1 的正则形态，★ 否则**自己的说明会变成断链**。

### 7h.6 ★ 落地清单

| 文件 | 改动 |
|---|---|
| `docs/architecture/hierarchical-plugin-architecture.md` | ★ 新增 **§0 导航层**（0.1 阅读路径 9 条 · 0.2 文档全景表 18 行 · 0.3 权威分布表 7 面 · 0.4 M-1~M-5）· ★ 头部加「本文在体系中的位置」声明 · ★ 标题加「入口」· ★ 修断链（§K17~§K36 ⇒ §K17~§K33）· ★ §7 残留 8 ⇒ **11 条**（⑨ 反向可达 ✅已闭合 · ⑩ 无门禁 ⚠️归 builder · ⑪ `ip/cpu/core/` 零 `.md` ⚠️归 builder）· ★ §11 治理状态更新 |
| `docs/architecture/adr/ADR-091-...md` | ★ §Related 加 2 行（ADR-094 = 装配唯一权威 · 主题文档 = 导航入口）⇒ ★ **回指 3 → 5** |
| `docs/architecture/parity-gate-contract.md` | ★ 头部加 1 行（导航入口指针 + 「本文件是门禁面唯一权威 · 不承载装配与命名规则」） |

★ **门禁**：`verify_adr.sh` ✅（33 / 0 STALE / 0 FAILED）· `doc_link_check.sh` ✅ 0 broken。

---
initiative: wave7-architecture-governance
priority: P1
version_target: v0.11.0
status: proposed
adr:
  - ADR-094
  - ADR-091
  - ADR-089
depends_on:
  - backend-parity-ci
---

# chmem-hierarchical-assembly —— 层次化装配框架地基（阶段 ①-A ~ ①-D）

> **决策授权**：`docs/architecture/adr/ADR-094-chmem-hierarchical-assembly-contract.md`（🚧 Proposed v0.1 · **决策已闭合**）
> ⇒ 本 change **不重新决策**，只把 D-1 的 r.1~r.16、D-3 的 D-3.1~D-3.11、D-6 的阶段 1/2 落成可执行交付。
> **契约细则**：`docs/architecture/parity-gate-contract.md` **S-17**（REG-1/REG-2/REG-3/REG-4）
> **主题文档**：`docs/architecture/hierarchical-plugin-architecture.md`

## Why

**当前态（实测）**：`ch::toVerilogHierarchical` 与 `ch_device` 在上游已落地，但 ChipForge 侧
**没有装配契约** —— 单元如何被创建、所有权归谁、顶层端口由谁声明、跨单元如何连接，
全靠各 IP 的隐式惯例。后果是可预判的三类事故：

| 风险 | 机制 | 观测面 |
|---|---|---|
| **组件树不完整** | 单元用裸 `make_unique` 创建 ⇒ 未登记进 ctx ⇒ 上游层次化分析 **FATAL** | 发射期直接失败 |
| **归属空洞** | 顶层端口 owner 落在非 root 组件 ⇒ 产物里出现 `Bubbled` / `Synthesized` 伪端口 | `port_provenance` 不匹配 |
| **双轨装配** | 没有「一 ctx 一 root」与「provider 单槽」约束 ⇒ 运行期可装出两套上下文 | 结构上不可证伪 |

⇒ 本 change 的目标是让上述三类事故**在框架层不可表达**，而不是靠 review 拦。

## What Changes

### 1. 装配契约落地（D-1 r.1~r.3 / r.7）

- `PipeBuilder` 持有**恰一个** root `Component` + 只读访问器 `root_component()`
- 单元 MUST 经 `root.create_child<T>(name)` 创建（禁裸 `make_unique`），返回值 MUST 检查非空
- 单元 MUST 以 `pb.ctx()` 调 `build()`（禁 `build(nullptr)`）
- **一 ctx 恰一个 root**；root 生命期由 `PipeBuilder` 管理

### 2. 归谁所有 / 谁声明端口（D-1 r.5 / r.6 / r.8 / r.14）

- 每个单元声明**外部驱动输入集**；`setup()` 结束后集合内每项 MUST `impl() != nullptr && impl()->driver() != nullptr`
- 顶层端口 MUST 经 `PipeBuilder::declare_in/out` 创建，且创建点 MUST 在 **root-scope**
  （`Component::set_current(root)` 生效、`scope_depth() == 0`）
- 跨单元连接 MUST 落 root scope（桥接 / `Payload`）；**禁 sibling 直连**
- **归属守卫前移**（r.14）：把上游 `hierarchy_analysis.cpp:211-227` 的「owner MUST 在 root 子树里」
  前移到 **`Component` 侧**，装配收尾即 fail-closed

### 3. 层 2 基类与标量端口（D-1 r.11 / r.16）

- 新增 ChipForge 侧**层 2 基类**，**编译期**校验 `io_type` 每个成员是标量 `ch_in` / `ch_out`
  （`ch_uint<N>` / `ch_bool`）⇒ bundle / `ch_stream` / `ch_mem` / `ch_reg` 作端口 **编译不过**
- 端口名 SHOULD 带单元局部名前缀（r.12，推荐非 MUST）

### 4. `build()` 幂等语义（D-1 r.10）

- **同一状态**重复调用 ⇒ **成功早退**（不重跑 `negotiate/setup/build`，不产生重复登记）
- **状态已变** ⇒ MUST 显式失败（不是静默重跑）

### 5. 契约身份与登记闸（D-3.2 / D-3.5 / D-3.7 / D-3.9 / D-3.10 / D-3.11）

- 契约身份与 ctx 节点名**解耦**；契约本体 = **S-17**
- root 名**冻结为契约常量 `cpu`**（改名 = 契约变更，需 ADR）
- **两道机械闸**：闸① 框架内（期望键集 + allowlist，由**调用方注入**，框架不内置 parity 概念）；
  闸② gate 侧。判据 = S-17 REG-3
- **绑定提供者模型**：两侧一致的「逻辑契约名 → 节点」映射，差异仅在绑定方式
  （开放核走 `manifest` provider，预载核走 `identity` provider）
- **provider 槽位按 ctx 划分、每 ctx 恰一个** ⇒ 重复安装**立即失败** ⇒ 结构上装不出双轨
- manifest 的架构侧约束见 S-17 REG-1/REG-2；allowlist 变更 **MUST 经 architect 审**（REG-4）

### 6. 两阶段迁移（D-6，顺序写死）

- **阶段 1**：`resolve_node_id` **三态化**（`kOk` / `kMissing` / `kAmbiguous`）
  ⇒ 前置 = **同名普查**（一次性只读；范围含现网 52 case + CH_MEM binary 全量 case）
- **阶段 2**：owner 切换 + 登记 + manifest + r.14 **同批不可拆**
  ⇒ 验收失败 MUST **整批回退**（git 级 revert 该批）

## Out of Scope

- **首批成层单元的实际落地**（RegFileComponent 等）⇒ 姊妹 change `chmem-hierarchical-unit-landing`
- **多 root / 多核形态**（跨 ctx 多 root 违 G-1p）⇒ 另立 ADR
- **FSM 私有 ctx 的归属裁决**（ADR-094 §残留 ④，形态 A/B 未定）
- **层次化产物进 parity 门禁**（D-4 明写：门禁仍消费**扁平** `top.v`）
- **`chlib` 在层 2 内部的周期成本登记**（D-5，属 ADR-089 例外白名单机制）

## Capabilities

- **装配事故不可表达**：组件树不完整 / 归属空洞 / 双轨装配 在框架层直接失败，不进产物
- **交付面可用**：层次化产物可单独综合、单元可独立复用（`ch_device<...>`）
- **门禁面零风险**：门禁消费扁平 `top.v` 的路径不变（D-4 回滚路径 = 整组丢弃层次化产物）
- **契约可审计**：契约身份与节点名解耦 + 两道机械闸 + allowlist 经 architect 审

## Impact

- **修改**：`include/cf/plugin/`（`PipeBuilder` / `Component`）· `src/cf_plugin/` 相关装配路径
- **新增**：层 2 基类（编译期标量端口校验）· provider 槽位与 manifest 装载 · 归属守卫
- **不动**：parity 门禁判定语义（S-1~S-17 除 S-17 的登记细则外）· `top.v` 扁平产物路径 · `Payload` / `CtrlLink`
- **风险**：阶段 2 的 owner 切换**不可拆** ⇒ 单批回退粒度大（已在 ADR-094 §D-4 显式接受）

## Acceptance Criteria

> AC 权威 = 本段。每条对应 ADR-094 §Verification 的 V 编号。
>
> ★ ★ **2026-11-08 修订（planner · D-22 裁决 + metis/critic 审查）**：★ 原 AC 为**平铺 18 条、无批次归属**
> ⇒ 实施者无法判断「哪条在 ①-A 就该验、哪条必须等 ①-D」⇒ ★ 现按 ADR-094 §D-6 的两阶段 +
> `.rddf/reviews/ADR-094-decision-ledger.md` §9.2 的批次**分组**，并订正 5 处：
> ① **V-40 归 ①-D**（★ §9.2 原列进 ①-A ⇒ 阶段 1 期间 `owner == nullptr` ⇒ 该判据**必然全量 RED**）
> ② **V-35 / V-36 基线捕获归 ①-C 出口**（★ ADR-094:157「基线 MUST 在阶段 1 结束 / 阶段 2 开工前捕获，切换后**无法事后补**」）
> ③ **V-41 拆两半**（★ ①-A 只验「provider 槽位单例 + 重复安装立即失败」；manifest / identity 的安装期自检归 ①-B）
> ④ **V-34 / V-32 补归属**（★ 二者原先在批次表里**无归属** ⇒ 违反 ADR-094:13「无遗漏」）
> ⑤ **★ 新增 ①-A′ 前置适配**（★ `regfile_bridge_chmem.h:124` 的裸 `make_unique` ⇒ ①-D 的 r.14 **必然 RED** ⇒ 改造 MUST 前置）
>
> ★ **V 侧面标记**：同一个 V 可能被两个 change 各覆盖一次（**框架侧** / **落地侧**）⇒ ★ 每条 AC MUST 标侧面，
> ★ 机检「无重复」按「**同一 V 的同一侧面**」判定（★ 细则 = 本 change 的 `design.md`）。

### ①-A 框架地基（★ 出口 V-42 / V-43 / V-41a / V-34）

- [ ] **AC-1** ★ `PipeBuilder` 持有**恰一个** root `Component` + 只读访问器 `root_component()`；root 与 `pb.ctx()` 同 ctx；root 构造 MUST 包在 `Component::ConstructionFrame` 内；root 名 MUST == 契约常量 `cpu`；★ **该 root 与层次化 emitter 的 `root` 参数 MUST 是同一对象**（指针相等）★ **变异**：两者指向不同对象 ⇒ 归属判据失真 ⇒ RED（覆盖 V: V-31, V-34 · 侧面: **框架侧**）
- [ ] **AC-2** 单元 MUST 经 `root.create_child<T>(name)` 创建，MUST NOT 裸 `make_unique`；返回值 MUST 检查非空；★ **变异**：改回裸 `make_unique` ⇒ 组件树缺该单元 ⇒ 层次化发射 FATAL（覆盖 V: V-31 · 侧面: **框架侧**）
- [ ] **AC-3** 负向三变体：① 漏 `add_child` ⇒ FATAL ② ctor 传 `nullptr` + 事后 `add_child` ⇒ RED ③ 登记但不 `build` ⇒ FATAL（覆盖 V: V-31 · 侧面: **框架侧**）
- [ ] **AC-4** 层 2 基类的**编译期**校验：`io_type` 成员 MUST 为标量 `ch_in`/`ch_out`；★ **变异**：把某成员改成 bundle / `ch_stream` ⇒ **编译不过**；★ **正向**：绕过基类直接继承 `ch::Component` ⇒ 门禁 grep MUST 报出（覆盖 V: V-42 · 侧面: **框架侧**）
- [ ] **AC-5** `build()` 幂等（r.10）：同一状态重复调用 ⇒ **成功早退**、不重跑 `negotiate/setup/build`、不产生重复登记；状态已变（插件集指纹不同）⇒ MUST 显式失败（`PluginError::BuildStateConflict`）；★ `built_` MUST 只在**成功收尾**时置；★ **插件集指纹的字段与顺序 MUST 显式定义**（覆盖 V: V-43 · 侧面: **框架侧**）
- [ ] **AC-6** provider 槽位单例（r.15 前半）：同一 ctx 第二次安装 provider ⇒ MUST **立即失败**；★ **变异**：装两个 provider ⇒ MUST RED（覆盖 V: V-41 · 侧面: **框架侧**）
- [ ] **AC-7** ★ **r.6 的负向前置断言**：①-A 阶段 root-scope **MUST NOT 全局化**（顶层端口 owner 仍 `nullptr`）⇒ 顶层端口的声明位置判据（`scope_depth() == 0`）**延到 ①-D 生效**；★ **变异**：在 ①-A 就全局化 ⇒ 阶段 1 全量 `kMissing` ⇒ 门禁恒红 ⇒ MUST 被本条拦住（覆盖 V: V-31 · 侧面: **框架侧**）
- [ ] **AC-8** 2 TU 链接探针通过（框架头与新核头零 `<ch.hpp>` include）（覆盖 V: V-34 · 侧面: **框架侧**）

### ①-A′ RegFile 前置适配（★ 无独立 V · 是 ①-D 的**硬前置**）

- [ ] **AC-9** ★ `ip/cpu/core/regfile_bridge_chmem.h` 的 `std::make_unique<RegFileComponent>(nullptr, "regfile")` MUST 改为 `root.create_child<RegFileComponent>(...)` + 以 `pb.ctx()` 调 `build()`；★ **理由**：该行是 ADR-094:23 点名的现存缺陷、且是 **V-40 的变异靶** ⇒ 不改则 ①-D 的归属守卫**必然 RED**（覆盖 V: V-40 · 侧面: **框架侧**）

### ①-B 观测登记（★ 出口 V-37 / V-38 / V-41b）

- [ ] **AC-10** 登记簿 + manifest + 两道机械闸（闸① 框架内 · 期望键集**由调用方注入**；闸② gate 侧）⇒ manifest 缺失时 MUST `ERROR(2)` + `manifest_missing`（防「闸②退化为『有 manifest 才查』」）；★ **变异**：摘掉闸① ⇒ 静默通过 ⇒ RED（覆盖 V: V-37 · 侧面: **框架侧**）
- [ ] **AC-11** provider 安装期自检（r.15 后半）：装 `manifest` provider 时校验每个键的 node 存在且 raw 名一致；装 `identity` provider 时校验精确匹配到**唯一**节点；不符 ⇒ 立即失败（`provider_conflict`，★ 消息点名**逻辑名 + 两个候选节点名**）（覆盖 V: V-41 · 侧面: **框架侧**）
- [ ] **AC-12** 绑定提供者一致：同一 case 中两侧解析出的**逻辑契约名集合**一致；★ 报告 MUST 标注每个观测的 provider（`manifest` / `identity`）（覆盖 V: V-38 · 侧面: **框架侧**）

### ①-C 阶段 1（★ 出口 V-39 + V-35 / V-36 基线捕获）

- [ ] **AC-13** `resolve_node_id` **三态化**（`kOk` / `kMissing` / `kAmbiguous`）；★ **变异**：歧义仍返回首个节点 ⇒ RED（覆盖 V: V-39 · 侧面: **框架侧**）
- [ ] **AC-14** 前置**同名普查**：现网 52 case + CH_MEM binary 全量 case 的 ctx 中同名节点组数 = 0（或已逐组定位并修复）；★ **普查窗口 MUST 钉住**（commit + CH_MEM binary SHA）+ ★ **新增 case ⇒ 增量普查**（覆盖 V: V-39 · 侧面: **框架侧**）
- [ ] **AC-15** ★ **V-35 / V-36 基线捕获**（★ **时机 MUST = ①-C 出口 / ①-D 开工前** —— ADR-094:157）：逐 case 逐拍全观测量 hash 链（FNV，与既有 `pchash` 同构造）+ 终值逐位数组 + 环境标识；★ **变异**：捕获后人为注入一处观测差异 ⇒ hash 链 MUST 不同 ⇒ RED（覆盖 V: V-35, V-36 · 侧面: **框架侧**）

### ①-D 阶段 2（★ **不可拆** · 出口 V-40 + V-35 / V-36 比对与退役）

- [ ] **AC-16** ★ **同批不可拆**：owner 切换（root-scope 覆盖**整个构建期**）+ 登记上线 + manifest 上线 + **r.14 归属守卫生效**；★ 验收失败 MUST **整批回退**（git 级 revert 该批 ⇒ MUST NOT 只回退其中一项 ⇒ 会留下 owner / 解析路径的混合态）（覆盖 V: V-40 · 侧面: **框架侧**）
- [ ] **AC-17** ★ **三条同批改动**（★ 理由写明「不改则编不过 / 判据恒 RED」）：① `tools/parity_runner/parity_sim_runner.cpp` 的 `resolve_node_id` 精确名解析 ⇒ 改 manifest 驱动（★ 顶层端口名从 `reg_1` 变 `cpu_reg_1` ⇒ 精确名**全量失效**）② `tools/verilator_runner/sim_main.cpp` 的 `top__DOT___reg_N` **编译期硬编码**引用 ⇒ 改名或改 manifest 驱动 ③ `ip/cpu/core/regfile_bridge_chmem.h` 的裸 `make_unique` ⇒ 改 `root.create_child`（★ 见 AC-9）（覆盖 V: V-40 · 侧面: **框架侧**）
- [ ] **AC-18** ★ **V-35 / V-36 比对与退役**：切换前后 hash 链**逐 case 相等**（硬）+ 终值逐位相等（第二判据）+ 方向守卫触发集 ⊆ 阶段 1 触发集且阶段 2 触发集为空；★ 阶段 2 验收后 **MUST 整体退役**（★ 不得留成僵尸判据）（覆盖 V: V-35, V-36 · 侧面: **框架侧**）

## ★ 拆批说明（为什么是 2 个 change）

| change | 承担 | ADR-094 阶段 |
|---|---|---|
| **本 change**（`chmem-hierarchical-assembly`） | 框架地基：装配契约 + 归属守卫 + 层 2 基类 + 两道闸 + 两阶段迁移 | ①-A / ①-B / ①-C / ①-D |
| 姊妹 change（`chmem-hierarchical-unit-landing`） | 首批成层单元落地（RegFileComponent 等）+ manifest 键集 + 单元级复用 | ② |

★ 两者**并行**：单元的 `io_type` 契约（标量端口）由本 change 的基类定义，单元落地可直接消费；
但单元**成层**（`ch_device<RegFileComponent>`）依赖本 change 的阶段 2 完成 ⇒ 实施上后者在前。

## References

- `docs/architecture/adr/ADR-094-chmem-hierarchical-assembly-contract.md`（D-1 ~ D-6 + V-27~V-43）
- `docs/architecture/parity-gate-contract.md` **S-17**（REG-1/REG-2/REG-3/REG-4）
- `docs/architecture/hierarchical-plugin-architecture.md`（主题文档 · §0 导航层 / 残留清单）
- `docs/architecture/adr/ADR-091-chmem-open-core-port-architecture.md` §D3a（端口契约）
- `docs/architecture/adr/ADR-089-backend-parity-differential-gate.md`（门禁仍消费扁平 `top.v`）

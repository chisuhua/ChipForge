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
  - chmem-hierarchical-assembly
---

# chmem-hierarchical-unit-landing —— 首批层 2 单元落地（阶段 ②）

> **决策授权**：`docs/architecture/adr/ADR-094-chmem-hierarchical-assembly-contract.md`（🚧 Proposed v0.1 · 决策已闭合）
> **前置**：`chmem-hierarchical-assembly`（阶段 ① 的装配契约 + 层 2 基类 + 两道闸 + 阶段 2 owner 切换）
> **拆批依据**：ADR-094 §D-6 的阶段 ② —— **首批成层单元**。

## Why

阶段 ① 交付的是**框架能力**（能装配、能成层、能 fail-closed）。但框架能力没有**真实单元**消费时：

- 层 2 基类的**编译期标量端口校验**无法证明「真实单元的 `io_type` 真能写出来」—— 只能证明「写错了编译不过」
- 两道机械闸（S-17 REG-3）的 `manifest` 键集**没有真实样本** ⇒ allowlist 与键集的一致性无法验证
- 「单元级复用」（`ch_device<RegFileComponent>`）与「独立可综合」是 ADR-094 **D-4 的交付定位**，
  没有落地单元 ⇒ 该定位仍是声明

⇒ 本 change 用**首批单元**把阶段 ① 的每条契约压到真实代码上。

## What Changes

### 1. 首批成层单元（ADR-094 §D-6 阶段 ②）

> ★ ★ **D-22 用户裁决（2026-11-08）**：★ **首批成层 = `RegFileComponent`**（★ **取代 D-17 的「首批 = ALU」** ——
> ★ D-17 是 2026-11-07 的用户裁决，写「首批成层 = ALU（纯组合、端口干净）；第二批 = regfile 观测面显式化 + regfile 成层」；
> ★ D-22 采纳「首批 = RegFileComponent」⇒ ★ 理由：① `RegFileComponent` **已存在**（`ip/cpu/core/regfile_chmem.h:93`）
> ⇒ ② 它的装配形态（`regfile_bridge_chmem.h:124` 裸 `make_unique`）正是 ADR-094:23 点名的缺陷 + **V-40 的变异靶**
> ⇒ ③ 新建 ALU Component 是**净新增代码**，无法验证「既有缺陷是否被修」）
> ⇒ ★ **代价**：`ALU` 单元推迟到后续批次（★ 仍保留「纯组合、端口干净」这一验证价值的追求位置）。

- `RegFileComponent` 作为**首个**层 2 单元**成层**：标量 `ch_in` / `ch_out` 端口（r.11 / r.16）
- 端口名带单元局部名前缀（r.12 推荐项）⇒ 避免 ctx 全局去重加 `_N` 后缀
- 经 `root.create_child<RegFileComponent>(name)` 创建（r.2），以 `pb.ctx()` 调 `build()`（r.3）
- ★ **本批不含**「预载核观测面显式化」（★ 即把内部 `ch_reg` 观测改成顶层 `ch_out`）⇒ 那是 D-17 第二批的剩余部分，
  ★ 也是 `identity` provider 的**退役条件**（D-3.9：载体统一）⇒ ★ 本批之后预载核侧继续走 `identity` provider

### 2. 单元级复用与独立验证（V-30）

- `ch_device<RegFileComponent>` **独立复用**（脱离 CPU 也能 elaborate）
- `ch_device<Wrapper>`（Wrapper 内 `ch_layer`，覆盖**父路径命名**场景）

### 3. manifest 键集与 provider 绑定（D-3.9 / D-3.10 / D-3.11 · S-17 REG-1/REG-2）

- 为落地单元声明 `manifest`：**契约身份 → 节点**（逻辑契约名，与 ctx 节点名解耦）
- 与阶段 ① 的 `identity` provider 形成**两侧一致**：同一 case 中两侧解析出的逻辑契约名集合一致（V-38）
- 键集与 allowlist 的一致性由闸① 校验（框架只做「扫 + 比 + fail-closed」，不内置 parity 概念）

### 4. 与 parity 门禁的关系（ADR-094 §D-4 边界）

**不改**：门禁仍消费**扁平** `top.v`；本 change **不**把层次化产物接进门禁。
层次化产物只承担**交付 / 可单独综合 / 单元级复用**三个用途。

## Out of Scope

- **装配框架本身**（root 持有 / create_child / root-scope / 层 2 基类 / 两道闸 / 两阶段迁移）
  ⇒ 姊妹 change `chmem-hierarchical-assembly`
- **多级 TLB / 多 root / 多核形态** ⇒ 另立 ADR
- ★ **预载核观测面显式化**（★ 内部 `ch_reg` 观测 ⇒ 顶层 `ch_out`）⇒ ★ D-17 第二批的剩余部分，
  ★ 也是 `identity` provider 的**退役条件**（D-3.9「载体统一」+ 使用计数 = 0）⇒ 本批之后预载核侧继续走 `identity` provider
- **`chlib` 在单元内部的周期成本登记**（D-5 · 属 ADR-089 例外白名单机制）
- **层次化产物进 parity 门禁**（D-4 明写不做）
- **FSM 私有 ctx 归属**（ADR-094 残留 ④，形态未定）

## Capabilities

- **契约在真实代码上可验**：层 2 基类的编译期校验、manifest 键集、provider 一致性都有落地样本
- **可交付性有证据**：单元能被 `verilator --lint-only` 接受、能独立 elaborate、能单独复用
- **回滚面小**：整组丢弃层次化产物即可；门禁消费扁平 `top.v` 的路径不变（D-4）

## Impact

- **新增**：`ip/cpu/core/` 下首批层 2 单元（`RegFileComponent`）及其 manifest
- **修改**：CPU 装配路径改为经 `root.create_child<T>` 登记（依赖阶段 ① 的 API）
- **不动**：parity 门禁 · `top.v` 扁平产物 · `Payload` / `CtrlLink` / 顶层端口机制
- **风险**：单元端口形态（逐字段标量）与既有 bundle 形态**不通用** ⇒ 首批只迁 1 个单元，
  用它的真实成本（端口数量 / 连线量 / 周期成本）判断是否继续迁第二批

## Acceptance Criteria

> AC 权威 = 本段。每条对应 ADR-094 §Verification 的 V 编号。
>
> ★ ★ **2026-11-08 修订（planner · D-22 裁决）**：★ 按 **V 归属**重新分组并补 2 条缺失判据
> （★ 原 AC 段漏 **V-27**（层次化发射成功 / warnings 归类 / 0 undriven-bubble / 0 Synthesized）
> 与 **V-29**（ForceInline 逐字节一致）⇒ 违反 ADR-094:13「无遗漏」）★ 并把「既有回归零退化」
> 标为 **回归侧**（★ 它不新增 V 声明，只断言 AC-10 / AC-11 同时成立）。
>
> ★ **V 侧面标记**：与姊妹 change `chmem-hierarchical-assembly` 共有的 V（V-37 / V-38 / V-40 / V-42）
> 在此标 **落地侧**、在 assembly 标 **框架侧** ⇒ ★ 机检「无重复」按「**同一 V 的同一侧面**」判定。

- [ ] **AC-1** `RegFileComponent` 作为层 2 单元**成层**：`io_type` 成员全为标量 `ch_in`/`ch_out`，经层 2 基类**编译期校验**通过；★ **变异**：任一成员改成 bundle ⇒ 编译不过；★ **正向**：绕过基类直接继承 `ch::Component` ⇒ 门禁 grep MUST 报出（覆盖 V: V-42 · **落地侧**, V-30）
- [ ] **AC-2** `ch_device<RegFileComponent>` **独立复用**：脱离 CPU 上下文可 elaborate 并发射 Verilog（覆盖 V: V-30）
- [ ] **AC-3** `ch_device<Wrapper>`（Wrapper 内含 `ch_layer`）**父路径命名**正确（端口 / 节点路径带父层前缀，不撞名）（覆盖 V: V-30, V-32）
- [ ] **AC-4** 每实例端口名**逐字不同**；单元每个 `ch_in` 均有 driver（r.5 外部驱动输入集非空且被满足）（覆盖 V: V-32）
- [ ] **AC-5** manifest 键集与 allowlist 一致：闸① 扫到的节点集合 == 期望键集；★ **变异**：删掉 allowlist 一项 ⇒ RED（覆盖 V: V-37 · **落地侧**）
- [ ] **AC-6** 绑定提供者一致：同一 case 中两侧解析出的**逻辑契约名集合**一致（落地单元走 `manifest` provider，预载路径走 `identity` provider）；★ 报告 MUST 标注每个观测的 provider（覆盖 V: V-38 · **落地侧**）
- [ ] **AC-7** 归属守卫在落地单元上生效：构建收尾时除 clock/reset 外每个节点 owner ∈ `{root}` ∪ `{已登记单元}`；★ **变异**：把某节点挂到 root 之外 ⇒ 立即失败（覆盖 V: V-40 · **落地侧**）
- [ ] **AC-8** `port_provenance` 对落地单元精确匹配：顶层 input=`Bubbled` · 顶层 output=`InlineFolded` · clock/reset=`ClockReset` · 单元自身=`Self`（覆盖 V: V-27b）
- [ ] **AC-9** ★ **层次化发射成功且 `warnings` ⊆ 归属空洞类别**：**0 条 undriven-bubble**；若宣称「交付接口干净」⇒ **0 个 `Synthesized`**；★ **变异**：删一条实例连接 ⇒ RED（覆盖 V: V-27）
- [ ] **AC-10** 层次化 vs 扁平**逐拍等价**（机制锚 = 上游 `ch::hier_eq`，★ ChipForge 侧须真实接入，**不得只写文档**）；★ **前置断言 module 数 ≥ 2**（否则全 inline 下两产物相同 = 恒绿）（覆盖 V: V-28）
- [ ] **AC-11** ★ `GranularityScope(ForceInline)` 下产物与扁平**逐字节一致**（同一 ctx、同一 elaborate，**不重新 build**）（覆盖 V: V-29）
- [ ] **AC-12** 产物可交付：`verilator --lint-only --top-module top <file>` **exit 0**，stderr 无 `%Error`（覆盖 V: V-33）
- [ ] **AC-13** ★ **既有回归零退化**：`[chmem]` + `[verilator]` + `[chmem-config]` 的既有 case 全部维持（新增单元不得改变扁平产物语义）★ **判据 = AC-10 与 AC-11 同时成立**（★ 本条**不新增 V 声明** ⇒ 标 **回归侧**）（覆盖 V: V-28, V-29 · **回归侧**）
- [ ] **AC-14** 首批成本登记：单元端口数 / 连线量 / 周期成本写进 change 的 `tasks.md`（作为「是否继续第二批（ALU 等）」的判断输入）（覆盖 V: V-32）

## ★ 与姊妹 change 的边界

| 维度 | `chmem-hierarchical-assembly`（阶段 ①） | 本 change（阶段 ②） |
|---|---|---|
| 交付物 | 框架能力（装配/成层/fail-closed） | 真实单元（RegFileComponent）+ manifest |
| 判据主体 | `PipeBuilder` / `Component` / 层 2 基类 / 两道闸 | `ch_device<...>` 复用 / 端口形态 / 键集一致 |
| 依赖方向 | 无（地基） | **依赖阶段 ① 的 API** |
| 可单独 archive | 是 | 否（须先 ① 完成阶段 2） |

## References

- `docs/architecture/adr/ADR-094-chmem-hierarchical-assembly-contract.md`（D-3.9~D-3.11 · D-4 · D-6 阶段 ② · V-27b/V-28/V-30/V-32/V-33/V-37/V-38/V-40/V-42）
- `docs/architecture/parity-gate-contract.md` **S-17**（REG-1/REG-2/REG-3/REG-4）
- `docs/architecture/hierarchical-plugin-architecture.md`（主题文档 · 层 2 单元接口 = 逐字段标量端口）
- `docs/architecture/adr/ADR-091-chmem-open-core-port-architecture.md` §D3a.7（层 2 单元接口权威）
- 姊妹 change：`openspec/changes/chmem-hierarchical-assembly/proposal.md`

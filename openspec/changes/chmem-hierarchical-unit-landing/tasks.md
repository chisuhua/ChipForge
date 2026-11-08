# tasks —— 首批层 2 单元落地（`RegFileComponent`，阶段 ②）

> **决策权威**：ADR-094（🚧 Proposed **v0.2**）· **用户裁决 D-22**（首批 = `RegFileComponent`）
> **实施设计**：`design.md` · **AC 权威**：`proposal.md` 的 `## Acceptance Criteria` 段
> ★ **前置**：姊妹 change `chmem-hierarchical-assembly` 的 **①-D 完成**（集成前置；开发可并行）

---

## 0. 前置

- [ ] **0.1** ★ 姊妹 change 的 **①-D 已完成且验收通过**（★ owner 切换 + 登记上线 + r.14 生效 + V-40 通过）⇒ ★ **未完成 ⇒ 本 change 的集成不可开始**
- [ ] **0.2** ★ 缺口 **G3′**（生产代码零 `declare_out` 调用 ⇒ owner 切换后测试批量变红）已由 architect 处置（assembly 的 0.3）⇒ ★ 本 change 的「零退化」判据依赖它
- [ ] **0.3** ★ 缺口 **G6**：「既有回归零退化」的基线数字**实测确定**（★ 不引用历史快照数字 ⇒ 实施时重测）
- [ ] **0.4** ★ 上游 `ch::hier_eq` 的闭合公共面**已核实覆盖**本 change 的必要场景（★ `DrivenInput` / `Observation` / `ProductSet` / `Result` / `emit` / `compare` / `make_observation`）⇒ ★ 未覆盖 ⇒ **回上游扩展**，★ **MUST NOT** 在本仓自建等价比对器

---

## 1. 单元改造（★ r.11 · r.12 · r.16）

- [ ] **1.1** ★ `RegFileComponent` 改继承 ChipForge **层 2 基类**（r.16 · D-3）⇒ 判据：编译期标量端口校验通过 ⇒ ★ **变异**：任一成员改成 bundle ⇒ **编译不过**（V: V-42 · V-30）
- [ ] **1.2** ★ `io` struct 成员**全为标量** `ch_in`/`ch_out`（★ `ch_reg` 只作内部存储，★ **MUST NOT** 作端口）（r.11）⇒ ★ **变异**：把某成员改成 `ch_mem` / `ch_stream` ⇒ 编译不过
- [ ] **1.3** ★ 端口名带单元局部前缀（r.12 · ★ **推荐非 MUST**）⇒ 判据：避免 ctx 全局去重加 `_N` 后缀 ⇒ ★ 当前 `regfile_rs1_addr` 等**已合规** ⇒ 本条为**回归确认**
- [ ] **1.4** ★ `describe()` MUST NOT 读 `Payload`（G-1q）；MUST NOT 用 `is_layer()` / `granularity()` **改变可观测行为**（★ 仅允许用于诊断与断言）（r.9）⇒ ★ **变异**：在 `describe()` 里读 `Payload` ⇒ 门禁 MUST 报出
- [ ] **1.5** ★ 经 `root.create_child<RegFileComponent>(name)` 创建 + 以 `pb.ctx()` 调 `build()`（r.2 · r.3）⇒ ★ **①-A′ 已完成 ⇒ 本条为契约复验**：owner == root 且出现在层次化产物里 ★ **变异**：改回裸 `make_unique` ⇒ 层次化发射 FATAL（V: V-40）

---

## 2. manifest 键集与 provider 绑定（★ D-3.9 · D-3.10 · D-3.11）

- [ ] **2.1** ★ 为落地单元声明 **manifest**（逻辑契约名 → 节点）；★ **每侧各生成**（★ `node_id` 是 ctx 私有编号 ⇒ **MUST NOT** 用一份共享 manifest）（D-3.10）
- [ ] **2.2** ★ 键集与 allowlist 一致：闸① 扫到的节点集合 == 期望键集（★ 框架只做「扫 / 比 / fail-closed」）（V: V-37 · **落地侧**）⇒ ★ **变异**：删掉 allowlist 一项 ⇒ RED
- [ ] **2.3** ★ 绑定提供者一致：同一 case 中两侧解析出的**逻辑契约名集合**一致（★ 落地单元走 `manifest` provider，预载路径走 `identity` provider）（V: V-38 · **落地侧**）⇒ ★ 报告 MUST 标注每个观测的 provider
- [ ] **2.4** ★ 归属守卫在落地单元上生效：构建收尾时除 clock/reset 外每个节点 owner ∈ `{root}` ∪ `{已登记单元}`（V: V-40 · **落地侧**）⇒ ★ **变异**：把某节点挂到 root 之外 ⇒ MUST RED
- [ ] **2.5** ★ ★ **不得**把「预载核观测面显式化」纳入本批（★ 那是 `identity` provider 的**退役条件**，★ 本批后预载核侧继续走 `identity` provider）

---

## 3. 独立复用与命名（★ V-30 · V-32）

- [ ] **3.1** ★ `ch_device<RegFileComponent>` **独立复用**：脱离 CPU 上下文可 elaborate 并发射 Verilog；★ 端口列表与该单元的 `io_type` 一一对应（V: V-30）
- [ ] **3.2** ★ `ch_device<Wrapper>`（Wrapper 内含 `ch_layer`）**父路径命名**正确：端口 / 节点路径带父层前缀，不撞名（V: V-30 · V-32）
- [ ] **3.3** ★ 每实例端口名**逐字不同**；单元每个 `ch_in` 均有 driver（★ r.5 的外部驱动输入集**非空且被满足**）（V: V-32）

---

## 4. 层次化产物可交付（★ V-27 · V-27b · V-28 · V-29 · V-33）

- [ ] **4.1** ★ 层次化发射成功且 `warnings` ⊆ 归属空洞类别：**0 条 undriven-bubble**；若宣称「交付接口干净」⇒ **0 个 `Synthesized`**（V: V-27）⇒ ★ **变异**：删一条实例连接 ⇒ RED
- [ ] **4.2** ★ `port_provenance` 精确匹配：顶层 input=`Bubbled` · 顶层 output=`InlineFolded` · clock/reset=`ClockReset` · 单元自身=`Self`（V: V-27b）
- [ ] **4.3** ★ 层次化 vs 扁平**逐拍等价**（★ 机制锚 = 上游 `ch::hier_eq`，★ ChipForge 侧须**真实接入**，**不得只写文档**）⇒ ★ **前置断言 module 数 ≥ 2**（否则全 inline 下两产物相同 = **恒绿**）（V: V-28）⇒ ★ **变异**：反转一个端口 / 删一条连接 ⇒ RED
- [ ] **4.4** ★ `GranularityScope(ForceInline)` 下产物与扁平**逐字节一致**（同一 ctx、同一 elaborate，★ **不重新 build**）（V: V-29）
- [ ] **4.5** ★ 产物可交付：`verilator --lint-only --top-module top <file>` **exit 0**，stderr 无 `%Error`（V: V-33）

---

## 5. 回归与成本登记

- [ ] **5.1** ★ **既有回归零退化**：`[chmem]` + `[verilator]` + `[chmem-config]` 的既有 case 全部维持（★ 新增单元不得改变扁平产物语义）⇒ 判据 = **AC-10 与 AC-11 同时成立**（★ 回归侧，不新增 V 声明）（V: V-28 · V-29 · **回归侧**）
- [ ] **5.2** ★ **首批成本登记**：单元端口数 / 连线量 / 周期成本写进本文件（★ 作为「**是否继续第二批（ALU 等）**」的判断输入）（V: V-32）
- [ ] **5.3** ★ **出口 gate**：V-27 / V-27b / V-28 / V-29 / V-30 / V-32 / V-33 + 落地侧的 V-37 / V-38 / V-40 / V-42 逐条有证据 + 变异自检全绿 + 既有回归零退化
      ★ **回滚 commit**：`（实施时填）` ⇒ ★ 门禁消费扁平 `top.v` 的路径不变 ⇒ **回滚 = 整组丢弃层次化产物**（D-4）

---

## 6. 范围外（★ 明确不做）

- [x] ✗ 装配框架本身（root 持有 / `create_child` / root-scope / 层 2 基类 / 两道闸 / 两阶段迁移）⇒ 姊妹 change
- [x] ✗ **预载核观测面显式化**（内部 `ch_reg` ⇒ 顶层 `ch_out`）⇒ `identity` provider 的退役条件，★ 不在本批
- [x] ✗ **ALU 单元** ⇒ D-22 后推迟到后续批次
- [x] ✗ 层次化产物接进 parity 门禁（★ D-4：门禁仍消费扁平 `top.v`）
- [x] ✗ 多级 TLB / 多 root / 多核形态 ⇒ 另立 ADR

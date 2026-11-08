# design —— 首批层 2 单元落地（`RegFileComponent`）

> **决策权威**：ADR-094（🚧 Proposed **v0.2**）· **用户裁决 D-22**（首批成层单元 = `RegFileComponent`）
> **前置**：姊妹 change `chmem-hierarchical-assembly` 的 **①-D 完成**（★ 集成前置；★ 开发可并行）

---

## 1. 交付边界

| 维度 | 本 change | 姊妹 change（assembly） |
|---|---|---|
| 交付物 | **真实单元**（`RegFileComponent` 成层）+ manifest 键集 | 框架能力（装配 / 成层 / fail-closed） |
| 判据主体 | `ch_device<...>` 复用 / 端口形态 / 键集一致 | `PipeBuilder` / 层 2 基类 / 两道闸 |
| 依赖方向 | **依赖 ①-D** | 无（地基） |
| 可单独 archive | **否**（须先 ①-D 完成） | 是（按批） |

---

## 2. ★ D-22 的边界切分（★ 取代 D-17 的「首批 / 第二批」切法）

| 内容 | 归属 | 理由 |
|---|---|---|
| `RegFileComponent` **成层**（继承层 2 基类 + 标量端口 + 端口名前缀 + 经 `root.create_child` 创建 + `pb.ctx()` build） | ★ **本 change（首批）** | D-22 裁决 |
| manifest 键集声明（逻辑契约名 → 节点） | ★ **本 change** | 落地单元才有真实样本可声明 |
| 层次化产物可交付（lint / 逐拍等价 / `ForceInline` 逐字节一致） | ★ **本 change** | 需要真实单元 |
| **预载核观测面显式化**（内部 `ch_reg` 观测 ⇒ 顶层 `ch_out`） | ★ **不在本 change** | ① 那是 D-17 第二批的剩余部分；② 也是 `identity` provider 的**退役条件**（D-3.9「载体统一」+ 使用计数 = 0）⇒ 本批之后预载核侧继续走 `identity` provider |
| **ALU 单元** | ★ **不在本 change** | D-22 把首批给了 `RegFileComponent` ⇒ ALU 推迟到后续批次（「纯组合、端口干净」的验证价值保留位置） |

★ **V 侧面标记**：本 change 覆盖 V-37 / V-38 / V-40 / V-42 的 **落地侧**；assembly 侧标 **框架侧**。

---

## 3. 关键设计点

### 3.1 `RegFileComponent` 现状（实测）

`ip/cpu/core/regfile_chmem.h:93`：`class RegFileComponent : public ch::Component`
- `io` struct 全部为标量 `ch_in`/`ch_out`（`regfile_rs1_addr` 等**已带单元前缀** ⇒ r.12 **当前已合规**）★ `ch_reg` 只作**内部**存储
- ★ **唯一不合规点**：直接继承上游 `ch::Component`，**未继承 ChipForge 层 2 基类** ⇒ r.16 未满足
- ★ `describe()` 形态需按 r.9 核对（MUST NOT 读 `Payload`；`is_layer()` / `granularity()` **MUST NOT** 改变可观测行为）

### 3.2 改造清单（★ 相对 assembly 的 ①-A′）

| # | 改动 | 条款 | 批 |
|---|---|---|---|
| 1 | 改继承 ChipForge 层 2 基类 | r.16 | 本 change |
| 2 | 端口名带单元局部前缀（★ **当前已合规** ⇒ 只做回归确认） | r.12 | 本 change |
| 3 | 经 `root.create_child<RegFileComponent>(name)` 创建 + `pb.ctx()` build（★ **①-A′ 已完成 ⇒ 本批只做契约复验**） | r.2 · r.3 | 本 change（复验） |
| 4 | 声明 manifest 键集（逻辑契约名 → 节点） | D-3.9 · D-3.10 | 本 change |

### 3.3 独立复用与命名（V-30 / V-32）

- `ch_device<RegFileComponent>` **脱离 CPU 上下文**可 elaborate 并发射 Verilog
- `ch_device<Wrapper>`（Wrapper 内 `ch_layer`）⇒ **父路径命名**正确（端口 / 节点路径带父层前缀，不撞名）
- 每实例端口名**逐字不同**；单元每个 `ch_in` 均有 driver（★ r.5 的外部驱动输入集**非空且被满足**）

### 3.4 等价性验证（V-28 / V-29）—— ★ 机制锚不可自建

- **V-28**：层次化 vs 扁平**逐拍等价** ⇒ MUST 用上游 `ch::hier_eq`（★ **MUST NOT** 自建等价比对器；若 helper 的闭合公共面未覆盖某场景 ⇒ **回上游扩展**，★ 不在本仓自建）
- ★ **前置断言 module 数 ≥ 2**（否则全 inline 下两产物相同 = **恒绿**假绿通道）
- **V-29**：`GranularityScope(ForceInline)` 下产物与扁平**逐字节一致**（同一 ctx、同一 elaborate，★ **不重新 build**）

---

## 4. 已知缺口

| # | 缺口 | 影响 | owner |
|---|---|---|---|
| **G3′** | ★ 承接 assembly 的 **G3**（生产代码零 `declare_out` 调用 ⇒ ①-D owner 切换后测试批量变红） | 本 change 的「既有回归零退化」判据依赖它先被处置 | architect（0.3） |
| **G6** | 「既有回归零退化」的**基线数字**待定（★ TLM binary 的 case 数在 open-core 的 tasks 里已从 507 修到 515，本仓当前实际值需重测） | AC-13 的判据需可执行 | builder（实施时实测） |

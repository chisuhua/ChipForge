# design —— 观测契约名的歧义 fail-closed（ADR-091 V-25 落地）

> **决策授权**：ADR-091 v1.15 **V-25**（★ ctx 内观测契约名 MUST 唯一 ⇒ 同名 > 1 ⇒ MUST `ERROR(2)`）
> · ADR-089 **S-10**（三态退出码 + `error_kind` 单一真源）· ADR-094 §D-6 阶段 1（C-1 三态化）
> **边界**（★ 2026-11-08 与 L4 划定）：★ **本 change 交付三态化的实现**；
> ★ L4 的 `chmem-hierarchical-assembly` ①-C 只做「同名普查 + 消费」⇒ ★ **两者 MUST NOT 重复实现**

---

## 1. 交付边界

| 项 | 内容 |
|---|---|
| ★ **唯一改动文件** | `tools/parity_runner/parity_sim_runner.cpp`（★ **1 个文件**） |
| 改动性质 | 解释器侧**观测名解析**从「静默取首个」⇒ 「三态 + 歧义即 ERROR(2)」 |
| 协议变更 | ★ **零**（★ 观测契约名一字不改 · 退出码语义不变 · ★ **JSON 报告格式零变更** ⇒ 歧义走既有 stderr 诊断通道） |
| ★ **实际改动文件** | ★ **2 个**（★ **2026-11-08 实测修正** —— ★ 原写「唯一改动文件 = runner」是**错的**）：★ runner（★ 三态化 + 歧义诊断）· ★ `parity_gate.py`（★ **仅** `runner_error` 的分类细化 + 删旧报告 ⇒ ★ **不新增任何报告字段**） |
| 依赖 | ★ `backend-parity-observability`（**已归档** ⇒ 依赖已解除） |

★ **零协议变更是本 change 的关键性质**：★ `parity_gate.py` 现有约定已把 runner 的 exit 2 归为
**ERROR** ⇒ ★ 本 change **不碰** gate 的判定语义 ⇒ ⇒ **不可能**把「歧义」降级成分歧（FAIL）或静默跳过。

★ ★ **2026-11-08 实测修正（★ 原设计有两处错）**：
① ★ 「gate 零改动」**不成立** —— ★ 契约 S-10 要求 `error_kind=observation_ambiguous:<name>`，
   而 gate 现有逻辑把「runner exit 2 且无 JSON」一律归 `runner_error` ⇒ ★ **必须**加分类细化
   ⇒ ★ 但**只改分类**，★ **不新增 JSON 报告字段** ⇒ ★ 「零协议变更」仍然成立
② ★ gate 侧另有一个**假绿通道**（★ 变异对照实测抓到）：★ runner exit 2 时**不写** JSON
   ⇒ ★ work-dir 里**上次成功运行**的报告会被读到 ⇒ ★ 观测侧错、门禁却绿
   ⇒ ★ 修法 = 跑之前删旧报告（★ 见 `evidence/v25-ambiguity-evidence.txt` §4）

---

## 2. 与 V-14 的分工（★ 互补，缺一不可）

| Verification | 观测面 | 形态 | 本 change |
|---|---|---|---|
| ★ **V-14**（ADR-091 v1.11 · G-1o） | ★ **Verilog 侧**：生成网表的顶层端口名 MUST 等于契约名（MUST NOT 出现 `_N` 后缀） | 编译期 | ✗ 不涉及 |
| ★★ **V-25**（★ 本 change） | ★ **解释器侧**：观测契约名在 ctx 内 MUST 唯一（同名 ⇒ 不许取首个） | ★ **运行期门禁** | ★ **本 change** |

★ ★ **V-14 罩不住解释器侧**：★ Simulator-only 路径**根本不跑 codegen** ⇒ ⇒ 两者**必须并存**。

---

## 3. 实现形态

### 3.1 `resolve_node_id` 三态化

现状（`tools/parity_runner/parity_sim_runner.cpp:385` 附近）：线性扫描 `ctx.get_nodes()`，
**取首个** `node->name() == name` 的节点 ⇒ ★ 同名时**静默取首个**（谁先创建谁赢）⇒
★ 观测可能读到**内部 `ch_reg`** 而非端口 ⇒ **假绿**。

改法：

| 项 | 现状 | 目标 |
|---|---|---|
| 返回 | `std::optional<uint32_t>` | ★ **三态**：成功 / **缺失** / **歧义**（★ 缺失与歧义**必须可区分** ⇒ 「没这个观测」与「这个观测有多个候选」是不同故障） |
| 同名 > 1 | 静默取首个 | ★ **收集全部候选** ⇒ 返回歧义态 + **歧义契约名** + **冲突节点 id 列表** |
| 错误信息 | 「missing observation 'pchash': node pc not found」 | ★ 歧义态的消息 MUST 含**歧义契约名** + 候选节点 id 列表（★ 可定位） |
| 退出码 | — | ★ **2**（★ **非 0**（= 无观测）/ **非 1**（= 后端分歧）） |

### 3.2 调用点逐一分派

★ 5 类观测**各自独立**分派（★ 不许「一个总的解析失败」把 5 类混成一条）：

| 观测 | 契约名 | 歧义时的 `error_kind` |
|---|---|---|
| ① `tohost` | `tohost_probe_data_proxy` | `observation_ambiguous:tohost_probe_data_proxy` |
| ② 最终寄存器堆 | `reg_1..reg_31`（31 项） | `observation_ambiguous:reg_<i>` |
| ③ 内存签名 | `mem_sig` | `observation_ambiguous:mem_sig` |
| ③ 内存签名更新数 | `mem_sig_updates` | `observation_ambiguous:mem_sig_updates` |
| ⑤ 逐拍 PC 流 | `pc` | `observation_ambiguous:pc` |

★ **`error_kind` 不得另立字面量**：契约 **S-10 表**（`docs/architecture/parity-gate-contract.md:281`）
已登记 `observation_ambiguous:<key>` ⇒ ★ 只用该形式（★ key = **歧义契约名**）。

---

## 4. 变异对照的设计（★ A-1 承重判据）

★ 手法：把某个**内部 `ch_reg`** 改名**撞上契约名**（★ 如 `open_reg_1` → `reg_1`）
⇒ ★ ctx 内出现两个 `reg_1` ⇒ ★ runner MUST exit **2**（★ 非 0 / 非 1）且消息含歧义契约名。

★ ★ **纪律**：★ 临时改动 MUST 用 `cp <gold>` 覆盖 ⇒ ★ **MUST NOT** `git checkout`（★ 会丢掉未提交的工作）。
★ **回归验证**：★ 变异解除后 corpus MUST 逐位回到基线。

---

## 5. 前置体检（A-2，★ 先查后改）

★ **先跑既有 corpus** ⇒ MUST 确认**当前零歧义**。
★ ⇒ 若已存在歧义 ⇒ 说明**已有假绿通道在跑** ⇒ ★ **优先修歧义**，而非先加检测。

---

## 6. 已知缺口

| # | 缺口 | 影响 | 处置 |
|---|---|---|---|
| **H1** | ★ 「歧义」与「缺失」在**报告层**是否要区分字段（★ 现在都归 `observation_missing` / `observation_ambiguous` 两个 `error_kind`） | ★ 报告可读性 | ★ `error_kind` 已能区分 ⇒ **不额外加字段**（★ 加字段 = 加协议 = 超出本 change） |
| **H2** | ★ `reg_1..reg_31` 的**歧义判定成本**（31 次 × ctx 节点数线性扫描） | 性能 | ★ 现状已是一次全扫描 ⇒ 三态化**不增加**扫描次数 ⇒ **无回归** |

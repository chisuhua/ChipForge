---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v1.0.0
status: proposed
created_from: docs/architecture/adr/ADR-091-chmem-open-core-port-architecture.md
adr:
  - ADR-091
  - ADR-089
depends_on:
  - backend-parity-observability
plan: .rddf/plans/backend-parity-open-core.md
spec_changes: none  # ★ 权威声明在 .openspec.yaml（skip_specs: true）
---

# parity-observation-ambiguity-fail-closed

> ★ **ADR-091 v1.15 的 Verification V-25 落地**：★ 观测契约名（★ `tohost_probe_data_proxy` / `pc` / `reg_<i>` / `mem_sig` / `mem_sig_updates`）
> 在 ctx 内 MUST **唯一** ⇒ ★ 同名 > 1 ⇒ MUST **ERROR(2) 退出**（★ **MUST NOT** 「取首个匹配后继续」）。
>
> ★ **架构授权**：★ **ADR-091 v1.15**（★ G-1o 补 V-25 · ★ §D3a.7.1 的防误读注）· ★ **ADR-089**（★ S-10 三态退出码）
> ★ **执行路径**：`.rddf/plans/backend-parity-open-core.md`（★ §K34 待 builder 落）
> ★ **无 spec 变更**（★ 只改 `tools/` ⇒ ★ 权威声明见 **`.openspec.yaml`（`skip_specs: true`）** ⇒ ★ 归档时 `--skip-specs` 合规）

## Why

### 1. ★★ ADR-091 **已有文字**（G-1o 行），★ 但**代码零防线**

★ ADR-091 v1.14 的 **G-1o** 行**已明写**：

> ★ 同时 `ctx` 里**两个节点同名** ⇒ ★ `resolve_node_id(ctx, "pc")` 取**首个**匹配 ⇒ ★ 观测可能读到 `ch_reg` 而非端口

⇒ ★ **但 `tools/parity_runner/parity_sim_runner.cpp` 至今仍是「取首个后继续」**：

```cpp
// ★ 实测：tools/parity_runner/parity_sim_runner.cpp:249-255
std::optional<std::uint32_t> resolve_node_id(ch::core::context& ctx,
                                             const std::string& name) {
  for (const auto& node : ctx.get_nodes()) {
    if (node && node->name() == name) return node->id();   // ★ 静默取首个，无告警
  }
  return std::nullopt;
}
```

⇒ ★ **ADR 有文字 · 代码无防线** ⇒ ★ **G-1o 的后果由「理论」退化为「实际可触发」**。

### 2. ★★ 后果：**静默采错节点 ⇒ H-3 等价性自检读错数据 ⇒ 假绿**

| 环节 | 机制 |
|---|---|
| ★ **同名产生** | ★ **唯一化只发生在 Verilog codegen**（★ `CppHDL/src/codegen_verilog.cpp:64-76` 建 `node_names_` map · ★ 同名者加 `_N` 后缀）⇒ ★ **解释器侧（Simulator）不改名** ⇒ ★ ctx 里**两个同名节点并存** |
| ★ **解析** | ★ `resolve_node_id` 线性扫描**取首个** ⇒ ★ 谁先创建谁赢（★ `ctx.get_nodes()` = 创建序）⇒ ★ **不报错、不告警** |
| ★ **后果** | ★ 观测契约名（★ `pc` / `reg_<i>` / `mem_sig` / …）**采到内部节点** ⇒ ★ H-3 等价性自检**读错数据** ⇒ ★ **静默假绿**（★ 比「读不到」更坏：★ 读到的是**貌似合理的错值**） |

⇒ ★ ★ **典型症状**：★ 内部 `ch_reg` 改名撞上契约名 ⇒ ★ Verilator 侧顶层端口被改名（★ `pc` → `pc_1`）⇒ ★ Simulator 侧读到内部 reg ⇒ ★ **两侧不等** ⇒ ★ 门禁**误报 FAIL**；★ 或 ★ 两侧**都**读到内部节点 ⇒ ★ **两侧相等但都不是契约量** ⇒ ★ **真分歧被掩盖**（★ 更坏）。

### 3. ★ 与既有 **V-14** 的分工（★ 互补 · 缺一不可）

| Verification | 观测面 | 形态 |
|---|---|---|
| ★ **V-14**（ADR-091 v1.11 · G-1o） | ★ **Verilog 侧**：★ 生成网表的顶层端口名 MUST 等于契约名（★ **MUST NOT** 出现 `_N` 后缀） | ★ **变异测试**（★ 改内部名 ⇒ 生成网表端口名断言 RED） |
| ★★ **V-25**（★ 本 change） | ★ **解释器侧**：★ 观测契约名在 ctx 内 MUST 唯一（★ 同名 ⇒ 不许取首个） | ★ **运行期门禁**（★ runner 解析时判） |

⇒ ★ ★ **V-14 罩不住解释器侧**：★ Simulator-only 路径**根本不跑 codegen** ⇒ ★ ⇒ ★ 两者**必须并存**。

### 4. ★ 为什么**现在**做（★ 时机）

★ **A4-2（装配骨架 + decode/alu/csr 新 plugin）落地时**，★ 内部 `ch_reg` 数量将从 32 涨到数十个 ⇒ ★ **撞名概率显著上升** ⇒ ★ ★ **门禁必须在 A4-2 之前就有防线** ⇒ ★ 否则 A4-2 的新节点一旦撞名 ⇒ ★ **A6 的 H-3 自检读错数据** ⇒ ★ **阶段 A 的门禁结论不可信**。

## What Changes

### 1. ★ `tools/parity_runner/parity_sim_runner.cpp`（★ 唯一改动文件）

| 项 | 改动 |
|---|---|
| ★ **解析器** | ★ `resolve_node_id` ⇒ 改为返回**三态**（★ `kOk` / `kMissing` / ★★ **`kAmbiguous`**）★ 同名节点 **> 1** ⇒ ★ 返回 `kAmbiguous` + **冲突节点 id 列表** |
| ★ **调用点** | ★ 5 类观测逐一分派：★ `tohost_probe_data_proxy` · ★ `pc` · ★ `mem_sig` · ★ `mem_sig_updates` · ★ `reg_1..reg_31`（★ 31 个） |
| ★ **错误信息** | ★ MUST 含 **歧义契约名** + **冲突节点 id 列表**（★ 可定位）⇒ ★ exit **2** |
| ★ **退出码** | ★ **2**（★ contract **S-10** 的 ERROR 语义）⇒ ★ **不是** 1（★ 1 = 后端分歧） |

### 2. ★★ 不动的东西（★ 零协议变更 · 零回归的关键）

| 不动 | 理由 |
|---|---|
| ★ `tools/parity/parity_gate.py` | ★ ★ **零改动**：★ 其既有约定已把 runner 的 exit 2 归为 **ERROR**（★ `parity_gate.py:676-677`）⇒ ★ **不降级、不跳过**（★ 判据见下表） |
| ★ 观测契约名 | ★ `tohost_probe_data_proxy` / `pc` / `reg_<i>` / `mem_sig` / `mem_sig_updates` **一字不改** |
| ★ Verilator 侧 | ★ 本 change **只**治解释器侧的解析歧义 ⇒ ★ Verilator 侧零改动 |
| ★ `ip/cpu/**` | ★ 零改动（★ 变异对照的临时改动 MUST 用 `cp <gold>` 覆盖 · ★ **MUST NOT** `git checkout`） |
| ★ `openspec/specs/**` | ★ 无 spec 变更（★ ⇒ 归档时 `--skip-specs` 合规） |

### 3. ★ 与 `backend-parity-observability` 的关系

★ ★ `tools/parity_runner/parity_sim_runner.cpp` 的**所有者**是 `backend-parity-observability`（★ ★ `openspec list` 实测 = **✓ Complete**，★ 2d ago）⇒ ★ **本 change 独立成 change**（★ 而非并入）⇒ ★ 理由：
★ ① ★ planner 权限内**只有 `proposal.md` 可写**（★ `tasks.md` / `design.md` 属 builder）★ ② ★ 判据独立可验收（★ V-25 变异对照）★ ③ ★ **不追加**到已归档 change（★ openspec 惯例：已归档 change 不再改动）★ ④ ★ 归属清晰（★ 改的是它的文件，★ 但缺陷由 ADR-091 v1.15 的 **V-25** 引入 ⇒ ★ 归 V-25 的落地更准确）。
⇒ ★ ★ **归档顺序**：★ `backend-parity-observability` **已归档** ⇒ ★ 本 change **无顺序约束**；★ 但 frontmatter 仍登记 `depends_on`（★ 表达「改的是它的文件」）。

## Acceptance Criteria


- [ ] ★ **A-1** ★ ★ **变异对照（V-25 承重判据）**：★ 把某个内部 `ch_reg` 改名撞上契约名（★ 如 `open_reg_1` → `reg_1`）⇒ ★ runner MUST **exit 2**（★ **非** 0 / 非 1）⇒ ★ 且错误信息 MUST 含**歧义契约名**
- [ ] ★ **A-2** ★ **前置体检**：★ 先跑既有 corpus ⇒ ★ MUST 确认**当前零歧义**（★ ⇒ 若已存在歧义 ⇒ 说明**已有假绿通道在跑** ⇒ 优先修）
- [ ] ★ **A-3** ★ **既有门禁零退化**：★ `python3 tools/parity/parity_gate.py --self-test` ⇒ PASS
- [ ] ★ **A-4** ★ **既有 corpus 零退化**：★ 两侧 5 项观测量 + 确定性判据逐位相等（★ 与 `backend-parity-observability` 的既有基线一致）
- [ ] ★ **A-5** ★ ★ **协议一致性**：★ runner 的 exit 2 ⇒ `parity_gate.py` MUST 判为 **ERROR**（★ `error_kind=observation_ambiguous:<name>`）⇒ ★ **MUST NOT** 降级为 FAIL 或跳过
- [ ] ★ **A-6** ★ **正反两面都有**：★ 观测名**缺失** ⇒ 仍 MUST 是既有的「missing observation」⇒ exit 2（★ **MUST NOT** 被新路径吞掉）


★ ★★ **判据口径**：★ 本 change 是**门禁自身的加固**⇒ ★ 它的判据**本身就是**「门禁在坏输入上必须红」⇒ ★ A-1 是**唯一承重判据**（★ A-2~A-6 均为回归/协议断言）。

## Impact

| 维度 | 影响 |
|---|---|
| ★ **改动面** | ★ **1 个文件**（★ `tools/parity_runner/parity_sim_runner.cpp`）★ + 零协议变更 |
| ★ **爆炸半径** | ★ 极小（★ 只影响 runner 解析观测名的一步）⇒ ★ **不触碰** datapath / 时序 / Verilator 路径 |
| ★ **对既有 corpus** | ★ 若当前零歧义 ⇒ ★ **行为逐位不变**（★ 唯一可见差异 = 无歧义时不打印任何额外信息） |
| ★ **对 A4-2 / A6** | ★ ★ **前置**：★ A4-2 会新增数十个内部 `ch_reg` ⇒ ★ 本 change 先行 ⇒ ★ A6 的 H-3 自检**可信** |
| ★ **潜在行为变更** | ★ ★ **唯一一处**：★ 若 ctx 内**真的**存在同名节点 ⇒ ★ 旧行为「取首个继续」⇒ ★ 新行为「exit 2」⇒ ★ ★ **这是本次要修的缺陷，不是回归**（★ 但 MUST 在 A-2 体检中先确认当前不触发） |

## ★ 相关

| 项 | 位置 |
|---|---|
| ★ **ADR-091 v1.15** | ★ `docs/architecture/adr/ADR-091-chmem-open-core-port-architecture.md`（★ G-1o 行 · ★ Verification **V-25** · ★ §D3a.7.1 的防误读注） |
| ★ **ADR-089** | ★ `docs/architecture/adr/ADR-089-backend-parity-differential-gate.md`（★ S-10 三态退出码 · ★ V-1~V-6） |
| ★ **契约** | ★ `docs/architecture/parity-gate-contract.md`（★ S-2 §2.1 · ★ S-10） |
| ★ **实现锚** | ★ `tools/parity_runner/parity_sim_runner.cpp:249-255`（★ `resolve_node_id`） |
| ★ **门禁消费方** | ★ `tools/parity/parity_gate.py:6`（★ 三态约定）· `:16`（★ fail-closed）· `:676-677`（★ exit 2 ⇒ ERROR）· `:722`（★ 退出码映射） |
| ★ **上游机制** | ★ `CppHDL/src/codegen_verilog.cpp:64-76`（★ 名称唯一化**只在这里**）· ★ `CppHDL/src/core/context.cpp:200-237`（★ `create_input/create_output` **不**唯一化） |
| ★ **执行计划** | ★ `.rddf/plans/backend-parity-open-core.md`（★ §K34 待 builder 落 ★ 含 v1.15 的证据与 A-2 体检记录） |

★ ★★ **待补产物**（★ planner 权限边界）：★ ★ **`design.md` / `tasks.md` 属 builder**（★ planner 禁写）⇒ ★ 本 change 的**执行拆解待 builder 落**（★ rdd-builder P1）⇒ ★ 本提案只定**问题 · 范围 · 判据**。

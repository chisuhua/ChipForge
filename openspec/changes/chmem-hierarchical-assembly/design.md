# design —— CH_MEM 开放核的层次化装配框架地基（阶段 ①-A ~ ①-D）

> **决策权威**：`docs/architecture/adr/ADR-094-chmem-hierarchical-assembly-contract.md`（🚧 Proposed **v0.2**）
> **契约细则**：`docs/architecture/parity-gate-contract.md` **S-17 / S-17.1**（登记簿 · 逻辑契约名清单）
> **批次与治理**（5 批 + 出口判据 + 实施面清单）：`.rddf/reviews/ADR-094-decision-ledger.md` **§9.2 / §9.3**
> ★ 本文件只写**怎么做**（实施设计）⇒ 决策与判据一律不重述。

---

## 1. 批次总览

| 批 | 内容 | 出口判据 | 回滚粒度 | 不可拆说明 |
|---|---|---|---|---|
| **①-A** | 框架地基：`root_component()` / `create_child` 转发 / root-scope（RAII）· r.15 槽位单例 · r.16 层 2 基类 · r.10 `build()` 幂等 | **V-41a** · V-42 · V-43 · V-34 | 单 commit 可回 | — |
| **①-A′** | `regfile_bridge_chmem.h` 的裸 `make_unique` ⇒ `root.create_child` | 无独立 V | 单 commit 可回 | ★ **①-D 的硬前置**（不改 ⇒ V-40 恒 RED） |
| **①-B** | 观测登记：S-17.1 清单 · 登记簿 · manifest · provider 安装 · 闸①② · 报告 provider 标注 | V-37 · V-38 · **V-41b** | 单 commit 可回 | — |
| **①-C** | 阶段 1：`resolve_node_id` 三态化 + 前置同名普查 + **V-35/V-36 基线捕获** | V-39 · V-35/V-36（捕获） | 普查失败 ⇒ 不切 | ★ 基线**捕获**在此批出口（ADR-094:157） |
| **①-D** | ★ **阶段 2（不可拆）**：owner 切换 + 登记上线 + manifest 上线 + r.14 **生效** + 三条同批改动 | **V-40** · V-35/V-36（比对 + 退役） | ★ **整批 git revert** | ★ owner 切换与 r.14 启用**必须同批**（r.14「自阶段 2 生效」） |

★ **①-D 的回滚纪律**：验收失败 ⇒ **整批 revert**（`git revert` 该批的 commit 序列）⇒
★ **MUST NOT** 只回退其中一项 ⇒ 会留下「owner 已切 / 登记未上 / manifest 未上」的**混合态**。

---

## 2. 实施面清单（★ 订正台账 §9.3 的「10 处」）

> ★ 台账 §9.3 只写了「**10 处代码注释订正**」一句，**清单未落盘**。
> ★ 下表 = 2026-11-08 按契约条款反查的**实测清单**：**13 处实质位置 + 2 处补写缺口**（分布于 8 个文件）。
> ★ 「代码里零落点」的条目单列 §2.2 —— 它们**不是订正**，是**净新增**。

### 2.1 需订正的位置

| # | 位置 | 现状 | 条款 | 批 |
|---|---|---|---|---|
| 1 | `ip/cpu/core/regfile_bridge_chmem.h:124` | 裸 `make_unique<RegFileComponent>(nullptr,"regfile")` | r.2 | ①-A′ |
| 2 | `ip/cpu/core/regfile_bridge_chmem.h:37-40` | 注释：装配所有权在 plugin 的 `setup()` | r.1 / r.2 | ①-A′ |
| 3 | `ip/cpu/core/regfile_chmem.h:44-46` | 注释：桥接 plugin 负责 `build(pb.ctx())` | r.2 / r.3 | ①-A′ |
| 4 | `include/cf/plugin/pipe_builder.h:144` | `built_ = true` 在 `negotiate` **之前**置 ⇒ 失败后 `elaborate()` 不重试 ⇒ fail-open | **r.10** | ①-A |
| 5 | `include/cf/plugin/pipe_builder.h:141-143` | 注释称「幂等 · 重复调用安全」 | r.10 | ①-A |
| 6 | `include/cf/plugin/pipe_builder.h:615-622` | `built_` 注释只讲 G-1j | r.10 | ①-A |
| 7 | `include/cf/plugin/pipe_builder.h:423-427` | 注释「自动 build（幂等）」 | r.10 / r.14 | ①-A |
| 8 | `include/cf/plugin/pipe_builder.h:559-571` | `ctx()` 注释以「各 plugin 各自装配 Component」为前提；★ 引用行号已漂移 | r.1 | ①-A |
| 9 | `include/cf/plugin/pipe_builder.h:506-556` | `declare_in/out` 注释**未提** r.6「创建点 MUST 处 root-scope」 | r.6 | ①-A |
| 10 | `tools/parity_runner/parity_sim_runner.cpp:17` + `:20` + `:247-255` | 按 `node->name()` 精确名一次扫描解析 | D-3.2 / D-3.3 | ①-D |
| 11 | `tools/parity_runner/parity_sim_runner.cpp:404-406` | 报错文案把**逻辑名** `pc` 当节点名 | D-3.3 / T15 | ①-D |
| 12 | `tools/verilator_runner/sim_main.cpp:90-93` + `:96-126` | 31 个 `top__DOT___reg_N` **硬编码** + 注释 | D-3.5 / D-3.6 | ①-D |
| 13 | `tools/verilator_runner/sim_main.cpp:150 / :154 / :163 / :173-183` | `top__DOT__dmem` / `_pc` / `flush_count` / `bp_*` | D-3.6 | ①-D |
| 14 | `tools/verilator_runner/cpu_verilator_sim.cpp:320` | `--top-module top` 硬编码 | D-3.5 | ①-D |
| 15 | `ip/cpu/core/ibus_port_chmem.h:97-101` | 注释前提「顶层端口名 = 契约裸名」（`open_pc` / `open_pc_1`） | D-3.4 / D-3.5 | ①-D |
| 16 | `ip/cpu/core/regfile_chmem.h:91-93` | 类直接继承 `ch::Component`（未继承层 2 基类） | r.16 | ①-A |

★ **「runner 删 fallback」无代码可删**：`grep -rn open_pc tools/` = **0 命中**；`parity_sim_runner.cpp:385` 直接查 `"pc"`。
⇒ ★ 该项改写为「**订正已作废的表述**」（`backend-parity-open-core/tasks.md:135` 仍写「加 1 处 fallback」）⇒ **归属 planner**，非代码改动。

### 2.2 代码里**零落点**的契约条目（= 净新增）

| 条款 | 内容 | 新增位置建议 |
|---|---|---|
| **r.1** | `root_component()` + root 持有 + RAII root-scope | `include/cf/plugin/pipe_builder.h` + ★ 新文件 `include/cf/plugin/root_component.h` |
| **r.5** | 「外部驱动输入集」登记 | 层 2 基类（§3.3） |
| **r.8** | 跨单元连接落 root scope | ★ 当前**无单元间连接** ⇒ 机制先落（root-scope API），用法待单元出现 |
| **r.13 / r.15** | 闸①期望键集注入 + provider 槽位 | `include/cf/plugin/contract_registry.h`（新） |
| **r.14** | 归属守卫 | `pipe_builder.h` 的 `build()` / `elaborate()` 收尾 |
| **r.16** | 层 2 基类（编译期标量端口校验） | ★ 新文件 `include/cf/plugin/layer2_unit.h` |
| **D-3.2 / D-3.7 / D-3.10** | 登记簿 / manifest / 两道闸 | `include/cf/plugin/contract_registry.h` + `parity_gate.py` 的 `judge()` |

★ **ChipForge 侧不存在 `ch::Component` 包装层**（`grep -r Component include/cf` 仅 3 条注释命中）⇒ root 包装 / `create_child` 转发 / 层 2 基类**全部是净新增**。

---

## 3. 框架改动设计

### 3.1 ★ 动手前必须先定的 3 个物理事实

| # | 问题 | ★ 建议 | 依据 |
|---|---|---|---|
| ① | root 的持有类型 | **`std::shared_ptr<ch::Component>`** | 上游 `create_child` 内部 `make_shared` + 依赖 `weak_from_this()`；`add_child` 收 `unique_ptr` 并转 shared ⇒ root 必须是 shared_ptr 才能 `create_child` |
| ② | root 构造是否复用 `ConstructionFrame` | **必须复用** | 上游 `ch_device` 先例 `CppHDL/include/device.h:24-27`（ADR-094 引用的正是这行）⇒ 否则 root 自己的 `ch_reg`/端口归错层 |
| ③ | `top_module_name` 与 root 实例名 | ★ **两个不同常量，MUST NOT 混** | 上游 `kFlatModuleName = "top"`（`codegen_verilog.h:41`）/ `HierarchyEmitOptions::top_module_name = "top"`（`hierarchy_verilog.h:33`）是**Verilog 模块名**；D-3.5 的 root 实例名 = `cpu` 是**组件名** ⇒ ★ **模块名保持 `top` 不变**（D-4：门禁仍消费扁平 `top.v`）⇒ ★ 后果 = 顶层端口名从 `reg_1` 变 `cpu_reg_1` ⇒ 这就是 ①-D 必须同批改 `sim_main.cpp` 的根因 |

### 3.2 root 生命周期（r.1 / r.7）

```
PipeBuilder
  └─ ctx_           （生命期最外）
      └─ root_      （shared_ptr<ch::Component>，构造时包 ConstructionFrame）
          └─ units  （create_child 建的单元，shared_ptr 挂在 root 上）
```
- 生命期 **ctx > root > unit** ⇒ `PipeBuilder` 析构时 root 先于 ctx 释放。
- ★ `root_component()` 返回 `ch::Component&`（与上游 `toVerilogHierarchical(ctx, Component& root, ...)` 的**引用**形参对齐 ⇒ ★ **可与 emitter 的 root 是同一对象**，满足 r.1 末句）。
- root 名冻结为契约常量 `cpu` ⇒ 建议落 `constexpr` 在 `root_component.h`。

### 3.3 root-scope（RAII）

- 上游先例：`ch::Component::set_current`（public static）+ `scope_depth()`（诊断）+ `ConstructionFrame`（RAII 截断）。
- ★ ChipForge 侧自建 `RootScopeGuard`（RAII）：进入时 `set_current(root)`，**退出时恢复先前的 `current_`**（★ r.1 明确要求 ⇒ 嵌套 / 多 `PipeBuilder` 场景安全）。
- ★ **①-A 阶段 root-scope 不全局化**（owner 仍 `nullptr`）⇒ 该 guard 在 ①-A **只提供 API，不在构建期自动施加**（AC-7 的负向断言）。

### 3.4 层 2 基类（r.11 / r.16）

- 新文件 `include/cf/plugin/layer2_unit.h`：模板基类，MUST 在**编译期**校验 `io_type` 每个成员是标量 `ch_in`/`ch_out`（`ch_uint<N>` / `ch_bool`）。
- ★ 为何落在 ChipForge 侧：上游 `port<T,Dir>` **无** bundle 类型的编译期拒绝（`CppHDL/include/core/io.h:211/256` 只做类型转发）⇒ 不能改上游 ⇒ 校验 MUST 落本仓基类。
- ★ **正向 + 负向双向**：绕过基类直接继承 `ch::Component` 的单元 ⇒ 门禁 grep MUST 报出（否则编译期纪律形同虚设）。

### 3.5 `build()` 幂等语义（r.10）

| 情形 | 行为 | 现状 |
|---|---|---|
| 同一状态重复调用 | **成功早退**（不重跑 `negotiate/setup/build`、不产生重复登记） | ★ 现状 `built_` 早置 ⇒ 需移到**成功收尾** |
| 状态已变（插件集指纹不同） | **fail-closed**（`PluginError::BuildStateConflict`） | ★ 现状**无指纹** ⇒ 需新增 |
| `built_` 置位时机 | **成功收尾时** | ★ 现状在 `negotiate` **之前**（`pipe_builder.h:144`）⇒ fail-open |
| 重名 attach | 拒绝 | ★ 上游 `add_child` 只检测「同一 child 出现两次」，**不检测同名不同对象** ⇒ 本仓补 |

- ★ **插件集指纹**：MUST 显式定义字段与顺序（★ AC-5 要求）⇒ 建议 = `(plugin 类型名, 配置摘要)` 的**有序列表**哈希 ⇒ 顺序敏感 ⇒ 改顺序即状态变更。
- ★ `PluginError` 需新增码：`BuildStateConflict` / `ContractRegistryMismatch` / `NodeOwnershipEscape` / `ProviderConflict`（现 `plugin_error.h:32-52` 只有 20 个码，无这 4 个）。

---

## 4. 门禁设计（★ 5 条）

> ★ **诚实标注**：台账 D-17④ 只给了 5 条的**强度**（① 硬 · ④ WARN 先看噪声），**① 与 ④ 的判据文字缺失**
> ⇒ ★ 下表 ①④ 的判据是**本 design 的推断** ⇒ ★ **需 architect 确认**（★ 未经确认不得写进 tasks 的验收判据）。

| # | 判据（★ ①④ 为推断） | 强度 | 落点 | 载体 |
|---|---|---|---|---|
| ① | 层 2 单元 MUST 经 `root.create_child`（★ **禁裸 `make_unique`**） | **硬**（FAIL） | `tools/check_plugin_portability.sh` 新 Check（★ 仿 Check 7 的 grep 形态） | 源码扫描 |
| ② | 层 2 基类的**编译期**标量端口校验 | **硬**（编译不过） | `include/cf/plugin/layer2_unit.h` | 编译器 |
| ③ | 闸①：登记簿扫出的节点集合 == 期望键集（★ 期望键集**由调用方注入**） | 源码 WARN + **运行期硬失败** | 框架 `elaborate()` 收尾（★ `parity_gate.py` 的 `judge()` 增 `contract_registry_mismatch`） | 框架 + gate |
| ④ | root 名 / 端口前缀硬编码（★ 如 `top__DOT___reg_N`、`--top-module top`） | **WARN**（先看噪声） | `tools/check_plugin_portability.sh` 新 Check | 源码扫描 |
| ⑤ | 层 2 单元 MUST NOT 引用 `Payload`（G-1q） | WARN | `tools/check_plugin_portability.sh` 新 Check（★ 仿 Check 7） | 源码扫描 |

★ **`error_kind` 不得另立字符串**：`parity-gate-contract.md:274-289` 的 **S-10 表已登记**
`node_ownership_escape`（:289）· `provider_conflict`（:288）· `manifest_missing`（:287）⇒
★ 只需在 `parity_gate.py` 的 `judge()`（`:266-360`）与 `error_kind` 赋值点（`:291/:294/:306/:313/:319/:359`）**增判据**，
★ **MUST NOT** 在 gate 侧另造字面量。

---

## 5. AC ↔ V 机检规则（★ v0.2 新增）

- ★ **同一 V 可被两个 change 各覆盖一次**（**框架侧** / **落地侧** / **回归侧**）⇒ 机检「无重复」MUST 按
  「**同一 V 的同一侧面**」判定 ⇒ ★ **NOT** 按 V 计数。
- 当前覆盖（实测）：assembly 11 V + unit-landing 11 V ⇒ 并集 **18/18**（★ ADR-094 全集）⇒ **0 遗漏 · 0 越界**。
- 需工具化：`tools/check_ac_v_coverage.py` 现只覆盖 L1 两个 change ⇒ **需扩展**到 L4 两个 change + 支持**侧面标记**。

---

## 6. ★ 已知缺口（动手前必须有人拍板）

| # | 缺口 | 影响 | 建议 owner |
|---|---|---|---|
| **G1** | 门禁 ①④ 的**判据文字**在台账中缺失 | 验收判据无依据 | architect |
| **G2** | 台账「10 处」实为 **13+2** 处（§2.1） | tasks 的验收粒度 | architect 确认分组口径 |
| **G3** | ★ **生产代码里零 `declare_out` 调用**（只有测试 20+ 处）⇒ r.6 的「顶层端口 MUST 经 `declare_in/out`」在生产路径**无现存调用点** | ①-D owner 切换后**测试批量变红**（端口名 `open_pc` → `cpu.open_pc`）⇒ ★ ADR 未覆盖测试基线处置 | architect + builder |
| **G4** | 上游 `Component::build()` 也在 `create_ports` **之前**置 `built_`（`CppHDL/src/component.cpp:268`）⇒ 上游侧同样不可重试 | r.10 的**上游对应风险** | architect（是否需上游回执） |
| **G5** | r.14 的「**创建时机维度**」未覆盖（★ oracle 终审点名的**长期残留风险**）⇒ `at_stage` 闭包内新建的节点被归 root ⇒ 合法化为不可见 | 本次不解决 ⇒ 已登记为长期项 | architect（触发条件 = 幽灵节点被实际观测到） |
| **G7** | ★ ★ **与 `parity-observation-ambiguity-fail-closed` 的重叠**（★ 2026-11-08 实施前发现）：★ 该 change 做的正是「`resolve_node_id` 三态化」，★ 且改**同一个文件** `tools/parity_runner/parity_sim_runner.cpp` ⇒ ★ 若两者都做 ⇒ 改同一文件 + 同一判据 ⇒ 必然冲突 | ★ **已划界**（★ 见上表）：★ **三态化的实现**归该 change（1 文件 · 可独立验收 · 依赖已解除）★ 本 change 的 **①-C 只做「同名普查 + 消费」** ⇒ ★ L4 的 AC-13 / tasks 4.1 已改写 + `depends_on` 已加 | ★ **已闭合**（planner 2026-11-08） |

---

## 7. 验证策略

| 批 | 验证手段 |
|---|---|
| ①-A | `--self-test` 变异自检（★ 去掉判据 ⇒ MUST RED）+ 2 TU 链接探针（V-34）+ 编译期负向（bundle 端口 ⇒ 编译失败） |
| ①-A′ | 变异对照：改回裸 `make_unique` ⇒ 组件树缺该单元 ⇒ 层次化发射 FATAL |
| ①-B | 闸①变异：摘掉 ⇒ 静默通过 ⇒ RED；manifest 缺失 ⇒ `ERROR(2)` + `manifest_missing` |
| ①-C | 普查变异：注入一个同名节点 ⇒ 普查 MUST 报出该组 ⇒ RED；基线变异：捕获后注入观测差异 ⇒ hash 链 MUST 不同 |
| ①-D | r.14 变异：把某节点挂到 root 之外 ⇒ MUST RED；★ **失败 ⇒ 整批 revert**（★ 不许部分回退） |
| 全程 | ★ `openspec validate <change> --strict` 0 error + `tools/check_ac_v_coverage.py`（扩展后）0 孤儿/0 重复/0 遗漏 |

# fetch_stall Requirements (v0.11.0 — wave5-bp-btb Sprint 6-7 owner 设计挑战)

> **重要状态变更 (2026-10-16)**:
> - 原 `fetch_stall_protocol_v0.11.0.md` (Draft v0.1) 经 critic 审稿 (REJECT + 5 BLOCKERS) 后**降级为 requirements doc**。
> - 本文档**不是已确认协议**,而是**待解决设计挑战的需求清单** + 关键约束清单,**供 wave5-bp-btb owner Sprint 6-7 (2026-12-01 ~) 启动期作为设计输入**。
> - 任何 v0.11.0 范围内的 fetch stall 实装必须解决本文档列出的 5 个 BLOCKER 假设,然后再开始起草真正可执行的 v0.2 protocol spec。
> - **不要**直接基于本文档起草代码 — 它会引入 critic REJECT 的同样问题。

> **owner**: 待 wave5-bp-btb owner Sprint 6-7 启动期 review 与指派
>
> **关联**: mmu-chmem-pipeline-integration Phase C (`ibus_chmem.h` / `dmem_chmem.h` sv32 消费) 入口前置 — fetch stall framework 锚点
>
> **关联 ADR**: ADR-046 v2.0 (多周期 FSM 豁免) + ADR-048 (canonical ordering) + ADR-082 (negotiate) + ADR-083 (Plugin-style 单一 source of truth)
>
> **关联**: HANDOFF.md §3 + tasks.md line 12 (bp-btb-coordination-3items 提前到 Sprint 2 早期) + features/feat-mmu-chmem-integration.md line 16 (R-CHMEM-1 缓解)

---

## 0. 一句话定位

`fetch_stall` 是 MMU PTW walk 期间冻结 fetch stage 的"硬件信号"代理 — 解决 **Phase C (`ibus_chmem.h` sv32 消费) 启动期所需的 fetch stall framework 锚点**。本文档**不声明协议**,只汇总 design challenge + known constraints,等 wave5-bp-btb owner 在 Sprint 6-7 启动期 review 后自行起草 v0.2 协议 spec (在 critic BLOCKER 解决之前)。

---

## 1. 5 个 BLOCKER (需 owner 在 Sprint 6-7 启动期解决)

### 1.1 [BLOCKER-B1] `declare_substage` 当前**不产生调度顺序** — substage 关系只在 `nodes_` 里有索引,无读取路径

- **现状** (critic 实证):
  - `include/cf/plugin/pipe_builder.h:122-132` `declare_substage()` 只写 `substage_parent_[sub] = parent`,无任何 reader。
  - `pipe_builder.h:215-224` `canonical_stage_order()` 只按 `stages_` 的**首现序**排序;`run()` (182-183) 与 `elaborate()` (414-415) 都用它。
  - 项目文档已确认: `docs/lessons/phase-1.2-l1cacheplugin.md:77`, `docs/methodology/plugin-style-design-methodology-v1.md:273`, `docs/architecture/adr.md:881`, `docs/architecture/adr/ADR-045-plugin-ctrl-link-consumption.md:103`。
- **后果**: fetch substage MMU→BP→IBus 的声明顺序**对 runtime elaboration 调度无任何影响**。`pb.at_stage("fetch.<mmu>", ...)` 与 `pb.at_stage("fetch", NORMAL, ...)` 在 `nodes_` 里是**两个独立节点**,substage 关系不成立。
- **设计挑战**: 如何**真正**保证 MMU substage 先于 IBus fetch NORMAL 执行? 3 个候选路径:
  - (a) 扩展 PipeBuilder substage ordering API (ADR-048 候选扩展),引入 `pb.declare_substage_with_priority()` 或 stage priority enum
  - (b) 改用 plugin 注册顺序约束 (per ADR-048 当前机制:`MMU_REG_ORDER = ++PLUGIN_SEQ` + `check_canonical_ordering()` runtime assert),让 cpu_factory_chmem.h 显式控制注册序
  - (c) 引入 substage 内 `at_stage("fetch", NORMAL)` 闭包顺序约束 — 每个 plugin 内自己排 at_stage 顺序

### 1.2 [BLOCKER-B2] CH_MEM `register_ctrl_link("fetch", ...)` **不会冻结 pc_reg** — CtrlLink 在 elaborate() 路径下不消费

- **现状** (critic 实证):
  - `should_stall_stage()` 仅在 `run()` 185 行被调用,`elaborate()` (414-415) 不调用。
  - `elaborate()` 只在 `stage_payload_map_` 上聚合 CtrlLink (427-444),而 `stage_payload_map_` 仅由 `register_stage_payload_connector()` (344-382) 填充 — 没注册 connector 的 stage 在 elaborate 路径下完全没有 halt 效果。
  - pc_reg 实际由 `"branch"` LATE 驱动 (`ip/cpu/plugins/ibus_chmem.h:71`, `:177`),不是 `"fetch"`。
- **后果**: 即使 owner 按原 v0.1 spec 草拟 fetch_stall_chmem.h,它产出的代码在 CH_MEM 下是**惰性**的 — 不会真正冻结 fetch / pc_reg。
- **已知缺口**: `ip/cpu/plugins/hazard_chmem.h:152-167` 已记录 CtrlLink FP-4 — `halt_when` 只能 OR 累积,不能 toggle。
- **设计挑战**: 如何在 CH_MEM 下表达"stall → pc_reg 保持"? 候选路径:
  - (a) 引入新的 payload key `pl::PTW_ACTIVE_CH` (ch_bool 类型) + 让 fetch 的 at_stage 闭包读这个 key 而不是 CtrlLink halt_when
  - (b) 给 `"branch"` stage 注册 CtrlLink(因为 pc_reg 在 branch LATE 更新)+ 用 ctrl_link 控制 branch 闭包执行
  - (c) 引入新的 `fetch_stall_halt` Payload + ch_bool mux 到 pc_reg 驱动源(本质是把 stall 编码成 next_pc select)

### 1.3 [BLOCKER-B3] Payload Key 类型用 `ch_bool` / `ch_uint<3>` 直写 — 违反 ADR-040 v2.0 双模 + 命名空间不存在

- **现状** (critic 实证):
  - 全仓库 grep `cf::cpu::keys` = **0 命中**。实际约定是 `cf::cpu::core::payload::keys<T, XLEN>` (`ip/cpu/core/payload_common.h:113`) 与 `cf::ip::mmu::payload::mmu_keys<T>` (`ip/mmu/tlm/mmu_keys.h:27`)。
  - ADR-040 v2.0 + `bundles/mem_bundles.h:14,20,45-49` 要求共享字段用 `cf::plugin::uint_t<N>`/POD。`ch_bool` / `ch_uint<N>` 直接出现在 `bundles/*.h` 会导致 TLM 编译失败(ch 头仅在 `#ifdef CF_PLUGIN_USE_CH_MEM` 下 include)。
  - `tools/check_plugin_portability.sh:95-104` Check 2 要求 `*_chmem.h` **必须包含** `ch_uint|ch_reg|ch_bool|ch_mem|ch::core`,Check 7 要求 TLM `<name>.h` 不能含 ch_* — 原 v0.1 spec 样例在两端都会 FAIL。
- **后果**: 任何基于原 v0.1 spec 的实装**直接 FAIL CI portability check**。
- **设计挑战**: 如何定义 fetch_stall payload keys? 候选:
  - (a) 走 `cf::plugin::Payload<T>` 双模 (TLM=POD, CH_MEM=ch_uint<N>),参考 `ip/mmu/tlm/mmu_keys.h` 的 5 个新增 keys
  - (b) 把 keys 放到 `ip/cpu/core/payload_common.h` 而非 `bundles/`,与现有 `KT::PC` / `KT::INSTRUCTION` 等 keys 同样归属
  - (c) 引入 `_chmem.h` 文件承载所有 ch_* 类型 (ADR-083 双文件分离)

### 1.4 [BLOCKER-B4] Bare path 判定公式与已锁定 D4 决议**直接矛盾**

- **现状** (critic 实证):
  - 原 spec §2.4 写 "satp_ppn = 0 OR sv_mode = Bare" → Bare path
  - 但 HANDOFF.md:25 (D4) 明确 "Bare shortcut 公式 = **删除析取项**：删 `MMUPlugin.cpp:109` 的 `|| satp_ppn_ == 0`"
  - 当前 `ip/mmu/tlm/MMUPlugin.cpp:109` 已 `if (sv_mode_ == SvMode::Bare || satp_mode == 0)`,注释 `89-93` 写明「`satp_ppn_ == 0` **不等于** Bare：Sv-mode + PPN=0 是合法 Sv-mode 状态」
  - v0.10.4 回归 (`test_mmu_cache_integration.cpp EndToEndTranslationThroughBridge`) 的根因正是这个析取项
- **后果**: Sv32 + 未写 satp (PPN=0) 的启动瞬态,按原 spec 应"fetch_stall 永远不触发",按 D4 则应判为 Sv-mode → PTW walk → **必须** stall。实现者无法同时满足。
- **设计挑战**: Bare path 判定必须**严格按 `satp_mode` 字段** (TLM=byte, CH_MEM=`ch_uint<8>`),而非 `satp_ppn`。owner 起草 v0.2 时须直接引用 `MMUPlugin.cpp:89-93` 注释。

### 1.5 [BLOCKER-B5] `ptw_active` 是 `Payload<bool>` POD,CH_MEM `CtrlLink::halt_when` 需要 `ch_bool` — 无合法信号源

- **现状** (critic 实证):
  - `ip/mmu/tlm/mmu_keys.h:35` `PTW_ACTIVE` 是 **`Payload<bool>` (POD)**
  - CH_MEM 下 `CtrlLink::halt_when(Condition)` 的 `Condition = ch::core::ch_bool` (`ctrl_link.h:54,67`)
  - 跨类型取用正是 `mmu_exception_handler_chmem.h:15-27` 标注为"未确认"的路径
  - plan (`WT/.rddf/plans/mmu-chmem-pipeline-integration.md`) 把 key 类型统一推给 "Phase B (12-01)"
- **后果**: §1.2 stall 语义 (§2 `drive_stall_signal`) 在 Sprint 2 窗口内**没有可用的信号源**。
- **设计挑战**: 必须先定义 `ch_bool` 类型的 PTW_ACTIVE 信号(Phase B 实装),然后才能 stall。owner 在 Phase B 实装前无法起草 fetch_stall_chmem.h。

---

## 2. 7 个 SHOULD_FIX (owner 起草 v0.2 时一并处理)

| ID | 内容 | 来源 |
|----|------|------|
| S1 | Payload Keys 归属地错误 — 应放 `ip/cpu/core/payload_common.h` 而非 `bundles/`;plan:145 引用 `bundles/mmu_bundles.h` 不存在 | critic S1 |
| S2 | 插件计数 8→9 与本 change Task 2.3 不一致 — CH_MEM 工厂当前 8 个,Phase 2.3 计划 +2 (MMUPlugin_chmem + fetch_stall) 应是 10 个 | critic S2 |
| S3 | "byte-identical" 措辞与实测冲突 — plan:274 实测 `[ibus-sv32] cycle 478 ±46 = baseline 461±46` 是 ±10% 容差,**不是** byte-identical | critic S3 |
| S4 | 审查时序冲突 — owner review 定 12-01,但 `tasks.md:12,29` 已把 bp-btb-coordination 提前到 10-23 ~ 10-30,12-01 晚于其解锁的截止 | critic S4 |
| S5 | 阈值自相矛盾 + 风险 ID 悬空 — `§1.2` 写 ≥5% CoreMark 损失 (R-CHMEM-BTB-2),`§5` 写 ≥10%,全仓 grep `R-CHMEM-BTB-2` = 0 命中 | critic S5 |
| S6 | fetch substage 所有权未与既有声明对账 — `ip/mmu/tlm/MMUPlugin.cpp:57` 已 `declare_substage("fetch", "tlb_lookup_ifetch", 1)` (151 行 at_stage),与原 spec §1.3 假设冲突 | critic S6 |
| S7 | `§1.1 mmufault_redirect 是 Phase D 范畴` 论据过时 — `cpu_factory_chmem.h:331` 已注册 `RiscvCsrPluginChmem<T>` (第 8 plugin, csr-chmem-pipeline-integration) | critic S7 |

---

## 3. 启动序列 (Sprint 6-7, 2026-12-01 ~)

```bash
# 1. wave5-bp-btb owner 启动期阅读本 requirements doc
review .rddf/protocols/fetch_stall_requirements_v0.11.0.md

# 2. owner 解决 5 BLOCKER (见 §1) — 关键是:
#    - B1: 选 substage ordering API 扩展路径 (需新 ADR)
#    - B2: 选 stall 表达路径 (ch_bool payload / branch CtrlLink / pc_reg mux)
#    - B3: 定义 fetch_stall payload keys (双模, 归属 ip/cpu/core/payload_common.h)
#    - B4: Bare path 严格按 satp_mode 字段判定 (per D4)
#    - B5: 等 Phase B MMUPlugin_chmem 实装,获取 ch_bool ptw_active

# 3. owner 起草 v0.2 protocol spec (新文件 fetch_stall_protocol_v0.11.0.md),
#    解决 §1 BLOCKER 后提交. v0.2 spec 应:
#    - 显式引用解决的 BLOCKER (which design choice, which ADR)
#    - 通过 critic 二轮 review 才能进入实装

# 4. 实装 fetch_stall_chmem.h (~1-1.5 day, 估时基于 v0.11.0 简化版)
#    - CppHDL substage API 新 + 3 项协议接线需要 build/test 反馈

# 5. cpu_factory_chmem.h 注册 (Phase 2.3 C4 范围): 8→10 plugin
#    - + MMUPlugin_chmem + fetch_stall (per §2 S2 修正)

# 6. Phase C (ibus_chmem.h / dmem_chmem.h sv32 消费) 启动期已有 fetch stall framework 锚点
#    R-CHMEM-1 缓解兑现
```

---

## 4. Out-of-Scope (v0.11.0 范围不实施)

| Item | Reason | 推到 |
|------|--------|------|
| 5 BLOCKER 解决方案 | owner Sprint 6-7 启动期决策 | independent change |
| `bp.predict` 推测 disable 期间是否 flush 已 fetched | v0.11.0 简化 | wave6 or later |
| substage ordering API extension in PipeBuilder | 需要 ADR-048 扩展 | independent change |
| multi-cycle FSM for predict-with-PTW-fault | v1.0.0+ 优化, ADR-046 v2.0 + `chlib::ch_state_machine` DSL | v1.0.0 |
| `mmufault_redirect` priority 二次修订 | 取决于 MMU exception handler 实装 (Phase C.5) | when C.5 commits |

---

## 5. Decision Gate (Go / No-Go) — requirements 收集完成判断

**Go** (requirements doc 收集完成 → owner 可开始 BLOCKER 解决):
- 当前 ✅ (2026-10-16) — 5 BLOCKER + 7 SHOULD_FIX 已显式列出

**No-Go** (触发切换路线):
- BLOCKER 数量持续增长 (e.g. 新发现 fetch stall 与 CSR 冲突)
- Phase B MMUPlugin_chmem 实装在 Sprint 6-7 不能启动,B5 永久无解
- substage ordering API 扩展 (B1 候选路径 a) 8 周内不能落地

---

## 6. 跟踪

| Date | Event | Decision |
|------|-------|----------|
| 2026-10-16 | 起草 fetch_stall_protocol_v0.11.0.md (Draft v0.1, 237 行) | Draft v0.1 |
| 2026-10-16 | critic 审稿 REJECT + 5 BLOCKERS + 7 SHOULD_FIX | protocol 撤回 |
| 2026-10-16 | 降级为 fetch_stall_requirements_v0.11.0.md (本文档) | requirements doc v0.1 |
| 待 Sprint 6-7 (12-01) | wave5-bp-btb owner 解决 5 BLOCKER + 起草 v0.2 protocol | pending |
| 待 Sprint 6-7 (12-01) | v0.2 protocol 经 critic 二轮 review 通过后实装 | pending |

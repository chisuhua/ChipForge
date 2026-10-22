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

### 1.5 [BLOCKER-B5] ~~`ptw_active` 是 `Payload<bool>` POD,CH_MEM `CtrlLink::halt_when` 需要 `ch_bool` — 无合法信号源~~ → **已由 `chmem-mmu-keys-modality` 解除一半，另一半归 Phase C**

- **原状** (critic 实证):
  - `ip/mmu/tlm/mmu_keys.h:35` `PTW_ACTIVE` 是 **`Payload<bool>` (POD)**
  - CH_MEM 下 `CtrlLink::halt_when(Condition)` 的 `Condition = ch::core::ch_bool` (`ctrl_link.h:54,67`)
  - 跨类型取用正是 `mmu_exception_handler_chmem.h:15-27` 标注为"未确认"的路径
  - plan (`WT/.rddf/plans/mmu-chmem-pipeline-integration.md`) 把 key 类型统一推给 "Phase B (12-01)"
- **原后果**: §1.2 stall 语义 (§2 `drive_stall_signal`) 在 Sprint 2 窗口内**没有可用的信号源**。
- **原设计挑战**: 必须先定义 `ch_bool` 类型的 PTW_ACTIVE 信号(Phase B 实装),然后才能 stall。owner 在 Phase B 实装前无法起草 fetch_stall_chmem.h。

- **✅ 2026-10-22 更新（两半拆解）**:
  - **① key 类型（已解除）**: `chmem-mmu-keys-modality`（已 archive）把 `PTW_ACTIVE` 改为 `cf::plugin::uint_t<1>`（CH_MEM = `ch_uint<1>`）⇒ cell 能承载**运行期节点**，不再是 POD 常量。
  - **② `ch_uint<1>` → CtrlLink 条件的接线路径（已明确，无需新机制）**: **不走** `CtrlLink::halt_when` 的直接参数，而是经**聚合路径**（`include/cf/plugin/pipe_builder.h:435-452`）—— 插件把 `ch_uint<1>` 节点写进 payload cell，由 provider 在聚合点转成 `ch_bool` 条件交给 CtrlLink。`ch_uint<1>` 满足 `select()` 的 `HardwareType` 约束，也可经 `ch_uint<1>::operator ch_bool()`（上游 `core/uint.h:123-125`，仅 `N==1`）转成 `ch_bool` 节点（**不是常量折叠**）。
  - **③ 剩余阻塞（不属本 BLOCKER）**: `chmem-cpu-keys-modality`（姊妹 change，挂 Phase C.1 入口）负责 CPU 侧 `CPU_EXCEPTION_CODE` 等裸类型 key；本条 BLOCKER 的**信号源缺失**已不再是 Sprint 2 的硬障碍。
  - **结论**: 本条从 **BLOCKER** 降级为 **Sprint 3.5 可施工**（owner 仍为 mmu-chmem team；实施时须遵守 ADR-040 v3.0 §10.6 的「同 stage + EARLY/NORMAL」顺序契约）。

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
| 2026-10-22 | `chmem-mmu-keys-modality` archive ⇒ B5 的 key 类型半解除 | requirements doc §1.5 更新 |
| 2026-10-22 | `ctrl-link-chmem-reg-handle` archive ⇒ A1/A2 stateful halt 通道就位 | — |
| 2026-10-22 | Phase B（路线 C）落地 ⇒ `PTW_ACTIVE` 契约（key + 载体 + 顺序）确立 | — |
| **2026-10-22** | **v0.2 protocol 起草完成** ⇒ 5 BLOCKER 全部有解、7 SHOULD_FIX 全部处置（见 §7） | **本文档降级为「已消费的输入」**；协议正文 = `fetch_stall_protocol_v0.11.0.md` |
| 2026-10-22 | critic 二轮 review：**5 BLOCKER 已解**，但 v0.2 rev.1 自身引入 2 个新 BLOCKER（`br_taken_buf` 保持会丢指令；`PTW_ACTIVE` 解除缺下界）+ 4 SHOULD_FIX + 3 NITS | 修 rev.2 |
| 2026-10-22 | **v0.2 rev.2 完成**：`br_taken_buf` 改「冻结期强制 0」+ 逐拍时序表；新增 FS-C-12（解除下界）/ FS-C-13（两窗口不等宽）；CSR 改 **7 个** `we_*` + trap 写第二处守卫 + `mcycle_` 显式允许 + `mul_div_fsm` 显式排除；canonical 序修正为 `fetch,decode,execute,memory,branch,writeback`；AC 补注入机制 + 相对判据 | 待 critic 三轮 review |
| 2026-10-22 | critic 三轮：**REJECT** —— rev.2 的 `ptw_active` 全窗在冻结**进入端错相**（`imem.sread` 1 拍延迟 ⇒ 误杀页尾最后一条合法指令且永不重取）；`pc_reg` 保持吞掉进入拍的 branch redirect；CSR 漏**第三条**写入路径（writeback 内 `is_trap_1` 仲裁绕开 `we_*`）；§8.1 测试桩空指针 + 缺纯虚实现 | 修 rev.3 |
| 2026-10-22 | **v0.2 rev.3 完成**：核心机制改为 **`hold` / `hold_z` 双信号**（`FLUSH = br_taken_buf \| hold_z`，对齐读发起时刻）；`next_pc` 内层 select 实现 `trap > branch > freeze > sequential`；`br_taken_buf` 恢复原样（rev.2 的 mux 被证明冗余）；CSR **三处**守卫（`write_en_ch` 单点覆盖 7 个 `we_*` + `is_trap_1` + branch-LATE trap 直写）；桩补 `declare_substage` + `build()`；FS-C-12 重推导（+1 被 `hold_z` 吸收）；新增 FS-C-13/14/15 + AC-FS-11/12 | 待 critic 四轮 review |
| 2026-10-22 | critic 四轮：**机制层面 APPROVE**（逐拍表逐格无错、`next_pc` 嵌套与 `hold & !br_taken` 等价、CSR 三处枚举闭合）。REJECT 只卡两条文字级：§8.1 桩用了不存在的 `put_or_assign` + `PADDR`/`MMU_VADDR` 宽度写成 32（实为 `ch_uint<64>`）；FS-C-12 括号限定「且该地址的 walk 在途」使进入拍合规漏洞（registered-busy 实现会丢指令） | 修 rev.4 |
| 2026-10-22 | **v0.2 rev.4 完成**：§8.1 桩改直写范式 + `ch_uint<64>` + 新增「桩如何进入被测流水线」路径规范（`build_cpu()` 返回前已 `pb->build()`）+ AC-FS-13（桩必须可编译）；FS-C-12 改为按取指事实定义（miss 检出同拍必须为 1）+ AC-FS-14（3c 验收项）；另修 5 SHOULD_FIX + 5 NITS | 待 critic 五轮 review |
| 2026-10-22 | critic 五轮：**机制层继续 APPROVE**（上轮 2 BLOCKER 由 critic 独立复跑编译确认清零）。REJECT 只卡 1 个 rev.4 自身引入的矛盾：FS-C-12 主句漏 `fault=0` ⇒ page fault 期永久冻结（与同段派生公式 + §9.1 互斥） | 修 rev.5 |
| 2026-10-22 | **v0.2 rev.5 完成**：FS-C-12 主句补 `fault=0` + "fault 周期 MUST 归 0" + fault 变体 AC；§2.1「产生方」改派生公式（不再写 FSM busy）；§8.1 桩补 live 节点逐拍示例 + reset 口径定案（不调 `reset()`）+ `payload.h:122` 上流注释过时注 + 方案 A 所有权副作用；AC 表重排 1..13（桩可编译降为 §8.1 规范性检查项）；补 `hold ≡ PTW_ACTIVE` 冗余项说明 | 待 critic 六轮 review |
| 2026-10-22 | critic 六轮：**机制层继续 APPROVE**（rev.5 的 1 BLOCKER + 4 SHOULD_FIX + 7 NITS 全落实，编译独立复跑 EXIT=0）。REJECT 只卡 1 个 rev.5 自身引入的洞：`fault` 排除项使 fault 拍 bare 取指不再被 `hold_z` 抑制 ⇒ `F+1` 裸提交并持续 | 修 rev.6 |
| 2026-10-22 | **v0.2 rev.6 完成**：采纳**次序硬约束**（而非 critic 建议的机制改法 A —— A 需要 `fault` 的 live 载体，而 `PTW_FAULT` 是裸 `uint8_t`、模式化属 3d）：新增 FS-C-18「fault 语义 MUST 与 trap redirect 同批交付，在此之前 walk MUST NOT 产生 fault」+ AC-FS-14；§9.1 重写、§6.2 把 fault 移出 3c、桩 rst 改 `valid=1` + skew 表、§7 case 数 11→10 | 待 critic 七轮 review |
| 2026-10-22 | critic 七轮：**机制层继续 APPROVE**，且 critic 撤回其机制改法 A（`PTW_FAULT` 裸 `uint8_t` 的载体论证经其独立核实成立）。REJECT 两点：① FS-C-18 的 3c 半段零验证（§5/§6.2/AC-FS-14/AC-FS-9 四处互相推诿；且 `mmu_ptw_chmem.h:136` 的 `FAULT = 4` 已存在 ⇒ 违规非假想）② "同批交付"把 `pc_reg ← mtvec` 写成抑制手段之一 —— 但 PC 重定向拿不掉已发起的读 | 修 rev.7 |
| 2026-10-22 | **v0.2 rev.7 完成**：新增 **AC-FS-15**（3c 行为负向：坏 PTE 下 `PADDR_VALID` 不得为 1 且 `PADDR_VALID=0 ⇒ PTW_ACTIVE=1` 恒成立）+ **AC-FS-16**（3c 结构反回归：`PTW_ACTIVE`/`hold` 不得消费 `fault`/`fault_code`/`ptw_fault`），挂进 §5 与 §6.2 的 3c 门；FS-C-18 补交付清单 (a) `trap_pending` 第 4 驱动源 (b) 与 `br_taken_buf` 同构的 1 拍延迟 flush (c) `mepc ← faulting PC`；删除 "PC ← mtvec 二选一" 支；§9.1 加"在 fault 语义交付前两者无共用信号"；§1.2 fault 归属改指 FS-C-18；§6.2 依赖图改旁挂节点；§7 case 数 11 | 待 critic 八轮 review |
| 2026-10-22 | critic 八轮：**机制层第 7 轮连续 APPROVE**，rev.7 的 2 BLOCKER 清零、12/12 落实；但又暴露 rev.7 新增的两道 3c 门自身 2 个 BLOCKER（(b) 载体不存在 / AC-FS-15/16 不可执行） | 修 rev.8 |
| 2026-10-22 | **v0.2 rev.8 完成**：(b) 载体点名 `mmu_keys::EXCEPTION_CODE`（`mmu_keys.h:86` mode-aware，`MMUPlugin_chmem.h:282` 已写在 `tlb_lookup_ifetch` 同节点 ⇒ 无需新 key）；AC-FS-15 三类坏 PTE 按代码编码 + 窗口限 `sv_active=1` + 提交断言移出 3c 门；AC-FS-16 改 `grep -nw` + 语句行区间 + 合并批次合法例外；另修 SF-1/4/5/6 + N-1/2/3/4/6 | 待 critic 九轮 review |
| 2026-10-22 | critic 九轮：**机制层第 8 轮连续 APPROVE**，rev.8 的 2 BLOCKER 清零。但 rev.8 新引入 1 BLOCKER：两道 3c 门各自可绕过（`grep -nw fault` 漏检 `fault_live`/`ptw_fault`/`PTW_FAULT`/`fault_code`；AC-FS-15 ② 的"walk 在途"窗口排除 fault 拍（`mmu_ptw_chmem.h:283` FAULT→IDLE 无条件、`fault` 只 1 拍））⇒ `PTW_ACTIVE = sv & !pv & !ptw_fault` 能同时通过两门 | 修 rev.9 |
| 2026-10-22 | **v0.2 rev.9 完成**：AC-FS-16 ① 改子串 grep（作用域 = 语句行区间，区间内无 `default`）+ 排除注释行（`check_plugin_portability.sh:482` 范式）+ **明文禁用 `-w` 并附实测反例**；AC-FS-15 ② 窗口改按取指事实定义（含 FAULT 拍及其后各拍）+ 明文禁"FSM 在途"；AC-FS-15 加"不靠桩"与生产者点名（`MMUPluginChmem` fetch EARLY 闭包）+ 指出 `PtwFixture` 只注册 FSM；AC-FS-16 ④ 改"合并交付后整体退役"；AC-FS-14 ⑤ 扩为两处缺一即违约；`ch_uint<8>(0)` → `ch_literal<0,8>` | 待 critic 十轮 review |
| 2026-10-22 | critic 十轮：**机制层第 9 轮连续 APPROVE**。REJECT 1 BLOCKER：AC-FS-16 的 grep 规格**大小写敏感** ⇒ 拦不住它自己点名的 `PTW_FAULT`；且 `PTW_FAULT` 是裸 `uint8_t` ⇒ CH_MEM 下只能装常量 ⇒ 行为门对它天然无感 ⇒ 两道门同时失明 | 修 rev.10 |
| 2026-10-22 | **v0.2 rev.10 完成**：AC-FS-16 ① 加 `-i`（大小写不敏感是 MUST）+ 作用域钉死为**文件整体/闭包体**（禁被检者自选边界）+ 固定 allowlist（`default` 含子串 `fault` ⇒ `MMUPlugin_chmem.h` 1 项 / `ibus_chmem.h` 3 项）+ 明确排除注释行；AC-FS-15 补 **anti-vacuity 前提断言**（三类坏 PTE 各自 MUST 观察到 `state_out()==4` ≥1 拍，未观察到 ⇒ 判 FAIL）；`:55` 措辞、AC-FS-14 ⑤ 口径、tasks.md 轮次同步；协议文件 `git add -N`（可审计） | 待 critic 十一轮 review |
| 2026-10-22 | critic 十一轮：**机制层第 10 轮连续 APPROVE**。REJECT 1 BLOCKER：AC-FS-16 的**行号 allowlist** 因本协议自己的 §7 item 1（3g 在 `ibus_chmem.h` 插入 `hold_z_` 成员）**必然漂移** ⇒ 假红；补救路径又留"就地扩项"假绿通道 | 修 rev.11（变换判据） |
| 2026-10-22 | **v0.2 rev.11 完成**：行号 allowlist → **变换判据**（注释过滤 → 删 `default`（`-i`）→ 仍含 `fault` ⇒ 候选；实测：pristine 空、3g 插入成员后仍空、注入 `FAULT` 被抓住）；② 升级为逐语句文件级；AC-FS-15 前提断言加**实例同一性**锚 + 驱动权 fail-closed；① 改锚数据流 + 派生式必须内联 + scope 跟随 producer 文件；5 NITS；rev.10 快照留 `/tmp/opencode/protocol_rev10_base.md`（真 diff base） | 待 critic 十二轮 review |
| 2026-10-22 | critic 十二轮：**机制层第 11 轮连续 APPROVE**，rev.11 的 diff 首次可验证。REJECT 1 BLOCKER：**交付物结构** —— rev.11 把 ```bash 围栏塞进表格行 ⇒ 围栏奇数（5）+ AC 表在 `:575` 断裂 + `doc_checker.py` EOF 停在 code block 内 ⇒ 尾部 66 行（含 §10 跟踪表 38 行）全部逃检 | 修 rev.12（围栏移出表格） |
| 2026-10-22 | **v0.2 rev.12 完成**：围栏移出表格行（命令改内联 + `<br>`）⇒ `grep -c '^```'` 回到 4、`EOF in_code_block=False`、skipped 116（与 rev.10 一致）；AC-FS-16 ①② 改 **RHS 操作数白名单**（W1/W2 固定表，白名单外标识符 ⇒ FAIL，与命名无关；3c 需新操作数 MUST 修订 AC）；③ 降为提示性扫网 + 候选登记**可核对工件**；AC-FS-15 前提断言加**句柄同源**（`pb.plugins()` 指针相等）+ **因果耦合**（好 PTE ⇒ refill；好/坏行为相同 ⇒ FAIL）两条机械锚；3 NITS | 待 critic 十三轮 review |
| 2026-10-22 | critic 十三轮：**机制层第 12 轮连续 APPROVE**。REJECT 2 BLOCKER（rev.12 白名单判据自身）：提取方式未指定 + W 表混用 qualified 形态 ⇒ 两种合法读法相反判定；W2 缺 `hold_z` ⇒ 协议自述的 3g 形状必然假红 | 修 rev.13 |
| 2026-10-22 | **v0.2 rev.13 完成**：新增 **⓪ 提取范式**（写语句定位 + RHS 切片 + 剥注释 + token 提取，限定名每段都算）；W1/W2 改 **bare-token**；W2 补 `hold_z`/`hold_z_`；§7 item 1 钉 canonical 拼写（成员 `hold_z_reg_` + 语句用 `hold_z`）；SF-1 采纳方案 (a)（W1 scope 含操作数定义语句 ⇒ 操作数投毒被机械覆盖）；**判据自测 13/13**（`/tmp/opencode/wl_check.py`，自测抓到 1 个真缺口并修正） | 待 critic 十四轮 review |
| 2026-10-22 | critic 十四轮：**APPROVE_WITH_NITS —— 判据层首次零 BLOCKER**（机制层连续 12 轮 APPROVE）。2 SHOULD_FIX（⓪ 别名洞 + 定义语句识别/词表纪律）+ 4 NITS | 修 rev.14 |
| 2026-10-22 | **v0.2 rev.14 完成**：⓪ 加「先剥注释再做一切」+「LHS MUST 字面含 key 名（禁别名/宏间接写）」+「操作数定义语句 = 同一闭包内 LHS 含该操作数名的语句，全部检查」；W1 逃生条款扩为任何新 token 来源 + pin `:266-270` 词表 + 明文不做传递闭包检查；`ch_literal` 去重；AC-FS-15 (b) 补「观察面不钉文件」；自测脚本归宿写死（3c MUST 收进 `tests/`/`tools/`） | 待 critic 十五轮（仅核对 diff） |
| 2026-10-22 | critic 十五轮：**APPROVE**（判据层零 BLOCKER、零 SHOULD_FIX；2 条 NIT）⇒ **可进入 §3.5.3 验收归档流程** | ✅ rev.14 定稿 |
| ✅ | **v0.2 rev.14 定稿** ⇒ 归 `chmem-ptw-walk` 的 3g（≈3.2d；3c 需同时满足 **AC-FS-9 + AC-FS-15 + AC-FS-16**） | **done 2026-10-22** |

---

## 7. 状态更新（2026-10-22）—— 本文档已被 v0.2 协议消费

> **本文档的 5 BLOCKER + 7 SHOULD_FIX 已在 v0.2 protocol 中逐条给出解与证据**：
> `.rddf/protocols/fetch_stall_protocol_v0.11.0.md`（§3 = 5 BLOCKER 的解，§4 = 7 SHOULD_FIX 的处置）。
>
> **本文档保留不动**，作为「问题是怎么被发现的」的历史留档（5 BLOCKER 的现状描述在 2026-10-16 是准确的）。

### 7.1 5 个 BLOCKER 的当前状态

| ID | 状态 | 结论摘要（详见 v0.2 §3） |
|----|------|------------------------|
| B1 substage 不产生调度顺序 | ✅ 已解 | **不需要**扩展 substage 排序 API —— 用 ADR-040 v3.0 §10.6 的「同 stage 名 + `EARLY`/`NORMAL`」契约；`elaborate()`（`pipe_builder.h:413-424`）的 stage×phase 二重循环保证同 stage 内 phase 序 |
| B2 CtrlLink 在 elaborate 路径不消费 | ✅ 已解（**结论推翻 v0.1 方案骨架**） | `register_stage_payload_connector` **全仓仅测试用** ⇒ 生产代码 `stage_payload_map_` 为空 ⇒ **CtrlLink 的 per-stage stall 在 CH_MEM 下零消费者** ⇒ fetch stall 改走 **payload 信号路径**（`fetch.FLUSH` 扩展 + `pc_reg` 保持 mux），**不新建插件** |
| B3 payload key 类型/归属 | ✅ 已解（**不需要新 key**） | `PTW_ACTIVE` 已是 `uint_t<1>`（mode-aware）；`FLUSH` 是 CH_MEM-only 声明，TLM 零消费者 ⇒ 当前用法不违规。归属问题随之作废 |
| B4 Bare 判定公式 vs D4 | ✅ 已解 | 严格按 `satp` MODE 字段（`MMUPlugin.cpp:97-110`）；v0.1 §2.4 的 `satp_ppn == 0` 析取项**作废**。且 fetch stall 侧**无需**重复实现 Bare 判定（`PTW_ACTIVE` 恒 0 即天然不触发） |
| B5 `PTW_ACTIVE` 信号源 | ✅ 已解（key 半解 + 路径已明） | key 类型已由 `chmem-mmu-keys-modality` 解除；载体 = `tlb_lookup_ifetch` cell；顺序契约 = 同 stage + `EARLY`/`NORMAL`；`ch_uint<1>` → `ch_bool` 走 `operator ch_bool()`（N==1）**非折叠**。**剩余**：live 节点由 `chmem-ptw-walk` 提供 |

### 7.2 7 个 SHOULD_FIX 的当前状态

| ID | 状态 | 摘要 |
|----|------|------|
| S1 key 归属 | 作废 | 本协议不新增 key |
| S2 插件计数 | 作废 | 本协议不新增插件；`MMUPluginChmem` 注册计数归 C.4 |
| S3 "byte-identical" 措辞 | 已修正 | 改为**信号级**等价断言（`br_taken_buf \| false == br_taken_buf` ⇒ 现有 `[chmem]` 全量零变化） |
| S4 审查时序 | 已消解 | v0.2 起草窗口 Sprint 3 后段（11-06 ~ 11-30），早于 C.4 入口 |
| S5 阈值矛盾 | 作废 | 不以 CoreMark 为判据（CoreMark 归 v1.0.0 的 BP 范畴）⇒ 改用信号级判据，无阈值歧义 |
| S6 fetch substage 所有权 | ✅ **已对账** | Phase B 的 `MMUPlugin_chmem.h` 采用**同名** `declare_substage("fetch", "tlb_lookup_ifetch", 1)`，与 `MMUPlugin.cpp:57` 完全一致 ⇒ 命名无冲突；且 substage 不参与调度 ⇒ 所有权问题不成立 |
| S7 `mmufault_redirect` 论据过时 | ✅ 已过时 | `cpu_factory_chmem.h:338` 已注册 `RiscvCsrPluginChmem<T>` ⇒ CSR 通路存在；trap PC 跳转归 R11.1 单独裁决 |

### 7.3 落地归属的变化（**需注意**）

| 原设想 | 现归属 | 理由 |
|--------|--------|------|
| 新建 `fetch_stall_chmem.h` 插件（1-1.5d） | **取消** | B2：CtrlLink 路径在 CH_MEM 无消费者；FLUSH 路径不需要新插件 |
| fetch stall 接线作为独立增量 | **归 `chmem-ptw-walk` 的 3g**（与 PTW 的 live `PTW_ACTIVE` 同批落地） | 否则得到**恒 false 惰性骨架**（`ctrl-link-chmem-reg-handle` 期间 A4 同款陷阱） |
| `chmem-cpu-keys-modality` | ✅ **已闭合**（archive 2026-10-22, commit `d08d3ba`） | CSR 守卫要读 `CSR_WRITE_EN` 作条件；该 key 原为 `Payload<bool>`（POD），现为 `uint_t<1>` ⇒ 前置解除 |

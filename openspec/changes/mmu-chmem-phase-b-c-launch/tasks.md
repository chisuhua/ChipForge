# Tasks — mmu-chmem-phase-b-c-launch (Sprint 2-5 follow-up placeholder)

> **状态 (2026-10-17)**: 🟡 占位 (placeholder), 由 `mmu-chmem-pipeline-integration` Phase 2 done (2026-10-17) 抽出, 接管 Sprint 2-5 (2026-10-23 ~ 2026-11-30) + Sprint 6-7 (2026-12-01 ~ 2026-12-22) 实施窗口。
>
> **本 tasks.md 仅为元数据 anchor**, 不动 code/test/CMakeLists.txt/design.md。所有 task 描述详见源 change `openspec/changes/mmu-chmem-pipeline-integration/tasks.md` (583 行) 的 Phase B/C/D/E 段落。本文件用极简 [ ] 占位等 Sprint 2-5 rdd-builder P1 plan gen + rdd-planner 阶段细化。

---

## pre_flight (Sprint 1 启动期必做)

> **2026-10-22 双审重构**: 原 4 项 pre_flight 中, #2 (`fetch-stall-protocol-v0.2`) 与 #3 (`bp-btb-coordination-3items`) **移出 pre_flight** —— #2 的 owner (wave5-bp-btb) 已于 2026-10-19 S3 决策 defer 到 v1.0.0 ⇒ **Sprint 2-5 窗口内无人可交付**; 且 `fetch_stall_requirements_v0.11.0.md` §1.5 (BLOCKER-B5, `:74-82`) 明写 *"必须先定义 ch_bool 类型的 PTW_ACTIVE 信号（Phase B 实装），然后才能 stall"* ⇒ **fetch-stall v0.2 反向依赖 Phase B**, 放在 Phase B 的 pre_flight 里是**逻辑倒挂**。二者合并为 **Phase 3.5 slot** (见下), 挂载点 = **C.4 入口条件**。
> **剩 2 项 pre_flight 均有 owner + 交付物 + 窗口 + 挂载点, 全部可满足**。

- [x] **R11.1 重审** (2026-10-22, **0.5d**) — 裁决见 §Phase 3.5 的 **3.5.1**（分拆二判：ECALL/EBREAK/MRET redirect 实装为独立 change `csr-chmem-trap-pc-jump`；MMU-fault redirect defer 到 `chmem-ptw-walk` 之后）
- [x] **CtrlLink API gap (FP-4) 解锁** (2026-10-22) — 已由 **`ctrl-link-chmem-reg-handle`**（archive 2026-10-22）交付 A1/A2/A3/A5：`halt_when_stateful(const ch_reg<ch_bool>&)` + `halt_when(ch_reg<ch_bool>)` 自动有状态重载 + `OR_combined_halt_conds()` 单一 OR 实现（DAG 节点，可被消费方引用）。A4（`hazard_chmem` 的 mmufault instance bool → 信号驱动）按原计划改挂本 change 的 **C.5**

## Phase 3.5 (Sprint 3 后段, 2026-11-06 ~ 2026-11-30, 3.5d)

> **由 pre_flight #2 + #3 重构而来** —— 挂载点 = **C.4 入口条件** (C.2/C.3 已落地不需要它; 真正需要它的是 C.4 注册激活 + C.6 集成测试)。

- [x] **3.5.1** (2026-10-22, **0.5d**) **R11.1 trap PC 跳转可行性重审** — 调研结论：**分拆二判**（`2026-10-15 csr-chmem-writeback-complete` archive 后 R11 失效的前提已变，重审后不是"全实装"也不是"全 defer"）。
  **① ECALL / EBREAK / MRET redirect（R2.3 范围）→ 实装，但作为独立 change `csr-chmem-trap-pc-jump`（≈1.5d），不并入 C.1**
  - **硬前置**：`chmem-cpu-keys-modality` 必须先 **commit + archive** ✅ **已于 2026-10-22 满足**（commit `d08d3ba` + archive `2026-10-22-chmem-cpu-keys-modality`）—— `TRAP_PENDING`/`TRAP_TARGET`/`CSR_WRITE_EN` 已模式化为 `uint_t<1>`，且 `csr_chmem.h:459-461` 已拆掉 `static_cast<bool>` 折叠、改为直写 DAG 节点。**若晚于该 archive 重做本项，须复核这批 key 未被改回 POD**（回滚即 G3 惰性骨架：redirect 消费到常量 false）
  - **插入点**：`ip/cpu/plugins/ibus_chmem.h:171-185` 的 `update_pc` —— `next_pc = select(trap_pending, trap_target, select(br_taken, br_target, pc_plus_4))`（**trap > branch 优先级**，per `openspec/specs/csr-chmem-zicsr/spec.md:94`）；同步把 `br_taken_buf <<= br_taken` 扩为 `br_taken | ch_bool(trap_pending)`（复用本 protocol 的 `FLUSH` 抑制机制压 fall-through）
  - ⚠ **生产者必须从 branch-LATE 移到 branch-EARLY**（`csr_chmem.h:426` 的 trap 闭包）：同 stage+phase 桶内按 `stages_` 插入序执行，而 IBus 的 `update_pc`（plugin #3，`cpu_factory_chmem.h:331`）**先于** CSR 的 trap 写（plugin #6，`:338`）⇒ 现状是**消费者先于生产者**（违反 ADR-040 §10.6，会读到 emplace-on-miss 的空 cell）。改 EARLY 后依赖 `last_funct12_`（execute-LATE 产出，execute < branch ✓）与 `mepc_`/`mtvec_`（自有 `ch_reg` ✓）与 `KeyType::PC`（decode-LATE propagate，decode < branch ✓）
  - **时序可行，零 bubble**：canonical 序实测 `fetch → decode → execute → memory → branch → writeback` ⇒ memory < branch；且 CH_MEM 数据通路**无 stage 间寄存器**（全组合 DAG）⇒ 同拍组合 redirect 成立，无需额外 bubble。**R11 旧假设"物理不可能"在时序上不复成立**
  - **原 R2.3 设计（在 ibus 新增 branch-NORMAL 消费闭包）作废**：现行 ibus 只有 `fetch NORMAL` + `branch LATE` 两个注册点（`ibus_chmem.h:70-71`），`pc_reg` 的唯一驱动者在 branch LATE。最小改法 = 生产者移 EARLY + 改 `update_pc` 一处
  **② MMU-fault redirect → 继续 defer 到 `chmem-ptw-walk` 之后**（打包为 follow-up change，建议名 `chmem-mmu-fault-trap-entry`，目标 v0.11.1 或 PTW 窗口）
  - **理由**：Phase C 内其驱动 `EXCEPTION_CODE` 在真实运行**恒 0**（`ip/mmu/plugins/MMUPlugin_chmem.h:282`，路线 C 无 fault 语义）⇒ 实装即"看起来接上了但永不触发"，与 `hazard_chmem.h:174-179` 已明文拒绝的 G3 同款；且必须捆绑 R11.3 三 driver 仲裁 + mtvec 跨 plugin 读取，scope 从 +1d 膨胀到 +3d 以上
  - **不留代码占位**：F6 注释（`csr_chmem.h:346-357`）已登记三 driver 仲裁扩展点且 owner 明确，本身就是合格路标（项目纪律：不留 for future use 死代码）
  **③ 排期影响**：C.1（2d）/ C.4（2d）/ C.5（1d）/ C.6（3d）范围与估时**均不变**；新增独立 change ≈1.5d
  **④ R11.2 必须包含的事实更正**（本项不做，留给 R11.2 的 7 文件批次）：
  - `openspec/specs/csr-chmem-zicsr/spec.md:69` —— 现文「consumed by IBus at `at_stage("branch", NORMAL)` (registered after IBus branch-LATE, so IBus NORMAL wins in elaboration order)」**双错**：① `ibus_chmem.h:70-71` 只有 `fetch NORMAL` + `branch LATE` 两个注册点，**没有** branch-NORMAL 闭包；② 「NORMAL wins in elaboration order」的推理方向反了 —— `elaborate()` 的 phase 循环是 EARLY→NORMAL→LATE（`pipe_builder.h:416-423`），NORMAL 恒先于 LATE，不是"NORMAL wins"。正确表述：「CH_MEM 侧当前**零消费者**（`grep TRAP_PENDING ip/cpu/plugins/ibus_chmem.h` = 0）；接线插入点 = `ibus_chmem.h::update_pc`（branch 阶段），且**生产者须从 branch-LATE 移到 EARLY**」
  - `bundles/csr_bundles.h:29` —— 「接线归 mmu-chmem-phase-b-c-launch 的 C.1」⇒ 改指新 change `csr-chmem-trap-pc-jump`（R11.1 裁决：不并入 C.1）
  - ✅ `bundles/csr_bundles.h:26-29` 的「零消费者」登记**已由 `d08d3ba` 修好**，无需再动
  **⑤ 过时线索更正**：`TRAP_PENDING` **已不是** `Payload<bool>` —— `d08d3ba` 已改 `Payload<uint_t<1>>` 并直写 DAG 节点（`csr_chmem.h:459-461`）；任何仍称其为 POD 的文档均需 R11.2 一并修
- [x] **3.5.2** (2026-10-22, **1d**) **bp-btb 收窄 2 项** — 去掉 `pc_reg mux` 的 **BP 分支**（`feat-bp-btb-gshare` 已 defer 到 v1.0.0），保留 **stall 语义** + **fetch stage at_stage 顺序**；owner 改 mmu-chmem team（= requirements doc BLOCKER-B1 + B2）。
  **交付**：两项均已并入 v0.2 protocol 的规范性条款（**2026-10-22 rev.5 定稿后同步更新本节**）——
  - **stall 语义** → `fetch_stall_protocol_v0.11.0.md` §2.2（`FLUSH` 第二来源 = **`br_taken_buf | hold_z`**，`hold_z` 是 `hold = ptw_active & !paddr_valid` 的 1 拍延迟副本）+ §2.3（`pc_reg` 的 `hold` 保持分量；**`br_taken_buf` 保持原样不做门控**）+ §2.4（**14 项「提交点与新增状态」覆盖矩阵**，含 3 项显式"允许/排除"；含发现的新漏洞：CSR 写回无 FLUSH 守卫 + `is_trap_1` 仲裁绕开 `we_*` + branch-LATE trap 直写，共 **3 处**守卫）。⚠ **该 pc_reg mux 只含 stall 分量，不含 BP 分支** ⇒ 与 2026-10-19 S3 的 BP defer 决策不冲突
  - **fetch stage at_stage 顺序** → v0.2 §3 B1：生产者在 `("fetch", Phase::EARLY)` 写、消费者在 `("fetch", Phase::NORMAL)`（发布 FLUSH）+ `("branch", Phase::LATE)`（算 `hold` / 驱动 PC 与 `hold_z`）读；canonical 序 = **`fetch → decode → execute → memory → branch → writeback`**（`branch` **早于** `writeback`；由阶段名**首现序**决定 —— `cpu_factory_chmem.h:213-216` 四行 `at_stage` 的 stage 参数**全是 `"decode"`**，不引入新 stage）⇒ 跨 stage 读安全
  > ⚠ **rev.1/rev.2 期的本节描述已作废**：曾写「`FLUSH` 第二来源 `br_taken_buf | ptw_active`」「4 个提交点」「canonical 序 … memory → writeback → branch」——三者均已被 rev.3~rev.5 的实证推翻（见 protocol §10 跟踪表）。
- [x] **3.5.3** (2026-10-22, **critic 十五轮 APPROVE**) **fetch-stall v0.2 protocol 起草** — 产出 `.rddf/protocols/fetch_stall_protocol_v0.11.0.md`（**v0.2 rev.14 终稿**）。
  **5 BLOCKER 全部有解**（v0.2 §3，每条带 file:line 证据）：B1 同 stage+phase 契约（不需扩展 substage 排序 API）｜ B2 **实证推翻 v0.1 方案骨架** —— `register_stage_payload_connector` 全仓仅测试用 ⇒ 生产代码 `stage_payload_map_` 为空 ⇒ **CtrlLink 的 per-stage stall 在 CH_MEM 下零消费者** ⇒ 改走 payload 信号路径（`FLUSH` 扩展 + PC 保持 mux），**不新建插件 / 不新增 key / 不改框架** ｜ B3 `PTW_ACTIVE` 已 mode-aware ⇒ 无需新 key ｜ B4 严格按 `satp` MODE 字段（`MMUPlugin.cpp:97-110`）｜ B5 key+载体+顺序+类型转换路径全通
  **7 SHOULD_FIX 全部处置**（v0.2 §4）：S1/S2/S5 作废 ｜ S3 改为**信号级**等价断言 ｜ S4 已消解 ｜ S6 已对账（Phase B 用同名 `declare_substage("fetch","tlb_lookup_ifetch",1)`）｜ S7 已过时
  **关键结论 — 落地归属调整**（v0.2 §6.3）：接线**不得**作为"先接线后填驱动"的独立增量（`PTW_ACTIVE` 在路线 C 下是常量 0 ⇒ 会得到恒 false 惰性骨架）⇒ **归 `chmem-ptw-walk` 的 3g**，与 PTW 的 live 信号同批落地
  **依赖状态**：对 `chmem-cpu-keys-modality` 的依赖 **已闭合**（archive 2026-10-22, commit `d08d3ba`；`CSR_WRITE_EN` 等已由 `Payload<bool>` 改为 `uint_t<1>`）
  **策划史**：critic **十五轮** review（rev.1 REJECT 5 BLOCKER → rev.2 REJECT 2 → rev.3 REJECT 4 → rev.4 REJECT 2 → rev.5 REJECT 1 → rev.6 REJECT 1 → rev.7 REJECT 2 → rev.8 REJECT 1 → rev.9 REJECT 1 → rev.10 REJECT 1 → rev.11 REJECT 1 → rev.12 REJECT 2 → rev.13 REJECT 2 → rev.14 **APPROVE_WITH_NITS** → rev.15 **APPROVE**）；**机制层自 rev.3 起连续 13 轮 APPROVE**（零触碰）—— REJECT 全部落在**判据层**（AC 的可执行性 / 可绕过性）、**交付物结构**（rev.11 围栏嵌表格）与**判据自洽性**（rev.12 白名单）；**判据层在 rev.15 首次归零**。逐轮修法见 protocol §10 跟踪表。**验收**：protocol §8.1 桩编译演练 EXIT=0（`-fsyntax-only`）、AC-FS-16 判据自测 13/13（`/tmp/opencode/wl_check.py`；**3c change MUST 把等价自测收进 `tests/`/`tools/`**）、`doc_link_check` 0 broken、`roadmap-r9-check` pass=10

## Phase B (Sprint 2-3, 2026-10-23 ~ 2026-11-06) — **路线 C 收窄后 3.0d（原 15d）**

> **估时口径 (2026-10-22 双审 → 2026-10-22 路线 C 重算)**: 原 `B.2 3d + B.3 5d + B.4 2d + B.5 5d = 15d`（`.rddf/plans/mmu-chmem-pipeline-integration.md` 原写 10d **整段丢失 B.5 的 5d 集成回归**, 已回改）。**用户裁决路线 C 后**：B.2（适配器）与 PTW walk 一并推迟到新 change `chmem-ptw-walk`（估 6.5d）⇒ 本 change 的 Phase B = **B.0 0.1d + B.3(TLB-only) 1.5d + B.4 1.0d + B.5 0.5d = 3.0d**。**B.1 跳过** (Oracle D1-B 收窄版锁定)。
> **路线 C 理由**: `PtWalkFsmPlugin` 不能表达式驱动（侦察结论 #3）⇒ 复用前需先重构, 而该重构不得脱离消费者单独落地; 且多周期 walk 期间冻结 fetch 需要 fetch stall 协议（Phase 3.5 未起草）。收窄为 TLB-only 后上述两个未决问题都不阻塞交付。
> **路径与 key 裁决**: CH_MEM 新文件落 `ip/mmu/plugins/MMUPlugin_chmem.h` (**非** `ip/mmu/tlm/` — 后者同时逃出 `check_plugin_portability.sh` Check 2 与 Check 7 扫描范围 = CI 盲区); **不新建 bundles key 文件**, 复用 `ip/mmu/tlm/mmu_keys.h` 的 `mmu_keys<std::uint64_t>` 既有静态实例 (PayloadStore 按**指针身份**索引 ⇒ key 不同 ⇒ 消费端 `has()` 恒 false ⇒ 静默回退 Bare ⇒ **测试全绿但 sv32 从未生效**)。
> **key 类型 + producer 顺序裁决 (2026-10-22, 硬前置 `chmem-mmu-keys-modality`)**: 上面两条只解决**身份**, 没解决**模态** —— 裸类型 key 在 CH_MEM 下 cell 是 POD, 装不下 `ch_uint` 节点。前置 change 已把 5 个输出 key 模式化 + 2 个消费点改「读节点」+ 成文顺序契约 (ADR-040 v3.0 §10.5/§10.6)。**B.2/B.3 实施时 producer SHALL 与消费端挂同一 stage 名 + `Phase::EARLY`**, **禁**注册新 stage 名 (否则 `canonical_stage_order()` 把 producer 排到 consumer 之后 ⇒ 永久静默 Bare)。

### Phase B 施工前侦察结论 (2026-10-22, 逐条带 file:line 证据)

> 本节是 B.0-B.5 的**施工前提修正**, 4 条结论均改了原计划假设, 实施前必读。

1. **B.2 / B.3 施工顺序修正 —— B.3 必须先行**
   `RiscvMMUPluginChmem`(B.2, `ip/cpu/plugins/mmu_chmem.h`) 是 `MMUPluginChmem`(B.3, `ip/mmu/plugins/MMUPlugin_chmem.h`) 的**派生适配器**, 与 TLM 侧同构 (蓝本 `ip/cpu/plugins/mmu.h:36` `class RiscvMMUPlugin : public cf::ip::mmu::MMUPlugin`)。基类在 B.3 才存在 ⇒ **B.2 无法独立编译**。原计划"B.2 → B.3"只能作为**逻辑分层**读, 实际施工顺序 = **B.3 基类骨架 → B.2 适配器 → B.3 TLB/PTW 主体 → B.4 测试**, 三者同一 commit 或同一 change 内连续提交, 不可拆成互不编译的中间态。

2. **vaddr 源已可用 —— B.2 不依赖姊妹 change**
   `ip/cpu/core/payload_common.h:126` `keys<T,XLEN>::PC` 声明为 `Payload<T>` (**按 T 参数化**, 不是硬编码裸类型)。CH_MEM 工厂以 `CpuFactoryChmem<ch_uint<32>>` 实例化 (`tests/cpu/test_cpu_chmem_vendored_elf.cpp:146`) ⇒ `T = ch_uint<32>` ⇒ cell 本就是 `ch_uint<32>` 节点。`ibus_chmem.h:120` / `dmem_chmem.h` 的 `using KT = keys<T, kXlenBits>` 与工厂**同一实例** ⇒ 父节点 `PC` / `MEM_ADDR` 可读到真节点。
   ⚠ **陷阱**: MMU 侧读父节点时必须用 `keys<ch_uint<32>, 32>`（即同一 `T`）, 写 `keys<std::uint32_t, 32>` 会得到**不同静态实例** ⇒ 指针不等 ⇒ `has()` 恒 false（与 `mmu_keys` 的身份陷阱同源）。TLM 侧现有写法 `mmu.cpp:145-146` 的 `Keys32/Keys64` 是双 KeyType 查找, CH_MEM 侧改为单一 `ch_uint<32>` 实例即可。

3. **`PtWalkFsmPlugin` 不能表达式驱动 —— B.3 复用前必须重构**
   `ip/cpu/plugins/mmu_ptw_chmem.h:226-243`: 输入 `ch_uint` 信号在 `create_fsm()` **内部**才创建; `start_sig` / `pte_ok_sig` / `v_sig`… 在 `:238-243` 立刻从 `*start_` / `*pte_valid_` **拉边捕获**。`setup()` 在 `:165` 注册 `at_stage("mmu_ptw", NORMAL, create_fsm)` 是唯一入口 ⇒ 外部**无法在捕获发生前**注入表达式（elaboration 结束后再 `*start_ = expr` 只是重绑句柄, 已捕获的 DAG 边仍指向旧字面量 0）。
   ⇒ B.3 复用前先抽取自由函数：`build_pt_walk_fsm(ch_uint<1>& start, ch_uint<1>& pte_valid, …, ch_uint<22>& pte_ppn) -> PtWalkFsmOutputs{state, done, fault, fault_code, result_ppn}`（输入按值/引用传入, 输出为节点结构）。`PtWalkFsmPlugin` 保留为**薄包装**（`create_fsm()` 内调用该函数）, 保证 `tests/cpu/test_mmu_ptw_fsm_chmem.cpp` 零改动通过。
   ⚠ **该重构不得脱离 B.3 单独落地** —— 否则成为无消费者的死抽象（违反 scope 最小化）。

4. **姊妹 change 的准确范围（比原立项更宽, 且含一处活体缺陷）**
   `csr_keys` 并非全 mode-aware：
   - `bundles/csr_bundles.h:39` `CSR_READ_VALUE` / `:43` `CSR_WRITE_ADDR` / `:44` `CSR_WRITE_VALUE` / `:51` `TRAP_TARGET` 是 `Payload<T>` — 经 `cpu_factory_chmem.h:184` 以 `T = ch_uint<32>` 实例化 ⇒ **已 mode-aware** ✅
   - `bundles/csr_bundles.h:40` `CSR_READ_VALID` / `:45` `CSR_WRITE_EN` / `:48` `Zicsr_VALID` / `:51` `TRAP_PENDING` 是 **`Payload<bool>`**（POD）❌；`:49` `Zicsr_OP` 是 `Payload<std::uint8_t>` ❌
   - `ip/cpu/tlm/cpu_keys.h:31-33` `static_assert` **无 `#ifndef CF_PLUGIN_USE_CH_MEM` 守护** ⇒ `cpu_keys` 永远只能以裸 `T` 实例化 ⇒ `SAT` / `SFENCE_*` / `CPU_EXCEPTION_CODE`(`:46`, `Payload<std::uint8_t>`) 全是 POD ❌
   ⇒ 姊妹 change `chmem-cpu-keys-modality` 覆盖 **`cpu_keys` 一族 + `csr_bundles` 的 5 个裸类型 key**, 且必须一并**拆除 `csr_chmem.h:191-194` 的常量折叠 workaround**（`static_cast<bool>(select(...))` 把 `CSR_READ_VALID` 折成 elaboration 期常量 —— 这是同一缺陷类的**活体实例**; 现有 `[chmem]` 测试因经 `last_csr_write_en_` 观察口而全绿, 属"测试全绿但语义为零"）。
   另: `TRAP_PENDING` 在 CH_MEM 侧 grep **零消费者**（`csr_bundles.h:17` 声称由 `ibus_chmem.h` NORMAL select 消费, 实际不存在）⇒ trap 重定向路径疑似从未接线, 已登记 ADR-040 v3.0 §10.8, **不并入**姊妹 change。

5. **`Simulator::reset()` 会清掉 `ch_reg` 的 ctor init（施工期实证, B.3 实施时踩到）**
   `reset()` 把每个 `ch_reg` 打成 `rst_val`；ctor 未显式给 reset ⇒ `nullptr` ⇒ 0。先例注释：`tests/cpu/test_cpu_chmem_vendored_elf.cpp:153-156`（*"sim->reset() is intentionally NOT called … clobbering the PC's init value 0x80000000"*）。
   ⇒ **任何"elaboration 期常量初值"型寄存器（satp / pc / 配置寄存器）都不能只靠 ctor init 存活**，必须同时写 next 值（`*reg <<= expr`）—— 后者在 reset 后第一拍恢复。本 change 的 `satp` 即按此实现（`MMUPlugin_chmem.h:176-183`），变异 D 证实只留 ctor init ⇒ `satp.MODE` 恒 0 ⇒ 恒 Bare（3 个 translate case 全红）。
   ⚠ 该陷阱对后续 `chmem-ptw-walk`（PTW 的 `satp`/page-table base 改写）与 Phase C.4（`cpu_factory_chmem` 接线）**同样适用**。

### Phase B 路线 C 收窄 (2026-10-22 用户裁决) —— PTW walk 推迟到 `chmem-ptw-walk`

原 B.2 (`RiscvMMUPluginChmem` 派生适配器) + B.3 的「复用 `PtWalkFsmPlugin`」**一并推迟**，理由 = 上面侦察结论 #1 + #3：适配器依赖基类（B.3 先行才对），而 PTW 复用又依赖对 `PtWalkFsmPlugin` 的重构，该重构不得脱离消费者单独落地。本期只交付 **TLB 命中路径**，TLB 由测试直接装载（refill = `entry_*` 的 ch_reg 句柄），不做「miss → 逐级读 PTE → refill」。

**估时口径重算**：原 `B.2 3d + B.3 5d + B.4 2d + B.5 5d = 15d` → 收窄后 `B.3(TLB-only) 1.5d + B.4 1.0d + B.5 0.5d = 3.0d`（B.0 0.1d 已完成）；推迟部分（B.2 适配器 + PTW walk + FSM 重构 + fetch stall 协议）移入新 change **`chmem-ptw-walk`**，估 6.5d。

- [x] **B.0** (2026-10-22, **0.1d**) **Payload key 前置复核** — `chmem-mmu-keys-modality` 已 archive；发射 key 与消费端同实例（`mmu_keys<std::uint64_t>`, 模板参数保持 raw）；CH_MEM 下 `get(PADDR_VALID)` 解析为 `ch_uint<1>&`; producer 未注册新 stage 名（只有 `declare_substage` + `at_stage("fetch"/"memory", EARLY)`）。参照锚 `tests/mmu/test_mmu_keys_modality.cpp` (`[keys]`) ✅
- [x] **B.3** (2026-10-22, **1.5d**) **实装 `MMUPluginChmem`（TLB-only 收窄版）** — 文件 `ip/mmu/plugins/MMUPlugin_chmem.h`（新）。交付内容：
  - 8 项**全相联** `ch_reg` TLB + 组合查找（8 路 tag 比较 → OR 归约 → select 链 → `page | page_off`）
  - `satp` 的 `ch_reg` 初值 + **Bare 隔离**：门控读 **`satp` 寄存器的 MODE 位（bit31）**，不是编译期 `sv_mode_` 常量 ⇒ 后续 `csr_write_satp` 接线时模式可改。`encode_satp(mode, satp_value)` 在 ctor 编初值（`Sv32` + `satp_value==0` ⇒ `0x8000_0000`，与 TLM `RiscvMMUPlugin` ctor 对齐）。⚠ 初值必须**同时**走 ctor init 与 next 值 `<<=`：`Simulator::reset()` 会把 reg 打成 `rst_val`（⇒ 0），只写 ctor init 的版本会静默恒 Bare（变异 D 已实证）
  - `ifetch` / `loadstore` 两个 lookup 端口共享同一 TLB
  - **顺序契约**：`declare_substage("fetch","tlb_lookup_ifetch")` / `("memory","tlb_lookup_loadstore")`（只建 node）+ `at_stage("fetch"/"memory", `Phase::EARLY`)` 写节点；**未**注册新 stage 名（ADR-040 v3.0 §10.6）
  - `bind_ifetch_vaddr` / `bind_loadstore_vaddr`（输入端走插件句柄，ADR-040 §10.6）+ 未绑定时惰性 0
  - ⚠ **偏离记录**：TLB 组织用**全相联**而非原计划的「8 项直接映射」——真实 TLB 即全相联，且组合 DAG 里全相联只需比较+OR+select（直接映射反而要用 ch 信号做 index mux）。entry 数 8 不变 ⇒ Phase D 估算口径不变。
- [x] **B.4** (2026-10-22, **1.0d**) **`test_mmu_chmem_basic` 单元测试 — 7 case / 24 assertions** — 文件 `tests/mmu/test_mmu_chmem_basic.cpp`（新）+ 已加入 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES`。case 表：
  | case | 判据 |
  |---|---|
  | `mmu_chmem_tlb_hit_translates_page` | 命中 ⇒ 非恒等翻译（`PADDR = entry.paddr[31:12] \| vaddr[11:0]`）+ `EXCEPTION_CODE=0` + `PTW_ACTIVE=0` |
  | `mmu_chmem_tlb_miss_keeps_paddr_invalid` | miss ⇒ `PADDR_VALID=0` **且** `EXCEPTION_CODE=0`（miss ≠ fault） |
  | `mmu_chmem_bare_mode_is_isolated` | `SvMode::Bare` + 有命中项 ⇒ 仍不翻译 |
  | `mmu_chmem_invalid_entry_does_not_translate` | `entry.valid=0` 而 tag/paddr 都对 ⇒ 不命中（变异对照锚） |
  | `mmu_chmem_loadstore_port_shares_tlb` | 两端口共享同一 TLB |
  | `mmu_chmem_unbound_ports_are_inert` | 未绑定时惰性 0，无悬空句柄 |
  | `mmu_chmem_payload_is_live_node` | 逐拍翻转 ≥4 次（常量折叠实现 ⇒ 0 翻转 ⇒ RED） |
  **变异对照 4/4 被抓**（驱动脚本 `/tmp/opencode/mutate_b4.py`，证据 `/tmp/opencode/red-evidence/mmu-chmem-phase-b-B4-mutation.txt`；每次单点替换 → 重建 → 单跑 → 回退，回退后确认仍 7/7 全绿）：

  | # | 变异 | RED case |
  |---|---|---|
  | A | 去掉 `entry_valid` 参与命中判定（只比 tag） | `invalid_entry_does_not_translate` |
  | B | `PADDR_VALID` 折成 elaboration 期常量 1 | **5 case**（miss / bare / invalid-entry / unbound / live-node） |
  | C | `sv_active` 折成常量 `true`（取消 Bare 门控） | `bare_mode_is_isolated` |
  | D | 去掉 `satp` 的 next 值（只留 ctor init） | **3 case**（hit / loadstore / live-node） |

  变异 D 不是假想：**它是我第一版实现的真实 bug** —— 夹具调了 `Simulator::reset()`，而 `reset()` 把每个 `ch_reg` 打成 `rst_val`（ctor 未给 ⇒ `nullptr` ⇒ 0），清掉了 ctor init ⇒ `satp.MODE` 恒 0 ⇒ 恒 Bare（先例注释见 `tests/cpu/test_cpu_chmem_vendored_elf.cpp:156`）。修法 = satp 初值同时走 ctor init（无 reset 的宿主）与 next 值 `<<=`（有 reset 的宿主），见 `MMUPlugin_chmem.h:176-183`。
- [x] **B.5** (2026-10-22, **0.5d**) **Phase B 集成回归** — 判据 = **failing-set 逐条相等**（非"全绿"）：
  - TLM `chipforge_tests`：**481 case / 1 failed**（`test_7stage_riscv.cpp:124`，baseline-known，不修）＝ baseline 相等
  - CH_MEM `chipforge_tests_chmem`：**125 case / 19661 assertions / All passed**（10m19s）；baseline `118 / 19637` ⇒ **+7 case / +24 assertions 全部来自 B.4**，0 回归
  - 5 架构 gate 全 PASS：`verify_adr` / `verify_plugin_decision` / `check_plugin_portability` **13/13**（含 Check 13 payload key 模式化）/ `doc_link_check` / `roadmap-r9-check`
  - **期间 `MMUPluginChmem` 未注册进 `cpu_factory_chmem`**（注册是 C.4 范围）✅

### Phase B 推迟项 → 新 change `chmem-ptw-walk`（未立项，待路线确认）

- [ ] **B.2'** `RiscvMMUPluginChmem` 派生适配器（`ip/cpu/plugins/mmu_chmem.h`）— `csr_write_satp` / `sfence_vma` / `mmu_exit` 闭包 + 3 substage。**前置**：本文件（`MMUPluginChmem` 基类）
- [ ] **B.3'** PTW 页表 walk — 前置重构：从 `PtWalkFsmPlugin::create_fsm()` 抽取自由函数 `build_pt_walk_fsm(...)`（返回值/节点结构），插件保留为薄包装（`tests/cpu/test_mmu_ptw_fsm_chmem.cpp` 零改动）
- [ ] **B.3''** TLB miss ⇒ walk 触发 + refill + fault 语义（`EXCEPTION_CODE` 12/13/15）+ fetch stall 协议（依赖 CtrlLink 有状态 halt, capability `ctrllink-continuous-handle` 已就位）
- [ ] **B.4'** `chmem-ptw-walk` 测试：PTW fault (unit 级) / sv32 非恒等页表向量（真走 walk, 非预载 TLB）
- [ ] **B.3'''** `MEM_ADDR` 源接线（改父节点读 `keys<ch_uint<32>,32>::MEM_ADDR`）或由 C.4 绑 DMEM 地址信号 —— 二选一需在 C.4 前定


## Phase C wiring (Sprint 4, 2026-11-30 ~ 2026-12-15, 5d)

- [ ] **C.1** mmu_exception_handler_chmem CPU_EXCEPTION_CODE wiring — combinational `mmufault_clear = (CPU_EXCEPTION_CODE != 0)` (2d)
- [ ] **C.4** cpu_factory_chmem fprintf 删除 + MMUPluginChmem 注册到 PipeBuilder (2d)
- [ ] **C.5** hazard_chmem halt_when() wiring — 解 CtrlLink gap (1d)

## Phase C.6 集成测试 (Sprint 5, 2026-12-15 ~ 2026-12-22, 3d)

- [ ] **C.6** `test_cpu_chmem_mmu_pipeline` 集成测试 — 真 sv32 PTE ELF + plant_identity_page_table + tohost=1 (3d)

## Phase D 验证与回归 (Sprint 6, 2026-12-22 ~ 2027-01-13, 5d, 与 Phase B/C 跟踪同步)

> ⚠ **路线 C 对 Phase D 的连带影响（2026-10-22 记录，D.1 启动前必须裁决）**: D.1 / D.2 的「真 sv32 翻转」原设计走"页表在内存里由 PTW 走"路线。PTW 已推迟到 `chmem-ptw-walk` ⇒ 本 change 内 D.1/D.2 要拿到真翻译，只有两条路：
> **(i)** 等 `chmem-ptw-walk` 落地后再开 D.1（Phase D 顺延）；**(ii)** 给 `MMUPluginChmem` 加 **elaboration 期 TLB 初始装载**（ctor 传 entry 初值，而非只能靠 `<<=` next 值），让 Verilog/Verilator 路径也能带预载 TLB 跑 sv32 翻译。
> 注意现有 `entry_*` 的 `<<=` 通道主要服务 C++ Simulator 单测（无条件 next 赋值 ⇒ Verilog 侧会生成 `always_ff r <= const`，PTW 接线后必须换成条件写）；`ch_reg` 的 **ctor init** 才会进 Verilog 的寄存器初值。**(ii) 是新增 API，属 D 的范围，不在本 change 的 Phase B 交付内。**

- [ ] **D.1** `test_mmu_bare_plumbing_verilator` TEST_CASE 4 sv32 e2e flip (3d) — **依赖上面的裁决**
- [ ] **D.2** `test_cpu_l1_mmu_demo` TEST_CASE 7 真 sv32 翻转 (3d) — **依赖上面的裁决**
- [ ] **D.3** `build_sv32_pte.S` vendor (1d)
- [ ] **D.4** `sv32-baseline-matrix.csv` — 5 ELF × median cycle × mode=sv32_mmu (2d)

## Phase D + E 收官 (Sprint 7, 2027-01-13 ~ 2027-01-24, 2d)

- [ ] **D.5** Architecture Gate Final — 4 个脚本 0 失败
- [ ] **D.6** Regression Final Check — `tools/run_chipforge_tests.sh --all`
- [ ] **E.1** AGENTS.md workaround 移除 + `[v0.11.0 follow-up]` 标记
- [ ] **E.2** `openspec/specs/verilator-mmu-bare-plumbing/spec.md` 新增 Requirement "TEST_CASE 4"
- [ ] **E.3** `openspec/specs/cpu-l1-mmu-demo/spec.md` 新增 Requirement "TEST_CASE 7"
- [ ] **E.4** `openspec change validate mmu-chmem-phase-b-c-launch` 0 error
- [ ] **E.5** `CHANGELOG.md` v0.11.0 段新增条目
- [ ] **E.6** `openspec archive mmu-chmem-phase-b-c-launch -y` + 同步归档 `mmu-chmem-pipeline-integration` (因 Phase 2 done)

---

## F7 re-evaluate 任务 (mmu-chmem Phase C 启动期执行, 与 source change R11.x 一致)

> 详见 `mmu-chmem-pipeline-integration/tasks.md` line 519-548 (R11.1/R11.2/R11.3 段)

- [x] **R11.1** (2026-10-22) **trap PC 跳转可行性重新评估** —— 裁决与证据见 §Phase 3.5 的 **3.5.1**。要点：插入点**不是** NORMAL select 覆写，而是 `ibus_chmem.h:171-185` 的 `update_pc`（branch 阶段，`pc_reg` 唯一驱动者）；memory < branch ⇒ 同拍组合 redirect 可行、零 bubble；ECALL/EBREAK/MRET 部分实装（独立 change `csr-chmem-trap-pc-jump` ≈1.5d），MMU-fault 部分 defer
- [ ] **R11.2** 批量更新 R11 表述 (7 处文件: HANDOFF.md + proposal.md + research-v0.11.0-prep.md + **`bundles/csr_bundles.h:26`** + **`openspec/specs/csr-chmem-zicsr/spec.md:69`** — 后两处由 R11.1 结论新增，内容见 3.5.1 ④)
- [ ] **R11.3** D3-A 多 driver 仲裁扩展点 (csr_chmem.h writeback 闭包 mcause/mepc 仲裁 → 3 driver)

---

**总计**: ≈ 30 tasks, ≈ 26d, ≈ 5 周 (Sprint 2-5 + Sprint 6-7)
**触发前置**: pre_flight 4 项全部 [x] 才能启动 Phase B
**不阻塞**: pre-existing baseline 退化 (Phase 6d.7 owner)

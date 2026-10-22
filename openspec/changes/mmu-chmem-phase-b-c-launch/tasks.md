# Tasks — mmu-chmem-phase-b-c-launch (Sprint 2-5 follow-up placeholder)

> **状态 (2026-10-17)**: 🟡 占位 (placeholder), 由 `mmu-chmem-pipeline-integration` Phase 2 done (2026-10-17) 抽出, 接管 Sprint 2-5 (2026-10-23 ~ 2026-11-30) + Sprint 6-7 (2026-12-01 ~ 2026-12-22) 实施窗口。
>
> **本 tasks.md 仅为元数据 anchor**, 不动 code/test/CMakeLists.txt/design.md。所有 task 描述详见源 change `openspec/changes/mmu-chmem-pipeline-integration/tasks.md` (583 行) 的 Phase B/C/D/E 段落。本文件用极简 [ ] 占位等 Sprint 2-5 rdd-builder P1 plan gen + rdd-planner 阶段细化。

---

## pre_flight (Sprint 1 启动期必做)

> **2026-10-22 双审重构**: 原 4 项 pre_flight 中, #2 (`fetch-stall-protocol-v0.2`) 与 #3 (`bp-btb-coordination-3items`) **移出 pre_flight** —— #2 的 owner (wave5-bp-btb) 已于 2026-10-19 S3 决策 defer 到 v1.0.0 ⇒ **Sprint 2-5 窗口内无人可交付**; 且 `fetch_stall_requirements_v0.11.0.md` §1.5 (BLOCKER-B5, `:74-82`) 明写 *"必须先定义 ch_bool 类型的 PTW_ACTIVE 信号（Phase B 实装），然后才能 stall"* ⇒ **fetch-stall v0.2 反向依赖 Phase B**, 放在 Phase B 的 pre_flight 里是**逻辑倒挂**。二者合并为 **Phase 3.5 slot** (见下), 挂载点 = **C.4 入口条件**。
> **剩 2 项 pre_flight 均有 owner + 交付物 + 窗口 + 挂载点, 全部可满足**。

- [ ] **R11.1 重审**: trap PC 跳转可行性 (per `csr-chmem-writeback-complete` archive 2026-10-15 后, R11 已失效, 重审是否在 Phase C 实装) — **owner: mfc-chmem / 窗口: Sprint 3 后段 / 挂载点: Phase 3.5 Task 3.5.1 (0.5d)**
- [ ] **CtrlLink API gap (FP-4) 解锁**: `hazard_chmem.h:152-154` 注释承认, 实装 `halt_when(ch_bool)` toggle — **owner: framework / 窗口: Sprint 2 早期 / 挂载点: openspec change `ctrl-link-chmem-reg-handle` (stateful halt 通道 A1/A2; A4 信号驱动改造改挂本 change 的 C.5)**

## Phase 3.5 (Sprint 3 后段, 2026-11-06 ~ 2026-11-30, 3.5d)

> **由 pre_flight #2 + #3 重构而来** —— 挂载点 = **C.4 入口条件** (C.2/C.3 已落地不需要它; 真正需要它的是 C.4 注册激活 + C.6 集成测试)。

- [ ] **3.5.1** R11.1 trap PC 跳转可行性重审 (0.5d) — 见上 pre_flight
- [ ] **3.5.2** bp-btb 收窄 2 项 (1d) — 去掉 `pc_reg mux` 的 **BP 分支** (`feat-bp-btb-gshare` 已 defer 到 v1.0.0), 保留 **stall 语义** + **fetch stage at_stage 顺序**; owner 改 mmu-chmem team (= requirements doc BLOCKER-B1 + B2)
- [ ] **3.5.3** fetch-stall v0.2 protocol 起草 (3d) — 解 5 BLOCKER + 7 SHOULD_FIX, 产出 `fetch_stall_protocol_v0.11.0.md`, 过 critic 二轮 review (BLOCKER-B5 由 Phase B 的 B.3 交付 ch_bool PTW_ACTIVE)

## Phase B (Sprint 2-3, 2026-10-23 ~ 2026-11-06, **15d**)

> **估时口径 (2026-10-22 双审)**: B.2 3d + B.3 5d + B.4 2d + B.5 5d = **15d**。`.rddf/plans/mmu-chmem-pipeline-integration.md` 原写 10d **整段丢失 B.5 的 5d 集成回归**, 已回改。**B.1 跳过** (Oracle D1-B 收窄版锁定)。
> **路径与 key 裁决**: CH_MEM 新文件落 `ip/mmu/plugins/MMUPlugin_chmem.h` (**非** `ip/mmu/tlm/` — 后者同时逃出 `check_plugin_portability.sh` Check 2 与 Check 7 扫描范围 = CI 盲区); **不新建 bundles key 文件**, 复用 `ip/mmu/tlm/mmu_keys.h` 的 `mmu_keys<std::uint64_t>` 既有静态实例 (PayloadStore 按**指针身份**索引 ⇒ key 不同 ⇒ 消费端 `has()` 恒 false ⇒ 静默回退 Bare ⇒ **测试全绿但 sv32 从未生效**)。

- [ ] **B.0** (2026-10-22 新增, 0.2d) **Payload key 复用确认** — `MMUPlugin_chmem` 发射用的 key 必须与 `ibus_chmem.h:121` / `dmem_chmem.h:142` 消费的 `MmuKeys` 是**同一静态实例** (`mmu_keys<std::uint64_t>`, 指针相等)。阻塞全部后续 task
- [ ] **B.2** 实装 `RiscvMMUPlugin_chmem` — elaboration-time satp ch_reg 初值 + at_stage 闭包 (3d) — 文件 `ip/cpu/plugins/mmu_chmem.h` (新)
- [ ] **B.3** 实装 `MMUPlugin_chmem` 收窄版 — 单级 ch_mem TLB (8 项直接映射) + 复用 PtWalkFsmPlugin (5d) — 文件 `ip/mmu/plugins/MMUPlugin_chmem.h` (新)
- [ ] **B.4** `test_mmu_chmem_basic` 单元测试 — **5 case**: TLB hit / TLB miss / PTW fault (unit 级) / sv32 模式 (非恒等页表向量) / Bare 隔离 (2d) — 文件 `tests/mmu/test_mmu_chmem_basic.cpp` (新) + **手动加入 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES`**
- [ ] **B.5** Phase B 集成回归 — chipforge_tests_chmem 全套 (5d) — **期间 `MMUPluginChmem` 不得注册进 `cpu_factory_chmem`** (注册是 C.4 范围)

## Phase C wiring (Sprint 4, 2026-11-30 ~ 2026-12-15, 5d)

- [ ] **C.1** mmu_exception_handler_chmem CPU_EXCEPTION_CODE wiring — combinational `mmufault_clear = (CPU_EXCEPTION_CODE != 0)` (2d)
- [ ] **C.4** cpu_factory_chmem fprintf 删除 + MMUPluginChmem 注册到 PipeBuilder (2d)
- [ ] **C.5** hazard_chmem halt_when() wiring — 解 CtrlLink gap (1d)

## Phase C.6 集成测试 (Sprint 5, 2026-12-15 ~ 2026-12-22, 3d)

- [ ] **C.6** `test_cpu_chmem_mmu_pipeline` 集成测试 — 真 sv32 PTE ELF + plant_identity_page_table + tohost=1 (3d)

## Phase D 验证与回归 (Sprint 6, 2026-12-22 ~ 2027-01-13, 5d, 与 Phase B/C 跟踪同步)

- [ ] **D.1** `test_mmu_bare_plumbing_verilator` TEST_CASE 4 sv32 e2e flip (3d)
- [ ] **D.2** `test_cpu_l1_mmu_demo` TEST_CASE 7 真 sv32 翻转 (3d)
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

- [ ] **R11.1** trap PC 跳转可行性重新评估 (combinational network → NORMAL select 覆写 PC 节点)
- [ ] **R11.2** 批量更新 R11 表述 (7 处文件: HANDOFF.md + proposal.md + research-v0.11.0-prep.md)
- [ ] **R11.3** D3-A 多 driver 仲裁扩展点 (csr_chmem.h writeback 闭包 mcause/mepc 仲裁 → 3 driver)

---

**总计**: ≈ 30 tasks, ≈ 26d, ≈ 5 周 (Sprint 2-5 + Sprint 6-7)
**触发前置**: pre_flight 4 项全部 [x] 才能启动 Phase B
**不阻塞**: pre-existing baseline 退化 (Phase 6d.7 owner)

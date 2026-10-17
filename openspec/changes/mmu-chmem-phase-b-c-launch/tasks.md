# Tasks — mmu-chmem-phase-b-c-launch (Sprint 2-5 follow-up placeholder)

> **状态 (2026-10-17)**: 🟡 占位 (placeholder), 由 `mmu-chmem-pipeline-integration` Phase 2 done (2026-10-17) 抽出, 接管 Sprint 2-5 (2026-10-23 ~ 2026-11-30) + Sprint 6-7 (2026-12-01 ~ 2026-12-22) 实施窗口。
>
> **本 tasks.md 仅为元数据 anchor**, 不动 code/test/CMakeLists.txt/design.md。所有 task 描述详见源 change `openspec/changes/mmu-chmem-pipeline-integration/tasks.md` (583 行) 的 Phase B/C/D/E 段落。本文件用极简 [ ] 占位等 Sprint 2-5 rdd-builder P1 plan gen + rdd-planner 阶段细化。

---

## pre_flight (Sprint 1 启动期必做)

- [ ] **R11.1 重审**: trap PC 跳转可行性 (per `csr-chmem-writeback-complete` archive 2026-10-15 后, R11 已失效, 重审是否在 Phase C 实装)
- [ ] **fetch-stall-protocol-v0.2 起草**: 解 critic 5 BLOCKERS (B1-B5 在 `fetch_stall_requirements_v0.11.0.md §B`)
- [ ] **bp-btb-coordination-3items 协商**: pc_reg mux 优先级 / stall 语义 / fetch stage at_stage 顺序 (与 `wave5-bp-btb` owner)
- [ ] **CtrlLink API gap (FP-4) 解锁**: `hazard_chmem.h:152-154` 注释承认, 实装 `halt_when(ch_bool)` toggle

## Phase B (Sprint 2-3, 2026-10-23 ~ 2026-11-06, 10d)

- [ ] **B.2** 实装 `RiscvMMUPlugin_chmem` — elaboration-time satp ch_reg 初值 + at_stage 闭包 (3d)
- [ ] **B.3** 实装 `MMUPlugin_chmem` 收窄版 — 单级 ch_mem TLB (8 项直接映射) + 复用 PtWalkFsmPlugin (5d)
- [ ] **B.4** `test_mmu_chmem_basic` 单元测试 — TLB hit / TLB miss / PTW fault 3 case (2d)
- [ ] **B.5** Phase B 集成回归 — chipforge_tests_chmem 全套 (5d)

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

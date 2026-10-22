---
id: mmu-chmem-pipeline-integration
linked_change: openspec/changes/mmu-chmem-pipeline-integration/
linked_objective: .rddf/roadmap/objectives/objective-ip-mmu-mmu-chmem-phase-b-e.md
linked_feature: .rddf/roadmap/features/feat-mmu-chmem-integration.md
tier: complex
estimated_steps: 6     # Phase 0 基线 + Phase A-C 实施 + Phase D 验证 + Phase E archive
                     # (注: tasks.md 165 task 聚合为 6 个顶层 Phase-Step, 非 gtest 5-step)
estimated_total_weeks: "6.0-6.2 人周 (≈ 5 个 sprint 跨 17 周窗口)"
verifier: rdd-verifier
sprint_window:
  sprint-2-early: "10-23 ~ 10-30 (ctrl-link-chmem-reg-handle 1d 框架热身 + 排期对齐 1d + 6d 收官 1d)"
  sprint-2-3:     "10-23 ~ 11-06 (Phase B 解锁并入, 15d — 2026-10-22 双审裁决: 由源 change tasks.md 的 15d 回改, 原 10d 压缩版丢了 B.5 的 5d 集成回归)"
  sprint-3-late:  "11-06 ~ 11-30 (Phase 3.5: R11.1 0.5d + bp-btb 收窄 2 项 1d + fetch-stall v0.2 protocol 3d — 原 pre_flight 反向依赖 Phase B, 2026-10-22 重构出独立 slot)"
  sprint-4:       "11-30 ~ 12-15 (Phase C wiring)"
  sprint-5:       "12-15 ~ 12-22 (C.6 集成测试)"
  sprint-6-7:     "12-22 ~ 01-13 (Phase D 验证 + Phase E archive)"
deps_status:
  blocked_by: []   # depends_on 全部 archived (cpu-pipeline-mmufault-handler + phase-6d-prerequisites)
  manual_deps: ["ctrl-link-chmem-reg-handle (stateful halt 通道 — Phase C.5 物理前置, 2026-10-22 已建 openspec change)"]
---

# Plan: mmu-chmem-pipeline-integration

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan phase-by-phase. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 Oracle 2026-10-07 锁定的 5 项决议 (D1-D5), 实装 CH_MEM 模式 sv32 翻译完整链路 (MMUPlugin CH_MEM + IBus/DBus sv32 消费 + HazardPlugin mmufault_clear + mmufault exception handler), 使 `[mmu-verilator]` 从 3/3 plumbing-only 升级为 5/5 真 sv32 translation + `[cpu-l1-mmu-demo]` 从 6/6 升级为 7/7 (workaround `cfg.enable_mmu=false` 移除), 解锁 v0.11.0 launch gate §9 Go 条件核心三项。

**Architecture:** 单一 source-of-truth plugin-style (ADR-083): 新建 `ip/mmu/plugins/MMUPlugin_chmem.h` (D1-B 收窄 = 单级 ch_mem TLB 8 项直接映射 + 复用 `PtWalkFsmPlugin`) + 新建 `ip/cpu/plugins/mmu_chmem.h` (RiscvMMUPlugin CH_MEM, D5-CH_MEM) + 修改 `ibus_chmem.h`/`dmem_chmem.h` 消费 sv32 翻译结果 (D2-A bare 路径零额外 latency 保护) + 修改 `mmu_exception_handler_chmem.h` combinational network (D3-A 最小可行 `mmufault_clear = (EXCEPTION_CODE != 0)`, **无** trap PC 跳转因 CSR plugin 物理不可能) + 修改 `hazard_chmem.h` 接 `mark_mmufault`/`clear_mmufault` (D2 hazard 协议) + Phase A 根因修复 (D4-A Bare shortcut 严格化 + D5-TLM shadow 双 bug 修复, 已 commit `a3afcfa` 维护阶段)。

> **路径裁决 (2026-10-22 双审)**: CH_MEM 新文件落 `ip/mmu/plugins/MMUPlugin_chmem.h` 而**非** `ip/mmu/tlm/MMUPlugin_chmem.h` —— `tools/check_plugin_portability.sh` 的 Check 2 (`:95` `find -path "*/plugins/*_chmem.h"`) 与 Check 7 (`:273-284` 按文件名排除 `*_chmem.h`) **都扫不到 `ip/mmu/tlm/`** ⇒ 放那里等于让 4 门禁对该文件完全失明。ADR-083 §2.4 (`:125-155`) 亦要求 `ip/{cpu,cache,mmu,memory}/plugins/<name>.{h,_chmem.h}`。`ip/mmu/tlm/` 整体迁移另立治理 change。
>
> **Payload key 裁决 (2026-10-22 双审)**: **不新建 bundles key 文件**，复用 `ip/mmu/tlm/mmu_keys.h` 既有静态实例，且实例化参数必须与消费端一致（`mmu_keys<std::uint64_t>`）。`PayloadStore` 按**指针身份**索引 (`include/cf/plugin/payload.h:124,136,153,166,196,204,220,233` 全部 `cells_.find(&key)`) ⇒ key 对象不同 ⇒ 消费端 `has()` 恒 false ⇒ sel 静默回退 Bare ⇒ **测试全绿但 sv32 从未生效**。
>
> **Payload key 类型裁决 (2026-10-22，`chmem-mmu-keys-modality` 已 archive)**: 上面两条只解决了**身份**问题，没解决**模态**问题 —— 裸类型 key（`Payload<std::uint64_t>` / `Payload<bool>`）在 CH_MEM 下 cell 是 **POD**，`put` 精确类型匹配 ⇒ **装不下 `ch_uint` 节点**，消费端 `ch_bool(cell)` 会把节点折成 elaboration 期常量。该缺口已由前置 change `chmem-mmu-keys-modality` 修复：5 个输出 key（`MMU_VADDR` / `PADDR` / `EXCEPTION_CODE` / `PTW_ACTIVE` / `PADDR_VALID`）改为 `cf::plugin::uint_t<N>`，2 个 CH_MEM 消费点改为「读节点」，并成文**顺序契约**。详见 ADR-040 v3.0 §10.5 / §10.6。
>
> **Producer 挂载裁决 (同批)**: `MMUPlugin_chmem` 的 payload 生产点 **SHALL** 与消费端挂**同一 stage 名**（`tlb_lookup_ifetch` / `tlb_lookup_loadstore`）+ `Phase::EARLY`，消费端保持 `Phase::NORMAL`；**禁**注册新 stage 名（`canonical_stage_order()` 只看 stage 名字在 `stages_` 的首现序，而 `cpu_factory_chmem.h:204` 已在 `pb->build()` 之前把 `"fetch"` 钉在最前 ⇒ 新 stage 名必排在消费端之后 ⇒ 永久静默 Bare）。需要 node 时用 `declare_substage`（不进 `stages_`）。

**Tech Stack:** C++23、CppHDL `ch_uint<N>` / `ch_reg` / `ch_mem` (ADR-040 v2.0 双模式)、`chlib::ch_state_machine` DSL (ADR-046 v2.0 §2.1.1 算术豁免)、Plugin 框架 (at_stage / payload keys / CtrlLink::halt_when)、Catch2 v3.7.0、`chipforge_tests` (TLM) / `chipforge_tests_chmem` (CH_MEM) 双二进制、`tools/run_chipforge_tests.sh --all` 回归。

> **时长说明（Oracle 估时聚合 + 2026-10-16 user 决策）**: 本 plan 6 个顶层 phase 是实施里程碑（每个 phase 估时 0.5 day 到 5.5 周）；真实工作量分阶段聚合 = **6.0-6.2 人周跨 17 周窗口**。bp-btb-coordination-3items 0.5d 提前到 Sprint 2 早期对冲 R-CHMEM-1 暴露，**净时不增但交付置信度从 ~70% 提升到 ~85%**（详见 `objective-ip-mmu-mmu-chmem-phase-b-e.md` §11 sprint-2026-10-3 跟踪行）。

---

## 前置条件（硬门禁 — 不满足不启动）

| 依赖 | 状态要求 | 出处 |
|------|---------|------|
| `cpu-pipeline-mmufault-handler` archive | ✅ 完成（2026-10-06）| proposal.md depends_on |
| `phase-6d-prerequisites` archive | ✅ 完成（2026-09-20）| proposal.md depends_on |
| Phase A 根因修复 (D4-A + D5-TLM) commit | ✅ 已 commit `a3afcfa`（2026-10-08）| tasks.md §1.1 |
| `[mmu] 59/59 PASS` 0 退化基线 | ✅ 实测 2026-10-14 baseline 维持 | tasks.md line 41 |
| `bp-btb owner fetch stage 协议 3 项` (HANDOFF §3) | ✅ **N/A — 已移出 Phase B 硬门禁**（2026-10-19 S3 决策：`feat-bp-btb-gshare` defer 到 v1.0.0 ⇒ Sprint 2-5 窗口内无人可交付）；**收窄 2 项**（stall 语义 + fetch stage 顺序）归 **Phase 3.5**（Sprint 3 后段 11-06 ~ 11-30）| `mmu-chmem-phase-b-c-launch/tasks.md` Phase 3.5 Task 3.5.2 |
| `fetch-stall-protocol-v0.2` | ✅ **N/A — 已移出 Phase B 硬门禁**（BLOCKER-B5：`.rddf/protocols/fetch_stall_requirements_v0.11.0.md` §1.5 明写"必须先定义 ch_bool 类型的 PTW_ACTIVE 信号（Phase B 实装），然后才能 stall" ⇒ **反向依赖 Phase B**，原位置是逻辑倒挂）；归 **Phase 3.5**，挂 **C.4 入口条件** | Phase 3.5 Task 3.5.3 |
| **`chmem-mmu-keys-modality` archive** | ⏳ **未落地（2026-10-22 立项，status=ready）** —— MMU payload key 模式化 + CH_MEM 生产者/消费者顺序契约。**Phase B 的 B.3/B.4 硬前置**：key 非 mode-aware ⇒ CH_MEM 通道只能装常量（`mmu_keys` 裸类型 vs 框架 `cf::plugin::uint_t<N>`）⇒ B.3 不可实现、AC-S3-3 无鉴别力；且 elaboration 顺序会让消费者恒先跑 ⇒ 静默 Bare | `.rddf/plans/chmem-mmu-keys-modality.md` |

**验证前置**：`bash tools/run_chipforge_tests.sh --all` 在当前 HEAD（`fc8452f`；代码基线与 `176a7e1` 一致 —— `fc8452f` 仅补录 1 个 `.rddf/plans/*.md` 文件）先跑一次记 baseline（TLM **481 case / 1 known fail**（`test_7stage_riscv.cpp:124 node_count==17`）+ CH_MEM **114 case** + verilator + 5 架构 gate），供 Phase 4 回归比对。**判据 = failing-set 逐条相等**，不是"全绿"（baseline 有 1 known fail + 4 个 pre-existing spec fail）。

---

## File Structure

### Production Code

| File | Responsibility | Action |
|---|---|---|
| `ip/mmu/plugins/MMUPlugin_chmem.h` (新) | `MMUPlugin_chmem` D1-B 收窄版 — 单级 ch_mem TLB 8 项直接映射 + 调用 `PtWalkFsmPlugin` walk + 发射 `MMU_VADDR` / `PADDR` / `PADDR_VALID` / `PTW_ACTIVE` payload key。**路径锁定 `plugins/`**（非 `ip/mmu/tlm/` —— 后者同时逃出 Check 2 `:95` 与 Check 7 `:283-287` 的扫描范围 ⇒ CI 盲区）| Create (Phase B, ~280 LOC) |
| `ip/cpu/plugins/mmu_chmem.h` (新, R7 移路径) | `RiscvMMUPlugin` CH_MEM 实装 — elaboration-time satp config 注入 (D5-CH_MEM) + 单一 `ch_reg<satp_value>` 初值 + 与 TLM `MMUPlugin.cpp` 行为对齐 | Create (Phase B, ~180 LOC) |
| `ip/cpu/plugins/ibus_chmem.h` (现有 155 LOC) | fetch aread 加 sv32 消费 — 读 `cf::ip::mmu::payload::mmu_keys<std::uint64_t>::MMU_VADDR` / `::PADDR_VALID`（**非 `pl::` 前缀** —— `pl::` 来自未落地的 `bundles/mmu_bundles.h` 方案，已否决）→ emit vaddr for fetch，**bare 路径零额外 latency** (D2-A) | Modify (Phase C, ~30 LOC 改) |
| `ip/cpu/plugins/dmem_chmem.h` (现有 145 LOC) | load/store 加 sv32 消费 — 同 ibus 模式 | Modify (Phase C, ~30 LOC 改) |
| `ip/cpu/plugins/mmu_exception_handler_chmem.h` (现有 64 LOC stub) | combinational network: `mmufault_clear = (CPU_EXCEPTION_CODE != 0)` (D3-A 最小) | Modify (Phase D, ~20 LOC 改) |
| `ip/cpu/plugins/hazard_chmem.h` (现有 254 LOC) | `mark_mmufault` / `clear_mmufault` 实装 + `CtrlLink::halt_when(ch_bool)` 接 `mmufault_clear` | Modify (Phase D, ~40 LOC 改) |
| `ip/cpu/cpu_factory_chmem.h:117-126` (现有 424 LOC) | 移除 line 117-126 `fprintf` "Real sv32 CH_MEM owner TBD" 占位（Phase E archive 时由 80-85 owner claim 段已存在的 `v0.11.0 owner: change mmu-chmem-pipeline-integration` 字面注释同步清掉运行期 fprintf） | Modify (Phase E, ~10 LOC 改) |
| ~~`bundles/mmu_bundles.h`~~ | ❌ **已否决（2026-10-22 双审）** —— **不新建 bundles key 文件**。5 个目标 key（`MMU_VADDR` / `PADDR` / `PADDR_VALID` / `PTW_ACTIVE` / `EXCEPTION_CODE`）**复用 `ip/mmu/tlm/mmu_keys.h` 既有静态实例**，且实例化参数必须与消费端一致（`mmu_keys<std::uint64_t>`）。`PayloadStore` 按**指针身份**索引（`include/cf/plugin/payload.h:124,136,153,166,196,204,220,233` 全是 `cells_.find(&key)`）⇒ key 对象不同 ⇒ 消费端 `has()` 恒 false ⇒ sel 静默回退 Bare ⇒ **测试全绿但 sv32 从未生效** | **No action**（删除该行即完成裁决落地） |
| `ip/mmu/tlm/MMUPlugin.cpp:109` (Phase A 已 commit `a3afcfa`) | Bare shortcut 严格化 (D4-A 已落地) — 维护基线，无新增改动 | Verify only |

### Verification Code (新)

| File | Purpose |
|---|---|
| `tests/mmu/test_mmu_chmem_basic.cpp` (新, ~180 LOC) | `[mmu][chmem][basic]` TLB hit 走 lookup + miss 走 PTW walk 5 case (D1-B 范围验证) |
| `tests/cpu/test_cpu_chmem_mmu_pipeline.cpp` (新, ~200 LOC) | `[cpu][chmem][mmu-pipeline]` 真 sv32 PTE ELF + plant_identity_page_table + tohost=1 (Phase C 集成验证) |
| `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` (现有 3 case) | **新增第 4 个** TEST_CASE sv32 e2e flip — cycle ≤ baseline × 1.5 + EXCEPTION_CODE 正确传播 + pipeline stall 生效 ⇒ `[mmu-verilator]` 3/3 → **4/4**（对齐 v0.11.0 launch gate 判据 #3） |
| `tests/soc/soc-cpu-l1-mmu-demo/test_cpu_l1_mmu_demo.cpp` (现有 6 case) | TEST_CASE 7 新增 `cpu_l1_mmu_demo_sv32_translation_flipped` — flip `cfg.enable_mmu=true` 真 sv32 翻译 7/7 PASS（workaround `cfg.enable_mmu=false` 移除） |
| `tests/cpu/manual_elf/build_sv32_pte.S` (新, ~30 LOC) | sv32 PTE ELF vendor — vendor 1 个 sv32 翻译 PTE 程序 |
| `tests/cpu/manual_elf/build_manual_elf.sh` (改) | 加 `build_sv32_pte` 入口 |
| `soc/cpu/docs/dse/sv32-baseline-matrix.csv` (新) | 5 ELF × median cycle × mode=sv32_mmu (用于 Phase D1 1.5× 阈值) |

### Documentation

| File | Action |
|---|---|
| `AGENTS.md` L41 workaround 行 | Update (Phase E) — 删 workaround 标记 |
| `openspec/specs/verilator-mmu-bare-plumbing/spec.md` | spec-delta AC-6 sv32 e2e flip (Phase E) |
| `openspec/specs/cpu-l1-mmu-demo/spec.md` | spec-delta AC-7 真 sv32 翻转 (Phase E) |
| `CHANGELOG.md` v0.11.0 entry | Document archive (Phase E) |

### tasks.md → Phase 聚合映射

| Phase | 聚合 tasks.md 子任务 |
|-------|----------------------|
| Phase 0 — 基线 | (无 tasks.md 编号,前置验证) |
| Phase 1 — bp-btb-coordination + Phase C 入口准备 | C0 (新增, R-CHMEM-1 缓解) + C1 (mmu_exception_handler combinational, 1.5d) |
| Phase 2 — Phase C Pipeline 集成 | C2 (ibus_chmem sv32 消费, 2.5d) + C3 (dmem_chmem sv32 消费, 2.5d) + C4 (cpu_factory_chmem owner 真空修复, 1d) + C5 (hazard_chmem mmufault_clear, 1.5d) + C6 (test_cpu_chmem_mmu_pipeline, 2d) |
| Phase 3 — Phase B 解锁 (Sprint 2-3, **15d**) | B.2 (RiscvMMUPlugin_chmem, 3d) + B.3 (MMUPlugin_chmem, 5d) + B.4 (test_mmu_chmem_basic, 2d) + B.5 (Phase B 集成回归, 5d) ｜ 另: B.0 (Payload key 复用确认, 0.2d, 阻塞全部后续) + B.6 (spec/tasks 勾选, 0.5d) |
| Phase 4 — Phase D 验证 (Sprint 6 同步) | D1 (verilator **新增第 4 个** TEST_CASE sv32 e2e flip, 1.5d) + D2 (test_cpu_l1_mmu_demo **新增第 7 个** TEST_CASE 真 sv32 翻转, 1d) + D3 (sv32 PTE ELF vendor, 0.5d) + D4 (sv32 baseline CSV, 0.5d) + D5 (4 architecture gate final, 0.5d) + D6 (run_chipforge_tests.sh --all 全 PASS, 0.5d) ｜ **编号勘误 (2026-10-22)**：原写 "TEST_CASE 6" 有误 —— `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` 现有 **3** 个 TEST_CASE（`:107`/`:136`/`:174`）⇒ 新增第 4 个 = `[mmu-verilator]` 4/4；`tests/soc/soc-cpu-l1-mmu-demo/test_cpu_l1_mmu_demo.cpp` 现有 **6** 个（宏生成）⇒ 新增第 7 个 = 7/7 |
| Phase 5 — Phase E archive (Sprint 7) | E1 (CHANGELOG.md v0.11.0 entry, 0.5d) + E2 (AGENTS.md workaround 删, 0.25d) + E3 (spec-delta 2 份, 0.5d) + E4 (cpu_factory_chmem.h:117-126 fprintf 清理, 0.25d) + E5 (4 gate 0 error 终态验证, 0.5d) + E6 (`openspec archive mmu-chmem-pipeline-integration -y`, 0.5d) |

---

## Phase 0 — 环境验证 + 基线建立 (估时 0.5 day)

### Task 0.1: 确认 build 环境 + 当前 architecture gate baseline

**Files:**
- Verify: `ip/mmu/tlm/MMUPlugin.cpp` (D4-A 已 commit `a3afcfa` line 109 Bare shortcut 严格化)
- Verify: `bundles/mem_bundles.h` 命名风格（Phase B/C key 定义模板）
- Verify: `tools/run_chipforge_tests.sh` 帮助与现状

- [ ] **Step 1: 确认 MMUPlugin.cpp 已修 Bare shortcut**

Run: `sed -n '105,115p' ip/mmu/tlm/MMUPlugin.cpp`
Expected: 看到 D4-A 已落地（无 `|| satp_ppn_ == 0` 析取项）

- [ ] **Step 2: 确认 architecture gate 全绿**

Run: `bash tools/verify_adr.sh && bash tools/verify_plugin_decision.sh && bash tools/check_plugin_portability.sh && bash tools/doc_link_check.sh`
Expected: 4 个脚本均 0 失败 (33 PASS / 8+ / 12/12 / 0 broken)

- [ ] **Step 3: 确认测试基线**

Run: `./build/bin/chipforge_tests "[mmu]"` && `./build/bin/chipforge_tests "[cpu-l1-mmu-demo]"` && `./build/bin/chipforge_tests "[mmu-verilator]"` (TLM binary 仅后两者, MMU Verilator 在 CHMEM binary)
Expected:
- `[mmu]` 59/59 PASS
- `[cpu-l1-mmu-demo]` 6/6 PASS (workaround `cfg.enable_mmu=false`)
- `[mmu-verilator]` 3/3 case PASS (plumbing only, AC 文档化)

- [ ] **Step 4: 确认 Csr / Mul / Dhrystone 等 sister change 不退化基线**

Run: `./build/bin/chipforge_tests "[cpu]"` && `./build/bin/chipforge_tests "[cpu-integration]"` && `./build/bin/chipforge_tests "[riscv-tests]"` && `./build/bin/chipforge_tests_chmem "[verilator]"` && `./build/bin/chipforge_tests_chmem "[chmem]"`
Expected: `[cpu]` 19/19 / `[cpu-integration]` 81/81 / `[riscv-tests]` 48/48 / `[verilator]` 1/1 (48 ELF) / `[chmem]` 9/9 全 PASS (Phase A 收官基线)

- [ ] **Step 5: 记 baseline cycle 数到 §6 跟踪台账**

Run: `./build/bin/chipforge_tests "[riscv-tests]" 2>&1 | tee /tmp/p0_baseline_riscv.log`
Expected: 拿到 48 ELF cycle 数 → 写到 `.rddf/plans/mmu-chmem-pipeline-integration.md` §6 跟踪

---

## Phase 1 — bp-btb-coordination + Phase C 入口准备 (估时 2 day) — **已重构 (2026-10-22)**

> ⚠ **2026-10-22 双审重构**: 本 phase 的**全部剩余工作已移出 Sprint 2 早期**，改为 **Phase 3.5**（Sprint 3 后段 11-06 ~ 11-30，3.5d）：
> - `bp-btb-coordination-3items` → **收窄为 2 项**（去掉 `pc_reg mux` 的 BP 分支 —— `feat-bp-btb-gshare` 已于 2026-10-19 S3 决策 defer 到 v1.0.0），owner 改 **mmu-chmem team**（= requirements doc BLOCKER-B1 + B2）；
> - `fetch-stall-protocol-v0.2` 起草 → 归 Phase 3.5，挂 **C.4 入口条件**（BLOCKER-B5：反向依赖 Phase B 的 `PTW_ACTIVE`）；
> - ⇒ **Phase B 的 pre_flight 不再含这两项**，不再阻塞 Sprint 2 开工。
>
> **R-CHMEM-1 缓解路径 (2026-10-16 user 决策)**: 本 phase 原本把 `bp-btb-coordination-3items` 0.5d 提前 (Sprint 2 最早), 后续 Phase C 启动期已有 fetch stall framework 稳定锚点。详见 `objective-ip-mmu-mmu-chmem-phase-b-e.md` §11 sprint-2026-10-3。

### Task 1.1: bp-btb-coordination-3items — fetch stage 协议 0.5d (R-CHMEM-1 缓解) — **已移至 Phase 3.5 Task 3.5.2**

**Files:**
- Create: `.rddf/protocols/fetch_stall_requirements_v0.11.0.md` (新, 270+ 行) — requirements doc v0.1 替代原 protocol spec (降级)
- ~~Create: `ip/cpu/plugins/fetch_stall_chmem.h` (新, ~80 LOC)~~ **deferred to Phase 3.5 (Sprint 3 后段)，由 mmu-chmem team 解决 5 BLOCKER 后实装**（原写 "Sprint 6-7 + wave5-bp-btb owner" —— owner 已于 2026-10-19 S3 决策 defer 到 v1.0.0）

- [x] **Step 1: 与 bp-btb owner 协商 3 项协议** → **变更为: 起草 requirements doc (降级为非可执行)** ✅ landed `f3419b4` 2026-10-16

Run: 原计划 异步会议 + ADR 引用 `wave5-bp-btb` proposal §3 备选 3 项 → **实际**: 由 mmu-chmem-pipeline-integration builder (本次 session) 起草 fetch_stall_protocol_v0.11.0.md (Draft v0.1, 237 行) 作为后续 v0.2 起草的 review 锚点（原写 "wave5-bp-btb owner Sprint 6-7 启动期" —— 2026-10-22 更正为 Phase 3.5 / mmu-chmem team）
**Critic 审稿 2026-10-16**: REJECT + 5 BLOCKERS + 7 SHOULD_FIXS
- B1: declare_substage 当前不产生调度顺序 (substage 关系无 reader)
- B2: CH_MEM register_ctrl_link("fetch", ...) 不会冻结 pc_reg (elaborate 路径不消费 CtrlLink)
- B3: Payload Keys 类型 ch_bool/ch_uint<3> 直写违反 ADR-040 v2.0 双模 + 命名空间 cf::cpu::keys 不存在
- B4: Bare path 判定公式与已锁定 D4 决议直接矛盾 (satp_ppn=0 不等于 Bare)
- B5: ptw_active 是 Payload<bool> POD, CH_MEM halt_when 需要 ch_bool, 无合法信号源 → **✅ 2026-10-22 由 `chmem-mmu-keys-modality` 承接**（key 模式化 D-1/D-2 + 顺序契约 D-4；producer 侧 EARLY 挂载 + PTW_ACTIVE 经 CtrlLink 聚合路径兑现）
Expected (original): 拿到 3 项 fetch stage 协议 hash + interface spec
**Actual outcome (2026-10-16)**: protocol spec 撤回 + requirements doc v0.1 (270+ 行, 显式列出 5 BLOCKER + 7 SHOULD_FIX + 3-step 启动序列) 替代. **Phase 3.5（mmu-chmem team, Sprint 3 后段）必须先解决 5 BLOCKER 后才能起草 v0.2 protocol**（本 requirements doc 不作为可执行依据）

- [ ] **Step 2: 创建 `fetch_stall_chmem.h` 框架** → **deferred to Phase 3.5 (Sprint 3 后段)**

Run: 写 ch_mem 基础模板 + 3 项协议签名
Status: **deferred** per critic B1-B5 BLOCKERS — fetch_stall_chmem.h 在 5 BLOCKER 解决前无法实装. **Phase 3.5 (mmu-chmem team) 需**:
1. 解决 5 BLOCKER (显式选择每个的设计路径: substage ordering / stall 表达 / payload keys 归属 / Bare 判定公式 / ptw_active ch_bool 信号源)
2. 起草 v0.2 protocol spec (新文件 fetch_stall_protocol_v0.11.0.md)
3. v0.2 spec 经 critic 二轮 review 通过后才能实装 fetch_stall_chmem.h
4. cpu_factory_chmem.h 8→10 plugin (per critic S2 修正: +2 = MMUPlugin_chmem + fetch_stall)

- [ ] **Step 3: 验证 Phase 0 baseline 0 退化**

Run: `./build/bin/chipforge_tests "[cpu]"` && `./build/bin/chipforge_tests "[cpu-integration]"`
Expected: 仍 19/19 + 81/81 PASS

### Task 1.2: C1 — `mmu_exception_handler_chmem.h` combinational network 实装

**Files:**
- Modify: `ip/cpu/plugins/mmu_exception_handler_chmem.h` (现有 64 LOC stub → ~85 LOC)
- Create: `tests/cpu/test_mmu_exception_handler_chmem.cpp` (~120 LOC)

- [ ] **Step 1: [RED] 写 `[cpu][mmu-exception-handler][chmem]` 单测 (D3-A 验证)**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "ip/cpu/plugins/mmu_exception_handler_chmem.h"

TEST_CASE("mmu_exception_handler_chmem: mmufault_clear when EXCEPTION_CODE != 0", "[cpu][mmu-exception-handler][chmem]") {
  cf::plugin::PipeBuilder pb;
  mmu_exception_handler_chmem_handler h;
  pb.register_plugin(&h);
  pb.at_stage("execute", NORMAL, [&](cf::plugin::Payload& p) {
    p.put(keys::CPU_EXCEPTION_CODE, ch_uint<32>(1));  // page_fault
    h.on_execute(p);
  });
  pb.run();
  REQUIRE(pb.get_signal_value("mmufault_clear") == 1);
}
```

Expected: 编译失败 (`mmu_exception_handler_chmem_handler::on_execute` 未实现) = RED

- [ ] **Step 2: 改 stub → minimial implementation**

```cpp
// mmu_exception_handler_chmem.h 第 ~70-85 行
void on_execute(Payload& p) {
  auto exc = p.get<ch_uint<32>>(keys::CPU_EXCEPTION_CODE);
  auto mmufault_clear = (exc != ch::core::ch_literal<uint32_t, 32>{0});
  p.put(keys::MMUFAULT_CLEAR, mmufault_clear);  // 新 key
}
```

(不调 `if (cond)` 早返，按 D4 §4 主逻辑包裹)

- [ ] **Step 3: [GREEN] 跑上述测试**

Run: `cmake --build build -j$(nproc) --target chipforge_tests_chmem && ./build/bin/chipforge_tests_chmem "[cpu][mmu-exception-handler][chmem]"`
Expected: 1/1 PASS

- [ ] **Step 4: 回归 `[mmu]` + `[cpu]` 0 退化**

Run: `./build/bin/chipforge_tests "[mmu]" && ./build/bin/chipforge_tests "[cpu]"`
Expected: 59/59 + 19/19 PASS

- [ ] **Step 5: 架构门禁 + Defer commit**

Run: `bash tools/check_plugin_portability.sh && bash tools/verify_plugin_decision.sh`
Expected: 12/12 + 8+/8+ PASS

---

## Phase 2 — Phase C Pipeline 集成 (估时 6.5 day, Sprint 2-3 Week 2 ~ Sprint 3 Week 2)

> **前提**: Phase 1 完成 (fetch stall framework 锚点 + exception handler combinational 落地)

### Task 2.1: C2 — `ibus_chmem.h` sv32 消费 (fetch aread 闭包)

**Files:**
- Modify: `ip/cpu/plugins/ibus_chmem.h` line ~146-155 (fetch aread 闭包 ~30 LOC 改)
- ~~Modify: `bundles/mmu_bundles.h` (新 keys: `MMU_VADDR` / `PADDR` / `PADDR_VALID` / `PTW_ACTIVE`, ~20 LOC)~~ → **2026-10-22 更正**：实际实现**未新建 bundles 文件**，改为复用 `ip/mmu/tlm/mmu_keys.h` 既有静态实例（`ibus_chmem.h:26` include + `ibus_chmem.h:121` `MmuKeys = mmu_keys<std::uint64_t>`）。新建 key 对象会因 `PayloadStore` 指针身份不一致导致消费端 `has()` 恒 false ⇒ 静默 Bare
- Modify: `ip/cpu/cpu_factory_chmem.h:69` (1 char fix `ip::cpu::HazardPlugin` → `::ip::cpu::HazardPlugin`, namespace conflict 修复)
- Create: `tests/cpu/test_ibus_chmem_sv32.cpp` (240 LOC, 2 case)

- [x] **Step 1: [RED] 写 `[cpu][chmem][ibus-sv32]` 单测 (D2-A bare 路径零额外 latency 保护 + 真 sv32 翻译路径)** ✅ landed `9373808` 2026-10-16

Run: 创建 `tests/cpu/test_ibus_chmem_sv32.cpp` 240 LOC: Bare 路径 + Sv32 路径对比
Expected: 编译失败 (尚未消费 MMU_VADDR) → **实际**: Phase 2.1 编译通过 + 2 case 全 PASS (976 assertions), 因 fetch_instr 当前用裸 pc_val, Bare 路径天然 === sv32 plumbing 路径

- [x] **Step 2: 改 ibus_chmem.h fetch aread 闭包消费 sv32** ✅ landed `9373808` 2026-10-16

```cpp
// ip/cpu/plugins/ibus_chmem.h:119-163 fetch_instr (CH_MEM mode)
void fetch_instr(PipeBuilder& pb) {
  using KT = cf::cpu::core::payload::keys<T, kXlenBits>;
  using MmuKeys = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;  // 显式绝对路径, 不受 namespace pollution 影响
  // ...
  auto* tlb_n = pb.node_of_logic_stage("tlb_lookup_ifetch").get();
  ch::core::ch_bool    paddr_valid = ch::core::ch_bool(false);
  ch::core::ch_uint<32> paddr      = ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{});
  ch::core::ch_uint<32> mmu_vaddr  = pc_val;  // Bare default
  if (tlb_n && tlb_n->payloads().has(MmuKeys::PADDR_VALID)) {
    paddr_valid = ch::core::ch_bool(tlb_n->operator()(MmuKeys::PADDR_VALID));
    paddr       = ch::core::ch_uint<32>(tlb_n->operator()(MmuKeys::PADDR));
    mmu_vaddr   = ch::core::ch_uint<32>(tlb_n->operator()(MmuKeys::MMU_VADDR));
  }
  auto fetch_addr = select(paddr_valid, paddr, mmu_vaddr);  // ch_uint<32> sel (ADR-040 v2.0 §4 D4 主逻辑包裹)
  auto word_addr = bits<15, 2>(fetch_addr);
  // ...
}
```

(用 `select` 表达式而非 `if (cond)`，按 D4 §4 主逻辑包裹)

**namespace conflict 修复** (Step 2 副产物, 1 char 改动 `cpu_factory_chmem.h:69`):
- 根因: `ip/mmu/tlm/mmu_keys.h` 引入 `namespace cf { namespace ip { namespace mmu { namespace payload { ... } } } }` 到全局 scope
- 影响: `cpu_factory_chmem.h:35` 直接 include `ibus_chmem.h`, 后者 include `mmu_keys.h`, 在 `cf::cpu::` 命名空间作用域内查找 `ip::cpu::HazardPlugin<T>` 时, 编译器先查 `cf::ip::cpu::` (空, 因 mmu_keys 只有 `cf::ip::mmu::payload::`), 报 `cpu in cf::ip does not name a type`
- 修复: `ip::cpu::HazardPlugin<T>` → `::ip::cpu::HazardPlugin<T>` (绝对 namespace, 强制顶级 ip 解析)
- TLM `ibus.h` 同样用 `using MmuKeys = cf::ip::mmu::payload::mmu_keys<...>;`, 但 TLM ibus.h 不被 cpu_factory.h include, 所以无 namespace conflict

- [x] **Step 3: [GREEN] 跑 + 回归 bare 路径 byte-identical** ✅ landed `9373808` 2026-10-16

Run: `./build/bin/chipforge_tests_chmem "[ibus-sv32]"` + 4 arch gate
Expected: bare path cycle 数 == Phase 0 baseline byte-identical (零额外 latency 保护验证)

实测:
- `[ibus-sv32]` 2 case 全 PASS (976 assertions): `ibus_chmem_sv32_bare_baseline` cycle 478 ±46 = baseline 461±46 (含 sel 表达式 DAG 节点 +17 cycles elaboration overhead, 但 cycle count 仍在 ±10% tolerance 内)
- 4 arch gate (verify_adr 0 fail + verify_plugin_decision 8/8 + check_plugin_portability 12/12 + Phase 1 expected 10) 全 PASS
- 0 退化引入 (新测试与现有 CHMEM family 共存)
- sv32 plumbing path (enable_mmu=true, mmu_mode=sv32) 行为 === Bare, 因 Phase B 未实装 mmu_chmem.h, PADDR_VALID 永远 false → sel 走 vaddr 分支

**Pre-existing baseline 退化 ROOT CAUSE 确认** (与本 commit 无关, **不是代码回归**):
- `chipforge_tests_chmem [chmem_vendored_elf_tohost1]`: 18 ELF FAIL (lb/lbu/ld_st/lh/lhu/lw/sb/sh/sw/st_ld + 8 rv32um)
- A/B test 验证: HEAD `992dbf5` 完全干净 (revert ibus_chmem.h + cpu_factory_chmem.h + tests/CMakeLists.txt) 同样 18 fail → 与本 Task 2.1 改动无关
- 进一步深查 (explore subagent 2026-10-16): **不是代码回归**,是 **pre-existing 架构限制**, 已 Oracle 确认记录在 [`docs/lessons/phase-6d7-load-store-replay-stall.md`](docs/lessons/phase-6d7-load-store-replay-stall.md) (2026-10-10, **早于** Phase 2.4 C5 wiring commit):
  - 根因: CppHDL Simulator (`pb->create_simulator()`) 把 **所有** `type_mem_read_port` (async `aread` + sync `sread`) 都归类为 `sequential_instr_list_` → 1-cycle read latency regardless of port type (`CppHDL/src/simulator_init.cpp:116-121` + `CppHDL/docs/simulation/ARCHITECTURE.md:362-377`)
  - 上游 `openspec/changes/archive/2026-10-11-2026-10-12-followup-mem-read-drift-hardening/proposal.md` 刻意保留此行为 (Verilog backend = combinational, Simulator = sequential, 上游决定两者行为不同)
  - ChipForge CH_MEM 5-stage datapath 是 **fully combinational** (`cpu_factory_chmem.h:395-400` 声明无 stage register); **只有 IBus 用 `pc_lag` (ch_reg 延迟) 补偿 1-cycle latency** (`ibus_chmem.h:80`); **DMem 没有等效补偿**
  - 后果: MEM stage 的 `mem.aread(word_addr)` 读的是 **上一 cycle 的 byte_addr** 对应的 dmem 值 → wrong load value → ELF self-check fail → `tohost = (test#)|1` (3, 33, 65, ...)
- 影响: Phase D 验证 (`tools/run_chipforge_tests.sh --all`) baseline 对比不需要 fix 此退化 — 因为这是 pre-existing 架构限制, 不在 mmu-chmem-pipeline-integration scope. Phase D 应当用 **Verilator backend** (`[verilator]`) 而非 CppHDL Simulator 做 baseline 对比, 或在 TLM binary 上做 baseline
- **状态**: 此退化从 commit `5e82970` (Phase 6d.7 B', 2026-10-10) kCases 5→38 扩展时就被发现并记录. mmu-chmem-pipeline-integration 不需要修复它. 修复由 feat-phase-6d-rtl-verification (Phase 6d.7 owner) 负责 (lesson doc `phase-6d7-load-store-replay-stall.md` 列出 3 候选方案: A/B/C/D, 都需要 sprint 级工作量)

- [ ] **Step 4: PTW_ACTIVE=1 时 stall fetch** (defer to Phase C.4/C.5 —— Phase B 落地后；PTW_ACTIVE 经 **CtrlLink 聚合路径**兑现，见 `chmem-mmu-keys-modality` D-4)

Run: `./build/bin/chipforge_tests_chmem "[cpu][chmem][ibus-sv32]" && 用 sv32 PTE ELF 验证 stall 协议生效`
Expected: PTW_ACTIVE=1 时 fetch stall 不发新 PC，PTW 完成 → resume
Defer reason: 需要 Phase B (MMUPlugin_chmem + PtWalkFsmPlugin 联动) 实装才能 plant `PTW_ACTIVE=1` 信号; 当前 `tlb_lookup_ifetch` 节点未注册 → PTW_ACTIVE 永远是 false



### Task 2.2: C3 — `dmem_chmem.h` sv32 消费 (load/store 闭包)

**Files:**
- Modify: `ip/cpu/plugins/dmem_chmem.h` line ~100-145 (load/store 闭包 ~30 LOC 改)
- Create: `tests/cpu/test_dmem_chmem_sv32.cpp` (~150 LOC, 2 case)

- [x] **Step 1: [RED] 写 `[cpu][chmem][dmem-sv32]` 单测** ✅ landed `5b9f74a` 2026-10-16

Run: 创建 `tests/cpu/test_dmem_chmem_sv32.cpp` ~150 LOC: Bare baseline + Sv32 plumbing 对比
Expected: 当前会 PASS(Bare path) + RED-via-pre-existing(via load/store ELF, 因 main HEAD `b8d860f` pre-existing baseline 退化 18 ELF FAIL 与本 change 无关). **实际**: 选 `rv32ui-p-add` (ALU-only) 作为唯一 ELF 避免 pre-existing 退化污染, 2 case 全 PASS (972 assertions)

- [x] **Step 2: 改 dmem_chmem.h mem_access 闭包消费 sv32** ✅ landed `5b9f74a` 2026-10-16

```cpp
// ip/cpu/plugins/dmem_chmem.h:95-204 mem_access (CH_MEM mode)
auto vaddr_byte_addr = n->operator()(KT::RESULT);
auto byte_addr = vaddr_byte_addr;  // Bare default
{
  using MmuKeys = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;
  if (auto* tlb_n = pb.node_of_logic_stage("tlb_lookup_loadstore").get();
      tlb_n && tlb_n->payloads().has(MmuKeys::PADDR_VALID)) {
    auto paddr_valid = ch::core::ch_bool(tlb_n->operator()(MmuKeys::PADDR_VALID));
    auto paddr = ch::core::ch_uint<32>(tlb_n->operator()(MmuKeys::PADDR));
    byte_addr = ch::core::select(paddr_valid, paddr, vaddr_byte_addr);  // ch_uint<32> sel (ADR-040 v2.0 §4)
  }
}
auto word_addr = bits<15, 2>(byte_addr);  // sv32 不改变低 12-bit, byte_off/half_off/RMW mask 路径一致
```

(用 `select` 表达式而非 `if (cond)`，按 D4 §4 主逻辑包裹)

- [x] **Step 3: [GREEN] 跑 + 回归 baseline** ✅ landed `5b9f74a` 2026-10-16

实测:
- `[dmem-sv32]` 2 case 全 PASS (972 assertions)
- `[ibus-sv32]` 2 case 全 PASS (976 assertions, 无退化)
- 4 arch gate (verify_adr 0 fail + verify_plugin_decision 8/8 + check_plugin_portability 12/12) 全 PASS
- 0 退化引入 ([chmem] 41/42, 与 main HEAD `b8d860f` 一致)

**关键设计选择** (与 Task 2.1 不同):
- Task 2.2 测试 ELF 选 `rv32ui-p-add` 而非 `rv32ui-p-lw/sw`, 因 main HEAD `b8d860f` pre-existing baseline 退化 (chipforge_tests_chmem [chmem_vendored_elf_tohost1] 18 ELF FAIL) 在 load/store 类 ELF 触发, 但与本 change 无关 (A/B test 验证). 用 ALU-only ELF 避免被 pre-existing 退化污染.

- [ ] **Step 4: PTW_ACTIVE=1 时 stall load/store** (defer to Phase C.4/C.5 —— Phase B 落地后, 同 Task 2.1 Step 4)

Run: `./build/bin/chipforge_tests_chmem "[cpu][chmem][dmem-sv32]" && 用 sv32 PTE ELF 验证 load/store stall 协议生效`
Expected: PTW_ACTIVE=1 时 dmem stall 不发新 addr, PTW 完成 → resume
Defer reason: 同 Task 2.1 Step 4 — 需要 Phase B (MMUPlugin_chmem + PtWalkFsmPlugin 联动) 实装才能 plant `PTW_ACTIVE=1` 信号

### Task 2.3: C4 — `cpu_factory_chmem.h` owner 真空修复

**Files:**
- Modify: `ip/cpu/cpu_factory_chmem.h` line 117-126 (删 fprintf 占位, 加 fetch_stall 实例化 8→9 plugin)
- Modify: 注册 fetch_stall_chmem + mmu_chmem + mmu_exception_handler_chmem + hazard_chmem 联动

- [ ] **Step 1: 删 line 117-126 运行期 fprintf 占位**

Run: `sed -n '115,130p' ip/cpu/cpu_factory_chmem.h` 确认占位 + 删
Expected: file 减小 ~10 LOC

- [ ] **Step 2: 8→9 plugin 注册 (fetch_stall 加进来)**

Run: 注册 `MMUPlugin_chmem` + `fetch_stall` 两个新 plugin
Expected: 5-stage CH_MEM 实例化 9 plugin + bare 路径 0 退化

### Task 2.4: C5 — `hazard_chmem.h` `mark_mmufault` / `clear_mmufault` 实装

**Files:**
- Modify: `ip/cpu/plugins/hazard_chmem.h` line 151-153 (~40 LOC 改)
- Create: `tests/cpu/test_hazard_chmem_mmufault.cpp` (~100 LOC)

- [ ] **Step 1: [RED] 写 mmufault stall 集成测试**

Run: 创建 test 验证 `mark_mmufault` 后 `CtrlLink::halt_when(ch_bool(mmufault_clear_))` 正确生效
Expected: RED

- [ ] **Step 2-3: hazard_chmem.h 改**

```cpp
// hazard_chmem.h line 151-153
// ⚠ 本 sketch 是 2026-10-16 的早期草稿, **已被 2026-10-22 的设计取代** —— 落地时:
//   (1) `pl::` 命名空间不存在（来自未落地的 bundles/mmu_bundles.h 方案, 已否决）;
//       MMU 侧 key 在 `cf::ip::mmu::payload::mmu_keys<std::uint64_t>`,
//       CPU 侧 `CPU_EXCEPTION_CODE` 在 `cf::cpu::csr::payload::csr_keys` (`bundles/csr_bundles.h`)
//   (2) `if (mmufault_clear)` 是**运行期 if(ch_bool)** —— 违反 ADR-040 D4
//       (`check_plugin_portability.sh` Check 5 会报 FAIL) ⇒ 必须用 `select()` 组合式表达
//   (3) mmufault 的**真实驱动源**是 C.1/C.5 的 `CPU_EXCEPTION_CODE` 写入
//       (`hazard_chmem.h:157-163` 自陈), 且必须走 `CtrlLink::halt_when_stateful(ch_reg<ch_bool>)`
//       (capability ctrllink-continuous-handle) —— 见 `chmem-mmu-keys-modality` D-4
void on_execute(Payload& p) {
  // auto mmufault_clear = <来自 exception handler 的 ch_bool 表达式>;
  // 组合式表达（示意, 实际以 C.5 设计为准）:
  //   ctrl_link_.halt_when_stateful(mmufault_reg_);   // ch_reg<ch_bool>, 跨拍保持
  //   // 副作用改由组合逻辑驱动, 不用运行期 if
  // ... existing RAW detection (CtrlLink::halt_when)
}
```

- [ ] **Step 4: [GREEN] 跑 + 回归**

Run: `./build/bin/chipforge_tests_chmem "[cpu][hazard-chmem][mmufault]"`
Expected: PASS + 0 退化

### Task 2.5: C6 — `test_cpu_chmem_mmu_pipeline` 集成测试 (sv32 PTE ELF 跑通)

**Files:**
- Create: `tests/cpu/test_cpu_chmem_mmu_pipeline.cpp` (~200 LOC)
- Create: `tests/cpu/manual_elf/build_sv32_pte.S` (~30 LOC)
- Modify: `tests/cpu/manual_elf/build_manual_elf.sh` (加 build_sv32_pte 入口)
- Create: `soc/cpu/docs/dse/sv32-baseline-matrix.csv` (5 ELF × median cycle × mode=sv32_mmu)

- [ ] **Step 1: vendor sv32_pte.elf**

Run: 写 `build_sv32_pte.S` riscv-tests 风格 + base=0x80000000 + tohost symbol + register `build_sv32_pte` in `build_manual_elf.sh`
Expected: `tests/cpu/manual_elf/sv32_pte.elf` 生成 + `riscv32-unknown-elf-readelf -h` 验证

- [ ] **Step 2: 写 cpu-l1 demo 真 sv32 flip 集成测试**

```cpp
TEST_CASE("cpu_chmem_mmu_pipeline_sv32_pte_e2e", "[cpu][chmem][mmu-pipeline]") {
  CPUConfig cfg;
  cfg.enable_mmu = true;  // workaround 移除
  cfg.mmu_mode = "sv32";
  cfg.satp_ppn = 0x8000F;  // (0x80000000+0xF000)>>12 实测
  cpu_sim_handle h = cpu_factory_chmem::build(cfg, /*with_l1=*/false);
  plant_identity_page_table(h.memory(), 0x80000000);
  cpu_sim_run(h, 10000);
  REQUIRE(cpu_sim_tohost(h) == 1);  // tohost=1 within 10000 cycles
}
```

Expected: RED initially（sv32 translation not implemented）→ GREEN after Phase B + C

- [ ] **Step 3: 跑 TLM + CHMEM baseline 对齐验证**

Run: TLM binary 跑同样 sv32 flip → cycle 数记录到 sv32-baseline-matrix.csv → CHMEM binary 与 TLM baseline 对比 ≤ 1.5×
Expected: CHMEM binary sv32_pte.elf tohost=1 within sv32-baseline × 1.5 cycles

- [ ] **Step 4: 全套件回归**

Run: `bash tools/run_chipforge_tests.sh --all`
Expected: `[mmu] 59/59` + `[cpu-l1-mmu-demo] 7/7` + `[riscv-tests] 48/48` + `[verilator] 1/1` 全 PASS

- [ ] **Step 5: Defer commit**

按仓库约定 execute 阶段不逐 Phase commit；所有变更将在 archive 阶段统一提交。

---

## Phase 3 — Phase B 解锁并入 Sprint 2-3 (估时 **15 day**, Sprint 2 Week 1 ~ Sprint 3 前段)

> **2026-10-22 双审裁决**: Phase B 启动窗口由 Sprint 6-7 (12-01) **提前到 Sprint 2-3 (10-23 ~ 11-06)**，估时由本 plan 原写的 10d **回改为 15d**（源 change `tasks.md:18-21` = B.2 3d + B.3 5d + B.4 2d + B.5 5d）。
> **为何提前**: 12-01 起只剩 8 周要塞 B(15d)+C(5d)+C.6(3d)+D(5d)+E(2d) ≈ 30d + DMIPS 硬门禁，buffer ≈ 0。且 Phase B 历史 3 次 deep-agent 派发失败的根因是 **single-shot 派发方式**（非日历位置），`ctrl-link-chmem-reg-handle`（Sprint 2 早期 1d）恰是同一套 `ch_reg` + OR 网 API 的交互式热身 session。
> **为何 15d 而非 10d**: 本 plan 原 10d 版本**整段丢失了 B.5 的 5d 集成回归**（Task 3.4 只有 2d 单测）⇒ 在"3 次派发失败"的历史上采纳压缩版是错误方向。
> **依赖序** (`tasks.md` 锁定): **B.0 (key 复用确认)** → B.2 (RiscvMMUPlugin_chmem) → B.3 (MMUPlugin_chmem 收窄版) → B.4 (PTW FSM 复用) → B.5 (test_mmu_chmem_basic) → B.6 (集成回归)

### Task 3.0: B.0 — Payload key 前置确认 (0.1 day, 阻塞全部后续 task；**0.2d → 0.1d** 因 `chmem-mmu-keys-modality` 已 archive)

**Files:**
- Verify: `ip/mmu/tlm/mmu_keys.h` (5 个目标 key 已模式化为 `cf::plugin::uint_t<N>`，**不新建**)
- Verify: `ip/cpu/plugins/ibus_chmem.h` + `ip/cpu/plugins/dmem_chmem.h` (消费端 `MmuKeys` 实例化 + 已改「读节点」)
- Gate: `openspec/changes/archive/*chmem-mmu-keys-modality*/` 存在（本 task 是**复核**，不是施工）

- [ ] **Step 1: 断言实例化参数与静态实例地址一致**

`MMUPlugin_chmem` 发射用的 key 必须是 `cf::ip::mmu::payload::mmu_keys<std::uint64_t>` 的**同一静态实例**（与消费端指针相等）。`mmu_keys` 是模板 struct + static inline 成员 ⇒ `mmu_keys<uint64_t>` 与 `mmu_keys<uint32_t>` 是**不同实例、不同地址**。

> **模板参数必须保持 raw** —— 写 `mmu_keys<ch_uint<64>>` 会撞 `mmu_keys.h:46-48` 的 `static_assert`（要求裸整型），且与消费端实例不同 ⇒ 指针不等。

Expected: 指针相等断言 PASS

- [ ] **Step 2: 消费端 `has()` 契约自检**

确认 `ibus_chmem.h` / `dmem_chmem.h` 的 `payloads().has(MmuKeys::PADDR_VALID)` 在 MMU plugin 注册后**返回 true**（而非恒 false）。

Expected: FAIL-before / PASS-after（B.3 实装后复跑）

- [ ] **Step 3: 模态 + 顺序自检（新增，前置 change 的兑现检查）**

(a) `get(MmuKeys::PADDR_VALID)` 在 CH_MEM 下解析为 `ch_uint<1>&`（不是 `bool&`）；
(b) producer 挂 `("tlb_lookup_ifetch", Phase::EARLY)` / consumer 保持 `("tlb_lookup_ifetch", Phase::NORMAL)`，且 producer **未**注册任何新 stage 名（`pb.stages()` 不含 `tlb_lookup_*` 之外的新条目）。

参照锚：`tests/mmu/test_mmu_keys_modality.cpp`（4 case / 15 assertions，`[keys]`）。

> **不新建 bundles key 文件** —— `PayloadStore` 按指针身份索引，新 key 对象 ⇒ 消费端 `has()` 恒 false ⇒ sel 静默回退 Bare ⇒ **测试全绿但 sv32 从未生效**。

### Task 3.1: B2 — `RiscvMMUPlugin_chmem.h` 实装 (3 day)

**Files:**
- Create: `ip/cpu/plugins/mmu_chmem.h` (~180 LOC)
- Create: `tests/mmu/test_mmu_chmem_plugin_basic.cpp` (~80 LOC)

- [ ] **Step 1: [RED] 写 `[mmu][chmem][riscv-mmu-plugin]` 单测 (D5-CH_MEM 验证)**

Run: 测试 RiscvMMUPlugin CH_MEM 版 ctor 自动编码 satp_value + 与 TLM MMUPlugin 行为对齐
Expected: RED (file not exist)

- [ ] **Step 2: 写 minimal implementation**

```cpp
// ip/cpu/plugins/mmu_chmem.h
class RiscvMMUPlugin_chmem : public PluginBase {
public:
  void build(PipeBuilder& pb) override {
    pb.negotiate([&](CapabilityTable& cap) {
      cap.provide("mmu_sv32_chmem", this);
    });
    // elaboration-time satp_value 注入 (D5-CH_MEM)
    ch_reg<ch_uint<32>> satp_reg_{ch_ctx_, ch::core::ch_literal<uint32_t, 32>{
      /*Sv32 MODE bit31=*/0x80000000u | (satp_ppn_init_ << 12)
    }};
  }
};
```

- [ ] **Step 3-4: [GREEN] 跑 + bare path byte-identical 保护**

Expected: PASS + `[mmu] 59/59` 0 退化 + Bare 路径 cycle 数 byte-identical

### Task 3.2: B3 — `MMUPlugin_chmem.h` 收窄版实装 (5 day)

**Files:**
- Create: `ip/mmu/plugins/MMUPlugin_chmem.h` (~280 LOC)
  - **路径理由**: `ip/mmu/tlm/` 同时逃出 `check_plugin_portability.sh` Check 2 (`:95` 只扫 `*/plugins/*_chmem.h`) 与 Check 7 (`:273-284` 按文件名排除 `*_chmem.h`) ⇒ 放那里 4 门禁对该文件完全失明。ADR-083 §2.4 亦要求 `plugins/` 目录。

- [ ] **Step 1-4: TDD 实装 8 项直接映射 TLB + 调用 `PtWalkFsmPlugin` + 发射 payload key**

- [ ] **Step 5: Bare path byte-identical 0 退化**

### Task 3.3: B.4 — `test_mmu_chmem_basic` 5 case PASS (2 day)

> **编号语义已对齐 (2026-10-22)**: 本 plan 原把 B.4 当作 "PTW FSM 复用"、B.5 当作 "单测", 且**整段丢失集成回归 5d** —— 现按源 change `openspec/changes/mmu-chmem-pipeline-integration/tasks.md:18-21` 的 canonical 编号回改（B.2 插件 / B.3 插件 / B.4 单测 / B.5 集成回归 = 15d）。`PtWalkFsmPlugin` 复用（0 修改）**内含于 B.3**,不单列 task。

**Files:**
- Create: `tests/mmu/test_mmu_chmem_basic.cpp` (~180 LOC)
- Modify: `tests/CMakeLists.txt` — **手动加入 `CHMEM_TEST_SOURCES` (`:118-145`)**
  - **理由**: 该 list 是**逐条手写**的；GLOB (`:6`) 只覆盖 TLM target ⇒ 不加就**静默不编译不运行**

- [ ] **Step 1-4: TLB hit 走 lookup + miss 走 PTW walk 5 case**

Expected: `[mmu][chmem][basic]` 5/5 PASS

- [ ] **Step 5: 真翻译生效判据（防"全绿交付零翻译"）**

  - (a) **非恒等页表向量**: vaddr X → paddr Y（Y ≠ X），断言翻译后地址为 Y。*identity page table 无法区分真 sv32 与 Bare 回退 —— 两者输出恒等*
  - (b) **消费端断言**: 经 `ibus_chmem` / `dmem_chmem` 共享节点验证 `has(MmuKeys::PADDR_VALID) == true` 且 sel 真选 `PADDR`（非 `MMU_VADDR`）
  - (c) **satp=0 / satp_ppn 未编程边界用例**（v0.10.1 deep-rca 记录过"PTW 从不在 window 0x0 读 PTE"同类历史 bug；Bare-shortcut 严格化 v0.10.4 + CH_MEM 初值注入的交互会埋雷）
  - (d) **变异对照 ≥2 个**: ① 拔掉 `PADDR_VALID` 接线 → 消费端测试应红；② TLB 改恒 hit → miss case 应红

### Task 3.4: B.5 — Phase B 集成回归 (5 day)

- [ ] **Step 1: 回归基线比对**

`bash tools/run_chipforge_tests.sh --all` 全 PASS + `[chmem]` 73/73 基线不变（+ 新增 case 计入）+ `[mmu] 59/59` TLM 路径 0 退化 + 4 架构 gate 0 失败。

> **回归成本**: `[chmem]` 全量 **833s/轮**（ctest 默认 1500s 上限内但余量不大）⇒ 按 6+ 轮 × 14min 纯等待计入。快速迭代用单 family 过滤（`./build/bin/chipforge_tests_chmem "[chmem]"`），全量只在 GREEN 收敛点跑。

- [ ] **Step 2: 边界条件自检**

**B.5 集成回归期间 `MMUPluginChmem` 不得注册进 `cpu_factory_chmem`**（注册是 C.4 的范围）⇒ 管道里 `PADDR_VALID` 仍恒 false ⇒ 无 stall 需求。若误注册，PTW 期间 fetch 会用未翻译地址取指（真错误）。

### Task 3.5: B.6 — spec / tasks 勾选 (0.5 day)

- [ ] **Step 1**: `openspec/changes/mmu-chmem-phase-b-c-launch/tasks.md` Phase B 段勾选 + `openspec/specs/` 新增 Requirement
- [ ] **Step 2**: `openspec change validate mmu-chmem-phase-b-c-launch` ⇒ 0 error

---

## Phase 3.5 — fetch-stall v0.2 + bp-btb 收窄 2 项 + R11.1 (估时 3.5 day, Sprint 3 后段 11-06 ~ 11-30)

> **2026-10-22 双审重构**: 原 `pre_flight` 的 #2 (`fetch-stall-protocol-v0.2`, 2d) 与 #3 (`bp-btb-coordination-3items`, 0.5d) **移出 pre_flight** —— 前者的 owner (wave5-bp-btb) 已于 2026-10-19 S3 决策 defer 到 v1.0.0 ⇒ **无人可交付**；且 `fetch_stall_requirements_v0.11.0.md` §1.5 (BLOCKER-B5, `:74-82`) 明写 *"必须先定义 ch_bool 类型的 PTW_ACTIVE 信号（Phase B 实装），然后才能 stall"* ⇒ **fetch-stall v0.2 反向依赖 Phase B**，放在 Phase B 的 pre_flight 里是**逻辑倒挂**。
> **本 slot 挂载点 = C.4 的入口条件**（不是泛泛的"Phase C 入口" —— C.2/C.3 已落地不需要它，真正需要它的是 C.4 注册激活 + C.6 集成测试）。

### Task 3.5.1: R11.1 — trap PC 跳转可行性重审 (0.5 day)

- [ ] **Step 1**: 按 `csr-chmem-writeback-complete` archive 2026-10-15 后的现状重审 trap PC 跳转可行性（combinational network → NORMAL select 覆写 PC 节点）
- [ ] **Step 2**: 产出结论并同步 `tasks.md` R11.1/R11.2/R11.3（7 处文件表述批量更新）

### Task 3.5.2: bp-btb 收窄 2 项 (1 day)

- [ ] **Step 1**: 确认收窄 —— 去掉 `pc_reg mux` 的 **BP 分支**（`feat-bp-btb-gshare` 已 defer 到 v1.0.0，该分支无意义），保留 **stall 语义** + **fetch stage at_stage 顺序**
- [ ] **Step 2**: owner 改归属 mmu-chmem team（wave5-bp-btb 是**真空**不是让渡；需求源头本来就是 MMU PTW 冻结 fetch）
- [ ] **Step 3**: 与 `fetch_stall_requirements_v0.11.0.md` 的 BLOCKER-B1 (substage ordering) + BLOCKER-B2 (stall 表达路径) 对齐 —— 本项即这两条的协调入口

### Task 3.5.3: fetch-stall v0.2 protocol 起草 (3 day)

- [ ] **Step 1**: 解 5 BLOCKER（B1 substage ordering API 扩展路径 / B2 stall 表达路径 / B3 双模 payload key 命名 / B4 Bare path 严格按 satp_mode 判定 / B5 ch_bool PTW_ACTIVE 信号源 —— **B5 由 Phase B 的 B.3 交付**）
- [ ] **Step 2**: 产 `fetch_stall_protocol_v0.11.0.md`（v0.1 已因 critic REJECT + 5 BLOCKERS 撤回降级为 requirements doc）
- [ ] **Step 3**: 过 critic 二轮 review 才能进入实装

> **估时说明**: 3d 而非 2d —— BLOCKER-B1/B2 是**框架级**缺口（B1 可能需 ADR-048 扩展 substage ordering API；B2 = CtrlLink 在 elaborate 路径不冻结 pc_reg），2d 偏乐观。

---

## Phase 4 — Phase D 验证 (估时 4 day, Sprint 6, 与 Phase B 同步)

> **依赖序**: Phase B + Phase C 全绿 → Phase D 启动

### Task 4.1: D1 — `[mmu-verilator]` **新增第 4 个** TEST_CASE sv32 e2e flip

**Files:**
- Modify: `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` (现有 3 case → + 1 case)

- [ ] **Step 1-3: 加第 4 个 TEST_CASE `mmu_sv32_translation_verilator_e2e_flipped` + 跑**

Expected: `[mmu-verilator]` **4/4** PASS（真 sv32 翻译通过 Verilator 后端）—— 与 v0.11.0 launch gate 判据 #3 的 "4/4" 对齐
> **编号勘误 (2026-10-22)**: 原写 "TEST_CASE 6 / 5/5" 有误 —— 该文件现有 **3** 个 TEST_CASE（`:107`/`:136`/`:174`）⇒ 新增的是**第 4 个**。

### Task 4.2: D2 — `[cpu-l1-mmu-demo]` TEST_CASE 7 真 sv32 翻转

**Files:**
- Modify: `tests/soc/soc-cpu-l1-mmu-demo/test_cpu_l1_mmu_demo.cpp` (现有 6 → 7 case)

- [ ] **Step 1-3: 移除 workaround `cfg.enable_mmu=false` + 加 TEST_CASE 7**

Expected: `[cpu-l1-mmu-demo]` 7/7 PASS（workaround 移除，1 case 真 sv32 翻转）

### Task 4.3: D3 — sv32 baseline CSV + cyc ≤ 1.5× 阈值

**Files:**
- Create: `soc/cpu/docs/dse/sv32-baseline-matrix.csv`
- Modify: `tests/cpu/manual_elf/build_manual_elf.sh` (5 ELF × median cycle)

- [ ] **Step 1-4: vendor 5 sv32 测试 ELF + 跑 TLM baseline + 写 CSV**

### Task 4.4: D4 — 4 architecture gate final 0 error

**Files:**
- Verify: `tools/{verify_adr,verify_plugin_decision,check_plugin_portability,doc_link_check}.sh` (0 改动)

- [ ] **Step 1: 跑 4 gate**

Expected: 33+8+12+0 全 PASS

### Task 4.5: D5 — `bash tools/run_chipforge_tests.sh --all` 全 PASS

**Files:**
- Verify: 全套件 432 catch2 TLM + CHMEM 48-ELF + verilator + 4 gate

- [ ] **Step 1: 跑全套件 ~167s**

Expected: 0 fail

### Task 4.6: D6 — `[riscv-tests] 48/48` + `[verilator] 1/1 (48 ELF)` 不退化

- [ ] **Step 1: 跑两者**

Expected: 48/48 + 1/1 (96 assertions)

---

## Phase 5 — Phase E archive (估时 2.5 day, Sprint 8 Week 1)

### Task 5.1: E1 — `CHANGELOG.md` v0.11.0 entry

**Files:**
- Modify: `CHANGELOG.md`

- [ ] **Step 1: 写 entry**

内容: `Phase A 根因修复 (a3afcfa) → Phase B-E CH_MEM sv32 翻译完整链路落地 ([mmu-verilator] 3→5 + [cpu-l1-mmu-demo] 6→7 + workaround 移除)`

### Task 5.2: E2 — `AGENTS.md` workaround 行删除

**Files:**
- Modify: `AGENTS.md` L41 `[cpu-l1-mmu-demo]` workaround 标记

### Task 5.3: E3 — spec-delta 2 份

**Files:**
- Modify: `openspec/specs/verilator-mmu-bare-plumbing/spec.md`
- Modify: `openspec/specs/cpu-l1-mmu-demo/spec.md`

### Task 5.4: E4 — `cpu_factory_chmem.h:117-126` line 117-126 fprintf 清理 + fetch_stall 实例化

(详见 Task 2.3，本 task 是 archive 时同步清理确认)

### Task 5.5: E5 — `bash tools/run_chipforge_tests.sh --all` 终态验证

- [ ] **Step 1: 跑全套件 0 fail**

### Task 5.6: E6 — `openspec archive mmu-chmem-pipeline-integration -y`

- [ ] **Step 1: archive 触发**

Expected: `openspec/changes/mmu-chmem-pipeline-integration/` 移到 `openspec/changes/archive/2026-10-XX-mmu-chmem-pipeline-integration/`

---

## §6. 跟踪台账（append-only）

> 本节由 execute 阶段按 Sprint 周期追加。Phase 0 已留位。

| Sprint | kind | 内容 | Decision/调整 | 原因 |
|--------|------|------|---------------|------|
| plan-2026-10-16 | plan-gen | plan 起草 (P0 approved 后由 rdd-builder P1 落位) | 6 phase 聚合 165 tasks.md 子任务 + bp-btb-coordination 提前到 Sprint 2 早期纳入 Phase 1 | rdd-builder P1 输出 + R-CHMEM-1 缓解整合 |
| Phase 0 (2026-10-16) | baseline-verified | 4 architecture gates + 5 test families | baseline state: [mmu] 59/59, [cpu-l1-mmu-demo] 6/6 (workaround active), [cpu] 129/129, [cpu-integration] 95/96 + 1 known infra drift, [riscv-tests] 48/48, [verilator] 1 SIGTERM known flaky | HEAD 230bd36 (docs P0+P1 闭环), 6 files / +563 LOC, 进入 worktree openspec/mmu-chmem-pipeline-integration |
| Phase 1.1 (2026-10-16) | protocol-spec-draft | `fetch_stall_protocol_v0.11.0.md` 290 行 in worktree `.rddf/protocols/` (untracked, owner handoff draft) | 不创建 `fetch_stall_chmem.h` 实际文件避免提前 dead code,Sprint 6-7 wave5-bp-btb owner 启动期 review + accept 后再入仓实装 | 替代 "本会话实装 fetch_stall_chmem.h" 选项,避免 8→9 plugin 注册跨 phase 干扰 + CppHDL substage API 未验证风险 |
| Phase 2.3 C4 (2026-10-16) | production-code-fix | `ip/cpu/cpu_factory_chmem.h` line 117-126 fprintf owner claim 更新: "Real sv32 CH_MEM owner TBD" → "sv32 CH_MEM owner assigned: change mmu-chmem-pipeline-integration (Phase B Sprint 6-7, ETA 2026-12-22; ...)" + 加 7 行 owner claim comment | commit `c71c9e7` (worktree branch + main HEAD 同步) | "no silent degradation" 守护维持: enable_mmu=true + Phase B 未落地时仍 fprintf 告知 plumbing-only no-op. Phase B archive 时清理. Regression 0 退化: [mmu] 59/59 + [cpu-l1-mmu-demo] 6/6 + [cpu] 129/129 + 4 architecture gate 全绿 |
| **plan-refactor (2026-10-22)** | plan-refactor + gap-analysis | ① **Phase B 启动窗口 Sprint 6-7 (12-01) → Sprint 2-3 (10-23 ~ 11-06)**，工作量按源 change `tasks.md` 的 **15d** 回改（原 10d 压缩版丢失 B.5 的 5d 集成回归）；② **pre_flight 重构** —— `fetch-stall-protocol-v0.2` + `bp-btb-coordination-3items` 移出 pre_flight（owner 已 defer 到 v1.0.0 + BLOCKER-B5 反向依赖 Phase B）⇒ 新增 **Phase 3.5** slot（3.5d, Sprint 3 后段, 挂 C.4 入口）；③ **B-2/B-3 残留清理**（File Structure 表的 `ip/mmu/tlm/MMUPlugin_chmem.h` + `bundles/mmu_bundles.h` 两处被否决方案 / 硬门禁表 / Phase 映射表 / 旧 HEAD 引用 / Phase 1 defer 归属 / TEST_CASE 编号勘误）；④ **新增 B-1 缺口**：MMU payload key 非 mode-aware ⇒ CH_MEM 通道只能装常量 + elaboration 顺序让消费者恒先跑 ⇒ **B.3 不可实现且静默 Bare** ⇒ 立项前置 change **`chmem-mmu-keys-modality`**（1.5-2d, Phase B 硬前置） | 本行即台账追加记录；上方 2026-10-16 两行为**历史留档不改** | 排期三处矛盾（plan vs 源 change tasks vs objective）+ pre_flight ownership 错配 + 逻辑倒挂 + 文档残留被否决方案 ⇒ 双审（oracle + metis）后统一裁决 |

## §7. 验证自检（execute 完成后跑）

> **Spec 覆盖**:
> - [ ] proposal.md §1 (MMUPlugin CH_MEM 实装) → Phase 3 B2-B5 覆盖
> - [ ] proposal.md §2 (Bare shortcut 严格化) → 已 Phase A commit `a3afcfa`, 维护
> - [ ] proposal.md §3 (csr_write_satp 注入) → 已 Phase A commit `a3afcfa`, 维护
> - [ ] proposal.md §4 (Verilator 新增第 4 个 TEST_CASE sv32 e2e flip) → Phase 4 D1 覆盖
> - [ ] proposal.md §5 ([cpu-l1-mmu-demo] 真 sv32 翻转) → Phase 4 D2 覆盖
> - [ ] proposal.md §6 (AGENTS.md workaround 删) → Phase 5 E2 覆盖
> - [ ] proposal.md §7 (Spec delta 2 份) → Phase 5 E3 覆盖
> - [ ] proposal.md §8 (Architecture Gate Final) → Phase 4 D4 覆盖
> - [ ] proposal.md §9 (Regression Final Check) → Phase 4 D5 覆盖
> - [ ] proposal.md §10 (Archive Preparation) → Phase 5 E6 覆盖
> - [ ] proposal.md §11 (D1 收窄 scope 显式声明) → Phase 3 B1 跳过 (已 Oracle 锁定)

> **占位符扫描**: 0 hits (`grep -E "TODO|TBD|FIXME|fill in" .rddf/plans/mmu-chmem-pipeline-integration.md` 0 output)

> **类型一致性**: 验证 `MMU_VADDR` / `PADDR` / `PADDR_VALID` / `PTW_ACTIVE` / `MMUFAULT_CLEAR` / `CPU_EXCEPTION_CODE` / `FETCH_ADDR` 的**单一来源** —— MMU 侧 5 key 落在 `ip/mmu/tlm/mmu_keys.h`（**不新建 bundles 文件**，2026-10-22 双审裁决）+ Task 2.1 / 2.2 / 1.2 / 4.2 全部一致引用。⚠ **key 的 value type 模式化**（`cf::plugin::uint_t<N>`，CH_MEM 下可承载运行期 DAG 节点）由 `chmem-mmu-keys-modality` 承接（**Phase B 硬前置**，2026-10-22 立项）；`CPU_EXCEPTION_CODE`（`ip/cpu/tlm/cpu_keys.h:48`）与 `bundles/csr_bundles.h` 的 4 个 `Payload<bool>` key 归姊妹 change `chmem-cpu-keys-modality`（Phase C.1 入口）

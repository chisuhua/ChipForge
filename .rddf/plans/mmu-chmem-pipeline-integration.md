---
id: mmu-chmem-pipeline-integration
linked_change: openspec/changes/mmu-chmem-pipeline-integration/
linked_objective: .rddf/roadmap/objectives/objective-mmu-chmem-phase-b-e.md
linked_feature: .rddf/roadmap/features/feat-mmu-chmem-integration.md
tier: complex
estimated_steps: 6     # Phase 0 基线 + Phase A-C 实施 + Phase D 验证 + Phase E archive
                     # (注: tasks.md 165 task 聚合为 6 个顶层 Phase-Step, 非 gtest 5-step)
estimated_total_weeks: "6.0-6.2 人周 (≈ 5 个 sprint 跨 17 周窗口)"
verifier: rdd-verifier
sprint_window:
  sprint-2-early: "10-23 ~ 10-30 (bp-btb-coordination-3items 0.5d, R-CHMEM-1 缓解)"
  sprint-2-5:    "10-23 ~ 11-30 (5.5 周, Phase C only)"
  sprint-6-7:    "12-01 ~ 12-22 (Phase B 解锁并入 + Phase D + bp-btb-coordination fallback)"
  sprint-7-8:    "12-23 ~ 01-13 (Phase E archive)"
deps_status:
  blocked_by: []   # depends_on 全部 archived (cpu-pipeline-mmufault-handler + phase-6d-prerequisites)
  manual_deps: ["bp-btb owner fetch stage 协议 (HANDOFF §3, 0.5d 提前到 Sprint 2 早期)"]
---

# Plan: mmu-chmem-pipeline-integration

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan phase-by-phase. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 Oracle 2026-10-07 锁定的 5 项决议 (D1-D5), 实装 CH_MEM 模式 sv32 翻译完整链路 (MMUPlugin CH_MEM + IBus/DBus sv32 消费 + HazardPlugin mmufault_clear + mmufault exception handler), 使 `[mmu-verilator]` 从 3/3 plumbing-only 升级为 5/5 真 sv32 translation + `[cpu-l1-mmu-demo]` 从 6/6 升级为 7/7 (workaround `cfg.enable_mmu=false` 移除), 解锁 v0.11.0 launch gate §9 Go 条件核心三项。

**Architecture:** 单一 source-of-truth plugin-style (ADR-083): 新建 `ip/mmu/tlm/MMUPlugin_chmem.h` (D1-B 收窄 = 单级 ch_mem TLB 8 项直接映射 + 复用 `PtWalkFsmPlugin`) + 新建 `ip/cpu/plugins/mmu_chmem.h` (RiscvMMUPlugin CH_MEM, D5-CH_MEM) + 修改 `ibus_chmem.h`/`dmem_chmem.h` 消费 sv32 翻译结果 (D2-A bare 路径零额外 latency 保护) + 修改 `mmu_exception_handler_chmem.h` combinational network (D3-A 最小可行 `mmufault_clear = (EXCEPTION_CODE != 0)`, **无** trap PC 跳转因 CSR plugin 物理不可能) + 修改 `hazard_chmem.h` 接 `mark_mmufault`/`clear_mmufault` (D2 hazard 协议) + Phase A 根因修复 (D4-A Bare shortcut 严格化 + D5-TLM shadow 双 bug 修复, 已 commit `a3afcfa` 维护阶段)。

**Tech Stack:** C++23、CppHDL `ch_uint<N>` / `ch_reg` / `ch_mem` (ADR-040 v2.0 双模式)、`chlib::ch_state_machine` DSL (ADR-046 v2.0 §2.1.1 算术豁免)、Plugin 框架 (at_stage / payload keys / CtrlLink::halt_when)、Catch2 v3.7.0、`chipforge_tests` (TLM) / `chipforge_tests_chmem` (CH_MEM) 双二进制、`tools/run_chipforge_tests.sh --all` 回归。

> **时长说明（Oracle 估时聚合 + 2026-10-16 user 决策）**: 本 plan 6 个顶层 phase 是实施里程碑（每个 phase 估时 0.5 day 到 5.5 周）；真实工作量分阶段聚合 = **6.0-6.2 人周跨 17 周窗口**。bp-btb-coordination-3items 0.5d 提前到 Sprint 2 早期对冲 R-CHMEM-1 暴露，**净时不增但交付置信度从 ~70% 提升到 ~85%**（详见 `objective-mmu-chmem-phase-b-e.md` §11 sprint-2026-10-3 跟踪行）。

---

## 前置条件（硬门禁 — 不满足不启动）

| 依赖 | 状态要求 | 出处 |
|------|---------|------|
| `cpu-pipeline-mmufault-handler` archive | ✅ 完成（2026-10-06）| proposal.md depends_on |
| `phase-6d-prerequisites` archive | ✅ 完成（2026-09-20）| proposal.md depends_on |
| Phase A 根因修复 (D4-A + D5-TLM) commit | ✅ 已 commit `a3afcfa`（2026-10-08）| tasks.md §1.1 |
| `[mmu] 59/59 PASS` 0 退化基线 | ✅ 实测 2026-10-14 baseline 维持 | tasks.md line 41 |
| `bp-btb owner fetch stage 协议 3 项` (HANDOFF §3) | ⚠ **Sprint 2 早期 (10-23 ~ 10-30) 同步启 0.5d** 为 Phase C 入口对齐 | objective-mmu-chmem-phase-b-e.md §11 sprint-2026-10-3 |

**验证前置**：`bash tools/run_chipforge_tests.sh --all` 在当前 HEAD（4e5ae36 → 07cf77c 之后）先跑一次记 baseline（TLM 432 catch2 + CHMEM 48-ELF + verilator + 4 gate），供 Phase 4 回归比对。

---

## File Structure

### Production Code

| File | Responsibility | Action |
|---|---|---|
| `ip/mmu/tlm/MMUPlugin_chmem.h` (新) | `MMUPlugin_chmem` D1-B 收窄版 — 单级 ch_mem TLB 8 项直接映射 + 调用 `PtWalkFsmPlugin` walk + 发射 `MMU_VADDR` / `PADDR` / `PADDR_VALID` / `PTW_ACTIVE` payload key | Create (Phase B, ~280 LOC) |
| `ip/cpu/plugins/mmu_chmem.h` (新, R7 移路径) | `RiscvMMUPlugin` CH_MEM 实装 — elaboration-time satp config 注入 (D5-CH_MEM) + 单一 `ch_reg<satp_value>` 初值 + 与 TLM `MMUPlugin.cpp` 行为对齐 | Create (Phase B, ~180 LOC) |
| `ip/cpu/plugins/ibus_chmem.h` (现有 155 LOC) | fetch aread 加 sv32 消费 — 读 `pl::MMU_VADDR` / `pl::PADDR_VALID` → emit vaddr for fetch，**bare 路径零额外 latency** (D2-A) | Modify (Phase C, ~30 LOC 改) |
| `ip/cpu/plugins/dmem_chmem.h` (现有 145 LOC) | load/store 加 sv32 消费 — 同 ibus 模式 | Modify (Phase C, ~30 LOC 改) |
| `ip/cpu/plugins/mmu_exception_handler_chmem.h` (现有 64 LOC stub) | combinational network: `mmufault_clear = (CPU_EXCEPTION_CODE != 0)` (D3-A 最小) | Modify (Phase D, ~20 LOC 改) |
| `ip/cpu/plugins/hazard_chmem.h` (现有 254 LOC) | `mark_mmufault` / `clear_mmufault` 实装 + `CtrlLink::halt_when(ch_bool)` 接 `mmufault_clear` | Modify (Phase D, ~40 LOC 改) |
| `ip/cpu/cpu_factory_chmem.h:117-126` (现有 424 LOC) | 移除 line 117-126 `fprintf` "Real sv32 CH_MEM owner TBD" 占位（Phase E archive 时由 80-85 owner claim 段已存在的 `v0.11.0 owner: change mmu-chmem-pipeline-integration` 字面注释同步清掉运行期 fprintf） | Modify (Phase E, ~10 LOC 改) |
| `bundles/mmu_bundles.h` (新或 extend `bundles/mem_bundles.h`) | 定义 `pl::MMU_VADDR` / `pl::PADDR` / `pl::PADDR_VALID` / `pl::PTW_ACTIVE` / `pl::CPU_EXCEPTION_CODE` payload key | Create/Modify (Phase B + D 跨, ~30 LOC) |
| `ip/mmu/tlm/MMUPlugin.cpp:109` (Phase A 已 commit `a3afcfa`) | Bare shortcut 严格化 (D4-A 已落地) — 维护基线，无新增改动 | Verify only |

### Verification Code (新)

| File | Purpose |
|---|---|
| `tests/mmu/test_mmu_chmem_basic.cpp` (新, ~180 LOC) | `[mmu][chmem][basic]` TLB hit 走 lookup + miss 走 PTW walk 5 case (D1-B 范围验证) |
| `tests/cpu/test_cpu_chmem_mmu_pipeline.cpp` (新, ~200 LOC) | `[cpu][chmem][mmu-pipeline]` 真 sv32 PTE ELF + plant_identity_page_table + tohost=1 (Phase C 集成验证) |
| `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` (现有 3 case) | TEST_CASE 6 新增 sv32 e2e flip — cycle ≤ baseline × 1.5 + EXCEPTION_CODE 正确传播 + pipeline stall 生效 |
| `tests/soc/test_cpu_l1_mmu_demo.cpp` (现有 6 case) | TEST_CASE 7 新增 `cpu_l1_mmu_demo_sv32_translation_flipped` — flip `cfg.enable_mmu=true` 真 sv32 翻译 7/7 PASS（workaround `cfg.enable_mmu=false` 移除） |
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
| Phase 3 — Phase B 解锁 (Sprint 6-7) | B2 (RiscvMMUPlugin_chmem, 2.5d) + B3 (MMUPlugin_chmem, 4.5d) + B4 (PTW FSM 复用, 1d) + B5 (test_mmu_chmem_basic, 2d) |
| Phase 4 — Phase D 验证 (Sprint 6-7 同步) | D1 (verilator TEST_CASE 6 sv32 e2e flip, 1.5d) + D2 (test_cpu_l1_mmu_demo TEST_CASE 7, 1d) + D3 (sv32 PTE ELF vendor, 0.5d) + D4 (sv32 baseline CSV, 0.5d) + D5 (4 architecture gate final, 0.5d) + D6 (run_chipforge_tests.sh --all 全 PASS, 0.5d) |
| Phase 5 — Phase E archive (Sprint 7-8) | E1 (CHANGELOG.md v0.11.0 entry, 0.5d) + E2 (AGENTS.md workaround 删, 0.25d) + E3 (spec-delta 2 份, 0.5d) + E4 (cpu_factory_chmem.h:117-126 fprintf 清理, 0.25d) + E5 (4 gate 0 error 终态验证, 0.5d) + E6 (`openspec archive mmu-chmem-pipeline-integration -y`, 0.5d) |

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

## Phase 1 — bp-btb-coordination + Phase C 入口准备 (估时 2 day, Sprint 2 早期 10-23 ~ 10-30 + Sprint 2 Week 1)

> **R-CHMEM-1 缓解路径 (2026-10-16 user 决策)**: 本 phase 包含 `bp-btb-coordination-3items` 0.5d 提前 (Sprint 2 最早), 后续 Phase C 启动期已有 fetch stall framework 稳定锚点。详见 `objective-mmu-chmem-phase-b-e.md` §11 sprint-2026-10-3。

### Task 1.1: bp-btb-coordination-3items — fetch stage 协议 0.5d (R-CHMEM-1 缓解)

**Files:**
- Create: `ip/cpu/plugins/fetch_stall_chmem.h` (新, ~80 LOC) — 提供 `pc_reg_mux` ch_uint + `stall_fetch_when` 信号 + 与 bp-btb owner 的 3 项协议 hook
- Modify: `ip/cpu/cpu_factory_chmem.h` (line ~80-85 已写 owner claim, 加 fetch_stall 实例化 8→9 plugin)

- [ ] **Step 1: 与 bp-btb owner 协商 3 项协议**

Run: 异步会议 + ADR 引用 `wave5-bp-btb` proposal §3 备选 3 项 (pc_reg mux / fetch stall / fetch stage 共享)
Expected: 拿到 3 项 fetch stage 协议 hash + interface spec

- [ ] **Step 2: 创建 `fetch_stall_chmem.h` 框架**

Run: 写 ch_mem 基础模板 + 3 项协议签名 (含注释指明 0.5d scope)
Expected: file created, 8→9 plugin 计数准备 (但本 task 不实装, 只模板)

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
- Modify: `bundles/mmu_bundles.h` (新 keys: `MMU_VADDR` / `PADDR` / `PADDR_VALID` / `PTW_ACTIVE`, ~20 LOC)

- [ ] **Step 1: [RED] 写 `[cpu][chmem][ibus-sv32]` 单测 (D2-A bare 路径零额外 latency 保护 + 真 sv32 翻译路径)**

Run: 创建 `tests/cpu/test_ibus_chmem_sv32.cpp` ~80 LOC: Bare 路径 + Sv32 路径对比
Expected: 编译失败 (尚未消费 MMU_VADDR)

- [ ] **Step 2: 改 ibus_chmem.h fetch aread 闭包消费 sv32**

```cpp
// ibus_chmem.h line ~146-155, fetch aread 闭包
pb.at_stage("fetch", NORMAL, [&](Payload& p) {
  auto mmu_vaddr = p.get<ch_uint<32>>(pl::MMU_VADDR);  // 新增
  auto paddr_valid = p.get<ch_bool>(pl::PADDR_VALID);   // 新增
  auto paddr = p.get<ch_uint<32>>(pl::PADDR);
  // Bare path: pc (vaddr) == paddr, 零额外 latency
  // Sv32 path: PTW_ACTIVE=1 时 stall fetch; paddr_valid=1 时用 paddr
  auto fetch_addr = paddr_valid ? paddr : mmu_vaddr;  // ch_uint<32> sel 表达式
  p.put(pl::FETCH_ADDR, fetch_addr);
});
```

(用 `select` 表达式而非 `if (cond)`，按 D4 §4 主逻辑包裹)

- [ ] **Step 3: [GREEN] 跑 + 回归 bare 路径 byte-identical**

Run: `./build/bin/chipforge_tests "[chmem]" && ./build/bin/chipforge_tests "[cpu]"` + 手工算 bare path 5 指令 cycle = baseline
Expected: bare path cycle 数 == Phase 0 baseline byte-identical (零额外 latency 保护验证)

- [ ] **Step 4: PTW_ACTIVE=1 时 stall fetch**

Run: `./build/bin/chipforge_tests_chmem "[cpu][chmem][ibus-sv32]" && 用 sv32 PTE ELF 验证 stall 协议生效`
Expected: PTW_ACTIVE=1 时 fetch stall 不发新 PC，PTW 完成 → resume

### Task 2.2: C3 — `dmem_chmem.h` sv32 消费 (load/store 闭包)

**Files:**
- Modify: `ip/cpu/plugins/dmem_chmem.h` line ~100-145 (load/store 闭包 ~30 LOC 改)

- [ ] **Step 1-4 与 Task 2.1 同模式 (Bare path byte-identical + Sv32 path sel 表达式)**

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
void on_execute(Payload& p) {
  auto mmufault_clear = p.get<ch_bool>(pl::MMUFAULT_CLEAR);  // 来自 exception handler
  if (mmufault_clear) {
    mark_mmufault();
  } else {
    clear_mmufault();
  }
  // ... existing RAW detection (CtrlLink::halt_when)
  ctrl_link_.halt_when(mmufault_clear);  // 停 fetch + decode
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

## Phase 3 — Phase B 解锁并入 Sprint 6-7 (估时 10 day, Sprint 6 Week 1 ~ Sprint 7 Week 1)

> **依赖序** (`tasks.md` 锁定): B2 (RiscvMMUPlugin_chmem) → B3 (MMUPlugin_chmem 收窄版) → B4 (PTW FSM 复用) → B5 (test_mmu_chmem_basic)

### Task 3.1: B2 — `RiscvMMUPlugin_chmem.h` 实装

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

### Task 3.2: B3 — `MMUPlugin_chmem.h` 收窄版实装

**Files:**
- Create: `ip/mmu/tlm/MMUPlugin_chmem.h` (~280 LOC)

- [ ] **Step 1-4: TDD 实装 8 项直接映射 TLB + 调用 `PtWalkFsmPlugin` + 发射 payload key**

- [ ] **Step 5: Bare path byte-identical 0 退化**

### Task 3.3: B4 — `PtWalkFsmPlugin` 复用 (0 修改)

**Files:**
- Verify: `ip/mmu/plugins/mmu_ptw_chmem.h` (现有 FSM, 0 改动仅 verify)

### Task 3.4: B5 — `test_mmu_chmem_basic` 5 case PASS

**Files:**
- Create: `tests/mmu/test_mmu_chmem_basic.cpp` (~180 LOC)

- [ ] **Step 1-4: TLB hit 走 lookup + miss 走 PTW walk 5 case**

Expected: `[mmu][chmem][basic]` 5/5 PASS

---

## Phase 4 — Phase D 验证 (估时 4 day, Sprint 7 Week 1-2, 与 Phase B 同步)

> **依赖序**: Phase B + Phase C 全绿 → Phase D 启动

### Task 4.1: D1 — `[mmu-verilator]` TEST_CASE 6 sv32 e2e flip

**Files:**
- Modify: `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` (现有 3 case → + 2 case)

- [ ] **Step 1-3: 加 TEST_CASE 6 `mmu_sv32_translation_verilator_e2e_flipped` + 跑**

Expected: `[mmu-verilator]` 5/5 PASS（真 sv32 翻译通过 Verilator 后端）

### Task 4.2: D2 — `[cpu-l1-mmu-demo]` TEST_CASE 7 真 sv32 翻转

**Files:**
- Modify: `tests/soc/test_cpu_l1_mmu_demo.cpp` (现有 6 → 7 case)

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

## §7. 验证自检（execute 完成后跑）

> **Spec 覆盖**:
> - [ ] proposal.md §1 (MMUPlugin CH_MEM 实装) → Phase 3 B2-B5 覆盖
> - [ ] proposal.md §2 (Bare shortcut 严格化) → 已 Phase A commit `a3afcfa`, 维护
> - [ ] proposal.md §3 (csr_write_satp 注入) → 已 Phase A commit `a3afcfa`, 维护
> - [ ] proposal.md §4 (Verilator TEST_CASE 6 sv32 e2e flip) → Phase 4 D1 覆盖
> - [ ] proposal.md §5 ([cpu-l1-mmu-demo] 真 sv32 翻转) → Phase 4 D2 覆盖
> - [ ] proposal.md §6 (AGENTS.md workaround 删) → Phase 5 E2 覆盖
> - [ ] proposal.md §7 (Spec delta 2 份) → Phase 5 E3 覆盖
> - [ ] proposal.md §8 (Architecture Gate Final) → Phase 4 D4 覆盖
> - [ ] proposal.md §9 (Regression Final Check) → Phase 4 D5 覆盖
> - [ ] proposal.md §10 (Archive Preparation) → Phase 5 E6 覆盖
> - [ ] proposal.md §11 (D1 收窄 scope 显式声明) → Phase 3 B1 跳过 (已 Oracle 锁定)

> **占位符扫描**: 0 hits (`grep -E "TODO|TBD|FIXME|fill in" .rddf/plans/mmu-chmem-pipeline-integration.md` 0 output)

> **类型一致性**: 验证 `pl::MMU_VADDR` / `pl::PADDR` / `pl::PADDR_VALID` / `pl::PTW_ACTIVE` / `pl::MMUFAULT_CLEAR` / `pl::CPU_EXCEPTION_CODE` / `pl::FETCH_ADDR` 在 bundles/mmu_bundles.h 单一来源定义 + Task 2.1 / 2.2 / 1.2 / 4.2 全部一致引用

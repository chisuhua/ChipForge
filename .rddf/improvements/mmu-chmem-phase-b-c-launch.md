---
id: mmu-chmem-phase-b-c-launch
status: ready
initiative: wave5-isa-coverage-and-bp
priority: P1
phase: phase-4
theme: phase-4
version_target: v0.11.0
manual_deps:
  - ctrl-link-chmem-reg-handle  (CtrlLink stateful halt 通道 — Phase C.5 物理前置；2026-10-22 立项，openspec change 已建，0/33 tasks)
review_by: 2026-11-15
---

# mmu-chmem-phase-b-c-launch — mmu-chmem Phase B/E 真翻译 + C wiring 收官（v0.11.0 Sprint 2-7）

> **状态**：`ready`（2026-10-22 rdd-planner 阶段退出）。源 openspec change `openspec/changes/mmu-chmem-phase-b-c-launch/` 已建（0/27 tasks），事实 SSOT 为 `openspec/changes/mmu-chmem-pipeline-integration/`（Phase 2 done）+ `.rddf/plans/mmu-chmem-pipeline-integration.md`（37KB TDD 5 步 plan）。
> **主题**: CH_MEM MMU/PTW pipeline 集成 (v0.11.0 owner)
> **优先级**: P1

## 架构依据

### 关键路径缺口：Phase B 的真翻译层根本不存在

`ip/mmu/tlm/` 只有 `MMUPlugin.h` / `MMUPlugin.cpp` / `mmu_keys.h` 三个文件 —— **没有任何 CH_MEM 版 MMU plugin**。

而 CH_MEM 侧消费端**已经就位**：

- `ip/cpu/plugins/ibus_chmem.h:121-149` — `MmuKeys = mmu_keys<std::uint64_t>`，`PADDR_VALID` 恒 false ⇒ sel 走 `MMU_VADDR`（Bare）
- `ip/cpu/plugins/dmem_chmem.h:142-147` — 同上

⇒ **sv32 消费端代码已落地但永远走 Bare** —— 这是"MMU owner 真空"（`cpu_factory_chmem.h:80-85` 自证 "Real sv32 CH_MEM owner TBD"）的技术实质。

### ADR-083 §2.4 双文件分离 + CI 可见性（2026-10-22 双审新增约束）

ADR-083 §2.4（`docs/architecture/adr/ADR-083-plugin-style-single-source-of-truth.md:125-155`）要求业务 IP 强制双文件分离，目录约定 `ip/{cpu,cache,mmu,memory}/plugins/<name>.{h,_chmem.h}`。

**关键陷阱**：`tools/check_plugin_portability.sh` 的扫描范围是

- Check 2（`:95`）= `find ${TARGET_DIRS} -path "*/plugins/*_chmem.h"` —— **只扫 `plugins/`**
- Check 2 TLM 侧（`:107`）= `find ${TARGET_DIRS} -path "*/plugins/*.h"` —— **同样只扫 `plugins/`**
- Check 7（`:273-284`）按文件名排除 `*_chmem.h`

⇒ **`ip/mmu/tlm/MMUPlugin_chmem.h` 同时逃出 Check 2 与 Check 7** —— 4 门禁全绿但 CI 对该文件完全失明。
⇒ **裁决：CH_MEM 新文件落 `ip/mmu/plugins/MMUPlugin_chmem.h`**（CI 可见 + ADR-083 目录约定满足）。`ip/mmu/tlm/` 整体迁移到 `ip/mmu/plugins/`（对齐 `cache-tlm-to-plugins-rename` 先例）另立治理 change，本轮不做。

### Payload key 指针身份（M1/M2 —— 静默失效陷阱）

`PayloadStore` 用**指针身份**索引：`include/cf/plugin/payload.h:124,136,153,166,196,204,220,233` 全部是 `cells_.find(&key)`。

`ip/mmu/tlm/mmu_keys.h:44-53` 已定义全部 5 个目标 key（`MMU_VADDR` / `PADDR` / `PADDR_VALID` / `PTW_ACTIVE` / `EXCEPTION_CODE`），且 `mmu_keys` 是**模板 struct + static inline 成员** ⇒ `mmu_keys<uint64_t>` 与 `mmu_keys<uint32_t>` 是**不同静态实例、不同地址**。

⇒ **若 Phase B 新建 bundles key 对象，或用与消费端不同的模板参数实例化**：
消费端 `has()` 恒 false ⇒ sel 静默回退 Bare ⇒ **所有测试照 PASS（Bare 回退是 graceful 设计）而 sv32 从未生效**。
⇒ **裁决：复用 `ip/mmu/tlm/mmu_keys.h` 既有静态实例，且实例化参数必须与 `ibus_chmem.h:121` / `dmem_chmem.h:142` 同为 `std::uint64_t`**。

### fetch-stall 的逻辑倒挂（BLOCKER-B5）

`.rddf/protocols/fetch_stall_requirements_v0.11.0.md` §1.5（`:74-82`）明写：*"必须先定义 ch_bool 类型的 PTW_ACTIVE 信号（Phase B 实装），然后才能 stall"* ⇒ **fetch-stall v0.2 protocol 反向依赖 Phase B**。

原 `pre_flight` 把 `fetch-stall-protocol-v0.2` 放在 Phase B 之前是**逻辑倒挂**，且其 owner（wave5-bp-btb）已于 2026-10-19 S3 决策 defer 到 v1.0.0 ⇒ 无人可交付。
⇒ **裁决：移出 pre_flight，另立 slot（Phase 3.5，Sprint 3 后段），挂 C.4 入口条件**。

## 范围

### In Scope

- **Phase B（15d，Sprint 2-3）**：
  - **B.2** `ip/cpu/plugins/mmu_chmem.h`（新，~180 LOC）—— `RiscvMMUPlugin` CH_MEM 实装（elaboration-time satp 注入 + 单一 `ch_reg<satp_value>` 初值 + 与 TLM `MMUPlugin.cpp` 行为对齐）
  - **B.3** `ip/mmu/plugins/MMUPlugin_chmem.h`（新，~280 LOC）—— 单级 `ch_mem` TLB（8 项直接映射）+ 复用 `PtWalkFsmPlugin` walk + 发射 `MMU_VADDR`/`PADDR`/`PADDR_VALID`/`PTW_ACTIVE`（**复用既有 `mmu_keys` 静态实例**）
  - **B.4** `ip/cpu/plugins/mmu_ptw_chmem.h` 复用（0 修改；**修正 37KB plan Task 3.3 的错误路径** `ip/mmu/plugins/mmu_ptw_chmem.h` → `ip/cpu/plugins/mmu_ptw_chmem.h`）
  - **B.5** `tests/mmu/test_mmu_chmem_basic`（新，5 case）+ **手动加入 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES`（`:118-145` 手工 list，GLOB 只覆盖 TLM target）**
  - **B.6** Phase B 集成回归（5d）
- **Phase C wiring（5d，Sprint 4）**：C.1 `mmu_exception_handler_chmem` wiring → C.4 `cpu_factory_chmem` fprintf 删除 + MMUPluginChmem 注册 → C.5 `hazard_chmem.h` halt_when wiring（**含 `ctrl-link-chmem-reg-handle` 移出的 A4：mmufault pending 改信号驱动**）→ C.6 集成测试（3d，Sprint 5）
- **Phase D 验证（5d，Sprint 6）**：D.1 `[mmu-verilator]` TEST_CASE 4 sv32 e2e flip + D.2 `[cpu-l1-mmu-demo]` TEST_CASE 7 + D.3 `build_sv32_pte.S` vendor + D.4 `sv32-baseline-matrix.csv`
- **Phase E 收官（2d，Sprint 7）**：E.1-E.6 文档 + archive

### Out of Scope

- 多级 TLB / LRU/RRIP / ASID≠0 / Sv39/48 / 权限检查 / megapage 完整 PPN 拼接
- **不新建 bundles key 文件**（复用 `ip/mmu/tlm/mmu_keys.h` 既有静态实例）
- **不把 `ip/mmu/tlm/` 整体迁到 `ip/mmu/plugins/`**（另立治理 change）
- **不清全仓 deprecated `&&`/`||`**（~134 站点，另立 change）
- **不改 TLM 模式的 CtrlLink 语义**（`ctrl-link-chmem-reg-handle` 的 scope）

## Why

1. **v0.11.0 launch gate 判据 #3 的唯一 owner**：目标 = `[cpu-l1-mmu-demo] 7/7` + `[mmu-verilator] 4/4`（现状 3/3 plumbing-only + D.1 新增 TEST_CASE 4）。判据 #1（`[riscv-tests] 48/48`，rv32um 8/8）与 #2（5 cpu_sim CLI PASS）已由 `mfc-extract-fsm-h`（2026-10-10 archive）与 `cpu-pipeline-tick-fix`（2026-10-16 archive）兑现。
2. **Phase B 是真关键路径**：CH_MEM 消费端 sel 表达式已落地（`ibus_chmem.h:131-149` / `dmem_chmem.h:132-147`）但 `PADDR_VALID` 恒 false ⇒ 没有 MMU plugin 就永远走 Bare ⇒ 消费端代码是"裸"的。
3. **Phase B 历史上 3 次 deep-agent 派发失败**（`mmu-chmem-pipeline-integration/tasks.md` 2026-10-10 注记：*"超出 deep agent single-shot 30-min timeout 能力"*）⇒ 必须**交互式** CppHDL API 探查 + build/test 反馈循环，且估时按源 change 的 **15d**（不是 37KB plan 的 10d —— 那份丢了 B.5 的 5d 集成回归）。
4. **前置门槛按现状不可满足**：原 `pre_flight` 4 项中 2 项的 owner 已 defer（见"架构依据"）⇒ 不先修门槛就进 rdd-builder，P0 gate 空转一轮。
5. **上游风险已消除**：`chmem-dmem-read-latency` 于 2026-10-21 闭环（上游 CppHDL `c27543d`）⇒ `[chmem]` 73/73 PASS、vendored ELF 47/47 PASS、`[verilator]` 48 ELF 全 PASS ⇒ **本 change 的基线是干净的**（proposal 里"18 ELF FAIL"那条风险条已过期，需更正）。

## What Changes

### Phase B（Sprint 2-3，15d）

| 步骤 | 文件 | 内容 | 估时 |
|---|---|---|---|
| B.0 | — | **key 复用确认**：`MMUPlugin_chmem` 发射用 `mmu_keys<std::uint64_t>` 的**同一静态实例**（与 `ibus_chmem.h:121` / `dmem_chmem.h:142` 指针相等） | 0.2d |
| B.2 | `ip/cpu/plugins/mmu_chmem.h`（新） | `RiscvMMUPlugin` CH_MEM 实装 | 3d |
| B.3 | `ip/mmu/plugins/MMUPlugin_chmem.h`（新） | 单级 `ch_mem` TLB + 复用 `PtWalkFsmPlugin` + 发射 4 个 payload key | 5d |
| B.4 | `ip/cpu/plugins/mmu_ptw_chmem.h` | 复用（0 修改，修正 plan 路径） | 1d |
| B.5 | `tests/mmu/`（新）+ `tests/CMakeLists.txt` | `test_mmu_chmem_basic` 5 case + 手动加入 `CHMEM_TEST_SOURCES` | 2d |
| B.6 | — | Phase B 集成回归 | 5d |

**Session 拆分**（避免极端不平衡 + 交接契约显式化）：
- session 0 = B.0（0.2d）
- session 1 = B.2 + B.4（3.75d）→ 交付：① 编译通过的 `mmu_chmem.h` ② key 复用清单（实例化 + 指针相等证明）③ RED 测试骨架
- session 2 = B.3 + B.5（6.5d）→ 交付：① `MMUPlugin_chmem.h` 可 elaboration ② 5 case GREEN ③ 变异对照全红

**回归成本**：`[chmem]` 全量 **833s/轮**（ctest 默认 1500s 上限内但余量不大）⇒ 按 6+ 轮 × 14min 纯等待计入。快速迭代用单 family 过滤，全量只在 GREEN 收敛点跑。

### Phase 3.5（Sprint 3 后段，3.5d）★ 由 pre_flight 重构而来

| 步骤 | 内容 | 估时 |
|---|---|---|
| 3.5.1 | **R11.1** trap PC 跳转可行性重审（原 pre_flight #1，**原计划无 slot，此处补上**） | 0.5d |
| 3.5.2 | **bp-btb 收窄 2 项**（stall 语义 + fetch stage 顺序）= requirements doc BLOCKER-B1 + B2 | 1d |
| 3.5.3 | **fetch-stall v0.2 protocol 起草**（解 5 BLOCKER + 7 SHOULD_FIX） | 3d |

### Phase C / D / E

见 `openspec/changes/mmu-chmem-phase-b-c-launch/tasks.md` 与源 change `tasks.md`。

## Acceptance

### Phase B
- [ ] **AC-B0** **key 复用已证明** —— `MMUPlugin_chmem` 发射的 key 与 `ibus_chmem.h:121` / `dmem_chmem.h:142` 消费的 key 是**同一静态实例**（`mmu_keys<std::uint64_t>`，指针相等），**未新建 bundles key 对象**
- [ ] **AC-B1** 文件路径符合 ADR-083 + CI 可见性 —— `ip/mmu/plugins/MMUPlugin_chmem.h` + `ip/cpu/plugins/mmu_chmem.h`；**CI 可见性证明**：故意写一个不含 `ch_*` 的 `_chmem.h` 应被 `check_plugin_portability.sh` Check 2 报 FAIL
- [ ] **AC-B2** `test_mmu_chmem_basic` 5 case PASS，其中 **"PTW fault" case 明确标注为 TLB 内部单元级**（Phase B 窗口无 fetch stall 机制，pipeline 级不可验 —— CH_MEM 数据通路全组合，`cpu_factory_chmem.h:431-435` 无 stage 寄存器）
- [ ] **AC-B3** **真翻译生效判据**（缺此项 = Phase B 可"全绿交付零翻译"）：
  - (a) **非恒等页表向量** —— vaddr X → paddr Y（Y ≠ X），断言翻译后地址为 Y。identity page table **无法区分真 sv32 与 Bare 回退**（两者输出恒等）
  - (b) **消费端断言** —— 经 `ibus_chmem` / `dmem_chmem` 共享节点验证 `has(MmuKeys::PADDR_VALID) == true` 且 sel 真选 `PADDR`（非 `MMU_VADDR`）
  - (c) **satp=0 / satp_ppn 未编程边界用例**（v0.10.1 deep-rca 记录过"PTW 从不在 window 0x0 读 PTE"同类历史 bug；Bare-shortcut 严格化 v0.10.4 + CH_MEM 初值注入的交互会埋雷）
- [ ] **AC-B4** **变异对照 ≥2 个**（遵循仓库近期惯例）：① 拔掉 `PADDR_VALID` 接线 → 消费端测试应红；② TLB 改恒 hit → miss case 应红
- [ ] **AC-B5** **B.5 集成回归期间 `MMUPluginChmem` 未注册进 `cpu_factory_chmem`**（注册是 C.4 范围）⇒ `PADDR_VALID` 恒 false ⇒ 无 stall 需求。若误注册，PTW 期间 fetch 会用未翻译地址取指（真错误）
- [ ] **AC-B6** 零回归 —— `[chmem]` 73/73 基线不变（+ 新增 case 计入）；`[mmu] 59/59` TLM 路径 0 退化；4 架构 gate 0 失败；`bash tools/run_chipforge_tests.sh --all` 全 PASS
- [ ] **AC-B7** 新 CH_MEM 测试**已手动加入 `CHMEM_TEST_SOURCES`** 且在 `chipforge_tests_chmem` 中可见可跑

### 排期对齐（Phase 2）
- [ ] **AC-P1** Phase B 窗口 + 工作量在 4 处文档一致（37KB plan Phase 3 段 / 源 change tasks Phase Schedule Table / `mmu-chmem-phase-b-c-launch/tasks.md` / objective §8+§10），**均为 Sprint 2-3 + 15d**
- [ ] **AC-P2** 37KB plan frontmatter `sprint_window` / `manual_deps` 已同步；Task 3.3 错误路径已修
- [ ] **AC-P3** `mmu-chmem-phase-b-c-launch/tasks.md` pre_flight 每项**都有 owner + 交付物 + 窗口 + 挂载点**，且全部可满足（#1 R11.1 → Phase 3.5、#4 CtrlLink → `ctrl-link-chmem-reg-handle`）
- [ ] **AC-P4** `mmu-chmem-phase-b-c-launch/proposal.md` 过期风险条（"18 ELF FAIL"）已更正
- [ ] **AC-P5** `.rddf/state/.planner-handoff.json` 全字段刷新 + `.planner-state.json` 的 `active_projects` 同步
- [ ] **AC-P6** `bash tools/roadmap-r9-check.sh` exit 0 + 4 架构 gate 0 失败 + `openspec change validate` 0 error

### 整体
- [ ] **AC-X1** `[cpu-l1-mmu-demo] 7/7 PASS` + `[mmu-verilator] 4/4 PASS`（v0.11.0 判据 #3）
- [ ] **AC-X2** `openspec archive mmu-chmem-phase-b-c-launch -y` + 同步归档 `mmu-chmem-pipeline-integration`（Phase 2 done）

## Capabilities

- **CH_MEM MMU/PTW 真翻译**（capability `mmu-chmem-integration`）：CH_MEM 模式下 MMU SHALL 把 vaddr 翻译为 paddr 并置 `PADDR_VALID=true`，使 `ibus_chmem` / `dmem_chmem` 的消费端 sel 真选 `PADDR`
- **Payload key 单一注册**（capability `mmu-chmem-integration`）：MMU plugin 与其消费端 SHALL 使用**同一** Payload key 静态实例（`mmu_keys<std::uint64_t>`），避免指针身份不一致导致的静默回退
- **sv32 e2e 验证**（capability `verilator-mmu-bare-plumbing` 扩展）：Verilator 后端 SHALL 有真 sv32 翻译用例（TEST_CASE 4）

## Impact

### 受影响文件

| 文件 | 动作 | 影响 |
|------|------|------|
| `ip/cpu/plugins/mmu_chmem.h` | 新增 | `RiscvMMUPlugin` CH_MEM 实装 |
| `ip/mmu/plugins/MMUPlugin_chmem.h` | 新增 | `MMUPlugin` CH_MEM 收窄版（单级 TLB + PTW 复用） |
| `ip/cpu/plugins/mmu_ptw_chmem.h` | 改（仅路径注释） | 37KB plan 错误路径修正 |
| `ip/cpu/cpu_factory_chmem.h` | 改（C.4） | fprintf 删除 + MMUPluginChmem 注册 |
| `ip/cpu/plugins/hazard_chmem.h` | 改（C.5） | mmufault pending 改信号驱动（**A4 从 `ctrl-link-chmem-reg-handle` 移入**） |
| `ip/cpu/plugins/mmu_exception_handler_chmem.h` | 改（C.1） | CPU_EXCEPTION_CODE wiring |
| `bundles/` | **不动** | 复用 `ip/mmu/tlm/mmu_keys.h` 既有静态实例 |
| `tests/mmu/` + `tests/CMakeLists.txt` | 新增 + 改 | 单测 + `CHMEM_TEST_SOURCES` 手工 list |
| `.rddf/plans/mmu-chmem-pipeline-integration.md` | 改 | 排期 + 任务形状对齐（15d） |
| `.rddf/roadmap/objectives/objective-soc-cpu-v0110-launch.md` | 改 | §8 时间盒 + §10 + §11 台账 |
| `.rddf/roadmap/features/feat-mmu-chmem-integration.md` | 改 | R-CHMEM-1 缓解段落过期（BP 已 defer） |

### 风险

- **R1（中高）** Phase B 再次卡在 CppHDL API 探查（历史 3 次失败）⇒ 拆 session 0/1/2 + 交接契约 + 每 session 以 `[chmem]` 编译通过为收敛判据；回退 = Phase B 后退到 Sprint 6-7（原 plan 排期仍可回退）
- **R6（中）** **key 实例不一致导致静默 Bare 回退**（测试全绿假象）⇒ AC-B0 + AC-B3 + AC-B4 三重防护
- **R7（中）** **CI 盲区**（新文件放 `ip/mmu/tlm/`）⇒ 路径锁 `ip/mmu/plugins/` + AC-B1 可见性证明
- **R8（中）** 新测试静默不运行 ⇒ AC-B7 显式检查 `CHMEM_TEST_SOURCES`
- **R9（高）** 回归时间成本 —— `[chmem]` 833s/轮 × 6+ 轮 ⇒ 削减轮次而非覆盖面
- **R-CHMEM-1（中）** fetch-stall BLOCKER-B1/B2 是框架级缺口（B1 可能需 ADR-048 扩展）⇒ Phase 3.5 的 3.5d slot 是唯一缓冲，不可再压

### 下游依赖

- `ctrl-link-chmem-reg-handle`（**硬前置** —— stateful halt 通道，C.5 物理依赖）
- `chmem-mext-fsm-cycle-parity`（下游受益方 —— Phase B 后可评估）
- `feat-mmu-chmem-integration`（v0.11.0 owner）
- `objective-ip-mmu-mmu-chmem-phase-b-e.md`（Phase B-E owner）

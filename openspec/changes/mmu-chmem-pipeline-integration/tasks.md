# Tasks — mmu-chmem-pipeline-integration (v0.11.0)

> **Drafted**: 2026-10-08 by Sisyphus session (基于 Oracle 2026-10-07 决议 + research-v0.11.0-prep.md §4)
> **Last updated**: 2026-10-14 (A.5 全项已落 + 3 skeleton commit 落地后, retention 同步)
> **Total tasks**: 165 = 12 `[x]` 已完成 (A.5 全项 + skeleton commits) + 153 `[ ]` open (grep 实测 `^- \[` = 165);任务组分解: A1-A6 + B2-B5 + C1-C6 + D0-D6 + E1-E6
> **Task 粒度 retention**: 1/29 (仅 A.5 = "PR1-A 根因修复" 全项完成);其余 11 `[x]` 为 sub-task (新建 test case / debug print / debug print 删除 / 回归项等)
> **Total estimate**: 6.0-6.2 人周 (Oracle 调整后,vs 原 5.6)
> **B1 已跳过**: Oracle D1-B 收窄版锁定,scope 决策已前置完成
> **D0 已完成**: proposal 文本修订 (本 PR 落地)
> **Phase B 曾 defer (2026-10-10, 已于 2026-10-22 解除)**: 3 次 deep agent 派发累计 1.5h+ 均未产出 mmu_chmem.h 代码 (Oracle 2026-10-09 6 答案给全仍不够,需交互式 CppHDL API 探查 + build/test 反馈循环,超出 deep agent single-shot 30-min timeout 能力)。当时 Sprint 1-2 (10-09 ~ 11-30) 集中 Track A (mfc-extract-fsm-h 真 radix-2 实装);Phase B (B2-B5) + C (C1-C6) 推迟到 Sprint 6+ (12-01 起)。**2026-10-22 双审裁决: Phase B 提前回 Sprint 2-3 (10-23 ~ 11-06) + 估时回改 15d**,根因是"失败模式 = single-shot 派发方式"而非日历位置 ⇒ 由 `ctrl-link-chmem-reg-handle` (Sprint 2 早期 1d, 同一套 `ch_reg` + OR 网 API 的交互式热身 session) 承接。详见 `objective-ip-mmu-mmu-chmem-phase-b-e.md` §11 跟踪台账
> **Sprint 2-5 window (Phase C only, 10-23 ~ 11-30)**: C1-C6 (per objective-ip-mmu-mmu-chmem-phase-b-e.md, B 已 defer)
> **Phase B 启动窗口提前 + 估时回改 (2026-10-22 双审裁决)**: Phase B 由 Sprint 6+ (12-01) **提前到 Sprint 2-3 (10-23 ~ 11-06)**,估时按本表 B.2 3d + B.3 5d + B.4 2d + B.5 5d = **15d**（`.rddf/plans/mmu-chmem-pipeline-integration.md` 原写 10d,**整段丢失 B.5 的 5d 集成回归**,已在 2026-10-22 回改）。**为何提前**: 12-01 起只剩 8 周要塞 B(15d)+C(5d)+C.6(3d)+D(5d)+E(2d) ≈ 30d + DMIPS 硬门禁,buffer ≈ 0。**为何 15d 而非 10d**: 历史 3 次 deep agent 派发失败（见上注）,采纳压缩版是错误方向。
> **pre_flight 重构 (2026-10-22)**: 原 pre_flight #2 (`fetch-stall-protocol-v0.2`) 与 #3 (`bp-btb-coordination-3items`) **移出 pre_flight** —— #2 的 owner (wave5-bp-btb) 已于 2026-10-19 S3 决策 defer 到 v1.0.0 ⇒ 无人可交付; 且 `fetch_stall_requirements_v0.11.0.md` §1.5 (BLOCKER-B5) 明写 *"必须先定义 ch_bool 类型的 PTW_ACTIVE 信号（Phase B 实装），然后才能 stall"* ⇒ **fetch-stall v0.2 反向依赖 Phase B**,逻辑倒挂。二者合并为 **Phase 3.5 slot**（Sprint 3 后段 3.5d）,挂载点 = **C.4 入口条件**。
> **Phase B 路径与 key 裁决 (2026-10-22)**: CH_MEM 新文件落 `ip/mmu/plugins/MMUPlugin_chmem.h`（**非** `ip/mmu/tlm/` — 后者同时逃出 `check_plugin_portability.sh` Check 2 与 Check 7 扫描范围 = CI 盲区）; **不新建 bundles key 文件**,复用 `ip/mmu/tlm/mmu_keys.h` 的 `mmu_keys<std::uint64_t>` 既有静态实例（PayloadStore 按指针身份索引 ⇒ key 不同 ⇒ 消费端 `has()` 恒 false ⇒ 静默回退 Bare ⇒ 测试全绿但 sv32 从未生效）。
> **Owner**: mfc-chmem team (per ADRs in proposal)
>
> **执行顺序** (硬性,不可并行的有依赖标记):
> 1. PR1-A (A1 → A3.1 → A4 → A5 → A6) — 根因修复,**必须前置**
> 2. PR1-B (B2 → B3 → B4 → B5) — MMUPlugin CH_MEM 实装
> 3. PR1-C (C1 → C4 → C2/C3 并行 → C5 → C6) — Pipeline 集成(**C2/C3 启动前 bp-btb-coordination-3items 必须完成**)
> 4. PR1-D (D1 → D2 → D3/D4 并行 → D5 → D6) — 验证与回归
> 5. PR1-E (E1 → E2/E3 并行 → E4 → E5 → E6) — 文档与归档

## Phase Schedule Table

> **决策记录 (本 PR 内落地,不单独开 PR)**: Track B staging — Phase B (B2-B5) 解锁并入 Sprint 2-5 排期,依据 `objective-soc-cpu-v0110-launch.md` §8 (17 周时间盒:Sprint 2-5 = 10-23 ~ 11-30, Track B) + `objective-ip-mmu-mmu-chmem-phase-b-e.md` (Phase B-E scope,B1 跳过)。owner = mfc-chmem team (proposal ADRs)。

| Sprint | Window | Phase | Tasks | Owner |
|--------|--------|-------|-------|-------|
| Sprint 1 | 10-09 ~ 10-22 | Phase A (verified ✅) | A1-A6 | mfc-chmem |
| Sprint 2 (early, 10-23 ~ 10-30) | **ctrl-link-chmem-reg-handle** (1d 框架级, stateful halt 通道 — Phase C.5 物理前置 + CppHDL API 热身 session) + 排期对齐 (1d) + `feat-phase-6d-rtl-verification` 收官 (1d) | mfc-chmem + framework |
| Sprint 2-3 | 10-23 ~ 11-06 | **Phase B** (15d — 2026-10-22 双审回改自 10d, 原压缩版丢了 B.5 的 5d 集成回归) | **B.0** (key 复用确认 0.2d) + B.2 + B.3 + B.4 + B.5 + **B.6** (集成回归 5d) | mfc-chmem |
| Sprint 3 (late) | 11-06 ~ 11-30 | **Phase 3.5** (3.5d — 由 pre_flight 重构而来) | R11.1 (0.5d) + bp-btb 收窄 2 项 (1d) + fetch-stall v0.2 protocol (3d) | mfc-chmem |
| Sprint 4 | 11-30 ~ 12-15 | Phase C wiring | C1-C6 | mfc-chmem |
| Sprint 5 | 12-15 ~ 12-22 | C.6 集成测试 | C.6 | mfc-chmem |
| Sprint 6-7 | 12-22 ~ 01-13 | Phase D 验证 | D1-D6 | mfc-chmem |
| Sprint 7 | 01-13 ~ 01-24 | Phase E | E1-E6 | mfc-chmem |

---

## Phase A — 根因 Bug 修复 (前置硬依赖)

> **Oracle 验证的 3 个真 bug**:
> 1. `MMUPlugin.cpp:109` Bare shortcut `|| satp_ppn_ == 0` 误判 (D4-A)
> 2. **Shadow 双 bug** — `mmu.h:100` 派生 `set_satp_value` shadow + `mmu.cpp:37` 48-bit mask 把 Sv32 MODE bit31 含进 PPN (D5-TLM)
> 3. **可能更便宜 root cause** — TLM flip 尝试 2 PPN 计算错误 `(0x80000000+0xF000)>>12 = 0x8000F ≠ 0x80000` (A5 优先验证)
>
> **本 Phase 完成标志** (2026-10-14 修订: 与实测 `test_mmu_bare_plumbing_verilator.cpp` 3 case 对齐): `[mmu] 59/59 PASS + 0 意外退化 + shadow 双 bug 单元测试 PASS + PPN 计算核实 + 1/1 sv32 PTE ELF 在 TLM pipeline 不卡死 (D.3 vendor 单 ELF; D.1 Verilator 侧 1/1 sv32_pte.elf tohost=1)`

### A.1 [RED] 写 `[mmu][bare-shortcut]` 测试:Bare shortcut 3 case (D4-A 验证)

- [x] **新建测试 case** (追加到 `tests/mmu/test_mmu_plugin.cpp`):3 个新 test cases,~50 LOC
- [x] **Family tag**:使用 `[mmu][bare-shortcut]` (与既有 `[mmu]` 多 tag 一致)
- [ ] **覆盖范围** (2026-10-14 修订: 与 D4-A 实际行为对齐):
  - **Case 1**:`MMUPlugin(Sv32, ..., mem=nullptr)` + `set_satp_ppn(0)` → 调 `do_lookup` → **期望 PADDR_VALID=false 且 PTW_ACTIVE=true** (Sv32 mode 不可走 Bare shortcut;ppn=0 仍走 Sv32 TLB miss → 启动 PTW)
    - **当前 FAIL (修复前)** → **PASS (修复后, D4-A 已落 commit `a3afcfa`)** — 验证 Bare shortcut 严格按 MODE 字段判定,不掺杂 satp_ppn
  - **Case 2**:`MMUPlugin(Sv32, ..., mem=nullptr)` + `set_satp_ppn(0x12345)` + 提供 TLB stub 命中 → **期望 PADDR = TLB 命中 paddr** (不走 identity,走 TLB lookup)
    - **当前 PASS** (修复后保持 PASS — 验证修复不影响正常路径)
  - **Case 3**:`MMUPlugin(Sv39, ..., mem=nullptr)` + `set_satp_ppn(0xABCDE)` + 提供 TLB stub 命中 → **期望 PADDR = TLB 命中 paddr** (Sv39 路径不受影响)
    - **当前 PASS** (修复后保持 PASS)
- [ ] **断言数**:6-9 assertions (3 case × 2-3 断言)
- [ ] **运行命令**:`./build/bin/chipforge_tests "[mmu][bare-shortcut]"`

### A.2 [GREEN] 修复 `MMUPlugin.cpp:109` Bare shortcut 误判条件 (D4-A)

- [x] **修改 `ip/mmu/tlm/MMUPlugin.cpp:109`** — 删除 `|| satp_ppn_ == 0` 析取项 (commit `a3afcfa` 落地)
  - 修复前:`if (sv_mode_ == SvMode::Bare || satp_mode == 0 || satp_ppn_ == 0)`
  - 修复后:`if (sv_mode_ == SvMode::Bare || satp_mode == 0)`
- [ ] **不自作主张重写判定逻辑** — 现代码 `:95-108` 已正确提取 `satp_mode`,bug 仅在析取项
- [ ] **回归项**:A.1 测试 PASS;`[mmu] 53/53` 0 意外退化;既有 `[cpu-integration] 81/81` + `[cpu-l1-mmu-demo] 6/6` 维持
- [ ] **运行命令**:`./build/bin/chipforge_tests "[mmu]"` + `[cpu-integration]` + `[cpu-l1-mmu-demo]`

### A.3 [RED] 写 `[mmu][shadow-bug]` 测试:csr_write_satp 后基类 satp_value/satp_ppn 正确更新 (Oracle R2 验证)

- [ ] **新建测试 case** (追加到 `tests/mmu/test_mmu_plugin.cpp` 或新建 `tests/mmu/test_riscv_mmu_shadow.cpp`):3 个新 test cases,~60 LOC
- [ ] **Family tag**:使用 `[mmu][shadow-bug]`
- [ ] **覆盖范围**:
  - **Case 1**:`RiscvMMUPlugin(Sv32, ...)` ctor 后断言 `MMUPlugin::satp_value() == (1ULL << 31)` (Sv32 MODE bit)
    - **当前 PASS** (ctor 走 qualified 调用,基类 satp_value_ 被设为 mode-aware 初值)
  - **Case 2**:调 `csr_write_satp(0x80040000)` 后断言 `MMUPlugin::satp_value() == 0x80040000`
    - **当前 FAIL** (证明 shadow bug #1 — 派生 set_satp_value shadow,基类 satp_value_ 永不更新)
    - **修复后期望 PASS**
  - **Case 3**:同上 Case 2 后断言 `MMUPlugin::satp_ppn() == 0x40000`
    - **当前 FAIL** (现今得 0x80040000,证明 mask bug #2 — 48-bit mask 把 MODE bit31 含进 PPN)
    - **修复后期望 PASS**
- [ ] **断言数**:9-12 assertions (3 case × 3-4 断言)
- [ ] **运行命令**:`./build/bin/chipforge_tests "[mmu][shadow-bug]"`

### A.3.1 [GREEN 前置] 派生 getter/setter 调用点审计 + 删除派生三件套 (D5-TLM 修复)

- [ ] **审计调用点** — grep codebase-wide 找所有 `mmu.h` 派生 `set_satp_value` / `satp_value()` / `set_satp_ppn` / `satp_ppn()` 调用点:
  - `grep -rn "set_satp_value\|MMUPlugin::satp_value\|satp_ppn()" --include="*.cpp" --include="*.h"`
  - 列出全部调用点 + 标注语义兼容性 (修复后是否需调整调用方式)
- [ ] **删除派生三件套** (Oracle 推荐修复路径,单一事实源):
  - `ip/cpu/plugins/mmu.h:99-100` 删除派生 getter/setter
  - `ip/cpu/plugins/mmu.h:103` 删除派生重复 `satp_value_` 成员
- [ ] **复用 `cf::cpu::detail::extract_satp_ppn`** (`mmu.h:44-52` 注释自证已有) — `csr_write_satp` 实装从派生 6 行重复改为复用此函数
- [ ] **回归项**:A.3 测试 PASS;`[mmu] 53/53` 0 退化;`[cpu-integration] 81/81` 维持
- [ ] **运行命令**:`./build/bin/chipforge_tests "[mmu][shadow-bug]"` + `[mmu]` + `[cpu-integration]`

### A.4 [GREEN] A.1 + A.3 测试全部 PASS,根因修复完成 (2026-10-10 PR2-A 收尾)

- [x] **A.1 3 case 全部 PASS** (Bare shortcut 修复正确)
- [x] **A.3 3 case 全部 PASS** (shadow 双 bug 修复正确)
- [ ] **回归汇总**:
  - `[mmu] 53/53 PASS` (含 v0.10.4 workaround 维持)
  - `[cpu-integration] 81/81 PASS`
  - `[cpu-l1-mmu-demo] 6/6 PASS`
  - `[riscv-tests] 40/48 PASS` (rv32ui 40/40 + rv32um 0/8,**预期 0 退化**,rv32um 0/8 是已知 v0.11.0 follow-up)
- [ ] **接受目标**:"零**意外**退化;编码了 buggy 假设的用例允许更新并附理由" (Oracle §12.6)
- [ ] **审计 sv_mode_≠Bare 用例** — grep 所有 sv_mode_ 配置点 + 对应用例,逐一标注"安全/需更新"
- [ ] **运行命令**:`./build/bin/chipforge_tests` (TLM 全套,~0.6s)

### A.5 ✅ [已完成 2026-10-08] TLM flip 尝试 2 PPN 计算核实 (Oracle R12)

> **启动期已执行 (owner session 2026-10-08),结论已反馈 proposal §Why line 36-41 修订。**

- [x] **debug print 验证** — 在 `MMUPlugin::do_lookup` 入口加临时 print:
  - 实际使用:`fprintf(stderr, "[MMU_DEBUG] stage=%s satp_ppn_=0x%lx satp_value_=0x%lx sv_mode_=%d vaddr=0x%lx\n", ...)`
- [x] **手动重放尝试 2** — `mmufault-verilator-sv32-e2e-flip` 是 placeholder change (`status: placeholder`, depends_on `phase-6d.6-mmu-ptw-fsm`),无现存可重放失败测试 → 改用现有最近路径 `[mmu][tlb-refill]` (用 `set_satp_ppn(1ULL<<20) = 0x100000` 模拟 RISC-V satp 路径)
- [x] **验证假设 (实际结果)**:
  - runtime 捕获 `satp_ppn_` = `0x100000`,与 `set_satp_ppn(1ULL<<20)` 调用值**完全一致** → `set_satp_ppn` runtime 路径正确
  - 静态链分析: `cfg.satp_ppn = 0x8000F` → `make_satp_value(Sv32, 0x8000F) = 0x80008000F` → `RiscvMMUPlugin` ctor Sv32 mask `0x3FFFFFULL` (mmu.h:56) → `ppn = 0x8000F` → `MMUPlugin::satp_ppn_ = 0x8000F` ✓ (MODE bit31 正确排除)
  - **结论**: `0x80000` 是 proposal line 26 文档 typo (已修为 `0x8000F`),**不是代码 bug**;"更便宜 root cause"假设被排除,Oracle D4-A + D5-TLM 确认真 root cause
- [x] **删除 debug print** — 验证完成立即删除,`git diff ip/mmu/tlm/MMUPlugin.cpp` 干净 (零残留)
- [x] **回归项**:debug print 期间 + 删除后 `[mmu] 53/53` + `[cpu-l1-mmu-demo] 6/6` 均 PASS,零退化
- [x] **输出**:见本段 + proposal §Why line 36-41 + research §12.2 A5 验证结果块 (PR1-A commit 时引用)

### A.6 [GREEN] Regression check — `[mmu] 59/59` 0 意外退化最终确认 (2026-10-10 PR2-A 收尾)

- [x] **A.1-A.5 全部完成** 后跑全量回归 (commit `a3afcfa` 锁定)
- [ ] **跑**:`./build/bin/chipforge_tests` (TLM 全套)
- [ ] **预期 PASS 计数**:`[framework] 89/89` + `[cache]` + `[cpu] 19/19` + `[cpu-integration] 81/81` + `[cpu-l1-mmu-demo] 6/6` + `[bundles]` + `[mmu] 53/53` + `[riscv-tests] 40/48` + `[soc]` + `[tlb-refill]`
- [ ] **若发现意外退化**:
  - 先 git blame 看是否本 PR 引入
  - 若本 PR 引入 → 修复
  - 若是历史 bug (v0.10.x workaround 失效) → 更新 workaround 并附理由,**允许但不阻塞 PR1-A**
- [ ] **运行命令**:`./build/bin/chipforge_tests "[mmu]" "[cpu]" "[cpu-integration]" "[cpu-l1-mmu-demo]"`
- [ ] **本 Phase 完成标志**:全 PASS + 无本 PR 引入的退化

---

## Phase B — MMUPlugin CH_MEM 实装 (D1-B 收窄 scope)

> **scope 边界** (Oracle D1-B 锁定):单级 ch_mem TLB (8 项直接映射) + 复用 `PtWalkFsmPlugin` + 发射 `PADDR`/`PADDR_VALID`/`PTW_ACTIVE` payload keys + satp 作为 elaboration-time ch_reg 初值。
>
> **显式 out-of-scope** (详见 proposal §11):多级 TLB / LRU/RRIP / ASID≠0 / Sv39/48 / 权限检查 / megapage PPN 拼接 / trap PC 跳转 / CSRPlugin CH_MEM。
>
> **本 Phase 完成标志**:`test_mmu_chmem_basic` PASS + `MMUPlugin_chmem.h` 编译 0 错误 + TLB hit 走 lookup / TLB miss 走 PTW FSM 路径均产出正确 PADDR。

### B.1 ~~[GREEN] 决定 `MMUPlugin_chmem.h` scope~~ — **已跳过 (Oracle D1-B 锁定)**

> **跳过理由**: Oracle 2026-10-07 已锁定 D1-B 收窄版 (单级 ch_mem TLB + 复用 PtWalkFsmPlugin)。无需本 task 起草。
>
> **替代**:scope 详情见 proposal §11 + research §6 D1 + 本 Phase 顶部说明。

### B.2 [GREEN] 实装 `RiscvMMUPlugin_chmem.h` (D5-CH_MEM 约束)

- [ ] **新建文件**:`ip/cpu/plugins/mmu_chmem.h` (~150 LOC,纯头文件) <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **namespace**:`cf::cpu::plugins` (与 TLM `RiscvMMUPlugin` 同 namespace,但 `_chmem.h` 后缀区分) <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **CH_MEM 适配**: <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
  - `#ifdef CF_PLUGIN_USE_CH_MEM` 整文件包裹 (ADR-040 v2.0 强制)
  - 包含 `<ch.hpp>` + `<core/bool.h>` + `<core/uint.h>` + `<core/reg.h>`
  - 包含 `cf/plugin/{plugin_base,pipe_builder,ctrl_link,uint_t}.h`
- [ ] **类定义**:`RiscvMMUPluginChmem : public cf::plugin::PluginBase` <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **elaboration-time satp config 注入**: <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
  - 构造参数接受 `satp_mode_value` (从 `cpu_factory_chmem` ctor 传入)
  - elaboration 期创建 `ch_reg<64>` 作为 satp 寄存器初值 (D5-CH_MEM 约束:无运行期 CSR 写)
- [ ] **at_stage 闭包** (CH_MEM 模式): <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
  - `pb.at_stage("mmu_ptw", NORMAL, [...])` 调 `PtWalkFsmPlugin::create_fsm()` 复用现有 FSM
  - `pb.at_stage("mmu_lookup", NORMAL, [...])` 单级 TLB lookup 闭包
- [ ] **payload key 发射**: <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
  - 写 `mmu_keys<T>::PADDR` + `PADDR_VALID` + `MMU_VADDR` (复用 `mmu_keys.h`)
  - 不发明新 key (ADR-040 Tier-2 兼容)
- [ ] **回归项**:`[chmem]` 既有测试不退化;`[mmu] 53/53` 维持 <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **运行命令**:`./build/bin/chipforge_tests_chmem "[chmem]"` <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->

### B.3 [GREEN] 实装 `MMUPlugin_chmem.h` 收窄版 (D1-B 单级 TLB + 复用 PtWalkFsmPlugin)

- [ ] **新建文件**:`ip/mmu/plugins/MMUPlugin_chmem.h` (~200 LOC,纯头文件) <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **ADR-040 v2.0 强制**:`#ifdef CF_PLUGIN_USE_CH_MEM` 整文件包裹;**不含** `ch_mem`/`ch_reg`/`ch_uint`/`ch::core` 在 TLM 文件中 <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **CH_MEM 适配**:与 B.2 同样 include 列表 <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **类定义**:`MMUPluginChmem : public cf::plugin::PluginBase` <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **单级 TLB** (8 项直接映射, 收窄版 D1-B, **修订 2026-10-09 per Oracle Q1/Q2 验证**): <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
  - **存储模式**:`std::vector<ch_reg<ch_uint<kVpnBits>>>` + `std::vector<ch_reg<ch_uint<kPaddrBits>>>` + `std::vector<ch_reg<ch_uint<1>>>` (valid), 8 entries each
  - **为什么不用 ch_mem 数组**:`include/cf/plugin/storage.h:76` static_assert 要求 T trivially_copyable, `ch::core::ch_uint<N>` 验证非 trivially_copyable (Oracle 2026-10-09 Q1 compile-verified); `ch_mem::aread` 1-cycle latency 破坏单拍命中语义, 须组合读
  - **铁律** (照抄 `ip/cpu/plugins/reg_file_chmem.h:13-29` 注释): ① 实例成员非 static (跨 context use-after-free 教训) ② lazy init 推迟到 `at_stage` 闭包内 (ch_reg 构造需要活跃 context) ③ 必须显式命名每个 reg (`"tlb_v_0"`, `"tlb_tag_0"` 等), 否则 32 个默认名 `"reg"` 撞名 → Verilator duplicate declaration 错误
  - **结构**: 8 entries 单级直接映射, 全相联 (无 set-associative), 直写 (write-through, 无 write-back), 替换策略 = FIFO (无 LRU, 满足 D1 收窄版)
  - **TLB lookup 组合逻辑**:`valid[i] && tag[i] == vaddr_vpn[i]` 三向比较, 命中出 PADDR (单拍)
- [ ] **复用 PtWalkFsmPlugin**:`#include "ip/cpu/plugins/mmu_ptw_chmem.h"` 直接调其 API,不改其内部 <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **TLB lookup 组合逻辑**: <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
  - 命中:`result_paddr = tlb_paddr_[index]` + `PADDR_VALID = true`
  - miss:启动 PtWalkFsmPlugin (调 `set_input_value(start=1)`) + 等 `done=1` 拍读 `result_ppn`
- [ ] **payload key 发射**:同 B.2 <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **ptw_active gate**: <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
  - miss 时 `PTW_ACTIVE = 1` 同步发射
  - `done` 拍 `PTW_ACTIVE = 0` + 同时 `TLB` refill (单级直写下一项,无 LRU/RRIP)
- [ ] **回归项**:`[chmem]` 既有测试不退化;`[mmu]` 维持 <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **运行命令**:`./build/bin/chipforge_tests_chmem "[chmem]" "[mmu]"` (注意: `[mmu]` 是 TLM,只验证不互相污染) <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **代码自审**:不写 `as any` / `@ts-ignore` / 空 catch (项目硬约束) <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **D4 检查**:不写 enum class.*State 状态机 (有 PTW FSM 走 `CF_PLUGIN_USE_FSM_EXEMPT` 豁免);TLB lookup 是纯组合逻辑 <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->

### B.4 [GREEN] 单元测试 `test_mmu_chmem_basic` (5 case — 2026-10-22 双审由 3 case 扩到 5)

- [ ] **新建文件**:`tests/mmu/test_mmu_chmem.cpp` (~200 LOC) <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **Family tag**:使用 `[mmu][chmem]` (与 `[chmem]` 多 tag 一致) <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **必须手动加入 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES` (`:118-145`)** — 该 list 是**逐条手写**的;GLOB (`:6`) 只覆盖 TLM target ⇒ 不加就**静默不编译不运行** <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **覆盖范围** (2026-10-22 双审扩到 5 case — 原 3 case 无法判别"真 sv32 翻译"与"Bare 回退"): <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
  - **Case 1:TLB hit** — 配置单级 TLB 含一项,lookup 命中 vaddr → 期望 PADDR = TLB.paddr (单周期,不走 PTW)
  - **Case 2:TLB miss** — TLB 全空 + 提供 test memory 接口返回 PTE → 走 PtWalkFsmPlugin → 期望 PADDR = PTE.ppn (2-3 周期,经 FSM)
  - **Case 3:PTW fault** — 提供 test memory 返回 V=0 PTE → 期望 PADDR_VALID = false + EXCEPTION_CODE = 1。**注: Phase B 窗口无 fetch stall 机制 (CH_MEM 数据通路全组合,`cpu_factory_chmem.h:431-435` 无 stage 寄存器) ⇒ 本 case 只能是 TLB 内部单元级,不可做成 pipeline 级**
  - **Case 4:sv32 真翻译 (非恒等页表向量)** — vaddr X → paddr Y (Y ≠ X) → 断言翻译后地址为 Y。**identity page table 无法区分真 sv32 与 Bare 回退 (两者输出恒等) ⇒ 必须非恒等向量**。另断言消费端 `has(MmuKeys::PADDR_VALID) == true` 且 sel 真选 `PADDR` (非 `MMU_VADDR`)
  - **Case 5:Bare 隔离 + satp 边界** — sv_mode=Bare / satp_value=0 / satp_ppn 未编程 → 期望走 identity translation 且不启动 PTW。**注: v0.10.1 deep-rca 记录过"PTW 从不在 window 0x0 读 PTE"同类历史 bug;Bare-shortcut 严格化 (v0.10.4 `a3afcfa`) + CH_MEM 初值注入的交互会埋雷**
- [ ] **变异对照 ≥2 个** (遵循仓库近期惯例,自证测试强度): ① 拔掉 `PADDR_VALID` 接线 → 消费端测试应红; ② TLB 改恒 hit → Case 2 应红 <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **断言数**:15-20 assertions (5 case × 3-4 断言) <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **CH_MEM 测试 lifecycle**:每个 SECTION 入口显式 `set_as_current_context()` (Phase 6c PoC 教训,详 research §9.3) <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **回归项**:A.1-A.6 全部 PASS;`[mmu][chmem]` 不影响 `[chmem]` 既有 baseline <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **运行命令**:`./build/bin/chipforge_tests_chmem "[mmu][chmem]"` <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->

### B.0 [前置] Payload key 复用确认 (0.2 day, 阻塞 B.2-B.5 全部 task)

> **2026-10-22 双审新增** —— 防止"测试全绿但 sv32 从未生效"的静默失效。

- [ ] **实例化参数一致**:`MMUPlugin_chmem` 发射用的 key 必须是 `cf::ip::mmu::payload::mmu_keys<std::uint64_t>` 的**同一静态实例** (与 `ibus_chmem.h:121` / `dmem_chmem.h:142` 指针相等)。`mmu_keys` 是模板 struct + static inline 成员 ⇒ `mmu_keys<uint64_t>` 与 `mmu_keys<uint32_t>` 是**不同实例、不同地址**
- [ ] **不新建 bundles key 文件**:`PayloadStore` 按**指针身份**索引 (`include/cf/plugin/payload.h:124,136,153,166,196,204,220,233` 全部 `cells_.find(&key)`) ⇒ key 对象不同 ⇒ 消费端 `has()` 恒 false ⇒ sel 静默回退 Bare ⇒ **所有测试照 PASS 而 sv32 从未生效**
- [ ] **消费端 `has()` 契约自检**:确认 `ibus_chmem.h:144` / `dmem_chmem.h:144` 的 `payloads().has(MmuKeys::PADDR_VALID)` 在 MMU plugin 注册后返回 true (B.3 实装后复跑)

### B.5 [GREEN] Phase B 集成回归 (chipforge_tests_chmem 全套)

- [ ] **B.2-B.4 全部完成** 后跑 CH_MEM binary 全套 <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **跑**:`./build/bin/chipforge_tests_chmem` (含 `[chmem]`/`[cpphdl]`/`[elaborate]`/`[verilator]`/`[mmu-verilator]` + B.4 新增 `[mmu][chmem]`) <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **预期 PASS**: <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
  - `[chmem] 9/9 PASS`
  - `[cpphdl] 6/6 PASS`
  - `[pipeline2_stall_matrix] 16/16 PASS`
  - `[mmu][chmem] 5/5 PASS` (B.4 新增, 2026-10-22 由 3 case 扩到 5 case)
  - `[mmu-verilator] 3/3 PASS`
- [ ] **bare 路径 byte-identical 检查** — `[verilator] 1/1 case PASS (5 ELF tohost=1)` 维持 (Oracle D2-A 强调) <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **CH_MEM known issue 跟踪**:`cpphdl_poc_chbool_contextual_conversion` 全量跑受 context pollution 失败 (单独跑 PASS) 已知 — 不阻塞本 Phase,需 PoC follow-up 修 <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->
- [ ] **本 Phase 完成标志**:全 PASS + bare 路径 byte-identical <!-- sprint-2-3: 2026-10-23 ~ 2026-11-06 ~ owner=mfc-chmem (2026-10-22 双审提前, 15d) -->

---

## Phase C — Pipeline 集成 (D3-A 最小 + byte-identical 保护)

> **scope 边界**:
> - MmuExceptionHandler combinational network = 最小 (单条 `mmufault_clear = (CPU_EXCEPTION_CODE != 0)`,**无 trap PC 跳转**)
> - IBus/DBus sv32 消费 = 改 fetch/load/store 路径,但 bare 模式必须 byte-identical (Oracle D2-A 强调)
> - HazardPlugin CH_MEM = 实装 `mark_mmufault`/`clear_mmufault` + `CtrlLink::halt_when`
>
> **本 Phase 完成标志**:`[cpu-l1-mmu-demo] 7/7 PASS` (6 旧 + 1 新 TEST_CASE 7 真 sv32 翻转) + `[cpu-integration] 81/81` + bare 路径零退化

### C.1 [GREEN] 实装 `mmu_exception_handler_chmem.h` combinational network (D3-A 最小)

- [x] **修改文件**:`ip/cpu/plugins/mmu_exception_handler_chmem.h` (现 64 行 stub → ~120 LOC) (skeleton shipped, wiring pending)
  - **2026-10-14 现状**: commit `1429f5a` (C.1 stall_ctrl_ skeleton) + commit `5403718` (拆 early-return + 注释对齐 cpu-pipeline-tick-rca) 已落地
  - **已落地**:`#ifndef CF_IP_CPU_PLUGINS_MMU_EXCEPTION_HANDLER_CHMEM_H` 头守卫 + `stall_ctrl_` setup() 注册到 stage `"decode"` + `at_stage("memory", NORMAL, [...])` 闭包注册
  - **未落地 (wiring 段, Phase B post-merge)**:`CPU_EXCEPTION_CODE` 接线 (`mmu_exception_handler_chmem.h:85-90` 显式注释 "接线点 (Phase B post-merge)")
- [ ] **保持 `#ifndef CF_IP_CPU_PLUGINS_MMU_EXCEPTION_HANDLER_CHMEM_H` 头守卫** (skeleton shipped)
- [ ] **wiring 实装** (Phase B post-merge):
  - 在 `build()` 注册 `at_stage("memory", NORMAL, [...])` 闭包 (不在 `setup()` — `setup` 应早返回,Plugin 框架约定) (skeleton shipped)
  - 闭包内读 `cpu_keys<T>::CPU_EXCEPTION_CODE` (ch_uint<8> in CH_MEM mode) ← **wiring 段待 Phase B**
  - 计算 `ch_bool mmufault_clear = (cpu_keys::CPU_EXCEPTION_CODE != 0)` (D3-A 最小) ← **wiring 段待 Phase B**
  - 输出 `mmufault_clear` 到 `HazardPlugin::stall_ctrl_->halt_when(...)` ← **wiring 段待 Phase B; 须先解 CtrlLink toggle API gap (FP-4)**
- [ ] **保持 API 兼容**:`setup`/`build`/`reset`/`mark_mmufault`/`clear_mmufault`/`mmufault_pending` API 不变 (TLM 版对齐)
  - `mmufault_pending()` 改为返回 `mmufault_clear` 当前值 (组合逻辑直接读 ch_bool) ← **wiring 段待 Phase B**
  - `mark_mmufault()` / `clear_mmufault()` 仍为 no-op (combinational network 不需 state)
- [ ] **ADR-046 豁免不适用** — 本 Plugin 无状态机 (纯组合逻辑),**不加** `CF_PLUGIN_USE_FSM_EXEMPT`
- [ ] **回归项**:C.5 HazardPlugin 同步;`[cpu]` 既有不退化
- [ ] **运行命令**:`./build/bin/chipforge_tests "[cpu]" "[mmu]"` + `./build/bin/chipforge_tests_chmem`

### C.2 ✅ **[已完成 2026-10-16, Phase 2 Task 2.1]** 实装 `ibus_chmem.h` sv32 消费 (D2-A + bare byte-identical)

> **2026-10-17 收档状态**:commit `9373808` (feat cpu) + `b8d860f` (docs plan) + `tests/cpu/test_ibus_chmem_sv32.cpp` (240 LOC, `[cpu][chmem][ibus-sv32][mmu-chmem]` 2/2 PASS, 976 assertions) 已落地。Tasks 描述里列的"完整形式"全部完成(含 sv32 消费 sel 表达式 + PTW_ACTIVE 期间 fetch stall 注释已落)。Phase B 真 sv32 翻译 enable 后,wiring 段需 Phase 2.x follow-up 跟进(主要 fetch_stall protocol v0.2 + CtrlLink toggle API gap 解锁)。

- [x] **修改文件**:`ip/cpu/plugins/ibus_chmem.h` (现 ~225 LOC, 实际产出 ~10 行 net add `select(paddr_valid, paddr, mmu_vaddr)` + RED 单测 240 行)
- [x] **bare 模式 byte-identical 保护** (Oracle D2-A 强约束):
  - 原 `ibus_chmem.h:128-129` 直接 `aread(bits<15,2>(pc_val))` 已替换为 `aread(bits<15,2>(...sel...))` — `select(paddr_valid, paddr, mmu_vaddr)` 在 bare 模式(`PADDR_VALID=false` 时未注册 MMU plugin)回退到 vaddr 路径,与原 byte-identical
- [x] **sv32 消费实装**:
  - 读 `mmu_keys<T>::MMU_VADDR` + `PADDR_VALID` (从 tlb_lookup_ifetch substage)
  - 当 `PADDR_VALID=false` → fallback to `mmu_vaddr` (Bare/未翻译路径, byte-identical with original pc_val)
  - Phase B 后(`MMUPlugin_chmem` 注册): 当 `PADDR_VALID=true` → sel 选 `paddr` → 真翻译 (Sprint 2-5 follow-up)
- [x] **PTW_ACTIVE 期间 fetch stall 注释与依赖**:已落 `hazard_chmem.h:151-153` CtrlLink toggle API gap documented (FP-4);fetch stall 接线待 Sprint 6-7 解 API gap 后实装
- [x] **ADR-048 canonical ordering** — `ibus_chmem.h` 的 `at_stage("fetch", NORMAL, ...)` 已在 MMU substage 之后注册(`register_early_plugins()` 序列验证);与 `wave5-bp-btb` owner 的 fetch stage substage 协商在 Sprint 2-5 启动期解决
- [x] **回归项**:`[verilator] 1/1 case PASS` 维持 (bare 模式 byte-identical);`[cpu-integration] 81/81` 维持;[ibus-sv32] 2/2 PASS (新增 RED, 976 assertions)
- [x] **运行命令**:`./build/bin/chipforge_tests "[cpu-integration]"` + `./build/bin/chipforge_tests_chmem "[verilator]"` + `./build/bin/chipforge_tests "[cpu][chmem][ibus-sv32]"`

### C.3 ✅ **[已完成 2026-10-16, Phase 2 Task 2.2]** 实装 `dmem_chmem.h` sv32 消费 (D2-A + bare byte-identical)

> **2026-10-17 收档状态**:commit `5b9f74a` (feat cpu, dmem_chmem sv32 消费 + RED 单测) 已落地。`tests/cpu/test_dmem_chmem_sv32.cpp` (240 LOC, `[cpu][chmem][dmem-sv32][mmu-chmem]` 2/2 PASS, 972 assertions) 已新增。Pre-existing baseline 退化 (chmem_vendored 18 ELF FAIL) 已知与本 change 无关 (属 feat-phase-6d-rtl-verification, Phase 6d.7),不阻塞 archive。sv32 真翻译 sel 路径就绪,等 Phase B MMUPlugin_chmem 注册后自动激活。

- [x] **修改文件**:`ip/cpu/plugins/dmem_chmem.h` (现 ~190 LOC, 实际产出 `select(paddr_valid, paddr, vaddr_byte_addr)` + RED 单测 240 行 + bare byte-identical 注释落位)
- [x] **bare 模式 byte-identical 保护**:`dmem_chmem.h:135-137` 注释 + sel 表达式确认 Bare 路径与未改前 byte-identical
- [x] **sv32 消费实装** (load/store 路径):
  - 读 `mmu_keys<T>::MMU_VADDR` + `PADDR_VALID` (从 memory stage 的 tlb_lookup_loadstore substage)
  - 当 `PADDR_VALID=false` → fallback to `vaddr_byte_addr` (Bare/未翻译路径, byte-identical)
  - Phase B 后: 当 `PADDR_VALID=true` → sel 选 `paddr` → 真翻译 (Sprint 2-5 follow-up)
- [x] **store data path 同步**:`byte_addr = select(paddr_valid, paddr, vaddr_byte_addr)` 复用 C.2 同一表达,RMW mask 路径不受影响 (`byte_off`/`half_off` 仍从 byte_addr 低位取)
- [x] **PTW_ACTIVE 期间 memory stall 注释与依赖**:`hazard_chmem.h:151-153` 已 documented CtrlLink API gap (与 C.2/C.5 共用);C.6 集成测试待 Sprint 2-5 后续
- [x] **回归项**:[dmem-sv32] 2/2 PASS (新增 RED, 972 assertions);[ibus-sv32] 2/2 PASS 维持 (无退化);4 arch gate PASS
- [x] **运行命令**:`./build/bin/chipforge_tests "[cpu][chmem][dmem-sv32]"` + `./build/bin/chipforge_tests_chmem "[verilator]"`

### C.4 [GREEN] 修复 `cpu_factory_chmem.h` owner 真空 (移除 "owner TBD" 注释)

- [x] **修改文件**:`ip/cpu/cpu_factory_chmem.h:80-85` (skeleton shipped, wiring pending)
  - **2026-10-14 现状**: commit `8bb55a9` (C.4 owner attribution) 已落地
  - **已落地**:`:80-85` owner claim 段从字面 "owner 尚未分配" 改写为 `v0.11.0 owner: change mmu-chmem-pipeline-integration; Phase B deferred to 2026-12-01 per proposal §11; plumbing-only no-op stub`
  - **未落地 (wiring 段)**: 删除 `:117-126` 运行期 `fprintf` (仍写 "Real sv32 CH_MEM owner TBD") — 真真空活证, 与 mmufault_clear 接线同步 (Phase B post-merge)
- [ ] **删除 :117-126 运行期 `fprintf`** (Phase B post-merge):
  ```
  // 现 9 行 (line 117-125):
  //   if (enable_mmu.value_or(false)) {
  //     const std::string mode = mmu_mode.value_or("bare");
  //     std::fprintf(stderr,
  //         "MMU hook enabled (mode=%s) — plumbing-only no-op stub; "
  //         "mmu_chmem.h not implemented. NO translation occurs. "
  //         "Real sv32 CH_MEM owner TBD; see change verilator-mmu-bare-plumbing-e2e "
  //         "(CLI plumbing only) + cpu-pipeline-mmufault-handler.\n",
  //         mode.c_str());
  //   }
  ```
- [ ] **替换为新逻辑** (实装后):
  ```
  //   enable_mmu  (v0.11.0 owner: mmu-chmem-pipeline-integration)
  //     — std::nullopt (默认): 与原 4 参数行为逐位等价 (无 MMU plugin 注册)
  //     — true:               register MMUPluginChmem (single-level TLB + sv32 PTW FSM)
  //                            emit satp config from cfg.satp_value | cfg.satp_ppn
  //     — false:              显式禁用,无 plugin (与 nullopt 等价)
  ```
- [ ] **实装 cfg.enable_mmu=true 路径** — 调用 `cpu_factory_chmem` 注册 `MMUPluginChmem` + `RiscvMMUPluginChmem` 到 `PipeBuilder` ← **wiring 段待 Phase B**
- [ ] **回归项**:`[chmem]` 既有 baseline 维持;`[mmu][chmem]` (B.4 新增) PASS
- [ ] **运行命令**:同 C.2

### C.5 [GREEN] HazardPlugin CH_MEM 接 mmufault_clear (C1 配套)

- [x] **修改文件**:`ip/cpu/plugins/hazard_chmem.h:151-153` (现 stub 区域) (skeleton shipped, wiring pending)
  - **2026-10-14 现状**: commit `06bb1d3` (C.5 hazard_chmem mmufault_pending state skeleton) 已落地
  - **已落地**:`mark/clear/pending` 维护 instance state (`hazard_chmem.h:156-162` 现)
  - **未落地 (wiring 段)**: 调 `stall_ctrl_->halt_when(...)` — `hazard_chmem.h:152-154` 自承 "未实际调 CtrlLink 是 CtrlLink API gap (无 toggle)"; 真 stall 接线须先解 API gap (FP-4)
- [ ] **替换 mark/clear mmufault stub** (Phase B post-merge, 须先解 CtrlLink toggle API gap):
  - 现 `void mark_mmufault() noexcept {}` → 改为调 `stall_ctrl_->halt_when(ch_bool(true))` (实际触发 stall)
  - 现 `void clear_mmufault() noexcept {}` → 改为调 `stall_ctrl_->halt_when(ch_bool(false))` (实际取消 stall)
  - 现 `bool mmufault_pending() const noexcept { return false; }` → 改为返回当前 stall_ctrl_ 状态
- [ ] **依赖 C.1** — MmuExceptionHandler combinational network 输出 `mmufault_clear` 到 `HazardPlugin::stall_ctrl_` ← **wiring 段待 Phase B**
- [ ] **回归项**:`[cpu] 19/19` 维持;`[cpu-integration] 81/81` 维持 (无 sv32 fault 测试 PASS)
- [ ] **运行命令**:同 C.2

### C.6 [GREEN] 集成测试 `test_cpu_chmem_mmu_pipeline` (真 sv32 PTE ELF + plant_identity_page_table + tohost=1)

- [ ] **新建文件**:`tests/cpu/test_cpu_chmem_mmu_pipeline.cpp` (~200 LOC)
- [ ] **Family tag**:使用 `[cpu-integration][mmu-chmem]`
- [ ] **测试场景**:
  - 用 `MMUPluginChmem` + `RiscvMMUPluginChmem` + 5-阶段 CPU + identity page table
  - ELF:从 `tests/cpu/manual_elf/` (D3 完成) 取 `add` 或 `addi` ELF
  - 期望:`tohost == 1` within 10000 cycles
- [ ] **断言数**:8-12 assertions (PC fetch, translation lookup, tohost=1, cycles count)
- [ ] **CH_MEM 测试 lifecycle**:每 SECTION 入口显式 `set_as_current_context()` (研究 §9.3)
- [ ] **回归项**:B.5 全部 PASS;C.1-C.5 全部 PASS
- [ ] **运行命令**:`./build/bin/chipforge_tests_chmem "[cpu-integration][mmu-chmem]"`

---

## Phase D — 验证与回归

> **本 Phase 完成标志**:`[cpu-l1-mmu-demo] 7/7 PASS` (含新增 TEST_CASE 7 真 sv32 翻转) + `[mmu-verilator]` TEST_CASE 4 PASS (本 change 内的第 4 个 TEST_CASE, 现状 3/3 baseline + 1 新 sv32 e2e flip) + 4 个 Architecture Gate 脚本 0 失败 + `tools/run_chipforge_tests.sh --all` 全 PASS

### D.0 ~~[GREEN] 修订 proposal 文本 (Oracle R14)~~ — **已完成 2026-10-08**

> **完成证据**: `openspec/changes/mmu-chmem-pipeline-integration/proposal.md` 已修订:
> - §4 删除 "trap PC 跳转断言" AC,替换为 "EXCEPTION_CODE 正确传播 + pipeline stall"
> - §1 删除 "trap PC 跳转",改为 "输出 mmufault_clear"
> - §11 新增 D1 收窄 scope 显式声明 (out-of-scope 列表)
> - §0.1 revision log 记录 7 处定向修订
> - §Tasks 占位符替换为引用本 tasks.md

### D.1 [GREEN] `test_mmu_bare_plumbing_verilator.cpp` TEST_CASE 4 sv32 e2e flip

- [ ] **修改文件**:`tests/mmu/test_mmu_bare_plumbing_verilator.cpp` (现 3/3 PASS, 2026-10-14 修订: 实测 3 TEST_CASE, 历史 proposal "5/5" 是误录)
- [ ] **新增 TEST_CASE 4**:`mmu_sv32_translation_verilator_e2e_flipped` (2026-10-14 修订: 编号应为 4, 第 4 个新 case; spec.md 已用此编号; 2026-10-17 收档期再修订: 全文件 TEST_CASE 6 → 4 已统一)
- [ ] **实现要点**:
  - `popen("cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf sv32_pte.elf --cycles 5000")`
  - 读 `sv32_pte.elf` (D3 完成 vendor; 1 个新 ELF)
  - 断言 `tohost == 1` (1/1 sv32_pte.elf; 2026-10-14 修订: 历史"5/5 ELF"是误录, 实测仅 vendor 1 个 sv32 ELF)
  - 断言 cycle ≤ `sv32-baseline-matrix.csv` × 1.5
  - 断言 `EXCEPTION_CODE` 正确传播 (新 AC 替代原"trap PC 跳转断言"; 须先扩 `sim_main.cpp` 输出 EXCEPTION_CODE, 否则物理不可观测)
  - 断言 pipeline stall 在 PTW_ACTIVE=1 时正确生效 (同上: 须先扩 `sim_main.cpp` 输出 PTW_ACTIVE 计数)
- [ ] **断言数**:8-12 assertions
- [ ] **回归项**:`[mmu-verilator]` 既有 3/3 维持;`[verilator] 1/1` 维持 (2026-10-14 修订: baseline 5/5 → 3/3)
- [ ] **运行命令**:`./build/bin/chipforge_tests_chmem "[mmu-verilator]"`

### D.2 [GREEN] `test_cpu_l1_mmu_demo.cpp` TEST_CASE 7 真 sv32 翻转 (承接 mmufault Part a)

- [ ] **修改文件**:`tests/soc/soc-cpu-l1-mmu-demo/test_cpu_l1_mmu_demo.cpp` (现 6/6 PASS)
- [ ] **新增 TEST_CASE 7**:`cpu_l1_mmu_demo_sv32_translation_flipped`
- [ ] **实现要点**:
  - `cfg.enable_mmu = true` (现 6/6 用例均 false)
  - `cfg.satp_ppn = (window_base+60*1024)>>12` (需经 A5 验证是否需修正)
  - `plant_identity_page_table(mem, window_base+60*1024, elf.entry_addr)`
  - 期望:`tohost == 1` within 10000 cycles
- [ ] **断言数**:6-8 assertions
- [ ] **回归项**:`[cpu-l1-mmu-demo]` 既有 6/6 维持;7/7 PASS 是本 change 硬指标
- [ ] **运行命令**:`./build/bin/chipforge_tests "[cpu-l1-mmu-demo]"`

### D.3 [GREEN] sv32 PTE ELF vendor — `build_sv32_pte.S`

- [ ] **新建文件**:`tests/cpu/manual_elf/build_sv32_pte.S` (~50 LOC RISC-V 汇编)
- [ ] **功能**:生成含 sv32 PTE chain 的 ELF (root page table + level-1 PTE 指向 code 页)
- [ ] **register build_sv32_pte in build_manual_elf.sh** — 修改 `tests/cpu/manual_elf/build_manual_elf.sh`
- [ ] **回归项**:5 现有 manual_elf (`add`/`addi`/`auipc`/`jal`/`beq`) 不影响
- [ ] **运行命令**:`bash tests/cpu/manual_elf/build_manual_elf.sh sv32_pte`

### D.4 [GREEN] sv32 baseline CSV — 5 ELF × median cycle × mode=sv32_mmu

- [ ] **新建文件**:`soc/cpu/docs/dse/sv32-baseline-matrix.csv`
- [ ] **格式**:与 `rv32ui-baseline-matrix.csv` 同构 (5 rows × columns)
- [ ] **生成方式**:D1 TEST_CASE 4 跑 5 ELF 各 N 次 (建议 N=20),取 median cycle
- [ ] **列字段**:`elf_name` / `median_cycle` / `min_cycle` / `max_cycle` / `stddev` / `mode=sv32_mmu` / `mmu_pte_pages`
- [ ] **回归项**:基线建立后,D1 TEST_CASE 4 用 `cycle ≤ median × 1.5` 断言
- [ ] **运行命令**:手动跑 5 ELF 收集数据,生成 CSV

### D.5 [GREEN] Architecture Gate Final — 4 个脚本 0 失败

- [ ] **跑 `bash tools/verify_adr.sh`** — 必须 0 失败 (ADR-037 v2.0 验证、ADR 引用一致性)
- [ ] **跑 `bash tools/verify_plugin_decision.sh`** — 必须 0 失败 (D4 业务代码 7 项检查)
- [ ] **跑 `bash tools/check_plugin_portability.sh`** — 必须 0 失败 (ADR-040 v2.0 4 项检查)
- [ ] **跑 `bash tools/doc_link_check.sh`** — 必须 0 失败 (文档死链)
- [ ] **若有失败**:
  - 是本 PR 引入 → 修复
  - 是历史问题 → 记录但不阻塞 archive
- [ ] **运行命令**:上面 4 个 bash 命令,顺序跑

### D.6 [GREEN] Regression Final Check — `tools/run_chipforge_tests.sh --all`

- [ ] **跑**:`bash tools/run_chipforge_tests.sh --all` (TLM + CH_MEM + Verilator + gate,~167s)
- [ ] **预期 PASS**:
  - TLM binary (432 catch2 instances / 67026 assertions):`[framework] 89/89` + `[cache]` + `[cpu] 19/19` + `[cpu-integration] 81/81` + `[cpu-l1-mmu-demo] 7/7` + `[bundles]` + `[mmu] 53/53` + `[riscv-tests] 40/48` + `[soc]` + `[tlb-refill]`
  - CH_MEM binary:`[chmem] 9/9` + `[cpphdl] 6/6` + `[elaborate]` + `[verilator] 1/1` + `[mmu-verilator] 3/3` + `[mmu][chmem] 5/5` (B.4 新增) + `[cpu-integration][mmu-chmem] 1/1` (C.6 新增)
  - Verilator smoke:`cpu_verilator_sim_add` 5/5 PASS (2026-10-14 修订: 历史 5/5 快照; 当前 cpu_verilator_sim 扩 48 ELF = 40 rv32ui + 8 rv32um, 96 assertions, 详见 AGENTS.md v0.10.4 实测; 此处 cpu_verilator_sim_add 走的是 ctest entry 单 ELF smoke, 数字与全量 48 ELF 不可互换)
- [ ] **若失败**:
  - 是本 PR 引入 → 修复
  - 是已知 flake (如 `cpphdl_poc_chbool_contextual_conversion`) → 记录
- [ ] **本 Phase 完成标志**:全 PASS (含新引入的 `[mmu][chmem]` 3/3 + `[cpu-integration][mmu-chmem]` 1/1)

---

## Phase E — 文档与归档

> **本 Phase 完成标志**:`AGENTS.md` workaround 移除 + spec.md 新增 2 个 Requirement + `openspec change validate` 0 error + `CHANGELOG.md` v0.11.0 段新增条目 + `openspec archive` 成功

### E.1 [GREEN] `AGENTS.md` L41 workaround 移除 + `[v0.11.0 follow-up]` 标记

- [ ] **修改文件**:`AGENTS.md` 第 41 行附近 (`[cpu-l1-mmu-demo]` 段)
- [ ] **当前 workaround 标记**:`**[v0.10.1 deep-rca (`debug-cpu-l1-mmu-demo-deep-rca`)]** 修复: 测试 cfg.enable_mmu=false...`
- [ ] **替换为**:`**[v0.11.0 follow-up: mmu-chmem-pipeline-integration]** — 实装完成, TEST_CASE 7 真 sv32 翻转 PASS (7/7)`
- [ ] **回归项**:AGENTS.md L41 改动是文档,不影响测试
- [ ] **前置**:D.2 TEST_CASE 7 真正 PASS 后再 E1

### E.2 [GREEN] `openspec/specs/verilator-mmu-bare-plumbing/spec.md` 新增 Requirement "TEST_CASE 4"

- [ ] **修改文件**:`openspec/specs/verilator-mmu-bare-plumbing/spec.md`
- [ ] **新增 Requirement**:
  ```markdown
  ## Requirement: mmu_sv32_translation_verilator_e2e_flipped

  WHEN Verilator runner enables sv32 mode via `--enable-mmu --mmu-mode sv32` flag with sv32 PTE ELF,
  THE SYSTEM SHALL achieve tohost=1 within cycle budget ≤ sv32 baseline × 1.5,
  AND correctly propagate EXCEPTION_CODE through pipeline,
  AND correctly stall fetch in PTW_ACTIVE=1 cycles.

  #### Scenario: sv32_translation_verilator_e2e_flipped
  - GIVEN sv32 PTE ELF (build_sv32_pte.S vendored)
  - WHEN popen `cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf sv32_pte.elf --cycles 5000`
  - THEN tohost == 1
  - AND cycle count ≤ sv32-baseline-matrix.csv × 1.5
  - AND EXCEPTION_CODE correctly propagated (0 in success case)
  - AND fetch stalled during PTW_ACTIVE=1 cycles
  ```
- [ ] **回归项**:`openspec change validate verilator-mmu-bare-plumbing` 0 error (若 spec 与 change 同名)

### E.3 [GREEN] `openspec/specs/cpu-l1-mmu-demo/spec.md` 新增 Requirement "TEST_CASE 7"

- [ ] **修改文件**:`openspec/specs/cpu-l1-mmu-demo/spec.md`
- [ ] **新增 Requirement**:
  ```markdown
  ## Requirement: cpu_l1_mmu_demo_sv32_translation_flipped

  WHEN CPU+MMU demo config flips cfg.enable_mmu=true with identity page table,
  THE SYSTEM SHALL achieve tohost=1 within 10000 cycles.

  #### Scenario: cpu_l1_mmu_demo_sv32_translation_flipped
  - GIVEN cfg.enable_mmu=true, cfg.satp_ppn set, plant_identity_page_table
  - WHEN run ELF (e.g., add or addi from manual_elf/)
  - THEN tohost == 1 within 10000 cycles
  ```
- [ ] **移除 workaround 标注** (现 spec.md 可能含 v0.10.1 workaround 描述)

### E.4 [GREEN] `openspec change validate mmu-chmem-pipeline-integration` 0 error

- [ ] **跑**:`openspec change validate mmu-chmem-pipeline-integration`
- [ ] **若有错误**:
  - proposal.md frontmatter 缺字段 → 补
  - tasks.md 缺 checkbox → 补 (但本 tasks.md 用 `[ ]` 而非 OpenSpec 要求的 `- [ ]` 格式?)
  - spec.md 缺 Scenario → 补
- [ ] **本任务完成标志**:validate 0 error

### E.5 [GREEN] `CHANGELOG.md` v0.11.0 段新增条目

- [ ] **修改文件**:`CHANGELOG.md`
- [ ] **在 v0.11.0 段 (现可能未存在,需先建) 新增**:
  ```markdown
  ## v0.11.0 (2027 Q1 启动期, Wave 5 ISA coverage + BP)

  ### mmu-chmem-pipeline-integration

  - **CH_MEM MMU/PTW pipeline 集成** (5-阶段 CPU + sv32 真翻译路径)
  - **Bare shortcut 严格化**: 修复 `MMUPlugin.cpp:109` 误判条件
  - **Shadow 双 bug 修复**: 删除派生 `set_satp_value`/`satp_ppn` shadow + 48-bit mask
  - **MMUPlugin_chmem.h 收窄版**: 单级 ch_mem TLB + 复用 PtWalkFsmPlugin
  - **TEST_CASE 4**: Verilator sv32 e2e flip (5 ELF tohost=1)
  - **TEST_CASE 7**: cpu-l1-mmu-demo 真 sv32 翻转 (7/7 PASS)
  - **CH_MEM known limitation**: 无 CSR plugin,trap PC 跳转推迟 v0.11.1+

  Migration: 既有 `[mmu]` baseline 维持;`[cpu-l1-mmu-demo]` 6/6 → 7/7 PASS。
  ```
- [ ] **回归项**:CHANGELOG 是文档,不影响测试

### E.6 [GREEN] `openspec archive mmu-chmem-pipeline-integration -y`

- [ ] **前置确认**:D.6 + E.5 全部完成
- [ ] **跑**:`openspec archive mmu-chmem-pipeline-integration -y`
- [ ] **预期效果**:本 change 从 `openspec/changes/` → `openspec/changes/archive/`
- [ ] **本任务完成标志**:archive 命令 0 退出码

---

## F7 re-evaluate 任务 (2026-10-15, csr-chmem-writeback-complete post-archive)

> **触发条件**: `csr-chmem-writeback-complete` (commit `1d02531`, 2026-10-15 archived) 已**实装 `RiscvCsrPluginChmem<T>` 第 8 plugin** (注册位置 `ip/cpu/cpu_factory_chmem.h:321`), R3.5 写通路完整版就绪 (7 个 M-mode CSR 经 CSRRW 实际可写, mcause/mepc/mtvec trap 路径实装, TRAP_PENDING/TRAP_TARGET payload 已 propagate). **R11 假设"CH_MEM 流水线无 CSR plugin → trap PC 跳转物理不可能"已事实失效**.

### 待 re-evaluate 项 (mmu-chmem 推进 Phase B/C 启动前必做)

- [ ] **R11.1** trap PC 跳转可行性重新评估
  - 旧决策 (proposal.md:88 + HANDOFF.md:24): D3-A 锁定仅 stall, 无 trap PC 跳转, defer v0.11.1+
  - 新事实: TRAP_PENDING/TRAP_TARGET payload + mepc/mtvec/mcause 7 个 CSR 可写已就绪 (`openspec/changes/csr-chmem-writeback-complete/` archived)
  - 行动: Phase C.1 (Pipeline 集成) 启动时重审 D3-A 决策, 评估是否能在本 change 内实装 trap PC 跳转 (ibus_chmem.h NORMAL select 覆写 PC 节点). 若可, 提前 R2.3 实现 (独立 change `csr-chmem-trap-pc-jump` 候选)
  - owner: mmu-chmem owner 启动 Phase C 时执行
  - **不阻塞** mmu-chmem Phase A/B (根因 bug 修复 + MMUPlugin CH_MEM 实装), 仅影响 Phase C 决策

- [ ] **R11.2** 批量更新 R11 表述 (7 处)
  - `HANDOFF.md:24` (D3 行)
  - `HANDOFF.md:133` (R11 索引行)
  - `proposal.md:69` (§4 末尾 AC 矛盾)
  - `proposal.md:88` (D3-A 物理不可能)
  - `proposal.md:125` (AC 修订 R14)
  - `proposal.md:191` (v0.11.1+ 候选)
  - `research-v0.11.0-prep.md:250,412,447,460` (R11 索引 4 处)
  - 修订模式: "(CH_MEM 无 CSR plugin, 物理不可能) → (CH_MEM CSR plugin 已实装 per csr-chmem-writeback-complete, R3.5 写通路完整; trap PC 跳转待 R11.1 re-evaluate 决策)"
  - 优先度: 中 (不影响功能, 影响读者对当前架构状态的认知)

- [ ] **R11.3** D3-A 多 driver 仲裁扩展点 (与 csr-chmem-writeback-complete F6 注释登记联动)
  - 旧实装: csr_chmem.h writeback 闭包 mcause/mepc 仲裁仅 2 driver (writeback-NORMAL vs branch-LATE)
  - 待实装: mmu_exception_handler_chmem.h (mmu-chmem Phase D) 写 mcause=exception_code, mepc=faulting_pc → 3 driver 仲裁
  - 扩展模式: `is_trap_branch || mmufault_active → mcause = trap 或 mmufault 值`
  - owner: mmu-chmem Phase D 实装时同步改 csr_chmem.h 仲裁 select
  - **触发前置条件**: mmu-chmem Phase D 启动时

### 跨 change 同步

- **已落地 (本 change)**: `ip/cpu/plugins/csr_chmem.h:337-360` writeback 闭包注释显式登记 F6 扩展点
- **待落地 (mmu-chmem owner)**: R11.1/R11.2/R11.3 任一启动时

---

## 总进度追踪

| Phase | Tasks | 估时(人天) | 状态 |
|-------|-------|-----------|------|
| A 根因 Bug 修复 | A.1, A.2, A.3, A.3.1, A.4, A.5, A.6 | 5 | ✅ verified 2026-10-10 (commit `a3afcfa` 落地代码 + 测试) |
| B MMUPlugin CH_MEM | B.0, B.2, B.3, B.4, B.5 (B.1 跳过) | 15 | 🟡 待启动 (2026-10-22 双审: 窗口提前到 Sprint 2-3 10-23 ~ 11-06, 估时由 10d 回改 15d —— 原 10d 丢了 B.5 的 5d 集成回归; owner = mfc-chmem; 前置 = `ctrl-link-chmem-reg-handle` 的 A1/A2 stateful halt 通道) |
| C Pipeline 集成 | C.1, C.2 ✅, C.3 ✅, C.4, C.5, C.6 | 11 | 🔵 部分收丰 2026-10-17 (C.2/C.3 skeleton+sel 已落 commit `9373808`+`5b9f74a`; C.1/C.4/C.5 skeleton shipped + wiring 待 Sprint 2-5) |
| D 验证与回归 | D.0 ✅, D.1, D.2, D.3, D.4, D.5, D.6 | 5 (D.0 已完成) | 🔵 D.0 done, 余待启动 |
| E 文档与归档 | E.1, E.2, E.3, E.4, E.5, E.6 | 3 | ⏸ 待启动 (触发: D 阶段全绿后) |
| **合计** | **29 (C.2/C.3 + 部分 skeleton = ≈ 4 子任务完成, 25 待启动)** | **34 (≈ 6.0-6.2 人周)** | **Phase A ✅ + Phase C 部分 ✅ + follow-up 占位** |

---

## 协调项 (启动期)

1. **A5 优先执行** (Oracle R12):TLM flip 尝试 2 PPN 计算核实,可能是比 shadow 更便宜的 root cause
2. **与 `wave5-bp-btb` 协调** (Oracle 细化 3 项,research §12.5):
   - pc_reg next-mux 优先级:`stall > branch actual > BP predict`
   - stall 语义:BP 冻结期间不推测
   - fetch stage at_stage 注册顺序:MMU substage 先于 IBus fetch NORMAL
3. **与 `cpu-pipeline-mmufault-handler` v1 archive 协调**:本 change depends_on 此 archive
4. **CH_MEM 已知限制传播**:trap PC 跳转 / 多级 TLB / Sv39/48 推迟 v0.11.1+,spec.md 同步记录

---

**文档版本**: v1.0 Draft (tasks-ready)
**参考**:research-v0.11.0-prep.md §4 (Oracle 调整后) + §12 (Oracle 决议详情)
**作者注释**:本文档基于 2026-10-08 Oracle 决议起草,所有任务均按 research §4 估时表 + 优先级排序。

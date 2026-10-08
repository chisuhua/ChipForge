# mmu-chmem-pipeline-integration — Owner Handoff Note

> **状态**: 📋 v0.11.0 启动期待激活
> **激活前置**:
> - ✅ v0.10.x `cpu-pipeline-mmufault-handler` v1 archive (depends_on)
> - ✅ v0.10.x `phase-6d-prerequisites` archive (depends_on)
> - ✅ `wave5-bp-btb` owner fetch stage 协调协议达成 (同 v0.11.0 窗口)
> **激活方式**: 启动期 owner 读完本文 + tasks.md + research-v0.11.0-prep.md §6 + §12 后,按 tasks.md Phase A → E 顺序执行

---

## 0. 一句话定位

`mmu-chmem-pipeline-integration` 关闭 CH_MEM MMU 集成层 owner gap (`cpu_factory_chmem.h:80-85` 自证 *"Real sv32 CH_MEM owner TBD"*),实装单级 TLB + 复用 PtWalkFsmPlugin 的 sv32 翻译路径,接入 IBus/DBus/HazardPlugin 流水线,修复 Bare shortcut + shadow 双 bug,达成 `[cpu-l1-mmu-demo] 7/7 PASS` + Verilator `[mmu-verilator] 4/4 PASS` 硬指标。

---

## 1. Oracle 2026-10-07 锁定 5 项决议 (tasks.md 必须严格遵守)

| ID | 决议 | 不可妥协点 |
|----|------|-----------|
| **D1** | MMUPlugin_chmem.h scope = **B 收窄版** | 单级 ch_mem TLB + 复用 PtWalkFsmPlugin。**多级 TLB / LRU/RRIP / ASID≠0 / Sv39/48 / 权限检查 / megapage 完整 PPN 拼接 全部 out-of-scope** (详见 proposal §11) |
| **D2** | IBus/DBus sv32 消费 = **v0.11.0 内** | 真翻译路径在 TEST_CASE 6 (Verilator) 而非 TEST_CASE 7 (TLM);**bare 路径 byte-identical 保护** (zero sv32-induced latency) |
| **D3** | mmu_exception_handler combinational = **最小** | `mmufault_clear = (CPU_EXCEPTION_CODE != 0)` 单条;**无 trap PC 跳转** (CH_MEM 无 CSR plugin,物理不可能) |
| **D4** | Bare shortcut 公式 = **删除析取项** | 删 `MMUPlugin.cpp:109` 的 `\|\| satp_ppn_ == 0`。**不重写判定逻辑** |
| **D5** | csr_write_satp 注入 = **C (TLM)** + **shadow 双 bug 修复** | 删除 `mmu.h:99-100,103` 派生三件套,让基类存取器被继承;CH_MEM 侧 satp 必须是 ctor/config 参数 (elaboration 期固化) |

---

## 2. ✅ A5 已执行完成 (2026-10-08) — Oracle R12 PPN 验证

> **A5 已由 owner 启动期执行 (session 2026-10-08),Phase B 起点已确认,无阻塞。**

**原假设 (Oracle R12, 保留为审计记录)**: proposal §Why 转述尝试 2 PPN = `0x80000`,但 `(0x80000000+0xF000)>>12 = 0x8000F` (≠ `0x80000`)。若转述准确,PTW root 指向 ELF 代码页,指令字节被当 PTE 读,足以解释"5 ELF 卡住"。

**A5 实际执行**:
1. 在 `MMUPlugin::do_lookup` 入口加临时 `fprintf(stderr, "[MMU_DEBUG] stage=%s satp_ppn_=0x%lx satp_value_=0x%lx sv_mode_=%d vaddr=0x%lx\n", ...)`
2. `mmufault-verilator-sv32-e2e-flip` 是 **placeholder change** (`status: placeholder`, depends_on `phase-6d.6-mmu-ptw-fsm`) — "5 ELF 卡住" 失败模式**无现存可重放测试** → 改用现有最近路径 `[mmu][tlb-refill]` (用 `set_satp_ppn(1ULL<<20) = 0x100000` 模拟 RISC-V satp 路径)
3. 捕获 runtime `satp_ppn_` = `0x100000` (与调用值完全一致) → `set_satp_ppn` runtime 路径正确
4. **验证完成立即删除 print** — `git diff ip/mmu/tlm/MMUPlugin.cpp` 干净
5. 结论已反馈 proposal §Why line 36-41 + research §12.2 + tasks.md A.5

**A5 结果 (决定 Phase B 起点)**:
- ❌ 原分支"print 值 = 0x8000F → 更便宜 root cause" — **被排除**: `0x80000` vs `0x8000F` 是 proposal 文档 typo (line 26 已修),不是代码 bug;静态链 (`make_satp_value` → `RiscvMMUPlugin` ctor Sv32 mask `0x3FFFFFULL` → `set_satp_ppn`) 对 `0x8000F` 正确
- ❌ 原分支"print 值 ≠ 0x80000 → 重新 root cause" — **不适用**: 无真实测试场景可比 (placeholder change)
- ✅ **等效结论**: `set_satp_ppn` runtime 链正确 + 现有 `[mmu]` 测试 satp_ppn_ 非零触发 PTW 路径 → **Oracle D4-A (Bare shortcut) + D5-TLM (shadow 双 bug) 确认为真 root cause,Phase B 可继续**

---

## 3. 与 `wave5-bp-btb` 协调 3 项 (Oracle 细化,必须启动期协商)

| # | 议题 | 建议协议 |
|---|------|---------|
| 1 | **pc_reg next-mux 优先级** | `stall > branch actual > BP predict` — 否则 elaboration DAG 多驱动 |
| 2 | **stall 语义** | MMU `CtrlLink::halt_when(ptw_active)` 冻结 fetch;BP 冻结期间**不推测** (v0.11.0 简化) |
| 3 | **fetch stage at_stage 注册顺序** | MMU substage 先于 IBus fetch NORMAL (ADR-048 canonical ordering 等价物);谁拥有 `fetch` stage 的 substage 声明权需明确,避免 `declare_substage("fetch", ...)` 冲突 |

**协调失败备选**: 若 bp-btb 启动期未达成协议,本 change C2 (ibus_chmem.h) 推迟到协议达成后,Phase C 阻塞。

---

## 4. Critical 文件路径速查

| 文件 | 角色 |
|------|------|
| `openspec/changes/mmu-chmem-pipeline-integration/proposal.md` | 修订后的 change 主文档 (含 §0.1 revision log + §11 D1 scope) |
| `openspec/changes/mmu-chmem-pipeline-integration/tasks.md` | **29 个 task 详细分解 + RED/GREEN/REFACTOR + 估时表** |
| `openspec/changes/mmu-chmem-pipeline-integration/research-v0.11.0-prep.md` | **研究背景 + Oracle 决议详情 (§12) + 风险登记 + 已知陷阱** |
| `openspec/changes/mmu-chmem-pipeline-integration/specs/mmu-chmem-pipeline-integration/spec.md` | ADDED Requirements (5 个新功能) |
| `openspec/changes/mmu-chmem-pipeline-integration/specs/verilator-mmu-bare-plumbing/spec.md` | MODIFIED (TEST_CASE 4 + 7/7 baseline) |
| `openspec/changes/mmu-chmem-pipeline-integration/specs/cpu-l1-mmu-demo-regression/spec.md` | MODIFIED (TEST_CASE 7 + AGENTS.md workaround 移除) |
| `ip/mmu/tlm/MMUPlugin.cpp:109` | D4-A 删除目标 |
| `ip/cpu/plugins/mmu.h:99-100,103` + `mmu.cpp:36-40` | D5 shadow 双 bug 修复目标 |
| `ip/cpu/plugins/mmu_ptw_chmem.h` | **复用,0 修改** |
| `ip/cpu/cpu_factory_chmem.h:80-85` | C4 owner 真空修复 |
| `ip/cpu/plugins/ibus_chmem.h` | C2 sv32 消费实装 |
| `ip/cpu/plugins/dmem_chmem.h` | C3 sv32 消费实装 |
| `ip/cpu/plugins/mmu_exception_handler_chmem.h` | C1 combinational network |
| `ip/cpu/plugins/hazard_chmem.h:151-153` | C5 mark/clear mmufault 实装 |

---

## 5. 启动期 owner 第一步 (建议顺序)

1. **读完本文 + tasks.md 全文 + research §6 + §12** (~30 min)
2. ✅ **A5 验证已完成** (Oracle R12,见 §2) (2026-10-08 启动期执行完毕)
3. **与 bp-btb owner 协调 3 项** (Oracle §3) (~0.5d) — **尚未执行,Phase C 前置**
4. **执行 Phase A (A1-A6 根因修复)** — **硬前置**,否则 B/C 失败率高 (~5d)
5. **执行 Phase B → C → D → E** 按 tasks.md 顺序 (~28d)
6. **Phase D5 跑 4 个 Architecture Gate 脚本** (硬门禁)
7. **Phase E6 archive** (`openspec archive mmu-chmem-pipeline-integration -y`)

---

## 6. 估时表 (Oracle 调整后)

| Phase | Tasks | 人天 |
|-------|-------|------|
| A 根因 | A1, A2, A3, A3.1, A4, A5 ✅, A6 | **4.5d (A5 已完成 2026-10-08)** |
| B MMU CH_MEM | B2, B3, B4, B5 (B1 跳过) | **10** |
| C Pipeline 集成 | C1, C2, C3, C4, C5, C6 | **11** |
| D 验证 | D0 ✅, D1, D2, D3, D4, D5, D6 | **5** |
| E 文档 | E1, E2, E3, E4, E5, E6 | **3** |
| **合计** | **28 待启动 + 1 ✅** | **34 (≈ 6.0-6.2 人周)** |

**降级路径** (若 v0.11.0 窗口硬性 < 5 周):
- D1 收窄版再砍为"TLB-less" (每 access 走 PTW FSM,5-指令 ELF 场景可接受) → 省 ~1.5d
- 必须在 archive 前于 proposal §11 显式记录 PoC 限制

---

## 7. D0 已完成 (2026-10-08)

proposal 修订已落地:
- ✅ §4 删除 "trap PC 跳转断言" AC (Oracle R14)
- ✅ §1 删除 "MmuExceptionHandler → trap PC 跳转" (Oracle R14)
- ✅ §2 明确 Bare shortcut 修复 = 删除 `MMUPlugin.cpp:109` 析取项 (Oracle D4)
- ✅ §11 新增 D1 收窄 scope 显式声明 (Oracle D1)
- ✅ §Why 增加 shadow 双 bug 详细解释 + Oracle session 引用 (Oracle R2)
- ✅ §3 明确 shadow 双 bug 修复 = 删除派生三件套 (Oracle D5)
- ✅ §0.1 revision log 记录 7 处定向修订

---

## 8. 已知陷阱 (research §9 完整版)

1. `ch_literal<0, N>{}` 必须 (v0.3.1 M6 上游 SEGV 修复)
2. at_stage 闭包内禁用运行期 `if(ch_bool)` (CI grep 兜底)
3. CH_MEM 测试显式 `set_as_current_context()` (PoC 教训)
4. `ch_bool::explicit operator bool()` 静默取 false (必须显式包装)
5. `uint_t<N>` 在 TLM vs CH_MEM 是不同类型 (CI Check 7)
6. MMUPlugin lib/ vs tlm/ 严格切分 (D4 强制)
7. **CH_MEM 流水线无 CSR plugin** (Oracle R11) — 影响 D3-A 物理可能性 + D5-CH_MEM 约束
8. **proposal §4 AC 与 D3-A 矛盾已修** (D0 完成)

---

## 9. 关联 active changes (启动期关注)

- `mmufault-verilator-sv32-e2e-flip` — Part a/b 来源,本 change 承接实现
- `mfc-extract-fsm-h` — 同 v0.11.0 窗口,scope 互不重叠
- `wave5-bp-btb` — 同 v0.11.0 窗口,共享 fetch stage (见 §3 协调)
- `phase-6d-prerequisites` — depends_on

---

## 10. 退出标准 (E6 archive 前必须全勾)

- [ ] A1-A6 全 PASS,`[mmu]` 0 意外退化
- [ ] B1-B5 (B1 跳过) 全 PASS,`[chmem]` 0 退化
- [ ] C1-C6 全 PASS,`[cpu-integration] 81/81` + `[cpu-l1-mmu-demo] 7/7`
- [ ] D1-D6 全 PASS,`[verilator] 1/1` + `[mmu-verilator] 4/4` + `[mmu] 53/53` + `[cpu-l1-mmu-demo] 7/7` + `[cpu] 19/19`
- [ ] E1-E6 全部 commit
- [ ] 4 个 Architecture Gate 全 0 error
- [ ] `bash tools/run_chipforge_tests.sh --all` 全 PASS
- [ ] AGENTS.md L41 workaround 移除 + `[v0.11.0 follow-up]` 标记
- [ ] CHANGELOG.md v0.11.0 段新增条目
- [ ] `openspec change validate mmu-chmem-pipeline-integration` 0 error
- [ ] `openspec archive mmu-chmem-pipeline-integration -y` 成功

---

**Handoff note 版本**: v1.0 (2026-10-08, Oracle-locked)
**关联**:tasks.md (29 tasks) + research-v0.11.0-prep.md (541 LOC, v0.2) + proposal.md (200 LOC, 修订)

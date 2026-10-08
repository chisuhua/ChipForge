---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: ready
depends_on:
  - cpu-pipeline-mmufault-handler
  - phase-6d-prerequisites
oracle_session: bg_13cd7890
oracle_date: 2026-10-07
---

# mmu-chmem-pipeline-integration — CH_MEM MMU/PTW pipeline 集成 (v0.11.0 owner)

> **Scope 拆分 (2026-10-07, Sisyphus 决策)**: 本 change 从 `mmufault-verilator-sv32-e2e-flip` (Part a/b) 拆分出的 follow-up，承担 **CH_MEM 模式 MMU/PTW pipeline 集成层**职责。
>
> **Oracle 决议 (2026-10-07, session `bg_13cd7890`)**: D1-D5 全部锁定。详见 [research-v0.11.0-prep.md](research-v0.11.0-prep.md) §6 + §12。
>
> **Status 修订 (2026-10-08)**: 从 placeholder → ready。tasks.md 已起草。proposal §4 已修订(去 "trap PC 跳转断言" AC,详见 §0.1 + §4)。

## Why

`mmufault-verilator-sv32-e2e-flip` (wave5 P1) Part a (TLM `cfg.enable_mmu=true` flip) 实装 3 次尝试均 FAIL (2026-10-07 Sisyphus bootstrap session):

1. **尝试 1**: `cfg.enable_mmu=true` + `plant_identity_page_table(mem, window_base+60*1024, elf.entry_addr)` — 5 个 ELF 10000 cycles 卡住,exit_code=-1
2. **尝试 2**: 加 `cfg.satp_ppn = (window_base+60*1024)>>12 = 0x80000` 修复 PPN 路径 — 仍然 5/5 FAIL
3. **尝试 3**: 加 DEBUG 探针但破坏 macro 结构 — 回滚

**Oracle 2026-10-07 决议的 Root cause** (session `bg_13cd7890`,详见 [research-v0.11.0-prep.md](research-v0.11.0-prep.md) §12):

1. **`MMUPlugin.cpp:109` Bare shortcut 误判条件** `|| satp_ppn_ == 0` — Sv-mode + satp_ppn=0 也走 identity translation,把 vaddr 当 paddr 写。修复 = 删除该析取项 (D4-A,详见 §2)
2. **Shadow 双 bug** (proposal §Why 假设 #4 已 Oracle 验证为真):
   - **Bug #1**: `mmu.h:100` 派生 `set_satp_value` shadow 基类非虚函数 + `mmu.h:103` 派生重复 `satp_value_` 成员 → `csr_write_satp` 后基类 `satp_value_` **永不更新**,停留在 ctor 值
   - **Bug #2**: `mmu.cpp:37` `& 0x0FFFFFFFFFFFFULL` 48-bit mask 把 Sv32 MODE bit31 含进 PPN → PTW root 指向天文地址
   - **Oracle 推荐修复**: 删除派生三件套 (`mmu.h:99-100,103`),让基类存取器被继承;`csr_write_satp` 复用 `cf::cpu::detail::extract_satp_ppn` (单测入口已存在,`mmu.h:44-52` 注释自证)
3. **可能更便宜的 root cause**: proposal 转述尝试 2 PPN = `0x80000`,但 `(0x80000000+0xF000)>>12 = 0x8000F`(≠ `0x80000`)— PTW root 指向 ELF 代码页,指令字节被当 PTE 读足以解释"5 ELF 卡住"。A5 启动期优先用 print/debug 验证

**owner 真空**:
- `ip/cpu/cpu_factory_chmem.h:80-85` 源码注释自证 *"sv32 translation 实现 owner 尚未分配"*
- `tools/verilator_runner/cpu_verilator_sim.cpp:137-138` 自证 *"--enable-mmu is a no-op log; real sv32 translation is not delivered by this runner"*
- 当前状态 = CH_MEM MMU 集成层 scope **无 change 持有**

**失败教训**:
- 决策 #2 "接受 Part a/b 拆分" 假设 1-4h 可 ship 是 v0.10.0 window 的工程估算
- 实际工作=证实 MMU BFTA 真翻译路径需要更深层 debug (估时从 Oracle 1-3 周 → 本 change 总估时 6.0-6.2 人周,详见 research §4 总估时表)
- 与决策 #3 一致:**mmu_chmem.h 实装不应塞进 mmufault-verilator-sv32-e2e-flip scope** (D4 隔离)
- 但 mmufault Part a 实装也**间接受阻**于同根因 (TLM 真翻译路径需要 MMU plugin 内部 satp_value 初始化机制修订)

**v0.11.0 启动决策**:
- 本 change 与 `wave5-bp-btb` (v0.11.0) 同窗口规划
- 共享 fetch stage (避免 fetch path ownership 冲突)
- **Oracle 2026-10-07 已锁定 5 项决策** (D1-D5,详见 §6):
  - D1: `MMUPlugin_chmem.h` scope = **B 收窄版** (单级 ch_mem TLB + 复用 PtWalkFsmPlugin)
  - D2: IBus/DBus sv32 消费 = **v0.11.0 内** (硬依赖 TEST_CASE 6)
  - D3: exception_handler combinational = **最小** (单条 mmufault_clear,无 trap PC 跳转)
  - D4: Bare shortcut 公式 = **删除析取项** `|| satp_ppn_ == 0`
  - D5: csr_write_satp 注入 = **C (TLM 现行)** + **shadow 双 bug 修复**

### 0.1 Proposal 修订日志 (2026-10-08)

| 修订 | 原因 | 来源 |
|------|------|------|
| §4 末尾删除 "trap PC 跳转断言" AC | 与 D3-A 矛盾 (CH_MEM 无 CSR plugin,trap PC 跳转物理不可能) | Oracle R14 |
| §1 删除 "MmuExceptionHandler → trap PC 跳转" | 同上 | Oracle R14 |
| §2 明确 Bare shortcut 修复 = 删除 `MMUPlugin.cpp:109` 析取项 | D4-A 锁定 | Oracle D4 |
| §11 新增 D1 收窄 scope 显式声明 (out-of-scope 列表) | 防 review 期 scope 膨胀 | Oracle D1 |
| §Why 增加 shadow 双 bug 详细解释 + Oracle session 引用 | Oracle 已验证为真 | Oracle R2 |
| §3 明确 shadow 双 bug 修复 = 删除派生三件套 | D5-TLM 锁定 | Oracle D5 |
| §10 移除 "[cpu-l1-mmu-demo] 6/6 → 7/7 PASS" 的暗示 | E1 由 E5 验证,需 7/7 真正达 | Oracle D2 |

## What Changes

### 1. CH_MEM MMU/PTW pipeline 集成层实装

**位置**: `ip/cpu/cpu_factory_chmem.h` (修改) + `ip/cpu/plugins/mmu_ptw_chmem.h` (复用,0 修改) + `ip/cpu/plugins/ibus_chmem.h` (修改) + `ip/cpu/plugins/mmu_exception_handler_chmem.h` (修改) + `ip/mmu/tlm/MMUPlugin_chmem.h` (新增) + `ip/cpu/plugins/mmu_chmem.h` (新增 RiscvMMUPlugin CH_MEM 版)

实装 CH_MEM 模式 fetch/memory 路径:
- IBus CH_MEM 接入 sv32 翻译结果 (`pl::MMU_VADDR` / `pl::PADDR_VALID` 消费)
- DBus CH_MEM 接入 sv32 翻译结果 (同上 load/store 路径)
- **MmuExceptionHandler CH_MEM 消费 EXCEPTION_CODE → 输出 `mmufault_clear` 到 HazardPlugin stall_ctrl_** (D3-A 锁定: 仅 stall,**无 trap PC 跳转** — CH_MEM 流水线无 CSR plugin,trap PC 物理不可能)
- HazardPlugin CH_MEM 接收 `mmufault_clear` → `mark_mmufault`/`clear_mmufault` 实装 + `CtrlLink::halt_when`

**Bare 路径约束** (Oracle D2-A 强调): 所有 sv32 消费改动对 bare 模式路径必须 **byte-identical 保护**(`[chmem]` + `[verilator]` baseline 零额外 latency)。

### 2. MMUPlugin Bare shortcut 严格化

**位置**: `ip/mmu/tlm/MMUPlugin.cpp:109` (修) + `ip/mmu/tlm/MMUPlugin_chmem.h` (新增)

**修复** (D4-A 锁定): 删除 `MMUPlugin.cpp:109` 的 `|| satp_ppn_ == 0` 析取项。现代码 `:95-108` 已正确从 `satp_value_` 提取 `satp_mode`,bug 仅在 `:109` 保留误判条件。**不重写判定逻辑,只删除析取项**。

**MMUPlugin_chmem.h 范围** (D1-B 收窄版):
- 单级 ch_mem TLB (8 项直接映射,可 PR 内调参)
- 复用现有 `PtWalkFsmPlugin` (`ip/cpu/plugins/mmu_ptw_chmem.h`,**0 修改**)
- 发射 `PADDR`/`PADDR_VALID`/`PTW_ACTIVE` payload keys + satp 作为 elaboration-time ch_reg 初值
- **显式 out-of-scope 列表** 详见 §11

### 3. RiscvMMUPlugin::csr_write_satp 注入时机审查 + Shadow 双 bug 修复

**位置**: `ip/cpu/plugins/mmu.h:99-100,103` (修) + `ip/cpu/plugins/mmu.cpp:36-40` (修)

**Oracle 验证为真的双 bug**:
- **Bug #1** (`mmu.h:100`): 派生 `set_satp_value` shadow 基类非虚函数 + `mmu.h:103` 派生重复 `satp_value_` 成员 → `csr_write_satp` 后基类 `satp_value_` **永不更新**,停留在 ctor 值
- **Bug #2** (`mmu.cpp:37`): `& 0x0FFFFFFFFFFFFULL` 48-bit mask 把 Sv32 MODE bit31 含进 PPN → PTW root 指向天文地址

**修复** (D5-TLM 锁定): **删除派生三件套** (`mmu.h:99-100` getter/setter + `:103` 重复成员),让基类 `MMUPlugin` 的 `set_satp_value`/`satp_value`/`set_satp_ppn`/`satp_ppn` 存取器被 `RiscvMMUPlugin` 继承;`csr_write_satp` 复用 `cf::cpu::detail::extract_satp_ppn` (单测入口已存在,`mmu.h:44-52` 注释自证)。单一事实源,根除整个 shadow bug 类别。

**CH_MEM 侧约束** (D5-CH_MEM,非选项): CH_MEM 流水线无 CSR plugin (`cpu_factory_chmem.h:304-310` 注册清单无 CSR) → 运行期 `csrw satp` 无消费者 → sv32 PTE ELF 无法自配 satp → **satp 必须是 MMU CH_MEM 的 ctor/config 参数,elaboration 期固化为 ch_reg 初值**。

### 4. Verilator TEST_CASE 6 mmu_sv32_translation_verilator_e2e_flipped

**位置**: `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` (修改)

承接 `mmufault-verilator-sv32-e2e-flip` Part b (原 tasks.md §4):
- sv32 PTE ELF vendor (`build_sv32_pte.S` + register `build_sv32_pte` in `build_manual_elf.sh`)
- sv32 baseline CSV (5 ELF × median cycle × mode=sv32_mmu)
- popen `cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf sv32_pte.elf --cycles 5000`
- **AC 修订** (Oracle R14): REQUIRE `TOHOST=1` + cycle ≤ sv32 baseline × 1.5 + **EXCEPTION_CODE 正确传播** (替代原"trap PC 跳转断言",因 D3-A 物理不可能) + **pipeline stall 在 PTW_ACTIVE=1 时正确生效**

### 5. [cpu-l1-mmu-demo] 真 sv32 翻转 (从 mmufault Part a 拆分)

**位置**: `tests/soc/test_cpu_l1_mmu_demo.cpp` (修改)

承接 `mmufault-verilator-sv32-e2e-flip` Part a (原 tasks.md §5):
- flip `cfg.enable_mmu=true` + `cfg.satp_ppn` + plant_identity_page_table
- 新增 TEST_CASE 7 `cpu_l1_mmu_demo_sv32_translation_flipped`
- 验证 7/7 PASS (6 旧 + 1 新)

**前置**: 本 change §1-3 完成 (CH_MEM 集成 + Bare shortcut 严格化 + csr_write_satp 审查)

### 6. AGENTS.md workaround 行更新 (从 mmufault Part a 拆分)

**位置**: `AGENTS.md` 第 41 行 `[cpu-l1-mmu-demo]`

移除 workaround 标记,加 `[v0.11.0 follow-up]` 标记。

### 7. Spec Delta 编辑

**位置**: `openspec/specs/verilator-mmu-bare-plumbing/spec.md` + `openspec/specs/cpu-l1-mmu-demo/spec.md` (修改)

新增 Requirement "TEST_CASE 6 mmu_sv32_translation_verilator_e2e_flipped" + "TEST_CASE 7 真 sv32 translation 翻转" + 移除 workaround 标注。

### 8. Architecture Gate Final

- 跑 `bash tools/verify_adr.sh` 必须 0 失败
- 跑 `bash tools/verify_plugin_decision.sh` 必须 0 失败
- 跑 `bash tools/check_plugin_portability.sh` 必须 0 失败
- 跑 `bash tools/doc_link_check.sh` 必须 0 失败

### 9. Regression Final Check

- 跑 `bash tools/run_chipforge_tests.sh` 全部测试
- `[mmu-verilator] 5/5` + `[mmu] 53/53` + `[cpu-integration] 81/81` + `[cpu-l1-mmu-demo] 7/7` + `[cpu] 19/19` + `[verilator] 1/1`

### 10. Archive Preparation

- 所有 AC checkbox 勾选完毕
- `openspec change validate mmu-chmem-pipeline-integration` 0 error
- AGENTS.md `[cpu-l1-mmu-demo]` workaround 移除 + `[v0.11.0 follow-up]` 标记
- CHANGELOG.md v0.11.0 段新增条目 `mmu-chmem-pipeline-integration`
- `[cpu-l1-mmu-demo] 7/7 PASS` (E1 + E5 验证 workararound 真无回归后,E1 移除 workaround 标记)
- `openspec archive mmu-chmem-pipeline-integration -y`

### 11. D1 收窄 scope 显式声明 (Oracle D1-B 锁定, 2026-10-07)

> **目的**: 防 review 期被要求"补完"导致 scope 膨胀。本节明确列出 **本 change 不交付的功能**,在 archive 时同步 commit 进 spec.md。

**本 change 交付 (in scope)**:
- ✅ sv32 翻译 (sv_mode=Sv32)
- ✅ 单级直接映射 TLB (ch_mem,8 项)
- ✅ sv32 5 状态 PTW FSM (复用 `mmu_ptw_chmem.h` 现有 `PtWalkFsmPlugin`,**0 修改**)
- ✅ Bare mode identity translation
- ✅ `EXCEPTION_CODE` 正确传播 + pipeline stall in PTW_ACTIVE=1 (替代 trap PC 跳转)
- ✅ 5-指令 ELF tohost=1 (add/addi/auipc/jal/beq + lui/sw/bne)
- ✅ Bare 路径 byte-identical 保护 (无 sv32 消费引入额外 latency)

**本 change **不**交付 (out of scope, 推迟到后续 change)**:
- ❌ **Sv39/Sv48 翻译** — 当前仅 sv32;Sv39/48 推迟 v0.11.1+ (`mmu-sv32-sv48-ext` change 候选)
- ❌ **多级 TLB (L0/L1/Ln-1 编排)** — 当前仅单级直接映射;真 workload 命中率不足推迟 v0.11.1+ (依赖 `cache-dse-sweep` Wave 3 数据)
- ❌ **LRU/RRIP/FIFO 替换策略** — 当前单级用直接映射 (无需策略);多策略待多级 TLB 一起实装
- ❌ **ASID≠0 路径** — 当前仅 ASID=0;Sv32 ASID<10 bits 测试推迟
- ❌ **访问权限检查 (R/W/X vs load/store/ifetch)** — 当前 PoC 只走 walk 成功/失败;权限检查推迟 v0.11.1+
- ❌ **megapage (level-1 叶, 4MB) 完整 PPN 拼接** — 当前透传 ppn 不拼接 vaddr 剩余位 (cf. `mmu_ptw_chmem.h:82` PoC 简化)
- ❌ **trap PC 跳转 / mepc / mtvec CSR 读写** — 当前物理不可能 (CH_MEM 无 CSR plugin,Oracle R11);需独立 change 建立 `CSRPlugin_chmem.h` (v0.11.1+ 候选)
- ❌ **CSRPlugin CH_MEM** — 当前 CH_MEM 流水线注册清单无 CSR plugin (`cpu_factory_chmem.h:304-310`);trap PC/mtvec/mepc CSR 读写需独立 change
- ❌ **split_id 拓扑** — `ip/mmu/STATUS.md:41` 已 defer
- ❌ **MMUTLMBridge CH_MEM 版** — 与 `L1CacheTLMBridge` 同构,TLM-only defer
- ❌ **rtl/ CppHDL 完整实装** — Phase 5+,依赖 CH_MEM 完整化

**降级路径** (若 v0.11.0 窗口硬性 < 5 周):
- D1 收窄版再砍为"**TLB-less**"(每 access 走 PTW FSM)— 5-指令 ELF 场景可接受,省 ~1.5d
- **必须在 archive 前于本节显式记录 PoC 限制**

---

## Tasks (drafted, 详见 [tasks.md](tasks.md))

> **状态**: ✅ tasks.md 已起草 (2026-10-08,基于 Oracle-locked 决议 + research §4)。
> **总任务数**: 29 (A1-A6 + B2-B5 + C1-C6 + D0-D6 + E1-E6)
> **总估时**: 6.0-6.2 人周 (Oracle 调整后,vs 原 5.6)
> **B1 已跳过**: Oracle D1-B 收窄版锁定,scope 决策已前置完成。
> **当前占位**: 仅声明 change intent + depends_on + scope 边界
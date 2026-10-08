# Tasks — mmu-chmem-pipeline-integration (v0.11.0)

> **Drafted**: 2026-10-08 by Sisyphus session (基于 Oracle 2026-10-07 决议 + research-v0.11.0-prep.md §4)
> **Total tasks**: 29 (A1-A6 + B2-B5 + C1-C6 + D0-D6 + E1-E6)
> **Total estimate**: 6.0-6.2 人周 (Oracle 调整后,vs 原 5.6)
> **B1 已跳过**: Oracle D1-B 收窄版锁定,scope 决策已前置完成
> **D0 已完成**: proposal 文本修订 (本 PR 落地)
>
> **执行顺序** (硬性,不可并行的有依赖标记):
> 1. PR1-A (A1 → A3.1 → A4 → A5 → A6) — 根因修复,**必须前置**
> 2. PR1-B (B2 → B3 → B4 → B5) — MMUPlugin CH_MEM 实装
> 3. PR1-C (C1 → C4 → C2/C3 并行 → C5 → C6) — Pipeline 集成
> 4. PR1-D (D1 → D2 → D3/D4 并行 → D5 → D6) — 验证与回归
> 5. PR1-E (E1 → E2/E3 并行 → E4 → E5 → E6) — 文档与归档

---

## Phase A — 根因 Bug 修复 (前置硬依赖)

> **Oracle 验证的 3 个真 bug**:
> 1. `MMUPlugin.cpp:109` Bare shortcut `|| satp_ppn_ == 0` 误判 (D4-A)
> 2. **Shadow 双 bug** — `mmu.h:100` 派生 `set_satp_value` shadow + `mmu.cpp:37` 48-bit mask 把 Sv32 MODE bit31 含进 PPN (D5-TLM)
> 3. **可能更便宜 root cause** — TLM flip 尝试 2 PPN 计算错误 `(0x80000000+0xF000)>>12 = 0x8000F ≠ 0x80000` (A5 优先验证)
>
> **本 Phase 完成标志**: `[mmu] 53/53 PASS + 0 意外退化 + shadow 双 bug 单元测试 PASS + PPN 计算核实 + 至少 5/5 sv32 PTE ELF 在 TLM pipeline 不卡死`

### A.1 [RED] 写 `[mmu][bare-shortcut]` 测试:Bare shortcut 3 case (D4-A 验证)

- [ ] **新建测试 case** (追加到 `tests/mmu/test_mmu_plugin.cpp`):3 个新 test cases,~50 LOC
- [ ] **Family tag**:使用 `[mmu][bare-shortcut]` (与既有 `[mmu]` 多 tag 一致)
- [ ] **覆盖范围**:
  - **Case 1**:`MMUPlugin(Sv32, ..., mem=nullptr)` + `set_satp_ppn(0)` → 调 `do_lookup` → **期望 PADDR = vaddr** (走 identity,Bare shortcut 触发)
    - **当前 FAIL** (修复后会 PASS — 验证修复正确触发 Bare shortcut)
  - **Case 2**:`MMUPlugin(Sv32, ..., mem=nullptr)` + `set_satp_ppn(0x12345)` + 提供 TLB stub 命中 → **期望 PADDR = TLB 命中 paddr** (不走 identity,走 TLB lookup)
    - **当前 PASS** (修复后保持 PASS — 验证修复不影响正常路径)
  - **Case 3**:`MMUPlugin(Sv39, ..., mem=nullptr)` + `set_satp_ppn(0xABCDE)` + 提供 TLB stub 命中 → **期望 PADDR = TLB 命中 paddr** (Sv39 路径不受影响)
    - **当前 PASS** (修复后保持 PASS)
- [ ] **断言数**:6-9 assertions (3 case × 2-3 断言)
- [ ] **运行命令**:`./build/bin/chipforge_tests "[mmu][bare-shortcut]"`

### A.2 [GREEN] 修复 `MMUPlugin.cpp:109` Bare shortcut 误判条件 (D4-A)

- [ ] **修改 `ip/mmu/tlm/MMUPlugin.cpp:109`** — 删除 `|| satp_ppn_ == 0` 析取项
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

### A.4 [GREEN] A.1 + A.3 测试全部 PASS,根因修复完成

- [ ] **A.1 3 case 全部 PASS** (Bare shortcut 修复正确)
- [ ] **A.3 3 case 全部 PASS** (shadow 双 bug 修复正确)
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

### A.6 [GREEN] Regression check — `[mmu] 53/53` 0 意外退化最终确认

- [ ] **A.1-A.5 全部完成** 后跑全量回归
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

- [ ] **新建文件**:`ip/cpu/plugins/mmu_chmem.h` (~150 LOC,纯头文件)
- [ ] **namespace**:`cf::cpu::plugins` (与 TLM `RiscvMMUPlugin` 同 namespace,但 `_chmem.h` 后缀区分)
- [ ] **CH_MEM 适配**:
  - `#ifdef CF_PLUGIN_USE_CH_MEM` 整文件包裹 (ADR-040 v2.0 强制)
  - 包含 `<ch.hpp>` + `<core/bool.h>` + `<core/uint.h>` + `<core/reg.h>`
  - 包含 `cf/plugin/{plugin_base,pipe_builder,ctrl_link,uint_t}.h`
- [ ] **类定义**:`RiscvMMUPluginChmem : public cf::plugin::PluginBase`
- [ ] **elaboration-time satp config 注入**:
  - 构造参数接受 `satp_mode_value` (从 `cpu_factory_chmem` ctor 传入)
  - elaboration 期创建 `ch_reg<64>` 作为 satp 寄存器初值 (D5-CH_MEM 约束:无运行期 CSR 写)
- [ ] **at_stage 闭包** (CH_MEM 模式):
  - `pb.at_stage("mmu_ptw", NORMAL, [...])` 调 `PtWalkFsmPlugin::create_fsm()` 复用现有 FSM
  - `pb.at_stage("mmu_lookup", NORMAL, [...])` 单级 TLB lookup 闭包
- [ ] **payload key 发射**:
  - 写 `mmu_keys<T>::PADDR` + `PADDR_VALID` + `MMU_VADDR` (复用 `mmu_keys.h`)
  - 不发明新 key (ADR-040 Tier-2 兼容)
- [ ] **回归项**:`[chmem]` 既有测试不退化;`[mmu] 53/53` 维持
- [ ] **运行命令**:`./build/bin/chipforge_tests_chmem "[chmem]"`

### B.3 [GREEN] 实装 `MMUPlugin_chmem.h` 收窄版 (D1-B 单级 TLB + 复用 PtWalkFsmPlugin)

- [ ] **新建文件**:`ip/mmu/tlm/MMUPlugin_chmem.h` (~200 LOC,纯头文件)
- [ ] **ADR-040 v2.0 强制**:`#ifdef CF_PLUGIN_USE_CH_MEM` 整文件包裹;**不含** `ch_mem`/`ch_reg`/`ch_uint`/`ch::core` 在 TLM 文件中
- [ ] **CH_MEM 适配**:与 B.2 同样 include 列表
- [ ] **类定义**:`MMUPluginChmem : public cf::plugin::PluginBase`
- [ ] **单级 ch_mem TLB** (8 项直接映射):
  - `ch_mem<...> tlb_entries_[8]` (ch_mem 数组)
  - `ch_reg<...> tlb_valid_[8]` (valid 位)
  - `ch_reg<...> tlb_tag_[8]` (tag)
  - `ch_reg<...> tlb_paddr_[8]` (paddr 缓存)
- [ ] **复用 PtWalkFsmPlugin**:`#include "ip/cpu/plugins/mmu_ptw_chmem.h"` 直接调其 API,不改其内部
- [ ] **TLB lookup 组合逻辑**:
  - 命中:`result_paddr = tlb_paddr_[index]` + `PADDR_VALID = true`
  - miss:启动 PtWalkFsmPlugin (调 `set_input_value(start=1)`) + 等 `done=1` 拍读 `result_ppn`
- [ ] **payload key 发射**:同 B.2
- [ ] **ptw_active gate**:
  - miss 时 `PTW_ACTIVE = 1` 同步发射
  - `done` 拍 `PTW_ACTIVE = 0` + 同时 `TLB` refill (单级直写下一项,无 LRU/RRIP)
- [ ] **回归项**:`[chmem]` 既有测试不退化;`[mmu]` 维持
- [ ] **运行命令**:`./build/bin/chipforge_tests_chmem "[chmem]" "[mmu]"` (注意: `[mmu]` 是 TLM,只验证不互相污染)
- [ ] **代码自审**:不写 `as any` / `@ts-ignore` / 空 catch (项目硬约束)
- [ ] **D4 检查**:不写 enum class.*State 状态机 (有 PTW FSM 走 `CF_PLUGIN_USE_FSM_EXEMPT` 豁免);TLB lookup 是纯组合逻辑

### B.4 [GREEN] 单元测试 `test_mmu_chmem_basic` (TLB hit / TLB miss 两条路径)

- [ ] **新建文件**:`tests/mmu/test_mmu_chmem.cpp` (~150 LOC)
- [ ] **Family tag**:使用 `[mmu][chmem]` (与 `[chmem]` 多 tag 一致)
- [ ] **覆盖范围**:
  - **Case 1:TLB hit** — 配置单级 TLB 含一项,lookup 命中 vaddr → 期望 PADDR = TLB.paddr (单周期,不走 PTW)
  - **Case 2:TLB miss** — TLB 全空 + 提供 test memory 接口返回 PTE → 走 PtWalkFsmPlugin → 期望 PADDR = PTE.ppn (2-3 周期,经 FSM)
  - **Case 3:PTW fault** — 提供 test memory 返回 V=0 PTE → 期望 PADDR_VALID = false + EXCEPTION_CODE = 1
- [ ] **断言数**:9-12 assertions (3 case × 3-4 断言)
- [ ] **CH_MEM 测试 lifecycle**:每个 SECTION 入口显式 `set_as_current_context()` (Phase 6c PoC 教训,详 research §9.3)
- [ ] **回归项**:A.1-A.6 全部 PASS;`[mmu][chmem]` 不影响 `[chmem]` 既有 baseline
- [ ] **运行命令**:`./build/bin/chipforge_tests_chmem "[mmu][chmem]"`

### B.5 [GREEN] Phase B 集成回归 (chipforge_tests_chmem 全套)

- [ ] **B.2-B.4 全部完成** 后跑 CH_MEM binary 全套
- [ ] **跑**:`./build/bin/chipforge_tests_chmem` (含 `[chmem]`/`[cpphdl]`/`[elaborate]`/`[verilator]`/`[mmu-verilator]` + B.4 新增 `[mmu][chmem]`)
- [ ] **预期 PASS**:
  - `[chmem] 9/9 PASS`
  - `[cpphdl] 6/6 PASS`
  - `[pipeline2_stall_matrix] 16/16 PASS`
  - `[mmu][chmem] 3/3 PASS` (B.4 新增)
  - `[mmu-verilator] 3/3 PASS`
- [ ] **bare 路径 byte-identical 检查** — `[verilator] 1/1 case PASS (5 ELF tohost=1)` 维持 (Oracle D2-A 强调)
- [ ] **CH_MEM known issue 跟踪**:`cpphdl_poc_chbool_contextual_conversion` 全量跑受 context pollution 失败 (单独跑 PASS) 已知 — 不阻塞本 Phase,需 PoC follow-up 修
- [ ] **本 Phase 完成标志**:全 PASS + bare 路径 byte-identical

---

## Phase C — Pipeline 集成 (D3-A 最小 + byte-identical 保护)

> **scope 边界**:
> - MmuExceptionHandler combinational network = 最小 (单条 `mmufault_clear = (CPU_EXCEPTION_CODE != 0)`,**无 trap PC 跳转**)
> - IBus/DBus sv32 消费 = 改 fetch/load/store 路径,但 bare 模式必须 byte-identical (Oracle D2-A 强调)
> - HazardPlugin CH_MEM = 实装 `mark_mmufault`/`clear_mmufault` + `CtrlLink::halt_when`
>
> **本 Phase 完成标志**:`[cpu-l1-mmu-demo] 7/7 PASS` (6 旧 + 1 新 TEST_CASE 7 真 sv32 翻转) + `[cpu-integration] 81/81` + bare 路径零退化

### C.1 [GREEN] 实装 `mmu_exception_handler_chmem.h` combinational network (D3-A 最小)

- [ ] **修改文件**:`ip/cpu/plugins/mmu_exception_handler_chmem.h` (现 64 行 stub → ~120 LOC)
- [ ] **保持 `#ifndef CF_IP_CPU_PLUGINS_MMU_EXCEPTION_HANDLER_CHMEM_H` 头守卫**
- [ ] **combinational network 实装**:
  - 在 `build()` 注册 `at_stage("memory", NORMAL, [...])` 闭包 (不在 `setup()` — `setup` 应早返回,Plugin 框架约定)
  - 闭包内读 `cpu_keys<T>::CPU_EXCEPTION_CODE` (ch_uint<8> in CH_MEM mode)
  - 计算 `ch_bool mmufault_clear = (cpu_keys::CPU_EXCEPTION_CODE != 0)` (D3-A 最小)
  - 输出 `mmufault_clear` 到 `HazardPlugin::stall_ctrl_->halt_when(...)`
- [ ] **保持 API 兼容**:`setup`/`build`/`reset`/`mark_mmufault`/`clear_mmufault`/`mmufault_pending` API 不变 (TLM 版对齐)
  - `mmufault_pending()` 改为返回 `mmufault_clear` 当前值 (组合逻辑直接读 ch_bool)
  - `mark_mmufault()` / `clear_mmufault()` 仍为 no-op (combinational network 不需 state)
- [ ] **ADR-046 豁免不适用** — 本 Plugin 无状态机 (纯组合逻辑),**不加** `CF_PLUGIN_USE_FSM_EXEMPT`
- [ ] **回归项**:C.5 HazardPlugin 同步;`[cpu]` 既有不退化
- [ ] **运行命令**:`./build/bin/chipforge_tests "[cpu]" "[mmu]"` + `./build/bin/chipforge_tests_chmem`

### C.2 [GREEN] 实装 `ibus_chmem.h` sv32 消费 (D2-A + bare byte-identical)

- [ ] **修改文件**:`ip/cpu/plugins/ibus_chmem.h` (现 155 行 → ~200 LOC)
- [ ] **bare 模式 byte-identical 保护** (Oracle D2-A 强约束):
  - 当前 `ibus_chmem.h:128-129` 直接 `aread(bits<15,2>(pc_val))` — 走 pc 分支
  - 新增 `aread(bits<15,2>(select(paddr_valid, paddr, pc_val)))` — bare 模式 `paddr_valid=false` → select 恒取 `pc_val` 分支,与原 byte-identical
- [ ] **sv32 消费实装**:
  - 读 `mmu_keys<T>::MMU_VADDR` + `PADDR_VALID` (从 fetch stage)
  - 当 `PADDR_VALID=true` 且 `MMU_VADDR` 有值 → 用 `MMU_VADDR` 作 aread index
  - 当 `PADDR_VALID=false` → fallback to `pc_val` (Bare/未翻译路径)
- [ ] **PTW_ACTIVE 期间 fetch stall**:
  - 读 `mmu_keys<T>::PTW_ACTIVE` (ch_bool)
  - 当 `PTW_ACTIVE=1` → 调 `CtrlLink::halt_when(true)` 冻结 fetch (替代当前 stall_ctrl_ 路径)
- [ ] **ADR-048 canonical ordering** — `ibus_chmem.h` 的 `at_stage("fetch", NORMAL, ...)` 在 MMU substage 之后注册 (Wave 6d.3 等价物,需与 `wave5-bp-btb` owner 协商谁拥有 `fetch` stage 的 substage 声明权)
- [ ] **回归项**:`[verilator] 1/1 case PASS (5 ELF tohost=1)` 维持 (bare 模式 byte-identical);`[cpu-integration] 81/81` 维持
- [ ] **运行命令**:`./build/bin/chipforge_tests "[cpu-integration]"` + `./build/bin/chipforge_tests_chmem "[verilator]"`

### C.3 [GREEN] 实装 `dmem_chmem.h` sv32 消费 (D2-A + bare byte-identical)

- [ ] **修改文件**:`ip/cpu/plugins/dmem_chmem.h` (现 145 行 → ~190 LOC)
- [ ] **与 C.2 同样架构** — bare 模式 byte-identical 保护
- [ ] **sv32 消费实装** (load/store 路径):
  - 读 `mmu_keys<T>::MMU_VADDR` + `PADDR_VALID` (从 memory stage)
  - 当 `PADDR_VALID=true` → 用 `MMU_VADDR` 作 aread index (load/store 真翻译)
  - 当 `PADDR_VALID=false` → fallback to `mem_addr` (Bare/未翻译路径)
- [ ] **store data path 同步** — 写内存时 `paddr_valid=true` 且 `paddr` 已知 → 直接写 `paddr`;否则 fallback to `mem_addr`
- [ ] **PTW_ACTIVE 期间 memory stall** — 同 C.2 fetch stall 逻辑
- [ ] **回归项**:同 C.2
- [ ] **运行命令**:同 C.2

### C.4 [GREEN] 修复 `cpu_factory_chmem.h` owner 真空 (移除 "owner TBD" 注释)

- [ ] **修改文件**:`ip/cpu/cpu_factory_chmem.h:80-85`
- [ ] **删除注释** (现 6 行):
  ```
  //   enable_mmu  (Phase 6d P1 placeholder, **CH_MEM plumbing-only no-op stub**)
  //     — std::nullopt (默认): ...
  //     — true:               emit stderr/log "MMU hook enabled (mode=<mmu_mode>) — plumbing-only no-op stub; mmu_chmem.h not implemented. NO translation occurs."
  //                            **不会** 注册任何 MMU plugin (mmu_chmem.h 不在本 change scope)。
  //                            sv32 translation 实现 owner 尚未分配 — see change verilator-mmu-bare-plumbing-e2e
  //                            (CLI plumbing only, also plumbing-only) + cpu-pipeline-mmufault-handler for downstream plan.
  //     — false:              ...
  ```
- [ ] **替换为新注释** (实装后):
  ```
  //   enable_mmu  (v0.11.0 owner: mmu-chmem-pipeline-integration)
  //     — std::nullopt (默认): 与原 4 参数行为逐位等价 (无 MMU plugin 注册)
  //     — true:               register MMUPluginChmem (single-level TLB + sv32 PTW FSM)
  //                            emit satp config from cfg.satp_value | cfg.satp_ppn
  //     — false:              显式禁用,无 plugin (与 nullopt 等价)
  ```
- [ ] **实装 cfg.enable_mmu=true 路径** — 调用 `cpu_factory_chmem` 注册 `MMUPluginChmem` + `RiscvMMUPluginChmem` 到 `PipeBuilder`
- [ ] **回归项**:`[chmem]` 既有 baseline 维持;`[mmu][chmem]` (B.4 新增) PASS
- [ ] **运行命令**:同 C.2

### C.5 [GREEN] HazardPlugin CH_MEM 接 mmufault_clear (C1 配套)

- [ ] **修改文件**:`ip/cpu/plugins/hazard_chmem.h:151-153` (现 stub 区域)
- [ ] **替换 mark/clear mmufault stub**:
  - 现 `void mark_mmufault() noexcept {}` → 改为调 `stall_ctrl_->halt_when(ch_bool(true))` (实际触发 stall)
  - 现 `void clear_mmufault() noexcept {}` → 改为调 `stall_ctrl_->halt_when(ch_bool(false))` (实际取消 stall)
  - 现 `bool mmufault_pending() const noexcept { return false; }` → 改为返回当前 stall_ctrl_ 状态
- [ ] **依赖 C.1** — MmuExceptionHandler combinational network 输出 `mmufault_clear` 到 `HazardPlugin::stall_ctrl_`
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

> **本 Phase 完成标志**:`[cpu-l1-mmu-demo] 7/7 PASS` (含新增 TEST_CASE 7 真 sv32 翻转) + `[mmu-verilator]` TEST_CASE 6 PASS + 4 个 Architecture Gate 脚本 0 失败 + `tools/run_chipforge_tests.sh --all` 全 PASS

### D.0 ~~[GREEN] 修订 proposal 文本 (Oracle R14)~~ — **已完成 2026-10-08**

> **完成证据**: `openspec/changes/mmu-chmem-pipeline-integration/proposal.md` 已修订:
> - §4 删除 "trap PC 跳转断言" AC,替换为 "EXCEPTION_CODE 正确传播 + pipeline stall"
> - §1 删除 "trap PC 跳转",改为 "输出 mmufault_clear"
> - §11 新增 D1 收窄 scope 显式声明 (out-of-scope 列表)
> - §0.1 revision log 记录 7 处定向修订
> - §Tasks 占位符替换为引用本 tasks.md

### D.1 [GREEN] `test_mmu_bare_plumbing_verilator.cpp` TEST_CASE 6 sv32 e2e flip

- [ ] **修改文件**:`tests/mmu/test_mmu_bare_plumbing_verilator.cpp` (现 5/5 PASS)
- [ ] **新增 TEST_CASE 6**:`mmu_sv32_translation_verilator_e2e_flipped`
- [ ] **实现要点**:
  - `popen("cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf sv32_pte.elf --cycles 5000")`
  - 读 `sv32_pte.elf` (D3 完成 vendor)
  - 断言 `tohost == 1` (5/5 ELF)
  - 断言 cycle ≤ `sv32-baseline-matrix.csv` × 1.5
  - 断言 `EXCEPTION_CODE` 正确传播 (新 AC 替代原"trap PC 跳转断言")
  - 断言 pipeline stall 在 PTW_ACTIVE=1 时正确生效
- [ ] **断言数**:8-12 assertions
- [ ] **回归项**:`[mmu-verilator]` 既有 5/5 维持;`[verilator] 1/1` 维持
- [ ] **运行命令**:`./build/bin/chipforge_tests_chmem "[mmu-verilator]"`

### D.2 [GREEN] `test_cpu_l1_mmu_demo.cpp` TEST_CASE 7 真 sv32 翻转 (承接 mmufault Part a)

- [ ] **修改文件**:`tests/soc/test_cpu_l1_mmu_demo.cpp` (现 6/6 PASS)
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
- [ ] **生成方式**:D1 TEST_CASE 6 跑 5 ELF 各 N 次 (建议 N=20),取 median cycle
- [ ] **列字段**:`elf_name` / `median_cycle` / `min_cycle` / `max_cycle` / `stddev` / `mode=sv32_mmu` / `mmu_pte_pages`
- [ ] **回归项**:基线建立后,D1 TEST_CASE 6 用 `cycle ≤ median × 1.5` 断言
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
  - CH_MEM binary:`[chmem] 9/9` + `[cpphdl] 6/6` + `[elaborate]` + `[verilator] 1/1` + `[mmu-verilator] 3/3` + `[mmu][chmem] 3/3` (B.4 新增) + `[cpu-integration][mmu-chmem] 1/1` (C.6 新增)
  - Verilator smoke:`cpu_verilator_sim_add` 5/5 PASS
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

### E.2 [GREEN] `openspec/specs/verilator-mmu-bare-plumbing/spec.md` 新增 Requirement "TEST_CASE 6"

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
  - **TEST_CASE 6**: Verilator sv32 e2e flip (5 ELF tohost=1)
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

## 总进度追踪

| Phase | Tasks | 估时(人天) | 状态 |
|-------|-------|-----------|------|
| A 根因 Bug 修复 | A.1, A.2, A.3, A.3.1, A.4, A.5, A.6 | 5 | ⏸ 待启动 |
| B MMUPlugin CH_MEM | B.2, B.3, B.4, B.5 (B.1 跳过) | 10 | ⏸ 待启动 |
| C Pipeline 集成 | C.1, C.2, C.3, C.4, C.5, C.6 | 11 | ⏸ 待启动 |
| D 验证与回归 | D.0 ✅, D.1, D.2, D.3, D.4, D.5, D.6 | 5 (D.0 已完成) | 🔵 D.0 done, 余待启动 |
| E 文档与归档 | E.1, E.2, E.3, E.4, E.5, E.6 | 3 | ⏸ 待启动 |
| **合计** | **29 (28 待启动)** | **34 (≈ 6.0-6.2 人周)** | **1/29 ✅** |

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

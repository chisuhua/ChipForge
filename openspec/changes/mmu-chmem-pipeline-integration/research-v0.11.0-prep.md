# mmu_chmem 实装技术调研 (v0.11.0 启动前预备)

> **状态**: 📝 Draft v0.2 — Oracle 2026-10-07 已决议全部 5 项,等待用户最终批准 + proposal 文本修订
> **作者**: Sisyphus session (2026-10-08)
> **关联 change**: `openspec/changes/mmu-chmem-pipeline-integration` (placeholder, version_target: v0.11.0)
> **目标读者**: v0.11.0 启动期 owner (tasks.md 起草者)、Oracle reviewer

---

## 0. 元信息

| 项 | 值 |
|----|----|
| 关联 change | `mmu-chmem-pipeline-integration` (active, proposal.md 130 行, **无 tasks.md**) |
| 关联 initiative | `wave5-isa-coverage-and-bp` |
| 版本目标 | v0.11.0 |
| 强依赖 | `cpu-pipeline-mmufault-handler` (v0.10.x IN_PROGRESS) + `phase-6d-prerequisites` |
| 协同窗口 | `wave5-bp-btb` (同 v0.11.0 窗口,共享 fetch stage ownership) |
| 上游 precedent | `mfc-cpu-pipeline-multi-cycle-fsm` Phase D.1 (MulDivFsmPlugin CH_MEM 占位) |
| 风险等级 | 🔴 **High** — Oracle 已锁定 5 项决议 + 发现 shadow 双 bug + 1 个 proposal AC 矛盾 |
| Oracle session | `bg_13cd7890` (2026-10-07),详见 §12 |
| 估时(Oracle 调整) | **6.0-6.2 人周** (vs 原 5.6 估时;v0.10.0 期估 1-3 周失败教训) |

---

## 1. 当前状态盘点 (2026-10-08 实测基线)

### 1.1 TLM MMU — ✅ 已完整集成 + 真实内存读

| 组件 | 位置 | 状态 |
|------|------|------|
| 算法层 | `ip/mmu/lib/{tlb,tlb_base,tlb_entry,tlb_lookup,tlb_factory,multi_level_tlb,ptw,memory_interface}.h` | ✅ 完整,纯 C++,HDL-friendly |
| 替换策略 | `ip/mmu/policies/{fifo,lru,rrip,no_replacement,tlb_replacement_policy}.h` | ✅ 4 策略完整 |
| Plugin 层 | `ip/mmu/tlm/MMUPlugin.{h,cpp}` (151 + LOC) | ✅ 5-stage at_stage,csr_write_satp hook,sfence_vma,mmu_exit |
| Key 集合 | `ip/mmu/tlm/mmu_keys.h` (61 行) | ✅ 14 Key (10 原始 + 4 commit 8 + 1 PADDR_VALID) |
| 测试 | `tests/mmu/*` (11 文件) | ✅ **53/53 PASS,131 assertions** |
| 集成 | `[mmu-verilator]` 5/5 PASS (Bare plumbing,e2e flip 推迟 v0.11.0) |

**v0.10.4 hotfix 状态**: Bare shortcut `satp_ppn_==0` 误判 bug **未修复**,仅引入 `satp_value_` 字段 + 注释 (commit ad48fcf)。Sv-mode + satp_ppn=0 仍走 identity translation (vaddr 当 paddr)。

### 1.2 CH_MEM 现有部件 — ⚠️ 碎片化,缺核心

| 文件 | LOC | 状态 | 备注 |
|------|-----|------|------|
| `ip/cpu/plugins/mmu_ptw_chmem.h` | 314 | ✅ **完整 PoC** | sv32 5 状态 FSM (IDLE/L0_WAIT/L1_WAIT/DONE/FAULT),`CF_PLUGIN_USE_FSM_EXEMPT`,elaborate-time DAG 发射 |
| `ip/cpu/plugins/mmu_exception_handler_chmem.h` | 64 | ⚠️ **stub only** | mmufault_clear 永远 false,combinational network **未实装** (cf. file header §"当前状态") |
| `ip/mmu/tlm/MMUPlugin_chmem.h` | 0 | ❌ **不存在** | TLM-only MMUPlugin 无 CH_MEM 对应文件 (违反 ADR-040 v2.0 双文件分离) |
| `ip/cpu/plugins/ibus_chmem.h` | 155 | ⚠️ **fetch stub** | sv32 翻译结果消费路径 owner TBD (cf. §3.1) |
| `ip/cpu/plugins/dmem_chmem.h` | 145 | ⚠️ **load/store stub** | 同上 |
| `ip/cpu/cpu_factory_chmem.h:80-85` | (注释 6 行) | ❌ **owner gap 自证** | 源码注释:*"Real sv32 CH_MEM owner TBD; see change verilator-mmu-bare-plumbing-e2e"* |

### 1.3 CH_MEM 测试 — ✅ 验证基础存在

| 测试 | 位置 | 状态 |
|------|------|------|
| `test_mmu_ptw_fsm_chmem.cpp` (CH_MEM) | `tests/cpu/` | ✅ 3/3 PASS (sv32 walk success / reserved encoding / invalid PTE) |
| `test_chmem_multi_cycle_fsm.cpp` | `tests/framework/` | ✅ PASS (FSM DSL 模板验证) |
| `test_l1cache_refill_fsm_chmem.cpp` | `tests/cache/` | ✅ PASS (cache FSM precedent) |

### 1.4 跨模式集成 precedent — ✅ 模式成熟

| precedent | 关键模式 |
|-----------|---------|
| `hazard_chmem.h` (254 行) | RAW detection + `CtrlLink::halt_when(ch_bool)` + `pb.at_stage("decode", NORMAL, ...)` 闭包 + `node_of_logic_stage` 跨 stage 取值 |
| `decode_chmem.h` | decoder table + ch_uint<N> 解码 + `DECODED_INST` Payload key |
| `branch_chmem.h` | branch 决策 + 跳转 PC 计算 (ch_uint<XLEN>) |
| `cpu_factory_chmem.h` (424 行) | 5-stage 集成 + 11 baseline plugins + 5 topology + 2 lane dispatch |
| `l1_cache_refill_fsm_chmem.h` (228 行) | 多周期 FSM (IDLE/LOOKUP/MISS/REFILL_WAIT) + `set_input_value`/`get_value` 测试接口 |
| `mul_div_fsm_chmem.h` (69 行) | helper namespace 占位,future refactor target (Phase D.4) |

---

## 2. v0.11.0 目标 (源自 proposal.md §What Changes)

> **澄清**: 本节列出的是 **change 已经声明的目标**,非作者新创。tasks.md 起草者按本表 + §4 分解为可执行任务。

| § | 目标 | 文件 | 性质 |
|---|------|------|------|
| 1 | CH_MEM MMU/PTW pipeline 集成层实装 | `cpu_factory_chmem.h` + `mmu_ptw_chmem.h` + `ibus_chmem.h` + `mmu_exception_handler_chmem.h` (修改) | **核心实装** |
| 2 | MMUPlugin Bare shortcut 严格化 | `ip/mmu/tlm/MMUPlugin.cpp:109` (修) + `ip/mmu/tlm/MMUPlugin_chmem.h` (新增) | **Bug 修复** |
| 3 | RiscvMMUPlugin::csr_write_satp 注入时机审查 | `ip/cpu/plugins/mmu.cpp:34` | **Bug 修复** |
| 4 | Verilator TEST_CASE 6 mmu_sv32_translation_verilator_e2e_flipped | `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` | **验证** |
| 5 | [cpu-l1-mmu-demo] 真 sv32 翻转 TEST_CASE 7 | `tests/soc/test_cpu_l1_mmu_demo.cpp` | **验证** |
| 6 | AGENTS.md workaround 行更新 | `AGENTS.md` L41 | **文档** |
| 7 | Spec Delta 编辑 | `openspec/specs/verilator-mmu-bare-plumbing/spec.md` + `openspec/specs/cpu-l1-mmu-demo/spec.md` | **文档** |
| 8 | Architecture Gate Final | `tools/{verify_adr,verify_plugin_decision,check_plugin_portability,doc_link_check}.sh` | **门禁** |
| 9 | Regression Final Check | `bash tools/run_chipforge_tests.sh --all` | **回归** |
| 10 | Archive Preparation | `openspec archive mmu-chmem-pipeline-integration -y` | **流程** |

---

## 3. 关键差距分析 (Owner Gap Map)

### 3.1 缺 §1 MMUPlugin CH_MEM 文件

- **现状**: `ip/mmu/tlm/` 仅含 `MMUPlugin.h/cpp` (TLM),无 `_chmem.h` 对应。
- **影响**: 违反 ADR-040 v2.0 双文件分离 (TLM 文件内 `#ifdef CF_PLUGIN_USE_CH_MEM` 是技术债)。
- **决策点**: ⏳ `ip/mmu/tlm/MMUPlugin_chmem.h` 应该承载什么?
  - 选项 A: 单纯 `RiscvMMUPlugin` CH_MEM 实装 + csr/sfence hook
  - 选项 B: 完整 `MMUPlugin` 基类 CH_MEM (含 TLB lookup + PTW walk 全链路)
  - 选项 C: 仅声明 stub + bare plumbing,e2e flip 仍走 workaround
- **建议**: 选项 B (与 cache `l1_cache_refill_fsm_chmem.h` precedent 对齐),但需 Oracle 确认 sv32 5 状态 FSM 是否足以覆盖 v0.11.0 范围。

### 3.2 缺 §1 ibus/dmem CH_MEM sv32 消费路径

- **现状**: `ibus_chmem.h` (155 行) + `dmem_chmem.h` (145 行) 是 stub,不读 `pl::MMU_VADDR` / `pl::PADDR_VALID`。
- **影响**: 即使 `MMUPlugin` 写出 PADDR,IBus/DBus 也消费不到 → 真翻译结果在 fetch/memory stage 不生效。
- **决策点**: ⏳ IBus/DBus 应该什么时候实装 sv32 消费?
  - 选项 A: v0.11.0 内,作为 §1 pipeline 集成的一部分
  - 选项 B: v0.11.1+ 推迟,先用 stub (Bare plumbing only)
- **建议**: 选项 A。理由:v0.11.0 的硬指标 = `cpu-l1-mmu-demo` TEST_CASE 7 真 sv32 翻转 PASS (proposal §5)。不实装 IBus sv32 消费则 TEST_CASE 7 必然 FAIL。

### 3.3 缺 §1 mmu_exception_handler CH_MEM combinational network

- **现状**: `mmu_exception_handler_chmem.h` 是 64 行 stub,`mmufault_clear` 永远 false。
- **影响**: HazardPlugin stall_ctrl_ 接不到 mmufault → `cpu-l1-mmu-demo` 真 sv32 失败时无法 stall decode。
- **决策点**: ⏳ Combinational network 应该多完整?
  - 选项 A: 最小可行 — `mmufault_clear = (cpu_keys::CPU_EXCEPTION_CODE != 0)` 单条
  - 选项 B: 完整 — 含 trap PC 跳转、mtval 写、优先级排序
- **建议**: 选项 A (与 `cpu-pipeline-mmufault-handler` v1 scope 对齐)。trap PC 跳转走 mepc CSR 路径,在 CSRPlugin CH_MEM 范围内,本 change 不应跨界。

### 3.4 缺 §2 Bare shortcut 严格化修复

- **现状**: `MMUPlugin::do_lookup` 仍用 `satp_ppn_==0` 判定 (cf. `MMUPlugin.cpp:81-92` 注释自证 bug)。
- **影响**: Sv-mode + satp_ppn=0 → identity translation → vaddr 当 paddr 写 → 5 ELF 卡住。
- **决策点**: ⏳ Bare shortcut 判定公式应该是什么?
  - 选项 A: `sv_mode_ == Bare || (satp_value_ & MODE_mask) == 0` (proposal §2 推荐)
  - 选项 B: `satp_value_ == 0` (整个 CSR 为 0,语义最严格)
- **建议**: 选项 A,与 proposal §2 一致 + 与 `satp_value_` 字段命名意图对齐 (Bare=0, Sv32=bit31, Sv39/48=bit[63:60])。

### 3.5 缺 §3 csr_write_satp 注入时机审查

- **现状**: ctor 注入 satp_value 后,production CPU 不写 satp CSR 时,基类 satp_value_ 是否被 reset 未验证。
- **影响**: 真 sv32 测试需要 satp CSR 已设,production CPU 启动期 (reset vector 之前) satp_value 未配,MMU 走 PTW walk 但 PTE 未就绪 → fault。
- **决策点**: ⏳ 注入时机应该在哪里?
  - 选项 A: ctor 末尾按 sv_mode_ 自动编码 (cf. `MMUPlugin.cpp:37-44` v0.10.4 hotfix 注释)
  - 选项 B: cpu_factory ctor 注入 (cf. `cpu_factory.h:390` `satp_value` 字段)
  - 选项 C: 实装版 "先 ctor 默认值,再 RiscvMMUPlugin 派生类 ctor 覆盖"
- **建议**: 选项 C,需 Oracle 验证 `set_satp_value` 是否被派生类 shadow (proposal §Why 假设 #4)。

---

## 4. 实装分解建议 (tasks.md 草案)

> **状态**: 🟢 Oracle 2026-10-07 已决议全部 5 项,本节反映决议 + 估时调整。
> **声明**: 本节是研究输出,**非 tasks.md 终稿**。Owner 启动期需:
> 1. ✅ Oracle 决议 §6 5 个决策点 (已锁定)
> 2. ⏸ 与 `wave5-bp-btb` owner 协商 fetch stage 共享协议 (3 项,§7.4)
> 3. ⏸ 按本节分解写 tasks.md 并提交 review

### Task Group A: 根因 Bug 修复 (§2+§3 prerequisite, 含 shadow 双 bug)

| ID | 任务 | 文件 | 估时 |
|----|------|------|------|
| A1 | Bare shortcut 严格化 — 删除 `MMUPlugin.cpp:109` 的 `\|\| satp_ppn_ == 0` 析取项 (D4-A) | `ip/mmu/tlm/MMUPlugin.cpp:109` | 0.5d |
| A2 | ctor satp_value_ 自动编码 (已部分实装,补全 Sv39/Sv48 case 测试) | `ip/mmu/tlm/MMUPlugin.cpp` ctor 末尾 | 0.5d |
| A3 | **shadow 双 bug 修复** — 删除 `mmu.h:99-100,103` 派生三件套,让基类存取器被继承;`csr_write_satp` 复用 `cf::cpu::detail::extract_satp_ppn` (D5-TLM) | `ip/cpu/plugins/mmu.h` + `mmu.cpp:34-40` | 1.5d |
| A3.1 | 派生 getter/setter 调用点审计 — grep 全部 `satp_value()`/`set_satp_value` 调用,确认删除后语义兼容 | codebase-wide | 0.5d |
| A4 | 单元测试: Bare shortcut 3 case (Bare+Sv32_ppn0, Sv32_ppn_nonzero, Sv39_ppn_nonzero) + shadow 双 bug 单测 (现 ctor + csr_write_satp 后断言基类 satp_value/satp_ppn) | `tests/mmu/test_mmu_plugin.cpp` | 1.5d |
| A5 | **TLM flip 尝试 2 PPN 计算验证 (已完成 2026-10-08)** — Oracle 指出 `(0x80000000+0xF000)>>12 = 0x8000F` ≠ proposal 转述 `0x80000`,可能是比 shadow 更便宜的 root cause | debug print 验证 | ✅ 0.5d (已完成: runtime 实证 chain 正确,假设被排除,`0x80000` 是 doc typo) |
| A6 | Regression check: `[mmu] 53/53` 0 **意外**退化 (编码了 buggy 假设的用例允许更新并附理由) | — | 0.5d |

**估时小计**: 5d (~1 人周)

### Task Group B: MMUPlugin CH_MEM 实装 (§1+D1 收窄 scope)

> **D1 收窄 scope**: 单级 ch_mem TLB (8 项直接映射) + 复用 `PtWalkFsmPlugin` + 发射 PADDR/PADDR_VALID/PTW_ACTIVE。**显式 out-of-scope**: 多级 TLB / LRU/RRIP / ASID≠0 / Sv39/48 / 权限检查。

| ID | 任务 | 文件 | 估时 |
|----|------|------|------|
| B1 | ~~决定 `MMUPlugin_chmem.h` scope~~ (Oracle 已锁 D1-B 收窄,跳过) | — | 0d |
| B2 | **`RiscvMMUPlugin_chmem.h` 实装** — elaboration-time satp config 注入 + 单一 ch_reg 初值 (D5-CH_MEM 约束) | `ip/cpu/plugins/mmu_chmem.h` (新增) | 2.5d |
| B3 | **`MMUPlugin_chmem.h` 收窄版实装** — 单级 ch_mem TLB (8 项直接映射) + 发射 PADDR/PADDR_VALID/PTW_ACTIVE (D1-B 收窄) | `ip/mmu/tlm/MMUPlugin_chmem.h` (新增) | 4.5d |
| B4 | PTW FSM 调用 `mmu_ptw_chmem.h` 现有 `PtWalkFsmPlugin` (复用,不改) | `ip/mmu/tlm/MMUPlugin_chmem.h` | 1d |
| B5 | 单元测试: `test_mmu_chmem_basic` (TLB hit 走 lookup,miss 走 PTW walk) | `tests/mmu/test_mmu_chmem.cpp` (新增) | 2d |

**估时小计**: 10d (~2 人周)

### Task Group C: Pipeline 集成 (§1+§3.2+§3.3 + byte-identical 保护)

| ID | 任务 | 文件 | 估时 |
|----|------|------|------|
| C1 | `mmu_exception_handler_chmem.h` combinational network 实装 — mmufault_clear 单条 `= (CPU_EXCEPTION_CODE != 0)` (D3-A) | `ip/cpu/plugins/mmu_exception_handler_chmem.h` | 1.5d |
| C2 | `ibus_chmem.h` sv32 消费 — 读 `pl::MMU_VADDR` / `pl::PADDR_VALID` → emit vaddr for fetch,**bare 路径零额外 latency** (D2-A) | `ip/cpu/plugins/ibus_chmem.h` | 2.5d |
| C3 | `dmem_chmem.h` sv32 消费 — 同上 load/store 路径,bare 路径零额外 latency | `ip/cpu/plugins/dmem_chmem.h` | 2.5d |
| C4 | `cpu_factory_chmem.h` owner 真空修复 — 移除 "owner TBD" 注释,接入 MMUPlugin CH_MEM 实装 | `ip/cpu/cpu_factory_chmem.h:80-85` | 1d |
| C5 | HazardPlugin CH_MEM 接 mmufault_clear — `mark_mmufault`/`clear_mmufault` 实装 + `CtrlLink::halt_when` | `ip/cpu/plugins/hazard_chmem.h:151-153` | 1.5d |
| C6 | 集成测试: `test_cpu_chmem_mmu_pipeline` — 真 sv32 PTE ELF + plant_identity_page_table + tohost=1 | `tests/cpu/test_cpu_chmem_mmu_pipeline.cpp` (新增) | 2d |

**估时小计**: 11d (~2.2 人周)

### Task Group D: 验证与回归 (§4+§5+§8+§9 + proposal 文本修订)

| ID | 任务 | 文件 | 估时 |
|----|------|------|------|
| D0 | **proposal 文本修订** — 删 §4 最后一行 "trap PC 跳转断言" AC (与 D3-A 矛盾),降级为 "EXCEPTION_CODE 正确传播 + pipeline stall";声明 D1 收窄 scope (显式 out-of-scope 列表) | `openspec/changes/mmu-chmem-pipeline-integration/proposal.md` | 0.5d |
| D1 | `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` TEST_CASE 6 新增 sv32 e2e flip | 同 | 1.5d |
| D2 | `tests/soc/test_cpu_l1_mmu_demo.cpp` TEST_CASE 7 新增真 sv32 翻转 (7/7 PASS) | 同 | 1d |
| D3 | sv32 PTE ELF vendor — build_sv32_pte.S + register build_sv32_pte in build_manual_elf.sh | `tests/cpu/manual_elf/` | 0.5d |
| D4 | sv32 baseline CSV — 5 ELF × median cycle × mode=sv32_mmu | `soc/cpu/docs/dse/sv32-baseline-matrix.csv` | 0.5d |
| D5 | Architecture Gate — `verify_adr` + `verify_plugin_decision` + `check_plugin_portability` + `doc_link_check` | — | 0.5d |
| D6 | Regression Final — `bash tools/run_chipforge_tests.sh --all` | — | 0.5d |

**估时小计**: 5d (~1 人周)

### Task Group E: 文档与归档 (§6+§7+§10)

| ID | 任务 | 文件 | 估时 |
|----|------|------|------|
| E1 | AGENTS.md L41 workaround 移除 + `[v0.11.0 follow-up]` 标记 | `AGENTS.md` | 0.5d |
| E2 | `openspec/specs/verilator-mmu-bare-plumbing/spec.md` 新增 Requirement "TEST_CASE 6" | 同 | 0.5d |
| E3 | `openspec/specs/cpu-l1-mmu-demo/spec.md` 新增 Requirement "TEST_CASE 7" + 移除 workaround 标注 | 同 | 0.5d |
| E4 | `openspec change validate mmu-chmem-pipeline-integration` 0 error | — | 0.5d |
| E5 | `CHANGELOG.md` v0.11.0 段新增条目 | `CHANGELOG.md` | 0.5d |
| E6 | `openspec archive mmu-chmem-pipeline-integration -y` | — | 0.5d |

**估时小计**: 3d (~0.6 人周)

### 总估时 (Oracle 调整后)

| Group | 人天 | 人周 | 调整 |
|-------|------|------|------|
| A 根因 | 5 | 1.0 | +1.5d (shadow 删除 + mask 修复 + 派生 getter 调用点审计 + 2 个新单测 + PPN 验证) |
| B MMU CH_MEM | 10 | 2.0 | +2d (收窄版仍偏乐观,ch_mem 单级 TLB + FSM 编排 + PADDR 契约是新建) |
| C Pipeline 集成 | 11 | 2.2 | +2d (byte-identical baseline 保护有真实成本) |
| D 验证 | 5 | 1.0 | +0.5d (新增 D0 proposal 文本修订) |
| E 文档 | 3 | 0.6 | 0 |
| **合计** | **34** | **~6.0-6.2 人周** | +6d (vs 5.6 原始估时) |

> **对比 v0.10.0 估算 (Oracle 2026-10-07)**: 1-3 周。本研究两次上调: 原始 5.6 → Oracle 调整 6.0-6.2 人周,因 v0.10.0 估时仅含 Part a/b flip,未含 MMUPlugin CH_MEM + csr_write_satp shadow 修复 + 文档/回归/归档。
>
> **降级路径** (若 v0.11.0 窗口硬性 < 5 周): D1 收窄版再砍为"TLB-less"(每 access 走 PTW FSM) → 省 ~1.5d,但 5-指令 ELF 可接受,**必须在 proposal 显式标 PoC 限制**。

---

## 5. 风险登记 (Risk Register)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|------|------|------|
| R1 | Bare shortcut 修复引入 Sv39/Sv48 退化 — **Oracle: 实际比预估更严重**(53 用例中凡 sv_mode_≠Bare 且从未 `set_satp_ppn(nonzero)` 的用例,当前靠 buggy 假设走 identity;修复后进入 walk) | 中→**高** | 高 | A1 前快照 `[mmu] 53/53` + `[cpu-integration] 81/81` + `[cpu-l1-mmu-demo] 6/6` 基线;逐一审计 sv_mode_≠Bare 用例;允许"零**意外**退化" (编码了 buggy 假设的用例更新并附理由) |
| R2 | ~~csr_write_satp shadow bug 修复需要改 RiscvMMUPlugin 派生链~~ | ~~高~~ | ~~高~~ | ✅ **Oracle 已验证为真,且是双 bug (shadow #1 + mask #2)。修复已锁定**: 删除 `mmu.h:99-100,103` 派生三件套,让基类存取器被继承;`csr_write_satp` 复用 `cf::cpu::detail::extract_satp_ppn` |
| R3 | ~~`MMUPlugin_chmem.h` scope 决策错误导致返工~~ | ~~中~~ | ~~中~~ | ✅ **Oracle 已锁定 D1-B 收窄版**。Tasks A3.1 (派生 getter 调用点审计) + B2-B5 (收窄 scope 实装) 严格遵守 |
| R4 | `ibus_chmem.h` / `dmem_chmem.h` sv32 消费实装触发现有 baseline 退化 | 中 | 高 | C2-C3 用 `[cpu-integration] 81/81` + `[chmem]` + `[verilator]` baseline 锁定对比;**bare 路径必须 byte-identical 保护** (Oracle D2-A 强调) |
| R5 | `mmu_exception_handler_chmem.h` combinational network 误接 stall_ctrl_ 触发 fetch stall 退化 | 中 | 高 | C1 单独 commit + `[riscv-tests] 40/48` + `[cpu-l1-mmu-demo] 6/6` 维持 |
| R6 | sv32 PTE ELF vendor 失败 (build_sv32_pte.S 链接问题) | 低 | 中 | D3 早期启动,与 B/C 并行 |
| R7 | `wave5-bp-btb` 同窗口 fetch stage 冲突 — **Oracle 细化**: pc_reg next-mux 三方优先级协议 + stall 语义 + fetch at_stage 注册顺序 | 中 | 高 | **强制启动期与 bp-btb owner 协商 3 项** (Oracle §7.4),否则 elaboration DAG 多驱动 |
| R8 | v0.11.0 window 受 `mfc-extract-fsm-h` Phase D.4 影响 (若 D.4 提前) | 低 | 低 | B4 复用现有 `mmu_ptw_chmem.h`,不依赖 fsm.h 提取 |
| R9 | megapage (level-1 叶, 4MB) PoC 简化透传 ppn 触发后续回归 | 低 | 中 | 完整 PPN 拼接推迟 v0.11.1+,本 change 显式记录 PoC 限制 |
| R10 | `[cpu-l1-mmu-demo]` 真 sv32 翻转后,既有 workaround 路径移除触发 baseline 退化 | 中 | 中 | E1 前先 E5 验证 workaround 是否真无回归 |
| **R11** | **CH_MEM 流水线无 CSR plugin** (`cpu_factory_chmem.h:304-310` 注册清单无 CSR) — Oracle 新发现,影响 D1/D3/D5 CH_MEM 侧答案 | 中 | 高 | **CH_MEM 侧 satp 必须是 ctor/config 参数,elaboration 期固化为 ch_reg 初值**;trap PC 跳转单独立 change (v0.11.1+) |
| **R12** | **TLM flip 尝试 2 planted root PPN 计算错误** — `(0x80000000+0xF000)>>12 = 0x8000F` ≠ proposal 转述 `0x80000`;PTW root 指向 ELF 代码页,可能是比 shadow 更便宜的 root cause | 中 | 中 | **A5 启动期先用 print/debug 验证实际 planted root PPN 与 satp_ppn 是否相等** → ✅ **已关闭 (2026-10-08)**: A5 实证 `set_satp_ppn` runtime 链正确,`0x80000` 是 proposal 文档 typo (line 26 已修为 `0x8000F`),不是代码 bug;"更便宜 root cause"假设被排除,Oracle D4-A + D5-TLM 确认为真 root cause |
| **R13** | v0.11.0 窗口硬性 < 5 周触发降级路径 — D1 收窄版再砍为"TLB-less" | 低 | 中 | 提前在 proposal D0 标注 PoC 限制 (TLB-less path) 备用 |
| **R14** | proposal §4 "trap PC 跳转断言" AC 与 D3-A 矛盾 — Oracle 明确指出 | 高 | 低 | **D0 必须先修订 proposal 文本**,tasks.md 起草前完成 |

---

## 6. 决策点 (Oracle 2026-10-07 已决议全部 5 项)

> **状态**: 🟢 全部 5 项已 Oracle 决议并锁定。tasks.md 起草可基于本节。

### D1. `MMUPlugin_chmem.h` scope — ✅ **B 收窄版**

- **决议**: B,但**显式收窄**(非 1:1 移植 `MultiLevelTLB`)
- **scope 内容**:
  - 单级直接映射小 TLB (ch_mem,如 8 项,可 PR 内调参)
  - 复用现有 `PtWalkFsmPlugin` (`mmu_ptw_chmem.h`,0 修改)
  - 发射 `PADDR`/`PADDR_VALID`/`PTW_ACTIVE` + satp 作为 elaboration-time 配置的 ch_reg
- **显式 out-of-scope** (must declare in proposal):
  - 多级 TLB (L0/L1/Ln-1 编排)
  - LRU/RRIP 替换策略 (单级用直接映射)
  - ASID≠0 路径
  - Sv39/Sv48 翻译
  - 访问权限检查 (R/W/X vs load/store/ifetch)
- **Reference**: `l1_cache_refill_fsm_chmem.h` precedent (FSM 编排,非全量 L1Cache 移植)

### D2. IBus/DBus sv32 消费实装时机 — ✅ **A (v0.11.0 内)**

- **决议**: v0.11.0 内实装
- **修正引证**: 真正硬依赖 = **TEST_CASE 6 (Verilator sv32 e2e, proposal §4)**,不是 TEST_CASE 7 (TLM binary,`chipforge_tests`)
- **实施要点**:
  - 现状 `ibus_chmem.h:128-129` 直接 `aread(bits<15,2>(pc_val))` —— vaddr 当 window index 用
  - 最小实装: `aread(bits<15,2>(select(paddr_valid, paddr, pc)))` + PTW_ACTIVE 期间 fetch stall
  - bare 路径必须 byte-identical 保护 (`[chmem]` + `[verilator]` baseline 不退)

### D3. mmu_exception_handler combinational 范围 — ✅ **A (最小)**

- **决议**: 最小 — `mmufault_clear = (CPU_EXCEPTION_CODE != 0)` 单条
- **理由**: B 在 v0.11.0 **物理上不可能** — CH_MEM 流水线无 CSR plugin (`cpu_factory_chmem.h:304-310` 注册清单无 CSR),trap PC 跳转需要 mepc/mtvec CSR 读写
- **副作用**: proposal §4 最后一行 "trap PC 跳转断言" AC 与 D3-A 矛盾 → **必须修订 proposal 文本**(走 `openspec-propose` 修订流程)→ tasks.md 起草前完成
- **降级验收**: "EXCEPTION_CODE 正确传播 + pipeline stall" (替代 trap PC 跳转)

### D4. Bare shortcut 判定公式 — ✅ **A (删除析取项)**

- **决议**: 删除 `MMUPlugin.cpp:109` 的 `|| satp_ppn_ == 0` 析取项
- **理由**: 现代码 `MMUPlugin.cpp:95-108` 已正确从 `satp_value_` 提取 `satp_mode`,bug 仅在 `:109` 保留误判条件。B 选项 `satp_value_ == 0` 是语义错(RISC-V spec §4.3.1: MODE=0 即 Bare,其余字段忽略)
- **副作用**: **R1 比预估更严重**。`[mmu]` 53 测试中,凡 sv_mode_≠Bare 且从未 `set_satp_ppn(nonzero)` 的用例,当前靠 `satp_ppn_==0` 走 identity;修复后它们将进入 TLB lookup → miss → PTW walk(root PPN=0)。`test_ptw_tlb_refill_integration` 已显式 set PPN(安全),其余需逐一审计
- **目标改述**: "53/53 零**意外**退化;编码了 buggy 假设的用例允许更新并附理由"

### D5. csr_write_satp 注入时机 — ✅ **C (TLM) + shadow 修复**

- **TLM 侧决议**: C (维持现行实装) — 基类 ctor 按 sv_mode_ 自动编码 (`MMUPlugin.cpp:46-52`) + 派生 ctor qualified 覆盖 (`mmu.h:52`)
- **CH_MEM 侧约束** (非选项,是约束): CH_MEM 流水线无 CSR plugin → 运行期 `csrw satp` 无消费者 → sv32 PTE ELF 无法自配 satp → **satp 必须是 MMU CH_MEM 的 ctor/config 参数,elaboration 期固化为 ch_reg 初值**
- **shadow 假设 Oracle 验证为真,且是双 bug** (详见 §12 新发现):
  - Bug #1: `mmu.h:100` 派生 `set_satp_value` shadow 基类非虚函数 + `mmu.h:103` 派生重复成员 → 基类 `satp_value_` 在 `csr_write_satp` 后**永不更新**
  - Bug #2: `mmu.cpp:37` `& 0x0FFFFFFFFFFFFULL` 48-bit mask 把 Sv32 MODE bit31 含进 PPN → PTW root 指向天文地址
- **Oracle 推荐修复**: **删除派生三件套** (`mmu.h:99-100` getter/setter + `:103` 成员),让基类存取器被继承;`csr_write_satp` 复用 `cf::cpu::detail::extract_satp_ppn` (单测入口已存在)。单一事实源,根除整个 shadow bug 类别
- **tasks.md 拆分**: D5 应拆成 "TLM: C + shadow 修复" / "CH_MEM: config 注入" 两条

---

## 7. 关联项与依赖图

### 7.1 v0.11.0 启动前置

```
v0.10.x cpu-pipeline-mmufault-handler v1 archive
  ↓
v0.10.x phase-6d-prerequisites archive
  ↓
[并行启动]
├── mmu-chmem-pipeline-integration (本 change)
└── wave5-bp-btb (同窗口,共享 fetch stage)
```

### 7.2 上游 precedent (可复用)

| precedent | 复用点 |
|-----------|--------|
| `mmu_ptw_chmem.h` (314 行) | B4 直接复用,无修改 |
| `l1_cache_refill_fsm_chmem.h` (228 行) | B3 FSM 编排模式参考 |
| `hazard_chmem.h` (254 行) | C5 mmufault_clear → stall_ctrl_ 接法参考 |
| `decode_chmem.h` | B3 Payload key 命名约定参考 |

### 7.3 与 `mfc-extract-fsm-h` 的关系

- `mfc-extract-fsm-h` 是 `mul_div_fsm.h` CH_MEM inline → `mul_div_fsm_chmem.h` 重构
- 本 change **不依赖** `mfc-extract-fsm-h`,因为 `mmu_ptw_chmem.h` 已 ADR-040 v2.0 严格分离
- 同 v0.11.0 窗口,但 scope 互不重叠,需 owner 协调启动顺序

---

## 8. 验证策略 (Verification Plan)

### 8.1 测试金字塔

```
Level A (lib/ 单元)        — 已存在,不动
  ↓
Level B (Plugin 集成)      — A4 新增 Bare shortcut 3 case
  ↓
Level C (CPU 集成)         — C6 新增 test_cpu_chmem_mmu_pipeline
  ↓
Level D (SoC demo)         — D2 新增 TEST_CASE 7 真 sv32 翻转
  ↓
Level E (Verilator e2e)    — D1 新增 TEST_CASE 6 sv32_translation_verilator_e2e_flipped
```

### 8.2 退出标准 (Exit Criteria)

- [ ] A1-A5 全 PASS,`[mmu]` 0 退化
- [ ] B1-B5 全 PASS,`[chmem]` 0 退化
- [ ] C1-C6 全 PASS,`[cpu-integration] 81/81` + `[cpu-l1-mmu-demo] 7/7`
- [ ] D1-D6 全 PASS,`[verilator] 1/1` + `[mmu-verilator] 5/5` + `[mmu] 53/53` + `[cpu-l1-mmu-demo] 7/7` + `[cpu] 19/19`
- [ ] E1-E6 全部 commit
- [ ] 4 个 Architecture Gate 全 0 error
- [ ] `bash tools/run_chipforge_tests.sh --all` 全 PASS

---

## 9. 已知陷阱 (Trap List)

> 来自现有 CH_MEM 实装的踩坑经验,Owner 启动期注意。

1. **`ch_literal<0, N>{}` 必须**: `ch_reg` / `ch_mem` 初始化用字面值,避免 v0.3.1 M6 SEGV (`mul_div_fsm_chmem.h` 上游教训)
2. **at_stage 闭包内禁用运行期 `if(ch_bool)`**: `check_plugin_portability.sh` Check 5 静态 grep 兜底 (`hazard_chmem.h` 已示范)
3. **PoC 测试显式管理 `ch::core::context` lifecycle**: `set_as_current_context()` 在每个 SECTION 入口 (`test_mmu_ptw_fsm_chmem.cpp` 示范)
4. **`ch_bool::explicit operator bool()` + C++17 contextual conversion 静默取 false**: 必须显式 `ch_bool(condition)` 包装
5. **`uint_t<N>` 在 TLM vs CH_MEM 是不同类型**: `cf::plugin::uint_t<N>` (POD) vs `ch::core::ch_uint<N>`,业务代码不感知但 CI 抓 (`check_plugin_portability.sh` Check 7)
6. **MMUPlugin lib/ vs tlm/ 严格切分**: `lib/` 0 依赖 `cf::plugin::*` (除 `uint_t.h`);`tlm/` 反之 (D4 强制)

---

## 10. 待续工作 (Next Steps) — Oracle 调整后

### 10.1 立即 (本周)

1. ✅ **本文档 review** — 用户已 review,Oracle 2026-10-07 已决议全部 5 项
2. ✅ **Oracle 决议 D1-D5** — 全部锁定 (见 §6)
3. ⏸ **D0: 修订 proposal 文本** — **Oracle R14 强制**: 删 §4 最后一行 "trap PC 跳转断言" AC (与 D3-A 矛盾),降级为 "EXCEPTION_CODE 正确传播 + pipeline stall";声明 D1 收窄 scope (显式 out-of-scope 列表);走 `openspec-propose` 修订流程
4. ⏸ **tasks.md 起草** — 基于 §4 调整后 (含 A3.1/A5/D0/B2/B3/C1-C6 拆细 + 估时 6.0-6.2 人周) 写 `openspec/changes/mmu-chmem-pipeline-integration/tasks.md`
5. ⏸ **A5 启动期 print/debug 验证** — Oracle R12: 先核实实际 planted root PPN 与 satp_ppn 是否相等,可能是比 shadow 更便宜的 root cause
6. ⏸ **R13 降级路径标 PoC** — 若 v0.11.0 窗口 < 5 周,D1 收窄版再砍为"TLB-less",需在 proposal D0 标注

### 10.2 启动期 (v0.11.0 owner 上任后)

1. **A1-A6 根因修复 commit** — 必须前置,**含 shadow 双 bug** (A3 + A3.1) + PPN 验证 (A5) + 53 用例逐一审计 (A6),否则 B/C 失败率极高
2. **wave5-bp-btb 协调会议** — **Oracle 细化 3 项** (详见 §7.4):
   - pc_reg next-mux 优先级协议 (`stall > branch actual > BP predict`)
   - stall 语义 (BP 冻结期间不推测)
   - fetch stage at_stage 注册顺序 (MMU substage 先于 IBus fetch NORMAL)
3. **`docs/research/mmu_chmem_design.md`** — 详细设计文档 (本文件是 research 摘要)
4. **PR 顺序合并**:
   - PR1-A (A1-A6) → PR1-B (B2-B5, B1 已锁) → PR1-C (C1-C6) → PR1-D (D0-D6) → PR1-E (E1-E6)

### 10.3 后续 (v0.11.1+ 路线图项)

- Sv39/Sv48 CH_MEM 实装 (依赖 sv32 验证稳定)
- megapage 完整 PPN 拼接 (`mmu_ptw_chmem.h:82` 简化透传 → 完整拼接)
- 访问权限检查 (R/W/X vs load/store/ifetch,PoC 跳过)
- split_id 拓扑 (`ip/mmu/STATUS.md` L41 已 defer)
- MMUTLMBridge CH_MEM 版 (与 L1CacheTLMBridge 同构,TLM-only defer)
- **CSRPlugin CH_MEM** (Oracle R11: 当前流水线无 CSR plugin,trap PC 跳转/mepc/mtvec CSR 读写无法实现,需独立 change)
- **tlm/CH_MEM satp 单一事实源收敛** (Oracle §12.2: `MMUPlugin.cpp:46-52` ctor 与 `cpu_factory.h:444 make_satp_value` 存在职责重叠)
- rtl/ 实装 (Phase 5+,依赖 CH_MEM 完整化)

---

## 11. 参考资料

### 11.1 OpenSpec changes

- `openspec/changes/mmu-chmem-pipeline-integration/proposal.md` (本 change 主文档)
- `openspec/changes/mfc-extract-fsm-h/proposal.md` (同 v0.11.0 窗口)
- `openspec/changes/mfc-cpu-pipeline-multi-cycle-fsm/proposal.md` (Phase D.1 PoC precedent)
- `openspec/changes/mmufault-verilator-sv32-e2e-flip` (Part a/b 来源)
- `openspec/changes/verilator-mmu-bare-plumbing-e2e` (Part 1 plumbing)
- `openspec/changes/archive/2026-09-21-phase-6d-fsm-chmem` (FSM CH_MEM archive)

### 11.2 ADRs

- **ADR-040 v2.0**: TLM→HDL 移植性约束 (CH_MEM 是新正道)
- **ADR-046**: 多周期协议引擎豁免 D4 无状态机禁令
- **ADR-044**: L1 Cache VIPT coherence (MMU 输出 `pl::MMU_VADDR` 已落地)
- **ADR-049**: D4 范式方法学 (lib/tlm 严格切分)

### 11.3 关键代码引用

| 文件 | 引用 |
|------|------|
| `ip/mmu/tlm/MMUPlugin.cpp:81-92` | Bare shortcut bug 自证注释 |
| `ip/mmu/tlm/MMUPlugin.cpp:95-108` | satp_mode 字段提取正确实装 (bug 仅在 :109 析取项) |
| `ip/mmu/tlm/MMUPlugin.cpp:109` | Bare shortcut 误判条件 (D4-A 删除目标) |
| `ip/mmu/tlm/MMUPlugin.cpp:37-44` | ctor satp_value_ 自动编码意图 (v0.10.4 hotfix) |
| `ip/cpu/plugins/mmu_ptw_chmem.h:131-137` | sv32 5 状态枚举 |
| `ip/cpu/plugins/mmu_exception_handler_chmem.h:10-17` | 当前 stub 限制自证 |
| `ip/cpu/cpu_factory_chmem.h:80-85` | "Real sv32 CH_MEM owner TBD" 自证 |
| `ip/cpu/cpu_factory_chmem.h:304-310` | CH_MEM 注册清单无 CSR plugin (Oracle R11 新发现) |
| `ip/cpu/plugins/hazard_chmem.h:151-153` | mark/clear mmufault stub |
| **`ip/cpu/plugins/mmu.h:99-100,103`** | **Oracle 新发现: shadow 三件套** (派生 getter/setter + 重复成员) |
| **`ip/cpu/plugins/mmu.cpp:36-40`** | **Oracle 新发现: csr_write_satp 实装 — shadow #1 (派生 set_satp_value 隐藏基类) + mask #2 (48-bit mask 把 Sv32 MODE bit31 含进 PPN)** |
| **`ip/cpu/plugins/mmu.h:44-52`** | **Oracle 新发现: extract_satp_ppn 单测入口已存在,可被 csr_write_satp 复用** |
| `tests/cpu/test_mmu_ptw_fsm_chmem.cpp:36-49` | sv32 walk success 时序 |

### 11.4 Oracle 决议记录

- **Session ID**: `bg_13cd7890` (2026-10-07, completed)
- **Continuation ID**: `ses_ee792049bffeb9f9NoHSZijeWj`
- **触发背景**: v0.11.0 mmu_chmem 实装启动期需 5 项决策
- **决议**: D1-B 收窄 / D2-A / D3-A / D4-A / D5-C+shadow 修复
- **关键发现**: shadow 双 bug (R2 验证) + proposal AC 矛盾 (R14) + PPN 计算错误可能 (R12) + CH_MEM 无 CSR (R11)

---

## 12. Oracle 2026-10-07 决议详情与新发现

> 本节记录 Oracle consultation 的完整推理与未在 §4-§11 体现的额外发现。tasks.md 起草者必读。

### 12.1 Shadow 双 bug 验证路径 (A3 + A3.1 测试参考)

Oracle 通过代码自证 `set_satp_value` shadow 假设为真。owner 写单测时按以下 3 步构造 FAIL-then-PASS 证据:

1. 构造 `RiscvMMUPlugin(Sv32, ..., satp_value=0x80080000)`,断言 `MMUPlugin::satp_value()==0x80080000`
   - 现今 PASS (ctor 走 qualified 调用)
2. 调 `csr_write_satp(0x80040000)`,断言基类 `satp_value()==0x80040000`
   - **现今 FAIL** (证明 shadow bug #1 — 基类 satp_value_ 在 csr_write_satp 后永不更新)
3. 断言 `satp_ppn()==0x40000`
   - **现今 FAIL** (现今得 0x80040000,证明 mask bug #2 — 48-bit mask 把 Sv32 MODE bit31 含进 PPN)

**Oracle 推荐修复路径** (单一事实源,根除整个 shadow bug 类别):
- 删除派生 shadow 三件套 (`mmu.h:99-100` getter/setter + `:103` 重复成员)
- 让基类 `MMUPlugin` 的 `set_satp_value`/`satp_value`/`set_satp_ppn`/`satp_ppn` 存取器被 `RiscvMMUPlugin` 继承
- `csr_write_satp` 复用 `cf::cpu::detail::extract_satp_ppn` (`mmu.h:44-52` 注释已自证此为单测入口,直接复用而非再复制 6 行)

### 12.2 TLM Flip 尝试 2 PPN 计算可能错误 (R12)

research §Why 转述尝试 2: *"cfg.satp_ppn = (window_base+60*1024)>>12 = 0x80000"*

但 `(0x80000000+0xF000)>>12 = 0x8000F`,**不是 0x80000**。

若转述准确,尝试 2 的 PTW root 指向 ELF 代码页(指令字节被当 PTE 读),这本身足以解释"5 ELF 卡住"。**建议 A5 前用一次 print/debug 核实实际 planted root PPN 与 satp_ppn 是否相等** — 这是比 shadow 更便宜的检查,可能是 TLM flip 失败的近因,应优先排除。

> **A5 验证结果 (2026-10-08, 已关闭)**: A5 在 `MMUPlugin::do_lookup` 入口加临时 fprintf + 跑 `[mmu][tlb-refill]` (用 `set_satp_ppn(1ULL<<20) = 0x100000`),捕获 runtime `satp_ppn_` = `0x100000` (与调用值完全一致),证明 `set_satp_ppn` 路径正确。静态链分析 (`make_satp_value` → `RiscvMMUPlugin` ctor Sv32 mask `0x3FFFFFULL` → `set_satp_ppn`) 对 `0x8000F` 同样正确。**结论: `0x80000` 是 proposal line 26 的文档 typo (已修为 `0x8000F`),不是代码 bug;本假设被排除,Oracle D4-A + D5-TLM 是确认真 root cause。** 另注:`mmufault-verilator-sv32-e2e-flip` 是 placeholder change,"5 ELF 卡住" 失败模式无现存可重放测试,A5 实证基于现有最近路径 + 静态分析。

### 12.3 CH_MEM 流水线无 CSR plugin (R11)

Oracle 验证 `cpu_factory_chmem.h:304-310` 注册清单:

```
CH_MEM Plugin 注册 (v0.10.x): Decoder + Hazard + IBus + RegFile + IntAlu + Branch + DMem (7 plugins)
TLM Plugin 注册 (baseline):   上述 + CSRPlugin + MMUPlugin + ... (含 CSR)
```

CH_MEM 流水线**无 CSR plugin** → 运行期 `csrw satp` 无消费者 → sv32 PTE ELF 无法自配 satp → **satp 必须是 MMU CH_MEM 的 ctor/config 参数,elaboration 期固化为 ch_reg 初值**。这不是选项而是约束。

副作用:
- trap PC 跳转 (需 mepc/mtvec CSR 读写) 在 v0.11.0 **物理上不可能**
- proposal §4 "trap PC 跳转断言" AC 与 D3-A 矛盾 → D0 必须先修订 proposal
- CSRPlugin CH_MEM 实装应单独立 change (v0.11.1+ 候选)

### 12.4 TLM/CH_MEM satp 单一事实源收敛机会 (v0.11.1+)

Oracle 观察到职责重叠:
- `MMUPlugin.cpp:46-52` ctor 按 sv_mode_ 自动编码 satp_value_
- `cpu_factory.h:444 make_satp_value` 也编码 satp_value

两处都会编 satp_value。**v0.11.1+ 可考虑收敛到单一入口** (避免 future satp encoding 规则改动需改两处)。本 change 不处理,记入 §10.3 后续项。

### 12.5 wave5-bp-btb 协调议题 (Oracle 细化 3 项)

启动期必须与 bp-btb owner 协商:

| # | 议题 | 建议协议 |
|---|------|---------|
| 1 | pc_reg next-mux 优先级 | `stall > branch actual > BP predict` — 否则 elaboration DAG 多驱动 |
| 2 | stall 语义 | MMU `CtrlLink::halt_when(ptw_active)` 冻结 fetch;BP 冻结期间**不推测** (v0.11.0 简化) |
| 3 | fetch stage at_stage 注册顺序 | MMU substage 先于 IBus fetch NORMAL(ADR-048 canonical ordering 等价物)— 谁拥有 `fetch` stage 的 substage 声明权需明确,避免 `declare_substage("fetch", ...)` 冲突 |

### 12.6 Bare shortcut 修复后 53/53 用例逐一审计清单 (A6 任务)

Oracle 指出"零退化"目标应改述为"零**意外**退化;编码了 buggy 假设的用例允许更新并附理由"。

需审计的用例类别:
1. sv_mode_≠Bare + 从未 `set_satp_ppn(nonzero)` — 当前靠 `satp_ppn_==0` 走 identity,修复后进入 walk
2. `test_ptw_tlb_refill_integration` 已显式 set PPN (安全,可豁免审计)
3. 其余 sv_mode_=Bare 用例不受影响

建议 A6 owner 写一个 grep 脚本列出所有 sv_mode_ 配置点 + 对应用例,逐一标注"安全/需更新"。

---

**文档版本**: v0.2 Draft (Oracle-locked)
**下次更新**: 用户最终批准 + D0 proposal 文本修订后 → v0.3 (tasks.md-ready)
**作者注释**: 本文档基于 2026-10-08 实测基线 + Oracle 2026-10-07 决议起草,所有路径/行号均当时验证。如发现漂移,优先信任 git HEAD 实测。

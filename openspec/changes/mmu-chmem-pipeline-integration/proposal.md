---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: placeholder
depends_on:
  - cpu-pipeline-mmufault-handler
  - phase-6d-prerequisites
---

# mmu-chmem-pipeline-integration — CH_MEM MMU/PTW pipeline 集成 (v0.11.0 owner)

> **Scope 拆分 (2026-10-07, Sisyphus 决策)**: 本 change 从 `mmufault-verilator-sv32-e2e-flip` (Part a/b) 拆分出的 follow-up，承担 **CH_MEM 模式 MMU/PTW pipeline 集成层**职责。
>
> **v0.10.0 期内 NOT IMPLEMENTED** — 仅作占位,实际启动推迟 v0.11.0 (与 `wave5-bp-btb` 同窗口)。

## Why

`mmufault-verilator-sv32-e2e-flip` (wave5 P1) Part a (TLM `cfg.enable_mmu=true` flip) 实装 3 次尝试均 FAIL (2026-10-07 Sisyphus bootstrap session):

1. **尝试 1**: `cfg.enable_mmu=true` + `plant_identity_page_table(mem, window_base+60*1024, elf.entry_addr)` — 5 个 ELF 10000 cycles 卡住,exit_code=-1
2. **尝试 2**: 加 `cfg.satp_ppn = (window_base+60*1024)>>12 = 0x80000` 修复 PPN 路径 — 仍然 5/5 FAIL
3. **尝试 3**: 加 DEBUG 探针但破坏 macro 结构 — 回滚

**Root cause 假设** (待 Oracle / deep debug 验证):
- `MMUPlugin::do_lookup` (ip/mmu/tlm/MMUPlugin.cpp:100) Bare shortcut 条件 `satp_ppn_ == 0` 仍存在 (v0.10.4 hotfix ad48fcf 仅引入 `satp_value_` 字段注释未修复判定逻辑)
- 但 satp_ppn=0x80000 ≠ 0,Bare shortcut 不应触发,理论走 PTW walk
- 实测 5 ELF 仍 10000 cycles 卡住,意味 fetch 阶段未拿到有效指令
- 推测: `cfg.satp_ppn` 透传到 RiscvMMUPlugin 后,`RiscvMMUPlugin::csr_write_satp()` (ip/cpu/plugins/mmu.cpp:34) 未被调用 (production CPU 不写 satp CSR 时,csr_write_satp 不会被触发,satp_value 仅 ctor 注入一次)
- 进一步: `set_satp_value` shadowed ctor (mmu.h:100) 派生类覆盖同名方法,可能 ctor 注入值被后续 setup/build 阶段 reset 覆盖

**owner 真空**:
- `ip/cpu/cpu_factory_chmem.h:80-85` 源码注释自证 *"sv32 translation 实现 owner 尚未分配"*
- `tools/verilator_runner/cpu_verilator_sim.cpp:137-138` 自证 *"--enable-mmu is a no-op log; real sv32 translation is not delivered by this runner"*
- 当前状态 = CH_MEM MMU 集成层 scope **无 change 持有**

**失败教训**:
- 决策 #2 "接受 Part a/b 拆分" 假设 1-4h 可 ship 是 v0.10.0 window 的工程估算
- 实际工作=证实 MMU BFTA 真翻译路径需要更深层 debug (估计 1-3 周,类似 `mmu_chmem.h` 实装估时)
- 与决策 #3 一致:**mmu_chmem.h 实装不应塞进 mmufault-verilator-sv32-e2e-flip scope** (D4 隔离)
- 但 mmufault Part a 实装也**间接受阻**于同根因 (TLM 真翻译路径需要 MMU plugin 内部 satp_value 初始化机制修订)

**v0.11.0 启动决策**:
- 本 change 与 `wave5-bp-btb` (v0.11.0) 同窗口规划
- 共享 fetch stage (避免 fetch path ownership 冲突)
- 实施前应 Oracle 决议 root cause (MMUPlugin Bare shortcut + csr_write_satp 注入时机)

## What Changes

### 1. CH_MEM MMU/PTW pipeline 集成层实装

**位置**: `ip/cpu/cpu_factory_chmem.h` (修改) + `ip/cpu/plugins/mmu_ptw_chmem.h` (修改) + `ip/cpu/plugins/ibus_chmem.h` (修改) + `ip/cpu/plugins/mmu_exception_handler_chmem.h` (修改)

实装 CH_MEM 模式 fetch/memory 路径:
- IBus CH_MEM 接入 sv32 翻译结果
- DBus CH_MEM 接入 sv32 翻译结果
- MmuExceptionHandler CH_MEM 消费 EXCEPTION_CODE → trap PC 跳转
- HazardPlugin CH_MEM 接收 mmufault_clear 信号

### 2. MMUPlugin Bare shortcut 严格化

**位置**: `ip/mmu/tlm/MMUPlugin.cpp:100` + `ip/mmu/tlm/MMUPlugin_chmem.h` (新增)

`do_lookup` Bare shortcut 判定改为:
- 仅 `sv_mode_ == Bare || (satp_value_ & MODE_mask) == 0` 触发 Bare shortcut
- 移除 `satp_ppn_ == 0` 条件 (v0.10.4 hotfix ad48fcf 引入的字段注释已指出此 bug)

### 3. RiscvMMUPlugin::csr_write_satp 注入时机审查

**位置**: `ip/cpu/plugins/mmu.cpp:34`

确保 cpu_factory ctor 传入 satp_value 后,即使无 csr_write_satp 调用,基类 satp_value_ 仍保持 ctor 注入值 (可能涉及派生 set_satp_value shadow 修复)。

### 4. Verilator TEST_CASE 6 mmu_sv32_translation_verilator_e2e_flipped

**位置**: `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` (修改)

承接 `mmufault-verilator-sv32-e2e-flip` Part b (原 tasks.md §4):
- sv32 PTE ELF vendor (build_sv32_pte.S + register build_sv32_pte in build_manual_elf.sh)
- sv32 baseline CSV (5 ELF × median cycle × mode=sv32_mmu)
- popen cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf sv32_pte.elf --cycles 5000
- REQUIRE TOHOST=1 + cycle ≤ sv32 baseline × 1.5 + trap PC 跳转断言

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
- AGENTS.md `[cpu-l1-mmu-demo]` 6/6 → 7/7 PASS
- CHANGELOG.md v0.11.0 段新增条目 `mmu-chmem-pipeline-integration`
- `openspec archive mmu-chmem-pipeline-integration -y`

## Tasks (placeholder, 实际编码留给 v0.11.0 启动期)

> **0/N**: tasks.md 实际列表留待 v0.11.0 启动期由 owner 起草
> **当前占位**: 仅声明 change intent + depends_on + scope 边界
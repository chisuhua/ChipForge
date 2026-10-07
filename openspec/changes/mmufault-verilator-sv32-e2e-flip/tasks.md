# Tasks — mmufault-verilator-sv32-e2e-flip

> **Status**: placeholder (等 v1 `cpu-pipeline-mmufault-handler` archive + Phase 6d.6 `mmu_chmem.h` 实装后启动)

## 1. 启动前置 (Spike)

- [ ] 1.1 验证 `cpu-pipeline-mmufault-handler` v1 已 archive (依赖 v1 TLM/CH_MEM exception routing)
- [ ] 1.2 验证 Phase 6d.6 (`phase-6d.6-mmu-ptw-fsm`) 已 archive (依赖 `mmu_chmem.h` 真翻译实现)
- [ ] 1.3 验证 `tools/verilator_runner/cpu_verilator_sim.cpp:96-101` 注释已更新（不再是 no-op stub）
- [ ] 1.4 验证 sv32 翻译语义端到端可执行（手动 popen cpu_verilator_sim --elf sv32_pte.elf --enable-mmu --mmu-mode sv32 --cycles 5000, 期望 TOHOST=1 + cycle 合理）

## 2. sv32 PTE 测试程序 vendor

- [ ] 2.1 新建 `tests/cpu/manual_elf/build_sv32_pte.S`: 最小 sv32 程序，含 `csrw satp, ...` + L0/L1 PTE 建立 + trap handler @ 0x80000010 + tohost write（参考 `build_mmu_bare.S` 模板）
- [ ] 2.2 修改 `tests/cpu/manual_elf/build_manual_elf.sh` 注册 `build_sv32_pte()` 函数 + 默认 build 列表追加 `sv32_pte`
- [ ] 2.3 跑 `bash tests/cpu/manual_elf/build_manual_elf.sh sv32_pte` 生成 `sv32_pte.elf`
- [ ] 2.4 验证 `sv32_pte.elf` tohost=1（手动 popen cpu_sim 或跑现有 `[mmu]` 测试用例）

## 3. sv32 baseline CSV 生成

- [ ] 3.1 跑 `cpu_verilator_sim --elf <5 ELF> --enable-mmu --mmu-mode sv32 --cycles 5000` × 5 runs × median cycle, 追加到 `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv` (mode=`sv32_mmu`)
- [ ] 3.2 验证 baseline CSV schema 合规 (4 列, header 存在, 5 行 sv32_mmu + 5 行 bare_mmu 共 10 行)
- [ ] 3.3 验证 median_cycles 在合理范围（sv32_mmu cycle ≥ bare_mmu cycle，因 PTW walk 开销）

## 4. Verilator TEST_CASE 6 实装

- [ ] 4.1 修改 `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` 追加 TEST_CASE 6 `mmu_sv32_translation_verilator_e2e_flipped` (family tags: `[mmu-verilator][e2e][sv32]`)
- [ ] 4.2 body 实现: popen `cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf tests/cpu/manual_elf/sv32_pte.elf --cycles 5000`, REQUIRE TOHOST=1 + cycle ≤ sv32 baseline × 1.5 + trap PC 跳转断言 (via CPU_EXCEPTION_CODE payload read)
- [ ] 4.3 注释块顶部加 `承接 Change 2a follow-up — 真 sv32 translation 翻转` 标记 + 引用 "v0.10.4 hotfix class"
- [ ] 4.4 跑 TEST_CASE 6 (5 次), 验证 PASS rate ≥ 3/3（若 2/3, 触发 design.md §R4 mitigation: cap 放宽至 × 2.0）
- [ ] 4.5 跑 max_cycles=5000 验证 hazard retry 循环清理（cycle ≤ 5000）

## 5. [cpu-l1-mmu-demo] 真 sv32 翻转

> **2026-10-07 BLOCKED (Sisyphus bootstrap session 实装 3 次失败)**:
> - 尝试 1: `cfg.enable_mmu=true` + `plant_identity_page_table` — 5/5 ELF 10000 cycles 卡住, exit_code=-1
> - 尝试 2: 加 `cfg.satp_ppn = (window_base+60*1024)>>12 = 0x80000` — 仍然 5/5 FAIL
> - 尝试 3: 加 DEBUG 探针但破坏 macro 结构 — 回滚到 workaround 状态
>
> **Root cause 假设** (待 Oracle / deep debug 验证):
> - `MMUPlugin::do_lookup` (ip/mmu/tlm/MMUPlugin.cpp:100) Bare shortcut 条件 `satp_ppn_ == 0` 仍存在 (v0.10.4 hotfix ad48fcf 仅引入 `satp_value_` 字段注释未修复判定逻辑)
> - `RiscvMMUPlugin::csr_write_satp()` (ip/cpu/plugins/mmu.cpp:34) 未被调用时,派生 set_satp_value shadow 可能覆盖 ctor 注入
> - 真 sv32 TLM 翻译路径需要更深层 MMU plugin 修订 (1-3 周, 超出 mmufault Part a scope)
>
> **Follow-up owner**: `mmu-chmem-pipeline-integration` change (v0.11.0, decision 2026-10-07) 显式认领 owner 真空,本节 tasks §5.1-§5.4 整体移交.

- [ ] ~~5.1 修改 `tests/soc/test_cpu_l1_mmu_demo.cpp:113` flip `cfg.enable_mmu=false → true`（移除 workaround）~~ → **DEFERRED to `mmu-chmem-pipeline-integration` §5 (v0.11.0)**
- [ ] ~~5.2 新增 TEST_CASE 7 `cpu_l1_mmu_demo_sv32_translation_flipped` ([soc][cpu-l1-mmu-demo][sv32] family tags): 用 sv32_pte.elf 替换 add ELF, REQUIRE tohost=1~~ → **DEFERRED to `mmu-chmem-pipeline-integration` §5 (v0.11.0)**
- [ ] ~~5.3 跑 `[cpu-l1-mmu-demo]` 7/7 PASS（6 旧 + 1 新）~~ → **DEFERRED to `mmu-chmem-pipeline-integration` §5 (v0.11.0)**
- [ ] ~~5.4 修改 AGENTS.md `[cpu-l1-mmu-demo]` workaround line 40 → 移除 workaround 标记, 加 `[v0.10.x follow-up]` 标记~~ → **DEFERRED to `mmu-chmem-pipeline-integration` §6 (v0.11.0)**

## 6. Spec Delta 编辑

- [ ] 6.1 修改 `openspec/specs/verilator-mmu-bare-plumbing/spec.md`: 在 `## ADDED Requirements` section 新增 Requirement "TEST_CASE 6 mmu_sv32_translation_verilator_e2e_flipped"
- [ ] 6.2 修改 `openspec/specs/cpu-l1-mmu-demo/spec.md`: 新增 Requirement "TEST_CASE 7 真 sv32 translation 翻转" + 移除 workaround 标注
- [ ] 6.3 跑 `openspec change validate mmufault-verilator-sv32-e2e-flip` 校验 spec delta 0 错误

## 7. Architecture Gate Final

- [ ] 7.1 跑 `bash tools/verify_adr.sh` 必须 0 失败
- [ ] 7.2 跑 `bash tools/verify_plugin_decision.sh` 必须 0 失败
- [ ] 7.3 跑 `bash tools/check_plugin_portability.sh` 必须 0 失败
- [ ] 7.4 跑 `bash tools/doc_link_check.sh` 必须 0 失败

## 8. Regression Final Check

- [ ] 8.1 跑 `bash tools/run_chipforge_tests.sh` 全部测试: 期望 0 regression（`[mmu-verilator] 5/5` + `[mmu] 53/53` + `[cpu-integration] 81/81` + `[cpu-l1-mmu-demo] 7/7` + `[cpu] 19/19` + `[verilator] 1/1`）
- [ ] 8.2 `git diff main --stat` 检查 diff < 400 LOC（设计目标 vendor ~50 + test ~150 + CSV ~50 + doc ~50 + spec delta ~100 LOC）

## 9. Archive Preparation

- [ ] 9.1 确认所有 AC checkbox 勾选完毕
- [ ] 9.2 跑 `openspec change validate mmufault-verilator-sv32-e2e-flip` 校验 0 error
- [ ] 9.3 修改 AGENTS.md `[mmu-verilator]` 3/3 → 5/5 PASS, `[cpu-l1-mmu-demo]` 6/6 → 7/7 PASS, 移除 workaround 标记
- [ ] 9.4 修改 CHANGELOG.md v0.10.x 段新增条目 `mmufault-verilator-sv32-e2e-flip`
- [ ] 9.5 提交 commit + 跑 `openspec archive mmufault-verilator-sv32-e2e-flip -y` 归档

## 10. Downstream Handoff

- [ ] 10.1 通知 wave5 mfc-... owner: 本 change archive 后, Phase G "DMIPS/MHz ≥1.4" 硬门禁可消费真 sv32 翻译路径
- [ ] 10.2 通知 wave6 readiness: wave6+ linux-and-productization 真 OS 启动测试需要 sv32 translation, 本 change 提供完整链路

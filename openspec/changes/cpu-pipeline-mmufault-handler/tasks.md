## 1. Pre-flight (Spike)

- [ ] 1.1 **依赖校验**: `openspec change validate verilator-mmu-bare-plumbing-e2e` 必须 PASS（Change 2a archive）。
- [ ] 1.2 **Spike 1.2**: 复现 hazard retry 循环 — 跑 `cpu_verilator_sim --elf <sv32_test>.elf --enable-mmu --mmu-mode sv32`, 预期卡死 / SEGV / cycle > 2000 不退出（记录当前 CPU pipeline 行为）。
- [ ] 1.3 **CPU pipeline current state 检查**: 检查 `ip/cpu/plugins/` 确认无 `mmu_exception_handler.h`（现状）。
- [ ] 1.4 **Payload Key 现状**: 检查 `ip/cpu/core/payload_common.h` 确认 `cpu_keys::CPU_EXCEPTION_CODE` Payload Key 已声明（消费方无）。

## 2. sv32 PTE 程序 vendor (TDD Step 0 - Pre-red)

- [ ] 2.1 新建 `tests/cpu/manual_elf/build_sv32_pte.S`: 最小 sv32 程序, 含 `csrw satp, ...` + L0/L1 PTE 建立 + trap handler @ 0x80000010 + tohost write。
- [ ] 2.2 修改 `tests/cpu/manual_elf/build_manual_elf.sh` 注册 `sv32_pte` 模板入口。
- [ ] 2.3 跑 `bash tests/cpu/manual_elf/build_manual_elf.sh sv32_pte` 生成 `sv32_pte.elf`。

## 3. cycle Baseline 表生成 (TDD Step 1)

- [ ] 3.1 跑脚本生成 sv32_pte.elf × 5 runs × median cycle, 追加到 Change 2a `test_mmu_bare_plumbing_verilator_baselines.csv` (mode=`sv32_mmu`)。
- [ ] 3.2 验证 baseline CSV schema 合规 (4 列, header 存在)。

## 4. HazardPlugin clear_mmufault() API 实装 (TDD Step 2 - Red)

- [ ] 4.1 修改 `ip/cpu/plugins/hazard.h`: 新增 `void clear_mmufault()` public method, 重置内部 hazard retry counter。
- [ ] 4.2 修改 `ip/cpu/plugins/hazard_chmem.h`: CH_MEM 版同步实装（用 ch_reg / ch_state_machine DSL）。
- [ ] 4.3 ABI smoke test: 跑 `tests/cpu/test_hazard.cpp` 验证现有 hazard 测试不退化。
- [ ] 4.4 跑 `bash tools/verify_plugin_decision.sh` 0 失败（D4 无状态机禁令 + at_stage 闭包模式）。

## 5. cpu_factory.h:390 修复 (TDD Step 3)

- [ ] 5.1 修改 `ip/cpu/cpu_factory.h:390`: 删除硬写 `/*satp_value=*/0`, 替换为 `cpu_config.satp_value` (per `cpu-factory-satp-mapping` helpers)。
- [ ] 5.2 ABI smoke test: 重 build 5 个 TLM 工厂调用方 (`tools/cpu_sim` / `test_cpu_factory` / `test_3stage_riscv` / `test_5stage_riscv` / `test_10stage_riscv`), 验证 0 error 0 warning。
- [ ] 5.3 跑 `bash tools/run_chipforge_tests.sh --tag "[cpu]"` 验证 19/19 不退化。
- [ ] 5.4 跑 `bash tools/run_chipforge_tests.sh --tag "[cpu-integration]"` 验证 81/81 不退化（硬不退化约束）。

## 6. MmuExceptionHandlerPlugin TLM 实装 (TDD Step 4)

- [ ] 6.1 新建 `ip/cpu/plugins/mmu_exception_handler.h`: `MmuExceptionHandlerPlugin<T>` 类, 在 `at_stage("memory", Phase::LATE, ...)` 闭包消费 `mmu_keys::EXCEPTION_CODE`, 触发 CtrlLink flush_when + HazardPlugin::clear_mmufault()。
- [ ] 6.2 跑 `bash tools/verify_plugin_decision.sh` 0 失败（D4 无 enum class State 禁令）。
- [ ] 6.3 跑 `bash tools/check_plugin_portability.sh` 0 失败（ADR-040 Tier-1 4 项）。

## 7. MmuExceptionHandlerPlugin CH_MEM 实装 (TDD Step 5)

- [ ] 7.1 新建 `ip/cpu/plugins/mmu_exception_handler_chmem.h`: CH_MEM 版, 用 ch_bool / ch_reg / ch_state_machine DSL（仅 trap PC 跳转走 FSM, 符合 ADR-046 v2.0 豁免范围）。
- [ ] 7.2 跑 `bash tools/check_plugin_portability.sh` 0 失败（特别注意 ip/cpu/plugins/ CH_MEM 文件用 ch_* 渗透是允许的，但 TLM 文件 mmh.h 无 ch_* 渗透）。
- [ ] 7.3 跑 `bash tools/run_chipforge_tests.sh --chmem`（如有）验证 CH_MEM 测试通过。

## 8. cpu_factory_chmem.h::build_cpu 联动 (TDD Step 6)

- [ ] 8.1 修改 `ip/cpu/cpu_factory_chmem.h` 在 `if (enable_mmu.value_or(false) && mmu_mode.value_or("bare") != "bare")` 分支内追加 `MmuExceptionHandlerPlugin<T>` 注册。
- [ ] 8.2 ABI smoke test: 4 个调用方（cpu_verilator_sim + test_cpu_chmem_vendored_elf + test_cpu_5stage + test_cpu_decoded_inst_migration）零修改通过 build。
- [ ] 8.3 跑 `bash tools/run_chipforge_tests.sh --tag "[verilator]"` 验证 1/1 不退化。

## 9. TEST_CASE 6 实装 (TDD Step 7)

- [ ] 9.1 修改 `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` 追加 TEST_CASE 6 `mmu_sv32_translation_verilator_e2e_flipped` (family tags: `[mmu-verilator][e2e][sv32]`)。
- [ ] 9.2 body 实现: popen `cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf tests/cpu/manual_elf/sv32_pte.elf --cycles 5000`, REQUIRE TOHOST=1 + cycle ≤ baseline × 1.5 + trap PC 跳转断言 (via CPU_EXCEPTION_CODE payload read)。
- [ ] 9.3 注释块顶部加 `承接 Change 2a follow-up — 真 sv32 translation 翻转` 标记 + 引用 "v0.10.4 hotfix class"。
- [ ] 9.4 跑 TEST_CASE 6 (5 次), 验证 PASS rate ≥ 3/3（若 2/3, 触发 design.md §R4 mitigation: cap 放宽至 × 2.0）。
- [ ] 9.5 跑 max_cycles=5000 验证 hazard retry 循环清理（cycle ≤ 5000）。

## 10. Spec Delta 编辑 (TDD Step 8)

- [ ] 10.1 修改 `openspec/specs/mmu-cache-integration-test/spec.md`: 在 `## ADDED Requirements` section 新增 Requirement "MMU exception propagation to CPU pipeline"。
- [ ] 10.2 修改 `openspec/specs/cpu-mmu-exception-routing/spec.md`: 从 TBD 占位升级为具体 spec, 记录 CPU_EXCEPTION_CODE Payload Key 写入路径。
- [ ] 10.3 修改 `openspec/specs/verilator-mmu-bare-plumbing/spec.md` (Change 2a spec): 在 `## ADDED Requirements` section 新增 Requirement "TEST_CASE 6 mmu_sv32_translation_verilator_e2e_flipped" (承接 follow-up)。
- [ ] 10.4 跑 `openspec validate --changes` 校验 spec delta 0 错误。

## 11. Documentation Sync (TDD Step 9)

- [ ] 11.1 修改 `AGENTS.md` "[mmu-verilator]" 行: 3/3 → 4/4 PASS, 并新增 "**[cpu-pipeline-mmufault-handler]** `1 change` (v0.10.x 真 sv32 Verilator e2e 翻转 owner)" 行。
- [ ] 11.2 修改 `CHANGELOG.md` v0.10.x 段新增条目, 标题 `cpu-pipeline-mmufault-handler`, body 列 5 项 (a-e, 见 spec.md)。
- [ ] 11.3 跑 `bash tools/v0100-bootstrap.sh review` 输出 §honesty_audit 段, 确认 `[mmu-verilator] 4/4 PASS` + 其他 baseline 数字不变。

## 12. Architecture Gate Final

- [ ] 12.1 跑 `bash tools/verify_adr.sh` 必须 0 失败（特别注意 ADR-040 v2.0 + ADR-045 + ADR-046 v2.0 + ADR-047）。
- [ ] 12.2 跑 `bash tools/verify_plugin_decision.sh` 必须 0 失败（MmuExceptionHandlerPlugin 无 enum class State + at_stage 闭包）。
- [ ] 12.3 跑 `bash tools/check_plugin_portability.sh` 必须 0 失败（特别注意 ip/cpu/plugins/mmu_exception_handler.h 无 ch_* 渗透, ip/cpu/plugins/mmu_exception_handler_chmem.h 有 ch_* 渗透是允许的）。
- [ ] 12.4 跑 `bash tools/doc_link_check.sh` 必须 0 失败。

## 13. Regression Final Check

- [ ] 13.1 跑 `bash tools/run_chipforge_tests.sh` 全部测试: 期望 0 regression（`[mmu-verilator]` 4/4 PASS, `[mmu]` 53/53, `[cpu-integration]` 81/81（硬不退化）, `[cpu-l1-mmu-demo]` 6/6, `[cpu]` 19/19, `[verilator]` 1/1）。
- [ ] 13.2 跑 `bash tools/run_chipforge_tests.sh --chmem`（如有）: 验证 CH_MEM 二进制 build 0 error, CH_MEM 测试通过。
- [ ] 13.3 `git diff main --stat` 检查 diff < 700 LOC（设计目标 ~260 LOC plugin + 30 LOC factory 修复 + 80 LOC test + 40 LOC sv32_pte.S + 200 LOC 文档 + 100 LOC spec delta），无 scope 失控。

## 14. Archive Preparation

- [ ] 14.1 确认所有 AC checkbox 勾选完毕（proposal.md + spec.md）。
- [ ] 14.2 跑 `openspec change validate cpu-pipeline-mmufault-handler` 校验 0 error。
- [ ] 14.3 跑 `bash tools/v0100-bootstrap.sh review` 最终 §honesty_audit 数字确认（最后一次 sync）。
- [ ] 14.4 提交 commit（按 CONTRIBUTING.md commit 规范, CHANGELOG + AGENTS.md + plugin 双文件 + factory 修复 + test + sv32_pte.S + spec delta 同时关注）。
- [ ] 14.5 跑 `openspec archive cpu-pipeline-mmufault-handler -y` 归档（先移除 frontmatter `status: placeholder`）。

## 15. Downstream Handoff (Post-Archive)

- [ ] 15.1 **通知 wave5 mfc-... owner**: 本 change archive 后, Phase G "DMIPS/MHz ≥1.4 (CH_MEM + Verilator 实测)" 硬门禁可消费 sv32 路径（真翻译不再是 plumbing-only, 是完整 sv32 translation）。
- [ ] 15.2 **通知 wave6 readiness**: wave6+ (linux-and-productization) 的真 OS 启动测试需要 sv32 translation, 本 change 提供 owner 链路。
- [ ] 15.3 **回顾 archive quality**: 在 `docs/lessons/` 加 `phase-6d-cpu-pipeline-mmufault.md` (Lessons Learned), 记录 "D4 + ADR-046 v2.0 双轨" 实装模式（at_stage 闭包主逻辑 + ch_state_machine DSL 仅 FSM 状态机交互）。
- [ ] 15.4 **ADR-082 跟进**: 本 change 隐式声明了 HazardPlugin::clear_mmufault() capability, ADR-082 (Drafting) v2 升级时需补全 `negotiate()` 接口声明 mmufault capability（避免 future refactor 误删 HazardPlugin::clear_mmufault()）。
- [ ] 15.5 **真 sv32 e2e cycle baseline 长期维护**: `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv` sv32_mmu mode 行在 future Phase 6d 优化（如 radix-4 SRT DIV）后需重新生成 baseline（ARCHITECTURAL CHANGE 标签）。
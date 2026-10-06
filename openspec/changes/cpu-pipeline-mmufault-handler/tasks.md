# Tasks — cpu-pipeline-mmufault-handler (v1, scope 拆分 2026-10-06)

> **Scope 拆分 (2026-10-06, Oracle 决策)**: 本 v1 change **移除原 Phase 5 (cpu_factory.h:390 修复)** + **原 Phase 9 (TEST_CASE 6 真 sv32 e2e 翻转)**。
>
> **理由**:
> 1. **Phase 5 已过时**: `ip/cpu/cpu_factory.h` satp_value=0 硬写**已由 v0.10.2 archive change `cpu-factory-satp-mapping` 修复** (`make_satp_value()` helper at lines 78-106 + 替换硬写 at lines 440-447). tasks.md 原始行号 390 实际是 `pb.build()` 注释, satp_value 问题行号误标. 5.3 `[cpu]` 19/19 + 5.4 `[cpu-integration]` 81/81 硬不退化约束在 v0.10.2 archive 时已验证.
> 2. **Phase 9 强依赖 Phase 6d.6**: `tools/verilator_runner/cpu_verilator_sim.cpp:96-101` 明确 `--enable-mmu --mmu-mode sv32` 是 **no-op stub** (mmu_chmem.h not implemented). 当前无 `mmu_chmem.h`, 即使本 change archive 后, TEST_CASE 6 端到端验证也无法通过 (会卡在 plumbing-only 路径).
> 3. **scope gate < 700 LOC**: 原 99 任务 / 15 阶段 / 估算 ~700 LOC 边界已接近上限, 移除 Phase 5+9 后 ~470 LOC, 安全余量充足.
>
> **v2 follow-up**: `mmufault-verilator-sv32-e2e-flip` change 占位, 等 Phase 6d.6 (`mmu_chmem.h`) 实装后启动, 承接原 Phase 2 (sv32_pte.S vendor) + Phase 3 (sv32 baseline CSV) + Phase 9 (TEST_CASE 6).

## 1. Pre-flight (Spike)

- [ ] 1.1 **依赖校验**: `openspec change validate verilator-mmu-bare-plumbing-e2e` 必须 PASS（Change 2a archive）。
- [ ] 1.2 **CPU pipeline current state 检查**: 检查 `ip/cpu/plugins/` 确认无 `mmu_exception_handler.h`（现状；已确认 17 个 plugin 文件）。
- [ ] 1.3 **Payload Key 现状**: 检查 `ip/cpu/tlm/cpu_keys.h` 确认 `cpu_keys::CPU_EXCEPTION_CODE` Payload Key 已声明（已确认 line 48，消费方无）。
- [ ] 1.4 **MMU 写入路径确认**: 检查 `ip/mmu/tlm/MMUPlugin.cpp:139-141` PTW fault 写入 `mmu_keys::EXCEPTION_CODE` 路径（已确认）+ `ip/cpu/plugins/mmu.cpp:126-134` mmu_exit 闭包传播到 `cpu_keys::CPU_EXCEPTION_CODE` 路径（已确认）。

## 2. HazardPlugin clear_mmufault() API 实装 (TLM + CH_MEM)

- [ ] 2.1 修改 `ip/cpu/plugins/hazard.h`: 新增 `void clear_mmufault()` public method（在 `clear_in_flight()` 后 ~line 150），重置内部 hazard retry counter + MMU fault 标志字段（新增 `bool mmufault_pending_ = false` 成员）。
- [ ] 2.2 修改 `ip/cpu/plugins/hazard_chmem.h`: CH_MEM 版同步实装 —— **决策**: 用 `ch_state_machine` DSL（per ADR-046 v2.0 FSM 豁免），新增 `mmufault_clear` 输入信号（ch_bool），由 MmuExceptionHandlerPlugin 在 trap PC 跳转时拉高。设计备注：纯函数 static API 不变，新增 `ch_reg<bool> mmufault_pending_` 成员字段 + `setup()` 内 reset。
- [ ] 2.3 ABI smoke test: 跑 `tests/cpu/test_hazard.cpp` 验证现有 hazard 测试不退化（已有 89/89 PASS 不退化约束）。
- [ ] 2.4 跑 `bash tools/verify_plugin_decision.sh` 0 失败（D4 无状态机禁令 + at_stage 闭包模式 — 注意 CH_MEM 版 `ch_state_machine` 使用属 ADR-046 v2.0 豁免范围）。

## 3. MmuExceptionHandlerPlugin TLM 实装

- [ ] 3.1 新建 `ip/cpu/plugins/mmu_exception_handler.h`: `MmuExceptionHandlerPlugin<T>` 类，在 `at_stage("memory", Phase::LATE, ...)` 闭包消费 `cpu_keys::CPU_EXCEPTION_CODE`（不是 `mmu_keys::EXCEPTION_CODE`，因为 mmu_exit 已传播到 CPU_EXCEPTION_CODE），触发 CtrlLink `flush_when(mmu_exception_pending)` + HazardPlugin::clear_mmufault()。
- [ ] 3.2 跑 `bash tools/verify_plugin_decision.sh` 0 失败（D4 无 enum class State 禁令 — 注意 trap PC 跳转逻辑使用 ch_state_machine 是 FSM DSL 例外，但 TLM 文件禁用 ch_* 渗透）。
- [ ] 3.3 跑 `bash tools/check_plugin_portability.sh` 0 失败（ADR-040 Tier-1 4 项：早返 / ch_mem 渗透 / pb.run / array_store）。
- [ ] 3.4 单元测试: 新建 `tests/cpu/test_mmu_exception_handler.cpp` ~80 LOC，测试 4 个 case: (a) EXCEPTION_CODE=12 page fault → flush 触发；(b) EXCEPTION_CODE=0 (no fault) → no flush；(c) CtrlLink flush_when 与 HazardPlugin::clear_mmufault() 协同；(d) ABI smoke (reset 后可重复使用)。

## 4. MmuExceptionHandlerPlugin CH_MEM 实装

- [ ] 4.1 新建 `ip/cpu/plugins/mmu_exception_handler_chmem.h`: CH_MEM 版，用 ch_bool / ch_reg / ch_state_machine DSL（仅 trap PC 跳转走 FSM，符合 ADR-046 v2.0 豁免范围）。设计备注：消费 `cpu_keys::CPU_EXCEPTION_CODE` (ch_uint<8>) + 输出 `mmufault_clear` (ch_bool) 到 HazardPlugin。
- [ ] 4.2 跑 `bash tools/check_plugin_portability.sh` 0 失败（特别注意 ip/cpu/plugins/ CH_MEM 文件用 ch_* 渗透是允许的，但 TLM 文件 mmu_exception_handler.h 无 ch_* 渗透）。
- [ ] 4.3 跑 `bash tools/run_chipforge_tests.sh --chmem`（如有）验证 CH_MEM 测试通过（`[chmem]` 9/9 PASS 不退化约束）。

## 5. cpu_factory_chmem.h::build_cpu 联动

- [ ] 5.1 修改 `ip/cpu/cpu_factory_chmem.h` 在 `if (enable_mmu.value_or(false) && mmu_mode.value_or("bare") != "bare")` 分支内追加 `MmuExceptionHandlerPlugin<T>` 注册（与现有 MMUPlugin 注册逻辑绑定）。
- [ ] 5.2 ABI smoke test: 4 个调用方（`cpu_verilator_sim` + `test_cpu_chmem_vendored_elf` + `test_cpu_5stage` + `test_cpu_decoded_inst_migration`）零修改通过 build。
- [ ] 5.3 跑 `bash tools/run_chipforge_tests.sh --tag "[verilator]"` 验证 1/1 不退化（`[verilator]` family 仍维持 plumbing-only 模式 — 真 sv32 e2e 翻转推迟到 v2 follow-up）。

## 6. cpu_factory.h (TLM) 联动验证

- [ ] 6.1 验证 `ip/cpu/cpu_factory.h:440-447` 当前 `make_satp_value()` 调用路径仍正确（v0.10.2 archive 时已修，本 change 仅消费不动）。
- [ ] 6.2 跑 `bash tools/run_chipforge_tests.sh --tag "[cpu]"` 验证 19/19 不退化。
- [ ] 6.3 跑 `bash tools/run_chipforge_tests.sh --tag "[cpu-integration]"` 验证 81/81 不退化（**硬不退化约束**）。

## 7. Spec Delta 编辑

- [ ] 7.1 修改 `openspec/specs/mmu-cache-integration-test/spec.md`: 在 `## ADDED Requirements` section 新增 Requirement "MMU exception propagation to CPU pipeline"。
- [ ] 7.2 修改 `openspec/specs/cpu-mmu-exception-routing/spec.md`: 从 TBD 占位升级为具体 spec，记录 `cpu_keys::CPU_EXCEPTION_CODE` Payload Key 写入路径（`mmu_exit` 闭包 `ip/cpu/plugins/mmu.cpp:128-130`）。
- [ ] 7.3 跑 `openspec change validate cpu-pipeline-mmufault-handler` 校验 spec delta 0 错误。

## 8. Documentation Sync

- [ ] 8.1 修改 `AGENTS.md` "[cpu]" / "[cpu-integration]" 行: **保持现有数字不变**（v0.10.2 hotfix 已固化），仅在 `[cpu-pipeline-mmufault-handler]` 行加 `1 change (v0.10.x wave5 P1 owner)`。
- [ ] 8.2 修改 `CHANGELOG.md` v0.10.x 段新增条目，标题 `cpu-pipeline-mmufault-handler`，body 列 4 项 (a-d, 见 spec.md)。
- [ ] 8.3 跑 `bash tools/v0100-bootstrap.sh review` 输出 §honesty_audit 段，确认 baseline 数字不变 + 新增 v2 follow-up change 占位条目。

## 9. Architecture Gate Final

- [ ] 9.1 跑 `bash tools/verify_adr.sh` 必须 0 失败（特别注意 ADR-040 v2.0 + ADR-045 + ADR-046 v2.0 + ADR-047）。
- [ ] 9.2 跑 `bash tools/verify_plugin_decision.sh` 必须 0 失败（MmuExceptionHandlerPlugin 无 enum class State + at_stage 闭包）。
- [ ] 9.3 跑 `bash tools/check_plugin_portability.sh` 必须 0 失败（特别注意 `ip/cpu/plugins/mmu_exception_handler.h` 无 ch_* 渗透, `ip/cpu/plugins/mmu_exception_handler_chmem.h` 有 ch_* 渗透是允许的）。
- [ ] 9.4 跑 `bash tools/doc_link_check.sh` 必须 0 失败。

## 10. Regression Final Check

- [ ] 10.1 跑 `bash tools/run_chipforge_tests.sh` 全部测试: 期望 0 regression（`[cpu-l1-mmu-demo]` 6/6, `[cpu]` 19/19, `[cpu-integration]` 81/81, `[mmu]` 53/53, `[verilator]` 1/1, `[mmu-verilator]` 3/3 — 仍 plumbing-only）。
- [ ] 10.2 跑 `bash tools/run_chipforge_tests.sh --chmem`（如有）: 验证 CH_MEM 二进制 build 0 error, CH_MEM 测试通过（`[chmem]` 9/9）。
- [ ] 10.3 `git diff main --stat` 检查 diff < 500 LOC（设计目标 ~260 LOC plugin 双文件 + 80 LOC test + 100 LOC spec delta + 60 LOC doc），无 scope 失控。

## 11. Archive Preparation

- [ ] 11.1 确认所有 AC checkbox 勾选完毕（proposal.md + spec.md）。
- [ ] 11.2 跑 `openspec change validate cpu-pipeline-mmufault-handler` 校验 0 error。
- [ ] 11.3 跑 `bash tools/v0100-bootstrap.sh review` 最终 §honesty_audit 数字确认（最后一次 sync）。
- [ ] 11.4 提交 commit（按 CONTRIBUTING.md commit 规范, CHANGELOG + AGENTS.md + plugin 双文件 + factory 联动 + test + spec delta 同时关注）。
- [ ] 11.5 跑 `openspec archive cpu-pipeline-mmufault-handler -y` 归档（先移除 frontmatter `status: placeholder`）。

## 12. Downstream Handoff (Post-Archive)

- [ ] 12.1 **通知 wave5 mfc-... owner**: 本 change archive 后，TLM + CH_MEM 路径有 MmuExceptionHandlerPlugin 注册，但**真 sv32 e2e 翻转仍依赖 v2 follow-up**。mfc Phase G "DMIPS/MHz ≥1.4" 硬门禁消费本 change 的 TLM exception routing 修复，sv32 翻转消费 v2 follow-up。
- [ ] 12.2 **通知 v2 follow-up owner**: `mmufault-verilator-sv32-e2e-flip` change 启动条件 = Phase 6d.6 (`mmu_chmem.h`) 实装（per `soc/cpu/docs/roadmap/execution-roadmap.md §3.5.2` Phase 6d.6 P1 priority）。
- [ ] 12.3 **回顾 archive quality**: 在 `docs/lessons/` 加 `phase-6d-cpu-pipeline-mmufault-v1.md` (Lessons Learned), 记录 "D4 + ADR-046 v2.0 双轨" 实装模式（at_stage 闭包主逻辑 + ch_state_machine DSL 仅 FSM 状态机交互）。
- [ ] 12.4 **ADR-082 跟进**: 本 change 隐式声明了 HazardPlugin::clear_mmufault() capability, ADR-082 (Drafting) v2 升级时需补全 `negotiate()` 接口声明 mmufault capability（避免 future refactor 误删 HazardPlugin::clear_mmufault()）。
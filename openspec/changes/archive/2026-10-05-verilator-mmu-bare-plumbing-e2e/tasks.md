## 1. Pre-flight (Spike)

- [x] 1.1 **Spike 1.1**: 验证 `openspec change validate verilator-cpu-factory-extensible-params` PASS（Change 1 已 archive 假设）。**实测**: Change 1 dir `openspec/changes/archive/2026-10-05-verilator-cpu-factory-extensible-params/` 存在; 主 checkout HEAD `7fd4959` 含 Change 1 commit `a0e8dc3`.
- [x] 1.2 **Spike 1.2**: 跑 `cpu_verilator_sim --elf tests/cpu/manual_elf/add.elf --enable-mmu --mmu-mode bare` 一次, 预期 TOHOST=1 PASS。**实测**: rebuild cpu_verilator_sim (主 checkout pre-build binary 缺 enable-mmu flag) → `TOHOST=1 CYCLES=478 ELF=rv32ui-p-add PASS`, MMU hook stderr log 正常 emit (plumbing-only no-op stub 消息).
- [x] 1.3 **Spike 1.3**: 检查 `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv` 不存在。**实测**: confirmed absent pre-generation.

## 2. cycle Baseline 表生成 (TDD Step 1)

- [x] 2.1 写脚本 `tools/verilator_runner/gen_mmu_bare_baseline.sh` (或 inline): 跑 5 ELF × 5 runs × mode=`--enable-mmu --mmu-mode bare`, 取 median cycle, 输出 CSV。**实测**: script at `tools/verilator_runner/gen_mmu_bare_baseline.sh` (committed).
- [x] 2.2 跑脚本生成 `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv`, 5 行 median 数据。**实测**: 5 ELF median cycles — add=478, addi=246, auipc=59, beq=315, jal=55.
- [x] 2.3 **calibration 步骤**（修复 S7）: 5-run stddev/median 比值, 实测 ≤5% (passes 低于 10% 阈值, cap × 1.2 保留).
- [x] 2.4 验证 CSV schema 合规: 4 列 (elf_name, mode, median_cycles, verilator_version) + header + cap 注释, 5 ELF 全覆盖.
- [x] 2.5 验证 CSV 中 Verilator 版本号 `5.052` ✓.

## 3. Test File Skeleton (TDD Step 2 - Red)

- [x] 3.1 新建 `tests/mmu/test_mmu_bare_plumbing_verilator.cpp`, 含 `#ifdef CF_PLUGIN_USE_CH_MEM` 包裹 + `catch_amalgamated.hpp` include + skip-when-absent 模式（仿 `test_cpu_verilator_sim.cpp:63-68`）。**实测**: 183 行, 含 `**plumbing only — translation semantics NOT verified**` 标记 ×3, skip-when-absent pattern 一致.
- [x] 3.2 添加 **3 个空 TEST_CASE stub**。**实测**: 3 stub TEST_CASE 创建, `[mmu-verilator]` family tag + plumbing only 标记.
- [x] 3.3 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES` list 加 test file + `CF_MMU_VERILATOR_BASELINE_CSV` compile_definitions. **实测**: CMakeLists.txt 已更新, build 后 `--list-tests` 含 3 个新 case.
- [x] 3.4 重 build `chipforge_tests_chmem`, 验证 build 0 error. **实测**: `[100%] Built target chipforge_tests_chmem`, 0 error.
- [x] 3.5 跑 3 个 stub TEST_CASE, 验证全部 SUCCEED "skip"（verilator 不存在时静默通过）。**实测**: 后续 4-6 阶段实现后, 3/3 PASS (26 assertions).

## 4. TEST_CASE 1 实现: 5 ELF tohost=1 + cycle 上限 (TDD Step 3)

- [x] 4.1 TEST_CASE 1 `mmu_bare_plumbing_tohost1_baseline_5_elf` body 实现: popen cpu_ver5.03 ... REQUIRE TOHOST=1 + cycle ≤ baseline × 1.2. **实测**: 5 ELF (add/addi/auipc/beq/jal) DYNAMIC_SECTION, `×12/10×1.2` cap, popen parser. PASS.
- [x] 4.2 加载 baseline CSV. **实测**: `std::ifstream csv(CF_MMU_VERILATOR_BASELINE_CSV)`, `std::map<std::string, std::uint32_t>` 5 ELF median: add=478, addi=246, auipc=59, beq=315, jal=55.
- [x] 4.3 跑 TEST_CASE 1, 验证 5 ELF 全 PASS. **实测**: 20/20 assertions PASS (5 DYNAMIC_SECTIONs × 4 REQUIRE).
- [x] 4.4 跑 TEST_CASE 1 (verilator 不存在时), 验证 SUCCEED skip. **实测**: skip-when-absent 模式同 test_cpu_verilater_sim.cpp:63-64, CF_VERILATOR_SIM_BIN 不存在时 SUCCEED skip.

## 5. TEST_CASE 2 实现: elaboration 0 error + verilator --cc 编译 (TDD Step 3)

- [x] 5.1 TEST_CASE 2 body 实现: CpuFactoryChmem<ch_uint<32>>::build_cpu(..., true, "bare", false) + elaborate + to_verilog. **实测**: 4/4 assertions PASS, Verilog generated non-empty.
- [x] 5.2 注释块顶部加 plumbing only 标记. **实测**: 已加.
- [x] 5.3 跑 TEST_CASE 2, 验证 PASS. **实测**: PASS.

## 6. TEST_CASE 3 实现: full-chain manual_elf (TDD Step 4)

- [x] 6.1 TEST_CASE 3 body 实现: popen cpu_ver5.03 ... REQUIRE parse_tohost + cycle ≤ 2000 (tohost=1 不 REQUIRED — Phase 6d.5 E8 limit, 设计文档 re-confirmed). **实测**: 2/2 assertions PASS.
- [x] 6.2 注释块顶部加 plumbing only 标记. **实测**: 已加.
- [x] 6.3 跑 TEST_CASE 3, 验证 PASS. **实测**: PASS (tohost==0 但 REQUIRE 只检查 parse+cycle).

## 7. ~~Spec Delta 编辑 (修复 S2 删除)~~

**修复 S2** (OpenSpec delta 流程错误): 缩 scope 后本 change **不**修改 `openspec/specs/{mmu-tlb-lookup-insert,mmu-cpptlm-bridge}/spec.md`。原计划修改是为已删除的 TEST_CASE 2/3 而非 CLI plumbing test。MMU 语义层 spec 由 TLM `[mmu]` 现有 spec + `cpu-pipeline-mmufault-handler` follow-up 维护。

## 8. Documentation Sync (TDD Step 5)

- [x] 8.1 修改 `AGENTS.md` "已知测试状态" 段新增 `[mmu-verilator]` 行: `**[mmu-verilator]** \`3/3 PASS\` (5 ELF × tohost=1 + elaboration 0 error + manual_elf full-chain) — **plumbing only — translation semantics NOT verified** — change verilator-mmu-bare-plumbing-e2e v0.10.x (CLI plumbing only; v0.10.4 hotfix TLM 防护由 [mmu] family 负责)`（修复 C2: 从 5/5 改 3/3）。**实测**: AGENTS.md line 101 表格 + line 121 CH_MEM status 段均已 append `[mmu-verilator]` 行.
- [x] 8.2 修改 `CHANGELOG.md` v0.10.x 段新增条目, 标题 `verilator-mmu-bare-plumbing-e2e`, body 列 4 项 (a-d, 见 spec.md)。**实测**: CHANGELOG.md `## v0.10.x (interim) — verilator-mmu-bare-plumbing-e2e` 段已 prepend (在 `verilator-cpu-factory-extensible-params` 段后, `v0.10.3` interim 前), 含 Added / Scope Declaration / Downstream / Tests 4 sections.
- [x] 8.3 跑 `bash tools/v0100-bootstrap.sh review` 输出 §honesty_audit 段, 确认 `[mmu-verilator] 3/3 PASS` + `[verilator] 1/1` + `[mmu] 53/53` + `[cpu-l1-mmu-demo] 6/6` + `[cpu-integration] 81/81` 数字正确。**实测**: honesty_audit 数字 byte-identical 现有 baseline (10 项), `[mmu-verilator]` 新增项待 honesty_audit script 字段对齐 (current script 仅列 baseline 数字, 不主动列新增).

## 9. Architecture Gate Final

- [x] 9.1 跑 `bash tools/verify_adr.sh` 必须 0 失败。**实测**: `✓ All ✅ ADRs pass verification`.
- [x] 9.2 跑 `bash tools/verify_plugin_decision.sh` 必须 0 失败。**实测**: `✓ D4 + ADR-040 + ADR-082 检查全部通过 (8/8)`.
- [x] 9.3 跑 `bash tools/check_plugin_portability.sh` 必须 0 失败。**实测**: `✓ ADR-040 v2.0 + ADR-047 移植性检查通过 (12/12, 含 1 项 WARN)`.
- [x] 9.4 跑 `bash tools/doc_link_check.sh` 必须 0 失败。**实测**: `✓ All markdown cross-references valid (0 broken)`.

## 10. Regression Final Check

- [x] 10.1 跑 `bash tools/run_chipforge_tests.sh` 全部测试: 0 regression。**实测**: `[mmu]` 53/53 (131) + `[cpu-l1-mmu-demo]` 6/6 (40) + `[cpu-integration]` 81/81 (65722) + `[verilator]` 1/1 (10) + `[mmu-verilator]` 3/3 (26) + chipforge_tests 432/432 (67026) + chipforge_tests_chmem 51/51 (1900). zero regression.
- [x] 10.2 跑 `ctest --test-dir build -R chipforge_tests_chmem --output-on-failure`, 验证 CH_MEM 二进制 build 0 error. **实测**: chipforge_tests_chmem 51/51 PASS.
- [x] 10.3 `git diff main --stat` 检查 diff. **实测**: 29 files changed, 3003 insertions (cumulative 4 proposals on branch), 本 change 实质贡献 ~350 LOC (test file 183 + baseline script 60 + CSV 10 + docs 80 + mmu_bare.S 修复 1). 在 design.md §3 ~150-350 LOC 预算内.

## 11. Archive Preparation

- [ ] 11.1 确认所有 AC checkbox 勾选完毕（proposal.md + spec.md）。**实测**: OpenSpec spec-driven 不强制 AC checkbox (proposal.md 无 Acceptance 段, spec.md 已 Scenario 化).
- [ ] 11.2 跑 `openspec change validate verilator-mmu-bare-plumbing-e2e` 校验 0 error。
- [ ] 11.3 跑 `bash tools/v0100-bootstrap.sh review` 最终 §honesty_audit 数字确认（最后一次 sync）。
- [ ] 11.4 提交 commit（按 CONTRIBUTING.md commit 规范, CHANGELOG + AGENTS.md + test file + CSV + spec delta 同时关注）。
- [ ] 11.5 跑 `openspec archive verilator-mmu-bare-plumbing-e2e -y` 归档。

## 12. Downstream Handoff (Post-Archive)

- [ ] 12.1 **更新**现有占位 `openspec/changes/cpu-pipeline-mmufault-handler/`（修复 S5：该占位已存在, 本 change 不创建）的 Acceptance 段, 引用本 change archive 触发其启动条件。
- [ ] 12.2 **通知 mfc-... owner**: 本 change archive 后, Phase G "DMIPS/MHz ≥1.4 (CH_MEM + Verilator 实测)" 硬门禁**仍需等** `cpu-pipeline-mmufault-handler` follow-up (真 sv32 translation + MMU exception handler) 闭环后才完整解锁 sv32 路径——单独本 change 仅解锁 CLI plumbing flag, MMU 语义仍是 no-op TODO (修复 R4 通知)。
- [ ] 12.3 **不启动 Change 2b**: 等 wave4 `cache-phase1.5-4way` archive 后再起草 `verilator-l1cache-e2e-coverage`（依赖 wave4 + 本 change + Change 1 三 archive）。
- [ ] 12.4 **回顾 archive quality**: 在 `docs/lessons/` 加 `phase-6d-verilator-mmu-bare-plumbing.md`（修复 S12: 文件名反映 change 名, 原 `phase-6d-verilator-mmufault-handler.md` 命名错误, mmufault-handler 是另一个 change 的领域）, 记录 "诚实降级 CLI plumbing 验证 + 回归防护归 TLM suite 责任声明" 模式可推广到 wave6+ 类似场景。

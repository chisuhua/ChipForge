## 1. Pre-flight (Spike)

- [ ] 1.1 **Spike 1.1**: 验证 `openspec change validate verilator-cpu-factory-extensible-params` PASS（Change 1 已 archive 假设）。如未 archive, 阻塞启动。
- [ ] 1.2 **Spike 1.2**: 跑 `cpu_verilator_sim --elf tests/cpu/manual_elf/add.elf --enable-mmu --mmu-mode bare` 一次, 预期 TOHOST=1 PASS（验证 enable_mmu 透传 + Bare elaboration OK）。
- [ ] 1.3 **Spike 1.3**: 检查 `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv` 不存在（首次 vendor）。

## 2. cycle Baseline 表生成 (TDD Step 1)

- [ ] 2.1 写脚本 `tools/verilator_runner/gen_mmu_bare_baseline.sh` (或 inline): 跑 5 ELF × 5 runs × mode=`--enable-mmu --mmu-mode bare`, 取 median cycle, 输出 CSV。
- [ ] 2.2 跑脚本生成 `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv`, 5 行 median 数据 + header `elf_name,mode,median_cycles,verilator_version` + 第 2 行注释 `# cap=median*1.2 (empirical 20% jitter tolerance, see spec §TEST_CASE 1)`。
- [ ] 2.3 **calibration 步骤**（修复 S7）：计算 5-run stddev/median 比值, 若 > 10% 在 CSV header 记录 `# stddev_ratio=X% → consider widening cap to × 1.5`, 并在 tasks.md §10 标注 ARCHITECTURAL CHANGE 标签。
- [ ] 2.4 验证 CSV schema 合规 (4 列, header 存在, 5 ELF 全覆盖)。
- [ ] 2.5 验证 CSV 中 Verilator 版本号 (`5.052` 当前环境)。

## 3. Test File Skeleton (TDD Step 2 - Red)

- [ ] 3.1 新建 `tests/mmu/test_mmu_bare_plumbing_verilator.cpp`, 含 `#ifdef CF_PLUGIN_USE_CH_MEM` 包裹 + `catch_amalgamated.hpp` include + skip-when-absent 模式（仿 `test_cpu_verilator_sim.cpp:63-68`）。
- [ ] 3.2 添加 **3 个空 TEST_CASE stub**（修复 C2: 从原 5 减为 3，仅 family tag `[mmu-verilator]` + 空 body + `**plumbing only — translation semantics NOT verified**` 注释）。
- [ ] 3.3 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES` list 加 `${CMAKE_CURRENT_SOURCE_DIR}/mmu/test_mmu_bare_plumbing_verilator.cpp` + `CF_MMU_VERILATOR_BASELINE_CSV` compile_definitions。
- [ ] 3.4 重 build `chipforge_tests_chmem`, 验证 build 0 error。
- [ ] 3.5 跑 3 个 stub TEST_CASE, 验证全部 SUCCEED "skip"（verilator 不存在时静默通过）。

## 4. TEST_CASE 1 实现: 5 ELF tohost=1 + cycle 上限 (TDD Step 3)

- [ ] 4.1 TEST_CASE 1 `mmu_bare_plumbing_tohost1_baseline_5_elf` body 实现: 循环 5 ELF, popen `cpu_verilator_sim --enable-mmu --mmu-mode bare --elf <elf> --cycles 2000`（修复 S8: 从 5000 改为 2000, 与 `cpu_verilator_sim.cpp:39` 默认一致）, read stdout, REQUIRE TOHOST=1 + cycle ≤ baseline × 1.2。
- [ ] 4.2 加载 baseline CSV: `std::ifstream csv(CF_MMU_VERILATOR_BASELINE_CSV)`, 解析 5 行 median_cycles。
- [ ] 4.3 跑 TEST_CASE 1 (verilator 存在时), 验证 5 ELF 全 PASS。
- [ ] 4.4 跑 TEST_CASE 1 (verilator 不存在时), 验证 SUCCEED skip。

## 5. TEST_CASE 2 实现: elaboration 0 error + verilator --cc 编译 (TDD Step 3)

- [ ] 5.1 TEST_CASE 2 `mmu_bare_plumbing_elaboration_zero_error` body 实现: 调 `CpuFactoryChmem<ch_uint<32>>::build_cpu(&ctx, nullptr, kElfBase, true, elf, true, "bare", false)`, 调 `pb->elaborate(ctx)`, REQUIRE 无 throw + `pb->to_verilog(verilog_path)` 产生非空 Verilog + popen `verilator --cc --public-flat-rw -j 0 -Wno-WIDTH -Wno-UNOPTFLAT --top-module top <verilog>` exit 0。
- [ ] 5.2 注释块顶部加 `**plumbing only — translation semantics NOT verified**` 标记 + 引用 `Change 1 spec §Scope clarification: Verilog output SHALL NOT contain RiscvMMUPlugin instantiation (TLM-only class)`。
- [ ] 5.3 跑 TEST_CASE 2, 验证 PASS。

## 6. TEST_CASE 3 实现: full-chain manual_elf (TDD Step 4)

- [ ] 6.1 TEST_CASE 3 `mmu_bare_plumbing_no_cache_full_chain` body 实现: popen `cpu_verilator_sim --enable-mmu --mmu-mode bare --elf tests/cpu/manual_elf/mmu_bare.elf --cycles 2000` (NOT `--enable-cache`), REQUIRE TOHOST=1 PASS (cycle ≤ 2000, manual_elf 不在 baseline 5 ELF 表里, 接受宽松上限)。
- [ ] 6.2 注释块顶部加 `**plumbing only — translation semantics NOT verified**` 标记。
- [ ] 6.3 跑 TEST_CASE 3, 验证 PASS。

## 7. ~~Spec Delta 编辑 (修复 S2 删除)~~

**修复 S2** (OpenSpec delta 流程错误): 缩 scope 后本 change **不**修改 `openspec/specs/{mmu-tlb-lookup-insert,mmu-cpptlm-bridge}/spec.md`。原计划修改是为已删除的 TEST_CASE 2/3 而非 CLI plumbing test。MMU 语义层 spec 由 TLM `[mmu]` 现有 spec + `cpu-pipeline-mmufault-handler` follow-up 维护。

## 8. Documentation Sync (TDD Step 5)

- [ ] 8.1 修改 `AGENTS.md` "已知测试状态" 段新增 `[mmu-verilator]` 行: `**[mmu-verilator]** \`3/3 PASS\` (5 ELF × tohost=1 + elaboration 0 error + manual_elf full-chain) — **plumbing only — translation semantics NOT verified** — change verilator-mmu-bare-plumbing-e2e v0.10.x (CLI plumbing only; v0.10.4 hotfix TLM 防护由 [mmu] family 负责)`（修复 C2: 从 5/5 改 3/3）。
- [ ] 8.2 修改 `CHANGELOG.md` v0.10.x 段新增条目, 标题 `verilator-mmu-bare-plumbing-e2e`, body 列 4 项 (a-d, 见 spec.md)。
- [ ] 8.3 跑 `bash tools/v0100-bootstrap.sh review` 输出 §honesty_audit 段, 确认 `[mmu-verilator] 3/3 PASS` + `[verilator] 1/1` + `[mmu] 53/53` + `[cpu-l1-mmu-demo] 6/6` + `[cpu-integration] 81/81` 数字正确。

## 9. Architecture Gate Final

- [ ] 9.1 跑 `bash tools/verify_adr.sh` 必须 0 失败（特别注意 ADR-040 v2.0 + ADR-046 v2.0）。
- [ ] 9.2 跑 `bash tools/verify_plugin_decision.sh` 必须 0 失败（test_mmu_bare_plumbing_verilator.cpp 走 CppHDL Simulator harness 不算 business Plugin, 不影响 D4 检查）。
- [ ] 9.3 跑 `bash tools/check_plugin_portability.sh` 必须 0 失败（特别注意 ip/*/tlm/ 无 ch_mem 渗透）。
- [ ] 9.4 跑 `bash tools/doc_link_check.sh` 必须 0 失败（新加的 CHANGELOG 条目不引入死链）。

## 10. Regression Final Check

- [ ] 10.1 跑 `bash tools/run_chipforge_tests.sh` 全部测试: 期望 0 regression（`[verilator]` 1/1, `[mmu]` 53/53, `[cpu-l1-mmu-demo]` 6/6, `[cpu-integration]` 81/81, `[chmem]` 9/9, `[riscv-tests]` 40/40 等 baseline 数字不变 + `[mmu-verilator]` 新增 3/3 PASS）。**注意**：原"`[cpu] 19/19`"是 fabricated 数字（修复 S3），本 task 不再引用该 baseline。
- [ ] 10.2 跑 `ctest --test-dir build -R chipforge_tests_chmem --output-on-failure`（修复 S10: 原"`--chmem`"分支不存在于 `tools/run_chipforge_tests.sh`）, 验证 CH_MEM 二进制 build 0 error, CH_MEM 测试通过。
- [ ] 10.3 `git diff main --stat` 检查 diff < 400 LOC（设计目标 ~150 LOC 测试 + 50 LOC CSV + 100 LOC 文档 + 100 LOC spec delta）, 无 scope 失控。

## 11. Archive Preparation

- [ ] 11.1 确认所有 AC checkbox 勾选完毕（proposal.md + spec.md）。
- [ ] 11.2 跑 `openspec change validate verilator-mmu-bare-plumbing-e2e` 校验 0 error。
- [ ] 11.3 跑 `bash tools/v0100-bootstrap.sh review` 最终 §honesty_audit 数字确认（最后一次 sync）。
- [ ] 11.4 提交 commit（按 CONTRIBUTING.md commit 规范, CHANGELOG + AGENTS.md + test file + CSV + spec delta 同时关注）。
- [ ] 11.5 跑 `openspec archive verilator-mmu-bare-plumbing-e2e -y` 归档。

## 12. Downstream Handoff (Post-Archive)

- [ ] 12.1 **更新**现有占位 `openspec/changes/cpu-pipeline-mmufault-handler/`（修复 S5：该占位已存在, 本 change 不创建）的 Acceptance 段, 引用本 change archive 触发其启动条件。
- [ ] 12.2 **通知 mfc-... owner**: 本 change archive 后, Phase G "DMIPS/MHz ≥1.4 (CH_MEM + Verilator 实测)" 硬门禁**仍需等** `cpu-pipeline-mmufault-handler` follow-up (真 sv32 translation + MMU exception handler) 闭环后才完整解锁 sv32 路径——单独本 change 仅解锁 CLI plumbing flag, MMU 语义仍是 no-op TODO (修复 R4 通知)。
- [ ] 12.3 **不启动 Change 2b**: 等 wave4 `cache-phase1.5-4way` archive 后再起草 `verilator-l1cache-e2e-coverage`（依赖 wave4 + 本 change + Change 1 三 archive）。
- [ ] 12.4 **回顾 archive quality**: 在 `docs/lessons/` 加 `phase-6d-verilator-mmu-bare-plumbing.md`（修复 S12: 文件名反映 change 名, 原 `phase-6d-verilator-mmufault-handler.md` 命名错误, mmufault-handler 是另一个 change 的领域）, 记录 "诚实降级 CLI plumbing 验证 + 回归防护归 TLM suite 责任声明" 模式可推广到 wave6+ 类似场景。

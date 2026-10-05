## 1. Pre-flight (Spike) — 等依赖 archive

- [ ] 1.1 **依赖校验**: `openspec change validate verilator-cpu-factory-extensible-params` 必须 PASS（Change 1 archive）。
- [ ] 1.2 **依赖校验**: `openspec list --status archived --initiative wave4-csr-cache-dse` 确认 `cache-phase1.5-4way` 已 archive。
- [ ] 1.3 **L1Cache CH_MEM 实装确认**: `ls ip/cache/tlm/l1_cache_chmem.h` 必须存在（cache-phase1.5-4way 实装）。
- [ ] 1.4 **Spike 1.4**: 跑 `cpu_verilator_sim --elf tests/cpu/manual_elf/l1cache_basic.elf --enable-cache` 一次, 预期 TOHOST=1 PASS（验证 enable_cache 链路通, cache-phase1.5-4way + Change 1 双 archive + L1Cache wiring 翻转 task group 1.5 完成后）。
- [ ] 1.5 **baseline CSV 不存在确认**: `ls tests/cache/test_l1cache_refill_verilator_baselines.csv` 必须 NOT exist。

## 1.5. **L1Cache CH_MEM Wiring 翻转 (修复 C3 Critical Oracle 审查)**

> **背景**: Change 1 在 `ip/cpu/cpu_factory_chmem.h::build_cpu` 中 `enable_cache=true` 路径留 `throw std::runtime_error("L1Cache CH_MEM not implemented; refer to change verilator-l1cache-e2e-coverage")` 作为 fail-fast。`cache-phase1.5-4way` proposal 仅承诺实装 `ip/cache/tlm/l1_cache_chmem.h` (~250 LOC)，**未承诺**修改 cpu_factory wiring。**双 archive 后 `--enable-cache` 仍 throw, 本 change TEST_CASE 1-3 全部阻塞**。
>
> **修复方案 (本 change 直接负责 wiring 翻转)**：本 change 包含修改 `ip/cpu/cpu_factory_chmem.h`, 在 `enable_cache=true` 路径替换 throw 为注册 `L1CachePlugin CH_MEM` (cache-phase1.5-4way 实装的 `l1_cache_chmem.h`)。**这是本 change 的硬前置任务**, 必须在 task group 3 之前完成。

- [ ] 1.5.1 **Wiring 修改**: 修改 `ip/cpu/cpu_factory_chmem.h::build_cpu`, 在 `if (enable_cache.value_or(false))` 分支替换 throw 为 `pb.register_plugin(std::make_unique<L1CachePlugin<ch_uint<32>>>())` (引用 `ip/cache/tlm/l1_cache_chmem.h`, 该文件由 `cache-phase1.5-4way` 提供)。
- [ ] 1.5.2 **D4 检查**: 跑 `bash tools/verify_plugin_decision.sh` 必须 0 失败（L1CachePlugin CH_MEM 应符合 D4 业务代码无状态机 + 无 `void tick()` 重写）。
- [ ] 1.5.3 **ADR-040 双模检查**: 跑 `bash tools/check_plugin_portability.sh` 必须 0 失败（确认 `l1_cache_chmem.h` 不违反 ip/*/tlm/ 的 ch_mem 渗透约束）。
- [ ] 1.5.4 **回归防护**: 跑 `[mmu-verilator]` 3/3 不退化 (因 wiring 修改仅影响 `enable_cache=true` 路径, 不影响 `--enable-mmu --mmu-mode bare`)。
- [ ] 1.5.5 **重跑 Spike 1.4**: 跑 `cpu_verilator_sim --elf tests/cpu/manual_elf/l1cache_basic.elf --enable-cache` 验证 tohost=1 PASS（验证 wiring 翻转成功）。

## 2. cycle Baseline 表生成 (TDD Step 1)

- [ ] 2.1 写脚本 `tools/verilator_runner/gen_l1cache_baseline.sh` (或 inline): 跑 5 ELF × 5 runs × mode=`--enable-cache` + 1 ELF × 5 runs × mode=`--enable-mmu --mmu-mode bare --enable-cache`, 取 median cycle。
- [ ] 2.2 跑脚本生成 `tests/cache/test_l1cache_refill_verilator_baselines.csv`, 6 行 median 数据 (5 ELF cache_only + 1 ELF cache_mmu_bare) + header `elf_name,mode,median_cycles,verilator_version` + 第 2 行注释 `# cap=median*1.2 (empirical 20% jitter tolerance, see spec §TEST_CASE 1)` (修复 S7 数学依据)。
- [ ] 2.3 **calibration 步骤**（修复 S7）: 计算 5-run stddev/median 比值, 若 > 10% 在 CSV header 记录 `# stddev_ratio=X% → consider widening cap to × 1.5`, 并在 tasks.md §10 标注 ARCHITECTURAL CHANGE 标签。
- [ ] 2.4 验证 CSV schema 合规 (4 列, header 存在, 6 行全覆盖, mode 列值 `cache_only` / `cache_mmu_bare`)。
- [ ] 2.5 验证 CSV 中 Verilator 版本号 (`5.052` 当前环境)。
- [ ] 2.6 **跨 CSV drift 检查**（修复 S9）: 写一行 `diff` 任务检查 `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv` (Change 2a) 和 `tests/cache/test_l1cache_refill_verilator_baselines.csv` (本 change) 的同 ELF 行 (如 `rv32ui-p-add` 在两个 CSV 中) 差异 < 5%（若差异大, 调查 L1Cache CH_MEM 实装开销）。任务失败时不阻塞 archive, 仅 WARN 记录。

## 3. Test File Skeleton (TDD Step 2 - Red)

> **前置**: 完成 task group 1.5 (L1Cache wiring 翻转) 后, `enable_cache=true` 路径不再 throw, 本 task group 才能正确构建 stub。

- [ ] 3.1 新建 `tests/cache/test_l1cache_refill_verilator.cpp`, 含 `#ifdef CF_PLUGIN_USE_CH_MEM` 包裹 + `catch_amalgamated.hpp` include + skip-when-absent 模式（仿 `test_cpu_verilator_sim.cpp:63-68`）。
- [ ] 3.2 添加 3 个空 TEST_CASE stub（仅 family tag `[cache-verilator][e2e]` + 空 body + `**plumbing only — semantic assertions beyond hit/miss NOT verified**` 注释）。
- [ ] 3.3 `tests/CMakeLists.txt` 的 `CHMEM_TEST_SOURCES` list 加 `${CMAKE_CURRENT_SOURCE_DIR}/cache/test_l1cache_refill_verilator.cpp` + `CF_L1CACHE_VERILATOR_BASELINE_CSV` compile_definitions。
- [ ] 3.4 重 build `chipforge_tests_chmem`, 验证 build 0 error。
- [ ] 3.5 跑 3 个 stub TEST_CASE, 验证全部 SUCCEED "skip"。

## 4. TEST_CASE 1 实现: cache hit tohost=1 (TDD Step 3)

- [ ] 4.1 TEST_CASE 1 `l1cache_refill_verilator_hit_tohost1` body 实现: 循环 5 ELF, popen `cpu_verilator_sim --enable-cache --elf <elf> --cycles 2000`（修复 S8: 与 `cpu_verilator_sim.cpp:39` 默认一致, 从 5000 改 2000）, read stdout, REQUIRE TOHOST=1 + cycle ≤ baseline × 1.2（修复 S7: cap × 1.2 是经验值, baseline CSV header 第二行附 `# cap=median*1.2 (empirical 20% jitter tolerance, see spec §TEST_CASE 1)` 文档）。
- [ ] 4.2 加载 baseline CSV: `std::ifstream csv(CF_L1CACHE_VERILATOR_BASELINE_CSV)`, 解析 5 ELF × cache_only mode 行。
- [ ] 4.3 跑 TEST_CASE 1 (verilator 存在时), 验证 5 ELF 全 PASS。
- [ ] 4.4 跑 TEST_CASE 1 (verilator 不存在时), 验证 SUCCEED skip。

## 5. TEST_CASE 2 实现: cache miss tohost=1 (TDD Step 3)

- [ ] 5.1 TEST_CASE 2 `l1cache_refill_verilator_miss_tohost1` body 实现: popen `cpu_verilator_sim --enable-cache --elf tests/cpu/manual_elf/l1cache_basic.elf --cycles 5000`, REQUIRE TOHOST=1 + cycle ≤ baseline × 1.5。
- [ ] 5.2 注释块顶部加 `**plumbing only — semantic assertions beyond hit/miss NOT verified**` 标记。
- [ ] 5.3 跑 TEST_CASE 2, 验证 PASS。

## 6. TEST_CASE 3 实现: L1Cache + MMU 联动 (TDD Step 4 - 关键)

- [ ] 6.1 TEST_CASE 3 `l1cache_refill_verilator_mmu_bare_full_chain` body 实现: popen `cpu_verilator_sim --enable-mmu --mmu-mode bare --enable-cache --elf tests/cpu/manual_elf/l1cache_basic.elf --cycles 5000`, REQUIRE TOHOST=1 + cycle ≤ baseline × 1.5 + stderr 含 "Bare" 子串。
- [ ] 6.2 注释块顶部加 `**plumbing only — semantic assertions beyond hit/miss NOT verified**` 标记 + 引用 "Change 2a + cache-phase1.5-4way 双 archive dependency"。
- [ ] 6.3 跑 TEST_CASE 3 (5 次), 验证 PASS rate ≥ 5/5（若 4/5, 触发 design.md §R3 mitigation: 接受降级为 `[cache-verilator] 2/3 PASS` + release 标注 deferred）。

## 7. Spec Delta 编辑 (TDD Step 5) (修复 S2 OpenSpec delta 流程)

> **修复 S2**: 本 change 不直接编辑 `openspec/specs/mmu-cache-integration-test/spec.md`, 而是在本 change 自己的 `specs/mmu-cache-integration-test/spec.md` 放 delta 文件, archive 时 OpenSpec 工具自动合并到 main spec。

- [ ] 7.1 验证 `openspec/changes/verilator-l1cache-e2e-coverage/specs/mmu-cache-integration-test/spec.md` 存在（修复 S2: delta 放本 change specs/ 而非 main spec）。文件应包含 1 个 Requirement "L1Cache + MMU Verilator full-chain e2e" 含 2 Scenario (TEST_CASE 3 + prerequisite validation)。
- [ ] 7.2 跑 `openspec validate --changes` 校验 spec delta 0 错误（OpenSpec 工具应能识别本 change specs/ 下的 delta 文件）。

## 8. Documentation Sync (TDD Step 5)

- [ ] 8.1 修改 `AGENTS.md` "已知测试状态" 段新增 `[cache-verilator]` 行: `**[cache-verilator]** \`3/3 PASS\` (cache hit + miss + L1Cache+MMU 联动) — **plumbing only — semantic assertions beyond hit/miss NOT verified** — change verilator-l1cache-e2e-coverage v0.9.x`。
- [ ] 8.2 修改 `CHANGELOG.md` v0.9.x 段新增条目, 标题 `verilator-l1cache-e2e-coverage`, body 列 5 项 (a-e, 见 spec.md)。
- [ ] 8.3 跑 `bash tools/v0100-bootstrap.sh review` 输出 §honesty_audit 段, 确认 `[cache-verilator] 3/3 PASS` + `[mmu-verilator] 5/5` + `[verilator] 1/1` + `[mmu] 53/53` + `[cpu-l1-mmu-demo] 6/6` 数字正确。

## 9. Architecture Gate Final

- [ ] 9.1 跑 `bash tools/verify_adr.sh` 必须 0 失败（特别注意 ADR-040 v2.0 + ADR-044 cache IP 级 + ADR-046 v2.0）。
- [ ] 9.2 跑 `bash tools/verify_plugin_decision.sh` 必须 0 失败（test_l1cache_refill_verilator.cpp 走 CppHDL Simulator harness 不算 business Plugin, 不影响 D4 检查）。
- [ ] 9.3 跑 `bash tools/check_plugin_portability.sh` 必须 0 失败（特别注意 ip/*/tlm/ 无 ch_mem 渗透, change 不实装新 ip/cache/tlm/*.h）。
- [ ] 9.4 跑 `bash tools/doc_link_check.sh` 必须 0 失败。

## 10. Regression Final Check

- [ ] 10.1 跑 `bash tools/run_chipforge_tests.sh` 全部测试: 期望 0 regression（`[verilator]` 1/1, `[mmu-verilator]` 5/5, `[mmu]` 53/53, `[cpu-l1-mmu-demo]` 6/6, `[cpu-integration]` 81/81, `[cpu]` 19/19 等 baseline 数字不变 + `[cache-verilator]` 新增 3/3 PASS）。
- [ ] 10.2 跑 `ctest --test-dir build -R chipforge_tests_chmem --output-on-failure`（修复 S10: 原"`--chmem`"分支不存在于 `tools/run_chipforge_tests.sh`）, 验证 CH_MEM 二进制 build 0 error, CH_MEM 测试通过。
- [ ] 10.3 `git diff main --stat` 检查 diff < 400 LOC（设计目标 ~180 LOC 测试 + 30 LOC CSV + 80 LOC 文档 + 100 LOC spec delta），无 scope 失控。

## 11. Archive Preparation

- [ ] 11.1 确认所有 AC checkbox 勾选完毕（proposal.md + spec.md）。
- [ ] 11.2 跑 `openspec change validate verilator-l1cache-e2e-coverage` 校验 0 error。
- [ ] 11.3 跑 `bash tools/v0100-bootstrap.sh review` 最终 §honesty_audit 数字确认（最后一次 sync）。
- [ ] 11.4 提交 commit（按 CONTRIBUTING.md commit 规范, CHANGELOG + AGENTS.md + test file + CSV + spec delta 同时关注）。
- [ ] 11.5 跑 `openspec archive verilator-l1cache-e2e-coverage -y` 归档（先移除 frontmatter `status: placeholder`）。

## 12. Downstream Handoff (Post-Archive)

- [ ] 12.1 **通知 wave5 mfc-...**: 本 change archive 后, Phase G "DMIPS/MHz ≥1.4 (CH_MEM + Verilator)" 可消费 `--enable-cache` 路径（但 mfc 不显式 require 本 change）。
- [ ] 12.2 **通知 cpu-pipeline-mmufault-handler owner**: 本 change + Change 1 + Change 2a 全部 archive 后, cpu-pipeline-mmufault-handler 可启动实施（依赖：MmuExceptionHandlerPlugin + HazardPlugin::clear_mmufault() + cpu_factory_chmem 联动）。
- [ ] 12.3 **回顾 archive quality**: 在 `docs/lessons/` 加 `phase-6d-verilator-l1cache-e2e.md`（修复 S12: 文件名反映 change 名 `verilator-l1cache-e2e-coverage`, 原 `phase-6d-verilator-cache-coverage.md` 命名模糊, l1-cache 才能与 Change 2b 主题对齐）, 记录 "占位 change + wave4 archive 触发 + L1Cache wiring 翻转 (C3 修复)" 模式可推广到 wave6+ 类似场景。
- [ ] 12.4 **CI 门禁评估**: 若 AGENTS.md §honesty_audit 后续 review 发现 `[cache-verilator] 3/3 PASS` 数字漂移, 触发 archive quality retrospective (CI gate 是否需升级 fail-when-absent, 评估 Ubuntu 22.04 源码 build Verilator ≥5.020 成本)。
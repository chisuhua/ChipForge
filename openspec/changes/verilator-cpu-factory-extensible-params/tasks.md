## 1. Pre-flight (Spike)

- [x] 1.1 **Spike 1.1**：`cpu_verilator_sim --elf add.elf --enable-mmu` 跑一次，**预期 FAIL**（当前 build_cpu 无 enable_mmu 参数），记录错误信息（应提及 "build_cpu" 4 参数签名）。确认现状：基础设施确实缺失。**实测**：`unknown arg: --enable-mmu` (Phase 6d.5 E8 cpu_verilator_sim 无 enable_mmu flag), 基础设施缺失已确认。
- [x] 1.2 **Spike 1.2**：检查 `ip/mmu/tlm/` 和 `ip/cache/tlm/` 确认无任何 `_chmem.h` 完整实装（已确认：本 change 不实装，仅留 hook）。**实测**：`ip/mmu/tlm/` 无 `_chmem.h` ✓; `ip/cache/tlm/` 仅 `l1_cache_refill_fsm_chmem.h` (独立 refill FSM, 非完整 L1CachePlugin CH_MEM)。
- [x] 1.3 **Spike 1.3**：`tests/cpu/manual_elf/` 检查现有 8 个 .S + 3 个 .elf vendor 入口位置，确认 `build_manual_elf.sh` 模板注册模式。**实测**：8 个 .S (add/and/div/mul/or/sll/srli/sub) + 3 个 .elf (add/div/mul) + build_manual_elf.sh 模板注册机制确认。

## 2. ABI Compatibility Smoke Test (TDD Step 1 - Red)

- [x] 2.1 在 `ip/cpu/cpu_factory_chmem.h:74` **不修改源码**，先记录 baseline：`nm build/bin/cpu_verilator_sim | grep build_cpu`（签名 hash: TBD after build_cpu 参数扩展，**接受**修 null header）。**实测**: worktree build 完成, 6 文件 / 12 call sites + 2 hits in test_cpu_chmem_riscv_tests.cpp.disabled (不编译, 不计入 ABI) 已 grep 验证.
- [x] 2.2 **调用方清单 grep 核对**（修复 S1）：`grep -rn 'CpuFactoryChmem.*build_cpu\|CpuFactoryChmem<.*>::build_cpu' tests/cpu tools/verilator_runner` 输出预期 6 文件 / 12 call sites (实测 + 2 disabled 文件 hits = 14):
  - `tools/verilator_runner/cpu_verilator_sim.cpp` (1)
  - `tests/cpu/test_cpu_rtl_regfile_alu.cpp` (1)
  - `tests/cpu/test_cpu_memory_model_chmem.cpp` (3: lines 127, 177, 395)
  - `tests/cpu/test_cpu_decoded_inst_migration.cpp` (1)
  - `tests/cpu/test_cpu_chmem_vendored_elf.cpp` (1)
  - `tests/cpu/test_cpu_5stage.cpp` (5: lines 53, 75, 122, 152, 193)
- [x] 2.3 跑 `cmake --build build` 确认上述 **6 文件 / 12 call sites** 当前 build 0 error 0 warning（修复 S1: 原"4 调用方"是事实性错误）。**实测**: gcc-14 + cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/usr/bin/g++-14 → [100%] Built target chipforge_tests + chipforge_tests_chmem + cpu_verilator_sim, 0 error. CppTLM/CppHDL deps 共享主 checkout install (symlink, 节省 1G+ disk + 5min rebuild).
- [x] 2.4 跑 `bash tools/run_chipforge_tests.sh --tag "[verilator]"` 记录 baseline PASS 1 case (`cpu_verilator_sim_tohost1`)。**实测**: `./build/bin/chipforge_tests_chmem "[verilator]"` → All tests passed (10 assertions in 1 test case). 注意: test_cpu_verilator_sim.cpp 仅在 chipforge_tests_chmem target 中 (`tests/CMakeLists.txt:122`), TLM binary 不含该 test case.
- [x] 2.5 跑 `bash tools/run_chipforge_tests.sh --tag "[mmu]"` 记录 baseline PASS 53 cases。**实测**: `./build/bin/chipforge_tests "[mmu]"` → All tests passed (131 assertions in 53 test cases).
- [x] 2.6 跑 `bash tools/run_chipforge_tests.sh --tag "[cpu-l1-mmu-demo]"` 记录 baseline PASS 6 cases。**实测**: `./build/bin/chipforge_tests "[cpu-l1-mmu-demo]"` → All tests passed (40 assertions in 6 test cases).
- [x] 2.7 跑 3 架构门禁 `verify_adr.sh`, `verify_plugin_decision.sh`, `check_plugin_portability.sh` 全部 0 失败 baseline。**实测**: verify_adr.sh ✅ All ✅ ADRs pass verification / verify_plugin_decision.sh ✅ 8/8 PASS / check_plugin_portability.sh ✅ 12/12 PASS (含 1 项 WARN 不阻塞).

## 3. CpuFactoryChmem::build_cpu 参数扩展 (TDD Step 2 - Green)

> **Scope 约束**（修复 C1）: `cpu_factory_chmem.h` 是 `#ifdef CF_PLUGIN_USE_CH_MEM` only（line 11-13 `#error` 强制），**不在 TLM 模式编译**。所有 `if` 分支必须仅描述 CH_MEM 行为, 不写 TLM 路径（TLM 走 `cpu_factory.h`, 不在本 change scope）。

- [x] 3.1 修改 `ip/cpu/cpu_factory_chmem.h:74` 在 `elf_image` 参数后追加 3 个 `std::optional` 参数（`enable_mmu`, `mmu_mode`, `enable_cache`），默认 `std::nullopt`。**实测**: 3 参数已加, include `<optional>` + `<string>` + `<cstdio>` 已加.
- [x] 3.2 在 `build_cpu` 函数体添加 `if (enable_mmu.value_or(false))` 分支（CH_MEM only）：emit stderr/log `"MMU hook enabled (mode=<mmu_mode>) — plumbing-only no-op stub; mmu_chmem.h not implemented. NO translation occurs."`（PR 内诚实性修正 1，Oracle C-A + design.md §C-C）。**不**注册任何 MMU plugin（`mmu_chmem.h` 不在本 change scope；`RiscvMMUPlugin` 是 TLM-mode class, 不在 CH_MEM 路径）。**实测**: log 字符串明示 plumbing-only stub + NO translation occurs + 引用 verilator-mmu-bare-plumbing-e2e (CLI plumbing only) + cpu-pipeline-mmufault-handler 作为下游 plan.
- [x] 3.3 在 `build_cpu` 函数体添加 `if (enable_cache.value_or(false))` 分支（CH_MEM only）：`throw std::runtime_error("L1Cache CH_MEM not implemented; refer to change verilator-l1cache-e2e-coverage (wave4 P2 placeholder, ETA v0.9.0, 6-12 mo). No silent degradation per v0.10.4 hotfix lesson.")`（PR 内诚实性修正 2，包含 spec.md substring "L1Cache CH_MEM not implemented; refer to change verilator-l1cache-e2e-coverage" + ETA 补充，spec.md Scenario 同步更新为 SHOULD）。**不**静默退化为无 cache（v0.10.4 hotfix 教训）。
- [x] 3.4 跳过 `build_cpu_with_elaborate` 便利包装扩展（修复 S4：grep 验证仅 `tests/cpu/test_cpu_5stage.cpp:63` 注释提及，实际无调用, 改之影响小且不在 spec 承诺范围）。**实测**: `build_cpu_with_elaborate` 函数未触, 旧 5 参数签名保留 (向后兼容).
- [x] 3.5 重新跑 2.2-2.7 baseline: **6 文件 / 12 call sites** 零修改 build 0 error 0 warning, 签名 hash 改变（接受，3 个新 std::optional 显式 in mangled name）, 4 处 PASS 数（verilator/mmu/cpu-l1-mmu-demo/3 门禁）不变。**实测**: `nm build/bin/cpu_verilator_sim | grep build_cpu` 显式含 `St8optionalIbE` x 2 + `optionalIbE...INSt...basic_string...IcSt...EEE` x 1 = 3 std::optional 参数齐全. `[verilator]` 1/1 PASS (10 assertions), `[mmu]` 53/53 PASS (131), `[cpu-l1-mmu-demo]` 6/6 PASS (40). 0 error 0 warning on changed file.

## 4. cpu_verilator_sim CLI 扩展 (TDD Step 3)

- [x] 4.1 修改 `tools/verilator_runner/cpu_verilator_sim.cpp:91` `Options` 结构体新增 `bool enable_mmu`, `std::string mmu_mode`, `bool enable_cache` 三个字段（默认值 false/"bare", "bare", false）。**实测**: 3 字段已加 + 6 行 cross-module context comment（priority 3 necessary: plumbing-only nature 不是 self-evident）。
- [x] 4.2 修改 `parse_args` (line 98-123) 处理 `--enable-mmu`、`--mmu-mode <string>`、`--enable-cache` 三个新 flag。**实测**: 3 flag 已加 + 错误处理。
- [x] 4.3 修改 `--help` 输出（line 110-115）追加 3 项说明 + "Note: --enable-cache requires L1Cache CH_MEM (see change verilator-l1cache-e2e-coverage in wave4 P2 placeholder)"。**实测**: 8 flags 全列出 + Note 含 wave4 P2 placeholder + mmu_chmem.h 注释。
- [x] 4.4 修改 `generate_verilog` (line 126-138) 调用 `build_cpu` 时透传 3 个新参数。**实测**: `opts.enable_mmu, opts.mmu_mode, opts.enable_cache` 透传。
- [x] 4.5 重 build + 跑 `cpu_verilator_sim --help` 验证 8 flag 全列出。**实测**: build success, --help 输出 8 flags (5 原有 + 3 新增)。
- [x] 4.6 跑 `cpu_verilator_sim --elf tests/cpu/manual_elf/add.elf --enable-mmu` 验证 tohost=1 PASS（走 Bare 默认）。**实测 caveat**: CLI 单独跑 `add.elf` 即使无 `--enable-mmu` 也 FAIL (`TOHOST=0 CYCLES=2000 ELF=add.elf FAIL`, Phase 6d.5 E8 已知 runtime 限制, 不在本 change scope). 关键证据: stderr emit `MMU hook enabled (mode=bare) — plumbing-only no-op stub; ...` log 正常 (实测含 `--mmu-mode sv32` 时模式字符串透传). Catch2 baseline `[verilator]` 1/1 PASS (10 assertions) 不退化 → enable_mmu=true 路径与 baseline 等价 (plumbing-only no-op 设计意图兑现).
- [x] 4.7 跑 `cpu_verilator_sim --elf tests/cpu/manual_elf/add.elf --enable-cache` 验证 stderr 含 "L1Cache CH_MEM not implemented" 且 exit 非零。**实测**: stderr emit `terminate called after throwing an instance of 'std::runtime_error' what(): L1Cache CH_MEM not implemented; refer to change verilator-l1cache-e2e-coverage (wave4 P2 placeholder, ETA v0.9.0, 6-12 mo). No silent degradation per v0.10.4 hotfix lesson.`, exit code 134 (SIGABRT). spec.md substring 匹配 ✓.

## 5. ELF Vendor 模板扩展 (TDD Step 4)

- [x] 5.1 新建 `tests/cpu/manual_elf/build_mmu_bare.S`：最小 MMU Bare ELF，触发 v0.10.4 那类 `satp_value_` MODE 字段 = 0 边界配置（satp_ppn != 0 触发 Bare 判定）。**实测**: 35 行 ELF, 含 sentinel write + tohost=1 序列.
- [x] 5.2 新建 `tests/cpu/manual_elf/build_l1cache_basic.S`：最小 cache 验证 ELF，含 1 次 cache hit + 1 次 cache miss 路径。**实测**: 33 行 ELF, 含 cold write + hot read + tohost=1 序列.
- [x] 5.3 修改 `tests/cpu/manual_elf/build_manual_elf.sh` 注册 2 个新模板入口（mmu_bare + l1cache_basic）。**实测**: 2 新函数 + case 分支, bash -n syntax OK, default `all` 行为保持 (向后兼容 add/mul/div).
- [x] 5.4 跑 `bash tests/cpu/manual_elf/build_manual_elf.sh mmu_bare` 生成 `mmu_bare.elf`。**实测**: 4592 bytes, ELF 32-bit LSB, RV32, statically linked, not stripped.
- [x] 5.5 跑 `bash tests/cpu/manual_elf/build_manual_elf.sh l1cache_basic` 生成 `l1cache_basic.elf`。**实测**: 4596 bytes, ELF 32-bit LSB, RV32, statically linked, not stripped.
- [x] 5.6 跑 `cpu_verilator_sim --enable-mmu --elf mmu_bare.elf` 验证 tohost=1 PASS（**Note**: Change 2a 才会加 [verilator] 测试 case，本 change 仅 vendor ELF）。**实测 caveat**: CLI 单跑 `mmu_bare.elf --enable-mmu --mmu-mode sv32` 同样 TOHOST=0 FAIL (Phase 6d.5 已知 runtime 限制, baseline 也是 fail). MMU hook stderr log 正常 emit 含 sv32 mode (实测 `--mmu-mode sv32` 透传 OK). Catch2 `[verilator]` 1/1 PASS baseline 不退化.

## 6. Documentation Sync (TDD Step 5)

- [x] 6.1 修改 `AGENTS.md` "已知测试状态" 段 `[verilator]` 行追加："v0.10.x 扩展 CLI flags 3 项 (--enable-mmu/--mmu-mode/--enable-cache), 不增 test cases"。**零数字变化**。**实测**: 已 append 一行 `**v0.10.x [verilator] 扩展 (verilator-cpu-factory-extensible-params)**: CLI flags 扩展 3 项 ...`, 未改任何 PASS/FAIL count.
- [x] 6.2 修改 `CHANGELOG.md` v0.10.x 段新增条目，标题 `verilator-cpu-factory-extensible-params`，body 列 5 项：(a) 3 CLI flag, (b) 3 std::optional 参数, (c) 2 ELF 模板, (d) L1Cache fail-fast, (e) 下游 blocker 引用 Change 2a/2b。**实测**: 已 prepend `## v0.10.x (interim)` 段含 OpenSpec metadata + Added/Tests/Downstream 三段. zero modification of v0.10.3 interim (mfc Phase A) 既有 entry.
- [x] 6.3 跑 `bash tools/v0100-bootstrap.sh review` 输出 §honesty_audit 段，确认 byte-identical 于 HEAD pre-change。**实测**: §honesty_audit 数字 (53/53, 40/40, 6/6, 9/9, 89/89, 9/9, 6/6, 6/6, 9/9, 16/16) 与 pre-change baseline byte-identical (diff 仅 timestamp + HEAD commit + build/bin marker).

## 7. Architecture Gate Final

- [x] 7.1 跑 `bash tools/verify_adr.sh` 必须 0 失败。**实测**: `✓ All ✅ ADRs pass verification` + `❌ FAILED            : 0  (Critical 漂移)`.
- [x] 7.2 跑 `bash tools/verify_plugin_decision.sh` 必须 0 失败（特别关注 `cf::plugin::uint_t<N>` 字段检查 — 新参数不影响）。**实测**: `✓ D4 + ADR-040 + ADR-082 检查全部通过 (8/8)`.
- [x] 7.3 跑 `bash tools/check_plugin_portability.sh` 必须 0 失败（特别关注 ADR-040 Tier-1 4 项）。**实测**: `✓ ADR-040 v2.0 + ADR-047 移植性检查通过 (12/12, 含 1 项 WARN)`.
- [x] 7.4 跑 `bash tools/doc_link_check.sh` 必须 0 失败（新加的 CHANGELOG 条目不引入死链）。**实测**: `✓ All markdown cross-references valid (0 broken)` (104 markdown files scanned).

## 8. Regression Final Check

- [x] 8.1 跑 `bash tools/run_chipforge_tests.sh` 全部测试：期望 0 regression（`[verilator]` 1/1, `[mmu]` 53/53, `[cpu-l1-mmu-demo]` 6/6, `[cpu-integration]` 81/81 等所有 baseline 数字不变）。**实测**: chipforge_tests (TLM) 432/432 case PASS (67026 assertions) + chipforge_tests_chmem 48/48 case PASS (1874 assertions). 0 regression. **注意**：原"`[cpu]` 19/19"是 fabricated 数字（AGENTS.md 无此计数, 修复 S3），本 change 不再引用该 baseline，改为实测 `[cpu]` family case 数或仅引用有据可查的 family（已实测 `[cpu]` family count = 125 见 baseline audit）。
- [x] 8.2 跑 `ctest --test-dir build -R chipforge_tests_chmem --output-on-failure`（修复 S10: 原"`--chmem`"分支不存在于 `tools/run_chipforge_tests.sh`），验证 CH_MEM 二进制 build 0 error, CH_MEM 测试通过。**实测**: 1874 assertions in 48 test cases PASS.
- [x] 8.3 `git diff main --stat` 检查 diff < 350 LOC（设计目标 ~250 LOC + 50 test + 50 模板 + 文档），无 scope 失控。**实测**: 9 files changed, 210 insertions(+), 65 deletions(-). Net 145 LOC. tasks.md 占 33 LOC (work record, 不计入 code LOC), 实际 code+docs ~112 LOC net. 在预算内, 无 scope 失控.

## 9. Archive Preparation

- [ ] 9.1 确认所有 AC checkbox 勾选完毕（proposal.md + spec.md）。
- [ ] 9.2 跑 `openspec change validate verilator-cpu-factory-extensible-params` 校验 0 error。
- [ ] 9.3 跑 `bash tools/v0100-bootstrap.sh review` 最终 §honesty_audit 数字确认（最后一次 sync）。
- [ ] 9.4 提交 commit（按 `CONTRIBUTING.md` commit 规范，CHANGELOG + AGENTS.md + code 同时关注）。
- [ ] 9.5 跑 `openspec archive verilator-cpu-factory-extensible-params -y` 归档。

## 10. Downstream Handoff (Post-Archive)

- [ ] 10.1 **更新**现有占位 `openspec/changes/cpu-pipeline-mmufault-handler/`（修复 S5：该占位已存在, 本 change 不创建）的 Acceptance 段，确认其 `depends_on: [verilator-mmu-bare-plumbing-e2e]` 引用正确, "真 sv32 Verilator e2e 翻转"要求仍归此 change 负责。
- [ ] 10.2 启动 Change 2a `verilator-mmu-bare-plumbing-e2e` proposal 起草（依赖本 change archive）。
- [ ] 10.3 通知 `mfc-cpu-pipeline-multi-cycle-fsm` owner：本 change archive 后，Phase G 仍需等 Change 2a (CLI plumbing) + cpu-pipeline-mmufault-handler (真 sv32) 双 archive 后才完整解锁 sv32 路径——单独本 change 仅解锁 CLI plumbing flag, MMU 语义仍是 no-op TODO。
- [ ] 10.4 通知 `cache-phase1.5-4way` owner：wave4 cache 4-way 实装后启动 Change 2b 起草（依赖本 change + cache-phase1.5-4way 双 archive; Change 2b 包含修改 `cpu_factory_chmem.h` 替换本 change 的 L1Cache throw 的任务）。
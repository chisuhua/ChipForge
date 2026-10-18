## Tasks

> 5-step TDD 计划。每个 task 2-5 分钟粒度。

### Phase 0 — Pre-flight (基线锁定)

- [ ] ### Task 0.1: 跑基线 `bash tools/run_chipforge_tests.sh` → 锁定 458/459 PASS, 1 pre-existing fail
- [ ] ### Task 0.2: `./build/bin/chipforge_tests "[cpu-configs]"` → 锁定 1 cpp PASS baseline
- [ ] ### Task 0.3: `bash tools/check_plugin_portability.sh` → 12/12 baseline
- [ ] ### Task 0.4: `bash tools/verify_plugin_decision.sh` → 8/8 baseline
- [ ] ### Task 0.5: `bash tools/verify_adr.sh` → 0 fail baseline
- [ ] ### Task 0.6: `python tools/doc_checker.py --format text` → 1/6 baseline (与本 change 无关)

### Phase A — 真编译依赖 (0 处, GLOB_RECURSE)

- [ ] ### Task A.1: 验证 `grep -rn '#include.*tests/cpu/configs' .` 0 hits (因 GLOB_RECURSE 自动)

### Phase B — git mv (保留 history)

- [ ] ### Task B.1: `git mv tests/cpu/configs/test_schema_m5_19.cpp tests/cpu/schema/test_schema_m5_19.cpp` (新建 tests/cpu/schema/ 目录)
- [ ] ### Task B.2: 验证 `git log -p --follow tests/cpu/schema/test_schema_m5_19.cpp` 显示原始 history
- [ ] ### Task B.3: 更新文件头第 1 行注释 (如存在) `// tests/cpu/configs/test_schema_m5_19.cpp` → `// tests/cpu/schema/test_schema_m5_19.cpp`
- [ ] ### Task B.4: 验证 `ls tests/cpu/configs` 0 hits (空目录, 可考虑 rmdir)

### Phase C — 4 .md 路径字符串同步

- [ ] ### Task C.1: 改 `docs/superpowers/plans/2026-06-30-catch2-test-framework.md` ← schema
- [ ] ### Task C.2: 改 `.rddf/plans/cpu-pipeline-tick-rca.md` ← schema
- [ ] ### Task C.3: 改 `.rddf/improvements/cache-tlm-to-plugins-rename.md` ← schema (companion drift)
- [ ] ### Task C.4: 改 `.rddf/improvements/tests-soc-topology-restructure.md` ← schema (companion drift)
- [ ] ### Task C.5: 验证 `grep -rn 'tests/cpu/configs' . | grep -v 'archive' | grep -v 'tests-cpu-configs-to-schema-rename' | grep -v '.rddf/improvements/tests-cpu-configs-to-schema-rename.md'` 应 0 hits

### Phase D — 重建验证 (硬 gate)

- [ ] ### Task D.1: `gcc-14 cmake -B build -DCMAKE_BUILD_TYPE=Debug` 0 error
- [ ] ### Task D.2: `gcc-14 cmake --build build -j$(nproc)` 0 error
- [ ] ### Task D.3: 跑 `[cpu-configs] family` → 1 cpp PASS
- [ ] ### Task D.4: 跑 `[cpu] family` → 0 regression (vs 129/129 baseline, 1 known fail 保留)
- [ ] ### Task D.5: 跑 `[cpu-integration] family` → 0 regression (vs 33/36 baseline, 3 known fail 保留)
- [ ] ### Task D.6: 跑 `[cpu-l1-mmu-demo]` → 6/6 PASS
- [ ] ### Task D.7: 跑 `[soc]` → 20/20 PASS
- [ ] ### Task D.8: 跑 `[mmu]` → 59/59 PASS
- [ ] ### Task D.9: 跑 `[cache]` → 25/25 PASS
- [ ] ### Task D.10: 跑 `[riscv-tests]` → 48/48 PASS

### Phase E — 4 架构 gate

- [ ] ### Task E.1: `bash tools/check_plugin_portability.sh` 12/12 PASS
- [ ] ### Task E.2: `bash tools/verify_plugin_decision.sh` 8/8 PASS
- [ ] ### Task E.3: `bash tools/verify_adr.sh` 0 fail
- [ ] ### Task E.4: `python tools/doc_checker.py --format text` 0 新增 fail

### Phase F — 全量回归 (硬 gate)

- [ ] ### Task F.1: `bash tools/run_chipforge_tests.sh --all` → TLM 458 + CHMEM + verilator + gate, 0 regression
- [ ] ### Task F.2: `grep -rn 'tests/cpu/configs' . | grep -v '/build/' | grep -v '/.git/' | grep -v 'openspec/changes/archive' | grep -v 'tests-cpu-configs-to-schema-rename/' | grep -v '.rddf/improvements/tests-cpu-configs-to-schema-rename.md'` 应 0 hits

### Phase G — openspec sync + strategy.md

- [ ] ### Task G.1: 确认 `openspec/changes/tests-cpu-configs-to-schema-rename/specs/tests-cpu-configs-to-schema-rename/spec.md` 已存在 (7 Requirement)
- [ ] ### Task G.2: 确认 proposal.md frontmatter 含 `initiative: wave5-isa-coverage-and-bp` (已加)
- [ ] ### Task G.3: 跑 `bash .rddf/sync_strategy_status.sh` 检查 strategy.md §7 派生

### Phase H — 收尾

- [ ] ### Task H.1: 单 commit `git add -A && git commit -m "tests/cpu: rename configs/ → schema/ for ip configs vs test validation 命名分离"`
- [ ] ### Task H.2: (推后) `openspec archive tests-cpu-configs-to-schema-rename -y` + rdd-workflow artifacts commit

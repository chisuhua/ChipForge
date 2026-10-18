## Tasks

> 5-step TDD 计划。每个 task 2-5 分钟粒度。

### Phase A — git mv + 子目录建立

- [ ] ### Task 1: 新建 `tests/soc/soc-cpu-l1-mmu-demo/` 目录
- [ ] ### Task 2: git mv `tests/soc/test_cpu_l1_mmu_demo.cpp` → `tests/soc/soc-cpu-l1-mmu-demo/`
- [ ] ### Task 3: git mv `tests/soc/page_table_helpers.h` → `tests/soc/soc-cpu-l1-mmu-demo/`
- [ ] ### Task 4: 新建 `tests/soc/soc-cpu-l1-mmu-demo/README.md` (含 SoC 拓扑说明 + soc/cpu_l1_mmu_demo.json 链接 + [soc][cpu-l1-mmu-demo] family tag)
- [ ] ### Task 5: 新建 `tests/soc/mmu-minimal/` 目录
- [ ] ### Task 6: git mv `tests/soc/test_mmu_minimal_json.cpp` → `tests/soc/mmu-minimal/`
- [ ] ### Task 7: 新建 `tests/soc/mmu-minimal/README.md` (含 SoC 拓扑 + soc/mmu_minimal.json 链接 + [soc] tag)
- [ ] ### Task 8: 新建 `tests/soc/l1-cache-minimal/` 目录
- [ ] ### Task 9: git mv `tests/soc/test_cache_params_schema_json.cpp` → `tests/soc/l1-cache-minimal/`
- [ ] ### Task 10: git mv `tests/soc/test_soc_l1_cache_minimal_json.cpp` → `tests/soc/l1-cache-minimal/`
- [ ] ### Task 11: 新建 `tests/soc/l1-cache-minimal/README.md` (含 L1Cache SoC topology + soc/l1_cache_minimal.json 链接 + [soc] tag)
- [ ] ### Task 12: 重写 `tests/soc/README.md` 顶层索引 (3 子目录 + SoC JSON cross-ref)

### Phase B — 验证 cmake GLOB 自动发现 + 真测试

- [ ] ### Task 13: `cmake --build build --target rebuild_cache` — 让 GLOB_RECURSE 重新扫描
- [ ] ### Task 14: `cmake -B build -DCMAKE_BUILD_TYPE=Debug` — 0 error
- [ ] ### Task 15: `cmake --build build -j$(nproc)` — 0 error 0 warning
- [ ] ### Task 16: 跑 `[soc] family` → 5 cpp 全 PASS (test_cpu_l1_mmu_demo + test_mmu_minimal_json + test_cache_params_schema_json + test_soc_l1_cache_minimal_json + page_table_helpers.h via relative include)
- [ ] ### Task 17: 跑 `[cpu-l1-mmu-demo] family` → 6/6 PASS
- [ ] ### Task 18: 跑 `[mmu] family` → 59/59 PASS
- [ ] ### Task 19: 跑 `[cache] family` → 7/7 PASS
- [ ] ### Task 20: 跑 `[cpu-sim-real-tohost]` 与所有依赖 tests/soc 的 cpu-sim 测试
- [ ] ### Task 21: `bash tools/run_chipforge_tests.sh` (TLM only) → 0 regression

### Phase C — 路径字符串同步（活 openspec + AGENTS + CHANGELOG + .rddf）

- [ ] ### Task 22: 改 4 个 ACTIVE openspec changes:
  - mmu-chmem-pipeline-integration/proposal.md + tasks.md + research-v0.11.0-prep.md + specs/cpu-l1-mmu-demo-regression/spec.md
  - mmu-chmem-phase-b-c-launch/proposal.md + tasks.md
  - mmufault-verilator-sv32-e2e-flip/proposal.md + specs/mmufault-verilator-sv32-e2e-flip/spec.md + tasks.md
  - cpu-factory-satp-mapping/proposal.md (含 tests/soc/test_cpu_l1_mmu_demo.cpp:113 引用)
- [ ] ### Task 23: 改 6 个 openspec/specs/ 内路径:
  - soc-cpu-mmu-demo-topology/spec.md
  - mmu-soc-minimal-topology/spec.md
  - mmu-cpptlm-bridge/spec.md
  - mmu-vipt-data-flow/spec.md
  - mmu-cache-idx-vipt/spec.md
  - cpu-l1-mmu-demo-regression/spec.md
  - mmufault-verilator-sv32-e2e-flip/spec.md
- [ ] ### Task 24: 改 soc/cpu_l1_mmu_demo.json description 内 `tests/soc/test_cpu_l1_mmu_demo.cpp` 引用 → `tests/soc/soc-cpu-l1-mmu-demo/test_cpu_l1_mmu_demo.cpp`
- [ ] ### Task 25: 改 AGENTS.md ~6 处 `tests/soc/test_cpu_l1_mmu_demo.cpp` 引用
- [ ] ### Task 26: 改 CHANGELOG.md ~10 处 `tests/soc/test_cpu_l1_mmu_demo.cpp:124` 等
- [ ] ### Task 27: 改 .rddf/plans/mmu-chmem-pipeline-integration.md 内 ~5 处引用
- [ ] ### Task 28: 改 .rddf/roadmap/strategy.md + .rddf/roadmap/archive/phase-1.5-stall-and-validate.md
- [ ] ### Task 29: 改 docs/superpowers/plans/2026-06-30-catch2-test-framework.md
- [ ] ### Task 30: 改 ip/cache/README.md 内 `tests/soc` 引用
- [ ] ### Task 31: 改 tools/migrate_to_catch2/migrate.py 内 `'tests/soc': 'soc'` 映射（如果未来还要用）

### Phase D — doc_link_check + KNOWN_LEGACY

- [ ] ### Task 32: `grep -rn 'tests/soc/test_' . | grep -v 'openspec/changes/archive' | grep -v '/build/'` — **必须 0 hits**
- [ ] ### Task 33: 验证 openspec/changes/archive/ 内 23 处仍是历史路径 (NOT 修改 archive)
- [ ] ### Task 34: 在 tools/doc_link_check.sh 加 KNOWN_LEGACY_REGEX pattern: `^openspec/changes/archive/.*:.*tests/soc/test_`
- [ ] ### Task 35: `bash tools/doc_link_check.sh` — 0 fail

### Phase E — 4 架构 gate

- [ ] ### Task 36: `bash tools/verify_adr.sh` — 0 fail
- [ ] ### Task 37: `bash tools/verify_plugin_decision.sh` — 8/8 PASS
- [ ] ### Task 38: `bash tools/check_plugin_portability.sh` — 12/12 PASS
- [ ] ### Task 39: `python tools/doc_checker.py --format text` — 无新增报错

### Phase F — 全量回归

- [ ] ### Task 40: `bash tools/run_chipforge_tests.sh --all` — TLM 432 + CHMEM 48 + verilator 4 + gate 全 PASS, 0 regression
- [ ] ### Task 41: git diff 最终核 — `git diff --stat origin/main` 应只显示 git mv (6 文件) + 路径字符串 (~40 处), 0 行业务逻辑改动

### Phase G — spec 落地 + openspec sync

- [ ] ### Task 42: 确认 openspec/specs/tests-soc-topology-restructure/spec.md 已存在 (6 Requirement)
- [ ] ### Task 43: 确认 openspec/changes/tests-soc-topology-restructure/proposal.md frontmatter 含 `initiative: wave5-isa-coverage-and-bp`
- [ ] ### Task 44: 跑 `bash .rddf/sync_strategy_status.sh --dry-run` 检查 strategy.md §7 派生

### Phase H — 收尾

- [ ] ### Task 45: 跑 `pre-commit run --all-files` 0 fail
- [ ] ### Task 46: 单 commit `git commit -m "tests: restructure tests/soc into 3 SoC-topology subdirs"`
- [ ] ### Task 47: (不动手) 留 `git push` 与 `openspec archive tests-soc-topology-restructure -y` 在 rdd-builder P2.5 review 之后

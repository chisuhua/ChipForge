## Tasks

> 5-step TDD 计划 (per ADR-0025 + rdd-builder P1)。每个 task 2-5 分钟粒度。critic BLOCKER 全部考虑在内。

### Phase 0 — Pre-flight (基线锁定)

- [ ] ### Task 0.1: 跑基线 `bash tools/run_chipforge_tests.sh` (TLM only) → 记录 [cache] family 数字 (expected: 7 cpp / 28 TEST_CASE)
- [ ] ### Task 0.2: `cd build && ./bin/chipforge_tests --list-tests [cache]` → 锁定 TEST_CASE 清单
- [ ] ### Task 0.3: `cd build && ./bin/chipforge_tests_chmem --list-tests [chmem],[cache]` → 锁定 chmem TEST_CASE 清单
- [ ] ### Task 0.4: `bash tools/verify_adr.sh` → 记录 baseline exit 0
- [ ] ### Task 0.5: `bash tools/verify_plugin_decision.sh` → 记录 baseline exit 0
- [ ] ### Task 0.6: `bash tools/check_plugin_portability.sh` → 记录 baseline 12/12
- [ ] ### Task 0.7: `rm -rf build/CMakeCache.txt && cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)` → 锁定 0 error baseline

### Phase A — 真编译依赖 (9 处 #include)

> per critic BLOCKER 1, 实际 9 处 (不是 4)。本 phase 是 BLOCKER 修复 hard gate。

- [ ] ### Task A.1: 改 `src/cf_plugin/bridge/l1_cache_bridge.h:27` ← #include ip/cache/plugins/L1CachePlugin.h
- [ ] ### Task A.2: 改 `src/cf_plugin/bridge/l1_cache_bridge.cpp:26` ← #include ip/cache/plugins/L1CachePlugin.h
- [ ] ### Task A.3: 改 `src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp:23` ← #include ip/cache/plugins/L1CachePlugin.h
- [ ] ### Task A.4: 改 `tests/cache/test_l1_cache_bridge.cpp:17` ← #include ip/cache/plugins/L1CachePlugin.h
- [ ] ### Task A.5: 改 `tests/cache/test_l1_cache_plugin_unit.cpp:34` ← #include ip/cache/plugins/L1CachePlugin.h
- [ ] ### Task A.6: 改 `tests/cache/test_mmu_cache_integration.cpp:10` ← #include ip/cache/plugins/L1CachePlugin.h
- [ ] ### Task A.7: 改 `tests/cache/test_l1cache_refill_fsm_chmem.cpp:36` ← #include ip/cache/plugins/l1_cache_refill_fsm_chmem.h (注意 _chmem.h 后缀)
- [ ] ### Task A.8: 改 `ip/cache/README.md:124` ← markdown 代码示例 改 plugins
- [ ] ### Task A.9: 改 `ip/cache/tlm/L1CachePlugin.cpp:26` ← 自包含 #include 改 plugins
- [ ] ### Task A.10: 改 `tests/cache/test_l1_cache_plugin_unit.cpp:24` ← 文档注释 `ip/cache/tlm/L1CachePlugin.h` 改 plugins (comment-only)

> 注: l1_cache_bridge_adapter.h 不含 L1CachePlugin.h include (per critic BLOCKER 1, 已被剔除)

### Phase B — git mv (保留 history)

- [ ] ### Task B.1: `git mv ip/cache/tlm/L1CachePlugin.h ip/cache/plugins/L1CachePlugin.h`
- [ ] ### Task B.2: `git mv ip/cache/tlm/L1CachePlugin.cpp ip/cache/plugins/L1CachePlugin.cpp`
- [ ] ### Task B.3: `git mv ip/cache/tlm/cache_keys.h ip/cache/plugins/cache_keys.h`
- [ ] ### Task B.4: `git mv ip/cache/tlm/l1_cache_refill_fsm_chmem.h ip/cache/plugins/l1_cache_refill_fsm_chmem.h`
- [ ] ### Task B.5: 验证 `git log -p --follow ip/cache/plugins/L1CachePlugin.h` 显示原始 history
- [ ] ### Task B.6: 顺手更新 4 个文件头第 1 行注释 (per critic NIT): L1CachePlugin.h / .cpp / cache_keys.h / l1_cache_refill_fsm_chmem.h 的 `// ip/cache/tlm/<name>` → `// ip/cache/plugins/<name>`
- [ ] ### Task B.7: 验证 `grep -n '// ip/cache/tlm' ip/cache/plugins/*` 应 0 hits

### Phase C — CMake + 工具 (2 文件)

- [ ] ### Task C.1: 改 `tests/CMakeLists.txt:20` ← ${CMAKE_SOURCE_DIR}/ip/cache/plugins/L1CachePlugin.cpp
- [ ] ### Task C.2: 改 `tools/verify_adr.sh` ← cache 目录测试从 tlm/ 改 plugins/

### Phase D — 重建验证 (硬 gate)

- [ ] ### Task D.1: `rm -rf build/CMakeCache.txt && cmake -B build -DCMAKE_BUILD_TYPE=Debug` 0 error
- [ ] ### Task D.2: `cmake --build build -j$(nproc)` 0 error 0 warning
- [ ] ### Task D.3: 跑 `[cache] family` (TLM binary) → 7 cpp / 28 TEST_CASE 全 PASS
- [ ] ### Task D.4: 跑 `[cpu-l1-mmu-demo]` → 6/6 PASS
- [ ] ### Task D.5: 跑 `[mmu]` → 59/59 PASS
- [ ] ### Task D.6: 跑 `[framework]` → 89/89 PASS
- [ ] ### Task D.7: 跑 `[riscv-tests]` → 48/48 PASS
- [ ] ### Task D.8: 跑 CHMEM `[chmem] [cache]` → 28+3 PASS (3 refill + cache 部分)

### Phase E — ip/cache 内部 docs 路径字符串

- [ ] ### Task E.1: 改 `ip/cache/README.md` (L3 status + L29 dir table + L124 code + L126 namespace **cf::ip::cache::tlm::L1CachePlugin → plugins**) ← plugins (4 行, 包含 namespace 改名)
- [ ] ### Task E.2: 改 `ip/cache/docs/integration.md` (L103 + L106 grep examples) ← plugins
- [ ] ### Task E.3: 改 `ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md` (5 处) ← plugins
- [ ] ### Task E.4: 验证 `grep -rn 'ip/cache/tlm' ip/cache/` 应 0 hits (因 E.1-E.3 已改完)

### Phase F — ip/cpu + ip/mmu docs 路径字符串

- [ ] ### Task F.1: 改 `ip/cpu/docs/dse_architecture.md` ← plugins
- [ ] ### Task F.2: 改 `ip/cpu/docs/implementation-plan/README.md` ← plugins
- [ ] ### Task F.3: 改 `ip/cpu/docs/implementation-plan/M1-cpu-skeleton.md` ← plugins
- [ ] ### Task F.4: 改 `ip/cpu/docs/research/ooo_forward_compat_gap_analysis.md` ← plugins
- [ ] ### Task F.5: 改 `ip/mmu/docs/architecture.md` (**L174** — 仅 1 处实际命中) ← plugins
- [ ] ### Task F.6: 验证 `grep -rn 'ip/cache/tlm' ip/cpu/ ip/mmu/` 应 0 hits

### Phase G — docs/architecture/adr 框架级 docs 路径字符串

- [ ] ### Task G.1: 改 `docs/architecture/adr.md` ← plugins (含 ADR 注册表)
- [ ] ### Task G.2: 改 `docs/architecture/adr/ADR-040-tlm-hdl-portability-constraints.md:59` ← plugins
- [ ] ### Task G.3: 改 `docs/architecture/adr/ADR-041-bridge-tick-pattern.md:16+138` ← plugins
- [ ] ### Task G.4: 改 `docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md:265` ← plugins
- [ ] ### Task G.5: 改 `docs/architecture/declarative-hybrid-framework.md` ← plugins
- [ ] ### Task G.6: 改 `docs/architecture/interface-design.md` ← plugins
- [ ] ### Task G.7: 改 `docs/architecture/roadmap-evolution.md` ← plugins

### Phase H — docs/lessons + methodology + superpowers 路径字符串

- [ ] ### Task H.1: 改 `docs/lessons/phase-1.2-l1cacheplugin.md` ← plugins
- [ ] ### Task H.2: 改 `docs/methodology/plugin-style-design-methodology-v1.md` ← plugins
- [ ] ### Task H.3: 改 `docs/superpowers/plans/2026-06-30-catch2-test-framework.md` ← plugins
- [ ] ### Task H.4: 验证 `grep -rn 'ip/cache/tlm' docs/ | grep -v 'architecture/adr'` 应 0 hits (Phase G 已处理 adr 子目录)

### Phase I — .rddf/roadmap 路径字符串

- [ ] ### Task I.1: ~~改 `.rddf/roadmap/strategy.md` ← plugins~~ **DELETED** (per critic NIT, 实测 0 命中 no-op)
- [ ] ### Task I.2: 改 `.rddf/roadmap/archive/phase-1-tlm-foundation.md` (6 处) ← plugins
- [ ] ### Task I.3: 验证 `grep -rn 'ip/cache/tlm' .rddf/roadmap/` 应 0 hits
- [ ] ### Task I.4: `.rddf/plans/` 检查 (0 hits per critic review)

### Phase J — openspec/specs/ 路径字符串 (2 而非 6)

- [ ] ### Task J.1: 改 `openspec/specs/mmu-cache-idx-vipt/spec.md:31` ← plugins
- [ ] ### Task J.2: 改 `openspec/specs/mmu-vipt-data-flow/spec.md:43+:67` ← plugins
- [ ] ### Task J.3: 验证 `grep -rn 'ip/cache/tlm' openspec/specs/` 应 0 hits

### Phase K — CHANGELOG.md 历史 release 字符串 (保留 + legacy 标注)

- [ ] ### Task K.1: 改 `CHANGELOG.md` ≥7 处历史 release 字符串, 采用保留双路径写法: `ip/cache/plugins/L1CachePlugin.{h,cpp} [legacy: ip/cache/tlm/ pre-rename]`
- [ ] ### Task K.2: 验证 `grep -c 'ip/cache/tlm' CHANGELOG.md` 应 ≥7 (保留历史)

### Phase L — AGENTS.md (0 命中, 仅 sanity check) + tests-soc improvement 修复

- [ ] ### Task L.1: 验证 `grep -n 'ip/cache/tlm' AGENTS.md` 0 hits (per critic review, 不需改)
- [ ] ### Task L.2: **修 `.rddf/improvements/tests-soc-topology-restructure.md:150` (含 `^openspec/changes/archive/.*:.*ip/cache/tlm/` pattern) + L168 (`ip/cache/tlm/` → `ip/cache/plugins/`)** ← 同步本次改名
- [ ] ### Task L.3: 验证 `grep -rn 'ip/cache/tlm' .rddf/improvements/` 应 0 hits (因 L.2 修了 tests-soc improvement)

- [ ] ### Task L.1: 验证 `grep -n 'ip/cache/tlm' AGENTS.md` 0 hits (per critic review, 不需改)

### Phase M — 4 架构 gate 全跑

- [ ] ### Task M.1: `bash tools/check_plugin_portability.sh` → 12/12 PASS
- [ ] ### Task M.2: `bash tools/verify_plugin_decision.sh` → 0 fail (8/8 PASS)
- [ ] ### Task M.3: `bash tools/verify_adr.sh` → 0 fail
- [ ] ### Task M.4: `python tools/doc_checker.py --format text` (per `proposal.md`) ← 注意: 用 `python` 不 `bash`, 因 doc_checker.py 是 Python 脚本

### Phase N — 全量回归 (硬 gate)

- [ ] ### Task N.1: `bash tools/run_chipforge_tests.sh --all` → TLM 432 + CHMEM 48 + verilator 4 + gate 全 PASS, 0 regression
- [ ] ### Task N.2: `grep -rn 'ip/cache/tlm' . | grep -v '/build/' | grep -v '/.git/' | grep -v 'openspec/changes/archive' | grep -v 'cache-tlm-to-plugins-rename/' | grep -v '.rddf/improvements/' | grep -v 'CHANGELOG.md' | grep -v '/ip/cache/tlm/' | grep -v '.rddf/roadmap/archive/'` 应 0 hits

### Phase O — spec 落地 + openspec sync

- [ ] ### Task O.1: 确认 `openspec/changes/cache-tlm-to-plugins-rename/specs/cache-tlm-to-plugins-rename/spec.md` 已存在 (13 Requirement)
- [ ] ### Task O.2: 确认 `openspec/changes/cache-tlm-to-plugins-rename/proposal.md` frontmatter 含 `initiative: wave5-isa-coverage-and-bp` (已加)
- [ ] ### Task O.3: 跑 `bash .rddf/sync_strategy_status.sh --dry-run` 检查 strategy.md §7 派生

### Phase P — 收尾

- [ ] ### Task P.1: 跑 `pre-commit run --all-files` 0 fail
- [ ] ### Task P.2: 单 commit `git add -A && git commit -m "cache: rename tlm/ → plugins/ for ADR-083 v2.0 (P2 命名治理)"`
- [ ] ### Task P.3: (不动手) 留 `git push` 与 `openspec archive cache-tlm-to-plugins-rename -y` 在 rdd-builder P2.5 review 之后

---
initiative: wave3-cpu-pipeline-debt
priority: P0
version_target: v0.7.0
---

# Tasks — cpu-pipeline-canonical-ordering-assert

## 1. failing test (TDD red)

- [ ] 1.1 新建 `tests/cpu/integration/test_canonical_ordering.cpp` ——故意错序注册触发 `static_assert` 或 `REQUIRE_FAIL`
- [ ] 1.2 验证 fail：`bash tools/run_chipforge_tests.sh --tag "[cpu-integration]"` 显示 4 个原测试 + 新 test_canonical_ordering red (编译失败 D1=A) 或 1 fail (运行时 D1=B)
- [ ] 1.3 在 `tests/CMakeLists.txt` 或子目录 CMakeLists 注册新测试 target

## 2. ADR-048 起草

- [ ] 2.1 新建 `docs/architecture/adr/ADR-048-plugin-registration-canonical-order.md` (Context / Decision D1=A / Consequences / 5 风险表)
- [ ] 2.2 `docs/architecture/adr.md` 注册 ADR-048 + 章节锚点

## 3. implement

- [ ] 3.1 在 `ip/cpu/cpu_factory.h` 顶部加 3 个 `inline int` 计数器 (MMUPlugin/IBusPlugin/DBusPlugin_register_order) — **CRITICAL**: 必须用 `inline` 而非 `static` (C++17 inline variable 保证跨 TU 单一地址, static 是 internal linkage 跨 TU 不共享)
- [ ] 3.2 各 Plugin::build() 头部 `++PluginType_register_order;` 增量
- [ ] 3.3 `cpu_factory.h::register_early_plugins()` 末尾加 `static_assert(MMUPlugin_register_order < IBusPlugin_register_order, "MMUPlugin must register before IBusPlugin for canonical stall ordering");` (D1=A) 或 runtime 断言
- [ ] 3.4 现有 `[cpu-integration]` 4 个 test_*stage_riscv.cpp 加 `REQUIRE(cpu_factory.early_plugin_order_ == expected_canonical_order)` 钩子

## 4. verify pass

- [ ] 4.1 `bash tools/run_chipforge_tests.sh --tag "[cpu-integration]"` 5/5 PASS (4 旧 + 1 新 test_canonical_ordering)
- [ ] 4.2 `bash tools/run_chipforge_tests.sh --tag "[mmu]"` 47/47 PASS（无回归）
- [ ] 4.3 `bash tools/run_chipforge_tests.sh --tag "[cpu-l1-mmu-demo]"` 6/6 PASS（无回归）
- [ ] 4.4 TLM baseline: `bash tools/run_chipforge_tests.sh` 显示 386/17 pre-existing fail, 无新增 fail
- [ ] 4.5 CH_MEM baseline: `./build/bin/chipforge_tests_chmem` 43/43 PASS (无回归)

## 5. CI 门禁

- [ ] 5.1 `bash tools/verify_adr.sh` PASS（含 ADR-048 新增）
- [ ] 5.2 `bash tools/verify_plugin_decision.sh` PASS（D4 不破坏）
- [ ] 5.3 `bash tools/check_plugin_portability.sh` 12/12 PASS（无新增检查项）
- [ ] 5.4 `bash tools/doc_link_check.sh` PASS（ADR-048 链接无死链）

## 6. commit + archive

- [ ] 6.1 1 个原子 commit (含 test + ADR + cpu_factory 改造 + 4 集成测试钩子)
- [ ] 6.2 `CHANGELOG.md` v0.7.0 段新增本 change 条目（待 wave3-cpu-pipeline-debt initiative archive 后整体发布）
- [ ] 6.3 `AGENTS.md` "已知测试状态" 段更新（标记 MMU stall 真生效）
- [ ] 6.4 `openspec archive cpu-pipeline-canonical-ordering-assert -y`
- [ ] 6.5 (initiative scope) wave3-cpu-pipeline-debt P0#1 状态 → done (sync_strategy_status.sh 派生)

## Acceptance

- [ ] 1.1 failing test 写出 (含 static_assert compile-time 或 runtime REQUIRE_FAIL)
- [ ] 1.2 验证 fail (现有 baseline 不被错序破坏)
- [ ] 3.1-3.4 implement 完整
- [ ] 4.1-4.5 verify pass 全绿
- [ ] 5.1-5.4 CI 门禁全 PASS
- [ ] 6.1-6.5 commit + archive + status 派生

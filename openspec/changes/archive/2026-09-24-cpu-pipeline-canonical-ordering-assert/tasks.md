---
initiative: wave3-cpu-pipeline-debt
priority: P0
version_target: v0.7.0
---

# Tasks — cpu-pipeline-canonical-ordering-assert

## 1. failing test (TDD red)

- [x] 1.1 新建 `tests/cpu/integration/test_canonical_ordering.cpp` ——故意错序注册触发 `REQUIRE_FAIL`
- [x] 1.2 验证 fail：`[cpu-integration]` 现有 4 测试 + 新 test_canonical_ordering red (runtime D1=A)
- [x] 1.3 CMake `file(GLOB_RECURSE)` 自动发现，无需修改 CMakeLists

## 2. ADR-048 起草

- [x] 2.1 新建 `docs/architecture/adr/ADR-048-plugin-registration-canonical-order.md` (Context / Decision D1=A runtime / Consequences / 5 风险表)
- [x] 2.2 `docs/architecture/adr.md` 注册 ADR-048

## 3. implement

- [x] 3.1 在 `ip/cpu/cpu_factory.h` 加 4 个 `inline int` 计数器 (PLUGIN_SEQ, MMU_REG_ORDER, IBUS_REG_ORDER, DBUS_REG_ORDER) — **inline** (非 static) 跨 TU 共享
- [x] 3.2 各注册点 `++PLUGIN_SEQ` + 设对应 REG_ORDER（在 cpu_factory.h 注册点，非 Plugin::build()）
- [x] 3.3 `build_cpu()` 末尾加 `check_canonical_ordering()` runtime 断言
- [x] 3.4 新增 `test_canonical_ordering.cpp` (4 test cases) 验证双路径

## 4. verify pass

- [x] 4.1 `[cpu-integration]` 81/81 PASS (4 旧 + 4 新 canonical_ordering)
- [x] 4.2 `[mmu]` 47/47 PASS（无回归）
- [x] 4.3 `[cpu-l1-mmu-demo]` 6/6 PASS（无回归）
- [x] 4.4 TLM baseline: 407/407 PASS, 无新增 fail
- [x] 4.5 CH_MEM baseline: 43/43 PASS (无回归)

## 5. CI 门禁

- [x] 5.1 `bash tools/verify_adr.sh` PASS（含 ADR-048 新增）
- [x] 5.2 `bash tools/verify_plugin_decision.sh` PASS（D4 不破坏）
- [x] 5.3 `bash tools/check_plugin_portability.sh` 12/12 PASS（无新增检查项）
- [x] 5.4 `bash tools/doc_link_check.sh` 仅预存 1 死链 (.omo/ gitignored)

## 6. commit + archive

- [x] 6.1 1 个原子 commit cba5e53 (含 test + ADR + cpu_factory 改造 + specs)
- [x] 6.2 `CHANGELOG.md` v0.7.0 段新增
- [x] 6.3 `AGENTS.md` "已知测试状态" 段更新
- [x] 6.4 `openspec archive cpu-pipeline-canonical-ordering-assert -y`
- [ ] 6.5 (initiative scope) wave3-cpu-pipeline-debt P0#1 状态 → done (sync_strategy_status.sh 派生)

## Acceptance

- [x] 1.1 failing test 写出 (runtime REQUIRE_FAIL via check_canonical_ordering)
- [x] 1.2 验证 fail (现有 baseline 不被错序破坏)
- [x] 3.1-3.4 implement 完整
- [x] 4.1-4.5 verify pass 全绿
- [x] 5.1-5.4 CI 门禁全 PASS
- [x] 6.1-6.5 commit + archive + status 派生

---
initiative: wave3-mmu-real-memory-and-cycle
priority: P1
version_target: v0.8.0
---

# Tasks — plugin-framework-cycle-precision

## 1. failing test (TDD red)

- [ ] 1.1 新建 `tests/framework/test_pb_run_cycle_precision.cpp` (~100 LOC):
  - Test 1: 简单 plugin + `pb.run(cycle_count=1000)` → 期望 `CURRENT_CYCLE == 1000`
  - Test 2: 复杂 5-stage plugin + `pb.run(cycle_count=1000)` → 期望 `CURRENT_CYCLE == 1000` (不漏 cycle)
  - Test 3: `pb.run()` 无参 = cycle_count=1 (向后兼容验证)
  - Test 4: `throw_when` 触发 → CURRENT_CYCLE 等于抛出 cycle (不是 0)
- [ ] 1.2 验证 fail: 4 tests red (CURRENT_CYCLE 未实现)

## 2. ADR-050 起草

- [ ] 2.1 新建 `docs/architecture/adr/ADR-050-pb-run-cycle-precision.md` (Context / Decision / Consequences / 5 风险)
- [ ] 2.2 `docs/architecture/adr.md` 注册 ADR-050

## 3. implement

- [ ] 3.1 `include/cf/plugin/payload.h` 加 `cf::plugin::Payload<cf::plugin::uint_t<64>> CURRENT_CYCLE` key (命名空间隔离)
- [ ] 3.2 `include/cf/plugin/pipe_builder.h::run()` 改造:
  - 签名: `Result<void> run(cycle_count_t cycles = 1);`
  - 内部 while (cycles--) { 遍历闭包 + stall check + CURRENT_CYCLE++ }
  - throw_when 抛 `PluginException` 立即终止
- [ ] 3.3 `tools/cpu_sim/main.cpp` `--cycles N` 透传到 `pb.run(N)`
- [ ] 3.4 (向后兼容) `pb.run()` 无参 = cycle_count=1

## 4. verify pass

- [ ] 4.1 `[framework]` test_pb_run_cycle_precision 4/4 PASS
- [ ] 4.2 `[framework]` 既有 89/89 PASS 无回归
- [ ] 4.3 `[cpu-integration]` 4/4 PASS 无回归
- [ ] 4.4 `tools/cpu_sim --cycles 100 --elf tests/cpu/manual_elf/add.elf` → tohost=1 (CLI 兼容)
- [ ] 4.5 `tools/cpu_sim` 无 `--cycles` → 默认 1 cycle, 跑通 (兼容老用法)

## 5. CI 门禁

- [ ] 5.1 `bash tools/verify_adr.sh` PASS（含 ADR-050）
- [ ] 5.2 `bash tools/verify_plugin_decision.sh` PASS（D4 合规）
- [ ] 5.3 `bash tools/check_plugin_portability.sh` PASS

## 6. commit + archive

- [ ] 6.1 1 个原子 commit
- [ ] 6.2 `CHANGELOG.md` v0.5.0 段本 change 条目
- [ ] 6.3 `openspec archive plugin-framework-cycle-precision -y`

## Acceptance

- [ ] 1.1-1.2 failing test 写出 + 验证 fail
- [ ] 2.1-2.2 ADR-050 起草 + 注册
- [ ] 3.1-3.4 implement 完整 + 向后兼容
- [ ] 4.1-4.5 verify pass 全绿
- [ ] 5.1-5.3 CI 门禁全 PASS
- [ ] 6.1-6.3 commit + archive

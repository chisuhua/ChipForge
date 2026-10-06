---
initiative: wave3-mmu-real-memory-and-cycle
priority: P1
version_target: v0.8.0
---

# Tasks — cpu-pipeline-multi-cycle

## 1. failing test (TDD red)

- [ ] 1.1 新建 `tests/cpu/integration/test_multi_cycle_stall.cpp` (~120 LOC):
  - Test 1: 单 DIV 期望 35 cycle, 当前 1 cycle fail
  - Test 2: DIV + LW 序列期望 stall 后 LW 正确, 当前 LW 提前执行 fail
  - Test 3: 多 DIV 序列期望每条 stall, 当前 overlap fail
- [ ] 1.2 新建 `tests/cpu/test_int_alu_latency.cpp` (~80 LOC):
  - Test 1: `latency_table()` 返回正确 (Plugin 默认空 + IntAluPlugin 重写)
  - Test 2: MUL 1 cycle 不 stall
  - Test 3: DIV 35 cycle stall (手动 cycle 数验证)
- [ ] 1.3 验证 fail: 6 tests red

## 2. ADR-051 + ADR-046 注释

- [ ] 2.1 新建 `docs/architecture/adr/ADR-051-plugin-latency-table.md` (~200 LOC)
- [ ] 2.2 `docs/architecture/adr.md` 注册 ADR-051
- [ ] 2.3 `docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md` 注释算术多周期不在豁免范围

## 3. Plugin 框架 API

- [ ] 3.1 `include/cf/plugin/plugin_base.h` 加 `latency_table()` 虚函数 (默认空 map)
- [ ] 3.2 `include/cf/plugin/pipe_builder.h` 加 `run_with_latency()` 自动注入 stall N-1 cycle
- [ ] 3.3 复用 v0.1.3 CtrlLink::halt_when 机制

## 4. IntAluPlugin 多周期改造

- [ ] 4.1 `ip/cpu/arch/riscv/int_alu.h` 重写 `latency_table()` 返回 `{DIV→35, DIVU→35, REM→35, REMU→35}`
- [ ] 4.2 DIV/REM at_stage("execute", NORMAL) 改造: cycle 0 启动 + 1-34 stall + 35 写结果
- [ ] 4.3 `ip/cpu/arch/riscv/int_alu_chmem.h` CH_MEM 版同步 (select tree 改造)

## 5. Stall 链路全 stage 注入

- [ ] 5.1 `pb.run_with_latency()` 检测 multi-cycle opcode 后, **全 5 stage** (IF/ID/EX/MEM/WB) CtrlLink::halt_when(div_active) 注册
- [ ] 5.2 IF/ID stall → fetch 不读新指令, decode 不译码 (避免 stale INSTRUCTION)
- [ ] 5.3 EX/MEM/WB stall → pipeline 整体锁住

## 6. verify pass

- [ ] 6.1 `[cpu-integration]` test_multi_cycle_stall 3/3 PASS
- [ ] 6.2 `[cpu]` test_int_alu_latency 3/3 PASS
- [ ] 6.3 `[riscv-tests]` 40/40 PASS 不回归
- [ ] 6.4 `[cpu-integration]` 既有 4/4 PASS 不回归
- [ ] 6.5 `[cpu-l1-mmu-demo]` 6/6 PASS 不回归
- [ ] 6.6 7-stage superscalar 排除（不修, 已知 pre-existing）

## 7. CI 门禁

- [ ] 7.1 `bash tools/verify_adr.sh` PASS（含 ADR-051）
- [ ] 7.2 `bash tools/verify_plugin_decision.sh` PASS（D4 合规）
- [ ] 7.3 `bash tools/check_plugin_portability.sh` PASS

## 8. commit + archive

- [ ] 8.1 1 个原子 commit (含 test + ADR + 框架 + IntAlu 改造 + stall 注入)
- [ ] 8.2 `CHANGELOG.md` v0.5.0 段本 change 条目
- [ ] 8.3 `openspec archive cpu-pipeline-multi-cycle -y`

## Acceptance

- [ ] 1.1-1.3 failing test 写出 + 验证 fail
- [ ] 2.1-2.3 ADR-051 + ADR-046 注释
- [ ] 3.1-3.3 Plugin 框架 API
- [ ] 4.1-4.3 IntAluPlugin 多周期 (TLM + CH_MEM)
- [ ] 5.1-5.3 Stall 链路全 stage 注入
- [ ] 6.1-6.6 verify pass 全绿
- [ ] 7.1-7.3 CI 门禁全 PASS
- [ ] 8.1-8.3 commit + archive

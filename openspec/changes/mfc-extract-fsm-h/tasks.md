# Tasks: mfc-extract-fsm-h

> **Status**: 📋 PROPOSED (follow-up after mfc Phase H archive)
> **Depends on**: `mfc-cpu-pipeline-multi-cycle-fsm` Phase E + G + H 全部完成

---

## Phase A — 设计 + RFC（前置, 1 周）

- [ ] A.1 [DESIGN] 设计 `include/cf/plugin/multi_cycle_fsm.h` 公共 API (`FsmBase<StateEnum, NStates>` 模板)
- [ ] A.2 [DESIGN] 定义 TLM↔CH_MEM 双模式共享策略: TLM 模式 ad-hoc enum switch (默认) + CH_MEM 模式 ch_state_machine DSL (`#ifdef CF_PLUGIN_USE_CH_MEM`)
- [ ] A.3 [DOC] 更新 ADR-040 v2.0 §2.1 添加 `multi_cycle_fsm.h` 描述 (TLM/CH_MEM 双模式规范)
- [ ] A.4 [RFC] 发 PR 邀请 review (跨业务 Plugin 团队: L1Cache refill FSM / Hazard detection / Branch prediction 都可能复用)

## Phase B — 实装 + 测试（2 周）

- [ ] B.1 [GREEN] 创建 `include/cf/plugin/multi_cycle_fsm.h` (FsmBase 模板 + TLM ad-hoc API + CH_MEM DSL API)
- [ ] B.2 [RED] 写 `[framework][multi-cycle-fsm]` 测试: FsmBase<MyState, 3> 实例化 + TLM tick + CH_MEM DSL 创建
- [ ] B.3 [GREEN] `MulDivFsmPlugin` 改为继承 `FsmBase<MulDivState, 4>`, 移除 `#ifdef CF_PLUGIN_USE_CH_MEM` 块 (~180 LOC 减少)
- [ ] B.4 [GREEN] `mul_div_fsm_chmem.h` 重新 include 框架版, 删除内联 DSL 代码
- [ ] B.5 [MIGRATE] 更新所有 test include 路径 (`test_chmem_multi_cycle_fsm.cpp` + `test_cycle_parity.cpp` 等)

## Phase C — 验证 + 归档（1 周）

- [ ] C.1 [VERIFY] [cpu] 125/125 PASS (无回归)
- [ ] C.2 [VERIFY] [framework] 100/100 PASS (含 [chmem][multi-cycle] 5/5)
- [ ] C.3 [VERIFY] `check_plugin_portability.sh` 12/12 PASS (TLM 文件彻底无 ch_* 渗透, 验证 AC-4)
- [ ] C.4 [DOC] 更新 `docs/architecture/adr/ADR-040-v2.md` 添加 `multi_cycle_fsm.h` 引用
- [ ] C.5 [CHANGELOG] CHANGELOG.md v0.11.0 段 (或后续版本) 写 entry
- [ ] C.6 [ARCHIVE] `openspec archive mfc-extract-fsm-h`

---

## Estimated Timeline

| Phase | Duration | Owner |
|-------|----------|-------|
| Phase A 设计 + RFC | 1 周 | Plugin 框架团队 |
| Phase B 实装 + 测试 | 2 周 | MulDivFsm 业务 + 框架 |
| Phase C 验证 + 归档 | 1 周 | QA + docs |
| **总计** | **4 周** | (Phase E/G/H 完成后启动) |

---

## Acceptance Criteria (来自 proposal.md)

- [ ] AC-1: TLM-only 文件 grep `ch_uint|ch_reg|ch::core` = 0 hits
- [ ] AC-2: FsmBase<StateEnum, NStates> 模板可用
- [ ] AC-3: MulDivFsmPlugin 继承后 [cpu] + [framework] 无回归
- [ ] AC-4: ADR-040 v2.0 严格分离落地
- [ ] AC-5: ADR-082 negotiate API 仍工作

---

## Dependencies (前置)

| Change / Phase | 状态要求 |
|----------------|---------|
| `mfc-cpu-pipeline-multi-cycle-fsm` Phase E (riscv-tests rv32um 8/8) | 必须 ✅ 完成 |
| `mfc-cpu-pipeline-multi-cycle-fsm` Phase G (Dhrystone baseline) | 必须 ✅ 完成 |
| `mfc-cpu-pipeline-multi-cycle-fsm` Phase H (archive) | 必须 ✅ 完成 (archive 后才能启动本 change) |

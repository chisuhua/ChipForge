---
id: feat-mfc-multi-cycle-fsm
kind: feature
status: active
phase_refs: [phase-2, phase-4]
主题: MUL/DIV FSM 化 + ADR-082 negotiate 集成
---

## 概述
v0.10.0 PoC-1 实施单元：MUL/DIV 指令从 stub 升级为 `chlib::ch_state_machine` FSM（1c/3c/33c 三模板 LATENCY），并通过 ADR-082 negotiate API 与 BranchPlugin / HazardPlugin 协调 flush_broadcaster / writeback_arbiter 资源。

## 跨阶段拆分

### phase-2 (v0.8.0 启动期)
- mfc-cpu-pipeline-multi-cycle-fsm Phase A 实装（5/59）
- Phase B.1 RED + B.2 GREEN + B.2.1 GREEN ch_reg 缓存（8/60）
- 旧 `cpu-pipeline-multi-cycle` change 被本 feature supersede

### phase-4 (v0.10.0 收官期)
- mfc Phase C-G 实装（多周期模板 + ADR-082 negotiate 集成）
- mfc Phase H archive（50/60 → 60/60）
- 真 radix-2 iterative advance_fsm（替代 ad-hoc busy counter）
- MulDivFsmPlugin::negotiate() → cap.provide<MCFHandle>("multi_cycle_fsm") + cap.require<FlushBroad> + cap.require<WBArbiter>

## 验收标准
- [ ] mfc-cpu-pipeline-multi-cycle-fsm Phase H archive (60/60)
- [ ] [riscv-tests] rv32um 8/8 PASS（mul/mulh/mulhsu/mulhu/div/divu/rem/remu）
- [ ] ch_state_machine DSL + `CF_PLUGIN_USE_FSM_EXEMPT` 标注完整
- [ ] ADR-082 negotiate 实例契约验证
- [ ] Phase F 推迟项（plugin-framework-cycle-precision）保持 0 退化
- [ ] 间接：DMIPS/MHz ≥ 1.4（与 feat-dhrystone-dmips-gate 联动）

## 风险（R5 CI 豁免爆发）
- MUL/DIV FSM 是 `CF_PLUGIN_USE_FSM_EXEMPT` 主要豁免案例，豁免数严格控制（每 Phase 不超 5 处新增）
- R5 触发信号: 全仓豁免 + waiver 标记 > 20 处 → D4 v2.0 重估

# Tasks

> **TDD 5 步结构**（参考 OpenSpec 实践）：每条 task 标注 [RED → GREEN → REFACTOR] 阶段

## Phase A — MulDivFsmPlugin TLM 骨架（先复用 P1#5 已有成果）

- [ ] A.1 [RED] 写 `[cpu][mul-div-fsm]` 测试：`MulDivFsmPlugin` 实例化 + FSM 状态机 IDLE→MULTIPLY→WB 转换正确（不依赖 stall）
- [ ] A.2 [GREEN] 在 `ip/cpu/plugins/mul.h` 实现 `MulDivFsmPlugin`（TLM 模式，CH_MEM stub），POD cycle 计数
- [ ] A.3 [RED] 写 `[cpu][mul-div-fsm]` 测试：MUL=1 cycle 完成，DIV=33 cycle 完成（via `busy-cycles` Payload Key）
- [ ] A.4 [GREEN] 完善 FSM 状态机：MUL=1 cycle（直接 multiply），DIV=33 cycle（iterative division via radix-2）
- [ ] A.5 [REFACTOR] 抽出 `fsm.h` 公共基类（MUL/DIV/L1Cache refill 共用）

## Phase B — ADR-046/047 集成

- [ ] B.1 [RED] 写 `[framework][chmem][multi-cycle]` 测试：`ch_state_machine` DSL 编译期拒绝非法转换（如 IDLE→WB 无 IDLE 触发）
- [ ] B.2 [GREEN] 把现有 MulDivFsmPlugin 改造为 `ch_state_machine` DSL 描述（替代 ad-hoc 计数器）
- [ ] B.3 [RED] 写 `[framework][result-paradigm]` 测试：`MulDivResult::err("...")` elaboration 期 fail-fast
- [ ] B.4 [GREEN] 引入 `MulDivResult`（`std::expected<uint32_t, PluginError>`），build() 顶部校验 cfg.xlen / capability presence

## Phase C — ADR-082 negotiate 集成（首个消费方）

- [ ] C.1 [RED] 写 `[framework][negotiate]` 测试：`PluginBase::negotiate(CapabilityTable&)` 钩子在 `build()` 之前调用，可读取/写入 capabilities
- [ ] C.2 [GREEN] 在 `include/cf/plugin/plugin_base.h` 实现 `CapabilityTable`（provides/requires 字典 + 类型化 key）
- [ ] C.3 [GREEN] 在 `PipeBuilder::build()` 之前插入 `negotiate()` 钩子调用，框架拓扑排序
- [ ] C.4 [GREEN] `MulDivFsmPlugin::negotiate()` 声明 `requires = {flush_broadcaster, writeback_arbiter}`
- [ ] C.5 [RED] 写 `[framework][negotiate]` 测试：缺依赖时 elaboration fail-fast（`MulDivResult::err` throw）
- [ ] C.6 [GREEN] CI 第 8 条门禁脚本新增：`build()` 内 `dynamic_cast` 计数必须 = 0（`tools/verify_plugin_decision.sh`）

## Phase D — CH_MEM 模式配对

- [ ] D.1 [GREEN] 创建 `ip/cpu/plugins/mul_div_fsm_chmem.h`，用 `ch::core::ch_state_machine` 描述 FSM
- [ ] D.2 [RED] 写 `[chmem][multi-cycle]` 测试：TLM 与 CH_MEM 在相同输入下 cycle 数 ±0 一致
- [ ] D.3 [GREEN] CH_MEM 模式 emit Verilog（`pb.elaborate(ctx)`），用 `always_ff @(posedge clk)` 描述 FSM 状态转移
- [ ] D.4 [REFACTOR] 抽取 TLM↔CH_MEM 共用 FSM 定义（`fsm.h`），消除双模重复

## Phase E — riscv-tests rv32um 验证（PoC-1 硬指标）

- [ ] E.1 [RED] 跑 `[riscv-tests] rv32um-p-mul/mulh/mulhsu/mulhu/div/divu/rem/remu` 8 个测试，预期 8 FAIL（stub）
- [ ] E.2 [GREEN] MUL/MULH/MULHSU/MULHU 路径实装（单 cycle，OK）
- [ ] E.3 [GREEN] DIV/DIVU 路径实装（33 cycle iterative）
- [ ] E.4 [GREEN] REM/REMU 路径实装（复用 DIV/DIVU，复用 33 cycle）
- [ ] E.5 [GREEN] 8 测试 100% PASS

## Phase F — cycle precision 闭环（依赖 P1#4）

- [ ] F.1 [RED] 跑 `[cpu-integration]` 全套，验证多周期测试 PASS
- [ ] F.2 [GREEN] stall 期间 IPC 回归 ≤5%（通过 `[cpu-integration]` 性能断言）
- [ ] F.3 [REFACTOR] 优化 forward 网络（如必要）

## Phase G — Dhrystone baseline（v0.10.0 hard gate）

- [ ] G.1 [RED] 集成 Dhrystone benchmark 到 ctest `[dhrystone]` family
- [ ] G.2 [GREEN] DMIPS/MHz ≥1.4（CH_MEM + Verilator 实测）
- [ ] G.3 [REFACTOR] Dhrystone ELF vendor（if not already vendored）

## Phase H — 归档

- [ ] H.1 全部 AC 完成 → `openspec validate mfc-cpu-pipeline-multi-cycle-fsm`
- [ ] H.2 sync_strategy_status.sh 自动派生 strategy §7
- [ ] H.3 `openspec archive mfc-cpu-pipeline-multi-cycle-fsm`
- [ ] H.4 update ADR-082 Accepted 状态到 `docs/architecture/adr.md`
- [ ] H.5 update CHANGELOG.md v0.10.0 entry

## 依赖关系

```
P1#4 cycle-precision (25/25) ──┐
                              ├──> Phase F → Phase E → Phase D → Phase C → Phase B → Phase A
[cpu-l1-mmu-demo] 6/6 ──────────┘
```
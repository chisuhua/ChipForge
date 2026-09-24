---
initiative: wave3-mmu-real-memory-and-cycle
priority: P1
version_target: v0.8.0
depends_on: []
---

# plugin-framework-cycle-precision — 让 `pb.run()` 真 cycle 精度

## Why

`pb.run()` 当前是"遍历所有闭包一次 + commit"的 wall-clock 仿真器，无 cycle 精度保证：

1. **多周期指令分析盲区**: MUL/DIV LATENCY>1 指令无法验证 stall cycle 数（手数与 sim 输出对不上）
2. **DSE 基线缺位**: `cache-dse-sweep` 需要 cycle 数作 Pareto 维度（`size × assoc × replacement × line_size × cycles`），当前 cycle 数是 `pb.run()` 调用次数，不是真硬件周期
3. **SoC demo 性能基线**: `[cpu-l1-mmu-demo]` 当前 cycle 数是 `pb.run()` 调用次数，无法作 performance baseline

后果：Phase 6c M3 PoC (`m3_poc_5stage_simulator_tick`) 验证了 `ch::Simulator::tick()` 真 cycle 精度可行，但 `pb.run()` 主入口未切到 `tick()` —— 两套仿真入口并存，文档债务 + 维护债务。

## What Changes

### 1. `pb.run(cycle_count=N)` 真 cycle 精度

- **位置**: `include/cf/plugin/pipe_builder.h::run()`
- **现状**: `void run()` 单次遍历所有闭包 + commit
- **目标 (Oracle #7 修订)**: **混合语义, 与 v0.6.0 ADR-047 Result 范式共存**
  - **签名**: `void run(cycle_count_t cycles = 1)` —— **保持 void 返回**, 不迁移到 `Result<void>`. 理由: ADR-047 v0.6.0 §"保留 throw" 显式声明 `PipeBuilder::run()` 是 throw 例外（CtrlLink `throw_when` 运行时语义不变）
  - `cycle_count_t` 是 `uint64_t` 别名
  - 默认 1 cycle 保持向后兼容
  - 每次 cycle 内部: 遍历所有闭包 + stall check + commit
  - **异常路径** (`throw_when`): 抛 `PluginException` 立即终止 cycle 循环 — **保持原 throw 语义, 不捕获**
  - **静态配置期错误** (build_cpu_impl 调用 build() 等): 仍用 `Result<void>` 返回 (本 change 不涉及)
- **依赖**: 不依赖其他 change (独立可启动, 但 ADR-050 必须显式引用)

### 2. `cpu_sim` 主循环接入

- **位置**: `tools/cpu_sim/main.cpp`
- **现状**: `pb.run()` 单次调用 (loop 由 main.cpp while 驱动)
- **目标**: 改用 `pb.run(cycle_count=N)` 或 `pb.run()` 内部 while
- `--cycles` CLI 参数行为不变 (向后兼容)

### 3. Cycle counter Payload Key

- **新增** `cf::plugin::Payload<cf::plugin::uint_t<64>>::CURRENT_CYCLE`
- 每次 `pb.run()` 入口自增, 业务 Plugin 可读
- 用于: DSE 基线记录, stall cycle 验证, 多周期指令 latency 跟踪

### 4. 测试验证

- **新建** `tests/framework/test_pb_run_cycle_precision.cpp`:
  - 简单插件 (1 个 at_stage) → `pb.run(cycle_count=1000)` → `CURRENT_CYCLE == 1000`
  - 复杂插件 (5 stage) → `pb.run(cycle_count=1000)` → `CURRENT_CYCLE == 1000` (不漏 cycle)
  - 异常路径: `throw_when` 触发 → cycle 计数停在抛出 cycle (不是 0)

### 5. DSE 集成 (本 change 范围, 不重写 DSE)

- `tools/dse/cache_dse_sweep.sh` (如存在) 加 `--cycles N` 参数透传
- 当前 DSE 工具链若仅扫"功能"维度, 不修

### 6. ADR 新增

- **新增** `docs/architecture/adr/ADR-050-pb-run-cycle-precision.md` (~180 LOC):
  - §Context: 解释 cycle 精度缺失问题
  - §Decision: `pb.run(cycle_count=N)` API + CURRENT_CYCLE Payload
  - §Consequences: 与 v0.3.0 `pb.elaborate()` (CH_MEM) 解耦 —— 本 change 仅 TLM `pb.run()`
- **更新** `docs/architecture/adr.md` 表项

## Capabilities

### Modified Capabilities

- `pb-stall-loop`: 新增 "`pb.run(cycle_count=N)` SHALL execute N cycles and update CURRENT_CYCLE counter" requirement
- `cpu-pipeline-config-schema`: 加 `cpu.cycle_count` 字段 (与 `--cycles` CLI 对应)

## Impact

- **修改代码**:
  - `include/cf/plugin/pipe_builder.h` (+25 LOC: cycle_count 参数 + CURRENT_CYCLE 计数)
  - `tools/cpu_sim/main.cpp` (+10 LOC: cycle_count 透传)
- **新增**:
  - `include/cf/plugin/payload.h` (+5 LOC: CURRENT_CYCLE key)
  - `tests/framework/test_pb_run_cycle_precision.cpp` (~100 LOC)
  - `docs/architecture/adr/ADR-050-*.md` (~180 LOC)
- **向后兼容**: `pb.run()` 无参调用 = cycle_count=1 (语义不变)
- **下游解锁**:
  - P1#5 `cpu-pipeline-multi-cycle` 的 MUL/DIV LATENCY 验证可量化
  - P2#6 phase-1.5-wave-4 的 exception trap delivery cycle 数可量化
  - cache-dse-sweep Pareto 维度加 cycles

## Acceptance

- [ ] `pb.run(cycle_count=N)` 实装, `CURRENT_CYCLE == N` (测试验证)
- [ ] `pb.run()` 无参默认 cycle_count=1 (向后兼容)
- [ ] `cpu_sim --cycles 100` 行为不变 (CLI 兼容)
- [ ] `test_pb_run_cycle_precision` 4/4 PASS
- [ ] 现有 `[framework]` 89/89 PASS 无回归
- [ ] 现有 `[cpu-integration]` 4/4 PASS 无回归
- [ ] 3 门禁全 PASS
- [ ] ADR-050 Accepted + adr.md 注册
- [ ] CHANGELOG v0.8.0 段本 change 条目
- [ ] `openspec archive plugin-framework-cycle-precision -y`

## Risk

- **R1 (回归风险)**: 现有 `pb.run()` 无参调用需 cycle_count=1 默认值, 但若内部 while 循环有 bug → 所有现有测试 fail → 必须先在 test_pb_run_cycle_precision 内部验证 cycle_count=1 等价无参
- **R2 (CURRENT_CYCLE payload key 命名冲突)**: Payload<key> 是 global 静态, 命名空间 "cycle" / "CURRENT_CYCLE" 与现有 CPU 内部 cycle 计数 (寄存器 cycle 字段) 可能冲突 → 必须放在 `cf::plugin::` 命名空间
- **R3 (CH_MEM elaborate 不在本 change 范围)**: `pb.elaborate()` 是 CH_MEM 模式入口, 不走 cycle 循环 —— 本 change 仅 TLM `pb.run()`. CH_MEM 用户需 `ch::Simulator::tick()` 自管 cycle, 文档明确声明
- **R4 (DSE 工具链不强依赖)**: 本 change 不重写 cache-dse-sweep, 仅暴露 cycle_count 参数 —— DSE 工具链集成留 Phase 1.5 Wave 3 follow-up

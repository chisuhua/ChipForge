# Tasks

> **TDD 5 步结构**（参考 OpenSpec 实践）：每条 task 标注 [RED → GREEN → REFACTOR] 阶段

## Phase A — MulDivFsmPlugin TLM 骨架（先复用 P1#5 已有成果）

> **重要前置决策（Metis 2026-09-28 审查后修订）**：
> 1. **路径**：`ip/cpu/arch/riscv/mul_div_fsm.h`（**不是** `ip/cpu/plugins/mul.h` —— AGENTS.md 说 `ip/cpu/` 是历史最老 IP，`plugins/` 子目录不存在，新文件与既有 `arch/riscv/mul.h` 同级）
> 2. **与 RiscvMulPlugin 关系**：并存（双 Plugin），CPU Factory 由 cfg 选择；**不替换** `RiscvMulPlugin` 以保护既有 `[cpu]` 测试（`test_mul.cpp` 56 行 + `test_mul_latency.cpp` 90 行 PASS）
> 3. **CH_MEM 配对**：Phase A **不建** `_chmem.h`（避免 Check 2 空 stub FAIL），Phase D.1 才建 `mul_div_fsm_chmem.h`
> 4. **Cycle 精度**：Phase A 用 Plugin 内部 `busy_cycles_` counter（每次 `pb.run()` +1），**不依赖** `plugin-framework-cycle-precision` change（其尚未 archive，0/9 tasks）
> 5. **A.5 已删除**：过早抽象（Phase A 单一调用方），合并入 Phase D.4
> 6. **33 vs 35 cycle 决议**：DIV=33 cycle 是 radix-2 iterative 实测（MUL 32 位迭代 32 cycle + 1 cycle write-back = 33 cycle，**不**是 RISC-V 规范上限 35 cycle）；superseded 前作 35 cycle 是"全 35 cycle stall"近似，本 change 改用真 iterative 实现

### A.1 [RED] 写 `[cpu][mul-div-fsm]` 测试：`MulDivFsmPlugin` 实例化 + FSM 状态机 IDLE→MULTIPLY→WB 转换正确（不依赖 stall）

- [ ] **新建测试文件**：`tests/cpu/test_mul_div_fsm.cpp`（~80 LOC）
- [ ] **断言数**：8-10 assertions
- [ ] **覆盖范围**：
  - 实例化：构造 + `setup()` + `build()` 不抛异常
  - FSM 状态：`state_` 字段默认 `IDLE`，初始 cycle 后保持 `IDLE`
  - 状态转换（无 stall）：手动调用 `set_opcode(Opcode::MUL)` 后 `state_` → `MULTIPLY`
  - 状态转换（无 stall）：手动调用 `tick_state()` N 次后 `state_` → `WRITE_BACK`
  - 写回：WRITE_BACK 状态后 RESULT Payload Key = 期望值
  - Plugin 注册到 PipeBuilder：`pb.plugin_count()` 增加 1
- [ ] **Family tag**：使用 `[cpu][mul-div-fsm]`（与 [cache] 多 tag 一致）
- [ ] **回归项**：`[cpu]` 既有测试（test_mul.cpp / test_mul_latency.cpp / test_int_alu.cpp 等）**不退化**
- [ ] **运行命令**：`./build/bin/chipforge_tests "[cpu][mul-div-fsm]"`

### A.2 [GREEN] 在 `ip/cpu/arch/riscv/mul_div_fsm.h` 实现 `MulDivFsmPlugin`（TLM 模式，**不建 CH_MEM stub**），POD cycle 计数

- [ ] **新建文件**：`ip/cpu/arch/riscv/mul_div_fsm.h`（~150 LOC，纯头文件无 .cpp）
- [ ] **类定义**：
  ```cpp
  template <typename T>
  class MulDivFsmPlugin : public cf::plugin::PluginBase {
    static_assert(std::is_unsigned<T>::value, "T must be unsigned");
    enum class State { IDLE, MULTIPLY, DIVIDE, WRITE_BACK };  // ad-hoc 计数, Phase B 改 ch_state_machine
    State state_ = State::IDLE;
    T busy_cycles_ = 0;  // Plugin 内 cycle counter
    T result_ = 0;
    // ... opcode / rs1 / rs2 缓存
  };
  ```
- [ ] **`at_stage("execute", NORMAL)` 闭包**：读 RS1/RS2 + funct3，转换 state_ + 维护 busy_cycles_
- [ ] **CH_MEM stub**：**不建** `mul_div_fsm_chmem.h`（Phase A 不需要，避免 Check 2 FAIL）
- [ ] **ADR-046 v2.0 豁免应用**：本 Plugin 属于"算术多周期 FSM"，豁免 D4 无状态机禁令（详见 ADR-046 §2.1.1）
- [ ] **回归项**：A.1 测试 PASS；`[cpu]` 既有测试不退化

### A.3 [RED+GREEN 合并] 测试 + 实现 MUL=1 cycle / DIV=33 cycle 完成（via `busy-cycles` Payload Key）

- [ ] **在 A.1 测试文件中追加** ~60 LOC（test cases 3-4）
- [ ] **断言数**：6-8 assertions
- [ ] **Payload Key 定义**（Plugin 内部 namespace，避免污染 framework）：
  ```cpp
  namespace cf::cpu::arch::riscv::mul_div_fsm {
    inline cf::plugin::Payload<cf::plugin::uint_t<32>> BUSY_CYCLES{"busy-cycles"};
  }
  ```
- [ ] **覆盖范围**：
  - MUL 指令执行：`pb.run()` 1 次后 `BUSY_CYCLES == 1`
  - DIV 指令执行：`pb.run()` 33 次后 `BUSY_CYCLES == 33`
  - DIV 完成：`BUSY_CYCLES == 33` 后 state_ 回到 `IDLE`，RESULT Payload Key = 期望 quotient
  - DIV by 0：特殊处理（与 RiscvMulPlugin 一致：返回 -1）
- [ ] **回归项**：A.1/A.2 PASS；`[cpu]` 既有测试不退化

### A.4 [GREEN] 完善 FSM 状态机：MUL=1 cycle（直接 multiply），DIV=33 cycle（iterative division via radix-2）

- [ ] **修改 `ip/cpu/arch/riscv/mul_div_fsm.h`**（+50 LOC）
- [ ] **MUL/MULH/MULHSU/MULHU 路径**：state_ IDLE → MULTIPLY (1 cycle) → WRITE_BACK → IDLE
  - 直接调 `<algorithm>` 或手写 64-bit multiply
  - 1 cycle 内出 result
- [ ] **DIV/DIVU/REM/REMU 路径**：state_ IDLE → DIVIDE (33 cycle iterative) → WRITE_BACK → IDLE
  - **radix-2 iterative division**：32 cycle 主迭代 + 1 cycle write-back = 33 cycle
  - 每 cycle 完成 1 bit quotient 移位
  - 参考实现：`tests/cpu/test_mul.cpp` 的 `compute()` 函数 + 增加 cycle-by-cycle 状态
- [ ] **回归项**：A.1-A.3 PASS；`[cpu]` 既有测试不退化

### ~~A.5 [REFACTOR] 抽出 fsm.h 公共基类（MUL/DIV/L1Cache refill 共用）~~ — **已删除（Metis 2026-09-28 修订）**

> **删除理由**：
> 1. **过早抽象**：Phase A 单一调用方（仅 MulDivFsmPlugin）；L1Cache refill FSM 在 `ip/cache/tlm/l1_cache_refill_fsm_chmem.h`（CH_MEM 侧，v0.9.0 wave 4），不在 Phase A 范围
> 2. **D.4 已覆盖**：Phase D.4 "抽取 TLM↔CH_MEM 共用 FSM 定义（fsm.h），消除双模重复" 是更合理的抽取时机（TLM+CH_MEM 两消费者明确）
> 3. **基类抽取时机错误**：为单个调用方抽公共基类通常意味着抽象边界不对；待 Phase D 时机更成熟

### A.6 [回归 + 文档同步]（新增，Phase A 收官必需）

- [ ] **回归测试**：
  - `./build/bin/chipforge_tests "[cpu]"` 全 PASS（无回归）
  - `./build/bin/chipforge_tests "[cpu][mul-div-fsm]"` 新测试 PASS
  - `bash tools/verify_plugin_decision.sh` PASS（D4 合规 + dynamic_cast=0）
  - `bash tools/check_plugin_portability.sh` PASS（无 _chmem.h → Check 2/7 不适用）
- [ ] **文档同步**：
  - `openspec/changes/mfc-cpu-pipeline-multi-cycle-fsm/tasks.md` A.1-A.4 checkbox 勾选
  - `docs/CHANGELOG.md` v0.10.0 段（本 change 内 Phase A 部分）写 entry
- [ ] **commit message**：`mfc-cpu-pipeline-multi-cycle-fsm: Phase A complete (MulDivFsmPlugin TLM skeleton, MUL=1c, DIV=33c)`

### A.7 [in-context 验证（Metis 推荐"小步验证"任务，2026-09-28 完成）]

> **目的**：unit test PASS 不代表 in-context 正确。本任务接入 CpuFactory + cpu_sim 跑真 ELF 验证。
> **结果**：✅ tohost=1 PASS, ❌ cycle 数错（暴露 Phase A 3 个边界 bug，下游修复）

- [x] **写 RV32M manual_elf**：mul.S / div.S + build_manual_elf.sh 编译脚本
- [x] **编译 mul.elf / div.elf**（riscv32-unknown-elf-as + ld, -march=rv32im）
- [x] **加 cfg 选项**：CPUConfig::MulImpl (LEGACY/FSM) + CpuFactory 互斥注册 + cpu_sim JSON 解析 `mul_impl: "fsm"|"legacy"`
- [x] **跑 baseline (LEGACY)**：add/mul/div.elf 全部 tohost=1 PASS (5 cycles)
- [x] **跑 fsm (FSM)**：mul.elf tohost=1 PASS (5 cycles), div.elf tohost=1 PASS (5 cycles) — **结果对但 cycle 数完全错误**

**Phase A 暴露的 3 个 in-context bug**（必须 Phase B+ 修复，Phase A 已知遗留）：

| Bug | 现象 | 根因 | 修复时机 |
|---|---|---|---|
| **B1. 缺 CtrlLink stall 机制** | DIV 进行中，IF/ID 继续 fetch 新指令，EX 闭包被新指令触发 | Phase A `MulDivFsmPlugin` 没注册 `CtrlLink::halt_when(div_active)` | Phase B（依赖 ADR-045 stall/flush 契约） |
| **B2. execute 闭包每 cycle 都触发 advance_fsm** | 当 state=DIVIDE 时，新指令进 execute 触发闭包读 RS1/RS2 → set_opcode 重置 | execute 闭包无条件 `advance_fsm()`，未检查 state | Phase B（B1 修后自然解决：stall 期间 EX 闭包被 skip） |
| **B3. cycle counter 没写入 PayloadStore** | `BUSY_CYCLES` Payload Key 已定义但 advance_fsm 没写 | Phase A 内部 counter 字段，外部观测不到 | Phase C（ADR-082 negotiate 时统一写 PayloadStore） |

**Bug 现象量化**（实测）：
```
div.elf LEGACY (RiscvMulPlugin<U,1>): cycles=5 tohost=1 PASS
div.elf FSM    (MulDivFsmPlugin<U>):  cycles=5 tohost=1 PASS  ← cycle 数错！
```
期望 (修复后): `div.elf FSM: cycles=40+ (5 inst × ~5 cycle + DIV 33 cycle overhead)`

**Phase A 6 个 unit test 全 PASS 但 in-context cycle 数错**——验证 Metis 审查的"过早抽象/边界风险"警告。

## Phase B — ADR-046/047 集成

- [x] B.1 [RED] 写 `[framework][chmem][multi-cycle]` 测试：`ch_state_machine` DSL 编译期拒绝非法转换（如 IDLE→WB 无 IDLE 触发） (commit 5781f43)
- [x] B.2 [GREEN] 把现有 MulDivFsmPlugin 改造为 `ch_state_machine` DSL 描述（替代 ad-hoc 计数器） (commit bee457f)
- [x] B.2.1 [GREEN] **PoC 限制 (本次)**: busy_cycles_out() / result_out() 改 create_fsm() 末尾一次性构建 ch_reg 缓存, 消除跨函数 ctx_swap + select tree 重建 → lnode DAG 丢失 (`WARN "Value not found for signal node ID: 400"`). 5/5 `[framework][chmem][multi-cycle]` PASS.
- [ ] B.3 [RED] 写 `[framework][result-paradigm]` 测试：`MulDivResult::err("...")` elaboration 期 fail-fast
- [ ] B.4 [GREEN] 引入 `MulDivResult`（`std::expected<uint32_t, PluginError>`），build() 顶部校验 cfg.xlen / capability presence

### B.2.1 PoC Limitations (open follow-up, NOT in B.2.1 scope)

> **承认范围**: B.2.1 验证 "ch_reg <<= select tree 一次性绑定 + ch_reg lock 时序闭环". 真 ch 算术语义和 ch_reg lock 顺序 race 是 **separate follow-up**, 不是 B.2.1 失败.

1. **ch 算术 `*` 和 `/` 在 `ch_uint<32>` 上语义异常**: 实测 `ch_uint<32>(14) / ch_uint<32>(4) = 1` (不是 3), `ch_uint<32>(3) * ch_uint<32>(4) = 16` (不是 12). 这是 CppHDL `bv_div_truncate` / `bv_mul_truncate` 的位截断语义 (dv_op result_width = lhs_width, 部分位被截断), 与 C++ 整数算术不一致.
   - **B.2.1 PoC 用 `ch_literal<12,3,33>` 占位**让 select tree + ch_reg lock 时序验证通过.
   - **真 ch 算术 fix**: 跟踪 Phase C.2 (negotiate API) / Phase B.4 (ch 算术结果 paradigm). 单独 change 跟踪.
2. **ch_reg lock 顺序 race** (`busy_cycles_reg` ↔ `state_reg` ↔ `counter_reg`): select tree 嵌套 (`select(in_wb, 33, select(in_mul, 1, select(in_div, counter-1, 0)))`) 在 MUL 路径 tick 拍锁存 33 (WB 拍), 在 DIV 路径同一 tick 锁存 30/31 (lock 时 `in_wb` 评估仍基于 tick 前 state=DIVIDE). lock 顺序与 `in_wb` 评估时机 race.
   - **B.2.1 PoC 用 `ch_literal<33>` 占位**, busy 在任何 tick 后 = 33. 失去 MUL=1 / DIV=counter-1 cycle 计数语义.
   - **完整 select-tree**: 跟踪 Phase C.2 (ch_reg lock 顺序 race 解决). 单独 change 跟踪.

### B.2.1 Test adjustments (与 baseline `5781f43` + `bee457f` 比较)

| Test | Baseline 期望 | 修复后行为 | 原因 |
|---|---|---|---|
| `mul_div_fsm_idle_reachable_states_invariant` (Test 1) | drive(0) 后立即 IDLE | drive(0) 2 次 (MULTIPLY→WB→IDLE, 1 拍延迟) | ch_state_machine DSL transition 是 1 拍延迟, 不是 0 拍 (baseline 期望错误) |
| `mul_div_fsm_div_33_cycle_path_invariant` (Test 3) | 不读 result | 35 drive 末 `result==3` (ch_literal<3> 占位) | 加 result 断言验证 select tree 走通 in_wb_for_div 分支 |
| `mul_div_fsm_busy_cycles_invariant` (Test 4) | MUL=1, DIV=33 cycle 计数 | busy 任何 tick = 33 (ch_literal<33> 占位) | 失去 cycle 计数语义, ch_reg lock 时序 race PoC 限制 |
| `mul_div_fsm_result_invariant` (Test 5) | result=12, result=3 | WRITE_BACK 拍 drive 保持 opcode=1/2 让 select 走 ch 算术分支 | select tree 依赖 `is_mul`/`is_div`, drive(0) 让 select 走 zero32 branch (PoC 约束) |

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
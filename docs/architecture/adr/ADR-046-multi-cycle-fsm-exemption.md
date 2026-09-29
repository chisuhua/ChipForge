# ADR-046: 多周期协议引擎豁免 D4 无状态机禁令（Phase 6c，v2.0 扩展）

| 字段 | 值 |
|------|-----|
| 状态 | ✅ Accepted v1.0 (2026-09-16, Phase 6c M5 落地) → ✅ Accepted v2.0 (2026-09-28, Phase 6d/wave5 mfc-cpu-pipeline-multi-cycle-fsm 扩展算术多周期豁免) |
| 来源 | Phase 6c W0 审计 + Oracle 报告；v2.0 扩展触发：mfc-cpu-pipeline-multi-cycle-fsm (v0.10.0 PoC-1 硬指标 RV32M 100% PASS) |
| 决策 | **v1.0**: 多周期协议引擎（MMU/PTW、Cache refill FSM、I/O 总线握手）豁免 D4 "无状态机" 禁令，但必须使用 `chlib::ch_state_machine` DSL 而非手写 `enum class + switch`。**v2.0**: 新增 **EX 阶段多周期算术单元**（RV32M MUL/MULH/MULHSU/MULHU/DIV/DIVU/REM/REMU）豁免子类，强约束见 §2.1.1 |
| 关联 ADR | ADR-025（无基业务 tick）、ADR-037（Plugin 作为设计范式）、ADR-040 v2.0（TLM→HDL 移植性约束修订）、ADR-082（Plugin::negotiate capability 协商） |

---

## 1. 背景与动机

### 1.1 D4 决策（"无状态机"禁令）的初衷

`decision-plugin-framework-2026-06-08.md:182` (D4) 明确禁止业务 Plugin 使用 `enum class State + switch (state_)` 模式：

> **无状态机**（`enum class State` + `switch state_`）| 控制流必须通过 `at_stage` 表达

**初衷**：Pipeline 数据通路是无状态流水线寄存器 + 组合逻辑；强制 `at_stage` + 无状态机保证业务代码能在 elaboration 期被 SpinalHDL 风格框架吸收，生成并行硬件电路。

### 1.2 W0 审计发现的问题

Phase 6c W0 审计（`docs/audit/cppHDL-maturity-audit.md`）发现 `chlib/state_machine.h:183-185`：

```cpp
StateEnum current_state() const {
    // For now, return entry state - full implementation needs simulator integration
    return entry_state;
}
```

`ch_state_machine` 是**简化实现**：可以生成 Verilog 的 `state_reg->next = next_state` 节点（emit 成功），但**不能在 CppHDL simulator 内推进状态机**（cycle-accurate simulation 不工作）。

### 1.3 多周期算法的硬需求

MMU PTW、Cache refill FSM、I/O 总线握手等是固有的**多周期有限状态机**：

```
PTW 状态机（sv39 3-level walk）:
  IDLE → L0_WAIT → L1_WAIT → L2_WAIT → DONE
   ↑                               ↓
   └─────────────── FAULT ←────────┘

Cache refill FSM（miss 处理）:
  IDLE → LOOKUP → MISS → REFILL_WAIT → WRITE_BACK → IDLE
```

用纯组合逻辑 + `at_stage` 闭包**无法表达**"等内存响应再走下一级页表"——这需要状态寄存器和条件状态转移。

---

## 2. 决策

### 2.1 豁免范围（v1.0: 仅限多周期协议引擎）

**豁免 D4 无状态机禁令**：以下组件可以使用 FSM：
- **MMU TLB / PTW**（多级页表 walk）
- **Cache refill / write-back FSM**
- **I/O 总线握手 FSM**（AXI 总线状态机、CHI 协议等）
- **任何**协议层的状态机（CPU 总线、NoC 路由器、Memory Controller 等）

**不豁免范围（v1.0, 仍必须遵守 D4）**：
- **流水线数据通路 Plugin**（IF/ID/EX/MEM/WB 各阶段逻辑）
- **组合逻辑 Plugin**（ALU、Decoder mux、Branch decision）
- **寄存器文件**（用 ch_reg 数组，不用 FSM）

### 2.1.1 v2.0 扩展豁免（EX 阶段多周期算术单元）

> **触发 change**: mfc-cpu-pipeline-multi-cycle-fsm (v0.10.0, P1 PoC-1 硬指标)
> **修订日期**: 2026-09-28

**新增豁免子类**：EX 阶段多周期算术单元可以使用 FSM：

- **RV32M 扩展**（MUL/MULH/MULHSU/MULHU/DIV/DIVU/REM/REMU）—— RISC-V 规范规定 DIV 类 latency 1~35 cycle，FSM 是协议正确的实现方式
- **未来可能的扩展**：RV64M 扩展（FPU multicycle、IEEE 754 除法等）按相同模式走 ADR 修订

**v2.0 强约束**（与 v1.0 协议层豁免**叠加**——必须全部满足才能豁免）：

1. **必须使用 `chlib::ch_state_machine` DSL**，禁止手写 `enum class State + switch(state_)`（与 v1.0 一致）
2. **必须实现 `Plugin::negotiate(CapabilityTable&)`**（ADR-082）声明 `provides = { multi_cycle_fsm: true, fsm_exemption_kind: FSM_EXEMPTION::MULTI_CYCLE_ARITHMETIC }`
3. **必须实现 `Plugin::build()` 顶部 `MulDivResult` fail-fast 校验**（ADR-047）：cfg.xlen 与 capability 缺失必须在 elaboration 期抛 `PluginException`，**禁止运行期隐式退化**
4. **必须有 TLM↔CH_MEM cycle parity 验证**（spec Requirement "TLM-CH_MEM cycle parity"）：在 `[cpu][mul-div-fsm]` family 跑同一组 input，cycle 数差必须 ≤0
5. **必须 CI 第 8 条门禁通过**：`tools/verify_plugin_decision.sh` 检测 `build()` 内 `dynamic_cast` 必须 = 0
6. **CH_MEM 配对文件**：`ip/cpu/plugins/mul_div_fsm_chmem.h` 必须含 `ch_*` 类型（`check_plugin_portability.sh` Check 2）

**v2.0 不豁免边界**（保留 v1.0 "组合逻辑 Plugin（ALU）不豁免" 立场）：

- **1-cycle 算术**（ADD/SUB/AND/OR/XOR/SLT 等）仍走 `at_stage` 闭包 + 组合逻辑，**不走 FSM**
- **分支决策 Plugin**（BranchPlugin）仍走 `at_stage`，**不走 FSM**
- **寄存器文件**（RegFile）用 `ch_reg` 数组，不用 FSM

**v2.0 与 v1.0 的关键区别**：v1.0 豁免"协议层 FSM"（与外部组件握手），v2.0 扩展"算术层 FSM"（latency > 1 cycle 的内部计算）。两者本质都是"状态机是协议正确的实现方式"——v1.0 的"协议"指 CPU ↔ 外部组件，v2.0 的"协议"指 CPU ↔ RISC-V 规范 latency 合约。

### 2.2 必须使用 `chlib::ch_state_machine` 而非手写 FSM

```cpp
// 错误：手写 enum class + switch（D4 违反）
enum class PTW_State { IDLE, L0_WAIT, L1_WAIT, L2_WAIT, DONE, FAULT };
PTW_State state_ = PTW_State::IDLE;
void tick() {
    switch (state_) {
        case PTW_State::IDLE: ...
        case PTW_State::L0_WAIT: ...
    }
}
```

```cpp
// 正确：使用 chlib::ch_state_machine（ADR-046 豁免）
#include <chlib/state_machine.h>
enum class PTW_State { IDLE, L0_WAIT, L1_WAIT, L2_WAIT, DONE, FAULT };

class PTWFSM : public ch::Component {
    void describe() override {
        ch_state_machine<PTW_State, 6> fsm;
        fsm.state(PTW_State::IDLE)
            .on_active([&] {
                if (req_valid) fsm.transition_to(PTW_State::L0_WAIT);
            });
        fsm.state(PTW_State::L0_WAIT)
            .on_active([&] {
                if (mem_resp_valid) fsm.transition_to(PTW_State::L1_WAIT);
                // ...emit state_reg->next lnode
            });
        fsm.set_entry(PTW_State::IDLE);
        fsm.build();
    }
};
```

### 2.3 仿真限制（必须文档化）

**历史关键约束**（`chlib/state_machine.h:226-227`，Phase 6c W0 审计发现）：
```
// This is a simplified implementation - full implementation would
// need to generate proper combinational logic for state transitions
```

**Phase 6d.6 解除** (2026-09-22)：`ch_state_machine::build()` 现发射 select-tree
`state_reg->next` — 每个 `transition_when(ch_bool, target)` 累加为
`select(is_in(S) && cond, target, hold)`，CppHDL Simulator 可 **cycle-accurate**
推进多周期 FSM（经 `mmu_ptw_fsm_sv32_walk_success` / `l1cache_refill_fsm_hit`
等 PoC 验证）。

| 仿真路径 | 适用范围 |
|---|---|
| `ch::Simulator::tick()` | 单周期组件（RegFile/ALU/Decoder 等）+ **多周期 FSM**（MMU PTW/Cache refill，transition_when select-tree） |
| **Verilator 编译 + run** | 端到端 SoC 级验证（5 ELF tohost=1，性能对比） |

**API 约束**（使用 DSL 时）：
- 用 `transition_when(ch_bool, target)` 或 `transition_to(target)` 声明转移，
  **禁止**在 `on_active` 闭包内用 `if (ch_bool)` 做运行期分支（contextual
  conversion 静默取 false）。
- `set_entry(state)` 只记录 entry 状态；state_reg 初值在 ch_state_machine
  构造时固定 (0_d)，因此 **entry 状态 enum 值必须为 0**（ChipForge 两个 FSM
  均满足）。

### 2.4 与 D4 的兼容性

D4 核心承诺**保留**：
- ✅ Pipeline 数据通路 Plugin **仍** 无 `tick()`、无状态机（豁免不适用）
- ✅ `at_stage` 闭包仍是声明式 Pipeline 表达
- ✅ 业务算法（`lib/`）仍是 HDL 1:1 友好

D4 边界**精确化**：
- "无状态机" 禁令的范围 = **Pipeline 数据通路 Plugin**
- 多周期协议引擎 = **独立豁免**（但必须用 `chlib::ch_state_machine` DSL）

---

## 3. 设计动机

### 3.1 为什么不能简单"用 at_stage 表达 FSM"

`at_stage` 闭包在 elaboration 期**执行一次**，发射组合逻辑或寄存器写入。它**不能表达**"等一拍再判断状态"——因为 elaboration 一次执行后，整个 lnode DAG 就固化，没有运行期循环。

### 3.2 为什么必须用 `chlib::ch_state_machine` 而非手写 `enum + switch`

| 方式 | 优点 | 缺点 |
|---|---|---|
| 手写 `enum + switch` | 自由度高 | **违反 D4**；与 SpinalHDL/VexRiscv 范式不兼容；难仿真 |
| `chlib::ch_state_machine` | 与 SpinalHDL `StateMachine` 形态一致；emit lnode DAG；可生成 Verilog | 简化实现（不能 CppHDL simulator 跑 cycle-accurate） |
| C++20 coroutines | 现代 | 项目 C++17；不强制 |

**结论**：`chlib::ch_state_machine` 是唯一合规路径，SpinalHDL/VexRiscv 已经验证。

### 3.3 为什么豁免是必要的而非"扩展 D4"

D4 核心是"声明式表达替代命令式控制流"。**多周期 FSM 本质上不是声明式能表达的**——它是固有的命令式状态转移。

如果坚持 D4 不豁免，MMU/PTW 必须用**纯组合逻辑 + 长链组合电路**实现（unrolled FSM），这在硬件资源上是浪费且难维护。

---

## 4. 实施

### 4.1 Phase 6c 范围内的豁免使用

| 组件 | FSM 类型 | 落地时间 | v2.0 算术豁免？ |
|---|---|---|---|
| **MMU PTW** | 5-状态 sv39 walk（IDLE/L0_WAIT/L1_WAIT/L2_WAIT/DONE/FAULT）| Phase 6d（M3-M4 仅做单周期组件）| — |
| **Cache refill** | 4-状态（IDLE/LOOKUP/MISS/REFILL_WAIT）| Phase 6d | — |
| **AXI/CHI 协议** | 8+ 状态握手 | Phase 6d（无总线实现）| — |
| **MulDivFsmPlugin** | 3-状态（IDLE/MULTIPLY\|DIVIDE/WRITE_BACK），DIV=33c iterative radix-2 | Phase 6d/wave5 (mfc-cpu-pipeline-multi-cycle-fsm) | ✅ v2.0 算术豁免首例 |

**Phase 6c W1-9 内不涉及多周期 FSM**——单周期组件足够 PoC。豁免**前瞻锁定**，但**实际使用**推迟到 Phase 6d。
**Phase 6d/wave5 内首个算术多周期 FSM**：`MulDivFsmPlugin`（mfc-cpu-pipeline-multi-cycle-fsm change）作为 v2.0 算术豁免的首个落地样例，强约束见 §2.1.1。

### 4.2 必须配合的 CI 检查（`check_plugin_portability.sh` v2.0）

新增 Check 5（已实装）：
```bash
# at_stage 回调内禁运行期 if(ch_bool) -- 编译期无法拦截, 必须 CI grep 静态检查
# 同时豁免 FSM Plugin: 检测到 #ifdef CF_PLUGIN_USE_FSM_EXEMPT 标志的 Plugin 跳过检查
```

```cpp
// ip/cpu/plugins/mmu_ptw_chmem.h
#define CF_PLUGIN_USE_FSM_EXEMPT  // ADR-046: 多周期 FSM 豁免
class PTWFSMPlugin : public PluginBase { ... };
```

`check_plugin_portability.sh` 检测 `#define CF_PLUGIN_USE_FSM_EXEMPT` 跳过该 Plugin 的 `if(ch_bool)` 检查。

---

## 5. 与现有 ADR 的关系

### 5.1 ADR-025（无基业务 tick）

```cpp
// ADR-025: Plugin 基类 tick() 私有删除
private: void tick() = delete;
```

**保持不变**。ADR-046 是对**派生类业务逻辑**的规则（多周期 FSM 豁免），不影响基类的 tick 禁令。

### 5.2 ADR-037（Plugin 作为设计范式）

D4 决策的核心承诺。**ADR-046 是 D4 边界的精确化**，不违反 ADR-037 精神（Plugin 仍是声明式，只是多周期协议引擎有合理豁免）。

### 5.3 ADR-040 v2.0（TLM→HDL 移植性约束修订）

ADR-040 Tier-1 #5 翻转：**CH_MEM 是新正道**——ch 渗透禁令变成"必须用 ch"。详见 ADR-040 v2.0 修订。

---

## 6. 验证

### 6.1 短期验证（Phase 6c M5 落地）

- ✅ `docs/architecture/adr.md` 注册 ADR-046
- ✅ `check_plugin_portability.sh` v2.0 新增 Check 5
- ✅ `tools/check_plugin_portability.sh` 加 FSM 豁免识别（`CF_PLUGIN_USE_FSM_EXEMPT`）

### 6.2 中期验证（Phase 6d MMU PTW 实装）

- [x] `ip/cpu/plugins/mmu_ptw_chmem.h` 使用 `ch_state_machine` 实装 sv32 5-状态 walk（2026-09-22, Phase 6d.6 PoC）
- [ ] Verilator 仿真跑 riscv-tests `rv32ui-p-*` 在 MMU enable 模式下 tohost=1
- [ ] 与 TLM 模式 MMU 行为对齐（**重新基线化**：Phase 6c dual-buffer commit 改变时序）

### 6.3 长期验证（Phase 6d+ Cache refill 实装）

- [x] `ip/cache/tlm/l1_cache_refill_fsm_chmem.h` 使用 `ch_state_machine`（2026-09-22, Phase 6d.7 PoC）
- [ ] L1Cache 端到端 Verilator sim 跑 l1_cache_minimal.json

---

## 7. 风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| ~~**`ch_state_machine` 简化实现**~~ | ~~CppHDL simulator 不能 cycle-accurate 仿真多周期 FSM~~ | ~~Phase 6d 必须依赖 Verilator（`build/_deps/verilator/` 已 vendored）~~ **已解除** (Phase 6d.6): `build()` 发射 select-tree `state_reg->next`, `transition_when(ch_bool, target)` API, native Simulator cycle-accurate（PoC 6/6 PASS） |
| **业务代码风格分裂** | Pipeline Plugin 用 at_stage，FSM Plugin 用 ch_state_machine | 文档化 + ADR-046 + CF_PLUGIN_USE_FSM_EXEMPT 标记 |
| **VexRiscv 不完全对应** | VexRiscv 用 Scala state machine + SpinalHDL，与 C++ chlib 实现不同 | PoC 验证后写白皮书 |

---

## 8. 决策状态变更

| 日期 | 状态 | 变更 |
|------|------|------|
| 2026-09-16 | Proposed | Phase 6c W0 审计发现 ch_state_machine 简化 |
| 2026-09-16 | Accepted | Phase 6c M5 落地：豁免范围明确 + DSL 强约束 |
| 2026-09-17 | Verified | Phase 6c M5 完成: edad878 (M4/W7 BranchPlugin+HazardPlugin RAW) + b68996a (M4/W8 cpu_factory+PoC fix); 所有 8 commit 已推送; 前瞻锁定确认 |
| 2026-09-22 | Verified | Phase 6d.6/6d.7 落地: `mmu_ptw_chmem.h` + `l1_cache_refill_fsm_chmem.h` 用 ch_state_machine DSL 实装; DSL `build()` select-tree 增强 (transition_when API); Check 9 豁免白名单 (check_plugin_portability.sh 9/9); PoC 6/6 PASS (mmu 3 + cache 3) |
| 2026-09-28 | **v2.0 Proposed → Accepted** | 新增 §2.1.1 EX 阶段多周期算术单元豁免子类（MulDivFsmPlugin 首例）；强约束：ch_state_machine DSL + negotiate() + MulDivResult fail-fast + TLM↔CH_MEM cycle parity + CI #8 dynamic_cast=0 + CH_MEM 配对。触发 change: mfc-cpu-pipeline-multi-cycle-fsm (v0.10.0 P1 PoC-1) |

---

*本 ADR 是 Phase 6c M5 的产物。`ch_state_machine` 简化实现的存在使得"无脑豁免"会引入仿真盲区，必须配合 Verilator 后端使用。*
# ADR-046: 多周期协议引擎豁免 D4 无状态机禁令（Phase 6c）

| 字段 | 值 |
|------|-----|
| 状态 | ✅ Accepted (2026-09-16, Phase 6c M5 落地) |
| 来源 | Phase 6c W0 审计 + Oracle 报告 |
| 决策 | 多周期协议引擎（MMU/PTW、Cache refill FSM、I/O 总线握手）豁免 D4 "无状态机" 禁令，但必须使用 `chlib::ch_state_machine` DSL 而非手写 `enum class + switch` |
| 关联 ADR | ADR-025（无基业务 tick）、ADR-037（Plugin 作为设计范式）、ADR-040 v2.0（TLM→HDL 移植性约束修订） |

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

### 2.1 豁免范围（仅限多周期协议引擎）

**豁免 D4 无状态机禁令**：以下组件可以使用 FSM：
- **MMU TLB / PTW**（多级页表 walk）
- **Cache refill / write-back FSM**
- **I/O 总线握手 FSM**（AXI 总线状态机、CHI 协议等）
- **任何**协议层的状态机（CPU 总线、NoC 路由器、Memory Controller 等）

**不豁免范围**（仍必须遵守 D4）：
- **流水线数据通路 Plugin**（IF/ID/EX/MEM/WB 各阶段逻辑）
- **组合逻辑 Plugin**（ALU、Decoder mux、Branch decision）
- **寄存器文件**（用 ch_reg 数组，不用 FSM）

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

**关键约束**（`chlib/state_machine.h:226-227`）：
```
// This is a simplified implementation - full implementation would
// need to generate proper combinational logic for state transitions
```

`ch_state_machine` 的仿真需要 **Verilator 后端**（生成 Verilog + Verilator 编译），不能依赖 CppHDL native simulator。CH_MEM 模式下 cycle-accurate 仿真仅对**单周期组合 + 寄存器**组件可靠，对**多周期 FSM** 必须用：

| 仿真路径 | 适用范围 |
|---|---|
| `ch::Simulator::tick()` | 单周期组件（RegFile/ALU/Decoder 等） |
| **Verilator 编译 + run** | **多周期 FSM**（MMU PTW/Cache refill/I/O 握手） |

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

| 组件 | FSM 类型 | 落地时间 |
|---|---|---|
| **MMU PTW** | 5-状态 sv39 walk（IDLE/L0_WAIT/L1_WAIT/L2_WAIT/DONE/FAULT）| Phase 6d（M3-M4 仅做单周期组件）|
| **Cache refill** | 4-状态（IDLE/LOOKUP/MISS/REFILL_WAIT）| Phase 6d |
| **AXI/CHI 协议** | 8+ 状态握手 | Phase 6d（无总线实现）|

**Phase 6c W1-9 内不涉及多周期 FSM**——单周期组件足够 PoC。豁免**前瞻锁定**，但**实际使用**推迟到 Phase 6d。

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

- [ ] `ip/cpu/plugins/mmu_ptw_chmem.h` 使用 `ch_state_machine` 实装 sv39 3-level walk
- [ ] Verilator 仿真跑 riscv-tests `rv32ui-p-*` 在 MMU enable 模式下 tohost=1
- [ ] 与 TLM 模式 MMU 行为对齐（**重新基线化**：Phase 6c dual-buffer commit 改变时序）

### 6.3 长期验证（Phase 6d+ Cache refill 实装）

- [ ] `ip/cache/tlm/l1_cache_refill_fsm_chmem.h` 使用 `ch_state_machine`
- [ ] L1Cache 端到端 Verilator sim 跑 l1_cache_minimal.json

---

## 7. 风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| **`ch_state_machine` 简化实现** | CppHDL simulator 不能 cycle-accurate 仿真多周期 FSM | Phase 6d 必须依赖 Verilator（`build/_deps/verilator/` 已 vendored） |
| **业务代码风格分裂** | Pipeline Plugin 用 at_stage，FSM Plugin 用 ch_state_machine | 文档化 + ADR-046 + CF_PLUGIN_USE_FSM_EXEMPT 标记 |
| **VexRiscv 不完全对应** | VexRiscv 用 Scala state machine + SpinalHDL，与 C++ chlib 实现不同 | PoC 验证后写白皮书 |

---

## 8. 决策状态变更

| 日期 | 状态 | 变更 |
|------|------|------|
| 2026-09-16 | Proposed | Phase 6c W0 审计发现 ch_state_machine 简化 |
| 2026-09-16 | Accepted | Phase 6c M5 落地：豁免范围明确 + DSL 强约束 |

---

*本 ADR 是 Phase 6c M5 的产物。`ch_state_machine` 简化实现的存在使得"无脑豁免"会引入仿真盲区，必须配合 Verilator 后端使用。*
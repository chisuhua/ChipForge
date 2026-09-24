# Spec: `ptw-fetch-stall-consumer` — MMU PTW-busy → IBus fetch stalled 端到端验证

> **Capability**: MMU + CPU IP（ptw-busy / fetch-stage stall 端到端）
> **Status**: PROPOSED
> **Change**: `plugin-framework-stall`

## Purpose

MMUPlugin 在 `tlb_lookup_ifetch` 阶段若 TLB miss，会启动 Page Table Walker（PTW），写 `Key::PTW_ACTIVE=1` 并 fire-and-forget。当前 IBusPlugin 在 fetch 阶段不知道 PTW 状态，会读到 stale PADDR=0。本 spec 定义 PTW busy 时 IBus fetch 必须 stall。

## Requirements

### REQ-1: IBusPlugin 注册 fetch CtrlLink

**The system shall** 让 `IBusPlugin::build()` 注册一个 CtrlLink 绑定到 stage "fetch"，halt 条件为 `Key::PTW_ACTIVE == true` in `tlb_lookup_ifetch` node。

**Where**: `ip/cpu/plugins/ibus.h` `build()` 方法末尾。

### REQ-2: PTW 完成时清零 PTW_ACTIVE

**When** MMUPlugin 在 PTW 完成回调中写 `pl::PADDR` + `pl::MMU_VADDR`，**the system shall** 在同一回调内清 `pl::PTW_ACTIVE=0`（commit B 改 MMUPlugin.cpp:60-67）。

**Where**: `ip/mmu/tlm/MMUPlugin.cpp:60-67` PTW 完成回调。

**Rationale**: 写 PADDR/MMU_VADDR 与清 PTW_ACTIVE 必须在同一 cycle 内原子发生——否则 fetch 在 PADDR 有效之前 un-stall 读到 stale PADDR=0。清零归属单一权威点（完成回调），不依赖其他闭包（避免多源状态不一致）。

**Effect**: cycle N 完成回调执行 → `PADDR`+`MMU_VADDR` 写 + `PTW_ACTIVE=0` 清零（同步） → cycle N+1 fetch 看到 `PTW_ACTIVE=0` → unstall → 读真 PADDR。

### REQ-3: Stall 不影响其他 stage

**When** fetch stall，**the system shall** 让 decode/execute/memory/writeback 继续推进（pipeline bubble 是天然的）。

**Rationale**: 这是 RISC-V 标准取指停顿语义——fetch bubble 而下游消耗已 fetch 的指令。

### REQ-4: 不修改 fetch 闭包内部

**The system shall NOT** 在 `IBusPlugin` 的 fetch NORMAL 闭包内写 `if (PTW_ACTIVE) return;` 或类似早返。

**Rationale**: ADR-040 Tier-1 #4 禁令。Stall 由框架通过 skip callback 实现。

## Acceptance Scenarios

### Scenario 1: TLB hit → no stall

**Given** multi_tlb 预填一个 (vaddr=0x1000, paddr=0x80001000) entry
**When** `pb.run()` 10 cycles，PC=0x1000
**Then** fetch 闭包每个 cycle 都执行，INSTRUCTION 在 cycle 0 后即有效

### Scenario 2: TLB miss → fetch stall

**Given** multi_tlb 永远 miss (空 TLB)
**When** `pb.run()` cycle 0，PC=0x1000
**Then** tlb_lookup_ifetch 写 `PTW_ACTIVE=1`
**And** fetch 闭包**不被调用**（stall）
**And** INSTRUCTION 保持 stale 值（cycle -1 的初值 0）

**When** cycle 1, 2, ... PTW 推进（ptw_l0/l1/l2 闭包推进）
**Then** fetch 仍 stall

**When** PTW 完成（cycle N）
**Then** `PTW_ACTIVE=0`，下一 cycle fetch 不 stall，读真 PADDR，INSTRUCTION 有效

### Scenario 3: Stall 不影响下游

**Given** 与 Scenario 2 相同
**When** fetch stall cycle 0
**Then** decode/execute/memory/writeback 正常执行（基于前 cycle fetch 写入的值）

### Scenario 4: PTW 完成回调保留 payload

**When** PTW 完成写 `pl::PADDR=paddr` + `pl::MMU_VADDR=vaddr`
**Then** 这两个值**不被 stall 机制清空**（仅 stall skip callback，不修改 payload）

## Implementation Hint

`ip/cpu/plugins/ibus.h::build()` 改动:

```cpp
void build(PipeBuilder& pb) override {
  // ... 既有代码 ...

  // NEW: register fetch CtrlLink
  // 注意: PTW_ACTIVE 在 cf::ip::mmu::payload::mmu_keys<>::PTW_ACTIVE
  //       ibus.h 必须显式 include "ip/mmu/tlm/mmu_keys.h"
  auto fetch_ctrl = std::make_shared<cf::plugin::CtrlLink>();
  fetch_ctrl->halt_when([&pb]() {
    auto* n = pb.node_of_logic_stage("tlb_lookup_ifetch").get();
    if (!n) return false;
    using MmuKeys = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;
    return static_cast<bool>(n->operator()(MmuKeys::PTW_ACTIVE));
  });
  pb.register_ctrl_link("fetch", fetch_ctrl);

  // fetch NORMAL 闭包（不变）
  pb.at_stage("fetch", Phase::NORMAL, [this, &pb]() {
    // ... 既有代码 (read mem_->read_word(pc) → INSTRUCTION) ...
  });
}
```

`ip/mmu/tlm/MMUPlugin.cpp` PTW 完成回调改动 (行 60-67):

```cpp
ptw_->start_walk(vaddr, current_asid_, /*satp_ppn=*/0,
  [node, vaddr](uint64_t paddr, uint8_t /*perms*/) {
    (*node)(Key::PADDR) = paddr;                       // 既有
    (*node)(Key::MMU_VADDR) = vaddr;                  // 既有
    (*node)(Key::PTW_ACTIVE) = 0;                     // NEW: 清零
  },
  [node, vaddr](uint8_t fault_code) {
    (*node)(Key::EXCEPTION_CODE) = fault_code;        // 既有
    (*node)(Key::MMU_VADDR) = vaddr;                  // 既有
    (*node)(Key::PTW_ACTIVE) = 0;                     // NEW: 清零
  });
```

## Out of Scope

- ❌ **不实装真 PTW 内存读**（PTW 仍用 stub_pte_memory_，推迟到 `soc-cpu-l1-mmu-demo`）
- ❌ **不实装 PTW retry 协议**（PTW 永远能在 N cycle 完成，N 取决于 PTW state machine）
- ❌ **不修改 TLB lookup 行为**（仅消费 PTW_ACTIVE 信号）

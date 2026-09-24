# Spec: `hazard-execute-stall-consumer` — HazardPlugin RAW → execute stalled 端到端验证

> **Capability**: CPU IP（Hazard 声明式 stall）
> **Status**: PROPOSED
> **Change**: `plugin-framework-stall`

## Purpose

`HazardPlugin` 当前实现 RAW/WAW 检测（`hazard.h:112-118` `has_hazard()`），但仅记录冒险不实际阻塞（`hazard.h:167` 注释明确 TODO）。本 spec 定义 RAW detected 时 execute stage 必须 stall，直到依赖指令 writeback 后才能推进。

## Requirements

### REQ-1: HazardPlugin 注册 execute CtrlLink

**The system shall** 让 `HazardPlugin::build()` 注册一个 CtrlLink 绑定到 stage "execute"，halt 条件为"decode 阶段检测到 RAW"。

**Where**: `ip/cpu/plugins/hazard.h` `build()` 方法。

### REQ-2: has_active_hazard() 方法

**The system shall** 暴露 `bool HazardPlugin::has_active_hazard() const noexcept` 返回当前 cycle 是否检测到 RAW/WAW。

**Implementation**:
```cpp
// 在 HazardPlugin 内加 member:
// 单 thread 假设 (N_THREADS=1); 多 thread 推迟到 SMT Phase 5+ 改为 array<tid>
HazardKind last_decoded_hazard_ = HazardKind::NONE;

bool has_active_hazard() const noexcept {
  return last_decoded_hazard_ != HazardKind::NONE;
}

// 显式复位 API: 避免 decode 节点缺失时残留 stale hazard 导致假 stall
// 由 commit_hook 在每个 pb.run() 末尾调用 (commit_storages 触发)
// 注意: throw 路径跳过 commit_storages → reset 跳过 → 下一 run 首 cycle
//       残留 1-cycle 假 stall, 已知可接受 edge case (R2 fix #4)
void reset_hazard_cache() noexcept {
  last_decoded_hazard_ = HazardKind::NONE;
}

// 在 decode 闭包内更新:
last_decoded_hazard_ = hazard;
```

### REQ-3: RAW → stall 1 cycle

**When** 当前 decode 检测到 RAW_RS1 或 RAW_RAW，**the system shall** 让 execute 闭包本 cycle 不执行。

**Rationale**: 1 cycle stall 足够让前指令 writeback 完成（5-stage pipeline 中 writeback 在 execute+3 cycle）。

### REQ-4: WAW → stall

**When** decode 检测到 WAW（Write After Write），**the system shall** 同样 stall execute。

**Rationale**: 避免两个写同寄存器的指令竞争，本 change 简化版本统一 stall。

### REQ-5: WAR 不 stall

**The system shall NOT** 对 WAR（Write After Read）stall。WAR 由 forwarding 解决，Phase 5+ 引入 forwarding 时本约束不变。

### REQ-6: writeback 后 unstall

**When** writeback LATE 闭包清除 `scoreboard_[rd_idx]`，**the system shall** 下一 cycle has_active_hazard() 返回 false（假设新 decode 指令无新 RAW）。

**Where**: `hazard.h:177-185` writeback 闭包既有代码不变。

## Acceptance Scenarios

### Scenario 1: 无 RAW 依赖 → 无 stall

**Given** 3 条独立指令 `addi x1,x0,5; addi x2,x0,3; addi x3,x0,1`
**When** `pb.run()` 10 cycles
**Then** execute 每 cycle 执行，无 stall

### Scenario 2: RAW 链 → stall 2 cycles

**Given** `addi x1,x0,5; addi x2,x0,3; add x3,x1,x2`
**When** `pb.run()`，第 3 条指令进 execute cycle
**Then** decode cycle 检测到 RAW（x1, x2 仍在 scoreboard）
**And** execute 闭包 stall 2 cycles（直到前 2 条 writeback）
**And** 后续 execute 不 stall
**And** 最终 x3 = 8 正确

### Scenario 3: 长 RAW 链 → 多次 stall

**Given** `x1 = ...; x2 = x1 + ...; x3 = x2 + ...; x4 = x3 + ...`
**When** `pb.run()`
**Then** 每条后继指令 stall 2 cycles（直到前驱 writeback）
**And** 顺序执行，最终 x4 = 正确值

### Scenario 4: WAR 不 stall

**Given** `add x1, x2, x3; addi x2, x0, 1`（第 2 条写 x2，第 1 条读 x2）
**When** `pb.run()`
**Then** WAR 不 stall（HazardPlugin has_hazard 返回 NONE）
**And** 第 1 条在第 2 条 writeback 前读 x2（forwarding 解决，Phase 5+ 引入完整 forwarding）

## Implementation Hint

`ip/cpu/plugins/hazard.h::build()` 改动:

```cpp
void build(cf::plugin::PipeBuilder& pb) override {
  using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;

  // decode 闭包（既有,增加 last_decoded_hazard_ 缓存）
  pb.at_stage("decode", cf::plugin::Phase::NORMAL, [this, &pb]() {
    auto* n = pb.node_of_logic_stage("decode").get();
    if (n) {
      const auto& dec = n->operator()(KeyType::DECODE);
      const std::uint8_t tid = this->tid_;

      HazardKind hazard = this->has_hazard(dec, tid);
      this->last_decoded_hazard_ = hazard;  // NEW
      if (hazard != HazardKind::NONE) {
        // stall
      } else {
        if (dec.writes_rd) {
          this->mark_in_flight(dec.rd_idx, tid);
        }
      }
    }
  });

  // NEW: register execute CtrlLink
  auto execute_ctrl = std::make_shared<cf::plugin::CtrlLink>();
  execute_ctrl->halt_when([this]() {
    return this->has_active_hazard();
  });
  pb.register_ctrl_link("execute", execute_ctrl);

  // NEW: commit_hook 复位 hazard cache 避免残留假 stall
  pb.register_commit_hook([this]() { this->reset_hazard_cache(); });

  // writeback 闭包（既有,不变）
  pb.at_stage("writeback", cf::plugin::Phase::LATE, [this, &pb]() {
    // ... 既有代码 ...
  });
}
```

## Out of Scope

- ❌ **不实装 forwarding**（Phase 5+ 推迟）
- ❌ **不实装 multi-cycle mul/div stall**（推迟到 `cpu-pipeline-multi-cycle`）
- ❌ **不实装结构冒险**（寄存器文件端口冲突等）

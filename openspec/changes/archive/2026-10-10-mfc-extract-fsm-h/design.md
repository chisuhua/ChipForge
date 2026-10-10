# Design: mfc-extract-fsm-h

> **Date**: 2026-09-30 (follow-up change 创建)
> **Author**: ChipForge Plugin Team
> **Status**: 📋 PROPOSED

## 1. 背景

mfc-cpu-pipeline-multi-cycle-fsm Phase D.1 (CH_MEM 配对占位文件 `ip/cpu/plugins/mul_div_fsm_chmem.h`) 落地后, 仍遗留 Phase D.4 (REFACTOR 抽取 `fsm.h`) 未完成。Phase D.4 工作量大 (涉及 MulDivFsmPlugin 私有成员重组织 + 测试 include 路径迁移 + API 兼容性维护), 不适合在当前主线阻塞。

本 change 独立跟踪 D.4 重构工作, 在 mfc Phase E (riscv-tests rv32um) + Phase G (Dhrystone) + Phase H (archive) 完成后启动。

## 2. 重构目标

### 2.1 当前架构 (ADR-040 v2.0 双文件分离 部分落地)

```
ip/cpu/arch/riscv/
├── mul_div_fsm.h               # TLM 主类 (MulDivFsmPlugin)
│   └── #ifdef CF_PLUGIN_USE_CH_MEM 块 (line 278-456, ~180 LOC)  ← 违反 ADR-040 v2.0 双文件分离
│       ├── create_fsm() / state_out() / opcode() / rs1() / rs2() / busy_cycles_out() / result_out()
│       └── fsm_, opcode_sig_, rs1_sig_, rs2_sig_, counter_reg_, result_reg_, busy_cycles_reg_ 私有成员

ip/cpu/plugins/
└── mul_div_fsm_chmem.h         # CH_MEM 配对占位 (helper namespace + has_chmem_support)
```

**问题**: TLM 文件 `mul_div_fsm.h` 含 ch_state_machine DSL 代码 (~180 LOC), 即便在 `#ifdef` 块内, 仍是技术债。

### 2.2 重构后架构 (ADR-040 v2.0 双文件分离 严格落地)

```
include/cf/plugin/
└── multi_cycle_fsm.h           # 框架级 FsmBase<StateEnum, NStates> 模板 (新增, 框架级而非 IP 级)
    ├── TLM 模式 API: state() / tick() (ad-hoc enum class)
    ├── CH_MEM 模式 API: ch_state_machine DSL (仅 #ifdef CF_PLUGIN_USE_CH_MEM)
    └── negotiate(CapabilityTable&) 提供 multi_cycle_fsm capability

ip/cpu/arch/riscv/
├── mul_div_fsm.h               # TLM 主类 (MulDivFsmPlugin : public FsmBase<MulDivState, 4>)
│   └── 无 ch_* 代码 (TLM-only 严格分离)

ip/cpu/plugins/
└── mul_div_fsm_chmem.h         # CH_MEM 路径仅 include 框架版 + 适配 layer
```

**效果**: TLM 文件彻底无 ch_* 渗透, 业务 Plugin 可复用 FsmBase 模板 (L1Cache refill FSM / Hazard detection / Branch prediction 都可继承)。

## 3. FsmBase API 设计

### 3.1 公共 API (TLM 模式默认编译)

```cpp
namespace cf::plugin::multi_cycle_fsm {

template <typename StateEnum, std::size_t NStates>
class FsmBase {
  static_assert(std::is_enum_v<StateEnum>, "StateEnum must be enum class");
  static_assert(NStates >= 2 && NStates <= 16, "NStates must be in [2, 16]");

public:
  // 派生类需 override:
  virtual void on_tick(StateEnum current) = 0;  // TLM tick 行为

  // 公共 API:
  StateEnum state() const noexcept { return state_; }
  void tick() {
    auto prev = state_;
    on_tick(state_);
    // 派生类负责 state_ 转移
  }

  // ADR-082 negotiate API
  void negotiate(CapabilityTable& cap) {
    cap.provide("multi_cycle_fsm", this);
    // requires: 可由派生类 override 添加
  }

  // 默认 build() 空实现 (派生类重写 at_stage 注册)
  virtual void build(PipeBuilder&) {}

protected:
  StateEnum state_ = static_cast<StateEnum>(0);
};

}  // namespace cf::plugin::multi_cycle_fsm
```

### 3.2 CH_MEM 模式 API (仅 #ifdef CF_PLUGIN_USE_CH_MEM)

```cpp
#ifdef CF_PLUGIN_USE_CH_MEM
namespace cf::plugin::multi_cycle_fsm {

template <typename StateEnum, std::size_t NStates>
class FsmBase<StateEnum, NStates> {
  // ... 公共 API 之上 ...

public:
  // 派生类 override
  virtual void on_active_state(StateEnum current) = 0;  // ch_state_machine on_active 行为
  virtual StateEnum next_state(StateEnum current) = 0;  // 状态转换条件

  // CH_MEM DSL API
  chlib::ch_state_machine<StateEnum, NStates>& ch_fsm() { return ch_fsm_; }
  ch::core::context* ch_context() const { return ch_ctx_; }
  void set_ch_context(ch::core::context* ctx) { ch_ctx_ = ctx; }

  // 一次性创建 CH_MEM FSM (在 create_fsm() 内调一次)
  void create_ch_fsm() {
    ch::core::ctx_swap guard(ch_ctx_);
    ch_fsm_.set_entry(static_cast<StateEnum>(0));
    ch_fsm_.state(static_cast<StateEnum>(0)).on_active([&]() {
      on_active_state(static_cast<StateEnum>(0));
    });
    // ... 派生类 override 添加更多状态转换
    ch_fsm_.build();
  }

private:
  std::unique_ptr<ch::core::context> ch_ctx_;
  chlib::ch_state_machine<StateEnum, NStates> ch_fsm_;
};

}  // namespace cf::plugin::multi_cycle_fsm
#endif
```

### 3.3 MulDivFsmPlugin 改写示例

```cpp
// ip/cpu/arch/riscv/mul_div_fsm.h
#include "cf/plugin/multi_cycle_fsm.h"

class MulDivFsmPlugin : public cf::plugin::multi_cycle_fsm::FsmBase<MulDivState, 4> {
public:
  enum class MulDivState : std::uint8_t { IDLE, MULTIPLY, DIVIDE, WRITE_BACK };
  // 注意: StateEnum 必须用 MulDivState (与 MulDivFsmPlugin::State 兼容)

  void on_tick(MulDivState current) override {
    // TLM ad-hoc tick 行为 (从 mul_div_fsm.h 的 advance_fsm 提取)
    // ...
  }

  void build(PipeBuilder& pb) override {
    // 注册 execute 闭包 (无 ch_* 代码, TLM-only)
    pb.at_stage("execute", Phase::NORMAL, [&]() { /* ... */ });
  }

  // CH_MEM 模式 (派生类 override, 但所有 ch_* 代码在 _chmem.h 内)
  // (具体实现见 mul_div_fsm_chmem.h 后续重构)
};
```

## 4. 迁移路径

### 4.1 步骤 1: 新框架头文件

创建 `include/cf/plugin/multi_cycle_fsm.h` (公共 FsmBase 模板)。

### 4.2 步骤 2: 迁移 MulDivFsmPlugin

- 移除 `ip/cpu/arch/riscv/mul_div_fsm.h` 内 `#ifdef CF_PLUGIN_USE_CH_MEM` 块 (~180 LOC)
- MulDivFsmPlugin 改为继承 `FsmBase<MulDivState, 4>`
- 实现 `on_tick(MulDivState)` (TLM 行为从 advance_fsm 提取)
- 实现 `build(PipeBuilder&)` (at_stage 注册, 无 ch_*)

### 4.3 步骤 3: 迁移 mul_div_fsm_chmem.h

- 移除 helper namespace (`has_chmem_support` 常量)
- 实现 MulDivFsmPlugin 的 CH_MEM 部分 override (create_ch_fsm, on_active_state, next_state)
- 所有 ch_state_machine DSL 代码集中在此文件

### 4.4 步骤 4: 测试迁移

- `tests/framework/test_chmem_multi_cycle_fsm.cpp`: 改 include 路径
- `tests/framework/test_mul_div_fsm_cycle_parity.cpp`: 同步更新
- 立即编译 + 立即测试

### 4.5 步骤 5: CI 门禁验证

- `check_plugin_portability.sh` 12/12 PASS
- TLM-only 文件 grep `ch_uint|ch_reg|ch::core` = 0 hits (AC-4 验证)

## 5. 风险评估

### R1: API 兼容性破坏

MulDivFsmPlugin::create_fsm() / state_out() / opcode() 等方法签名变化, 可能影响现有测试和未来使用方。

**缓解**:
- 提供 typedef 兼容层 (`using OldCreateFsm = create_ch_fsm`)
- 1 个 version deprecate 警告 + 下个 version 删除
- CHANGELOG 明示 breaking change

### R2: FsmBase 模板抽象过早

如果 FsmBase 设计过细, 可能不适用于其他业务 Plugin (L1Cache refill FSM 等)。

**缓解**:
- Phase A RFC 跨业务 Plugin 团队 review (L1Cache / Hazard / Branch)
- 最小 API surface (3 个 virtual + 2 个 API)
- 不强制所有业务 Plugin 迁移, 仅 MulDivFsmPlugin 必迁

### R3: 测试 include 路径大量改动

5+ 测试文件需要更新 include 路径。

**缓解**:
- 批量改 + 立即编译 + 立即测试
- 一次性 commit (避免渐进式改动引入 transient 编译错误)

## 6. 替代方案

### Alt-1: 维持现状 (不重构)

- 优点: 零工作量, 维持 ADR-040 v2.0 软约束状态
- 缺点: 技术债持续累积, 业务 Plugin 复用 ch_state_machine 模板不便

### Alt-2: 仅重构, 不提取 FsmBase

- 优点: 工作量小, 仅移动 CH_MEM 代码到独立文件
- 缺点: 仍需在每个业务 Plugin 重复实现 ch_state_machine DSL, 不提供 reusable 模板

### Alt-3 (本方案): 提取 FsmBase + 重构 MulDivFsmPlugin

- 优点: 框架级 reusable 模板, 业务 Plugin 可继承复用
- 缺点: 工作量大, 需 4 周 + 严格 API 兼容性维护

## 7. 决策

采用 Alt-3。理由: FsmBase 模板是 framework 长期投资, 一次投入多次复用 (L1Cache refill / Hazard / Branch 都可受益)。推迟到 Phase E/G/H 完成后启动, 避免阻塞当前主线。

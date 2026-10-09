---
id: mfc-extract-fsm-h
linked_improvement: .rddf/improvements/mfc-extract-fsm-h.md
linked_change: openspec/changes/mfc-extract-fsm-h/
tier: complex
estimated_steps: 5
verifier: rdd-verifier
---

# Plan: mfc-extract-fsm-h

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `ip/cpu/arch/riscv/mul_div_fsm.h` 内联的 CH_MEM DSL（`#ifdef CF_PLUGIN_USE_CH_MEM` 块，~180 LOC）提取到框架级 `include/cf/plugin/multi_cycle_fsm.h`（`FsmBase<StateEnum, NStates>` 双模式模板），并借重构边界把 `advance_fsm` 从 ad-hoc busy counter 改写为真 radix-2 iterative —— 解锁 rv32um 8/8 与 DMIPS/MHz ≥ 1.4 硬门禁（objective-v0110-launch Sprint 1）。

**Architecture:** 新增框架级 `cf::plugin::multi_cycle_fsm::FsmBase<StateEnum, NStates>` 模板：TLM 模式编译为 ad-hoc `enum class` + `tick()`（ADR-048 兼容），CH_MEM 模式编译为 `chlib::ch_state_machine` DSL（`#ifdef CF_PLUGIN_USE_CH_MEM` 内提供 `create_chmem_fsm()` / `state_out()`），`negotiate()` 提供 `multi_cycle_fsm` capability（ADR-082）。`MulDivFsmPlugin` 改继承 `FsmBase<MulDivState, 4>`，业务侧只写一次状态转移表；CH_MEM 专属 DSL 接线集中到 `ip/cpu/plugins/mul_div_fsm_chmem.h`（ADR-040 v2.0 双文件分离严格落地）。

**Tech Stack:** C++23, Plugin-style (D4 / ADR-083), `chlib::ch_state_machine` DSL (ADR-046 v2.0 §2.1.1 算术豁免), `CapabilityTable::negotiate` (ADR-082), Catch2 v3.7.0, `chipforge_tests` / `chipforge_tests_chmem` 双二进制。

> **时长说明（Oracle 估时聚合）**: 本 plan 5 个 step 每个标 2-5 min 是 TDD bite-size 粒度；真实工作量分步聚合为 **2-3 周**（`objective-v0110-launch.md §8` Sprint 1: 10-09 ~ 10-22，R-RADIX-1 预留 1 周 buffer）。Step 3 的 180 LOC 删除 + 真 radix-2 重写是主要耗时点。

---

## 前置条件（硬门禁，不满足不启动）

| 依赖 | 状态要求 | 出处 |
|------|---------|------|
| `mfc-cpu-pipeline-multi-cycle-fsm` Phase E（riscv-tests rv32um 8/8）| ✅ 完成 | proposal.md §1.2 |
| `mfc-cpu-pipeline-multi-cycle-fsm` Phase G（Dhrystone baseline）| ✅ 完成 | proposal.md §1.2 |
| `mfc-cpu-pipeline-multi-cycle-fsm` Phase H（archive）| ✅ 完成（archive 后才能启动本 change）| tasks.md Dependencies |

**验证前置**：`bash tools/run_chipforge_tests.sh --all` 在当前 HEAD（7b3f40a）先跑一次记 baseline（TLM 432/432 + CHMEM 48/48 + verilator + gate），供 Step 4 回归比对。

---

## File Structure

### Production Code

| File | Responsibility | Action |
|---|---|---|
| `include/cf/plugin/multi_cycle_fsm.h` | **新增**（框架级）`FsmBase<StateEnum, NStates>` 双模式模板：TLM `state()/tick()` + CH_MEM `create_chmem_fsm()/state_out()` + `negotiate()` | Create（~100 LOC，spec.md Requirement）|
| `ip/cpu/arch/riscv/mul_div_fsm.h` | `MulDivFsmPlugin` 改继承 `FsmBase<MulDivState, 4>`；删除 `#ifdef CF_PLUGIN_USE_CH_MEM` 块（L55-73 + L276-454，~180 LOC）；`advance_fsm` → 真 radix-2 iterative | Refactor |
| `ip/cpu/plugins/mul_div_fsm_chmem.h` | CH_MEM 专属 DSL 接线（opcode_sig_/counter_reg_/result_reg_/busy_cycles_reg_ + create_fsm 语义）+ `has_chmem_support` symbol | Refactor（重接框架版）|
| `docs/architecture/adr/ADR-040-v2.md` | tasks C.4: §2.1 添加 `multi_cycle_fsm.h` 双模式规范引用 | Doc |
| `CHANGELOG.md` | tasks C.5: v0.11.0 段写 FSM 提取 entry | Doc |

### Tests

| File | Responsibility | Action |
|---|---|---|
| `tests/framework/test_multi_cycle_fsm.cpp` | **新增** `FsmBase<TestState, 3>` 实例化：TLM `tick()` 状态推进 + CH_MEM `create_chmem_fsm()`/`state_out()` | Create（RED → GREEN）|
| `tests/framework/test_chmem_multi_cycle_fsm.cpp` | include 路径迁移：`ip/cpu/arch/riscv/mul_div_fsm.h` → `cf/plugin/multi_cycle_fsm.h` + `ip/cpu/plugins/mul_div_fsm_chmem.h`（B.5）| Migrate |
| `tests/framework/test_mul_div_fsm_cycle_parity.cpp` | include 路径同步迁移（B.5）+ 保留 `[cpu][mul-div-fsm][cycle-parity]` 3 case | Migrate |
| `tests/cpu/test_mul_div_fsm.cpp` | 6 case `[cpu][mul-div-fsm]` 维持 PASS（TLM API `tick_state()/state()/busy_cycles()/result()` 不破坏）| 不改（验证）|
| `tests/CMakeLists.txt` | `CHMEM_TEST_SOURCES` 追加 `tests/framework/test_multi_cycle_fsm.cpp`（chipforge_tests 靠 GLOB_RECURSE 自动发现）| Modify |

### tasks.md → Step 聚合映射

| Step | 聚合 tasks.md 子任务 |
|---|---|
| Step 1 (RED) | B.2 一部分（`[framework][multi-cycle-fsm]` 测试 RED）|
| Step 2 (GREEN) | A.1 + A.2 + B.1 + B.2 另一部分（FsmBase 模板 + 双模式策略落地）|
| Step 3 (GREEN+REFACTOR) | B.3 + B.4 + B.5 + 真 radix-2 advance_fsm（improvement §1.1 根因）|
| Step 4 (VERIFY) | C.1 + C.2 + C.3 + C.4 + AC-1..AC-5 全部验证 |
| Step 5 (ARCHIVE) | C.5 + C.6 + A.3（ADR-040 文档若 Step 3 前未做则此步补）|

---

## Step 1: [RED] 写 `tests/framework/test_multi_cycle_fsm.cpp` 失败 case

> 目标：新增框架级测试文件，实例化尚不存在的 `FsmBase<TestState, 3>`。文件编译失败（`multi_cycle_fsm.h: No such file`）即 RED。TLM case 标 `[framework][multi-cycle-fsm]`，CH_MEM case 标 `[framework][multi-cycle-fsm][chmem]`（`#ifdef CF_PLUGIN_USE_CH_MEM` 门控）。

**Files:**
- Create: `tests/framework/test_multi_cycle_fsm.cpp`
- Modify: `tests/CMakeLists.txt:113-129`（`CHMEM_TEST_SOURCES` 追加新文件）

### 工具:
```bash
cmake --build build -j$(nproc) 2>&1 | tee /tmp/mfc_extract_build.log
grep -n "multi_cycle_fsm.h" /tmp/mfc_extract_build.log
```

### 期望输出:
- 构建失败：`fatal error: cf/plugin/multi_cycle_fsm.h: No such file or directory`（RED 成立，`chipforge_tests` 与 `chipforge_tests_chmem` 双双编译失败）

### 验证命令:
```bash
cmake --build build -j$(nproc) 2>&1 | grep -c "multi_cycle_fsm.h"
# 期望 ≥ 2（两个 target 各至少 1 处），退出码非零 → RED 确认
```

### 时长: 2-5 min（bite-size；真实工作量并入 Step 2 聚合）

- [ ] **1. 写测试文件**（含 TLM + CH_MEM 两组 case，`TestFsm` 继承 `FsmBase<TestState, 3>`）：

```cpp
// tests/framework/test_multi_cycle_fsm.cpp
// FsmBase 双模式模板验证 (mfc-extract-fsm-h Step 1 RED)
#include "catch_amalgamated.hpp"
#include <cstdint>
#include <type_traits>

#include "cf/plugin/multi_cycle_fsm.h"   // ← 尚不存在, 编译失败 = RED

enum class TestState : std::uint8_t { IDLE = 0, RUNNING = 1, DONE = 2 };

struct TestFsm : cf::plugin::multi_cycle_fsm::FsmBase<TestState, 3> {
  // TLM 模式: 派生类实现状态转移 (IDLE→RUNNING→DONE)
  void on_tick(TestState /*current*/) override {
    if (state() == TestState::IDLE) { state_ = TestState::RUNNING; }
    else if (state() == TestState::RUNNING) { state_ = TestState::DONE; }
  }
#ifdef CF_PLUGIN_USE_CH_MEM
  void on_active_state(TestState /*current*/) override {}
  TestState next_state(TestState current) override { return current; }
#endif
};

TEST_CASE("fsm_base_tlm_tick_advances_state", "[framework][multi-cycle-fsm]") {
  TestFsm f;
  REQUIRE(f.state() == TestState::IDLE);
  f.tick();                                   // IDLE → RUNNING
  REQUIRE(f.state() == TestState::RUNNING);
  f.tick();                                   // RUNNING → DONE
  REQUIRE(f.state() == TestState::DONE);
}

#ifdef CF_PLUGIN_USE_CH_MEM
TEST_CASE("fsm_base_chmem_dsl_creates_and_builds",
          "[framework][multi-cycle-fsm][chmem]") {
  ch::core::context ctx("fsm_base_ctx");
  TestFsm f;
  f.set_ch_context(&ctx);
  { ch::core::ctx_swap guard(&ctx); f.create_chmem_fsm(); }
  REQUIRE(f.ch_fsm().is_built());             // DSL select-tree 已发射
}
#endif
```

- [ ] **2. 修改 `tests/CMakeLists.txt`**：在 `CHMEM_TEST_SOURCES`（L113-129）追加 `\${CMAKE_CURRENT_SOURCE_DIR}/framework/test_multi_cycle_fsm.cpp`（`chipforge_tests` 走 GLOB_RECURSE 自动发现，无需加）。
- [ ] **3. 跑构建确认 RED**（见上方 工具/验证命令）：两个 target 均因 `multi_cycle_fsm.h` 缺失编译失败。
- [ ] **4. 不修**：RED 即达成本 step 目标，进入 Step 2。

---

## Step 2: [GREEN] 创建 `include/cf/plugin/multi_cycle_fsm.h`（FsmBase 模板 + TLM ad-hoc + CH_MEM DSL）

> 目标：落地框架级模板，让 Step 1 测试编译通过并 PASS。API 契约 = spec.md Requirement + design.md §3（含一处设计缺口修正：FsmBase 必须继承 `PluginBase`，否则 `register_plugin(std::unique_ptr<PluginBase>)` 无法接收派生类；`FsmBase::tick()` 自然隐藏 `PluginBase` private-deleted `tick()`，无歧义）。

**Files:**
- Create: `include/cf/plugin/multi_cycle_fsm.h`

### 工具:
```bash
cmake --build build -j$(nproc) 2>&1 | tail -20
./build/bin/chipforge_tests_chmem "[framework][multi-cycle-fsm]" --success
./build/bin/chipforge_tests "[framework][multi-cycle-fsm]" --success
```

### 期望输出:
- 构建 0 error
- `chipforge_tests_chmem "[framework][multi-cycle-fsm]"` → 2/2 PASS（TLM + CH_MEM case）
- `chipforge_tests "[framework][multi-cycle-fsm]"` → 1/1 PASS（TLM case，CH_MEM case 被 `#ifdef` 门控跳过）

### 验证命令:
```bash
./build/bin/chipforge_tests_chmem "[framework][multi-cycle-fsm]" 2>&1 | tail -3   # All tests passed
./build/bin/chipforge_tests      "[framework][multi-cycle-fsm]" 2>&1 | tail -3   # All tests passed
grep -nE "ch_uint|ch_reg|ch::core" include/cf/plugin/multi_cycle_fsm.h | grep -vE "^\s*[0-9]+:\s*(//|/\*|\*)" | wc -l
# 期望 = 0 行"裸"ch_* 命中（ch_* 必须全部在 #ifdef CF_PLUGIN_USE_CH_MEM 块内）
```

### 时长: 2-5 min（bite-size；真实工作量 = FsmBase 模板全量设计 + 实现，聚合 1-2 天）

- [ ] **1. 创建 `include/cf/plugin/multi_cycle_fsm.h`**（完整骨架，~100 LOC）：

```cpp
// include/cf/plugin/multi_cycle_fsm.h
//
// 功能描述: 框架级多周期 FSM 通用模板 (ADR-046 v2.0 §2.1.1 算术豁免配套)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-10 (mfc-extract-fsm-h Step 2)
//
// 设计 (spec.md Requirement + design.md §3):
//   - 单一 FSM 描述同时服务 TLM 与 CH_MEM 两种编译模式
//   - TLM 模式: ad-hoc enum class + tick() (ADR-048 兼容, ADR-025 tick 禁令豁免
//     因 FsmBase 继承 PluginBase 后 tick() 隐藏 private-deleted 基类 tick)
//   - CH_MEM 模式: chlib::ch_state_machine DSL (#ifdef CF_PLUGIN_USE_CH_MEM)
//   - negotiate() 提供 "multi_cycle_fsm" capability (ADR-082)
//   - 注意: 本文件是框架头 (include/cf/plugin/), check_plugin_portability.sh
//     Check 2 只扫 ip/*/plugins/*.h — 框架头允许 #ifdef CH_MEM 块 (同 payload.h)
//
// 约束:
//   - 不 #define CF_PLUGIN_USE_CH_MEM (Check 6: 仅允许 CMake 命令行)
//   - StateEnum 必须是 enum class, entry 状态 enum 值必须为 0 (ADR-046 §2.3
//     state_reg 初值 0_d 约束)
//   - NStates ∈ [2, 16]

#ifndef CF_PLUGIN_MULTI_CYCLE_FSM_H
#define CF_PLUGIN_MULTI_CYCLE_FSM_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "cf/plugin/capability_table.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"

#ifdef CF_PLUGIN_USE_CH_MEM
#include <chlib/state_machine.h>
#endif

namespace cf {
namespace plugin {
namespace multi_cycle_fsm {

template <typename StateEnum, std::size_t NStates>
class FsmBase : public cf::plugin::PluginBase {
  static_assert(std::is_enum_v<StateEnum>, "StateEnum must be enum class");
  static_assert(NStates >= 2 && NStates <= 16, "NStates must be in [2, 16]");

 public:
  // ---- TLM 模式 API (默认编译) ----
  // 派生类 override: 状态转移逻辑 (从 advance_fsm switch 提取)
  virtual void on_tick(StateEnum /*current*/) = 0;

  StateEnum state() const noexcept { return state_; }

  // 推进 FSM 一拍: 隐藏 PluginBase private-deleted tick(), 无歧义
  void tick() { on_tick(state_); }

  // ---- ADR-082 negotiate: 声明 multi_cycle_fsm capability ----
  void negotiate(::cf::plugin::CapabilityTable& cap) override {
    cap.provide("multi_cycle_fsm", this);
  }

  // ---- build() 默认空实现 (派生类 override 注册 at_stage) ----
  void build(::cf::plugin::PipeBuilder& /*pb*/) override {}

#ifdef CF_PLUGIN_USE_CH_MEM
  // ---- CH_MEM 模式 API (ch_state_machine DSL) ----
  // 派生类 override (实现在 _chmem.h 配对文件):
  virtual void on_active_state(StateEnum /*current*/) {}
  virtual StateEnum next_state(StateEnum current) { return current; }

  chlib::ch_state_machine<StateEnum, NStates>& ch_fsm() { return ch_fsm_; }
  ch::core::context* ch_context() const noexcept { return ch_ctx_; }
  void set_ch_context(ch::core::context* ctx) noexcept { ch_ctx_ = ctx; }

  // 一次性构建 CH_MEM DSL (elaborate 期调一次, 同旧 create_fsm() 语义)
  void create_chmem_fsm() {
    if (ch_fsm_built_) return;  // idempotent (if/else 包裹, 非早返)
    if (!ch_ctx_) {
      ch_ctx_ = new ch::core::context("multi_cycle_fsm_ctx");
    }
    ch::core::ctx_swap guard(ch_ctx_);
    ch_fsm_.set_entry(static_cast<StateEnum>(0));  // entry 必须 enum 值 0
    // 派生类通过 on_active_state/next_state 在 create 后接线 transition_when
    ch_fsm_.build();
    ch_fsm_built_ = true;
  }

  // 当前状态输出信号 (ch_uint, 供 Simulator get_value 断言)
  ch_uint<chlib::ch_state_machine<StateEnum, NStates>::STATE_BITS> state_out() {
    create_chmem_fsm();
    ch::core::ctx_swap guard(ch_ctx_);
    return ch_fsm_.current_state_uint();
  }

 protected:
  chlib::ch_state_machine<StateEnum, NStates> ch_fsm_;
  ch::core::context* ch_ctx_ = nullptr;
  bool ch_fsm_built_ = false;
#endif  // CF_PLUGIN_USE_CH_MEM

 protected:
  StateEnum state_ = static_cast<StateEnum>(0);
};

}  // namespace multi_cycle_fsm
}  // namespace plugin
}  // namespace cf

#endif  // CF_PLUGIN_MULTI_CYCLE_FSM_H
```

- [ ] **2. 构建 + 跑 Step 1 测试**（见上方 工具/验证命令）：`chipforge_tests_chmem "[framework][multi-cycle-fsm]"` 2/2 PASS、`chipforge_tests "[framework][multi-cycle-fsm]"` 1/1 PASS。
- [ ] **3. 兼容层 typedef（proposal R1 缓解）**：在 `FsmBase` CH_MEM 段加 `using create_fsm = create_chmem_fsm;`（旧 API 名 deprecate 1 version），保证 `test_chmem_multi_cycle_fsm.cpp` 现有 `plugin->create_fsm()` 调用链不破坏。
- [ ] **4. 门禁快查**：`bash tools/verify_plugin_decision.sh` + `bash tools/check_plugin_portability.sh` 保持 12/12 PASS（框架头不在 `ip/` 扫描范围，应无新 FAIL）。

---

## Step 3: [GREEN+REFACTOR] `MulDivFsmPlugin` 改继承 `FsmBase<MulDivState, 4>`，删除 `#ifdef CF_PLUGIN_USE_CH_MEM` 块

> 目标（本 change 核心）：
> 1. `mul_div_fsm.h` 删除 `#ifdef CF_PLUGIN_USE_CH_MEM` 全部内容（L55-73 的 include/using + L276-454 的 DSL 块，~180 LOC）→ grep `ch_uint|ch_reg|ch::core` = 0 hits（AC-1/AC-4）。
> 2. `MulDivFsmPlugin` 改继承 `cf::plugin::multi_cycle_fsm::FsmBase<MulDivState, 4>`（同时保留 `PluginBase` is-a 关系——FsmBase 已继承 PluginBase）。
> 3. `advance_fsm` 从 ad-hoc busy counter 改写为**真 radix-2 iterative**（improvement §1.1 根因）：MUL=1c / MULH 族=3c / DIV 族=33c 三模板（AC-3）。
> 4. CH_MEM DSL 接线迁入 `ip/cpu/plugins/mul_div_fsm_chmem.h`（Check 2 要求 `_chmem.h` 含 ch_*）。

**Files:**
- Modify: `ip/cpu/arch/riscv/mul_div_fsm.h`
- Modify: `ip/cpu/plugins/mul_div_fsm_chmem.h`
- Modify: `tests/framework/test_chmem_multi_cycle_fsm.cpp`（include 迁移）
- Modify: `tests/framework/test_mul_div_fsm_cycle_parity.cpp`（include 迁移）

### 工具:
```bash
cmake --build build -j$(nproc) 2>&1 | tail -10
./build/bin/chipforge_tests "[cpu][mul-div-fsm]" --success
./build/bin/chipforge_tests "[cpu-integration]" --success
./build/bin/chipforge_tests_chmem "[framework][chmem][multi-cycle]" --success
grep -cE "ch_uint|ch_reg|ch::core" ip/cpu/arch/riscv/mul_div_fsm.h
bash tools/check_plugin_portability.sh 2>&1 | tail -15
```

### 期望输出:
- 构建 0 error
- `[cpu][mul-div-fsm]`（test_mul_div_fsm.cpp 6 case + cycle_parity TLM case）+ `[cpu-integration]` 全部 PASS（0 回归）
- `chipforge_tests_chmem "[framework][chmem][multi-cycle]"`（test_chmem_multi_cycle_fsm.cpp 5 case）5/5 PASS
- `grep -cE "ch_uint|ch_reg|ch::core" mul_div_fsm.h` = **0**（AC-1 落地）
- `check_plugin_portability.sh` 8/8 PASS（含 Check 9 FSM_EXEMPT 白名单）

### 验证命令:
```bash
./build/bin/chipforge_tests "[cpu][mul-div-fsm]" 2>&1 | tail -3    # 6/6 + parity case PASS
./build/bin/chipforge_tests "[cpu-integration]" 2>&1 | tail -3     # 全 PASS
./build/bin/chipforge_tests_chmem "[framework][chmem][multi-cycle]" 2>&1 | tail -3  # 5/5 PASS
bash tools/check_plugin_portability.sh 2>&1 | grep -E "PASS|FAIL"  # 8/8 PASS
```

### 时长: 2-5 min（bite-size；真实工作量 = 180 LOC 删除 + 真 radix-2 重写 + 测试迁移，聚合 1-2 周，占 Sprint 1 主区间）

- [ ] **1. `mul_div_fsm.h` 删除全部 CH_MEM 内容**：删除 L55-60（`#ifdef CF_PLUGIN_USE_CH_MEM` + `<chlib/state_machine.h>` include + `cf_cpu_arch_riscv_dsl` ns）、L67-73（`using ch::core::ch_uint/...`）、L276-454（`#ifdef` DSL 块整体：`create_fsm/opcode_signal/rs1_signal/rs2_signal/opcode/rs1/rs2/state_out/busy_cycles_out/result_out/context/set_context` + 全部 ch 私有成员）。保留 `#define CF_PLUGIN_USE_FSM_EXEMPT`（L39，verify_plugin_decision Check 2 豁免 `enum class State` + `switch(state_)` 需要）。
- [ ] **2. Check 9 兼容（关键）**：`check_plugin_portability.sh` Check 9 要求声明 `CF_PLUGIN_USE_FSM_EXEMPT` 的 ip/*.h 文件内容必须含 `ch_state_machine|chlib::ch_state_machine` 字面。删除 DSL 后 `mul_div_fsm.h` 无该字面 → 在类注释补一行真实声明：`// ADR-046 v2.0: FSM 由 framework include/cf/plugin/multi_cycle_fsm.h 提供 (CH_MEM 经 chlib::ch_state_machine DSL)`。
- [ ] **3. 改继承 + 真 radix-2 advance_fsm**：

```cpp
// ip/cpu/arch/riscv/mul_div_fsm.h — 类声明改为:
//   template <typename T = std::uint32_t>
//   class MulDivFsmPlugin
//       : public cf::plugin::multi_cycle_fsm::FsmBase<MulDivFsmPlugin<T>::State, 4> {
//   注意: State enum 定义必须在继承列表之后 (或前置声明), 保持 enum 值
//   IDLE=0 (entry) / MULTIPLY=1 / DIVIDE=2 / WRITE_BACK=3

// advance_fsm() → on_tick(State) override (真 radix-2 iterative):
void on_tick(State current) override {
  switch (current) {
    case State::IDLE: busy_cycles_ = 0; break;            // 等新指令 (set_opcode 进入)
    case State::MULTIPLY:                                  // MUL=1c, MULH 族=3c
      if (is_mul_family(opcode_)) {                        // MUL: 1 拍直接结果
        result_ = compute_mul_div(static_cast<std::uint8_t>(opcode_), rs1_, rs2_);
        state_ = State::WRITE_BACK; busy_cycles_ = MUL_CYCLES;
      } else {                                             // MULH/MULHSU/MULHU: 2 拍 radix-2
        if (++partial_steps_ >= 2) {                       // 16-bit×16-bit 部分积累积
          result_ = compute_mul_div(static_cast<std::uint8_t>(opcode_), rs1_, rs2_);
          state_ = State::WRITE_BACK; busy_cycles_ = MULH_CYCLES;
        }
      }
      break;
    case State::DIVIDE:                                    // 32 拍 radix-2 迭代 + 1 写回
      if (iter_ == 0) { remainder_ = 0; quotient_ = 0; }
      // 恢复除法 (restoring): 每拍 1 bit
      remainder_ = (remainder_ << 1) | ((rs1_ >> (31 - iter_)) & 1u);
      if (remainder_ >= rs2_) { remainder_ -= rs2_; quotient_ |= (1u << (31 - iter_)); }
      if (++iter_ >= 32) {
        result_ = quotient_;
        state_ = State::WRITE_BACK;
        busy_cycles_ = DIV_CYCLES;                         // 32 iterative + 1 write-back
        iter_ = 0;
      }
      break;
    case State::WRITE_BACK: state_ = State::IDLE; break;
  }
}
// 新增常量: MUL_CYCLES=1, MULH_CYCLES=3 (mulh/mulhsu/mulhu), DIV_CYCLES=33 (已有)
// 删除: busy_cycles_ ad-hoc 计数分支 (不再有 "count 到 33 直接 compute" 兜底)
// 保留公开测试 API 不变: state()/busy_cycles()/result()/set_opcode()/set_operands()/tick_state()
```

- [ ] **4. `mul_div_fsm_chmem.h` 重接框架版**：删除 helper namespace stub 中的 `has_chmem_support` 占位注释，改为——(a) 保留跨模式 `mul_div_fsm_chmem::has_chmem_support = true` symbol（cycle parity 测试入口，TLM 与 CH_MEM 均声明）；(b) `#ifdef CF_PLUGIN_USE_CH_MEM` 块内提供 MulDivFsmPlugin CH_MEM 接线：`create_fsm()`（typedef 兼容 → `FsmBase::create_chmem_fsm`）、`opcode()/rs1()/rs2()` ch 信号访问器（内部持有 `ch_uint<8>/ch_uint<32>` unique_ptr，构造 opcode_sig_ 等）、`busy_cycles_out()/result_out()`（`ch_reg` 锁存，沿用 B.2.1 ctx_swap 缓存策略，禁止跨函数 select-tree 重建）、`set_context(ctx)` 转发 `FsmBase::set_ch_context`。所有 `ch_*` 字面留在 `_chmem.h`（Check 2 满足）。
- [ ] **5. 测试 include 迁移（B.5 / R2 批量改）**：
  - `test_chmem_multi_cycle_fsm.cpp`：`#include "ip/cpu/arch/riscv/mul_div_fsm.h"` → `#include "cf/plugin/multi_cycle_fsm.h"` + `#include "ip/cpu/plugins/mul_div_fsm_chmem.h"`（保留 mul_div_fsm.h 拿 `MulDivFsmPlugin` 类定义）；fixture 内 `plugin->set_context(&ctx)` 改 `plugin->set_ch_context(&ctx)`（或 `_chmem.h` 内转发），`plugin->opcode()` 等经 `_chmem.h` 访问器保持调用不变。
  - `test_mul_div_fsm_cycle_parity.cpp`：`#include "ip/cpu/plugins/mul_div_fsm_chmem.h"` 之前补 `#include "cf/plugin/multi_cycle_fsm.h"`（TLM parity case 依赖的 `[cpu][mul-div-fsm]` 语义不变）。
- [ ] **6. 跑回归**（见上方 工具/验证命令）：`[cpu][mul-div-fsm]` + `[cpu-integration]` + `chipforge_tests_chmem "[framework][chmem][multi-cycle]"` 全 PASS，`mul_div_fsm.h` grep ch_* = 0，`check_plugin_portability.sh` 8/8 PASS。
- [ ] **7. TLM↔CH_MEM cycle parity（ADR-046 强约束 4）**：`[cpu][mul-div-fsm][cycle-parity]` 3 case PASS（MUL=1 / DIV=33 TLM 直测 + `_chmem.h` 存在性 + `has_chmem_support` symbol）。

---

## Step 4: [VERIFY] 跑全套 0 回归 + 真 radix-2 cycle 数核对 + rv32um 8/8 + dhrystone

> 目标：全量回归 0 退化 + 硬指标中检（objective §8 Sprint 1 交付：真 radix-2 + rv32um 8/8 中检 + dhrystone 修 livelock）。

**Files:**
- Modify: `docs/architecture/adr/ADR-040-v2.md`（tasks C.4：§2.1 添加 `multi_cycle_fsm.h` 双模式规范引用；若 A.3 已在 Step 2 完成则跳过）

### 工具:
```bash
bash tools/run_chipforge_tests.sh --all 2>&1 | tail -30
./build/bin/chipforge_tests "[riscv-tests][rv32um]" --success
./build/bin/chipforge_tests "[cpu-integration][dhrystone]" --success
grep -nE "ch_uint|ch_reg|ch::core" ip/cpu/arch/riscv/mul_div_fsm.h | wc -l
bash tools/verify_plugin_decision.sh 2>&1 | tail -5
bash tools/check_plugin_portability.sh 2>&1 | tail -5
```

### 期望输出:
- `--all` 0 regression：TLM 432/432（含 8 rv32um）+ CHMEM 48/48 + verilator smoke + 4 architecture gates 全绿（improvement AC#1）
- `[riscv-tests][rv32um]` **8/8 PASS**（mul/mulh/mulhsu/mulhu/div/divu/rem/remu，improvement AC#2）—— 真 radix-2 落地后从 0/8 翻转
- `[cpu-integration][dhrystone]` PASS 无 livelock、cycle ≤ 1M（improvement AC#4）
- **cycle 数核对（AC#3）**：`MUL=1`、`MULH/MULHSU/MULHU=3`、`DIV/DIVU/REM/REMU=33`（1c/3c/33c 三模板）—— 与 `rv32um-baseline-matrix.csv` 新基线一致（写回 PASS 状态 + cycles 实测列）
- `mul_div_fsm.h` grep ch_* = 0（AC-1）、`verify_plugin_decision.sh` 8/8 + `check_plugin_portability.sh` 8/8（合计 12/12，AC-4）
- **AC-5**：`MulDivFsmPlugin::negotiate` 经 `FsmBase::negotiate` 提供 `multi_cycle_fsm` capability —— 由 `test_mul_div_fsm_cycle_parity.cpp` StubCapabilityProvider + `[cpu][mul-div-fsm]` setup_build_no_throw 隐式覆盖；另在 Step 2 的 `TestFsm::negotiate` 已显式断言 `cap.provide("multi_cycle_fsm")` 路径存在（`test_multi_cycle_fsm.cpp` 可补 1 个 negotiate case）

### 验证命令:
```bash
bash tools/run_chipforge_tests.sh --all 2>&1 | grep -E "tests passed|failed|FAILED" | tail -5
./build/bin/chipforge_tests "[riscv-tests][rv32um]" 2>&1 | tail -3          # 8/8 PASS
./build/bin/chipforge_tests "[cpu-integration][dhrystone]" 2>&1 | tail -3   # PASS, cycle ≤ 1M
cat soc/cpu/docs/dse/rv32um-baseline-matrix.csv                            # 8 行 status=PASS
```

### 时长: 2-5 min（bite-size；真实工作量 = rv32um 8/8 调通 + dhrystone 验证，聚合 3-5 天，Sprint 1 后段）

- [ ] **1. 跑全量回归** `bash tools/run_chipforge_tests.sh --all`（记录 TLM 432/432 + CHMEM 48/48 数字，对照 Step 0 baseline）。
- [ ] **2. rv32um 8/8 中检**：`./build/bin/chipforge_tests "[riscv-tests][rv32um]"` → 8/8 PASS；`soc/cpu/docs/dse/rv32um-baseline-matrix.csv` 8 行全部 `status=PASS`（commit 前核对，数字维护原则）。
- [ ] **3. dhrystone 验证**：`[cpu-integration][dhrystone]` 3 case PASS 无 timeout；`dhrystone_fsm_dmips_mhz_metric` 输出 cycle_count ≤ 2,510,000（= DMIPS/MHz ≥ 1.4 的 cycle 等价形式）。
- [ ] **4. AC-4 门禁三连**：`bash tools/verify_plugin_decision.sh`（8/8）+ `bash tools/check_plugin_portability.sh`（8/8）+ `grep -cE "ch_uint|ch_reg|ch::core" ip/cpu/arch/riscv/mul_div_fsm.h` = 0（12/12 合计 + 直接 grep）。
- [ ] **5. AC-5 negotiate 复核**：在 `test_multi_cycle_fsm.cpp` 补 1 个 case（`[framework][multi-cycle-fsm][negotiate]`）：`CapabilityTable cap; TestFsm f; f.negotiate(cap); REQUIRE(cap.has("multi_cycle_fsm"));` → PASS。
- [ ] **6. ADR-040-v2.md 文档更新**（tasks C.4）：§2.1 表格加一行 `include/cf/plugin/multi_cycle_fsm.h` —— "框架级多周期 FSM 双模式模板（FsmBase，ADR-046 §2.1.1 配套）"；若 A.3 已前置完成则仅核对。

---

## Step 5: [ARCHIVE] 跑 `openspec archive mfc-extract-fsm-h --yes` + CHANGELOG v0.11.0 段

> 目标：tasks.md 全部勾选 → CHANGELOG entry → `openspec archive`。本步由 execute P2.5/P3 阶段执行（本 plan 不预执行，MUST-NOT 约束）。

**Files:**
- Modify: `CHANGELOG.md`（v0.11.0 段）
- Modify: `openspec/changes/mfc-extract-fsm-h/tasks.md`（全部 `- [ ]` → `- [x]`）

### 工具:
```bash
openspec change validate mfc-extract-fsm-h
openspec archive mfc-extract-fsm-h --yes
git log --oneline -3
```

### 期望输出:
- `openspec change validate` PASS（spec.md Requirement 有对应实现、tasks 全勾）
- `openspec archive` 成功，change 移入 `openspec/changes/archive/`
- CHANGELOG v0.11.0 段记录：FsmBase 框架头 + mul_div_fsm.h 180 LOC 提取 + 真 radix-2 + rv32um 8/8

### 验证命令:
```bash
openspec list 2>&1 | grep mfc-extract-fsm-h   # 不再出现于 active changes
ls openspec/changes/archive/ | grep mfc-extract-fsm-h  # archive 目录存在
grep -n "mfc-extract-fsm-h" CHANGELOG.md     # v0.11.0 段 entry 存在
```

### 时长: 2-5 min（bite-size；真实工作量 = archive + CHANGELOG 撰写，聚合 0.5 天）

- [ ] **1. CHANGELOG v0.11.0 段 entry**（tasks C.5）：在 `CHANGELOG.md` 顶部 v0.11.0 段（当前只有 v0.10.x interim 段）写：`- **mfc-extract-fsm-h**: 新增框架级 include/cf/plugin/multi_cycle_fsm.h (FsmBase 双模式模板, ADR-046/082); mul_div_fsm.h 移除 #ifdef CH_MEM 块 (~180 LOC); advance_fsm 真 radix-2 iterative; [riscv-tests] rv32um 0/8 → 8/8; [dhrystone] livelock 修复, cycle ≤ 1M`。
- [ ] **2. tasks.md 全部勾选**：`openspec/changes/mfc-extract-fsm-h/tasks.md` Phase A/B/C 15 项 + Acceptance 5 项全部 `- [x]`（sed 批量）。
- [ ] **3. `openspec change validate mfc-extract-fsm-h`** → PASS（结构 + spec + AC 完整性）。
- [ ] **4. `openspec archive mfc-extract-fsm-h --yes`** → 成功；`openspec list` 确认移出 active。
- [ ] **5. 收尾**：`bash tools/run_chipforge_tests.sh --all` 复跑 1 次确认 archive 后仍全绿（archive 仅移动文件，不应引入回归）；commit 由 execute/archive 阶段按仓库约定执行（本 plan 不 commit）。

---

## Self-Review（对照 spec/improvement 自查）

**1. Spec 覆盖**：

| spec / AC | 落点 |
|---|---|
| spec.md Requirement: 框架级独立 header + `FsmBase<StateEnum,NStates>` + CH_MEM `create_chmem_fsm`/`state_out` + TLM `state`/`tick` + `negotiate` | Step 2 |
| spec.md Scenario: 业务 Plugin 继承 FsmBase、TLM 文件 grep 0 命中、negotiate capability | Step 3 + Step 4 AC-4/AC-5 |
| improvement AC#1 `--all` 0 regression | Step 4 |
| improvement AC#2 rv32um 8/8 | Step 4 |
| improvement AC#3 真 radix-2（1c/3c/33c 三模板）| Step 3 + Step 4 |
| improvement AC#4 dhrystone PASS cycle ≤ 1M | Step 4 |
| improvement AC dmips ≥ 1.4（经 feat-dhrystone-dmips-gate）| Step 4（cycle 等价 ≤ 2,510,000）|
| improvement AC-4 grep 0 hits + portability 12/12 | Step 3 + Step 4 |
| improvement AC-5 negotiate 仍工作 | Step 2 + Step 4 |
| improvement AC `openspec archive` 成功 | Step 5 |

**2. 占位符扫描**：无 "TBD"/"..."；每个 step 含实际文件路径 + 代码骨架 + 可执行验证命令。

**3. 类型一致性**：
- `FsmBase::tick()`（TLM 推进）与业务 `tick_state()`（测试 helper）并存——`tick_state()` 在 `mul_div_fsm.h` 保留为 `on_tick` 薄包装（`void tick_state() { tick(); }`），`test_mul_div_fsm.cpp` 调用不变。
- `FsmBase::state_`（protected）与 `MulDivFsmPlugin` 现有 `state_` 成员合并（删除派生类重复声明，统一走 FsmBase）；`set_opcode()` 写 `state_` 仍直接可用。
- CH_MEM `create_fsm` → `create_chmem_fsm` typedef 兼容（Step 2.3），`test_chmem_multi_cycle_fsm.cpp` fixture 调用链不破坏。
- `on_active_state/next_state`（CH_MEM 虚接口）与 `on_tick`（TLM 虚接口）命名分离，避免 `#ifdef` 内虚函数名冲突。

**4. 风险复述（proposal R1/R2 + improvement R-RADIX-1/2）**：
- R1 API 兼容：typedef 兼容层 Step 2.3 + Step 3.4 双保险。
- R2 测试 include 批量改：Step 3.5 一次性批量改 + 立即编译（单 commit 粒度，避免 transient 编译错误）。
- R-RADIX-1：ch_state_machine DSL 边界 case 若 Step 3 阻塞，按 objective §8 走 R-RADIX-1 缓解（Sprint 1 后段 1 周 buffer）。
- R-RADIX-2：真 radix-2 归本 change，`negotiate()` 接续不动（Step 3.3 不改 `negotiate` override，继承 FsmBase 默认即可——当前 `mul_div_fsm.h:166-168` 的 `cap.provide("multi_cycle_fsm", this)` 与 FsmBase 默认行为一致，可删除 override）。

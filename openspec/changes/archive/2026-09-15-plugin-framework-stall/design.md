# Design: `plugin-framework-stall`

> **Status**: DRAFT
> **Date**: 2026-09-15
> **Author**: Sisyphus
> **Provenance**: `proposal.md` (openspec/changes/plugin-framework-stall/proposal.md)

## 1. Architecture Overview

```
                     ┌─────────────────────────────────────┐
                     │  pb.run() —— Stall Loop (NEW)        │
                     └─────────────────────────────────────┘

  for stage in canonical_stage_order:
      if should_stall_stage(stage):        ← NEW
          skip stage callback              ← NEW
          continue
      for phase in [EARLY, NORMAL, LATE]:
          for s in stages_ where s.name == stage and s.phase == phase:
              s.callback()

  commit_storages()
```

**关键变化**：在 `pb.run()` 的 canonical stage 循环中插入"should_stall_stage?" 检查。true → skip 该 stage 所有 phase 闭包；false → 正常执行。

## 2. PipeBuilder API 扩展

### 2.1 新公开 API

```cpp
// include/cf/plugin/pipe_builder.h
class PipeBuilder {
 public:
  // 注册一个 shared_ptr<CtrlLink> 绑定到具体 stage 名
  // - 多次调用同名 stage 不会覆盖,新 CtrlLink 会被追加到 stage 的 ctrl_links_ 列表
  // - 阶段 stall 是 OR 合并语义: stage halt if any ctrl_link in stage's list halt
  // - 注册时机: 必须在 pb.build() 之前,典型在 Plugin::build() 内
  // - shared_ptr 避开 CtrlLink = delete 拷贝约束 (保持原 CtrlLink API 严格性)
  void register_ctrl_link(const std::string& stage_name,
                          std::shared_ptr<CtrlLink> ctrl);

  // 查询: 指定 stage 当前是否应 stall (CtrlLink OR 合并)
  bool should_stall_stage(const std::string& stage_name) const;

  // 调试/测试访问器
  std::size_t ctrl_link_count(const std::string& stage_name) const noexcept;
  std::shared_ptr<CtrlLink> get_ctrl_link(const std::string& stage_name,
                                          std::size_t index) const;
  void clear_ctrl_links();  // 单元测试用
};
```

### 2.2 新私有状态

```cpp
class PipeBuilder {
 private:
  // CtrlLink 绑定: stage_name → vector<shared_ptr<CtrlLink>>
  // OR 合并语义在 should_stall_stage() 内执行
  // shared_ptr 保证 lambda 捕获对象生命周期长于 PipeBuilder 引用
  std::unordered_map<std::string, std::vector<std::shared_ptr<CtrlLink>>>
      stage_ctrl_links_;
};
```

**约束**：
- `CtrlLink` 保持**不可拷贝**（`ctrl_link.h:31-32` 不变）。
- 用 `std::shared_ptr<CtrlLink>` 持有，避开拷贝约束。
- shared_ptr 构造在 `Plugin::build()` 调用时（注册期），不在 `at_stage` callback 内（执行期），符合 ADR-040 精神。

### 2.3 `pb.run()` 改造

```cpp
void run() {
  for (std::uint8_t tid = 0; tid < n_threads_; ++tid) {
    for (auto& p : plugins_) p->set_tid(tid);
    const auto order = canonical_stage_order();
    for (const auto& stage_name : order) {
      // NEW: stall check
      if (should_stall_stage(stage_name)) {
        continue;  // skip all phases of this stage
      }
      // EXISTING: per-phase dispatch (commit A from cpu-pipeline-stubs-replace)
      for (int p_idx = 0; p_idx < 3; ++p_idx) {
        const Phase target_phase = static_cast<Phase>(p_idx);
        for (const auto& s : stages_) {
          if (s.name == stage_name && s.phase == target_phase) {
            s.callback();
          }
        }
      }
    }
  }
  commit_storages();
}
```

**注意**：`continue` 不触发 ADR-040 Tier-1 #4 的 "no early return" 禁令——它是 PipeBuilder 自身的循环控制，不是 `at_stage` 回调内部的 `if(cond) return;`。Plugin 内的 callback 仍需遵守 `if (cond) { /* do work */ } else { /* no-op */ }` 模式。

### 2.4 `throw_when` 与 `flush_when` 消费

`pb.run()` 当前不消费这两类。设计：

**throw_when**（异常注入）：
- `pb.run()` 末尾追加 `if (any_ctrl_link.should_throw()) throw PluginException(...);`
- 不影响 stall 行为（halt 与 throw 独立 OR）
- **类型定义**（commit A 同步新增）：`cf::plugin::PluginException` 定义于 `include/cf/plugin/plugin_exception.h`，继承 `std::runtime_error`，构造 `PluginException(stage_name, msg)`。
- 实装：`pb.run()` 末尾：
```cpp
for (const auto& [stage, ctrls] : stage_ctrl_links_) {
  for (const auto& c : ctrls) {
    if (c->should_throw()) {
      throw PluginException(stage, "throw_when condition triggered");
    }
  }
}
commit_storages();
```

**flush_when**（payload 清空）：
- 框架**不自动**清空 payload（保持 payload 稳定性）
- Plugin 自己在 `at_stage` 闭包内调 `c.should_flush() ? reset_payloads() : noop()`
- **演示桩**：`BranchPredictorPlugin` 在 `at_stage("fetch", EARLY)` 闭包内：
```cpp
if (branch_predictor_ctrl.should_flush()) {
  auto* n = pb.node_of_logic_stage("fetch").get();
  if (n) {
    n->reset();  // 清空本 stage 所有 payload
  }
}
```
- **不强制**：框架**不**在 pb.run() 内自动消费 flush。理由：flush 语义依赖具体 Plugin（"清空哪个 stage"），框架无法统一决定。这是 `plugin-framework-stall` 的边界——flush 实装推迟到 `cpu-pipeline-mispredict`。

**Summary table**：

| CtrlLink API | 框架自动消费？ | 实装位置 |
|--------------|--------------|---------|
| `halt_when` | ✅ 自动（pb.run stall loop） | `pb.should_stall_stage()` |
| `throw_when` | ✅ 自动（pb.run 末尾异常抛出） | `pb.run()` 末尾 |
| `flush_when` | ❌ Plugin 自行消费 | `BranchPredictorPlugin` 演示桩 |
| `bypass` | ❌ Plugin 自行消费 | （不实装演示，本 change 范围外） |

## 3. Consumer A: PTW-busy → IBus fetch stalled

### 3.0 PTW_ACTIVE 写/清零归属（关键设计决策）

| 时机 | 写者 | 值 |
|------|------|-----|
| TLB miss 启动 PTW | `MMUPlugin::do_lookup` miss 分支（MMUPlugin.cpp:57） | `= 1` |
| PTW 完成回调 | **MMUPlugin 完成回调**（MMUPlugin.cpp:60-67, commit B 改） | `= 0`（在写 PADDR+MMU_VADDR 同时清） |

**决策**：清零归属 **MMUPlugin 完成回调**（与 design.md 原始 §3 一致）。理由：
1. 完成回调是 PTW 周期的终结事件，单一权威点。
2. 写 `PADDR/MMU_VADDR` 与清 `PTW_ACTIVE` 必须在同一 cycle 内原子发生（避免下游 fetch 在 PADDR 有效之前 un-stall）。
3. 若改由 `tlb_lookup_ifetch` 下一 cycle 清零，会引入 1-cycle 假 stall（fetch unstall 滞后于 PADDR 可读），与场景 3.1.3 端到端断言（"PTW 完成下一 cycle fetch 即读真 PADDR"）冲突。

**任务**：tasks §2.3 必须包含 `ip/mmu/tlm/MMUPlugin.cpp` 修改项（commit B 同步改动）。

### 3.1 数据流

```
cycle N:
  ┌─────────────────────────────────────────────────────┐
  │ tlb_lookup_ifetch (NORMAL)                          │
  │   MMUPlugin.do_lookup(ifetch_vaddr)                 │
  │     → multi_tlb_.lookup() → miss!                   │
  │     → 写 (*node)(PTW_ACTIVE) = 1                    │
  │     → ptw_->start_walk(...) → async                 │
  └─────────────────────────────────────────────────────┘
                              ↓
              should_stall_stage("fetch")?
                  → YES (PTW_ACTIVE == 1)
                              ↓
  ┌─────────────────────────────────────────────────────┐
  │ fetch (NORMAL)  ← SKIPPED by pb.run() stall loop   │
  │   IBusPlugin.at_stage("fetch", NORMAL)             │
  │     → 本 cycle 不读 INSTRUCTION, 不更新 PC         │
  └─────────────────────────────────────────────────────┘

cycle N+1, N+2, ...:
  ptw_l0, ptw_l1, ptw_l2 闭包推进 PTW
  fetch 持续 skip

cycle N+M (PTW 完成):
  ptw 完成回调 → 写 PADDR + MMU_VADDR → 清 PTW_ACTIVE
                              ↓
              should_stall_stage("fetch")?
                  → NO
                              ↓
  ┌─────────────────────────────────────────────────────┐
  │ fetch (NORMAL)                                      │
  │   IBusPlugin.at_stage("fetch", NORMAL)             │
  │     → 读 PADDR 拿真指令                             │
  │     → 更新 INSTRUCTION                              │
  └─────────────────────────────────────────────────────┘
```

### 3.2 IBusPlugin 改动

`ip/cpu/plugins/ibus.h::build()`：

```cpp
void build(PipeBuilder& pb) override {
  // ... 既有代码 ...

  // NEW: register fetch CtrlLink (PTW-busy halt)
  // 注意: PTW_ACTIVE 是 mmu_keys 而非 keys<T,XLEN> 的成员
  //       ibus.h 需要显式 include ip/mmu/tlm/mmu_keys.h
  auto fetch_ctrl = std::make_shared<cf::plugin::CtrlLink>();
  fetch_ctrl->halt_when([&pb]() {
    auto* n = pb.node_of_logic_stage("tlb_lookup_ifetch").get();
    if (!n) return false;
    using MmuKeys = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;
    return static_cast<bool>(n->operator()(MmuKeys::PTW_ACTIVE));
  });
  pb.register_ctrl_link("fetch", fetch_ctrl);

  // fetch NORMAL 闭包（既有,不变）
  pb.at_stage("fetch", Phase::NORMAL, [this, &pb]() {
    // 既有代码: read mem_->read_word(pc) → INSTRUCTION
    // 注意: 当 stall 时本闭包不被调用,所以无需内部 if (PTW_ACTIVE)
  });
}
```

**Include 改动**：`ip/cpu/plugins/ibus.h` 头部追加 `#include "ip/mmu/tlm/mmu_keys.h"`。

**关键**：`fetch` 闭包内部**不**写 `if (PTW_ACTIVE) return;`——这是 ADR-040 Tier-1 #4 禁令。stall 由框架控制。

### 3.3 测试

**`tests/mmu/test_ptw_stall_integration.cpp`**（新文件，3 case）：

```cpp
TEST_CASE("ptw_busy_stalls_ibus_fetch", "[mmu][stall]") {
  // 构造 PipelineBuilder,注册 IBusPlugin(mem) + MMUPlugin(sv39)
  // 注入 TLB miss (multi_tlb_->lookup 永远 miss)
  // cycle 0: fetch 应该 skip,INSTRUCTION 应为 stale/0
  // cycle 1..N: fetch 持续 skip,PTW walk 推进
  // cycle N: PTW 完成,fetch 不再 skip
  // 断言: cycle N 时 fetch 读到真 PADDR 对应的 instruction
}

TEST_CASE("no_ptw_no_stall", "[mmu][stall]") {
  // 默认 TLB 全 hit → fetch 不 stall
  // 跑 10 cycle,每个 cycle INSTRUCTION 都被更新
}

TEST_CASE("stall_does_not_block_other_stages", "[mmu][stall]") {
  // PTW busy 时,fetch stall 但 decode/execute/memory/writeback 正常推进
}
```

## 4. Consumer B: HazardPlugin RAW → execute stalled

### 4.1 数据流

```
cycle N (execute 当前指令 + 后继 RAW 依赖):
  ┌─────────────────────────────────────────────────────┐
  │ execute (NORMAL)                                    │
  │   RiscvIntAluPlugin.execute                         │
  │     → dec.rs1_idx, dec.rs2_idx 读 RegFile           │
  │     → 当前指令 rd_idx = 3 (尚未 writeback)          │
  │     → scoreboard_[3] = true                         │
  └─────────────────────────────────────────────────────┘
                              ↓
  cycle N+1 (后继指令进 execute):
  ┌─────────────────────────────────────────────────────┐
  │ decode (NORMAL)                                     │
  │   HazardPlugin.decode                              │
  │     → dec.reads_rs1 && has_raw(rs1_idx)             │
  │     → RAW detected, 不 mark_in_flight              │
  └─────────────────────────────────────────────────────┘
                              ↓
  ┌─────────────────────────────────────────────────────┐
  │ execute (NORMAL)  ← STALLED by HazardPlugin        │
  │   RiscvIntAluPlugin.execute                         │
  │     → 本 cycle 不执行 (skip)                       │
  │     → scoreboard 不变                              │
  └─────────────────────────────────────────────────────┘
                              ↓
  cycle N+2 (前指令 writeback):
  ┌─────────────────────────────────────────────────────┐
  │ writeback (LATE)                                    │
  │   RegFilePlugin.writeback                          │
  │     → 写 x3 = result                                │
  │   HazardPlugin.writeback                           │
  │     → clear_in_flight(3)                           │
  └─────────────────────────────────────────────────────┘
                              ↓
  cycle N+3 (后继进 execute):
  ┌─────────────────────────────────────────────────────┐
  │ execute (NORMAL)  ← UN-STALLED                     │
  │   RiscvIntAluPlugin.execute                         │
  │     → 读 rs1 (x3) = 真值                          │
  │     → 正常推进                                      │
  └─────────────────────────────────────────────────────┘
```

### 4.2 HazardPlugin 改动

`ip/cpu/plugins/hazard.h::build()`：

```cpp
void build(PipeBuilder& pb) override {
  using KeyType = cf::cpu::core::payload::keys<T, sizeof(T) * 8>;

  // decode 闭包（既有）
  pb.at_stage("decode", cf::plugin::Phase::NORMAL, [this, &pb]() {
    // ... 既有代码 (mark_in_flight 等) ...
  });

  // NEW: register CtrlLink for execute stage
  // 命名与 has_active_hazard() 一致 (修订 R2 fix #1)
  auto execute_ctrl = std::make_shared<cf::plugin::CtrlLink>();
  execute_ctrl->halt_when([this] {
    // 当前 tid 的 scoreboard 有任何飞行中且后续指令会读它
    // 缓存自上一 decode 闭包 (last_decoded_hazard_)
    return this->has_active_hazard();
  });
  pb.register_ctrl_link("execute", execute_ctrl);

  // NEW: commit_hook 复位 hazard cache 避免残留假 stall
  // 注意: commit_hook 实际在 pb.run() 末尾 (commit_storages) 触发
  //       等价于"下一 run 起始已复位"; 但 throw 路径会跳过 commit_storages
  //       (见 pb-stall-loop spec Scenario 4) → reset 跳过 → 下一 run 首 cycle
  //       残留 1-cycle 假 stall, 这是已知可接受的 edge case
  pb.register_commit_hook([this]() { this->reset_hazard_cache(); });

  // writeback 闭包（既有）
  pb.at_stage("writeback", cf::plugin::Phase::LATE, [this, &pb]() {
    // ... 既有代码 (clear_in_flight) ...
  });
}
```

**has_active_hazard() 设计**（新增 HazardPlugin 公开方法，命名从 has_pending_raw_for_next_inst 修订为 has_active_hazard）：

```cpp
// 检查 decode 节点的 DECODE 是否与 scoreboard 冲突
// 注意: read decode node 需要在 build() 闭包内通过 pb 拿
// 实现: 通过 member variable 缓存 has_hazard result
//
// 单 thread 假设 (N_THREADS=1): last_decoded_hazard_ 是单成员足够.
// n_threads > 1 时 (SMT 推迟到 Phase 5+):
//   需要改为 std::array<HazardKind, N_THREADS> last_decoded_hazard_{};
//   索引 = this->tid_ (由 factory 端 set_tid 注入, 见 M4G-extend G.X)
//   ADR-045 决策 7: 当前 commit B 不实装 per-tid 隔离,留 TODO 注释
HazardKind last_decoded_hazard_ = HazardKind::NONE;

bool has_active_hazard() const noexcept {  // R2 fix #1 命名统一
  return last_decoded_hazard_ != HazardKind::NONE;
}

// 显式复位: 每个 pb.run() 起始或 stall-loop 起点调用,避免 decode 节点缺失
// 时残留旧值导致假 stall
void reset_hazard_cache() noexcept {
  last_decoded_hazard_ = HazardKind::NONE;
}
```

**更新点**（decode 闭包内）：
```cpp
pb.at_stage("decode", NORMAL, [this, &pb]() {
  // ... 既有代码 ...
  HazardKind hazard = this->has_hazard(dec, tid);
  this->last_decoded_hazard_ = hazard;  // NEW: 缓存 (无 n_decode 时保留旧值)
  // ...
});
```

**Reset 语义**：HazardPlugin 在每个 `pb.run()` 起始调 `reset_hazard_cache()`，由 `Plugin::build()` 注册一个 commit_hook 完成（与既有 cache commit_hook 模式同形）。无 reset 会导致 decode 节点缺失时残留 stale hazard → 假 stall。

### 4.3 测试

**`tests/cpu/integration/test_hazard_stall.cpp`**（新文件，4 case，R2 fix #3 与 tasks §3.1 对齐）：

```cpp
TEST_CASE("addi_addi_add_no_stall", "[cpu-integration][stall]") {
  // addi x1, x0, 5; addi x2, x0, 3; add x3, x1, x2
  // 第 3 条读 x1/x2 (依赖前 2 条), 前 2 条写 x1/x2
  // 预期: 第 3 条 stall 2 cycles, x3 = 8 仍正确
}

TEST_CASE("independent_instrs_no_stall", "[cpu-integration][stall]") {
  // addi x1, x0, 5; addi x2, x0, 3; addi x3, x0, 1
  // 全独立, 无 stall
}

TEST_CASE("stall_does_not_corrupt_x1_x2", "[cpu-integration][stall]") {
  // 复杂 RAW 链: x1 = ... ; x2 = x1 + ...; x3 = x2 + ...
  // 验证 stall 后 x1/x2 真值最终写入 RegFile
}
```

## 5. CtrlLink 绑定策略：shared_ptr 不放宽拷贝约束

### 5.1 当前约束（不变）

`include/cf/plugin/ctrl_link.h:31-32`:
```cpp
CtrlLink(const CtrlLink&) = delete;
CtrlLink& operator=(const CtrlLink&) = delete;
```

**保持不变**。`PluginException` 不在此处定义，详见 §2.4（commit A 同步新增）。

### 5.2 决策：shared_ptr<CtrlLink> 存储

```cpp
void register_ctrl_link(const std::string& stage_name,
                        std::shared_ptr<CtrlLink> ctrl);
private:
  std::unordered_map<std::string, std::vector<std::shared_ptr<CtrlLink>>>
      stage_ctrl_links_;
```

**理由**：
1. **不放松 CtrlLink API 严格性**：保持 `= delete` 拷贝约束
2. **生命周期安全**：Plugin 在 build() 内 `auto ctrl = std::make_shared<CtrlLink>();` 然后 `pb.register_ctrl_link("fetch", ctrl)`，shared_ptr 延长 lambda 捕获对象的生命周期到 PipeBuilder 销毁
3. **内存开销可接受**：每个 Plugin ~1 个 CtrlLink，整个 SoC ~10-20 个，开销 < 1KB
4. **构建期分配，不在 run() 路径**：make_shared 在 `Plugin::build()` 内（注册期），不在 `at_stage` callback 内（执行期），符合 ADR-040 "at_stage 内无动态分配" 精神

### 5.3 测试访问器

```cpp
// 单元测试断言用: 按 stage_name + index 取 CtrlLink
std::shared_ptr<CtrlLink> get_ctrl_link(const std::string& stage_name,
                                       std::size_t index) const;

// 清空所有绑定: 单元测试间隔离
void clear_ctrl_links();
```

### 5.4 不破坏现有 API

`tests/framework/test_ctrl_link.cpp` 11 个 case **零修改**（只用 default ctor + `halt_when`，不触碰拷贝）。

### 5.5 ADR-045 决策摘要

**Decision 1**: PipeBuilder::run() 增加 stall loop，per-stage 检查 CtrlLink。
**Decision 2**: `register_ctrl_link(stage_name, shared_ptr<CtrlLink>)` API 形式。
**Decision 3**: throw_when 在 pb.run() 末尾全局异常抛出（与 halt per-stage 不同）。
**Decision 4**: flush_when / bypass 不框架消费（推迟到 `cpu-pipeline-mispredict`）。
**Decision 5**: CtrlLink 拷贝约束保持 `= delete`，不放松。
**Decision 6**: `PluginException` 在 commit A 同步新增于 `include/cf/plugin/plugin_exception.h`。

## 6. ADR-045 设计

新 ADR：`docs/architecture/adr/ADR-045-plugin-ctrl-link-consumption.md`

**核心 Decision**：

1. **PipeBuilder::run() 增加 stall loop**（commit A 之外的新扩展）
2. **throw_when 在 pb.run() 末尾异常抛出**（不依赖 Plugin 主动消费）
3. **flush_when 不框架自动消费**（推迟到 `cpu-pipeline-mispredict`）
4. **bypass 不框架自动消费**（推迟）
5. **stage binding 用 shared_ptr<CtrlLink>**（不要求 CtrlLink 可拷贝）
6. **`PluginException` 新增**（`include/cf/plugin/plugin_exception.h`），`throw_when` 抛出此类型
7. **HazardPlugin `last_decoded_hazard_` 单 thread only**（n_threads>1 推迟到 SMT Phase 5+）
8. **Canonical at_stage 注册序约束（R2 fix #7）**：MMUPlugin 必须在 IBusPlugin 之前 build（保证 `tlb_lookup_ifetch` 在 `fetch` 之前出现在 canonical_stage_order）。CpuFactory 端通过调用顺序保证；如未来 plugin 注册顺序动态化，需在 `pb.run()` 内显式断言此约束。

## 7. D4 / ADR-040 合规检查

| 约束 | 满足？ | 说明 |
|------|--------|------|
| 无 `void tick()` | ✅ | PipeBuilder run 内 stall loop 不属于 tick；Plugin 仍无 tick |
| 无 `if(cond) return;` | ✅ | Plugin 闭包不变；PipeBuilder 内 `continue` 是循环控制，非早返 |
| 无 `enum class State` + `switch(state_)` | ✅ | 不引入新 FSM |
| Bundle 字段 `uint_t<N>` | ✅ | 不动 Bundle |
| `at_stage` 回调内 `if (cond) { ... }` 全分支包裹 | ✅ | 不改 Plugin 闭包结构 |

## 8. 测试矩阵

| 测试 | 覆盖 | family tag |
|------|------|-----------|
| `test_pipe_builder_stall.cpp` (新) | pb.run() stall loop 12 case | `[framework]` |
| `test_ctrl_link.cpp` (现有 11 case) | CtrlLink API 行为不变 | `[framework]` |
| `test_ptw_stall_integration.cpp` (新) | PTW → fetch stall 3 case | `[mmu]` |
| `test_mmu_plugin.cpp` (现有) | MMUPlugin baseline 行为不变 | `[mmu]` |
| `test_hazard_stall.cpp` (新) | Hazard RAW → execute stall 4 case | `[cpu-integration]` |
| `test_*stage_riscv.cpp` (现有) | 端到端 add.elf → tohost=1 | `[cpu-integration]` |

**预期 baseline**:
- 现有 318 PASS 全部保留
- 新增 19 case（12 + 3 + 4）→ baseline 318 + 19 = **337**（R2 fix #3 统一）
- 0 退化

## 9. Commit 序列

| # | commit | 范围 |
|---|--------|------|
| 0 | `chore(framework): CtrlLink copy constraint → shared_ptr usage` | API 文档说明，不改 .h 文件 |
| A | `feat(framework): PipeBuilder::register_ctrl_link + should_stall_stage + run() stall loop` | 框架层 + 新 framework 测试 |
| B | `feat(cpu): IBusPlugin registers fetch CtrlLink (PTW_ACTIVE halt) + hazard stall` | 消费者 A + B + 测试 |
| C | `feat(mmu): expose PTW stall hook for integration tests` | mmu 测试支撑 |
| D | `feat(cpu): HazardPlugin CtrlLink integration + execute stall` | (在 B 内完成) |
| E | `docs: ADR-045 + CHANGELOG v0.1.3 + tasks complete` | 文档 |
| F | `chore: openspec archive plugin-framework-stall` | 归档 |

合并 B + D 为单 commit（同一 Plugin 改动相邻）。最终 5 commits。

## 10. 风险与未决项

| 风险 | 缓解 |
|------|------|
| `pb.run()` 加 stall loop 改变全 IP 行为 | 318 测试兜底；新 API 默认 no-op（无 CtrlLink 不影响） |
| `has_active_hazard()` 实现复杂度（与单 pass-per-run 流水线语义交互） | tasks.md §3 明确简化版（基于 last_decoded_hazard_ + commit_hook reset）；TDD 在 commit B 暴露 scoreboard 生命周期问题就地解决。后续 Phase 5+ 完整化 |
| `flush_when` 不框架自动消费违反直觉 | 文档明确化；推迟到 `cpu-pipeline-mispredict` |
| **canonical 注册序决定 stall 时效** (R2 fix #5) | `tlb_lookup_ifetch` (MMUPlugin) 必须先于 `fetch` (IBusPlugin) 在 `at_stage` 注册；否则 stall 生效晚 1 cycle，本 change 要修的 stale-read bug 残留。CpuFactory 端需要按 "MMU 先 IBus 后" 顺序注册 plugin，并在 ADR-045 / tasks §2.7 显式声明 |
| **Stall 期间下游 stage 对 stale payload 重复执行** (R2 fix #6) | 本 sim 无 instruction queue，fetch stall 时 decode/execute 继续跑在 stale INSTRUCTION/DECODE 上 → 同一条指令被重复 decode+execute N 次。整数 ALU 指令幂等无副作用（addi/add），所以 add.elf → tohost=1 端到端正确；但 store/CSR/atomic 不幂等。**验证范围限定为整数 ALU 程序**（add.elf + riscv-tests rv32ui 推迟到下一 change 验证） |
| **tlb_lookup_ifetch 在 PTW 期间不 stall**（pre-existing） | 每 cycle 重复 do_lookup → TLB 未 refill → 重复 `start_walk`，ptw_max_inflight=2 限制下行为未分析。**不属本 change 范围**，仅记录 |
| **commit_storages 交互**（throw 路径） | throw 跳过 commit_storages → 连带跳过 array_store commit + hazard reset_hook，1-cycle 假 stall 残留。已知可接受 edge case |

## 11. 参考文献（同 proposal.md §7）

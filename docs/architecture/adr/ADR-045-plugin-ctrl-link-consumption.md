# ADR-045: Plugin CtrlLink 消费契约 + PipeBuilder::run() Stall Loop

| 字段 | 值 |
|------|-----|
| 状态 | ✅ Accepted (2026-09-15, plugin-framework-stall change) |
| 适用范围 | `cf::plugin` 框架层 (include/cf/plugin/) |
| 父 ADR | ADR-025 (Plugin 无 tick) / ADR-033 (CtrlLink 4 API) / ADR-040 (TLM→HDL 移植性) |
| 关联 | SpinalHDL/VexRiscv `stage.arbitration.haltItself` 模式 |

## 1. Context (背景)

`cf::plugin::CtrlLink` 自 `ADR-033` 起即实现 `halt_when/throw_when/flush_when/bypass` 四种控制 API，但**所有 API 在所有 Plugin 中 0 消费者**：

| 原语 | 位置 | 状态 |
|------|------|------|
| `CtrlLink::halt_when` | `ctrl_link.h:34` | ✅ API 已实现 / ❌ 0 消费者 |
| `CtrlLink::throw_when` | `ctrl_link.h:40` | ✅ API 已实现 / ❌ 0 消费者 |
| `CtrlLink::flush_when` | `ctrl_link.h:46` | ✅ API 已实现 / ❌ 0 消费者 |
| `CtrlLink::bypass` | `ctrl_link.h:52` | ✅ API 已实现 / ❌ 0 消费者 |
| `PipeNode::State` 5 态 | `pipe_node.h:36-42` | ✅ 已实现 / ❌ 0 消费者 |
| `PipeArbitration::arb_` | `pipe_node.h:128-129` | ✅ 已实现 / ❌ 0 消费者 |

**DSE 文档期望**（`dse_architecture_v2_design_research.md:219-225`）：

> "框架脊柱...**不需要改**。`commit_storages` hook **已经是** OoO commit 原语。`flush_when` **已经是** mispredict squash 原语。"

但 `pb.run()` 当前是单 pass 无 cycle 区分（`plugin-style-design-methodology-v1.md:118-123`）—— 即使 CtrlLink `should_halt()=true`，stage 回调仍会被调用。本 ADR 兑现**框架级 stall 原语** + **消费者契约**。

## 2. Decision (决策)

### Decision 1: PipeBuilder::run() 插入 stall loop

在 canonical stage 循环外层包 stall check：

```cpp
for (const auto& stage_name : order) {
  if (should_stall_stage(stage_name)) continue;  // NEW
  for (int p_idx = 0; p_idx < 3; ++p_idx) {
    // ... 既有 EARLY→NORMAL→LATE 顺序 ...
  }
}
```

**理由**：D4 框架级抽象。Plugin 仍无 `tick()`，调度逻辑由 PipeBuilder 决定。

### Decision 2: throw_when 框架级异常抛出

`pb.run()` 末尾追加 throw 检查：

```cpp
for (const auto& [stage, ctrls] : stage_ctrl_links_) {
  for (const auto& c : ctrls) {
    if (c && c->should_throw()) {
      throw PluginException(stage, "throw_when condition triggered");
    }
  }
}
```

**理由**：throw 是异常注入的全局信号，与 halt per-stage 性质不同。抛 `PluginException` (新类型，继承 `std::runtime_error`)，抛时跳过 `commit_storages()`。

### Decision 3: flush_when / bypass 不框架自动消费

**理由**：
- flush 目标由 plugin 决定（"清空哪个 stage 的哪个 payload"），框架无法统一语义
- bypass 按 key 存储，plugin 内显式消费更直观
- 与 VexRiscv 模型一致（plugin 自行决定 flush 目标）

推迟到 `cpu-pipeline-mispredict`（flush 真实消费者）和 Phase 5+（bypass forwarding）实装。

### Decision 4: stage binding 用 shared_ptr<CtrlLink>

```cpp
void register_ctrl_link(const std::string& stage_name,
                        std::shared_ptr<CtrlLink> ctrl);
```

**理由**：
- 保持 `CtrlLink::operator= / copy ctor = delete` 不放松
- `shared_ptr` 延长 lambda 捕获对象生命周期到 PipeBuilder 销毁
- 构造时机在 `Plugin::build()` 注册期，**不**在 `at_stage` callback 内（执行期），符合 ADR-040 精神

### Decision 5: PluginException 新增

`include/cf/plugin/plugin_exception.h`：

```cpp
class PluginException : public std::runtime_error {
  explicit PluginException(const std::string& stage_name,
                           const std::string& message);
  const std::string& stage_name() const noexcept;
};
```

**理由**：与 `std::exception` 体系集成，携带 `stage_name` 便于调试。

### Decision 6: Canonical at_stage 注册序约束

**MMUPlugin 必须在 IBusPlugin 之前 build**，保证 `tlb_lookup_ifetch` 在 `fetch` 之前出现在 canonical_stage_order。否则 PTW stall 晚 1 cycle，**本 change 要修的 stale-read bug 残留**。

**实装要求**：CpuFactory 端通过调用顺序保证（典型：先 MMUPlugin 后 IBusPlugin）。如未来 plugin 注册顺序动态化，需在 `pb.run()` 内显式断言此约束（推迟到 Phase 5+ 流水线拓扑动态化时）。

> **⚠️ 修订注解 (2026-09-16, Wave 2 Oracle 评审)**：本 Decision 的约束当前**未满足且操作上 inert**。实际代码中 `CpuFactory::build_cpu()` 在 `TopologyBuilder<5>::expand`（`cpu_factory.h:253-269`，创建 `fetch` 节点）之后注册 MMUPlugin（`cpu_factory.h:306-322`），且 `MMUPlugin::setup()` 用 `declare_substage("fetch", "tlb_lookup_ifetch")` 使 `tlb_lookup_ifetch` 成为 `fetch` 的子阶段——因此 `canonical_stage_order()`（`pipe_builder.h:149-158`，按 `stages_` 首次出现排序）中 `fetch` 排在 `tlb_lookup_ifetch` 之前。这在单次 pass-per-run 模型中**不咬人**：PTW 走查在同一个 `pb.run()` 内完成（`ptw_l0/l1/l2` 阶段晚于 `tlb_lookup_ifetch` 运行），`PTW_ACTIVE` 在下次 `fetch` 入口检查前已被清零，故 stall 机制从未真正触发。加之 IBus/DBus 不消费 MMU 翻译的 PADDR（`ibus.h:76` 直接读 `pc`，`dbus.h:66` 直接读 `MEM_ADDR`），本 stall 原语在 Wave 2 是"mechanism present, inert"。**修复推迟到 Wave 3 `cpu-pipeline-multi-cycle`**（届时多周期 stall 跨越多个 `pb.run()` 使顺序可观察，且 PADDR consumption + PTW real-memory wiring 一起落地）。

### Decision 7: HazardPlugin last_decoded_hazard_ 单 thread only

**当前实装**：单成员 `last_decoded_hazard_` 缓存（cache 上一 cycle decode 闭包的 hazard 检测结果）。

**N_THREADS > 1 推迟**到 Phase 5+ SMT 阶段，改为 `std::array<HazardKind, N_THREADS> last_decoded_hazard_{}`，索引 = `this->tid_`。

**当前 commit 不实装 per-tid 隔离**，但显式记录为 TODO。

### Decision 8: 风险表（已记录于 design §10）

| 风险 | 缓解 |
|------|------|
| `pb.run()` 加 stall loop 改变全 IP 行为 | 318 → 337 PASS（零退化）；新 API 默认 no-op（无 CtrlLink 不影响） |
| `has_active_hazard()` 与单 pass-per-run 流水线语义交互 | commit_hook reset；TDD 在 commit B 暴露 scoreboard 生命周期问题就地解决 |
| `flush_when` 不框架自动消费 | 文档明确化；推迟到 `cpu-pipeline-mispredict` |
| Canonical 注册序决定 stall 时效 | CpuFactory 调用顺序保证（MMU 先 IBus 后） |
| Stall 期间下游 stage 对 stale payload 重复执行 | 验证范围限定为整数 ALU 程序（add.elf + riscv-tests rv32ui 推迟到下一 change） |
| `tlb_lookup_ifetch` 在 PTW 期间不 stall（pre-existing） | 不属本 change 范围，仅记录 |
| `commit_storages` 交互（throw 路径） | 跳过 reset，1-cycle 假 stall 残留，已知可接受 |

## 3. Compliance (合规)

**D4 / ADR-040 检查**：
- ✅ 无 `void tick()` (stall loop 在 PipeBuilder)
- ✅ 无 `if(cond) return;` 于 at_stage 闭包 (stall 由框架 skip 实现)
- ✅ 无 `enum class State` + `switch(state_)` (CtrlLink 条件收集器，非 FSM)
- ✅ 无 `std::optional` / `virtual` / dynamic_alloc 在 `ip/`
- ✅ `if (cond) { ... }` 包裹模式符合
- ✅ shared_ptr 构造在 build() 注册期，不在 at_stage 执行期

**SpinalHDL/VexRiscv 兼容性**：
- `halt_when` 对齐 `stage.arbitration.haltItself`（per-stage、OR 合并）
- OR 合并语义与 chlib `pipeline_stall_ctrl` 一致

## 4. References (引用)

- `include/cf/plugin/pipe_builder.h:100-122, 200-260` — run() + register_ctrl_link
- `include/cf/plugin/ctrl_link.h:34-96` — CtrlLink 4 API
- `include/cf/plugin/plugin_exception.h` — 新增类型
- `docs/architecture/plugin-framework.md:402-418` — CtrlLink OR 合并规范
- `docs/methodology/plugin-style-design-methodology-v1.md:118-123` — D4 pb.run 现状
- `ip/cpu/docs/dse_architecture_v2_design_research.md:219-225` — DSE 原语期望
- `openspec/changes/plugin-framework-stall/` — 本 change 完整 artifacts

## 5. Change Log (变更历史)

- **2026-09-15** (v0.1.3, plugin-framework-stall archived) — 初版 Accepted

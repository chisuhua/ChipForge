# Phase 6d cpu-pipeline-mmufault-handler v1 教训与模式 (D4 + ADR-046 v2.0 双轨实装)

> **沉淀自 cpu-pipeline-mmufault-handler v1 实施期间 (7 commits: `a95b822` → `2d79a48`, 2026-10-06, archive `2026-10-06-cpu-pipeline-mmufault-handler`)**
> **目的**: 收集 CPU pipeline MMU exception handler + HazardPlugin clear_mmufault 实施中的实装模式, 供 wave5+ 真 sv32 e2e 翻转 + HazardPlugin capability negotiate 复用参考。
> **关联文档**:
> - **ADR-045** (CtrlLink halt_when/flush_when): `docs/architecture/adr/ADR-045-ctrllink-pause-halt-flush-throw.md`
> - **ADR-046 v2.0** (FSM 豁免): `docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md` §2.1.1
> - **ADR-082** (Plugin::negotiate): `docs/architecture/adr/ADR-082-plugin-negotiate-capability.md`
> - **OpenSpec archive**: `openspec/changes/archive/2026-10-06-cpu-pipeline-mmufault-handler/`
> - **v2 follow-up**: `openspec/changes/mmufault-verilator-sv32-e2e-flip/` (真 sv32 e2e 翻转, 等 Phase 6d.6 mmu_chmem.h)

---

## 一、D4 + ADR-046 v2.0 双轨实装模式

### 1.1 TLM-side Plugin (`mmu_exception_handler.h`) + CH_MEM-side Plugin (`mmu_exception_handler_chmem.h`) 双文件

```cpp
// ✅ TLM 文件 — at_stage 闭包主逻辑 (TLM runtime per-cycle evaluation)
template <typename T>
class MmuExceptionHandlerPlugin : public cf::plugin::PluginBase {
  void setup(PipeBuilder& pb) override {
    flush_ctrl_ = std::make_shared<cf::plugin::CtrlLink>();
    flush_ctrl_->flush_when([this] { return mmu_exception_pending_; });  // std::function<bool()>
    pb.register_ctrl_link("memory", flush_ctrl_);
  }
  void build(PipeBuilder& pb) override {
    pb.at_stage("memory", Phase::LATE, [this, &pb] {
      auto* n = pb.node_of_logic_stage("memory").get();
      if (n) {
        if (n->operator()(cpu_keys::CPU_EXCEPTION_CODE) != 0) mmu_exception_pending_ = true;
      }
    });
  }
};

// ✅ CH_MEM 文件 — combinational network (elaboration-time ch_bool DAG)
template <typename T>
class MmuExceptionHandlerPlugin : public cf::plugin::PluginBase {
  void setup(PipeBuilder& pb) override { (void)pb; }  // stub (combinational 推迟)
  void build(PipeBuilder& pb) override { (void)pb; }   // stub
};
```

**核心**: TLM 用运行期谓词 (std::function<bool()>), CH_MEM 用 elaboration 期 ch_bool DAG. cpu_factory_chmem.h 用 `#ifndef CF_PLUGIN_USE_CH_MEM` 守护 include + register (TLM-only Plugin 在 CH_MEM binary 不被 include / 不注册).

### 1.2 HazardPlugin::clear_mmufault() API 对齐 (TLM + CH_MEM 双模式 stub)

```cpp
// TLM hazard.h — instance state (mmufault_pending_ 标志)
void mark_mmufault() noexcept { mmufault_pending_ = true; }
void clear_mmufault() noexcept { mmufault_pending_ = false; reset_hazard_cache(); }
bool mmufault_pending() const noexcept { return mmufault_pending_; }

// CH_MEM hazard_chmem.h — stub (combinational RAW 检测无 instance state)
void mark_mmufault() noexcept {}
void clear_mmufault() noexcept {}
bool mmufault_pending() const noexcept { return false; }
```

**核心**: HazardPlugin TLM 版维护 instance state (mmufault_pending_ bool), CH_MEM 版是纯 combinational RAW 检测. mmufault coordination 由 MmuExceptionHandlerPlugin CH_MEM 拉高 stall_ctrl_ (per ADR-046 v2.0 §2.1.1 FSM 豁免, exception path 是 multi-cycle FSM 而非 ad-hoc state machine).

---

## 二、`CPU_EXCEPTION_CODE` Payload Key 流向契约 (3 点路径)

### 2.1 Producer: MMU PTW fault writes `mmu_keys::EXCEPTION_CODE`

```cpp
// ip/mmu/tlm/MMUPlugin.cpp:141 (PTW fault 写入)
(*node)(Key::EXCEPTION_CODE) = fault_code;  // mmu.exception_code (uint8_t)
```

### 2.2 Propagator: `mmu_exit` closure writes `cpu_keys::CPU_EXCEPTION_CODE`

```cpp
// ip/cpu/plugins/mmu.cpp:128-130 (mmu_exit 闭包)
if (node->has(mmu_keys_t::EXCEPTION_CODE)) {
  const std::uint8_t exc_code = node->operator()(mmu_keys_t::EXCEPTION_CODE);
  node->put(cpu_keys_t::CPU_EXCEPTION_CODE, exc_code);  // cpu.exception_code (uint8_t)
}
```

### 2.3 Consumer: `MmuExceptionHandlerPlugin` reads `CPU_EXCEPTION_CODE` + flush pipeline

```cpp
// ip/cpu/plugins/mmu_exception_handler.h (consumer at memory LATE)
pb.at_stage("memory", Phase::LATE, [this, &pb] {
  auto* n = pb.node_of_logic_stage("memory").get();
  if (n) {
    const auto exc_code = n->operator()(cpu_keys::CPU_EXCEPTION_CODE);
    if (exc_code != 0) mmu_exception_pending_ = true;  // 触发 CtrlLink::flush_when
  }
});
```

**核心**: 单向 (MMU → CPU), MMU never reads `CPU_EXCEPTION_CODE`. 3 点路径保证 namespace 隔离 (mmu_keys 在 ip/mmu/tlm/, cpu_keys 在 ip/cpu/tlm/) + Direction 隔离 (写入方向不同: MMU 输出 / CPU 输入).

---

## 三、CtrlLink flush_when 卸载 pipeline 实装模式

### 3.1 HazardPlugin stall_ctrl_ (execute stage halt)

```cpp
// ip/cpu/plugins/hazard.h:201-211 — HazardPlugin::build()
auto execute_ctrl = std::make_shared<cf::plugin::CtrlLink>();
execute_ctrl->halt_when([this] { return this->last_decoded_hazard_ != HazardKind::NONE; });
pb.register_ctrl_link("execute", execute_ctrl);
```

**陷阱** (per mfc A.7 实测): 原实现 `halt_when(has_active_hazard())` 在单 pass 流水线语义下形成死锁 (decode NORMAL 每轮对 writes_rd 指令 mark_in_flight → execute 阶段入口 has_active_hazard() 恒 true → execute 被永久 skip → StageLink execute EARLY (decode→execute 数据复制) 不执行 → writeback 空 DECODE 导致 PC = 0+4 恒卡 0x4). 修复: 仅当本轮 decode 判定当前指令确实读飞行中寄存器才 stall.

### 3.2 MmuExceptionHandlerPlugin flush_ctrl_ (memory stage flush)

```cpp
// ip/cpu/plugins/mmu_exception_handler.h:53-57
flush_ctrl_ = std::make_shared<cf::plugin::CtrlLink>();
flush_ctrl_->flush_when([this] { return mmu_exception_pending_; });
pb.register_ctrl_link("memory", flush_ctrl_);
```

**核心差异**: HazardPlugin 用 `halt_when` (阻止 stage 进入, 新指令 stall), MmuExceptionHandler 用 `flush_when` (stage 已执行指令 flush, 触发 trap handler PC 跳转). 两个 CtrlLink 互不冲突 — HazardPlugin 处理 RAW/WAW (decode-to-execute dependency), MmuExceptionHandler 处理 MMU exception (memory stage exception propagation).

---

## 四、Plugin 注册顺序 (cpu_factory.h canonical ordering 强化)

### 4.1 TLM binary: `register_early_plugins` 内 `if (config.enable_mmu)` 分支

```cpp
// ip/cpu/cpu_factory.h:437-447 (TLM)
pb.register_plugin(std::make_unique<cf::cpu::plugins::RiscvMMUPlugin>(
    sv_mode, mmu_levels, cf::ip::mmu::MMUPlugin::PTWConfig{2},
    satp_value, static_cast<cf::ip::mmu::MemoryInterface*>(mem)));
// cpu-pipeline-mmufault-handler v1 Phase 6: mmu_exit 传播 CPU_EXCEPTION_CODE 后卸载 fault.
pb.register_plugin(
    std::make_unique<cf::cpu::plugins::MmuExceptionHandlerPlugin<U>>());
```

**核心**: MmuExceptionHandlerPlugin 必须在 RiscvMMUPlugin **之后**注册 (依赖 mmu_exit 闭包传播 CPU_EXCEPTION_CODE — RiscVMMUPlugin 的 mmu_exit 闭包先注册).

### 4.2 CH_MEM binary: `cpu_factory_chmem.h::build_cpu` 内 `if (enable_mmu && mmu_mode != "bare")` 分支 + `#ifndef CF_PLUGIN_USE_CH_MEM` 守护

```cpp
// ip/cpu/cpu_factory_chmem.h (CH_MEM)
pb->register_plugin(std::move(dmem));

#ifndef CF_PLUGIN_USE_CH_MEM
// cpu-pipeline-mmufault-handler v1 Phase 5: mmu_exit 传播 CPU_EXCEPTION_CODE 后卸载 fault.
if (enable_mmu.value_or(false) && mmu_mode.value_or("bare") != "bare") {
  pb->register_plugin(
      std::make_unique<cf::cpu::plugins::MmuExceptionHandlerPlugin<T>>());
}
#endif
```

**核心**: `#ifndef CF_PLUGIN_USE_CH_MEM` 守护 include (line 35) + register call (line 312). CH_MEM binary 编译时 TLM 版 Plugin 不被 include, register call 不被编译. MmuExceptionHandlerPlugin CH_MEM 版 (Phase 4.1 stub) 通过 `ip/cpu/plugins/mmu_exception_handler_chmem.h` 提供, 但 cpu_factory_chmem.h 不引用 (当前 mmufault 协调无 instance state, combinational network 推迟到 Phase 6d.6 mmu_chmem.h).

---

## 五、Spec Delta 文档模式 (`## ADDED Requirements` section)

### 5.1 新 Requirement 跨多个 capability spec 分发

```markdown
<!-- openspec/specs/mmu-cache-integration-test/spec.md -->
## ADDED Requirements
### Requirement: MMU exception propagation to CPU pipeline (cpu-pipeline-mmufault-handler v1)
When MMU PTW walk raises a fault, the exception code MUST propagate through
`cpu_keys::CPU_EXCEPTION_CODE` Payload Key to `MmuExceptionHandlerPlugin::at_stage("memory", Phase::LATE)` closure...

#### Scenario: Page fault (code 12) sets CPU_EXCEPTION_CODE + flush
- **WHEN** `RiscvMMUPlugin::at_stage` walks L0/L1 PTE and finds V=0
- **AND** `mmu_exit` closure (`ip/cpu/plugins/mmu.cpp:128-130`) writes `cpu_keys::CPU_EXCEPTION_CODE = 12`
- **THEN** `MmuExceptionHandlerPlugin::at_stage("memory", Phase::LATE)` MUST consume code 12 and set `mmu_exception_pending_ = true`
```

**核心**: 新 Requirement 通过 `## ADDED Requirements` section (而非修改既有 Requirement) 添加到现有 spec — 保持 baseline 不破坏, change archive 时 merge 到 baseline.

### 5.2 TBD Purpose section 升级为具体 spec

```markdown
<!-- openspec/specs/cpu-mmu-exception-routing/spec.md (前 TBD) -->
## Purpose
MMU exception routing through `cpu_keys::CPU_EXCEPTION_CODE` Payload Key from MMU
PTW fault to CPU pipeline exception handler. Producer is `ip/mmu/tlm/MMUPlugin.cpp:139-141`;
propagator is `ip/cpu/plugins/mmu.cpp:128-130` `mmu_exit` closure; consumer is
`ip/cpu/plugins/mmu_exception_handler.h` `MmuExceptionHandlerPlugin::at_stage("memory", Phase::LATE)`.
Direction: MMU → CPU (one-way, MMU never reads CPU_EXCEPTION_CODE).
```

**核心**: Purpose section 从 `TBD - created by archiving change ...` 升级为具体 spec, 列出 3 点路径 + Direction, 让 reader 立即理解 producer/propagator/consumer 关系.

---

## 六、测试完备性矩阵

| 测试层级 | 测试用例 | Phase | 状态 |
|---------|---------|-------|------|
| Unit | `tests/cpu/test_mmu_exception_handler.cpp` (4 cases) | Phase 3.4 | ✅ 4/4 PASS |
| Integration | `[cpu-l1-mmu-demo]` 6/6 (cfg.enable_mmu=false workaround) | Pre-existing | ✅ 6/6 |
| Integration | `[cpu-integration]` 81 cases / 65725 + 11 pre-existing from mfc | Pre-existing | ⚠️ 11 pre-existing |
| MMU | `[mmu]` 53/53 | Pre-existing | ✅ 53/53 |
| Verilator | `[verilator]` 1/1 (chipforge_tests_chmem binary) | Phase 6.3 | ✅ 1/1 |
| CH_MEM | `[chmem]` 37/37 | Phase 10.2 | ✅ 37/37 |

**核心**: Phase 3 unit test (4/4) + Phase 6 cpu_factory 联动 (CPU_EXCEPTION_CODE path validated) + Phase 10 全 family 0 regression (排除 mfc pre-existing 11 failures).

---

## 七、Archive Prep 5 步 (v1 tasks.md Phase 11)

1. **11.1 tasks.md checkbox 全勾选** (43 tasks: Phase 1-12)
2. **11.2 openspec change validate** (`openspec validate --changes` verb-first 命令)
3. **11.3 v0100-bootstrap.sh review §honesty_audit** 数字确认 (基于 HEAD commit 实测, 符合"数字维护原则 v0.10.0 起")
4. **11.4 commit** (1 个 polish commit: spec delta + doc sync + plugin 双文件 + factory 联动 + test + tasks checkbox)
5. **11.5 openspec archive --yes** (先移除 frontmatter `status: placeholder`, 然后 archive command verb-first)

**陷阱**: OpenSpec v1.4.1 不再支持 `openspec change ...` 子命令 (deprecated), 必须用 verb-first (`openspec archive`, `openspec validate --changes`). deprecation warning 可忽略.

---

## 八、Phase 4-12 后续工作 (Post-Archive)

| Task | Status | 描述 |
|------|--------|------|
| MmuExceptionHandlerPlugin CH_MEM combinational network | ⏸ 推迟 | mmu_chmem.h 实装 (Phase 6d.6) 后启用, 读 CPU_EXCEPTION_CODE → 输出 mmufault_clear ch_bool 到 HazardPlugin stall_ctrl_ |
| MmuExceptionHandlerPlugin CH_MEM 在 cpu_factory_chmem.h 注册 | ⏸ 推迟 | 当前 stub 不被 include (TLM-only 在 CH_MEM binary 被 #ifndef 守护跳过) |
| 真 sv32 e2e 翻转 (v2 follow-up) | ⏸ 等 Phase 6d.6 | `mmufault-verilator-sv32-e2e-flip` change 启动条件 = mmu_chmem.h 实装 |
| ADR-082 v2 升级 (mmufault capability) | ⏸ 待 ADR owner | HazardPlugin::clear_mmufault() 隐式声明 capability, ADR-082 v2 需补全 negotiate() 接口 |

---

## 九、关键决策记录

| 决策 | 选择 | 理由 |
|------|------|------|
| MMU exception routing via Payload Key | 单向 MMU → CPU | namespace 隔离 + Direction 隔离 (cf::ip::mmu::payload vs cf::cpu::tlm::payload) |
| flush_when vs halt_when | flush_when for exception (unload pipeline) vs halt_when for hazard (stall new instr) | semantics 区分: flush 卸载已执行指令, halt 阻止新指令 |
| MmuExceptionHandlerPlugin at "memory" LATE | consumer 在 memory stage (mmu_exit 在 memory) | stage timing: mmu_exit 在 memory LATE 写入, MmuExceptionHandler 在 memory LATE 消费 (同 stage LATE 顺序由 register 顺序保证) |
| TLM-only include guard in cpu_factory_chmem.h | `#ifndef CF_PLUGIN_USE_CH_MEM` 双向守护 | CH_MEM binary 编译时 Plugin 不存在 → register call 不被编译 |
| MmuExceptionHandlerPlugin CH_MEM 推迟 | combinational network 推迟到 mmu_chmem.h Phase 6d.6 | 当前 EXCEPTION_CODE 恒 0 (CH_MEM 无 MMU 实装), network 没 consumer |
| HazardPlugin clear_mmufault() API 对齐 (TLM instance + CH_MEM stub) | 双文件 + API 对齐 | TLM 维护 mmufault_pending_ bool, CH_MEM combinational — 两者语义一致, API 对齐让 MmuExceptionHandler 调用 contract 统一 |

---

## 十、与 wave5 后续工作的契约

| 契约 | 接收方 | 描述 |
|------|--------|------|
| `HazardPlugin::clear_mmufault()` API | mfc-pipeline-multi-cycle-fsm | mul_fsm 在 hazard 重试循环检测到 MMU exception 时可调 (per A.7 B1 bug) |
| `MmuExceptionHandlerPlugin` TLM 注册路径 | cpu_factory.h::register_early_plugins | enable_mmu && mmu_mode != "bare" 时自动注册, CPU Factory caller 无需手动注册 |
| `CPU_EXCEPTION_CODE` Payload Key 流向 | mfc, cache, mmucache-integration | MMU → mmu_exit → MmuExceptionHandler 单向, 任何 Plugin 不得反向读取 |
| CtrlLink::flush_when(`memory` stage) | future trap handler Plugin | trap handler 完成 mepc/mcause 写后调 `MmuExceptionHandlerPlugin::reset()` 复位 pending |
| ADR-082 capability negotiate | future Plugin author | MmuExceptionHandlerPlugin CH_MEM combinational 实装时, 应通过 CapabilityTable 提供 `mmu_exception_handler` capability 给 HazardPlugin consume |

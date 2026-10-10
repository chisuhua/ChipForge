---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
depends_on:
  - mfc-cpu-pipeline-multi-cycle-fsm  # Phase E + G (G deferred to v0.11.0 per mfc-defer D'); H 阶段与本 change 并行
revision_history:
  - date: 2026-10-09
    author: rdd-planner (Sisyphus-orchestrated, Oracle 2026-10-09 P1.5 deps 验证修订)
    summary: |
      解除 P1.5 依赖环 (BLOCKED 状态). 原 §1.2 硬门禁 "Phase H archive 后才启动"
      与 `2026-10-07-mfc-defer-v0.11.0` (已 archive) D' 决策 (G.2/H defer v0.11.0) 互锁.
      修订为 v0.11.0 启动期与 mfc-cpu-pipeline-multi-cycle-fsm 并行, 仅需 Phase E 完成 (riscv-tests
      rv32um 8/8) + Phase G 软基线 (deferred to v0.11.0, 见 mfc-defer D' note 2).
      priority P2 → P1 (per .rddf/improvements/mfc-extract-fsm-h.md + objective-v0110-launch §8 Sprint 1).
---

# Change: mfc-extract-fsm-h

> **OpenSpec change**: `mfc-extract-fsm-h` (proposed)
> **Initiative**: `wave5-isa-coverage-and-bp` (PoC-1 follow-up)
> **Depends on**: `mfc-cpu-pipeline-multi-cycle-fsm` Phase E (rv32um 8/8) + Phase G 软基线 (deferred to v0.11.0)
> **Authors**: ChipForge Plugin Team
> **Created**: 2026-09-30
> **Last revised**: 2026-10-09 (P1.5 deps 修订, Oracle 验证)
> **Status**: 📋 PROPOSED (v0.11.0 启动期, 与 mfc 并行)

---

## Why

### 1.1 当前状态 (2026-09-30, mfc Phase D.1 落地后)

mfc-cpu-pipeline-multi-cycle-fsm change Phase D.1 落地了 `ip/cpu/plugins/mul_div_fsm_chmem.h` (CH_MEM 配对占位),但 CH_MEM DSL 代码 (create_fsm / opcode_sig_ / counter_reg_ / result_reg_ / busy_cycles_reg_) 仍内联在 `ip/cpu/arch/riscv/mul_div_fsm.h` `#ifdef CF_PLUGIN_USE_CH_MEM` 块 (line 278-456, ~180 LOC)。

**当前问题**:

1. **违反 ADR-040 v2.0 双文件分离精神**: TLM 文件 (`mul_div_fsm.h`) 含 `ch_uint`/`ch_reg`/`ch_state_machine`,即使在 `#ifdef` 块内。CI grep 虽豁免 `_chmem.h` 文件, 但 TLM 文件内 `#ifdef` 块仍是技术债。
2. **复用难**: 业务 Plugin 想复用 ch_state_machine DSL 模板, 需要 include 整个 `mul_div_fsm.h` (含 TLM 逻辑), 不是干净 reusable header。
3. **Phase D.4 工作量大**: 涉及私有成员重组织 + API 兼容性维护 + 测试迁移 (`test_chmem_multi_cycle_fsm.cpp` + `test_cycle_parity.cpp` 改 include 路径), 单 PR 不适合阻塞主线。

### 1.2 启动时机约束 (修订 2026-10-09, Oracle P1.5 deps 验证)

本 change **在 v0.11.0 启动期与 `mfc-cpu-pipeline-multi-cycle-fsm` 并行**, 启动条件修订如下:

| Phase | 修订前要求 | 修订后要求 | 修订理由 |
|-------|-----------|-----------|----------|
| **Phase E** | 必须完成 (rv32um 8/8 PASS) | **保持** | rv32um 8/8 是 PoC-1 硬指标, 不可让步 |
| **Phase G** | 必须完成 (Dhrystone baseline) | **defer to v0.11.0, 软基线** | `2026-10-07-mfc-defer-v0.11.0` D' 决策 note 2: G.2 实测 FAIL 已 defer v0.11.0; 本 change 性能契约由 Phase 4 VERIFY (radix-2 cycle 数核对) 间接验证 |
| **Phase H** | mfc change 必须先 archive | **不要求先 archive, 与本 change 并行** | 原约束与 mfc-defer D' 决策 (H 阶段亦 defer v0.11.0) 互锁, 形成依赖环; 修订为并行 |

**风险评估**: 与 mfc change 并行, 可能出现 1-2 天双向 PR 阻塞 (P0#1 canonical-ordering 期间经验证可解, 双向 PR 不致命); 接受此风险, 优先级 P1 (per `.rddf/improvements/mfc-extract-fsm-h.md`).

**冲突解决**: 如并行期出现 mfc 本体 archive 紧迫需求, 本 change 临时切 feature 分支独立推进, 待 mfc archive 后回 main 同步. rdd-workflow P2 worktree 模式原生支持此场景.

---

## What Changes

### 2.1 提取独立 `fsm.h`

**新增文件**: `include/cf/plugin/multi_cycle_fsm.h` (框架级, 不是 IP 级)

```cpp
// include/cf/plugin/multi_cycle_fsm.h
//
// 多周期 FSM 通用模板 (ADR-046 v2.0 §2.1.1 算术多周期 FSM 豁免配套)
// 复用 ch_state_machine DSL, TLM 和 CH_MEM 双模式共享同一 FSM 描述
//
// 约束:
//   - 仅在 #ifdef CF_PLUGIN_USE_CH_MEM 下提供 CH_MEM DSL 块
//   - TLM 模式用 ad-hoc enum class + switch (向后兼容 ADR-048)
//   - 必须提供 negotiate() capability (ADR-082 集成)
//
namespace cf::plugin::multi_cycle_fsm {

template <typename StateEnum, std::size_t NStates>
class FsmBase {
  static_assert(std::is_enum_v<StateEnum>, "StateEnum must be enum class");
  StateEnum state_ = /* default first state */;

public:
  // TLM 模式 API
  StateEnum state() const noexcept;
  void tick();  // ad-hoc enum switch

#ifdef CF_PLUGIN_USE_CH_MEM
  // CH_MEM 模式 API (ch_state_machine DSL)
  chlib::ch_state_machine<StateEnum, NStates> ch_fsm_;
  void create_chmem_fsm();
  ch_uint<...> state_out();
#endif

  // 公共 API
  void negotiate(CapabilityTable& cap) {
    cap.provide("multi_cycle_fsm", this);
  }
};

}  // namespace cf::plugin::multi_cycle_fsm
```

### 2.2 重构 MulDivFsmPlugin

**修改文件**:
- `ip/cpu/arch/riscv/mul_div_fsm.h`: 移除 `#ifdef CF_PLUGIN_USE_CH_MEM` 块 (line 278-456), 改为继承 `cf::plugin::multi_cycle_fsm::FsmBase<State, 4>`
- `ip/cpu/plugins/mul_div_fsm_chmem.h`: 删除 (CH_MEM 代码已上提到 `multi_cycle_fsm.h`), 改为重新 include 框架版
- `tests/framework/test_chmem_multi_cycle_fsm.cpp`: 改 include 路径 (`ip/cpu/arch/riscv/mul_div_fsm.h` → `cf/plugin/multi_cycle_fsm.h`)
- `tests/framework/test_mul_div_fsm_cycle_parity.cpp`: 同步更新

### 2.3 验证门禁

- `[cpu]` 全 PASS (无回归)
- `[framework]` 全 PASS (含 [chmem][multi-cycle] 5/5)
- `check_plugin_portability.sh` 12/12 PASS (TLM 文件彻底无 ch_* 渗透)
- ADR-040 v2.0 严格分离达成 (TLM 文件 `<name>.h` 不含 ch_mem)

---

## Acceptance Criteria

- [ ] AC-1: `ip/cpu/arch/riscv/mul_div_fsm.h` 不含 `ch_uint` / `ch_reg` / `ch_state_machine` 任何字面 (用 grep 验证)
- [ ] AC-2: `include/cf/plugin/multi_cycle_fsm.h` 提供 `FsmBase<StateEnum, NStates>` 模板, 业务 Plugin 可继承
- [ ] AC-3: MulDivFsmPlugin 继承 FsmBase 后, [cpu] 125/125 PASS + [framework][chmem][multi-cycle] 5/5 PASS (无回归)
- [ ] AC-4: ADR-040 v2.0 严格分离落地, TLM-only 文件 grep `ch_uint|ch_reg|ch::core` = 0 hits
- [ ] AC-5: ADR-082 negotiate API 仍工作 (MulDivFsmPlugin::negotiate override 通过 FsmBase::negotiate 提供 multi_cycle_fsm capability)

---

## Risks & Rollback

### R1: API 兼容性破坏

- **信号**: MulDivFsmPlugin::create_fsm() / state_out() / opcode() 等签名变化
- **缓解**: 提供 typedef 兼容层 (`using old_name = new_name`), deprecate 1 个 version 后删除

### R2: 测试 include 路径大量改动

- **信号**: test_chmem_multi_cycle_fsm.cpp + test_cycle_parity.cpp 等 5+ 测试文件需改 include
- **缓解**: 批量改 + 立即编译 + 立即测试

### 回退

- git revert 即可 (独立 commit, 不与其他 change 交织)

---

## Related

- **ADR-040 v2.0**: TLM→HDL 移植性约束 (双文件分离)
- **ADR-046 v2.0**: FSM 豁免 + ch_state_machine DSL 强约束
- **ADR-082**: Plugin::negotiate() capability 协商
- **mfc-cpu-pipeline-multi-cycle-fsm** tasks.md D.4 段: 原始 follow-up 标记

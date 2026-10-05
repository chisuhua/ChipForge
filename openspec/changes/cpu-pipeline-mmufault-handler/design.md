## Context

### 现状（v0.10.4 hotfix 后, 2026-10-04）

`verilator-mmu-bare-plumbing-e2e` (Change 2a) §Non-Goals NG1 显式声明: **不**验证 sv32 translation 语义、PTW walk、page fault 处理。这是诚实的 plumbing-only 范围,测试名 `-bare-plumbing-` 即声明。

但 AGENTS.md 明确记录真 sv32 翻译需要:
1. `cpu_factory.h:390` 硬写 `/*satp_value=*/0` 让 `cfg.mmu_mode="sv32"` config inert (v0.10.0 hotfix Phase C 部分修但 cpu_factory.h 仍未跟随)
2. CPU pipeline 在 vaddr=0 PTW fault 后陷入 hazard 重试循环 (无 exception handler 卸载 fault)
3. AGENTS.md 原文: "真 sv32 translation 测试需 (a) `cpu-pipeline-mmufault-handler` change (CPU pipeline 加 MMU exception handler, P1 priority), 然后 (b) flip `cfg.enable_mmu=true` 在本测试"

### Phase 6d 集成背景

- 6d.6 (MMU/PTW FSM) — `ip/cpu/plugins/mmu_ptw_chmem.h` 已实装 sv32 5 状态 FSM (IDLE/L0_WAIT/L1_WAIT/DONE/FAULT)
- 6d.7 (L1Cache refill FSM) — `ip/cache/tlm/l1_cache_refill_fsm_chmem.h` 已实装 4 状态 FSM (IDLE/LOOKUP/MISS/REFILL_WAIT)
- 6d.5 (Verilator integration) — `tools/verilator_runner/cpu_verilator_sim.cpp` 已实装 E8 降级 tohost=1 验证

### CPU pipeline 当前 MMU exception 处理

`ip/cpu/plugins/` 文件清单 (不含 MMU exception handler):
- `decode_chmem.h` / `decode.h` (RISC-V decode)
- `branch_chmem.h` / `branch.h`
- `hazard_chmem.h` / `hazard.h`
- `ibus_chmem.h` / `ibus.h`
- `dmem_chmem.h` / `dmem.h`
- `reg_file_chmem.h` / `reg_file.h`
- `mmu.cpp` (RiscvMMUPlugin, **不含** exception handler)
- `mmu_ptw_chmem.h` (Phase 6d.6 PTW FSM)
- `payload_common.h` / `payload_riscv.h` (Payload Key 定义)

`payload_common.h` 已有 `cpu_keys::CPU_EXCEPTION_CODE` Payload Key 字段定义（声明空间预留,无消费方）。

### 当前 sv32 e2e 测试覆盖

- TLM 模式: `[mmu]` 53/53 PASS, 含 `test_ptw_tlb_refill_integration` (用 `satp_ppn_` workaround 跳过真 sv32 翻译, AGENTS.md 明确 "5 ELF baseline 0 不退化")
- CH_MEM 模式: `test_mmu_ptw_fsm_chmem.cpp` 测 FSM 状态机本身 (5 状态转换), **不挂 CPU pipeline**
- Verilator 模式: Change 2a `[mmu-verilator]` 3/3 PASS (plumbing-only), TEST_CASE 3 sv32+ppn=0 边界防护

### Stakeholders

- **Change 2a (verilator-mmu-bare-plumbing-e2e)**: 硬阻塞, archive 后才启动本 change
- **Change 1 (verilator-cpu-factory-extensible-params)**: 间接依赖 (通过 Change 2a)
- **wave5 mfc-... Phase G "DMIPS/MHz ≥1.4"**: 本 change archive 后 mfc 可消费 sv32 路径
- **3 架构门禁**: 必须 0 失败

## Goals / Non-Goals

### Goals

1. **G1**: 实装 `MmuExceptionHandlerPlugin` (TLM + CH_MEM), 监听 `mmu_keys::EXCEPTION_CODE`, 写 `cpu_keys::CPU_EXCEPTION_CODE`, 触发 `CtrlLink` flush
2. **G2**: 修复 `cpu_factory.h:390` 硬写 `/*satp_value=*/0`, 让 `cfg.mmu_mode="sv32"` config 真正透传
3. **G3**: `cpu_factory_chmem.h::build_cpu` 在 `enable_mmu=true && mmu_mode != "bare"` 时注册 `MmuExceptionHandlerPlugin`
4. **G4**: Hazard retry 循环清理 (新增 `HazardPlugin::clear_mmufault()` API)
5. **G5**: trap handler PC 跳转到 `mtvec` (简化: hardcode 0x80000010, 后续 `mfc-cpu-pipeline-multi-cycle-fsm` 完整 CSR 写)
6. **G6**: 承接 Change 2a follow-up: 新增 TEST_CASE 6 `mmu_sv32_translation_verilator_e2e_flipped`, 验证真 sv32 translation 跑通 tohost=1
7. **G7**: 零回归: `[mmu-verilator]` 3/3 → 4/4 PASS (TEST_CASE 6 加入), `[mmu]` 53/53 + `[cpu-integration]` 81/81 + `[cpu]` 19/19 + `[cpu-l1-mmu-demo]` 6/6 全部不变
8. **G8**: 3 架构门禁 0 失败

### Non-Goals

1. **NG1**: **不**实装 `mfc-cpu-pipeline-multi-cycle-fsm` 的完整 CSR 写 + `mtvec` 配置 (本 change 简化 hardcode `0x80000010`)
2. **NG2**: **不**实装完整 CSR module (CSR `mcause`, `mtval`, `mepc` 写), 仅写 `CPU_EXCEPTION_CODE` Payload Key 触发 trap
3. **NG3**: **不**修改 PTW FSM (`mmu_ptw_chmem.h` 6d.6 已实装 sv32 5 状态), 仅消费其 `EXCEPTION_CODE` 输出
4. **NG4**: **不**修改 L1Cache refill FSM (`l1_cache_refill_fsm_chmem.h` 6d.7), 仅消费 cache flush 信号联动
5. **NG5**: **不**实装完整 supervisor mode (S-mode) / user mode (U-mode) 切换, 仅 M-mode trap
6. **NG6**: **不**实装 interrupt controller (PLIC/CLINT), 仅 synchronous exception
7. **NG7**: **不**升级 CI 门禁 (保留 skip-when-absent, 与 Change 1/2a/2b 一致)
8. **NG8**: **不**扩展 7-stage superscalar (沿用 Phase 6d.8 显式排除)

## Decisions

### D1: `MmuExceptionHandlerPlugin` 走 `at_stage` 闭包而非 enum class State

**Decision**: 异常处理用 `at_stage("memory", Phase::LATE, ...)` 闭包消费 `mmu_keys::EXCEPTION_CODE`, 触发 `CtrlLink::flush_when(...)`。**不**新增 `enum class State { IDLE, TRAP_PENDING, ... }` + switch。

**Rationale**:
- D4 业务 Plugin 禁止状态机 (per verify_plugin_decision.sh Check 2)
- ADR-045 CtrlLink 4-control-API 已支持 flush_when
- 闭包内逻辑简洁: 检查 exception_code → 写 CPU_EXCEPTION_CODE → flush → 清 hazard

**Alternatives considered**:
- (A) enum class State FSM: 违反 D4, ❌
- (B) CtrlLink flush_when 闭包 (✓): 符合 D4 + ADR-045
- (C) ADR-046 v2.0 ch_state_machine DSL 例外: 仅在 trap handler PC 跳转逻辑使用, 主逻辑仍走 (B)

### D2: TLM + CH_MEM 双文件分离

**Decision**: `ip/cpu/plugins/mmu_exception_handler.h` (TLM) + `ip/cpu/plugins/mmu_exception_handler_chmem.h` (CH_MEM)。

**Rationale**:
- ADR-040 v2.0 双模约束 (`check_plugin_portability.sh` Check 2/7 强制)
- 与 `branch_chmem.h` / `branch.h` / `hazard_chmem.h` / `hazard.h` 等现有双文件模式一致

**Alternatives considered**:
- (A) 单文件 `#ifdef CF_PLUGIN_USE_CH_MEM` 包裹: 违反 ADR-040 v2.0 + 9 项 CI 检查, ❌
- (B) 双文件 (✓): 与现有 7 plugin 模式一致

### D3: trap handler PC 简化 hardcode `0x80000010`

**Decision**: trap handler PC 硬编码 `0x80000010`, 暂不读 CSR `mtvec`。

**Rationale**:
- 本 change 核心是 CPU pipeline 修复, 不是 CSR module 实装 (mfc-... scope)
- hardcode PC 0x80000010 与 riscv-tests sv32 PTE 程序约定地址一致 (test_sv32_pte.S 默认 trap handler 在 0x80000010)
- 简化降低 CPU pipeline 改动 scope

**Alternatives considered**:
- (A) 完整 CSR `mtvec` 读: scope 失控, 属 mfc-... / 完整 CSR module change, ❌
- (B) hardcode 0x80000010 (✓): 与 riscv-tests sv32 PTE ELF 约定一致, scope 可控

### D4: HazardPlugin::clear_mmufault() 新 API 而非复用现有 clear API

**Decision**: 在 `HazardPlugin<T>` 新增 `void clear_mmufault()` 方法, MmuExceptionHandlerPlugin 闭包内调用。

**Rationale**:
- 现有 `HazardPlugin::clear_*()` API 是 clear exception (但 mmufault 不算"exception", 是 hazard retry loop 清理)
- 新 API 语义明确: 清除 mmufault 引起的 hazard retry counter
- ADR-082 (Drafting) negotiate() 后续可声明 mmufault capability

**Alternatives considered**:
- (A) 复用现有 clear(): 语义不清, ❌
- (B) 新 clear_mmufault() API (✓): 语义明确, 给 ADR-082 留 hook

### D5: TEST_CASE 6 新增而非修改 TEST_CASE 1-5

**Decision**: 在 `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` 追加 TEST_CASE 6, 不修改 TEST_CASE 1-5。

**Rationale**:
- 现有 TEST_CASE 1-5 已是 plumbing-only, 范围清晰
- TEST_CASE 6 是承接 Change 2a follow-up, 名字 `mmu_sv32_translation_verilator_e2e_flipped` 即声明范围
- AGENTS.md `[mmu-verilator]` 数字 3/3 → 4/4 升级, 不退化

**Alternatives considered**:
- (A) 修改 TEST_CASE 1-5 加 sv32 翻译断言: 破坏 plumbing-only 范围, ❌
- (B) 新 TEST_CASE 6 (✓): 范围清晰, 数字升级

### D6: skip-when-absent 沿用 Change 1/2a/2b 模式

**Decision**: TEST_CASE 6 顶部用 `std::ifstream bin(CF_VERILATOR_SIM_BIN); if (!bin.is_open()) { SUCCEED("skip"); return; }` 模式。

**Rationale**:
- 与 Change 2a `tests/mmu/test_mmu_bare_plumbing_verilator.cpp:skip` 完全一致
- AGENTS.md v0.10.0 维护原则: "skip 不计入实测数字"

### D7: sv32 PTE 程序 ELF vendor `tests/cpu/manual_elf/build_sv32_pte.S`

**Decision**: 新增 vendor 入口, 含 `csrw satp, ...` 指令 + L0/L1 PTE 建立 + trap handler 在 0x80000010。

**Rationale**:
- 现有 5 ELF (add/addi/auipc/beq/jal) 无 sv32 translation 路径
- vendor 入口与 Change 1 `build_mmu_bare.S` 同模式 (build_manual_elf.sh 模板注册)

**Alternatives considered**:
- (A) 复用现有 5 ELF: 无 sv32 翻译, ❌
- (B) 新 vendor `build_sv32_pte.S` (✓): 与 Change 1 同模式

## Risks / Trade-offs

### R1: CPU pipeline hazard retry 循环修复复杂度

**Risk**: CPU pipeline 缺 MMU exception handler 是 v0.10.0 hotfix 已知缺口, 修复涉及 hazard + flush + trap PC 跳转联动, 估时 +1 周。

**Mitigation**:
- tasks.md §1 Spike 1.1 先验证 CPU pipeline 当前 hazard retry 行为 (复现 AGENTS.md 描述的 "vaddr=0 PTW fault → hazard 循环")
- 分阶段实装: 先 minimum HazardPlugin::clear_mmufault() → 再 CtrlLink flush → 最后 trap PC 跳转
- 每个阶段独立测试 (TDD 5 步)

### R2: Phase 6d.6/6d.7 FSM 集成

**Risk**: `MmuExceptionHandlerPlugin` 闭包消费 `mmu_ptw_chmem.h` (6d.6) 的 `EXCEPTION_CODE` + `l1_cache_refill_fsm_chmem.h` (6d.7) 的 cache flush 信号, FSM 状态机交互复杂。

**Mitigation**:
- ADR-046 v2.0 豁免范围明确: 仅 6d.6 PTW FSM + 6d.7 refill FSM 走 `ch_state_machine` DSL, 本 change 闭包不引入新 FSM
- `mmu_keys::EXCEPTION_CODE` Payload Key 已在 mmu_ptw_chmem.h 实装, 本 change 仅消费
- `cache_keys::FLUSH` (或类似) Payload Key 联动 6d.7 FSM

### R3: `[cpu-integration]` 81/81 不退化压力

**Risk**: 引入 MMU exception handler 影响 CPU pipeline 5-stage + 7-stage superscalar + 10-stage 全部流水线, [cpu-integration] 81/81 不退化是硬约束。

**Mitigation**:
- tasks.md §11 Regression Final Check 显式 `[cpu-integration] 81/81 PASS` 不退化验证
- 若任一 case 退化, 走 Phase 6d.8 显式排除 7-stage superscalar config (类似现有模式)
- 接受标准 cap = 81/81 全部不变 (与 Change 2a `[mmu-verilator] 3/3` 一致)

### R4: 真 sv32 translation e2e cycle cap 不确定

**Risk**: sv32 PTE 程序 cycle 数可能远高于 5 ELF baseline (因 PTW walk 多 cycle + trap handler PC 跳转开销), baseline × 1.5 cap 可能不足。

**Mitigation**:
- tasks.md §2 Spike 2.1 先跑 baseline 表生成 (类似 Change 2a §1.2)
- cap 接受标准 cycle ≤ baseline × 1.5 (放宽至 Change 2a × 1.2 的 1.25 倍)
- 若 cycle 异常, 通过 `test_mmu_ptw_fsm_chmem.cpp` FSM 行为排查

### R5: `cpu_factory.h:390` 修改影响 TLM 模式工厂 ABI

**Risk**: 删除硬写 `/*satp_value=*/0` 影响 TLM 模式 5 调用方 ABI 兼容。

**Mitigation**:
- tasks.md §5 显式 5 调用方零修改 ABI smoke test (类似 Change 1 §2)
- `cpu-factory-satp-mapping` change 已实装 helpers + ctor propagation, 本 change 仅替换硬写 0 → `cpu_config.satp_value`

### Trade-off: 真 sv32 e2e vs Hazard clear API scope

**Trade-off**: HazardPlugin::clear_mmufault() 新 API 增加 HazardPlugin 复杂度, 但不引入则 CPU pipeline hazard retry 循环无法清理。

**Decision 接受**: 新 API 必要性高, ADR-082 后续可声明 mmufault capability, scope 可控

## Migration Plan

### Phase 1: Spike (Pre-flight, TDD Step 1)

```bash
# 验证 Change 2a archive
openspec change validate verilator-mmu-bare-plumbing-e2e  # PASS
# 复现 hazard retry 循环
./build/bin/cpu_verilator_sim --elf <sv32_test>.elf --enable-mmu --mmu-mode sv32
# 预期: 卡死 / SEGV / cycle > 2000 不退出
# 记录 baseline 行为供后续对比
```

### Phase 2: cycle Baseline 表生成 (TDD Step 2)

```bash
# sv32 PTE 程序 × 5 runs × median
for i in 1 2 3 4 5; do
  ./build/bin/cpu_verilator_sim --elf tests/cpu/manual_elf/sv32_pte.elf \
    --enable-mmu --mmu-mode sv32 --cycles 5000 2>/dev/null | grep CYCLES
done | sort -n | awk '{a[NR]=$1} END {print a[int(NR/2)]}'
# 追加到 Change 2a baseline CSV
```

### Phase 3: HazardPlugin clear_mmufault() API 实装

```cpp
// ip/cpu/plugins/hazard.h
class HazardPlugin {
  // 现有 API ...
  void clear_mmufault();  // 新增
};

// ip/cpu/plugins/hazard_chmem.h
// CH_MEM 版同步实装 (在 at_stage 闭包内)
```

### Phase 4: MmuExceptionHandlerPlugin 实装 (TLM + CH_MEM)

```cpp
// ip/cpu/plugins/mmu_exception_handler.h (TLM)
class MmuExceptionHandlerPlugin : public PluginBase {
public:
  void setup(PipeBuilder& pb) override {
    pb.at_stage("memory", Phase::LATE, [this, &pb]() {
      auto exception_code = pb.node_of_logic_stage("memory")->get(mmu_keys::EXCEPTION_CODE);
      if (exception_code != 0) {
        pb.node_of_logic_stage("writeback")->put(cpu_keys::CPU_EXCEPTION_CODE, exception_code);
        pb.flush_link()->flush_when(mmu_exception_pending);
        hazard_->clear_mmufault();
      }
    });
  }
};

// ip/cpu/plugins/mmu_exception_handler_chmem.h (CH_MEM)
// 同步实装, 用 ch_bool / ch_reg / ch_state_machine (ADR-046 v2.0 DSL 例外)
```

### Phase 5: cpu_factory.h:390 修复

```cpp
// ip/cpu/cpu_factory.h:390
// 修改前:
// pb.register_plugin(std::make_unique<RiscvMMUPlugin>(...), /*satp_value=*/0);
// 修改后:
pb.register_plugin(std::make_unique<RiscvMMUPlugin>(...), cpu_config.satp_value);
```

### Phase 6: cpu_factory_chmem.h 联动

```cpp
// ip/cpu/cpu_factory_chmem.h
// 在 if (enable_mmu.value_or(false) && mmu_mode != "bare") 分支内追加:
if (enable_mmu.value_or(false) && mmu_mode.value_or("bare") != "bare") {
  pb.register_plugin(std::make_unique<MmuExceptionHandlerPlugin<ch_uint<32>>>());
}
```

### Phase 7: TEST_CASE 6 实装

```cpp
// tests/mmu/test_mmu_bare_plumbing_verilator.cpp 追加
TEST_CASE("mmu_sv32_translation_verilator_e2e_flipped", "[mmu-verilator][e2e][sv32]") {
  // **承接 Change 2a follow-up — 真 sv32 translation 翻转**
  // skip-when-absent check ...
  // popen cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf sv32_pte.elf --cycles 5000
  // REQUIRE TOHOST=1, cycle ≤ baseline × 1.5
  // 断言 trap handler PC = 0x80000010 (via CPU_EXCEPTION_CODE payload 后续 read)
}
```

### Phase 8: sv32 PTE 程序 vendor

```bash
# tests/cpu/manual_elf/build_sv32_pte.S
.section .text
.globl _start
_start:
    # Set up satp CSR (Sv32 mode)
    li t0, 0x80050000  # MODE=Sv32 (bit 31) + PPN=0x80050
    csrw satp, t0
    # ... PTE setup + load to tohost ...
    li t1, 1
    la t2, tohost
    sw t1, 0(t2)
    j .

.section .data
.align 12
tohost: .word 0
```

### Phase 9: Documentation Sync (TDD Step 5)

```bash
# AGENTS.md "[mmu-verilator]" 3/3 → 4/4 PASS
# AGENTS.md 段新增 "cpu-pipeline-mmufault-handler v0.10.x 引用" 行
# CHANGELOG v0.10.x 段新增本 change 条目
bash tools/v0100-bootstrap.sh review
```

### Phase 10: Architecture Gate Final

```bash
bash tools/verify_adr.sh
bash tools/verify_plugin_decision.sh
bash tools/check_plugin_portability.sh
bash tools/doc_link_check.sh
# 全部 0 失败
```

### Phase 11: Regression Final Check

```bash
bash tools/run_chipforge_tests.sh
# 期望:
# [mmu-verilator] 4/4 PASS (3 + 1 new)
# [mmu] 53/53 PASS
# [cpu-integration] 81/81 PASS (硬不退化)
# [cpu-l1-mmu-demo] 6/6 PASS
# [cpu] 19/19 PASS
# [chmem] 9/9 PASS
# [verilator] 1/1 PASS
```

### Rollback Strategy

若 Phase 5 cpu_factory.h:390 修复触发 5 调用方 ABI 不兼容:
1. Revert Phase 5 改动
2. 重设计 (e.g., 引入 `cpu_config.satp_value_overload` 字段, 保留原 0 硬写作为 default)

若 TEST_CASE 6 cycle cap 不足:
1. 接受 cap 放宽至 × 2.0
2. release 标注 "真 sv32 e2e 翻转成功, cycle cap 放宽, 后续优化"

## Open Questions

### Q1: trap handler PC 简化 hardcode vs CSR mtvec 读?

候选:
- (A) hardcode 0x80000010 (本 change)
- (B) CSR mtvec 读 (mfc-... scope)
- (C) 配置项 `cpu_config.trap_pc` (灵活但增加 config 字段)

倾向 (A): 与本 change scope 可控 + riscv-tests sv32 PTE ELF 约定一致

### Q2: HazardPlugin::clear_mmufault() API 命名?

候选:
- (A) `clear_mmufault()` (本 change)
- (B) `clear_page_fault()` (更通用)
- (C) `clear_exception_pending()` (抽象, 不限于 mmufault)

倾向 (A): 命名具体, 与 CPU pipeline 当前命名风格一致

### Q3: TEST_CASE 6 cycle cap?

候选:
- (A) × 1.5 (与 Change 2a manual_elf 一致)
- (B) × 2.0 (放宽, 真 sv32 walk 开销大)
- (C) × 1.2 (与 Change 2a 5 ELF baseline 一致, 可能不足)

倾向 (A): 与 Change 2a §R4 mitigation 一致

### Q4: sv32 PTE 程序 ELF 是否 vendor 到 tests/cpu/riscv_tests/elf/?

候选:
- (A) tests/cpu/manual_elf/build_sv32_pte.S (现有 manual_elf 目录)
- (B) tests/cpu/riscv_tests/elf/sv32-p-* (新增 family, 与 rv32ui-p-* 对齐)
- (C) tests/cpu/manual_elf/build_sv32_basic.S (单 ELF)

倾向 (A): 与 Change 1 `build_mmu_bare.S` 同目录, vendor 工具链复用

### Q5: MmuExceptionHandlerPlugin 在 D4 + ADR-046 v2.0 边界?

候选:
- (A) 主逻辑用 at_stage 闭包 + CtrlLink flush_when, 仅 trap PC 跳转用 ch_state_machine (本 change)
- (B) 全 at_stage 闭包 (不引入 ch_state_machine)
- (C) 全 ch_state_machine (FSM DSL)

倾向 (A): D4 + ADR-046 v2.0 边界清晰: 主逻辑无状态机 (D4), 仅 trap PC 跳转因状态机交互 FSM (ADR-046 豁免)
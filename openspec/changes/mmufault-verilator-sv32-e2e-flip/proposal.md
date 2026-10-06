---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.10.x
status: placeholder
depends_on:
  - cpu-pipeline-mmufault-handler
  - phase-6d.6-mmu-ptw-fsm
---

# mmufault-verilator-sv32-e2e-flip — 真 sv32 Verilator e2e 翻转（v2 follow-up）

> **Scope 拆分 (2026-10-06, Oracle 决策)**: 本 change 从 `cpu-pipeline-mmufault-handler` v1 拆分出的 follow-up，承担原 Phase 2/3/9 的 Verilator 后端 e2e 翻转职责。
>
> **前置依赖**:
> 1. `cpu-pipeline-mmufault-handler` v1 archive（TLM/CH_MEM MmuExceptionHandlerPlugin 实装 + HazardPlugin::clear_mmufault() API）
> 2. **Phase 6d.6** (`mmu_chmem.h`) 实装 —— 当前 `tools/verilator_runner/cpu_verilator_sim.cpp:96-101` 明确 `--enable-mmu --mmu-mode sv32` 是 no-op stub；sv32 真实翻译语义需 Phase 6d.6 MMU/PTW FSM 输出后才能端到端验证。

## Why

`cpu-pipeline-mmufault-handler` v1 archive 后，CPU pipeline 端有 `MmuExceptionHandlerPlugin` + `HazardPlugin::clear_mmufault()` 支持 MMU exception routing（page fault → flush → trap PC 跳转），但 **Verilator 后端真 sv32 翻转仍未端到端验证**——因为 `mmu_chmem.h` 不存在，sv32 翻译语义仅 plumbing-only。

承接 `verilator-mmu-bare-plumbing-e2e` (Change 2a, P2) 的 Non-Goals §NG1 显式声明，本 change 是该 Non-Goals 的 follow-up owner。

**本 change 范围**:
1. sv32 ELF vendor（`build_sv32_pte.S`：csrw satp + L0/L1 PTE + trap handler @ 0x80000010 + tohost write）
2. sv32 baseline CSV 建立（5 ELF × median cycle × mode=sv32_mmu）
3. Verilator TEST_CASE 6 `mmu_sv32_translation_verilator_e2e_flipped` 实装（popen `cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf sv32_pte.elf --cycles 5000`）
4. `[cpu-l1-mmu-demo]` 真 sv32 翻转测试（flip `cfg.enable_mmu=true`，独立 TEST_CASE 7）

## What Changes

### 1. sv32 PTE 测试程序 vendor

**位置**: `tests/cpu/manual_elf/build_sv32_pte.S`（新建）

最小 sv32 程序结构（per `tests/cpu/manual_elf/build_mmu_bare.S` 模板）:
```asm
.section .text
.globl _start
_start:
    # 1. 设 satp (Sv32 mode + L0 PPN = PTE table addr >> 12)
    csrw satp, ...

    # 2. 跳转到 sv32 翻译后的虚拟地址 (load 触发 PTW walk)
    la t0, _sv32_code
    csrw mepc, t0
    mret

_sv32_code:
    # 3. PTE 触发 fault → trap handler @ 0x80000010 (hardcode)
    # 4. trap handler 写 tohost = 1 (test pass indicator)
```

**依赖**: `tests/cpu/manual_elf/build_manual_elf.sh` 注册 `build_sv32_pte()` 函数（仿 `build_mmu_bare()`）。

### 2. sv32 baseline CSV 生成

**位置**: `tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv`（追加 mode=sv32_mmu 行）

CSV schema 不变（`elf_name,mode,median_cycles,verilator_version`），新增 mode=`sv32_mmu` 5 行（rv32ui-p-add/addi/auipc/beq/jal baseline）。

**触发条件**: 需先实装 `mmu_chmem.h`（Phase 6d.6 输出），否则 baseline 数据无意义。

### 3. Verilator TEST_CASE 6 实装

**位置**: `tests/mmu/test_mmu_bare_plumbing_verilator.cpp`（追加 TEST_CASE 6）

family tags: `[mmu-verilator][e2e][sv32]`

body 实现:
```cpp
TEST_CASE("mmu_sv32_translation_verilator_e2e_flipped", "[mmu-verilator][e2e][sv32]") {
    // popen cpu_verilator_sim --enable-mmu --mmu-mode sv32 --elf sv32_pte.elf --cycles 5000
    // REQUIRE TOHOST=1 (parse stdout)
    // REQUIRE cycle <= baseline × 1.5 (sv32 baseline from #2)
    // REQUIRE trap PC jump via CPU_EXCEPTION_CODE payload read
}
```

**依赖**: `cpu_verilator_sim.cpp` 的 `--enable-mmu --mmu-mode sv32` 必须实装真翻译（当前是 stub）。

### 4. `[cpu-l1-mmu-demo]` 真 sv32 翻转

**位置**: `tests/soc/test_cpu_l1_mmu_demo.cpp:113`（flip `cfg.enable_mmu=false → true`）

新增 TEST_CASE 7 `cpu_l1_mmu_demo_sv32_translation_flipped`:
```cpp
TEST_CASE("cpu_l1_mmu_demo_sv32_translation_flipped", "[soc][cpu-l1-mmu-demo][sv32]") {
    // cfg.enable_mmu = true; cfg.mmu_mode = "sv32";
    // 用 sv32_pte.elf 替换 rv32ui-p-add/addi/.../beq ELF
    // REQUIRE tohost = 1 (CPU pipeline MMU exception handler 正常卸载 fault)
}
```

**依赖**: AGENTS.md §已知测试状态 `[cpu-l1-mmu-demo]` workaround（line 40）需 flip，文档同步更新。

## Acceptance Criteria

- [ ] `sv32_pte.elf` build 0 error + tohost=1 验证 (sv32 mode 翻译正确)
- [ ] sv32 baseline CSV 5 行新增 + schema 合规
- [ ] TEST_CASE 6 `mmu_sv32_translation_verilator_e2e_flipped` 5/5 PASS（tohost=1 + cycle ≤ baseline×1.5 + trap PC 跳转）
- [ ] TEST_CASE 7 `cpu_l1_mmu_demo_sv32_translation_flipped` 1/1 PASS
- [ ] `[mmu-verilator]` family 3/3 → 5/5 PASS（含 TEST_CASE 1-3 plumbing + TEST_CASE 6 + TEST_CASE 7 sv32）
- [ ] `[cpu-l1-mmu-demo]` family 6/6 → 7/7 PASS（workaround flip）
- [ ] 6 个 hard gate family 0 regression: `[mmu-verilator] 5/5` + `[mmu] 53/53` + `[cpu-integration] 81/81` + `[cpu-l1-mmu-demo] 7/7` + `[cpu] 19/19` + `[verilator] 1/1`
- [ ] `git diff main --stat` < 400 LOC（vendor ~50 LOC + test ~150 LOC + CSV ~50 LOC + doc ~50 LOC）

## Out of Scope

- TLM/CH_MEM MmuExceptionHandlerPlugin 实装 → 已在 v1 (`cpu-pipeline-mmufault-handler`) 覆盖
- HazardPlugin::clear_mmufault() API → 已在 v1 覆盖
- cpu_factory.h satp_value 修复 → 已由 v0.10.2 `cpu-factory-satp-mapping` archive
- MMU/PTW FSM 实装 → 由 Phase 6d.6 (`phase-6d.6-mmu-ptw-fsm`) 独立 change 覆盖

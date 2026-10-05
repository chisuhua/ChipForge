## Context

### 现状（v0.10.4 hotfix 后, 2026-10-04）

`tests/cpu/test_cpu_verilator_sim.cpp` 唯一 Verilator e2e 测试 (`cpu_verilator_sim_tohost1`) 覆盖 5 个 RV32UI ELF × tohost=1, 共 1 case / 5 assertions / 10 total checks (per AGENTS.md §verilator)。**未涉及 MMU/L1Cache**: `--elf` 路径下 `cpu_verilator_sim` 走 Bare translation + 无 cache。

`ip/mmu/tlm/MMUPlugin.cpp:96-109` Bare shortcut 判定代码（v0.10.4 hotfix 修复点）:
```cpp
switch (sv_mode_) {
  case SvMode::Sv32: satp_mode = (satp_value_ >> 31) & 0x1u;
  case SvMode::Sv39: satp_mode = (satp_value_ >> 60) & 0xFu;
  case SvMode::Sv48: satp_mode = (satp_value_ >> 60) & 0xFu;
  case SvMode::Bare:
}
if (sv_mode_ == SvMode::Bare || satp_mode == 0 || satp_ppn_ == 0) {
  // Bare identity translation: paddr = vaddr
}
```

关键事实:
- **TLM 模式**: hotfix 已修,用 `satp_value_` MODE 字段 + `satp_ppn_` 双重判定
- **CH_MEM 模式**: `ip/mmu/tlm/` 无 `mmu_chmem.h`, Change 1 archive 后 `build_cpu` enable_mmu=true 路径在 CH_MEM 下仅 emit TODO log (不实装)
- **v0.10.0 ad48fcf Phase D workaround**: 用 `satp_ppn_ == 0` 替代 `satp_value_` 判定导致 v0.10.0 hotfix (`debug-cpu-l1-mmu-demo-paddr-regression`) 修复 5 个 [cpu-integration] 回归

### Verilator 链路 MMU 测试空缺

`tests/mmu/` 9 个测试, 全部走 TLM 或 CppHDL Simulator 模式:
- `test_mmu_config_schema.cpp` (JSON)
- `test_mmu_plugin.cpp` (TLM Plugin)
- `test_multi_level_tlb.cpp` (lib 算法)
- `test_ptw_real_memory.cpp` (TLM 单元)
- `test_ptw_stall_integration.cpp` (lib 算法)
- `test_ptw_tlb_refill_integration.cpp` (TLM 集成, 含 [tlb-refill] 2 个 case)
- `test_ptw_unit.cpp` (lib 算法)
- `test_tlb_factory.cpp` (lib 算法)
- `test_tlb_unit.cpp` (lib 算法)

### Change 1 依赖产出（archive 后）

`verilator-cpu-factory-extensible-params` archive 后,本 change 可用:
- `cpu_verilator_sim --enable-mmu --mmu-mode bare --elf <elf>` CLI flag
- `cpu_verilator_sim --enable-mmu --mmu-mode bare` 跑 add.elf 验证 tohost=1
- `tests/cpu/manual_elf/build_mmu_bare.S` vendor 入口 (sv32+ppn=0 边界 ELF)

### TLB API 表面 (`ip/mmu/lib/`)

| File | 用途 |
|------|------|
| `tlb_base.h` | TLB 基类 |
| `tlb.h` | TLB 主类 |
| `tlb_entry.h` | TLB entry 数据结构 |
| `tlb_lookup.h` | lookup API |
| `tlb_factory.h` | factory 创建 |
| `multi_level_tlb.h` | multi-level TLB |
| `ptw.h` | Page Table Walker |
| `memory_interface.h` | MemoryInterface (for PTW real PTE read) |

### Stakeholders

- **wave5 mfc-...**: 间接消费 — Phase G "DMIPS/MHz ≥1.4 (CH_MEM + Verilator)" 硬门禁需 enable_mmu 路径
- **wave3-mmu-real-memory-and-cycle (v0.8.0, 进行中)**: 软依赖 — MMU PTW real PTE read 已实装 (mmu-paddr-consume-and-real-memory), 但 mmufault handler 未实装
- **3 架构门禁**: 必须 0 失败

## Goals / Non-Goals

### Goals

1. **G1**: 引入 5 个 `[mmu-verilator]` TEST_CASE（family tag 与 [verilator] 区分）
2. **G2**: 验证 `cpu_verilator_sim --enable-mmu --mmu-mode bare` 链路跑通 5 ELF tohost=1
3. **G3**: `CpuFactoryChmem::build_cpu` w/ `enable_mmu=true` + `mmu_mode="bare"` + CH_MEM elaborate 0 error + verilator --cc 编译通过（修复 C-B: 原 G3 "TLB prefill-hit path" 是已删除 TEST_CASE 2 内容, 同步 proposal 缩 scope 为 3 TEST_CASE）
4. **G4**: `--enable-mmu --mmu-mode bare` (无 `--enable-cache`) 跑通 manual_elf mmu_bare.elf（修复 C-B: 原 G4 "sv32+ppn=0 回归防护" 是已删除 TEST_CASE 3 内容, 同步 proposal 缩 scope）
5. **G5**: 5 ELF cycle baseline 表生成 (CH_MEM+enable_mmu vs CPU-only baseline)
6. **G6**: 零回归: `[verilator]` 1/1 + `[mmu]` 53/53 + `[cpu-l1-mmu-demo]` 6/6 + `[cpu-integration]` 81/81 全部不变
7. **G7**: 3 架构门禁 0 失败

### Non-Goals

1. **NG1**: **不**验证 sv32 translation 语义、PTW walk、page fault 处理
2. **NG2**: **不**实装 `mmu_chmem.h`（不在本 change scope; Change 1 archive 后 enable_mmu=true 在 CH_MEM 模式仅 emit TODO log）
3. **NG3**: **不**新增 L1Cache Verilator e2e 测试（属 Change 2b `verilator-l1cache-e2e-coverage` scope）
4. **NG4**: **不**修复 `cpu-pipeline-mmufault-handler`（独立 P1 follow-up change, 本 change 占位 boundary）
5. **NG5**: **不**修改 `MMUPlugin::do_lookup` Bare shortcut 判定代码（v0.10.4 hotfix 已修, 本 change 仅消费已修代码）
6. **NG6**: **不**升级 CI 门禁（保留 skip-when-absent 模式, 与 Change 1 一致）
7. **NG7**: **不**扩 7-stage superscalar（沿用 Phase 6d.8 显式排除）
8. **NG8**: **不**扩展 `--enable-cache` 联动测试（`--enable-cache` 撞 Change 1 的 L1Cache fail-fast; 本 change 不撞）

## Decisions

### D1: 改名 `verilator-mmu-bare-plumbing-e2e` 而非 `verilator-mmu-e2e-coverage`

**Decision**: change name 含 `-bare-plumbing-` 关键字。

**Rationale**:
- 名字即声明范围 (per Oracle 终轮建议)
- 防止未来 reader 误读为"MMU 翻译语义 e2e 验证"
- 与 follow-up `cpu-pipeline-mmufault-handler` (真 sv32 e2e) 区分清晰

**Alternatives considered**:
- (A) `verilator-mmu-e2e-coverage`: 名字暗示翻译验证, ❌ 误导
- (B) `verilator-mmu-bare-only-e2e`: `-only-` 易被理解为 "only Bare, no Sv32"; 本 change 修复 C2 缩 scope 后**仅** Bare mode CLI plumbing（不测 Sv32 翻译, TLB prefill API 行为, sv32+ppn=0 误判防护——这些由 TLM `[mmu]` 53/53 负责）
- (C) `verilator-mmu-bare-plumbing-e2e` (✓): 同时表达 "Bare 模式" + "plumbing only" + "e2e"

### D2: TEST_CASE 注释强制 `**plumbing only — translation semantics NOT verified**` 标记

**Decision**: 每个 `[mmu-verilator]` TEST_CASE 顶部注释含此字符串字面量。

**Rationale**:
- 防止后续 maintainer 误以为这些测试验证翻译正确性
- AGENTS.md + OpenSpec 文档 grep 可自动 audit 该标记是否存在
- 与 D1 改名呼应, 双重保险

**Alternatives considered**:
- (A) 仅在 spec.md / proposal.md 写一次: 散落、易丢失
- (B) 用新 family tag `[mmu-verilator-plumbing]`: 现有 OpenSpec family 不支持多 tag, 维护成本高

### D3: cycle baseline 通过 median 5 次取样而非单次

**Decision**: 各 ELF 跑 5 次, 取 cycle 中位数落盘 CSV。

**Rationale**:
- Verilator 仿真 cycle 受 OS 调度抖动影响, 单次不可靠
- 5 次足够稳态, 10 次浪费 CI 时间
- median 比 mean 抗 outlier 强 (某次 OS swap 拉到 mean 失真)

**Alternatives considered**:
- (A) 单次: 抖动风险
- (B) 10 次取 mean: 浪费 CI, mean 易 outlier
- (C) 5 次取 median (✓): 平衡成本与稳定性

### D4: ~~TEST_CASE 3 (sv32+ppn=0) 用 manual_elf mmu_bare.S + 自定义 harness 注入 satp CSR 写~~ (修复 C-B 废弃)

**原 Decision**: 不修改 `tests/cpu/manual_elf/build_mmu_bare.S` (Change 1 vendor), 在 harness 侧通过 `mmu_keys::SAT` payload 注入 sv32 mode + satp_ppn=0 配置。

**废弃原因** (修复 Oracle C-B Critical 复审):
- popen `cpu_verilator_sim` 拿不到 MMU 内部 `paddr`/`PTW::start_walk()` 状态, e2e 断言在物理上不可达
- chipforge_tests_chmem 不链接 MMU lib 源码, 直接构造 MMUPlugin/MultiLevelTLB 会 link error
- v0.10.4 hotfix 类回归防护已在 TLM `[mmu]` 53/53 (含 `[tlb-refill]` 2 cases) 覆盖
- 真 sv32 翻译由 `cpu-pipeline-mmufault-handler` follow-up (TEST_CASE 6) 闭环后承接

**修复后的 D4 内容**: 本 change 缩 scope 为 3 TEST_CASE, D4 替换为 "**`--enable-cache` fail-fast 行为契约**" — 当 `enable_cache=true` 时 `cpu_factory_chmem.h` throw（由 Change 2b task group 1.5 翻转替换为 L1CachePlugin CH_MEM 注册）, 本 change 不涉及此契约（仅消费 Change 1 的 fail-fast 行为）。

### D5: skip-when-absent 沿用 Change 1 模式

**Decision**: 5 个 TEST_CASE 顶部用 `std::ifstream bin(CF_VERILATOR_SIM_BIN); if (!bin.is_open()) { SUCCEED("skip"); return; }` 模式, verilator binary 不存在时静默跳过。

**Rationale**:
- 与 `tests/cpu/test_cpu_verilator_sim.cpp:63-68` 完全一致
- CI 不装 verilator 时静默 PASS, 不阻塞 gate
- AGENTS.md v0.10.0 维护原则: "skip 不计入实测数字"

**Alternatives considered**:
- (A) fail-when-absent: 与 Change 1 决策不一致 (升级 CI 是独立大工程), ❌
- (B) skip-when-absent (✓): 与现状 + Change 1 一致

### D6: baseline CSV 走 median 而非 mean + stddev

**Decision**: CSV 只存 median, 不存 stddev。

**Rationale**:
- median 已足够判定 "是否 < median × 1.2 上限"
- stddev 维护成本高, 5 次取样 stddev 不稳定
- 后续如果发现某 ELF 抖动大, 可单独追加 stddev 字段 (extension, 非 breaking)

**Alternatives considered**:
- (A) median + stddev: 维护成本高, 5 次 stddev 不可靠
- (B) median (✓): 简洁, 后续可扩展

## Risks / Trade-offs

### R1: cycle baseline 跨平台差异

**Risk**: Linux/macOS/Windows 上 Verilator 仿真 cycle 数差异可能 > 20%, 跨平台 CI 失败。

**Mitigation**:
- CSV baseline 仅适用于 Linux (与现有 `cpu_verilator_sim` 一致)
- macOS/Windows 跑 TEST_CASE 时若 cycle > Linux baseline × 1.2, 输出 WARN 但不 FAIL
- tasks.md §2 显式测试 Linux baseline + 1 次 macOS (如有) 校准
- cap × 1.2 是经验值, tasks.md §2 calibration 步骤计算 stddev/median 比值, 若 > 10% cap 放宽至 × 1.5

### R2: ~~sv32+ppn=0 TEST_CASE 触发 PTW walk 但 hazard 重试循环~~ (修复 C2 删除)

**Risk (原)**: AGENTS.md 明确: 真 sv32 + PTW fault 后 CPU 陷入 hazard 重试循环 (CPU pipeline 缺 MMU exception handler)。TEST_CASE 3 可能 SIM 卡死或 SEGV。

**删除原因 (修复 C2 Oracle 审查)**:
- popen `cpu_verilator_sim` 拿不到 MMU 内部 `paddr` / `PTW::start_walk()` 状态, e2e 断言在物理上不可达
- chipforge_tests_chmem 不链接 MMU lib 源码, 直接构造 MMUPlugin/MultiLevelTLB 会 link error
- v0.10.4 hotfix 类回归防护已在 TLM `[mmu]` 53/53 中, e2e 重复价值低
- 真 sv32 walk + MMU exception handler 由 `cpu-pipeline-mmufault-handler` follow-up 闭环后承接

### R3: enable_mmu=true 在 CH_MEM elaboration 仅 emit TODO log (no-op 行为)

**Risk** (修复 C1 措辞精确化): Change 1 archive 后, `cpu_verilator_sim --enable-mmu --mmu-mode bare` 在 CH_MEM 下 elaboration 仅 emit TODO log (`MMU hook enabled (mode=bare) — mmu_chmem.h not yet implemented`), **不**实例化任何 MMU plugin（CH_MEM 路径无 RiscvMMUPlugin, 因它是 TLM-only class）。Verilog 输出不含 MMU 电路。

**Mitigation**:
- 本 change TEST_CASE 2 (= 缩 scope 后的第 2 个 case) 显式断言 elaboration 0 error + Verilog 产生非空 + verilator --cc 编译通过（修复 C-B: 原 TEST_CASE 2 是 TLB prefill-hit, 已删除; 新 TEST_CASE 2 是 elaboration）
- 本 change TEST_CASE 1 跑 tohost=1 cycle 数若异常高 (> baseline × 2), 标记为 "elaboration 退化" 调查
- 接受 MMU 行为退化 = Bare identity translation (与 Change 1 archive 前一致, 仅多 emit TODO log)

### R4 (新增, 修复 C2 后): MMU 语义层 e2e 无防护 (承认现状)

**Risk**: 本 change archive 后, wave5 `mfc-cpu-pipeline-multi-cycle-fsm` Phase G "DMIPS/MHz ≥1.4 (CH_MEM + Verilator 实测)" 硬门禁可消费 `--enable-mmu` 路径但**仅 plumbing**, MMU 语义验证需 `cpu-pipeline-mmufault-handler` follow-up（它实装真 sv32 translation + MMU exception handler,届时才有可验证的真 e2e）。

**Mitigation**:
- 在 proposal.md §Why、spec.md §Scope 修订声明、tasks.md §12 Handoff 均**显式承认**此现状
- AGENTS.md `[mmu-verilator]` 行加 "(CLI plumbing only; v0.10.4 hotfix TLM 防护由 [mmu] family 负责)" 标记
- tasks.md §12.2 通知 mfc-... owner: Phase G 硬门禁仍需等 `cpu-pipeline-mmufault-handler` follow-up
- **不**通过此 change 假装"e2e 覆盖 MMU 翻译"——诚实降级是本 change 的核心价值

### R5: baseline CSV 修改影响 3 个 TEST_CASE

**Risk**: baseline CSV 修改后, 3 个 TEST_CASE 全部受影响（修复 C2: 从原 5 缩为 3）。

**Mitigation**:
- baseline CSV 在 commit 时由 tasks.md §10 显式记录修改原因 (CHANGELOG 引用)
- baseline CSV 改 ≥ 10% 必须 ARCHITECTURAL CHANGE 标签 (proposal frontmatter)
- tasks.md §10 加 baseline CSV 校验: diff < 10% threshold (防止误改)

### Trade-off: TEST_CASE 数量 vs baseline 表维护成本

**Trade-off**: 3 个 TEST_CASE × 5 次 median = 15 数据点, 维护成本低。

**Decision 接受**: 3 个 TEST_CASE 是 CLI plumbing 验证的最小集（5 ELF tohost=1 + elaboration 0 error + manual_elf full-chain），修复 C2 后回归防护归 TLM suite 责任, 维护成本显著降低

### R6 (新增, 修复 C2): cap × 1.2 数学依据缺失

**Risk**: cap `cycle_count ≤ baseline × 1.2` 无数学依据, 3 个 change 反复出现, 可能 flaky PASS/FAIL。

**Mitigation** (修复 S7):
- 在 baseline CSV header 第二行附 `# cap=median*1.2 (empirical 20% jitter tolerance, see spec §TEST_CASE 1)` 文档
- tasks.md §2.3 新增 calibration 步骤: 计算 5-run stddev/median 比值, 若 > 10% cap 放宽至 × 1.5 并 ARCHITECTURAL CHANGE 标签
- tasks.md §10.3 git diff 监控确保本 change 不引入 cycle-sensitive 隐式依赖

## Migration Plan

### Phase 1: Spike 验证 (Pre-flight, TDD Step 1)

```bash
# Change 1 archive 假设
openspec change validate verilator-cpu-factory-extensible-params  # 应 PASS
# 跑 enable_mmu 验证基础设施
./build/bin/cpu_verilator_sim --elf tests/cpu/manual_elf/add.elf --enable-mmu --mmu-mode bare
# 预期 PASS: TOHOST=1 PASS (5 cycles 或类似)
```

### Phase 2: cycle baseline 表生成 (TDD Step 2)

```bash
# 5 ELF × 5 runs × 2 mode 组 = 50 数据点
# 仅 enable_mmu 组 (enable_cache 触 fail-fast 不跑)
for elf in add addi auipc beq jal; do
  for i in 1 2 3 4 5; do
    ./build/bin/cpu_verilator_sim --elf tests/cpu/riscv_tests/elf/rv32ui-p-$elf \
      --enable-mmu --mmu-mode bare --cycles 5000 2>/dev/null | grep CYCLES
  done
done | sort -n | awk '{a[NR]=$1} END {print a[int(NR/2)]}'  # median per ELF
# 写入 tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv
```

### Phase 3: TEST_CASE 实现 (TDD Step 3-4)

```cpp
// tests/mmu/test_mmu_bare_plumbing_verilator.cpp
#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"
// ... include mmu_keys, cpu_keys, etc.

// **plumbing only — translation semantics NOT verified**
TEST_CASE("mmu_bare_plumbing_tohost1_baseline_5_elf", "[mmu-verilator][e2e]") {
  // ... skip-when-absent check ...
  for (const auto& elf : {"rv32ui-p-add", "rv32ui-p-addi", ...}) {
    // run cpu_verilator_sim --enable-mmu --mmu-mode bare --elf <elf>
    // REQUIRE TOHOST=1, cycle_count <= baseline_csv[elf] * 1.2
  }
}

// ... elaboration, full-chain ...  (修复 C-B: 原"TLB prefill-hit, sv32+ppn=0 guard"已删除)

#endif  // CF_PLUGIN_USE_CH_MEM
```

### Phase 4: tests/CMakeLists.txt 注册

```cmake
list(APPEND CHMEM_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/mmu/test_mmu_bare_plumbing_verilator.cpp
)
target_compile_definitions(chipforge_tests_chmem PRIVATE
    CF_MMU_VERILATOR_BASELINE_CSV="${CMAKE_SOURCE_DIR}/tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv"
)
```

### Phase 5: 文档 + 架构门禁 (TDD Step 5)

```bash
bash tools/verify_adr.sh
bash tools/verify_plugin_decision.sh
bash tools/check_plugin_portability.sh
# 0 失败
# AGENTS.md §honesty_audit 增加 [mmu-verilator] 3/3 PASS (修复 C-B: 5→3 PASS 与缩 scope 后 TEST_CASE 数对齐)
# CHANGELOG v0.10.x 段新增本 change 条目
```

### Rollback Strategy

若任何 TEST_CASE (1/2/3) 触发 hazard 重试循环 (例如 sv32 PTE 边界配置意外引入):
1. 接受降级: TEST_CASE 3 仅断言 "不 SEGV + max_cycles 内退出"
2. 不删 case, 但加注释: "v0.10.4 hotfix class protection deferred to cpu-pipeline-mmufault-handler follow-up"
3. v0.10.x release 时标注 [mmu-verilator] 4/5 PASS (1 deferred)

## Open Questions

### Q1: baseline CSV 跨平台策略?

候选:
- (A) 仅 Linux (与现状 cpu_verilator_sim 一致)
- (B) Linux + macOS (两份 CSV)
- (C) Linux + 跨平台 cycle cap × 1.5 (容差大)

倾向 (A): 与现状一致, 后续跨平台时单独评估

### Q2: TEST_CASE 失败降级策略?（修复 C-B: 原针对已删除 TEST_CASE 3 sv32+ppn=0 提问, 重新定位为通用降级策略）

候选:
- (A) 硬性失败: 5/5 必须 PASS, 否则阻塞 archive
- (B) 软性降级: PASS rate ≥ 80% 即可, 4/5 PASS 接受
- (C) 阻塞但 release 标注: 3/3 PASS 才 release, 2/3 接受 archive 但 release 标注 deferred（修复 C-B: 5→3 与缩 scope 对齐）

倾向 (C): 与 v0.10.4 hotfix 模式一致 (5 个 regression 修复, archive + release 都标)

### Q3: 是否在 csv baseline 表附 Verilator 版本?

候选:
- (A) CSV 第一行注释 `# verilator 5.052`
- (B) 单独 metadata 文件 `test_mmu_bare_plumbing_verilator_baselines.meta.json`
- (C) 不记录, 跨版本 baseline 复用

倾向 (A): 简洁, 与现有 manual_elf/build_manual_elf.sh 头注释一致

### Q4: AGENTS.md "已知测试状态" 段措辞?

候选:
- (A) "[mmu-verilator] `3/3 PASS` (plumbing only — translation semantics NOT verified) — change verilator-mmu-bare-plumbing-e2e v0.10.x"（修复 C-B: 5→3 与缩 scope 对齐）
- (B) "[mmu-verilator-plumbing] `3/3 PASS` ..."
- (C) "[verilator][mmu] `3/3 PASS` ..."

倾向 (A): 与 change name 一致 (含 `-bare-plumbing-`), grep 友好
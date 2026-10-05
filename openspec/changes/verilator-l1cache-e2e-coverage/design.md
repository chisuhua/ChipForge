## Context

### 现状（v0.10.4 hotfix 后, 2026-10-04）

`tests/cache/` 7 个测试,全部走 TLM 模式或独立 CppHDL Simulator (`test_l1cache_refill_fsm_chmem.cpp` 仅验证 FSM 状态机本身,不挂 L1CachePlugin):

| 测试文件 | 模式 | 测什么 |
|---------|------|--------|
| `test_l1_cache_bridge.cpp` | TLM | Bridge 接口 |
| `test_l1_cache_json_instantiate.cpp` | TLM | JSON 装配 |
| `test_l1_cache_plugin_e2e.cpp` | TLM | e2e 功能 |
| `test_l1_cache_plugin_unit.cpp` | TLM | 单元 |
| `test_l1cache_refill_fsm_chmem.cpp` | CppHDL Simulator | **refill FSM 独立 PoC** (6d.7 已实装) |
| `test_mmu_cache_integration.cpp` | TLM mock | L1Cache + MMU 集成 (mock MMU 输出) |
| `test_replacement_policy.cpp` | TLM | 替换策略 |

`ip/cache/tlm/` 文件清单:
- `L1CachePlugin.h/.cpp` (TLM 完整实装)
- `cache_keys.h`
- `l1_cache_refill_fsm_chmem.h` (6d.7 独立 refill FSM, **非完整 L1CachePlugin CH_MEM**)

**`l1_cache_chmem.h` (完整 L1CachePlugin CH_MEM 版) 当前不存在**。

### Change 1 依赖产出（archive 后）

`verilator-cpu-factory-extensible-params` archive 后:
- `cpu_verilator_sim --enable-cache` CLI flag 可用
- `cpu_factory_chmem.h::build_cpu(..., enable_cache=true)` 触发 L1Cache fail-fast: `throw std::runtime_error("L1Cache CH_MEM not implemented; refer to change verilator-l1cache-e2e-coverage")`
- `tests/cpu/manual_elf/build_l1cache_basic.S` vendor 入口 (cache hit + miss path ELF)

### `cache-phase1.5-4way` 依赖（archive 后）

`cache-phase1.5-4way` (wave4 P2 占位, 0/43 tasks) archive 后:
- `ip/cache/tlm/l1_cache_chmem.h` 完整实装 (256 sets × 4 way set-associative)
- `tests/cache/test_l1cache_4way_lru.cpp` + `test_l1cache_4way_vipt_safety.cpp` PASS
- `l1_cache_refill_fsm_chmem.h` 6d.7 FSM 配合 4-way 升级 (MISS → REFILL_WAIT 状态加 way 选择)
- Oracle #8 修订: "5 ELF cycle-identical 不回归" 约束可通过现有 `tests/cpu/test_cpu_verilator_sim.cpp` 5 ELF × tohost=1 PASS 验证

### Stakeholders

- **cache-phase1.5-4way**: 硬阻塞 #2, archive 后才启动本 change
- **Change 1 (verilator-cpu-factory-extensible-params)**: 硬阻塞 #1, archive 后才启动本 change
- **Change 2a (verilator-mmu-bare-plumbing-e2e)**: 软依赖, TEST_CASE 3 消费 `--enable-mmu` 但不强依赖
- **3 架构门禁**: 必须 0 失败

## Goals / Non-Goals

### Goals

1. **G1**: 引入 3 个 `[cache-verilator]` TEST_CASE（family tag 与 [mmu-verilator] / [verilator] 区分）
2. **G2**: 验证 `cpu_verilator_sim --enable-cache` 链路 cache hit path 跑通 tohost=1
3. **G3**: 验证 cache miss path 触发 refill FSM 6d.7 后 tohost=1
4. **G4**: 验证 L1Cache + MMU 联动 (`--enable-mmu --mmu-mode bare --enable-cache`) 跑通 l1cache_basic.elf
5. **G5**: L1Cache-only + L1Cache+MMU 联动 cycle baseline 表生成
6. **G6**: 零回归: `[verilator]` 1/1 + `[mmu-verilator]` 5/5 (Change 2a) + `[mmu]` 53/53 + `[cache]` 既有 21 baseline + `[cpu-integration]` 81/81 全部不变
7. **G7**: 3 架构门禁 0 失败

### Non-Goals

1. **NG1**: **不**实装 `l1_cache_chmem.h`（属 `cache-phase1.5-4way` scope; 本 change 仅消费已实装的 CH_MEM 版）
2. **NG2**: **不**测 4-way set-associative（属 `cache-phase1.5-4way` Acceptance line 121 "既有 21 baseline + 新增 ≥3 测试 PASS"）
3. **NG3**: **不**测 VIPT aliasing safety（属 `cache-phase1.5-4way` Acceptance line 119 "VIPT aliasing 安全验证通过 (test_l1cache_4way_vipt_safety PASS)"）
4. **NG4**: **不**改写 `l1_cache_refill_fsm_chmem.h`（仅消费 6d.7 已实装的 FSM）
5. **NG5**: **不**新增 LRU 替换策略测试（属 `cache-phase1.5-4way` lru_4way_policy 实装 scope）
6. **NG6**: **不**扩展 DSE 工具链（`tools/dse/cache_dse_sweep.sh` assoc 维度属 `cache-phase1.5-4way` Acceptance line 125）
7. **NG7**: **不**升级 CI 门禁（保留 skip-when-absent 模式, 与 Change 1 / Change 2a 一致）

## Decisions

### D1: 走"最小验证链路"而非完整 cache e2e 重写

**Decision**: 仅验证 refill FSM hit/miss basic path + L1Cache + MMU 联动, 不做 cache DSE sweep、不做 4-way 专项测试。

**Rationale**:
- Oracle 终轮推荐 "最小验证链路" 模式, 与 Change 2a "plumbing-only" 风格一致
- cache-phase1.5-4way 已覆盖 4-way + VIPT + LRU + DSE, 本 change 复测属 double-accounting
- 3 个 TEST_CASE 足够锁住 Verilator 链路可达性

**Alternatives considered**:
- (A) 完整 cache e2e (4-way + VIPT + LRU + DSE): double-accounting, scope 失控, ❌
- (B) 仅验证 refill FSM 独立 Verilog 仿真: 与 cache-phase1.5-4way `test_l1cache_4way_*` 重叠, ❌
- (C) 最小验证链路 (✓): 与 cache-phase1.5-4way 互补, 3 个 TEST_CASE 锁定 Verilator 链路可达性

### D2: cycle baseline CSV 与 Change 2a MMU baseline 分离

**Decision**: 本 change 维护独立 CSV `tests/cache/test_l1cache_refill_verilator_baselines.csv`, 与 Change 2a 的 MMU baseline 不混合。

**Rationale**:
- 不同 family 维护不同 CSV (避免 reader 误用 mode 列值)
- mode 列值: `cache_only` / `cache_mmu_bare` (与 Change 2a `bare_mmu` 区分)
- cross-mode cycle 对拍通过外部脚本 (tasks.md §11 后置脚本) 不内嵌到 CSV

**Alternatives considered**:
- (A) 合并到 Change 2a MMU baseline CSV: 跨 family 维护, reader 易误用, ❌
- (B) 独立 CSV (✓): family-specific, 维护责任清晰

### D3: TEST_CASE 3 联动测试消费 manual_elf l1cache_basic.elf

**Decision**: TEST_CASE 3 复用 Change 1 vendor 的 `build_l1cache_basic.S` (含 1 cache hit + 1 cache miss 路径)。

**Rationale**:
- Change 1 已 vendor 该 ELF, 不重复 vendor
- 测试 1 cache hit + 1 cache miss 路径足够覆盖 L1Cache + MMU 联动
- manual_elf 5 ELF baseline 不覆盖此 ELF, 所以 TEST_CASE 3 cycle cap 放宽至 baseline × 1.5 (per design.md §D4 类似 manual_elf 接受标准)

**Alternatives considered**:
- (A) 新 vendor `build_l1cache_mmu_full.S` (增加 vendor 工具链 entry): Change 1 范围溢出, ❌
- (B) 复用 Change 1 vendor (✓): 与 Change 1 vendor 边界清晰

### D4: skip-when-absent 沿用 Change 1/2a 模式

**Decision**: 3 个 TEST_CASE 顶部用 `std::ifstream bin(CF_VERILATOR_SIM_BIN); if (!bin.is_open()) { SUCCEED("skip"); return; }` 模式。

**Rationale**:
- 与 `tests/cpu/test_cpu_verilator_sim.cpp:63-68` + Change 2a `tests/mmu/test_mmu_bare_plumbing_verilator.cpp:skip` 完全一致
- CI 不装 verilator 时静默 PASS, 不阻塞 gate
- AGENTS.md v0.10.0 维护原则: "skip 不计入实测数字"

**Alternatives considered**:
- (A) fail-when-absent: 与 Change 1/2a 决策不一致, ❌
- (B) skip-when-absent (✓): 与现状一致

### D5: spec delta 修改 `mmu-cache-integration-test` 而非新建 spec

**Decision**: TEST_CASE 3 L1Cache + MMU 联动 requirement 写入 `mmu-cache-integration-test` spec (而非新建 spec)。

**Rationale**:
- `mmu-cache-integration-test` spec 已存在 (TLM 模式), 与 Change 2a 修改的 `mmu-tlb-lookup-insert` / `mmu-cpptlm-bridge` 同类
- 复用现有 spec 命名空间, 避免 spec 散落
- 测试 `[cache-verilator]` family 但 spec 归属 `[mmu-cache-integration]` 维度 (cross-cutting)

**Alternatives considered**:
- (A) 新建 `verilator-l1cache-mmu-integration` spec: spec 命名空间散落, ❌
- (B) delta `mmu-cache-integration-test` (✓): 与现有 spec 同类, 维护成本低

### D6: L1Cache CH_MEM 实装缺失时 fail-fast 而非 silent disable

**Decision**: 沿用 Change 1 design.md §D2 决策: `enable_cache=true` 在 CH_MEM 下 throw 异常, Change 2b 必须在 cache-phase1.5-4way archive 后启动。

**Rationale**:
- 与 Change 1 fail-fast 一致 (v0.10.4 hotfix 教训)
- 防止 silent disable 隐藏 cache CH_MEM 缺失

**Alternatives considered**:
- (A) 静默退化为无 cache: ❌ 与 Change 1 决策冲突
- (B) 沿用 Change 1 fail-fast (✓): 与 Change 1 一致

## Risks / Trade-offs

### R1: cache-phase1.5-4way archive 时序不可控

**Risk**: 当前 0/43 tasks, archive 时间估时 6-12 月。本 change 长期处于 placeholder 状态。

**Mitigation**:
- `status: placeholder` 显式标记, OpenSpec 工具识别为 not-ready-for-archive
- 实际启动时机由 cache-phase1.5-4way archive 触发, 而非本 change 自身驱动
- tasks.md §13 显式 "启动依赖" 段落

### R2: L1Cache CH_MEM 行为差异

**Risk**: cache-phase1.5-4way 实装的 `l1_cache_chmem.h` 行为可能与 TLM L1CachePlugin 有 subtle 差异 (e.g., 4-way 升级后 refill 状态机行为变化), baseline × 1.2 cap 可能不足。

**Mitigation**:
- cache-phase1.5-4way Acceptance line 120 "5 ELF cycle-identical 不回归约束" 已锁住 cycle baseline
- 本 change baseline 表与 cache-phase1.5-4way 共享 5 ELF, 跨差 ≥ 10% 触发 ARCHITECTURAL CHANGE 标签
- tasks.md §10 baseline CSV 校验: diff < 10% threshold (与 Change 2a 一致)

### R3 (新增, 修复 C3 Oracle 审查): L1Cache wiring 翻转 Owner 缺失

**Risk**: Change 1 在 `ip/cpu/cpu_factory_chmem.h::build_cpu` 留 `throw std::runtime_error("L1Cache CH_MEM not implemented; refer to change verilator-l1cache-e2e-coverage")`. `cache-phase1.5-4way` proposal 只承诺实装 `ip/cache/tlm/l1_cache_chmem.h` (~250 LOC), **未承诺**修改 cpu factory wiring. 双 archive 后 `--enable-cache` 仍 throw, 本 change TEST_CASE 1-3 全部阻塞 + Change 1 §4.7 fail-fast smoke test 必然失败.

**Mitigation**:
- 本 change 自带 task group 1.5 (修复 C3 后新增) 在 task group 3 之前完成 L1Cache wiring 翻转: 修改 `ip/cpu/cpu_factory_chmem.h::build_cpu`, 在 `enable_cache=true` 路径替换 throw 为 `pb.register_plugin(std::make_unique<L1CachePlugin<ch_uint<32>>>())` (引用 `cache-phase1.5-4way` 实装的 `l1_cache_chmem.h`).
- task group 1.5.2 显式验证 `bash tools/verify_plugin_decision.sh` 0 失败 (D4 无状态机 + 无 `void tick()` 重写).
- task group 1.5.3 显式验证 `bash tools/check_plugin_portability.sh` 0 失败 (ADR-040 Tier-1 4 项).
- task group 1.5.4 跑 `[mmu-verilator]` 3/3 不退化 (wiring 修改仅影响 `enable_cache=true` 路径, 不影响 `--enable-mmu --mmu-mode bare`).
- **不**依赖 `cache-phase1.5-4way` 主动加 wiring 任务 (其 owner 可能未识别此缺口).

### R4: L1Cache + MMU 联动 (TEST_CASE 3) 触发 cache-phase1.5-4way + Change 2a 双依赖

**Risk**: TEST_CASE 3 同时依赖 cache-phase1.5-4way (L1Cache CH_MEM) + Change 2a (`--enable-mmu` plumbing verified), 任一未 archive 则 TEST_CASE 3 fail-fast。

**Mitigation**:
- TEST_CASE 3 仅在 cache-phase1.5-4way + Change 2a 双 archive 后才 PASS (wave4 + wave5 同步)
- 若 TEST_CASE 3 失败, 标记为 [cache-verilator] 2/3 PASS (TEST_CASE 1+2 PASS, 3 deferred to 双 archive 后)
- tasks.md §11 显式 archive ordering 校验

### R5: VIPT aliasing 测试覆盖

**Risk**: cache-phase1.5-4way §3 VIPT aliasing safety 测试属 cache-phase1.5-4way scope, 本 change 不复测。但若 cache-phase1.5-4way archive 后 VIPT safety 测试 PASS, 本 change TEST_CASE 2 (miss path) 仍可能暴露 aliasing 引起的 cycle 异常。

**Mitigation**:
- TEST_CASE 2 baseline cap 放宽至 × 1.5 (与 Change 2a manual_elf 接受标准一致)
- 若 cycle 异常, 通过 `tests/cache/test_l1cache_4way_vipt_safety.cpp` (cache-phase1.5-4way 实装) 排查

### R6: AGENTS.md `[cache-verilator]` 与 `[mmu-verilator]` 命名一致性

**Risk**: `[mmu-verilator]` (Change 2a) 与 `[cache-verilator]` (本 change) 命名风格需统一, 否则 grep 不友好。

**Mitigation**:
- 两个 family tag 命名规则一致: `<area>-verilator`
- AGENTS.md "已知测试状态" 段两行相邻, 便于读者对比
- tasks.md §9 显式 "AGENTS.md 新增行措辞" 与 Change 2a §D 类似

### Trade-off: TEST_CASE 数量 vs cache-phase1.5-4way 范围边界

**Trade-off**: 3 个 TEST_CASE 偏少, 可能漏测 cache CH_MEM 关键路径。

**Decision 接受**: cache-phase1.5-4way 自身 Acceptance line 120 "5 ELF cycle-identical 不回归" 已锁住 cycle 行为, 本 change 3 个 TEST_CASE 锁定 Verilator 链路可达性即足够。cache-phase1.5-4way 4-way / VIPT / LRU 专项测试留给 cache-phase1.5-4way 自身

## Migration Plan

### Phase 1: 占位状态维护 (现状)

本 change 提交后, `status: placeholder` 让 OpenSpec 标记为 not-ready-for-archive。

### Phase 2: cache-phase1.5-4way archive 触发启动 (未来)

```bash
# wave4 cache-phase1.5-4way archive
openspec archive cache-phase1.5-4way -y
# 验证 l1_cache_chmem.h 完整实装
ls ip/cache/tlm/l1_cache_chmem.h  # 期望存在
# 启动本 change 实施
openspec apply verilator-l1cache-e2e-coverage
```

### Phase 3: Spike 验证 Change 1 + Change 2a 双 archive (Pre-flight)

```bash
openspec change validate verilator-cpu-factory-extensible-params  # PASS
openspec change validate verilator-mmu-bare-plumbing-e2e  # PASS
# 跑 enable_cache 验证基础设施
./build/bin/cpu_verilator_sim --elf tests/cpu/manual_elf/l1cache_basic.elf --enable-cache  # 期望 fail-fast 或 tohost=1 (cache-phase1.5-4way archive 后)
```

### Phase 4: L1Cache-only baseline 表生成 (TDD Step 1)

```bash
# 5 ELF × 5 runs × mode=cache_only
for elf in add addi auipc beq jal; do
  for i in 1 2 3 4 5; do
    ./build/bin/cpu_verilator_sim --elf tests/cpu/riscv_tests/elf/rv32ui-p-$elf \
      --enable-cache --cycles 5000 2>/dev/null | grep CYCLES
  done
done | sort -n | awk '{a[NR]=$1} END {print a[int(NR/2)]}'  # median per ELF
# 追加 manual_elf l1cache_basic.elf × cache_mmu_bare 组
for i in 1 2 3 4 5; do
  ./build/bin/cpu_verilator_sim --elf tests/cpu/manual_elf/l1cache_basic.elf \
    --enable-mmu --mmu-mode bare --enable-cache --cycles 5000 2>/dev/null | grep CYCLES
done | sort -n | awk '{a[NR]=$1} END {print a[int(NR/2)]}'
# 写入 tests/cache/test_l1cache_refill_verilator_baselines.csv
```

### Phase 5: TEST_CASE 实现 (TDD Step 2-4)

```cpp
// tests/cache/test_l1cache_refill_verilator.cpp
#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"
// ... include cache_keys, mmu_keys, etc.

TEST_CASE("l1cache_refill_verilator_hit_tohost1", "[cache-verilator][e2e]") {
  // **plumbing only — semantic assertions beyond hit/miss NOT verified**
  // skip-when-absent check ...
  // popen cpu_verilator_sim --enable-cache --elf <elf>
  // REQUIRE TOHOST=1 + cycle ≤ baseline_csv[elf][cache_only] × 1.2
}

TEST_CASE("l1cache_refill_verilator_miss_tohost1", "[cache-verilator][e2e]") {
  // **plumbing only — semantic assertions beyond hit/miss NOT verified**
  // skip-when-absent check ...
  // 配置 manual_elf l1cache_basic.elf (1 hit + 1 miss) ...
  // REQUIRE TOHOST=1 + cycle ≤ baseline × 1.5
}

TEST_CASE("l1cache_refill_verilator_mmu_bare_full_chain", "[cache-verilator][mmu][e2e]") {
  // **plumbing only — semantic assertions beyond hit/miss NOT verified**
  // skip-when-absent check ...
  // popen cpu_verilator_sim --enable-mmu --mmu-mode bare --enable-cache --elf l1cache_basic.elf
  // REQUIRE TOHOST=1 + cycle ≤ baseline_csv[l1cache_basic][cache_mmu_bare] × 1.5
}

#endif  // CF_PLUGIN_USE_CH_MEM
```

### Phase 6: tests/CMakeLists.txt 注册

```cmake
list(APPEND CHMEM_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cache/test_l1cache_refill_verilator.cpp
)
target_compile_definitions(chipforge_tests_chmem PRIVATE
    CF_L1CACHE_VERILATOR_BASELINE_CSV="${CMAKE_SOURCE_DIR}/tests/cache/test_l1cache_refill_verilator_baselines.csv"
)
```

### Phase 7: 文档 + 架构门禁 (TDD Step 5)

```bash
bash tools/verify_adr.sh
bash tools/verify_plugin_decision.sh
bash tools/check_plugin_portability.sh
# 0 失败
# AGENTS.md §honesty_audit 增加 [cache-verilator] 3/3 PASS
# CHANGELOG v0.9.x 段新增本 change 条目
```

### Rollback Strategy

若 TEST_CASE 3 (L1Cache + MMU 联动) 因 cache-phase1.5-4way 4-way 升级后行为差异 fail:
1. 接受降级: TEST_CASE 3 标注为 "deferred to cache-phase1.5-4way cycle-identical validation"
2. release 标注: `[cache-verilator] 2/3 PASS`, 1 deferred

## Open Questions

### Q1: `[cache-verilator]` 与 `[mmu-verilator]` family tag 命名风格?

候选:
- (A) `[cache-verilator]` (单一 family tag, 与 `[mmu-verilator]` 对齐)
- (B) `[cache-verilator][e2e]` (双 family tag, 与 `[mmu-verilator][e2e]` 一致)
- (C) `[verilator][cache]` (嵌套, 复用现有 [verilator])

倾向 (B): 与 Change 2a `[mmu-verilator][e2e]` 风格一致, Catch2 `--list-tags` 输出对齐

### Q2: TEST_CASE 2 miss path 触发 refill FSM 后 cycle cap?

候选:
- (A) × 1.2 (与 hit path baseline 一致)
- (B) × 1.5 (manual_elf 接受标准, 因为不在 5 ELF baseline 表)
- (C) × 2.0 (宽松, refill 状态机开销可能较大)

倾向 (B): 与 Change 2a TEST_CASE 5 (full-chain no-cache manual_elf) 接受标准一致

### Q3: L1Cache CH_MEM 实装差异导致 TEST_CASE 1 fail?

候选:
- (A) 接受降级, release 标注 deferred
- (B) 调整 baseline cap 至 × 1.5, 重新跑 baseline
- (C) 阻塞 archive 直到 cache-phase1.5-4way 修复

倾向 (A): 与 cache-phase1.5-4way Acceptance line 120 "5 ELF cycle-identical 不回归" 已有约束, 本 change 复用该约束

### Q4: L1Cache baseline CSV 跨平台策略?

候选:
- (A) 仅 Linux
- (B) Linux + macOS (两份 CSV)
- (C) 跨平台 cycle cap × 1.5

倾向 (A): 与 Change 2a 一致

### Q5: cache-phase1.5-4way 占位触发本 change 启动的机制?

候选:
- (A) 人工触发 (cache-phase1.5-4way archive 后 reviewer 手动改本 change status)
- (B) CI 自动触发 (openspec validate 检测到依赖 archive 自动激活)
- (C) GitHub webhook (cache-phase1.5-4way merged → 本 change 自动 status: ready)

倾向 (A): 现状 OpenSpec 无 auto-trigger, 人工触发最稳
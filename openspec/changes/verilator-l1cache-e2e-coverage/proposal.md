---
initiative: wave4-csr-cache-dse
priority: P2
version_target: v0.9.0
status: placeholder
depends_on:
  - verilator-cpu-factory-extensible-params
  - cache-phase1.5-4way
---

# verilator-l1cache-e2e-coverage — L1Cache CH_MEM Verilator 端到端验证（最小验证链路, **wave4 占位, v0.9.0**）

## Why

`tests/cache/` 7 个测试,**0 个**走 Verilator 端到端。`tests/cpu/test_cpu_verilator_sim.cpp` 唯一 Verilator e2e 测试覆盖"CPU + Bare translation + 无 L1Cache"单一路径——L1Cache 在 Verilator 链路**完全不可达**。

后果:
1. `cache-phase1.5-4way`（Oracle #8 修订）显式约束 "5 ELF cycle-identical 不回归" —— 但当前 L1CachePlugin **没有 CH_MEM 完整实装**（`ip/cache/tlm/` 仅 `l1_cache_refill_fsm_chmem.h` 6d.7 独立 FSM PoC,229 行），所以 cache 4-way 升级**无法在 Verilator 链路验证**——只能在 TLM / CppHDL Simulator 跑。
2. wave4 DSE 维度（size × assoc × replacement × line_size sweep）`tools/dse/cache_dse_sweep.sh` 缺 assoc 维度,缺 Verilator sim cycle baseline 数据。
3. v0.10.4 hotfix 仅修 MMU, L1Cache CH_MEM 无对应防护层。

**本 change 范围**: 在 `cache-phase1.5-4way` archive（提供 `l1_cache_chmem.h` 完整实装）**之后**启动, 引入 L1Cache Verilator 端到端 e2e 测试,采用**最小验证链路**（仅验证 refill FSM + hit/miss basic path, 不做 4-way set-associative 测试、不做完整 L1CachePlugin CH_MEM 重构——这些属 cache-phase1.5-4way 已完成的 scope）。

## What Changes

### 1. 新增 L1Cache Verilator 端到端测试 (`[cache-verilator]` family)

**位置**: `tests/cache/test_l1cache_refill_verilator.cpp` (CH_MEM-only, `#ifdef CF_PLUGIN_USE_CH_MEM`)

3 个新 TEST_CASE:

| # | TEST_CASE 名 | 验证内容 |
|---|-------------|---------|
| 1 | `l1cache_refill_verilator_hit_tohost1` | cache hit 路径 → L1CachePlugin CH_MEM hit → tohost=1, cycle ≤ baseline × 1.2 |
| 2 | `l1cache_refill_verilator_miss_tohost1` | cache miss 路径 → refill FSM 触发 → refill_done → tohost=1 |
| 3 | `l1cache_refill_verilator_mmu_bare_full_chain` | `--enable-mmu --mmu-mode bare --enable-cache` 跑通 manual_elf l1cache_basic.elf, 验证 L1Cache + Bare translation 联动 |

**关键约束** (从 Oracle 推荐的"最小验证链路"继承):
- **不**测 4-way set-associative（属 cache-phase1.5-4way Acceptance, 它自己跑 `test_l1cache_4way_*`）
- **不**做完整 L1CachePlugin CH_MEM 重构（cache-phase1.5-4way §What Changes #1 已完成）
- **不**改写 `l1_cache_refill_fsm_chmem.h`（仅消费 6d.7 已实装的 FSM）
- **复用** cache-phase1.5-4way 实装的 `l1_cache_chmem.h`（本 change 仅写 e2e 测试, 不实装）

### 2. cycle baseline 表 (L1Cache-only)

**位置**: `tests/cache/test_l1cache_refill_verilator_baselines.csv`

`cpu_verilator_sim --enable-cache --elf <rv32ui-p-X>` 跑 5 次取 median, 与 Change 2a MMU baseline 表类似 schema (`elf_name,mode,median_cycles,verilator_version`)。

mode 列值 `cache_only` (区分于 Change 2a 的 `bare_mmu`)。

### 3. L1Cache + MMU 联动 baseline (TEST_CASE 3)

**位置**: `tests/cache/test_l1cache_refill_verilator_baselines.csv` 第二段（同 CSV 文件追加）

`cpu_verilator_sim --enable-mmu --mmu-mode bare --enable-cache --elf l1cache_basic.elf` 跑 5 次取 median, mode 列值 `cache_mmu_bare`。

注: 本 change 与 Change 2a 共享 `--enable-mmu` 路径, 但 baseline 表分离（不同 family 维护不同 CSV）。

### 4. CHANGELOG/AGENTS.md 同步

遵守 AGENTS.md §honesty_audit:
- CHANGELOG v0.9.x §Verification 段新增 `[cache-verilator]` PASS cases
- AGENTS.md "已知测试状态" 段增加 `[cache-verilator]` 行
- `[verilator]` `[mmu-verilator]` 数字不变（wave4 不动 wave5 baseline）

### 5. ADR 锚点

- **ADR-040 v2.0** (✅): 9 项 CI 检查; `l1cache_refill_verilator.cpp` 走 CppHDL Simulator harness, 不增加 D4 约束检查
- **ADR-044** (cache IP 级): §3.2 VIPT 索引契约, 本 change 复用 cache-phase1.5-4way 升级后 4-way 数据, 验证方式同 cache-phase1.5-4way Acceptance line 120 cycle-identical
- **ADR-046 v2.0** (✅): 复用 `l1_cache_refill_fsm_chmem.h` 6d.7 FSM, 不新增 FSM

## Capabilities

### New Capabilities

- `verilator-l1cache-e2e`: 定义 `cpu_verilator_sim --enable-cache` 路径在 Verilator 端到端的 e2e 验证 contract, 包括 cache hit / miss / MMU-L1Cache 联动 3 个 TEST_CASE。

### Modified Capabilities

- **`mmu-cache-integration-test`** (`openspec/specs/mmu-cache-integration-test/spec.md`): 新增 "L1Cache + MMU Verilator full-chain e2e" requirement — `--enable-mmu --mmu-mode bare --enable-cache` 链路在 Verilator 仿真下 tohost=1 + cycle ≤ baseline × 1.5。具体 delta 文件: `openspec/changes/verilator-l1cache-e2e-coverage/specs/mmu-cache-integration-test/spec.md` (修复 S2 OpenSpec delta 流程: delta 放本 change 自己的 specs/, 不直接编辑 main spec; archive 时合并)。

**Cache 侧 spec 排除说明** (修复 S11): 本 change §Modified 列表**不**包含 cache 侧 spec（如 `cache-plugin-contract`, `cache-keys`, `replacement-policy`），原因：
- TEST_CASE 1 (cache hit) 与 TEST_CASE 2 (cache miss) 是 CLI plumbing 验证, 走 `cpu_verilator_sim --enable-cache` 跑 baseline 5 ELF tohost=1, 不修改 L1Cache plugin 行为契约
- cache 侧 spec (cache plugin / cache keys / replacement policy / VIPT safety) 由 `cache-phase1.5-4way` archive 时维护, 本 change 仅**消费**已实装的 `l1_cache_chmem.h` (cache-phase1.5-4way 提供), 不修改 cache 侧 spec
- L1Cache + MMU 联动 (TEST_CASE 3) 通过 `--enable-mmu --mmu-mode bare --enable-cache` 三 flag 组合, 验证链路可达性, 同样**不**修改 cache 侧 spec

若 cache 侧 spec 后续需要 e2e 测试覆盖, 由独立 change 处理 (类似 wave6+ follow-up)。

## Impact

- **修改代码** (~250 LOC):
  - **`ip/cpu/cpu_factory_chmem.h::build_cpu`** (修复 C3: 替换 `enable_cache=true` 路径的 throw 为 `pb.register_plugin(L1CachePlugin<ch_uint<32>>(...))`, ~30 LOC)
  - `tests/cache/test_l1cache_refill_verilator.cpp` (新建, ~180 LOC)
  - `tests/CMakeLists.txt` (+5 LOC: 新 test source + baseline CSV compile_definitions)
  - `tests/cpu/manual_elf/build_l1cache_basic.S` 已存在 (Change 1 vendor), 本 change 不重写
- **新增测试 cases**: 3 个 TEST_CASE (`[cache-verilator]` family)
- **新增 baseline 数据文件**: `tests/cache/test_l1cache_refill_verilator_baselines.csv` (10 行: 5 ELF × cache_only + 1 ELF l1cache_basic.elf × cache_mmu_bare)
- **修改 specs**:
  - `openspec/specs/mmu-cache-integration-test/spec.md` (delta: 新增 Verilator full-chain requirement)
- **新增文档**:
  - `openspec/changes/verilator-l1cache-e2e-coverage/design.md` (~120 LOC)
  - `openspec/changes/verilator-l1cache-e2e-coverage/tasks.md` (~120 LOC, TDD 5 步 + Spike + L1Cache wiring task group 1.5)
  - `openspec/changes/verilator-l1cache-e2e-coverage/specs/verilator-l1cache-e2e/spec.md` (~70 LOC)
  - CHANGELOG.md v0.9.x 段 +5-8 行
  - AGENTS.md "已知测试状态" 段 +2-3 行
- **依赖关系**:
  - **硬阻塞 #1**: Change 1 `verilator-cpu-factory-extensible-params` (archive 后才有 `--enable-cache` CLI flag)
  - **硬阻塞 #2**: `cache-phase1.5-4way` archive 后才有 `l1_cache_chmem.h` 完整实装, 否则 Change 1 的 L1Cache fail-fast 阻塞
  - **本 change 内 wiring 翻转** (修复 C3): 本 change 自带 task group 1.5, 替换 Change 1 的 L1Cache throw 为注册 `L1CachePlugin CH_MEM`。这是双 archive 后的**第三步启动条件**。
  - **与 Change 2a 软依赖**: TEST_CASE 3 消费 `--enable-mmu --mmu-mode bare`, 但 Change 2a 自身也仅是 plumbing 验证, 不强依赖
- **风险**:
  - **R1** (cache-phase1.5-4way 时序): 当前 0/43 tasks, archive 时间估时 6-12 月。`status: placeholder` 反映此风险。
  - **R2** (L1Cache CH_MEM 行为差异): cache-phase1.5-4way 实装的 `l1_cache_chmem.h` 行为可能与 TLM L1CachePlugin 有 subtle 差异, baseline × 1.2 上限可能不足。
  - **R3** (VIPT aliasing): cache-phase1.5-4way §3 VIPT aliasing safety 测试属 cache-phase1.5-4way scope, 本 change 不复测 (Oracle #8 cycle-identical 已约束)。
  - **R4 (新增, 修复 C3)**: L1Cache wiring 修改需评估对 `verify_plugin_decision.sh` 的影响——L1CachePlugin CH_MEM 注册走 `at_stage` 闭包是否符合 D4 无状态机 + 无 `void tick()` 重写, 需 task group 1.5.2 显式验证。
- **估时**: 2-3 周（与 cache-phase1.5-4way 同步启动, archive 后 ~2 周完成, 含 L1Cache wiring task group 1.5 的 ~0.5 周）
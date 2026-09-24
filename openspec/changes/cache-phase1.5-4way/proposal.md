---
initiative: wave4-csr-cache-dse
priority: P2
version_target: v0.9.0
status: placeholder
depends_on: []
---

# cache-phase1.5-4way — L1Cache 4-way LRU 升级 (占位)

> **状态**: 🟡 占位 change, 待 wave3 (`wave3-mmu-real-memory-and-cycle`) archive 后展开 design.md / tasks.md 细化
> **战略位置**: 与 `phase-1.5-wave-4` 并行 (无依赖), 但同属 v0.6.0 版本节点
> **关键约束**: ADR-044 §2.5 建议升级, 当前 256×1 direct-mapped 在 VIPT 安全性上有 edge case 风险

## Why

当前 L1Cache 是 **256 sets × 1 way direct-mapped** (Phase 1.2 实现):

1. **VIPT 安全性**: VIPT (Virtual Index Physically Tagged) 在 direct-mapped 下, 当 `kIdxBits + kOffsetBits > page_offset_bits` 时存在 synonym / aliasing 问题
   - 4KB page offset = 12 bits, 当前 `kOffsetBits=6` + `kIdxBits=8` = 14 bits > 12 → **synonym 风险**
   - 升级到 4-way 后, VIPT 索引从 `vaddr[13:6]` (8 bits) 改为 `vaddr[13:6]` (8 bits, 但 way 0-3 分担), 单 way 仍 8 bits → 但因为 4-way 可缓解 alias (cache line 在不同 way 各一份)
2. **DSE 维度缺失**: `cache-dse-sweep` 当前仅扫 size/replacement/line_size, 缺 assoc 维度 → assoc=4 是经典 L1 DSE 候选
3. **ADR-044 §2.5 显式建议**: "升级到 64×4 set-associative (LRU 4-way 替换策略)"

Phase 6d 6d.7 已实装 `l1_cache_refill_fsm_chmem.h` (4 状态 FSM), 但当前 FSM 配合 1-way direct-mapped, **未触发 miss rate 优势**——升级 4-way 后 FSM 行为差异才有意义。

## Pending Open Questions (须 wave3 archive 后回答)

- **Q1**: LRU 实现策略? **精确 LRU** (每 set 4 entry LRU counter) vs **PLRU** (pseudo-LRU, 树形 bit, 面积小)?
- **Q2**: 4-way 与 VIPT 兼容性? 索引位是否保持 `vaddr[kIdxBits+kOffsetBits-1:kOffsetBits]` (8 bits), 仅 way 选择变 4 路?
- **Q3**: 与 P1#3 `mmu-paddr-consume-and-real-memory` 的 PADDR-first 路径集成? 当前 `L1CachePlugin` 接收 `CacheReq.address = paddr` (TLM 模式), 4-way 后 PADDR 是否需要重哈希?
- **Q4**: DSE 工具链是否同步支持 assoc 维度 (默认 sweep size × assoc × replacement × line_size)?

## What Changes (待 wave3 后展开)

> ⚠️ 以下为占位大纲, 实际 implementation 待 wave3 archive 后细化

### 1. L1CachePlugin 4-way 改造

- **位置**: `ip/cache/tlm/L1CachePlugin.h/.cpp` + `ip/cache/tlm/l1_cache_chmem.h` (新建 CH_MEM 版)
- **现状**: `ch_mem<cf::plugin::uint_t<kTagBits>, kNumSets> tags_` + `ch_mem<cf::plugin::uint_t<kLineDataBits>, kNumSets> data_`
- **目标**: 升级为 4-way
  ```cpp
  static constexpr size_t kWays = 4;
  array_store<ch_mem<cf::plugin::uint_t<kTagBits>, kNumSets>, kWays> tags_;
  array_store<ch_mem<cf::plugin::uint_t<kLineDataBits>, kNumSets>, kWays> data_;
  array_store<ch_mem<cf::plugin::bool_t, kNumSets>, kWays> valid_;
  array_store<ch_mem<cf::plugin::uint_t<8>, kNumSets>, kWays> lru_counter_;  // 0=MRU, 255=LRU
  ```
- ADR-040 Tier-2 #1 `array_store` 优先 (双缓冲 CH_MEM 支持)

### 2. LRU 替换策略实装

- **新建** `ip/cache/policies/lru_4way_policy.h/.cpp` (~150 LOC):
  - `LRUPolicy4Way::select_victim(set_idx)` → 返回 LRU way 编号
  - `LRUPolicy4Way::on_access(set_idx, way)` → 更新 LRU counter
- 与现有 `ip/mmu/policies/lru_policy.h` 同模式 (模板化 + 工厂创建)

### 3. VIPT 安全验证

- **新建** `tests/cache/test_l1cache_4way_vipt_safety.cpp` (~150 LOC):
  - 2 个虚拟地址映射到同一 cache set 但不同 physical address → 4-way 下应同时 cache (不冲突)
  - 5+ aliases 场景 → 验证 LRU 替换正确
- 这是 ADR-044 §2.5 显式要求的 VIPT 安全性验证

### 4. DSE 集成 (依赖 Q4 回答)

- **修改** `tools/dse/cache_dse_sweep.sh` (如有): 加 assoc 维度
- 新增 `soc/cpu/docs/dse/cache-dse-sweep/assoc_4way.csv` 落盘

### 5. CH_MEM FSM 集成

- **修改** `ip/cache/tlm/l1_cache_refill_fsm_chmem.h` (Phase 6d 6d.7 引入):
  - `MISS → REFILL_WAIT` 状态加 way 选择 (LRU victim way)
  - `REFILL_WAIT → IDLE` 状态写选定 way 而非 way 0

### 6. ADR-044 v2.0 修订

- **修改** `ip/cache/docs/adr/ADR-044-*.md` (~80 LOC 增量):
  - §2.5 升级路径: 256×1 → 64×4 set-associative (LRU)
  - §3.2 VIPT 索引契约双端验证 (4-way 安全性)
- **新增** ADR `l1cache-4way-lru-policy` (~180 LOC):
  - §Context: direct-mapped VIPT 风险
  - §Decision: 4-way + 精确 LRU counter (vs PLRU)
  - §Consequences: 与 ADR-040 Tier-2 array_store 集成

## Capabilities

### New Capabilities

- `cache-4way-lru-policy`: 定义 L1Cache 4-way set-associative + LRU 替换策略 contract

### Modified Capabilities

- `mmu-cache-integration-test`: 新增 "VIPT aliasing safety under 4-way set-associative" requirement
- `dse-cache-policy-foundation` (如有): 新增 assoc 维度

## Impact (估算, 待 wave3 后细化)

- **修改代码**:
  - `ip/cache/tlm/L1CachePlugin.h/.cpp` (大改造, +200 LOC: 4-way storage)
  - `ip/cache/tlm/l1_cache_chmem.h` (新建 CH_MEM 版, ~250 LOC)
  - `ip/cache/tlm/l1_cache_refill_fsm_chmem.h` (+30 LOC: way 选择)
  - `ip/cache/policies/lru_4way_policy.h/.cpp` (新建, ~150 LOC)
- **新增测试**:
  - `tests/cache/test_l1cache_4way_lru.cpp` (~100 LOC)
  - `tests/cache/test_l1cache_4way_vipt_safety.cpp` (~150 LOC)
- **新增 ADR**:
  - `ip/cache/docs/adr/l1cache-4way-lru-policy.md` (~180 LOC)
  - ADR-044 v2.0 修订 (+80 LOC)
- **DSE**:
  - `soc/cpu/docs/dse/cache-dse-sweep/assoc_4way.csv` 落盘
  - `tools/dse/cache_dse_sweep.sh` 改造 (依赖 Q4)

## Acceptance (待 wave3 后细化)

- [ ] 4-way + LRU 实装 (新测试 PASS)
- [ ] VIPT aliasing 安全验证通过 (`test_l1cache_4way_vipt_safety` PASS)
- [ ] CH_MEM FSM 配合 4-way (Phase 6d 6d.7 测试不回归)
- [ ] **5 ELF cycle-identical 不回归约束 (Oracle #8 修订)**: Phase 6d 6d.5 E8 实测 add/addi/auipc/beq/jal 5 ELF 与 Verilator sim 0% diff, 本 change 升级 4-way 后必须重新跑该验证 (`tests/cpu/test_cpu_verilator_sim.cpp` 含 5 ELF × tohost=1 验证, 全部 PASS). **若 cycle-identical 失败, 必须显式声明破坏并跟随重新 baseline**
- [ ] `[cache]` 既有 21 baseline + 新增 ≥3 测试 PASS (含 Phase 1.2 mmr/miss/refill)
- [ ] `[mmu]` 50 + 新增 VIPT safety 测试 PASS
- [ ] `[cpu-l1-mmu-demo]` 6/6 PASS 不回归 (L1 升级后 demo 仍能跑 5 ELF)
- [ ] `[riscv-tests]` 40/40 PASS 不回归
- [ ] DSE assoc 维度可扫 (依赖 Q4)
- [ ] 3 门禁全 PASS
- [ ] CHANGELOG v0.9.0 段本 change 条目
- [ ] `openspec archive cache-phase1.5-4way -y`

## Risk (待 wave3 后细化)

- **R1 (Q1 LRU 选型)**: 精确 LRU 面积大 (每 way 8-bit counter), PLRU 面积小但 miss rate 略高 → 需 P1#4 cycle-precision + DSE 数据驱动决策
- **R2 (Q2 VIPT 兼容性)**: 4-way 下索引位 8 bits, VIPT 索引 source 仍为 `pl::MMU_VADDR` → 与 P1#3 集成, 验收需联合测试
- **R3 (CH_MEM FSM 维护成本)**: `l1_cache_refill_fsm_chmem.h` 已实装 (Phase 6d 6d.7), 4-way 改造需同步, 双文件维护风险
- **R4 (DSE sweep 维度爆炸)**: assoc=4 是新增维度, sweep config 数翻倍 → DSE runtime 可能从 60s → 240s (P1#4 cycle-precision 加速可缓解)

## 后续 Action (本 change 不 pending 具体任务)

- ⏸ 等 wave3-mmu-real-memory-and-cycle archive (与 P2#6 并行, 无依赖)
- ⏸ 回填 4 个 Open Questions (Q1-Q4) 答案
- ⏸ 启动 cache-4way 实装 (TDD 5 步)
- ⏸ Phase 2 RV64GC 启动后, Cache 4-way 是 L2 DSE 模板 (Phase 5+ 复用)

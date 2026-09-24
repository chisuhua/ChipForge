---
initiative: wave4-csr-cache-dse
priority: P2
version_target: v0.9.0
status: placeholder
---

# Tasks — cache-phase1.5-4way (占位)

> ⚠️ 占位 tasks, 待 wave3 (`wave3-mmu-real-memory-and-cycle`) archive 后展开

## 0. 前置条件

- [ ] 等 wave3-mmu-real-memory-and-cycle archive (与 phase-1.5-wave-4 并行, 无依赖)

## 1. Open Questions 回答 (wave3 archive 后立即)

- [ ] 1.1 Q1 决策: LRU 实现策略 (精确 LRU vs PLRU)
- [ ] 1.2 Q2 决策: 4-way 与 VIPT 兼容性 (索引位 + way 选择)
- [ ] 1.3 Q3 决策: 与 P1#3 PADDR-first 路径集成
- [ ] 1.4 Q4 决策: DSE 工具链是否同步支持 assoc 维度

## 2. L1CachePlugin 4-way 改造

- [ ] 2.1 `ip/cache/tlm/L1CachePlugin.h/.cpp` 升级:
  - `static constexpr size_t kWays = 4;`
  - 4 个 `array_store<ch_mem<...>, kWays>` (tags_/data_/valid_/lru_counter_)
  - `at_stage("lookup", NORMAL)` 4-way 并行 tag 比对
- [ ] 2.2 新建 `ip/cache/tlm/l1_cache_chmem.h` (~250 LOC, CH_MEM 版)

## 3. LRU 替换策略

- [ ] 3.1 新建 `ip/cache/policies/lru_4way_policy.h/.cpp` (~150 LOC)
- [ ] 3.2 `LRUPolicy4Way::select_victim(set_idx)` + `on_access(set_idx, way)` 实装
- [ ] 3.3 工厂 `LRUPolicyFactory::create("4way_lru")` 注册

## 4. VIPT 安全验证

- [ ] 4.1 新建 `tests/cache/test_l1cache_4way_vipt_safety.cpp` (~150 LOC):
  - 2 vaddr → 1 cache set, 4-way 同时 cache (aliasing 安全)
  - 5+ aliases → LRU 替换正确
- [ ] 4.2 新建 `tests/cache/test_l1cache_4way_lru.cpp` (~100 LOC):
  - LRU counter 更新正确
  - 4-way hit/miss 路径

## 5. CH_MEM FSM 集成

- [ ] 5.1 修改 `ip/cache/tlm/l1_cache_refill_fsm_chmem.h` (+30 LOC):
  - `MISS → REFILL_WAIT` 状态加 way 选择 (LRU victim)
  - `REFILL_WAIT → IDLE` 状态写选定 way
- [ ] 5.2 verify `l1cache_refill_fsm` 6d.7 测试不回归

## 6. ADR 修订

- [ ] 6.1 修改 `ip/cache/docs/adr/ADR-044-*.md` (§2.5 + §3.2, +80 LOC)
- [ ] 6.2 新建 `ip/cache/docs/adr/l1cache-4way-lru-policy.md` (~180 LOC)
- [ ] 6.3 `docs/architecture/adr.md` 注册新 ADR

## 7. DSE 集成 (依赖 Q4)

- [ ] 7.1 修改 `tools/dse/cache_dse_sweep.sh` 加 assoc 维度
- [ ] 7.2 新建 `soc/cpu/docs/dse/cache-dse-sweep/assoc_4way.csv` 落盘
- [ ] 7.3 DSE runtime ≤ 240s (P1#4 cycle-precision 加速)

## 8. verify pass

- [ ] 8.1 `[cache]` 既有 21 + 新增 ≥3 测试 PASS
- [ ] 8.2 `[mmu]` 50 + 新增 VIPT safety 测试 PASS
- [ ] 8.3 `[cpu-l1-mmu-demo]` 6/6 PASS 不回归
- [ ] 8.4 `[riscv-tests]` 40/40 PASS 不回归
- [ ] 8.5 `[cpphdl]`/`[chmem]` Phase 6d 测试不回归 (CH_MEM FSM 同步)

## 9. CI 门禁 + 文档

- [ ] 9.1 `bash tools/verify_adr.sh` PASS (含新 ADR)
- [ ] 9.2 `bash tools/verify_plugin_decision.sh` PASS (D4 合规)
- [ ] 9.3 `bash tools/check_plugin_portability.sh` PASS (array_store 优先)
- [ ] 9.4 `CHANGELOG.md` v0.6.0 段本 change 条目
- [ ] 9.5 `soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md` Wave 4 段更新

## 10. commit + archive

- [ ] 10.1 1 个原子 commit (含 test + ADR + 4-way 改造 + LRU policy + VIPT 验证 + DSE 集成)
- [ ] 10.2 `openspec archive cache-phase1.5-4way -y`

## Acceptance

- [ ] 0 阻塞项解除
- [ ] 1.1-1.4 Open Questions 全部回答
- [ ] 2.1-2.2 L1CachePlugin 4-way (TLM + CH_MEM)
- [ ] 3.1-3.3 LRU 替换策略
- [ ] 4.1-4.2 VIPT 安全验证
- [ ] 5.1-5.2 CH_MEM FSM 集成
- [ ] 6.1-6.3 ADR 修订
- [ ] 7.1-7.3 DSE 集成
- [ ] 8.1-8.5 verify pass 全绿
- [ ] 9.1-9.5 CI 门禁 + 文档同步
- [ ] 10.1-10.2 commit + archive

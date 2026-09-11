# L1Cache 微架构

> 微架构文档配套 ADR-044 (VIPT 锁定)。Phase 1.3 当前实现是 PIPT 简化版 (offset=4 bits, 16B 粒度), Phase 1.5+ 升级到 VIPT (64B 真 cacheline, 4-way)。

## 1. 地址映射

### Phase 1.3 当前状态 (PIPT 简化)

```
addr (统一 vaddr/paddr) →
  idx    = addr[11:4]      (8 bits → 256 sets)
  tag    = addr[31:12]     (20 bits)
  offset = addr[3:0]       (4 bits, Phase 0 退化为 16B 粒度)
```

### Phase 1.5+ VIPT 目标 (ADR-044)

```
vaddr → idx    = vaddr[11:6]   (6 bits → 64 sets, VIPT 索引)
paddr → tag    = paddr[31:12]  (20 bits, 物理 tag 比对)
        offset = paddr[5:0]    (6 bits, 64B cacheline)
```

**VIPT 安全条件** (`idx_bits + offset_bits ≤ 12`):
- 4KB page page offset = 12 bits → idx_bits ≤ 6 (for 64B line)
- 2MB megapage page offset = 21 bits → idx_bits ≤ 15 (大页无 alias)
- 等价公式: `cache_bytes_per_way ≤ page_size`

## 2. 两阶段流水线

```
at_stage("lookup"):  vaddr idx + paddr tag → hit/miss → CacheResp
at_stage("refill"):  MemResp → write set → valid/dirty → 后续 hit
```

替换策略 LRUPolicy 支持 4-way (Phase 1.4 已落地, 见 `ip/cache/policies/`)。

## 3. 与 MMU 耦合 (VIPT 数据流方案 A)

MMUPlugin 在前置 stage 提供 `pl::MMU_VADDR` + `pl::PADDR` (双 Payload Key 方案),
L1CachePlugin 在 lookup 阶段读取两个 Payload Key 分别用于索引 (vaddr) 和 tag 比对 (paddr)。
详细接口契约见 ADR-044 §3.2。

```
[IBusPlugin 写 pl::PC (vaddr)] →
    [MMUPlugin::tlb_lookup_ifetch] → 写 pl::PADDR + pl::MMU_VADDR →
        [L1CachePlugin::lookup] →
            idx = extract_idx_from_vaddr(pl::MMU_VADDR)
            tag = extract_tag(pl::PADDR)
            → hit/miss
```

## 4. VIPT 安全参数配置

| 参数 | Phase 1.3 (当前) | Phase 1.5+ (目标) |
|------|-------------------|-------------------|
| kNumSets | 256 | 64 |
| Associativity | 1-way (direct-mapped) | 4-way |
| kIdxBits | 8 | 6 |
| kOffsetBits | 4 (Phase 0 简化) | 6 (64B) |
| kLineDataBits | 512 (64B 字段宽, 数据降级) | 512 (64B 真 cacheline) |
| 总容量 | 16KB (256×1×64B) | 16KB (64×4×64B) |
| VIPT 安全 | 仅 Phase 0 简化下形式安全 | ✅ 正式安全 |
| idx+offset bits | 8+4=12 ✓ | 6+6=12 ✓ |

编译期约束 (`L1CachePlugin.h`):
```cpp
static_assert(kIdxBits + kOffsetBits <= 12,
              "VIPT safety: idx_bits + offset_bits must <= 12. See ADR-044 §2.2.");
```

## 5. DSE 扫描边界

DSE 维度 (见 `ip/cache/README.md` §6) 中 `[4, 8, 16, 32, 64] KB` 容量范围:
- ≤ 16KB 1-way + 64B line: VIPT 安全 (Phase 0 简化下)
- 32KB 4-way + 64B line: VIPT 安全 (8KB/way > 4KB → 需 OS page coloring 或反别名硬件, 推迟 Phase 2+)
- 64KB+ : 不在 Phase 1.x 范围, 推迟 Phase 2+ L2CachePlugin

## 6. 替换策略

LRUPolicy (Phase 1.4 reference impl):
- 4-way set-associative: 每 set 4 个 entry, 维护 4-bit LRU 状态
- 硬件代价 < 10 门/way
- 完整 4 策略 (`None`/`FIFO`/`LRU`/`RRIP`) 见 `ip/mmu/policies/` (TLB 同构, 命名空间隔离)

## 7. 已知限制

- **Phase 0 简化**: `kLineDataBits=512` 退化为 `uint64_t` (因 `uint_t<512>` 上限 64), 实际 64B cacheline 数据完整性要 Phase 6 multi-precision
- **VIPT 数据流方案 A 未实装**: MMUPlugin 暂只写 `pl::PADDR`, 不写 `pl::MMU_VADDR` — 推迟到 `mmu-tlb-ptw-impl` 实施
- **32KB+ 反别名**: 超出 VIPT 安全容量, 需 page coloring 或 abort/retry 硬件, 推迟 Phase 2+

## 8. 相关文档

- [ADR-044 VIPT 锁定](adr/ADR-044-l1-cache-vipt-coherence.md) — 本文档决策依据
- [L1Cache 集成契约](integration.md) — CPU/MMU 集成接口
- [L1CachePlugin README](../README.md) — IP 总览
- [RISC-V 核比较表](adr/ADR-044-l1-cache-vipt-coherence.md#12-三种方案在-risc-v-行业的分布)

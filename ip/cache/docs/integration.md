# L1Cache 集成契约

> 配套 ADR-044 (VIPT 锁定) + mmu-tlb-ptw-impl 实施时的接口规范。

## 1. CPU Pipeline 集成

L1CachePlugin 通过 `PipeBuilder::at_stage()` 注册在 `lookup` + `refill` 两个阶段。
与 CPU 的集成点（Phase 1.5+ VIPT 数据流方案 A）:

```
[IBusPlugin::fetch] →
    写 pl::PC (vaddr)
    [MMUPlugin::tlb_lookup_ifetch] →
        读 pl::PC (vaddr) → TLB lookup
        hit: 写 pl::PADDR = paddr
        hit: 写 pl::MMU_VADDR = vaddr (Phase 1.5+ VIPT 数据流)
        miss: 写 pl::PTW_ACTIVE=1 + pl::PTW_VADDR + pl::PTW_ASID
    [L1CachePlugin::lookup] →
        读 pl::MMU_VADDR → extract_idx_from_vaddr() → set index
        读 pl::PADDR     → extract_tag()             → tag compare
        → 写 pl::CACHE_HIT (bool) + pl::CACHE_DATA
    [IBusPlugin 后续阶段] →
        读 pl::CACHE_HIT, 若 hit 直接用 pl::CACHE_DATA
```

## 2. VIPT 数据流 Payload Key 接口 (Phase 1.5+)

### 输入 Payload Key (L1CachePlugin 读取)

| Key 命名空间 | Key 名 | 类型 | 来源 | 用途 |
|---|---|---|---|---|
| `cf::ip::mmu::payload::mmu_keys` | `MMU_VADDR` | `uint_t<64>` | MMUPlugin 输出 | VIPT cache 索引 |
| `cf::ip::mmu::payload::mmu_keys` | `PADDR` | `uint_t<64>` | MMUPlugin 输出 | 物理 tag 比对 |
| `cf::ip::cache::payload::cache_keys` | `CACHE_VADDR` | `uint_t<64>` | (alias MMU_VADDR) | L1Cache 内部使用 |

### 输出 Payload Key (L1CachePlugin 写出)

| Key 命名空间 | Key 名 | 类型 | 用途 |
|---|---|---|---|
| `cf::ip::cache::payload::cache_keys` | `CACHE_HIT` | `bool_t` | 命中/未命中 |
| `cf::ip::cache::payload::cache_keys` | `CACHE_DATA` | `uint_t<kLineDataBits>` | 缓存读出数据 |
| `cf::ip::cache::payload::cache_keys` | `CACHE_ERROR` | `bool_t` | 错误标志 |

### `mmu_keys.h` 增量需求

需要在 `ip/mmu/tlm/mmu_keys.h` 新增:
```cpp
static inline cf::plugin::Payload<uint64_t> MMU_VADDR{"mmu.translated_vaddr"};
```

**命名约定**: VADDR 是 MMU 阶段**输入** (上游写入), MMU_VADDR 是 MMU 阶段**输出** (供 L1Cache 消费)。

## 3. Bridge 适配层

`L1CacheTLMBridge` (`src/cf_plugin/bridge/l1_cache_bridge.h`) 在 `tick()` 末尾调 `pb.run()`。
Bridge 不感知 VIPT/PIPT 差异 — 索引逻辑全部在 Plugin 内部 at_stage 闭包。

**Bridge 边界**:
- Bridge 接受 `cf::bundles::CacheReq` (含 `address` 字段)
- Phase 1.3 当前: `address` 是统一 vaddr/paddr (MMU 暂未集成, 简化为 PIPT)
- Phase 1.5+: `address` 仍是 `paddr` (MMUPlugin 在前置 stage 已翻译), VIPT 索引用 vaddr 走 payload

## 4. SoC JSON 拓扑 (Phase 1.5+ 预期)

```json
{
  "modules": [
    {"name": "cpu",   "type": "RiscVCpuTLM",      "params": {...}},
    {"name": "mmu",   "type": "MMUTLMBridge",     "params": {"sv_mode": "sv39", "asid_bits": 9}},
    {"name": "l1i",   "type": "L1CacheTLMBridge", "params": {"num_sets": 64, "associativity": 4, "line_data_bits": 512}},
    {"name": "l1d",   "type": "L1CacheTLMBridge", "params": {"num_sets": 64, "associativity": 4, "line_data_bits": 512}},
    {"name": "mem",   "type": "MemoryTLM",        "params": {"size_kb": 1024}}
  ],
  "connections": [
    {"src": "cpu", "dst": "mmu", "port": "fetch_vaddr"},
    {"src": "cpu", "dst": "mmu", "port": "loadstore_vaddr"},
    {"src": "mmu", "dst": "l1i", "port": "fetch_paddr"},
    {"src": "mmu", "dst": "l1d", "port": "loadstore_paddr"},
    {"src": "l1i", "dst": "cpu", "port": "fetch_instr"},
    {"src": "l1d", "dst": "cpu", "port": "loadstore_data"}
  ]
}
```

## 5. 与 MMU 集成检查清单 (mmu-tlb-ptw-impl 实施时)

| # | 检查项 | 责任方 |
|---|--------|-------|
| 1 | `mmu_keys.h` 新增 `MMU_VADDR` Key | mmu-tlb-ptw-impl |
| 2 | `MMUPlugin::at_stage("tlb_lookup_*")` 同时写 `K::PADDR` + `K::MMU_VADDR` | mmu-tlb-ptw-impl |
| 3 | `L1CachePlugin` 引入 `cache_keys` 命名空间, 加 `CACHE_HIT`/`CACHE_DATA`/`CACHE_ERROR` Key | l1-cache-64b-4way change |
| 4 | `L1CachePlugin` 实现 `extract_idx_from_vaddr` helper, 修改 lookup 闭包用 vaddr 索引 | l1-cache-64b-4way change |
| 5 | `L1CachePlugin` 从 1-way → 4-way 升级 + `kIdxBits` 8→6 + `kOffsetBits` 4→6 | l1-cache-64b-4way change |
| 6 | L1Cache 4 个单元测试更新, 加 VIPT aliasing 反别名测试 | l1-cache-64b-4way change |

## 6. 集成检查命令

```bash
# 验证 mmu_keys.h 包含 MMU_VADDR
grep -q "MMU_VADDR" ip/mmu/tlm/mmu_keys.h

# 验证 L1CachePlugin 用 vaddr 索引
grep -q "extract_idx_from_vaddr" ip/cache/tlm/L1CachePlugin.cpp

# 验证 L1CachePlugin 编译期 VIPT 安全
grep -q "static_assert.*kIdxBits.*kOffsetBits.*12" ip/cache/tlm/L1CachePlugin.h

# 验证既有测试 PASS
bash tools/run_chipforge_tests.sh -L cache
```

## 7. 相关文档

- [L1Cache 微架构](architecture.md) — 地址映射 + VIPT 安全条件
- [ADR-044 VIPT 锁定](adr/ADR-044-l1-cache-vipt-coherence.md) — 决策依据
- [MMU 集成契约](../../mmu/docs/integration.md) — CPU 集成接口
- [MMU docs/architecture.md](../../mmu/docs/architecture.md) — MMU 总体数据流

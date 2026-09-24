# mmu-tlb-coherence-protocol

## Purpose

定义 `MultiLevelTLB` 多级 TLB 编排器的 coherence 协议：浅层 shadow fill、深层 PTW refill 传播、ASID 切换的反向失效。协议在 `MultiLevelTLB` 层集中实现，单级 `TLB` 不知道其他级存在。

## ADDED Requirements

### Requirement: 多级 TLB 浅层 shadow fill 协议

当 `MultiLevelTLB::lookup(vaddr, asid)` 在深层 Ln (n>0) 命中时，**当且仅当** `shadow_fill_from_next=true` 时，浅层 L0~L(n-1) MUST 全部回填相同的 translation。

回填 MUST 走 `TLB::insert_from()` 接口（专用 API，避免触发 statistics 计数），保证回填操作不影响 hit/miss 统计。

#### Scenario: L0 miss, L1 hit 触发 shadow fill
- **WHEN** 配置 `{levels: [L0, L1]}`, `shadow_fill_from_next=true`, L0 miss, L1 hit
- **THEN** MUST 返回 L1 的 LookupResult
- **AND** L0 MUST 收到 insert_from() 调用, 含相同 (vaddr, paddr, asid, perms)
- **AND** 第二次 lookup 同一 (vaddr, asid) MUST 在 L0 hit
- **AND** L0 hit_count() MUST 增加, L0 miss_count() MUST 仅增加 1 (只算原始 miss)

#### Scenario: shadow_fill_from_next=false 时不传播
- **WHEN** 配置 `{shadow_fill_from_next: false}`, L0 miss, L1 hit
- **THEN** MUST 返回 L1 的 LookupResult
- **AND** L0 MUST NOT 收到 insert_from() 调用
- **AND** 第二次 lookup 同一 (vaddr, asid) MUST 仍 L0 miss, L1 hit

### Requirement: PTW refill 写入最深层 + 浅层回填

`MultiLevelTLB::refill_from_ptw(vaddr, asid, paddr, perms)` MUST:
1. 写入最深层 (L_last) 的 `insert()` (计 evict 统计)
2. 当 `shadow_fill_from_next=true` 时, 浅层 L0 ~ L(n-1) MUST 全部 `insert_from()`
3. 同 (asid, vaddr) 的旧条目 MUST 被新条目覆盖, 无需显式反向失效（insert 覆盖语义）

#### Scenario: PTW 完成后 L1 写入 + L0 shadow fill
- **WHEN** 配置 `{levels: [L0, L1]}`, `shadow_fill_from_next=true`, 调用 `refill_from_ptw(vaddr, 0, paddr, 0xFF)`
- **THEN** L1 MUST 调用 insert() 含 (vaddr, paddr, 0, 0xFF)
- **AND** L0 MUST 收到 insert_from() 调用含相同内容
- **AND** 立即 lookup 同一 (vaddr, 0) MUST L0 hit
- **AND** L1 hit_count() MUST 不变 (PTW refill 不算 hit)

#### Scenario: shadow_fill=false 时仅写最深层
- **WHEN** 配置 `{shadow_fill_from_next: false}`, 调用 `refill_from_ptw(...)`
- **THEN** L1 MUST 调用 insert()
- **AND** L0 MUST NOT 收到 insert_from()
- **AND** 立即 lookup 同一 (vaddr, 0) MUST L0 miss, L1 hit

### Requirement: ASID 切换的反向失效协议

`MultiLevelTLB::invalidate_asid(asid)` MUST 逐级调用 `TLB::invalidate_asid(asid)`。`MultiLevelTLB::invalidate_vaddr(vaddr, asid)` MUST 逐级调用。`MultiLevelTLB::invalidate_all()` MUST 逐级调用。

SFENCE.VMA 指令语义映射:
- `SFENCE.VMA` (rs1=0, rs2=0): `invalidate_all()`
- `SFENCE.VMA rs1`: `invalidate_vaddr(rs1, 0)` (简化: 实际对所有 ASID)
- `SFENCE.VMA rs1, rs2`: `invalidate_vaddr(rs1, rs2)`
- `SFENCE.VMA zero, zero`: `invalidate_all()`

#### Scenario: ASID 切换清空所有级
- **WHEN** 配置 `{levels: [L0, L1]}`, 调用 `invalidate_asid(0x100)`
- **THEN** L0 MUST 收到 invalidate_asid(0x100) 调用
- **AND** L1 MUST 收到 invalidate_asid(0x100) 调用
- **AND** L0 中任何 asid==0x100 的条目 MUST 被清掉
- **AND** L1 中任何 asid==0x100 的条目 MUST 被清掉
- **AND** 其他 ASID 的条目 MUST 保留

#### Scenario: 单 vaddr 失效仅清匹配项
- **WHEN** 调用 `invalidate_vaddr(0x1000, 0x100)`
- **THEN** L0 和 L1 中 (asid==0x100 AND vaddr==0x1000) 的条目 MUST 被清掉
- **AND** L0 和 L1 中其他条目 MUST 保留

### Requirement: coherence 协议对单级 TLB 接口零侵入

单级 `TLB<ENTRIES, WAYS, ...>` MUST NOT 知道兄弟级存在, 接口 MUST 保持简洁:
- `lookup()`, `insert()`, `invalidate_vaddr()`, `invalidate_asid()`, `invalidate_all()`, `insert_from()`

`insert_from()` MUST 与 `insert()` 区别: 不计 hit/miss 统计, 不计 evict 统计（shadow fill 是后台行为, 不污染用户视角的命中率）。

#### Scenario: insert_from 不污染统计
- **WHEN** 调用 TLB `insert_from(vaddr, paddr, asid, perms)` 100 次
- **THEN** hit_count() MUST 不变
- **AND** miss_count() MUST 不变
- **AND** evict_count() MUST 不变
- **AND** entries_ 中 MUST 有 100 个有效条目

#### Scenario: 相同 vaddr 多次 insert 覆盖
- **WHEN** 同一 TLB 调 `insert(v1, p1, 0, 0)` 一次, 然后 `insert(v1, p2, 0, 0)` 一次
- **THEN** entries_ 中 vaddr==v1 的条目 MUST 存在, paddr==p2
- **AND** entries_ 中 vaddr==v1 的条目 MUST 仅 1 条 (覆盖, 不新增)
- **AND** 第二次 insert MUST 触发 evict_count() +1 (覆盖等价于 evict 旧 + insert 新)

### Requirement: MultiLevelTLB 自身状态查询

`MultiLevelTLB` MUST 提供:
- `size_t num_levels() const` 返回级数
- `TLBBase* level(size_t i) const` 返回第 i 级 TLB 指针 (i=0 是 L0, 最快最小)
- `uint64_t total_hit_count() const` 累加所有级 hit_count()
- `uint64_t total_miss_count() const` 累加所有级 miss_count()
- `uint64_t total_evict_count() const` 累加所有级 evict_count() (排除 insert_from 引起)

#### Scenario: 状态查询
- **WHEN** 配置 `{levels: [L0(8/8/LRU), L1(64/4/LRU)]}`, L0 hit 3, miss 2, L1 hit 5, miss 1
- **THEN** `num_levels()` MUST 返回 2
- **AND** `level(0)->name()` MUST 返回 "L0"
- **AND** `level(1)->name()` MUST 返回 "L1"
- **AND** `total_hit_count()` MUST 返回 8
- **AND** `total_miss_count()` MUST 返回 3

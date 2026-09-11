# ADR-044: L1 Cache↔MMU 耦合策略 — VIPT 锁定 + 反别名安全边界

| 字段 | 值 |
|------|-----|
| 状态 | 🚧 Phase 1 提案（锁设计方向，实装推迟到 `mmu-tlb-ptw-impl` 以后） |
| 来源 | MMU IP mmu-tlb-ptw-impl 预备调研（2026-07-01），对比行业 RISC-V 核 + 学术惯例 |
| 决策 | L1 ICache/DCache **统一 VIPT**；当前 Phase 1.3 16KB/1-way 安全；设计目标 64B 真正 cacheline 时 **必须 ≥ 4-way associativity**（安全条件：idx_bits+offset_bits ≤ 12）；L2 Cache **PIPT** |
| 关联 ADR | ADR-040（TLM→HDL 移植性约束）、ADR-041（Bridge 适配器 tick 许可）、ADR-042（MMU 推迟 → 已重启）、ADR-037（Plugin 范式） |

---

## 1. 背景与动机

### 1.1 MMU 集成暴露的空白决策

`ip/mmu/` IP 骨架已于 2026-06-29 落地（`mmu-ip-skeleton`）。MMU 是 vaddr→paddr 翻译的核心，但当前 `ip/cache/tlm/L1CachePlugin.h:17-23` 注释注明 "64-bit 物理" —— Cache 假定接收的是**物理地址**。MMU 与 L1 Cache 之间的耦合策略（Cache 索引使用 vaddr 还是 paddr）**全仓库无明文决策**。

`openspec/changes/archive/2026-06-29-mmu-ip-skeleton/design.md:47` 将 VIVT/VIPT/PIPT 推迟到 `mmu-tlb-ptw-impl`。本 ADR 在启动 `mmu-tlb-ptw-impl` 之前锁定此决策。

### 1.2 三种方案在 RISC-V 行业的分布

| 方案 | 使用核 | 延迟 | Alias 风险 | 适用 |
|------|--------|:---:|:----------:|------|
| **VIVT** | NVIDIA GPU | 无 TLB 开销 | 严重（同义 + homonym） | GPU L1 texture cache |
| **VIPT** ✅ | Rocket / BOOM / XiangShan / CVA6 D$ | 无额外（与 TLB 并行） | 轻微（安全条件下无） | **所有主流 RISC-V CPU** |
| **PIPT** | Xuantie C910 / 大多数 L2 | +TLB 延迟 | 无 | 保守设计 / L2 |

**结论**：VIPT 是 RISC-V 高性能核的**行业一致选择**。ChipForge L0 TLB hit 1 cycle 的性能目标只有 VIPT 才能达成（Cache lookup 与 TLB lookup 并行，命中时 paddr tag 对比）。

### 1.3 当前 L1CachePlugin 参数

来自 `ip/cache/tlm/L1CachePlugin.h:72-76`：

| 参数 | 值 | 说明 |
|------|:---:|------|
| `kNumSets` | 256 | 8-bit idx |
| `kTagBits` | 20 | addr[31:12] |
| `kIdxBits` | 8 | addr[11:4] |
| `kOffsetBits` | 4 | addr[3:0] (Phase 0 简化, 实际 64B line 应 6 bits) |
| `kLineDataBits` | 512 | 64-byte (Phase 0 退化为 uint64_t) |

**Phase 0 简化说明**：当前 offset=4 bits（16-byte 粒度），不是真正的 64B cacheline。这是 uint_t<512> 退化为 uint64_t 的阶段限制。本 ADR 按**真正 64B line（offset=6 bits）**给出安全条件，Phase 1.5+ 实装时切换。

---

## 2. 决策

### 2.1 L1 ICache + DCache 统一采用 VIPT

**虚地址索引 + 物理地址 Tag**:
- Cache **索引**（set index）从 **vaddr** 提取
- Cache **Tag** 从 **paddr** 提取（与 TLB 输出 paddr 比对）
- Cache line **数据** 用物理地址唯一标识

TTBL lookup 与 Cache lookup **并行启动**:
```
vaddr → [ TLB lookup ] ──paddr──┐
       ↓                         ↓
       [ Cache tag read ] ─── tag compare → hit/miss
       [ Cache index = vaddr[11:6] ]      (用 paddr tag 比)
```

### 2.2 VIPT 无 alias 的安全条件

**条件**: `idx_bits + offset_bits ≤ page_offset_bits`

对于 4KB page:
- `offset_bits = 6` (64B cacheline) → `idx_bits ≤ 6` → **最多 64 sets/way**

对于 2MB megapage (大页):
- `offset_bits = 6` → `idx_bits ≤ 15` (SV39 mega page offset is 21 bits)
- 大页时 alias 不是问题

**等价公式**: `cache_bytes_per_way ≤ page_size`

| Cache 形态 | bytes/way | 4KB 安全? | 结论 |
|---|---|---|---|
| 256 sets × 1-way × 64B = 16KB | 16KB | ❌ (idx_bits=8 > 6) | **需加 associativity** |
| 64 sets × 4-way × 64B = 16KB | **4KB** | ✅ (idx_bits=6 ≤ 6) | ✅ VIPT 安全 |
| 128 sets × 2-way × 64B = 16KB | 8KB | ❌ | 有 alias |
| 128 sets × 4-way × 64B = 32KB | 8KB | ❌ | 有 alias 但 64KB/way 对大页安全 |
| 64 sets × 8-way × 64B = 32KB | **4KB** | ✅ | ✅ VIPT 安全 |

### 2.3 Phase 1.3 当前 16KB/1-way 的安全性

当前 L1CachePlugin 使用 **Phase 0 简化**（offset=4 bits，16B 粒度，不是真正 64B）:
- `idx_bits(8) + offset_bits(4) = 12 ≤ 12` → **形式上安全**（所有 idx bits 在 page offset 内）
- 但这只是 `uint_t<512>` 降级为 `uint64_t` 的副产品，不是有意设计
- **当升级到真正 64B cacheline 时**：`idx_bits(8) + offset_bits(6) = 14 > 12` → **不安全**
- 因此 Phase 1.5 实装 64B line 时**必须同步改 associativity 到 ≥ 4-way**

### 2.4 L2 Cache 始终 PIPT

- L2 延迟容忍（L1 miss 后再查 L2，TLB 已经完成）
- L2 容量 > 256KB 时 VIPT alias 不可避免
- 行业惯例一致（Rocket/BOOM/XiangShan/CVA6 L2 全部 PIPT）

### 2.5 Phase 1.5 L1Cache 64B line 升级路径

| 参数 | Phase 1.3 (当前) | Phase 1.5+ (VIPT 安全) |
|------|-------------------|------------------------|
| `kNumSets` | 256 | **64** |
| Associativity | 1-way (direct-map) | **4-way** (set-associative) |
| `kLineDataBits` | 512 (64-byte) | 512 (64-byte) |
| `kIdxBits` | 8 (addr[11:4]) | **6** (addr[11:6]) |
| `kOffsetBits` | 4→6 | **6** (addr[5:0]) |
| 总容量 | 16KB (256×1×64B) | 16KB (64×4×64B) |
| VIPT 安全 | 仅 Phase 0 简化下安全 | ✅ 正式安全 |

**注**：增加 associativity 也消除了 direct-mapped 的 conflict miss 问题，命中率提升。

**时序依赖**：Phase 1.5 L1Cache 升级依赖 `mmu-tlb-ptw-impl` 完成 MMUPlugin 的 vaddr+paddr 双写 Payload 能力（见 §3.2 数据流方案）。建议两个 change **同窗口并行开发**，但 L1Cache 侧 PR 在 mmu-tlb-ptw-impl 落地后 merge。

---

## 3. 与现有架构的关系

### 3.1 与 ADR-040（TLM→HDL 移植性约束）的关系

| 约束 | VIPT 影响 |
|------|----------|
| Tier-1 #5: tlm/ 无 ch_mem 渗透 | ✅ 无影响 — VIPT 影响地址解码逻辑，不影响存储后端 |
| Tier-2 #2: 位提取走 extract_idx/tag helper | ⚠️ `extract_idx/tag` 当前接受 **paddr**。Phase 1.5 引入 MMU 后需**分两个 helper**：`extract_idx_from_vaddr` (VIPT 索引) 和 `extract_tag_from_paddr` (PIPT tag 比对)。内部实现不变，调用点需区分 |

### 3.2 与 D4 Plugin 范式的关系

VIPT 不影响 Plugin 范式。`L1CachePlugin::build()` 仍通过 `at_stage()` 注册回调。Cache lookup 阶段的 vaddr→paddr 翻译由 `MMUPlugin` 在上一阶段完成，Cache 阶段读 `pl::PADDR` + 用 `extract_idx_from_vaddr` 索引。

**VIPT 数据流方案（方案 A：payload 双 Key）**：

MMUPlugin 在 TLB lookup 命中时**同时写 paddr 和保留 vaddr**，L1CachePlugin 在 lookup 阶段**分读两个 Payload Key**：

```
[IBusPlugin 写 pl::PC (vaddr)] →
   [MMUPlugin::tlb_lookup_ifetch] →
      读 pl::PC (vaddr)
      TLB lookup →
      hit: 写 pl::PADDR = paddr      ─┐
      hit: 写 pl::MMU_VADDR = vaddr  ─┤
                                       ↓
  [L1CachePlugin::lookup] →
      读 pl::MMU_VADDR → extract_idx_from_vaddr() → set index
      读 pl::PADDR     → extract_tag()             → tag compare
```

**接口契约**：
- `mmu_keys.h` 新增 `Payload<uint64_t> MMU_VADDR{"mmu.translated_vaddr"}`（与现有 `VADDR` Key 区别：VADDR 是 MMU 阶段**输入**，MMU_VADDR 是 MMU 阶段**输出**，供 downstream L1Cache 消费）
- `MMUPlugin::at_stage("tlb_lookup_ifetch", ...)` 在 write `K::PADDR` 同时 write `K::MMU_VADDR`
- `L1CachePlugin` 新增 `payload::cache_keys` Key 集合含 `CACHE_VADDR`（读取 mmu_keys 的 MMU_VADDR）

**备选方案**（不推荐，但保留记录）：
- 方案 B: `cache_keys.h` 加独立 `g_vaddr` Key，IBusPlugin 双写 PC + g_vaddr — 需要所有 fetch plugin 改，不如方案 A 集中在 MMUPlugin 一处
- 方案 C: `CacheReq` Bundle 加 vaddr 字段，Bridge 透传 — 破坏 Bridge 独立性（Bridge 不该感知 vaddr/paddr 区别）

### 3.3 与 L1CacheTLMBridge 的关系

当前 Bridge 接受 `CacheReq` (含 `address` 字段)。引入 MMU 后：
- `CacheReq.address` 语义不变（物理地址，因为 MMUPlugin 在前置阶段翻译）
- Bridge 不需要修改（PIPT 兼容 VIPT 的 paddr tag 接口）
- VIPT 只影响 **Plugin 内部的索引计算**（用 vaddr 索引，用 paddr tag），Bridge 层无感知

### 3.4 与 RISC-V 规范的关系

- SFENCE.VMA 刷新 TLB 后，**不需要刷新 VIPT L1 Cache**（因为 tag 是物理的，TLB 失效后旧 tag 与新的 paddr 不匹配 → 自然 miss）
- `fence.i` 需要同步 ICache 和 DCache，但这与 VIPT/PIPT 无关（是 I/D 一致性协议，Phase 4+ 范围）
- VIVT 才会在 SFENCE.VMA 后需要 flush cache（tag 是虚拟的），VIPT 无此问题

---

## 4. 替代方案评估

### 4.1 PIPT (被否决)

**优点**: 无 alias，实现简单，无需关心页大小。

**否决理由**:
- TLB hit 1 cycle **无法兑现**（Cache 须等 TLB 完成）
- Phase 1.3 当前 16KB/1-way 设计 PIPT 可工作，但**性能退化为 2-cycle hit**
- 与行业惯例背离（所有 RISC-V 高性能核都用 VIPT）

### 4.2 VIVT (被否决)

**优点**: 最快（无 TLB 延迟）。

**否决理由**:
- **Aliasing 严重**：同义地址（多个 vaddr → 同一 paddr）导致多份 cacheline
- **Context switch 必须 flush**：ASID 切换时每个 vaddr tag 需清空
- SFENCE.VMA 后必须 flush cache（tag 是虚拟的）
- 仅 NVIDIA GPU 使用（纹理 cache 天然无 alias 问题）
- 与 RISC-V 通用 CPU 惯例不符

### 4.3 保持 direct-mapped 1-way + 反别名硬件 (被否决)

**方案**: 256 sets × 1-way 不变，但加 speculative index + abort/retry（CVA6 HPDCache 64KB/2-way 模式）。

**否决理由**:
- 复杂度高（需要保存上一次翻译结果 + 比对 + 中止 + 重试）
- CVA6 使用是因为 64KB D$，ChipForge L1=16KB 远小于此
- 4-way associativity 本身也提升命中率（减少 conflict miss）
- 16KB 4-way VIPT 与大多数 RISC-V 核的 L1 D$ 规模一致

---

## 5. 后果

### 5.1 正面后果

- ✅ mmu-tlb-ptw-impl 启动时无阻塞（耦合决策已定）
- ✅ L0 TLB hit 1 cycle 性能目标成立（VIPT 并行查找）
- ✅ 行业惯例对齐（Rocket/BOOM/XiangShan 一致）
- ✅ L1 size 可安全扩展到 32KB/8-way 或 32KB/4-way（若用 2MB 大页，甚至 64KB/4-way）
- ✅ Phase 5 CppHDL 转换路径无额外复杂（idx_bits ≤ 6 的 VIPT 在 RTL 也是直接 wire 连接）

### 5.2 负面后果与缓解

| 负面后果 | 缓解 |
|----------|------|
| L1Cache 必须从 1-way → 4-way | associativity 提升带来命中率提升（conflict miss 减少），**净收益** |
| 替换策略复杂度增加 | Phase 1.4 已落地 `LRUPolicy`；4-way LRU 代价低（每 set 4 个 entry，< 10 个门电路） |
| `extract_idx` 语义需区分 vaddr/paddr | 新增 `extract_idx_from_vaddr` + `extract_idx_from_paddr` 两个 helper，Phase 1.5 迁移时不改调用方 |
| 后续扩容到 32KB+ 需重新评估 | 32KB/4-way = 8KB/way > 4KB，需加反别名硬件或 OS page coloring；**推迟到 Phase 2+** |

### 5.3 与其他 ADR 的关系

| ADR | 影响 |
|-----|------|
| **ADR-040** | Tier-2 #2 约束 `extract_idx/tag` helper 签名需扩展（见 §3.1） |
| **ADR-041** | L1CacheTLMBridge 无感知 VIPT/PIPT 切换 — Bridge 层兼容 |
| **ADR-042** | MMU 推迟已重启（mmu-ip-skeleton 落地），本 ADR 消除 MMU↔Cache 耦合盲区 |
| **ADR-037** | Plugin 范式不变；VIPT 是地址解码层决策 |

---

## 6. 实施检查清单

| # | 检查项 | Phase | 验证方式 |
|---|--------|-------|----------|
| 1 | `kIdxBits` 从 8 → 6，`kNumSets` 从 256 → 64 | Phase 1.5 L1Cache | 编译期 `static_assert(kIdxBits + kOffsetBits <= 12)` |
| 2 | Associativity 从 1 → 4 | Phase 1.5 L1Cache | `kNumSets × kWays × (64) = 16384` = 16KB |
| 3 | 新增 `extract_idx_from_vaddr(addr)` helper | Phase 1.5 L1Cache | 调用点替换 + 单元测试 |
| 4 | `extract_tag` 保持从 paddr 提取（无需改动） | Phase 1.5 L1Cache | 既有测试 PASS |
| 5 | `mmu-tlb-ptw-impl` 落地 MMU Plugin (vaddr→paddr) | Phase 1.5 | MMU hit 后写 `pl::PADDR`；Cache 并行读 vaddr idx + paddr tag |
| 6 | 确认 L2 Cache 走 PIPT | Phase 2+ | L2 接收 paddr，无 vaddr 索引 |

---

## 7. 验证命令

```bash
# 验证 L1CachePlugin 编译期 VIPT 安全条件
grep -q "static_assert.*kIdxBits.*kOffsetBits.*12" ip/cache/tlm/L1CachePlugin.h

# 验证 associativity ≥ 4
grep -q "kWays\|associativity.*4" ip/cache/tlm/L1CachePlugin.h

# 验证既有 13 个测试 PASS（Phase 1.3 baseline）
bash tools/run_chipforge_tests.sh -L cache | grep "100% tests passed"

# 验证 ADR-040 Tier-1 无新增违规
bash tools/check_plugin_portability.sh

# 验证 ADR 漂移检测通过（CI 强制，见 .github/workflows/architecture-gates.yml）
bash tools/verify_adr.sh

# 验证 ADR-044 已注册到主表
grep -q "ADR-044" docs/architecture/adr.md
```

---

## 8. 参考

- **RISC-V 核比较**: Rocket Chip (VIPT), BOOM (VIPT), XiangShan kunminghu-v3 (VIPT), CVA6 HPDCache (VIPT), Xuantie C910 (PIPT)
- **L1CachePlugin 当前参数**: `ip/cache/tlm/L1CachePlugin.h:72-76`
- **VIPT 安全条件**: [Hennessy & Patterson, Computer Architecture: A Quantitative Approach, 6th Ed., §2.3]
- **CVA6 HPDCache 反别名方案**: CVA6 HPDCache docs, see [openhwgroup/cva6](https://github.com/openhwgroup/cva6/blob/master/core/cache_subsystem/hpdcache_doc.rst)
- **mmu-ip-skeleton 推迟声明**: `openspec/changes/archive/2026-06-29-mmu-ip-skeleton/design.md §Non-Goals Line 47`
- **行业调研**: ChipForge mmu-tlb-ptw-impl 预备调研 (2026-07-01)，对比 Rocket/BOOM/XiangShan/CVA6/Xuantie C910
- **RISC-V Privileged Spec v1.12**: §4.3 Sv32, §4.4 Sv39, §4.5 Sv48, §12.2.1 SFENCE.VMA

---

*本 ADR 由 mmu-tlb-ptw-impl 预备调研驱动（2026-07-01）。Phase 1.5 L1Cache upgrade 时从 🚧 升级到 ✅，并同步更新 [`docs/architecture/adr.md`](../../../docs/architecture/adr.md) 主表。*

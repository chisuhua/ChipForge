# ADR-086-D02 — 级联 lookup 与 hit latency 分档

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构；实装未开始） |
| 父 ADR | 无；**增量对象** = [ADR-085](ADR-085-mmu-ptw-architecture.md) |
| 架构文档 | [architecture.md](../architecture.md) |
| 层级 | **IP 级** |
| 决策编号 | `D02` |
| 关联验证判据 | U-L1-01、U-L1-02、U-L1-03 |
| 受影响文件 | `ip/mmu/plugins/MMUPlugin_chmem.h` |

## 上下文

用户明确要求 **L0 miss 后再向 L1 TLB 发请求**（级联），而现行 `openspec/specs/mmu-multi-level-coherence/spec.md` 要求 **并行查所有层、取最深 hit**。两者相反，必须显式裁决并登记分叉。

## 决策正文

**决策**：CH_MEM 侧采用**串行级联** lookup：

```
拍 T    : L0 查表 → miss ⇒ 置 PTW_ACTIVE=1（ADR-085 D12）⇒ 向 L1 发请求
拍 T+1  : L1 查表 → hit ⇒ 产出 {paddr, perms}；miss ⇒ 交 PTW walk
拍 T+2  : （L1 命中路径）TLB entry 写回可见 ⇒ PADDR_VALID=1
```

**hit latency 分档（规范性）**：

| 路径 | 命中拍数 | 说明 |
|------|:--------:|------|
| L0 命中 | **1** | 组合比较，无级联开销 |
| L1 命中 | **2** | L0 判定 + L1 判定 |
| PTW 命中 | **4~6** | ADR-085 §4.2（sread 模型） |
| L0+L1 双 miss | ≥ 4 | 且 `PTW_ACTIVE` 全程保持 1 |

**理由（级联 vs 并行）**：

| | 级联（采纳） | 并行（spec 现行） |
|---|---|---|
| 比较器面积 | L0 与 L1 **各一套**，但**不同时** ⇒ 可共用一套多路选择器 | 两套**同拍**工作 ⇒ 面积 ×2 |
| L0 命中延迟 | 1 拍 | 1 拍 |
| L1 命中延迟 | 2 拍 | 1 拍 |
| hit rate 收益 | 高（大 L1 先兜住） | 同 |
| 时序简单性 | 单一 FSM，无"取最深"仲裁 | 需仲裁 + 取最深 + 一致性判定 |
| `PTW_ACTIVE` 口径 | miss 检出**同拍**为 1（L0 miss 拍）⇒ 天然满足 ADR-085 D12 与 protocol AC-FS-9 | 需额外保证"最浅 miss 拍即置位" |

⇒ **代价集中在 L1 命中路径（2 拍 vs 1 拍）**，用"少一套同拍比较器"换。**L0 要足够大**（8 项全相联）才能让绝大多数访问停在 1 拍。

**一致性语义不变**：spec 的 "reverse-invalidate shallow levels on deep evict"（L1 evict ⇒ 反向失效 L0）**原样复用**（见 D09）—— 本决策只改 lookup 的**调度方式**，不改 coherence 语义。

## 交叉引用

- 架构文档：[architecture.md](../architecture.md)
- 增量对象：[ADR-085](ADR-085-mmu-ptw-architecture.md)（D12 `PTW_ACTIVE` 派生式）
- 相关决策：[D01](ADR-086-D01-l0-l1-leaf-cache-split.md)、[D03](ADR-086-D03-l1-hit-backfill-l0.md)、[D09](ADR-086-D09-coherence-and-invalidation.md)、[D11](ADR-086-D11-relation-to-adr085-and-spec.md)
- 验证判据：见 [D12](ADR-086-D12-verification-criteria.md)

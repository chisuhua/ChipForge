# MMU 总架构 — L0 TLB / L1 TLB / PTW 三单元的数据流与外部对接

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（CH_MEM 前向架构）+ 🟡 TLM 侧已落地部分 |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 决策组 | [ADR-085](adr/ADR-085-mmu-ptw-architecture.md)（MMU_PTW，21 条子决策） + [ADR-086](adr/ADR-086-l0-l1-tlb-architecture.md)（L0/L1 TLB，12 条子决策）|
| 平台前提 | **Sv39**（3 级页表：VPN[2]/VPN[1]/VPN[0] 各 9 bit + 12 bit 页内偏移）⇒ **需 64-bit XLEN**，见 §4 |

> **本文档 = 总架构**：只讲**跨 L0 / L1 / PTW 的数据流**、**三单元的职责边界**、**与外部的接口契约**。
> **各单元内部结构**在子文档里（见 §0）；**决策**在 2 个 ADR 决策组里（33 条子 ADR）。

---

## 0. 阅读路径

| 想知道 | 去哪 |
|--------|------|
| MMU 总体怎么跑、数据流怎么走 | **本文档 §1–§3** |
| L0 TLB 内部结构（8 项全相联、多粒度、比较器） | [mmu-l0-tlb-architecture.md](mmu-l0-tlb-architecture.md) |
| L1 TLB 内部结构（256 set × 8 way、组相连、XOR interleave、容量） | [mmu-l1-tlb-architecture.md](mmu-l1-tlb-architecture.md) |
| PTW walk FSM / 目录项缓存 / 编码判据 | [mmu-ptw-architecture.md](mmu-ptw-architecture.md)（ADR-085） |
| TLM 侧（MultiLevelTLB 编排、lib/tlm 分层、HDL 友好性） | [mmu-tlm-architecture.md](mmu-tlm-architecture.md) |
| 某条决策的完整论证与替代方案 | `adr/ADR-085-D01..D21` / `adr/ADR-086-D01..D12` |

---

## 1. 三单元与职责边界

```
                     ┌────────────────────────────────────────────────┐
   vaddr ──────────►│  L0 TLB  ·  8 项全相联 · 1 拍 · 多粒度         │──hit──► paddr
                     │  热点页快路径（全 VPN 直比 / 掩码比，见子文档）   │
                     └───────────────┬────────────────────────────────┘
                                     │ miss（同拍 PTW_ACTIVE=1）
                                     ▼
                     ┌────────────────────────────────────────────────┐
                     │  L1 TLB  ·  256 set × 8 way 组相连 · 2 拍     │──hit──► paddr
                     │  容量扩展 + 大页翻译（per-way page_shift 掩码）   │  （并回填 L0）
                     │  XOR interleave（基准 16MiB）                  │
                     └───────────────┬────────────────────────────────┘
                                     │ miss
                                     ▼
                     ┌────────────────────────────────────────────────┐
                     │  PTW  ·  多周期 walk FSM（ADR-085）            │──► paddr
                     │  编码/权限判据（D13）+ superpage 拼接（D14）      │    （按粒度回填 L0/L1）
                     │  可选 PTWCache：缓存**页目录项**（非 leaf）       │
                     └───────────────┬────────────────────────────────┘
                                     │ 读物理内存（绕过翻译）
                                     ▼
                          物理内存（DMemPlugin 的 ch_mem）
```

| 单元 | 缓存/产出什么 | 组织 | 命中延迟 | 详细文档 |
|------|--------------|------|:--------:|---------|
| **L0 TLB** | **leaf 翻译**（`{paddr, perms, page_shift}`）| 8 项**全相联** | 1 拍 | [L0 文档](mmu-l0-tlb-architecture.md) |
| **L1 TLB** | **leaf 翻译**（多粒度）| 256 set × 8 way **组相连** | 2 拍 | [L1 文档](mmu-l1-tlb-architecture.md) |
| **PTW** | walk 结果（leaf 翻译）+ 可选**页目录项**缓存 | 5 态 FSM（ADR-085 D09）| 4~6 拍 | [PTW 文档](mmu-ptw-architecture.md) |

**职责边界（规范性）**：

- **L0 与 L1 存同一种东西**（leaf 翻译）⇒ 差别只在**容量 / 组织 / 延迟** ⇒ L0 miss ⇒ 向 L1 发请求（D02 级联）。
- **PTWCache 存的是页目录项**，与 L0/L1 **不重叠**（ADR-085 D04）⇒ 三个缓存各自解决不同问题：L0 省**延迟**、L1 省**延迟 + 支持大页**、PTWCache 省**访存次数**。
- **`PADDR` 只能由 TLB 查表产生**（ADR-085 D15）⇒ PTW 不直接写 `PADDR`，只回填 TLB entry。
- **PTW 读物理内存时绕过翻译**（ADR-085 D16）⇒ 防递归。

---

## 2. 跨 L0 / L1 / PTW 的数据流与时序

### 2.1 路径 A — L0 命中（1 拍）

| 拍 | 动作 | `PTW_ACTIVE` | `hold` |
|----|------|:-------------:|:-----:|
| T | L0 8 路比较 ⇒ 命中 ⇒ `paddr = (pfn << page_shift) \| (vaddr & mask)` ⇒ 写 `PADDR` / `PADDR_VALID=1` | 0 | 0 |

### 2.2 路径 B — L0 miss → L1 命中（2 拍）

| 拍 | 动作 | `PTW_ACTIVE` | `hold` |
|----|------|:-------------:|:-----:|
| T | L0 miss ⇒ **`PTW_ACTIVE=1`（同拍，protocol AC-FS-9）** ⇒ 向 L1 发请求 | **1** | 1 |
| T+1 | L1 组相连比较（per-way 掩码）⇒ 命中 ⇒ 产出 `paddr`（大页拼接）⇒ 按粒度回填 L0 | 1 | 1 |
| T+2 | `PADDR_VALID=1`，`PTW_ACTIVE=0` ⇒ 释放 | 0 | 0 |

### 2.3 路径 C — 双 miss → PTW（4~6 拍）

| 拍 | 动作 |
|----|------|
| T | L0 miss ⇒ `PTW_ACTIVE=1` ⇒ L1 查询 |
| T+1 | L1 miss ⇒ 交 PTW walk（ADR-085 §4.2 的 `REQ`/`WAIT`/`DONE` 序列） |
| T+2 ~ T+5 | walk 进行中（`PTW_ACTIVE` 全程 1） |
| T+6 | `DONE` ⇒ 结果**按粒度**回填（4KiB → L0；2MiB → L1）⇒ `PADDR_VALID=1` |

### 2.4 跨级回填与失效

| 事件 | 动作 | 决策 |
|------|------|------|
| L1 命中 | 回填 L0（多粒度 L0 ⇒ 大页 entry 也能回填） | ADR-086 D03 |
| PTW 成功 | 按粒度回填 L0 或 L1 | ADR-086 D09 |
| L1 evict | **反向失效** L0 同 `(vaddr, asid)` 副本 | ADR-086 D09 |
| `satp` 变更 | **两级 flush-all**（`satp_changed` 影子寄存器比较） | ADR-085 D20 + ADR-086 D09 |
| `sfence.vma` | 精确失效（推迟到 CSR 批次） | ADR-086 D09 |

### 2.5 hit latency 分档

| 路径 | 延迟 | 占比影响 |
|------|:----:|---------|
| L0 命中 | **1 拍** | 绝大多数访问（热点页） |
| L1 命中 | **2 拍** | 中等命中率（容量扩展 / 大页） |
| PTW 命中 | **4~6 拍** | 冷启动 |

⇒ **L0 够大**是这套结构的关键：L0 每多 1 项 ⇒ 1 拍路径的命中率线性提升；L1 命中虽只多 1 拍，但**不回填 L0 就永远 2 拍**（ADR-086 D03）。

---

## 3. 外部对接

### 3.1 CSR `satp`（Sv39 编码）

| 字段 | 位域 | 说明 |
|------|------|------|
| `MODE` | `satp[63:60]` | `0000` = Bare；`1000` = Sv39 |
| `ASID` | `satp[59:44]` | 16 bit（Sv39）；本期位宽预留 |
| `PPN` | `satp[43:0]` | 根页表基址 PPN（44 bit） |

⇒ **`pt_base` 不设独立寄存器**，取 `satp` 的组合派生视图（ADR-085 D08）⇒ 避免"模式改了但根表地址没改"的静默错误翻译。
⇒ `satp` 变更 ⇒ 两级 TLB flush-all（§2.4）。

### 3.2 fetch-stall protocol（取指冻结）

| 契约 | 内容 | 归属 |
|------|------|------|
| `PTW_ACTIVE` | **组合派生** = `sv_active && !paddr_valid`（不是 FSM busy 状态） | ADR-085 D12 |
| miss 检出拍 | `PTW_ACTIVE` **同拍**为 1（AC-FS-9）⇒ **L0 miss 即置位** | ADR-086 D02 |
| 窗口 | protocol **AC-FS-15 ②**（按取指事实定义，含 FAULT 拍） | ADR-085 D12 |
| 结构反回归 | protocol **AC-FS-16 ①**（`PTW_ACTIVE` 写语句的 RHS token 白名单 W1） | ADR-085 D12 |

⚠ **协议必须升 rev.15**（ADR-085 D13）：AC-FS-15 ② 当前把 `V=1 && R=1 && W=1` 钉为"权威坏 PTE"并注"以代码为准"⇒ **protocol 正在把 bug 变成验收判据**。

### 3.3 物理内存（PTW 的读端口）

- PTW 读的是**物理地址** ⇒ 读端口**绕过 MMU 翻译**（防递归，ADR-085 D16）。
- CH_MEM 侧需要 `DMemPlugin` 暴露 `ch_mem& mem()`（当前 `mem_` 是 private 且惰性创建，MMU 的 `("fetch", EARLY)` 拿不到句柄）。
- **所有权**：归 `DMemPlugin`（创建者）；MMU 只持**非拥有引用**，句柄在 `setup()` 阶段取（ADR-085 D21）。

### 3.4 TLB ↔ PTW 接口

| 方向 | 字段 |
|------|------|
| 请求（TLB → PTW）| `vaddr`(XLEN) / `asid` / `access_type`(2b: load/store/ifetch) |
| 响应（PTW → TLB）| `paddr`(XLEN) / `perms`(4b) / `fault_code`(8b: 0 / 12 / 13 / 15) |

⇒ 同拍发起、无握手延迟；PTW 忙时请求**丢弃**（不排队），下拍重试（ADR-086 D01 / ADR-085 D11）。

### 3.5 SoC JSON 配置

| 配置项 | 默认 | 说明 |
|--------|------|------|
| `params.asid_bits` | 9 | Sv39 的 ASID 位宽（本期只预留） |
| `params.sv_mode` | `Sv39` | 目标模式（见 §4） |
| `params.supported_page_sizes` | `[4096, 2097152]` | 4KiB + 2MiB（**本期不含 1GiB**） |
| `params.l1.sets` / `.ways` / `.epl` | 16 / 4 / 8 | L1 组织（本期 P0；目标 256/8/8） |
| `params.l0.entries` | 8 | L0 全相联项数 |
| `params.levels[]` | 保留 | **TLM 侧** MultiLevelTLB 的层级配置（CH_MEM 侧不消费） |

### 3.6 与其它 IP 的边界

| 外部 | 边界 |
|------|------|
| **L1Cache** | 消费 `PADDR` / `PADDR_VALID`；MMU 不感知 cache（VIPT 安全条件见 ADR-044） |
| **CPU（fetch）** | `IBusPlugin` 的 `pc_reg` ⇒ `("fetch", EARLY)` 闭包 ⇒ MMU；MMU ⇒ `PADDR` / `PTW_ACTIVE` |
| **CPU（LSU）** | `("loadstore", NORMAL)` 闭包 ⇒ MMU 的 loadstore 端口 |
| **Trap** | `EXCEPTION_CODE` cell 的消费侧**推迟**到 trap 批次（ADR-085 D19.1 的 L2） |

---

## 4. 平台前提 —— Sv39 ⇒ **需 64-bit XLEN**（⚠ 当前全仓是 32-bit）

**目标模式**：Sv39，39 位有效虚拟地址 = `VPN[2]`(9) + `VPN[1]`(9) + `VPN[0]`(9) + 页内偏移(12)

| 位域 | 宽度 | 含义 |
|------|-----:|------|
| `vaddr[38:30]` | 9 | **顶级**页目录索引（`VPN[2]`，根表） |
| `vaddr[29:21]` | 9 | **中间**页目录索引（`VPN[1]`） |
| `vaddr[20:12]` | 9 | **页表**索引（`VPN[0]`，叶表） |
| `vaddr[11:0]` | 12 | 页内偏移（4 KiB 页） |

**页大小**（Sv39）：

| 粒度 | 叶 PTE 所在级 | `page_shift` | VA 跨度 |
|------|---------------|-------------:|--------:|
| **4 KiB** | `VPN[0]`（叶表） | 12 | `vaddr[11:0]` |
| **2 MiB** | `VPN[1]`（中间表） | 21 | `vaddr[20:0]` |
| 1 GiB | `VPN[2]`（根表） | 30 | `vaddr[29:0]` —— **本期不考虑** |

**⚠ 前置条件（当前不满足）**：Sv39 定义在 **RV64** 上；**全仓当前是 32-bit**：

| 位置 | 现状 |
|------|------|
| `ip/mmu/plugins/MMUPlugin_chmem.h:81` | `kXlenBits = 32` |
| `ip/cpu/plugins/mmu_ptw_chmem.h:151` | `kXlenBits = 32` |
| `ip/cpu/plugins/dmem_chmem.h:34` / `ibus_chmem.h:36` | `kXlenBits = 32` |
| `ip/cpu/cpu_factory_chmem.h:63` | `kXlenBits = 32`（注释 `// RV32 PoC`） |

⇒ **平台升级到 RV64 是本架构的前置条件**，影响面：地址位宽（`kXlenBits` 5 处）、`satp` 编码（Sv39 64-bit 布局）、PTW 的 3 级 VPN 提取、`paddr` 宽度（Sv39 PPN 44 bit）、`TLBEntry` 的 `TAG_BITS`（27）/ `pfn`（44）。
⇒ 该升级**不属本组范围**，登记为 §7 的 **R1**（最高等级）。

---

## 5. 与既有架构的关系

| 既有物 | 关系 |
|--------|------|
| **ADR-085**（MMU_PTW 架构） | 本组的上游决策组（21 条子 ADR）—— 定义 PTW 侧；**不回改**，ADR-086 的 delta 见 [D11](adr/ADR-086-D11-relation-to-adr085-and-spec.md) |
| **ADR-086**（L0/L1 TLB 架构） | 本组定义 L0/L1（12 条子 ADR）—— **只做 CH_MEM 侧** |
| **TLM 侧 `MultiLevelTLB`** | ⚠ **语义分叉**：TLM = **并行**查所有层 + 取最深 hit（`openspec/specs/mmu-multi-level-coherence/spec.md` 的 MUST 条款）；CH_MEM = **串行级联**。spec 约束的是 TLM 侧 C++ 类 ⇒ **不是违约，是后端分叉**。CH_MEM 侧成员名用 `l0_tlb_` / `l1_tlb_`，**不得**复用 `MultiLevelTLB` |
| **`ip/mmu/docs/architecture.md`（本文件）** | 2026-10-23 重组：原 TLM 侧内容外移到 [mmu-tlm-architecture.md](mmu-tlm-architecture.md) |
| **ADR-040 v2.0** | §10.5 payload key 必须 mode-aware；`array_store` 双缓冲 |
| **ADR-046** | 多周期 FSM 豁免（PTW walk） |
| **ADR-083** | Plugin-style SSOT ⇒ CH_MEM 新文件落 `ip/mmu/plugins/`（不放 `tlm/`） |
| **ADR-049** | MMU PADDR 契约 + MemoryInterface —— D15「`PADDR` 只由查表产生」是其在 CH_MEM 侧的落地 |
| **ADR-076**（规划中） | ASID vs 全局 TLB 冲刷 —— 与 `satp` flush-all 的粗粒度限制主题相邻 |

---

## 6. 决策路由索引

| 决策组 | 编号 | 主题 | 归属文档 |
|--------|------|------|---------|
| [ADR-085](adr/ADR-085-mmu-ptw-architecture.md) | D01–D21 | PTW walk FSM / 目录项缓存 / 编码判据 / superpage / 失效 / 验证 | [mmu-ptw-architecture.md](mmu-ptw-architecture.md) |
| [ADR-086](adr/ADR-086-l0-l1-tlb-architecture.md) | D01 | 两级 leaf 缓存的职责划分 | 本文档 §1 |
| | D02 | 级联 lookup 与 latency 分档 | 本文档 §2 |
| | D03 | L1 命中回填 L0 | 本文档 §2.4 |
| | D04–D07 | L1 组织参数 / set index / 粒度与掩码比较 / entry 字段 | [L1 文档](mmu-l1-tlb-architecture.md) |
| | D08 | L0 组织 | [L0 文档](mmu-l0-tlb-architecture.md) |
| | D09 | 跨级一致性与失效 | 本文档 §2.4 |
| | D10 | 容量与节点预算 | [L1 文档](mmu-l1-tlb-architecture.md) |
| | D11 | 与 ADR-085 / openspec spec 的关系 | 本文档 §5 |
| | D12 | 验证判据 | [L0](mmu-l0-tlb-architecture.md) + [L1](mmu-l1-tlb-architecture.md) |

---

## 7. 风险登记（跨单元）

| ID | 风险 | 等级 | 缓解 |
|----|------|:----:|------|
| **R1** | **平台前提不满足**：Sv39 需 64-bit XLEN，而全仓是 32-bit（5 处 `kXlenBits = 32`）⇒ 架构实现前必须先做 RV64 升级 | **高** | 登记为**前置条件**；本组不擅自扩到 RV64；实现 change 必须先有 RV64 基础 |
| **R2** | **`PTW_ACTIVE` 置位点前移**：从"TLB miss"变成"L0 miss" ⇒ protocol 的窗口定义需重述（虽判据口径不变） | 中 | ADR-086 D02 已定义分档；protocol 升 rev.15 时一并核对 AC-FS-15 ② |
| **R3** | **三个缓存的一致性面**（L0 / L1 / PTWCache）⇒ 逐出/失效遗漏 ⇒ 陈旧命中 | 中 | §2.4 的 4 条路径 + `U-L1-07/08` 判据 + 变异对照 ⑤/⑥ |
| **R4** | **L1 命中比 L0 慢 1 拍** ⇒ 若不回填 L0，L1 容量收益被延迟抵消 | 中 | ADR-086 D03（回填）+ §2.5 的分档 |
| **R5** | **PTW 侧尚未实装**（`PTW_ACTIVE` / `EXCEPTION_CODE` 恒 0）⇒ 双 miss 路径难端到端验证 | 中 | 先做 L0/L1 命中路径（桩驱动 PTE），PTW 随 ADR-085 的 change 落地 |
| **R6** | **命名冲突**：`L0`/`L1` 在 TLM 侧是"深度层"、在 ADR-086 是"leaf 缓存级" | 低 | 本文档 §0 的路径表 + [TLM 文档](mmu-tlm-architecture.md) 顶部的警示 + CH_MEM 侧成员名 `l0_tlb_`/`l1_tlb_` |
| **R7** | **L1 节点数**（目标 256×8 ⇒ ~1.6~4.9 万 `ch_reg`）⇒ CH_MEM 全量测试时长上升（基线 833 s/轮） | 中 | ADR-086 D10：本期 P0 = 16×4×8 = 512 entry；entry 打包成 1 个 `ch_uint<64>`（节点 ÷3） |

---

## 8. 验证命令

```bash
# 架构门禁（5 个）
bash tools/verify_adr.sh
bash tools/verify_plugin_decision.sh
bash tools/check_plugin_portability.sh
bash tools/doc_link_check.sh
bash tools/roadmap-r9-check.sh

# 回归
./build/bin/chipforge_tests "[mmu]"                            # TLM
./build/bin/chipforge_tests_chmem "[mmu-chmem-basic]"          # CH_MEM
bash tools/run_chipforge_tests.sh --all                        # 全量（CH_MEM ~833s/轮）
```

各单元的结构性检查命令见 [L0 文档](mmu-l0-tlb-architecture.md) / [L1 文档](mmu-l1-tlb-architecture.md) / [PTW 文档](mmu-ptw-architecture.md) 的验证命令段。

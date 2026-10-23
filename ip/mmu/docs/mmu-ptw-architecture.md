# MMU_PTW 架构设计 — TLB/PTW 分层、PTWCache（页目录项缓存）与多级 walk

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`） |
| 决策组 | [ADR-085](adr/ADR-085-mmu-ptw-architecture.md) —— 伞索引，登记 **D01–D21** 共 21 条子 ADR（一决策一文件） |
| 层级 | **IP 级**（随 `ip/mmu/` 存亡） |
| 适用范围 | **CH_MEM 前向架构**（TLM 侧本期不动，除 D13 的共享 helper 抽取与 D14 的同判据对齐） |
| 归属 change | `chmem-ptw-walk`（**未立项**；范围增量见 §9） |

> **本文档 = 设计正文**：背景与动机、职责边界、数据流与时序、替代方案评估、后果、实施清单、验证命令。
> **决策本身 = 21 个子 ADR**：每个决策一个文件，含上下文 / 决策正文 / 交叉引用；§3 是它们的索引。
>
> **引用约定**：`§0`–`§11` 指**本文档**的章节（`§3` 为决策索引）；`§D7.x` / `§D14.1` 指**对应子 ADR 内**的子节；**其它文档**的章节号保持原样并自带文档名（RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。


## 0. 待确认项（默认决策已给出，用户可否决）

| # | 议题 | 本决策组的默认决策 | 影响面 |
|---|------|------------------|--------|
| **Q1** | PTWCache 是否也缓存**叶 PTE**？ | **否**（只存非叶目录项） | 若改为"是"，walk 可在 PTWCache 全命中时零访存完成，但 PTWCache 与 TLB 职责重叠、污染策略复杂 |
| **Q2** | "64B cacheline ⇒ 8 页"的确切含义？ | **8 个 PTE**（相邻索引的目录项），**不是** 8 个 4KB 页；且 entry 宽度固定 8B（sv32 用低 4B）⇒ sv39/48 天然 8 项/line，sv32 若按 4B 实体则 16 项/line | line 格式与 Sv mode 的耦合 |
| **Q3** | CH_MEM 模式下 PTWCache 的默认规模？ | **极小**（默认 1 set × 1 way × 4 entries = 4 entries），容量作为配置参数 | CH_MEM elaboration 节点数与全量测试时长（现 833s/轮） |

---

## 1. 背景与动机

### 1.1 现状事实

| # | 事实 | 证据 |
|---|------|------|
| F1 | `ip/mmu/` 现有 **8 项全相联 TLB**（每项 3 个 `ch_reg`：valid / vaddr / paddr）+ `satp_` 寄存器，**组合查表**，命中即出 `PADDR` | `ip/mmu/plugins/MMUPlugin_chmem.h:166-283` |
| F2 | **无任何 PTW walk 实装**：`PTW_ACTIVE` / `EXCEPTION_CODE` 恒写常量 0，TLB miss ⇒ `PADDR_VALID=0` ⇒ 消费端回退 vaddr | `MMUPlugin_chmem.h:281-282`（文件头 `:17-26` 显式列为 defer） |
| F3 | 存在一个 **CH_MEM sv32 5 态 PTW FSM PoC**（`PtWalkFsmPlugin`），但：① 输入在 `create_fsm()` 内创建并立即拉边捕获 ⇒ **不可被表达式驱动**；② **未被任何生产路径注册**；③ 位于 `ip/cpu/plugins/`（MMU 组件放在 CPU 目录，违反 ADR-083 §2.4） | `ip/cpu/plugins/mmu_ptw_chmem.h:226-243`。**"未注册"的准确口径**：`grep -rn PtWalkFsmPlugin ip/cpu/cpu_factory_chmem.h` = **0 命中**；全仓非注释命中仅 3 类 —— 定义处（`mmu_ptw_chmem.h:140/150/156-159`）、`MMUPlugin_chmem.h:19` 的**注释**、`tests/cpu/test_mmu_ptw_fsm_chmem.cpp:56/61` 的**测试直接实例化** |
| F4 | TLM 侧有纯 C++ PTW（`lib/ptw.h`），由 MMUPlugin 在 **3 个 logical stage** 各推进 1 级 | `ip/mmu/lib/ptw.cpp:126-134`；`ip/mmu/tlm/MMUPlugin.cpp:163-165` |
| F5 | `ip/mmu/lib/tlb_entry.h` 的 `TLBEntry` **已有正确的字段集**：`valid / tag / pfn / asid / perms / global` | `ip/mmu/lib/tlb_entry.h:49-61` |
| F6 | `ip/mmu/policies/` 已有 4 种替换策略（None / FIFO / LRU / RRIP），但是**纯 C++ 虚函数**（运行期调用） | `ip/mmu/policies/tlb_replacement_policy.h` |
| F7 | CH_MEM 数据通路**全组合**、stage 间**无流水寄存器**；但**单插件内部有 `ch_reg` 状态**（`pc_reg` / `satp_` / TLB entry / `mcycle_` …） | `cpu_factory_chmem.h:438-442`；`ibus_chmem.h:79-81` |
| F8 | **纯组合环被上游禁止**，只有 `type_reg` 允许成环 ⇒ 多周期行为只能经 `ch_reg` 反馈表达 | 上游 `src/core/context.cpp:294-296` |
| F9 | `ch_mem::aread` = 0-cycle（Simulator 与 Verilog 已对齐，上游 `c27543d`）；`ch_mem::sread` = 1-cycle；**寄存器驱动地址已上游实测** | 上游 `core/mem.h:125/153`；**本地可复核** `/workspace/project/CppHDL/tests/test_mem_comb_read.cpp:281-290`（symlink 仓库在仓内） |
| F10 | **CH_MEM 里零个生产级多周期 FSM**：3 个 CH_MEM FSM（PTW / L1 refill / mul_div）**全部未被注册** | `grep -rn "l1_cache_refill_fsm_chmem\|mul_div_fsm_chmem" ip/cpu/cpu_factory_chmem.h` = 0 |
| F11 | `ip/cpu/plugins/ibus_chmem.h:82-84` 的注释写「`aread`/`sread` both have 1-cycle latency」——**与 F9 矛盾**（`aread` 是 0-cycle）⇒ 既有注释债，引用该文件时不得据此建模 | `sed -n '80,86p' ip/cpu/plugins/ibus_chmem.h` |

### 1.2 问题的本质

现状的"MMU"是 **TLB-only**：命中就翻译，miss 就放弃（回退 vaddr）。这不是"还没做完的 walk"，而是**缺一整层架构**：

1. **翻译缺失**（功能）：sv32 模式下 TLB miss ⇒ 静默用 vaddr 取指/访存 ⇒ **"测试全绿但翻译从未生效"**（项目已多次踩坑的 G3 惰性骨架）。
2. **walk 无处安放**（结构）：walk 是**多周期串行**操作，而 CH_MEM 的 CPU 数据通路是**全组合**的 ⇒ walk 必须以 `ch_reg` 反馈的方式存在 ⇒ 这在 CH_MEM 里是**零生产实例的新形态**（F10）。
3. **访存层次缺失**（性能/健壮性）：业界所有 RISC-V 核的 MMU 都是 **TLB（叶 PTE 缓存）+ PTW（页表遍历引擎）** 两级，且 PTW 内部通常再挂一个**页表缓存（PT cache）**缓存遍历中间结果。没有这一层，每次 miss 都要完整走 2~3 次内存访问。

### 1.3 目标架构（用户提案 + 本决策组的补全）

用户提出的骨架：

```
MMU
├── MMU_TLB
│   └── TLBCache          — cacheline 存 PTE(leaf)
└── MMU_PTW
    ├── PTWCache          — cacheline 存页目录项（可配置项数）
    └── pt_base (寄存器)  — 第一级目录表 PA
```

本决策补全的部分见 §3 的 [D1](adr/ADR-085-D01-mmu-tlb-ptw-split.md)–[D21](adr/ADR-085-D21-physical-mem-handle-ownership.md)，其中**对用户提案做了 3 处修正**（均给出替代方案与理由）：

| # | 用户提案 | 本决策组的修正 | 原因 |
|---|---------|--------------|------|
| **M1** | "pt_base 存放第一级目录表 PA 的**寄存器**" | **`pt_base` 不设独立寄存器**，取 `satp` 的组合派生视图 | 两个真值源 ⇒ 漂移。项目已有前例：v0.10.4 hotfix `a3afcfa` 修的正是 `satp_ppn_` / `satp_value_` 双字段语义漂移 |
| **M2** | "多级页目录**同时**进行 hit/miss test" | 跨 level 的**地址**无法同时计算（串行依赖，见 [D7](adr/ADR-085-D07-start-level-selection.md) §D7.1 的证明）⇒ 可实现的形式是 **同一拍内串联多级 test**（`test(k)` 命中 ⇒ 下一级地址当拍可算）+ **`start_level`** 决定剩余访存次数；line 内 K 路 tag 比较天然并行 | 否则会把一个物理上不可能的设计写进 spec，实现者必然做错。**另注**：sv32 下连续命中前缀最长 1 级 ⇒ 该机制**并行度为 0**、只省 1 次访存（[D7](adr/ADR-085-D07-start-level-selection.md) §D7.4） |
| **M3** | "PTWCache 存放页目录项" | 保留该表述，但**明确排除叶 PTE**，并给出理由与替代方案（[D4](adr/ADR-085-D04-ptwcache-no-leaf-pte.md)） | 叶 PTE 若入 PTWCache，与 MMU_TLB 职责重叠；且叶 PTE 访问频率 ≫ 目录项，混存会挤掉目录项 |

---

## 2. 术语与职责边界

| 术语 | 定义 | 存放内容 | 命中延迟 |
|------|------|---------|---------|
| **TLBCache** | MMU_TLB 的存储体。cacheline 存**叶 PTE** | `{valid, asid, tag(=VPN), pfn(=PPN), perms(R/W/X/U), global}` | **组合**（0 拍命中） |
| **PTWCache** | MMU_PTW 的存储体。cacheline 存**非叶目录项（PDE）** | `{valid, asid, level, parent_ppn(=父表 PPN), next_ppn(=子表 PPN), perms}` | **组合**（0 拍命中） |
| **pt_base** | 根页表的**物理基址**。取 `satp` 的组合派生 | 非独立寄存器 | — |
| **walk** | 由 PTWCache + 物理内存读端口 + walk FSM 组成的翻译缺失补偿流程 | — | **多周期**（sv32 ≈ 4~6 拍） |
| **start_level** | walk 中**第一次需要发物理读**的那一级 = 连续命中前缀之后的那一级；**派生量，不是状态**。剩余访存次数 = `start_level + 1`（sv32 ∈ {1,2}） | — | — |
| **entry / EPL** | cache entry / entries-per-line（每条 cacheline 的 entry 数） | — | — |

### 2.1 三层职责边界（规范性）

| 组件 | **做** | **不做** |
|------|------|---------|
| **MMU_TLB** | ① 组合查表（2 端口：ifetch / loadstore）② 命中 ⇒ 出 `PADDR` / `PADDR_VALID` / `MMU_VADDR` ③ miss ⇒ **组合派生** `PTW_ACTIVE=1` 并向 MMU_PTW 发起 walk 请求 ④ 接收 walk 结果 ⇒ 回填 TLB | ① 不含任何多周期逻辑 ② 不直接访问物理内存 ③ **不把 walk 结果直接写进 `PADDR`**（`PADDR` 只能来自查表，见 [D15](adr/ADR-085-D15-walk-refill-tlb-only.md)） |
| **MMU_PTW** | ① 接收 walk 请求 ② PTWCache 同拍串联 hit test（得出 `start_level`） ③ 从 `start_level` 起逐级读**物理**内存 ④ 逐级编码/保留位检查（[D13](adr/ADR-085-D13-pte-encoding-and-permission.md)） ⑤ 产出 `{paddr, perms}` 或 `{fault_code}` ⑥ 把读到的**目录项**回填 PTWCache | ① 不做最终翻译（最终翻译是 TLB 的事）② **不缓存叶 PTE**（[D4](adr/ADR-085-D04-ptwcache-no-leaf-pte.md)）③ 不参与 fetch stall 的消费侧（那是 protocol 的 `ibus_chmem.h` 改动）④ 不持有物理内存所有权（[D21](adr/ADR-085-D21-physical-mem-handle-ownership.md)） |
| **MMU_TLB ↔ MMU_PTW 接口** | 请求 `{vaddr, asid, access_type}` / 响应 `{paddr, perms}` 或 `{fault_code}` | — |

---

## 3. 决策索引（D01–D21）

本决策组的 **21 条决策各自独立成文件**（一个决策一个文件，位于 [`adr/`](adr/) 目录，编号 `ADR-085-D01` … `ADR-085-D21`）。下表是索引；每条决策的**上下文 / 决策正文 / 交叉引用**见对应文件。

| 编号 | 决策标题 | 一句话要点 | 关联 U 判据 |
|------|---------|-----------|-----------|
| [D01](adr/ADR-085-D01-mmu-tlb-ptw-split.md) | MMU 分解为 MMU_TLB + MMU_PTW（两个协作单元，同一 MMU 插件内） | MMU 拆成 MMU_TLB（组合查表）+ MMU_PTW（多周期 walk），对外仍是**一个** MMU 插件 | U1 |
| [D02](adr/ADR-085-D02-tlb-entry-layout.md) | TLBCache 的 entry 与 cacheline 组织 | TLB entry = `{valid,asid,tag,pfn,perms,page_shift,global}`；cacheline 存 EPL 个 entry，EPL 路 tag 比较并行 | U1 |
| [D03](adr/ADR-085-D03-tlb-dual-port-arbitration.md) | MMU_TLB 的 2 端口与"同拍双 miss" | 两个端口可同拍 miss；walk 请求由 PTW 仲裁（ifetch > loadstore），低优先级**丢弃不排队**、下拍重试 | U1 |
| [D04](adr/ADR-085-D04-ptwcache-no-leaf-pte.md) | PTWCache 只缓存**非叶**目录项（PDE），不缓存叶 PTE | PTWCache 只存**非叶目录项**，不存叶 PTE ⇒ 与 TLB 职责不重叠；代价是 walk 最少 1 次访存 | U2 |
| [D05](adr/ADR-085-D05-ptwcache-entry-width-epl.md) | PTWCache entry 宽度固定 8B；cacheline size 与 EPL 可配 | entry 宽度**固定 8B**（与 Sv mode 解耦）；cacheline 可配，64B ⇒ 8 entries/line；tag = `{level,parent_ppn,asid}` | U2 |
| [D06](adr/ADR-085-D06-ptwcache-index-replace-policy.md) | PTWCache 的 index / set 映射与替换策略 | index = `parent_ppn` 低位；CH_MEM 用**直接映射**（1 way）规避组合替换逻辑；策略语义放 `lib/` | U2 |
| [D07](adr/ADR-085-D07-start-level-selection.md) | PTWCache 命中 ⇒ `start_level` 选择（"从哪一级开始 walk"） | 同拍串联 hit test 得出 `start_level` = 连续命中前缀之后那一级 ⇒ 剩余访存 = `start_level+1` | U3 |
| [D08](adr/ADR-085-D08-pt-base-derived-from-satp.md) | `pt_base` 不设独立寄存器，取 `satp` 的组合派生视图（修正 M1） | **不设独立 `pt_base` 寄存器**，取 `satp` 的组合派生视图（按 mode 写具体位域，不写通式） | U10 |
| [D09](adr/ADR-085-D09-walk-fsm-state-set.md) | walk FSM 的形态与状态集 | walk FSM = 固定 5 态 `{IDLE,REQ,WAIT,DONE,FAULT}` + `cur_level` 寄存器；**枚举值与现状一致** ⇒ 断言值不变 | U9 |
| [D10](adr/ADR-085-D10-fsm-base-wrapper.md) | walk FSM 用框架级 `FsmBase` 封装（不再裸用 `ch_state_machine`） | PTW FSM 统一到框架级 `FsmBase`，不再裸用 `ch_state_machine` | U9 |
| [D11](adr/ADR-085-D11-tlb-ptw-interface.md) | TLB ↔ PTW 接口契约 | 请求 `{vaddr,asid,access_type}` / 响应 `{paddr,perms,fault_code}`；同拍发起、忙时丢弃 | U1 |
| [D12](adr/ADR-085-D12-ptw-active-combinational.md) | `PTW_ACTIVE` 由 **TLB miss 组合派生**，不是 PTW 的 busy 状态 | `PTW_ACTIVE = sv_active && !paddr_valid`（组合派生）⇒ **只成立于 miss 窗口内** | U8 |
| [D13](adr/ADR-085-D13-pte-encoding-and-permission.md) | 逐级权限检查 + RISC-V 的 reserved 编码（**修正现有实现**） | 按 spec 翻译算法 step 3/4/5 判定；`is_reserved = !r && w`；判据**单点实现**于 `lib/pte_decode.h` | U4、U5 |
| [D14](adr/ADR-085-D14-superpage-two-sided.md) | Superpage（巨页）拼接（PTW 侧 + TLB 侧） | `pteshift = 12 + VPN_BITS*k`（sv32 k=1 ⇒ **22**）；**TLB 侧同步加 `page_shift`** + 掩码 tag 比较 | U6、U11 |
| [D15](adr/ADR-085-D15-walk-refill-tlb-only.md) | walk 结果只回填 TLB；`PADDR` 只能来自查表（地址打标，取消机制） | PTW **不直接写 `PADDR`**；结果只回填 TLB（按锁存的 `walk_vaddr`）⇒ 自动满足 FS-C-13/14 | U7 |
| [D16](adr/ADR-085-D16-ptw-physical-read-port.md) | PTW 的物理内存读端口（绕过翻译） | PTW 读物理地址 ⇒ 读端口**绕过翻译**；需要 `DMemPlugin` 暴露 `ch_mem& mem()` | U3 |
| [D17](adr/ADR-085-D17-chmem-capacity-limit.md) | CH_MEM 容量上限（elaboration 节点爆炸） | 两个 cache 容量全部配置化，CH_MEM 给**小默认值**（TLB 1×8×1 / PTWCache 1×1×4）⇒ 避免 elaboration 节点爆炸 | — |
| [D18](adr/ADR-085-D18-file-layout-dual-file.md) | 文件组织与双文件分离（ADR-083 / ADR-040） | PTW 代码落 `ip/mmu/plugins/`（修 ADR-083 目录违规）；PTE 判据**只允许一份实现**（`lib/pte_decode.h`） | U5 |
| [D19](adr/ADR-085-D19-verification-criteria.md) | 验证要求（U1–U11 + D19.1 fault 两层语义） | U1–U11 单元判据 + 6 项变异对照 + e2e + 5 门禁；D19.1 裁决 fault 的两层语义（L1 必做 / L2 推迟） | U1–U11 |
| [D20](adr/ADR-085-D20-satp-invalidation-path.md) | `satp` 变更 / `sfence.vma` 的失效路径（rev.2 新增） | `satp` 变更 ⇒ **flush-all**（影子寄存器比较）；`sfence.vma` 落地后改为显式触发；飞行中 walk 须中止 | U10 |
| [D21](adr/ADR-085-D21-physical-mem-handle-ownership.md) | 跨插件物理内存句柄的所有权（rev.2 新增，配合 D16） | `ch_mem` 所有权归 `DMemPlugin`；MMU 只持**非拥有引用**，句柄在 `setup()` 阶段取 | U3 |

**用户的 3 处提案修正**（对应 M1/M2/M3）落在：M1 → [D08](adr/ADR-085-D08-pt-base-derived-from-satp.md)、M2 → [D07](adr/ADR-085-D07-start-level-selection.md)、M3 → [D04](adr/ADR-085-D04-ptwcache-no-leaf-pte.md)。

## 4. 数据流与时序

### 4.1 TLB 命中路径（0 拍）

```
[MMU_TLB, ("fetch", EARLY)]
  vaddr ← ibus.pc_reg (活寄存器 proxy)
  ├─ line 内 8 路 tag 比较（asid + `vaddr >> entry.page_shift`）→ hit
  ├─ paddr = (entry.pfn << entry.page_shift) | (vaddr & ((1 << entry.page_shift) - 1))
  ├─ sv_active ← satp.MODE (bit31)
  ├─ PADDR_VALID = sv_active && hit
  ├─ PTW_ACTIVE  = sv_active && !PADDR_VALID      ← 组合派生（D12）
  └─ 写 5 个 key → tlb_lookup_ifetch 节点
```

### 4.2 TLB miss → walk 路径（sv32，多周期）

| 拍 | MMU_TLB | MMU_PTW | `PTW_ACTIVE` | `hold`（fetch 侧） |
|---|---|---|---|---|
| T | miss 检出 ⇒ 发 walk 请求（vaddr/asid/access_type） | IDLE ⇒ 接受请求 | **1** | **1**（PC 冻结） |
| T+1 | 保持 miss | REQ：呈现 `pte1_addr`（由 `pt_base` 算出）⇒ 发物理读 | 1 | 1 |
| T+2 | — | **WAIT**：收 L1 目录项 ⇒ 编码判定（[D13](adr/ADR-085-D13-pte-encoding-and-permission.md)）⇒ **仅指针时回填 PTWCache**（叶不入，[D4](adr/ADR-085-D04-ptwcache-no-leaf-pte.md)）；呈现 `pte0_addr` | 1 | 1 |
| T+3 | — | **WAIT**：收叶 PTE ⇒ 权限检查 ⇒ `DONE` | 1 | 1 |
| T+4 | — | `DONE`：写回 TLB entry（按 `walk_vaddr`）；`ch_reg` 写回**次拍**可见 | 1 | 1 |
| T+5 | TLB 命中 ⇒ `PADDR_VALID=1` ⇒ `PTW_ACTIVE=0` | `IDLE` | 0 | 0（PC 前进） |

> 拍数随"PTWCache 是否命中 L1 目录项"变化（`start_level`，[D7](adr/ADR-085-D07-start-level-selection.md)）与"物理读延迟模型（`aread` 0 拍 / `sread` 1 拍）"变化。上表按 `sread` + 冷 PTWCache 给出。**基线数字以实测为准，登记在 `sv32-baseline-matrix.csv`**（Phase D D.4）。

### 4.3 PTWCache 命中时的 `start_level` 场景

| 拍 | 场景 | 行为 |
|---|---|---|
| T | TLB miss | 发请求 |
| T+1 | REQ：当拍 `test(L1)` ⇒ **hit** ⇒ L0 基址当拍已知 ⇒ 同拍 `test(L0)` ⇒ **必 miss**（叶不入 PTWCache，[D4](adr/ADR-085-D04-ptwcache-no-leaf-pte.md)）⇒ `start_level = L0` | 无 L1 物理读；发出 L0 叶 PTE 的 1 次物理读 |
| T+2 | 收叶 ⇒ 编码/权限检查 ⇒ DONE | 回填 TLB |
| T+3 | TLB 命中 | 释放 |

⇒ **省掉 1 次物理访存**（sv32 共 2 次 ⇒ 降为 1 次）。

---

## 5. 与现有架构的关系

| 既有物 | 关系 |
|--------|------|
| **ADR-040 v3.0** | §10.5（payload key 必须 mode-aware）—— 本决策组不新增跨插件 key（TLB↔PTW 是**同插件内**的接口 ⇒ 走 C++ 句柄，不走 payload cell）⇒ **不触发新的 key 模式化需求**。§10.6（生产者/消费者顺序契约）—— MMU_TLB 仍是 `("fetch", EARLY)` 的生产者，不变 |
| **ADR-044**（L1 Cache↔MMU VIPT） | 不变。TLB 命中 ⇒ `PADDR` 语义不变 ⇒ VIPT 的 `idx_bits + offset_bits ≤ 12` 安全条件不变。⚠ 本决策组让 `PADDR_VALID` 的**来源**从"查表恒 false"变成"查表真值" ⇒ **ADR-044 描述的"实装推迟到 mmu-tlb-ptw-impl 以后"现在可以解锁** |
| **ADR-046**（多周期 FSM 豁免） | walk FSM 带 `CF_PLUGIN_USE_FSM_EXEMPT`（既有）⇒ 不变 |
| **ADR-049**（MMU PADDR 消费契约 + `MemoryInterface`） | `MemoryInterface::read_word(uint64_t phys_addr)`（`ip/mmu/lib/memory_interface.h:44-58`）正是 PTW 的物理读抽象 ⇒ **CH_MEM 侧应当复用同一抽象语义**（但实现是 `ch_mem` 端口，不是 TLM 的虚函数）⇒ 建议：`lib/memory_interface.h` 保留 TLM 用；CH_MEM 侧用 `ch_mem` 端口 + 一层薄适配 |
| **ADR-083**（Plugin-style SSOT） | §2.4 双文件分离 + `plugins/` 目录 ⇒ [D18](adr/ADR-085-D18-file-layout-dual-file.md) 的文件组织 |
| **fetch-stall v0.2 protocol** | 本决策组是其"生产者侧"的补齐。**不修改 protocol 任何条款**；但 **[D13](adr/ADR-085-D13-pte-encoding-and-permission.md) 会迫使 protocol 升 rev.15**（AC-FS-15 的坏 PTE 编码）⇒ 那是 protocol 的独立评审流程 |
| **`MMUPluginChmem`（现有 TLB-only）** | 迁移为 MMU_TLB 主体：8 项全相联 ⇒ 1 line × 8 ways；**新增 perms 字段**（现无）；`PTW_ACTIVE` 从常量 0 改为组合派生 |
| **`PtWalkFsmPlugin`（现有 PoC）** | 迁移到 `ip/mmu/plugins/mmu_ptw_chmem.h`，改用 `FsmBase`，输入改为表达式驱动（3a 的自由函数抽取），状态集改名但**枚举值不变**（[D9](adr/ADR-085-D09-walk-fsm-state-set.md)） |
| **`lib/ptw.h`（TLM 纯 C++ PTW）** | 保留给 TLM 侧。CH_MEM 侧**不复用**（命令式推进 vs 组合 DAG 不同构）。**但 PTE 解码/分类/权限判定抽到 `lib/` 共享**（[D13](adr/ADR-085-D13-pte-encoding-and-permission.md)） |

---

## 6. 替代方案评估

### 6.1 walk 形态：多周期 FSM vs 组合展开（否决组合展开）

| 维度 | 多周期 FSM（采纳） | 组合展开（否决） |
|------|-------------------|----------------|
| 可行性 | ✅ `ch_reg` 反馈是上游**唯一允许**的环（`context.cpp:294-296` 禁的是**环**，不是长链） | ❌ 在**单一 `ch_mem` 模型**下展开 ⇒ `mem` 输出回灌自己的地址输入 ⇒ **真组合环**（非长链）⇒ 上游禁止；即便拆成 N 个独立 mem 实例，也只是**无界组合深度**（N 级串联的 mem 读 + 地址计算）⇒ 不可综合 / 时序不可收敛 |
| `PTW_ACTIVE` 语义 | ✅ 有真实的 miss 窗口 | ❌ 恒 0 ⇒ fetch-stall protocol 的 3g 接线全部变死代码 |
| 架构保真 | ✅ 与 TLM 侧 `lib/ptw.h` 同构 | ❌ 与 RISC-V 架构不同构（真实 PTW 必然多周期） |
| 代价 | 需要 `ch_reg` + `FsmBase`；CH_MEM 第一个生产级多周期 FSM（F10 ⇒ 风险） | — |

### 6.2 跨 level 并行：同拍串联命中 test vs 投机并行（否决投机并行）

| 维度 | 同拍串联命中 test（采纳，[D7](adr/ADR-085-D07-start-level-selection.md) P2） | 投机并行（P3，否决） |
|------|------------------------------|------------------|
| 物理可行 | ✅ 只测**地址已知**的级（命中给出 `next_ppn` ⇒ 当拍可算下一级地址） | ⚠️ 需要 base predictor 猜深层地址 |
| 额外状态 | ✅ 无 | ❌ 预测器 + 取消逻辑 + 错误预测处理 |
| 收益（sv32） | ✅ 省 1 次访存（2→1；连续命中前缀最长 1 级 ⇒ **无跨级并行度**，见 [D7](adr/ADR-085-D07-start-level-selection.md) §D7.4） | 边际（省 1 拍，但预测错误要回滚） |
| 收益（sv39/48） | ✅ 连续命中前缀可达 2 级 ⇒ 有真实并行度 | 边际 |
| 复杂度 | 低 | 高 |

### 6.3 PTWCache 是否含叶 PTE（默认否决，见 §0 Q1）

| 维度 | 只存非叶（采纳） | 也存叶（备选） |
|------|------------------|---------------|
| 职责 | ✅ TLB 是叶的唯一缓存 | ❌ 两处缓存叶 |
| 访存次数（sv32） | 冷 2 次 / PTWCache 命中 1 次 | 冷 2 次 / 全命中 0 次 |
| 一致性 | ✅ 只读目录项，无写回负担 | ❌ 需处理 A/D 位写回与副本不一致 |
| 污染 | ✅ 目录项复用率高，各自用合适容量 | ❌ 需在"高复用目录项"与"低复用叶"间折中 |

### 6.4 `pt_base` 独立寄存器 vs `satp` 派生（否决独立寄存器，见 [D8](adr/ADR-085-D08-pt-base-derived-from-satp.md)）

理由见 [D8](adr/ADR-085-D08-pt-base-derived-from-satp.md) §1（单一真值源 + 项目事故前例）。

### 6.5 PTWCache 组合式替换 vs 直接映射（本期否决组合式）

`policies/` 的虚函数替换策略**无法在 CH_MEM 组合逻辑里使用**（[D6](adr/ADR-085-D06-ptwcache-index-replace-policy.md)）。本期用直接映射规避；未来 4-way 时用 PLRU（组合、无虚函数）。

---

## 7. 后果

### 7.1 正面后果

- ✅ **补齐 MMU 的第二层**：TLB（叶）+ PTW（目录项 + walk）⇒ sv32 真翻译的架构前提成立。
- ✅ **职责边界清晰** ⇒ 与业界核（Rocket/BOOM/CVA6）心智模型一致 ⇒ 未来移植真实核零成本。
- ✅ **`start_level` 机制**把"PTWCache 命中"从概念变成**可测的访存次数减少**（U3 是硬判据）。
- ✅ **`pt_base` 派生化**消除第二真值源 ⇒ 制度化预防 v0.10.4 `a3afcfa` 同款漂移。
- ✅ **状态集枚举值不变** ⇒ 现有 FSM 测试的断言值、protocol AC-FS-15 的 `FAULT==4` 机械锚**数值上不用改**（但构造方式与坏 PTE **编码**要改，见 [D9](adr/ADR-085-D09-walk-fsm-state-set.md)/[D13](adr/ADR-085-D13-pte-encoding-and-permission.md)）。
- ✅ **[D13](adr/ADR-085-D13-pte-encoding-and-permission.md) 修正了一个真实缺陷**（CH_MEM 把合法 RW 叶判为 fault），并把判据**收进 `lib/` 单点实现** ⇒ 同款 bug 已在 TLM 侧被修过一次（`lib/ptw.cpp:79-80` 注释的 "Oracle C1"），收拢后不再复发。
- ✅ **[D14](adr/ADR-085-D14-superpage-two-sided.md)/D14.1 补齐了 superpage 的两侧**（PTW 拼接 + TLB `page_shift`）—— 这是 rev.1 遗漏的一半：只拼 PTW 不补 TLB，4MiB 映射仍然全错。
- ✅ **[D20](adr/ADR-085-D20-satp-invalidation-path.md) 补上了失效路径**，避免"改 `satp` 不生效"这类**静默错误翻译**。

### 7.2 负面后果与缓解

| 负面后果 | 缓解 |
|----------|------|
| CH_MEM 节点数上升（PTWCache 的 `ch_reg` 数组） | [D17](adr/ADR-085-D17-chmem-capacity-limit.md) 的容量上限 + 阶段化抬升；用 `--test-case` 过滤做快回归 |
| CH_MEM **第一个生产级多周期 FSM**（F10）⇒ Verilator 后端行为未验证 | 强制在 walk change 内加一条 **Verilator 后端的 FSM 行为断言**（不只 C++ Simulator 绿） |
| TLB 的 2 端口共享存储体 ⇒ 比较逻辑 ×2 面积 | 已知代价；未来可做端口合并或复制体 |
| loadstore 请求可能被 ifetch 抢占 ⇒ 饥饿 | §7.3 R3；本期靠"重试"自愈 |
| A/D 位未实装 | 记为已知限制；entry 的 `perms` 预留 A/D 位 |
| **superpage 在 PTW 与 TLB 两侧都是错的**（PTW 侧直传 PPN；TLB 侧按 4KiB 写死 `paddr` 合成与全 `VPN` tag 比较） | [D14](adr/ADR-085-D14-superpage-two-sided.md)（PTW 拼接，sv32 shift=22）+ **[D14](adr/ADR-085-D14-superpage-two-sided.md) §D14.1**（TLB 带 `page_shift` + 掩码 tag）+ U6 判据（含"与 4KiB 路径结果不同"的防退化断言） |
| **TLB entry 加 `page_shift` ⇒ entry 变宽（+3 bit）、tag 比较走掩码 ⇒ 比较器不能再直接比全 `VPN`** | 已知代价；掩码比较本身与"按 4KiB 比较"同阶，只多一次与 `page_shift` 相关的选择 |
| **[D20](adr/ADR-085-D20-satp-invalidation-path.md) 的 `satp` flush-all 是粗粒度**：每次 `satp` 写清空 TLB + PTWCache ⇒ 上下文切换后必有一次冷 walk | 本期可接受（无 `sfence.vma`、无 ASID）；ASID 粒度失效登记为 follow-up（`cpu-pipeline-smode-umode` 之后） |
| **[D21](adr/ADR-085-D21-physical-mem-handle-ownership.md) 的句柄生命周期约束**：MMU 持非拥有 `ch_mem*` ⇒ 若 `PipeBuilder` 允许单独销毁 DMem，会变悬垂指针 | 所有权归 `DMemPlugin`（创建者）+ 同一 `pb` 内同生共死；`setup()` 阶段取句柄并缓存（不在 `at_stage` 里惰性获取） |
| [D13](adr/ADR-085-D13-pte-encoding-and-permission.md) 迫使 protocol 升 rev.15 并走 critic | 必要代价：protocol 判据层已 15 轮零 BLOCKER，改判据必须重新评审 |

### 7.3 风险登记

| ID | 风险 | 等级 | 缓解 |
|----|------|:----:|------|
| R1 | **CH_MEM 多周期 FSM 无生产先例** ⇒ Verilator 后端可能不按预期推进 | **高** | change 内强制加 Verilator 断言；先做 1 个最小 FSM 冒烟（可作为 walk change 的 session 0） |
| R2 | PTWCache 的 `ch_reg` 数量爆炸 ⇒ CH_MEM 全量测试时长失控 | 中 | [D17](adr/ADR-085-D17-chmem-capacity-limit.md) 容量上限；实测单测时长，超阈即降容量 |
| R3 | ifetch 持续抢占 ⇒ loadstore 饥饿 | 中 | 记为已知限制；未来加"同拍合并"或最小延迟保证 |
| R4 | **PTW 读物理内存的句柄跨模块**（`DMemPlugin::mem()`）未在 change 清单里 ⇒ 实现者撞墙 | 中 | **必须补进受影响文件清单**（[D16](adr/ADR-085-D16-ptw-physical-read-port.md)） |
| R5 | protocol rev.15 与本决策组的 [D13](adr/ADR-085-D13-pte-encoding-and-permission.md) 耦合 ⇒ 两处评审节奏不一致 | 低 | [D13](adr/ADR-085-D13-pte-encoding-and-permission.md) 与 protocol rev.15 绑定为**同一个原子交付**（先改 spec 再改代码） |
| R6 | `PTW_ACTIVE` 组合派生 ⇒ TLB 的 `PADDR_VALID` 必须逐地址正确，否则 hold 逻辑错 | 中 | U1/U8 + protocol AC-FS-11（同页多拍 `PADDR_VALID` 持续） |
| R7 | **多物理内存（imem/dmem 分离）** ⇒ [D21](adr/ADR-085-D21-physical-mem-handle-ownership.md) 的单句柄假设失效；若 MMU 只拿到 dmem 句柄，取指侧 walk 会读错内存 | 中 | 本期**明确假设单一 `ch_mem`**（`soc/cpu_l1_mmu_demo` 即如此）；将来分离时句柄改双端口/列表（所有权规则不变），并补 U 判据 |
| R8 | **walk 在飞行中 `satp` 被改写** ⇒ 已取回的 PTE 属于旧页表，若继续用会产出**跨地址空间**的 `paddr` | 中 | [D20](adr/ADR-085-D20-satp-invalidation-path.md) 的 `satp_changed` 同拍**中止飞行中的 walk**（回到 `IDLE`、清 `PTW_ACTIVE`、不回填 TLB），与 [D15](adr/ADR-085-D15-walk-refill-tlb-only.md) 的"地址变更即弃"同一纪律；U10 增加该场景 |

---

## 8. 实施检查清单（分阶段）

> **本清单是架构层面的阶段划分，不构成 change 拆分**（change 拆分属 planner 职责；已知约束见 `chmem-ptw-walk` 的 3a~3g 与本决策组§9）。

| # | 阶段 | 内容 | 验证 |
|---|------|------|------|
| 1 | **P0 规格同步** | ① [D13](adr/ADR-085-D13-pte-encoding-and-permission.md) 的编码判据写进 `lib/pte_decode.h` 并让 TLM/CH_MEM **共用**；② `fetch_stall_protocol_v0.11.0.md` 升 rev.15（AC-FS-15 坏 PTE 编码改 `!R && W` + 增正向判别力 + 删"以代码为准"pin）；③ `test_mmu_ptw_fsm_chmem.cpp` Test 2 换向量 + 增正向用例 | `[mmu]` 59/59 不退化；protocol critic 通过 |
| 2 | **P1 骨架** | `lib/pte_decode.h`（`classify_pte` / `check_leaf_perms`，单测）；`ip/mmu/plugins/mmu_ptw_chmem.h` 的 `FsmBase` 骨架 + 状态集（值不变）+ `build_pt_walk_fsm()` 自由函数 + `ptw_fsm()` 访问器 | 编译通过；FsmBase 状态机最小冒烟 |
| 3 | **P2 PTWCache**（**可默认关闭**，[D7](adr/ADR-085-D07-start-level-selection.md) §D7.4） | line 格式（[D5](adr/ADR-085-D05-ptwcache-entry-width-epl.md)）+ 索引/替换（[D6](adr/ADR-085-D06-ptwcache-index-replace-policy.md)）+ 同拍串联 test（[D7](adr/ADR-085-D07-start-level-selection.md) P2）+ 回填 | U2 判据；关闭时 walk 仍全功能 |
| 4 | **P3 walk** | `pt_base` 派生（[D8](adr/ADR-085-D08-pt-base-derived-from-satp.md)）+ 逐级物理访存（[D16](adr/ADR-085-D16-ptw-physical-read-port.md)/[D21](adr/ADR-085-D21-physical-mem-handle-ownership.md) 句柄）+ `start_level`（[D7](adr/ADR-085-D07-start-level-selection.md)）+ 回填 TLB（[D15](adr/ADR-085-D15-walk-refill-tlb-only.md)）+ superpage（[D14](adr/ADR-085-D14-superpage-two-sided.md)，**含 TLB 侧 `page_shift`**）+ 编码/权限（[D13](adr/ADR-085-D13-pte-encoding-and-permission.md)） | U3~U7 判据 |
| 5 | **P4 接线** | `MMUPluginChmem` 的 `PTW_ACTIVE` 组合派生（[D12](adr/ADR-085-D12-ptw-active-combinational.md)）；`MMU_TLB` 加 `perms` + `page_shift`；`satp` 变更 flush-all + 飞行中 walk 中止（[D20](adr/ADR-085-D20-satp-invalidation-path.md)/R8）；与 fetch-stall protocol 的 3g 同批 | U8/U9/U10/U11 + protocol AC-FS-9/15/16 |
| 6 | **P5 验证** | e2e（真页表）+ 变异对照 ≥6（[D19](adr/ADR-085-D19-verification-criteria.md) 列 6 项）+ Verilator 后端 FSM 断言 + 5 架构 gate | [D19](adr/ADR-085-D19-verification-criteria.md) 全部 |

---

## 9. 与 `chmem-ptw-walk` 的关系（change 边界提示）

本决策组 **不创建 change**。以下是它与既有 `chmem-ptw-walk` 拆解（protocol §6.2 / `openspec/changes/mmu-chmem-phase-b-c-launch/tasks.md:125-131`）的**映射与偏差**：

| 既有子项 | 本决策组的对应 | 偏差 |
|----------|--------------|------|
| 3a `build_pt_walk_fsm` 自由函数抽取 | [D9](adr/ADR-085-D09-walk-fsm-state-set.md) + [D10](adr/ADR-085-D10-fsm-base-wrapper.md)（`FsmBase` + 状态集） | ⚠ 抽取后**删除 `PtWalkFsmPlugin`**（避免"看起来能注册、实际不能"的惰性骨架）⇒ **`tests/cpu/test_mmu_ptw_fsm_chmem.cpp:50-66` 的 `PtwFixture` 必须改写**为调用自由函数；生产插件须暴露 `ptw_fsm()` 访问器（AC-FS-15 (a) 锚要求） |
| 3b `RiscvMMUPluginChmem` 适配器 | [D8](adr/ADR-085-D08-pt-base-derived-from-satp.md)（satp 派生）+ [D11](adr/ADR-085-D11-tlb-ptw-interface.md)（接口契约） | ⚠ 适配器必须**不引入独立 `pt_base` 寄存器** |
| 3c walk 触发 + refill | [D7](adr/ADR-085-D07-start-level-selection.md) + [D12](adr/ADR-085-D12-ptw-active-combinational.md) + [D13](adr/ADR-085-D13-pte-encoding-and-permission.md) + [D14](adr/ADR-085-D14-superpage-two-sided.md) + [D15](adr/ADR-085-D15-walk-refill-tlb-only.md) + [D16](adr/ADR-085-D16-ptw-physical-read-port.md) + [D21](adr/ADR-085-D21-physical-mem-handle-ownership.md) | ⚠ **新增 4 项未在清单里的工作**：① [D16](adr/ADR-085-D16-ptw-physical-read-port.md)/[D21](adr/ADR-085-D21-physical-mem-handle-ownership.md) 的 `DMemPlugin::mem()` 跨模块 API + 所有权约定；② [D14](adr/ADR-085-D14-superpage-two-sided.md) 的 superpage 拼接**含 TLB 侧 `page_shift`**；③ `lib/pte_decode.h` 抽取（[D13](adr/ADR-085-D13-pte-encoding-and-permission.md)）；④ [D20](adr/ADR-085-D20-satp-invalidation-path.md) 的 `satp` 变更 flush-all |
| 3d `mmu_keys` 剩余 key 模式化 | 与本决策组正交 | — |
| 3e MEM_ADDR 源接线 + 数据侧冻结 | 与本决策组正交，但**依赖** [D15](adr/ADR-085-D15-walk-refill-tlb-only.md)（`PADDR` 只由查表产生） | — |
| 3f 测试 | [D19](adr/ADR-085-D19-verification-criteria.md) | — |
| 3g fetch-stall 接线 | protocol §7（不变） | 不变 |

**另外必须列入受影响文件清单的既有债**（不影响架构，但不改会误导实现者）：

- `ip/cpu/plugins/ibus_chmem.h:82-84` 的注释「`aread`/`sread` both have 1-cycle latency」——与 F9 矛盾（`aread` 是 0-cycle）⇒ 同批改注释。
- `ip/cpu/plugins/mmu_ptw_chmem.h:245-247` / `:220` 的两处 reserved 编码注释（[D13](adr/ADR-085-D13-pte-encoding-and-permission.md)）—— 与代码同 commit 修正。
- `tests/cpu/test_mmu_ptw_fsm_chmem.cpp` Test 2 的坏 PTE 向量（[D13](adr/ADR-085-D13-pte-encoding-and-permission.md)）。

⇒ **偏差汇总（≥5 项，不是 4 项）**：本决策组给 `chmem-ptw-walk` 增加的**未列工作** =

1. `DMemPlugin::mem()` 跨模块 API + `ch_mem` 所有权约定（[D16](adr/ADR-085-D16-ptw-physical-read-port.md)/[D21](adr/ADR-085-D21-physical-mem-handle-ownership.md)）；
2. superpage 拼接**含 TLB 侧 `page_shift` + 掩码 tag**（[D14](adr/ADR-085-D14-superpage-two-sided.md) / §D14.1）；
3. `lib/pte_decode.h` 抽取并让 TLM/CH_MEM 共用（[D13](adr/ADR-085-D13-pte-encoding-and-permission.md)）；
4. `satp` 变更 flush-all + 飞行中 walk 中止（[D20](adr/ADR-085-D20-satp-invalidation-path.md) / R8）；
5. **3a 的连带改动**：删除 `PtWalkFsmPlugin` + 改写 `PtwFixture`（`test_mmu_ptw_fsm_chmem.cpp:50-66`）+ 新增 `ptw_fsm()` 访问器 + `FsmBase` 迁移（[D9](adr/ADR-085-D09-walk-fsm-state-set.md)/[D10](adr/ADR-085-D10-fsm-base-wrapper.md)）—— 清单里 3a 只写了"自由函数抽取"，**没写"删插件 + 改测试"**。

并**否决了 1 项**既有设计（独立 `pt_base` 寄存器）、**改判了 1 项 protocol 判据**（AC-FS-15 的坏 PTE 编码 + 新增正向判别力）。⇒ 估时需相应上调（**估时/排期属 planner 职责，本决策组只负责把范围增量列全**）。

---

## 10. 验证命令

```bash
# 架构门禁（5 个）
bash tools/verify_adr.sh
bash tools/verify_plugin_decision.sh
bash tools/check_plugin_portability.sh
bash tools/doc_link_check.sh
bash tools/roadmap-r9-check.sh

# 本决策组的结构性检查（P0~P3 落地后；⚠ 极性/路径经 rev.2 修正）
grep -q "is_reserved" ip/mmu/plugins/mmu_ptw_chmem.h && echo "FAIL: 判定仍在 CH_MEM 侧（D13 要求集中到 lib/pte_decode.h）"
grep -q "classify_pte" ip/mmu/lib/pte_decode.h || echo "FAIL: lib/pte_decode.h 未提供 classify_pte（D13）"
grep -qE "PPN\[1\]|<< *22|pteshift" ip/mmu/plugins/mmu_ptw_chmem.h || echo "FAIL: sv32 巨页 shift=22 未落地（D14）"
grep -q "page_shift" ip/mmu/plugins/MMUPlugin_chmem.h || echo "FAIL: TLB entry 缺 page_shift（D14 §D14.1）"
grep -qE "satp_changed|flush" ip/mmu/plugins/MMUPlugin_chmem.h || echo "FAIL: satp 变更无 flush 路径（D20）"
# D8：以「接口存在性」代替命名判据 —— 要求 pt_base 只能从 satp 派生
grep -qE "pt_base" ip/mmu/plugins/mmu_ptw_chmem.h || echo "FAIL: PTW 未从 satp 派生根表基址（D8）"
grep -qE "ch_reg.*pt_base|pt_base.*ch_reg" ip/mmu/plugins/mmu_ptw_chmem.h && echo "FAIL: 出现独立的 pt_base 寄存器（D8 禁止）"

# 回归
./build/bin/chipforge_tests "[mmu]"                            # TLM
./build/bin/chipforge_tests_chmem "[mmu-chmem-basic]"          # CH_MEM
bash tools/run_chipforge_tests.sh --all                        # 全量（CH_MEM ~833s/轮）
```

---

## 11. 参考

- **RISC-V Privileged Spec v1.12**：**§5.3.2 虚拟地址翻译过程（step 3 是编码/reserved 的唯一权威判据、step 4 是叶/指针判据、step 5 是叶权限）**；§12.3 与 §12.3.2（Sv32，VPN 10 bit）；`satp` 编码见 §5.3 / §12.3；页错误优先级 §5.4；`sfence.vma` §12.2.1；`Svade`/`Svadu`（A/D 位）§12.10
  > ⚠ **不要引用"Table 5.1"作为编码权威**（rev.1 如此引用并因此写错）—— 编码与 fault 条件的权威表述是**翻译算法的 step 3/4 原文**：「If `pte.v=0`, or if `pte.r=0 and pte.w=1`, or if any bits or encodings that are reserved for future standard use are set within `pte`, stop and raise a page-fault exception」，以及「If `pte.r=1` or `pte.x=1`, go to step 5. Otherwise, this PTE is a pointer to the next level」。
- **上游 CppHDL**：`include/chlib/state_machine.h`（FSM DSL + `if(ch_bool)` 陷阱）、`include/core/reg.h` + `lnode/reg.tpp`（`ch_reg` next 语义）、`include/core/mem.h:125/153`（aread/sread）、`src/core/context.cpp:294-296`（组合环只允许 reg）
- **上游 commit**：`5af3e40`（`ch_state_machine` cycle-accurate）、`c27543d`（`aread` 0-cycle 对齐）、`7d69f56`（无 next 的 `ch_reg` 生成 hold-state）
- **ChipForge 内部**：`include/cf/plugin/multi_cycle_fsm.h`（`FsmBase`）、`include/cf/plugin/ctrl_link.h`（stateful halt）、`.rddf/protocols/fetch_stall_protocol_v0.11.0.md`（v0.2 rev.14）、`openspec/changes/mmu-chmem-phase-b-c-launch/tasks.md:125-131`（推迟项）、`.rddf/improvements/mmu-chmem-phase-b-c-launch.md:37-44`（CI 盲区）
- **业界对照**：Rocket Chip（TLB + PTW，串行 walk）、BOOM（TLB + PTW）、CVA6（TLB + PTW + PTW cache，含叶 PTE 的变体）

---

*本决策组由 `chmem-ptw-walk` 架构调研（architect session，2026-10-22）驱动。实装开始后，状态从 📝 升到 🚧（设计锁定、实装中）再到 ✅（实装 + 集成测试通过）。同步更新 [`docs/architecture/adr.md`](../../../docs/architecture/adr.md) 主表。*

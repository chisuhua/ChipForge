# ADR-085: MMU_PTW 架构 — 伞索引（D01–D21）

| 字段 | 值 |
|------|-----|
| 状态 | 📝 **设计提案**（架构方向锁定，实装未开始 —— 归属未立项 change `chmem-ptw-walk`）<br>注：全局注册表 `docs/architecture/adr.md` 只有 `✅/⚠️/🚧` 三态、无 📝 ⇒ 本 ADR 在那张表里登记为 **🚧 + 备注"设计提案"**；本文的 📝 是更细的自有生命周期标记（📝 提案 → 🚧 实装中 → ✅ 接受） |
| 层级 | **IP 级**（删除 `ip/mmu/` 后本 ADR 失去意义 ⇒ 归 `ip/mmu/docs/adr/`，非框架级） |
| 架构文档 | **[mmu-ptw-architecture.md](../mmu-ptw-architecture.md)** —— 设计正文（背景 / 职责边界 / 数据流与时序 / 替代方案 / 后果 / 实施清单 / 验证命令） |
| 决策文件 | 本目录下 `ADR-085-D01` … `ADR-085-D21`（**21 个，一决策一文件**） |
| 来源 | `chmem-ptw-walk`（**未立项** change；范围增量见架构文档 §9） |

---

## 0. 如何阅读

本 ADR 组按**职责分离**组织成两类文件：

| 文件 | 内容 | 何时读 |
|------|------|--------|
| **架构设计文档**（`../mmu-ptw-architecture.md`） | 设计正文：背景与动机、职责边界、**决策索引（§3）**、数据流与时序、替代方案评估、后果（正面/负面/风险登记）、实施检查清单 P0–P5、与 change 的关系、验证命令 | **先读这个** —— 建立全局认知 |
| **21 个子 ADR**（`ADR-085-D01..D21`） | 每个决策一个文件：上下文 / 决策正文 / 交叉引用 | 按需精读某一条决策时 |

**全局注册表只登记本条**（ADR-085）—— 子决策用 `ADR-085-Dnn` 子号，不占用全局 ADR 号段（`docs/research/adr-matrix.md` 已把 **ADR-070~084** 规划给本路线图的后续决策；`ADR-076` 预留给了 ASID 主题，见 §3）。

**引用约定**：`§0`–`§11` 指架构文档的章节（`§3` 为决策索引）；`§D7.x` / `§D14.1` 指对应子 ADR 内的子节；其它文档的章节号自带文档名（RISC-V spec `§5.3.2`、ADR-040 `§10.5`、fetch-stall protocol `§2.1`）。

---

## 1. 决策清单（D01–D21）

| 编号 | 决策标题 | 一句话要点 | 关联 U 判据 |
|------|---------|-----------|-----------|
| [D01](ADR-085-D01-mmu-tlb-ptw-split.md) | MMU 分解为 MMU_TLB + MMU_PTW | MMU 拆成 MMU_TLB（组合查表）+ MMU_PTW（多周期 walk），对外仍是**一个** MMU 插件 | U1 |
| [D02](ADR-085-D02-tlb-entry-layout.md) | TLBCache 的 entry 与 cacheline 组织 | TLB entry = `{valid,asid,tag,pfn,perms,page_shift,global}`；cacheline 存 EPL 个 entry，EPL 路 tag 比较并行 | U1 |
| [D03](ADR-085-D03-tlb-dual-port-arbitration.md) | MMU_TLB 的 2 端口与"同拍双 miss" | 两个端口可同拍 miss；walk 请求由 PTW 仲裁（ifetch > loadstore），低优先级**丢弃不排队**、下拍重试 | U1 |
| [D04](ADR-085-D04-ptwcache-no-leaf-pte.md) | PTWCache 只缓存**非叶**目录项（PDE） | 不存叶 PTE ⇒ 与 TLB 职责不重叠；代价是 walk 最少 1 次访存 | U2 |
| [D05](ADR-085-D05-ptwcache-entry-width-epl.md) | PTWCache entry 宽度固定 8B；cacheline size 与 EPL 可配 | entry 宽度**固定 8B**（与 Sv mode 解耦）；64B ⇒ 8 entries/line；tag = `{level,parent_ppn,asid}` | U2 |
| [D06](ADR-085-D06-ptwcache-index-replace-policy.md) | PTWCache 的 index / set 映射与替换策略 | index = `parent_ppn` 低位；CH_MEM 用**直接映射**（1 way）规避组合替换逻辑 | U2 |
| [D07](ADR-085-D07-start-level-selection.md) | PTWCache 命中 ⇒ `start_level` 选择 | 同拍串联 hit test 得出 `start_level` = 连续命中前缀之后那一级 ⇒ 剩余访存 = `start_level+1` | U3 |
| [D08](ADR-085-D08-pt-base-derived-from-satp.md) | `pt_base` 不设独立寄存器，取 `satp` 的组合派生视图（修正 M1） | **不设独立寄存器** ⇒ 避免「模式改了但根表地址没改」的静默错误翻译 | U10 |
| [D09](ADR-085-D09-walk-fsm-state-set.md) | walk FSM 的形态与状态集 | 固定 5 态 `{IDLE,REQ,WAIT,DONE,FAULT}` + `cur_level` 寄存器；**枚举值与现状一致** ⇒ 断言值不变 | U9 |
| [D10](ADR-085-D10-fsm-base-wrapper.md) | walk FSM 用框架级 `FsmBase` 封装 | 统一到框架级 `FsmBase`，不再裸用 `ch_state_machine` | U9 |
| [D11](ADR-085-D11-tlb-ptw-interface.md) | TLB ↔ PTW 接口契约 | 请求 `{vaddr,asid,access_type}` / 响应 `{paddr,perms,fault_code}`；同拍发起、忙时丢弃 | U1 |
| [D12](ADR-085-D12-ptw-active-combinational.md) | `PTW_ACTIVE` 由 **TLB miss 组合派生** | `PTW_ACTIVE = sv_active && !paddr_valid`（组合派生）⇒ **只成立于 miss 窗口内** | U8 |
| [D13](ADR-085-D13-pte-encoding-and-permission.md) | 逐级权限检查 + RISC-V 的 reserved 编码（**修正现有实现**） | 按 spec 翻译算法 step 3/4/5 判定；`is_reserved = !r && w`；判据**单点实现**于 `lib/pte_decode.h` | U4、U5 |
| [D14](ADR-085-D14-superpage-two-sided.md) | Superpage（巨页）拼接（PTW 侧 + TLB 侧） | `pteshift = 12 + VPN_BITS*k`（sv32 k=1 ⇒ **22**）；**TLB 侧同步加 `page_shift`** + 掩码 tag 比较 | U6、U11 |
| [D15](ADR-085-D15-walk-refill-tlb-only.md) | walk 结果只回填 TLB；`PADDR` 只能来自查表 | PTW **不直接写 `PADDR`**；结果只回填 TLB（按锁存的 `walk_vaddr`）⇒ 自动满足 FS-C-13/14 | U7 |
| [D16](ADR-085-D16-ptw-physical-read-port.md) | PTW 的物理内存读端口（绕过翻译） | 读端口**绕过翻译**（防递归）；需要 `DMemPlugin` 暴露 `ch_mem& mem()` | U3 |
| [D17](ADR-085-D17-chmem-capacity-limit.md) | CH_MEM 容量上限（elaboration 节点爆炸） | 容量全部配置化，CH_MEM 给**小默认值**（TLB 1×8×1 / PTWCache 1×1×4） | — |
| [D18](ADR-085-D18-file-layout-dual-file.md) | 文件组织与双文件分离（ADR-083 / ADR-040） | PTW 代码落 `ip/mmu/plugins/`（修 ADR-083 目录违规）；PTE 判据**只允许一份实现** | U5 |
| [D19](ADR-085-D19-verification-criteria.md) | 验证要求（U1–U11 + D19.1 fault 两层语义） | U1–U11 + 6 项变异对照 + e2e + 5 门禁；D19.1 裁决 fault 两层（L1 必做 / L2 推迟） | U1–U11 |
| [D20](ADR-085-D20-satp-invalidation-path.md) | `satp` 变更 / `sfence.vma` 的失效路径 | `satp` 变更 ⇒ **flush-all**；`sfence.vma` 落地后改显式触发；飞行中 walk 须中止 | U10 |
| [D21](ADR-085-D21-physical-mem-handle-ownership.md) | 跨插件物理内存句柄的所有权 | `ch_mem` 所有权归 `DMemPlugin`；MMU 只持**非拥有引用**，句柄在 `setup()` 阶段取 | U3 |

**用户的 3 处提案修正**：M1（`pt_base`）→ [D08](ADR-085-D08-pt-base-derived-from-satp.md)；M2（跨级同时 test）→ [D07](ADR-085-D07-start-level-selection.md)；M3（PTWCache 含不含叶 PTE）→ [D04](ADR-085-D04-ptwcache-no-leaf-pte.md)。

---

## 2. 修订历史（critic 评审记录）

| 版本 | 日期 | 评审结论 | 主要变更 |
|------|------|---------|---------|
| rev.1 | 2026-10-22 | critic 一轮 **REJECT**（2 BLOCKER） | 初稿：D1–D21 + 时序 + 替代方案 + 后果 + 实施清单 + 判据 |
| rev.2 | 2026-10-22 | critic 二轮 **REJECT**（B1/B2 闭环 + 1 新 BLOCKER B3） | D13 按 **spec 翻译算法原文**重写（不再自编编码表）；D14 sv32 shift 21→**22**；新增 §D14.1（TLB 侧 `page_shift`）；D7 `resume level` → `start_level`；D9 撤回"测试零改动"；D8 删参数化通式；新增 D20 / D21；§9 偏差汇总 |
| rev.3 | 2026-10-23 | 定向复核：**B3 闭环 / rev.3 自洽 OK / 新 BLOCKER 无** | 新增 **D19.1**（fault 两层语义，解决 U9 与 U5 字面矛盾）；D12 生命周期 scope；D9 三处时序叙事统一（REQ 发首级 / WAIT 收后续并自循环）；§9 偏差 4 → **≥5 项**；perms 位宽；sv32 paddr 34 位截断（已知限制）；`is_reserved` 公式统一；D12 引用指针修正 |
| **拆分** | 2026-10-23 | — | 拆为「**架构设计文档** + **21 个子 ADR** + **本伞索引**」；修正 2 处悬空内部引用（原文 `§3.6` / `§3.4` 指向不存在的章节） |

### 2.1 评审中**撤回**的错误（留档，防止复发）

| 错误 | 出处 | 正确表述 |
|------|------|---------|
| 把 `R=1,W=0,X=0`（**合法只读叶**）判为 reserved | rev.1 的 PTE 编码表 | 唯一 reserved 判据是 `!R && W`（spec 翻译算法 step 3） |
| 指控 TLM 侧 `lib/ptw.cpp:79` 的 `w && !r` 有缺陷 | rev.1 | TLM 侧**本来就与 spec 一致**，不改；缺陷只在 CH_MEM 侧的 `r && w` |
| sv32 巨页 `<< 21` | rev.1 | sv32 的 VPN 是 **10 bit** ⇒ `pteshift = 12 + 10k`，k=1 ⇒ **22**（`<< 21` 是 sv39 的 2MB shift） |
| 把「多级同时 hit test」写成可实现形态 | rev.1 | 跨级**地址**串行依赖 ⇒ 只有「同拍串联 test + `start_level`」可实现 |

---

## 3. 与既有 ADR / 协议 / 规划的关系

| 既有物 | 关系 |
|--------|------|
| **ADR-040 v2.0** | §10.5（payload key 必须 mode-aware）—— D11/D12 的 cell 派生受其约束 |
| **ADR-046** | 多周期 FSM 豁免 —— walk FSM 必须带 `CF_PLUGIN_USE_FSM_EXEMPT`（D09） |
| **ADR-049** | MMU PADDR 契约 —— D15「`PADDR` 只能来自查表」是其在 CH_MEM 侧的落地 |
| **ADR-082** | capability 协商 —— D10 的 `FsmBase` 走 `cap.provide("multi_cycle_fsm", …)` |
| **ADR-083** | Plugin-style SSOT —— D18 的 `plugins/` 落位与双文件分离 |
| **fetch-stall protocol v0.2 rev.14** | ⚠ **必须升 rev.15**（D13）：AC-FS-15 ② 当前把 `V=1&&R=1&&W=1` 钉为"权威坏 PTE"并注"以代码为准" ⇒ **protocol 正在把 bug 变成验收判据**。rev.15 需改坏 PTE 编码 + 删 pin + **新增正向判别力**（RW 叶必须翻译成功）。改判据须重新走 critic |
| **`.rddf/protocols/` AC-FS-16** | W1 = `PTW_ACTIVE` 写语句的 **RHS token 白名单**（结构反回归）；窗口定义在 **AC-FS-15 ②**（按取指事实定义，含 FAULT 拍）—— D12 引用的是后者 |
| **`docs/research/adr-matrix.md:102`** | 已预留 `ip/mmu/docs/adr/ADR-076-...`（**ASID vs 全局 TLB 冲刷**，owner mmu）⇒ 与 **D20**（`satp` 失效路径）主题相邻。两者须在实现时对齐：ASID 粒度失效是 D20 登记的 follow-up |

---

## 4. 门禁

本 ADR 组的文档必须通过 5 个架构 gate（实测命令见[架构文档 §10](../mmu-ptw-architecture.md)）：

```bash
bash tools/verify_adr.sh
bash tools/verify_plugin_decision.sh
bash tools/check_plugin_portability.sh
bash tools/doc_link_check.sh
bash tools/roadmap-r9-check.sh
```

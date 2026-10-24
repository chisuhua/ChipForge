# ③ chmem-l1cache-body — 执行文件

> **change**: `chmem-l1cache-body`（**改名理由见索引 D3**：原候选 `chmem-l1cache-本体` 中文名不合 kebab-case 惯例）
> **规模**: 大 · **前置**: 无（可与 ② 并行）· **owner**: 本 change 事实承接（`verilator-l1cache-e2e-coverage` **不存在** ⇒ 原 owner 声明是空指针）
> **路线图**: `feat-tlm-backend-retirement` B1 第 3 项 · 台账 `[cache]` 25 条 + `[MMUCacheIntegration]` 4 条
> **上游**: ADR-088 v1.1 §5.2 判据 2（台账 `not-covered` 条目须有归属）· ADR-085/086（MMU 侧目标架构）

---

## Goal

把 **L1CachePlugin 本体**移植到 CH_MEM 后端（当前 CH_MEM 侧**只有 refill FSM**），并解除 `enable_cache=true` 的 fail-fast throw，使 CH_MEM 工厂能真正启用 Cache。

**为什么必须做**：台账里 `[cache]` 25 条 + `[MMUCacheIntegration]` 4 条因 L1Cache 本体缺失而判 `partial` / `not-covered`；其中 **4 条 `not-covered` 的缺失原因直接写着「CH_MEM 侧无 L1CachePlugin 本体」** ⇒ 本 change 是那 4 条能否从 `not-covered` 升级的前提。

## Architecture

```
ip/cache/plugins/L1CachePlugin.h / .cpp   (TLM 本体: 228 + 285 行, C++ 类 + std 容器)
        ↓ 移植（ADR-040 v3.0 约束下的重写, 非逐行翻译）
ip/cache/plugins/L1CachePlugin_chmem.h    (新: CH_MEM 版)
        ├─ 存储: ch_mem 数组 (data + tag + valid)
        ├─ 替换策略: lru_policy / no_replacement_policy → 移植为 ch_uint 比较
        ├─ 命中判定: tag 比较 + valid 位
        └─ 与 MMU 的交互: 查 MMUPluginChmem 得 PADDR（MMU 侧目标架构 = ADR-085/086）
ip/cpu/cpu_factory_chmem.h   (改)
        └─ enable_cache=true → 不再 throw；注册 L1CachePluginChmem
```

**已知缺口（必须先解决）**：CH_MEM 侧**无 MMU 流水线级接线**（`enable_mmu=true` 是 plumbing-only no-op，`cpu_factory_chmem.h:86`）⇒ L1Cache 要拿 PADDR，必须先有**可用的地址来源**：
- 方案 A：L1Cache 在 MMU 未接线时**只支持 Bare 模式**（PAddr = VAddr）⇒ 可独立交付，MMU 接线后再扩展
- 方案 B：等 `chmem-ptw-walk`（⑤）落地后一起做 ⇒ L1Cache 被 ⑤ 阻塞
⇒ **建议方案 A**（解耦，见 Task 2 Step 2 的决策点）；若 architect 判定 A 会造成「半真半假」的 Cache 语义，则改走 B。

## Tech Stack

C++23 · CppHDL（`ch_mem` / `ch_uint` / `ch_reg`）· Catch2 · ADR-040 v3.0 移植性约束（`check_plugin_portability.sh` 13/13）

---

## Tasks

### Task 1: 事实复核 + owner 归属修正

- [ ] **Step 1: 读 TLM 本体** —— `ip/cache/plugins/L1CachePlugin.h`（228）+ `.cpp`（285）+ `ip/cache/policies/{lru_policy,no_replacement_policy,replacement_policy}.{h,cpp}` ⇒ 提取语义（命中/未命中路径、写策略、替换时机、tag 比较、与 IBus/DMem 的接口）
- [ ] **Step 2: 修正 owner 注释** —— `ip/cpu/cpu_factory_chmem.h:89-91` 声称 owner = `verilator-l1cache-e2e-coverage`（**该 change 不存在**）⇒ 改为本 change 名
- [ ] **Step 3: 确认无 owner 冲突** —— `grep -rn "l1cache" openspec/changes/ .rddf/roadmap/`（排除 archive）⇒ 确认无既有 change 同时在做
- [ ] **Step 4: 提取 TLM 侧判据** —— 台账 `[cache]` 25 条 + `[MMUCacheIntegration]` 4 条的 case 名 ⇒ 作为 CH_MEM 版验收判据来源
- [ ] **Step 5: ★容量参数先定（oracle：v1 一个都没定 ⇒ agent 自选 ⇒ tag 恒真/恒假）** —— TLM 侧是**模板参数**（`ip/cache/plugins/L1CachePlugin.h:140/149` 的 `kTagBits` / `kLineDataBits` + set-indexed API `is_set_valid` / `read_tag` / `write_set`）⇒ CH_MEM 侧必须**显式定值**（容量/行宽/相联度/索引位宽/tag 比较宽度），并在 plan 中落成一张**参数表**。⚠️ **窗口约束**：CH_MEM dmem 只有 **64KB**（`dmem_chmem.h:35`，`kMemWords = 16*1024`）⇒ Bare 方案下 PAddr=VAddr ⇒ **tag 取 VAddr 高位几乎无区分度** ⇒ tag 必须包含足够区分位（如行内偏移以上的地址位 + 有效位），否则命中判定退化
- [ ] **Step 6: ★refill FSM 存废裁决（oracle：v1 未预见的关键设计题）** —— `ip/cache/plugins/l1_cache_refill_fsm_chmem.h:19-44` 的 **LOOKUP 是多拍状态机**，与 CH_MEM **全组合通路**不自洽 ⇒ 三选一（**必须显式裁决，不得含糊**）：(a) **重写**为「组合 lookup + refill-only FSM」（保留 FSM 兜底语义，lookup 改组合）· (b) **废弃**，全部并入本 change 的新实现 · (c) 保留不动（⇒ 与全组合不自洽，**不可接受**）⇒ **建议 (a)**。⚠️ 另注：该 FSM **当前无工厂注册**（`grep` 仅测试引用）⇒ 本 change 需同时裁决其**注册与归属**
- [ ] **Step 7: ★stage 名合规核对（oracle 漏查项）** —— `l1_cache_refill_fsm_chmem.h:112` 注册了新 stage 名 `"cache_refill"` ⇒ 须核对 ADR-040 §10.6「**生产者禁注册新 stage 名**（`canonical_stage_order()` 只看 stage 名首现序）」的适用性 ⇒ 若违规 ⇒ 必须改用 `declare_substage` 或与生产者同 stage 名 ⇒ **在写代码前先核对，不要踩在实现里发现**

### Task 2: MMU 解耦方案（**已决定：方案 A** —— 决策点已关闭）

- [x] **Step 1: 决策已定 —— 方案 A**（2026-10-24 用户接受建议）：只支持 Bare（PAddr = VAddr）先落地，**不被 ⑤ 阻塞**；接 Cache 命中路径随 ⑤ 完成后补
- [ ] **Step 2: Bare 语义可行性自检（方案 A 的成立条件）** —— Cache 的 tag 必须能表达「物理页标识」：Bare 下 PAddr=VAddr ⇒ tag = VAddr 的高位 ⇒ **可行**；但若 CH_MEM 侧 Cache 设计要求 tag 含「物理页号」⇒ **方案 A 不成立** ⇒ 立刻回报 architect（不得自行改走方案 B）
- [ ] **Step 3: `MMUCacheIntegration` 4 条判据的预判** —— Bare 语义下翻译环节消失 ⇒ 逐条预判为 `covered` / `partial`（差异 = 「Bare 直通，无真实翻译」）/ `not-covered`（不适用 + 显式理由）⇒ **不得**默认升级为 `covered`
- [ ] **Step 4: 记录决策后果** —— 决策与 Bare 语义边界写入 plan；架构后果（MMU 与 Cache 的接口契约）归 architect 在 ADR-088 登记

### Task 3: CH_MEM Cache 数据结构 + 替换策略

- [ ] **Step 1: 存储** —— data/tag/valid 三数组（`ch_mem`）或打包单数组；容量/行大小/相联度取自 TLM 侧配置
- [ ] **Step 2: 替换策略** —— `lru_policy` → CH_MEM 版比较链（`ch_uint` 比较 ⇒ select 树）；`no_replacement_policy` 简单直译
- [ ] **Step 3: 命中判定** —— tag + valid ⇒ 活节点（禁 `static_cast<bool>`，ADR-040 §10.5）
- [ ] **Step 4: 单元测试** —— 移植 TLM 侧 hit/miss/replace 判据

### Task 4: 与 CPU 数据通路接线

- [ ] **Step 1: 接口** —— Cache 位于 IBus（取指）与 DMem（访存）之间 ⇒ 定义 payload key（地址/命中/数据），mode-aware（`uint_t<N>`）
- [ ] **Step 2: 顺序契约** —— ADR-040 §10.6：生产者 EARLY ⇒ 消费者 NORMAL，同 stage 名；**不得**依赖注册顺序
- [ ] **Step 3: 工厂注册 + 解禁** —— `enable_cache=true` 不再 throw；注册 `L1CachePluginChmem`；**无静默退化**：配置了却未生效 ⇒ throw
- [ ] **Step 4: 回归** —— `[chmem]` + `[verilator]` 0 退化；`enable_cache=true` 的行为测试（现在期望 throw ⇒ 改后期望成功）

### Task 5: 与 refill FSM 的关系（避免双实现）

- [ ] **Step 1: 现状核对** —— `ip/cache/plugins/l1_cache_refill_fsm_chmem.h`（229 行）已存在 ⇒ 它是「refill 状态机」还是「Cache 本体」？
- [ ] **Step 2: 关系定性（★v2：结合 oracle 的发现）** —— 若 refill FSM 是本体的**子组件** ⇒ 本体直接复用它（避免两套 refill 逻辑）；但实测其 **LOOKUP 是多拍状态**，与全组合不自洽 ⇒ 若采纳 (a) 重写方案 ⇒ **必须同步改写 FSM 的 lookup 部分**，且原 FSM 的行为锚（`tests/` 中的现有 case）**必须逐条核对语义等价**（不得删测试凑绿）⇒ 若判定为**平行实现** ⇒ 需合并或明确废弃其一（**登记为决策**）
- [ ] **Step 3: 验证** —— 有 case 证明 refill 路径在合并后行为不变

### Task 6: 收口

- [ ] **Step 1: 8 门禁** —— 全绿
- [ ] **Step 2: 全量回归** —— 0 退化
- [ ] **Step 3: 台账更新** —— `[cache]` 25 条 + `[MMUCacheIntegration]` 4 条逐条重判（`not-covered` → `covered`/`partial`/`not-covered(不适用)`），每条填 CH_MEM 锚点
- [ ] **Step 4: commit**

---

## Acceptance

- [ ] AC-1: `ip/cache/plugins/L1CachePlugin_chmem.h` 存在，实现存储 + 替换策略 + 命中判定；**容量参数表已落定**（容量/行宽/相联度/索引位宽/tag 宽度），且 tag 在 **64KB dmem 窗口**下**有足够区分度**（否则命中判定退化 ⇒ 有 case 证明不同地址不误命中）
- [ ] AC-2: `cpu_factory_chmem.h` 的 `enable_cache=true` **不再 throw**，且 Cache **确实注册并生效**（有 case 证明命中路径发生）
- [ ] AC-3: 配置了 Cache 但未生效 ⇒ **fail-fast throw**（不静默退化）
- [ ] AC-4: 台账条目重判时，**「不适用」的边界必须严格**（★metis 指出 v1 的二读）：
  - **「不适用」= 「该 case 在 CH_MEM 语义下无对应物」**（例：框架级多线程 TID 机制）；
  - **「有对应物但尚未实现」= `partial`**（差异说明 = 未落地），**不是** `not-covered` / 不是「不适用」；
  - ⇒ 台账红线（`check_coverage_parity_ledger.sh`）：`not-covered` 必须有**显式手写理由**、不得为继承标记、**同一理由 ≤10 次** ⇒ 若多条判同一「不适用」理由，**条数须 ≤10**，否则须逐条差异化 ⇒ 否则**门禁直接失败**
- [ ] AC-5: `[MMUCacheIntegration]` 4 条 `not-covered` 逐条重判完毕并填锚点
- [ ] AC-6: refill FSM 与本体的关系**已显式裁决**（重写 / 废弃 / 保留三选一，**不接受含糊**）+ 其 stage 名 `"cache_refill"` 的 ADR-040 §10.6 合规性**已核对并给出结论**（违规 ⇒ 已改 `declare_substage` 或同 stage 名）+ 其原有行为锚**逐条语义等价核对**完成
- [ ] AC-7: 命中/未命中信号为**活节点**（ADR-040 §10.5）；★**命中生效的可机检锚**：命中计数 **> 0**、命中与未命中**两条分支各 ≥1 次**、命中时数据来源可区分（Cache vs DMem）
- [ ] AC-8: 8 门禁全 PASS + 全量回归 0 退化
- [ ] AC-9: `cpu_factory_chmem.h` 中失效的 owner 注释已修正为本 change；★**「MMU → Cache 的 PADDR/valid 接口契约」草案已随本 change 交付**（oracle：③ 与 ⑤ 存在前向耦合 ⇒ ⑤ 落地时 ③ 返工的风险 ⇒ 契约须在 ③ 交付时就写下）⇒ 契约内容：地址宽度 / 有效信号 / Bare(PAddr=VAddr) 与真实 PADDR 两种来源的**仲裁规则**（两者同时在分支上时的优先级）

## 风险与未知

| # | 风险 | 缓解 |
|---|------|------|
| R1 | **MMU 未接线 ⇒ Cache 无真实 PADDR 来源** | ✅ 已决定方案 A（Bare 解耦，不阻塞）；成立条件由 Task 2 Step 2 自检 ⇒ 不成立即回报 architect |
| R2 | **refill FSM 可能是平行实现** ⇒ 两套 refill | Task 5 定性；必要时合并 |
| R3 | CH_MEM 侧 Cache 的**时序形态**与 TLM 完全不同（组合式 vs 多级）⇒ 「命中延迟」语义不可比 | 台账差异说明必须写明；**不得**直接复用 TLM 的 cycle 判据 |
| R4 | `ch_mem` 的存储规模 ⇒ Verilator 生成的 Verilog 体积与仿真速度 | 沿用 TLM 侧容量配置；若过大 ⇒ 与 architect 议 |
| R5 | 与 ⑤ `chmem-ptw-walk` 的接口面（MMU → Cache 的 PADDR/有效信号）未定义 | ★**已在 AC-9 固化为交付物**（契约草案随 ③ 交付）+ 与 ⑤ **同时立项**共用一份契约 |
| R7 | ★**refill FSM 与全组合通路不自洽**（LOOKUP 多拍） ⇒ v1 未预见，可能导致 ③ 的实现路径整个改写 | Task 1 Step 6 显式裁决（建议「组合 lookup + refill-only FSM」重写）+ 原有行为锚逐条核对 |
| R8 | ★**Bare 窗口下 tag 区分度不足**（dmem 仅 64KB）⇒ 命中判定退化 | Task 1 Step 5 先定参数表 + AC-1 的「不同地址不误命中」case |

## 迁移说明

落地为 `.rddf/plans/chmem-l1cache-body.md`；建议**完整 rdd-builder P1–P3**（规模大 + 含需 architect 拍板的设计决策点）。**Task 2 决策点已关闭（方案 A）**；但 Task 2 Step 2 的 Bare 可行性自检是硬闸。

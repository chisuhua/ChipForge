---
id: backend-parity-gate-spec
status: proposed
initiative: wave5-isa-coverage-and-bp
priority: P0
phase: phase-4
theme: phase-4
version_target: v0.11.0
manual_deps: [architect-裁决-ADR-089]
review_by: 2026-11-09
---

# backend-parity-gate-spec — 双后端差分 parity 门禁的契约落盘（零代码）

> **定位**：★ **阶段 A1** —— 在写任何门禁代码**之前**，先把「什么算一致、怎么比、什么算例外、缺数据怎么办」钉死。
> **来源**：ADR-089（双后端差分 parity 门禁，✅ Accepted 2026-10-26）+ ADR-088 v1.4 §10（目标态与三段路径）。
> **工期**：0.5~1.0 人日（**纯文档，零代码**）｜ **窗口**：★ **G-A0 通过后**（★ 2026-10-26 用户裁决：「不要立即开工，等 Oracle + Metis 双审查结束后开工」）⇒ 双审对象 = **parity 架构（ADR-089 + ADR-088 v1.4 §10）+ 本路线图**。
> **路线图**：`objective-backend-parity-and-single-model` §3.1 里程碑 A1 · `feat-backend-parity-gate`。
> **同族先例**：`chmem-tlm-backend-freeze` / `chmem-tlm-coverage-parity-ledger`（治理类 change 的「先立口径再施工」模式）。

**主题**: 验证基建 — 同一模型 × 两后端差分执行的门禁契约
**优先级**: P0（★ **阶段 A 的开工前置**；★ 也是 **ADR-088 §5.2 删除判据 6** 的源头）

## Why

★ **今天两个仿真后端从不互比**：vendored ELF 47 跑 **Simulator**、Verilator 48 ELF 跑 **Verilator**，两侧**不同 corpus、不同 oracle**（都只判 `tohost==1`）⇒ **跨后端断言仅 1 条**（`[mem-drift]` 锚，只钉 `ch_mem::aread` 的 0-cycle）⇒ ★ **「两边都 tohost==1 但状态不同」的模型今天完全不可见**。

★ **而现有的「一致性证据」是人工计数代理且已在漂**：台账 463 条，头部 partial **292** vs 条目级实测 **282** ⇒ 代理指标开始骗自己，且它对「两后端是否一致」**完全无感**。

★ **先写工具后定口径的老问题已经发生过一次**：`chmem-spec-rewrite-and-doc-hygiene` 就是因为「先写工具、口径后补」而返工 ⇒ ★ 本 change 只落**契约**，零代码。

## What Changes

1. **观测量采集接口规范**（ADR-089 D2）—— 5 个观测量（`tohost` / 最终 regfile / **DUT 自算内存签名（mailbox）** / **总 cycle 数（标量）** / **逐拍 PC 流 hash**）各自：**从哪个 payload / 哪个 host 符号读**、**在 Simulator 侧与 Verilator 侧分别怎么取**、**归一化规则**（字节序 / 字段宽度 / 是否含未初始化区）；★ **③ 明确写「首选 = DUT 自算签名写 mailbox；host dump = 待上游增强项」**（依据：`Simulator::get_memory()` 是空 stub + 未启用的 `ENABLE_MEMORY_API`）；★ **⑤ 明确 PC 轨迹定义**（采样点 = fetch / commit？采样率？采样相位？）；★ **④/⑤ 的分工写清**（总拍数 vs 逐拍流；★ **MUST NOT** 用「④+末态」替代 ⑤）
1b. ★ **确定性判据**（ADR-089 D2 前置条件）—— ★ 「同一输入」的**确定性**如何判定（node_count + 顶层信号名名单 hash 两侧一致 ⇒ 不符先 FAIL）；★ **elaboration 必然是两次**（Simulator 在测试进程 / Verilog 在 runner 进程）⇒ 契约 MUST NOT 写「同一次 elaboration 的产物」
2. **对齐规则**（D3）—— ★ **逐拍相等为默认**；例外白名单的**文件格式**（★ **5 必填字段**：机制说明 / 上游状态+上游 issue 链接 / 变异对照 / **收敛期限**（≤ 1 个发布窗口）/ **corpus 作用域**）+ **扩张权**（★ **architect 审，实施者不得自行扩张**）+ **规模护栏**（条目数 > N ⇒ 强制 architect 复审；★ **MUST NOT** 用「硬上限 ⇒ 门禁红」）+ **常驻 1 条合成例外**（防机制随清零腐化）
3. **fail-closed 规则**（D2）—— 观测量缺失 / 不可采集 / 被截断 ⇒ **FAIL**；★ **MUST NOT** 跳过或降级（沿用项目既有 `dropped` / `FLUSH_COUNT` 纪律）；★ **每个观测量各一例**（≥5 例，不是「关闭一个」即可）；★ **三态退出码**（FAIL = 后端分歧 / ERROR = 门禁自身故障（TIMEOUT / runner 崩溃）⇒ **MUST NOT** 计入分歧清单）
4. **语料清单与分层**（D4）—— vendored 47 + branchbench 4 档为首批；Dhrystone 后置；★ 每层的**期望时长**与**CI 可否跑**（分层 = 快子集 / 全量夜间）
5. **报告格式** —— 门禁输出的**机器可读产物**（JSON）+ **人读摘要**；★ 「既存后端分歧清单」的**分类字段**（上游对齐 / 有意例外 / 真 bug）
6. **鉴别力验收**（V-3/V-4）—— ★ 门禁**自身**的变异对照写法（★ **≥2 例**：改 1 拍 + 改 1 字节内存写 ⇒ MUST RED）+ 例外白名单的**合规校验**（★ 5 字段任缺 ⇒ FAIL；★ 期限过期 ⇒ FAIL 且升级为真 bug 类）
6b. ★ **契约「粘住」机制** —— ★ **本契约的条款 MUST 编号化（S-1…S-n）**；★ **A2 的 tasks.md 每条任务 MUST 引用 ≥1 条 S-编号**；★ **G-A2 的检查项 MUST 含「S-编号 ⇒ 实现/测试」覆盖矩阵**（每个 S-编号至少一个实现落点，或显式 N/A + 理由）⇒ ★ **没有这一条，A1 就是下一份会漂移的文档**

⇒ ★ **本 change 不写门禁实现**（归阶段 A2a `backend-parity-observability` / A2b `backend-parity-harness`）⇒ ★ **不碰任何 `ip/**` `src/**` `tests/**` 代码**。

## Acceptance

- [ ] **AC-1** 5 个观测量的采集接口规范成文（每项含**两侧取法** + 归一化规则）
- [ ] **AC-2** 对齐规则成文：默认逐拍相等 + 例外白名单**文件格式**（★ **5 必填字段**）+ **扩张权限**（architect 审）+ **规模护栏**（> N ⇒ 复审）+ **常驻合成例外**
- [ ] **AC-3** fail-closed 规则成文（含「缺观测量 ⇒ FAIL」的**判定点**清单）
- [ ] **AC-4** 语料清单 + 分层表成文（★ 首批 = **两侧交集 47 ELF**（★ Verilator 侧另有 `rv32ui-p-simple` ⇒ 补进 Simulator 侧 1 行）+ branchbench 4 档；Dhrystone 后置；★ **两侧 corpus MUST 抽成单一清单文件**；★ 每层时长 + CI 可否跑）
- [ ] **AC-5** 报告格式成文（机器可读产物 + 人读摘要 + 分歧清单分类字段）
- [ ] **AC-6** ★ **鉴别力验收写法**成文（V-3 **≥2 例**变异对照 + V-4 例外合规校验 + **三态退出码**）⇒ ★ **MUST NOT** 只写「门禁存在」
- [ ] **AC-8** ★★ **契约「粘住」机制**成文：★ 契约条款**编号化 S-1…S-n**；★ **A2 的 tasks.md 每条任务 MUST 引用 ≥1 条 S-编号**；★ **G-A2 MUST 含「S-编号 ⇒ 实现/测试」覆盖矩阵**（每 S-编号至少 1 个实现落点或显式 N/A + 理由）⇒ ★ 目的 = 防止本契约成为下一份会漂移的文档
- [ ] **AC-7** ★ **零代码改动**（`git diff --stat` 不含 `ip/**` / `src/**` / `tests/**` / ★ **`tools/**`**）⇒ ★ 排除 `tools/**` 是为了**堵住「先写工具」的后门**（否则 A1 可以名正言顺地往 `tools/` 塞门禁脚本雏形）

## Capabilities

- **capability: backend-parity-gate** —— 新增（★ 门禁**契约**层；实现归阶段 A2）
- **capability: spec-first-gate** —— 把「先写工具后定口径」反转为「**先定口径再施工**」（沿用 `chmem-spec-rewrite-and-doc-hygiene` 的教训）

## Impact

- **ADR-089**：★ 本 change 是其细则落地 ⇒ ADR 不改（★ 只补细则文档，★ **不改 ADR 正文**）
- **ADR-088 v1.4 §5.2 判据 6**：★ 判据的**可判定来源**（差分门禁 V-1~V-6）
- **阶段 A2 / A3**：★ 本 change 的交接清单（实现者据此写代码，**不自由发挥**）
- **测试预算**：★ 零新增测试（本 change 只落契约）

## 关联

- **ADR-089**（✅ Accepted 2026-10-26）· **ADR-088 v1.4 §10**（目标态与三段路径）
- `objective-backend-parity-and-single-model`（阶段 A 路线）· `feat-backend-parity-gate`（阶段 A feature）
- `feat-tlm-backend-retirement`（阶段 B/C；B3 的 5 条硬前置（★ 第 5 条 = 门禁体系自身改版））
- `docs/architecture/tlm-coverage-parity-ledger.md`（台账；阶段 B 起重新定性为覆盖边界规格）

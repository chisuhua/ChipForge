# 后续任务全表 + 实施路径（★ planner 交接件 · 2026-11-08）

> ★ **本文件 = 交接件**（★ 现状盘点 + 路径）⇒ ★ **不是 SSOT**（★ 战略 SSOT = `strategy.md` · ★ 架构演进 SSOT = `docs/architecture/roadmap-evolution.md` · ★ 版本门禁 = `objective-*.md` · ★ 跨阶段特性 = `feature-*.md`）⇒ ★ **不新建第二套路线图**。
> ★ ★ **本文件的唯一增量**：★ ① **把 5 条并行主线摊平成一张表**（★ architect 盘点 §6 只覆盖 2 条）★ ② **给出可执行的批次顺序 + 立项触发条件**★ ③ **标出「已立项 / 未立项 / 被架构签批卡住 / 远期 defer」四态**。
> ★ ★ **口径**：★ 每条带 `file:line` 或「实测」⇒ ★ **数字为 2026-11-08 实测**。

---

## 1. ★ 五条主线全景（★ 摊平）

| # | 主线 | ★ 目标 | ★ 载体 | ★ 现状（实测） | ★ 下一动作 | ★ owner |
|---|---|---|---|---|---|---|
| ★ **L1** | ★ **文档治理**（本轮主线） | ★ 顶层 = 真相源 + 决策导航入口 + 4 道门禁 | ★ `ADR-095` · ★ **本轮新立 2 change** | ★ 顶层 **18** 份（目标 **9**）· ★ `mixed/`/`archive/`/`README.md` **不存在** · ★ 4 份已净化 | ★ **★ 立项已完成**（★ 2 个 change 已建）⇒ ★ 切 builder 执行 | builder |
| ★ **L2** | ★ **双后端门禁**（阶段 A） | ★ 同一 DAG × 两后端差分执行 | ★ `ADR-089` · `ADR-088` §10 | ★ A1 **32/32** ✓ · A2a **29/29** ✓ · A2b **25/26** · A3 **23/25** · ★ **A4 门禁接线 + CI = 未立项** | ★ **★ 立项 `backend-parity-ci`**（★ 阶段 C 硬前置①） | planner→builder |
| ★ **L3** | ★ **TLM 退役**（阶段 B/C） | ★ 删 TLM 模式 ⇒ 只留 CH_MEM 单后端 | ★ `ADR-088` §10.3 · `ADR-093` | ★ **未启动**（★ 阶段 C 的 5 硬前置含「① 门禁绿」⇒ **被 A4 卡住**） | ★ 阶段 B 可与 A4 **并行**（★ 前置④ 台账冻结 = 纯规格） | builder |
| ★ **L4** | ★ **层次化装配** | ★ 层 2 单元成独立 module ⇒ 可单独综合/交付 | ★ `ADR-094`（🚧 Proposed v0.1）· 上游 ADR-058~062 | ★ **决策已闭合**（★ oracle→critic 0 BLOCKER）· ★ **2 个 change 未立项** | ★ **★ 立项 2 change** ⇒ ★ 之后 `ADR-094` 转 Accepted | planner |
| ★ **L5** | ★ **SoC 交付**（v0.11.0 → v1.3.0） | ★ 版本节点全绿 | ★ `objective-soc-cpu-v0110-launch` 等 5 个 objective | ★ `mmu-chmem-pipeline-integration` **29/173** · `mmu-chmem-phase-b-c-launch` **10/33** · `chmem-bp-btb-gshare` **4/45**（★ defer v1.0.0）· `phase-1.5-wave-4` **0/51**（★ placeholder）· `vexii-riscv-parity-poc` **0/74**（★ v1.0.0 远期） | ★ 见 §3 的 L5 子表 | builder |

★ ★ **待办总量（★ 实测）**：★ 11 个既有 change 合计 **192/549**（★ 357 项未完成）★ + ★ **3 个被 ADR/roadmap 点名但未立项**（★ `backend-parity-ci` · `chmem-hierarchical-assembly` · `chmem-hierarchical-unit-landing`）★ + ★ **本轮新立 2 个**。

---

## 2. ★ L1 文档治理线（★ 本轮已立项 · ★ 详设见两个 proposal）

| 项 | 内容 |
|---|---|
| ★ **change A** | ★ `architecture-truth-boundary`（★ 位置 + 引用）⇒ ★ **AC-101~AC-108 + AC-116~AC-118**（★ 11 条） |
| ★ **change B** | ★ `architecture-truth-boundary-enforcement`（★ 门禁 + 净化）⇒ ★ **AC-109c + AC-110~AC-115a/b + AC-119/120**（★ 10 条） |
| ★ **依赖** | ★ **B `depends_on: [A]`**（★ 门禁① 在迁移前落地必红 ⇒ 顺序锁死） |
| ★ **AC ↔ V** | ★ **21 条 AC 的 V 并集 == V-101~V-110 全集**（★ **机检 PASS** · ★ 0 孤儿 · ★ 0 重复）★ ⇒ 拆分后 **AC-115b 只挂 B**（★ 否则 A 单独 archive 时永久卡 P3） |
| ★ **★ 实施关键（★ architect §10 未算到）** | ★ 待移动 10 份内部共 **76 处**相对链接（★ 实测）★ 其中 **2 个环** ⇒ ★ **批次 MUST 按 SCC 划分**（★ 7 批）★ ⇒ 环内同批移动 + 同批改链 ⇒ **批批可绿** |
| ★ **★ 顺序锁死** | ★ **AC-110（工具+fixture）→ AC-109c（内容）→ AC-110 终验**（★ 否则 AC-109c 与 AC-110 互为前提） |

### ★ L1 的两个**新发现**（★ 本轮 planner 实测）

| # | 发现 | 处置 |
|---|---|---|
| ★ **N-1** | ★ **76 处内部相对链接**（★ architect §11 的 AC-104 清单只列「仓外 17 + `adr/` 5 + 留守 9 + 根级 2」⇒ **一处都没进 scope**） | ★ **AC-117**（★ 新增）★ 逐条改写与核销 |
| ★ **N-2** | ★ **2 个环** ⇒ 「按文件分批」不可能批批绿 | ★ **AC-116**（★ 新增）★ SCC 分批 + 每批 evidence |

---

## 3. ★ L2/L3/L4/L5 的立项触发条件（★ ★ 本轮**不**立项 ⇒ 给出「何时可立项」的判据）

| change | ★ 立项触发条件（★ 判据可核） | ★ 前置 |
|---|---|---|
| ★ `backend-parity-ci`（★ L2 的 A4） | ★ **立即可立项**（★ 不依赖任何未完成项）⇒ ★ AC：★ 门禁接线进 `tools/run_chipforge_tests.sh` + ★ CI 具备 build 能力（★ `ADR-088` §5.2 判据 4/7） | ★ 无 |
| ★ `chmem-hierarchical-assembly`（★ L4） | ★ **立即可立项**（★ `ADR-094` 决策已闭合）⇒ ★ AC：★ root/root-scope/attribution 守卫 r.14 + provider 槽位 r.15 + 登记簿 + manifest + 两道闸 + C-1 三态化 + 阶段 1 普查 + 迁移期基线 | ★ 无 |
| ★ `chmem-hierarchical-unit-landing`（★ L4） | ★ **立即可立项**（★ 同上）⇒ ★ AC：★ 首批成层单元 + 层次化等价性判据 V-27~V-31 | ★ 无 |
| ★ 阶段 B 台账冻结（★ L3） | ★ 可作为 change（★ 名待定）⇒ ★ AC：★ 台账头部计数修正 + 冻结脚本 + 「头部计数 == 条目级实测」等式校验（★ `ADR-089` V-6） | ★ 无（★ **可与 A4 并行**） |
| ★ 阶段 C 删 TLM（★ L3） | ★ **★ MUST 等 5 硬前置全绿**（★ ① 门禁绿 → A4 · ② CI build → A4 · ③ 语义抽规格 · ④ 台账冻结 · ⑤ 门禁体系改版 → `ADR-093`） | ★ **A4** |
| ★ `chmem-bp-btb-gshare`（★ L5） | ★ **★ defer v1.0.0**（★ 已决策 2026-10-19）⇒ ★ **不立项** | ★ — |
| ★ `phase-1.5-wave-4`（★ L5） | ★ **placeholder**（★ initiative `wave4-csr-cache-dse` status = exploring）⇒ ★ **不立项**（★ 须等 wave3 archive 后回填） | ★ — |
| ★ `vexii-riscv-parity-poc`（★ L5） | ★ **v1.0.0 远期 placeholder** ⇒ ★ **不立项** | ★ — |

★ ★ **L5 内部的真缺口**（★ 排除 defer/远期后）：
| change | 缺口 | owner |
|---|---|---|
| ★ `mmu-chmem-pipeline-integration` | ★ **144 项未完成**（★ 29/173）⇒ ★ **量最大** | builder |
| ★ `mmu-chmem-phase-b-c-launch` | ★ **23 项未完成**（★ 10/33）⇒ ★ **SoC 依赖它** | builder |
| ★ `backend-parity-open-core` | ★ **21 项未完成**（★ 40/61）⇒ ★ 含 **A5-2 ELF 加载** / **A5-3 核↔从设备集成**（★ **真实缺口**） | builder |

---

## 4. ★ 实施路径（★ 关键路径 + 可并行性）

```
★ 【本轮已完成】
  ★ planner：L1 立项 2 change + wave7 initiative + strategy.md §3 登记
  ★ architect：盘点文档 + ADR-095 Accepted + 顶层 4 份净化

★ 【下一轮 · 切 builder】
  └─▶ L1-A architecture-truth-boundary（11 AC）
        └─▶ L1-B architecture-truth-boundary-enforcement（10 AC）★ depends_on A
        └─▶ L4  层次化装配 2 change（并行 · 不同文件面）
        └─▶ L2  backend-parity-ci（A4 · 并行 · 不同文件面）
              └─▶ L3 阶段 B 台账冻结（并行 · 前置④）
                    └─▶ L3 阶段 C 删 TLM（★ 5 硬前置全绿后）
```

★ ★ **三条可并行线**（★ **不构成串行瓶颈**）：★ **① L1 治理线**（`docs/` + `.rddf/config`）· ★ **② L2/L3 门禁线**（`tools/` + CI）· ★ **③ L4/L5 装配线**（`ip/cpu/core/` + 上游）⇒ ★ ★ **仅弱交叉**：★ L1-B 的门禁② 改 `doc_link_check.sh` ⇒ ★ **与 L2 的 `tools/` 改动可能撞车** ⇒ ★ **L1-B 与 `backend-parity-ci` 建议串行或分文件提交**。

★ ★ **★ 关键路径 = L2 的 A4**（★ 阶段 C 删 TLM 的硬前置①）⇒ ★ **L2 的优先级应高于 L1**（★ L1 是治理价值，L2 是**功能阻塞**）。

---

## 5. ★ 本轮 planner 的**未做**（★ 明示边界）

| 项 | 原因 |
|---|---|
| ★ **L2/L4 的 3 个 change 立项** | ★ **本轮不立**（★ metis 建议：★ builder 是单线程能力 · 排队 >2 个会让 handoff 过期）⇒ ★ 只给**触发条件**（§3） |
| ★ **L5 的层次化落点登记**（★ `objective-soc-cpu-v0110-launch.md`） | ★ ★ **实测该文件「层次化」命中 = 0**（★ 确为缺口）⇒ ★ **归 P-5**，★ 可与 L4 立项同批做 |
| ★ **既有 10 个 change 的 AC 段头统一** | ★ **归 L1-B 的 AC-120**（★ builder 面）⇒ ★ planner 只登记 |
| ★ **改已签批 ADR 的决策文字** | ★ ★ **planner 越界**（★ `ADR-095:177` 的 AC 权威口径属 architect 的 A-2）⇒ ★ **登记为 architect 待办** |
| ★ **`tasks.md` / `design.md`** | ★ **builder 的活**（★ 角色矩阵：★ planner 禁写）⇒ ★ 本轮**只出 `proposal.md`**（★ 与项目既成惯例一致：★ 11 个 active change 中 **10 个**是手写 proposal） |

---

## 6. ★ 本轮 planner 侧发现的问题登记（★ **不擅改**）

| # | 问题 | ★ 归属 |
|---|---|---|
| ★ **Q-1** | ★ `rdd-builder/scripts/generate_full_proposal.py:210` **硬编码 `## Acceptance`**（★ verifier 正则只认 `验收标准\|Acceptance Criteria` ⇒ 生成物**提取为空**）★ 且**不输出 frontmatter**（★ ⇒ `initiative:` 丢失 ⇒ `sync_strategy_status.sh:121` 抽到空 ⇒ §7 无该 change） | ★ builder（★ **本轮走内置路径 ⇒ 不阻塞**） |
| ★ **Q-2** | ★ **ADR-095 §2.2 关注度数字不可复现**（★ 四种口径 + oracle 另测五种均对不上）★ 且 `ADR-095:95`（40/22/7/2）与 `:74-79`（41/23/6/2）**同一文件自相矛盾** | ★ architect（★ A-1） |
| ★ **Q-3** | ★ **AC 权威口径冲突**（★ `ADR-095:177` + `ADR-094:13` 写「权威 = `tasks.md`」vs verifier 提取源 = `proposal.md`）★ 且既有 10 个 change 的段头不合规 | ★ architect（★ A-2）★ ⇒ ★ 本轮 **2 个新 change 已用规范段头**，★ 但 ADR 措辞待 A-2 订正 |
| ★ **Q-4** | ★ **文档治理线原本无 initiative 装**（★ 现有 `wave3~6` 均不含）⇒ ★ 新 change 在 §7 派生中**静默消失**（★ `sync_strategy_status.sh:232-246` 只遍历已有 initiative 目录） | ★ ★ **本轮已处置**（★ 建 `wave7-architecture-governance` + `strategy.md` §3 登记 · ★ drift 校验 **0**） |
| ★ **Q-5** | ★ `ADR-095:166` 写门禁①「**首轮 WARN 级**」vs 本轮 AC-110 判据写「**报红**」⇒ ★ **两者冲突** | ★ architect（★ A-2 同批）★ ⇒ ★ 本轮 AC-110 判据取「**落地即阻塞 + fixture 证明不误伤**」（★ 不用 WARN 期） |

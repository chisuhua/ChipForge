---
id: bp-spec-rewrite-and-doc-hygiene
status: proposed
initiative: wave5-isa-coverage-and-bp
priority: P1
phase: phase-4
theme: phase-4
version_target: v0.11.0
manual_deps: [architect-裁决-A1-A6]
review_by: 2026-11-09
---

# bp-spec-rewrite-and-doc-hygiene — BP 规格整体回写 v2.3 + 文档卫生收口

> **定位**：`chmem-bp-btb-gshare`（② 主体）开工前的**硬前置** —— architect 裁决 A1~A6 的落点。
> **来源**：前置2 PoC（`chmem-bp-btb-gshare-poc`）实施期实测暴露的 **5 处机制规格缺陷 + 2 处判据缺陷 + 2 处文档卫生缺口**。
> **工期**：0.5~1.0 人日（纯文档/ADR，零代码）｜ **窗口**：② 主体开工前。
> **同族先例**：`chmem-tlm-backend-freeze` / `chmem-tlm-coverage-parity-ledger`（治理类 change 的「先立口径再施工」模式）。

**主题**: 分支预测 BTB + GShare + RAS（② 主体的规格侧输入）
**优先级**: P1（**② 主体开工前硬前置**）

## ★ 裁决摘要（architect 可直接签批 —— 每项都已给出「实施侧已取的一侧」）

| # | 待裁决 | 实施侧已取（PoC 已落地并逐位验证） | 备选 | 若裁决不同的影响 |
|---|---|---|---|---|
| **A1** | `bp_spec_taken_buf` 的定义 | **「投机被证明正确」（情形 C）** | 「投机臂确实生效」（= `pv & pt`） | ★ 选备选 ⇒ **情形 F 的错路不被抑制 ⇒ 副作用泄漏** ⇒ 需同步删 F 或补 F 的独立门控 |
| **A2** | AC-F2-3 分量 1 阈值 `ΔFLUSH ≥ 0.8×FLUSH(off)` | **不适用该阈值**（用 `ΔFLUSH == Δcycles` 的阈值无关判据） | 重校准为可满足的阈值 | ★ 选备选 ⇒ PoC 的判定依据要重写（但**已落地的自动判据不受影响**） |
| **A3** | `case_c` 臂的取值 | **`bp_next`**（保留链式投机） | `pc_reg + 4`（简报 §2.1.2 字面） | ★ 选备选 ⇒ **分支 target 本身是分支时走错路 + 错路不被 flush ⇒ 泄漏**（PoC B-1 已有单元锚 R8 锁死） |
| **A4** | 「错路指令维度」 | **排除**（`!flushed` 门控） | 不排除 | ★ 选备选 ⇒ **PC 出轨 + 过度 flush ⇒ 双向泄漏**（PoC B-2 已有单元锚 R11 锁死） |
| **A5** | ② 主体的范围 | **选项③**：先扩 `is_unconditional`+`jal` 覆盖并量收益，再定 GShare | ① 扩到 GShare ② 不扩 | ★ **已由用户裁定（2026-10-26）= 选项③** ⇒ 本 change 只需落实三档增量的判据 |
| **A6** | 净收益率的**分母口径** + **84.5% 作用在哪个量纲** | ★ **未裁决** ⇒ builder **同报两种分母取小**（fail-closed） | 只报一种 | ★ **必须裁决**，否则 AC-27 的判据语义未定义（84.5% 的出处是「差距闭合率」，与净收益率**不同量纲**）|
| **A7** | 简报**整体回写 v2.3**（5 处机制缺陷 + 2 处判据缺陷）+ 同批修表格结构伤 | ★ 逐条已定位到行 | 逐条打补丁 | ★ 逐条打补丁会继续漏（已累计 4 处缺陷 ⇒ 根因是「情形清单缺两个场景维度」）|

⇒ ★ **A3 / A4 的实施侧取值已被 PoC 的单元锚锁死**（R8 / R11）⇒ ★ 若 architect 选备选 ⇒ **不是改文档，而是改 PoC 的已验证机制** ⇒ ★ 代价远高于签批。
⇒ ★ **A6 是唯一真正阻塞实现的一项**（其余五项的实施侧取值已可用）。

## 架构依据

### 缺陷一：机制规格（简报 `docs/architecture/bp-btb-gshare-design-brief.md`）

| # | 缺陷 | 后果 | PoC 证据 |
|---|---|---|---|
| 1 | §2.1.2 的 select 链写 `select(情形C, pc_reg(N+1)+4, …)` | **丢链式投机** ⇒ 分支 target 本身是分支时**走错路** + 错路指令**不被 flush** ⇒ **store/写回副作用泄漏** | B-1 / 单元锚 R8 |
| 2 | 情形清单只列 A/B/C/D/E，**无 F**（预测 taken / 实际 taken / **目标不同**） | F 的错路（被取到的投机目标本身）**不被抑制** ⇒ 泄漏 | 缺口 #10 / R6 |
| 3 | `bp_spec_taken_buf` 定义为「投机臂**确实生效**」（= `pv & pt`） | 同上 ⇒ F 下泄漏；且**不是**「投机**被证明正确**」（= `case_c`） | 缺口 #10 |
| 4 | §3.4.2 的门控理由假定「错路 = fall-through」 | **不含**「被解析指令本身在错路上」这一维 ⇒ `case_a` 在错路指令上**误触发** ⇒ PC 出轨 + **过度 flush** | B-2 / R11 |
| 5 | §3.4.2 的 flush 表述为「`br_taken_buf` 分量**替换**为 `mispredict_buf` 分量」 | 「替换」是**错的** ⇒ 未投机而实际跳转（BTB miss / 情形 B / `jalr`）的 fall-through **副作用泄漏** | PoC 门控锚 R5/R7 |

⇒ ★ 五处**全部**指向同一个根因：**情形清单是「单指令分类」，缺两个场景维度**（① 被解析指令在错路上 ② 分支 target 本身是分支）
⇒ ★ **逐条打补丁会继续漏** ⇒ ★ **应整体回写 v2.3**。

### 缺陷二：判据（`chmem-bp-btb-gshare` 的 AC-16 / AC-21）

| # | 缺陷 | 后果 |
|---|---|---|
| A6 | AC-16 的「净收益率 ≥ 84.5% ⇒ **恢复绝对值门禁**」把**两件事**混成一件 | 实测：净收益率 **56.7%**（`68044/120065`），而绝对值门禁需闭合 **84.51%**（`101470/120066`）⇒ ★ **同向但不同阈值** ⇒ 用前者直接推出后者 ⇒ **假达标** |
| A2 | AC-F2-3 分量 1 阈值 `ΔFLUSH ≥ 0.8 × FLUSH(off)` | 该阈值**在本简报既定范围内结构性不可满足**（上界 0.286）⇒ 与 AC-F2-13 自述事实**自相矛盾** |

### 缺陷三：文档卫生（可机检 ⇒ 可根治）

| # | 缺陷 | 后果 |
|---|---|---|
| #13 | `soc/cpu/docs/dse/verilator-perf-baseline-matrix.csv` **缺 `workload_sha` / `build_id` 列**（branchbench CSV **有**） | 「重录的数值只因 BP 而变」这一归因**缺机器可验证支撑**（toolchain/ELF 差异无法在脚本层排除） |
| #14 | `docs/architecture/adr.md`（113/117-122/136-142 行）与 `docs/architecture/bp-btb-gshare-design-brief.md`（296/300/304/460/461 行）存在 **GFM 表格结构伤**（行 cell 数 ≠ 表头 cell 数 / code span 内未转义管道符） | GFM 渲染时 **excess cell 被静默丢弃** ⇒ 该行的**约束与行动指令在渲染视图里不可见**（PoC 的 S3-a / S3-d 两行正是这样被 critic 抓到的） |
| #15 | ★ **简报 §5.1 的 BP 条目状态与台账冲突**（#11 `per_thread_ghr_isolation`：简报 not-covered vs 台账 partial） | 主体 AC-3「台账逐条重判」**无基线** ⇒ 重判结果不可信 |

## 范围

- **只改文档/ADR/CSV 表头**：① `docs/architecture/bp-btb-gshare-design-brief.md`（回写 v2.3）② `docs/architecture/adr/ADR-088-tlm-backend-retirement.md`（§5.3.2 的结论段）③ `docs/architecture/adr.md`（注册表表格结构）④ `soc/cpu/docs/dse/verilator-perf-baseline-matrix.csv`（加两列 + 回填）
- ★ **不改任何 `ip/**` 代码**（`is_unconditional` / RAS / GShare 属 `chmem-bp-btb-gshare` 的 Task 7）
- ★ **不改任何门禁阈值**（判据重校准由 architect 在本 change 内落结论，实施者只落实）

## Why

★ 前置2 PoC 的**实现**已落地并验证（15 个单点变异 14 RED、12 case/143 assertions、4 档端到端、vendored ELF 47/47），
⇒ ★ **规格与实现之间已有 5 处偏差** ⇒ ★ 若不先回写规格，② 主体会**照旧规格实现** ⇒ ⇒ **同类功能缺口会再次逃逸**
（PoC 已经证明：端到端两个锚对「链式投机」「错路指令」**结构性不可达** ⇒ 只能靠单元锚 ⇒ 而单元锚是照规格写的）。

★ 且**判据缺陷（A6）已经在实测里显形**：若按 AC-16 字面结案，会以「净收益率达标」为由宣布**绝对值门禁恢复**，
而实测 DMIPS = **1.34472 < 1.4** ⇒ **假达标** ⇒ ⇒ 必须先把两件事拆开。

## What Changes

1. **简报回写 v2.3**（`docs/architecture/bp-btb-gshare-design-brief.md`）：
   - §2.1.2 的 select 链：引入 `bp_next` 层（`bp_predict` 项取 `bp_next`，不是裸 `pred_target`）
   - §2.1.2 的情形清单：**补情形 F** + **补「被解析指令在错路上」维度**（一切情形判定 MUST 排除）
   - §3.4.2 的门控规则行：`bp_spec_taken_buf` = 「投机**被证明正确**」；flush = **门控**（不是「替换分量」）
   - §5 的 AC-6：同步上述三处
   - §5.3 / §5.4 的判据：AC-16 的**净收益率**与**绝对值门禁**拆成两个结论 + AC-F2-3 分量 1 的重校准
2. **ADR-088 §5.3.2 落两个结论**（净收益率结论 + 绝对值门禁结论），并把 §5.3.1 的根因行用 PoC 的实测数据补强
   （`FLUSH(off)=120065` ≈ TLM 差距 `120066`；完美预测上限 `794479` ≈ TLM `794478`）
3. **`verilator-perf-baseline-matrix.csv` 加 `workload_sha` / `build_id` 两列** + 回填当前值（⇒ 与 branchbench CSV 对齐）
4. **`adr.md` 与简报的表格结构伤修复**（用 `tools/check_md_tables.py` 机检，★ **该脚本已在本仓库** ⇒ 零新增工具）
5. ★ **把 `tools/check_md_tables.py` 挂进门禁**（`tools/doc_link_check.sh` 或 `verify_adr.sh` 的一个 Check）⇒ ★ 「以后凡改 md 表格必跑」从纪律变成**机检**

## Acceptance

- [ ] **AC-A1** 简报 §2.1.2 的 select 链含 `bp_next` 层，且**可 grep 断言**（`grep -c "bp_next" docs/architecture/bp-btb-gshare-design-brief.md` ≥ 1）
- [ ] **AC-A2** 简报 §2.1.2 的情形清单含**情形 F** 与「被解析指令在错路上」维度（各可 grep）
- [ ] **AC-A3** 简报 §3.4.2 的 `bp_spec_taken_buf` 定义 = 「投机被证明正确」；flush 判据为**门控**式
- [ ] **AC-A4** ADR-088 §5.3.2 有**两个**结论段（净收益率 / 绝对值门禁），且**互不引用为前提**
- [ ] **AC-A5** perf CSV 有 `workload_sha` / `build_id` 两列且**非空**；`tools/measure_perf.sh` 的漂移检测**消费**这两列（缺列 ⇒ fail-closed）
- [ ] **AC-A6** `python3 tools/check_md_tables.py docs/architecture/adr.md docs/architecture/bp-btb-gshare-design-brief.md docs/architecture/bp-btb-gshare-design-brief.md` ⇒ **全部 OK**（0 FAIL）
- [ ] **AC-A7** `tools/check_md_tables.py` 被**至少一个门禁脚本**调用（`grep -l check_md_tables tools/*.sh` ≥ 1）⇒ ⇒ 「改 md 表格漏跑」从纪律变成**CI 可捕获**
- [ ] **AC-A9** ★★ **简报 §5.1 的 14 条 BP 行与台账逐条状态对齐** —— ★ **实测冲突**：简报 §5.1 的 #11 `per_thread_ghr_isolation` 记「not-covered」，而台账 `:270` 同行记「**partial**」⇒ ⇒ ★ **以哪边为准 MUST 由 architect 裁定并两边同步**（★ 主体 AC-3 的「台账逐条重判」会撞上这个冲突 ⇒ **不先对齐 ⇒ 重判无基线**）
- [ ] **AC-A10** ★★ **台账头部三态计数修正** —— ★ **实测**：头部声明 partial **292**，条目级实测 partial **282**（covered 154 / not-covered 27 / 合计 463 ⇒ 与 TLM 独占 463 case 数吻合）⇒ ★ 头部 **292 ⇒ 282**（★ 台账是**主体 owner** 的文件 ⇒ 由主体改；★ 本 change 只登记，不越界改）
- [ ] **AC-A8** ★ **零代码改动**（`git diff --stat` 不含 `ip/**` / `src/**` / `tests/**`）

## Capabilities

- **capability: md-table-structure** —— 新增（仓库已有脚本，本 change 只把它挂进门禁 + 修既有伤）
- **capability: spec-defect-rewrite** —— 把「实现期发现规格缺陷」从**逐条登记**升级为**整体回写**（避免同类缺陷继续从单指令分类的缝里漏出）

## Impact

- **ADR-088**：§5.3.2 新增两个结论段（净收益率 + 绝对值门禁）⇒ ★ **§5.3.1 根因行补强**（用 PoC 实测坐实「差距 = flush 气泡」）
- **`bp-btb-gshare-design-brief.md`**：版本 v2.2 → **v2.3**（★ 5 处机制缺陷 + 2 处判据缺陷 ⇒ 一次性回写）
- **`chmem-bp-btb-gshare`（② 主体）**：AC-6 / AC-16 / AC-21 的**落点文件**即本 change ⇒ ★ 主体 MUST NOT 自行改这些 AC
- **门禁**：新增 1 条 md 表格结构检查（★ 已有的 `check_md_tables.py` ⇒ **零新增工具**）
- ★ **本 change 不改任何门禁阈值** ⇒ 阈值重校准的**结论**由 architect 落，**数值**由 architect 落，实施者只落实

## 关联

- 前置2 PoC：`openspec/changes/chmem-bp-btb-gshare-poc`（缺口 #10/#11/#12/#13/#14 + 裁决表 A1~A6）
- ② 主体：`openspec/changes/chmem-bp-btb-gshare`（`design.md` §0 v2.3 覆盖声明 + `tasks.md` v2.3 覆盖声明 + Task 7 / AC-26 / AC-27）
- ADR-088：`docs/architecture/adr/ADR-088-tlm-backend-retirement.md`（§5.3.1 根因 / §5.3.2 裁决 / P-6）
- 路线图：`feat-bp-btb-gshare`（phase-5 三档增量）/ `objective-ip-cpu-tlm-backend-retirement`（§2.1 BP 承载力实测状态）
- 工具：`tools/check_md_tables.py`（本轮新增）/ `tools/measure_perf.sh` / `tools/measure_branchbench.sh`

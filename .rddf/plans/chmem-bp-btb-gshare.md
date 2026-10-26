# ② chmem-bp-btb-gshare — 执行文件

> **change**: `chmem-bp-btb-gshare`
> **规模**: 中-大 · **前置**: ① `chmem-verilator-perf-harness` ✅ **已归档**（2026-10-24，交付点 A）· ② `chmem-branch-microbench-fix` ✅ **已归档**（2026-10-26）· ③ `chmem-bp-btb-gshare-poc` ✅ **已归档**（2026-10-26，机制 PoC 8 case / 单元锚 14 case 205 assertions 全绿）⇒ ★ **开工门禁已解除**（2026-10-26 实测确认）
> **增量分档执行路径**: `.rddf/plans/chmem-bp-btb-gshare-increments.md`（★ 三档增量 + 决策点 + 收口；**2026-10-26 v2 审查修订版**，取代外部草稿 `/home/ubuntu/.opencode/plan/02b-*.md`）
>
> ⚠️ **原 P-2 前置（`chmem-load-use-bypass`）已于 2026-10-24 作废**（实施期 PoC 证伪：禁用 CH_MEM 侧 RAW stall 后 cycles **完全不变** 914544 ⇒ load-use **不是**差距根因；真根因 = **CH_MEM 无分支预测**，见 ADR-088 v1.2 §5.3.1）⇒ **不存在待落地的 load-use 前置**。**绝对值 1.4 门禁的恢复条件 = 本 change 自身落地**（ADR-088 v1.2 §5.3.2 **P-6**：与本 change 同源）⇒ 不再是外部依赖，而是本 change 的**交付物**
> **路线图**: `feat-bp-btb-gshare`（已改绑 CH_MEM-only + 归 B1 第 5 项）· `feat-tlm-backend-retirement` B1 第 5 项
> **上游裁决**: ADR-088 v1.1 §3 BP 行（**CH_MEM-only 且必须做；不适用 §5.2「不做」出口**）+ §5.2 BP 专项删除前置（双前置）
> **被依赖**: B3 删除门禁（`branch_predictor.h` 不可删）


---

## ★★★ v2.1 覆盖声明（architect 2026-10-24，**优先级高于本文件以下全部内容**）

> ⚠️ **本文件的 Goal / Task 3 / Task 4 / Task 5.5 / AC-1 / AC-8 旧文本 / Task 6 Step 4 / Task 7 Step 3-4 已被 architect 双审后的 v2.1 简报
> （`ip/cpu/docs/bp-btb-gshare-design-brief.md`）**逐条覆盖**。**
> ★ **理由**：builder 按本文件的 checklist 执行 ⇒ 若不划掉，实施者会去实现 GShare/RAS（GShare 还会触发 PHT 体积膨胀 ⇒ 触发 Task 5.5 的新阻塞），
> 并会自行发明阈值（「阈值由 0c 微基准基线确定」是**循环表述** —— 基线给的是绝对 cycle，不是信号强度）。

### 逐条作废清单

| 位置 | v1/v2 原内容 | ★ **v2.1 裁决** |
|------|--------------|----------------|
| **Goal** | 「BTB 4K-entry + GShare 13-bit PHT + RAS 8-entry」 | ★ **BTB（256×64b）+ Bimodal 2-bit PHT（256×2b）**；★ **GShare / RAS 本期不实现** |
| **Task 3（GShare）** | 实现 13-bit 全局历史 + PHT | ★ **作废**（本期不做）⇒ 理由见简报 §3.1 |
| **Task 4（RAS）** | 实现 8-entry call/ret 栈 | ★ **作废**（本期不做）⇒ 理由见简报 §3.1 |
| **Task 5.5（PHT 2^13 体积预算）** | PHT 2^13 的 Verilog 体积/编译耗时 | ★ **作废**（PHT 改 2^9 ⇒ 体积不再是阻塞）⇒ 体积预算仍需做，但按 **256×2b** 重算 |
| **AC-1** | 「BTB 4K + GShare 13-bit PHT(2-bit) + RAS 8-entry」 | ★ 改为「BTB（256×64b）+ Bimodal 2-bit PHT」；★ **GShare/RAS 明确不在范围内** |
| **AC-2** | 注册 + canonical ordering | ★ 保留 + 追加「BP 在 **fetch 侧**的注册位已定义（ADR-048）」 |
| **AC-3 / AC-10** | 14 条台账重判 | ★ 改为**逐条表**（简报 §5.1，含 #8/#9/#10 终态写死）+ ★ **台账 §7 同步** + ★ **计数同步** + ★ **机检 exit 0** |
| **AC-8** | 「方向变化频繁的两档降幅显著更大」+「阈值由 0c 微基准基线确定」+「四档单调性」 | ★ **整体作废** ⇒ 改为简报 §4.3 的 **AC-8a / 8b-1 / 8b-2 / 8b-3 / 8c / 8d / 8e** 七条子判据；★ **阈值由 architect 在 tasks.md 开工前预登记为精确整数**；★ **禁止**百分比 |
| **Task 6 Step 4** | 两段**重复**的「中间验收」+「四档单调性 always_taken→pseudo_random」 | ★ **作废** ⇒ 以简报 §4.3 的 AC-8 为准 |
| **Task 6 Step 4 的「如 `CPUConfig` 字段」** | BP 开关面用 CPUConfig | ★ **作废** ⇒ CH_MEM 工厂**不接 `CPUConfig`**（ADR-088 §9.1 NG1-3）⇒ 改「工厂参数 + CLI flag 双层」，详见简报 §5.3 |
| **Task 7 Step 3** | 「10 条 → covered」 | ★ 改为简报 §5.1 的**逐条表**（含 4 条 `partial` 的重判方向） |
| **Task 7 Step 4** | 「① 已达成 / ② 已达成 ⇒ B3 阻塞解除」 | ★ **改为**「① 已达成；**② 未达成（CoreMark harness 属交付点 B）⇒ B3 的 BP 阻塞未解除**」+ 登记「CI 判据 4 未满足（CI 不构建）」 |
| **实施期修正 #1** | 台账出口定为 (a) 不适用 | ★ **改为**简报 §3.2 的「物理 256 固定 + 运行期 mask」⇒ 4 条 `btb_*` 条目 **covered**（须有 M=16/64/256 的 CH_MEM 侧 case） |

### ★ 新增的强制项（v2.1）

| 项 | 内容 | 出处 |
|---|---|---|
| **实施顺序** | ★ **本 change 主体不得开工**，直到两个前置 change 完成：① `chmem-branch-microbench-fix` ② `chmem-bp-btb-gshare-poc` | 简报 §4 |
| **next_pc 五级嵌套** | ★ `trap > branch-actual/mispredict > freeze(hold) > bp-predict > seq`（与 `fetch_stall_protocol` rev.14 + R11.1 协调） | 简报 §3.4.2 |
| **完整三步时序** | ★ 正确预测 ⇒ **必须抑制该分支自身的 redirect**（否则目标指令被取两次 ⇒ Δcycles≈0 ⇒ 误判机制不成立） | 简报 §2.1.2 |
| **host 可见事件计数** | 「已执行(retired)指令数」+「FLUSH 事件数」⇒ 供 AC-8b 与前置2 判据（★ **两者均归前置2** —— 需 flush 信号） | AC-15 |
| **绝对值门槛** | ★ 净命中率门槛 **≥ 84.5%**（= 101461 / 120065）⇒ ★ 若前置2 报告的可达下界 < 84.5% ⇒ **不得以「GShare 延后」结案** | 简报 §0.5 |
| **「不做什么」机检化** | `branch_predictor_chmem.h` 内不得出现 GShare/GHR/RAS 标识、不得出现 `ch_reg<ch_uint<13>>` ⇒ grep 门禁 | AC-12 |
| **builder 开工须知** | ★ 10 条（含台账 7 列护栏 / case 名零交集 / CSV 按表头名取列 / `--btb-entries` 合法域 / 三 change 边界硬门禁） | 简报 §5.5 |

### ★ 引用

- **架构方案简报（权威）**：`ip/cpu/docs/bp-btb-gshare-design-brief.md`（v2.1）
- **实施边界简报**：`.rddf/plans/chmem-bp-btb-gshare-brief.md`（⚠️ 其 §1.3「BP 开关面用 `optional<bool>`」与 §3.3「ch_mem 深度编译期 ⇒ 建议固定容量」**已被 v2.2 覆盖** ⇒ 以 v2.2 为准）
- **Task 1 的 PoC 与机制结论**：`tests/cpu/test_cpu_chmem_bp_poc.cpp`（8 case / 51 assertions 全绿）· 本文件 §Task 1.5


---

## ★★★ v2.3 + v2.3.1 覆盖声明（2026-10-26，**优先级高于本文件以下全部内容**）

> ★ **本段是 `openspec/changes/chmem-bp-btb-gshare/design.md` §0 的**快照拷贝**（builder 2026-10-26 同步）
> ⇒ ★ **若本段与 `design.md` §0 冲突 ⇒ 以 `design.md` 为准**（★ `design.md` 是 master）
> ⇒ ★ **同步清单**：`openspec/changes/chmem-bp-btb-gshare/tasks.md` 的「★ planner 交接清单」段（P-1~P-10）
> ⇒ ★ **来源**：前置2 PoC 实施期实测（5 处机制缺陷）+ A5 决策（2026-10-26 用户裁定）+ metis 实施前 gap 分析（6 blocker + 6 quality）

> **来源**：前置2 PoC（`chmem-bp-btb-gshare-poc`）的实施期实测 ⇒ **5 处机制规格缺陷** + **1 处范围决策**。
> **依据**：PoC 的 15 个单点变异对照（14 RED）+ 12 case / 143 assertions 单元锚 + Dhrystone 真实负载对照。
> ★ **本节每条都给了「实施侧已取的一侧」**（PoC 已落地并逐位验证）⇒ 实施者**照此实现**；
> ★ 但 ★ **规格裁决权归 architect**（A1 / A3 / A4 / A5 / A6，见 `chmem-bp-btb-gshare-poc/tasks.md` 的裁决表）
> ⇒ ★ **本 change 的 `tasks.md` / `design.md` 已按本节同步**（不再重复旧表述）。

### 0.1 五级链的 `bp_predict` 项 MUST 取 **`bp_next`**（★ 不是裸 `pred_target`）

```
bp_next = select( pred_valid & pred_taken, pred_target, pc + 4 )   ← 「当前取指指令 pc_reg(N) 的投机」
next_pc = select( trap_redirect,        trap_target,     ← 最高（protocol rev.14）
              select( branch_actual,     actual_target,   ← 分支实际结果（含误预测纠错）
                select( freeze_hold,     hold_pc,
                  select( bp_predict,    bp_next,         ← ★ v2.3：由 pred_target 改为 bp_next
                           pc + 4 ) ) ) )
```

★ **为什么**（PoC B-1，功能正确性）：若 `bp_predict` 项取**裸 `pred_target`**，则「抑制当前分支自身 redirect」
与「保留**下一条指令**的投机」无法同时表达 ⇒ 场景：**分支 B1（预测 taken→P、实际 taken→P ⇒ 情形 C），
而 P 处本身又是一条被正确预测 taken 的分支 B2（预测→Q）** ⇒ 裸 `pred_target` 会让 PC 落到 P+4
⇒ **Q 永不被取**（走错路）⇒ 且情形 C 使 `spec_ok=1` ⇒ **错路指令不被 flush** ⇒ **store/写回副作用泄漏**
⇒ 校验和发散。★ 该场景在 branchbench 4 档与 vendored ELF 47 个上**结构性不可达** ⇒ 端到端抓不住
⇒ 由 PoC 单元锚 R8 锁定。

### 0.2 情形清单 MUST 含 **情形 F**（★ 简报未列的规格缺口）

★ **情形 F = 预测 taken 到 P / 实际 taken 到 T（P ≠ T）**（陈旧 BTB 项、`jalr` 建项、动态目标分支）
⇒ **投机目标 P 本身是错路**（不是 fall-through）⇒ MUST 纠到实际目标 T 且 **MUST flush**
⇒ 实现上**不需要独立选择臂**：`case_c` 因「目标不一致」恒 false ⇒ 自然落到 `actual_taken` 臂 + `spec_ok=0` ⇒ flush 生效。

### 0.3 一切情形判定 MUST 排除「**被解析指令本身在错路上**」（★ PoC B-2）

★ `BRANCH_TAKEN` 是**被 flush 掩码后**的值（`branch_chmem.h`：`taken = taken & !flush`）
⇒ **错路指令**的 taken 恒读作 0 ⇒ `case_a = pv & pt & !br_taken` 里的 `!br_taken` **恒真**
⇒ `case_a` 在错路指令上**误触发** ⇒ ① PC 跳到错路的 fall-through（PC 出轨）② `mispredict_A_buf` 置位
⇒ **过度 flush** ⇒ 抑制**正确路径**的指令 ⇒ **双向副作用泄漏**。
⇒ ★ 修法：`case_a = (!flushed) & pv & pt & (!br_taken)`（`flushed` = branch plugin 的 `BRANCH_FLUSHED`）
⇒ ★ `case_b` / `case_c` 要求 `br_taken=1` ⇒ 掩码后恒 0 ⇒ **自动安全**。
⇒ ★ `BRANCH_FLUSHED` 缺失 MUST **fail-fast**（抛异常），MUST NOT 静默降级为 false。

### 0.4 FLUSH 判据是**门控**，不是「替换分量」（★ §1.2 表格末行的旧表述作废）

```
flush = hold_z | (taken_buf & !spec_ok_buf) | mispredict_A_buf
```

★ 「`br_taken_buf` 分量**替换**为 `mispredict_buf` 分量」**是错的** ⇒ 那会让「未投机而实际跳转」
（BTB miss / 情形 B / `jalr`）的 fall-through **副作用泄漏** ⇒ 校验和不等。
⇒ ★ 门控式判据在 **BP 关闭时** ≡ `taken_buf` ⇒ **零退化**（逐位相同）。

★ **`spec_ok_buf` 的定义 = 「投机被证明正确」（情形 C）**，而不是「投机臂确实生效」（= `pv & pt`）：
★ 后者在**情形 F** 下也为真 ⇒ 会让 F 的错路**不被抑制** ⇒ 泄漏。
⇒ ★ 该定义与 `ip/cpu/docs/bp-btb-gshare-design-brief.md` §3.4.2 / AC-6 的字面表述**不一致**
⇒ ★ **裁决 A1（architect）**；★ 实施侧已取 `case_c` 这一侧（更保守 ⇒ 只可能多 flush ⇒ 不可能假绿）。

### 0.5 范围修订（★ **A5 决策：2026-10-26，用户裁定**）

★ **决策**：**先扩 `is_unconditional` + `jal`/`jalr` 覆盖并量收益，再定 GShare** ⇒ 本 change 拆成
**三个可独立开关、可独立测量的增量**（见 `tasks.md` 的 **Task 7**）：

| 增量 | 内容 | 参考实现 | 本期定位 |
|---|---|---|---|
| **增量 1** | BTB 增 `is_unconditional` 位 + `jal`/`j` **建项**（恒 taken 预测 ⇒ **不占 PHT**） | ★ TLM 参考**无**该位 ⇒ **新增设计** | ★ **本期做**（AC-17 已在规格内） |
| **增量 2** | **RAS** 8-entry（`jalr` → `ret` 模式预测）⇒ 才谈得上 `jalr` 覆盖 | ★ TLM 参考**无** RAS ⇒ **新增设计** | ⏳ **先量增量 1 的收益再定** |
| **增量 3** | **GShare**（GHR XOR 索引 PHT） | ★ TLM 参考**有**（`branch_predictor.h` 的 `gshare_predict` / `gshare_update` / `global_history_`，`GHR_BITS=8`）⇒ **移植**（★ **不是新设计** ⇒ 成本远低于先前假设） | ⏳ **先量增量 1(+2) 的收益再定** |

★ **决策规则（可判定 —— 2026-10-26 v3 修订，D9 待签批）**：★ **推进判据 = Dhrystone `cycles ≤ 813074`（整数比较）= DMIPS/MHz ≥ 1.4**
- 推导（**不依赖 A6**）：`DMIPS/MHz = 2000×1e6 / (1757×cycles) ≥ 1.4` ⇒ `2e9/(1757×1.4) = 813074.2337` ⇒ **整数阈值 813074**（★ `dmips@813074 = 1.400000`、`dmips@813109 = 1.399940` ⇒ ★ **MUST 用整数比较，不用 `dmips_per_mhz` 的 `%.5f` 打印值**）
- ★ **三口径数值重合**：AC-16 的差距闭合率精确口径 `101470/120066 = 84.5119%` ⇒ `914544 − 101470 = 813074` ⇒ **「84.5%」与「DMIPS 1.4」是同一条线** ⇒ ★ A6 的分母之争**不影响推进判定**（只影响报告口径）
- ★ 实测锚（2026-10-26，增量 1 定稿）：`cycles = 820513` · `dmips_per_mhz = 1.38731` ⇒ 距阈值 **7439 拍（0.91%）** ⇒ 增量 3（GShare）须再省 ≥ 7439 拍
- ★ **净收益率（84.5%）降为报告项**：其分母口径仍归 **A6**（`N_taken_total` vs `FLUSH(off)` 同报取小 ⇒ AC-27）⇒ ★ **AC-16 的勾选仍 MUST 等 A6**，但**增量分档的推进不再被 A6 阻塞**（两者 MUST 分两件事落）

### 0.7 ★ v2.3.1 增量（2026-10-26，metis 实施前 gap 分析的产物 —— **同样优先于以下全部内容**）

★ **metis 找出 6 个 blocker + 6 个 quality**，其中**规划侧已落**的部分：

| # | 发现 | 落点 |
|---|---|---|
| B2 | **增量 1 的「无条件」信号无来源**（`branch_chmem.h:201-203` 算出 `is_jal`/`is_jalr` 但未导出）⇒ 且 **owner 表漏 `branch_chmem.h`** ⇒ 主体改它无授权 ⇒ 撞三 change 边界硬门禁 | `tasks.md` owner 表 **新增一行** + §3 payload 契约 **新增行**（本节上方） |
| B3 | **「重写 vs 增量扩展」自相矛盾**（owner 表两行直接打架） | `tasks.md` owner 表取「**增量扩展**」+ **AC-28**（保留声明） |
| B4 | **「独立开关」的实现机制未钉** ⇒ builder 自由发挥（编译期宏 ⇒ 重编译 ×3 + 宏矩阵爆炸；运行期可重配 ⇒ CH_MEM 静态 elaboration 下白付硬件） | `tasks.md` **Task 7 Step 5b**：工厂布尔 + CLI flag（先例 `enable_bp`） |
| B5 | **AC-22 的 grep 断言会被「假 trap 项」骗过**（注释里的 `trap_redirect` 也命中） | §1.3 **v2.3.1 收紧** + **AC-22**（结构 pattern + 精确形态 + 惰性锚） |
| B6 | **开工门禁措辞**与「前置已实测完成」冲突（两个前置 change 仍未 archive） | `tasks.md` 定位段：**机制落地 + 单元锚全绿即可开工**；archive 仍 MUST 在本 change 归档前完成（三要素齐备） |
| Q3 | **Step 4b 的「~80%」是点估计** ⇒ 预测式数字腐烂的温床 | **已删**：改为「**静态上界** ⇒ 实测低于它 NOT 判 FAIL」 |
| Q4 | **Task 7 Step 1 的「62b 位宽须重算」是陈旧措辞** —— 实测 62b **已含** `is_unconditional`（bit[3]）⇒ 真正的缺口 = **ch 写路径 `:417` 未写该位** ⇒ host/ch 双实现漂移 | **Task 7 Step 1 已改写** + **AC-29**（④ 逐位相等断言） |
| Q5 | **AC-27 不 fail-closed**（分母口径归 A6 未裁决 ⇒ 单负载的 fail-open 论证） | **AC-27**：两种分母**同报取小** + **A6 兜底三要素**（触发/落点/时限） |
| Q8 | **终局兜底缺失**（三档全用尽仍不达标 ⇒ 归档路径被自己的 AC 堵死） | **Task 7 Step 6 + AC-16**：按 **P-3 临时口径**结案 + 移交后续 change ⇒ MUST NOT 不可归档 |
| Q11 | **`tools/check_md_tables.py` 是孤儿工具**（不在任何 CI / gate / AC-9 清单里）⇒ 「写了工具不接线 = 假护栏」 | **AC-9** 新增第 9 条门禁 + 须知 #1 |
| ★ | **v2.3 覆盖声明未传播到执行文件**（`.rddf/plans/chmem-bp-btb-gshare.md` 顶部只有 v2.1）⇒ ★ builder 的执行 SSOT 是 **plan 文件** ⇒ ★ **MUST 先同步** | ★ **plan 文件归 builder**（planner 不越界起草）⇒ 同步清单见 `tasks.md` 的「★ planner 交接清单」段 |

### 0.6 场景维度**尚未完备**（★ MUST NOT 声称「场景覆盖完备」）

★ 情形 A~F 是**单指令分类** ⇒ **不含**两个已证实会造成功能缺口的维：
① 「被解析指令本身在错路上」（0.3）② 「分支的 target 本身又是一条分支」（0.1）。
⇒ ★ 本 change MUST 把**链式投机**与**错路指令**列为**必测场景**（端到端负载结构性不可达 ⇒ 只能靠单元锚）。

---

---

## Goal

在 CH_MEM 后端实现**分支预测器**：`branch_predictor_chmem.h`（**BTB 256×64b + Bimodal 2-bit PHT 256×2b**），
注册进 `cpu_factory_chmem.h`，并用 ① 的 harness 产出**可量化的净收益**（`Δcycles(bp=off − bp=on)`）。

> ★ **已作废的 v1 范围**（不再实施）：`GShare 13-bit PHT` / `RAS 8-entry` / `BTB 4K-entry` ⇒ **以架构简报 v2.1 §3.1–§3.3 为准**。

> ⚠️ **TLM 版 `branch_predictor.h`（319 行）在本 change 全程保留** —— ADR-088 §5.2 BP 专项：删除需**双前置**（① CH_MEM 版存在**且已注册** ② CoreMark harness 在 `cpu_verilator_sim` 跑通并产出实测值）。

## Architecture

**CH_MEM-only 新实现**（不从 TLM 版逐行翻译 —— TLM 版是 C++ 类 + 私有成员，CH_MEM 侧需按 ADR-040 的 `ch_uint` / payload key 模态重写）：

```
ip/cpu/plugins/branch_predictor_chmem.h   (新)
  ├─ BTB   : 256 entry × 64b = {tag30 = PC[31:2], target30 = target[31:2],
  │                                 valid(1b), is_unconditional(1b)}  ⇒ ch_mem 承载
  │           ★ 物理深度 256 编译期固定；逻辑条目数 M 运行期 mask (literal(M-1))
  ├─ PHT   : 256 entry × 2-bit saturating counter（Bimodal，索引 = PC[31:2] & mask）
  │           ★ 饱和语义 0→1→2→3（不回退）；≥2 ⇒ 预测 taken
  └─ 决策入口: fetch NORMAL 以 pc_reg 做 **0 拍**查表 ⇒ 产出
               (pred_target, pred_taken, pred_valid) 三路 payload
  └─ misprediction recovery: flush 语义从「上拍 taken」改为「预测错误」
               （★ hold_z 分量保留，见简报 §3.4.2 五级 mux 嵌套）

ip/cpu/plugins/ibus_chmem.h   (改)
  └─ update_pc: next_pc 五级嵌套
     trap > branch-actual/mispredict > freeze(hold) > bp-predict > seq

ip/cpu/cpu_factory_chmem.h   (改)
  └─ 注册 BranchPredictorPluginChmem<T>（fetch 侧；canonical ordering 需核对 ADR-048）
  └─ BP 开关面: 普通 bool enable_bp = true + optional<uint32_t> btb_entries
     ★ 不是 CPUConfig 字段（CH_MEM 工厂不接它）
```

> ★ **已作废的 v1 Architecture**（不再实施）：`BTB 4096 entry` / `GShare 13-bit + 2^13 PHT` / `RAS 8-entry` ⇒ **以架构简报 v2.1 §3.2–§3.4 为准**。

**D4 / ADR-040 v3.0 硬约束**（`tools/check_plugin_portability.sh` 13/13 + `tools/verify_plugin_decision.sh` 8/8 会机检）：
- 无 `void tick()`（`PluginBase::tick() = delete`）
- 无状态机（禁 `enum class State` + `switch(state_)`）⇒ **BTB/PHT/RAS 必须是无状态组合 + 有状态存储**（`ch_reg` / `ch_mem`），**不得**写 FSM；多周期需求走 ADR-046 豁免（`CF_PLUGIN_USE_FSM_EXEMPT`）但**本 change 优先不用**
- Bundle 字段用 `cf::plugin::uint_t<N>`（CH_MEM 下 = `ch_uint<N>`），**禁**裸 `bool`/`uintN_t`（ADR-040 §10.5：裸类型 ⇒ cell 只能装 elaboration 期常量 ⇒ 信号退化为常量 ⇒ 测试全绿但语义为零）
- at_stage 回调内无 `if (cond) return;` 早返
- at_stage 内禁运行期 `if(ch_bool)`（`ch_bool::operator bool()` 静默取 false ⇒ 靠 CI grep 兜底）
- payload 生产者必须直写 ch 节点（`ch_uint<1>(<ch_bool 表达式>)` ⇒ 复用上游 `bool_to_uint` 分支 ⇒ 同一 `impl()` 指针）

## Tech Stack

C++23 · CppHDL（`ch_uint` / `ch_reg` / `ch_mem` / `ch_state_machine`）· Catch2 · 零 TLM 代码改动

---

## Tasks

### Task 1: 事实复核 + TLM 版语义提取（不写 CH_MEM 代码）

- [x] **Step 1: 读 TLM 版** —— `ip/cpu/plugins/branch_predictor.h`（319 行）+ `.cpp`（23 行）⇒ 提取**可测语义**：BTB 索引/替换策略/tag 比较、GShare 13-bit 历史移位与 PHT 2-bit 饱和计数、RAS 8-entry push/pop、misprediction 时的恢复动作
- [x] **Step 2: 提取 BP 判据全集（★v2 修正：v1 误引台账 §7 且漏 4 条）** —— BP 相关条目共 **14 条**，分布在台账 **§4 条目区**（不是 §7 裁决表）：
  - **10 条 `not-covered`**（§4 行 264–266, 271–274 + 行 349, 351, 357）：`no_predict_initially` · `predict_after_update` · `btb_entry` · `custom_btb_size` · `btb_runtime_16` · `btb_runtime_64` · `btb_runtime_256` · `d2_branch_predictor_multi_thread` · `d4_branch_predictor_tid_param` · `branch_predictor_set_tid_stores_tid`
  - **4 条 `partial`**（§4 行 267–270，**v1 完全漏掉**）：`not_taken_returns_zero` · `multiple_addresses` · `reset` · `per_thread_ghr_isolation`
  - ⇒ **14 条全部要重判**（10 条降级 + 4 条升级方向）；行号以台账实际为准，实施时 grep 复核（pattern: `^\| \`(btb_|predict|no_predict|not_taken|multiple_addresses|reset|per_thread_ghr|d2_branch_predictor|d4_branch_predictor|branch_predictor_set_tid)`）
- [x] **Step 3: 确认 canonical ordering** —— BP 在 fetch 路径 ⇒ 查 ADR-048 的注册规范序 + `cpu_factory_chmem.h` 现有 8-plugin 顺序 ⇒ 定 BP 的注册位（**必须**用 `tools/verify_plugin_decision.sh` 验证）
- [x] **Step 4: 确认 ① 已归档** —— ①（`chmem-verilator-perf-harness`）✅ 已归档 2026-10-24（基线 CSV 入库 + harness ctest PASS）⇒ **本 change 的功能实现可立即开工**；★但 **性能验收（Task 6 Step 4）另需**：`chmem-branch-microbench` 基线（增益度量）+ P-2 落地（绝对值判据）

### Task 1.5: ★实施期差距表 + 机制结论（2026-10-24 实测产出）

> **本节是 Task 1 的交付物**（plan 要求「输出一张表… 无表 ⇒ 不进实施」）。
PoC 代码：`tests/cpu/test_cpu_chmem_bp_poc.cpp`（**8 case / 51 assertions 全绿**，
> 归属 `chipforge_tests_chmem` binary）。

#### 差距表（现有 `PtWalkFsmPlugin` / `branch_predictor.h` → ADR-085/086 目标架构）

| 现有实现位置 | ADR-085/086 条款 | 差距描述 | 处置 | 落到 Task |
|---|---|---|---|---|
| `ip/cpu/plugins/mmu_ptw_chmem.h`（**不在 BP 范围**，仅记录） | D01/D04/D10/D18 | 裸 `ch_state_machine` DSL / 违规目录 / 无 PTWCache | **不属本 change** | ⑤ 批 1/2 |
| `ip/cpu/plugins/branch_predictor.h:150` `BranchPredictorPlugin`（**TLM 侧**，319 行） | ADR-088 §3 BP 行 | CH_MEM 侧**完全不存在**（符号数 0）⇒ **不是「移植」而是「CH_MEM-only 新写」** | **新写** `branch_predictor_chmem.h`（不复用 TLM 类的私有成员结构） | Task 2/3/4 |
| `branch_predictor.h:85` `kNumThreads`（TLM 模板参数） | ADR-040 §10.5 | CH_MEM 侧**无框架级多线程 TID 机制** ⇒ 3 条 TID 台账条目**无对应物** | **不实现**（台账显式 `not-covered` + 理由「CH_MEM 侧无框架级多线程 TID 机制」） | Task 7 Step 3 |
| `branch_predictor.h:48` `btb_entries`（运行时构造参数） | brief §3.3 | `ch_mem` depth 是**编译期模板参数** ⇒ 运行时改表项数**无载体** | **固定容量 + 运行时逻辑条目数**（读 0 ⇒ 未命中，PoC P-4 已证） | Task 2 Step 1 |
| —（新） | ADR-040 §10.6 | 生产者/消费者顺序契约 | BTB/PHT 更新走 **payload 驱动**（生产者写 payload，消费者 `bits<>` 派生地址） | Task 2/3 |
| —（新） | ADR-046 | 多周期协议引擎豁免 | **不需要**：BTB/PHT 全是**组合读 + 状态存储**，**无 FSM** ⇒ **不写 `CF_PLUGIN_USE_FSM_EXEMPT`** | Task 2 |

#### ★ PoC 机制结论（7 条，**对实现的硬约束**）

| # | 结论 | PoC 锚 | 对实现的约束 |
|---|---|---|---|
| 1 | **C++ 侧常量 ⇒ elaboration 期常量**（ADR-040 §10.5 的机制根源） | P-5 / P-5b | 预测方向/目标/写使能**必须**是 ch 节点，**禁** `static_cast<bool>` |
| 2 | **mem 端口地址必须来自「已被驱动」的信号** —— payload 派生**正确**（含逐拍变化）；**未被驱动**的 `ch_reg` proxy ⇒ 索引解析成 **0**（写到**错误表项** ⇒ 静默错乱）；★但**被 op 节点 `<<=` 驱动**的 reg（生产 `pc_reg <<= select(...)` 正是此形）**完全可行** | **P-2N（负例，已钉死）** / **P-2P（补测，关键）** / P-2 | BTB/PHT **写**地址 = `bits<...>(payload PC)`（payload 驱动）；BTB **读**地址 = `bits<...>(pc_reg)`（`pc_reg` 被驱动 ⇒ 可用）|
| 3 | **读/写共用同一地址表达式** | P-2 | BTB 读端口与写端口用同一 `bits<>` 表达式 |
| 4 | **enable 可以是 reg proxy**（与 addr 限制不同） | P-2B | 写使能可用 payload 里的真实 `BRANCH_TAKEN` |
| 5 | **2-bit PHT 可行** | P-3 | `ch_mem<ch_uint<2>, N>` 承载 2-bit 饱和计数器 ⇒ 体积不膨胀 |
| 6 | **越界索引读 = 0** | P-4 | 「固定容量 + 运行时逻辑条目数」成立 |
| 7 | **`ch_reg` 跨拍保持成立**（1→3→7） | P-1 | GHR 13-bit 全局历史用 `ch_reg` 可行（brief §5.2 P-1 待验项**通过**） |

#### ★ 分支信号源已核实（BP 更新的 payload 输入全部存在）

| BP 需要 | CH_MEM 现成来源 | 出处 |
|---|---|---|
| 分支 PC | `KT::PC`（branch stage） | `branch_chmem.h:167` `pc_val = n->operator()(KeyType::PC)` |
| 真实方向 | `BranchPlugin::BRANCH_TAKEN` | `branch_chmem.h:236` |
| 目标地址 | `RvKey::BRANCH_TARGET` | `branch_chmem.h:237` |
| PC-mux 接入点 | `pc_reg <<= select(br_taken, br_target, pc+4)` | `ibus_chmem.h` `update_pc`（`branch` LATE） |
| **预测路径的 PC** | `pc_reg`（**被 op 节点 `<<=` 驱动** ⇒ proxy 正常跟踪 ⇒ 可作 mem 读地址） | PoC **P-2P 通过** |

⇒ **BP 可落地**：在 fetch 期用 BTB 预测目标，产出 `predicted_target` / `predicted_taken`
payload；在 branch LATE 参与 PC-mux；branch stage 的真实信号驱动 BTB/PHT 更新。

### Task 2: BTB 实现（CH_MEM）

- [ ] **Step 1: 数据结构（★v3：256 entry + 62b 打包）** —— entry = `ch_mem<ch_uint<64>, 256>` 单表单 word：
      `{tag30 = PC[31:2], target30 = target[31:2], valid(1b), is_unconditional(1b)}` = **62 bit**（余 2 bit）。
      ★ 依据：**CH_MEM 只解 32 位指令**（`decode_chmem.h:82` 的 `instr_format` 无 16 位格式）⇒ `PC[1:0]=0`、target 4 字节对齐 ⇒ 低位恒 0 ⇒ 可安全裁掉。
      ★ **物理深度 256 编译期固定**；**逻辑条目数 M 运行期 mask**（`bits<7,0>(pc) & literal(M-1)`，M ∈ 16/32/64/128/256）。
      ★ **`is_unconditional` 位**：无条件跳转（`jal`）不占 PHT 表项 ⇒ 若无此位则 `jal` 的方向无来源 ⇒ **永不被预测** ⇒ call 气泡不可恢复（oracle 复审发现 2）。
- [ ] **Step 2: 索引函数（★v3）** —— ★ **256 entry（编译期物理深度）** ⇒ 索引取 PC 低 **8 bit** 后再 **AND `literal(M-1)`**（M = 运行期逻辑条目数，M ∈ 16/32/64/128/256）⇒ ★ **MUST NOT** 用 `bits<N-1,0>(PC)` 且 N 运行时可变（`bits<>` 边界是**编译期**模板参数 ⇒ 不可实例化）；tag = `PC[31:2]`（30b）
- [ ] **Step 3: 命中判定 + 未命中行为（★v3）** —— 命中 = `valid && tag30 == PC[31:2]`；★ **未命中 ⇒ 不预测**（不跳转、不 flush、不改 PC）⇒ ★ **MUST NOT** 写成「static predict fallback」—— v1 的该措辞与主体 tasks AC-4（「BTB miss ⇒ **不预测**」）**直接矛盾**
- [ ] **Step 4: payload key 模态** —— 预测结果（target / direction）用 `cf::plugin::uint_t<N>` key；方向信号必须是**活节点**（`ch_uint<1>(<ch_bool 表达式>)`）⇒ **不得** `static_cast<bool>`
- [ ] **Step 5: 单元测试（★v2：AC-3 出口写死，避免 agent 编造 case）** —— CH_MEM 是**静态 elaboration**（无运行时重配置面）⇒ `custom_btb_size` / `btb_runtime_{16,64,256}` 这类「运行时改表项数」判据在 CH_MEM 侧**无对应物**。**出口二选一（必须显式选一个，不得含糊）**：
  - (a) **不适用** —— 判「CH_MEM 静态 elaboration 无运行时重配置面」⇒ 台账标 `not-covered` + **显式手写理由**（⚠️ 台账红线：`check_coverage_parity_ledger.sh` 要求 not-covered 必须有显式理由、不得为继承标记、**同一理由 ≤10 次** ⇒ 这 4 条共享 1 个理由 = 4 ≤ 10 ✓ 合规）
  - (b) **改成编译期多实例** —— 4 个不同容量参数的模板实例各测一次 ⇒ 判 `covered`
  - ⇒ **建议 (b)**（保留可测性）；若 (b) 与 `ch_uint<N>` 静态宽度冲突 ⇒ 选 (a) 并显式登记。**无论哪种，都必须同步台账 §4 与 §2 族级表**

### Task 2b: ★ Bimodal 2-bit PHT 实现（v2.2 新增 —— v1 缺此实施步骤）

- [ ] **Step 1: 表结构** —— `ch_mem<ch_uint<2>, 256>`（PoC P-3 已验证 2-bit 可行 ⇒ 体积 512B）
- [ ] **Step 2: 索引** —— `bits<7,0>(PC) & literal(M-1)`（★ **Bimodal 语义，无全局历史 XOR** ⇒ GShare 不在本期）
- [ ] **Step 3: 饱和语义** —— 计数 **0→1→2→3（不回退）**；预测跳转阈值 = **计数 ≥ 2**
- [ ] **Step 4: 写使能（★关键）** —— ★ **「分支已解析」（任何方向）**，MUST NOT 仅在实际跳转时更新（否则学不到「不跳转」⇒ 在以不跳转为主的负载上恒预测跳转 ⇒ **净负收益**）
- [ ] **Step 5: 更新地址（★关键）** —— ★ **分支解析处的 `KT::PC`**（= 预测时的 `pc_reg`）⇒ 二者必须**同指一条分支**（off-by-one ⇒ 学到错误表项）
- [ ] **Step 6: 无条件跳转不占表项** —— `jal`/`jalr` 不更新 PHT（恒跳转 ⇒ 无需方向学习）

### Task 2c: ★ fetch NORMAL 预测 + payload 产出（v2.2 新增）

- [ ] **Step 1: BTB 0 拍查表** —— ★ **必须 `aread`（0 拍），MUST NOT 用 `sread`**（sread ⇒ 预测与真实分支结果同拍到达 ⇒ 价值归零）；★ 读地址 = `bits<7,0>(pc_reg) & literal(M-1)`（`pc_reg` 被 `<<=` 驱动 ⇒ proxy 可用，PoC P-2P 已证）
- [ ] **Step 2: 产出 payload** —— `pred_target`(32b) / `pred_taken`(1b) / `pred_valid`(1b) ⇒ mode-aware `uint_t<N>`（禁裸 `bool`，ADR-040 §10.5）
- [x] **Step 3: 方向合成** —— `pred_taken = pred_valid && (is_unconditional || pht_counter ≥ 2)`（★ `is_unconditional` 位必需 ⇒ `jal` 才可预测）★ **2026-10-26 已完成（增量 1）** ⇒ 见下三条实现细节
  - ★ **v2.3.1 增量 1 的三处实现细节**（★ **62b 编码已含 `is_unconditional`（bit[3]，PoC `branch_predictor_chmem.h:160-163`）⇒ NOT 加位、NOT 重算位宽**）：① **branch plugin 导出 jal 判别 payload**（`branch_chmem.h` 的 `is_jal` ⇒ 新增 `IS_JAL` / `IS_UNCOND`；★ `jalr` MUST NOT 含在内）；② **ch 写路径 MUST 补写 `is_unconditional` 位**（★ PoC `:417` 的 `packed_w = tgt_shifted | pc_shifted | valid_at` **未写该位** ⇒ 与 host 侧 `pack_entry`（`:210-218` 写了）**语义漂移**）；③ **BTB 写使能**从 `taken & is_branch & !flushed` 扩为「`taken & !flushed & (is_branch | is_jal)`」（★ `jalr` 仍不建项）
- [ ] **Step 4: 存预测记录** —— `ch_reg` 组（pred_taken / pred_target / pred_valid）⇒ 跨拍保持（PoC P-1 已证）

### Task 2d: ★ branch LATE mux + FLUSH 判据改写（v2.2 新增 —— ★ **v1 完全缺失 ⇒ 不做则功能必坏**）

- [ ] **Step 1: `ibus_chmem.h` 的 PC-mux** —— ★ **5 臂显式 select 链**（情形 A/B/C + actual + seq），完整伪码见 `openspec/changes/chmem-bp-btb-gshare/design.md` §2.2 ⇒ ★ **MUST NOT** 只写 3 臂（照抄会让 actual 优先于情形判定 ⇒ 正确预测的 target 被取两次 ⇒ Δcycles≈0 ⇒ 误判「机制未生效」）
  - ★★ **v2.3 修订（★ **本 Step 的旧文本「**四情形** select 链」已作废**）**：情形清单 MUST 是 **A/B/C/D/E/F 六情形 + 两个场景维度**：① **情形 F**（预测 taken / 实际 taken / **目标不同**）⇒ 纠到实际目标 + **MUST flush**；② ★ **`bp_next` 层**（`bp_predict` 项 MUST 取 `bp_next = select(pred_valid & pred_taken, pred_target, pc+4)`，**不是裸 `pred_target`**）⇒ ★ 否则**丢链式投机** ⇒ 分支 target 本身是分支时**走错路 + 错路指令不被 flush ⇒ 副作用泄漏**（PoC B-1 / 单元锚 R8）
  - ★★ **v2.3 修订（错路指令维度）**：★ 一切情形判定 MUST 排除「**被解析指令本身在错路上**」⇒ `case_a = (!flushed) & pv & pt & (!actual_taken)`（`flushed` = branch plugin 的 `BRANCH_FLUSHED`）⇒ ★ 缺失 MUST **fail-fast**（抛异常）⇒ 否则 `case_a` 在错路指令上**误触发** ⇒ PC 出轨 + **过度 flush**（PoC B-2 / 单元锚 R11）
- [ ] **Step 2: 情形臂用「存预测」，投机臂用「fresh payload」** —— ★ 两者 MUST NOT 混用（存预测是 1 拍前的 ⇒ 用于情形判定；fresh 是本拍的 ⇒ 用于投机）
- [ ] **Step 3: ★ FLUSH 判据（`ibus_chmem.h:167`）** —— `flush = hold_z | (taken_buf && !bp_spec_taken_buf) | mispredict_A_buf` ⇒ ★ **BP 关闭时与现状逐位相同（零退化）** ⇒ ★ **MUST NOT** 把 `br_taken_buf` 分量替换为 `mispredict_buf` 分量（⇒ BTB miss / 情形 B / `jalr` 的 fall-through 副作用泄漏 ⇒ 校验和不等）
  - ★★ **v2.3 修订（门控 vs 替换 + `spec_ok` 口径）**：★ 判据是**门控**（`taken_buf & !spec_ok_buf`）⇒ ★ **不是**「替换分量」；★ **`spec_ok_buf` = 「投机**被证明正确**」（情形 C = `pv & pt & actual_taken & (pred_target == actual_target)`）** ⇒ **不是**「投机臂**确实生效**」（= `pv & pt`）⇒ ★ 后者在**情形 F** 下也为真 ⇒ F 的错路不被抑制 ⇒ 泄漏 ⇒ ★ **裁决 A1**（实施侧已取 `case_c` 一侧）
- [ ] **Step 4: `bp_spec_taken_buf` 的产生** —— 投机跳转臂**确实生效**时置位（1 拍延迟 ⇒ `ch_reg`）⇒ ★ **MUST NOT** 用「组合路径（当拍即知）」：错路指令的副作用发生在**下一拍**
- [ ] **Step 5: 三个消费方一致** —— 分支决策 `branch_chmem.h:233` / 存储写回 `dmem_chmem.h:129` / 寄存器写回 `reg_file_chmem.h:222` MUST 全部按新判据

### Task 2e: ★ BP 开关面（v2.2 新增）

- [ ] **Step 1: 工厂参数** —— `bool enable_bp = true`（★ **普通 bool**，不用 `optional<bool>`：与 `enable_mmu`/`enable_cache` 的三态语义不同，避免 nullopt 与「显式关闭」撞车）+ `optional<uint32_t> btb_entries = nullopt`
- [ ] **Step 2: CLI flag** —— `--no-bp`（默认开）+ `--btb-entries N`；★ **MUST NOT** 保留 `--enable-bp`（与默认开语义冗余 ⇒ 消除「两者同给」的优先级歧义）
- [ ] **Step 3: 生效值回显** —— runner 在 **stderr** 回显 `BP=on/off` + `btb_entries=N` ⇒ 测量脚本据此断言与请求一致（否则「flag 未生效却测出无差异」的假绿通道开着）
- [ ] **Step 4: 参数转发完整** —— `build_cpu_with_elaborate`（`cpu_factory_chmem.h:451-458`）MUST 转发**全部 4 个**参数（现状已丢 3 个）⇒ ★ **MUST NOT** 用「登记为 known gap」当逃生口


### Task 2f: ★ GShare 移植（★ v2.3 新增 —— **增量 3**，★ **移植**：TLM 参考**已有**）

- [ ] **Step 1: 移植来源** —— ★ **2026-10-26 由「条件性」转为「本期做」**（★ 实测：增量 1 后 `cycles = 820513` / `dmips = 1.38731` ⇒ **未达 DMIPS 1.4**（阈值 = `cycles ≤ 813074`）⇒ AC-16 的「不达标 ⇒ MUST 拉回 GShare」分支被触发；★ 还差 **7439 拍 ≈ 0.91%**）⇒ ★ TLM 参考 `ip/cpu/plugins/branch_predictor.h` 的 `gshare_predict` / `gshare_update` / `global_history_`（`GHR_BITS=8` / `GSHARE_SZ` 表）⇒ ★ **移植**（★ **不是新设计** ⇒ 成本低）⇒ ★ **历史文档写的「13-bit」与代码不符 ⇒ 以代码为准**
- [ ] **Step 2: CH_MEM 侧的历史寄存器** —— `ch_reg<ch_uint<8>>` 跨拍保持（★ PoC 已证 `ch_reg` 可跨拍保持）⇒ 索引 = `(pc ^ ghr) & literal(M-1)`
- [ ] **Step 2b: ★★ 索引时序对齐（2026-10-26 审查新增 —— BLOCKER，D10 待签批）** —— ★ 预测在 **fetch NORMAL** 用 `pc_reg` 本拍值（`ibus_chmem.h:260` 的 `BP_PC`），更新在 **branch NORMAL** 用 `pc_lag` = **上一拍** `pc_reg`（`ibus_chmem.h:249`；时序见 `ibus_chmem.h:149-150`）⇒ **同一条分支的预测与更新相差 1 拍** ⇒ GHR 在此期间已移位（`branch_predictor_chmem.h:592-593`）⇒ ★ **更新的表项 ≠ 预测读的表项** ⇒ **GShare 学不到**（甚至被污染成随机 true ⇒ 负收益）
  ⇒ ★★ **论证口径（v3 更正）**：★ **不得**用「TLM 是同拍 ⇒ 现实现偏离参考」作理由 —— 实测 TLM 参考的 predict 挂 **fetch NORMAL**（`branch_predictor.h:121-131`）、update 挂 **execute LATE**（`:134-144`），且 predict 结果在 TLM 流水线里**未被消费**（`:127-129`）⇒ TLM 的闭环性只在**单元测试口径**下成立 ⇒ ★ **正确理由 = 「读写同一表项」是 GShare 兑现收益的内在要求**，与参考是否同拍无关
  ⇒ ★ **修法（MUST）**：`ghr_snap_`（`ch_reg<ch_uint<8>>`）在 fetch NORMAL 写 `<本拍 ghr>`；GShare **update 侧索引**改读 `ghr_snap_` 的**当前值**（= 上一拍 GHR = 预测该分支时的 GHR）⇒ ★ 与既有 `pred_*_buf_`（`ibus_chmem.h:146-168`，「fetch 写、下一拍 branch 读」的 1 拍快照）**同构**，已由 R11–R14 验证
  ⇒ ★ **MUST NOT** 直接沿用 `get_ghr()` 的当前值做更新索引（= 现在的实装形态）
- [ ] **Step 2c: ★★ GHR 驱动源收窄（v3 新增，D10）** —— `ghr_en = is_branch & !flushed`（★ **去掉 `jal_en`**）：① TLM 参考的 `update()` 只在 `OpClass::BRANCH` 时调用（`branch_predictor.h:138`）⇒ `jal`/`jalr` **从不**进 GHR；② 现实现把 jal 恒移入 1 ⇒ 无信息量的恒定比特**稀释只有 4 bit 的有效历史**；③ ★ 会破坏 Task 2h 的 2×2 正交性（`enable_uncond_` 同时改变 GHR 构成 ⇒ 归因污染）
- [ ] **Step 3: 更新时机** —— 与 Bimodal **同一拍**更新（★ 顺序契约：branch NORMAL → 消费方 branch LATE）⇒ ★ **MUST NOT** 注册新 stage 名
- [ ] **Step 4: 单元锚** —— GShare 索引 = `pc ^ ghr`（★ 断**值**不只断存在）⇒ 变异「改成纯 pc 索引 ⇒ RED」
- [ ] **Step 4b: ★★ 读改写闭环锚 R18 + 观测口（v3 重写 —— 2026-10-26 双审后）** —— ★ 缺口：R15（索引值）/ R16（GHR 序列）/ R17（开关关闭）**都断不到「更新的表项 == 预测读的表项」** ⇒ 现实现可全绿而无收益
  ⇒ ★★ **③ 观测口（先落，否则 R15–R19 物理上写不出来）**：现有 `get_gshare()` / `get_ghr()` 与 `gshare_rp_fetch_` 均在 `private:`（`branch_predictor_chmem.h:250`）之后 ⇒ 测试不可达 ⇒ **MUST 新增 3 个 payload**（值用 `uint_t<8>`，不改访问层级）：`BP_GSHARE_IDX`（fetch NORMAL，预测索引）/ `BP_GSHARE_UPD_IDX`（branch NORMAL，更新索引）/ `BP_GHR`（fetch NORMAL，当前 GHR）
  ⇒ ★ **R18 判据（v3 改写 —— 原「计数器 ≥2」不可判定）**：恒 PC（`fetch_pc=0` ⇒ 用 `br_pc_`）+ **每拍 `is_branch=1 & taken=1`**（GHR 每拍必移位）⇒ 逐拍断言 `BP_GSHARE_UPD_IDX(t) == BP_GSHARE_IDX(t−1)`（★ 现有夹具 `Phase{end_tick,taken,flushed,fetch_pc,is_branch,is_jal}` 即可构造，**无需扩展夹具**）
  ⇒ ★ 为什么不能用原写法：索引 mask=15 ⇒ GHR 低 4 位 4 拍内**饱和** ⇒ 稳态下修复版与 M19 的索引**重合** ⇒ M19 不 RED；瞬态期每次预测索引又都是「先读后写」⇒ 计数器恒 0 ⇒ 修复版也不 ≥2 ⇒ **该判据与「M19 MUST RED」不可同时成立**
  ⇒ ★ **变异 M19** = 更新索引改回 `get_ghr()` 当前值（= Step 2b 的反例）⇒ **R18 MUST RED**
- [ ] **Step 4c: ★ R19 GHR 驱动源锚（v3 新增）** —— 驱动 `is_branch=0 & is_jal=1` 的段 ⇒ `BP_GHR` 逐拍**不变**；★ **M20** = `ghr_en = (is_branch OR jal_en) & !flushed`（= Step 2c 的反例）⇒ **R19 MUST RED**
  ⇒ ★ 锚文件 = `tests/cpu/test_cpu_chmem_bp_plugin.cpp`（R15–R19 同文件；R15–R17 / M16–M18 的定义见 `.rddf/plans/chmem-bp-btb-gshare-increments.md` §3.2 的锚清单表）

### Task 2g: ★ RAS 8-entry（★ v2.3 新增 —— **增量 2**，★ **新增设计**：TLM 参考**无**）

- [ ] **Step 1: 前置判定** —— ★ **MUST 先看 Task 2h 的实测** ⇒ 若增量 1（+2f）已达 **`cycles ≤ 813074`（= DMIPS 1.4；D9 待签批）** ⇒ ★ **本 Task 显式结案为「本期不做」**（并登记）⇒ ★ **MUST NOT** 无条件开工
- [ ] **Step 2: RAS 结构** —— 8-entry return-address stack（push on `jal`/`jalr` call、pop on `ret`）⇒ ★ **新增设计**（TLM 参考无 ⇒ 无可对照实现 ⇒ 成本高 ⇒ 排在最后）
- [ ] **Step 3: `jalr` 建项** —— ★ 仅在 RAS 可用时 `jalr`/`ret` 才 MAY 建 BTB 项（★ AC-14：否则 `jalr` MUST NOT 建项 ⇒ `ret` 亦然）
- [ ] **Step 4: 单元锚** —— call/ret 配对正确率 100%（★ 断**值**：栈顶地址逐拍比对）

### Task 2h: ★★ 增量分档的量收益（★ v2.3 新增 —— **A5 决策**：「先扩 `is_unconditional` + `jal` 覆盖并量收益，再定 GShare」）

- [ ] **Step 1: 开关面（MUST 钉死机制 —— metis）** —— 开关面 = ★ **工厂布尔参数 + CLI flag**（★ **先例**：`cpu_factory_chmem.h:113` 的 `enable_bp` —— elaboration 期定形 DAG、进程内固定、CLI 透传 ⇒ 单 binary 多次运行即可逐档测量）⇒ ★ **MUST NOT** 用编译期宏（⇒ 重编译 ×3 + 宏矩阵爆炸）⇒ ★ **MUST NOT** 发明运行期可重配 mux（CH_MEM 静态 elaboration ⇒ 白付每拍一级 select）⇒ 三档 ⇒ 3 个布尔（如 `bp_uncond` / `bp_gshare` / `bp_ras`）+ ★ **生效值回显**
- [x] **Step 2: 测量矩阵（防「偷工只跑一档」）** —— 每档 MUST 跑 ① **Dhrystone on/off 双跑**（各 ~20s）② **branchbench 4 档 on/off 双跑**（各 ~55s）⇒ ★ 单档 ≈ **~4.5 分钟**（不含 build）⇒ ★ **三档全测 ≈ 15 分钟** ⇒ ★ **MUST NOT** 只跑一档就下结论 ★ **2026-10-26 增量 1 档已完成并落表**：
  | 配置 | DHrystone cycles | FLUSH_COUNT | 净收益率 | branchbench 4 档 cycles |
  |---|---|---|---|---|
  | BP=off | 914544 | 120065 | — | 86034 / 88034 / 86534 / 87056 |
  | BP=on，增量 1 关（= PoC 行为） | 846500 | 52021 | 56.67% | 84038 / 84040 / 84787 / 85037 |
  | **BP=on，增量 1 开** | **820513** | **26034** | **78.3%** | **78041 / 78043 / 78791 / 79041** |
  ⇒ ★ **ΔFLUSH ≡ Δcycles ≡ 25987** ⇒ ★ 净收益率 **56.67% → 78.3%（+21.6pp）** ⇒ ★ 与 Step 5 的静态上界估算「~80%」**吻合**
  ⇒ ★★ **未达 84.5%** ⇒ ⇒ **GShare（Task 2f）转本期** ⇒ ★★ **实测更正**：「branchbench 上 `jal` 覆盖收益**结构性为零**」是**错的**（★ 静态分析只数了 `jal`，**漏了 `j`** ⇒ `j` = `jal x0` 同 opcode ⇒ 也在覆盖范围 ⇒ 4 档全部改善 ~6000 拍）⇒ ⇒ **branchbench 与 DHrystone 双证**
  ⇒ ★ **AC-27 双分母（fail-closed，2026-10-26 审查补）**：★ 上表的净收益率分母 = **`FLUSH(off)` = 120065** ⇒ ★ MUST 同报 **`N_taken_total`** 口径并**取值较小者** ⇒ ★★ **v3 实测更正：`N_taken_total` 计数器全仓不存在** —— `--stats` 只输出 `BP_STATS hit=/miss=/pred_taken=`（`cpu_verilator_sim.cpp:381`），plugin 侧 `taken_count_` 计的是 **pred_taken**（`branch_predictor_chmem.h:733`）⇒ ★ **双分母需要新代码**（plugin 累计 actual-taken + `sim_main.cpp` 读出口 + runner 打印 `BP_TAKEN_TOTAL=` + 脚本缺字段 fail-closed）⇒ ★ **本表当前缺该列 ⇒ 增量 3 的量收益表 MUST 补齐**；★ 且 `tasks.md:117` 的 AC-27 已 `[x]` 而其 fail-closed 条款未满足 ⇒ 须按 increments 文件 §4 的 2.6 处置
⇒ ★★ **【2026-10-26 实测更正】** ★ 原判断「branchbench 的『校验和逐位相等』不能当增量生效的证据（增量收益在 branchbench 上**结构性为零**）」是**错的** ⇒ ★ 实测：增量 1 在 branchbench 4 档**全部改善 ~6000 拍**（78041/78043/78791/79041）且**校验和逐位不变**（103/183/147/101）⇒ ⇒ ★ **branchbench 是有效的增量生效证据**（★ 根因：静态分析**漏了 `j`** 站点 —— `j` = `jal x0` 同 opcode）⇒ ⇒ ★ 判据 = **branchbench + DHrystone 双证**
- [ ] **Step 4: 决策 + 落点** —— ★ **判据（2026-10-26 v3，D9 待签批）**：`Dhrystone cycles ≤ 813074`（整数比较，= DMIPS ≥ 1.4）⇒ 达标 ⇒ 绝对值门禁可恢复 ⇒ 剩余增量显式结案；不达标 ⇒ **MUST** 把下一档拉进本期 ⇒ ★ 三档全部用尽仍不达标 ⇒ 按 **P-3 临时口径**结案 + 移交后续 change ⇒ ★ **MUST NOT** 让本 change 因 AC 不可满足而**无法归档** ⇒ ★ 决策 MUST 落 **ADR-088 §5.3.2**（净收益率结论 + 绝对值门禁结论**分两件事**落）；★ **裁决表 MUST 4 行齐全**（on/off × uncond/gshare，含同 build `--no-bp` 对照行），缺行 ⇒ **不裁决**
- [ ] **Step 5: 收益上界参考（★ 只供量级参考，不参与判据）** —— DHrystone `.text` 55 个分支站点 = 无条件 36（`ret` 16 / `jal` 15 / `j` 5）+ 条件 19 ⇒ BP=on 后残余 flush **26.0/iter 绝大部分是无条件跳转**（BTB 写使能 = `taken & is_branch` ⇒ `jal`/`ret` 从不建项）⇒ 增量 1 覆盖 `jal`/`j`（20/36）⇒ ★ **静态上界 ≈ 14/iter ≈ 28,000 cycles** ⇒ ★ **实测低于该上界 NOT 判 FAIL**（★ 真实命中率与 `ret` 占比未知）

### Task 3 / Task 4: ★**已作废**（不再实施）

> ★ **本 plan v1 的 Task 3（GShare 13-bit 全局历史 + 2^13 PHT）与 Task 4（RAS 8-entry）已作废**
> —— 依架构简报 v2.1 §3.1：**方向预测采用 Bimodal（无全局历史）**；GShare 与 RAS **延后**
> （GShare 的边际收益待机制 PoC 的命中率数据；RAS 与 BTB 正交可独立立项）。
> ⇒ ★ **本 change 的范围 = Task 1 + Task 2（BTB + Bimodal PHT）+ Task 5 + Task 5.5 + Task 6 + Task 7**。

### Task 5: misprediction recovery（CtrlLink 消费）

- [ ] **Step 1: 接线** —— 预测错误 ⇒ 走 `CtrlLink::flush_when` 真实消费（feature 明写）
- [ ] **Step 2: ADR-040 §10.6 顺序契约（★v3）** —— ★ **跨 stage**（生产者 fetch NORMAL → 消费者 branch LATE）⇒ 保序机制 = **`elaborate()` 按 `canonical_stage_order()` 执行**（`pipe_builder.h:413-424` + `:215-224`：stage 首现序 × EARLY→NORMAL→LATE）⇒ fetch ≺ branch 且 NORMAL ≺ LATE ⇒ **与插件注册顺序无关** ⇒ ★ **MUST NOT** 注册新 stage 名（复用既有 `fetch` / `branch`）
- [ ] **Step 3: 验证** —— `check_plugin_portability.sh` 13/13 仍绿；新增 case 覆盖「预测错误 ⇒ flush 发生」

### Task 5.5: 容量与体积预算（★v2：oracle 补充，实施前必做）

- [ ] **Step 1: 先量 baseline（★v3）** —— ★ **BTB 256×64b（16KB）+ PHT 256×2b（512B）** ⇒ `ch_mem` 阵列规模 ≈ **16.5KB** ⇒ 影响**生成 Verilog 的体积**与**verilator 编译耗时**（`dmem` 靠 `--public-flat-rw` 访问，`sim_main.cpp:16-25`）⇒ ★ **实施前先量当前（无 BP）**的 Verilog 体积 + verilator 编译耗时作为 baseline
- [ ] **Step 2: 估算与判定** —— 加上 BP 阵列后的估算体积/耗时 vs baseline ⇒ 若超出可接受范围（**阈值由 Step 1 实测确定**，不预设）⇒ **上报 architect**（选项：减 PHT 表项 / 改 1-bit PHT / 分 Bank）

### Task 6: 工厂注册

- [ ] **Step 1: 注册** —— `ip/cpu/cpu_factory_chmem.h` 加 `BranchPredictorPluginChmem<T>`（**canonical ordering 位置须过 ADR-048 断言**）
- [ ] **Step 2: 无静默退化** —— 若某配置下 BP 不可用 ⇒ **fail-fast throw**（对齐 `enable_cache=true` 的既有风格），**不得**静默关掉
- [ ] **Step 3: ELF 级回归** —— `[chmem] chmem_vendored_elf_tohost1`（47 ELF）+ `[verilator]`（48 ELF）**0 退化**
- [ ] **Step 4: 性能验收（★v4 —— 以主体 tasks.md 的 AC-8a…AC-8e 为准，本段已作废）** ——
  - ★ **作废**：v1 的「方向变化频繁的档降幅显著更大」+「四档单调性 always_taken→pseudo_random」⇒ **已由 0c 实测证伪**（微基准有工作量污染 ⇒ 档间不可比）⇒ 改由前置1 修正后的基线 + **差值判据**（`Δ(mode) − Δ(always_not_taken)`，扣掉各档共有的循环回边）
  - ★ **作废**：v1 的「必须提供可关闭 BP 的配置面（如 `CPUConfig` 字段）」⇒ **CH_MEM 工厂不接 `CPUConfig`**（ADR-088 §9.1 NG1-3）⇒ 照做**会挂** ⇒ 改「**工厂参数 + CLI flag 双层**」（详见主体 tasks.md 的开工须知 #6 与 §5.3）
  - ★ **作废**：v1 的「依 P-2 CH_MEM 绝对值达标还须先补 load-use 通路」⇒ **P-2 已作废**（PoC 证伪 load-use 非根因）⇒ 绝对值门禁的恢复条件 = **本 change 落地**（ADR-088 v1.2 §5.3.2 **P-6**）
  - ★ **作废**：v1 的「绝对值判据（CoreMark/MHz ≥1.9）」⇒ 属**交付点 B** `chmem-verilator-coremark-harness` ⇒ 本 change **不宣告**绝对值达标
  - ⇒ ★ **本 Step 的唯一有效内容**：性能验收以 **主体 `tasks.md` 的 AC-8a … AC-8e** 为准；**落地后须重录 0c 基线**（`branchbench-baseline-matrix.csv` 扩充 `bp_mode` / `workload_sha` / `build_id` 列，保留无 BP 参照行作历史锚）


### Task 7: 收口

- [ ] **Step 1: 8 门禁** —— 全绿（`check_plugin_portability.sh` 的 Check 5/7/13 是本 change 的主要抓手）
- [ ] **Step 2: 全量回归** —— `run_chipforge_tests.sh --all` ⇒ 0 退化（★ **2026-10-26 新增前置**：`verilator_perf_baseline` 因 perf 基线陈旧**必 FAIL**（实测 820513 / DMIPS 1.38731 vs 基线 846500）⇒ ★ **MUST 先按 increments 文件路径 4.1 重录基线再跑本步**，否则「0 退化」不可达；★ 实测 `chipforge_tests_chmem` 单 entry **2856s（3.4× 文档基线 833s）** ⇒ 预留 **~50min**）
- [ ] **Step 2b: 变异证据入库（2026-10-26 审查新增）** —— 把 `/tmp/opencode/pcmux_mut/run3.sh` 的变异配方落进 `tests/cpu/test_cpu_chmem_bp_plugin.cpp` 注释或 increments 文件 ⇒ ★ **MUST NOT** 只留 `/tmp` 引用（不可复现）
- [ ] **Step 3: 台账更新（★v2：条数修正 + 族级同步）** —— **14 条** BP 相关条目逐条重判（10 条 `not-covered` + 4 条 `partial`）：10 条 → `covered`（CH_MEM 锚 = `branch_predictor_chmem.h` + 新 case + 实测）；4 条 `partial` → `covered`/`partial`（视 CH_MEM 是否实现对应语义）；3 条 TID → `not-covered` + 「CH_MEM 侧无框架级多线程 TID 机制」显式理由。
  - ⚠️ **必须同时改台账 §2 族级表**（`cpu` 族判定行）—— 条目级三态是**从族级继承**的（台账 §2 注记）⇒ 只改条目不改族级 ⇒ 台账自相矛盾，且 `check_coverage_parity_ledger.sh` **只查条目不查族级** ⇒ 漂移会静默通过
  - ⇒ 重判判据：**14 条条目态 + §2 族级判定行** 同时更新，且 `check_coverage_parity_ledger.sh` 仍 PASS
- [ ] **Step 4: ADR-088 §5.2 BP 双前置状态（★v3）** —— 记录「**① 已达成**；**② 未达成**（CoreMark harness 属**交付点 B** `chmem-verilator-coremark-harness`）⇒ **B3 的 BP 阻塞未解除**」+ 登记「**CI 判据 4 未满足**（CI 不构建项目 ⇒ 测量 step 恒 SKIP）」⇒ ★ **MUST NOT** 写成「① ② 已达成 ⇒ B3 阻塞解除」（= **假完工声明**）
- [ ] **Step 5: commit** —— 分批：① CH_MEM BP 组件 ② 工厂注册 ③ 测试 ④ 台账/ADR 同步

---

## Acceptance

- [ ] AC-1: `ip/cpu/plugins/branch_predictor_chmem.h` 存在且实现 **BTB（256×64b，62b 打包：tag30+target30+valid+is_unconditional）+ Bimodal 2-bit PHT（256×2b）**；★ **2026-10-26 v2 更正**：原「GShare / RAS 明确不在范围内（依架构简报 v2.1 §3.1）」**已失效** ⇒ 增量范围由 **A5 决策 + Task 2h 实测**决定（增量 1 已落地；增量 3 因 78.3% 未达门槛已转本期）⇒ ★ **范围以 `openspec/changes/chmem-bp-btb-gshare/tasks.md` 的 AC-1 / AC-16 / AC-27 为唯一权威**
- [ ] AC-2: `cpu_factory_chmem.h` **已注册** `BranchPredictorPluginChmem`（`grep` 可验），且 canonical ordering 断言 PASS
- [ ] AC-3: ★**BP 相关 14 条台账条目全部重判完毕**（10 条 `not-covered` + 4 条 `partial`：`not_taken_returns_zero` / `multiple_addresses` / `reset` / `per_thread_ghr_isolation`）⇒ 每条有明确态 + CH_MEM 锚点 + 显式理由；且台账 **§2 族级判定行同步更新**（族级继承规则）
- [ ] AC-4: ★ **BTB miss ⇒ 不预测**（不跳转、不 flush、不改 PC）⇒ ★ **MUST NOT** 用「static predict fallback」措辞（与主体 tasks AC-4 冲突；且 v1 该措辞隐含「miss 也改 PC」⇒ 语义错误）
- [ ] AC-5: 预测方向与目标为**活节点**（cell `impl()->is_const() == false`）⇒ 不得 `static_cast<bool>`（ADR-040 §10.5）
- [ ] AC-6: misprediction ⇒ CtrlLink flush **真实消费**（有 case 证明 flush 发生）
- [ ] AC-7: `[chmem]` + `[verilator]` 全量 **0 退化**（47 + 48 ELF）
- [ ] AC-8: ★ **已作废**（v1 的「方向变化频繁的档降幅显著更大」+「阈值由 0c 微基准基线确定」+「四档单调性」三处均已被证伪/循环定义）⇒ ★ **以主体 `openspec/changes/chmem-bp-btb-gshare/tasks.md` 的 AC-8a … AC-8e 为准**（含：校验和逐位相等 / 结构等式含暖机 W / 净收益模型只计情形 A / 反证限定 always_taken 档 / 跨档用差值形式）
- [ ] AC-9: 8 门禁全 PASS + 全量回归 0 退化
- [ ] AC-10: ★ **以主体 tasks.md 的 AC-3 / AC-10 为准**（★ 含**台账 §7 裁决行同步** + **计数二义澄清**（`:16` 的 463 = TLM 独占 case 数 MUST 不变；`:80` 的 463 = 条目级条目数，本 change 全部保留行 ⇒ 仍 463）+ **`check_coverage_parity_ledger.sh` exit 0**）
- [ ] AC-11: **TLM 侧 `branch_predictor.{h,cpp}` 全程保留**（删除属 B3，且需双前置）
- [ ] AC-12: ★ **v2.3.1：以 ② 主体 change 的 `tasks.md` 为 AC 的唯一权威** ⇒ ★ 本 plan 的 Acceptance 只列**执行层摘要**，**新增/修订的 AC-26（错路指令维度）/ AC-27（增量分档判据，fail-closed）/ AC-28（增量扩展的保留声明）/ AC-29（增量 1 的单元锚）以 `openspec/changes/chmem-bp-btb-gshare/tasks.md` 为准** ⇒ ★ 本 plan 与该 tasks.md 的 **Task 编号 MUST 一一对应**（★ 本 plan 的 **Task 2f/2g/2h** ≡ 该 tasks.md 的 **Task 7**；★ 本 plan 的 **Task 7（收口）** 与该 tasks.md 的 Task 7（增量分档）**编号不同** ⇒ ★ **以文件为准，勿按编号对齐**）

## 风险与未知

| # | 风险 | 缓解 |
|---|------|------|
| R1 | ~~CH_MEM 组合通路下 BTB/GShare 的「跨拍状态」语义不同~~ → **已关闭（oracle 取证 + PoC P-1 通过）**：框架先例 `csr_chmem.h:499` 的 `mcycle_ <<= mcycle_+1` 就写在 at_stage(fetch,NORMAL) 闭包内 ⇒ CH_MEM 下 at_stage 闭包在 `elaborate()` 期**执行一次**建立 DAG 边 ⇒ **天然满足**「`<<=` 须在 Simulator 创建前」⇒ **PoC P-1 实测 `ch_reg` 跨拍保持成立（1→3→7）** | ✅ 已验证；★ 本期不含 GHR ⇒ 无需再 PoC |
| R2 | ~~PHT 2^13 × 2-bit 在 Verilator 生成后**规模**（≈16Kbit）⇒ 仿真速度与 Verilog 体积~~ → ★ **已关闭**（v2.1 裁决：PHT 改 256×2b ⇒ ≈512B，不再是规模风险；BTB 256×64b = 16KB ⇒ 由 Task 5.5 实测 baseline 判定） | ⇒ 落点 = Task 5.5 的 baseline 实测报告；★ **触发阈值** = 实测 Verilator 编译耗时相对 baseline 增幅 > 50% ⇒ **落点** = `ADR-088 §5.3` 追加一行 + 本 change Task 记录；★ **时限** = **≤ 2 个工作日**，不得挂起等回复 |
| R3 | 预测错误的 recovery 需要「真实方向」信号 ⇒ 与 hazard/branch 的数据流耦合可能踩 ADR-040 §10.6 顺序契约 | Task 5 Step 2 显式核对；生产者用同 stage 名 EARLY、消费用 NORMAL |
| R4 | ~~**CoreMark/MHz 可能达不到 1.9**（CH_MEM 全组合的 cycle 数与 TLM 不可比 ⇒ 数值不可直接对标）~~ → ★ **已关闭**：绝对值判据属**交付点 B** ⇒ 本 change **不宣告**绝对值达标；★ 本 change 的性能判据是 **microbench 中间验收**（主体 AC-8a…8e） | ⇒ 绝对值门禁的恢复条件 = 本 change 落地（ADR-088 v1.2 §5.3.2 **P-6**）⇒ 由主体 **AC-16**（两分支终局式）在实测点裁决 |
| R5 | 3 条 TID 条目（+ 同族 6 条 `partial`）在 CH_MEM 侧无对应（**无框架级多线程 TID 机制**） | 显式标注 + 台账写明差异 ⇒ **不静默丢弃**；⚠️ 台账红线「同一理由 ≤10 次」⇒ 这批共享 1 个理由（≤9 条）合规，但**不得**扩大化 |
| R6 | **兜底句会诱发 AI 静默挂起**（v1 常见：「登记为决策」「报 architect」「与 architect 议」） | ★每处兜底补**触发阈值 + 落点 + 可判定时限**（简报 §5.4）：★ **本期不含 GShare** ⇒ 原「GShare PoC 不可行」触发条件**已作废**；★ 现行触发条件 = **机制 PoC 的 FLUSH 事件数不降** ⇒ 落点 = **ADR-088 §5.3.2 落结论**（恢复/不恢复绝对值门禁）⇒ 时限 = **≤ 2 个工作日**，不得挂起 |
| R7 | ~~**CH_MEM 的 load-use 通路缺失**（原假设：纯 stall 无旁路 ⇒ DMIPS 1.24467 < 1.4）⇒ 绝对值性能验收受阻~~ → **★前提已被 PoC 证伪（2026-10-24）**：禁用 CH_MEM 侧 RAW stall 后 **cycles 完全不变（914544）** ⇒ load-use **不是**根因 ⇒ **P-2 `chmem-load-use-bypass` 已作废，不立项**。**真根因 = CH_MEM 无分支预测**（ADR-088 v1.2 §5.3.1：比率随分支密度单调升 add 1.037 / jal 1.078 / beq 1.098 / Dhrystone 1.151）⇒ **与本 change 同源** | **本 change 即根因的解法** ⇒ 绝对值 1.4 门禁的恢复条件 = **本 change 落地**（ADR-088 v1.2 §5.3.2 **P-6**）⇒ 性能 AC 走 **microbench 中间验收**（P-5，绝对值由本 change 的副产品恢复）
| R7 | 「不适用 §5.2『不做』出口」意味着 B3 时 TLM BP **必须**能删 —— 若本 change 延期，B3 会被阻塞 | 把本 change 排在 ① 之后立即执行；进度风险已在 ADR-088 §3 BP 行登记 |

## 迁移说明

落地为 `.rddf/plans/chmem-bp-btb-gshare.md`；建议**完整 rdd-builder P1–P3**（规模大 + 触及 CH_MEM 组合通路的架构假设 + 需 architect 参与 R1/R2 判定），不走 rdd-quick。

---

## 实施期修正（2026-10-24，Task 1 产出）

| # | plan v1 的设计 | 实测问题 | 修正 |
|---|---|---|---|
| 1 | Task 2 Step 5「单元测试出口二选一：(a) 不适用 / (b) 编译期多实例」 | ★ v2.1 的 (b) 方案在 CH_MEM 侧的**可实例化性**当时未验证 | ★ **v2.2 裁决 = (b)**：物理深度 **256 编译期固定** + **运行期 mask**（`& literal(M-1)`，M ∈ 16/32/64/128/256）⇒ 「运行时可配 BTB 尺寸」**成立** ⇒ 台账 `custom_btb_size` / `btb_runtime_{16,64,256}` **4 条升 covered**（★ 须有 **M=16/64/256 三档分别实例化**的 CH_MEM 侧 case ⇒ 主体 AC-18）⇒ ★ **MUST NOT** 保留 (a)（那会把「运行时可配」误判为不适用 ⇒ 与 v2.2 §3.2 矛盾） |
| 2 | Task 6 Step 4「提供『可关闭 BP 的配置面』（**如 `CPUConfig` 字段**）」 | **CH_MEM 工厂不接 `CPUConfig`**（`cpu_factory_chmem.h:95-107` 显式「不读 cfg, 不引 CpuConfig 依赖」= ADR-088 §9.1 NG1-3）⇒ **照抄会挂** | 改为**工厂参数 + CLI flag 双层**（`build_cpu(..., std::optional<bool> enable_bp)` + `cpu_verilator_sim --enable-bp/--no-bp`），且**用普通 `bool` 不用 `optional<bool>`**（避免 nullopt 与「显式关闭」语义撞车） |
| 3 | Task 3 Step 1「GHR 用 `ch_reg`（跨拍保持）」 | 未验证 | **PoC P-1 通过**（1→3→7）⇒ 结论成立，**无需降级** |
| 4 | Task 5 Step 1「misprediction ⇒ `CtrlLink::flush_when`」 | CH_MEM 的 `flush_when(Condition)` **无有状态重载**（`ctrl_link.h:110` 只有 `Condition` 版；有状态版只有 `halt_when_stateful`）⇒ misprediction flush 信号若需跨拍保持**无对应 API** | misprediction 的 flush 信号按**组合路径**设计（misprediction 当拍即知）⇒ 不需要跨拍保持 ⇒ 用 `flush_when(ch_bool)` 即可 |
| 5 | Task 5.5「先量 baseline（Verilog 体积 + verilator 编译耗时）」 | 未执行 | **Task 1.5 已查 `ch_mem` 是真数组存储**（`memimpl.h:27` `init_data_` vector + depth）⇒ Verilog 里是 `reg [N-1:0] mem [depth-1:0]` ⇒ **4096×32bit = 16Kbit reg array，体积可控**（不展开成 mux）⇒ 预算风险等级从「高」降为「中」（仍需实测编译耗时） |


### ★★ per-file × per-symbol owner 表（★ v2.2 新增 —— metis 三审 S4/4a：三个 change 同改同一批文件而无 owner 表 ⇒ 边界实际不存在）

| 文件 / 符号 | 前置1 | 前置2 | 主体 | ★ 约定 |
|---|---|---|---|---|
| `tests/cpu/manual_elf/branchbench/branchbench.c` | ✓ owner | ✗ | ✗（硬门禁） | ✅ 闭合 |
| `soc/cpu/docs/dse/branchbench-baseline-matrix.csv` | ✓ **建列**（`workload_sha` / `build_id` / `bp_mode=off`）+ 改按表头取列 | ✓ **追加** BP 行 | ✓ **追加** BP 行 | ★ 建列归前置1；追加归后两者 |
| `tools/measure_branchbench.sh` | ✓ owner（改名 / 按表头取列 / 捕获 stderr） | ✓ **仅追加** `--bp=on\|off` flag 行 + 生效值断言 | ✗（硬门禁） | ★ **捕获 stderr 归前置1**（否则假绿通道开着）⇒ 前置2 只加 flag |
| `tools/verilator_runner/*` | ✓ `--stats` 框架 + `pc_changes`/PC 直方图（★ **NOT** retired） | ✓ **FLUSH 计数** + **retired 计数** + BP flag | ✓ `--no-bp` / `--btb-entries` + 回显 | ★ **框架归前置1**，增量归后两者，**MUST NOT** 另起一套 |
| `ip/cpu/plugins/branch_predictor_chmem.h` | ✗（禁 `ip/**`） | ✓ **创建**（最小版） | ★ **v2.3.1 增量扩展为完整版**（★ 原写「**重写**」与「前置2 已建的版本 MUST **增量扩展**」**直接打架** ⇒ 现取**增量扩展**） | ★ 前置2 的 PoC 版 **MUST NOT** 被主体当作已完成；★ **MUST 保留**前置2 已验证的机制点（`bp_next` 情形 C 臂 / 门控 flush / `!flushed` 情形门控 / payload fail-fast / 5 个 `ch_reg` 纠错记录）⇒ 见 ② 主体 **AC-28**；★ 主体 AC-1 MUST 检查**完整功能**（非文件存在）；★ PoC 残留的**删除/重写责任归主体**；★ `tests/cpu/test_cpu_chmem_bp_poc.cpp` 的 8 case **在主体后 MUST 仍绿** |
| `ip/cpu/plugins/branch_chmem.h`（导出 jal/jalr 判别 payload） | ✗ | ✓ **首次改**（`BRANCH_TAKEN_RAW` / `BRANCH_FLUSHED` / `IS_BRANCH` 三个 payload） | ✓ **二次改**：★ **增量 1 的「无条件」信号当前无来源**（`branch_chmem.h:201-203` 已算出 `is_jal`/`is_jalr` 但**未导出**）⇒ **MUST** 新增导出（`IS_JAL` / `IS_UNCOND`）⇒ ★ **MUST NOT** 让 BP 侧从 opcode 反推（branch 节点未导出 opcode） | ★ 主体 owner |
| `ip/cpu/plugins/ibus_chmem.h`（`update_pc` 的 mux + flush） | ✗ | ✓ **首次改**（五级 mux + ★ **flush 门控**（`hold_z \| (taken_buf & !spec_ok_buf) \| mispredict_A_buf`；★ 「双分量」表述已作废）+ FLUSH 计数） | ✓ **二次改**（补完整语义 + 过时注释 #8） | ★ **同函数两次改** ⇒ 前置2 的版本 MUST 可被主体**增量扩展**（MUST NOT 要求主体重写）⇒ ★ 主体 MUST 继承前置2 的**四情形 select 链**结构 |
| `ip/cpu/cpu_factory_chmem.h`（注册 + 开关 + 参数转发） | ✗ | ✓ **首次注册**（BP 开关） | ✓ **补全**（4 参数转发修复 #9 + 完整注册） | ★ 同上 ⇒ 增量扩展 |
| `docs/architecture/tlm-coverage-parity-ledger.md` | ✗（硬禁） | ✗（硬禁） | ✓ owner | ✅ 闭合 |
| `ip/cpu/plugins/branch_predictor.{h,cpp}`（TLM 侧） | — | — | ✓ **全程保留**（AC-11） | ✅ 闭合 |

### ★★ FLUSH 计数归属定案（★ v2.2 —— metis 三审 A7/4b：前置1 spec 要求输出 FLUSH 计数 vs 前置1 禁 `ip/**` ⇒ 互斥）

⇒ ★ **裁决**：**FLUSH 事件数归前置2**。
- 理由：FLUSH 是 DUT 内部 payload（`ibus_chmem.h:167` 的 `br_taken_buf`），**host 不可直接见** ⇒ 要计数须改 `ip/cpu/plugins/ibus_chmem.h` ⇒ **违反前置1 的「不得触碰 `ip/**`」边界** ⇒ 前置1 **只交付 host 可导出的「采样到的 PC 值的变化拍数」**（★ **不是** retired —— 采样流含非架构 PC 值，机制未确认）
- ⇒ ★ **前置1 spec 的「工作量计数须可观测」场景已改写**为「采样到的 PC 值的变化拍数 + 总周期数」（★ **不是** retired —— 采样流含非架构 PC 值，机制未确认；★ retired 与 FLUSH 均需 flush 信号 ⇒ 前置1 不可得 ⇒ 归前置2）⇒ ★ **4 档工作量的承重证据 = 静态结构论证**（档位选择无分支 + 分支两路径等长 ⇒ 4 档执行同一指令序列）；动态计数仅作**跨档活动量**佐证
- ⇒ ★ **前置2** 交付 FLUSH 计数（首个可改 `ip/` 的 change）⇒ 主体复用

### ★★ 「归档时必须完成」归属（★ v2.2 —— metis 三审 3c：该 H2 不在 `## Acceptance` 下 ⇒ verifier 不查）

⇒ ★ **已改为 AC-19 / AC-20 / AC-21**（在主体的 `## Acceptance` 内）⇒ verifier 会核。

### Task 1 实测留证（2026-10-24，branch `feature/chmem-bp-btb-gshare`）

| 项 | 实测 |
|---|---|
| PoC 文件 | `tests/cpu/test_cpu_chmem_bp_poc.cpp`（**8 case / 51 assertions 全绿**） |
| PoC 归属 | `chipforge_tests_chmem` binary（CH_MEM 专属 ⇒ 需 `#ifdef CF_PLUGIN_USE_CH_MEM` **整体 guard**，且 guard 必须在**所有 include 之前**，否则 TLM binary 会编译 CppHDL 头 ⇒ 符号重复定义） |
| 探索规模 | 实施期共跑 **21 个对照 case**（含 A/B/C 三变体 + 4 决定性对照 + 逐拍观测 + 全 entry 扫描 + 驱动边界补测）⇒ 收敛为最终 **8 个**（保留有决策价值的 + 钉死 1 个负例） |
| 机制结论 | **7 条**（见 Task 1.5 表）⇒ 核心 = **mem 端口地址必须来自「已被驱动」的信号**（payload 派生 ✓；未被驱动的 reg ⇒ 索引 0 ⇒ 静默错；**被 op 节点驱动的 reg ✓**） |
| 分支信号源 | 已核实齐备（`KT::PC` / `BRANCH_TAKEN` / `BRANCH_TARGET` / `update_pc` 的 PC-mux） |
| 差错误判（已更正） | 实施中曾判定「套件内存在测试间干扰」⇒ **该判断错误**（行号映射错误）；事实 = **套件失败集合 == 单跑失败集合** ⇒ 无干扰 |

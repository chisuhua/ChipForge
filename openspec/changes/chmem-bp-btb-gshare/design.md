# Design — chmem-bp-btb-gshare

> 架构方案简报（权威）：`ip/cpu/docs/bp-btb-gshare-design-brief.md` v2.2
> ★ 本文件是简报 §3.4.2 的**明文义务**落地：五级 mux 嵌套契约 + 三步时序 + payload 契约。

---

## 0. ★★★ v2.3 覆盖声明（2026-10-26，**优先级高于本文件以下全部内容**）

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

★ **决策规则（可判定）**：若「增量 1（+2）」在 Dhrystone 上的**净收益率 ≥ 84.5%** ⇒ 绝对值门禁可恢复
⇒ GShare/RAS 可延后；**否则** ⇒ MUST 把 GShare 拉回本期（AC-1 范围放开）。
★ 净收益率的**分母口径**归 **A6**（裁决项）⇒ ★ 实测数据见 `tasks.md` 的 Task 7。

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

## 1. 取址决策的五级嵌套（★ 契约）

### 1.1 为什么由本 change 给规格

- `fetch_stall_protocol_v0.11.0.md` **rev.14 定稿**（critic 十五轮 APPROVE）规定 `next_pc` MUST 为 **`trap > branch > freeze > sequential`**（`:201` / `:349` / `:603`）；
- **R11.1** 裁决把 ECALL/EBREAK/MRET 的 trap redirect 交给 `csr-chmem-trap-pc-jump`，★ **插入点 = `ibus_chmem.h:171-185` 的 `update_pc`**（protocol `:603`）—— **与 BP 的插入点同一处**；
- BP 已 defer 到 v1.0.0（2026-10-19 S3 决策），而 trap/freeze mux 在 v0.11.0 Phase 3.5/C.4（~2026-11）落地 ⇒ ★ **BP 落地时那 5 项 mux 已存在**；
- 且 protocol 明确写了「该 pc_reg mux 只含 stall 分量，**不含 BP 分支**」（`mmu-chmem-phase-b-c-launch/tasks.md:39`）⇒ ★ **本文件补的就是那个缺口**。

### 1.2 契约

```
next_pc = select( trap_redirect,        trap_target,     ← 最高（protocol rev.14）
              select( branch_actual,     actual_target,   ← 分支实际结果（含误预测纠错）
                select( freeze_hold,     hold_pc,         ← stall 期间 PC 保持
                  select( bp_predict,    bp_target,       ← ★ 本 change 新增
                           pc + 4 ) ) ) )                 ← 顺序取指
```

| 项 | 裁决 | 理由 |
|----|------|------|
| BP 预测项的位置 | ★ **在 `freeze` 之下、`seq` 之上** | ① `freeze`（PTW stall）期间 PC 必须**保持**；若 BP 预测项在 freeze 之上 ⇒ 预测会在 walk 期间把 PC 推走 ⇒ 恢复路径要多记一份 pre-hold PC ⇒ 复杂度 > 收益 ② 不违反 protocol rev.14 已 APPROVE 的不变量 |
| mispredict 纠错的位置 | 与 `branch_actual` **同层**（mispredict 优先于 actual） | 纠错目标 `pc_lag+4` vs actual target ⇒ 纠错更具体 ⇒ 优先 |
| ~~**FLUSH 的双分量**~~ | ★★ **作废（v2.3 §0.4）** —— 「替换分量」是**错的**（⇒ fall-through 副作用泄漏）⇒ ★ 正确形态 = **门控**：`flush = hold_z \| (taken_buf & !spec_ok_buf) \| mispredict_A_buf` | ★ BP 关闭时 ≡ `taken_buf` ⇒ **零退化**（逐位相同） |

### 1.3 合入顺序约束

★ **本 change 与 `csr-chmem-trap-pc-jump` 修改同一插入点** ⇒ 合入顺序：

| 序 | 内容 | 说明 |
|---|------|------|
| 1 | `csr-chmem-trap-pc-jump`（trap 项）落地 | protocol rev.14 的既有项 |
| 2 | ★ **本 change** 在其基础上**插入** `bp_predict` 项 | 必须**基于**已落地的 5 项 mux 扩展 |
| — | ★ 若本 change 先于 trap 项落地 ⇒ **必须预留 trap 插入位** | ★ 必须在源码里**留出 trap 项的位置**（否则后续 trap 项无处插入 ⇒ 返工）|

⇒ ★ **登记（本 change 实现 MUST 满足的可判定形式）**：`update_pc` 的 `next_pc` MUST 是**可 grep 的多层 select 链**，且**源码中 MUST 出现一个具名的 trap 项**（例如 `trap_redirect` / `trap_target` 标识）⇒ ★★ **v2.3.1 收紧（metis：原断言会被「假 trap 项」骗过）**：★ **grep pattern MUST 匹配 select 链内的实参**（如 `select(\s*trap_redirect`）⇒ **纯注释行不命中**⇒ ★★ **占位臂的精确形态（MUST）**：① 四个具名信号 `trap_redirect` / `trap_target` / `freeze_hold` / `hold_pc`；② 占位期恒绑定 **false / 0**；③ **trap 臂 MUST 在 select 链最顶层**（顺序断言）；④ ★ **占位臂惰性** ⇒ BP 关闭态的 mux 输出 MUST 与现状**逐位相同**（单元锚）⇒ ⇒ 否则占位臂接错优先级**无法被检出**⇒ 这样后续 `csr-chmem-trap-pc-jump` 落地时有**可插入的位置**（★ **MUST NOT** 只写「注释 + 结构预留」—— 注释不可机检 ⇒ 后续 trap 项无处插入 ⇒ 返工）

---

## 2. 三步时序（★ 实施者必须照此实现）

### 2.1 三个 PC 角色

| 符号 | 含义 | 何时可知 |
|------|------|---------|
| `pc_reg(N)` | **本拍正在取**的指令地址 | fetch NORMAL 读到它 |
| `pc_lag(N) = pc_reg(N-1)` | **本拍正在解析**的指令地址 | branch NORMAL 解析它 |
| `pc_reg(N+1)` | **本拍 branch LATE 决定的**下一取指地址 | branch LATE(N) 写 |

★ **关键**：BTB 在 fetch NORMAL(N) 查的是 `pc_reg(N)`，而 branch LATE(N) 的 actual 是 `pc_reg(N-1)` ⇒ **两者不是同一条分支**；但**两者都作用于 `pc_reg(N+1)`** ⇒ 可以放在同一个 mux（优先级 `actual` > `BTB 预测` > `pc+4`）。

### 2.2 完整时序（★ 实施者必须照此实现；★ v2.2 修正纠错作用域）

> ★★ **v2.2 关键修正**：旧版把「预测不一致 ⇒ 纠错到 `pc_lag+4`」**不分情形** ⇒ ★ **情形 B（预测 not-taken / 实际 taken）是暖机主路径**（PHT counter=1 ⇒ 预测 not-taken，而分支恒 taken）⇒ 照旧实现会**走错路径** ⇒ **校验和不等 ⇒ AC-7/AC-8a 必 RED**。
> ⇒ ★ **纠错 MUST 分情形**。

```
── cycle N ──────────────────────────────────────────────────────────
fetch NORMAL(N):
    pc_reg = B                            (B = pc_reg(N))
    ★ BTB aread(bits(B)) -> hit, target=T, pht_dir=D
      (0 拍; BTB 读地址 = 被 <<= 驱动的 pc_reg ⇒ proxy 可用)
    写 payload: pred_target=T, pred_taken=D, pred_valid=hit

branch NORMAL(N):
    解析 instr[pc_lag(N)] = instr[pc_reg(N-1)] => actual_taken(N), actual_target(N)

branch LATE(N):
    ★ 写 pc_reg(N+1)  —— ★★ **4 项显式 select 链**（可逐字实现 / 可 grep 断言）:
        select( actual_taken(N), actual_target(N),          <- instr[pc_reg(N-1)] 的结果 (优先)
          select( pred_valid && pred_taken, pred_target,     <- instr[pc_reg(N)] 的投机预测
                   pc_reg(N) + 4 ) )

── cycle N+1 ────────────────────────────────────────────────────────
branch NORMAL(N+1):
    解析 instr[pc_lag(N+1)] = instr[pc_reg(N)] => actual_taken(N+1), actual_target(N+1)
    ★ 与 1 拍前的预测记录比对:

    ★★ 情形 A（预测 taken / 实际 not-taken）—— **唯一需要「纠回 fall-through」的情形**
        pred_valid && pred_taken && !actual_taken(N+1)
          => 纠错目标 = pc_lag(N+1) + 4   (真正的 fall-through, 不是 pc_reg+4)
          => flush (抑制错路指令的副作用)

    ★★ 情形 B（预测 not-taken / 实际 taken）—— **★ MUST NOT 纠到 fall-through!**
        pred_valid && !pred_taken && actual_taken(N+1)
          => 纠错目标 = actual_target(N+1)   (跳到真实目标)
          => ★★ **flush：MUST 抑制！**（★ v2.2 修正：此处的 fall-through 是「分支实际 taken ⇒ 被跳过的非法路径」⇒ store/写回 MUST 被抑制）
          => ★ 这条是**暖机主路径**（PHT counter=1 ⇒ 预测 not-taken）

    ★★ 情形 C（预测 taken / 实际 taken 且 target 一致）—— **必须抑制自身 redirect**
        pred_valid && pred_taken && actual_taken(N+1) && pred_target == actual_target(N+1)
          => 抑制该分支自身 redirect  (否则 target 被取两次 ⇒ Δcycles≈0 ⇒ 误判「机制不成立」)
          => ★ **flush：MUST NOT 抑制**（投机 taken 臂生效 ⇒ fall-through 根本没被取 ⇒ `bp_spec_taken_buf` 拉高 ⇒ taken 分量被门控掉）

    ★★ 情形 D（预测 not-taken / 实际 not-taken）
        => 无需任何动作（顺序取指本就正确）；★ **flush：MUST NOT 抑制**（分支未 taken ⇒ fall-through 是合法路径）

    ★★ 情形 E（BTB miss ⇒ 未投机 / 分支实际 taken）—— ★★ **MUST 抑制！**
        !pred_valid && actual_taken(N+1)
          => fall-through **确实被取了**（顺序取指）且它是错路 ⇒ store/写回 MUST 被抑制
          => ★ 这是「BTB miss 时不得破坏程序语义」的关键情形

branch LATE(N+1):
    ★ 写 pc_reg(N+2)  —— ★★ **4 项显式 select 链**:
        select( 情形A, pc_lag(N+1) + 4,                        <- 纠回 fall-through
          select( 情形B, actual_target(N+1),                   <- 跳到真实目标 (暖机主路径!)
            select( 情形C, bp_next(N+1),                       <- ★ v2.3 §0.1: 由 pc_reg+4 改为 bp_next
                                                            <-    (抑制自身 redirect **且保留下一级投机**)
              select( actual_taken(N+1), actual_target(N+1),   <- 无预测时的正常行为
                      pc_reg(N+1) + 4 ) ) ) )
```

⇒ ★ **收益**：正确预测的 taken 转移（情形 C）⇒ `pc_reg(N+1)` 直接 = target ⇒ **fall-through 那一拍根本不被取出** ⇒ **气泡完全消除**（实测每条 taken 转移 1 拍气泡）。

⇒ ★ **误预测代价**：

| 情形 | 描述 | 相对无 BP 的代价 |
|------|------|----------------|
| **A** | 预测 taken / 实际 not-taken | **1 拍**（多取了一拍错误 target；纠错回到 fall-through 该拍本来就要取） |
| **B** | 预测 not-taken / 实际 taken | **0 拍**（与无 BP 行为相同） |

⇒ ★ **净收益模型**：`Δcycles(off − on) = N_correct×1 − N_false_A×1`（★ **N_false 只计情形 A**）


## 3. Payload 契约

| 方向 | 内容 |
|------|------|
| BP 的**输入** | fetch 期的 `pc_reg`（**被 op 节点 `<<=` 驱动** ⇒ 可作 mem 读地址） |
| BP 的**输出** | ① `pred_target`（32b）② `pred_taken`（1b）③ `pred_valid`（1b）⇒ **写入 payload**（mode-aware `uint_t<N>`，禁裸 `bool`） |
| 误预测记录 | `ch_reg` 组（pred_taken / pred_target / pred_valid）⇒ 跨拍保持 |
| ★ **BP 的表更新输入（branch plugin 导出）** | ★ **前置2 实现轮追加**（原契约未列 ⇒ 主体重写时易丢失）：① `BRANCH_TAKEN_RAW`（1b，未被 flush 抑制的原始方向）② `BRANCH_FLUSHED`（1b，错路标记 ⇒ 表更新门控）③ `IS_BRANCH`（1b，条件分支标志）⇒ ★ BP 侧**缺失即 fail-fast**（MUST NOT 静默降级为 false ⇒ 表不更新却无报错） |
| ★ **无条件跳转判别（★ v2.3.1 新增 —— 增量 1 的前置）** | ★ **branch plugin MUST 导出**「本条指令是 `jal`（PC 相对 ⇒ 目标静态 ⇒ 可安全建 BTB 项）」的 payload（★ **候选命名**：`IS_JAL`（1b，只含 `jal`）/ `IS_UNCOND`（1b，含 `jal`+`j`）；★ **语义待主体定，但 MUST 明确 `jalr` 是否包含** —— ★ **`jalr` MUST NOT 含在内**（目标随运行时变化 ⇒ 静态 BTB 项必错 ⇒ AC-14））⇒ ★ **来源已存在**：`branch_chmem.h:201-203` 已算出 `is_jal`/`is_jalr` ⇒ ★ **MUST NOT** 在 BP 侧从 opcode 反推（★ branch 节点**未导出** opcode ⇒ PoC 的 `branch_predictor_chmem.h:386-392` 已明写「jal 的方向问题留给主体 change」）⇒ ★ **缺失即 fail-fast**（与 `BRANCH_TAKEN_RAW` / `BRANCH_FLUSHED` / `IS_BRANCH` 同款纪律） |
| 纠错目标 | 「预测 taken、实际 not-taken」⇒ **`pc_lag + 4`** |
| ★ **顺序契约** | ★ **跨 stage**（生产者 fetch NORMAL → 消费者 branch LATE）⇒ 保序机制 = **`elaborate()` 按 `canonical_stage_order()` 执行**（stage 首现序 × EARLY→NORMAL→LATE）⇒ fetch ≺ branch 且 NORMAL ≺ LATE ⇒ **与插件注册顺序无关** |
| ★ **pc_reg 单驱动纪律** | BP **只写 payload**，**禁止**自身 `<<= pc_reg`（`pc_reg` 的唯一写者是 `update_pc`） |
| ★ **stage 名** | BP **MUST NOT** 注册新 stage 名（复用既有 `fetch` / `branch`） |

---

## 4. 表结构与更新规则

### 4.1 BTB（256 × 64b）

```
word = { tag30 = PC[31:2], target30 = target[31:2], valid(1b), is_unconditional(1b) }
        ← 62 bit（余 2 bit）
```

★ 依据：**CH_MEM 只解 32 位指令**（`decode_chmem.h:82` 的 `instr_format` 无 16 位格式）⇒ `PC[1:0]=0`、target 4 字节对齐 ⇒ 低位恒 0 ⇒ 可安全裁掉。

| 项 | 规则 |
|----|------|
| 物理深度 | **256**（编译期模板参数） |
| 逻辑条目数 M | **运行期 mask**：`bits<7,0>(pc) & literal(M-1)`，M ∈ {16,32,64,128,256} |
| 索引 | 上述 mask 结果 |
| 命中判定 | `valid && tag30 == pc[31:2]` |
| 目标恢复 | `target30 << 2` |
| key | ★ **vaddr**（分支 PC payload 本身是 vaddr ⇒ MMU on/off 间一致） |
| 替换策略 | ★ **直接覆盖**（同槽直接写） |
| 写使能 | ★ **taken**（目标此时才已知）★ **且「不是错路指令」** ⇒ **使能 = `taken & is_branch & !flushed`**（★ 前置2 实现轮追加：`flushed` = branch plugin 导出的 `BRANCH_FLUSHED`） |
| ★ **`is_unconditional`** | ★ 无条件跳转（`jal`）**不占 PHT 表项** ⇒ 若无此位则 `jal` 的方向**无来源** ⇒ **永不被预测** ⇒ call 气泡不可恢复 ⇒ 此位是**必需**的 |
| ★ **`jalr` 不建项** | 间接跳转目标每次可能不同 ⇒ 静态缓存必错 ⇒ `jalr` 走顺序取指 |
| ★ **陈旧目标自愈** | 直接覆盖策略下，目标变化的分支再次 taken 时**同槽重写** ⇒ **天然自愈**，不设误预测计数器 |

### 4.2 PHT（256 × 2-bit，Bimodal）

| 项 | 规则 |
|----|------|
| 索引 | `bits<7,0>(PC) & literal(M-1)`（**无全局历史 XOR** —— GShare 不在本期） |
| 饱和 | ★ **taken ⇒ 饱和 +1（0→1→2→3）；not-taken ⇒ 饱和 −1（3→2→1→0）**（★ 与 TLM 侧 `branch_predictor.h:274-286` 一致；★ 原「不回退」措辞有歧义 ⇒ 不递减 ⇒ 零方向信息 ⇒ 净负收益） |
| 预测 taken 阈值 | ★ **counter ≥ 2** |
| 写使能 | ★ **「分支已解析」（任何方向）** —— 若只在 taken 时更新 ⇒ 学不到 not-taken ⇒ 在 not-taken 密集的负载上**净负收益**。★ **且「不是错路指令」**（★ 前置2 实现轮追加）⇒ **使能 = `is_branch & !flushed`**（`flushed` = branch plugin 导出的 `BRANCH_FLUSHED`）⇒ 错路指令的 taken 已被抑制 ⇒ 用它学方向会学错 ⇒ 表项被污染 |
| 更新地址 | ★ **分支解析处的 `KT::PC`**（= 预测时的 `pc_reg`）⇒ 二者必须**同指一条分支**（off-by-one ⇒ 学到错误表项） |
| 无条件跳转 | ★ **不更新 PHT**（恒 taken ⇒ 无需方向学习）⇒ 不占表项 |

### 4.3 预测方向 `pred_taken` 的来源（★ 三者合成）

```
pred_taken = pred_valid && ( is_unconditional || (pht_counter >= 2) )
```

⇒ ★ **`is_unconditional` 位是必需的**（见 §4.1）：否则 `jal` 无方向来源。

---

## 5. 未纳入本 change 的项（★ 防止范围蔓延）

| 项 | 理由 | 归属 |
|---|------|------|
| GShare 全局历史（`ch_reg<ch_uint<13>>`） | 相对 Bimodal 的边际收益待机制 PoC 的命中率数据 | 后续 change（**不预先立项**） |
| RAS call/ret 预测 | 与 BTB 正交 | 后续 change |
| 多级 TLB / PTW | 与本 change 无关 | `chmem-ptw-walk-core` / `-wiring` |
| 改 CH_MEM 取指延迟（imem 改 `aread`） | 刻意保持 `pc_lag` 对齐的既有决策 | — |
| 改判据（DMIPS / CoreMark 阈值） | 需显式 ADR | architect |

⇒ ★ **机检**：`ip/cpu/**` 与 `tests/**` 的变更集内 MUST NOT 出现 `GShare` / `GHR` / `RAS` 标识、**MUST NOT** 出现 `ch_reg<ch_uint<13>>`。

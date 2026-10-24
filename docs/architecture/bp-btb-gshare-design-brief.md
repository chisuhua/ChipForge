# ② chmem-bp-btb-gshare — 架构方案简报（architect 主持）

> **状态**: 🚧 **v2.2 — 三审后修订**（oracle / metis 三审：6 个 P0 全部闭合 + 11 条「可声称通过但没验」的 AC 已修）
> **v2.1 → v2.2 摘要**：★ **BTB 布局统一到 62b**（旧布局在 3 处残留且**不可实现**：`valid=(target[1:0]!=0)` 与「target 4 字节对齐 ⇒ `[1:0]` 恒 0」**自相矛盾** ⇒ valid 恒 false）· ★ **误预测纠错的作用域限定为情形 A**（情形 B 是暖机主路径 ⇒ 照旧实现会走错路 ⇒ 校验和不等）· ★ **AC-16 改两分支终局式** · ★ **「归档时必须完成」移入 `## Acceptance`**（否则 verifier 不查）· ★ **FLUSH 计数归属定案** · ★ **新增 per-file owner 表**
> **v1 → v2 摘要**: oracle R1（**挂钩点错**：BP 应在 fetch NORMAL 提前一拍预测，不是 branch LATE）· oracle R3 + metis（**TLM 的 BP 是惰性桩 ⇒ ADR-088 §5.3.1 根因归因被证伪**）· metis（**缺 AC 清单 / 台账逐条 / 兜底句无时限 / 阈值循环定义**）。**v1 的 §2.2 时序推演与 §3.2「方向预测无用」的裁决均被推翻** —— 详见 §0.5 与 §7。
> **日期**: 2026-10-24 · **主持**: architect · **下游**: planner（openspec 立项）→ builder（实施）
> **上游**: ADR-088 v1.2 §3 BP 行（CH_MEM-only 且必须做）· §5.2 BP 专项删除前置 · §5.3.2 **P-6**（绝对值门禁恢复条件 = ② 落地）
> **执行文件**: `.rddf/plans/chmem-bp-btb-gshare.md`（40 open / 11 AC）· **边界简报**: `.rddf/plans/chmem-bp-btb-gshare-brief.md`
> **Task 1 已完成**（commit `50807f0` + `47a3c8b`）：PoC 8 case / 51 assertions 全绿 + 7 条机制结论

---

## 0. 摘要（给评审者）

**本简报的核心不是「怎么实现 BTB」，而是三个架构裁决 + 两个已完成的判决实验**。

### 0.1 三个裁决（v2 修订后）

| # | 裁决项 | 结论 | 与 v1 的关系 |
|---|--------|------|------------|
| **A** | **BP 的挂钩点在哪？** | ★ **fetch NORMAL 提前一拍**（不是 branch LATE）—— BTB 在 fetch 期查 `pc_reg`（0-cycle `aread`），预测结果喂给**同拍 branch LATE** 的 `next_pc` mux ⇒ **正确预测时 fall-through 根本不被取出 ⇒ 气泡完全消除** | ★ **v1 的 §2.2 推演错了**（oracle R1） |
| **B** | **② 的范围** | **BTB + Bimodal 2-bit PHT**（GShare/RAS 延后）⇒ 方向策略有定义；BTB-only 会「恒 taken」⇒ 在 not-taken 分支上**净负收益** | ★ **v1 的「方向预测无用」错了**（oracle R2）—— 方向预测在「提前一拍」的模型下**恢复标准价值** |
| **C** | **BP 的存储结构** | **单表 `ch_mem<ch_uint<64>, 256>`** · **62b 打包**：`{tag30 = PC[31:2], target30 = target[31:2], valid(1b), is_unconditional(1b)}`（单 word 64b，余 2b 保留）· 物理深度 **256**（编译期）· 逻辑条目数 M（运行期 mask `& literal(M-1)`） | ★ v1 的「2 表 × 4096 = 32KB」被推翻；★ v2 的「65 bit > 64」是**算术错误**；★ v2.1 的「valid 偷 target[1:0]」**不可实现**（target 4 字节对齐 ⇒ `[1:0]` 恒 0 ⇒ valid 恒 false）⇒ v2.2 改为 62b 独立 valid 位 + `is_unconditional` 位 |

### 0.2 判决实验（**v2 新增，已实测，代码已复原**）

| # | 实验 | 结果 | 结论 |
|---|------|------|------|
| **E-1** | 读 TLM `branch_predictor.h` + `ibus.h` | 预测结果**被丢弃**（`:126-129` 注释「M2 阶段: 仅存储预测结果」+ 空函数体）· `flush_when([]{return false;})` 恒 false 演示桩（`:149-151`）· `ibus.h` **0 处**引用预测 | **TLM 的 BP 对 cycle 数零影响** |
| **E-2** | ★**临时摘掉 TLM 的 BP 注册**（`cpu_factory.h:517`）→ 重建 → 跑 Dhrystone | cycles **794478 → 794478（逐位不变）** · DMIPS **1.43277 → 1.43277（逐位不变）** | ★ **TLM 的 BP 惰性 ⇒ ADR-088 §5.3.1 的根因归因（「TLM 有 BTB ⇒ 预测命中不 flush」）被证伪** |
| **E-3** | ★**逐拍采样 CH_MEM 的 PC**（临时改 `sim_main.cpp`，已复原）统计控制转移 | **taken 控制转移 = 120065 次** · 顺序步 = 794479 | ★ **气泡 = 1 拍 / 每次 taken 转移**。★**承重证据是 TLM 交叉验证**：`914544 − 120065 = 794479 ≈ TLM 的 794478`（残差 **1** 拍）⇒ **CH_MEM 与 TLM 的全部差距被「计数到的重定向」完全吸收，不存在第二种未计入的 slowdown 机制**。⚠️「794479 + 120065 = 914544 逐位吻合」是**分类恒等式**（每 cycle 必属二者之一），**证明力为零**，已降级为自洽性检查 |
| **E-4** | 读 TLM `ibus.h:114-124` | writeback LATE 在**同一 tick 内**把 `taken ? target : pc+4` 写回 fetch 节点 ⇒ **单遍 tick 语义 ⇒ 重定向零延迟** | ★ **真根因 = 重定向延迟的结构性差异**（TLM 0 拍 vs CH_MEM 1 拍） |

⇒ **根因（修正后）**：**TLM 单遍模型重定向零代价 vs CH_MEM 两级取指每条 taken 控制转移 1 拍气泡**（120065 拍/iter 集合）。
⇒ ★ **P-6 的方向仍成立**（BP 若能提前一拍预测 ⇒ 消除这 120065 拍 ⇒ 绝对值门禁可恢复），但**其表述依据必须换掉**（不是「TLM 有 BTB」）。

### 0.3 微基准污染（v1 已发现，双审确认）

0c 的 `branch_cond` 编译成**分支链**（3 个条件分支）+ `branch_sel` 两条路径**不等长**（4 vs 5 条）⇒ **4 档动态指令数不同** ⇒ 「.text 一致 ⇒ cycle 差只来自分支方向」**被证伪**。

⇒ 另发现（metis 边界情况 #3）：**档名与硬件分支方向相反** —— `branch_sel` 的 `bnez`（cond≠0 走 taken），而 `branch_cond` mode0 返回 0 ⇒ **mode0「always_taken」实为硬件上的 never-taken** ⇒ 任何按档名推断收益的推理都会**反**。

### 0.4 结论（v2）

### 0.5 ★★ 绝对值门禁的**定量门槛**（metis 复审 §隐藏意图 1 —— v2 缺失，本 v2.1 补上）

| 项 | 数值 | 依据 |
|---|------|------|
| CH_MEM Dhrystone 现状 | **914544 cycles** / 2000 iter = 457.3 cycle/iter · DMIPS **1.24467** | E-3 实测 |
| TLM 同负载 | 794478 cycles = 397.2 cycle/iter · DMIPS **1.43277** | 实测 |
| **DMIPS ≥ 1.4 需要的 cycles** | **≤ 813083**（= 2000e6/(1757×1.4)） | 公式 |
| **必须净省** | **≥ 101461 cycles**（= 914544 − 813083） | 计算 |
| **可消除的气泡总量** | **120065**（= E-3 的 taken 转移数） | E-3 |
| ⇒ ★ **净命中率门槛** | **≥ 84.5%**（= 101461 / 120065） | 计算 |

⇒ ★★ **这个数字改变了 ② 的风险评估**：
- 「GShare 延后」**不是低风险决定** —— 它把 1.4 门禁押在「**Bimodal（无全局历史）能否在 Dhrystone 的分支混合上达到 84.5% 净命中率**」这个**未验证**的假设上。
- ⇒ **裁决 C-2（范围）须由前置2 的实测数据触发复核**：若前置2 报告的**可达命中率下界 < 84.5%** ⇒ **不得以「GShare 延后」结案** ⇒ 须回 ② 补 GShare（或另立 change）。

⇒ **② 不取消**，但**必须**：
1. 先修 0c 微基准（工作量污染 + 档名反向）⇒ **独立 change**
2. 先做 **fetch 期预测的机制 PoC**（验证「提前一拍 ⇒ 气泡消除」）⇒ **独立 change**
3. ② 主体按 **BTB + Bimodal PHT** 实现，并**同步修订 ADR-088 §5.3.1**（根因表述）

## 1. 事实基础（全部实测，附出处）

### 1.1 CH_MEM 的取指/分支时序（**BP 设计的约束条件**）

```
cycle N     fetch  NORMAL: pc_reg(=P_N) ──► imem.sread(word_addr)   ← 取指地址此刻发出
                              pc_lag <<= pc_val
            branch NORMAL: BRANCH_TAKEN / BRANCH_TARGET ← 分支结果（同拍组合产出）
            branch LATE  : pc_reg <<= select(br_taken, br_target, pc+4)   ← 决定 P_{N+1}

★ 关键: branch 阶段解析的是 **pc_lag(N) = pc_reg(N-1)** 处取来的指令,
  而 fetch NORMAL 此刻取的是 **pc_reg(N)** 处的指令.
  ⇒ **branch LATE 决定的 P_{N+1} 落后于当前取指一拍** ⇒ 那拍取的
     fall-through 指令成为**浪费的气泡**（被 FLUSH 抑制副作用）。
```

| 事实 | 出处 | 对 BP 的含义 |
|------|------|------------|
| **imem 读有 1 拍延迟**（`sread`，非 `aread`） | `ibus_chmem.h:163` + 注释 `:157-162` | ★ **BP 必须在 fetch 期（当前取指那一拍）就预测** ⇒ 挂钩点是 **fetch NORMAL**，不是 branch LATE |
| **BTB 读必须用 `aread`（0-cycle）** | 上游 `c27543d` 后 aread = 0-cycle（`dmem_chmem.h` 用它） | 若 BTB 用 `sread`，预测与真分支结果同拍到达 ⇒ **价值归零** |
| **分支结果与当前取指错开一拍** | `ibus_chmem.h:64-69` 注释（target in lag(N+2), fall-through occupies lag(N+1)） | ★ **这是「提前一拍预测」有机会消除气泡的根据** |
| **PC-mux 在 `branch` LATE** | `ibus_chmem.h:171` `update_pc` | BP 的预测结果喂给**同拍** branch LATE 的 `next_pc` mux（fetch 期查表 ⇒ 同拍消费） |
| **flush 机制已存在** | `ibus_chmem.h:167` `n->operator()(FLUSH) = br_taken_buf` | misprediction ⇒ **复用**该通道 |
| **flush 的消费者（3 个）** | `branch_chmem.h:233` · `dmem_chmem.h:129` · `reg_file_chmem.h:222` | ★ **flush 语义必须从「上拍 taken」改为「预测错误」** —— 否则正确预测后 FLUSH 仍为 1 ⇒ 会把**目标指令**的写回/存储/分支决策一起抑制 ⇒ 静默功能错误 |
| **无 stage 寄存器**（全组合） | `cpu_factory_chmem.h`；`pc_reg`/`pc_lag`/`br_taken_buf` 是**唯一**的 `ch_reg` | BP 的表状态**必须**用 `ch_mem`/`ch_reg` 承载（**不需要** ADR-046 FSM 豁免） |

### 1.2 CH_MEM 的分支惩罚（**★ 已实测，替换 v1 的推断**）

| 项 | v1 的说法 | ★ **实测（E-3）** |
|---|---|---|
| 差距 | 120066 cycles / 2000 iter = 60 拍/iter | **120065 次 taken 控制转移 / 2000 iter = 60.03 拍/iter**（数字巧合，实测值） |
| 归因 | 「30 分支 × 2 拍 flush 代价」 | ★ **「120065 次 taken 转移 × 1 拍气泡」** —— 逐拍验证：794479（顺序步）+ 120065（气泡）= **914544 = CH_MEM 总 cycles（逐位吻合）** |
| 「30 分支」 | 被当作已知量 | ★ **循环论证**（30 是从「2 拍」反推的）；真实转移数是 **120065**（实测） |
| 根因 | 「TLM 有 BTB ⇒ 预测命中不 flush」 | ★ **证伪**（E-1/E-2）：TLM 的 BP 惰性（摘掉注册后 cycles 逐位不变）⇒ 真因是 **TLM 单遍 tick 重定向零延迟 vs CH_MEM 两级取指每条 taken 转移 1 拍气泡**（E-4） |

⇒ ★ **对 BP 的意义**：要补回这 **120065 拍**，BP 必须**消除那 1 拍气泡** ⇒ **必须提前一拍预测**（§1.1 的错位事实）⇒ **oracle R1 的机制是必要的**。

### 1.3 ★★ 0c 微基准的**前提被实测推翻**（本次新发现）

| 项 | 实测 | 出处 |
|---|---|---|
| **`branch_cond` 编译成分支链** | `beqz` + `beq`×2（**3 个条件分支**）⇒ **4 档走的路径长度不同** | `objdump build/branchbench_m0.elf` |
| **各档动态指令数不同** | mode0: 3 条 / mode1: 5 条 / mode2: 10 条 / mode3: 8 条 | 同上（`branch_cond` 逐档推演） |
| **`branch_sel` 两条路径不等长** | taken: `bnez`+`mv`+`mv`+`ret` = **4**；not-taken: `bnez`+`mv`+`j`+`mv`+`ret` = **5** | `objdump` `branch_sel` |
| ⇒ **0c 的 cycle 差混入工作量差** | 0c AC-2 的论证「.text sha256 一致 ⇒ 指令数相同 ⇒ cycle 差只来自分支方向」**不成立**（`.text` 一致只保证**静态**代码一致，不保证**动态**指令数一致） | — |

⇒ ★ **0c 的 4 档基线数字（66034 / 68034 / 79534 / 73012）不能直接用于 ② 的 AC-8 判据**。

### 1.4 Task 1 的 7 条机制结论（PoC 实测，**实现的地基**）

| # | 结论 | 对实现的约束 |
|---|------|------------|
| 1 | C++ 侧常量 ⇒ elaboration 期常量 | 预测方向/目标/写使能必须是 ch 节点（ADR-040 §10.5） |
| 2 | **mem 端口地址必须来自「已被驱动」的信号**（payload 派生 ✓；**未被驱动**的 reg ⇒ 索引 0 ⇒ 静默错；**被 op 节点 `<<=` 驱动**的 reg ✓） | BTB/PHT 写地址 = `bits<...>(payload PC)`；BTB 读地址 = `bits<...>(pc_reg)` |
| 3 | 读/写共用同一地址表达式 | BTB 读/写端口用同一 `bits<>` 表达式 |
| 4 | enable 可以是 reg proxy | 写使能用 payload 里的真实 `BRANCH_TAKEN` |
| 5 | 2-bit PHT 可行 | `ch_mem<ch_uint<2>, N>` |
| 6 | 越界索引读 = 0 | 「固定容量 + 运行时逻辑条目数」 |
| 7 | `ch_reg` 跨拍保持成立（1→3→7） | GHR 13-bit 可行（brief §5.2 P-1 待验项**通过**） |

---

## 2. 裁决 A：BP 在 CH_MEM 的**收益模型**（★ v2 已按 oracle R1 重写）

### 2.1 ★ 挂钩点与**完整时序**（v2.1 按 oracle 复审 + metis 复审综合修订）

#### 2.1.1 三个「PC 角色」必须分清（★ v2.1 的核心澄清）

| 符号 | 含义 | 何时可知 |
|------|------|---------|
| `pc_reg(N)` | **本拍正在取**的指令地址 | fetch NORMAL 读到它 |
| `pc_lag(N) = pc_reg(N-1)` | **本拍正在解析**的指令地址（branch 阶段解析它） | branch NORMAL 解析它 |
| `pc_reg(N+1)` | **本拍 branch LATE 决定的**下一取指地址 | branch LATE(N) 写 |

⚠️ ★ **关键澄清**（oracle 复审 Q1 与 metis 复审 §歧义 1 的分歧点，architect 逐拍重推后的裁定）：

- `branch LATE(N)` 消费的是 `actual_taken(N)` = **instr[pc_reg(N-1)]** 的分支结果（因为 branch NORMAL(N) 解析的是 `pc_lag(N)`）；
- 而 `fetch NORMAL(N)` 发起的取指是 **pc_reg(N)**（下一拍才解析）；
- ⇒ ★ **BTB 在 fetch NORMAL(N) 查的是 `pc_reg(N)`，而 branch LATE(N) 的 actual 是 `pc_reg(N-1)`** ⇒ **两者不是同一条分支**；
- ⇒ ★ **但两者都作用于 `pc_reg(N+1)`** ⇒ 所以它们**可以**放在同一个 mux 里（优先级：`actual` > `BTB 预测` > `pc+4`）⇒ **oracle 的机制主张成立**；
- ⇒ ★ **metis 指出的「必须抑制该分支自身 redirect」完全正确** —— 若不抑制，正确预测的 taken 分支会让 **target 指令被取两次**（`pc_reg(N+1) = target`，而 `pc_reg(N+2)` 又被 actual redirect 拉回 `target`）⇒ **Δcycles ≈ 0 甚至更差** ⇒ 会**误判「机制不成立」**。★ **v2 的伪码缺这一步 ⇒ 已在本 v2.1 补齐。**

#### 2.1.2 完整三步时序（★ 实施者必须照此实现；★ v2.2 修正纠错作用域）

> ★★ **v2.2 关键修正（metis 三审 A2）**：v2.1 的伪码把「预测不一致 ⇒ 纠错到 `pc_lag+4`」**不分情形** ⇒ ★ **情形 B（预测 not-taken / 实际 taken）是暖机主路径**（PHT counter=1 ⇒ 预测 not-taken，而分支恒 taken）⇒ 照旧实现会**走错路径** ⇒ **校验和不等 ⇒ AC-7/AC-8a 必 RED**。
> ⇒ ★ **纠错 MUST 分情形**（见下）。

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
          => ★★ **flush：MUST 抑制！**（★ v2.2 修正：此处的 fall-through 是
             「分支实际 taken ⇒ 被跳过的非法路径」⇒ 它的 store/写回 MUST 被抑制
             ⇒ **不是**「合法路径、无副作用」）
          => ★ 这条是**暖机主路径**（PHT counter=1 ⇒ 预测 not-taken）

    ★★ 情形 C（预测 taken / 实际 taken 且 target 一致）—— **必须抑制自身 redirect**
        pred_valid && pred_taken && actual_taken(N+1) && pred_target == actual_target(N+1)
          => 抑制该分支自身 redirect  (否则 target 被取两次 ⇒ Δcycles≈0 ⇒ 误判「机制不成立」)
          => ★ **flush：MUST NOT 抑制**（投机 taken 臂生效 ⇒ fall-through
             **根本没被取** ⇒ 无错路指令 ⇒ `bp_spec_taken_buf` 拉高 ⇒
             taken 分量被门控掉）

    ★★ 情形 D（预测 not-taken / 实际 not-taken）
        => 无需任何动作（顺序取指本就正确）；★ **flush：MUST NOT 抑制**
           （分支未 taken ⇒ fall-through 是合法路径）

    ★★ 情形 E（BTB miss ⇒ 未投机 / 分支实际 taken）—— ★★ **MUST 抑制！**
        !pred_valid && actual_taken(N+1)
          => fall-through **确实被取了**（顺序取指）且它是错路 ⇒ store/写回
             MUST 被抑制 ⇒ 与「无 BP」时的 `flush = taken_buf` 行为**一致**
          => ★ 这是「BTB miss 时不得破坏程序语义」的关键情形

branch LATE(N+1):
    ★ 写 pc_reg(N+2)  —— ★★ **4 项显式 select 链**:
        select( 情形A, pc_lag(N+1) + 4,                        <- 纠回 fall-through
          select( 情形B, actual_target(N+1),                   <- 跳到真实目标 (暖机主路径!)
            select( 情形C, pc_reg(N+1) + 4,                    <- 抑制自身 redirect (顺序推进)
              select( actual_taken(N+1), actual_target(N+1),   <- 无预测时的正常行为
                      pc_reg(N+1) + 4 ) ) ) )
```

⇒ ★ **收益**：正确预测的 taken 转移（情形 C）⇒ `pc_reg(N+1)` 直接 = target ⇒ **fall-through 那一拍根本不被取出** ⇒ **气泡完全消除**（实测每条 taken 转移 1 拍气泡）。

⇒ ★ **误预测代价（v2.2 修正，architect 逐拍重推）**：

| 情形 | 描述 | 相对无 BP 的代价 |
|------|------|----------------|
| **A** | 预测 taken / 实际 not-taken | **1 拍**（多取了一拍错误 target；纠错回到 fall-through 该拍本来就要取） |
| **B** | 预测 not-taken / 实际 taken | **0 拍**（与无 BP 行为相同：都取 fall-through，再跳到实际目标） |

⇒ ★ **净收益模型**：`Δcycles(off − on) = N_correct×1 − N_false_A×1`（★ **N_false 只计情形 A**；情形 B 代价 0 ⇒ **MUST NOT** 计入）
⇒ ★ **净收益率** := `Δcycles(off − on) / N_taken_total`
⇒ ★ **⇒ 84.5% 门槛的推导**：若不计情形 B，则 净收益率 = `(N_correct − N_false_A) / N_taken_total` ⇒ **不是** `2×命中率−1`（该式仅在 N_false_B≈0 时成立）⇒ ★ **门槛的等价命中率须按实测的 N_false_B 修正，MUST NOT 硬编码 92.25%**。


### 2.2 「BTB-only 会净负收益」（oracle R2）⇒ 必须有方向预测

BTB 只回答「这个 PC **曾经**是分支、目标在哪」，**不回答方向**。若无 PHT：

- BTB 命中 ⇒ 只能猜「taken」；
- 对「曾经 taken、本次 not-taken」的分支 ⇒ **凭空制造误预测**（气泡），而**无 BP 时该分支零代价**（`pc+4` 本来就对）⇒ **净负收益**。

⇒ ★ **裁决 B-2**：② 必须含 **Bimodal 2-bit PHT**（成本极低 —— PoC P-3 已验证 `ch_mem<ch_uint<2>, N>` 可行），GShare（全局历史）与 RAS 延后。

### 2.3 ★ 误预测的纠错（oracle R4.3：必须用 `pc_lag+4`）

「预测 taken、实际 not-taken」时，`pc_reg` 此刻**已经是错误 target** ⇒ 纠错目标**不是** `pc+4` 而是 **`pc_lag + 4`**（真正的 fall-through）⇒ 需 1 拍延迟的预测记录（`ch_reg` 组：pred_taken / pred_target / pred_valid —— PoC 结论 7 已验证 `ch_reg` 跨拍保持可行）。

### 2.4 ⇒ 裁决 A-3：0c 微基准**必须先修**（**② 的前置**）

**修法（已验证可行）**：

| 问题 | 修法 | 验证 |
|------|------|------|
| `branch_cond` 生成分支链（4 档路径长度不同） | 改**位运算 select**（`eq0&c0 \| eq1&c1 \| eq2&c2 \| eq3&c3`）⇒ 编译成**纯无分支** | ✅ 实测 `andi/sltiu/seqz/and/or`，**0 个分支** |
| `branch_sel` 两条路径不等长（4 vs 5） | taken 路径补一条 `j` ⇒ 两条路径**各 4 条** | ✅ 实测 `bnez/mv/j` + `mv/mv/ret`，**等长** |
| ★ **档名与硬件方向相反**（metis 边界情况 #3） | `bnez` 在 cond≠0 时 taken，而 mode0 的 `branch_cond` 返回 0 ⇒ **mode0 实为 never-taken** | ★ 须**重命名档位**或修正注释，否则后续按档名推断收益的推理全反 |

⇒ **产出**：0c 基线 CSV **必须重录**（数值会变；四档**可能趋平** ⇒ 跨档判据的信号强度需重新评估）。

### 2.5 ⇒ 裁决 A-4：收益**必须先实测**（**② 的前置 PoC**）

**在写完整 BP 之前**，先做**机制 PoC**：验证「fetch 期提前一拍预测 ⇒ 气泡消除」这个机制本身是否成立。

| 判据 | 动作 |
|------|------|
| PoC 后 | Dhrystone 的 **taken 控制转移数**（E-3 的方法）应**显著下降**（理想：接近 0），且 **总 cycles 下降 ≈ 下降的气泡数** |
| 反证 | 若 taken 转移数**不变** ⇒ 提前预测**没消除气泡** ⇒ 机制不成立 ⇒ **立即上报 architect**（不得继续写完整 BP） |

⇒ **裁决 A-4 的判据框架（修正 v1）**：v1 的「净收益 ≈2 拍 ⇒ P-6 成立 / ≈1 拍 ⇒ P-6 不成立」是**循环论证**（30 分支是从 2 拍反推的）。★ **修正后的判据**：
- **单转移收益**由 E-3 已知 = **1 拍气泡/次 taken 转移**（实测，非推断）
- **可恢复量** = 「能正确预测的 taken 转移数 × 1 拍」⇒ 需 BP 的**命中率**数据
- ⇒ **P-6 是否成立** = 「BP 命中率 × 120065 拍」是否足够补回与 1.4 门禁的差距 ⇒ 由 architect 在 PoC 数据到手后裁决

### 2.6 ⇒ 裁决 A-5：flush 语义变更的**影响面**（★必须显式登记）

flush 从「上拍 taken」改为「**预测错误**」是**语义变更**，波及 **3 个消费者**：

| 消费者 | 现状 | 变更后的要求 |
|--------|------|-------------|
| `branch_chmem.h:233-235` | `taken = taken && !flush` | 「预测错误」⇒ 该分支决策**必须**被抑制 |
| `dmem_chmem.h:129` | 抑制误取指令的 store | 同上 |
| `reg_file_chmem.h:222` | 抑制误取指令的写回 | 同上 |

⇒ ★ **回归锚**：`[chmem]` vendored ELF **47/47** + `[verilator]` **48 ELF** + branchbench 4 档 ⇒ 这三套**天然覆盖**「错误 flush 会破坏正确路径」的全部失效模式。
⇒ ★ **必须补一条判据**（metis 建议）：**BP on/off 的 4 档校验和逐位相等**（确定性 · 零阈值 · 且能直接抓出 flush 语义改错 —— 目标写回被抑制 ⇒ 校验和必变）。

## 3. 裁决 B：BP 的**架构形态**（v2 已按 oracle R1/R2/R4 + oracle 建议 6 修订）

### 3.1 ⇒ 裁决 B-1：② 交付 **BTB + Bimodal 2-bit PHT**（GShare / RAS 延后）

| 项 | 裁决 | 依据 |
|---|------|------|
| ② 的范围 | **BTB（tag+target+valid）+ Bimodal 2-bit PHT（方向）** | ★ BTB-only 无方向 ⇒ 猜「恒 taken」⇒ 在 not-taken 分支上**净负收益**（§2.2 / oracle R2） |
| **不含** GShare | 延后 | 相对 Bimodal 的**边际收益待 PoC 数据**（GHR 只在全局历史有额外相关性时才有增益）；PHT 索引改用 `PC bits`（Bimodal 语义） |
| **不含** RAS | 延后 | RAS 服务于**多 call-site** 的 `jalr x0, 0(x1)` 返回预测 ⇒ 与 BTB 正交 ⇒ 可独立立项。★ v1 说「CH_MEM 无 RAS 对应物」**不准确**（`ch_mem` 栈 + `sp` + decode 的 `rd/rs1=x1` 检测即可实现）—— **延后本身合理，理由须改** |
| 「方向预测在 CH_MEM 无收益」 | ★ **撤回该论断** | 方向预测在「提前一拍」的模型下**恢复标准价值**（§2.2） |
| 台账处置 | BTB 相关条目 → 升级；**PHT/Bimodal 相关**条目 → 升级；**GHR/RAS/TID** 条目 → 保持 `not-covered` + 差异化理由 | ★ metis 修正：`predict_after_update` / `no_predict_initially` 是 **BTB 也有**的语义（冷启动/更新路径）⇒ **不可**归入「方向预测无收益」 |

### 3.2 ⇒ 裁决 B-2：BTB 结构（v2.1 修正 oracle 复审 Q3 的两处错误）

| 项 | 决策 | 依据 |
|---|------|------|
| 布局 | ★ **62b 打包**：`{tag30 = PC[31:2], target30 = target[31:2], valid(1b), is_unconditional(1b)}`（单 word 64b，余 2b 保留） | ★ **v2.1 的「全 32b PC + valid 偷 target[1:0]」不可实现**（valid 恒 false，见 valid 行）⇒ v2.2 改为 62b；★ 依据：**CH_MEM 只解 32 位指令**（`decode_chmem.h:82`）⇒ `PC[1:0]=0`、target 4 字节对齐 ⇒ 低位恒 0 可安全裁掉 |
| valid 语义 | ★ **独立 valid 位**（打包进 word）⇒ `ch_mem` 零初始化 ⇒ valid=0 ⇒ **天然无效 ⇒ 不会假命中**。★ **MUST NOT** 用「`target[1:0] != 0`」判有效 —— target 4 字节对齐 ⇒ `[1:0]` **恒 0** ⇒ 该判据**恒 false** |
| 物理深度 | ★ **256（编译期，`ch_mem<ch_uint<64>, 256>`）** | `ch_mem` 的 depth 是**编译期模板参数** ⇒ 不可变 |
| 逻辑条目数 M | ★ **运行期 mask**：索引 = `bits<7,0>(pc) & literal(M-1)`，M ∈ {16,32,64,128,256} | ★ **v2 的「`bits<N-1,0>(PC)`，N 运行时可变」不可实例化**（`bits<>` 边界是**编译期**模板参数）⇒ 修正为「编译期 8-bit 索引 + 运行期 AND mask」 |
| ⇒ 「运行时可配」的正确论证 | ★ **不是** PoC 结论 6（「越界读 = 0」只是**安全网**，不是可配机制）⇒ 而是：**物理 256 固定 + 运行期 mask 限制有效条目数** ⇒ M=16 时索引只用 PC 低 4 位 ⇒ 逻辑上只有 16 项有效 | ⇒ 台账 4 条（`custom_btb_size` / `btb_runtime_{16,64,256}`）**可升 covered**，但 AC 须含「分别以 M=16/64/256 例化 BP 并验证功能正确 + 容量语义可观测」的 CH_MEM 侧 case |
| tag 宽度 | ★ **`PC[31:2]`（30b）** —— ★ **等价于全 32b PC 的判别力**（`PC[1:0]` 恒 0 ⇒ 无信息损失）⇒ 小容量（M=16）下**不产生结构性假命中** |
| 替换策略 | ★ **直接覆盖**（同槽直接写） | TLM 侧 `branch_predictor.h` 亦无替换策略（直接写）⇒ 对齐 |
| 别名风险 | ★ 256 槽 direct-mapped ⇒ 热点分支互驱逐 ⇒ ★ **前置2 须报告「去重后的热点分支数」** ⇒ 若 > 256 ⇒ 容量裁决提前做 | metis 边界情况 #5 |
| **BTB 只对直接分支建项** | ★ **`jalr`（间接跳转）不建 BTB 项** ⇒ 走 not-taken | ★ metis 缺失 AC #7：jalr 目标每次可能不同 ⇒ BTB 存「上次目标」必错 ⇒ 显式裁定并绑到台账 #9 终态 |
| ★ **`is_unconditional` 位（★ v2.2 新增，oracle 复审发现 2）** | ★ `jal` **不占 PHT 表项** ⇒ 若无此位则其方向**无来源** ⇒ **永不被预测** ⇒ call 气泡不可恢复 ⇒ 此位**必需** ⇒ `pred_taken = pred_valid && (is_unconditional \|\| pht_counter ≥ 2)` |
| **陈旧目标自愈** | ★ **直接覆盖策略 ⇒ 目标变化的分支再次 taken 时同槽重写 ⇒ 天然自愈**；★ **MUST NOT** 另设误预测计数器 | ★ v2.2 修正（与 AC-18 一致）：计数器的复杂度 > 收益，且直接覆盖已语义等价自愈 |
| key 是 vaddr 还是 paddr | ★ **vaddr**（分支 PC payload 本身就是 vaddr） | metis 边界情况 #4：MMU on/off 之间 key 必须一致 |

### 3.3 ⇒ 裁决 B-3：PHT 结构（v2.1 补 metis 的实现不变量）

| 项 | 决策 |
|---|------|
| 表 | `ch_mem<ch_uint<2>, N>`（N = 256）⇒ **2-bit 饱和计数器**（PoC P-3 已验证可行） |
| 索引 | ★ **Bimodal 语义**：`bits<7,0>(PC) & literal(M-1)`（**不用**全局历史 XOR）⇒ GShare 延后 |
| 饱和 | 0→1→2→3 饱和（不回退），**≥2 ⇒ 预测 taken** |
| 冷启动 | 初值 0（弱 not-taken）⇒ 冷启动语义由 `no_predict_initially` 条目覆盖 |
| ★ **PHT 更新使能** | ★ **「分支已解析」（任何方向）** —— ★ **不是**「仅 taken」！若只在 taken 时更新 ⇒ PHT 学不到 not-taken ⇒ 在 not-taken 分支上恒预测 taken ⇒ **净负收益**（§2.2 的老问题换个入口回来） |
| ★ **BTB 更新使能** | ★ **taken**（目标此时才已知） |
| ★ **更新地址** | ★ **分支解析处的 `KT::PC`**（= 预测时的 `pc_reg`）⇒ ★ **二者必须同指一条分支**（索引对齐不变量）；若误用 branch 阶段的当前 `pc_reg`（下一条 PC）⇒ **off-by-one ⇒ 学到错误表项** |
| ★ **无条件跳转不占 PHT 表项** | `jal`/`jalr` 恒 taken ⇒ 不需要方向预测 ⇒ 不更新 PHT（只更新 BTB） |
| ★ **不变量的可测性** | 需一条 AC：「**同一分支第二次被取时预测命中**」（覆盖 update 路径） |

### 3.4 ⇒ 裁决 B-4：预测的**产出与消费**（payload 契约 + ★ mux 嵌套）

#### 3.4.1 payload 契约

| 方向 | 决策 |
|------|------|
| BP 的**输入** | fetch 期的 `pc_reg`（**被 op 节点 `<<=` 驱动** ⇒ 可作 mem 读地址，PoC 结论 2 + P-2P） |
| BP 的**输出** | ① `pred_target`（32b）② `pred_taken`（1b）③ `pred_valid`（1b）⇒ **写入 payload**（mode-aware `uint_t<N>`，禁裸 `bool`，ADR-040 §10.5） |
| 误预测记录 | `ch_reg` 组（pred_taken / pred_target / pred_valid）⇒ 跨拍保持（PoC 结论 7） |
| 纠错目标 | 「预测 taken、实际 not-taken」⇒ **`pc_lag + 4`**（§2.1.2） |
| ★ **顺序契约** | ★ **跨 stage**（生产者 fetch NORMAL → 消费者 branch LATE）⇒ 保序机制 = **`elaborate()` 按 `canonical_stage_order()` 执行**（`pipe_builder.h:413-424` + `:215-224`：stage 首现序 × EARLY→NORMAL→LATE）⇒ fetch ≺ branch 且 NORMAL ≺ LATE ⇒ **与插件注册顺序无关** ⇒ v2 简报写的「同 stage 名 / EARLY→LATE」**表述错误，已更正** |
| ★ **pc_reg 单驱动纪律** | BP **只写 payload**，**禁止**自身 `<<= pc_reg`（`pc_reg` 的唯一写者是 `update_pc`） |

#### 3.4.2 ★★ `next_pc` 的**五级嵌套契约**（oracle 复审建议 8 —— **BLOCKER，v2 完全遗漏**）

**背景（时序倒挂 ⇒ 本简报是规格的责任方）**：
- `fetch_stall_protocol_v0.11.0.md` **rev.14 定稿**（critic 十五轮 APPROVE）规定 `next_pc` MUST 为 **`trap > branch > freeze > sequential`**（`:201` / `:349` / `:603`）；
- **R11.1** 裁决把 ECALL/EBREAK/MRET 的 trap redirect 交给 `csr-chmem-trap-pc-jump`，★ **插入点 = `ibus_chmem.h:171-185` 的 `update_pc`**（protocol `:603`）—— **与 BP 的插入点同一处**；
- BP 已 defer 到 v1.0.0（2026-10-19 S3 决策），而 trap/freeze mux 在 v0.11.0 Phase 3.5/C.4（~2026-11）落地 ⇒ ★ **BP 落地时那 5 项 mux 已存在** ⇒ **嵌套规格只能由本简报给出**；
- 且 protocol 明确写了「该 pc_reg mux 只含 stall 分量，**不含 BP 分支**」（`mmu-chmem-phase-b-c-launch/tasks.md:39`）⇒ **本简报补的就是那个缺口**。

⇒ ★ **裁决 B-4：五级优先级**

```
next_pc = select( trap_redirect,              trap_target,        ← 最高 (protocol rev.14)
              select( branch_actual,           actual_target,      ← 分支实际结果 (含误预测纠错)
                select( freeze_hold,           hold_pc,            ← stall 期间 PC 保持
                  select( bp_predict,          bp_target,          ← ★ 本简报新增 (BP 投机)
                           pc + 4 ) ) ) )                        ← 顺序取指
```

| 项 | 裁决 |
|---|------|
| **BP 预测项的位置** | ★ **在 `freeze` 之下、`seq` 之上** | 
| 理由 1 | `freeze`（PTW stall）期间 PC 必须**保持**；若 BP 预测项在 freeze 之上 ⇒ 预测会在 walk 期间把 PC 推走 ⇒ 恢复路径要多记一份 pre-hold PC ⇒ 复杂度 > 收益 |
| 理由 2 | ★ 不违反 protocol rev.14 已 APPROVE 的不变量（critic 十五轮磨出来的，不应由本 change 单方面改） |
| ★ **FLUSH 的判据（★ v2.2 唯一权威规则 —— metis 四审 M1：v2.1/v2.2 的「替换分量」写法**是错的**）** | ★ **MUST NOT** 简单把 `br_taken_buf` 分量替换为 `mispredict_buf` 分量 ⇒ 那会让「没投机」的 taken 分支（BTB miss / 情形 B / `jalr`）的 fall-through **副作用泄漏**（store / 写回）⇒ 校验和不等 ⇒ AC-7/AC-8a RED。★ **正确做法 = 门控**（见下行） |
| ★ **门控规则（唯一权威）** | ```
flush(N) = hold_z(N)                                          ← 停顿分量（protocol §2.2）
         | ( taken_buf(N) && !bp_spec_taken_buf(N) )           ← fall-through 被取过 + 分支实际 taken ⇒ 抑制
         | mispredict_A_buf(N)                                ← 投机 taken 但实际 not-taken ⇒ 抑制错 target
```

★ **关键性质**：`bp_spec_taken_buf(N)` = 上一拍 BP 的投机 taken 臂**确实生效**了
  ⇒ fall-through **根本没被取** ⇒ 不需抑制。
⇒ ★ **BP 关闭时 `bp_spec_taken_buf` 恒 false ⇒ `flush == taken_buf` ⇒ 与现状逐位相同**
  ⇒ **零退化**（这是判据能成立的前提）。
⇒ ★ **MUST NOT** 简单地把 `br_taken_buf` 分量「替换」为 `mispredict_buf` 分量 ——
  那会让「没投机」的 taken 分支（BTB miss / 情形 B / `jalr`）的 fall-through
  副作用**泄漏**（store / 写回）⇒ 校验和不等 ⇒ AC-7/AC-8a RED。
⇒ ★ 正确的做法是**门控**：taken 分量**仅在「投机未生效」时**才抑制。 |
| **mispredict 纠错的位置** | 与 `branch_actual` **同层**（都是「分支实际结果」的一部分）⇒ `mispredict` 优先于 `branch_actual`（纠错目标 `pc_lag+4` vs actual target） |
| **本简报的义务** | ★ **上述嵌套规格必须写进 ② 主体的 `design.md` 与 AC-6 的判据对象**（否则 ② 落地时与 protocol 冲突） |

### 3.5 ⇒ 裁决 B-5：**不做**的事（★防超范围）

| 不做 | 理由 | 归属 |
|------|------|------|
| GShare 全局历史 | 边际收益待 PoC 数据 | 后续 change（**不预先立项**） |
| RAS call/ret 预测 | 与 BTB 正交 | 后续 change |
| 多级 TLB / PTW | 与本 change 无关 | ⑤ 批 1/2 |
| **改 CH_MEM 的取指延迟**（imem 改 `aread`） | 上游 `c27543d` 已对齐 aread/Verilog 0-cycle，imem 用 sread 是**刻意**保持 `pc_lag` 对齐的决策（`ibus_chmem.h:157-162`） | — |
| **把 CH_MEM 改造成带 stage 寄存器的流水线** | 等于重做整个 CH_MEM 数据通路，与 ADR-088 §3「M 扩展 = CH_MEM 组合式为规范」冲突 | — |
| **在 PoC 数据前就写完整 BP** | 机制可行性未验证 ⇒ 可能造出「测试能绿、性能零变化」的惰性 BP | — |

## 4. 交付顺序（**v2 修订后的执行图**）

```
┌─ [前置0] TLM-BP 对照实验  ── ✅ **已完成**（E-2，architect 亲做，代码已复原）
│         └─ 结论: TLM BP 惰性 ⇒ ADR-088 §5.3.1 根因表述须改（§6）
│
├─ [前置1] chmem-branch-microbench-fix  ── 修 0c 的工作量污染 + 档名反向
│         ├─ branch_cond → 位运算 select（0 分支）      ✅ 已验证
│         ├─ branch_sel 两条路径补齐等长              ✅ 已验证
│         ├─ ★ 档名与硬件方向核对 + 重命名
│         ├─ ★ 报告 4 档的 taken 分支数/iter（信号强度）
│         └─ 重录 4 档基线 CSV
│
├─ [前置2] chmem-bp-btb-gshare-poc  ── ★ **机制 PoC**（验证「提前一拍 ⇒ 气泡消除」）
│         ├─ 最小 BTB（fetch NORMAL aread + branch LATE mux）
│         ├─ 测：taken 控制转移数（E-3 方法）+ 总 cycles
│         └─ ★ 判据: 转移数不降 ⇒ 机制不成立 ⇒ **立即上报 architect**
│
└─ [主体]  chmem-bp-btb-gshare  ── BTB + Bimodal PHT 正式实现
           ├─ ① BP plugin（BTB 256×64b + PHT 2-bit）
           ├─ ② fetch NORMAL 预测 + branch LATE mux 接入
           ├─ ③ flush 语义变更（taken → mispredict）+ 3 个消费者回归
           ├─ ④ BP 开关面（工厂参数 + CLI flag）
           ├─ ⑤ 台账逐条重判（含族级 §2 + 计数同步）
           └─ ⑥ 性能验收（用前置1 的新基线）
```

| change | 规模 | 前置 | 归属 | AC |
|--------|------|------|------|-----|
| `chmem-branch-microbench-fix` | 小 | 无 | **独立**（0c 的修正，不属 ②） | 见 §4.1 |
| `chmem-bp-btb-gshare-poc` | 小 | 前置1 | **独立**（事实测量，产出数据供 architect 裁决） | 见 §4.2 |
| `chmem-bp-btb-gshare` | **中**（比原「大」小） | 前置1 + 前置2 | **② 主体** | 见 §4.3 |

⚠️ **裁决**：**前置1 与前置2 未完成前，② 主体不得开工**（AC-8 的判据与 P-6 的前提都依赖它们）。

### 4.1 前置1 `chmem-branch-microbench-fix` 的 AC（v2.1 修正）

- [ ] **AC-F1-1** `objdump` 证明 `branch_cond` **0 条条件分支**（纯 `sltiu/seqz/and/or`）+ `branch_sel` 两条路径**指令数相等**（★ 仅证**静态**一致）
- [ ] **AC-F1-2** ★ **改用计数器**证明 4 档**动态指令数相等**（★ `.text` 相同 + objdump **无法**证动态一致 ⇒ 需 CH_MEM 侧「已执行指令数」计数器 ⇒ 依赖 AC-15 的常驻可观测点）⇒ 并报告 4 档的 **taken 分支数/iter**
- [ ] **AC-F1-3** 新 `.text` sha256 入册 + CSV 重录 + CI `branchbench_baseline` 仍绿
- [ ] **AC-F1-4** ★ **档名与硬件方向一致** —— ⚠️ **mode0 与 mode1 是「互换」不是单改名**（源码 `c0 = 0` ⇒ mode0 永不 taken；`c1 = 1` ⇒ mode1 恒 taken，而 `branch_sel` 的 `bnez` 在 cond≠0 时 taken）⇒ ★ **强制互换重命名**（**删掉「修正注释」这个弱选项**）+ ★ **改名传播清单**（同 commit）：CSV `mode_name` 列 · `tools/measure_branchbench.sh` 的 `MODE_NAMES` 数组 · plan Task 6 Step 4 的档名引用 · 文档旧档名
- [ ] **AC-F1-5** ★ **留痕**：0c 的 AC-2「`.text` sha256 一致」**事实仍成立**，但其推论「`.text` 一致 ⇒ 动态指令数相同」**被本次反汇编证伪** ⇒ 写入本 change 的 proposal + CSV notes（**不得**改动归档产物）
- [ ] **AC-F1-6** ★ **预登记默认出口**（★ 删除「或新增密度可调负载」这个开放式口子）：若跨档 taken 转移数差 **≤ 1 拍/iter**（★ **含恰好 1.0** —— ★ **本基准每 iter 恰 1 条条件分支 ⇒ 跨档差恰好 = 1.0 ⇒ 该条件对本基准恒真 ⇒ ★ 跨档判据（AC-8d / AC-8e）结构性不可用 ⇒ 主体性能判据 MUST 退化为「AC-8a + AC-8b-1 单档等式」）⇒ **直接**采用「`always_taken` 单档绝对 Δ + 解析模型」判据 ⇒ ★ **判定权归本 change**（独立执行者）⇒ 结论写入 CSV notes + 本 change 归档产物 ⇒ **不留给 ②**（② 对「判据不可用」有既得利益）
- [ ] **AC-F1-7** ★ **改名与脚本改动必须同一 commit** ⇒ 否则 `measure_branchbench.sh:52` 的 `grep -E "^${name},"` 找不到行 ⇒ CI `branchbench_baseline` 立刻红

### 4.2 前置2 `chmem-bp-btb-gshare-poc` 的 AC（★ v2.1 重写 —— v2 的判据**物理上不可达**）

> ★ **v2 的判据错误**（metis 复审 §歧义 3）：v2 说「PoC 后 taken 转移数应显著下降」⇒ ★ **taken 转移数在 BP 生效时是**不变量**（BP 生效后 PC 仍在每条 taken 分支上跳转；误预测还会**增加**非顺序跳转）⇒ 该判据的「成立」分支**不可达** ⇒ 必须改判据。

- [ ] **AC-F2-1** 被测对象与工作负载明确 ★ **必须含一个「恒 taken」定向档**（★ 否则 BTB-only 在 not-taken 密集的负载上净负 ⇒ 机制会被误判「不成立」）
- [ ] **AC-F2-2** ★ 测的量 = **FLUSH 事件数**（★ 对取指延迟语义变更鲁棒）+ **总 cycles** + **host 侧 PC 序列**（查重复 PC）—— **不再用「taken 转移数」作判据**
- [ ] **AC-F2-3** ★ **决策表两条分支各自的后果（终局式）**：
  - **成立**（FLUSH 事件数显著下降 + cycles 下降 ≈ 下降的 FLUSH 数）⇒ 进 ② 主体
  - **不成立** ⇒ ① 记录 ⇒ ② 在 **ADR-088 §5.3.2** 落**结论**（绝对值门禁恢复 / **不恢复**）⇒ ③ **不恢复时沿用 §5.3.2 已登记的 P-3 临时口径**（cycle 比率 ≤ 1.15 + notes 标注）⇒ ④ **本 change 关闭**，**不留悬空项**
- [ ] **AC-F2-4** ★ **兜底路径三要素**（触发阈值 + 落点文件 + ★ **可判定的时限**：「≤ N 个工作日」或「本 change 内闭环」）—— 继承 plan R6
- [ ] **AC-F2-5** ★ 显式声明「**本 PoC 不产出 P-6 结论**」（P-6 结论归 architect）
- [ ] **AC-F2-6** PoC 的代码**不复用**为 ② 主体（避免「半成品进主体」）
- [ ] **AC-F2-7** ★ **报告「去重后的热点分支数」** ⇒ 若 > 256 ⇒ 容量裁决提前做（§3.2）
- [ ] **AC-F2-8** ★ **报告「可达净命中率下界」** ⇒ 与 **84.5% 门槛**（§0.5）对照 ⇒ ★ **若下界 < 84.5% ⇒ 不得以「GShare 延后」结案**
- [ ] **AC-F2-9** ★ **测量设施产品化**：E-3 的「临时改 `sim_main.cpp`」必须变成**常驻输出**（`--stats`）⇒ 否则每个 change 重做一次性 hack，测量结论不可复核
- [ ] **AC-F2-10** ★ **TLM 侧 cycle 数同时输出** ⇒ 让「差距全吸收」交叉验证成为**可重复判据**（而非一次性手工核对）
- [ ] **AC-F2-11** ★ **测量方法护栏**：`enable_mmu=false`（**bare**）· **排除 trap 周期** · **无条件跳转（jal/jalr/ret）与条件分支分开统计**

### 4.3 ② 主体 `chmem-bp-btb-gshare` 的 AC（**v2.1 重写**）

| # | AC | 相对 v2 |
|---|-----|---------|
| **AC-1** | `ip/cpu/plugins/branch_predictor_chmem.h` 存在且实现 **BTB（256×64b，**62b 编码**：`tag30=PC[31:2]` + `target30=target[31:2]` + `valid` + `is_unconditional`）+ Bimodal 2-bit PHT（256×2b）**；★ **GShare / RAS 明确不在范围内** | ★ 重写（v2.2 布局统一） |
| **AC-2** | `cpu_factory_chmem.h` **已注册** BP plugin，canonical ordering 断言 PASS；BP 在 **fetch 侧**的注册位已定义（ADR-048） | 保留 |
| **AC-3** | ★ **台账逐条表**（§5.1）：14 条**逐条**给出「目标态 + CH_MEM 锚点 + 显式理由 + 是否删除该行」；★ **CH_MEM 侧 case 名与 TLM-only case 名零交集**；★ **台账 §7 的 BP 裁决行同步**（`tlm-coverage-parity-ledger.md:742`） | ★ 补 §7 同步项 |
| **AC-4** | **BTB miss ⇒ 不预测**（不跳转、不 flush） | 保留 |
| **AC-5** | 预测方向/目标为**活节点**（`impl()->is_const() == false`）⇒ 禁 `static_cast<bool>` | 保留 |
| **AC-6** | ★ **预测错误 ⇒ 只在预测错误时抑制错路指令**；且 flush 双分量语义正确（`br_taken_buf` 分量替换为 `mispredict_buf`，**`hold_z` 保留**，protocol §2.2） | ★ 补双分量 |
| **AC-6b** | ★★ **正确预测 ⇒ 该分支自身的 redirect 被抑制**（目标指令**只被取一次**）⇒ 验证：host 侧 PC 轨迹**按迭代边界切分后，每段内任一静态地址的取指次数 ≤ 1**（★ **迭代边界** = 循环回边 PC；★ **MUST NOT** 写成「全程无重复 PC」—— 循环回边使同一地址合法重复上千次；★ **MUST NOT** 写成「无连续重复」—— 自指分支在正确预测下产生合法的连续重复） |
| **AC-7** | `[chmem]` 47 ELF + `[verilator]` 48 ELF **0 退化**；★ **双跑**：BP 关一遍 + BP 开一遍，两套全绿；★ **BP on/off 的 4 档校验和逐位相等** | ★ 双跑 |
| **AC-8** | ★ **拆三条**（见下） | ★ 重写 |
| **AC-9** | ★ **具名门禁清单**：`verify_adr` · `verify_plugin_decision` · `check_plugin_portability` · `doc_link_check` · `roadmap-r9-check` · `check_xlen_hygiene` · `check_tlm_freeze` · `check_coverage_parity_ledger` + `bash tools/run_chipforge_tests.sh --all`（失败集合恰 1 个 pre-existing）★ 注明「**仅本地成立 ⇒ ADR-088 §5.2 判据 4 仍未满足**（CI 不构建）」 | ★ 具名化 |
| **AC-10** | 台账重判完毕 + **§2 族级判定行同步** + **「涉及 case 数」与头部「463 条」计数同步** + ★ **`check_coverage_parity_ledger.sh` exit 0**（CI warn 级 ⇒ 无强制力 ⇒ 须本地跑 + 贴 PR） | ★ 保留 |
| **AC-11** | **TLM 侧 `branch_predictor.{h,cpp}` 全程保留**（删除属 B3，需双前置） | 保留 |
| **AC-12** | ★ **「不做什么」机检化**：★ **脚本化** —— `tools/check_plugin_portability.sh` 新增 **Check 14**（★ 该脚本已是 PR **阻塞**门禁 ⇒ **无需构建** ⇒ CI 可承担）：scope = `ip/cpu/plugins/branch_predictor_chmem.h`（文件不存在则 skip）+ 新增文件名匹配 `*gshare*`/`*ghr*`/`*ras*` ⇒ 违反 ⇒ exit 1。★ **MUST NOT** 做成 tree-wide grep（TLM 侧 `branch_predictor.h` 合法含 GHR/RAS ⇒ 必误报）⇒ ★ **MUST NOT** 只写手工 grep（域「变更集」无基线定义 ⇒ 等价自证；且防不住下一个 change） | ★ v2.2 改 |
| **AC-13** | ★ **BP 必须可学**：同一分支**第二次**被取时预测命中（PHT 更新使能 = 分支已解析，任何方向；更新地址 = 分支解析处 `KT::PC`，与预测 `pc_reg` 同指一条分支） | ★ 新增 |
| **AC-14** | ★ **`jalr` 不建 BTB 项** ⇒ 走 not-taken；无条件跳转不占 PHT 表项 | ★ 新增 |
| **AC-15** | ★ **host 可见事件计数**：`已执行指令数` + `FLUSH 事件数`（两个计数器 + `cpu_verilator_sim --stats` 输出行）⇒ 供 AC-8b 与前置2 的机制判据使用 | ★ 新增（metis P0-3） |
| **AC-16** | ★★ **净收益率裁决点（两分支终局式）**：净收益率 := `Δcycles(bp=off − bp=on) / N_taken_total`（★ **不是命中率**；★ `N_false` 只计情形 A）⇒ **达标** ⇒ 恢复绝对值门禁（ADR-088 §5.3.2 落结论）；**不达标** ⇒ ★ **MUST NOT** 以「GShare 延后」结案 ⇒ **MUST** 把 GShare 拉回范围（或另立 change）⇒ 按 P-3 临时口径结案。★ **MUST NOT** 只写「MUST ≥ 84.5%」（不达标时 AC 恒 RED ⇒ 与「不恢复时结案」互斥 ⇒ 归档路径被自己的 AC 堵死） | ★ v2.2 新增 |
| **AC-17** | ★ **无条件跳转的可预测性** —— 分支目标表项 MUST 带「无条件跳转」标志（无条件跳转不占方向表项 ⇒ 若无此标志则其方向**无来源** ⇒ 永不被预测 ⇒ call 气泡不可恢复）；`pred_taken = pred_valid && (is_unconditional \|\| pht_counter ≥ 2)`；★ 间接跳转 **MUST NOT** 建表项（目标随运行时变化 ⇒ 静态缓存必错 ⇒ 走顺序取指） | ★ v2.2 新增（oracle 复审发现 2） |
| **AC-18** | ★ **陈旧目标自愈** —— 替换策略 MUST 为**直接覆盖** ⇒ 目标变化的分支再次 taken 时**同槽重写** ⇒ **天然自愈**；★ **MUST NOT** 另设误预测计数器 | ★ v2.2 新增 |
| **AC-19/20/21** | ★ 「归档时必须完成」的三条（ADR §5.3.1 根因修订 / B3 结论表述 / 绝对值门禁结论落地）**移入 `## Acceptance`** | ★ v2.2 新增（★ rdd-verifier 只核 `## Acceptance` ⇒ 独立 H2 段落**无强制力** ⇒ 假完工声明的温床） |
| **AC-22** | ★ **五级 mux 嵌套契约**（本 change 的核心契约）MUST **可 grep 断言**：`update_pc` 源码 MUST 是**多层 select 链**且 MUST 出现**具名的 trap 项**（如 `trap_redirect`）⇒ 否则后续 `csr-chmem-trap-pc-jump` 无处插入 ⇒ 返工。★ **MUST NOT** 只留「注释 + 结构预留」（注释不可机检） | ★ v2.2 新增 |
| **AC-23/24/25** | ★ **开关默认启用 + 生效值回显 + 4 参数转发完整**（AC-23）· ★ **M 越界 fail-fast**（AC-24）· ★ **scope 机检脚本化**（AC-25） | ★ v2.2 新增（★ 三者原只在「须知」里 ⇒ 须知**不是验收项**、verifier 不查 ⇒ 必须在 AC 层） |

#### AC-8 的三条子判据（★ metis P0-4：v2 的写法不可执行且会被正确实现判 FAIL）

| # | 判据 | 说明 |
|---|------|------|
| **AC-8a**（功能，无阈值） | 4 档在 `bp=on` 与 `bp=off` 下**校验和逐位相等** | 保留 |
| **AC-8b-1**（结构等式含暖机，★ v2.2 修正） | `always_taken` 档上 `Δcycles(bp=off − bp=on)` **逐位等于**「该档 bp=off 下的 **条件分支** taken 转移数 × 1 拍 **− W**」⇒ ★ **W = 2 × 该档静态条件分支数**（推导：迭代 1 = BTB miss、迭代 2 = 计数=1 预测不跳转）⇒ **MUST 实测并报告 W**（★ **MUST NOT** 写死魔数 `W ≤ 4` —— 档内静态分支数可能更多 ⇒ 正确实现会被判 RED）⇒ ★ **N MUST NOT 含 `jalr`**（不建 BTB 项 ⇒ 其气泡不可恢复 ⇒ 等式不可满足）⇒ ★ **「零误预测档」的准确表述 = 「零*代价*误预测**」** | ★ 把「1 拍/taken」从**分类恒等式**升级为**实测等式** |
| **AC-8b-2**（净收益模型，★ v2.2 按情形 A/B 修正） | 其余档 `Δcycles(off − on) = N_correct×1 − N_false_A×1`（N 由 **BP 内部计数器**导出）⇒ ★ **符号修正**（v2 写成 `on − off`，方向反了）⇒ ★ **`N_false` MUST 只计情形 A**（预测 taken / 实际 not-taken，代价 **1 拍**）⇒ ★ **情形 B**（预测 not-taken / 实际 taken）代价 **0 拍**（与无 BP 行为相同）⇒ **MUST NOT** 计入 ⇒ ★ **MUST NOT** 用「2×命中率 − 1」推导门槛（该式仅在情形 B ≈ 0 时成立） |
| **AC-8b-3**（有界替代，不引入计数器时） | 只断言 `0 ≤ Δcycles(off − on) ≤ N_taken×1`，精确净收益降级为 PoC 报告项 | 兜底 |
| **AC-8c**（反证）★ **限定 `always_taken` 档** | `Δcycles(off − on) == 0` ⇒ **BP 未生效**。⚠️ v2 的写法对 `always_not_taken` 档**不成立**（该档预测 not-taken 与无 BP 等价 ⇒ Δ 合法地 ≈ 0） | ★ 修正 |
| **AC-8d**（信号强度前置条件） | 若前置1 报告的跨档 taken 分支数差 **< 1 拍/iter** ⇒ **跨档判据不可用** ⇒ 改用「`always_taken` 单档绝对 Δ + 解析模型」。★ **判定权归前置1**（独立 change、独立执行者）⇒ 结论写入 CSV notes + 前置1 归档产物 ⇒ **不留给 ② 的执行者**（② 对「判据不可用」有既得利益） | ★ 判定权归属修正 |
| **AC-8e**（跨档判据形式） | ★ 跨档比较必须写成**差值**：`Δ(mode) − Δ(always_not_taken)` ⇒ 因 4 档都含**同一条循环回边**（每 iter 恒 1 次 taken ⇒ 各档统一收益）⇒ 解析模型须扣掉回边 | ★ 新增（metis 缺失 AC §3） |

## 5. 待裁决问题（**明确列出，不留给实施者**）

### 5.1 ★ 台账 14 条的**逐条目标态表**（v2.1 写死 #8/#9/#10 + 补 §7 同步项）

> ★ **构成说明**（防未来误读）：14 条 = **10 条 `not-covered`** + **4 条 `partial`** 的并集（台账 `tlm-coverage-parity-ledger.md:264-274` / `:349` / `:351` / `:357` + `:267-270`）。★ **注意**：台账 **§5.1/:742** 写的「10 条 BP 条目」只覆盖 `not-covered` 部分 ⇒ 与本表的「14 条」不矛盾（口径不同）。

| # | 条目 | 现状 | ★ **v2.1 目标态** | 理由（**逐条差异化，非加后缀**） |
|---|------|------|------------------|---------------------------|
| 1 | `btb_entry` | not-covered | **covered** | CH_MEM 侧有 BTB 结构 + 单测锚（case 名 `chmem_btb_entry` 等，**与 TLM-only case 名零交集**） |
| 2 | `custom_btb_size` | not-covered | **covered** | 物理 256 固定 + **运行期 mask** 限制逻辑条目数 M（§3.2）⇒ 运行时可配成立 |
| 3 | `btb_runtime_16` | not-covered | **covered** | 同上（M=16 为合法域之一） |
| 4 | `btb_runtime_64` | not-covered | **covered** | 同上（M=64） |
| 5 | `btb_runtime_256` | not-covered | **covered** | 同上（M=256 = 物理满容量） |
| 6 | `predict_after_update` | not-covered | **covered** | 预测器更新路径（AC-13 提供锚点） |
| 7 | `no_predict_initially` | not-covered | **covered** | 冷启动语义（`ch_mem` 零初始化 ⇒ target=0 ⇒ valid=0 ⇒ 天然不预测） |
| 8 | `not_taken_returns_zero` | partial | ★ **covered** | not-taken 时不预测（目标不投机）⇒ 语义等价 |
| 9 | `multiple_addresses` | partial | ★ **partial** | ★ **`jalr` 不建 BTB 项**（AC-14）⇒ 间接跳转的多地址语义**明确不覆盖** ⇒ 理由写「间接跳转不建 BTB 项（目标随运行时变化，不可静态缓存）」 |
| 10 | `reset` | partial | ★ **partial** | CH_MEM 无 reset 语义（`ch_mem` 零初始化 ⇒ 无需 reset） |
| 11 | `per_thread_ghr_isolation` | not-covered | **not-covered** | 「GShare 全局历史本期不实现（相对 Bimodal 的增益待前置2 的命中率数据）」 |
| 12 | `d2_branch_predictor_multi_thread` | not-covered | **not-covered** | 「CH_MEM 侧无框架级多线程 TID 机制」★ 补一句「BTB/PHT 为**全局共享**（CH_MEM 单线程 ⇒ 无隔离需求）」 |
| 13 | `d4_branch_predictor_tid_param` | not-covered | **not-covered** | 同上 |
| 14 | `branch_predictor_set_tid_stores_tid` | not-covered | **not-covered** | 同上 |

⇒ ★ **红线核算**：`not-covered` 分组 = **3 条**（TID，共享 1 个理由）+ **1 条**（GHR）⇒ **最大组 3 条 ≪ 红线 10** ✅
⇒ ★ **同步项**：① 台账 **§2 `cpu` 族判定行** ② **「涉及 case 数」（现 137）** ③ 文档头部「463 条」计数 ④ ★ **台账 §7 的 BP 裁决行**（`:742`「10 条 BP 条目 partial → not-covered」⇒ 须改为反映新终态）
⇒ ★ **机检**：① `check_coverage_parity_ledger.sh` **exit 0** ② CH_MEM 侧 case 名与 TLM-only case 名**零交集**（★ **先快照 TLM-only 名单**（`comm -23`）再命名 —— 否则新 case 撞名 ⇒ 该名从差集消失 ⇒ 台账对应行变「不在 TLM 独占差集」⇒ 违规）

### 5.2 待裁决问题

| # | 问题 | v2 倾向 | 需要的裁决 |
|---|------|---------|-----------|
| **Q1** | ② 范围 = **BTB + Bimodal PHT**（GShare/RAS 延后）？ | **是** | architect 确认（涉 ADR-088 §3 BP 行的解释） |
| **Q2** | 微基准修正作为**独立 change**？ | **是**（0c 已归档，改它需独立留痕） | 确认 |
| **Q3** | 机制 PoC 作为**② 的前置**？ | **是** | 确认 |
| **Q4** | BTB = **单表 64b × 256 entry**（运行时可配）？ | **是** | 确认 |
| **Q5** | 台账 14 条的**逐条目标态**（§5.1）？ | **是** | 确认（尤其 #6/#7 从 not-covered 升 covered） |
| **Q6** | ★ **净收益 < 预期 ⇒ P-6 前提不成立**的处置路径？ | ★ **终局式**：① 记录 ② 实测净收益 ⇒ ③ 在 **ADR-088 §5.3.2** 落**结论**（恢复 / **不恢复**绝对值门禁）⇒ ④ **不恢复时沿用 §5.3.2 已登记的 P-3 临时口径**（cycle 比率 ≤ 1.15 + notes 标注）⇒ ⑤ **本 change 关闭**，**不留悬空项** | 确认 |
| **Q9** | ★ **B3 的 BP 阻塞由谁解除？** | ★ **不由本 change 解除** —— ADR-088 §5.2 的双前置 ②（CoreMark harness 产出实测值）属**交付点 B**。⇒ ★ **plan Task 7 Step 4 的「① 已达成 / ② 已达成 ⇒ B3 阻塞解除」是**假完工声明**，须改为「① 已达成；**② 未达成（属交付点 B）⇒ B3 的 BP 阻塞未解除**」+ 登记「CI 判据 4 未满足（CI 不构建）」 | 确认 |
| **Q7** | ★ **flush 语义变更**（taken → mispredict）是否纳入 ② 的 scope？ | **是**（否则 BP 无法工作） | 确认（这是**语义变更**，波及 3 个消费者） |
| **Q8** | ★ **BP 开关面**的最终形态（见 §5.3）？ | 见 §5.3 | 确认 |

### 5.3 ★ BP 开关面定稿（metis 建议 11，消除三处不一致）

| 项 | 决策 |
|---|------|
| 工厂参数 | ★ **普通 `bool enable_bp = true`**（**不**用 `optional<bool>` —— 与 `enable_mmu`/`enable_cache` 的三态语义不同，避免 nullopt 与「显式关闭」撞车） |
| 工厂参数 2 | `std::optional<std::uint32_t> btb_entries = std::nullopt`（运行时可配 ⇒ **需要**三态） |
| CLI flag | ★ 只保留 **`--no-bp`**（默认开）+ **`--btb-entries N`**；**不保留** `--enable-bp`（与默认开语义冗余 ⇒ 消除「两者同给时的优先级」歧义） |
| 优先级 | 若同时给 `--no-bp` 与其它 ⇒ **`--no-bp` 胜**（显式关闭优先） |
| 可观测性 | ★ runner 在 **stderr 回显生效值**（`BP=on/off`、`btb_entries=N`）⇒ 测量脚本据此**断言**与请求一致（防止 flag 没生效却测出「无差异」并误判 BP 无效） |
| 转发完整性 | ★ `build_cpu_with_elaborate`（`cpu_factory_chmem.h:451-458`）**必须转发**新参数（否则该路径行为不同 ⇒ 静默不一致） |
| 测量脚本 flag 词汇 | ★ **统一一套**：`measure_branchbench.sh` 与 `cpu_verilator_sim` 用**同一套** `--bp=on|off`（v1 发明的 `--check-baseline --bp=on` 与边界简报的 `--enable-bp/--no-bp` 是**两套词汇** ⇒ 消除） |
| CSV 结构 | ★ 新列**追加在末尾**（保持 `$2=校验和`、`$3=cycles` 的位置语义，`measure_branchbench.sh:52,63` 按位置取列）⇒ **更稳的做法**：改脚本为**按表头名取列** |
| CSV 列 | 增 `bp_mode` + `workload_sha` ⇒ `--check-baseline` 按 `(workload_sha, bp_mode)` **分组精确比对** |
| 跨 build 漂移 | Verilator 确定性 ⇒ 同 build 噪声为 0 ⇒ 阈值可很紧；★ **跨 build 可能漂移** ⇒ CSV 必须记录 build 标识，否则「精确相等」会变成随机红 |

### 5.4 ★ 通用规则：**兜底句三要素**（v2.1 升级：时限必须**可判定**）

> **任何「交 architect / 上报 / 与 X 议 / 待裁决」句，必须同句给出：① 触发阈值 ② 落点文件 ③ 时限（★ **可判定形态**：「≤ N 个工作日」或「本 change 内闭环」）。**
> **无三要素的兜底句视为无效条款。**
> ★ **v2.1 追加**：**「不得跨 change 挂起」本身不是时限**（无日期/拍数）⇒ 必须写成可判定形态。
> ★ **自查**：§5.2 的 Q1–Q9 本身**也必须**符合本规则 ⇒ **「v2.1 倾向即裁决；异议须在 ≤ 2 个工作日内提出，否则按 v2.1 执行，不得挂起」**。

### 5.4.1 ★★ per-file × per-symbol owner 表（★ v2.2）

★ **权威落点 = 各 change 的 `tasks.md`**（★ 已**逐条抄进**三个 change ⇒ 满足 §5.5 的「不跨 change 引用」纪律 ⇒ 本节仅作总览，不作为实施依据）。

| 文件 / 符号 | 前置1 | 前置2 | 主体 |
|---|---|---|---|
| `tests/cpu/manual_elf/branchbench/branchbench.c` | ✓ owner | ✗ | ✗（硬门禁） |
| `soc/cpu/docs/dse/branchbench-baseline-matrix.csv` | ✓ **建列** | ✓ 仅追加行 | ✓ 仅追加行 |
| `tools/measure_branchbench.sh` | ✓ owner（改名 / 按表头取列 / **捕获 stderr**） | ✓ 仅追加 flag 行 | ✗（硬门禁） |
| `tools/verilator_runner/*` | ✓ `--stats` 框架 + retired 计数 | ✓ FLUSH 计数 | ✓ BP flag + 回显 |
| `ip/cpu/plugins/branch_predictor_chmem.h` | ✗ | ✓ **创建（最小版）** | ✓ **重写（完整版）** |
| `ip/cpu/plugins/ibus_chmem.h`（mux + flush） | ✗ | ✓ **首次改** | ✓ **二次改（增量扩展）** |
| `ip/cpu/cpu_factory_chmem.h`（注册 + 开关 + 转发） | ✗ | ✓ 首次注册 | ✓ 补全（+4 参数转发修复） |
| `docs/architecture/adr/ADR-088-*.md` | ✗ | ✓ **§5.3.2 机制结论段** | ✓ **绝对值门禁结论段**（★ MUST NOT 覆盖前者的「机制不成立」结论） |
| `docs/architecture/tlm-coverage-parity-ledger.md` | ✗（硬禁） | ✗（硬禁） | ✓ owner |

### 5.5 ★★ 「builder 开工须知」清单（★ metis P2-16 + 本轮新增，**必须随 change 的 tasks.md 交付**）

| # | 须知 | 出处 |
|---|------|------|
| 1 | ★ 台账条目行**必须正好 7 列**（`check_coverage_parity_ledger.sh:85` 的 NF==7 护栏）；★ **理由列不得含未转义的 `\|`** | 脚本脆弱性 |
| 2 | ★ 新增 CH_MEM 侧 case 前**先快照 TLM-only 名单**（`comm -23`）再命名 ⇒ 保证零交集 | 台账脚本 ① |
| 3 | ★ 台账脚本需**两个 binary**（TLM + CH_MEM）⇒ **本地跑 + 输出贴 PR**（CI 是 warn 级 ⇒ **无强制力**） | CI 现实 |
| 4 | ★ CSV / 脚本的**改名必须同一 commit** | `measure_branchbench.sh:52` 的 `grep -E "^${name},"` |
| 5 | ★ CSV **按表头名取列**（★ **强制**，不是「更稳的做法」）—— 现 notes 字段含逗号 ⇒ 位置访问必错位；`--check-baseline` 分组键 = `(workload_sha, bp_mode, build_id)` | metis 边界情况 #12 |
| 6 | ★ `--btb-entries N` 合法域 = **16/32/64/128/256**（与 TLM `CPUConfig::btb_entries` 对齐）；**越界 fail-fast**（不得静默 clamp） | metis 边界情况 #8 |
| 7 | ★ ② 主体的 diff **不得触碰** `tests/cpu/manual_elf/branchbench/` 与 `tools/measure_branchbench.sh`（那是前置1 的范围）⇒ 三 change 边界须为**硬门禁** | metis slop 风险 #6 |
| 8 | ★ 修正 `ibus_chmem.h:83-87` 的**过时注释**（「aread/sread 都是 1-cycle」与 §1.1 的 aread 0-cycle **承重前提直接矛盾**）⇒ 挂进前置2 的 AC | oracle 建议 4 |
| 9 | ★ `build_cpu_with_elaborate`（`cpu_factory_chmem.h:451-458`）**现状就已丢失** `enable_mmu`/`mmu_mode`/`enable_cache` 三个参数 ⇒ BP 参数进来前**顺带修全部 4 个的转发**（或显式登记为 known gap） | oracle 建议 6 |
| 10 | ★ 顺手修 `branch_chmem.h:20` 的注释（「JAL/JALR 推迟」）与 `:221-226` 的实装不符 | oracle 建议 9 |

## 6. 与上游 ADR 的关系（**★ 必须修订，不是「微调」**）

| ADR | v1 的说法 | ★ **v2 的裁决** |
|-----|-----------|---------------|
| ADR-088 §5.3.1（根因） | 「表述需微调」 | ★ **必须修订** —— 删掉「TLM 侧有 BTB ⇒ 预测命中不 flush」（**已被 E-1/E-2 证伪**）· 把「2 拍 flush 代价」改为「**120065 次 taken 转移 × 1 拍气泡**（实测）」· 补上「真根因 = **TLM 单遍 tick 重定向零延迟 vs CH_MEM 两级取指每条 taken 转移 1 拍气泡**」（E-4）· 补「**TLM 的 BP 对 cycle 数零影响**（摘掉注册后 cycles 逐位不变）」的留证 |
| ADR-088 §5.3.2 **P-6** | 「前提待验证」 | ★ **方向仍成立**（BP 若能提前一拍预测 ⇒ 消除 120065 拍 ⇒ 绝对值门禁可恢复）· ★ **但表述依据必须换掉**（不是「TLM 有 BTB」）· **数值前提**（60 拍/iter 的来源）须按 E-3 更正 |
| ADR-088 §3 BP 行 | 「CH_MEM-only 且必须做」 | **不变** —— BTB + Bimodal PHT 满足「CH_MEM 侧有 BP 结构」· ⚠️ 但该行的措辞若列了「GShare/RAS」⇒ 需同步（**由 architect 在本 change 归档时改**） |
| ADR-088 §5.2 BP 专项删除前置 | 「② 交付 ①，交付点 B 交付 ②」 | **不变** |
| ADR-087 §D4 护栏 | — | **无关**（本 change 不碰 XLEN 护栏） |

⇒ ★ **裁决**：**ADR-088 §5.3.1 的修订是本 change 的一部分**（不是「后续再说」）⇒ 归档时**必须**含 ADR 修订（`docs/architecture/adr/ADR-088-tlm-backend-retirement.md` + `docs/architecture/adr.md` 注册表）⇒ 走 `verify_adr.sh` 校验。

## 7. 评审请求（v2）

**v1 已经 oracle + metis 双审**，本轮为**复审**（重点验证 v2 是否正确吸收了 v1 的意见，以及是否有新的遗漏）。

请 oracle 重点回答：
1. **§2.1 的「提前一拍」模型**是否成立？特别是：`pc_lag(N) = pc_reg(N-1)` 与 fetch NORMAL 取 `pc_reg(N)` 的错位是否确实给出「预测当前正在取的那条分支」的机会？
2. **§2.2 的「BTB-only 净负」**是否成立？
3. **§2.3 的「纠错目标 = `pc_lag+4`」**是否正确？
4. **§3.2 的「单表 64b × 256 entry」**是否合理（尤其「运行时可配」是否真的成立 —— PoC 结论 6 说的是「越界读 = 0」，**不是**「运行时可配」）？
5. **§0.2 的 E-2/E-3 判决实验**结论是否可采信（是否还有别的解释）？

请 metis 重点回答：
1. **§4.3 的 AC 清单**是否够（尤其 AC-8 的四条子判据 + AC-12 的「不做什么」显式化）？
2. **§5.1 的台账逐条表**是否完整（14 条逐条 + 红线核算）？
3. **§5.4 的兜底句三要素规则**是否足以防挂起？
4. **§2.5 的 AC-F2-3 决策表**是否给了「不成立」时的**可执行**出路（而非又一次「等回复」）？
5. 是否还有 **AI slop 风险**（尤其：builder 会不会顺手把 GShare/RAS 写了？会不会自己发明阈值？）

按既有格式返回（oracle 4 段式 / metis 6 段式）。

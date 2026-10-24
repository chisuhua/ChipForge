# ② chmem-bp-btb-gshare — 架构方案简报（architect 主持）

> **状态**: 🚧 **v2 — 双审后修订**（oracle R1–R4 / metis P0–P2；**两个判决实验已完成，见 §0.5**）
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
| **C** | **BP 的存储结构** | **单表 `ch_mem<ch_uint<64>, N>`**（tag 32b + target 32b 一个 word）· 容量 **256 entry**（运行时可配，域与 TLM 侧 `btb_entries` 16–256 一致） | ★ v1 的「2 表 × 4096 = 32KB」被推翻（oracle 建议 6：容量过大 + tag 算术不自洽） |

### 0.2 判决实验（**v2 新增，已实测，代码已复原**）

| # | 实验 | 结果 | 结论 |
|---|------|------|------|
| **E-1** | 读 TLM `branch_predictor.h` + `ibus.h` | 预测结果**被丢弃**（`:126-129` 注释「M2 阶段: 仅存储预测结果」+ 空函数体）· `flush_when([]{return false;})` 恒 false 演示桩（`:149-151`）· `ibus.h` **0 处**引用预测 | **TLM 的 BP 对 cycle 数零影响** |
| **E-2** | ★**临时摘掉 TLM 的 BP 注册**（`cpu_factory.h:517`）→ 重建 → 跑 Dhrystone | cycles **794478 → 794478（逐位不变）** · DMIPS **1.43277 → 1.43277（逐位不变）** | ★ **TLM 的 BP 惰性 ⇒ ADR-088 §5.3.1 的根因归因（「TLM 有 BTB ⇒ 预测命中不 flush」）被证伪** |
| **E-3** | ★**逐拍采样 CH_MEM 的 PC**（临时改 `sim_main.cpp`，已复原）统计控制转移 | **taken 控制转移 = 120065 次** · 顺序步 = 794479 · 794479 + 120065 = **914544 = CH_MEM 总 cycles（逐位吻合）** | ★ **气泡 = 恰好 1 拍 / 每次 taken 转移**（v1 引用的「30 分支 × 2 拍」是**循环论证 + 数字错误**） |
| **E-4** | 读 TLM `ibus.h:114-124` | writeback LATE 在**同一 tick 内**把 `taken ? target : pc+4` 写回 fetch 节点 ⇒ **单遍 tick 语义 ⇒ 重定向零延迟** | ★ **真根因 = 重定向延迟的结构性差异**（TLM 0 拍 vs CH_MEM 1 拍） |

⇒ **根因（修正后）**：**TLM 单遍模型重定向零代价 vs CH_MEM 两级取指每条 taken 控制转移 1 拍气泡**（120065 拍/iter 集合）。
⇒ ★ **P-6 的方向仍成立**（BP 若能提前一拍预测 ⇒ 消除这 120065 拍 ⇒ 绝对值门禁可恢复），但**其表述依据必须换掉**（不是「TLM 有 BTB」）。

### 0.3 微基准污染（v1 已发现，双审确认）

0c 的 `branch_cond` 编译成**分支链**（3 个条件分支）+ `branch_sel` 两条路径**不等长**（4 vs 5 条）⇒ **4 档动态指令数不同** ⇒ 「.text 一致 ⇒ cycle 差只来自分支方向」**被证伪**。

⇒ 另发现（metis 边界情况 #3）：**档名与硬件分支方向相反** —— `branch_sel` 的 `bnez`（cond≠0 走 taken），而 `branch_cond` mode0 返回 0 ⇒ **mode0「always_taken」实为硬件上的 never-taken** ⇒ 任何按档名推断收益的推理都会**反**。

### 0.4 结论（v2）

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

### 2.1 ★ 挂钩点：**fetch NORMAL 提前一拍**（v1 错误已修正）

v1 断言「取指地址在 fetch NORMAL 发出、分支结果同拍产出 ⇒ BP 无法影响本拍取指」⇒ **这个断言混淆了两个 PC**：

| PC | 含义 | 何时可知 |
|----|------|---------|
| `pc_reg(N)` | **本拍正在取**的指令地址 | fetch NORMAL 读到它 |
| `pc_lag(N) = pc_reg(N-1)` | **本拍正在解析**的指令地址（branch 阶段解析它） | branch NORMAL 解析它 |

⇒ ★ **BP 在 fetch NORMAL 用 `pc_reg(N)` 查 BTB（0-cycle `aread`）⇒ 得到的预测喂给「同拍 branch LATE」的 `next_pc` mux** ⇒ 预测的是 **`pc_reg(N)` 处那条分支**（正是本拍正在取的）⇒ **提前整整一拍**。

```
cycle N:   fetch NORMAL: pc_reg = B
                     └─ BTB aread(bits(B)) → hit, target = T        ← ★ 预测点
            branch LATE: pc_reg <<= select(mispredict, correct_target,
                                          select(btb_hit, T, B+4))
                                                      ↑ 预测在此生效
cycle N+1: instr B 可用并解析(taken, T) —— 与预测一致 ⇒ 无纠错、无 flush
            fetch NORMAL: pc_reg = T
cycle N+2: instr T 可用  ⇒ B 与 T 背靠背, **0 气泡** ✅
```

⇒ ★ **收益模型（修正后）**：**正确预测 ⇒ 该 taken 转移的气泡被完全消除**（不是「只能省 fall-through 那一拍」）。

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

### 3.2 ⇒ 裁决 B-2：BTB 结构 = **单表 `ch_mem<ch_uint<64>, N>`**

| 项 | 决策 | 依据 |
|---|------|------|
| 布局 | **单表 64b/entry**（tag 32b + target 32b + valid 1b ⇒ 打包进 64b） | ★ 单读/写端口对 ⇒ **天然满足** PoC 结论 3「读/写共用同一地址表达式」；容量等价于 v1 的「双 32b 表」但**只有一个端口对**（oracle 建议 10） |
| 容量 | ★ **256 entry**（**不是** v1 的 4096） | ① TLM 侧 `btb_entries` 的配置域就是 **16–256**（`branch_predictor.h:70`）⇒ 对齐既有域；② Dhrystone/CoreMark 热点分支数十量级；③ 256×64b = **16KB**（v1 是 32KB）；④ **运行时可配**（PoC 结论 6「固定容量 + 运行时逻辑条目数」支撑）⇒ ★ **顺带把台账 `custom_btb_size` / `btb_runtime_{16,64,256}` 4 条升为 covered** |
| 索引 | `bits<N-1,0>(PC)`（N = 逻辑条目数，运行时可变） | 与运行时可配配合 |
| tag | PC 的高位（`bits<31, idx_bits>`） | ⚠️ **v1 的「tag 20b」与 4096 不自洽**（12-bit 索引 ⇒ tag = PC[31:14] = 18b）—— 已在 v2 修正为「tag = PC 高位，长度随容量」 |
| valid | ★ **独立 valid 位**（打包进 64b word） | ★ v1 的「tag == 0xFFFFFFFF ⇒ 无效」**不可行**：① `ch_mem` 零初始化 ⇒ 初值 tag = 0 ⇒ **低地址段首次查询即假命中**；② 真实 tag 可能取 0xFFFFFFFF |
| **key 是 vaddr 还是 paddr** | ★ **vaddr**（分支 PC payload 本身就是 vaddr） | MMU on/off 之间 key 必须一致 ⇒ 用 paddr 会在两种配置下产生不一致的表项（metis 边界情况 #4） |

### 3.3 ⇒ 裁决 B-3：PHT 结构

| 项 | 决策 |
|---|------|
| 表 | `ch_mem<ch_uint<2>, N>`（N = 256 或 1024）⇒ **2-bit 饱和计数器**（PoC P-3 已验证可行） |
| 索引 | ★ **Bimodal 语义**：`bits<idx_bits-1,0>(PC)`（**不用**全局历史 XOR）⇒ GShare 延后 |
| 饱和 | 0→1→2→3 饱和（不回退），2/3 ⇒ 预测 taken |
| 冷启动 | 初值 0（弱 not-taken）⇒ 冷启动语义由 `no_predict_initially` 条目覆盖 |

### 3.4 ⇒ 裁决 B-4：预测的**产出与消费**（payload 契约）

| 方向 | 决策 |
|------|------|
| BP 的**输入** | fetch 期的 `pc_reg`（**被 op 节点 `<<=` 驱动** ⇒ 可作 mem 读地址，PoC 结论 2 + P-2P） |
| BP 的**输出** | ① `predicted_target`（32b）② `predicted_taken`（1b）③ `predicted_valid`（1b）⇒ **写入 payload**（mode-aware `uint_t<N>`，禁裸 `bool`，ADR-040 §10.5） |
| 消费点 | **同拍 branch LATE** 的 `next_pc` mux（`ibus_chmem.h:171`）⇒ mux 优先级：**mispredict 纠错 > BTB 预测 > `pc+4`** |
| 误预测记录 | `ch_reg` 组（pred_taken / pred_target / pred_valid）⇒ 跨拍保持（PoC 结论 7） |
| 纠错目标 | 「预测 taken、实际 not-taken」⇒ **`pc_lag + 4`**（不是 `pc+4`，§2.3） |
| 顺序契约 | 生产者（BP，fetch 侧）写 payload ⇒ 消费者（`update_pc`，branch LATE）读 ⇒ **同 stage 名 / EARLY→LATE**，不得依赖插件注册顺序（ADR-040 §10.6） |

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

### 4.1 前置1 `chmem-branch-microbench-fix` 的 AC

- [ ] **AC-F1-1** `objdump` 证明 `branch_cond` **0 条条件分支**（纯 `sltiu/seqz/and/or`）+ `branch_sel` 两条路径**指令数相等**
- [ ] **AC-F1-2** 4 档**动态指令数相等**（★不只是 `.text` 相同）+ 4 档的 **taken 分支数/iter** 已报告
- [ ] **AC-F1-3** 新 `.text` sha256 入册 + CSV 重录 + CI `branchbench_baseline` 仍绿
- [ ] **AC-F1-4** ★ **档名与硬件方向一致**（mode0/mode1 的 `bnez` 实际方向已核对并据此重命名或修正注释）
- [ ] **AC-F1-5** ★ **留痕**：0c 的 AC-2「`.text` sha256 一致」**事实仍成立**，但其推论「`.text` 一致 ⇒ 动态指令数相同」**被本次反汇编证伪** ⇒ 该证伪写入 0c 的归档 change（**不得**改动归档产物 ⇒ 写入本 change 的 proposal + CSV notes）
- [ ] **AC-F1-6** 若跨档 taken 分支数差 **< 1 拍/iter** ⇒ **判定该微基准不足以支撑跨档 AC** ⇒ 改为「单档绝对 Δcycles + 解析模型」判据，**或**在本 change 内新增「分支密度可调」负载

### 4.2 前置2 `chmem-bp-btb-gshare-poc` 的 AC

- [ ] **AC-F2-1** 被测对象与工作负载明确（哪一档 / 哪些分支 / 迭代数）
- [ ] **AC-F2-2** 测的量与单位明确：**taken 控制转移数**（E-3 的方法）+ **总 cycles**
- [ ] **AC-F2-3** 决策表**两条分支各自的后果**（成立 ⇒ 进 ②；不成立 ⇒ 上报 architect + 在 ADR-088 §5.3.2 记录「② 不构成绝对值门禁恢复条件」+ 由 architect 另立绝对值路径 change，**不得以「等回复」结束本 change**）
- [ ] **AC-F2-4** ★ **兜底路径三要素**（触发阈值 + 落点文件 + 时限：不得跨 change 挂起）—— 继承 plan R6 的纪律
- [ ] **AC-F2-5** ★ 显式声明「**本 PoC 不产出 P-6 结论**」（P-6 结论归 architect）
- [ ] **AC-F2-6** PoC 的代码**不复用**为 ② 主体（避免「半成品进主体」）

### 4.3 ② 主体 `chmem-bp-btb-gshare` 的 AC（**v2 重写**）

| # | AC | 相对 v1 |
|---|-----|---------|
| **AC-1** | `ip/cpu/plugins/branch_predictor_chmem.h` 存在且实现 **BTB（256 entry × 64b：tag+target+valid）+ Bimodal 2-bit PHT**；★ **GShare / RAS 明确不在范围内**（v1 的「+ GShare 13-bit PHT(2-bit) + RAS 8-entry」已删） | ★ 重写 |
| **AC-2** | `cpu_factory_chmem.h` **已注册** BP plugin，canonical ordering 断言 PASS；★ BP 在 **fetch 侧**的注册位已定义（ADR-048） | 保留 + 追加 |
| **AC-3** | ★ **台账逐条表**：14 条**逐条**给出「目标态 + CH_MEM 锚点 + 显式理由 + 是否删除该行」；★ **CH_MEM 侧 case 名与 TLM-only case 名零交集**（否则 `check_coverage_parity_ledger.sh` 会报「条目不在 TLM 独占差集」⇒ 推荐命名 `chmem_btb_entry` 等） | ★ 重写 |
| **AC-4** | **BTB miss ⇒ 不预测**（不跳转、不 flush） | 保留（语义微调） |
| **AC-5** | 预测方向/目标为**活节点**（`impl()->is_const() == false`）⇒ 禁 `static_cast<bool>` | 保留 |
| **AC-6** | ★ **预测错误 ⇒ 只在预测错误时抑制错路指令**；且 flush 语义变更后 **3 个消费者**（branch/dmem/reg_file）行为正确 | ★ 重写（判据对象变了） |
| **AC-7** | `[chmem]` 47 ELF + `[verilator]` 48 ELF **0 退化**；★ **BP on/off 的 4 档校验和逐位相等**（确定性 · 零阈值 · 能直接抓出 flush 语义改错） | ★ 追加 |
| **AC-8** | ★ **整体重写**（见下） | ★ 重写 |
| **AC-9** | 8 门禁全 PASS + 全量回归 0 退化（失败集合恰 1 个 pre-existing） | 保留 |
| **AC-10** | 台账重判完毕 + **§2 族级判定行同步** + **§2「涉及 case 数」与文档头部「463 条」计数同步** + ★ **`check_coverage_parity_ledger.sh` exit 0**（该脚本 CI 是 warn 级 ⇒ 否则会静默违规） | ★ 追加 |
| **AC-11** | **TLM 侧 `branch_predictor.{h,cpp}` 全程保留**（删除属 B3，需双前置） | 保留 |
| **AC-12** | ★ **「不做什么」显式化**：GShare / RAS / 多级 TLB / 改取指延迟 等**逐项标注「本期不实现（理由 + 归属）」** ⇒ 漏判可检测 | ★ 新增 |

#### AC-8 的新写法（★metis 建议，关键是去掉循环定义）

- **AC-8a（功能，无阈值）**：4 档在 `bp=on` 与 `bp=off` 下**校验和逐位相等**
- **AC-8b（收益，阈值由 architect 在 tasks.md 开工前预登记为精确整数）**：
  `Δcycles(bp=on − bp=off)` **等于**「该档被正确预测的 taken 转移数 × 1 拍」（1 拍 = E-3 实测）
  ⇒ **禁止**百分比、**禁止**「阈值由基线实测确定」（v1 的循环表述已删）
- **AC-8c（反证）**：若 `Δcycles == 0` ⇒ **BP 未生效**（不是「档选得不好」）
- **AC-8d（信号强度前置条件）**：若前置1 报告的跨档 taken 分支数差 **< 1 拍/iter** ⇒ **跨档判据不可用** ⇒ 改用「单档绝对 Δcycles + 解析模型」

## 5. 待裁决问题（**明确列出，不留给实施者**）

### 5.1 ★ 台账 14 条的**逐条目标态表**（metis 建议 9，**预登记以防漏判/误判**）

| # | 条目 | v1 说法 | ★ **v2 目标态** | 理由（**逐条差异化，非加后缀**） |
|---|------|---------|----------------|---------------------------|
| 1 | `btb_entry` | covered | **covered** | CH_MEM 侧有 BTB 结构 + 单测锚（case 名用 `chmem_btb_entry` 等，**与 TLM-only case 名零交集**） |
| 2 | `custom_btb_size` | not-covered | ★ **covered** | 容量 256 + **运行时逻辑条目数**（PoC 结论 6）⇒ 运行时可配成立 |
| 3 | `btb_runtime_16` | not-covered | ★ **covered** | 同上（16 是运行时可选值之一） |
| 4 | `btb_runtime_64` | not-covered | ★ **covered** | 同上 |
| 5 | `btb_runtime_256` | not-covered | ★ **covered** | 同上 |
| 6 | `predict_after_update` | （归 PHT） | ★ **covered** | ★ metis 修正：这是 **BTB 也有**的语义（预测器更新路径）⇒ **不可**归入「方向预测无收益」 |
| 7 | `no_predict_initially` | （归 PHT） | ★ **covered** | ★ 同上：冷启动语义（BTB 初值无效 ⇒ 不预测） |
| 8 | `not_taken_returns_zero` | partial | **partial 或 covered** | not-taken 时预测目标应为 0/无效 ⇒ 视实现语义定 |
| 9 | `multiple_addresses` | partial | **partial 或 covered** | 多地址（jalr 间接）⇒ BTB 存的是「某次的目标」⇒ 需显式说明 |
| 10 | `reset` | partial | **partial** | CH_MEM 无 reset 语义（`ch_mem` 零初始化 ⇒ BTB 天然全无效） |
| 11 | `per_thread_ghr_isolation` | （归 PHT） | **not-covered** | **GHR 属 GShare**（本期不做）⇒ 理由：「GShare 全局历史本期不实现（收益待 PoC 数据）」 |
| 12 | `d2_branch_predictor_multi_thread` | not-covered | **not-covered** | 「CH_MEM 侧无框架级多线程 TID 机制」 |
| 13 | `d4_branch_predictor_tid_param` | not-covered | **not-covered** | 同上 |
| 14 | `branch_predictor_set_tid_stores_tid` | not-covered | **not-covered** | 同上 |

⇒ ★ **红线核算**：`not-covered` 分组 = **3 条**（TID，共享 1 个理由）+ **1 条**（GHR）⇒ **最大组 3 条 ≪ 红线 10** ✅（★ metis 修正：v1 担心的「接近 10」**不成立**）
⇒ ★ **同步项**：台账 §2 `cpu` 族判定行 + **「涉及 case 数」（现 137）** + 文档头部「463 条」计数
⇒ ★ **机检**：① `check_coverage_parity_ledger.sh` **exit 0** ② CH_MEM 侧 case 名与 TLM-only case 名**零交集**

### 5.2 待裁决问题

| # | 问题 | v2 倾向 | 需要的裁决 |
|---|------|---------|-----------|
| **Q1** | ② 范围 = **BTB + Bimodal PHT**（GShare/RAS 延后）？ | **是** | architect 确认（涉 ADR-088 §3 BP 行的解释） |
| **Q2** | 微基准修正作为**独立 change**？ | **是**（0c 已归档，改它需独立留痕） | 确认 |
| **Q3** | 机制 PoC 作为**② 的前置**？ | **是** | 确认 |
| **Q4** | BTB = **单表 64b × 256 entry**（运行时可配）？ | **是** | 确认 |
| **Q5** | 台账 14 条的**逐条目标态**（§5.1）？ | **是** | 确认（尤其 #6/#7 从 not-covered 升 covered） |
| **Q6** | ★ **净收益 < 预期 ⇒ P-6 前提不成立**的处置路径？ | **默认动作 = 上报 architect + 在 ADR-088 §5.3.2 记录「② 不构成绝对值门禁恢复条件」+ 由 architect 另立绝对值路径 change** | 确认「**不得以「等回复」结束本 change**」 |
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

### 5.4 ★ 通用规则：**兜底句三要素**（metis 建议 17，防「AI 静默挂起」）

> **任何「交 architect / 上报 / 与 X 议 / 待裁决」句，必须同句给出：① 触发阈值 ② 落点文件 ③ 时限（不得跨 change 挂起）。**
> **无三要素的兜底句视为无效条款。**

（继承自 plan R6；v1 的 Q6/§2.6 未继承 ⇒ 本次补齐）

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

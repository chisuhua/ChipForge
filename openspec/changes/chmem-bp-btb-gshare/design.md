# Design — chmem-bp-btb-gshare

> 架构方案简报（权威）：`docs/architecture/bp-btb-gshare-design-brief.md` v2.1
> ★ 本文件是简报 §3.4.2 的**明文义务**落地：五级 mux 嵌套契约 + 三步时序 + payload 契约。

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
| **FLUSH 的双分量** | ★ 「`br_taken_buf` 分量**替换**为 `mispredict_buf` 分量，**`hold_z` 分量保留**」 | protocol §2.2 定 `FLUSH = br_taken_buf \| hold_z` ⇒ 若整体替换则与 PTW stall 语义相撞 |

### 1.3 合入顺序约束

★ **本 change 与 `csr-chmem-trap-pc-jump` 修改同一插入点** ⇒ 合入顺序：

| 序 | 内容 | 说明 |
|---|------|------|
| 1 | `csr-chmem-trap-pc-jump`（trap 项）落地 | protocol rev.14 的既有项 |
| 2 | ★ **本 change** 在其基础上**插入** `bp_predict` 项 | 必须**基于**已落地的 5 项 mux 扩展 |
| — | ★ 若本 change 先于 trap 项落地 ⇒ **必须预留 trap 插入位**（否则后续 trap 项无法插入 ⇒ 返工） |

⇒ ★ **登记**：本 change 的实现**MUST** 在 `update_pc` 中留下 trap 项的可插入位（注释 + 结构预留）。

---

## 2. 三步时序（★ 实施者必须照此实现）

### 2.1 三个 PC 角色

| 符号 | 含义 | 何时可知 |
|------|------|---------|
| `pc_reg(N)` | **本拍正在取**的指令地址 | fetch NORMAL 读到它 |
| `pc_lag(N) = pc_reg(N-1)` | **本拍正在解析**的指令地址 | branch NORMAL 解析它 |
| `pc_reg(N+1)` | **本拍 branch LATE 决定的**下一取指地址 | branch LATE(N) 写 |

★ **关键**：BTB 在 fetch NORMAL(N) 查的是 `pc_reg(N)`，而 branch LATE(N) 的 actual 是 `pc_reg(N-1)` ⇒ **两者不是同一条分支**；但**两者都作用于 `pc_reg(N+1)`** ⇒ 可以放在同一个 mux（优先级 `actual` > `BTB 预测` > `pc+4`）。

### 2.2 完整时序

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
    ★ 写 pc_reg(N+1):
        select( actual_taken(N), actual_target(N),            <- instr[pc_reg(N-1)] 的结果 (优先)
          select( pred_valid && pred_taken, pred_target,       <- instr[pc_reg(N)] 的投机预测
                   pc_reg(N) + 4 ) )
    ★ 记 1 拍预测记录 (ch_reg 组): pred_taken / pred_target / pred_valid

── cycle N+1 ────────────────────────────────────────────────────────
branch NORMAL(N+1):
    解析 instr[pc_lag(N+1)] = instr[pc_reg(N)] => actual_taken(N+1), actual_target(N+1)
    ★ 与 1 拍前的预测记录比对:
        预测一致 (pred_valid && pred_taken == actual_taken && pred_target == actual_target)
            => 该分支自身 redirect **必须被抑制**  (否则 target 被取两次)
        预测不一致 (误预测)
            => 纠错目标 = pc_lag(N+1) + 4   (真正的 fall-through, 不是 pc_reg+4)
            => flush (抑制错路指令的副作用)

branch LATE(N+1):
    ★ 写 pc_reg(N+2):
        select( 预测一致 ? 抑制 : actual_taken(N+1), actual_target(N+1),
          select( pred_valid && pred_taken, pred_target, pc_reg(N+1) + 4 ) )
```

⇒ ★ **收益**：正确预测的 taken 转移 ⇒ `pc_reg(N+1)` 直接 = target ⇒ **fall-through 那一拍根本不被取出** ⇒ **气泡完全消除**（实测每条 taken 转移 1 拍气泡）。

★ **抑制自身 redirect 是硬要求**：不抑制 ⇒ 目标指令被取两次 ⇒ `Δcycles ≈ 0` ⇒ **误判「机制不成立」**。

---

## 3. Payload 契约

| 方向 | 内容 |
|------|------|
| BP 的**输入** | fetch 期的 `pc_reg`（**被 op 节点 `<<=` 驱动** ⇒ 可作 mem 读地址） |
| BP 的**输出** | ① `pred_target`（32b）② `pred_taken`（1b）③ `pred_valid`（1b）⇒ **写入 payload**（mode-aware `uint_t<N>`，禁裸 `bool`） |
| 误预测记录 | `ch_reg` 组（pred_taken / pred_target / pred_valid）⇒ 跨拍保持 |
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
| 写使能 | ★ **taken**（目标此时才已知） |
| ★ **`is_unconditional`** | ★ 无条件跳转（`jal`）**不占 PHT 表项** ⇒ 若无此位则 `jal` 的方向**无来源** ⇒ **永不被预测** ⇒ call 气泡不可恢复 ⇒ 此位是**必需**的 |
| ★ **`jalr` 不建项** | 间接跳转目标每次可能不同 ⇒ 静态缓存必错 ⇒ `jalr` 走顺序取指 |
| ★ **陈旧目标自愈** | 直接覆盖策略下，目标变化的分支再次 taken 时**同槽重写** ⇒ **天然自愈**，不设误预测计数器 |

### 4.2 PHT（256 × 2-bit，Bimodal）

| 项 | 规则 |
|----|------|
| 索引 | `bits<7,0>(PC) & literal(M-1)`（**无全局历史 XOR** —— GShare 不在本期） |
| 饱和 | **0→1→2→3（不回退）** |
| 预测 taken 阈值 | ★ **counter ≥ 2** |
| 写使能 | ★ **「分支已解析」（任何方向）** —— 若只在 taken 时更新 ⇒ 学不到 not-taken ⇒ 在 not-taken 密集的负载上**净负收益** |
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

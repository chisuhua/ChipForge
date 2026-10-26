---
change: chmem-bp-btb-gshare
priority: P1
initiative: wave6-linux-and-productization
adr:
  - ADR-088
  - ADR-040
plan: .rddf/plans/chmem-bp-btb-gshare.md
status: in-progress
depends_on:
  - chmem-branch-microbench-fix
  - chmem-bp-btb-gshare-poc
---

# Tasks — chmem-bp-btb-gshare

> **定位**: B1 ② 主体。实现 **BTB + Bimodal 2-bit PHT**（★ GShare / RAS **不在本期**，见架构简报 §3.1）。
> **权威规格**: `docs/architecture/bp-btb-gshare-design-brief.md` v2.2 §2.1 / §3.2 / §3.3 / §3.4 / §4.3 / §5.1 / §5.5
> ★ **两个前置 MUST 先完成**（`chmem-branch-microbench-fix` + `chmem-bp-btb-gshare-poc`）⇒ **前置未完成前本 change 的实现 MUST NOT 开工**。
> ★ **执行文件** `.rddf/plans/chmem-bp-btb-gshare.md` 顶部有 **v2.2 覆盖声明**（逐条作废旧 Goal / Task 3 / Task 4 / Task 5.5 / AC-1 / AC-8 / Task 6 Step 4 / Task 7 Step 3-4 / 实施期修正 #1）⇒ **以覆盖声明为准**。

## Implementation
### ★★★ v2.3 覆盖声明（2026-10-26，**优先级高于本 change 以下全部内容**）

> **来源**：前置2 PoC（`chmem-bp-btb-gshare-poc`）实施期实测 ⇒ **5 处机制规格缺陷 + 1 处范围决策**。
> **权威细节**：`design.md` §0（v2.3 覆盖声明）—— ★ **实现前必读**。
> ★ **本 change 的 `design.md` / `tasks.md` 已按本声明同步**（旧表述已就地标注作废）。

| # | 修订 | 落点（本文档） | 依据 |
|---|---|---|---|
| 1 | ★ 五级链的 `bp_predict` 项 MUST 取 **`bp_next`**（不是裸 `pred_target`）⇒ 否则**丢链式投机** ⇒ 走错路 + 错路指令不被 flush ⇒ **副作用泄漏** | Task 4 / AC-6b | PoC B-1（单元锚 R8）|
| 2 | ★ 情形清单 MUST 含 **情形 F**（预测 taken / 实际 taken / **目标不同**）⇒ 纠到实际目标 + **MUST flush** | Task 4 / AC-6 | PoC 缺口 #10（单元锚 R6）|
| 3 | ★ 一切情形判定 MUST 排除「**被解析指令本身在错路上**」（`BRANCH_FLUSHED`）⇒ 否则 `case_a` 误触发 ⇒ PC 出轨 + **过度 flush** | Task 4 / **AC-26（新增）** | PoC B-2（单元锚 R11）|
| 4 | ★ FLUSH 判据 = **门控**（`hold_z \| (taken_buf & !spec_ok_buf) \| mispredict_A_buf`）⇒ **不是**「替换分量」 | AC-6 | PoC 门控锚 R5/R7 |
| 5 | ★ `spec_ok_buf` = 「投机**被证明正确**」（情形 C）⇒ **不是**「投机臂确实生效」（后者在情形 F 下会泄漏）⇒ ★ **裁决 A1** | AC-6 | PoC 缺口 #10 |
| 6 | ★ **范围**：拆成**三个可独立开关 + 独立测量**的增量（增量 1 = `is_unconditional` + `jal` 建项；增量 2 = RAS；增量 3 = GShare）⇒ **A5 决策**（2026-10-26）| AC-1 / AC-17 / **Task 7（新增）** / **AC-27（新增）** | 用户裁定 + PoC 实测 |
| 7 | ★ AC-16 的**净收益率分母口径** + 「达标 ⇒ 恢复绝对值门禁」的措辞 MUST **拆成两件事** ⇒ ★ **裁决 A6** | AC-16 / AC-21 | PoC 缺口 #13 + 本轮实测 |

★ **场景维度尚未完备**（★ MUST NOT 声称「场景覆盖完备」）：情形 A~F 是**单指令分类** ⇒ 不含
「被解析指令在错路上」（修订 3）与「分支 target 本身是分支」（修订 1）两个维 ⇒ ★ 二者 MUST 列为**必测场景**。


- [ ] **Task 1** BP 插件骨架 + BTB 表（256×64b，**62b 编码**：`tag30=PC[31:2]` + `target30=target[31:2]` + `valid(1b)` + `is_unconditional(1b)`；★ 依据 CH_MEM 只解 32 位指令 ⇒ `PC[1:0]=0`、target 4 字节对齐 ⇒ 低位恒 0）+ 逻辑条目数 M 的运行期 mask（`& literal(M-1)`）+ **替换策略 = 直接覆盖**（⇒ 陈旧目标天然自愈，★ **MUST NOT** 另设误预测计数器）+ **key = vaddr**
- [ ] **Task 2** Bimodal 2-bit PHT（256×2b）+ 更新使能（★ **分支已解析，任何方向**）+ 更新地址（★ 分支解析处 `KT::PC`）
- [ ] **Task 3** fetch NORMAL 预测（BTB **0 拍**查表）+ payload 输出（`pred_target`/`pred_taken`/`pred_valid`）
- [ ] **Task 4** branch LATE 接入（★ **五级嵌套**，★ **v2.3：链值 MUST 取 `bp_next`** —— 见 `design.md` §0.1）+ 正确预测**抑制自身重定向**（★ **抑制 `actual_taken` 臂、保留 `bp_next`** ⇒ 不丢链式投机）+ 误预测纠错（`pc_lag+4`）+ flush 语义改写（★ **门控**：`hold_z | (taken_buf & !spec_ok_buf) | mispredict_A_buf`）+ ★ **情形 F**（目标不一致 ⇒ 纠到实际目标 + MUST flush）+ ★ **错路指令维度**（`!flushed` 门控，见 AC-26）
- [ ] **Task 5** BP 开关面（工厂参数 + CLI）+ 生效值回显 + 参数转发完整
- [ ] **Task 6** 台账 14 条逐条重判 + 族级/§7/计数同步 + ADR-088 §5.3.1 根因修订 + 范围机检（grep 门禁）
- [ ] **Task 7** ★★ **增量分档的量收益**（★ v2.3 新增 —— **A5 决策**：「先扩 `is_unconditional` + `jal`/`jalr` 覆盖并量收益，再定 GShare」）
  - **Step 1（增量 1）** BTB 增 `is_unconditional` 位（62b 编码 ⇒ **位宽须重算**，见 `design.md` §4.1）+ `jal`/`j` 建项（写使能 **MUST NOT** 受 `is_branch` 限制）+ 预测方向 `is_unconditional ⇒ 恒 taken`
  - **Step 2（增量 2，条件性）** RAS 8-entry（`jalr`/`ret` 模式预测）⇒ ★ **TLM 参考实现无 RAS ⇒ 新增设计** ⇒ ★ **MUST** 先看 Step 4 的实测再决定是否做
  - **Step 3（增量 3，条件性）** GShare（GHR XOR 索引 PHT）⇒ ★ **TLM 参考实现有**（`branch_predictor.h` 的 `gshare_predict`/`gshare_update`/`global_history_`，`GHR_BITS=8`）⇒ ★ **移植**（成本远低于先前假设）
  - **Step 4（量收益 = 判据）** 每档 MUST 实测并落表：① **Dhrystone**（CH_MEM/Verilator）的 `FLUSH_COUNT` / `CYCLES` / `ΔFLUSH` / `Δcycles` / **净收益率**（★ 口径归 **A6**：分母 = `N_taken_total` 或 `FLUSH(off)`？★ 两者在 Dhrystone 上几乎相等 ⇒ 结论稳健）② **branchbench 4 档**的校验和**逐位相等**（零退化）
  - **Step 4b（★ 收益上界估算 —— planner 2026-10-26 静态分析，**给 Step 4 的预期区间**）**：Dhrystone 的 `FLUSH(off)` = **120065**（60.0/iter）⇒ BP=on 后残余 **52021**（26.0/iter）⇒ ★ 这 26/iter 的残余 flush **绝大部分是无条件跳转**（`jal`/`ret`）—— 因为 BTB 写使能 = `taken & is_branch`（**仅条件分支**）⇒ `jal`/`ret` **从不建项 ⇒ 从不预测**⇒ 静态构成（`.text` 共 55 个分支站点 = 无条件 36〔`ret` 16 / `jal` 15 / `j` 5〕+ 条件 19）⇒ 增量 1 覆盖 `jal`/`j`（20/36 的无条件站点）⇒ 覆盖上界 ≈ **14/iter ≈ 28,000 cycles**⇒ ★ 与「达 DMIPS 1.4 还需 33,477 cycles」**同量级** ⇒ ⇒ **增量 1 单独大概率不足以达 1.4**（预计把净收益率从 56.7% 推到 **~80%**）⇒ ⇒ ⇒ **执行序 MUST 是「增量 1 → GShare → RAS」**（见下）
  - **Step 5（★ 建议执行序 —— 按「实现成本」而非增量编号）**：**增量 1（`is_unconditional`+`jal`，新增设计，成本低）→ 增量 3（GShare，★ **TLM 参考已有 ⇒ 移植**，成本低）→ 增量 2（RAS，★ **无参考实现 ⇒ 新增设计**，成本高）**⇒ ★ 依据：GShare 是**移植**（`branch_predictor.h` 的 `gshare_predict`/`gshare_update`/`global_history_`，`GHR_BITS=8`）而 RAS 是**新设计** ⇒ ⇒ **先用便宜的**
  - **Step 6（决策）** 若「增量 1（+3）」的净收益率 **≥ 84.5%** ⇒ 绝对值门禁可恢复 ⇒ 剩余增量可延后（**并登记**）；否则 ⇒ **MUST** 把 RAS（增量 2）拉进本期 ⇒ ★ 决策 MUST 落 **ADR-088 §5.3.2**（净收益率结论 + 绝对值门禁结论**分两件事**落）

## Acceptance

- [ ] **AC-1** `ip/cpu/plugins/branch_predictor_chmem.h` 存在且实现 **BTB（256×64b）+ Bimodal 2-bit PHT（256×2b）**；★ **v2.3 范围修订（A5 决策）**：**本期 MUST 含增量 1**（BTB 增 `is_unconditional` 位 + `jal`/`j` 建项 ⇒ 恒 taken 预测、**不占 PHT**）⇒ ★ **GShare / RAS 本期「先量收益再定」**（增量 2 / 增量 3 ⇒ 见 **Task 7**）⇒ ★ **MUST NOT** 在 Task 7 实测前把 GShare/RAS 写成「不在范围内」（AC-16 的「不达标 ⇒ MUST 拉回 GShare」分支是**强制**的 ⇒ 范围只能由**实测**关闭，不能由声明关闭）
- [ ] **AC-2** `cpu_factory_chmem.h` **已注册** BP plugin，canonical ordering 断言 PASS；BP 在 **fetch 侧**的注册位已定义（ADR-048）
- [ ] **AC-3** ★ **台账逐条表**（架构简报 §5.1，14 条）：每条有明确态 + CH_MEM 锚点 + 显式理由 + 是否删除该行；★ **CH_MEM 侧 case 名与 TLM-only case 名零交集**；★ **台账 §7 的 BP 裁决行同步**
- [ ] **AC-4** **BTB miss ⇒ 不预测**（不跳转、不 flush）
- [ ] **AC-5** 预测方向/目标为**活节点**（`impl()->is_const() == false`）⇒ 禁 `static_cast<bool>`
- [ ] **AC-6** ★ **错路指令副作用抑制** —— ★ **flush 判据（v2.3 唯一权威 —— ★「门控」，不是「替换分量」）**：`flush = hold_z | (taken_buf & !spec_ok_buf) | mispredict_A_buf`
      - ★★ **v2.3 更正**：旧表述「`br_taken_buf` 分量**替换**为 `mispredict_buf` 分量」**是错的** ⇒ 那会让「未投机而实际跳转」（BTB miss / 情形 B / `jalr`）的 fall-through **副作用泄漏** ⇒ 校验和不等 ⇒ **该表述作废**
      - ★★ **`spec_ok_buf` = 「投机**被证明正确**」（情形 C = `pv & pt & actual_taken & (pred_target == actual_target)`）**⇒ **不是**「投机臂**确实生效**」（= `pv & pt`）⇒ ★ 后者在**情形 F** 下也为真 ⇒ 会让 F 的错路**不被抑制** ⇒ 泄漏 ⇒ ★ **裁决 A1**（architect 已登记；实施侧已取 `case_c` 一侧 ⇒ 更保守 ⇒ 只可能多 flush ⇒ 不可能假绿）
      - ★ **BP 关闭时 `spec_ok_buf` / `mispredict_A_buf` 恒 false ⇒ `flush == taken_buf` ⇒ 与现状逐位相同 ⇒ 零退化**
      - ★★ **情形 F**（预测 taken 到 P / 实际 taken 到 T≠P）= ★ **简报未列的规格缺口** ⇒ **MUST** 纠到实际目标 T 且 **MUST flush**⇒ 实现上**不需独立选择臂**（`case_c` 因目标不一致恒 false ⇒ 落到 `actual_taken` 臂 + `spec_ok=0` ⇒ flush 生效）
- [ ] **AC-6b** ★★ **正确预测 ⇒ 该分支自身的 redirect 被抑制**（目标指令**只被取一次**）⇒ 验证：host 侧 PC 轨迹**按迭代边界切分后，每段内任一静态地址的取指次数 ≤ 1**；★ **迭代边界的判定规则**：以「循环回边 PC（取指次数最多的转移源地址）」为段起点。★ **MUST NOT** 写成「全程无重复 PC」（循环回边使同一地址合法重复上千次）或「无连续重复」（自指分支在正确预测下产生合法的连续重复）
- [ ] **AC-7** `[chmem]` 47 ELF + `[verilator]` 48 ELF **0 退化**；★ **双跑**（BP 关一遍 + BP 开一遍，两套全绿）；★ **BP on/off 的 4 档校验和逐位相等**
- [ ] **AC-8a** 4 档在 BP 开/关下**校验和逐位相等**（功能，无阈值）
- [ ] **AC-8b-1** ★ **结构等式（含暖机修正）**：`always_taken` 档上 `Δcycles(bp=off − bp=on)` **逐位等于**「该档在 BP 关态下的 **条件分支** taken 转移数 × 1 拍 **− W**」（★ **N 的口径**：★ **MUST NOT** 含 `jalr`；★ `jal` 含（`is_unconditional` 位使其可预测）；★★ **N MUST 由源码语义 / 独立计数导出** —— **MUST NOT** 用 `(cycles − pc_changes)` 反推（★ 前置1 实测：该差值**不能**作为通则 —— `always_taken` 档的循环回边（`0x80000124 bne → 0x800000e8`，目标 ≠ pc+4）taken 1999 次却**不计入**该差值 ⇒ 该差值的语义**未确认**，**MUST NOT** 当作「taken 总数」）；★ **MUST NOT** 含循环回边（其是否计代价**未知** ⇒ 计入会引入未定义误差）），其中 **W = 暖机拍数**（★ 推导：单条静态分支需 2 次暖机 —— 迭代 1 = BTB miss、迭代 2 = 计数=1 预测不跳转 ⇒ ★ **W = 2 × 该档计入 N 的那组静态条件分支数**（★ **MUST NOT** 把循环回边计入 W —— 它已被排除出 N），★ **MUST NOT** 写死魔数 ⇒ ★ **MUST 实测并报告 W**）⇒ ★ **「零误预测档」的准确表述 = 「零*代价*误预测**」。★ **MUST NOT** 写成「逐位等于 N×1」。★★ **本 AC 的等式 MUST 在 ② 实施时用「BP 关态的 taken 数」独立验证**（前置1 未确认该模型对**所有** taken 分支成立）
- [ ] **AC-8b-2** ★ **净收益模型（★ v2.2 按情形 A/B 修正）**：其余档 `Δcycles(off − on) = N_correct×1 − N_false_A×1`（N 由 AC-15 计数器导出）⇒ ★ **符号为 `off − on`**；★ **N_false MUST 只计情形 A**（预测 taken / 实际 not-taken，代价 **1 拍**）；★ **情形 B**（预测 not-taken / 实际 taken）代价 **0 拍**（与无 BP 行为相同）⇒ **MUST NOT** 计入 ⇒ ★ **MUST NOT** 用「2×命中率 − 1」推导门槛（该式仅在情形 B ≈ 0 时成立）
- [ ] **AC-8b-3** **有界替代**（不引入计数器时）：只断言 `0 ≤ Δcycles(off − on) ≤ N_taken×1`，精确净收益降级为报告项
- [ ] **AC-8c** ★ **反证（限定 `always_taken` 档）**：该档 `Δcycles(off − on) == 0` ⇒ **BP 未生效**。⚠️ 该判据对 `always_not_taken` 档**不成立**（该档预测不跳转与无 BP 等价 ⇒ Δ 合法地 ≈ 0）
- [ ] **AC-8d** ★ **信号强度前置条件**：若前置1 报告的跨档 taken 转移数差 **≤ 1 拍/iter**（★ **含恰好 1.0**）⇒ **跨档判据不可用** ⇒ 改用「`always_taken` 单档绝对 Δ + 解析模型」。★ **判定权归前置1** ⇒ **✅ 前置1 已判定（2026-10-24）：跨档差 = 恰好 1.000 ⇒ 触发 ⇒ 本 change 的性能判据 MUST 用单档绝对 Δ**
- [ ] **AC-8e** ★ **跨档判据用差值形式**（★ **已被 AC-F1-6 判定为不可用** —— 跨档差恰好 1.000 ⇒ 恒真；保留此条仅为**记录**，MUST NOT 作为判据）：`Δ(mode) − Δ(always_not_taken)`
- [ ] **AC-9** ★ **具名门禁清单**：8 个门禁脚本（`verify_adr` / `verify_plugin_decision` / `check_plugin_portability` / `doc_link_check` / `roadmap-r9-check` / `check_xlen_hygiene` / `check_tlm_freeze` / `check_coverage_parity_ledger`）全 PASS + `bash tools/run_chipforge_tests.sh --all`。★ **失败集合判据**：失败集合 MUST **⊆ 已登记的 pre-existing 清单**（`tests/README.md` §已知问题 + 本仓库基线）且 ★ **新增失败 MUST = 0** ⇒ ★ **MUST NOT** 硬编码「恰 1 个」作为期望集合（清单会变 ⇒ 硬编码会假 RED，或让 builder 把额外失败「认领」为已知 ⇒ **掩盖回归**）
- [ ] **AC-10** 台账重判完毕 + **§2 族级判定行同步** + ★ **计数同步（注意二义）**：台账 `:16` 的 **463 = TLM 独占 case 数**（**本 change MUST 保持 463 不变**）· 台账 `:80` 的 **463 = 条目级台账条目数**（= 463 − 被删行数；★ 本 change 的 14 条**全部保留行**（不删）⇒ 仍为 463）⇒ ★ **MUST 写明本 change 采用哪种口径** + ★ **`check_coverage_parity_ledger.sh` exit 0**（CI warn 级 ⇒ 无强制力 ⇒ 须本地跑 + 输出贴 PR）
- [ ] **AC-11** **TLM 侧 `branch_predictor.{h,cpp}` 全程保留**（删除属 B3，需双前置）
- [ ] **AC-12** ★ **「不做什么」机检化** —— ★ **与 AC-25 同口径**（★ **MUST NOT** 写「变更集 grep」：变更集无基线定义 ⇒ 等价自证；且 tree-wide grep 必误报（TLM 侧 `branch_predictor.h` 合法含 GHR/RAS））⇒ 判据 = **AC-25 的脚本化门禁**：scope = `ip/cpu/plugins/branch_predictor_chmem.h`（不存在则 skip）+ 新增文件名匹配 `*gshare*`/`*ghr*`/`*ras*`
- [ ] **AC-13** ★ **BP 必须可学**：★ **同向连续两次解析后**，该分支**下一次**被取时预测命中（PHT 更新使能 = 分支已解析，**任何方向**；更新地址 = 分支解析处 `KT::PC`，与预测 `pc_reg` **同指一条分支**）。★ **MUST NOT** 写成「第二次被取即命中」—— PHT 饱和阈值是 2 ⇒ 首次解析后 counter=1 ⇒ 仍预测 not-taken
- [ ] **AC-14** ★ **`jalr` 不建 BTB 项** ⇒ 走顺序取指；无条件跳转不占 PHT 表项 ★ **v2.3 修订**：`jalr` 的目标依赖寄存器 ⇒ 静态 BTB 项会**陈旧** ⇒ 除非**同时实现 RAS**（增量 2），否则 `jalr` MUST NOT 建项（`ret` 亦然 ⇒ `ret` = `jalr x0, 0(x1)`）⇒ ★ **与 AC-17 的分工**：AC-17 覆盖 **`jal`/`j`**（PC 相对 ⇒ 目标静态 ⇒ 可安全建项）⇒ 二者不冲突
- [ ] **AC-16** ★★ **净收益率裁决点（两分支终局式，★ v2.3 措辞拆分）**：实测 **净收益率** := `Δcycles(bp=off − bp=on) / N_taken_total`（★ **不是命中率**）
      - ★★ **v2.3 更正（A6）**：★ **「净收益率达标」与「绝对值门禁达标」是**两件事** ⇒ **MUST NOT** 用前者直接推出后者 ⇒ ★ 实测对照（Dhrystone，CH_MEM/Verilator，2026-10-26）：净收益率 = 68044/120065 = **56.7%**；而绝对值门禁 DMIPS 1.4 需闭合 **84.51%** 的 TLM–CH_MEM 差距（`101470/120066`）⇒ ★ **同向但不同阈值** ⇒ ★ 裁决 MUST 分别落
      - ★ **「达标」（≥ **84.5%**）⇒** 依 ADR-088 §5.3.2 落**结论**（恢复 / **不恢复**）⇒ **不恢复时按 P-3 临时口径**结案
      - ★ **「不达标」⇒** ★ **MUST NOT** 以「GShare 延后」结案 ⇒ **MUST** 把 GShare 拉回范围（或另立 change）⇒ ★ **v2.3 落实方式**：由 **Task 7 的三档增量实测**决定（增量 1 → +2 → +3 逐档累加 ⇒ 哪一档让净收益率过 84.5% 就停在哪档）
      - ⇒ ★ **MUST NOT** 只写「MUST ≥ 84.5%」（不达标时 AC 恒 RED ⇒ 与「不恢复时按 P-3 结案」互斥 ⇒ 归档路径被自己的 AC 堵死）
- [ ] **AC-17** ★ **无条件跳转的可预测性** —— BTB 表项 MUST 带 `is_unconditional` 位（`jal` 不占 PHT 表项 ⇒ 若无此位则 `jal` 的方向**无来源** ⇒ 永远学不会）⇒ ★ **v2.3（A5 决策）：本条 = 增量 1，MUST 在本期落地** ⇒ ★ `jal`/`j` 的 BTB **写使能** MUST NOT 受 `is_branch`（条件分支标志）限制 ⇒ 但 **MUST NOT** 占 PHT 表项 ⇒ ★ 预测方向 = `is_unconditional` ⇒ 恒 taken（不查 PHT）⇒ ★ **TLM 参考实现（`branch_predictor.h`）无此位** ⇒ 属**新增设计**
- [ ] **AC-18** ★ **陈旧目标自愈** —— 替换策略为**直接覆盖** ⇒ 目标变化的分支再次 taken 时**同槽重写** ⇒ **天然自愈**，MUST NOT 另设误预测计数器
- [ ] **AC-19** ★ **ADR-088 §5.3.1 根因修订**（★ **必须** —— 现有表述已被 E-1/E-2 证伪）⇒ 删掉「参考实现侧存在预测器 ⇒ 预测命中不冲刷」⇒ 改述为「单遍模型重定向零代价 vs 两级取指每次跳转 1 拍额外取指」+ 附实测数据 ⇒ 走 `docs/architecture/adr.md` 注册表 + `verify_adr.sh`
- [ ] **AC-20** ★ **B3 结论的正确表述** —— 「① 已达成；**② 未达成**（CoreMark harness 属交付点 B）⇒ **B3 的 BP 阻塞未解除**」+ 登记「CI 判据 4 未满足（CI 不构建项目）」⇒ ★ **MUST NOT** 写成「① ② 已达成 ⇒ B3 阻塞解除」（= **假完工声明**）
- [ ] **AC-21** ★ **绝对值门禁结论落地** —— 依 AC-16 的实测结果在 **ADR-088 §5.3.2** 落**结论**（恢复 / **不恢复**）⇒ 不恢复时按 **P-3 临时口径**（cycle 比率 ≤ 1.15 + notes 标注）结案
- [ ] **AC-22** ★★ **五级 mux 嵌套契约**（★ 本 change 的核心契约，design.md §1.2 是其唯一落地）⇒ `next_pc` MUST 是 `trap > branch-actual/mispredict > freeze(hold) > bp-predict > seq`；★ **MUST 可 grep 断言**：`update_pc` 的源码 MUST 是**多层 select 链**且 MUST 出现**具名的 trap 项**（如 `trap_redirect` / `trap_target`）⇒ 否则后续 `csr-chmem-trap-pc-jump` 无处插入 ⇒ 返工。★ **MUST NOT** 只留「注释 + 结构预留」（注释不可机检）
- [ ] **AC-23** ★ **开关面默认启用 + 生效值回显 + 参数转发完整** —— BP **MUST 默认启用**（否则「未生效被误判为无效」的假绿通道）⇒ runner MUST 在 stderr 回显**实际生效状态与条目数**⇒ 测量脚本 MUST 据此断言与请求一致；★ `build_cpu_with_elaborate`（`cpu_factory_chmem.h:451-458`）**MUST 转发全部 4 个参数**（现状已丢 3 个）⇒ ★ **MUST NOT** 用「显式登记为 known gap」当逃生口
- [ ] **AC-24** ★ **M 越界 fail-fast** —— `--btb-entries N` 的 N MUST 落在 {16,32,64,128,256}；越界 MUST **快速失败**（★ **MUST NOT** 静默截断 ⇒ 容量语义错却全绿）
- [ ] **AC-25** ★ **scope 机检须脚本化**（★ 非手工 grep）—— `tools/check_plugin_portability.sh` 新增 **Check 14**（★ 该脚本已是 PR **阻塞**门禁 ⇒ **无需构建** ⇒ CI 可承担）：scope = `ip/cpu/plugins/branch_predictor_chmem.h`（文件不存在则 skip）+ 新增文件名匹配 `*gshare*`/`*ghr*`/`*ras*` ⇒ 违反 ⇒ exit 1。★ **MUST NOT** 做成 tree-wide grep（TLM 侧 `branch_predictor.h` 合法含 GHR/RAS ⇒ 必误报）
- [ ] **AC-15** ★ **host 可见事件计数（5 个）**：① **已执行(retired)指令数**（★ **归前置2** —— 需 flush 信号；★ 前置1 **不**交付它，见前置1 的 owner 表）② **FLUSH 事件数** ③ **BP 命中次数**（N_correct）④ **误预测次数**（N_false）⑤ **taken 转移数** ⇒ 全部经 `cpu_verilator_sim --stats` 输出。★ **「取指数」≠「已执行数」**（前者含投机取指）⇒ MUST 用 **retired**（被 flush 抑制的不计）


- [ ] **AC-26** ★★ **错路指令维度**（★ v2.3 新增，来自 PoC B-2）—— 一切情形判定 MUST 排除「**被解析指令本身在错路上**」：`case_a = (!flushed) & pv & pt & (!actual_taken)`（`flushed` = branch plugin 的 `BRANCH_FLUSHED`）⇒ ★ `BRANCH_FLUSHED` 缺失 MUST **fail-fast**（抛异常）⇒ MUST NOT 静默降级为 false ⇒ ★ 单元锚 MUST 覆盖（PoC 的 R11：去掉 `!flushed` 门控 ⇒ RED）
- [ ] **AC-27** ★★ **增量分档的量收益**（★ v2.3 新增，来自 **A5 决策**）—— 三个增量 MUST **可独立开关 + 独立测量**：增量 1 = `is_unconditional` + `jal` 建项；增量 2 = RAS 8-entry；增量 3 = GShare ⇒ ★ 每档 MUST 报出（① Dhrystone 的 ΔFLUSH / Δcycles / 净收益率 ② branchbench 4 档的校验和逐位相等（零退化））⇒ ★ **判据**：净收益率 ≥ 84.5% ⇒ 绝对值门禁可恢复 ⇒ GShare/RAS 可延后；否则 MUST 把下一档拉进本期 ⇒ ★ **MUST NOT** 用「声明不在范围内」替代实测（见 AC-1 / AC-16 的 v2.3 修订）

## ★★ per-file × per-symbol owner 表（★ 逐条抄进本 change —— 跨 change 引用会被漏读）

| 文件 / 符号 | 本 change | 其它 change | ★ 约定 |
|---|---|---|---|
| `tests/cpu/manual_elf/branchbench/branchbench.c` | ★ **前置1 owner** | 前置2 / 主体 ✗ | 主体 MUST NOT 触碰（硬门禁） |
| `soc/cpu/docs/dse/branchbench-baseline-matrix.csv` | ★ **前置1 建列**（`workload_sha` / `build_id` / `bp_mode=off`）+ 改按表头取列 | 前置2 / 主体 **仅追加行** | 建列归前置1；追加归后两者 |
| `tools/measure_branchbench.sh` | ★ **前置1 owner**（改名 / 按表头取列 / **捕获 stderr**） | 前置2 **仅追加** `--bp=on\|off` flag + 生效值断言；主体 ✗（硬门禁） | ★ **捕获 stderr 归前置1**（否则假绿通道开着）⇒ 前置2 只加 flag |
| `tools/verilator_runner/*` | ★ **前置1 建 `--stats` 框架 + `pc_changes`/PC 直方图**（★ **NOT** retired —— retired 需 flush ⇒ 归前置2） | 前置2 **补 FLUSH 计数**；主体 **补 BP flag + 回显** | ★ **框架归前置1**，增量归后两者，**MUST NOT** 另起一套 |
| `ip/cpu/plugins/branch_predictor_chmem.h` | ✗（禁 `ip/**`） | 前置2 **创建（最小版）**；主体 **重写（完整版）** | ★ 前置2 的 PoC 版 MUST NOT 被主体当作已完成 ⇒ 主体 AC-1 MUST 检查**完整功能**（非文件存在）；★ PoC 残留的**删除/重写责任归主体**；★ `tests/cpu/test_cpu_chmem_bp_poc.cpp` 的 8 case 在主体后 MUST 仍绿（机制锚）⇒ ★ **该 8 case 只能依赖公开接口** |
| `ip/cpu/plugins/ibus_chmem.h`（`update_pc` 的 mux + flush） | ✗ | 前置2 **首次改**（5 臂 mux + FLUSH 门控 + FLUSH 计数）；主体 **二次改**（补完整语义 + 过时注释 #8） | ★ **同函数两次改** ⇒ 前置2 的版本 MUST 可被主体**增量扩展**（MUST NOT 要求主体重写）⇒ ★ 主体 MUST 继承前置2 的**五臂 select 链**结构 |
| `ip/cpu/cpu_factory_chmem.h`（注册 + 开关 + 参数转发） | ✗ | 前置2 **首次注册**（BP 开关）；主体 **补全**（4 参数转发修复 #9 + 完整注册） | ★ 同上 ⇒ 增量扩展 |
| `docs/architecture/adr/ADR-088-*.md` | ✗ | 前置2 写 **§5.3.2 的「机制结论」段**；主体 写 **「绝对值门禁结论」段** | ★ **主体 MUST NOT 覆盖前置2 的机制结论段**（否则「机制不成立」的结论会被主体的「达标」结论抹掉） |
| `docs/architecture/tlm-coverage-parity-ledger.md` | ★ **前置1 MUST NOT 触碰** | 前置2 MUST NOT 触碰；主体 owner | ★ 台账重判只归主体 |
| `ip/cpu/plugins/branch_predictor.{h,cpp}`（TLM 侧） | — | — | ★ **全程保留**（主体 AC-11） |

★ **本 change 的角色**：主体 ⇒ 本表列出的「主体」项**归本 change**；★ **前置2 已建的版本 MUST 增量扩展**（MUST NOT 重写）。
## ★ 兜底句纪律

任何「上报 architect / 待裁决」句 MUST 同句给出 **① 触发阈值 ② 落点文件 ③ 可判定时限**（「≤ N 个工作日」或「本 change 内闭环」）；MUST NOT 以「等待回复」结束本 change。

## ★ builder 开工须知（★ **逐条抄进本 change**，不跨 change 引用 —— 跨 change 引用会被漏读）

| # | 须知 | 归属 |
|---|------|------|
| 1 | 台账条目行**必须正好 7 列**；**理由列不得含未转义的 `\|`** | ★ 本 change |
| 2 | 新增 CH_MEM 侧 case 前**先快照 TLM-only 名单**（`comm -23`）再命名 | ★ 本 change |
| 3 | 台账脚本需**两个 binary** ⇒ **本地跑 + 输出贴 PR**（CI warn 级无强制力） | ★ 本 change |
| 4 | CSV / 脚本的**改名必须同一 commit** | 前置1 |
| 5 | CSV **按表头名取列**（**强制**；现 notes 含逗号 ⇒ 位置访问必错位）；`--check-baseline` 分组键 = `(workload_sha, bp_mode, build_id)` | 前置1 建列 / 本 change 追加行 |
| 6 | `--btb-entries N` 合法域 = **16/32/64/128/256**；**越界 fail-fast**（不得静默 clamp） | ★ 本 change |
| 7 | ★ 本 change **MUST** 改 `ip/**`（BP 组件 + PC-mux）⇒ 「不得触碰 `ip/**`」**不适用于本 change** | 本 change 专属 |
| 8 | 修正 `ibus_chmem.h:83-87` 的**过时注释**（「aread/sread 都是 1-cycle」与 aread 0-cycle 的承重前提矛盾） | 前置2（首个可改 `ip/` 的 change） |
| 9 | `build_cpu_with_elaborate`（`cpu_factory_chmem.h:451-458`）**现状就已丢失** `enable_mmu`/`mmu_mode`/`enable_cache` 三个参数 ⇒ **BP 参数进来前 MUST 修全部 4 个的转发**（★ **MUST NOT** 用「显式登记为 known gap」当逃生口 —— 与 spec 的「MUST NOT 在某条路径上丢失」冲突） | ★ 本 change |
| 10 | 修正 `branch_chmem.h:20` 的注释（「JAL/JALR 推迟」）与 `:221-226` 的实装不符 | ★ 本 change |

★ **本 change 专属边界**：本 change 的 diff **MUST NOT** 触碰 `tests/cpu/manual_elf/branchbench/` 与 `tools/measure_branchbench.sh` 的**模式名数组与表头解析逻辑**（那是前置1 的范围）⇒ **三 change 边界为硬门禁**。

## ★ 原「归档时必须完成」（★ v2.2：**已移入 `## Acceptance` 作为 AC-19 / AC-20 / AC-21** —— rdd-verifier 只核 `## Acceptance` 下的勾选框 ⇒ 独立 H2 段落**无强制力** ⇒ 会成为假完工声明的温床）

⇒ ★ 三条义务现为：
- **AC-19** ADR-088 §5.3.1 根因修订
- **AC-20** B3 结论的正确表述
- **AC-21** 绝对值门禁结论落地

## 实测留证

（实施后填写）

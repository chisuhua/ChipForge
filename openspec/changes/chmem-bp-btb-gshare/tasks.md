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
> **权威规格**: `ip/cpu/docs/bp-btb-gshare-design-brief.md` v2.2 §2.1 / §3.2 / §3.3 / §3.4 / §4.3 / §5.1 / §5.5
> ★ **两个前置：✅ 已归档（2026-10-26）** —— `2026-10-26-chmem-branch-microbench-fix`（12/12 完成）+ `2026-10-26-chmem-bp-btb-gshare-poc`（**13/17 AC 闭合**；★ 4 项未闭合 AC-F2-2 / 10 / 11 / 15 的原因与归属见该 change 的**归档注记**）⇒ ★ **开工门禁已解除** ⇒ ★ 继承项：**AC-F2-11（无条件跳转与条件分支分开统计）/ AC-F2-15（无条件跳转占比报告）⇒ 归本 change 的 Task 2h**（★ 其余两项 AC-F2-2 / AC-F2-10 归 S4 / ADR-088 §5.2）⇒ ★ **MUST NOT** 把「可开工」读成「可跳过前置的验收口径」
> ★ **执行文件** `.rddf/plans/chmem-bp-btb-gshare.md` 顶部有 **v2.1 覆盖声明**（★ **v2.3 覆盖声明尚未落到该文件** —— ★ **该文件是 builder 的执行 SSOT** ⇒ ★ **MUST NOT** 只读本 change 的 `tasks.md` / `design.md` 就开工 ⇒ ★ **builder 开工前 MUST 先把本 change 的 v2.3 覆盖声明同步进 plan 文件**（含其 owner 表里「flush 双分量」「四情形 select 链」两条作废标注）⇒ ★ **同步清单见本 change 的「★ planner 交接清单」段**

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
  - [x] **Step 1（增量 1）** ★ **2026-10-26 已完成**（实现 + 4 个单元锚 + 双负载实测 ⇒ 见 Step 4 的表） ★ **v2.3.1 更正（实测核对 PoC `branch_predictor_chmem.h:160-163`）**：★ **62b 编码已含 `is_unconditional`（bit[3]，余 2b 保留）⇒ 本步 NOT 加位、NOT 重算位宽**；★ **真正的缺口是三处**：① **branch plugin 导出 jal 判别 payload**（`branch_chmem.h:201-203` 已算出 `is_jal`/`is_jalr` 但**未导出** ⇒ BP 侧无法区分 `jal`/`jalr` ⇒ 保守地只让条件分支建项 ⇒ `jal` 永不被预测）② **ch 写路径补写该位**（`:417` 的 `packed_w = tgt_shifted | pc_shifted | valid_at` **未写 `is_unconditional` 位** ⇒ DUT 侧写出的表项该位恒 0 ⇒ 与 host 侧 `pack_entry`（`:210-218` 写了该位）**语义漂移** ⇒ 缺口 #5「双实现」的具体化）③ **host / ch 两条写路径的语义对齐断言**（同一条 BTB 表项 ⇒ host `pack_entry` 与 ch 写路径产出的 word MUST 逐位相等）⇒ ★ 另：`jal` 建项 ⇒ BTB 写使能从 `taken & is_branch & !flushed` 扩为「`taken & !flushed & (is_branch | is_jal)`」⇒ `jalr` 仍不建项（AC-14）
  - **Step 2（增量 2，条件性）** RAS 8-entry（`jalr`/`ret` 模式预测）⇒ ★ **TLM 参考实现无 RAS ⇒ 新增设计** ⇒ ★ **MUST** 先看 Step 4 的实测再决定是否做
  - [ ] **Step 3（增量 3，GShare）** ★ **2026-10-26 由「条件性」转为「本期做」** —— ★ **实测依据**：增量 1 后净收益率 **78.3% < 84.5%** ⇒ ⇒ **AC-16 的「不达标 ⇒ MUST 把 GShare 拉回范围」分支被触发** ⇒ ★ GShare 是 **移植**（TLM 参考 `branch_predictor.h` 的 `gshare_predict`/`gshare_update`/`global_history_`，`GHR_BITS=8`）⇒ 成本低 ⇒ ★ 还差 **6.2pp ≈ 7440 cycles** ⇒ ★ **GShare + 增量 1 预期可过 84.5%**（⇒ 绝对值门禁可恢复）
  - ★★ **【2026-10-26 已完成：增量 1（`is_unconditional` + `jal`/`j` 建项）】** —— 实施 + 单元锚（4 个新 case）+ 双负载实测：
      | 配置 | DHrystone cycles | FLUSH_COUNT | 净收益率 | branchbench 4 档 cycles |
      |---|---|---|---|---|
      | BP=off | 914544 | 120065 | — | 86034 / 88034 / 86534 / 87056 |
      | BP=on，**增量 1 关**（= PoC 行为） | 846500 | 52021 | **56.67%** | 84038 / 84040 / 84787 / 85037 |
      | BP=on，**增量 1 开**（★ 本轮） | **820513** | **26034** | **78.3%** | **78041 / 78043 / 78791 / 79041** |
      ⇒ ★ **ΔFLUSH ≡ Δcycles ≡ 25987**（DHrystone）⇒ 每个被消除的 taken 气泡**恰 1 拍**（★ 恒等式在增量 1 后**继续成立**）
      ⇒ ★ **净收益率 56.67% → 78.3%（+21.6pp）** ⇒ ★ 与 Step 4b 的**静态上界估算**（≈ 80%）**高度吻合** ⇒ ★ 估算方法有效
      ⇒ ★★ **仍未达 84.5%**（还差 **6.2pp ≈ 7440 cycles**）⇒ ⇒ **GShare（增量 3，移植）MUST 进本期** ⇒ ⇒ Step 3 从「条件性」转为「**本期做**」
      ⇒ ★ **单元锚（新增 4 case）**：**R11**（`jal` 建项 ⇒ 恒 taken；★ PHT counter 恒 0 ⇒ 鉴别力强）/ **R12**（`jalr` 不建项，AC-14）/ **R13**（同槽覆盖 ⇒ `is_unconditional` 位**翻转** ⇒ 方向改由 PHT 决定）/ **R14**（host `pack_entry` 与 ch 写路径的语义对照）⇒ **14 case / 205 assertions**
      ⇒ ★ **开关面**：`bp_uncond` 工厂布尔（默认 true）+ CLI `--no-bp-uncond` + 生效值回显（`BP=on btb_entries=256 bp_uncond=on/off`）
      ⇒ ★ **顺手修**：`build_cpu_with_elaborate` 此前**只转发 5 个参数**（`enable_mmu`/`mmu_mode`/`enable_cache`/`enable_bp`/`btb_entries` **全被静默丢弃**）⇒ 已全量转发（须知 #9）
  - **Step 4（量收益 = 判据）** 每档 MUST 实测并落表：① **Dhrystone**（CH_MEM/Verilator）的 `FLUSH_COUNT` / `CYCLES` / `ΔFLUSH` / `Δcycles` / **净收益率**（★ 口径归 **A6**：分母 = `N_taken_total` 或 `FLUSH(off)`？★ 两者在 Dhrystone 上几乎相等 ⇒ 结论稳健）② **branchbench 4 档**的校验和**逐位相等**（零退化）
  - **Step 4b（★ 收益上界估算 —— planner 2026-10-26 静态分析，**仅供 Step 4 的量级参考，不参与判据**）**：Dhrystone 的 `FLUSH(off)` = **120065**（60.0/iter）⇒ BP=on 后残余 **52021**（26.0/iter）⇒ ★ 这 26/iter 的残余 flush **绝大部分是无条件跳转**（`jal`/`ret`）—— 因为 BTB 写使能 = `taken & is_branch`（**仅条件分支**）⇒ `jal`/`ret` **从不建项 ⇒ 从不预测**⇒ 静态构成（`.text` 共 55 个分支站点 = 无条件 36〔`ret` 16 / `jal` 15 / `j` 5〕+ 条件 19）⇒ 增量 1 覆盖 `jal`/`j`（20/36 的无条件站点）⇒ ★ **静态上界 ≈ 14/iter ≈ 28,000 cycles** ⇒ ★★ **这是「假设那 20 个 `jal`/`j` 站点每 iter 全部命中」的**上界** ⇒ ★ **实测低于它 NOT 判 FAIL** （真实命中率与 `ret` 占比未知 ⇒ 本估算**只用于判断量级是否值得做**，★ **不作为 AC 的判据**，★ AC-27 的判据只看**实测净收益率**）⇒ ★ 与「达 DMIPS 1.4 还需 33426 拍」**同量级** ⇒ ⇒ **增量 1 的量级值得做**，但**是否足以达 1.4 MUST 由 Step 4 实测回答**
  - **Step 5（★ 建议执行序 —— 按「实现成本」而非增量编号）**：**增量 1（`is_unconditional`+`jal`，新增设计，成本低）→ 增量 3（GShare，★ **TLM 参考已有 ⇒ 移植**，成本低）→ 增量 2（RAS，★ **无参考实现 ⇒ 新增设计**，成本高）**⇒ ★ 依据：GShare 是**移植**（`branch_predictor.h` 的 `gshare_predict`/`gshare_update`/`global_history_`，`GHR_BITS=8`）而 RAS 是**新设计** ⇒ ⇒ **先用便宜的**
  - **Step 5b（★ 「独立开关」的实现机制 MUST 钉死 —— metis 指出自由发挥空间太大）**：开关面 = ★ **工厂布尔参数 + CLI flag**（★ **先例**：`cpu_factory_chmem.h:113` 的 `enable_bp` —— elaboration 期定形 DAG、进程内固定、CLI 透传 ⇒ 单 binary 多次运行即可逐档测量）⇒ ★ **MUST NOT** 用编译期宏（⇒ 3 次重编译 + 宏组合矩阵爆炸）⇒ ★ **MUST NOT** 发明运行期可重配 mux（CH_MEM 是静态 elaboration ⇒ 白付每拍一级 select 的硬件成本）⇒ ★ 三档 ⇒ 3 个布尔参数（如 `bp_uncond` / `bp_gshare` / `bp_ras`），CLI 对应 flag，★ **生效值 MUST 回显**（先例：`BP=on/off btb_entries=N`）
⇒ ★★ **【2026-10-26 实测更正】原判断「branchbench 上 `jal` 覆盖的增量收益结构性为零」是**错的** ⇒ 必须更正**：★ 实测（增量 1 开 vs 关，branchbench 4 档）：m0 **84038 → 78041**（−5997）· m1 **84040 → 78043**（−5997）· m2 **84787 → 78791**（−5996）· m3 **85037 → 79041**（−5996）⇒ ★★ **4 档全部大幅改善** ⇒ 校验和**逐位不变**（103/183/147/101）⇒ ★ **根因**：★ 静态分析只数了 `jal`，**漏了 `j`**（`j` = `jal x0` ⇒ **同 opcode** ⇒ **也在增量 1 的覆盖范围内**）⇒ ★★ **branchbench 也是有效的增量生效证据**（4 档 + 校验和逐位不变 ⇒ 鉴别力强于 DHrystone 的单一 `tohost=1`）⇒ ⇒ ★ **判据更正为「branchbench 4 档 + DHrystone 双证」**，两者缺一 ⇒ 不下结论
  - **Step 6（决策 + ★ 终局兜底）**：若「增量 1（+3）」的净收益率 **≥ 84.5%** ⇒ 绝对值门禁可恢复 ⇒ 剩余增量（RAS）**显式结案为「本期不做」**（**并登记**）⇒ ★★ **若增量 1+2+3 全部用尽仍 < 84.5%** ⇒ MUST 按 **P-3 临时口径**结案（ADR-088 §5.3.2）⇒ 并把「绝对值门禁恢复」**移交后续 change** ⇒ ★ **MUST NOT** 让本 change 因 AC 不可满足而**无法归档**（★ 这正是 AC-16 自己警告过的「归档路径被自己的 AC 堵死」模式）⇒ ★ 决策 MUST 落 **ADR-088 §5.3.2**（净收益率结论 + 绝对值门禁结论**分两件事**落 ⇒ 见 **A6**）

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
- [ ] **AC-9** ★ **具名门禁清单**：8 个门禁脚本（`verify_adr` / `verify_plugin_decision` / `check_plugin_portability` / `doc_link_check` / `roadmap-r9-check` / `check_xlen_hygiene` / …）+ ★ **v2.3.1 新增第 9 条：`tools/check_md_tables.py`**（★ GFM 表格结构检查 ⇒ 「行 cell 数 == 表头 cell 数」+ 「code span 内管道符转义」+ 「不断表」）⇒ ★ **MUST 被至少一个门禁脚本调用**（★ 建议并入 `tools/doc_link_check.sh`）⇒ ★ 理由：PoC 期间 **连续 3 轮 critic**（十三/十四轮）抓到的都是**表格结构伤** ⇒ ⇒ ★ 手写表格 MUST 跑它
- [ ] **AC-10** 台账重判完毕 + **§2 族级判定行同步** + ★ **计数同步（注意二义）**：台账 `:16` 的 **463 = TLM 独占 case 数**（**本 change MUST 保持 463 不变**）· 台账 `:80` 的 **463 = 条目级台账条目数**（= 463 − 被删行数；★ 本 change 的 14 条**全部保留行**（不删）⇒ 仍为 463）⇒ ★ **MUST 写明本 change 采用哪种口径** + ★ **`check_coverage_parity_ledger.sh` exit 0**（CI warn 级 ⇒ 无强制力 ⇒ 须本地跑 + 输出贴 PR） ★★ **v2.3.1 预警（planner 实测 2026-10-26）**：台账 `:24` 的头部声明是「covered 154 · partial **292** · not-covered 27」，但**条目级实测分布 = covered 154 · partial **282** · not-covered 27（合计 463）** ⇒ ★ **partial 差 10** ⇒ ⇒ ★ **本 AC 的「计数同步」MUST 以**条目级实测**为准**（并同时修正头部的 292 ⇒ 282）⇒ ★ 登记此漂移是为免 builder 在重判中途被头部数字误导
- [ ] **AC-11** **TLM 侧 `branch_predictor.{h,cpp}` 全程保留**（删除属 B3，需双前置）
- [ ] **AC-12** ★ **「不做什么」机检化** —— ★ **与 AC-25 同口径**（★ **MUST NOT** 写「变更集 grep」：变更集无基线定义 ⇒ 等价自证；且 tree-wide grep 必误报（TLM 侧 `branch_predictor.h` 合法含 GHR/RAS））⇒ 判据 = **AC-25 的脚本化门禁**：scope = `ip/cpu/plugins/branch_predictor_chmem.h`（不存在则 skip）+ 新增文件名匹配 `*gshare*`/`*ghr*`/`*ras*`
- [ ] **AC-13** ★ **BP 必须可学**：★ **同向连续两次解析后**，该分支**下一次**被取时预测命中（PHT 更新使能 = 分支已解析，**任何方向**；更新地址 = 分支解析处 `KT::PC`，与预测 `pc_reg` **同指一条分支**）。★ **MUST NOT** 写成「第二次被取即命中」—— PHT 饱和阈值是 2 ⇒ 首次解析后 counter=1 ⇒ 仍预测 not-taken
- [ ] **AC-14** ★ **`jalr` 不建 BTB 项** ⇒ 走顺序取指；无条件跳转不占 PHT 表项 ★ **v2.3 修订**：`jalr` 的目标依赖寄存器 ⇒ 静态 BTB 项会**陈旧** ⇒ 除非**同时实现 RAS**（增量 2），否则 `jalr` MUST NOT 建项（`ret` 亦然 ⇒ `ret` = `jalr x0, 0(x1)`）⇒ ★ **与 AC-17 的分工**：AC-17 覆盖 **`jal`/`j`**（PC 相对 ⇒ 目标静态 ⇒ 可安全建项）⇒ 二者不冲突
- [ ] **AC-16** ★★ **净收益率裁决点（两分支终局式，★ v2.3 措辞拆分）**：实测 **净收益率** := `Δcycles(bp=off − bp=on) / N_taken_total`（★ **不是命中率**） ★ **2026-10-26 增量 1 档实测 78.3% < 84.5% ⇒ 本 AC 判定为「不达标」⇒ ⇒ GShare 拉回本期（已落 Task 7 Step 3）⇒ ★ **但仍不勾选**：★ 本 AC 的**口径**归 **A6**（architect 未裁决）⇒ ⇒ ★ 勾选前 MUST 等 A6 落 ⇒ ★ **MUST NOT** 为归档而勾选
      - ★★ **v2.3 更正（A6）**：★ **「净收益率达标」与「绝对值门禁达标」是**两件事** ⇒ **MUST NOT** 用前者直接推出后者 ⇒ ★ 实测对照（Dhrystone，CH_MEM/Verilator，2026-10-26）：净收益率 = 68044/120065 = **56.7%**；而绝对值门禁 DMIPS 1.4 需闭合 **84.51%** 的 TLM–CH_MEM 差距（`101470/120066`）⇒ ★ **同向但不同阈值** ⇒ ★ 裁决 MUST 分别落
      - ★ **「达标」（≥ **84.5%**）⇒** 依 ADR-088 §5.3.2 落**结论**（恢复 / **不恢复**）⇒ **不恢复时按 P-3 临时口径**结案
      - ★ **「不达标」⇒** ★ **MUST NOT** 以「GShare 延后」结案 ⇒ **MUST** 把 GShare 拉回范围（或另立 change）⇒ ★ **v2.3 落实方式**：由 **Task 7 的三档增量实测**决定（★ **成本序：增量 1 → 增量 3（GShare，移植）→ 增量 2（RAS，新增设计）**（★ 与 Task 7 Step 5 一致 ⇒ **AC-16 与 Task 7 的顺序 MUST 同源**）逐档累加 ⇒ 哪一档让净收益率过 84.5% 就停在哪档）
      - ⇒ ★★ **v2.3.1 终局兜底（metis）**：**三档全部用尽仍 < 84.5%** ⇒ MUST 按 **P-3 临时口径**结案 + 把「绝对值门禁恢复」**移交后续 change** ⇒ ★ **MUST NOT** 让本 change 不可归档 ⇒ ★ **MUST NOT** 只写「MUST ≥ 84.5%」（不达标时 AC 恒 RED ⇒ 与「不恢复时按 P-3 结案」互斥 ⇒ 归档路径被自己的 AC 堵死）
- [x] **AC-17** ★ **无条件跳转的可预测性** —— BTB 表项 MUST 带 `is_unconditional` 位（`jal` 不占 PHT 表项 ⇒ 若无此位则 `jal` 的方向**无来源** ⇒ 永远学不会）⇒ ★ **v2.3（A5 决策）：本条 = 增量 1，MUST 在本期落地** ⇒ ★ `jal`/`j` 的 BTB **写使能** MUST NOT 受 `is_branch`（条件分支标志）限制 ⇒ 但 **MUST NOT** 占 PHT 表项 ⇒ ★ 预测方向 = `is_unconditional` ⇒ 恒 taken（不查 PHT）⇒ ★ **TLM 参考实现（`branch_predictor.h`）无此位** ⇒ 属**新增设计** ★ **2026-10-26 已落地**：`is_unconditional` 位（62b 编码的 bit[3]，★ **编码本就含该位 ⇒ 未加位**）+ `jal`/`j` 建项（写使能扩为 `taken & !flushed & (is_branch | is_jal)`）+ ch 写路径补写该位（★ PoC `:417` 缺 ⇒ 与 host `pack_entry` 漂移）⇒ ★ 单元锚 R11/R12/R13/R14 ⇒ ★ 收益实测见 Task 7 Step 4
- [ ] **AC-18** ★ **陈旧目标自愈** —— 替换策略为**直接覆盖** ⇒ 目标变化的分支再次 taken 时**同槽重写** ⇒ **天然自愈**，MUST NOT 另设误预测计数器
- [ ] **AC-19** ★ **ADR-088 §5.3.1 根因修订**（★ **必须** —— 现有表述已被 E-1/E-2 证伪）⇒ 删掉「参考实现侧存在预测器 ⇒ 预测命中不冲刷」⇒ 改述为「单遍模型重定向零代价 vs 两级取指每次跳转 1 拍额外取指」+ 附实测数据 ⇒ 走 `docs/architecture/adr.md` 注册表 + `verify_adr.sh`
- [ ] **AC-20** ★ **B3 结论的正确表述** —— 「① 已达成；**② 未达成**（CoreMark harness 属交付点 B）⇒ **B3 的 BP 阻塞未解除**」+ 登记「CI 判据 4 未满足（CI 不构建项目）」⇒ ★ **MUST NOT** 写成「① ② 已达成 ⇒ B3 阻塞解除」（= **假完工声明**）
- [ ] **AC-21** ★ **绝对值门禁结论落地** —— 依 AC-16 的实测结果在 **ADR-088 §5.3.2** 落**结论**（恢复 / **不恢复**）⇒ 不恢复时按 **P-3 临时口径**（cycle 比率 ≤ 1.15 + notes 标注）结案
- [ ] **AC-22** ★★ **五级 mux 嵌套契约**（★ 本 change 的核心契约，design.md §1.2 是其唯一落地）⇒ `next_pc` MUST 是 `trap > branch-actual/mispredict > freeze(hold) > bp-predict > seq`；★ **MUST 可 grep 断言**：`update_pc` 的源码 MUST 是**多层 select 链**且 MUST 出现**具名的 trap 项**（如 `trap_redirect` / `trap_target`）⇒ 否则后续 `csr-chmem-trap-pc-jump` 无处插入 ⇒ 返工。★ **MUST NOT** 只留「注释 + 结构预留」（注释不可机检）★★ **v2.3.1 断言收紧（metis：原断言会被「假 trap 项」骗过）**：★ **grep pattern MUST 匹配 select 链内的实参**（如 `select(\s*trap_redirect`）⇒ **纯注释行不命中** ⇒ ★ **且 MUST 钉占位臂的精确形态**（4 个具名信号 `trap_redirect`/`trap_target`/`freeze_hold`/`hold_pc` + 常 false/0 绑定 + **trap 臂在 select 链最顶层**的顺序断言）⇒ ★★ **且 MUST 有「占位臂惰性」的单元锚**：占位臂恒 false 时，BP 关闭态的 mux 输出 MUST 与现状**逐位相同** ⇒ ⇒ 否则占位臂接错优先级（trap 不在最上层）**无法被检出**
- [ ] **AC-23** ★ **开关面默认启用 + 生效值回显 + 参数转发完整** —— BP **MUST 默认启用**（否则「未生效被误判为无效」的假绿通道）⇒ runner MUST 在 stderr 回显**实际生效状态与条目数**⇒ 测量脚本 MUST 据此断言与请求一致；★ `build_cpu_with_elaborate`（`cpu_factory_chmem.h:451-458`）**MUST 转发全部 4 个参数**（现状已丢 3 个）⇒ ★ **MUST NOT** 用「显式登记为 known gap」当逃生口
- [ ] **AC-24** ★ **M 越界 fail-fast** —— `--btb-entries N` 的 N MUST 落在 {16,32,64,128,256}；越界 MUST **快速失败**（★ **MUST NOT** 静默截断 ⇒ 容量语义错却全绿）
- [ ] **AC-25** ★ **scope 机检须脚本化**（★ 非手工 grep）—— `tools/check_plugin_portability.sh` 新增 **Check 14**（★ 该脚本已是 PR **阻塞**门禁 ⇒ **无需构建** ⇒ CI 可承担）：scope = `ip/cpu/plugins/branch_predictor_chmem.h`（文件不存在则 skip）+ 新增文件名匹配 `*gshare*`/`*ghr*`/`*ras*` ⇒ 违反 ⇒ exit 1。★ **MUST NOT** 做成 tree-wide grep（TLM 侧 `branch_predictor.h` 合法含 GHR/RAS ⇒ 必误报）
- [ ] **AC-15** ★ **host 可见事件计数（5 个）**：① **已执行(retired)指令数**（★ **归前置2** —— 需 flush 信号；★ 前置1 **不**交付它，见前置1 的 owner 表）② **FLUSH 事件数** ③ **BP 命中次数**（N_correct）④ **误预测次数**（N_false）⑤ **taken 转移数** ⇒ 全部经 `cpu_verilator_sim --stats` 输出。★ **「取指数」≠「已执行数」**（前者含投机取指）⇒ MUST 用 **retired**（被 flush 抑制的不计）


- [ ] **AC-26** ★★ **错路指令维度**（★ v2.3 新增，来自 PoC B-2）—— 一切情形判定 MUST 排除「**被解析指令本身在错路上**」：`case_a = (!flushed) & pv & pt & (!actual_taken)`（`flushed` = branch plugin 的 `BRANCH_FLUSHED`）⇒ ★ `BRANCH_FLUSHED` 缺失 MUST **fail-fast**（抛异常）⇒ MUST NOT 静默降级为 false ⇒ ★ 单元锚 MUST 覆盖（PoC 的 R11：去掉 `!flushed` 门控 ⇒ RED）
- [x] **AC-27** ★★ **增量分档的量收益**（★ v2.3 新增，来自 **A5 决策**）—— 三个增量 MUST **可独立开关 + 独立测量**（★ 开关机制见 Task 7 Step 5b：工厂布尔 + CLI flag，先例 `enable_bp`）：增量 1 = `is_unconditional` + `jal` 建项；增量 2 = RAS 8-entry；增量 3 = GShare ⇒ ★ 每档 MUST 报出（① Dhrystone 的 ΔFLUSH / Δcycles / **净收益率** ② branchbench 4 档的校验和逐位相等（**零退化**，★ **非**增量生效的证据））⇒ ★★ **fail-closed（metis）**：★ **A6 未裁决 ⇒ MUST 同报两种分母口径的净收益率（分母 = `N_taken_total` 与分母 = `FLUSH(off)`），判定取**较小值** ⇒ ⇒ ★★ **量纲提醒**：`84.5%` 这个数字的出处是**「TLM–CH_MEM 差距闭合率」**（`101470/120066`），★ **不是**净收益率量纲 ⇒ ⇒ **A6 裁决 MUST 明确「阈值作用在哪个量纲上」** ⇒ ★ **A6 的兜底三要素**：① 触发阈值 = 本 change 进入归档流程；② 落点 = **ADR-088 §5.3.2** + 简报 §5；③ 时限 = **本 change 归档前** ⇒ ⇒ ★ 判据：净收益率（取小口径）≥ 84.5% ⇒ 绝对值门禁可恢复 ⇒ 剩余增量可延后；否则 MUST 把下一档拉进本期 ⇒ ★ **MUST NOT** 用「声明不在范围内」替代实测（见 AC-1 / AC-16 的 v2.3 修订） ★ **2026-10-26 增量 1 档已实测并落表**（DHrystone + branchbench 双负载）⇒ ★ **净收益率 78.3%** ⇒ ⇒ ★ **未达 84.5%** ⇒ ⇒ **GShare 转本期**（AC-16 强制）⇒ ★ 增量 2（RAS）仍待增量 3 实测后决定
- [ ] **AC-28** ★★ **增量扩展的保留声明 + 增量 1 的单元锚**（★ v2.3.1 新增，来自 metis）—— ★ **(a) 保留声明**：主体对 `branch_predictor_chmem.h` 的改造 MUST 是**增量扩展**（★ **MUST NOT** 重写）⇒ MUST 保留前置2 已验证的机制点清单：`bp_next` 情形 C 臂 / 门控式 flush / `!flushed` 情形门控 / payload fail-fast / 5 个 `ch_reg` 纠错记录 ⇒ ★ 前置2 的单元锚（`test_cpu_chmem_bp_poc.cpp` 8 case + `test_cpu_chmem_ibus_bp_pcmux.cpp` 12 case / 143 assertions，含 R8 链式投机 / R11 错路指令）在主体后 MUST **仍全绿** ⇒ ⇒ ★ **每个前置2 变异锚对应的机制点 MUST 有 1:1 保留声明**（★ 防「重写时静默丢语义」—— 这是 PoC B-1/B-2 的同型风险）
- [x] **AC-29** ★★ **增量 1（`is_unconditional` + `jal` 建项）的单元锚**（★ v2.3.1 新增）—— MUST 覆盖：① `jal` 站点**被建项**且 `is_unconditional=1`；② `jalr` **不建项**（AC-14）；③ ★★ **同槽覆盖时 `is_unconditional` 翻转**的变异对照（★ `jal` 与条件分支复用同槽 ⇒ 直接覆盖策略下该位可能由 1 翻 0 ⇒ 方向来源瞬间消失 ⇒ 预测恒 not-taken ⇒ **每次该分支 taken 都是情形 B** ⇒ **净收益静默恶化而测试全绿** ⇒ 冻结该位 ⇒ MUST RED）；④ ★ host `pack_entry` 与 ch 写路径产出的 BTB word MUST **逐位相等**（★ 现状 `:417` 缺 `is_unconditional` 位 ⇒ 双实现漂移） ★ **2026-10-26 已落地**：R11（`jal` 建项 + 恒 taken）/ R12（`jalr` 不建项）/ R13（同槽覆盖 ⇒ uncond 位翻转）/ R14（host `pack_entry` 与 ch 写路径语义对照）⇒ ★ **14 case / 205 assertions** 全绿


## ★★ per-file × per-symbol owner 表（★ 逐条抄进本 change —— 跨 change 引用会被漏读）

| 文件 / 符号 | 本 change | 其它 change | ★ 约定 |
|---|---|---|---|
| `tests/cpu/manual_elf/branchbench/branchbench.c` | ★ **前置1 owner** | 前置2 / 主体 ✗ | 主体 MUST NOT 触碰（硬门禁） |
| `soc/cpu/docs/dse/branchbench-baseline-matrix.csv` | ★ **前置1 建列**（`workload_sha` / `build_id` / `bp_mode=off`）+ 改按表头取列 | 前置2 / 主体 **仅追加行** | 建列归前置1；追加归后两者 |
| `tools/measure_branchbench.sh` | ★ **前置1 owner**（改名 / 按表头取列 / **捕获 stderr**） | 前置2 **仅追加** `--bp=on\|off` flag + 生效值断言；主体 ✗（硬门禁） | ★ **捕获 stderr 归前置1**（否则假绿通道开着）⇒ 前置2 只加 flag |
| `tools/verilator_runner/*` | ★ **前置1 建 `--stats` 框架 + `pc_changes`/PC 直方图**（★ **NOT** retired —— retired 需 flush ⇒ 归前置2） | 前置2 **补 FLUSH 计数**；主体 **补 BP flag + 回显** | ★ **框架归前置1**，增量归后两者，**MUST NOT** 另起一套 |
| `ip/cpu/plugins/branch_predictor_chmem.h` | ✗（禁 `ip/**`） | 前置2 **创建（最小版）**；主体 **增量扩展为完整版** （★ **v2.3 修正**：原写「重写（完整版）」与本表总则「MUST 增量扩展」**直接打架** ⇒ ★ 现取「增量扩展」并加 **AC-28** 的保留声明） | ★ 前置2 的 PoC 版 MUST NOT 被主体当作已完成；★ 主体 MUST **保留**前置2 已验证的机制清单（`bp_next` 情形 C 臂 / 门控 flush / `!flushed` 情形门控 / payload fail-fast / 5 个 `ch_reg` 纠错记录）⇒ 见 **AC-28** |
| `ip/cpu/plugins/ibus_chmem.h`（`update_pc` 的 mux + flush） | ✗ | 前置2 **首次改**（5 臂 mux + FLUSH 门控 + FLUSH 计数）；主体 **二次改**（补完整语义 + 过时注释 #8） | ★ **同函数两次改** ⇒ 前置2 的版本 MUST 可被主体**增量扩展**（MUST NOT 要求主体重写）⇒ ★ 主体 MUST 继承前置2 的**五臂 select 链**结构 |
| `ip/cpu/plugins/branch_chmem.h`（导出 jal/jalr 判别 payload） | ✗ | 前置2 **首次改**（`BRANCH_TAKEN_RAW` / `BRANCH_FLUSHED` / `IS_BRANCH` 三个 payload）；★ **主体二次改**：★ **增量 1 的「无条件」信号当前无来源** —— `branch_chmem.h:201-203` 已算出 `is_jal`/`is_jalr` 但**未导出为 payload** ⇒ 主体 MUST 新增导出（`IS_JAL` / `IS_UNCOND` 语义待定，见 `design.md` §3）⇒ ★ **MUST NOT** 让 BP 插件从 opcode 反推（opcode 在 branch 节点不可得） | ★ 本 change（主体）owner |
| `ip/cpu/cpu_factory_chmem.h`（注册 + 开关 + 参数转发） | ✗ | 前置2 **首次注册**（BP 开关）；主体 **补全**（4 参数转发修复 #9 + 完整注册） | ★ 同上 ⇒ 增量扩展 |
| `docs/architecture/adr/ADR-088-*.md` | ✗ | 前置2 写 **§5.3.2 的「机制结论」段**；主体 写 **「绝对值门禁结论」段** | ★ **主体 MUST NOT 覆盖前置2 的机制结论段**（否则「机制不成立」的结论会被主体的「达标」结论抹掉） |
| `docs/architecture/tlm-coverage-parity-ledger.md` | ★ **前置1 MUST NOT 触碰** | 前置2 MUST NOT 触碰；主体 owner | ★ 台账重判只归主体 |
| `ip/cpu/plugins/branch_predictor.{h,cpp}`（TLM 侧） | — | — | ★ **全程保留**（主体 AC-11） |

★ **本 change 的角色**：主体 ⇒ 本表列出的「主体」项**归本 change**；★ **前置2 已建的版本 MUST 增量扩展**（MUST NOT 重写）；★ **v2.3**：增量扩展 MUST 有**保留声明**（哪些前置2 的机制点被保留）⇒ 见 **AC-28**。
## ★ 兜底句纪律

任何「上报 architect / 待裁决」句 MUST 同句给出 **① 触发阈值 ② 落点文件 ③ 可判定时限**（「≤ N 个工作日」或「本 change 内闭环」）；MUST NOT 以「等待回复」结束本 change。

## ★ builder 开工须知（★ **逐条抄进本 change**，不跨 change 引用 —— 跨 change 引用会被漏读）

| # | 须知 | 归属 |
|---|------|------|
| 1 | ★ **v2.3.1 更正**：台账条目行的列数 MUST **对齐台账自身的格式声明**（★ 原文写「正好 7 列」，但台账 `:80` 的格式声明是 **5 列** ⇒ ★ **两个数字打架** ⇒ MUST 以**台账自身的声明**为准并在台账里写死）；★ **理由列不得含未转义的 `\|`** ⇒ ★ **本条已有可机检工具**：`tools/check_md_tables.py` （★ **v2.3.1：该工具 MUST 被接进门禁** —— 现状它是**孤儿工具**，不在任何 CI / gate 脚本 / AC-9 清单里 ⇒ 「写了工具不接线 = 假护栏」）| ★ 本 change |
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

### ★★★ planner 交接清单（★ **执行文件 `.rddf/plans/chmem-bp-btb-gshare.md` 的同步清单** —— metis blocker-1）

> ★ **为什么必须先同步**：builder 按 **plan 文件** 执行（TDD 5 步的 SSOT）⇒ ★ **plan 顶部只有 v2.1 覆盖声明**，**无 v2.3 / v2.3.1** ⇒ ⇒ 新增的 **Task 7 / AC-26~AC-29 / `bp_next` / 门控 flush / 情形 F / 错路指令维度** 在执行文件里**全部缺席** ⇒ ⇒ ★ **builder MUST NOT 只读本 change 的 `tasks.md` / `design.md` 就开工**。
> ★ **planner 不越界起草 `.rddf/plans/**`**（角色边界）⇒ ⇒ 下面是**逐条同步清单**，由 builder（或获授权者）落盘。

| # | plan 文件的位置 | MUST 改成 |
|---|---|---|
| P-1 | plan 顶部（`v2.1 覆盖声明` 之前或之后） | ★ **追加 v2.3 + v2.3.1 覆盖声明**（内容 = 本 change `design.md` §0 全段 + §0.7 全段）⇒ ★ **可直接整段拷贝** |
| P-2 | plan 的 owner 表 `branch_predictor_chmem.h` 行 | 「✓ **重写**（完整版）」⇒ ★ 改为「✓ **增量扩展为完整版**」+ **保留声明**指向 **AC-28**（★ 与本 change owner 表一致 ⇒ 两处 MUST 同源）|
| P-3 | plan 的 owner 表 | ★ **新增一行** `ip/cpu/plugins/branch_chmem.h`（导出 jal 判别 payload ⇒ 增量 1 的前置）|
| P-4 | plan 的 owner 表 `ibus_chmem.h` 行的「flush **双分量**」表述 | ★ 改为「flush **门控**（`hold_z \| (taken_buf & !spec_ok_buf) \| mispredict_A_buf`）」⇒ ★ 旧表述（「双分量」）是 **v2.3 §0.4 已作废**的写法 |
| P-5 | plan 的 **Task 2d**（branch LATE mux + FLUSH 判据） | ★ 按 `design.md` §0.1/§0.4 重写：`bp_next` 层 + **门控** + **情形 F** + **错路指令维度**（`!flushed` 门控）⇒ ★ 旧文本的「**四情形** select 链」**作废**（⇒ 现在是六情形 + 两个维度）|
| P-6 | plan 的 **Task 2c Step 3**（`pred_taken` 的来源） | ★ 补增量 1 的实现细节：`jal` 建项（写使能扩为 `taken & !flushed & (is_branch \| is_jal)`）+ ★ **ch 写路径 `packed_w` MUST 补写 `is_unconditional` 位**（PoC `:417` 现状缺 ⇒ host/ch 漂移）|
| P-7 | plan 的 **Task 2b**（Bimodal）之后 | ★ **追加增量 3（GShare 移植）**与**增量 2（RAS，条件性）**两个可选步骤 ⇒ ★ 顺序按 **成本序 1 → 3 → 2** |
| P-8 | plan 的 Tasks 清单 | ★ **追加 Task 7**（增量分档的量收益，Step 1~6，含 Step 5b 开关机制与 Step 5c 测量矩阵）|
| P-9 | plan 的「不可触碰」/ owner 纪律段 | ★ 补：**`branch_chmem.h` 的改动授权**（本 change 主体 owner）⇒ 否则撞三 change 边界硬门禁 |
| P-10 | （非 plan 文件）★ **前置2 PoC 的收口** | ✅ **已完成（2026-10-26 归档** `2026-10-26-chmem-bp-btb-gshare-poc`**）**：13/17 AC 勾选 + 归档注记；★ 其中 **AC-F2-8**（净收益率下界）= `68044/120065` = **56.67%** ⇒ ★ **4 项未闭合**（AC-F2-2 / 10 / 11 / 15）⇒ ★ 其中 **AC-F2-11 / AC-F2-15 归本 change 的 Task 2h**（★ MUST 在 Task 2h 的测量里报出无条件跳转占比）|

★ **同步完成后**：builder 开工前 MUST 再核一遍 —— plan 的 Task 编号与本 change 的 Task 1~7 **一一对应**（★ 防止出现「plan 有 Task 8 但 tasks.md 没有」的孤儿）。

## 实测留证

（实施后填写）

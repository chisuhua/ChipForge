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
> **权威规格**: `docs/architecture/bp-btb-gshare-design-brief.md` v2.1 §2.1 / §3.2 / §3.3 / §3.4 / §4.3 / §5.1 / §5.5
> ★ **两个前置 MUST 先完成**（`chmem-branch-microbench-fix` + `chmem-bp-btb-gshare-poc`）⇒ **前置未完成前本 change 的实现 MUST NOT 开工**。
> ★ **执行文件** `.rddf/plans/chmem-bp-btb-gshare.md` 顶部有 **v2.1 覆盖声明**（逐条作废旧 Goal / Task 3 / Task 4 / Task 5.5 / AC-1 / AC-8 / Task 6 Step 4 / Task 7 Step 3-4 / 实施期修正 #1）⇒ **以覆盖声明为准**。

## Implementation

- [ ] **Task 1** BP 插件骨架 + BTB 表（256×64b：全 32b PC tag + 32b target + valid 偷 target[1:0]）+ 逻辑条目数 M 的运行期 mask
- [ ] **Task 2** Bimodal 2-bit PHT（256×2b）+ 更新使能（★ **分支已解析，任何方向**）+ 更新地址（★ 分支解析处 `KT::PC`）
- [ ] **Task 3** fetch NORMAL 预测（BTB **0 拍**查表）+ payload 输出（`pred_target`/`pred_taken`/`pred_valid`）
- [ ] **Task 4** branch LATE 接入（★ **五级嵌套**）+ 正确预测**抑制自身重定向** + 误预测纠错（`pc_lag+4`）+ flush 语义改写（双分量）
- [ ] **Task 5** BP 开关面（工厂参数 + CLI）+ 生效值回显 + 参数转发完整
- [ ] **Task 6** 台账 14 条逐条重判 + 族级/§7/计数同步 + ADR-088 §5.3.1 根因修订 + 范围机检（grep 门禁）

## Acceptance

- [ ] **AC-1** `ip/cpu/plugins/branch_predictor_chmem.h` 存在且实现 **BTB（256×64b）+ Bimodal 2-bit PHT（256×2b）**；★ **GShare / RAS 明确不在范围内**
- [ ] **AC-2** `cpu_factory_chmem.h` **已注册** BP plugin，canonical ordering 断言 PASS；BP 在 **fetch 侧**的注册位已定义（ADR-048）
- [ ] **AC-3** ★ **台账逐条表**（架构简报 §5.1，14 条）：每条有明确态 + CH_MEM 锚点 + 显式理由 + 是否删除该行；★ **CH_MEM 侧 case 名与 TLM-only case 名零交集**；★ **台账 §7 的 BP 裁决行同步**
- [ ] **AC-4** **BTB miss ⇒ 不预测**（不跳转、不 flush）
- [ ] **AC-5** 预测方向/目标为**活节点**（`impl()->is_const() == false`）⇒ 禁 `static_cast<bool>`
- [ ] **AC-6** ★ **预测错误 ⇒ 只在预测错误时抑制错路指令**；flush 双分量语义正确（`br_taken_buf` 分量替换为 `mispredict_buf`，**`hold_z` 保留**）
- [ ] **AC-6b** ★★ **正确预测 ⇒ 该分支自身的 redirect 被抑制**（目标指令**只被取一次**）⇒ 验证：host 侧 PC 轨迹**按迭代切分后，每段内任一静态 PC 的取指次数 ≤ 1**（★ **不是**「全程无重复 PC」—— 循环回边使同一 PC 合法重复上千次；★ **不是**「无连续重复」—— 自指分支在正确预测下产生合法的连续重复）
- [ ] **AC-7** `[chmem]` 47 ELF + `[verilator]` 48 ELF **0 退化**；★ **双跑**（BP 关一遍 + BP 开一遍，两套全绿）；★ **BP on/off 的 4 档校验和逐位相等**
- [ ] **AC-8a** 4 档在 BP 开/关下**校验和逐位相等**（功能，无阈值）
- [ ] **AC-8b-1** ★ **结构等式（含暖机修正）**：`always_taken` 档上 `Δcycles(bp=off − bp=on)` **逐位等于**「该档在 BP 关态下的 taken 转移数 × 1 拍 **− W**」，其中 **W = 暖机拍数**（PHT 需 2 次更新才达阈值 2 ⇒ 首个静态分支的前 2 次预测为 not-taken ⇒ 无收益）⇒ ★ **W MUST 实测并在报告中断言 `W ≤ 4`**。★ **「零误预测档」的准确表述 = 「零*代价*误预测」**（暖机期的 not-taken 预测不产生额外代价）。★ **MUST NOT** 写成「逐位等于 N×1」（正确实现会 FAIL）
- [ ] **AC-8b-2** ★ **净收益模型**：其余档 `Δcycles(off − on) = N_correct×1 − N_false×1`（N 由 AC-15 计数器导出）⇒ ★ **符号为 `off − on`**；★ **误预测代价 = 1 拍**（architect 2026-10-24 逐拍重推：错预测时**多取一拍**错误 target，纠错回到 fall-through（该拍本来就要取）⇒ 净多 1 拍）⇒ **MUST NOT** 忽略 `N_false` 项⇒ **MUST NOT** 忽略 `N_false` 项
- [ ] **AC-8b-3** **有界替代**（不引入计数器时）：只断言 `0 ≤ Δcycles(off − on) ≤ N_taken×1`，精确净收益降级为报告项
- [ ] **AC-8c** ★ **反证（限定 `always_taken` 档）**：该档 `Δcycles(off − on) == 0` ⇒ **BP 未生效**。⚠️ 该判据对 `always_not_taken` 档**不成立**（该档预测不跳转与无 BP 等价 ⇒ Δ 合法地 ≈ 0）
- [ ] **AC-8d** ★ **信号强度前置条件**：若前置1 报告的跨档 taken 分支数差 **< 1 拍/iter** ⇒ **跨档判据不可用** ⇒ 改用「`always_taken` 单档绝对 Δ + 解析模型」。★ **判定权归前置1**（独立 change、独立执行者）⇒ 结论写入 CSV notes + 前置1 归档产物 ⇒ **不留给本 change**（本 change 对「判据不可用」有既得利益）
- [ ] **AC-8e** ★ **跨档判据用差值形式**：`Δ(mode) − Δ(always_not_taken)` ⇒ 因 4 档都含**同一条循环回边**（每 iter 恒 1 次跳转 ⇒ 各档统一收益）⇒ 解析模型须扣掉回边
- [ ] **AC-9** ★ **具名门禁清单**：`verify_adr` · `verify_plugin_decision` · `check_plugin_portability` · `doc_link_check` · `roadmap-r9-check` · `check_xlen_hygiene` · `check_tlm_freeze` · `check_coverage_parity_ledger` + `bash tools/run_chipforge_tests.sh --all`（失败集合恰 1 个 pre-existing：`tests/cpu/integration/test_7stage_riscv.cpp:124`）★ 注明「**仅本地成立 ⇒ ADR-088 §5.2 判据 4 仍未满足**（CI 不构建项目）」
- [ ] **AC-10** 台账重判完毕 + **§2 族级判定行同步** + ★ **计数同步（注意二义）**：台账 `:16` 的 **463 = TLM 独占 case 数**（**本 change MUST 保持 463 不变**）· 台账 `:80` 的 **463 = 条目级台账条目数**（= 463 − 被删行数；★ 本 change 的 14 条**全部保留行**（不删）⇒ 仍为 463）⇒ ★ **MUST 写明本 change 采用哪种口径** + ★ **`check_coverage_parity_ledger.sh` exit 0**（CI warn 级 ⇒ 无强制力 ⇒ 须本地跑 + 输出贴 PR）
- [ ] **AC-11** **TLM 侧 `branch_predictor.{h,cpp}` 全程保留**（删除属 B3，需双前置）
- [ ] **AC-12** ★ **「不做什么」机检化**（非散文）：★ **本 change 的 `ip/` + `tests/` 变更集内** **MUST NOT** 出现 `GShare` / `GHR` / `RAS` 标识、**MUST NOT** 出现 `ch_reg<ch_uint<13>>`，且 **MUST NOT** 新增文件名匹配 `*gshare*` / `*ghr*` / `*ras*` 的文件 ⇒ grep 门禁（★ 扩域到变更集，否则换个文件名即绕过）
- [ ] **AC-13** ★ **BP 必须可学**：★ **同向连续两次解析后**，该分支**下一次**被取时预测命中（PHT 更新使能 = 分支已解析，**任何方向**；更新地址 = 分支解析处 `KT::PC`，与预测 `pc_reg` **同指一条分支**）。★ **MUST NOT** 写成「第二次被取即命中」—— PHT 饱和阈值是 2 ⇒ 首次解析后 counter=1 ⇒ 仍预测 not-taken
- [ ] **AC-14** ★ **`jalr` 不建 BTB 项** ⇒ 走顺序取指；无条件跳转不占 PHT 表项
- [ ] **AC-16** ★★ **净收益率裁决点**（★ metis P0-1：**从「归档清单」提到此处** —— 范围裁决 MUST 在**实测数据到手的那一刻**触发，不能等到归档口才发现前提被推翻）：实测 **净收益率** := `Δcycles(bp=off − bp=on) / N_taken_total`（★ **不是命中率**，是净收益率）；**MUST ≥ 84.5%**；★ 报告 MUST 同时给出 **N_correct / N_false**（★ **MUST NOT** 用 raw hit rate 对照 84.5%）⇒ 若 `N_false / N_taken_total > 5.2%` ⇒ **MUST NOT** 以「GShare 延后」结案
- [ ] **AC-17** ★ **无条件跳转的可预测性** —— BTB 表项 MUST 带 `is_unconditional` 位（`jal` 不占 PHT 表项 ⇒ 若无此位则 `jal` 的方向**无来源** ⇒ 永不被预测 ⇒ call 气泡不可恢复）；`pred_taken = pred_valid && (is_unconditional || pht_counter ≥ 2)`；★ `jalr` **MUST NOT** 建 BTB 项（间接跳转目标每次可能不同 ⇒ 静态缓存必错 ⇒ 走顺序取指）
- [ ] **AC-18** ★ **陈旧目标自愈** —— 替换策略为**直接覆盖** ⇒ 目标变化的分支再次 taken 时**同槽重写** ⇒ **天然自愈**，MUST NOT 另设误预测计数器
- [ ] **AC-15** ★ **host 可见事件计数（5 个）**：① **已执行(retired)指令数** ② **FLUSH 事件数** ③ **BP 命中次数**（N_correct）④ **误预测次数**（N_false）⑤ **taken 转移数**（BP 关态测得）⇒ 全部经 `cpu_verilator_sim --stats` 输出。★ **「取指数」≠「已执行数」** —— 前者 ≈ cycles ⇒ 用它证明「工作量相等」是**循环论证** ⇒ MUST 用 **retired**（被 flush 抑制的不计）

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

## ★ 归档时必须完成（★ 归入 `## Acceptance` 下方 ⇒ rdd-verifier 才会核）

- [ ] **ADR-088 §5.3.1 根因修订**（★ **必须** —— 现有表述已被 E-1/E-2 证伪）⇒ 走 `docs/architecture/adr.md` 注册表 + `verify_adr.sh`
- [ ] ★ **B3 结论的正确表述**：「① 已达成；**② 未达成（CoreMark harness 属交付点 B）⇒ B3 的 BP 阻塞未解除**」+ 登记「CI 判据 4 未满足（CI 不构建项目）」
- [ ] ★ **绝对值门禁结论**：★ **判定已在 AC-16 完成**（实测点，非归档点）⇒ 本项**只做落地**：依 AC-16 的结论在 **ADR-088 §5.3.2** 落结论（恢复 / **不恢复**）⇒ 不恢复时按 **P-3 临时口径**（cycle 比率 ≤ 1.15 + notes 标注）结案

## 实测留证

（实施后填写）

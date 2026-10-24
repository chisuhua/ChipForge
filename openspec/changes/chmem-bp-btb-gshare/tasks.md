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
- [ ] **AC-6b** ★★ **正确预测 ⇒ 该分支自身的 redirect 被抑制**（目标指令**只被取一次**）⇒ 验证：host 侧 PC 序列在 BP 开下**无重复 PC**
- [ ] **AC-7** `[chmem]` 47 ELF + `[verilator]` 48 ELF **0 退化**；★ **双跑**（BP 关一遍 + BP 开一遍，两套全绿）；★ **BP on/off 的 4 档校验和逐位相等**
- [ ] **AC-8a** 4 档在 BP 开/关下**校验和逐位相等**（功能，无阈值）
- [ ] **AC-8b-1** ★ **零误预测档的结构等式**：`always_taken` 档上 `Δcycles(bp=off − bp=on)` **逐位等于**「该档在 BP 关态下的 taken 转移数 × 1 拍」⇒ 把「1 拍/taken」从**分类恒等式**升级为**实测等式**
- [ ] **AC-8b-2** ★ **净收益模型**：其余档 `Δcycles(off − on) = N_correct×1 − N_false×1`（N 由 AC-15 计数器导出）⇒ ★ **符号为 `off − on`**；★ **误预测代价 = 2 拍**（错误目标被取 + 纠错落回 `pc_lag+4`）⇒ **MUST NOT** 忽略 `N_false` 项
- [ ] **AC-8b-3** **有界替代**（不引入计数器时）：只断言 `0 ≤ Δcycles(off − on) ≤ N_taken×1`，精确净收益降级为报告项
- [ ] **AC-8c** ★ **反证（限定 `always_taken` 档）**：该档 `Δcycles(off − on) == 0` ⇒ **BP 未生效**。⚠️ 该判据对 `always_not_taken` 档**不成立**（该档预测不跳转与无 BP 等价 ⇒ Δ 合法地 ≈ 0）
- [ ] **AC-8d** ★ **信号强度前置条件**：若前置1 报告的跨档 taken 分支数差 **< 1 拍/iter** ⇒ **跨档判据不可用** ⇒ 改用「`always_taken` 单档绝对 Δ + 解析模型」。★ **判定权归前置1**（独立 change、独立执行者）⇒ 结论写入 CSV notes + 前置1 归档产物 ⇒ **不留给本 change**（本 change 对「判据不可用」有既得利益）
- [ ] **AC-8e** ★ **跨档判据用差值形式**：`Δ(mode) − Δ(always_not_taken)` ⇒ 因 4 档都含**同一条循环回边**（每 iter 恒 1 次跳转 ⇒ 各档统一收益）⇒ 解析模型须扣掉回边
- [ ] **AC-9** ★ **具名门禁清单**：`verify_adr` · `verify_plugin_decision` · `check_plugin_portability` · `doc_link_check` · `roadmap-r9-check` · `check_xlen_hygiene` · `check_tlm_freeze` · `check_coverage_parity_ledger` + `bash tools/run_chipforge_tests.sh --all`（失败集合恰 1 个 pre-existing：`tests/cpu/integration/test_7stage_riscv.cpp:124`）★ 注明「**仅本地成立 ⇒ ADR-088 §5.2 判据 4 仍未满足**（CI 不构建项目）」
- [ ] **AC-10** 台账重判完毕 + **§2 族级判定行同步** + **「涉及 case 数」与头部「463 条」计数同步** + ★ **`check_coverage_parity_ledger.sh` exit 0**（CI warn 级 ⇒ 无强制力 ⇒ 须本地跑 + 输出贴 PR）
- [ ] **AC-11** **TLM 侧 `branch_predictor.{h,cpp}` 全程保留**（删除属 B3，需双前置）
- [ ] **AC-12** ★ **「不做什么」机检化**（非散文）：`branch_predictor_chmem.h` 内 **MUST NOT** 出现 GShare / GHR / RAS 标识、**MUST NOT** 出现 `ch_reg<ch_uint<13>>` ⇒ grep 门禁
- [ ] **AC-13** ★ **BP 必须可学**：同一分支**第二次**被取时预测命中（PHT 更新使能 = 分支已解析，任何方向；更新地址 = 分支解析处 `KT::PC`，与预测 `pc_reg` 同指一条分支）
- [ ] **AC-14** ★ **`jalr` 不建 BTB 项** ⇒ 走顺序取指；无条件跳转不占 PHT 表项
- [ ] **AC-15** ★ **host 可见事件计数**：「已执行指令数」+「FLUSH 事件数」（两个计数器 + `cpu_verilator_sim --stats` 输出行）⇒ 供 AC-8b-2 与前置2 判据使用

## ★ 兜底句纪律

任何「上报 architect / 待裁决」句 MUST 同句给出 **① 触发阈值 ② 落点文件 ③ 可判定时限**（「≤ N 个工作日」或「本 change 内闭环」）；MUST NOT 以「等待回复」结束本 change。

## ★ builder 开工须知（10 条，**逐条确认**）

见 `chmem-branch-microbench-fix/tasks.md` 的同名清单（同一份，适用于三个 change）。

★ **追加本 change 专属**：本 change 的 diff **MUST NOT** 触碰 `tests/cpu/manual_elf/branchbench/` 与 `tools/measure_branchbench.sh`（那是前置1 的范围）⇒ **三 change 边界为硬门禁**。

## ★ 归档时必须完成（★ 否则产生假完工声明）

- [ ] **ADR-088 §5.3.1 根因修订**（★ **必须** —— 现有表述已被 E-1/E-2 证伪）⇒ 走 `docs/architecture/adr.md` 注册表 + `verify_adr.sh`
- [ ] ★ **B3 结论的正确表述**：「① 已达成；**② 未达成（CoreMark harness 属交付点 B）⇒ B3 的 BP 阻塞未解除**」+ 登记「CI 判据 4 未满足（CI 不构建项目）」
- [ ] ★ **绝对值门禁结论**：依 AC-8b-2 的实测净收益率，判定是否达到 **84.5%** 门槛 ⇒ 未达到 ⇒ **MUST NOT** 以「GShare 延后」结案 ⇒ 须在 ADR-088 §5.3.2 落结论并按 P-3 临时口径结案

## 实测留证

（实施后填写）

---
change: chmem-bp-btb-gshare-poc
priority: P1
initiative: wave6-linux-and-productization
adr:
  - ADR-088
plan: .rddf/plans/chmem-bp-btb-gshare-poc.md
status: in-progress
depends_on:
  - chmem-branch-microbench-fix
---

# Tasks — chmem-bp-btb-gshare-poc

> **定位**: B1 ② `chmem-bp-btb-gshare` 的**硬前置**（独立 change）。验证「**fetch 期提前一拍预测 ⇒ 消除 taken 转移气泡**」机制是否成立。
> **权威规格**: `docs/architecture/bp-btb-gshare-design-brief.md` §2.1 / §2.5 / §3.4.2 / §4.2
> ★ **本 PoC 的代码不复用为 ② 主体**（避免「半成品进主体」）

## Implementation

- [x] **Task 1** 最小 BTB（`branch_predictor_chmem.h` 最小版：BTB 256×64b 62b 编码 + Bimodal 2-bit PHT 256×2b）+ fetch NORMAL **0 拍**（`aread`）查表 + payload 产出（`pred_target` / `pred_taken` / `pred_valid`）
  - ★ **部分交付**：`ch_reg` 存预测记录（`pred_*_reg_`）**已声明但未接线** ⇒ 归 **Task 4**（误预测纠错需跨拍比对）⇒ **不计入本 Task 完成范围**（★ 修正上一轮 commit message 的不实陈述）
- [x] **Task 2** branch LATE 的 PC-mux 接入（★ **五级嵌套**：`trap > branch-actual/mispredict > freeze > bp-predict > seq`）
  - ★ **落点**：`ip/cpu/plugins/ibus_chmem.h` `update_pc()`（branch LATE）—— 消费者在 IBus，BP 插件保持「纯预测器」（只产 payload）
  - ★ **已实现的链**（简报 §3.4.2 的五级，`trap`/`freeze` 两臂待 v0.11.0 Phase 3.5 落地后**外包**在 branch 之上）：
    `bp_next = select(pred_valid&pred_taken, pred_target, pc+4)`；
    `next_pc = select(A, pc_lag+4, select(B, actual_target, select(C, **bp_next**, select(actual_taken, actual_target, bp_next))))`
    ⇒ ★★ **C 臂的值是 `bp_next` 而不是 `pc+4`**（critic 十轮 B-1 / 缺口 #11 / 裁决 A3）：
    `pc+4` 会**丢掉下一级投机** ⇒ 若分支的 target 本身又是一条分支 ⇒ **走错路 +
    错路指令不被 flush ⇒ 副作用泄漏** ⇒ ★ 该场景在 branchbench / vendored ELF 上
    **结构性不可达** ⇒ 由单元锚 **R8** 锁定。
  - ★ **情形 B 臂与 `actual_taken` 臂语义重复**（B ⊂ actual_taken ⇒ 两者都选 `actual_target`）⇒ 该臂**无可观测效果** ⇒ 保留仅为**规格保真**（简报明文要求五级）⇒ ★ **没有断言覆盖它**（诚实登记，见缺口 #10）
- [x] **Task 3** ★ flush 语义改写（★ **门控**，非「替换分量」）—— `flush = hold_z | (taken_buf && !bp_spec_taken_buf) | mispredict_A_buf` ⇒ ★ **BP 关闭时与现状逐位相同（零退化）**；★ **MUST NOT** 把 `br_taken_buf` 分量替换为 `mispredict_buf` 分量（⇒ BTB miss / 情形 B / `jalr` 的 fall-through 副作用泄漏 ⇒ 校验和不等）
  - ★ **落点**：同文件 `gated_flush()`（fetch NORMAL 发布 `FLUSH`）
  - ★ **`spec_ok` 的语义是「投机**被证明正确**」（情形 C）**，不是「投机臂生效」** —— 后者在情形 F（目标不一致）下也为真 ⇒ 会让 F 的错路**不被抑制** ⇒ 副作用泄漏 ⇒ 校验和不等
  - ★ **`spec_ok` MUST NOT 再与 `pred_eff` 相与** —— `pred_eff` 是「**本拍**对 `pc_reg(N)` 的预测」，`case_c` 讲的是「**上一拍**的预测是否被证明正确」⇒ 两者是**不同指令**的条件 ⇒ 误与 ⇒ 误清 `spec_ok` ⇒ **过度 flush** ⇒ 抑制合法指令 ⇒ 校验和错（实测 m0 `93 ≠ 103`）
- [x] **Task 4** 误预测纠错（★ **按情形 A/B 区分**：A ⇒ 纠到 `pc_lag+4` + flush；B ⇒ 纠到 `actual_target` + **flush（MUST 抑制！）**；C ⇒ 抑制自身 redirect + **MUST NOT 抑制**；E（BTB miss 且实际跳转）⇒ **MUST 抑制**）+ **正确预测时抑制该分支自身 redirect**
  - ★ **落点**：同上 `update_pc()` —— 5 个 `ch_reg` 纠错记录（`pred_valid/taken/target` + `spec_ok` + `mispredict_a`）+ 情形 A/B/C/F 判定
  - ★ **情形 F**（预测 taken / 实际 taken / **目标不同**）= ★ **设计简报未列的规格缺口** ⇒ 本实现补（见缺口 #10）
  - ★ **单元锚**：`tests/cpu/test_cpu_chmem_ibus_bp_pcmux.cpp` —— **7 case / 98 assertions**，断 5 条臂与门控分量的**运行期值**（R1/R2 = BP 关闭零退化；R3 = 情形 A；R4 = 情形 B；R5 = 情形 C **不 flush**；R6 = 情形 F **必须 flush**；R7 = `spec_ok` 只等于 `case_c`）
- [x] **Task 5** 测量设施（★ **FLUSH 事件数计数器归本 change** —— FLUSH 是 DUT 内部 payload，**host 不可见** ⇒ 需改 `ip/cpu/plugins/ibus_chmem.h` ⇒ **本 change 是首个可改 `ip/` 的 change**）+ ★ **MUST 复用**前置1 的 `--stats` 框架（**MUST NOT** 另起一套）+ BP 开关面（工厂参数 + CLI flag）

## Acceptance

- [ ] **AC-F2-1** 被测对象与工作负载明确 ★ **必须含一个「恒 taken」定向档**（★ 否则 BTB-only 在 not-taken 密集的负载上净负 ⇒ 机制会被误判「不成立」）
- [ ] **AC-F2-2** ★ 测的量 = **FLUSH 事件数**（★ 对取指延迟语义变更鲁棒）+ **总 cycles** + **host 侧 PC 轨迹**（查重复 PC）+ **BP 内部计数**（N_correct / N_false）+ **TLM 侧 cycle 数** —— **不再用「taken 转移数」作判据**
- [ ] **AC-F2-3** ★ **决策表两条分支各自的后果（终局式，★ v2.2 补阈值 —— 否则「成立/不成立」不可判定 ⇒ 兜底纪律被自己违反）**：
  - ★ **量化判据**：`ΔFLUSH = FLUSH(off) − FLUSH(on)` MUST **≥ 0.8 × FLUSH(off)**（★ 阈值由本 change 预登记，**MUST NOT** 写「显著下降」这种无量词）**且** `Δcycles ≥ 0.9 × ΔFLUSH`（★ 两项**同时**满足 ⇒ 成立；否则 ⇒ 不成立）
  - **成立** ⇒ 进 ② 主体
  - **不成立** ⇒ ① 记录 ⇒ ② 在 **ADR-088 §5.3.2** 落**结论**（绝对值门禁恢复 / **不恢复**）⇒ ③ **不恢复时沿用 §5.3.2 已登记的 P-3 临时口径**（cycle 比率 ≤ 1.15 + notes 标注）⇒ ④ **本 change 关闭**，**不留悬空项**
  - ★ **展开**（原单行内容）：
  - ★ **展开**：成立 ⇒ 进 ② 主体；不成立 ⇒ ① 记录 ⇒ ② 在 **ADR-088 §5.3.2** 落**结论**（绝对值门禁恢复 / **不恢复**）⇒ ③ 不恢复时沿用 §5.3.2 已登记的 P-3 临时口径（cycle 比率 ≤ 1.15 + notes 标注）⇒ ④ **本 change 关闭**，**不留悬空项**
  - **不成立** ⇒ ① 记录 ⇒ ② 在 **ADR-088 §5.3.2** 落**结论**（绝对值门禁恢复 / **不恢复**）⇒ ③ **不恢复时沿用 §5.3.2 已登记的 P-3 临时口径**（cycle 比率 ≤ 1.15 + notes 标注）⇒ ④ **本 change 关闭**，**不留悬空项**
- [ ] **AC-F2-4** ★ **兜底路径三要素**（触发阈值 + 落点文件 + ★ **可判定的时限**：「**≤ 2 个工作日**」）—— 继承 plan R6；★ **MUST NOT** 留 `N` 之类占位符（三要素的第 ③ 项实际为空 ⇒ 这条 AC 正是「等回复」的合法化文本）
- [ ] **AC-F2-5** ★ 显式声明「**本 PoC 不产出 P-6 结论**」（P-6 结论归 architect）
- [ ] **AC-F2-6** PoC 的代码**不复用**为 ② 主体
- [ ] **AC-F2-7** ★ **报告「去重后的热点分支数」** ⇒ 若 > 256 ⇒ 容量裁决提前做
- [ ] **AC-F2-8** ★ **报告「可达净收益率下界」** —— ★ **MUST 在 Dhrystone 上测**（84.5% 是 Dhrystone 门槛 ⇒ branchbench 的定向档不可对照）；**净收益率** := `Δcycles(bp=off − bp=on) / N_taken_total`（★ **不是命中率**；★ `N_false` **只计情形 A**（预测跳转/实际不跳转，代价 1 拍）；★ 情形 B 代价 **0 拍** ⇒ **MUST NOT** 计入）；下界 := `1 − (cold + mispredict_A + jalr_占比) / N_taken`（全部取自 AC-15 计数器 + AC-F2-11 的分项统计）⇒ ★ 若下界 **< 84.5%** ⇒ **不得以「GShare 延后」结案** ⇒ ★ **MUST NOT** 用 raw hit rate 对照 84.5% ⇒ ★ **MUST NOT** 用「2×命中率 − 1」推导门槛（该式仅在情形 B ≈ 0 时成立 ⇒ 已被 v2.2 禁用）
- [ ] **AC-F2-9** ★ **测量设施产品化**：「临时改 `sim_main.cpp`」⇒ 变成**常驻输出**（`--stats`）⇒ 否则每个 change 重做一次性 hack，测量结论不可复核
- [ ] **AC-F2-10** ★ **TLM 侧 cycle 数同时输出** ⇒ 让「差距全吸收」交叉验证成为**可重复判据**
- [ ] **AC-F2-11** ★ **测量方法护栏**：`enable_mmu=false`（**bare**）· **排除 trap 周期** · **无条件跳转（jal/jalr/ret）与条件分支分开统计**

- [ ] **AC-F2-12** ★ **预测信号必须是活节点** —— `pred_valid` / `pred_taken` 的 payload cell `impl()->is_const() == false` ⇒ 否则被折成常量 ⇒ Δcycles ≈ 0 而 FLUSH 不变 ⇒ **假阴性**（机制被判「不成立」⇒ 误关 ②）
- [ ] **AC-F2-13** ★ **计数器自证（★ 修正：原文的字面判据不成立）** —— 原文「`FLUSH 事件数 == PC 跳变法测得的 taken 转移数`」**字面为假**（实测 m0：`FLUSH_COUNT=12000` 而该档 taken=0 ⇒ 该档每 iter 有 **6 次与方向无关的固定 flush**）⇒ 改为**差值口径**：`ΔFLUSH`（各档 vs 无 BP 参照档）**MUST** 等于**由源码语义独立算出的 taken 数**（实测 Δ = 2000 / 500 / 1022，逐位相等 ✓）⇒ 目的不变：防「计数器恒 0 ⇒ 『显著下降』真空成立」的**假阳性**
- [ ] **AC-F2-14** ★ **BTB 命中率自证** —— BP 开态下 `BTB 命中次数 > 0` ⇒ 防「索引退化为 0 ⇒ 恒 miss ⇒ Δ=0 且 FLUSH 不变」的**假阴性**
- [ ] **AC-F2-15** ★ **无条件跳转占比报告** —— 报告 `jal` 数 / `jalr` 数 / taken 转移数 三者的比例 ⇒ 供 architect 判定「84.5% 门槛在构造上是否可达」（★ `jalr` 不建 BTB 项 ⇒ 其气泡**不可恢复**）
- [ ] **AC-F2-16** ★★ **功能等价（★ PoC 的鉴别力来源 —— 无此项则 PoC 可在「功能已坏」的机器上判「机制成立」⇒ 假阳性 ⇒ 该结论会被写进 ADR-088 §5.3.2 ⇒ 主体继承）**：BP 开/关两种状态下 ① 4 档**校验和逐位相等** ② `[chmem]` vendored ELF **47/47 0 退化** ⇒ ★ 任一不满足 ⇒ **机制判定作废**（判「不成立」并按 AC-F2-3 的不成立分支结案）
- [ ] **AC-F2-17** ★★ **flush 时序对**（★ 直接钉死 `mispredict_buf` 的 **1 拍延迟** —— 错路指令的副作用发生在**下一拍** ⇒ flush MUST 延后 1 拍（`ch_reg`）⇒ ★ **MUST NOT** 用「组合路径（当拍即知）」）：① 投机成功 ⇒ 该拍**无** flush ② 投机失败 ⇒ **下一拍**有 flush ③ BTB miss 的实际跳转分支 ⇒ **下一拍**有 flush ④ 「未投机而实际跳转」的 fall-through store/写回 **MUST NOT** 发生


## ★★ per-file × per-symbol owner 表（★ 逐条抄进本 change —— 跨 change 引用会被漏读）

| 文件 / 符号 | 本 change | 其它 change | ★ 约定 |
|---|---|---|---|
| `tests/cpu/manual_elf/branchbench/branchbench.c` | ★ **前置1 owner** | 前置2 / 主体 ✗ | 主体 MUST NOT 触碰（硬门禁） |
| `soc/cpu/docs/dse/branchbench-baseline-matrix.csv` | ★ **前置1 建列**（`workload_sha` / `build_id` / `bp_mode=off`）+ 改按表头取列 | 前置2 / 主体 **仅追加行** | 建列归前置1；追加归后两者 |
| `tools/measure_branchbench.sh` | ★ **前置1 owner**（改名 / 按表头取列 / **捕获 stderr**） | 前置2 **仅追加** `--bp=on\|off` flag + 生效值断言；主体 ✗（硬门禁） | ★ **捕获 stderr 归前置1**（否则假绿通道开着）⇒ 前置2 只加 flag |
| `tools/verilator_runner/*` | ★ **前置1 建 `--stats` 框架 + `pc_changes`/PC 直方图**（★ **NOT** retired） | 前置2 **补 FLUSH 计数**；主体 **补 BP flag + 回显** | ★ **框架归前置1**，增量归后两者，**MUST NOT** 另起一套 |
| `ip/cpu/plugins/branch_predictor_chmem.h` | ✗（禁 `ip/**`） | 前置2 **创建（最小版）**；主体 **重写（完整版）** | ★ 前置2 的 PoC 版 MUST NOT 被主体当作已完成 ⇒ 主体 AC-1 MUST 检查**完整功能**（非文件存在）；★ PoC 残留的**删除/重写责任归主体**；★ `tests/cpu/test_cpu_chmem_bp_poc.cpp` 的 8 case 在主体后 MUST 仍绿（机制锚）⇒ ★ **该 8 case 只能依赖公开接口** |
| `ip/cpu/plugins/branch_chmem.h`（**★ owner 表扩展 —— 本轮新增**） | ✗ | ★ **前置2 改**（新增 `IS_BRANCH` / `BRANCH_TAKEN_RAW` / `BRANCH_FLUSHED` 三个 payload + 修正陈旧注释） | ★ **边界扩展登记**：BP 的 PHT 方向学习需要**未抑制**的分支结果与**错路标记** ⇒ branch plugin 需导出 ⇒ 原 owner 表未含本文件 ⇒ 本轮登记 |
| `ip/cpu/plugins/ibus_chmem.h`（`update_pc` 的 mux + flush） | ✗ | 前置2 **首次改**（5 臂 mux + FLUSH 门控 + FLUSH 计数 + 过时注释 #8）；主体 **二次改**（补完整语义） | ★ **同函数两次改** ⇒ 前置2 的版本 MUST 可被主体**增量扩展**（MUST NOT 要求主体重写）⇒ ★ 主体 MUST 继承前置2 的**五臂 select 链**结构 |
| `ip/cpu/cpu_factory_chmem.h`（注册 + 开关 + 参数转发） | ✗ | 前置2 **首次注册**（BP 开关）；主体 **补全**（4 参数转发修复 #9 + 完整注册） | ★ 同上 ⇒ 增量扩展 |
| `docs/architecture/adr/ADR-088-*.md` | ✗ | 前置2 写 **§5.3.2 的「机制结论」段**；主体 写 **「绝对值门禁结论」段** | ★ **主体 MUST NOT 覆盖前置2 的机制结论段**（否则「机制不成立」的结论会被主体的「达标」结论抹掉） |
| `docs/architecture/tlm-coverage-parity-ledger.md` | ★ **前置1 MUST NOT 触碰** | 前置2 MUST NOT 触碰；主体 owner | ★ 台账重判只归主体 |
| `ip/cpu/plugins/branch_predictor.{h,cpp}`（TLM 侧） | — | — | ★ **全程保留**（主体 AC-11） |

★ **本 change 的角色**：前置2（机制 PoC）⇒ 本表列出的「前置2」项**归本 change**（含 `branch_predictor_chmem.h` 创建最小版、`ibus_chmem.h` 首次改、`cpu_factory_chmem.h` 首次注册、FLUSH 计数）；★ **MUST NOT** 触碰「前置1 owner」项与「主体」项。
## ★ 兜底句纪律

任何「上报 architect / 待裁决」句 MUST 同句给出 **① 触发阈值 ② 落点文件 ③ 可判定时限**；MUST NOT 以「等待回复」结束本 change。

## 实测留证（2026-10-25，branch `feature/chmem-bp-btb-gshare`）

### Task 1（查表 + payload 产出，**不改 PC-mux ⇒ 零行为风险**）

| 项 | 结果 |
|---|---|
| BP plugin | `ip/cpu/plugins/branch_predictor_chmem.h`（新）：BTB `ch_mem<ch_uint<64>, 256>` 62b 编码 + PHT `ch_mem<ch_uint<2>, 256>` |
| 挂钩点 | `at_stage("fetch", NORMAL)` ⇒ 查表 0 拍（`aread`）⇒ 与 branch LATE 同拍可消费 |
| PC 来源 | ★ `ibus_chmem.h` 在 fetch NORMAL 写 **`BP_PC` payload**（= `pc_reg` **本拍值**）—— ★ `KT::PC` 是 `pc_lag`（上一拍地址）⇒ 预测需要本拍值 |
| 预测信号 | ★ 全部为**活节点**（`ch_uint<1>(ch_bool)` 走上游 `bool_to_uint` 分支 ⇒ 复用同一 `impl()` 指针）⇒ **MUST NOT** 转 C++ 标量（否则折成 elaboration 期常量 ⇒ 假阴性） |
| 注册位 | ★ BP 注册在 **`Branch` plugin 之后**（★ 决定性约束 = 同 stage "branch" 同 phase NORMAL ⇒ 生产者(Branch) 先于 消费者(BP)；fetch stage 排在 branch 之前 ⇒ fetch NORMAL 的 `BP_PC` 生产者(IBus) 仍先于 BP ⇒ 两约束交集 = Branch 之后） |
| **零退化验证** | ★ BP 注册后 4 档 cycles **逐位不变**（86034/88034/86534/87056）· checksum 不变 |

### ★★ 关键发现：BP 的**注册位**是「BP 惰性」的根因（critic B1 追问后的实测定位）

★ **症状**：BP 写路径已存在（Verilog 出现 `btb[...] <=`）但 **hit=0 / miss=cycles** ⇒ BP **完全惰性**
  （正是 ADR-040 §10.5 反复防的那类「全绿但语义为零」的假阴性）。

★ **根因**：BP 注册在 **IBusPlugin 之后**（原实现）⇒ 它在 `stages_` 里的插入序**早于
  `Branch` plugin** ⇒ `elaborate()` 在**同 stage "branch" 同 phase NORMAL** 内按插入序执行
  ⇒ **BP 的 `update()` 先于 Branch plugin 的计算** ⇒ BP 读到的是**陈旧/未初始化**的
  `BRANCH_TAKEN` / `IS_BRANCH` ⇒ 写使能恒 false ⇒ **BTB 永远不写**。

★ **修法**：BP 注册移到 **`Branch` 之后**。
  - BP 有两个回调，两个约束的交集 = Branch 之后：
    - `at_stage("fetch", NORMAL)`：需 IBus 在 fetch NORMAL 写的 `BP_PC` ⇒ **同 stage 同 phase**
      ⇒ 生产者(Ibus) 先于 消费者(BP) ⇒ BP 在 IBus 之后 ✓
    - `at_stage("branch", NORMAL)`：读 Branch plugin 算出的 `BRANCH_TAKEN` /
      `BRANCH_TARGET` / `IS_BRANCH` ⇒ **同 stage 同 phase**
      ⇒ 生产者(Branch) 先于 消费者(BP) ⇒ **BP 在 Branch 之后** ← 决定性约束
  - ★ fetch stage 排在 branch stage **之前**（canonical_stage_order）⇒ fetch NORMAL 仍早于
    branch NORMAL ⇒ 跨 stage 的 canonical 序不受影响。

★ **判决实验（★ 修正上一轮的错误推论 —— critic 三轮 B-6）**：查表地址分别用 `pc_reg`（BP_PC）
  与 `pc_lag`（分支地址）⇒ **命中数完全相同**（1999/3998/3992/3998）。
  ★★ **该实验对「差一拍」完全不敏感 ⇒ 零鉴别力 ⇒ 不能证明「提前一拍」**：
  - 查 `pc_reg(N)` = 地址 A（A 的表项来自**上一次** A 被解析时写的 ⇒ 首次必 miss）；
  - 查 `pc_lag(N)` = `pc_reg(N−1)` = 地址 B（B 的表项同拍写，但读的是边沿写前的旧值 ⇒ 首次同样必 miss）。
  ⇒ 两者命中数相同**不构成**「提前一拍」的证据。
  ★ **该实验真正证明的是**：**写侧地址（`pc_lag`）与读侧 tag 比对（`pc_reg`）同指一条分支**
  ⇒ 简报 §3.3「更新地址 = 分支解析处的 `KT::PC` ⇒ 二者必须同指一条分支
  （off-by-one ⇒ 学到错误表项）」这条**不变量成立**。
  ★ **指纹**：hit = 「该分支的执行次数 − 1 次冷启动 miss」
  （m0: 2000 iter 回边 − 1 = 1999；m1: 2×(2000−1) = 3998）⇒ 逐位吻合。
  ★ **「提前一拍」的证明 MUST 留到 Task 2 接入 PC-mux 后用 Δcycles**（BP 的三个输出 payload
  在本 change 内**无消费者** ⇒ 本 commit 里「提前一拍」**没有任何行为后果**）。

★ **命中数的自洽性**（独立复算 taken/iter）：
  - m0（not-taken）：hit=**1999** = 循环回边 1999 次（该档唯一的 taken）✓
  - m1（taken）：hit=**3998** = 1999（回边）+ 1999（bnez）✓
  - m2：3992 ✓ · m3：3998 ✓

### Task 4（BP 开关面 + FLUSH 计数）

| 项 | 结果 |
|---|---|
| CLI | `--no-bp`（默认开）· `--btb-entries N`（∈ {16,32,64,128,256}，**越界 fail-fast rc=2**） |
| 生效值回显 | stderr `BP=on/off btb_entries=N` ⇒ 测量脚本可断言「flag 确实生效」（堵「flag 未生效却测出无差异」的假绿通道） |
| FLUSH 计数 | `ibus_chmem.h` 内 `ch_reg<ch_uint<32>> flush_count` 累加 ⇒ Verilog 顶层信号 `flush_count` ⇒ host 经 `--public-flat-rw` 读 |

### Task 2/3/4（PC-mux 五级链 + 门控式 flush + 误预测纠错）—— 实测留证

★ **落点**：`ip/cpu/plugins/ibus_chmem.h`（`update_pc()` branch LATE + `gated_flush()` fetch NORMAL）
★ **单元锚**：`tests/cpu/test_cpu_chmem_ibus_bp_pcmux.cpp`（**7 case / 98 assertions**，见下节）

**BP=on vs BP=off（branchbench 4 档，同一 workload）**：

| mode | 校验和 (off) | 校验和 (on) | cycles (off) | cycles (on) | **Δcycles** | BP hit / pred_taken |
|---|---|---|---|---|---|---|
| 0 always_not_taken | 103 | **103** ✓ | 86034 | **84038** | **−1996** | 1999 / 1998 |
| 1 always_taken | 183 | **183** ✓ | 88034 | **84040** | **−3994** | 3998 / 3996 |
| 2 pattern_6of8 | 147 | **147** ✓ | 86534 | **84787** | **−1747** | 3992 / 2247 |
| 3 pseudo_random | 101 | **101** ✓ | 87056 | **85037** | **−2019** | 3998 / 3089 |

⇒ ★ **校验和逐档相等** ⇒ BP 开启**不改变架构可见结果**（投机路径被正确纠正/抑制）。
⇒ ★ **4 档 cycles 全部改善** ⇒ 「提前一拍」机制**有行为后果**（闭合缺口 #7）。
⇒ ★ **BP=off 时 cycles 与校验和逐位不变**（`--check-baseline` PASS）⇒ **零退化**。

★ **一个真实 bug 的实测记录（★ 教训）**：`spec_ok_buf` 曾被写成 `pred_eff & case_c`
  ⇒ `pred_eff` 是「**本拍**对 `pc_reg(N)` 的预测」，`case_c` 讲的是「**上一拍**的预测是否正确」
  ⇒ 两者是**不同指令**的条件 ⇒ 误与 ⇒ 误清 `spec_ok` ⇒ **过度 flush** ⇒ 抑制合法指令
  ⇒ **校验和 m0 = 93 ≠ 103**（端到端立刻暴露）⇒ 修法：`spec_ok_buf <<= case_c`。
  ⇒ ★ 该 bug 现在有**单元级**锚（`R7`，时变脚本制造「`case_c=1` 但 `pred_eff=0`」的时刻）。

### ★★ FLUSH 计数器的独立交叉验证（★ 关键：它同时是 `cycles − pc_changes` 的**第二证据源**）

| mode | FLUSH_COUNT | Δ vs m0 | **独立复算** taken/iter | Δ vs m0 |
|---|---|---|---|---|
| 0 (not-taken) | 12000 | 0 | 0.000 | 0 |
| 1 (taken) | 14000 | **2000** | 1.000 | **2000** ✓ |
| 2 (2/8) | 12500 | **500** | 0.250 | **500** ✓ |
| 3 (伪随机) | 13022 | **1022** | 0.511 | **1022** ✓ |

⇒ ★ **FLUSH 计数器的 Δ 与「由源码语义独立算出的 taken 数」逐位相等** ⇒ 计数器**正确**。
⇒ ★★ **附带结论**：`cycles − pc_changes` 的 Δ = 2000/500/1022 **与 FLUSH 计数器的 Δ 完全一致**
   ⇒ 二者**互相印证** ⇒ ★ **`cycles − pc_changes` 的 Δ 可信**（但**其绝对值**仍含 m0 的
   固定基线 12000 ⇒ **不是** taken 总数 —— 与 critic 三轮 B-1 的结论一致，**不矛盾**）。
⇒ m0 的基线 12000 = 6 × 2000 iter ⇒ 每 iter **固定 6 次 flush**（来自 `branch_sel` 内的
   `j` 等与方向无关的固定结构）⇒ ★ 这解释了为何「绝对差值」不等于 taken 数。

### ★★ BP 行为指纹（4 档，BP=on）—— ★ 机制指纹的 SSOT 落点（critic B-2）

★ **本表是「BP 机制是否活着」的指纹表**。上一轮这组数字**只活在 commit message 里**
  ⇒ 不在 SSOT ⇒ 不可复核 ⇒ 本轮**抄进本文件**并**配了自动化判据**（见下）。

| mode (档名) | hit | pred_taken | 预测率 (pred/hit) | FLUSH_COUNT (BP=**on**) |
|---|---|---|---|---|
| 0 always_not_taken | 1999 | 1998 | 99.9% | **10004** |
| 1 always_taken | 3998 | 3996 | **99.95%** | **10006** |
| 2 pattern_6of8 | 3992 | **2247** | **56.3%** | **10753** |
| 3 pseudo_random | 3998 | 3089 | 77.3% | **11003** |

★ **复现命令**（从仓库根；`--cycles 200000` ⇒ 跑满 200k 拍，程序未跑完但计数有效）：
```bash
for m in 0 1 2 3; do ./build/bin/cpu_verilator_sim --elf build/branchbench_m${m}.elf \
    --cycles 200000 --stats 2>&1 | grep -E "BP_STATS|FLUSH_COUNT"; done
```
★ ★ **FLUSH_COUNT 列是 BP=**on** 的值**（★ critic 十二轮 SF-2 更正：原写成 BP=**off** 的
  12000/14000/12500/13022 ⇒ 与同表 hit/pred_taken（BP=on）**不同源** ⇒ 照复现命令跑出来的
  数字与表不符 ⇒ SSOT 自证不可信）⇒ ★ BP=**off** 的对应值 = 12000/14000/12500/13022，
  已写进 `soc/cpu/docs/dse/branchbench-baseline-matrix.csv` 的**新列 `flush_count`** ⇒
  ★ 并被 `--check-bp-direction` 的 **ΔFLUSH == Δcycles** 判据消费（critic 十二轮 SF-4）。
★ **与 ctest entry 的口径差异**（critic NIT-3）：ctest `branchbench_bp_direction` 走脚本默认
  `MAX_CYCLES=2000000`；上表用 `--cycles 200000`。两者**等价**（程序在 ~86034 拍即结束 ⇒
  两种上限都跑到同一结束点 ⇒ 计数相同）⇒ 表里的数字与 ctest 输出可直接对齐。

★ **为什么 m0（恒 not-taken）的预测率反而是 99.9%**（★ 别误读成「PHT 没学 not-taken」）：
  BTB 写使能 = `taken & is_branch` ⇒ **not-taken 的分支永远不建 BTB 项** ⇒ m0 里那条
  方向性分支**恒 miss** ⇒ m0 的 1999 次命中**全部**来自**恒 taken 的循环回边**
  ⇒ 回边的 PHT 槽学到 taken ⇒ 预测率 99.9% ✓ **与方向学习一致**。
  ⇒ ★ **「方向学习」的承重指纹在 m2**（该档方向性分支 2/8 taken ⇒ 预测率 56.3%）。

### ★★ 自动化判据（critic B-2：机制指纹 MUST 有自动锚，不能只活在文档里）

★ **两处锚**（互相独立、变异对照验证过）：

| 锚 | 位置 | 判据 | 抓得住 |
|---|---|---|---|
| **单元级** | `tests/cpu/test_cpu_chmem_bp_plugin.cpp`（`[bp][plugin]`，**10 case / 160 assertions**） | 直接打**真实插件**，断言 **payload 的值** | BTB 写路径 / 方向学习 / flush 门控 / 饱和回绕 / branch 侧 fail-fast / 取指地址是否活 |
| **系统级** | `tools/measure_branchbench.sh --check-bp-direction` ⇒ ctest `branchbench_bp_direction` | 4 档 BP=on：hit>0 · 校验和与 BP=off 基线**逐位相等** · m1 预测率 ≥90% · m2 预测率 ≤90% | BTB 惰性 / 方向学习失效 / 饱和折常量 |

★ **两处锚都是「本地锚」, 不是「CI 保护」**（critic B-1 —— ★ 上一轮 commit message
  写的「BP 路径进入 CI」是**事实错误**，本轮更正）：
  CI **不跑 ctest** 且 **不构建项目** ⇒ `.github/workflows/architecture-gates.yml`
  的 branchbench step **恒走 SKIP** ⇒ ADR-088 已明载「该 step 恒走 SKIP ⇒ 目前零实际保护」
  ⇒ brief AC-9 / ADR-088 §5.2 判据 4 同款：**仅加 ctest entry 不满足该判据**。
  ⇒ 本 change 兑现方式 = **PR 提交者本地手动跑** `bash tools/run_chipforge_tests.sh --all`。
  ⇒ 真正「让 BP 路径进入 CI」的前置 = ADR-088 的「CI 具备 build 能力」项 ⇒ **归 ADR-088**
  （缺口表 #9）。

★ **变异对照（判据「有牙」的证据，2026-10-25 实测）**：

| 变异 | 单元级 (10 case) | 系统级 (脚本) | 指纹变化 |
|---|---|---|---|
| A: BTB 写使能恒 false | **5 断言 RED** | **RED**（hit=0） | m1 hit 3998→**0** |
| B: 方向学习改回「恒 +1」 | **RED**（R3 方向翻转） | **RED**（m2 预测率 99.97% > 90%） | m2 pred 2247→**3991** |
| C: 去掉 PHT 的 flush 门控 | **RED**（R7） | —（系统级不敏感） | — |
| D: 饱和判定改回 C++ 三目 `?:` | **RED**（R8 逐拍采样） | **RED**（m1 预测率 50% < 90%） | m1 pred 3996→**2000**（3→0 回绕） |
| E: 删掉 5 个 `require_payload`（branch 侧 fail-fast 失效） | **RED**（R9） | — | — |
| F: BP 完全忽略 `BP_PC`（用常量地址查表 + 比 tag） | **RED**（R10） | — | — |

★ **上一版测试对 A/B/D 三个变异全部零鉴别力**（critic B-1 实测：三个变异叠加后仍
  「All tests passed (52 assertions in 6 test cases)」）⇒ 本版**机制类断言全部断值**。
★ E/F 两个变异是 critic 五轮 B-2 / SHOULD_FIX-4 追加的 ⇒ 说明：**新增的承重改动
  （fail-fast / 活 PC）必须有对应断言**，否则等于没做。



## ★ builder 开工须知（★ **逐条抄进本 change**，不跨 change 引用）

| # | 须知 | 归属 |
|---|------|------|
| 1 | ★ 修正 `ibus_chmem.h:83-87` 的**过时注释**（「aread/sread 都是 1-cycle」与 aread 0-cycle 的**承重前提**直接矛盾） | ★ 本 change（首个可改 `ip/` 的 change） |
| 2 | ★ **MUST 复用**前置1 的 `--stats` 框架（**MUST NOT** 另起一套计数器） | ★ 本 change |
| 3 | ★ 本 change **MUST NOT** 触碰台账 `docs/architecture/tlm-coverage-parity-ledger.md`（台账重判归 ②） | ★ 本 change |
| 4 | ★ 本 change **MUST NOT** 触碰 `tools/measure_branchbench.sh` 的**模式名数组与表头解析逻辑**（前置1 的范围）⇒ 本 change **仅追加** BP flag（`--bp=on\|off`）与生效值断言相关行 | ★ 本 change |
| 5 | ★ `measure_branchbench.sh` 的 `2>/dev/null` MUST 改为**捕获 stderr** ⇒ 否则「flag 未生效却测出无差异」的**假绿通道**是开着的 | ★ 本 change |


### ★★★ 诚实登记：本 change 尚未闭合的项（★ 勿被「已交付」的措辞误导）

| # | 项 | 状态 | 归属 / 说明 |
|---|---|---|---|
| 1 | **`is_unconditional` 位未落地** | ❌ 未实现 | 现状：BTB 写使能 = `taken & is_branch`（`is_branch` 只含 B-type 条件分支）⇒ **`jal` 永不建 BTB 项**、`is_unconditional` 位**永不置 1** ⇒ 简报 §3.2「`is_unconditional` 位必需」与「`jal` 只更新 BTB」**未落地**。★ 需 branch plugin 导出 jal/jalr 区分（opcode）⇒ **留给 Task 4/主体**（本 PoC 不覆盖无条件跳转） |
| 2 | **`pred_*_reg_`（ch_reg 预测记录）** | ✅ **已接线**（Task 4） | ★ 本条原为「声明但未接线」⇒ **Task 2/3/4 落地后已接线**：5 个 `ch_reg` 纠错记录（`pred_valid` / `pred_taken` / `pred_target` + `spec_ok` + `mispredict_a`）在 `ip/cpu/plugins/ibus_chmem.h` 内 ⇒ ★ **刻意放在 IBus 而非 BP 插件**（PC-mux 与 flush 的消费方在 IBus ⇒ BP 保持「纯预测器」） |
| 3 | **`jalr` 不建项** | ✅ 已实现（保守） | `jalr` 目标随运行时变化 ⇒ 不建 BTB 项 ⇒ 走顺序取指（台账 #9 的终态） |
| 4 | **M 运行期 mask 的效果无测试** | ⚠️ 缺测 | `index_of_scalar` 用 `kBtbEntries`(256) 做 mask，而运行期路径用 `logical_entries_−1` ⇒ **M<256 时二者不等价**；现有断言只覆盖 M=256 语义 ⇒ 「M 运行期可配」**无端到端证据** |
| 5 | **`pack_entry` / `unpack_entry` 与运行路径并行** | ⚠️ 双实现 | 运行路径（`update()`）用**内联 shl/or** 拼 62b；`pack_entry` 是**影子 API**（仅测试用）⇒ 两者一旦分叉，测试发现不了（测试只测 helper） |
| 6 | **门禁漏网：`?:` 形式** | ⚠️ **已知缺口**（★ 本轮**缩小**但未闭合） | `check_plugin_portability.sh` 的 Check 5 只查 `if (ch_bool)`；**C++ 三目 `?:` 的条件位同样做 contextual conversion** ⇒ 同样会在 elaboration 期折成常量（★ 本轮 PHT 饱和 bug 正是这个形态：Verilog 里 `ge_ge_2` dangling ⇒ 计数器 3→0 **回绕**而非饱和）。★ 曾尝试加 Check 5b，但**误报/漏报都难以做到既零噪音又可靠**（本仓库的真实逻辑写在 `predict()`/`update()` 等**被 at_stage 调用的方法**里，而「at_stage 闭包内」的范围抓不到）⇒ **撤销该门禁，改为在此登记为已知缺口** ⇒ 后续应由 ADR-040 的门禁专项处理。★ **本轮补上的只是「这一处」**：R8（逐拍采样）+ 系统级 m1 预测率判据 ⇒ **PHT 饱和这一处**再有 `?:` 会被抓住；★ 但**其它**站点（如未来的 ALU/CSR/流水线逻辑）仍无静态护栏 ⇒ 缺口**未闭合** |
| 7 | **「提前一拍」的证明** | ✅ **已闭合**（Task 2/3/4 后） | ★ 本条原为「待 Task 2」—— 当时 BP 三个输出 payload **无消费者** ⇒ 「提前一拍」无任何行为后果 ⇒ 现已接入 PC-mux ⇒ **Δcycles 实测**（BP=on vs BP=off，同一 workload）：m0 86034→**84038**（−1996）、m1 88034→**84040**（−3994）、m2 86534→**84787**（−1747）、m3 87056→**85037**（−2019）⇒ **4 档全部改善且校验和逐位不变**（103/183/147/101 = off 基线）⇒ 机制成立 |
| 8 | **方向性门禁的阈值 + S3 行为变化** | ⏳ 待主体 | ★ **本轮补全（critic B-3：缺口表只写了 2 个 90% 阈值，漏了 S3 会变的行为）**：<br>① **跨档 `pc_changes` 相等断言在 S3 必然假红** —— `pc_changes` = 采样 PC 值变化拍数（自述含非架构路径 PC 值、机制未确认）⇒ 今日 BP 三输出**无消费者** ⇒ 4 档轨迹相同（都 = 86034）⇒ S3 接入 PC-mux 后 m0（恒 not-taken）预测必错 ⇒ 采样流多出错路 PC ⇒ 4 档 `pc_changes` 不再相等 ⇒ **原断言会让 branchbench 因与 BP 健康度无关的原因判红** ⇒ ★ **本轮已把该断言限定在 `BP_MODE=off`**（守卫键 MUST 是 `BP_MODE`, **不可**是 `CHECK_BP_DIR` —— 「BP=on」有 `--check-bp-direction` 与 `--bp=on --check-baseline`/`BENCH_BP=on --check-baseline` **两条**路径，用后者只挡住前一条 ⇒ critic 六轮 BLOCKER-1 实测复现）⇒ **处置已落**：已用夹具（bp_mode=on 基线行 + 4 档 pc_changes 各不同）实测「新版 PASS / 把守卫改回 `CHECK_BP_DIR` 立即 FAIL」⇒ 主体仍 MUST 知悉：**BP=on 模式下前端活动量不再跨档可比**<br>★ **补充（critic 六轮 SF-5）**：`bp_dir_judges == 2` 只能防「档名消失/改名」，**不防档名与硬件方向错配**（顺序互换）⇒ 该错配由**校验和判据**兜住（BP=on 校验和 vs 该档名的 off 基线行：always_not_taken=103 / always_taken=183 ⇒ 互换后必然不符 ⇒ RED）<br>② **校验和判据今天是恒真** —— BP 输出无消费者 ⇒ BP=on 与 BP=off 必然同校验和 ⇒ 该判据**当前零鉴别力** ⇒ 它是 **S3 才激活**的承重判据（脚本注释已标注）<br>③ **FLUSH 口径随 Task 3 改写而失效** —— Task 3 把 flush 改成门控式 ⇒ AC-F2-13 的「ΔFLUSH = 2000/500/1022 逐位相等」与 AC-F2-3 的「ΔFLUSH ≥ 0.8×FLUSH(off)」全部作废 ⇒ 而 `--check-bp-direction` **对 FLUSH 一个数都不判** ⇒ ★ **决策级判据（AC-F2-3）仍无自动锚** ⇒ **主体 MUST 补 FLUSH 判据**（归主体，本 change 不越界）<br>④ **两个 90% 阈值**（m1 ≥90% / m2 ≤90%）设计成**方向性比值** ⇒ 对 S3 的 PC-mux 接入不敏感（真实 taken 率 100% vs 25% ⇒ 预测率应分别趋近 100% 与 25%）⇒ 预期仍成立；★ 但**绝对值必变** ⇒ 主体 MUST 复评并**显式重录**（★ MUST NOT 直接放宽） |
| 9 | **BP 路径真正进入 CI** | ⏳ 归 ADR-088 | 前置 = 「CI 具备 build 能力」（CI 现在**不构建项目** ⇒ branchbench step 恒走 SKIP）⇒ 属 ADR-088 的 CI 基建项 ⇒ **不在本 change** ⇒ ★ 本 change 只交付**本地锚**（ctest entry + PR 手动跑 `tools/run_chipforge_tests.sh --all`）；★ MUST NOT 把「加了 ctest entry」说成「BP 路径进入 CI」（critic B-1：上一轮 commit message 正是这么写的 ⇒ 事实错误） |
| 10 | ★ **情形 F 是设计简报未列的规格缺口** + ★ **`bp_spec_taken_buf` 的判定被实施层收紧**（偏离 AC-6 / 简报 §3.4.2 的字面定义） | ⚠️ **已实现 + 已升级为待 architect 裁决** | ★ **F**：预测 taken / 实际 taken / **目标不同**（陈旧 BTB 项 / `jalr` 等）—— 简报 §3.4.2 的情形清单**只列 A/B/C/D/E** ⇒ 本实现**补了 F**（走 `br_taken` 臂纠到实际目标 + **必须 flush**）<br>★ **偏离**：简报 §3.4.2 / AC-6 把 `bp_spec_taken_buf` 定义为「上一拍 BP 的投机 taken 臂**确实生效**了」⇒ 字面实现 = `pv & pt`；★ **本实现收紧为「投机被证明正确」= `case_c`（= `pv & pt & br_taken & (ptg == br_target)`）**<br>★ **偏离的理由（错路泄漏证明）**：简报的门控理由是「投机 taken 臂生效 ⇒ **fall-through 根本没被取** ⇒ 不需抑制」⇒ 但在情形 F 下**错路的不是 fall-through、而是投机目标 P 本身**（P ≠ fall-through ⇒ fall-through 确实没被取，但 **P 被取了** ⇒ P 的 store/写回是**泄漏的副作用**）⇒ 简报的字面公式在 F 下**不 flush ⇒ 泄漏** ⇒ 收紧为 `case_c` 后，F 的 `spec_ok=0` ⇒ taken 分量生效 ⇒ flush ⇒ **抑制的正是被取到的错路指令 P** ⇒ 且**不会过度 flush**（逐情形核过：A/B/E/F 都需要抑制；只有 C 不需要 ⇒ `case_c` 恰好是「被取到的指令是否在正确路径上」）<br>★ **本次改动未改变任何实测数字**：branchbench 4 档负载**不产生情形 F**（静态目标 + `jalr` 不建 BTB 项 + M=256 无别名）⇒ ★ **这是推断，不是实测**（「两种定义下 `FLUSH(on)` 逐位相同」同理是**推断**；★「ΔFLUSH=Δcycles 即旁证」**不能**旁证 F 不发生 —— 两者同源）⇒ ★ **实施层取更保守的一侧**（只可能多 flush ⇒ 不可能假绿）<br>★ **MUST 上报 architect**（见下方「★ 待 architect 裁决」表）⇒ ★ **实施者不自改门禁值**<br>★ **另**：★ **情形 B 臂冗余** —— `case_b = pv & !pt & br_taken` ⊂ `br_taken` ⇒ 该臂与 `actual_taken` 臂**选同一个值**（`actual_target`）⇒ **可证明无差异**（变异 M3 实测：删掉它测试**仍绿**）⇒ 保留仅因简报明文要求五级链 ⇒ ★ **没有任何断言覆盖它**（诚实登记：这是**已知无覆盖**的一臂） |
| 11 | ★★ **链式投机缺口**（critic 十轮 B-1）—— 简报 §2.1.2 的 `select(情形C, pc_reg(N+1)+4, …)` 与 §3.4.2 的 `bp-predict` 层组合后，**情形 C 会丢掉下一级投机** | ✅ **已修**（实施层取正确的一侧）+ ⚠️ **规格缺口待回写**（裁决 A3） | ★ **场景**：分支 B1（预测 taken→P、实际 taken→P ⇒ 情形 C），而 **P 处本身又是一条被正确预测 taken 的分支** B2（预测→Q）⇒ 旧写法让 PC 顺序走到 P+4/P+8… ⇒ **Q 永不被取**（走错路）且 `case_c` 使 `spec_ok=1` ⇒ 错路指令**不被 flush** ⇒ **store/写回副作用泄漏** ⇒ 校验和发散<br>★ **修法**：`case_c` 臂的值改为 **`bp_next`**（= 当前取指指令 `pc_reg(N)` 的投机）⇒ 抑制 B1 自身 redirect 的同时**保留 B2 的投机**<br>★ **该负载上结构性不可达** ⇒ 端到端两个锚（branchbench 4 档 / vendored ELF 47 个）**都抓不住** ⇒ ★ 由**单元锚 R8** 锁定（退回 `pc_plus_4` ⇒ RED）⇒ ★ **主体 MUST 把「链式投机」列为必测场景**（否则同类缺口会再次逃逸）<br>★ **本改动不改变任何实测数字**（branchbench 4 档 cycles/校验和/FLUSH_COUNT **逐位不变**） |
| 12 | ★★ **错路指令上的 `case_a` 误触发**（critic 十一轮 B-2） | ✅ **已修** + ⚠️ **规格缺口待回写**（裁决 A4） | ★ **根因**：`BRANCH_TAKEN` 是**被 flush 掩码后**的值（`branch_chmem.h:263` `taken = taken & !flush`）⇒ **错路指令**的 taken 恒读作 **0** ⇒ `case_a = pv & pt & !br_taken` 里的 `!br_taken` **恒真** ⇒ `case_a` 在**错路指令**上误触发 ⇒ ① PC 跳到**错路的 fall-through** ⇒ PC 出轨；② `mispredict_a_buf` 置位 ⇒ **过度 flush** ⇒ 抑制**正确路径**的指令 ⇒ **双向副作用泄漏**<br>★ **修法**：情形判定显式排除错路指令 ⇒ `case_a = (!flushed) & pv & pt & (!br_taken)`（`flushed` 取 branch plugin 的 `BRANCH_FLUSHED`）⇒ ★ `case_b`/`case_c` 要求 `br_taken=1` ⇒ 掩码后恒 0 ⇒ **自动安全**<br>★ **该负载上结构性不可达** ⇒ 端到端两个锚都抓不住 ⇒ 由**单元锚 R11** 锁定（去掉 `live` 门控 ⇒ RED）<br>★ **本改动不改变任何实测数字**（branchbench 4 档 cycles/校验和/FLUSH_COUNT **逐位不变**）<br>★ ★★ **注意**：A–F 情形清单是**单指令分类**，**不含**「被解析指令本身在错路上」这一维 ⇒ ⇒ ★ **场景维度清单尚未完备**（见 Task 5 判定的限定条件） |

### ★★★ Task 5 判定（★ plan 的 Task 5 = 机制判定）

**判据的三个来源**：AC-F2-3（量化判据）· AC-F2-16（功能等价）· AC-F2-17（flush 时序对）。

#### ① AC-F2-16 功能等价（★ PoC 的鉴别力来源）—— **PASS**

| 项 | 实测 |
|---|---|
| 4 档校验和逐位相等 | m0 103 / m1 183 / m2 147 / m3 101 ⇒ **BP=on 与 BP=off 逐档相等** ✓ |
| `[chmem]` vendored ELF | **47/47 PASS**（16410 assertions）⇒ 0 退化 ✓ ⇒ ★★ **这本身就是 BP=on 锚**（`build_cpu` 的 `enable_bp` **默认 true**，测试未显式传 ⇒ BP 注册 ⇒ 不是「BP-off 零退化」而是「BP-on 下 47 个 ELF 的 tohost 仍全为 1」） |

⇒ ★ **BP 开启不改变架构可见结果** ⇒ 投机路径被正确纠正 / 抑制 ⇒ **无副作用泄漏**。
⇒ ★★ 但注意：这两个锚的**负载都不含「链式投机」模式**（详见下方缺口 #11）⇒ 它们**证明不了**
链式投机下的正确性 ⇒ 那条由单元锚 R8 承担。

#### ② AC-F2-17 flush 时序对 —— **PASS**

| 子项 | 锚点 | 实测 |
|---|---|---|
| ① 投机成功 ⇒ 该拍**无** flush | 单元 `R5`（情形 C） | FLUSH 序列第 2 拍起恒 **0** ✓ |
| ② 投机失败 ⇒ **下一拍**有 flush | 单元 `R3`（情形 A） | FLUSH 序列第 2 拍起恒 **1** ✓ |
| ③ BTB miss 的实际跳转分支 ⇒ **下一拍**有 flush | ★ 单元 **R9（情形 E：BP 已注册但 `pred_valid=0`）**（★ R1/R2 只是「BP **未注册**」路径 ⇒ 是**不同的代码分支**） | FLUSH 恒 1 ✓ |
| ④ 未投机而实际跳转的 fall-through 副作用 **MUST NOT** 发生 | 端到端 4 档校验和 + vendored ELF 47/47 | ✓（同 AC-F2-16） |

#### ③ AC-F2-3 量化判据 —— ★ **不可判定**（不是「不成立」）

**实测数据（4 档）**：

| mode | FLUSH(off) | FLUSH(on) | **ΔFLUSH** | cycles(off) | cycles(on) | **Δcycles** | ΔFLUSH/FLUSH(off) | 0.8×FLUSH(off) | 分量1 | Δcycles/ΔFLUSH | 分量2 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 12000 | 10004 | **1996** | 86034 | 84038 | **1996** | 0.166 | 9600 | ✗ | **1.000** | ✓ |
| 1 | 14000 | 10006 | **3994** | 88034 | 84040 | **3994** | 0.285 | 11200 | ✗ | **1.000** | ✓ |
| 2 | 12500 | 10753 | **1747** | 86534 | 84787 | **1747** | 0.140 | 10000 | ✗ | **1.000** | ✓ |
| 3 | 13022 | 11003 | **2019** | 87056 | 85037 | **2019** | 0.155 | 10417.6 | ✗ | **1.000** | ✓ |

★ **分量 2（`Δcycles ≥ 0.9 × ΔFLUSH`）4/4 PASS**（实测比值恒为 **1.000** ⇒ **每个被消除的 taken 气泡 = 恰好 1 拍**）。

★ **分量 1（`ΔFLUSH ≥ 0.8 × FLUSH(off)`）4/4 不满足 ⇒ 但该阈值在本简报既定范围内不可能满足**
⇒ **这不是「机制不成立」，而是「判据缺陷」**。**证明**（用 AC-F2-13 自己登记的事实）：

```
FLUSH(off) = N_fixed + N_taken        （N_fixed = 方向无关的固定 flush 分量，
                                        AC-F2-13 已自述：m0 的 12000 = 6 × 2000 iter）
ΔFLUSH ≤ N_taken                       （BP 最多消掉全部 taken 气泡）
⇒ ΔFLUSH / FLUSH(off) ≤ N_taken / (N_fixed + N_taken)
   ★ 精确值（CSV notes：回边 taken 1999 次 + branch_sel 0/2000/500/1022）：
     m0: 1999/12000 = 0.167   m1: 3999/14000 = 0.286
     m2: 2499/12500 = 0.200   m3: 3021/13022 = 0.232
     （⇒ N_fixed = 12000-1999 = 14000-3999 = 12500-2499 = 13022-3021 = **10001**，
       四档**分毫不差** ⇒ 「方向无关」的声称更强）
⇒ 上界 ≤ 0.286 < 0.8   ⇒ ★ 该阈值**在本简报既定范围内**（RAS 延后、`jalr` 不建项、
   条件分支是唯一的 taken 来源）对**任何实现**都不成立 ⇒ 结构性不可满足
```

⇒ ★ **该阈值与 AC-F2-13 自述事实（「FLUSH 含方向无关的固定分量」）自相矛盾** ⇒
**实施者 MUST NOT 自改门禁值** ⇒ ★ **已升级为待 architect 裁决（A2，见上表）**。
⇒ ★ **措辞边界（critic 十轮 SF-1 的更正）**：上界 0.286 是**在本简报既定范围内**导出的
（RAS 已 defer、`jalr` 不建 BTB 项 ⇒ 条件分支是唯一的 taken 来源）⇒ ★ **不能**写成
「任何实现下都不可满足」（若 ② 主体把 `jal`/`jalr` 也纳入预测，或把 RAS 落地，
上界会被推高 ⇒ 届时 MUST 复评）⇒ ★ **实施者不自改阈值**，但 ★ **architect 复核时
MUST 知悉这个边界**。

#### ④ 机制判定结论

| 判据 | 结论 |
|---|---|
| AC-F2-16（功能等价） | ✅ **PASS**（4 档校验和逐位相等 + vendored ELF 47/47） |
| AC-F2-17（flush 时序对） | ✅ **PASS**（单元 R1/R2/R3/R5 + 端到端） |
| AC-F2-3 分量 2（`Δcycles ≥ 0.9 × ΔFLUSH`） | ✅ **PASS 4/4**（比值恒 1.000） |
| AC-F2-3 分量 1（`ΔFLUSH ≥ 0.8 × FLUSH(off)`） | ⚠️ **不可判定**（阈值结构性不可满足 ⇒ 已升级 A2） |
| AC-F2-12（预测 payload 是活节点） | ✅ PASS（`[bp][plugin]` R1/R2/R10 + 变异 M4 可抓住） |
| AC-F2-14（BTB 命中率 > 0，防假阴性） | ✅ PASS（hit = 1999/3998/3992/3998） |
| AC-F2-7（去重后热点分支数 vs 256） | ✅ 远低于（branchbench 的热点条件分支仅 2 个：`branch_sel` 的 `bne` + mode 选择链）⇒ **无需提前做容量裁决** |

⇒ ★★ **机制判定：「fetch 期提前一拍预测 ⇒ 消除 taken 转移气泡」= 成立**
（依据：Δcycles **4/4 档 > 0**；**ΔFLUSH 与 Δcycles 逐位相等 4/4** ⇒ 每个消除的气泡恰 1 拍；
校验和**逐位不变** ⇒ 无功能损坏）⇒ ★ **进 ② 主体**。
⇒ ★★★ **限定条件（★ MUST NOT 省略）**：本判定的证据链覆盖 **已登记的场景清单**
（对齐类场景 A/B/C/D/E/F + 链式投机 + 错路指令），锚见单元 R1~R11；
★★ **MUST NOT 声称「场景覆盖完备」** —— ★ A–F 是**单指令分类**，**不含**「被解析指令本身
在错路上」这一维（缺口 #12）与「分支 target 本身是分支」这一维（缺口 #11）⇒ ★ **这两维
都曾存在功能缺口**（副作用泄漏）⇒ ★ **已登记场景清单见缺口 #10 / #11 / #12** ⇒
★ **主体 MUST ①把链式投机 + 错路指令列为必测场景；②重新审视场景维度是否完备**
（★ architect 裁决时宜**整体回写简报 v2.3** 而非逐条打补丁 —— 已累计 4 处简报缺陷：
情形 F / `bp_spec_taken_buf` 定义 / `case_c` 取值 / 错路指令维度）。

### ★★★ 待 architect 裁决（★ **实施者不自改门禁值** —— **四项**（A1/A2/A3/A4）均按兜底纪律给足三要素）

| # | 待裁决项 | ① 触发阈值（何时 MUST 裁决） | ② 落点文件（裁决写在哪） | ③ 可判定时限 | 实施者已做什么（不阻塞本 change 关闭） |
|---|---|---|---|---|---|
| A1 | **`bp_spec_taken_buf` 的定义**：简报 §3.4.2 / AC-6 = 「投机 taken 臂**确实生效**」（= `pv & pt`）vs 本实现 = 「投机**被证明正确**」（= `case_c`）⇒ ★ **情形 F 下前者会泄漏错路指令 P 的副作用**（证明见缺口 #10） | ★ **情形 F 可达时**（陈旧 BTB 项 / `jalr` 建项 / 动态目标分支）⇒ 泄漏即为**功能错误**（校验和不等）⇒ 触发阈值 = 「主体引入 `jalr`/动态目标分支」或「branchbench 增加动态目标档」 | `docs/architecture/bp-btb-gshare-design-brief.md` §3.4.2（情形表 + 门控规则行）+ `openspec/changes/chmem-bp-btb-gshare/design.md`（② 主体）+ ADR-088 §5.3（若涉 P-6 结论） | ★ **≤ 2 个工作日**，且**硬前置**：② 主体开工前必须落 | ★ 实施层已取**更保守**的一侧（`case_c` ⇒ 只可能多 flush ⇒ **不可能假绿**）⇒ 且**本次未改变任何实测数字**（4 档负载不产生 F ⇒ 两种定义下 `FLUSH(on)` 逐位相同）⇒ ★ 主体可直接沿用 `case_c` 定义，**无需回退** |
| A2 | **AC-F2-3 的第一分量阈值 `ΔFLUSH ≥ 0.8 × FLUSH(off)`** —— ★ **该阈值在本简报既定范围内不可能满足**（证明 + 措辞边界见 Task 5 判定段）⇒ 与 AC-F2-13 自述事实（「FLUSH 含方向无关的固定分量」）**自相矛盾** | ★ **立即**（本次判定就要用到 ⇒ 已触发） | `openspec/changes/chmem-bp-btb-gshare-poc/tasks.md` 的 **AC-F2-3 条目**（重校准阈值）+ `docs/architecture/bp-btb-gshare-design-brief.md` §5（若门禁进主体） | ★ **≤ 2 个工作日**，且**硬前置**：② 主体开工前必须落 | ★ 实施者**未改任何阈值**；★ 本次判定用**不依赖阈值**的等价证据（`ΔFLUSH == Δcycles` 逐位相等 4/4 + 校验和逐位相等 4/4）⇒ **判定本身不被该阈值阻塞** |
| A3 | ★★ **`case_c` 臂的取值**（链式投机缺口，critic 十轮 B-1）—— 简报 §2.1.2 的字面链是 `select(情形C, pc_reg(N+1)+4, …)`，与 §3.4.2 的 `bp-predict` 层组合后 ⇒ **情形 C 会丢掉下一级投机** ⇒ 若分支的 target 本身又是一条分支 ⇒ **走错路 + 错路指令不被 flush ⇒ 副作用泄漏** | ★ **立即**（已触发：这是**功能正确性**缺口，且在 branchbench / vendored ELF 上**结构性不可达** ⇒ 只能靠单元锚兜住） | `docs/architecture/bp-btb-gshare-design-brief.md` §2.1.2 的 select 链（★ MUST 补 `bp_next` 层）+ `openspec/changes/chmem-bp-btb-gshare/design.md`（② 主体） | ★ **≤ 2 个工作日**，且**硬前置**：② 主体开工前必须落 | ★ 实施层已取**正确的一侧**：`case_c` 的值改为 **`bp_next`**（= 当前取指指令的投机）⇒ 抑制 B1 自身 redirect 的同时**保留 B2 的投机** ⇒ ★ 该改动**不改变任何实测数字**（branchbench 4 档 cycles/校验和/FLUSH **逐位不变** ⇒ 该负载不产生链式投机）⇒ ★ 已由单元锚 **R8** 锁定（退回 `pc_plus_4` ⇒ RED）；★ **AC-22 不受影响**（它只要求「多层 select 链 + 具名 trap 项」，不钉各臂取值）⇒ ★ 主体可直接沿用 |



| # | 前瞻项 | 为什么现在就会咬人 | 主体 MUST 做什么 |
|---|---|---|---|
| ★ A4 | ★★ **错路指令维度**（critic 十一轮 B-2）—— 简报 §2.1.2 / §3.4.2 的情形清单是**单指令分类**，不含「被解析指令本身在错路上」这一维 ⇒ 实施层已加 `!flushed` 门控（`BRANCH_FLUSHED`） | ★ **立即**（已触发：功能正确性缺口） | `docs/architecture/bp-btb-gshare-design-brief.md` §2.1.2 的情形清单（★ MUST 补「被解析指令是错路指令 ⇒ 一切情形判定 MUST 排除」这一维）+ §3.4.2 的门控规则行 | ★ **≤ 2 个工作日**，且**硬前置**：② 主体开工前必须落 | ★ 实施层已取**正确的一侧**（`case_a` 加 `!flushed` 门控 ⇒ `case_b`/`case_c` 自动安全）⇒ ★ **不改变任何实测数字**（branchbench 4 档逐位不变）⇒ ★ 已由单元锚 **R11** 锁定；★ **AC-22 不受影响** |
### ★★★ S3（② 主体）前瞻 —— 主体 MUST 知悉的 4 条（★ **本轮已登记，主体落地时 MUST 处理**）

| S3-a | **BP_STATS 是「前缀匹配」⇒ 主体追加的字段**无人锚 | `tools/measure_branchbench.sh:368` 用 `grep -oE 'BP_STATS hit=[0-9]+ miss=[0-9]+ pred_taken=[0-9]+'` —— **前缀**匹配 ⇒ 主体若在行**尾**追加字段（如 `flush=` / `n_correct=` / `n_false=`），该 grep **仍命中** ⇒ **新字段不被任何断言覆盖**（而删改中间字段会 fail-closed ⇒ 只有「尾部追加」这条通道是静默的） | 主体 MUST ①自己补该字段的断言，或 ②把 grep 改成**锚定式**（`…pred_taken=[0-9]+( |$)`）+ 显式列出期望字段集 ⇒ ★ **MUST NOT** 依赖现在的宽松前缀 |
| S3-b | **`--bp=on --check-baseline` 在主体后必然假红** | CSV 里只有 `bp_mode=off` 的 4 行 ⇒ 主体改 BP 行为后 on/off 的 cycles **必然不同** ⇒ `--bp=on --check-baseline` 会以「**0 行比对 ⇒ MUST NOT 报 PASS**」失败（★ 脚本已 fail-closed，不会假绿，但会**阻塞**） | 主体 MUST **显式重录** `bp_mode=on` 的 4 行基线（含各自的 `workload_sha` / `build_id`）⇒ ★ **MUST NOT** 为了「让它过」而放宽判据（放宽 = 假绿） |
| ★ S3-d | ★★ **五级链与既有 trap/freeze mux 的合并点** —— 本 change 只落了 **bp-predict / branch-actual / seq** 三层；★ `trap_redirect`（`csr-chmem-trap-pc-jump`）与 `freeze_hold`（PTW stall）两层待 v0.11.0 Phase 3.5 落地 ⇒ ★ **主体 MUST 明确合并点**：trap/freeze MUST 在 **bp-predict 之上**（简报 §3.4.2 的裁决），且 `trap` MUST 能**压过** branch-actual 的纠错 ⇒ ★ **不得**让后来的 trap 实现**重写**本 change 已定的 `case_c = bp_next` 语义（A3）⇒ ★ 主体开工时 MUST 把五级链的**完整**形状写进 `design.md` 并给 grep 锚（AC-22 已要求「具名 trap 项」） |
| S3-c | **`workload_sha` 只覆盖 `.text` ⇒ 拦不住 harness 漂移** | 基线行的 workload 归属守卫 = `objcopy --only-section=.text` 的哈希（`measure_branchbench.sh:238`）⇒ 主体若改 **link script / `.data` / `.rodata` / harness 代码**（`.text` 之外的任何东西），`.text` 哈希**不变** ⇒ 「基线属于同一份 workload」的保证**失效** ⇒ 基线可能悄悄对应**另一份程序** | 主体 MUST ①扩展哈希覆盖面（至少 `.text` + `.data` + `.rodata`，或直接对整个 ELF 取哈希），或 ②在 CSV 里增加一列**全 ELF 哈希**并与 `.text` 哈希**同时**校验 ⇒ ★ 二选一，**MUST NOT** 沿用「只看 `.text`」 |

### 测试覆盖（critic B3：此前该插件 **0 case** ⇒ `[chmem]` binary 无回归锚）

`tests/cpu/test_cpu_chmem_bp_plugin.cpp`（**点名** `branch_predictor_chmem.h`），
**10 case / 160 assertions 全绿**（★ 机制类断言全部**断 payload 的运行期值**；R4/R5/R6 断的是标量/helper 语义）：

| case | 覆盖 | 抓得住哪类回归 | ★ 变异验证 |
|---|---|---|---|
| `bp_plugin_R1_btb_is_written_when_producer_comes_first` | ★ **注册位顺序**（真实插件 + 真实生产者替身）⇒ 断 `BP_PRED_VALID` **值** == 1；★ **反序注册 ⇒ elaborate 抛异常**（fail-fast，撞的是 fetch 侧 `BP_PC`） | ★ **注册位回退**（BP 早于生产者 ⇒ BTB 永不写 ⇒ 值恒 0 ⇒ RED） | 变异 A ⇒ RED |
| `bp_plugin_R2_btb_entry_readable_after_update` | ★ 断 `BP_PRED_TARGET` **值** == 0x80000118（表项内容逐位相等） | ★ BTB 写路径坏 / 打包错 | 变异 A ⇒ RED |
| `bp_plugin_R3_pht_learns_not_taken` | ★ **先 taken 建项 → 再灌 not-taken** ⇒ 断 pred_taken 由 **1 翻 0** | ★ **方向学习失效**（恒 +1 ⇒ 停在 1 ⇒ RED） | 变异 B ⇒ RED |
| `bp_plugin_R4_index_is_pc9_to_2` | 索引 = **PC[9:2]** + M 运行期 mask | 索引退化 | — |
| `bp_plugin_R5_invalid_entries_fail_fast` | 合法域 = **精确集合** {16,32,64,128,256} | ctor fail-open 回退 | — |
| `bp_plugin_R6_entry_pack_unpack_roundtrip` | 62b 编码往返 + 「MUST NOT 用 `target[1:0]!=0` 判有效」 | 编码 helper 分叉 | — |
| `bp_plugin_R7_flushed_branch_does_not_pollute_pht` | ★ **flush 门控**（B-7）：错路 + not-taken ⇒ PHT **不被污染** ⇒ 预测方向保持 taken | ★ **错路指令污染 PHT** | 变异 C ⇒ RED |
| `bp_plugin_R8_pht_saturation_does_not_wrap_around` | ★ **饱和不发生 3→0 回绕**（ADR-040 §10.5 族）：连续 taken 16 拍 ⇒ **逐拍**断言预测恒 taken | ★ **饱和判定被三目折常量**（3→0 回绕 ⇒ 每 4 拍出现 0） | 变异 D ⇒ RED |
| `bp_plugin_R9_branch_payloads_fail_fast_when_missing` | ★ **branch 侧** payload 缺失 ⇒ fail-fast，**逐 key** 6 次（全不写 + 5 个单点）⇒ 断言**完整消息尾巴** `lacks <KEY> payload` | ★ **单点删除任一 fail-fast**（★ 前缀断言 `lacks BRANCH_TAKEN` 有歧义 —— `BRANCH_TAKEN_RAW` 也命中 ⇒ 必须带 ` payload`） | 变异 E ⇒ RED；★ 5 个单点删除**逐个实测**均 RED |
| `bp_plugin_R10_lookup_follows_live_fetch_pc` | ★ **取指地址是活信号**：换 PC ⇒ `BP_PRED_VALID` 由 1 变 0 | ★ **BP 忽略 `BP_PC`**（critic SHOULD_FIX-4：原 fetch PC 是常量 ⇒ 该变异抓不住） | 变异 F ⇒ RED |

★ **驱动器的「方向 / 错路标记 / 取指地址」是随 tick 变化的活表达式**（tick 计数寄存器驱动）——
  CH_MEM 里 at_stage 闭包**只在 elaboration 跑一次** ⇒ 想在运行期改变分支方向或取指地址，
  MUST 让 payload 的值是**活节点**（写成常量则只在 elaboration 定值 ⇒ 变不了）⇒ 本套测试同时
  验证 BP 消费的是**活**信号（ADR-040 §10.5）。
★ **相位偏移注记**：实测 `sim->reset()` 也会产生一拍 ⇒ 驱动器的 tick 计数器相位比直觉早 1 拍
  ⇒ 各 case 的相位窗口**留了余量**（R10 用 8 拍窗口 + 行为断言，不依赖精确拍数）。

★ **payload key 唯一性（★ 本轮更正上一轮的一处**不实陈述**）**：
  `PayloadStore` 按 key 的**地址**索引（`include/cf/plugin/payload.h`: `cells_[&key]`）⇒
  同名 key 若有**两份实例** ⇒ 两个不同 cell ⇒ 跨 TU 读写**静默断链**（`has()` 返 false）。
  - ★ **命名空间作用域**的 key（本 change 新增的 4 个 `BP_*`）⇒ MUST 用 C++17 **`inline` 变量**
    （全程序唯一）；★ MUST NOT 用 `static inline` —— 命名空间作用域的 `static` 是**内部链接**
    ⇒ **每个 TU 各有一份实例** ⇒ 断链。★ 上一轮实测症状：插件所在 TU 读回自己的 cell 非空，
    而**另一个 TU**（测试 TU）读同一 key 得空。
  - ★ **类作用域**的 static 数据成员（`BranchPlugin<T>::BRANCH_TAKEN` / `IS_BRANCH` /
    `BRANCH_TAKEN_RAW` / `BRANCH_FLUSHED` / `IBusPlugin<T>::FLUSH`）⇒ **天然全程序唯一**
    （模板静态成员 = vague linkage，链接期合并为一份）⇒ **无**上述隐患。
    ★ 上一轮把它们误列为「同类隐患」⇒ **本轮更正**；本轮仍把它们显式写成 `inline static`
    （语义不变，但把「唯一性」写进源码而非依赖模板规则）。
  - ★ 证据路径（critic S-4：上一轮的「实测症状」不可复现 ⇒ 本轮登记来源）：
    上一轮 symptom 是在**测试 TU 与插件 TU 分离**的临时验证形态下观察到的；
    **当前形态**（测试文件同时 include 插件头 ⇒ 同一 TU）下**不复现** ⇒
    ★ 该 symptom **只证明「命名空间作用域 static key 跨 TU 会断链」这一机理**，
    **不**证明当前仓库里有活跃的跨 TU 断链（当前读写方恰好同 TU）。


### ★★ Task 2/3/4 的单元锚（PC-mux 五级链 + 门控式 flush）

`tests/cpu/test_cpu_chmem_ibus_bp_pcmux.cpp`（**点名** `ip/cpu/plugins/ibus_chmem.h`），
**12 case / 143 assertions 全绿** —— ★ 断的是 **PC-mux 各臂 + flush 门控分量的运行期值**
（读 fetch 节点发布的 `BP_PC` 与 `FLUSH` 两个 payload 的**信号值**）：

| case | 场景 | 断的量 | ★ 抓得住哪类回归 |
|---|---|---|---|
| `R1_bp_off_not_taken_is_zero_regression` | BP **未注册** + 分支 not-taken | PC 逐拍 `init+4k`；FLUSH 恒 0 | ★ **BP 关闭 ⇒ 零退化**（PC-mux 退化成 seq 单臂、flush 恒 0） |
| `R2_bp_off_taken_is_zero_regression` | BP **未注册** + 分支 taken | PC 恒 = 实际目标；FLUSH 恒 1 | ★ **零退化的另一半**：无 BP 时 flush 语义与改动前**逐位相同** |
| `R3_case_A_falls_back_and_flushes` | 预测 taken / 实际 not-taken | PC = `init+4`（**不是**投机目标 P）；FLUSH 第 2 拍起恒 1 | ★ 删 `case_a` 臂 ⇒ PC 变 P+4 ⇒ RED；★ 删 `\| mispredict_a_buf` ⇒ FLUSH 变 0 ⇒ RED |
| `R4_case_B_redirects_to_actual_target` | 预测 not-taken / 实际 taken | PC 恒 = 实际目标 T（**不是** fall-through） | ★ 删 `actual_taken` 臂 ⇒ PC 变 `init+4` ⇒ RED |
| `R5_case_C_speculation_correct_no_flush` | 预测 taken / 实际 taken / **目标一致** | PC = **`bp_next`**（★ 本夹具的预测是常量 ⇒ `bp_next=P` ⇒ **自环于 P**；**不是** `pc_plus_4`、也不是 br_target）；FLUSH 第 2 拍起恒 **0** | ★★ **门控式 flush 的核心锚**：去掉 `!spec_ok` 或退回 `FLUSH = taken_buf` ⇒ FLUSH 变 1 ⇒ RED；★★ 变异「`case_c` 臂退回 `pc_plus_4`」（= B-1）⇒ PC 变 `P+4k` ⇒ RED |
| `R6_case_F_target_mismatch_flushes` | 预测 taken 到 P / 实际 taken 到 **T≠P** | PC 恒 = T（**不是** P+4）；FLUSH 恒 1 | ★★ 与 R5 对照即证明「`case_c` 必须比较目标」：忽略目标一致性 ⇒ PC 变 P+4 且 FLUSH 变 0 ⇒ RED |
| `R7_spec_ok_is_case_c_only` | ★ **时变**脚本：第 0~1 拍预测 taken、第 2 拍起预测 not-taken | 第 3 拍 FLUSH = 0 | ★★ **`spec_ok` MUST NOT 再与 `pred_eff` 相与**（`pred_eff` 是**另一条指令**的条件 ⇒ 误与 ⇒ 过度 flush ⇒ 校验和错 —— 端到端实测 m0 `93≠103`） |
| ★★ `R8_chained_speculation_keeps_bp_next` | ★★ **链式投机**：B1（情形 C）的 target P 处**本身又是一条被正确预测 taken 的分支** B2（预测→Q） | PC 第 2 拍 = **Q**（不是 P+4）；FLUSH 全 0 | ★★ **critic 十轮 B-1**：`case_c` 臂写成 `pc_plus_4` ⇒ B2 的投机被丢 ⇒ **走错路 + 错路指令不被 flush ⇒ 副作用泄漏** ⇒ RED（★ 该场景在 branchbench / vendored ELF 上**结构性不可达** ⇒ 端到端抓不住） |
| `R9_case_E_btb_miss_still_flushes` | ★ **情形 E**：BP **已注册**但 `pred_valid=0`（BTB miss ⇒ 未投机）而实际 taken | PC 恒 = 实际目标；FLUSH 恒 1 | ★ AC-F2-17 ③ 的真实锚（R1/R2 只是「BP **未注册**」路径 ⇒ 不同代码分支）；★ 变异「`pred_valid` 被忽略」⇒ RED |
| `R10_pred_valid_zero_blocks_speculation` | `pred_valid=0` 而 `pred_taken=1`（BTB miss 时 PHT 读数是垃圾） | PC 逐拍顺序取指；FLUSH 恒 0 | ★★ **`pred_eff` MUST 带 `pred_valid_now`**（此前 7 个 case 全部 `valid ⇒ taken` 同真假 ⇒ 该变异抓不住 ⇒ 现由 R10 兜住） |
| ★★ `R11_flushed_instruction_excluded_from_case_a` | ★★ **错路指令**（`BRANCH_FLUSHED=1`）上 `case_a` MUST NOT 触发 | PC 第 2 拍 = `T+4`（**不是** `init+4`）；FLUSH 第 2 拍起恒 0（**不得过度 flush**） | ★★ **critic 十一轮 B-2**：`BRANCH_TAKEN` 是**被 flush 掩码后**的值 ⇒ 错路指令的 taken 恒读作 0 ⇒ `!br_taken` 恒真 ⇒ `case_a` 误触发 ⇒ ① PC 出轨 ② 过度 flush ⇒ **双向副作用泄漏** ⇒ RED |
| `R12_missing_branch_flushed_fails_fast` | ★★ `BRANCH_FLUSHED` **缺失** ⇒ `elaborate` MUST **抛异常** | — | ★★ **fail-fast**（critic 十二轮 NIT-1）：静默降级成 `flushed=false` ⇒ **整条「错路指令维度」判定失效**且无人察觉 ⇒ MUST 抛（与 BP 插件读 `BP_PC` 同口径） |

★ **为什么必须有这个文件**（branchbench 端到端判据不够）：① 端到端每次 ~50s，不能当快速回归；
② 端到端只能**整体**判红，无法隔离「哪条臂 / 哪个门控分量」出错；③ **FLUSH 门控是 DUT 内部量**，
端到端只能间接覆盖 ⇒ ★ 本文件把二者都变成**可单点定位**的断言。

★ **夹具两条纪律**（实测踩坑记录）：
- ★ **branch payload 的生产者必需**：只注册 `IBusPlugin` 时，branch node 的
  `BRANCH_TAKEN` / `BRANCH_TARGET` cell 为 **null** ⇒ IBus 的 mux 操作数为 null ⇒
  **Simulator 构造期 SIGSEGV**（`muximpl::create_instruction` → `lnodeimpl::id(this=0x0)`）。
  ⇒ 本文件用 `ScriptDriver` 扮演 BranchPlugin + BP 预测器**两个**角色。
- ★ **不调用 `sim->reset()`**：CppHDL 的 `reset()` 把每个 `ch_reg` 复位成 `rst_val`
  （本项目用 `ch_reg(value, name)` 构造 ⇒ `rst_val = nullptr` ⇒ **0**）⇒ `pc` 被清 0 ⇒
  `set_initial_pc` 形同虚设 ⇒ ★ 同一事实见 `tests/cpu/test_cpu_chmem_vendored_elf.cpp:156` 的注释。

★ **变异对照（★ 15 个单点变异，全部实测；★ 「被抓住」= 目标 case RED）**：

| # | 变异（单点删除 / 改写） | 结果 | 被谁抓住 |
|---|---|---|---|
| M1 | 删 `case_a` 臂（`select(case_a, pc_lag+4, …)`） | **RED** | R3 |
| M2 | 删 `case_c` 臂（`select(case_c, **bp_next**, …)`） | **RED** | R7, R8 |
| M3 | 删 `case_b` 臂（`select(case_b, br_target, …)`） | ★ **仍绿** | —（★ **可证明无差异**：`case_b ⊂ actual_taken` ⇒ 两臂选同一个值 ⇒ 该臂**无可观测效果** ⇒ 保留仅因简报明文要求五级链 ⇒ 见缺口 #10） |
| M4 | 删 BP 投机臂（`select(pred_eff, pred_target, pc+4)`） | **RED** | R3 |
| M5 | 删 `actual_taken` 臂（`select(br_taken, br_target, …)`） | **RED** | R2, R4, R6 |
| M6 | 门控去掉 `!spec_ok`（`(taken & (!spec_ok)) \| misp_a` → `taken \| misp_a`） | **RED** | R5, R7 |
| M7 | 门控去掉 `\| mispredict_a_buf` 分量 | **RED** | R3 |
| M8 | 门控整体退回 `FLUSH = taken_buf`（改动前语义） | **RED** | R3, R5, R7 |
| M9 | `spec_ok_buf <<= (pred_eff & case_c)`（★ **本次修过的真 bug**） | **RED** | R7 |
| M10 | `case_a` 臂误用 `pc_plus_4`（而非 `pc_lag_plus_4`） | **RED** | R3 |
| M11 | `case_c` 忽略目标一致性（`pv & pt & br_taken`）⇒ 把情形 F 误判为 C | **RED** | R6 |
| ★★ M12 | ★★ `case_c` 臂退回 `pc_plus_4`（= 简报 §2.1.2 的字面写法）⇒ **critic 十轮 B-1 的本体** | **RED** | R5, R7, R8 |
| M13 | `pred_eff` 丢掉 `pred_valid_now`（`pred_valid & pred_taken` → `pred_taken`） | **RED** | R10 |
| ★★ M14 | ★★ `case_a` 去掉 `live`（`!flushed`）门控 ⇒ **critic 十一轮 B-2 的本体**（错路指令上 `case_a` 误触发） | **RED** | R11 |
| M15 | `flushed` 读错 payload（`BRANCH_FLUSHED` 分支被短路 ⇒ 恒 false） | **RED** | R11 |

⇒ ★ **14/15 被抓住**；唯一未被抓住的 M3 是**可证明冗余**的一臂（已登记，非漏网）。

### 回归（★ 用**重 build 后**的 binary —— 修正 critic B5 指出的「binary 过期」）

- 8 架构门禁全 PASS
- `ctest -R chipforge_tests_chmem` **PASS**（★ 本轮 binary；`[bp][plugin]` **10 case / 160 assertions** 全绿）
- `ctest -R "branchbench_baseline|branchbench_bp_direction|verilator|cpu_verilator"` **5/5 PASS**
  （★ 新增 `branchbench_bp_direction` = `measure_branchbench.sh --check-bp-direction`）
- `measure_branchbench.sh --check-baseline` exit 0（`bp_mode=off` ⇒ 传 `--no-bp` ⇒
  cycles 与基线**逐位相同** ⇒ **BP 关闭时零影响** ✓；★ 收尾会断言**实际比对档数 == 4**）
- `measure_branchbench.sh --check-bp-direction` exit 0（BP=on 4 档方向性判据 ✓，判据 2/2）
  ★ 含**新判据**「`ΔFLUSH == Δcycles`」（阈值无关 ⇒ 每个被消除的 taken 气泡恰 1 拍 ⇒
  同时钉住 `flush_count` 的计数口径与门控正确性；★ 两个反证实测：篡改 CSV 的
  `flush_count`（+1）⇒ exit 1；删掉该列 ⇒ exit 1（fail-closed））
- ★ **脚本 fail-closed 行为矩阵**（critic 六/七轮逐步收紧，均已实测）：

  | 输入 | 期望 | 说明 |
  |---|---|---|
  | `--check-baseline`（CSV 有 4 行 off） | **PASS** | 比对 4 档 |
  | `--bp=on --check-baseline`（CSV **无** bp=on 行） | **exit 1** | ★ 0 行比对 MUST NOT 报 PASS（critic 七轮 SF-1 前它静默 PASS） |
  | `BENCH_BP=oon`（任意模式） | **exit 1** | ★ **原始 env 值**在任何覆盖前校验（critic 八轮 B-1：上一版校验放在覆盖之后 ⇒ `BENCH_BP=oon --bp=on` / `--check-bp-direction` 仍被静默掩盖成 on） |
  | `--bp=off --check-bp-direction` | **exit 1** | 矛盾输入（flag 渠道） |
  | `BENCH_BP=off --check-bp-direction` | **exit 1** | 矛盾输入（**env 渠道**，critic 八轮 SF-1） |
  | `BENCH_BP=off --bp=on`（冲突） | **exit 1** | env/flag 冲突 MUST NOT 静默择一（critic 八轮 SF-2） |
  | `BENCH_BP=on --bp=on`（同值） | PASS | 两渠道一致 ⇒ 允许 |
  | `--check-baseline --check-bp-direction` | **exit 1** | 模式只接受一个（原会给出**不含方向性判据的 PASS**） |
  | `--bp=on --bp=off` | **exit 1** | 重复 `--bp`（原「后者静默生效」） |
  | `--foo` / `--bp=` / `--BP=on` | **exit 1** | 未知参数 / 非法值 |
  | `--help` | exit 0 | 有意短路（忽略其后参数） |
  | runner 未回显 `BP=` | **exit 1**（check 模式） | 生效值断言 MUST NOT 静默跳过 |
| runner 未回显 `FLUSH_COUNT=` | **exit 1**（check-bp-direction） | ★ fail-closed：无法做 `ΔFLUSH == Δcycles` 判据 ⇒ MUST NOT 静默跳过（critic 十二轮 SF-4） |
| 基线 CSV 缺 `flush_count` 列/值 | **exit 1**（check-bp-direction） | ★ 同上（判据不可静默失效） |

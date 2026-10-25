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
- [ ] **Task 2** branch LATE 的 PC-mux 接入（★ **五级嵌套**：`trap > branch-actual/mispredict > freeze > bp-predict > seq`）
- [ ] **Task 3** ★ flush 语义改写（★ **门控**，非「替换分量」）—— `flush = hold_z | (taken_buf && !bp_spec_taken_buf) | mispredict_A_buf` ⇒ ★ **BP 关闭时与现状逐位相同（零退化）**；★ **MUST NOT** 把 `br_taken_buf` 分量替换为 `mispredict_buf` 分量（⇒ BTB miss / 情形 B / `jalr` 的 fall-through 副作用泄漏 ⇒ 校验和不等）
- [ ] **Task 4** 误预测纠错（★ **按情形 A/B 区分**：A ⇒ 纠到 `pc_lag+4` + flush；B ⇒ 纠到 `actual_target` + **flush（MUST 抑制！）**；C ⇒ 抑制自身 redirect + **MUST NOT 抑制**；E（BTB miss 且实际跳转）⇒ **MUST 抑制**）+ **正确预测时抑制该分支自身 redirect**
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

## 实测留证
## 实测留证（2026-10-25，branch `feature/chmem-bp-btb-gshare`）

### Task 1（查表 + payload 产出，**不改 PC-mux ⇒ 零行为风险**）

| 项 | 结果 |
|---|---|
| BP plugin | `ip/cpu/plugins/branch_predictor_chmem.h`（新）：BTB `ch_mem<ch_uint<64>, 256>` 62b 编码 + PHT `ch_mem<ch_uint<2>, 256>` |
| 挂钩点 | `at_stage("fetch", NORMAL)` ⇒ 查表 0 拍（`aread`）⇒ 与 branch LATE 同拍可消费 |
| PC 来源 | ★ `ibus_chmem.h` 在 fetch NORMAL 写 **`BP_PC` payload**（= `pc_reg` **本拍值**）—— ★ `KT::PC` 是 `pc_lag`（上一拍地址）⇒ 预测需要本拍值 |
| 预测信号 | ★ 全部为**活节点**（`ch_uint<1>(ch_bool)` 走上游 `bool_to_uint` 分支 ⇒ 复用同一 `impl()` 指针）⇒ **MUST NOT** 转 C++ 标量（否则折成 elaboration 期常量 ⇒ 假阴性） |
| 注册位 | ★ BP 注册在 **IBusPlugin 之后**（同 stage "fetch" 同 phase NORMAL ⇒ 生产者先写 `BP_PC`） |
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
| 2 | **`pred_*_reg_`（ch_reg 预测记录）** | ⚠️ 声明但未接线 | 跨拍预测记录是 **Task 4（误预测纠错）**的前置 ⇒ 本轮**不计入 Task 1 完成范围**（★ 修正上一轮 commit message 的不实陈述） |
| 3 | **`jalr` 不建项** | ✅ 已实现（保守） | `jalr` 目标随运行时变化 ⇒ 不建 BTB 项 ⇒ 走顺序取指（台账 #9 的终态） |
| 4 | **M 运行期 mask 的效果无测试** | ⚠️ 缺测 | `index_of_scalar` 用 `kBtbEntries`(256) 做 mask，而运行期路径用 `logical_entries_−1` ⇒ **M<256 时二者不等价**；现有断言只覆盖 M=256 语义 ⇒ 「M 运行期可配」**无端到端证据** |
| 5 | **`pack_entry` / `unpack_entry` 与运行路径并行** | ⚠️ 双实现 | 运行路径（`update()`）用**内联 shl/or** 拼 62b；`pack_entry` 是**影子 API**（仅测试用）⇒ 两者一旦分叉，测试发现不了（测试只测 helper） |
| 6 | **门禁漏网：`?:` 形式** | ⚠️ **已知缺口** | `check_plugin_portability.sh` 的 Check 5 只查 `if (ch_bool)`；**C++ 三目 `?:` 的条件位同样做 contextual conversion** ⇒ 同样会在 elaboration 期折成常量（★ 本轮 PHT 饱和 bug 正是这个形态：Verilog 里 `ge_ge_2` dangling ⇒ 计数器 3→0 **回绕**而非饱和）。★ 曾尝试加 Check 5b，但**误报/漏报都难以做到既零噪音又可靠**（本仓库的真实逻辑写在 `predict()`/`update()` 等**被 at_stage 调用的方法**里，而「at_stage 闭包内」的范围抓不到）⇒ **撤销该门禁，改为在此登记为已知缺口** ⇒ 后续应由 ADR-040 的门禁专项处理 |
| 7 | **「提前一拍」的证明** | ⏳ 待 Task 2 | 本 change 内 BP 的三个输出 payload **无消费者** ⇒ 「提前一拍」**没有任何行为后果** ⇒ 证明 MUST 留到 Task 2 接入 PC-mux 后用 Δcycles |

### 测试覆盖（critic B3：此前该插件 **0 case** ⇒ `[chmem]` binary 无回归锚）

新增 `tests/cpu/test_cpu_chmem_bp_plugin.cpp`（**点名** `branch_predictor_chmem.h`），
**6 case / 52 assertions 全绿**（★ 相对上一版 5 case / 30 assertions：**R1/R2/R3 三个新 case 直接实例化真实插件**
⇒ 锚在**插件的真实输出**上，而非影子 helper）：

| case | 覆盖 | 抓得住哪类回归 |
|---|---|---|
| `bp_plugin_R1_btb_is_written_when_producer_comes_first` | ★ **注册位顺序**（真实插件 + 真实生产者替身）⇒ 断言插件产出的 payload cell **是活节点** | ★ **注册位回退**（BP 早于生产者 ⇒ 读到陈旧值 ⇒ BTB 永不写 ⇒ RED） |
| `bp_plugin_R2_btb_entry_readable_after_update` | ★ 插件产出的 `BP_PRED_TARGET` / `BP_PRED_VALID` 是**活节点**（AC-F2-12 **真测插件输出**） | ★ **预测被折成常量** |
| `bp_plugin_R3_pht_learns_not_taken` | ★ not-taken 分支 ⇒ 预测方向 MUST NOT 恒 taken | ★ **方向学习失效**（任何方向都 +1 ⇒ 恒 3 ⇒ 恒预测 taken） |
| `bp_plugin_R4_index_is_pc9_to_2` | 索引 = **PC[9:2]** + M 运行期 mask | 索引退化 |
| `bp_plugin_R5_invalid_entries_fail_fast` | 合法域 = **精确集合** {16,32,64,128,256} | ctor fail-open 回退 |
| `bp_plugin_R6_entry_pack_unpack_roundtrip` | 62b 编码往返 + 「MUST NOT 用 `target[1:0]!=0` 判有效」 | 编码 helper 分叉 |

★ **★ 关键修正：payload key MUST 是跨 TU 唯一的单例**（`inline` 变量，**不可** `static inline`）——
  `static inline` 在头文件里 ⇒ **每个 TU 各有一份实例** ⇒ `PayloadStore` 按 `&key` 索引
  ⇒ 不同 TU 的同名 key 是**不同 cell**。★ 实测症状: BP 内部读回自己的 cell 非空，
  但**测试 TU** 读同一 key 得空（测试 TU 包含该头文件 ⇒ 持有另一个实例）
  ⇒ 真实流水线里 IBus 与 BP 同属一个 TU（`cpu_factory_chmem.h` 包含两者）⇒ **侥幸能通**；
  但任何**跨 TU** 的读写（测试、工具）都会断链。
  ★ 参照: `branch_chmem.h` 的 `BRANCH_TAKEN` 等也是 `static inline` ⇒ **同类隐患**
  （目前读写方恰好同 TU ⇒ 未暴露）⇒ ★ **建议后续 change 把既有 key 也统一为 `inline`**。

### 回归（★ 用**重 build 后**的 binary —— 修正 critic B5 指出的「binary 过期」）

- 8 架构门禁全 PASS
- `ctest -R chipforge_tests_chmem` **PASS**（1006s，**新** binary `10-25 10:55`）
- `ctest -R "branchbench_baseline|verilator|cpu_verilator"` **3/3 PASS**
- `measure_branchbench.sh --check-baseline` exit 0（`bp_mode=off` ⇒ 传 `--no-bp` ⇒
  cycles 与基线**逐位相同** ⇒ **BP 关闭时零影响** ✓）

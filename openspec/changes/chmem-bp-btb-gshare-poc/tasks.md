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

- [ ] **Task 1** 最小 BTB（BTB + Bimodal 2-bit PHT）+ fetch NORMAL 的 **0 拍**查表 + payload 输出（`pred_target` / `pred_taken` / `pred_valid`）
- [ ] **Task 2** branch LATE 的 PC-mux 接入（★ **五级嵌套**：`trap > branch-actual/mispredict > freeze > bp-predict > seq`）
- [ ] **Task 3** ★ flush 语义改写（★ **门控**，非「替换分量」）—— `flush = hold_z | (taken_buf && !bp_spec_taken_buf) | mispredict_A_buf` ⇒ ★ **BP 关闭时与现状逐位相同（零退化）**；★ **MUST NOT** 把 `br_taken_buf` 分量替换为 `mispredict_buf` 分量（⇒ BTB miss / 情形 B / `jalr` 的 fall-through 副作用泄漏 ⇒ 校验和不等）
- [ ] **Task 4** 误预测纠错（★ **按情形 A/B 区分**：A ⇒ 纠到 `pc_lag+4` + flush；B ⇒ 纠到 `actual_target` + **flush（MUST 抑制！）**；C ⇒ 抑制自身 redirect + **MUST NOT 抑制**；E（BTB miss 且实际跳转）⇒ **MUST 抑制**）+ **正确预测时抑制该分支自身 redirect**
- [ ] **Task 5** 测量设施（★ **FLUSH 事件数计数器归本 change** —— FLUSH 是 DUT 内部 payload，**host 不可见** ⇒ 需改 `ip/cpu/plugins/ibus_chmem.h` ⇒ **本 change 是首个可改 `ip/` 的 change**）+ ★ **MUST 复用**前置1 的 `--stats` 框架（**MUST NOT** 另起一套）+ BP 开关面（工厂参数 + CLI flag）

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
- [ ] **AC-F2-13** ★ **计数器自证** —— BP 关态下 `FLUSH 事件数 == PC 跳变法测得的 taken 转移数` ⇒ 防「计数器恒 0 ⇒ 『显著下降』真空成立」的**假阳性**
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
| `ip/cpu/plugins/ibus_chmem.h`（`update_pc` 的 mux + flush） | ✗ | 前置2 **首次改**（5 臂 mux + FLUSH 门控 + FLUSH 计数）；主体 **二次改**（补完整语义 + 过时注释 #8） | ★ **同函数两次改** ⇒ 前置2 的版本 MUST 可被主体**增量扩展**（MUST NOT 要求主体重写）⇒ ★ 主体 MUST 继承前置2 的**五臂 select 链**结构 |
| `ip/cpu/cpu_factory_chmem.h`（注册 + 开关 + 参数转发） | ✗ | 前置2 **首次注册**（BP 开关）；主体 **补全**（4 参数转发修复 #9 + 完整注册） | ★ 同上 ⇒ 增量扩展 |
| `docs/architecture/adr/ADR-088-*.md` | ✗ | 前置2 写 **§5.3.2 的「机制结论」段**；主体 写 **「绝对值门禁结论」段** | ★ **主体 MUST NOT 覆盖前置2 的机制结论段**（否则「机制不成立」的结论会被主体的「达标」结论抹掉） |
| `docs/architecture/tlm-coverage-parity-ledger.md` | ★ **前置1 MUST NOT 触碰** | 前置2 MUST NOT 触碰；主体 owner | ★ 台账重判只归主体 |
| `ip/cpu/plugins/branch_predictor.{h,cpp}`（TLM 侧） | — | — | ★ **全程保留**（主体 AC-11） |

★ **本 change 的角色**：前置2（机制 PoC）⇒ 本表列出的「前置2」项**归本 change**（含 `branch_predictor_chmem.h` 创建最小版、`ibus_chmem.h` 首次改、`cpu_factory_chmem.h` 首次注册、FLUSH 计数）；★ **MUST NOT** 触碰「前置1 owner」项与「主体」项。
## ★ 兜底句纪律

任何「上报 architect / 待裁决」句 MUST 同句给出 **① 触发阈值 ② 落点文件 ③ 可判定时限**；MUST NOT 以「等待回复」结束本 change。

## 实测留证

（实施后填写）


## ★ builder 开工须知（★ **逐条抄进本 change**，不跨 change 引用）

| # | 须知 | 归属 |
|---|------|------|
| 1 | ★ 修正 `ibus_chmem.h:83-87` 的**过时注释**（「aread/sread 都是 1-cycle」与 aread 0-cycle 的**承重前提**直接矛盾） | ★ 本 change（首个可改 `ip/` 的 change） |
| 2 | ★ **MUST 复用**前置1 的 `--stats` 框架（**MUST NOT** 另起一套计数器） | ★ 本 change |
| 3 | ★ 本 change **MUST NOT** 触碰台账 `docs/architecture/tlm-coverage-parity-ledger.md`（台账重判归 ②） | ★ 本 change |
| 4 | ★ 本 change **MUST NOT** 触碰 `tools/measure_branchbench.sh` 的**模式名数组与表头解析逻辑**（前置1 的范围）⇒ 本 change **仅追加** BP flag（`--bp=on\|off`）与生效值断言相关行 | ★ 本 change |
| 5 | ★ `measure_branchbench.sh` 的 `2>/dev/null` MUST 改为**捕获 stderr** ⇒ 否则「flag 未生效却测出无差异」的**假绿通道**是开着的 | ★ 本 change |
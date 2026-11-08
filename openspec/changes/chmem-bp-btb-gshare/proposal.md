---
initiative: wave6-linux-and-productization
priority: P1
version_target: v1.0.0
status: ready
created_from: ip/cpu/docs/bp-btb-gshare-design-brief.md
created_from_version: v2.2
adr:
  - ADR-088
  - ADR-040
depends_on:
  - chmem-branch-microbench-fix
  - chmem-bp-btb-gshare-poc
---

# chmem-bp-btb-gshare

> ⚠️ **本 change 的两个前置必须先完成**（架构简报 §4 的裁决）：① `chmem-branch-microbench-fix`（修 0c 微基准的工作量污染 + 档名反向）② `chmem-bp-btb-gshare-poc`（验证「提前一拍预测 ⇒ 消除气泡」机制）。
> ★ **前置未完成前，本 change 的实现 MUST NOT 开工**（判据地基与机制前提都依赖它们）。

## Why

**ADR-088 v1.2 §3 的 BP 行**：CH_MEM 侧必须实装分支预测器（**CH_MEM-only 且必须做；不适用 §5.2 的「不做」出口**）。

### ★ 根因（architect 判决实验，2026-10-24 —— 已**修正** ADR-088 §5.3.1 的表述）

| 实验 | 结果 |
|------|------|
| **E-2**（摘掉 TLM 的 BP 注册） | cycles **794478 → 794478（逐位不变）** ⇒ ★ **TLM 的 BP 是惰性桩**（`branch_predictor.h:126-129` 预测结果被丢弃、`:149-151` flush 恒 false、`ibus.h` 0 处引用）⇒ **「TLM 有 BTB ⇒ 预测命中不 flush」被证伪** |
| **E-3**（逐拍采样 CH_MEM 的 PC） | taken 控制转移 **120065 次**；`914544 − 120065 = 794479 ≈ TLM 794478`（残差 1）⇒ ★ **全部差距被「计数到的重定向」完全吸收** ⇒ 气泡 = **1 拍 / 每次 taken 转移** |
| **E-4**（读 TLM `ibus.h:114-124`） | TLM 是**单遍 tick**（writeback LATE 同 tick 内重定向）⇒ **重定向零延迟** |

⇒ ★ **真根因 = TLM 单遍模型重定向零代价 vs CH_MEM 两级取指每条 taken 转移 1 拍气泡**。
⇒ ★ **BP 若能提前一拍预测 ⇒ 消除这 120065 拍** ⇒ CH_MEM 的绝对值门禁（DMIPS ≥ 1.4）可恢复（ADR-088 §5.3.2 **P-6**）。

### ★ 绝对值门禁的**定量门槛**（决定 ② 的范围）

| 项 | 数值 |
|---|------|
| DMIPS ≥ 1.4 需要的 cycles | ≤ 813083（CH_MEM 现状 914544） |
| **必须净省** | **≥ 101461 cycles** |
| 可消除的气泡总量 | 120065 |
| ⇒ **净命中率门槛** | **≥ 84.5%** |

⇒ ★ 「GShare 延后」**不是低风险决定** —— 它把 1.4 门禁押在「**Bimodal（无全局历史）能否在 Dhrystone 的分支混合上达到 84.5% 净收益率**」这个**未验证**的假设上。
⇒ ★ **裁决 C-2（范围）须由前置2 的实测数据触发复核**：若前置2 报告的**可达净收益率下界 < 84.5%** ⇒ **不得以「GShare 延后」结案** ⇒ 须回 ② 补 GShare（或另立 change）。

#### ★★ oracle 复审发现 2：**`jal` 的方向无来源 ⇒ call 气泡可能不可恢复**（architect 待追认项）

| 项 | 内容 |
|---|---|
| **问题** | `jal`（无条件跳转）**不占 PHT 表项**（简报 §3.3）⇒ 其 PHT 项恒 0 ⇒ 若 `pred_taken = hit && (pht ≥ 2)` ⇒ ★ **`jal` 永不被预测** ⇒ call 气泡不可恢复 |
| **叠加** | `jalr`（含 `ret`）**不建 BTB 项**（间接跳转目标每次不同）⇒ 同样不可预测 ⇒ Dhrystone 的 call/ret 气泡**双重不可恢复** |
| **对 84.5% 门槛的影响** | 若 call+ret 占 120065 次 taken 转移的比例 **> 15.5%** ⇒ ★ **84.5% 门槛在构造上不可达** ⇒ 「GShare 延后」结案不成立 |
| **★ 已采纳的处置** | BTB 表项**增加 `is_unconditional` 位** ⇒ `pred_taken = hit && (is_unconditional \|\| pht ≥ 2)` ⇒ `jal` **可预测**（`jalr` 仍不可预测，因其目标本质上运行时决定） |
| **★ 待 architect 追认** | ① `is_unconditional` 位是否采纳（本 change 已按采纳设计，**需 architect 追认**）② 前置2 **MUST 报告** `jal` / `jalr` / taken 转移数的比例 ⇒ 若 `jalr` 占比使 84.5% 构造上不可达 ⇒ architect 须重裁绝对值门禁路径（**不得**由实施者自裁） |
| 落点 | 本 proposal + 主体 `design.md` §4.1 + 主体 `tasks.md` **AC-17** + 前置2 `tasks.md` **AC-F2-15** |

## What Changes

### In Scope（7 项）

1. **`ip/cpu/plugins/branch_predictor_chmem.h`（新）** —— **BTB（256×64b）+ Bimodal 2-bit PHT（256×2b）**
2. **BTB 布局** —— ★ **62b 打包**：`{tag30 = PC[31:2], target30 = target[31:2], valid(1b), is_unconditional(1b)}`（余 2 bit）⇒ 依据：**CH_MEM 只解 32 位指令**（`decode_chmem.h:82` 的 `instr_format` 无 16 位格式）⇒ `PC[1:0]=0`、target 4 字节对齐 ⇒ 低位恒 0 ⇒ 可安全裁掉。★ **`is_unconditional` 位是必需的**（见下「oracle 复审发现 2」）
3. **逻辑条目数 M** —— 物理 256 固定（编译期）+ **运行期 mask** `& literal(M-1)`，M ∈ {16,32,64,128,256}
4. **fetch NORMAL 预测 + branch LATE 接入** —— ★ **五级 mux 嵌套**（`trap > branch-actual/mispredict > freeze > bp-predict > seq`）
5. **flush 判据改写（★ 门控，非「替换分量」）** —— `flush = hold_z | (taken_buf && !bp_spec_taken_buf) | mispredict_A_buf` ⇒ ★ **BP 关闭时与现状逐位相同（零退化）** ⇒ ★ **MUST NOT** 把 `br_taken_buf` 分量「替换」为 `mispredict_buf` 分量（⇒ BTB miss / 情形 B / `jalr` 的 fall-through 副作用**泄漏** ⇒ 校验和不等 ⇒ AC-7/AC-8a RED）
6. **BP 开关面** —— 工厂参数（普通 `bool enable_bp = true` + `optional<uint32_t> btb_entries`）+ CLI `--no-bp` / `--btb-entries N`（★ **不是** `CPUConfig` 字段 —— CH_MEM 工厂不接它）
7. **台账 14 条重判 + ADR-088 §5.3.1 根因修订**（★ 修订**纳入本 change 的归档范围**）

### Out of Scope（6 项）

- ❌ **GShare 全局历史**（13-bit GHR）—— 延后（相对 Bimodal 的增益待前置2 数据）
- ❌ **RAS call/ret 预测** —— 延后（与 BTB 正交，可独立立项）
- ❌ **`jalr` 建 BTB 项** —— ★ 明确**不建**（间接跳转目标随运行时变化 ⇒ 静态缓存必错）
- ❌ **多级 TLB / PTW** —— 与本 change 无关
- ❌ **改 CH_MEM 的取指延迟**（imem 改 `aread`）—— 刻意保持 `pc_lag` 对齐的既有决策
- ❌ **改判据**（DMIPS / CoreMark 阈值）—— ★ 需显式 ADR，**不属本 change**

## Capabilities

- **CH_MEM 侧有 BP 结构且已注册**（ADR-088 §3 BP 行的门禁载体）
- **BP 收益可机械判定**（微基准四档 + 净收益模型）
- **绝对值门禁的恢复路径明确**（净命中率门槛 84.5% 已量化）

## Impact

- **代码**：`ip/cpu/plugins/branch_predictor_chmem.h`（新）· `ip/cpu/plugins/ibus_chmem.h`（PC-mux + flush）· `ip/cpu/cpu_factory_chmem.h`（注册 + 开关）
- **工具**：`tools/verilator_runner/` · `tools/measure_branchbench.sh`
- **数据**：`branchbench-baseline-matrix.csv` 扩充（`bp_mode` / `workload_sha` / `build_id` 列）
- **台账**：14 条重判 + §2 族级 + §7 裁决行 + 计数同步
- **ADR**：`ADR-088 §5.3.1` 根因修订（★ **必须** —— 现有表述已被 E-1/E-2 证伪）
- ★ **B3**：本 change **不解除** B3 的 BP 阻塞（双前置 ②「CoreMark harness 产出实测值」属**交付点 B**）

## Acceptance Criteria

> ★ 2026-11-08 迁移（change `architecture-truth-boundary-enforcement` AC-120①）：本段原为指针
> （「见 tasks.md 的 AC-1 … AC-25」）⇒ verifier 提取为空。现**移入正文**（单一副本），
> 逐条对应架构简报 v2.2 §4.3；AC-16/19/20/21/22/23/24/25 为 v2.2 新增。

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

## ★ per-file owner 表

见 **本 change 的 `tasks.md` §「per-file × per-symbol owner 表」**（★ 已逐条抄进三个 change 的 tasks.md ⇒ 跨 change 引用会被漏读 ⇒ 不引用外部文档）⇒ ★ **三个 change 同改同一批文件时的归属**（尤其 `branch_predictor_chmem.h` / `ibus_chmem.h` / `cpu_factory_chmem.h` / `tools/verilator_runner/*` / `measure_branchbench.sh`）⇒ ★ 前置2 的 PoC 版本 MUST 可被主体**增量扩展**（MUST NOT 要求主体重写）⇒ ★ PoC 残留的**删除/重写责任归主体**，且 `tests/cpu/test_cpu_chmem_bp_poc.cpp` 的 8 case **在主体后 MUST 仍绿**（机制锚）

## References

- 架构方案简报（权威）：`ip/cpu/docs/bp-btb-gshare-design-brief.md` v2.2
- 执行文件：`.rddf/plans/chmem-bp-btb-gshare.md`（★ 顶部有 **v2.2 覆盖声明**，逐条作废旧 Goal/Task/AC）
- 前置：`chmem-branch-microbench-fix` · `chmem-bp-btb-gshare-poc`
- 机制结论：`tests/cpu/test_cpu_chmem_bp_poc.cpp`

## 规格来源（spec preamble 的归档副本）

> Capability topic: `chmem-bp-btb-gshare`（CH_MEM 后端的分支预测器）。
> 上游 = ADR-088 v1.2 §3 BP 行 + 架构简报 `ip/cpu/docs/bp-btb-gshare-design-brief.md` v2.2。

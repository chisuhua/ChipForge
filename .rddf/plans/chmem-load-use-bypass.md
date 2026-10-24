# chmem-load-use-bypass — 执行文件（planner 起草，2026-10-24）

> **change**: `chmem-load-use-bypass`
> **来源**: ADR-088 **v1.2 §5.3**（性能门禁承接裁决，architect 2026-10-24）：P-1 不重校准判据 + P-2 新增本 change + P-3 临时口径 + P-4 修法不预设
> **规模**: 中 · **前置**: 无（**P-2a 选型 PoC 无前置，现在即可启动**）
> **被依赖**: ② `chmem-bp-btb-gshare`（性能验收）· ③ `chmem-l1cache-body` 阶段 B（Cache 命中路径同样需要 load-use 解析）· 交付点 B（CoreMark）
> **性质**: **能力补齐**（不是判据调整）—— 目标是把 CH_MEM 的 Dhrystone cycle 从 1.151×TLM 压到 ~1.03×TLM ⇒ **DMIPS ≥1.4 自然恢复**
>
> # ⛔ **本 plan 已作废（2026-10-24 实施期 PoC 证伪）—— 不立项**
>
> **证伪过程**：① 本 plan 的根因假设 = 「CH_MEM 侧 load-use 依赖全用 stall 气泡（`hazard_chmem.h:38` 自述『不实现数据前推 — 纯 stall 策略』）」② 实施期做了**决定性 PoC**：把 CH_MEM 侧 RAW stall 临时置为恒 false（`halt_when(result)` ⇒ `halt_when(ch_bool(false))`）后重建重测 ⇒ **cycles 完全不变（914544）** ⇒ **load-use 通路不是根因** ⇒ 本 change **不立项**。
>
> **真根因（ADR-088 v1.2 §5.3.1 已修订）**：比率随**分支密度**单调上升（无分支 `add` 1.037 / `addi` 1.034 · `jal` 1.078 · `beq` 1.098 · `bne` 1.105 · Dhrystone 1.151；delta 120066 ÷ 2000 iter = 60 拍/iter ≈ 30 分支 × 2 拍 flush 代价）⇒ **CH_MEM 无分支预测**（TLM 有 BTB ⇒ 预测命中不 flush）⇒ **与 B1 ② BP 同源** ⇒ 绝对值门禁的恢复条件 = **② 落地**（ADR-088 v1.2 §5.3.2 **P-6**）。
>
> **留证价值**：① 记录了「PoC 先行、不预设修法」这一流程**确实拦下一次基于错误根因的 change 立项**；② `hazard_chmem.h` 的**自比较伪冒险**（`ex/mem/wb` 的 rd 均来自 `populate_stage` 复制的同一份 `DECODED_INST` ⇒ 退化为「本指令读 rs 且写 rs」）是**独立于本次差距的真实代码异味**（它让本指令「读并写同一寄存器」插气泡，但**当前不产生 cycle 差异**——PoC 已证）⇒ 登记为**独立清理项**（不影响性能门禁），归 ③/④ 触及数据通路时顺带评估。
>
> **PoC 已撤销**：`ip/cpu/plugins/hazard_chmem.h` 已 `git checkout --` 复原（代码零残留）。

---

## Goal

让 **CH_MEM/Verilator 后端**正确处理 **load-use 依赖**（load 的结果被紧随其后的指令使用），消除当前「纯 stall 气泡」策略造成的 **15% 额外 cycle**。

## Architecture

**现状事实**（`ip/cpu/plugins/hazard_chmem.h:38` 自述）：

> 不实现数据前推 (forwarding) — 当前纯 stall 策略, 无旁路

CH_MEM 侧 RAW 检测把「上一拍写寄存器堆 / 本拍读」一律判为冒险 ⇒ 插入 stall 气泡。而 CH_MEM 是**全组合通路**（无 stage 寄存器）⇒ **load 数据同拍组合可得** ⇒ 多数 stall 气泡**本不需要**。

**两种候选修法（P-4 明确不预设，须 PoC 决定）**：

| 候选 | 做法 | 风险 |
|------|------|------|
| **A：精确化 RAW 检测** | 区分「组合路径同拍可解析的依赖」（load→use）**不 stall**，只对真正跨拍不可解析的依赖 stall | 需确认 DMem 读在同一拍内组合可得（`dmem_chmem.h` 的 read port 语义） |
| **B：真旁路（forwarding）** | ALU 操作数从 EX/MEM/WB 旁路取，不等寄存器堆 | **可能需要总线/数据通路重组** ⇒ 与 ④ `chmem-dbus-lsu` **潜在双写** ⇒ 须先定边界 |

⇒ **P-2a 的输出 = 候选 A/B 的实测对比 + 边界结论**（含「B 是否必须先做 ④ 阶段 A」）。

## Tech Stack

C++23 · CppHDL（`ch_bool` / `ch_reg` / payload key 模态）· Catch2 · ADR-040 v3.0 约束（§10.5 活节点 / §10.6 顺序契约）

---

## 依赖与边界（★与 ④ 的边界，planner 裁决）

| 事项 | owner |
|------|-------|
| RAW 检测的**判定条件**（哪些依赖可同拍解析） | **P-2**（本 change） |
| load-use 数据通路的**组织**（总线层 / 仲裁 / 宽度语义抽取） | **④** `chmem-dbus-lsu` |
| 若候选 B 需要**旁路数据通路**的重组 | **④ 阶段 A 先做**（否则 P-2 与 ④ 双写数据通路 ⇒ 双 owner） |

⇒ **执行顺序约束**：**P-2a（选型 PoC）→ 若结论 = 候选 B ⇒ ④ 阶段 A → P-2b（落地）；若结论 = 候选 A ⇒ P-2b 可直接落地（不依赖 ④）**。

---

## Tasks

### Task 1: 事实取证（不改代码）

- [ ] **Step 1: 读 RAW 检测实现** —— `ip/cpu/plugins/hazard_chmem.h` 的 `detect_raw_hazard` 闭包 ⇒ 记录它比较的三个来源（EX/MEM/WB 的 `rd` 与 `writes_rd`）与 **stall 注入点**（`halt_when` 接哪个 stage）
- [ ] **Step 2: 确认 DMem 读的同拍语义** —— `ip/cpu/plugins/dmem_chmem.h` 的 read port：CH_MEM 下 load 数据**是否在同一拍内组合可得**（这是候选 A 的成立前提）⇒ 读 port 是否为 async/组合
- [ ] **Step 3: 量化现状** —— 同一组 ELF（`rv32ui-p-add` / `-lw` / `-addi` + Dhrystone）在 TLM 与 CH_MEM 的 cycle 对照 ⇒ 确认「简单 ELF ~1.03 / Dhrystone 1.151」的形态（= ADR-088 §5.3.1 的基线）
- [ ] **Step 4: 确认无 owner 冲突** —— `grep -rn "load-use\|forwarding\|bypass" openspec/changes/ .rddf/roadmap/`（排除 archive）

### Task 2: P-2a 候选选型 PoC（★决策产出，**不改生产代码**）

- [ ] **Step 1: 候选 A 的最小 PoC** —— 在**测试侧**（或临时实验分支）构造「load → 紧随其 use」序列，验证「去掉该类 stall 后结果仍正确」⇒ 记录 cycle 降幅与正确性
- [ ] **Step 2: 候选 B 的可行性评估** —— 旁路需要哪些数据通路改造？是否必然触及 ④ 的范围（总线层）？⇒ 给出「B 是否必须等 ④ 阶段 A」的明确结论
- [ ] **Step 3: 对比结论** —— 给出：推荐候选 + 理由 + **预期 cycle 降幅**（目标：把 Dhrystone 的 1.151× 压到 ~1.03× ⇒ DMIPS 从 1.24467 回到 ≥1.4）
- [ ] **Step 4: 落点** —— 结论写入本 change 的 tasks + **ADR-088 §5.3.2 P-4 的「已决」标注**（architect 已授权本 change 落地该选型结论）

### Task 3: P-2b 落地（按 Task 2 的结论）

- [ ] **Step 1: 实现选定修法** —— 精确化 RAW 检测 **或** 旁路；**零 `ip/cpu/**` 之外的越界改动**
- [ ] **Step 2: 正确性回归** —— `[cpu]` / `[cpu-integration]` / `[riscv-tests]` / `[chmem]` **0 退化**；**新增**「load-use 依赖被正确解析」的 case（否则修法无锚点）
- [ ] **Step 3: 性能验证** —— 用 `tools/measure_perf.sh` 重测 Dhrystone ⇒ **cycles 下降**；写入新基线值
- [ ] **Step 4: 临时口径退出** —— 若达成 ~1.03× ⇒ **绝对值 1.4 判据恢复生效**（ADR-088 §5.3.2 P-3 的临时口径随之撤销）⇒ 基线 CSV notes 更新

### Task 4: 收口

- [ ] **Step 1: 8 门禁** + **Step 2: 全量回归 0 退化** + **Step 3: 基线 CSV 更新**（实测值 + notes 标注「P-2 落地后绝对值 1.4 恢复」）+ **Step 4: ADR-088 §5.3 状态更新** + **Step 5: commit**

---

## Acceptance

- [ ] AC-1: 现状量化基线入库（TLM vs CH_MEM 的 4 个 ELF 对照：add/lw/addi/Dhrystone），与 ADR-088 §5.3.1 一致
- [ ] AC-2: **候选 A / 候选 B 的 PoC 对比结论**（含「B 是否必须等 ④ 阶段 A」的明确判定）+ 预期 cycle 降幅
- [ ] AC-3: 选定修法落地后，**新增 case 证明 load-use 依赖被正确解析**（结果正确，非仅 cycle 下降）
- [ ] AC-4: Dhrystone 在 CH_MEM 的 **cycles 下降**（目标 ~1.03×TLM ⇒ DMIPS 回到 ≥1.4）；新基线值入库
- [ ] AC-5: `[cpu]` / `[cpu-integration]` / `[riscv-tests]` / `[chmem]` **0 退化** + 8 门禁全绿 + 全量回归 0 退化
- [ ] AC-6: **不重校准判据值**（1.4 原样）⇒ 达成后 ADR-088 §5.3.2 **P-3 临时口径撤销**（基线 CSV notes 同步）
- [ ] AC-7: 变更面不与 ④ 重叠（若结论 = 候选 B ⇒ 变更面限于依赖判定，**数据通路重组归 ④**）

## 风险与未知

| # | 风险 | 缓解 |
|---|------|------|
| R1 | 候选 A 可能有**正确性陷阱**：某些 load-use 场景其实不可同拍解析（DMem 读非组合） | Task 1 Step 2 先确认 read port 语义；PoC 必须带**正确性断言**，不能只看 cycle |
| R2 | 候选 B 与 ④ **双写数据通路** | Task 2 Step 2 明确边界；候选 B ⇒ 强制排在 ④ 阶段 A 之后 |
| R3 | 去掉 stall 气泡可能暴露**下游未实现的假设**（如 writeback 同拍可见性）⇒ 回归失败 | Task 3 Step 2 的 0 退化回归是硬闸 |
| R4 | 转发/精确化后 **cycle 下降 ⇒ TLM 侧基线不受影响**（TLM 已有 forwarding）⇒ 但**基线 CSV 需更新**（数值变化） | Task 3 Step 4 显式更新 + notes 说明 |
| R5 | 本 change 变成「性能工程」而非「能力补齐」⇒ 范围蔓延 | 范围锁定为 **load-use 依赖解析**；不顺手做其他优化（每个优化单独评估） |

## 迁移说明

落地为 `.rddf/plans/chmem-load-use-bypass.md`；change 名 `chmem-load-use-bypass`；建议走 **rdd-builder P1–P3**（触及 `ip/cpu/**` 数据通路 + 需 architect 参与 Task 2 选型）。
**P-2a（Task 1+2）可先单独交付**（只产出选型结论，零生产代码改动）⇒ 建议按 rdd-quick 处理 P-2a、P-2b 走完整流程。

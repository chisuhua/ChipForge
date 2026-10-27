---
id: objective-ip-cpu-tlm-backend-retirement
status: active
created: 2026-10-23
last_revised: 2026-10-23
primary_scope: ip-cpu
co_scopes: [ip-mmu, ip-cache]
review_by: 2027-06-30
owner: rdd-planner
priority: P0
manual_deps:
  - objective-soc-cpu-v0110-launch
  - feat-tlm-backend-retirement
  - feat-rv64-isa-declaration-integrity
supersedes: null
theme: TLM 后端退役 — CH_MEM 单后端收敛 (B0 冻结 → B1 承载力 → B2 语义裁决 → B3 删除)
---

# Objective: objective-ip-cpu-tlm-backend-retirement

## 1. 驱动诊断（Why now）

用户诉求（2026-10-23）：**「路线图中只需要 CH_MEM 版本；老的、不能生成 Verilog 后端代码的版本我希望移除。」**

现状（实测，非推断）：

1. **双后端已产生同 ISA 不同语义** —— 这不是"一份代码两个后端"，是"两份语义不同的实现"：
   - M 扩展：TLM radix-2 FSM（MUL=455c/DIV=380c）vs CH_MEM 组合式（mul=472c/div=94c）—— `AGENTS.md`「CH_MEM 模式状态」段自认「不同构、不可比」
   - CSR：TLM `ip/cpu/arch/riscv/csr.h:26-36` **纯 stub** vs CH_MEM `csr_chmem.h` 566 行实装
   - MMU PTW：TLM 完整 walk vs CH_MEM `MMUPlugin_chmem.h:149-150,224-225`「PTW 未落地前表现为回退 vaddr」
   - BP：TLM 有 BTB/GShare/RAS（CH_MEM binary 符号数 = 0）vs CH_MEM 无
   - MMU exception handler：CH_MEM 版存在但 `cpu_factory_chmem.h:55` 在 `#ifndef CF_PLUGIN_USE_CH_MEM` 内 ⇒ **未注册进 pipeline**
   ⇒ **ADR-083 的"单一 source of truth"只在 ALU/CSR/decoder 子集上成立**
2. **遗留面规模**：44 文件 / 5,331 行 TLM-only（`ip/` 的 46%）+ `src/cf_plugin/bridge/` 8 文件 / 727 行（CppTLM 直依赖）
3. **测试覆盖损失面**：TLM binary 481 case vs CH_MEM 130 case，**TLM 独占 451（93.7%）**
4. **两条性能硬门禁载体 100% 在 TLM**：Dhrystone DMIPS/MHz ≥1.4（v0.11.0 launch gate 判据 #3）+ CoreMark/MHz ≥1.9（PoC-6）
5. **CI 不跑测试**（Oracle 2026-10-06 决策）⇒ 删 TLM 后回归不可见
6. **ADR-087 交叉**：其"假 RV64 就绪"证据全部在 TLM 侧 ⇒ 护栏策略需随 TLM 退役切换形态

**为什么 now**：v0.11.0 窗口（17 周）正在做 MMU CH_MEM 集成（Phase B-E）—— 这是**最后一次在有双后端语境下做架构裁决**的窗口。再往后 CH_MEM 覆盖面越大，TLM 的"参考实现"价值越低、删除成本越高。

## 2. 目标愿景 + 完成判据

**终态**：仓库只有 CH_MEM 一条后端（`pb.elaborate(ctx)` → CppHDL Simulator / `ch::toVerilog(ctx)` / Verilator），无 TLM-only 业务代码，无 CppTLM 链接依赖。

| 阶段 | 判据 |
|------|------|
| **B0 冻结**（phase-4） | 冻结门禁脚本 CI 可执行；4 个死代码文件删除且双 binary 0 退化；ADR-088 Proposed；覆盖平价台账 100% 覆盖 |
| **B1 承载力**（phase-4/5） | Dhrystone 在 `cpu_verilator_sim` 跑通 + **新基线 CSV 入库**；CH_MEM 侧 LSU 全宽度 + MMU PTW walk FSM + MMU exception handler 注册 + BP CH_MEM 版 落地 |
| **B2 语义裁决**（phase-5） | ADR-088 §语义裁决表 5 子系统逐条表态；M 扩展 cycle 裁决明确；DMIPS 判据改绑 Verilator 后端 |
| **B3 删除**（phase-5） | CppTLM 链接归零；TLM-only 业务文件归零；全量 ctest 4 entries PASS + 5 架构门禁 PASS；`run_chipforge_tests.sh` 默认入口改指 CH_MEM |

### 2.1 ★ B1 的 BP 子项：**机制已成立，收益尚不足以恢复绝对值门禁**（2026-10-26 实测）

> **数据源**：前置2 PoC（`chmem-bp-btb-gshare-poc`，2026-10-26 收口）+ Dhrystone 真实负载对照（CH_MEM/Verilator 后端）。

| 指标 | BP=off | BP=on | Δ |
|---|---|---|---|
| Dhrystone cycles | 914544 | **846500** | **−68044（−7.4%）** |
| Dhrystone FLUSH_COUNT | 120065 | 52021 | **−68044** |
| Dhrystone DMIPS/MHz | 1.24467 | **1.34472** | **+8.0%** |
| branchbench 4 档 Δcycles | — | — | **1996 / 3994 / 1747 / 2019**（4 档全改善，校验和**逐位不变**） |

⇒ ★★ **两条对 objective 直接生效的结论**：

1. **★ ADR-088 §5.3.1 的根因（「CH_MEM 无 BP ⇒ 每条 taken 分支都要 flush」）被直接实测坐实**：
   `FLUSH(off) = 120065` ≈ TLM–CH_MEM 的 cycle 差距 `120066`（`914544 − 794478`）⇒ **差距几乎全部是 taken 分支的 flush 气泡**
   ⇒ 且 **完美预测的理论上限 = `914544 − 120065 = 794479` ≈ TLM 的 `794478`（差 1 拍）** ⇒ ★ **「CH_MEM 追平 TLM」在数学上可达**
   ⇒ 且 **ΔFLUSH 与 Δcycles 逐位相等**（68044）⇒ 每个被消除的 taken 气泡**恰 1 拍**（在真实负载上同样成立）。
2. ★★ **绝对值门禁（DMIPS ≥ 1.4）仍差 33426 拍（3.95%）**：达 1.4 需 cycles ≤ `2000e6/(1757×1.4)` = **813074**
   ⇒ 需闭合 TLM–CH_MEM 差距的 **84.51%**（`101470/120066`）⇒ 当前闭合 **56.7%**（`68044/120065`）
   ⇒ ⇒ ★ **B1 的 BP 子项尚未解锁 P-6**（绝对值门禁）⇒ ⇒ **后续必须靠「覆盖面 + 方向精度」而非「有没有 BP」**
   ⇒ ⇒ **三档增量**（`is_unconditional`+`jal` / RAS / GShare）见 `feat-bp-btb-gshare` 的 phase-5 段与
   `openspec/changes/chmem-bp-btb-gshare` 的 **Task 7 / AC-27**（★ **A5 决策**：先扩覆盖并量收益，再定 GShare）。

## 3. 架构依据

- **ADR-083**（★ **v1.2 2026-10-28 表述纠偏**）：CppTLM 是 legacy 仿真内核，「业务 IP 不使用 **CppTLM 组件范式**（`Module`/`Port`/`ModuleFactory`/`ChStreamModuleBase`/`StreamAdapter`）」⇒ ★ **CppHDL 的 `ch_stream<T>` 可用**（IP 之间 / 核 ↔ 从设备；核内 stage 间仍用 `Payload<T>`）—— 本 objective 是其定性结论的**执行计划**（B0 阶段出 v1.1 修订补冻结条款 + 退役时间线）
  ★ **配套**：**ADR-092**（TLM 参考实现的冻结与演进停止 = 本 objective 阶段 C 的 **Phase-1 准备**）
- **ADR-040 v2.0**：「CH_MEM 是新正道」—— 直接支持收敛方向；B3 完成后需 v3.0 移除 TLM 分支语义（远期，记入 ADR-088）
- **ADR-046**（FSM 豁免）：**强依赖** —— B1 的 PTW walk FSM / L1Cache refill FSM / M 扩展 FSM 版全走此豁免；收敛决策反而提高其使用密度
- **ADR-087**（Proposed）：正交互补 + 交叉点（D4 护栏形态随 TLM 退役切换，需 v1.1 修订）
- **ADR-085 / ADR-086**：MMU 侧已按 Sv39/Sv48 规划，CPU 侧 CH_MEM PTW 仍 sv32 ⇒ B1 的直接输入
- **Oracle 2026-10-23 架构咨询**：定性为「架构收敛（方向正确），但『移除』是终态而非动作」；按字面立即删除退化为范围蔓延

## 5. 反例（明确 out-of-scope）

- **不做"立即全量删除"** —— 两条性能硬门禁当场失去载体 + 93.7% 覆盖消失 + CI 不跑测试 ⇒ 回归不可见
- **不改判据来糊弄** —— Dhrystone DMIPS ≥1.4 是 v0.11.0 launch gate 判据 #3 且 `feat-dhrystone-dmips-gate` 已 archived done；废弃需显式 ADR 决策，非本 objective 权限
- **不删"目录名带 `tlm/` 但双模共享"的 10 文件** —— 误删即打断 CH_MEM（`mmu_keys.h` / `cpu_keys.h` / `ptw.h` / `decoder_table.h` / `payload_riscv.h` / `payload_common.h` / `memory_interface.h`）；这批文件的正确动作是**改名去误导**，不是删除
- **不退役 `[framework]` 94 个框架语义锚** —— pipe_builder / pipe_node / payload / ctrl_link / storage / negotiate 是双模共享框架的语义锚，只允许移植
- **不在 phase-4 窗口做 B1 主体** —— 17 周时间盒内只做 B0 + 策略 C（成本 <1 周），B1/B2/B3 归 phase-5 / v1.0.0+
- **不做 ADR-040 v3.0 移除 TLM 分支语义** —— 远期，记入 ADR-088 待办
- **不改 `tools/cpu_sim` 的功能定位** —— 它在 B3 前是 Dhrystone/CLI 测试载体；B3 时连同 `link_add.ld` 路由迁入 verilator_runner

## 9. 目标依赖与 Decision Gate

### 9.1 前置 objective 依赖

- `objective-soc-cpu-v0110-launch` — 判据 #3（Dhrystone DMIPS ≥1.4）的载体在 TLM；**本 objective 的 B1/B2 不得使该判据失去载体**
- `objective-ip-mmu-mmu-chmem-phase-b-e` — MMU CH_MEM 集成进行中；B1 的 PTW walk 承载力补齐依赖其 Phase B-E 收官
- `feat-bp-btb-gshare` — 其 phase-5 段实施目标**改绑 CH_MEM-only**（避免 BP 双份实现）
- `feat-rv64-isa-declaration-integrity` — ADR-087 D1/D2（声明纠偏）**独立于本 objective 先行**，因声明层越界在 CH_MEM 侧同样存在
- `feat-dhrystone-dmips-gate` — 其判据后端绑定在 B2 改判 + 重基线

### 9.2 Go / No-Go Decision Gate

**Gate 1 —— B0 → B1（phase-4 收口）**

| # | Go 条件 | No-Go 处置 |
|---|----------|-------------|
| G1.1 | 冻结门禁脚本 CI 可执行，TLM-only 文件新增 feature 函数即 fail | 不启动 B1（否则双写漂移扩大） |
| G1.2 | 4 个死代码文件删除 + 双 binary 编译 0 退化 + 全量测试 0 退化 | 回滚策略 C，单独排期 |
| G1.3 | ADR-088 Proposed（含冻结条款 + 语义裁决表 + 覆盖平价台账格式 + 删除判据） | 不启动 B1 |
| G1.4 | 覆盖平价台账 451 条 100% 标注，`[framework]` 94 条全部标「移植」 | 不启动 B1 |
| G1.5 | ADR-087 D1/D2 声明纠偏已落地（`isa` 默认 `rv32im`） | 不启动 B1（否则删除动作与虚假声明叠加） |

**Gate 2 —— B1 → B2（phase-5 前段）**

| # | Go 条件 |
|---|----------|
| G2.1 | Dhrystone 在 `cpu_verilator_sim` 跑通 + **新基线 CSV 入库**（TLM 基线 1.43 不可迁移） |
| G2.2 | CH_MEM 侧 LSU 全宽度 + LD/SD + MMU PTW walk FSM + MMU exception handler 注册 + BP CH_MEM 版 全部落地 |
| G2.3 | `[verilator]` 48 ELF 全 `tohost==1` + `[mmu-verilator]` 0 退化 |

**Gate 3 —— B2 → B3（删除放行，不可绕过）**

| # | Go 条件 |
|---|----------|
| G3.1 | ADR-088 §语义裁决表 5 子系统逐条表态；M 扩展 cycle 裁决明确（功能=组合式，cycle=待 FSM 收敛） |
| G3.2 | **DMIPS 判据改绑 Verilator 后端并重测基线** —— 判据**不删**，只是换载体重校准 |
| G3.3 | 覆盖平价台账无"批量退役"（同一理由重复 >10 条 ⇒ No-Go） |
| G3.4 | CppTLM 链接依赖可脱（`cf_plugin_link_cpptlm` 移除后 clean build 通过） |
| G3.5 | `tools/cpu_sim` CLI 测试改绑 `cpu_verilator_sim` 完成 |

## 10. next_sprint_candidates

- **Sprint 2（10-23 ~ 10-30）**：并行 3 条 —— ① `rv64-isa-declaration-correction`（ADR-087 Change 1，1.5d，纯配置/文档零代码风险）② `chmem-tlm-deadcode-cleanup`（策略 C，4 文件 201 行，0.5d）③ TLM 冻结门禁脚本起草（1d）④ `mmu-chmem-phase-b-c-launch` Phase B 继续（不挤占）
- **Sprint 2-3（10-23 ~ 11-06）**：冻结门禁上线 CI + ADR-088 起草（1.5d）+ 覆盖平价台账建立（1.5d，451 条三态标注）
- **Sprint 3（11-06 ~ 11-20）**：`ip/{cpu,mmu}/tlm/` 双模共享文件迁 `core/` 目录去误导（1d）
- **Sprint 4+**：B1 主体归 phase-5 / v1.0.0 窗口（不在 v0.11.0 17 周窗口内挤占）

## 11. 跟踪台账（append-only）

| 日期 | 事件 |
|------|------|
| 2026-10-23 | 立项。architect 阶段 RV64 符合性审查确认「实现 = RV32IM + Zicsr，声明层宣称 RV64GC」→ ADR-087 Proposed；Oracle 架构咨询定性为「架构收敛，非清理」→ 定 4 阶段 B0/B1/B2/B3；用户明示「路线图只要 CH_MEM 版本，移除不能生成 Verilog 的老版本」 |
| 2026-10-26 | **B1 的 BP 子项收口**：前置2 PoC（`chmem-bp-btb-gshare-poc`）机制判定 = **成立**（Δcycles 4 档全改善 + 校验和逐位不变 + ΔFLUSH≡Δcycles）⇒ ★ **Dhrystone 真实负载**：cycles 914544→846500（−7.4%）、DMIPS 1.24467→1.34472（+8.0%）⇒ ★ **但绝对值门禁（1.4）未恢复**（需闭合 84.56%，当前 56.7%）⇒ ★ 缺口从「有没有 BP」转为「覆盖面 + 方向精度」⇒ **三档增量**（`is_unconditional`+`jal` / RAS / GShare）按实测收益排序（**A5 决策**）⇒ ★ ADR-088 §5.3.1 根因被直接实测坐实（`FLUSH(off)=120065` ≈ TLM 差距 `120066`；完美预测上限 ≈ TLM 逐位）|

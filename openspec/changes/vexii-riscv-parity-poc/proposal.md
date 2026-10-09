---
initiative: wave6-linux-and-productization
priority: P1
version_target: v1.0.0
depends_on:
  - mfc-cpu-pipeline-multi-cycle-fsm (PoC-1/2 实装, v0.10.0)
  - plugin-framework-cycle-precision (cycle-accurate CoreMark 实测, v0.10.0+)
  - cache-icache-fence-i (PoC-3 L1 ICache, v0.10.0)
  - cache-phase1.5-4way (PoC-8 4-way RRIP, v1.2.0；仅 PoC-15 依赖，PoC-14 不依赖)
supersedes: []
related_to:
  - ADR-040 (TLM↔CH_MEM 双模同语义, v2.0)
  - ADR-080 (TLM↔CH_MEM 双模对拍协议, v1.3.0 转硬门禁)
  - ADR-082 (Plugin::negotiate, v0.10.0)
  - ADR-083 (write-back FSM, v1.x 净新增)
---

> **initiative 注册状态说明**：`wave6-linux-and-productization` 当前为 proposed 状态。正式注册需待 `wave3-mmu-real-memory-and-cycle` 残余 change（`plugin-framework-cycle-precision` Phase F optional 实装 [2026-09-29 (b) 决策降级, 当前 0/25 NOT STARTED] + `mmu-config-json-driven` archive）闭环后，按 §4.2 OpenSpec 同步流程追加注册。本 change 已通过 frontmatter `initiative:` 字段预声明占用编号，避免后续撞号。

# vexii-riscv-parity-poc — VexiiRiscv 客观对拍 HARD 门禁（PoC-14 + PoC-15）

> **承接关系**：本 change 是 **2026-09-29 新增**，由 [`docs/architecture/roadmap-evolution.md` §4 (v0.10.0) + §5 (v1.0.0) + §7 (v1.3.0) + §8 跨版本对比](../../../../docs/architecture/roadmap-evolution.md) 的 HARD 门禁升级衍生（migrated 2026-10-09: 原 `soc/cpu/docs/roadmap/execution-roadmap.md §3.2+§3.3` 已并入 evolution）。
>
> **技术定位**：把"内部绝对值 CoreMark/MHz ≥ N"升级为"与 VexiiRiscv 官方公开数对拍，偏差 ≤ X%"，作为架构正确性（生成 RTL 可对标行业最佳 in-order）与路线合理性（产品化对标窗口）的**形式化论证门禁**。
>
> **拆分原因**：原 PoC 表只有内部绝对值，无法论证"在 in-order 赛道的相对位置"；VexiiRiscv 是同 in-order + 同 Plugin/Elaboration 范式 + 同 FPGA 优先的真正对标（XiangShan OoO 永不可达，§1 非目标已声明）。

## Why

`docs/architecture/roadmap-evolution.md` §8 跨版本对比表（migrated from soc/cpu/docs/roadmap/execution-roadmap.md §3.2）中 v1.0.0 / v1.3.0 / v1.4+ 的"关键指标"列原写：

| 版本 | 原软指标 | 缺陷 |
|------|---------|------|
| v1.0.0 | CoreMark ≥ 2.3 / FMAX ≥ 100 MHz | 不知道 2.3 在 in-order 赛道算什么 |
| v1.3.0 | FPGA CoreMark ≥ 2.5 | 同上 |
| v1.4+ | dual-issue IPC ≥ 1.55 / CoreMark ≥ 4.5 [估] | 完全无对标坐标 |

`references/multi-core-comparison.md` 47 行 × 4 列对比矩阵 §3 关键定位已声明：
> "ChipForge 的可超越点集中在 8 个 🚀 独占维度（2026-09-29 扩展：新增第 48 项 ADR-080 双模对拍协议时间差优势），**全部围绕'双模 + 工程纪律'，无一是正面拼 IPC**。"
> "正面确认：XiangShan 在 ISA 广度（#1–10）、OoO 性能（#38–40、43）、流片（#44）、多核（#45）全面压制两家 in-order 核 —— **ChipForge 不与它同赛道竞争**，其出现于此表的唯一作用是**性能天花板坐标**。"

但 IPC 拼不过 XiangShan 是事实，**IPC 拼不过 VexiiRiscv 是缺陷**。当前 roadmap 缺少与 VexiiRiscv 客观对拍的 HARD 门禁：
1. **架构正确性无法形式论证**：没有"我们生成的 RTL 在 in-order 赛道达到主流水平"的客观证据
2. **路线合理性无法对外披露**：投资人/合作伙伴无法从绝对值判断项目水平
3. **PoC 缺失链路**：PoC-1（rv32um 100%）+ PoC-2（RV32C）+ PoC-3（ICache）已经覆盖 ISA 完整度，但没有**性能完整度** PoC

后果：v1.0.0 archive 时无法回答"我们达到了 in-order 主流什么水平"，v1.3.0 Linux-on-FPGA 没有性能对照基准。

## What Changes

### 1. PoC-14：v1.0.0 single-issue 客观对拍（HARD 门禁）

- **目标**：FPGA 实测 CoreMark/MHz 与 VexiiRiscv single-issue 官方配置（2.4–2.6 CoreMark/MHz）偏差 ≤ 10%
- **触发节点**：v1.0.0 archive 前必须通过
- **依赖实装**：`mfc-cpu-pipeline-multi-cycle-fsm`（v0.10.0 PoC-1/2 实装）+ `plugin-framework-cycle-precision`（v0.10.0+ cycle-accurate CoreMark 测量; **Phase F optional per 2026-09-29 (b) 决策**, 当前 0/25 NOT STARTED, 兜底路径 = `tools/cpu_sim/main.cpp:230-242` `actual_cycles`）+ `cache-icache-fence-i`（v0.10.0 L1 ICache ≥90% 命中）。**PoC-15 额外依赖** `cache-phase1.5-4way`（v1.2.0 PoC-8 4-way RRIP）。
- **实现内容**：
  - 在 `tests/cpu/parity/` 新建 `vexii_riscv_runner.cpp`（按 AGENTS.md tests-family 约定，与 `tests/cpu/riscv-tests-fixture/` 平级）：
      - 加载 ChipForge v1.0.0 配置生成的 FPGA bitstream
      - 跑 CoreMark 完整 suite，记录 cycle count
      - 计算 CoreMark/MHz 实测值
      - 与 VexiiRiscv single-issue 官方值（2.4–2.6）做偏差计算
      - 偏差 > 10% → 报 `[vexii-parity]` test FAIL
  - CI 新增 PoC-14 family：`./build/bin/chipforge_tests "[vexii-parity]"` 必须 PASS
- **VexiiRiscv 对标参考**（VexiiRiscv 官方 2025-07-01 status）：
  - single-issue 配置官方 CoreMark/MHz：**2.4–2.6**
  - single-issue DMIPS/MHz：~1.4 [估]
  - 数据来源：https://spinalhdl.github.io/VexiiRiscv-RTD/master/VexiiRiscv/Performance/index.html

### 2. PoC-15：v1.4+ dual 全家桶客观对拍（HARD 候选门禁）

- **目标**：dual 全家桶（dual-issue + HW prefetch + write-back + store buffer）CoreMark/MHz ≥ 4.5，与 VexiiRiscv dual+prefetch 官方（5.24 CoreMark/MHz）偏差 ≤ 15%
- **触发节点**：v1.4+ archive 前必须通过（v1.4 当前是远期候选分叉，本 PoC 在 v1.4 启动时激活）
- **依赖实装**：v1.0.0（BTB+GShare+RAS）+ v1.2.0（4-way RRIP）+ v1.3.0（write-back FSM ADR-083）+ dual-issue（v1.4+ 候选）
- **实现内容**：
  - 复用 PoC-14 的 `vexii_riscv_runner.cpp` 框架
  - 加载 v1.4+ dual 全家桶配置
  - 与 VexiiRiscv dual+prefetch 官方（5.24 CoreMark/MHz）做偏差计算
  - 偏差 > 15% → 报 `[vexii-parity][dual]` test FAIL（family tag 多 tag 组合：`[vexii-parity][dual][v1.4-pending]` 在 v1.4 启动前 SKIP）
  - 配套 IPC 实测 ≥ 1.5（**注**：PoC-12 HARD 候选门禁 IPC ≥ 1.55，PoC-15 配套 IPC ≥ 1.5 为更宽口径以容许 dual 全家桶配置差异；两者并存，差异点见 §3.4 R-B 风险）
  - 配套 IPC 实测 ≥ 1.5（PoC-12 IPC ≥ 1.55 HARD 候选门禁）
- **VexiiRiscv 对标参考**：
  - dual+prefetch 官方 CoreMark/MHz：**5.24**
  - dual-issue 官方 DMIPS/MHz：**2.50**
  - dual 全家桶 mispredict 率：~3–5%

### 3. proposal 文档（与 OpenSpec apply-change 流程对齐）

- 本 proposal 落地后自动 `openspec change show vexii-riscv-parity-poc` 可查
- `tasks.md` 用 TDD 5 步结构（先 failing-test，再实现，再验证）
- `design.md` 描述 vexii_riscv_runner 的架构（CoreMark ELF 加载 + FPGA cycle 计数 + 偏差计算 + CI gate）
- `specs/vexii-riscv-parity-runner/spec.md` 定义 capability 契约

## Capabilities

### New Capabilities

- `vexii-riscv-parity-runner`：VexiiRiscv 客观对拍测试 runner，包括 CoreMark ELF 加载、FPGA 实测 cycle 收集、与 VexiiRiscv 官方 single/dual 配置 CoreMark/MHz 的偏差计算、CI gate 触发逻辑。

### Modified Capabilities

无（不修改现有 spec 的 REQUIREMENTS，仅新增独立 capability）

## Acceptance Criteria

- [ ] `tests/cpu/parity/vexii_riscv_runner.cpp` 编译通过 + 单测 PASS
- [ ] PoC-14 failing test 写出（`[vexii-parity] single-issue` family）
- [ ] PoC-15 failing test 写出（`[vexii-parity][dual] dual-issue` family + `[v1.4-pending]` 标签，v1.4 启动前 SKIP，v1.4 archive 前激活）
- [ ] `vexii_riscv_runner` 文档化 VexiiRiscv 官方数值来源 + 偏差公式
- [ ] CI 新增 PoC-14 family tag，archive 时必须 PASS（v1.0.0 + v1.3.0 archive gate）
- [ ] CI PoC-15 family tag 在 v1.4+ archive 前激活
- [ ] 与 `references/multi-core-comparison.md §2` 8 个独占维度（ADR-080 双模对拍协议时间差优势）协同
- [ ] v1.0.0 archive 时，`[vexii-parity]` 必须 PASS（CoreMark/MHz 与 VexiiRiscv 2.4–2.6 偏差 ≤ 10%）
- [ ] v1.3.0 archive 时，PoC-14 复测 PASS（FPGA 实测，偏差 ≤ 10%）
- [ ] v1.4+ archive 时，PoC-15 PASS（dual 全家桶 ≥ 4.5，与 VexiiRiscv 5.24 偏差 ≤ 15%）

## ADR 锚点

- **ADR-040 v2.0**（已落地，2026-09-17）：TLM↔CH_MEM 双模同语义，本 runner 同时验证 TLM 与 CH_MEM 模式 CoreMark/MHz 一致
- **ADR-080**（v1.2.0 试点 + v1.3.0 HARD 门禁）：TLM↔CH_MEM byte-equal 协议，本 runner 是其性能层面的同源验证
- **ADR-082**（v0.10.0 落地）：Plugin::negotiate() capability 协商，本 runner 需要 ensure `cycle_counter` capability 提供方
- **ADR-083**（v1.x 净新增）：write-back FSM，本 runner 的 dual 全家桶指标依赖此 ADR 落地
- **新增 ADR 建议**（v1.0.0 archive 前）：`ADR-090 vexii-riscv-parity-runner-credential`（VexiiRiscv 官方数值 SSOT + 偏差公式 + CI gate 触发条件的正式 ADR 化）

## 失败 → 砍分叉动作

- **R-A** PoC-14 偏差 > 10%：
  - 先排查 v1.0.0 配置（BP mode = btb+gshare+ras? Cache 4-way? FMAX ≥ 100 MHz?）
  - 单项 RCA + 复测；若 4 周内仍不达标 → 推迟 v1.0.0 archive，砍 Zicsr 子集优先
- **R-B** PoC-15 偏差 > 15%：
  - 排查 dual 全家桶实装度（write-back 是否落地？store buffer 容量？HW prefetch 准确度？）
  - 单项 RCA + 复测；若 8 周内仍不达标 → 砍 dual-issue 分叉，v1.4 转 single+late-alu 保 FMAX 路线（与 `references/decision-1-plugin-evolution.md §2 R-B 信号 2` 对齐）
- **R-C** FPGA 综合不可行（FMAX 实测 < 80 MHz）：
  - 排查 synthesis 策略（资源 vs 时序？Logic vs DSP？）
  - 仍不可行 → 接受 v1.3.0 走 sim-only，FPGA 综合推迟 v1.4+
- **R-D** CoreMark ELF 加载问题（与 VexiiRiscv 同款 CoreMark v1.01 不可获取）：
  - 降级为 Dhrystone 实测对拍（VexiiRiscv 也支持）
  - 降级风险：Dhrystone 单 benchmark 噪声大，偏差阈值放宽到 15%

## 实施窗口

- **启动条件**：
  1. `mfc-cpu-pipeline-multi-cycle-fsm` v0.10.0 archive（提供 cycle-accurate MUL/DIV）
  2. `plugin-framework-cycle-precision` v0.10.0+ **Phase F optional** 实装（提供 `pb.run(N)` API + cycle_counter; 当前 0/25 NOT STARTED per 2026-09-29 (b) 决策; 兜底路径 = `tools/cpu_sim/main.cpp:230-242` `actual_cycles`）
  3. v0.10.0 L1 ICache 实装（PoC-3 PoC-3 ≥ 90% 命中）
- **PoC-14 估时**：2 周（PoC-14 单发射追平，2027 Q3 之前 = v1.0.0 archive 前）
- **PoC-15 估时**：3 周（v1.4+ 启动时，2029 Q3+）
- **顺序**：先 PoC-14 落地 → v1.0.0 archive gate 验证 → v1.3.0 复测 → v1.4+ 启动时激活 PoC-15

## 关联文档

- [`docs/architecture/roadmap-evolution.md` §8 跨版本对比 + §4 (v0.10.0) + §5 (v1.0.0) + §7 (v1.3.0)](../../../../docs/architecture/roadmap-evolution.md)：HARD 门禁落地位置（migrated from soc/cpu/docs/roadmap/execution-roadmap.md §3.2+§3.3, 2026-10-09）
- [`docs/research/multi-core-comparison.md §2`](../../../docs/research/multi-core-comparison.md)：8 个独占维度
- [`docs/research/decision-1-plugin-evolution.md §1(c)`](../../../docs/research/decision-1-plugin-evolution.md)：双追平线（single 2.4–2.6 + dual 5.24）
- VexiiRiscv 官方 status：`https://spinalhdl.github.io/VexiiRiscv-RTD/master/VexiiRiscv/Introduction/index.html`
- VexiiRiscv 官方 Performance：`https://spinalhdl.github.io/VexiiRiscv-RTD/master/VexiiRiscv/Performance/index.html`

## 风险与回退汇总（来自表 §3.4）

| 风险 | 触发 | 砍分叉 |
|------|------|--------|
| R-A PoC-14 偏差 > 10% | v1.0.0 archive 前 | 推迟 v1.0.0 + RCA 4 周 |
| R-B PoC-15 偏差 > 15% | v1.4+ archive 前 | 砍 dual-issue 分叉，转 single+late-alu |
| R-C FPGA 不可综合 | v1.3.0 archive 前 | 走 sim-only，FPGA 综合推迟 v1.4+ |
| R-D CoreMark 不可获取 | 任何时点 | 降级 Dhrystone 对拍，阈值放宽 15% |
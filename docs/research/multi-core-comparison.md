# Reference 3：多核对比矩阵（VexRiscv / VexiiRiscv / XiangShan / ChipForge）

> **主控文档**：[`../execution-roadmap.md`](../execution-roadmap.md) §1 目标表（差异化目标行 + 超越目标行）
> **关联**：本文是 §1 超越目标"8 个 🚀 独占维度（2026-09-29 扩展）"的依据

---

## 0. 数字口径说明

- **VexiiRiscv** = 官方 2025-07-01 status 页原文
- **VexRiscv** = 官方 README/paper 宣称值（标 ~）
- **XiangShan** = 公开 Nanhu 流片事实 + Kunminghu 公开口径（**OoO 旗舰，性能天花板坐标，量级不同**）
- **ChipForge**：实测值标 **[测]**，目标值标 **[目]**，估计值标 **[估]**
- **🚀** = ChipForge 独占维度（VexRiscv / VexiiRiscv / XiangShan 结构性无法跟进）

---

## 1. 47 行 × 4 列对比矩阵

| # | 维度 | VexRiscv | VexiiRiscv | XiangShan (Kunminghu) | ChipForge |
|---|------|----------|-----------|---------------------|-----------|
| 1 | ISA-I | RV32/64I | RV32/64I | RV64I | RV32I **[测]** 40/40 |
| 2 | ISA-M | ✅ | ✅ | ✅ | 多周期迭代中 27/35 **[测]** |
| 3 | ISA-A | ✅ | ✅（部分总线配置缺） | ✅ | ❌ v1.0.0 **[目]** |
| 4 | ISA-F | ✅ | ✅ | ✅ | ❌ v1.3.0 或 soft-float 降级线 |
| 5 | ISA-D | ✅ | ✅ | ✅ | ❌ v1.x 不做 |
| 6 | ISA-C | ✅ | ✅ | ✅ | ❌ v0.10.0 ADR-070 **[目]** |
| 7 | ISA-B | — | ✅ | ✅ | ❌ 未排期 |
| 8 | ISA-K（标量加密） | — | — | ✅ | ❌ |
| 9 | ISA-H（hypervisor） | — | — | ✅ | ❌ |
| 10 | ISA-V（向量） | — | — | ✅（RVV 1.0） | ❌ |
| 11 | 流水线深度 | 2–5 级可配 | 4–6+ 级可配 | 12+ 级（OoO） | 5 级 **[测]** + 7 级 **[测]** |
| 12 | Issue 宽度 | 1 | 1–2（可不对称） | 6（OoO 发射） | 1 **[测]**；2 为 v1.4 PoC-12 **[目]** |
| 13 | Early ALU | ✅ | ✅ | N/A（OoO） | ✅ **[测]**（EX1） |
| 14 | Late ALU | — | ✅（可选） | N/A | —（ADR-081 预研） |
| 15 | 站级 bypass/forwarding | ✅ | ✅（分级） | 完整 OoO 旁路网 | 全前递 **[测]**（commit 8909165） |
| 16 | 分支预测档位 | Static/BTB | **None→BTB→GShare+RAS 分级** | TAGE 级（业界第一梯队） | Static **[测]**；分级 ADR-075 **[目]** |
| 17 | mispredict 惩罚 | ~3–4 cycle | ~3 cycle | ~10+ cycle（深流水代价） | 3 cycle **[估]** |
| 18 | L1 I$ | 可选 | non-blocking + HW prefetch | 32KB+ | ❌ v0.10.0 PoC-3 **[目]** |
| 19 | L1 D$ | **write-through** | write-back non-blocking + store buffer | write-back non-blocking | write-through 256×1 direct **[测]**；WB+SB v1.x **[目]** |
| 20 | L2 | —（SoC 侧） | SoC 侧 | 1MB/核 | — |
| 21 | CBM（cache 块管理指令） | — | ✅ | ✅ | — |
| 22 | HW prefetch | — | ✅（I$ HW + D$ HW/SW） | ✅（多级） | —（v1.4+ 候选） |
| 23 | MMU | Sv32 | Sv32/Sv39 | Sv39/Sv48 | Sv32 真 PTW walk **[测]**（47 测试 110 断言） |
| 24 | ASID | — | ✅ | ✅ | —（ADR-076 v1.x 全局冲刷+hook） **[目]** |
| 25 | **TLB 替换策略** | 简单 PLRU | PLRU | 多级 PLRU | **RRIP** 🚀 **[测]** |
| 26 | 特权模式 | M（+S/U 部分） | M/S/U 完整 | M/S/U + H | M **[测]**；S/U v1.0.0 **[目]**（ADR-073） |
| 27 | 异常委托 medeleg/mideleg | 部分 | 完整 | 完整 | —（ADR-073 真值表先行） **[目]** |
| 28 | PMP | ✅ | ✅ | ✅ | —（v1.2.0 ADR-077） **[目]** |
| 29 | Debug | JTAG | JTAG + EmbeddedRiscvJtag + watchpoint | JTAG + 完整 trigger | —（gdbstub 先 ADR-078） **[目]** |
| 30 | Spike/RVLS lockstep | —（自研 golden 病灶） | RVLS+Spike 双 golden | difftest+Spike（业界标杆） | —（v1.2.0 PoC-9）；**双模对拍替代层** 🚀 |
| **31** | **TLM 快速功能模型** | — | —（SpinalSim 仍是 RTL 语义） | —（GEM5 是外部近似，非同源） | **同一源 TLM 模式，毫秒级** 🚀 **[测]** |
| **32** | **golden 与 RTL 同源** | ❌（两套代码） | ❌（RVLS 外部 + Spike 外部） | ❌（difftest 外部） | **是（TLM↔CH_MEM 同一 Plugin 源）** 🚀 **[测]** |
| **33** | **CI 架构纪律门禁数** | 0 | 0 | 内部流程（未公开计数） | **9 条硬阻塞 + 2 条候选 + 1 条 V0.10.0 新增 = 11 条** 🚀 **[测]** |
| **34** | **总线解耦抽象** | 弱（核内耦合） | AXI4/Wishbone/Tilelink 三实现 | AXI/CHI（SoC 级） | **MemoryInterface 单抽象** 🚀 **[测]**（ADR-049） |
| 35 | Konata trace 可视化 | — | ✅（原生） | ✅（内部） | —（输出 Konata 兼容 JSON 复用生态） **[目]** |
| 36 | OS：Linux/buildroot | 可跑 | 可跑（含 Debian on FPGA，LiteX） | 可跑（含 Debian/安卓实验） | 不可（v1.2.0 sim / v1.3.0 FPGA） **[目]** |
| 37 | OS：FreeRTOS/Zephyr/RT-Thread | FreeRTOS 可 | 可 | N/A（量级不符） | v1.0.0 FreeRTOS 3 任务 demo **[目]** |
| 38 | DMIPS/MHz | ~1.44（full 配置宣称） | 2.50（官方） | ~4+（2GHz 折算） | ~1.1 **[估]** → v1.0.0 ≥1.7 → v1.4 dual ≥2.4 **[目]** |
| 39 | CoreMark/MHz | ~2.44（宣称） | 5.24（官方，dual-issue 全家桶） | 5.3（公开口径） | ~1.5 **[估]** → v1.0.0 ≥2.3 → v1.3.0 ≥2.5 **[目]** |
| 40 | SPECint2006 | N/A | N/A | 19.10 @ 2GHz | N/A（v1.3.0 后可评估） |
| 41 | FPGA LUT（Artix-7 量级） | ~505（最小）/~3k（full）**[估]** | ~4–6k（single issue）**[估]** | N/A（ASIC 目标） | ~5k **[估]**（7 级 single） |
| 42 | FMAX FPGA（Artix-7） | ~216 MHz（小配置） | ~100–150 MHz（配置相关） | N/A（FPGA 原型 ~50 MHz 级） | 100 MHz **[目]** |
| 43 | FMAX ASIC | 无公开流片主频 | 无公开流片主频 | 3 GHz @ 14nm（公开口径） | —（v1.3.0 前纯 FPGA 验证，决策 2） |
| 44 | 流片历史 | 有（多第三方 SoC 采用） | 有（第三方采用） | Nanhu 28nm/2021；Kunminghu 14nm | —（首次 Shuttle 评估 2029） |
| 45 | 多核一致性 | — | ✅（HW coherency，Tilelink） | ✅（多核 SoC） | —（显式排除 v1.x） |
| **46** | **架构探索→RTL 单源生成** | 部分（Scala 参数化） | 部分（Param.scala 单文件配置） | 部分（Chisel 参数化） | **JSON → TLM 评估 → CH_MEM Verilog 一键双模** 🚀 **[目]**（ADR-079） |
| **47** | **定制指令侵入度** | 改 Scala 核（需懂 SpinalHDL） | Plugin 范式友好（doc 有 SIMD add 教程） | 重（OoO 侵入深） | **Decoder 条目 + 1 个 Execute Plugin，TLM 先验证** 🚀 **[测]**（D4 范式原生） |

---

## 2. 8 个 🚀 独占维度汇总（2026-09-29 扩展：新增 ADR-080 双模对拍协议时间差优势）

| # | 独占维度 | 三家对照（结构性无法跟进） |
|---|---------|--------------------------|
| 25 | **TLB RRIP 替换策略** | VexRiscv/VexiiRiscv 简单 PLRU；XiangShan 学术但不开源 |
| 31 | **TLM 快速功能模型（毫秒级）** | 三家单模（SpinalHDL/Chisel 仿真即 RTL，慢 100×） |
| 32 | **golden 与 RTL 同源** | 三家 golden model 与 RTL 都是两套代码 |
| 33 | **CI 架构门禁 11 条** | SpinalHDL/Chisel 生态靠人审，无机械阻塞门禁 |
| 34 | **MemoryInterface 总线解耦** | VexRiscv 核内耦合 / VexiiRiscv 三实现并存但未抽象 |
| 46 | **架构探索→RTL 单源生成（chip-selector）** | VexiiRiscv Param.scala 每次改需 SpinalHDL+Verilator |
| 47 | **定制指令侵入度极低** | VexRiscv 需懂 SpinalHDL / VexiiRiscv Plugin 友好但仍需懂流水线 / XiangShan OoO 侵入深 |
| **48** 🆕 | **双模对拍协议（ADR-080）—— TLM↔CH_MEM 同源 byte-equal** | VexiiRiscv 仅 Spike 外部黄金（RVLS 是社区项目，VexiiRiscv 自身无内部同源机制）；ChipForge v1.2.0 PoC-9 Spike lockstep + v1.3.0 ADR-080 TLM↔CH_MEM byte-equal HARD 门禁形成**同源 + 外部黄金双层防护**，比 VexiiRiscv 的单外部 golden 更彻底，结构性无法跟进 |

---

## 3. 关键定位结论

**正面确认**：XiangShan 在 ISA 广度（#1–10）、OoO 性能（#38–40、43）、流片（#44）、多核（#45）全面压制两家 in-order 核 —— **ChipForge 不与它同赛道竞争**，其出现于此表的唯一作用是**性能天花板坐标**。

ChipForge 的可超越点集中在 8 个 🚀 独占维度（2026-09-29 扩展，新增第 48 项 ADR-080 双模对拍协议时间差优势），**全部围绕"双模 + 工程纪律"**，无一是正面拼 IPC。

---

## 4. 数据来源

| 数据 | 来源 | 取数日期 |
|------|------|----------|
| VexRiscv 25 plugins + 参数 | [github.com/SpinalHDL/VexRiscv README](https://github.com/SpinalHDL/VexRiscv/blob/master/README.md) | 2026-09-26 |
| VexiiRiscv 07/01/2025 status | [spinalhdl.github.io/VexiiRiscv-RTD](https://spinalhdl.github.io/VexiiRiscv-RTD/master/VexiiRiscv/Introduction/index.html) | 2026-09-26 |
| VexiiRiscv 性能 + FPGA 数据 | [VexiiRiscv Performance](https://spinalhdl.github.io/VexiiRiscv-RTD/master/VexiiRiscv/Performance/index.html) | 2026-09-26 |
| SpinalHDL v1.14.0 新动向 | [SpinalHDL releases/tag/v1.14.0](https://github.com/SpinalHDL/SpinalHDL/releases/tag/v1.14.0) | 2026-09-26 |
| XiangShan Kunminghu 公开口径 | [XiangShan Tutorial slides](https://tutorial.xiangshan.cc/rvse25/slides/20250512-RVSE25-XiangShan-Tutorial.pdf) | 2026-09-26 |
| Ibex / CVA6 / Rocket / BOOM 对照 | 各项目 README + 公开 paper | 2026-09-26 |
| ChipForge 实测 | `git log` + ctest 408/413 PASS（2026-09-26） | 2026-09-26 |

> **诚实标注**：VexiiRiscv DivPlugin 的精确 cycle 数、VexRiscv 完整版 CoreMark/MHz 数字（README 给的是 Dhrystone 1.44 DMIPS/MHz，CoreMark 数字需自行测算）在原文档中不明确，本文标 **[估]** 处均带 ~ 号。XiangShan Kunminghu 部分数字来自用户公开口径，已在表中标 "公开口径"。
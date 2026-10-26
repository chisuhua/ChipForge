---
id: feat-backend-parity-gate
kind: feature
status: proposed
scope: cross
phase_refs: [phase-4, phase-5]
主题: 双后端差分 parity 门禁 —— 同一模型（CH_MEM）× 两后端（CppHDL Simulator / Verilator）差分执行
---

## 概述

**目标**：把项目的**一致性证据**从「463 条台账人工计数」换成「**同一输入的确定性双后端差分执行**」⇒ ★ 这不是新增一条测试，而是**替换整个度量方式**。

**架构授权**：ADR-089（✅ Accepted 2026-10-26）+ ADR-088 v1.4 §10（✅ Accepted 2026-10-26）。
**载体 objective**：`objective-backend-parity-and-single-model`（阶段 A/B/C 全程）· **阶段 B/C 载体**：`feat-tlm-backend-retirement`。

## 现状（2026-10-26 实测）

| 产物 | 跑哪个后端 | 比较对象 |
|---|---|---|
| vendored ELF 47 | **仅 CppHDL Simulator** | 只判 `tohost == 1` |
| Verilator 48 ELF | **仅 Verilator** | 只判 `tohost == 1` |
| 覆盖平价台账 463 条 | **不跑仿真** | 数「有没有对应 case 名」 |
| `[mem-drift]` 锚 | Simulator | ★ **唯一的跨后端断言**（只钉 `ch_mem::aread` 的 0-cycle） |

⇒ ★ **两后端从不互比** ⇒ 「两边都 `tohost==1` 但状态不同」的模型**今天完全不可见**。

## 完成条件

- [ ] ★ **G-A0 门禁** —— ★ **parity 架构（ADR-089）+ 本路线图经 Oracle + Metis 双审通过**（★ 2026-10-26 用户裁决：**不立即开工**）
- [x] **A1 契约落盘**（零代码）—— ✅ **已落（2026-10-26）** ⇒ 契约正文 = `docs/architecture/parity-gate-contract.md`（**S-1…S-16**，每条含「要求 / 验收方式 / 归属」）+ `openspec/changes/parity-gate-spec/`（proposal / design / tasks）
- [x] ★ **G-A1 门禁**（architect 签）✅ **2026-10-26**：**C-1 = ERROR(2)**（`determinism_mismatch`）· **C-2 = `init_semantics = ctor-init`**（★ 实测修正：原「两侧都 reset_mode=none」不可行 —— 生成的 Verilog 只在复位分支加载 init，跳过脉冲则 `pc` 从 0 起；而 `Simulator::reset()` 只清零。⇒ Simulator 不 reset / Verilator 打 1 拍 reset，语义等价，3/3 ELF 逐位对齐）
- [x] **A2a 可观测性** —— ✅ **2026-10-26 完成（3 个增量）**，★ **5/5 corpus 实测 5 项观测量 + 确定性判据两侧逐位相等**（3 ELF + branchbench m0/m2）⇒ ★ 只差「既有基线零退化」的抽样验证（`[verilator]` / `[vendored-elf]`跑批中）⇒ change `backend-parity-observability`（proposal/design/tasks） —— ★ **Simulator 侧通用 ELF runner**（CLI 对齐 `cpu_verilator_sim`）⇒ ★ 现状 Simulator 侧唯一 ELF 入口是 Catch2 硬编码 47 项表 ⇒ ★ 两侧 5 观测量采集（★ ③ 内存签名 = **DUT 自算写 mailbox**，**不是** host dump ⇒ `Simulator::get_memory()` 是空 stub）⇒ ★ 逐拍 PC 采样 MUST 用 `get_signal_id_by_name` + `data_map()` 直读（禁 per-cycle 线性扫描）⇒ ★ **样例 JSON 证据**（两侧各一份）
- [x] **A2b 比对器** —— ✅ **2026-10-26 完成**：`tools/parity/parity_gate.py`（纯 stdlib）+ `tests/cpu/parity/corpus.txt`（51 case，单一来源）+ `exceptions.json`（仅常驻合成例外）⇒ ★ **PR 层 8/8 PASS** · `--self-test` **9/9** 鉴别力 · **负向 6/6**（fail-closed）· ★ 契约 §7 覆盖矩阵 16 条已填实 · change `backend-parity-harness` —— ★ **确定性判据先行**（node_count + 信号名单 hash）⇒ ②③⑤ bit-exact + ⑤ 逐拍 PC 流 + ④ 总拍数 ⇒ ★ fail-closed（**≥5 例**，每观测量一例）⇒ ★ 三态退出码（FAIL=分歧 / ERROR=门禁故障）⇒ ★ 变异自检（**≥2 例**：改 1 拍 + 改 1 字节内存写）⇒ ★ 白名单校验脚本（5 字段 + 期限未过期 + 作用域）⇒ ★ **S-编号覆盖矩阵**（★ A1「粘住」机制）
- [ ] **A3 语料接入 + 首跑** —— ★ **两侧交集 47 ELF**（★ Verilator 侧另有 `rv32ui-p-simple` ⇒ 补进 Simulator 侧 1 行）+ branchbench 4 档（★ **Simulator 侧 runner 依赖 A2a**）⇒ ★ **两侧 corpus 抽成单一清单文件**（门禁断言集合相等）⇒ ★ 产出**既存后端分歧清单**（分类：上游对齐 / 有意例外 / 真 bug）⇒ 清单归零或全部显式登记
- [ ] **A4 门禁接线 + CI** —— 进 `tools/run_chipforge_tests.sh` + ★ **CI 具备 build 能力**（= ADR-088 §5.2 判据 4/7 落点）+ 分层 corpus（PR 快子集 + 夜间全量）+ ★ **单元级快车道**（不依赖 TLM 存活的本地快环）+ ★ **Verilog 构建产物按 config 缓存**
- [ ] ★ **阶段 A 门禁 G-A3 通过** ⇒ ★ 这是 **ADR-088 §5.2 删除判据 6** 的唯一来源 ⇒ ⇒ **未通过 ⇒ 不得删任何 TLM-only case**

## 观测量集（ADR-089 D2）

| # | 观测量 | 强度 |
|---|---|---|
| ① | `tohost` | bit-exact |
| ② | 最终寄存器堆（`x1..x31`） | bit-exact |
| ③ | 内存镜像 hash | bit-exact |
| ④ | **总 cycle 数（标量）** | bit-exact（★ 标量无「逐拍」可言 ⇒ 逐拍语义归 ⑤） |
| ⑤ | ★ **逐拍采样 PC 流 hash** | bit-exact（★ **逐拍判据的载体**；轨迹定义归 A1 细则） |
| ③ | ★ **DUT 自算内存签名（mailbox）** | bit-exact（★ **不是** host dump ⇒ `Simulator::get_memory()` 是空 stub） |
| ⑤ | PC 轨迹 hash | bit-exact |

⇒ ★ **fail-closed**：任一观测量缺失 / 不可采集 / 被截断 ⇒ **FAIL**（MUST NOT 跳过或降级）。

## 依赖

- ★ **前置**：ADR-089（已 Accepted）· `objective-backend-parity-and-single-model`
- ★ **不依赖**：`feat-bp-btb-gshare` 主线（阶段 A 独立 ⇒ 可并行；★ A3 的跑批可与 BP 增量 3 的量收益**合并执行**省一半机时）
- ★ **被依赖**：`feat-tlm-backend-retirement` 的 **B3 阶段 C**（删除判据 6）

## 风险

| # | 风险 | 兜底 |
|---|---|---|
| R1 | A3 首跑照出大量既存分歧 ⇒ 工期失控 | 分三类处置（上游对齐 / 有意例外 / 真 bug）⇒ ★ ≤ 2 个工作日出分类结论，不得挂起 |
| R2 | cycle 逐拍相等要求上游多轮对齐 | 例外白名单 + 上游 issue + 收敛期限 ⇒ ★ **MUST NOT** 改用容差 |
| R4 | ★ 门禁自身成为新的假绿通道 | 变异自检（V-3）+ fail-closed（V-2）+ 白名单合规校验（V-4）⇒ ★ 随 A2 交付，不得后补 |

## 关联

- **ADR-089** · **ADR-088 v1.4 §10** · ADR-083 · ADR-040 v3.0
- `objective-backend-parity-and-single-model` · `feat-tlm-backend-retirement` · `objective-ip-cpu-tlm-backend-retirement`
- `.rddf/improvements/backend-parity-gate-spec.md`（A1 立项草案）

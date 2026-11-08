---
initiative: wave7-architecture-governance
priority: P1
version_target: v0.11.0
status: in-progress
created_from: .rddf/roadmap/objectives/objective-backend-parity-and-single-model.md
adr:
  - ADR-089
  - ADR-088
depends_on:
  - backend-parity-corpus
---

# backend-parity-ci —— 差分门禁接线 + CI 具备 build 能力（阶段 A4）

> ★ **本 change = 阶段 A 的最后一段**（★ A4）⇒ ★ **★ 它是 ADR-088 §5.2 判据 4 + 判据 6 的唯一落点**（★ 阶段 C 删 TLM 的硬前置①②）⇒ ★ **不完成 ⇒ 不得删任何 TLM-only case**。
> ★ ★ **权威判据**（★ 四处一致，★ 本 change 是它们共同要求的实现）：★ `.rddf/roadmap/objectives/objective-backend-parity-and-single-model.md:86`（A4 拆解）· ★ 同文件 `:142`（G-A4 完成判据）· ★ `docs/architecture/adr/ADR-088-tlm-backend-retirement.md:148,151`（判据 4/7）· ★ `.rddf/roadmap/features/feat-backend-parity-gate.md:36`（A4 条目）。

## Why

★ ★ **病根（★ 实测，非推断）**：★ **CI 不构建项目** ⇒ ★ 凡依赖构建产物的门禁 step **恒 SKIP**。

| 现象 | ★ 实测证据 |
|---|---|
| ★ **CI 零构建** | ★ `.github/workflows/` 仅 2 个 workflow（★ `architecture-gates.yml` + `doc_check.yml`）· ★ **无任何 `cmake --build` 步骤** |
| ★ **门禁 step 恒 SKIP** | ★ `architecture-gates.yml:163-166`（`measure_perf.sh`）· ★ `:179-182`（`measure_branchbench.sh`）⇒ ★ **均以「产物不存在 ⇒ SKIP」短路**（★ 自身注释写明「未构建 CH_MEM binary」） |
| ★ **判据 4/6 退化为「本机成立」** | ★ `ADR-088:151` 明写：★ 「差分门禁若不在 CI 中执行 ⇒ 判据 4 与判据 6 都会退化为『本机成立』」 |
| ★ **门禁本机可跑**（★ ⇒ **缺的只是接线**） | ★ **实测**：`python3 tools/parity/parity_gate.py --layer pr` ⇒ **8 case 全 PASS · `verdict=PASS` · EXIT=0**（★ 本轮 2026-11-08 实测） |
| ★ **分层 corpus 已实现**（★ 无需新做） | ★ `--layer pr\|night\|all`（★ `parity_gate.py:553`）· ★ 清单 `tests/cpu/parity/corpus.txt` **61 行**含 `layer` 列（★ S-13 单一来源） |
| ★ **单元级快车道底座已实现**（★ 无需新做） | ★ `parity_gate.py --self-test` ⇒ **实测 16/16 PASS**（★ 纯函数 · **不碰文件、不跑进程** ⇒ **不依赖构建**） |

⇒ ★ ⇒ ★ **A4 的真实工作量 = 「接线」而非「实现」**：★ 4 个子项中 **2 个已有底座**（③ 单元级快车道 · ④ 见下）· ★ 2 个需新建（① 接线 · ② CI build）。

## What Changes

**In Scope**:

- ★ **AC-1** 差分门禁接进 `tools/run_chipforge_tests.sh`（★ `--parity` / `--parity-layer`）
- ★ **AC-2** CI 具备 build 能力（★ 新 workflow job · ★ 检出 CppTLM/CppHDL · ★ cmake configure+build · ★ **跑 parity 门禁** · ★ **回读结果**）
- ★ **AC-3** 分层 corpus 的**时长数据采集 + 分层表入库**（★ 契约 S-15：★ 「A4 MUST 先把实测 p95 写进报告 `timings`，再据此定阈值」）
- ★ **AC-4** 单元级快车道接线（★ `--self-test` ⇒ **零构建依赖** ⇒ PR 级可跑）
- ★ **AC-5** 门禁结论的 **CI 回读**（★ verdict + error_kind ⇒ **机器可读** ⇒ 供 `ADR-088` 判据 4/6 引用）

**Out of Scope**:

- ★ **删 TLM 模式**（★ 阶段 C ⇒ 依赖本 change + 其余 4 条硬前置）
- ★ **Verilog 跨 ELF 缓存**（★ ★ **已被实测推翻**：★ `parity-gate-contract.md:383-388` 实测「每个 ELF 的网表都不同（指令内存烤进网表）⇒ 跨 ELF 无可复用」⇒ ★ 现实约束 = 「**每 corpus 一次编译**」⇒ ★ 已登记为**开放项 O-5**（★ 需 DUT/上游级改动）⇒ ★ **★ MUST NOT 把「按 config 缓存」写成契约**（★ A4 拆解原文已明禁）
- ★ **门禁阈值硬编码**（★ S-15：★ 「本期不设未实测的硬阈值」⇒ ★ 本 change 只**采集** p95，**不定**阈值）
- ★ **夜间层进 PR 门禁**（★ PR 层 = 8 case ≈ **6.5 min**；★ 夜间 52 case ≈ **45~60 min** ⇒ ⇒ 夜间层**只**作定时任务）

### ★ ★ 子项拆解（★ 依据 `objective-backend-parity-and-single-model.md:86`）

| # | 子项 | ★ 现状（实测） | ★ 本 change 的动作 |
|---|---|---|---|
| ★ ① | ★ **差分门禁接进 `tools/run_chipforge_tests.sh`** | ★ 该脚本 `parity` 命中 **0** | ★ 加 `--parity` / `--parity-layer` · ★ **默认不跑**（★ 保持既有调用语义不变）· ★ 跑时**委托 `parity_gate.py`**（★ 不重实现判定逻辑） |
| ★ ② | ★ **CI 具备 build 能力** | ★ CI 零 build | ★ 新增 workflow job：★ checkout + checkout CppTLM/CppHDL + ★ **cmake configure + build**（★ 只 build 必需 target）· ★ **跑 parity 门禁**（★ PR 层）· ★ **回读 verdict** |
| ★ ③ | ★ **分层 corpus**（PR 快子集 + 夜间全量） | ★ ★ **已实现**（★ `--layer` + 清单 `layer` 列） | ★ ★ **不重做** ⇒ ★ 只做 **③ 的时长采集 + 分层表入库**（★ 见 AC-3） |
| ★ ④ | ★ **单元级快车道**（不依赖 TLM 存活的本地快环） | ★ ★ **底座已实现**（★ `--self-test` 16/16） | ★ 接线到 `tools/run_chipforge_tests.sh`（★ **零构建依赖** ⇒ PR 级可跑） |
| ★ ⑤ | ★ **Verilog 按 config 缓存** | ★ ★ **已被实测推翻** | ★ **★ 不实现**（★ 登记为 O-5 开放项 ⇒ ★ 归属上游） |

## Capabilities

- ★ **判据 4 满足**：★ 「CI 有可回归的自动测试」⇒ ★ **完成形式 = 差分门禁在 CI 中被执行且结果被回读**（★ `ADR-088:148`）
- ★ **判据 7 满足**：★ 「CI 具备 build 能力」⇒ ★ `ADR-088:151`
- ★ **判据 6 可判定**：★ 「双后端差分 parity 门禁已上线且全绿」⇒ ★ 门禁结论从「本机成立」变为「**CI 可回读**」
- ★ **阶段 C 解锁**：★ 硬前置①②（① 门禁绿 · ② CI build）⇒ ★ **本 change 是阶段 C 的关键路径**
- ★ **PR 反馈变快**：★ 单元级快车道（★ 零构建）⇒ ★ PR 级**秒级**反馈（★ 不等 6.5 min 的双后端跑批）

## Impact

- ★ **修改**：`tools/run_chipforge_tests.sh`（★ 加 3 个 flag · ★ 默认行为不变）· `tools/parity/parity_gate.py`（★ **只加 timings 采集** · ★ **不改判定逻辑** ⇒ ★ S-1~S-17 契约不变）
- ★ **新增**：`.github/workflows/parity-gate.yml`（★ 或在 `architecture-gates.yml` 加 job ⇒ ★ **决策点见下**）
- ★ **不动**：★ 门禁的判定语义（★ S 契约）· ★ corpus 清单（★ S-13 单一来源）· ★ 观测量的定义（★ ADR-089 D2）
- ★ ★ **CI 成本**：★ **PR 层 ≈ 6.5 min**（★ 实测 `jobs=4`）· ★ **加 build 时间**（★ CppTLM/CppHDL 首次构建 ≈ 数分钟 ⇒ ★ CI 无缓存时更久）· ★ ⇒ ★ **故 build job 宜设 `timeout-minutes` 上限**（★ 防止挂死占额度）
- ★ ★ **CI build 的已知脆性**：★ `CMakeLists.txt:108-122` 的 ExternalProject 隔离构建 ⇒ ★ **首次 build 需从 `CppTLM/` `CppHDL/` 源码构建 + install** ⇒ ★ CI 已 checkout 这两个仓（★ `architecture-gates.yml:101-111`）⇒ ★ 路径就绪 ⇒ ★ **但需实测 CI 上的真实耗时**
- ★ ★ **Verilator 依赖**：★ 差分门禁的 Verilator 侧需 `cpu_verilator_sim` ⇒ ★ CI 需装 Verilator（★ `architecture-gates.yml:50-63` 已有安装逻辑 ⇒ ★ 可复用）

## Acceptance Criteria

> ★ ★ **本段 = AC 权威**（★ `rdd-verifier` 从此段提取：★ `rdd-verifier/SKILL.md:148`）⇒ ★ **段头用 `## Acceptance Criteria`**（★ 正则只认 `验收标准|Acceptance Criteria`：★ 同文件 `:153`）。

- [ ] **AC-1** ★ **差分门禁接线到 `tools/run_chipforge_tests.sh`**（★ 子项①）⇒ ★ 判据：★ ① 支持 `--parity`（★ 跑 PR 层）与 `--parity-layer pr\|night\|all`；★ ② **默认不跑**（★ 不加 flag 时行为与改动前**逐字节一致**）；★ ③ 委托 `tools/parity/parity_gate.py`（★ **不重实现判定逻辑** ⇒ ★ S 契约不变）；★ ④ 门禁 **PASS ⇒ 退出码 0** · ★ **FAIL ⇒ 1** · ★ **ERROR ⇒ 2**（★ 与 `parity_gate.py:718` 的退出码映射一致）；★ ⑤ `--help` 列出新 flag。★ **覆盖 S-1~S-17**
- [ ] **AC-2** ★ **CI 具备 build 能力**（★ 子项② · ★ ADR-088 判据 7）⇒ ★ 判据：★ ① CI 有 **cmake configure + build** 步骤（★ 检出 CppTLM/CppHDL 到既有路径）；★ ② 构建出 `parity_sim_runner` 与 `cpu_verilator_sim`（★ **两个 runner 都要**）；★ ③ **build job 有 `timeout-minutes` 上限**；★ ④ Verilator 可用；★ ⑤ **实测**：★ CI 上从零 build 到门禁可跑**全流程通**。★ **覆盖 ADR-088 §5.2 判据 7**
- [ ] **AC-3** ★ **门禁结论在 CI 中被执行且结果被回读**（★ 子项①+② 的**完成形式** · ★ ADR-088 判据 4）⇒ ★ 判据：★ ① CI 中 parity 门禁 **被实际执行**（★ **非 SKIP** ⇒ ★ 产物存在性检查**不得**短路）；★ ② **verdict 被回读**（★ `report.json` 的 `verdict` + `error_kind` ⇒ ★ **机器可读** ⇒ ★ 上传为 artifact）；★ ③ **PR 事件可见**（★ 结论以 step 结论 + artifact 两种形式呈现）；★ ④ **判据 4 的完成形式逐字满足**（★ `ADR-088:148`「差分门禁在 CI 中被执行且结果被回读」）。★ **覆盖 ADR-088 §5.2 判据 4**
- [ ] **AC-4** ★ **分层 corpus 的时长数据采集 + 分层表入库**（★ 子项③）⇒ ★ 判据：★ ① `parity_gate.py` 的 `timings` 字段**从空占位 `{}` 变为实测填充**（★ **per-case 时长** + ★ **p50/p95**）；★ ② 清单的 `layer` 列与 `--layer` 的选集**机检相等**（★ S-15「分层集合 ⊆ S-13 清单」）；★ ③ 实测 p95 **写进报告**（★ S-15「A4 MUST 先把实测 p95 写进报告 timings，再据此定阈值」）；★ ④ ★ **MUST NOT 写死未实测的硬阈值**（★ S-15）。★ **覆盖 S-15**
- [ ] **AC-5** ★ **单元级快车道接线**（★ 子项④）⇒ ★ 判据：★ ① `tools/run_chipforge_tests.sh` 可触发 `parity_gate.py --self-test`（★ **零构建依赖**）；★ ② 实测 **16/16 PASS**（★ 与本 change 开工前的基线一致）；★ ③ **PR 级可跑**（★ 不需 cmake configure/build）；★ ④ 退出码语义与 AC-1 一致。★ **覆盖 ADR-089 D2（判别力自检）**

> ★ ★ **本 change 与 G-A4 完成判据的对应**（★ `objective-backend-parity-and-single-model.md:142`）：
> ★ 「① 门禁在 CI 中被执行且结果被回读」⇒ **AC-3** · ★ 「② 分层 corpus 的时长与分层表入库」⇒ **AC-4** · ★ 「③ 单元级快车道在 PR 级可跑」⇒ **AC-5** · ★ 另 **AC-1 + AC-2** 是前两者的**实现前提**。
> ★ ★ **★ 工期**：★ 约 **1~2 人日**（★ `objective:89` 的双审修正值）⇒ ★ **但 CI build 的真实耗时未知**（★ 首次从源码构建 CppTLM/CppHDL）⇒ ★ **AC-2⑤ 是唯一的耗时风险点**。

## References

- ★ `.rddf/roadmap/objectives/objective-backend-parity-and-single-model.md`（★ `:86` A4 拆解 · `:89` 工期 · `:142` G-A4 判据）
- ★ `.rddf/roadmap/features/feat-backend-parity-gate.md`（★ `:36` A4 条目）
- ★ `docs/architecture/adr/ADR-088-tlm-backend-retirement.md`（★ `:148` 判据 4 · `:151` 判据 7 · §10.3 阶段 C 硬前置）
- ★ `docs/architecture/adr/ADR-089-backend-parity-differential-gate.md`（★ D2 观测量 · D6 迁移路径）
- ★ `docs/architecture/parity-gate-contract.md`（★ S-13 语料单一来源 · S-15 运行时预算与分层 · S-16 粘住机制 · GN-1 通用总则 · O-5 开放项）
- ★ `openspec/changes/backend-parity-corpus/first-run-evidence.md`（★ A3 首跑证据 · 52/52 PASS）

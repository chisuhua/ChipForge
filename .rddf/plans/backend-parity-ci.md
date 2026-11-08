# A4 实施路径（差分门禁接线 + CI build 能力）

> **change**: `backend-parity-ci`（★ **本 plan 起草时已立项** ⇒ `openspec/changes/backend-parity-ci/`）
> **本文件**: A4 的可执行路径（repo 内 SSOT）★ ★ **AC 权威 = change 的 `proposal.md` 的 `## Acceptance Criteria` 段**（★ 与 `rdd-verifier` 提取源一致：★ `rdd-verifier/SKILL.md:148`）⇒ ★ **本 plan 是执行路径，不是 AC 权威**
> **架构授权**: **ADR-089**（双后端差分 parity 门禁，✅ Accepted v1.4）· **ADR-088 v1.4**（TLM 后端退役，✅ Accepted）· **ADR-083**（Plugin-style 单一 source of truth）
> **门禁契约**: `docs/architecture/parity-gate-contract.md`（★ S-1~S-17 · ★ GN-1 通用总则）⇒ ★ **本 plan MUST NOT 改判定语义**
> **规模**: 中（★ **约 1~2 人日** · ★ `objective-backend-parity-and-single-model.md:89` 的双审修正值）★ ★ **但 CI build 的真实耗时未知**（★ 唯一风险点）
> **前置**: ✅ A1 契约落盘（`parity-gate-spec` 32/32）· ✅ A2a 可观测性（29/29）· ✅ A2b 比对器（25/26）· ✅ A3 语料接入 + 首跑（23/25 · ★ **52/52 PASS** · ★ 分歧清单 = 空）· ✅ `ADR-089` Accepted · ✅ `ADR-088` Accepted
> **产出**: ★ **G-A4 通过** ⇒ ★ **阶段 C（删 TLM）硬前置①② 解锁**

---

## Goal

★ 把**已经能跑的门禁**接进**两个入口**（★ 本机 + CI）⇒ ★ 让 `ADR-088 §5.2` 的**判据 4**（CI 有可回归的自动测试）与**判据 7**（CI 具备 build 能力）从「本机成立」变成「**CI 可回读**」。

★ ★ **★ 本 change 的本质 = 接线，不是实现**（★ 实测基线）：

| 能力 | ★ 现状（2026-11-08 实测） | 本 plan 的动作 |
|---|---|---|
| ★ 门禁判定 | ★ ✅ **已实现**（★ `parity_gate.py` · ★ S-1~S-17） | ★ **不改判定语义** |
| ★ 分层 corpus | ★ ✅ **已实现**（★ `--layer pr\|night\|all` + ★ 清单 61 行 `layer` 列） | ★ 只加 **timings 采集** |
| ★ 单元级快车道 | ★ ✅ **底座已实现**（★ `--self-test` 实测 **16/16 PASS**） | ★ 只**接线** |
| ★ 门禁接线 | ★ ❌ `run_chipforge_tests.sh` 的 `parity` 命中 **0** | ★ ★ **新建** |
| ★ CI build | ★ ❌ CI **零构建** ⇒ ★ `measure_perf.sh` / `measure_branchbench.sh` **恒 SKIP** | ★ ★ **新建** |
| ★ Verilog 跨 ELF 缓存 | ★ ❌ **已被实测推翻**（★ 每 ELF 网表不同） | ★ **不实现** ⇒ 开放项 O-5 |

★ ★ **实测基线（★ 本 plan 的回归锚）**：
- ★ `python3 tools/parity/parity_gate.py --layer pr` ⇒ **8 case 全 PASS · `verdict=PASS` · EXIT=0**
- ★ `python3 tools/parity/parity_gate.py --self-test` ⇒ **16/16 PASS · EXIT=0**
- ★ `bash tools/run_chipforge_tests.sh --help` ⇒ **不含** `--parity`（★ 改动前基线）

---

## ★ Phase 划分（★ 5 阶段 · ★ 每阶段 = 一个可独立验收的增量）

| Phase | 内容 | AC | ★ 前置 |
|---|---|---|---|
| ★ **P1** | ★ **timings 采集**（★ `parity_gate.py` 加 per-case 时长 + p50/p95） | **AC-4** | — |
| ★ **P2** | ★ **本机接线**（★ `run_chipforge_tests.sh` 加 `--parity` / `--parity-layer` / `--parity-self-test`） | **AC-1** · **AC-5** | — |
| ★ **P3** | ★ **CI build job**（★ checkout deps + cmake + build 两个 runner） | **AC-2** | — |
| ★ **P4** | ★ **CI 门禁执行 + 结果回读**（★ 非 SKIP + artifact 上传） | **AC-3** | ★ P3 |
| ★ **P5** | ★ **收口**（★ G-A4 判据逐条核 + 记账回填 roadmap） | 全 | ★ P1~P4 |

★ ★ **★ P1 与 P2 可并行**（★ 改的文件不同：★ `tools/parity/parity_gate.py` vs ★ `tools/run_chipforge_tests.sh`）⇒ ★ **但 P2 的 `--parity-layer` 依赖 P1 的 timings 输出格式**（★ 只影响报告内容，不影响接线）⇒ ★ **P1 先做更省返工**。

---

## 1. ★ P1 — timings 采集（★ 只加字段 · ★ 不改判定）

### 1.1 ★ 现状（★ 实测）

`tools/parity/parity_gate.py:713` ⇒ `"timings": {}`（★ **空占位**）⇒ ★ 实测 `build/parity/report.json` 的 `timings` = `{}`（★ **确认为空**）。

★ `per_case` 的每条记录**无时长字段**（★ 实测字段集 = `name` / `determinism` / `observations` / `completion` / `vacuous_observations` / `first_divergence` / `verdict` / `error_kind` / `differences` / `exceptions_applied` / `cap` / `timeout_s`）⇒ ★ **时长需新增采集**。

### 1.2 ★ RED（★ 先写失败断言）

★ **★ 判据**：★ `parity_gate.py --layer pr` 跑完后 ⇒ ★ `report.json` 的 `timings` **非空** ⇒ ★ 且含 **p50** / **p95** ⇒ ★ 且 `per_case[*]` 每条含**自己的**时长。

★ 实测当前形态（★ RED 的证据）：★ `timings` = `{}` ⇒ ★ **RED 成立**。

### 1.3 ★ GREEN（★ 最小改动）

★ 在跑每个 case 时**记录墙钟时长**（★ `time.monotonic()` 差值）⇒ ★ 汇总：
```
"timings": {
  "per_case": {"<name>": <seconds>, ...},
  "count": <n>,
  "p50": <seconds>, "p95": <seconds>,
  "total": <seconds>
}
```
★ ★ **MUST NOT**：★ 改动任何判定逻辑（★ 退出码映射 · verdict 计算 · 差异比较）⇒ ★ **S-1~S-17 契约不变**。

### 1.4 ★ 验证

★ ① `python3 tools/parity/parity_gate.py --layer pr` ⇒ ★ **verdict 仍 = PASS · 退出码仍 = 0**（★ **零回归**）★ ② `report.json` 的 `timings` **非空**且含 p50/p95 ★ ③ `per_case[*]` 含时长 ★ ④ **★ MUST NOT 写死硬阈值**（★ S-15「本期不设未实测的硬阈值」）★ ⑤ 实测 p95 **写进报告**（★ S-15 的 A4 要求）。

---

## 2. ★ P2 — 本机接线（★ 默认行为不变 · ★ 委托不重实现）

### 2.1 ★ 现状（★ 实测）

`tools/run_chipforge_tests.sh`（★ 237 行）⇒ ★ `parity` 命中 **0** ⇒ ★ 参数解析在 `:77-136` ⇒ ★ `--help` 走 `sed -n '2,29p'`（★ **★ 注意：改头部会连带改 `--help` 的行号范围** ⇒ ★ 改动须同步）。

### 2.2 ★ RED

★ **★ 判据**：★ `run_chipforge_tests.sh --help` ⇒ ★ 输出含 `--parity` ⇒ ★ 且 **不**含 `--parity` 时脚本行为与改动前**一致**。

### 2.3 ★ GREEN（★ 3 个 flag）

| flag | 语义 | 默认 |
|---|---|---|
| `--parity` | ★ 跑差分门禁（★ **PR 层**） | **关** |
| `--parity-layer <pr\|night\|all>` | ★ 跑哪一层 | `pr` |
| `--parity-self-test` | ★ 跑**单元级快车道**（★ `--self-test` · ★ **零构建依赖**） | **关** |

★ ★ **MUST**：
- ★ **默认不跑** ⇒ ★ 不加 flag 时**行为逐字节一致**（★ 既有 CI / 开发者调用不受影响）
- ★ **委托** `tools/parity/parity_gate.py`（★ **不重实现判定逻辑** ⇒ ★ S 契约不变）
- ★ **退出码映射**：`PASS⇒0` / `FAIL⇒1` / `ERROR⇒2`（★ 与 `parity_gate.py:718` 一致）
- ★ ★ **`--parity-self-test` MUST NOT 依赖构建**（★ ⇒ ★ 排在 build 步骤**之前**执行 ⇒ ★ 否则「零构建依赖」失效）
- ★ `--help` 同步更新（★ 含新 flag 的用法行）

### 2.4 ★ 验证

★ ① `--help` 含 3 个新 flag ★ ② **不加 flag** ⇒ ★ 与改动前行为一致（★ 跑一次默认路径对比退出码）★ ③ `--parity-self-test` ⇒ ★ **16/16 PASS · EXIT=0** · ★ 且 **不触发 build** ★ ④ `--parity` ⇒ ★ 8 case PASS · EXIT=0 ★ ⑤ `--parity` 故意失败（★ `--only` 一个不存在 case）⇒ ★ 退出码 **≠ 0**（★ fail-closed）

---

## 3. ★ P3 — CI build job（★ 判据 7）

### 3.1 ★ 现状（★ 实测）

★ `.github/workflows/` 仅 2 个 workflow ⇒ ★ **无任何 `cmake --build`**。
★ `architecture-gates.yml:101-111` ⇒ ★ **已** checkout CppTLM / CppHDL 到 `${workspace}/CppTLM` / `${workspace}/CppHDL` ⇒ ★ ★ **路径就绪 ⇒ build 可行**。
★ `architecture-gates.yml:50-63` ⇒ ★ **已**有 Verilator 安装逻辑（★ noble apt / jammy 源码 build）⇒ ★ **可复用**。
★ `architecture-gates.yml:163-166` / `:179-182` ⇒ ★ `measure_perf.sh` / `measure_branchbench.sh` 因产物不存在而 **SKIP** ⇒ ★ **这正是判据 7 的病根**。

### 3.2 ★ RED

★ **★ 判据**：★ CI workflow 文件中**存在** `cmake --build` 步骤 ⇒ ★ 且**构建产出** `build/bin/parity_sim_runner` 与 `build/bin/cpu_verilator_sim`（★ **两个都要**）。

### 3.3 ★ GREEN

★ **决策点**：★ **新建** `.github/workflows/parity-gate.yml` ★ **vs** ★ 在 `architecture-gates.yml` 加 job。
★ ⇒ ★ **选新建独立 workflow**（★ 理由：★ ① 触发条件不同（★ 门禁须在**所有** PR 上跑，★ 而 architecture-gates 有 `paths` 过滤）★ ② timeout / 资源可独立控 ★ ③ **不污染**现有 PR 阻塞门禁的既有时序（★ `architecture-gates.yml` 的 4 个阻塞脚本**已**是 PR 门禁 ⇒ ★ 加 build job 会**显著拉长**每个 PR 的门禁时长）★ ④ 夜间层可挂在**定时触发**上（★ 与 PR 层分离）。

★ 步骤：
1. ★ `actions/checkout@v4`（ChipForge）
2. ★ `actions/checkout@v4` ×2（CppTLM / CppHDL ⇒ ★ 复用 `architecture-gates.yml:101-111` 的路径与 `vars.*` 约定）
3. ★ 装 Verilator（★ 复用 `architecture-gates.yml:50-63`）
4. ★ `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` ⇒ ★ `cmake --build build --target parity_sim_runner cpu_verilator_sim`（★ **只 build 两个 runner** ⇒ ★ 不 build 686 个 TLM case ⇒ ★ 省时长）
5. ★ `timeout-minutes` 上限（★ 防止挂死）
6. ★ **不**跑 `run_chipforge_tests.sh`（★ 门禁在 P4 单独跑）

### 3.4 ★ 验证

★ ① workflow **含** `cmake --build` ★ ② **只** build 两个 runner target（★ 不 build `chipforge_tests`）★ ③ 有 `timeout-minutes` ★ ④ Verilator 安装步骤存在 ★ ⑤ ★ **CI 真实耗时待实测**（★ 本地无法验证 ⇒ ★ **★ 这是本 change 的唯一外部依赖风险** ⇒ ★ 需在 PR 上跑一次真实 CI）

---

## 4. ★ P4 — CI 门禁执行 + 结果回读（★ 判据 4 的完成形式）

### 4.1 ★ RED

★ **★ 判据**：★ CI 中 parity 门禁 **被实际执行**（★ **非 SKIP**）⇒ ★ 且 `report.json` 的 `verdict` **被回读**（★ artifact 上传 + step 结论）。

### 4.2 ★ GREEN

★ ① **跑门禁**：★ `python3 tools/parity/parity_gate.py --layer pr`（★ **PR 层** = 8 case ≈ **6.5 min** 实测）
★ ② **回读**：★ 从 `build/parity/report.json` 读 `verdict` + `error_kind` ⇒ ★ **机器可读**
★ ③ **可见**：★ ★ **MUST NOT** 让「产物不存在 ⇒ SKIP」短路（★ ★ 这正是 `architecture-gates.yml:163-166` 的病根 ⇒ ★ **本步是判据 4 的核心**）⇒ ★ 产物缺失 ⇒ **FAIL loudly**，不 SKIP
★ ④ **artifact**：★ `actions/upload-artifact@v4` 上传 `build/parity/report.json`
★ ⑤ ★ **PR 级 vs 夜间级分离**：★ PR 跑 `pr` 层；★ 夜间跑 `night` 层（★ **定时触发** · ★ 52 case ≈ **45~60 min**）⇒ ★ 二者**同一个** workflow（★ 用不同 trigger + layer 参数）
★ ⑥ **失败即阻塞**：★ 门禁 FAIL ⇒ step 失败 ⇒ 阻止 PR merge（★ **判据 6「门禁已上线且全绿」的前置**）

### 4.3 ★ 验证

★ ① CI 上门禁 **非 SKIP** ★ ② `verdict` + `error_kind` **出现在 artifact 中** ★ ③ 故意制造分歧 ⇒ CI **红** ★ ④ 夜间层**定时**可跑（★ 手动触发亦可）★ ⑤ ★ **★ 判据 4 的完成形式逐字满足**（★ `ADR-088:148`）

---

## 5. ★ P5 — 收口（★ G-A4 逐条核 + 记账）

### 5.1 ★ G-A4 完成判据逐条核（★ `objective-backend-parity-and-single-model.md:142`）

| ★ G-A4 判据 | ★ 由哪个 AC 满足 | ★ 证据落点 |
|---|---|---|
| ★ 「门禁在 CI 中被执行且结果被回读」 | ★ **AC-3** | ★ CI run 链接 + artifact |
| ★ 「分层 corpus 表」 | ★ **AC-4** | ★ `report.json` 的 `timings` + 清单 `layer` 列 |
| ★ 「单元级快车道可跑」 | ★ **AC-5** | ★ `--parity-self-test` 输出 |

### 5.2 ★ 记账回填

★ ① `.rddf/roadmap/features/feat-backend-parity-gate.md:36` ⇒ ★ A4 勾选 + 完成日期
★ ② `.rddf/roadmap/objectives/objective-backend-parity-and-single-model.md:142` ⇒ ★ G-A4 勾选
★ ③ `openspec/changes/backend-parity-ci/tasks.md` ⇒ ★ 全部勾选
★ ④ ★ **★ 判据 4/6/7 的引用更新**：★ `ADR-088` 的状态行**不由 builder 改**（★ 属 architect）⇒ ★ 只在**审稿报告 / roadmap** 记录「判据 4/7 的完成形式已满足」

---

## 6. ★ 风险与边界

| # | 风险 | ★ 缓解 |
|---|---|---|
| ★ **R-A4-1** | ★ **CI build 真实耗时未知**（★ 首次从源码构建 CppTLM/CppHDL）⇒ ★ 可能超 CI 时限 | ★ ① **只 build 两个 runner target**（★ 不 build 686 case）★ ② `timeout-minutes` 上限 ★ ③ **★ 这是唯一外部依赖风险** ⇒ ★ 需真实 CI run 验证 |
| ★ **R-A4-2** | ★ **「SKIP 短路」复发**（★ `architecture-gates.yml:163-166` 的病根）⇒ ★ 门禁在 CI 里又变成 SKIP | ★ **★ AC-3③ 明写：产物缺失 ⇒ FAIL loudly，MUST NOT SKIP** |
| ★ **R-A4-3** | ★ **Verilator 装不上 / 版本过低** | ★ 复用 `architecture-gates.yml:50-63`（★ 已验证的安装路径） |
| ★ **R-A4-4** | ★ **改了判定逻辑 ⇒ 破坏 S 契约** | ★ **★ P1 明写：只加 timings 字段，不改判定** ⇒ ★ 回归锚 = `--layer pr` 的 **8 case PASS · EXIT=0**（★ 不变） |
| ★ **R-A4-5** | ★ **夜间层 45~60 min 拖垮 CI 额度** | ★ ★ **夜间层用定时触发**（★ PR 层只 6.5 min）⇒ ★ 二者分离 |
| ★ **R-A4-6** | ★ **Verilog 缓存被当成待办** | ★ ★ **契约 S-15 已实测推翻**（★ 每 ELF 网表不同）⇒ ★ **不实现** ⇒ ★ 开放项 **O-5**（★ 需 DUT/上游级改动）⇒ ★ **★ MUST NOT 把「按 config 缓存」写成契约** |

---

## 7. ★ 明确不做（★ 越界防护）

- ★ **不改判定语义**（★ S-1~S-17）· ★ **不改 corpus 清单**（★ S-13 单一来源）· ★ **不定义时长阈值**（★ S-15）
- ★ **不删 TLM 代码**（★ 阶段 C）· ★ **不改 ADR**（★ 属 architect）
- ★ **不写 `## Acceptance Criteria` 段**（★ ★ **AC 权威 = `proposal.md`** · ★ 本 plan 只列执行路径）
- ★ **不在本 plan 内立项 L1-A / L1-B / L4**（★ 独立 change）

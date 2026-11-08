---
change: backend-parity-ci
priority: P1
initiative: wave7-architecture-governance
adr:
  - ADR-089
  - ADR-088
plan: .rddf/plans/backend-parity-ci.md
status: in-progress
depends_on:
  - backend-parity-corpus
---

# tasks — backend-parity-ci（阶段 A4）

> ★ **AC 权威 = `proposal.md` 的 `## Acceptance Criteria` 段**（★ 与 `rdd-verifier` 提取源一致：★ `rdd-verifier/SKILL.md:148`）⇒ ★ **本文件 = 执行清单 + 勾选态**（★ 编号与 AC 一一对应）。
> ★ **执行路径** = `.rddf/plans/backend-parity-ci.md`（★ 5 阶段 P1~P5）。
> ★ ★ **回归锚（★ 改动前后必须不变）**：★ `parity_gate.py --layer pr` ⇒ **8 case PASS · EXIT=0**；★ `parity_gate.py --self-test` ⇒ **16/16 PASS · EXIT=0**。

## 1. ★ P1 — timings 采集（★ 只加字段 · ★ 不改判定）

- [ ] **1.1** ★ **RED**：★ 实测当前 `build/parity/report.json` 的 `timings` = `{}`（★ **空占位**）⇒ ★ 判据 = 「跑完后 `timings` **非空** 且含 p50/p95」
- [ ] **1.2** ★ **GREEN**：★ `tools/parity/parity_gate.py` 采集 per-case 墙钟时长（★ `time.monotonic()`）⇒ ★ 写入 `per_case[*]` 与 `timings`
- [ ] **1.3** ★ `timings` 结构 = `{per_case, p50, p95, total, jobs}`（★ 与 plan §1.3 一致）
- [ ] **1.4** ★ **零回归**：★ `--layer pr` ⇒ ★ **verdict 仍 = PASS · 退出码仍 = 0**（★ **判定逻辑一字未改**）★ **AC-4**
- [ ] **1.5** ★ **MUST NOT 写死未实测的硬阈值**（★ S-15）⇒ ★ 只**采集** p95 ★ **AC-4**
- [ ] **1.6** ★ 验证：★ `report.json` 的 `timings` 非空 + 含 p50/p95 + `per_case[*]` 含时长 ★ **AC-4**

## 2. ★ P2 — 本机接线（★ 默认行为不变）

- [ ] **2.1** ★ **RED**：★ `run_chipforge_tests.sh --help` 不含 `--parity`（★ 改动前基线）
- [ ] **2.2** ★ **GREEN**：★ `tools/run_chipforge_tests.sh` 加 3 个 flag：★ `--parity`（★ 跑 PR 层）· ★ `--parity-layer <pr\|night\|all>`（★ 默认 `pr`）· ★ `--parity-self-test`（★ 单元级快车道）
- [ ] **2.3** ★ ★ **默认不跑** ⇒ ★ 不加 flag 时**行为逐字节一致**（★ 既有 CI / 开发者调用零影响）★ **AC-1**
- [ ] **2.4** ★ ★ **委托** `tools/parity/parity_gate.py`（★ **不重实现判定逻辑** ⇒ ★ S-1~S-17 契约不变）★ **AC-1**
- [ ] **2.5** ★ 退出码映射：★ `PASS⇒0` / `FAIL⇒1` / `ERROR⇒2`（★ 与 `parity_gate.py:718` 一致）★ **AC-1**
- [ ] **2.6** ★ ★ `--parity-self-test` **MUST NOT 依赖构建**（★ ⇒ ★ 排在 build 步骤**之前**）⇒ ★ **零构建依赖**★ **AC-5**
- [ ] **2.7** ★ `--help` 同步更新（★ 含 3 个新 flag · ★ ★ 注意 `--help` 走 `sed -n '2,29p'` ⇒ ★ 改头部须同步行号范围）★ **AC-1** · **AC-5**
- [ ] **2.8** ★ 验证：★ `--parity-self-test` ⇒ **16/16 PASS · EXIT=0** 且**不触发 build**；★ `--parity` ⇒ 8 case PASS · EXIT=0；★ `--parity --only <不存在>` ⇒ 退出码 **≠ 0**（★ fail-closed）★ **AC-1** · **AC-5**

## 3. ★ P3 — CI build job（★ ADR-088 判据 7）

- [ ] **3.1** ★ **RED**：★ `.github/workflows/` 下**无**任何 `cmake --build`（★ 改动前基线）
- [ ] **3.2** ★ **GREEN**：★ 新建 `.github/workflows/parity-gate.yml`（★ **独立 workflow** · ★ 理由见 plan §3.3）
- [ ] **3.3** ★ checkout CppTLM / CppHDL 到 `${workspace}/CppTLM` / `${workspace}/CppHDL`（★ 复用 `architecture-gates.yml:101-111` 的路径与 `vars.*` 约定）★ **AC-2**
- [ ] **3.4** ★ 装 Verilator（★ 复用 `architecture-gates.yml:50-63` 的安装逻辑）★ **AC-2**
- [ ] **3.5** ★ `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` + ★ `cmake --build build --target parity_sim_runner cpu_verilator_sim`（★ ★ **只 build 两个 runner** ⇒ ★ 不 build 686 个 TLM case）★ **AC-2**
- [ ] **3.6** ★ `timeout-minutes` 上限（★ 防止挂死占额度）★ **AC-2**
- [ ] **3.7** ★ 验证：★ workflow 含 `cmake --build` + 只含两个 runner target + 有 `timeout-minutes` ★ **AC-2**
- [ ] **3.8** ★ ★ **CI 真实耗时待实测**（★ **本 change 的唯一外部依赖风险** · ★ 需真实 PR run 验证）★ **AC-2**

## 4. ★ P4 — CI 门禁执行 + 结果回读（★ ADR-088 判据 4）

- [ ] **4.1** ★ **RED**：★ CI 中门禁 step 恒 SKIP（★ 病根 = `architecture-gates.yml:163-166` 的「产物不存在 ⇒ SKIP」）★ **AC-3**
- [ ] **4.2** ★ **GREEN**：★ CI 跑 `python3 tools/parity/parity_gate.py --layer pr`（★ PR 层 = 8 case ≈ **6.5 min** 实测）★ **AC-3**
- [ ] **4.3** ★ ★ **MUST NOT** 让「产物不存在 ⇒ SKIP」短路 ⇒ ★ **产物缺失 ⇒ FAIL loudly**（★ ★ **判据 4 的核心**）★ **AC-3**
- [ ] **4.4** ★ **回读**：★ 从 `build/parity/report.json` 读 `verdict` + `error_kind`（★ **机器可读**）⇒ ★ `actions/upload-artifact@v4` 上传 ★ **AC-3**
- [ ] **4.5** ★ **PR 级 / 夜间级分离**：★ PR 跑 `pr` 层；★ 夜间跑 `night` 层（★ **定时触发** · ★ 52 case ≈ **45~60 min**）⇒ ★ 同一 workflow 不同 trigger + layer 参数 ★ **AC-3**
- [ ] **4.6** ★ **失败即阻塞**：★ 门禁 FAIL ⇒ step 失败 ⇒ 阻止 PR merge（★ ★ **判据 6 的前置**）★ **AC-3**
- [ ] **4.7** ★ 验证：★ CI 上门禁**非 SKIP** + artifact 含 `verdict` + 故意制造分歧 ⇒ CI **红** ★ **AC-3**

## 5. ★ P5 — 收口（G-A4 逐条核 + 记账）

- [ ] **5.1** ★ G-A4 判据 ①「门禁在 CI 中被执行且结果被回读」⇒ 核 + 存证（★ CI run 链接 + artifact）★ **AC-3**
- [ ] **5.2** ★ G-A4 判据 ②「分层 corpus 的时长与分层表入库」⇒ 核 + 存证（★ `report.json` 的 `timings`）★ **AC-4**
- [ ] **5.3** ★ G-A4 判据 ③「单元级快车道在 PR 级可跑」⇒ 核 + 存证★ **AC-5**
- [ ] **5.4** ★ 记账：★ `.rddf/roadmap/features/feat-backend-parity-gate.md:36` A4 勾选 + 完成日期
- [ ] **5.5** ★ 记账：★ `.rddf/roadmap/objectives/objective-backend-parity-and-single-model.md:142` G-A4 勾选
- [ ] **5.6** ★ ★ **★ 不改 ADR-088 的状态行**（★ 属 architect）⇒ ★ 只在 roadmap / 审稿报告记录「判据 4/7 的完成形式已满足」
- [ ] **5.7** ★ 遗留清扫：★ 本 change 范围内无 TODO / 半截实现 / 临时日志 / 被 skip 的测试 / 未替换的 mock

---

## ★ 阶段边界

| 阶段 | 内容 | ★ 本 change |
|---|---|---|
| ★ **P1** | timings 采集 | ✔ |
| ★ **P2** | 本机接线 | ✔ |
| ★ **P3** | CI build job | ✔ |
| ★ **P4** | CI 门禁执行 + 回读 | ✔ |
| ★ **P5** | 收口 + 记账 | ✔ |
| ★ — | ★ **删 TLM 模式**（★ 阶段 C） | ✘ ⇒ ★ 依赖本 change + 其余 4 条硬前置 |
| ★ — | ★ **Verilog 跨 ELF 缓存** | ✘ ⇒ ★ **已被实测推翻**（★ S-15）⇒ ★ 开放项 **O-5**（★ 需 DUT/上游级改动） |
| ★ — | ★ **门禁时长阈值** | ✘ ⇒ ★ S-15「本期不设未实测的硬阈值」⇒ ★ 本 change 只**采集** p95 |
| ★ — | ★ **L1-A / L1-B / L4 三个 change** | ✘ ⇒ ★ 独立 change |

## ★ 相关

- ★ **AC 权威**：`proposal.md` 的 `## Acceptance Criteria` 段（★ AC-1~AC-5）
- ★ **执行路径**：`.rddf/plans/backend-parity-ci.md`（★ P1~P5）
- ★ **门禁契约**：`docs/architecture/parity-gate-contract.md`（★ S-13 / S-15 / GN-1 / O-5）
- ★ **前置 change**：`backend-parity-corpus`（★ A3 · 52/52 PASS · 分歧清单 = 空）

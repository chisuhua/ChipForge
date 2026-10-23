# P2.5 Review 报告 — chmem-tlm-backend-freeze（TLM 退役 B0）

> **change**: `chmem-tlm-backend-freeze`（`feat-tlm-backend-retirement` 的 B0 阶段）
> **分支**: `feature/chmem-tlm-backend-freeze`（base `a9f0d97` = A 归档后）
> **审查时间**: 2026-10-24
> **审查范围**: 实施结果（9 Task）vs plan / proposal / design / ADR-088
> **审查方式**: AC 逐条机械复核 + commit 卫生核对 + 变更面核对 + 门禁三态实测复核

---

## 1. 结论

**PASS —— 建议进入 P3 归档**。

---

## 2. AC 逐条核对（11/11 通过）

| AC | 判据 | 实测证据 | 结论 |
|----|------|---------|------|
| AC1 | 门禁脚本存在且能抓新增函数 | `tools/check_tlm_freeze.sh`（95 行，可执行）；变异：在 `ip/cpu/arch/riscv/branch.h` 追加函数 ⇒ `VIOLATION: ... branch.h:142` + 签名 + 登记提示 + `violations=1` + **exit 1** | ✅ |
| AC2 | 门禁接入 CI（warn 级） | `architecture-gates.yml` 的 `architecture-gates` job 新增 step（`continue-on-error: true`，yaml 校验 10 steps）；脚本层对违规仍 exit 1 | ✅ |
| AC3 | 4 个死代码删除且 0 includer | 4 文件已删（`ac1581e`）；**完整路径**口径 `#include` 命中 = 0；类名口径的 6 处命中经查全是**文档提及** | ✅ |
| AC4 | 双 binary 编译 + 0 退化 | `chipforge_tests` ✅ + `chipforge_tests_chmem` ✅；`run_chipforge_tests.sh --all` = 4 entry（3 PASS + `chipforge_tests` **恰 1 个 pre-existing** `test_7stage_riscv.cpp:124`）；chmem 591.47s，总 655.51s；TLM 493 = 492 + 1；`[cpu]` 137；chmem binary 130 | ✅ |
| AC5 | ADR-088 存在 + 5 项必填 + 已注册 | §2/§3/§4/§5/§6 各 1 节（`grep -c "^### N\."` = 1×5）；§6 含 ADR-087 v1.1 待办**显式登记行**；`adr.md` 注册 1 行 + 修订历史 1.6 行 | ✅ |
| AC6 | 语义裁决表 5 子系统不落空 | §3 表 5 行齐全（TLM 实现 / CH_MEM 实现 / 已知分歧 / B2 表态留空），「留 B2」标记 6 处 | ✅ |
| AC7 | ADR-083 v1.1 | §2.5 冻结条款 + §5.4 退役时间线（B0→B1→B2→B3 + G1/G2/G3）+ 头部 v1.1 标记 + 关联 ADR 补 ADR-088 | ✅ |
| AC8 | ADR-087 v1.1 待办登记 | ADR-088 §6 显式登记行：TLM 侧 `static_assert` 随删除消失 ⇒ 须切 CH_MEM 侧 CI grep 门禁 | ✅ |
| AC9 | TLM-only 业务文件数下降 | **48 → 44**（实测口径：`find ip -name '*.h' -o -name '*.cpp' \| grep -v '_chmem\.'` = 53 ⇒ 减 9 共享 = 44，删前 48）| ✅ |
| AC10 | 5 个架构门禁全 PASS | verify_adr ✅ · verify_plugin_decision 8/8 ✅ · check_plugin_portability 13/13 ✅ · doc_link_check 0 broken ✅ · roadmap-r9-check 11 fragments ✅（另加 check_xlen_hygiene warn=0 + check_tlm_freeze violations=0） | ✅ |
| AC11 | 台账格式已定义 + 姊妹提案已立项 | ADR-088 §4 定义三态（`covered`/`partial`/`not-covered`）+ 必填字段 + 509≠451 口径注记；姊妹提案 `chmem-tlm-coverage-parity-ledger` 已存在（P1） | ✅ |

---

## 3. commit 卫生（5 批）

| commit | 内容 | 核对 |
|---|---|---|
| `5a8b491` | 门禁 + CI + README + 判定口径三件套 | 6 文件（state 三件套用 `git add -f`，因 `.gitignore:57 state/`）✅ |
| `ac1581e` | 4 死代码删除 + 文档漂移修正 | 5 文件；**删除路径不带 `git add`**（已 `git rm` 的路径 `git add` 会 exit 128）✅ |
| `b5b3ccd` | ADR-088 + ADR-083 v1.1 + 注册表 | 3 文件 ✅ |
| `596bebd` | change 文档同步（Task/AC 勾选 + 实测留证 + 数字纠正） | 2 文件 ✅ |
| （state） | builder handoff | ✅ |

**index 无残留**（`git diff --cached` 空）⇒ 批 2 的 4 个删除确实进了 commit。

---

## 4. 实施期发现并修正的 5 处 plan 缺陷（详见 plan「实施实测留证」）

| # | 缺陷 | 修正 |
|---|------|------|
| 1 | 检测面定义过窄（候选仅 18）⇒ 漏 `ip/mmu/{tlm,lib,policies}` | 扩到 `ip/**/*.h`（非 `_chmem.h`）⇒ 44 − 9 = 35（删死代码后 31） |
| 2 | 白名单还原用函数名 sed ⇒ 删不掉 ⇒ **残留会静默豁免** | `git checkout --` + 按登记文本删 + `grep -vc '^#'` = 0 复核 |
| 3 | yaml 校验读到 `toolchain-sanity` job | 显式 `d['jobs']['architecture-gates']` |
| 4 | ADR 章节自检 `grep "^## N"` 恒 0 | 改 `"^### N\."` |
| 5 | 死代码 includer basename 口径 ⇒ 假阳性 8 | 完整路径口径（真实 0） |

**外加 1 处数字纠正**：AC9 的 TLM-only 文件数（草案 45→41 / 44→40 均未实测）⇒ 实测 **48 → 44**，proposal/tasks 已同步并注明「草案数字未实测」。

---

## 5. 已知遗留（不阻塞归档）

| # | 事项 | 归属 |
|---|------|------|
| 1 | `test_7stage_riscv.cpp:124` node_count `9 == 17` | pre-existing，独立跟踪项 |
| 2 | 门禁**函数识别是粗粒度**（宁可多报，误报靠白名单豁免） | design R7，已明示 |
| 3 | 白名单 >5 条的「ADR-088 复审」提示**不自动阻断** | design R2/B2 职责 |
| 4 | ADR-088 §3 的 5 个子系统**正式表态留空** | B2 阶段（architect 主持） |
| 5 | ADR-088 §4 台账**内容**未交付 | 姊妹提案 `chmem-tlm-coverage-parity-ledger`（P1） |
| 6 | ADR-087 v1.1 待办（TLM 护栏切 CH_MEM 侧门禁）未启动 | architect，ADR-088 §6 已登记 |
| 7 | TLM-only 业务代码面仍 44 文件 | B1/B2/B3（Gate 3 放行删除） |
| 8 | 测试不进 CI ⇒ 冻结门禁与未来 TLM 退役均无 CI 级回归保护 | 既有决策（Oracle 2026-10-06）；ADR-088 §1.3 已登记为 B3 前置之一 |

---

## 6. 建议

1. **可进 P3 归档**
2. 归档后按 `feat-tlm-backend-retirement` 推进 **B1 承载力**（BP / LSU / DBus / L1Cache 本体 / MMU TLB 策略 / PTW 接入等缺口，ADR-088 §1.2 与 §3 已给出具体清单）
3. **B2 语义裁决** 需 architect 主持（ADR-088 §3 表的「留 B2」列）
4. **B3 删除** 须先解决「测试不进 CI」的盲区（ADR-088 §5.2 前置 4）

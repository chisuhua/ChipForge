---
change: chmem-tlm-coverage-parity-ledger
priority: P1
initiative: wave6-linux-and-productization
adr:
  - ADR-083
  - ADR-088
  - ADR-047
plan: .rddf/plans/chmem-tlm-coverage-parity-ledger.md
status: in-planning
---

# Tasks — chmem-tlm-coverage-parity-ledger

> 实施细节在 plan（TDD 5-step）；本文件是 OpenSpec 跟踪台账。execute 阶段逐 Task 勾选。
> **交付 ADR-088 §4 覆盖平价台账的「内容」**（格式已由 `chmem-tlm-backend-freeze` B0 交付）。

## Phase 0 备忘

- P0 决策：case-1 approve（auto-decision ADR-0049：advisory=complex + LLM=complex + AC=12 → 完整 P1–P3）
- **数字纠正（实测 vs 草案）**：TLM 独占 451 → **463**（`rv64-isa-declaration-correction` 新增 13 个 TLM-only case、删 1 个）；TLM 总 481 → **493**；闭合 463+30=493 / 100+30=130
- **状态词冲突裁决**：ADR-088 §4 三态（`covered`/`partial`/`not-covered`）为**权威**；草案的「已移植 CH_MEM」/「由 ELF 级覆盖」/「显式退役」映射入台账 §4
- **口径纪律**：覆盖口径一律用两个 binary 的 `--list-tests` **差集**；**禁止**族求和（族计数之和 = 678 ≠ 463）
- 铁律：**0 生产代码改动**（`.h` / `.cpp` 改动数 = 0）

## Implementation

- [x] Task 1 基线快照（两 binary case 总数 / 差集 / 闭合口径 / 基线 commit）
- [x] Task 2 `tools/gen_coverage_parity_ledger.sh`（生成骨架 + 闭合校验 + 族分布；数字不写死）
- [x] Task 3 `tools/check_coverage_parity_ledger.sh`（漂移检测 + 红线 + 口径守卫）
- [x] Task 4 族级判定（46 族逐族定三态 + CH_MEM 锚点 + 差异说明）
- [x] Task 5 台账生成（463 条条目级 + 措辞映射 + B1 承载力待办清单 + 维护规则）
- [x] Task 6 校验脚本变异自证（A: 新增未登记 case / B: 台账删 3 条 / C: framework 改 not-covered）
- [x] Task 7 CI 接入（`architecture-gates.yml` warn 级 step 6）+ `tools/README.md` 登记 2 个脚本
- [x] Task 8 收口（8 门禁 + 零生产代码核对 + AC 逐条机械复核 + `openspec change validate` + 分 2 批 commit）

## Acceptance

- [x] AC1 台账覆盖全部 **463** 个 TLM 独占 case（`--list-tests` 差集 100% 标注，无遗漏）
- [x] AC2 闭合校验两式成立：**463 + 30 = 493**（TLM 总数）；**100 + 30 = 130**（CH_MEM 总数）；覆盖 = 463（去重差集口径）
- [x] AC3 `[framework]` 94 条 **100% `covered`**，无一条 `not-covered`
- [x] AC4 `covered` / `partial` 条目 **100% 填写差异说明**（族级继承 + 条目级引用，check 脚本机检第 6 列非空）
- [x] AC5 `not-covered` 条目 **100% 有 ≥20 字理由 + ADR-088 引用**（17 条逐条差异化）
- [x] AC6 批量退役检测：同一理由 >10 次 = **0**；`not-covered` 总数 **17** ≤ 135（30%）
- [x] AC7 `tools/gen_coverage_parity_ledger.sh` 可重新生成骨架，**重跑条目集与台账一致**（463 = 463）
- [x] AC8 `tools/check_coverage_parity_ledger.sh` 已接入 CI（warn 级），能检出「新增 TLM-only case 未登记」（变异 A 实测）
- [x] AC9 `covered`/`partial` 条目汇总成 **B1 承载力补齐待办清单**（台账 §5，11 项，按 plugin/能力归组 + 对应 B1 项）
- [x] AC10 5 个架构门禁全 PASS（另加 check_xlen_hygiene / check_tlm_freeze / check_coverage_parity_ledger ⇒ 8 项全绿）
- [x] AC11 **0 生产代码改动**（`.h` / `.cpp` 改动数 = 0）

### 实测留证（2026-10-24，branch `feature/chmem-tlm-coverage-parity-ledger`）

| 项 | 实测 |
|---|---|
| 闭合校验 | TLM 独占 **463** + 共有 30 = **493** ✅；CHMEM 独有 100 + 共有 30 = **130** ✅ |
| 台账规模 | `docs/architecture/tlm-coverage-parity-ledger.md` 734 行；**463** 条 / **46** 族 / covered **154** · partial **292** · not-covered **17**（上限 135） |
| framework 红线 | 94 条**全部 `covered`**，`not-covered` 0 ✅ |
| not-covered 理由 | 17 条**逐条差异化**（同理由 ≤10 红线满足） |
| 脚本自检 | `gen`：闭合 + 族分布 + `--emit-ledger`；`check`：覆盖 463 / 闭合两式 / 红线 / 口径守卫 ⇒ **exit 0** |
| 变异 A | 新增 TLM-only case（`zzz_unregistered_probe`）未登记 ⇒ 检出（`TLM 独占 464 vs 台账 463`）+ exit 1 ✅（含重编译） |
| 变异 B | 台账删 3 条 ⇒ 报出 3 条缺失 + exit 1 ✅ |
| 变异 C | framework 条目改 `not-covered` ⇒ 红线检出（`framework 族出现 not-covered`）+ exit 1 ✅ |
| 8 门禁 | verify_adr ✅ · verify_plugin_decision 8/8 ✅ · check_plugin_portability 13/13 ✅ · doc_link_check 0 broken ✅ · roadmap-r9-check 11 fragments ✅ · check_xlen_hygiene warn=0 ✅ · check_tlm_freeze violations=0 ✅ · **check_coverage_parity_ledger PASS** ✅ |
| 零生产代码改动 | `.h` / `.cpp` 改动数 = **0** ✅ |

### 实施期修正（脚本落盘实跑暴露 3 处，已回写 plan 脚本全文）

| # | 问题 | 修正 |
|---|------|------|
| 1 | `parse \| sed \| awk '{print $1}'` —— **case 名可含空格** ⇒ 按空格切分截断名字（丢 14 个 ⇒ 闭合 449≠493） | 改 `cut -f1`（TAB 分隔） |
| 2 | `parse` 的 awk 在 **tag 行先 flush 名字** ⇒ tags 全空（族分布输出为空） | 改「tag 行累积、下个 name 行 flush」+ `END` 补最后一个 |
| 3 | `check` 的台账条目正则要求第 2 列是**单个**反引号族，而台账第 2 列是**多个 tag** ⇒ 只抓到 286/463 | 放宽为「第 2 列任意 + 第 3 列必须是三态之一」（后者天然排除 §2 族级表行） |

# P2.5 Review 报告 — chmem-tlm-coverage-parity-ledger

> **change**: `chmem-tlm-coverage-parity-ledger`（TLM 退役 B0 剩余项：台账**内容**）
> **分支**: `feature/chmem-tlm-coverage-parity-ledger`（base `913c20d`）
> **审查时间**: 2026-10-24 · **审查方式**: AC 逐条机械复核 + 变异对照证据 + commit 卫生 + 变更面核对

## 1. 结论

**PASS —— 建议进入 P3 归档**。

## 2. AC 核对（12/12）

| AC | 实测 | 结论 |
|----|------|------|
| AC1 覆盖 463 个 TLM 独占 case | `check` 报「TLM 独占 case: 463 \| 台账条目: 463」 | ✅ |
| AC2 闭合两式 | 463+30=493 ✓ · 100+30=130 ✓（脚本重算，数字不写死） | ✅ |
| AC3 framework 94 条全 covered | 「framework 族: 94 条 (not-covered 0)」 | ✅ |
| AC4 covered/partial 有差异说明 | 条目级第 6 列全部非空（族级继承 + 条目引用），check 机检 | ✅ |
| AC5 not-covered 有逐条理由 | 17 条**逐条差异化**（无「待补具体理由」残留） | ✅ |
| AC6 批量退役红线 | not-covered **17** ≤ 135；同理由 ≤10 | ✅ |
| AC7 gen 可重生成骨架 | `--emit-ledger` 产出条目集 = 台账条目集（463） | ✅ |
| AC8 check 接入 CI + 检出漂移 | `architecture-gates.yml` step 6（warn 级）；**变异 A 实测**（新增 `zzz_unregistered_probe` ⇒ 检出 + exit 1） | ✅ |
| AC9 B1 待办清单 | 台账 §5：11 项（按 plugin/能力归组 + 对应 B1 项） | ✅ |
| AC10 5 架构门禁 | 8 项全绿（含 check_xlen_hygiene / check_tlm_freeze / check_coverage_parity_ledger） | ✅ |
| AC11 零生产代码改动 | `git status` 中 `.h` / `.cpp` 改动数 = **0** | ✅ |

## 3. 变异对照（3/3 被检出）

| 变异 | 期望 | 实测 |
|------|------|------|
| A：新增 TLM-only case 未登记 | 检出 + exit 1 | ✅（`TLM 独占 464 vs 台账 463` + `VIOLATION: TLM-only case 未登记: zzz_unregistered_probe`） |
| B：台账删 3 条 | 检出 + exit 1 | ✅（报出 3 条缺失） |
| C：framework 条目改 `not-covered` | 红线检出 + exit 1 | ✅（`framework 族出现 not-covered`） |

## 4. 实施期修正（3 处脚本 bug，已回写 plan 全文）

1. `awk '{print $1}'` 按空格切分 ⇒ **case 名含空格被截断**（丢 14 个，闭合 449≠493）⇒ 改 `cut -f1`
2. `parse` 的 awk 在 tag 行先 flush 名字 ⇒ tags 全空（族分布空）⇒ 改「tag 累积 / 下行 flush」+ `END` 补尾
3. `check` 的条目正则要求第 2 列单反引号族，而台账第 2 列是多 tag ⇒ 只抓到 286/463 ⇒ 放宽为「第 3 列必须是三态」（天然排除族级表行）

## 5. 已知遗留（不阻塞归档）

| # | 事项 | 归属 |
|---|------|------|
| 1 | `not-covered` 17 条的判定基于**族级判断**（`partial` 291 条依赖族级差异说明继承，未逐条核对 CH_MEM 锚点） | B2 语义裁决阶段逐条复核 |
| 2 | `[mmu]` 52 条 / `[cpu]` 137 条 / `[cpu-integration]` 96 条的族级判定粒度较粗（族内异质性未拆分） | 同上 |
| 3 | 台账**条目级**的 CH_MEM 锚点以「继承族级」表达，不是逐条独立锚点 | ADR-088 §4 允许（必填字段有值即可）；如需逐条锚点归 B1 |
| 4 | 性能基线载体（dhrystone 3 / performance 1 / cycle-parity 3）标 `not-covered` | 需在 Verilator 侧重建（B1） |
| 5 | 工具面（cpu-sim 2）标 `not-covered` | 随工具迁移（B3） |
| 6 | 生成器（python，一次性）未入库 | 仓内只有 2 个 shell 脚本（符合 AC「只加 2 脚本」）；台账内容由 §2 族级表人工维护 |

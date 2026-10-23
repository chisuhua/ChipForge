# tlm-coverage-parity-ledger Specification

## Purpose
Delivers the **content** of the coverage-parity ledger whose format is fixed by ADR-088 §4. Every case that exists only in the legacy TLM binary (463 of 493 at the baseline commit) MUST be recorded with a three-state verdict (`covered` / `partial` / `not-covered`) plus the required fields, so that when the TLM backend is retired no behaviour silently loses its only regression anchor. The ledger is regenerable from the two binaries' `--list-tests` difference (never by summing per-family counts, because a case may carry several tags), its coverage MUST close arithmetically on both sides (TLM-only + shared = TLM total; CH_MEM-only + shared = CH_MEM total), and `not-covered` verdicts are bounded (≤30% of the TLM-only set, and never more than 10 cases sharing one rationale) so that mass retirement cannot bypass review.

## Requirements

### Requirement: tlm-coverage-parity-ledger-completeness

覆盖平价台账 MUST 覆盖两个 binary `--list-tests` 差集得出的**全部 TLM 独占 case**，且两个计数口径 MUST 各自闭合（TLM 独有 + 共有 = TLM 总数；CH_MEM 独有 + 共有 = CH_MEM 总数）。台账 MUST 标注基线 commit（计数随代码演进漂移，见 §5 R4）。

#### Scenario: 台账覆盖全部 TLM 独占 case

- **WHEN** 用 `tools/check_coverage_parity_ledger.sh` 比对台账条目集与两 binary 的 `--list-tests` 差集
- **THEN** 台账条目数 = TLM 独占 case 数（基线 commit `913c20d` 实测：**463**），无遗漏、无多余条目

#### Scenario: 两个口径各自闭合

- **WHEN** 运行 `tools/gen_coverage_parity_ledger.sh` 的闭合校验
- **THEN** 报出「TLM 独占 463 + 共有 30 = TLM 总数 493」且「CH_MEM 独有 100 + 共有 30 = CH_MEM 总数 130」，两式均成立（数字随基线变动，**不写死**）

#### Scenario: 族求和不得用作覆盖口径

- **WHEN** 有人用「按族 case 数求和」代替差集口径
- **THEN** 校验脚本拒绝该口径并报错（同一 case 可带多 tag ⇒ 族求和 > 差集数，基线实测 678 ≠ 463）

### Requirement: tlm-coverage-parity-ledger-annotation

每条台账条目 MUST 按 ADR-088 §4 的三态标注（`covered` / `partial` / `not-covered`），并 MUST 填写该态对应的必填字段：`covered`/`partial` MUST 填差异说明；`not-covered` MUST 有 ≥20 字理由 + ADR-088 裁决引用。台账 MUST 包含草案措辞到 ADR-088 三态的映射表（「已移植 CH_MEM」⇒ `covered`；「由 ELF 级覆盖」⇒ `covered`/`partial`（视语义等价性）；「显式退役」⇒ `not-covered`）。

#### Scenario: covered 条目仍须填差异说明

- **WHEN** 某条目标为 `covered`
- **THEN** 差异说明字段非空（ADR-088 §4 要求；`covered` 不等于行为等价，只是有对应覆盖）

#### Scenario: not-covered 条目须有理由与裁决引用

- **WHEN** 某条目标为 `not-covered`
- **THEN** 理由 ≥20 字且含 ADR-088 引用；否则校验脚本报错

#### Scenario: 措辞映射表存在

- **WHEN** 打开台账
- **THEN** 能找到「已移植 CH_MEM」/「由 ELF 级覆盖」/「显式退役」到 ADR-088 三态的映射说明（消除两套状态词并存）

### Requirement: tlm-coverage-parity-ledger-red-lines

`not-covered`（显式退役）条目总数 MUST ≤135（TLM 独占 case 的 30%），且**同一理由**出现 MUST ≤10 次。`[framework]` 族的 94 条 MUST 100% 标为 `covered`（框架语义锚已由 CH_MEM 侧承接，不得批量退役）。

#### Scenario: 批量退役被拦截

- **WHEN** 校验脚本统计出某同一理由出现 >10 次，或 `not-covered` 总数 >135
- **THEN** 脚本报错并列出违规条目（阻断「一次性显式退役大量测试」绕过评审）

#### Scenario: framework 族不得退役

- **WHEN** `[framework]` 的 94 条中任一条被标为 `not-covered`
- **THEN** 校验脚本报错（该族是语义锚，退役需显式 ADR）

### Requirement: tlm-coverage-parity-ledger-tooling

台账 MUST 可由 `tools/gen_coverage_parity_ledger.sh` 从两个 binary 的 `--list-tests` 重新生成骨架（重跑结果与台账条目集一致，防漂移）；`tools/check_coverage_parity_ledger.sh` MUST 接入 CI（warn 级）并能检出「新增 TLM-only case 未登记」。本 change MUST NOT 修改任何生产代码（`.h` / `.cpp` 改动数 = 0）。

#### Scenario: 骨架可重生成且条目集一致

- **WHEN** 运行 `tools/gen_coverage_parity_ledger.sh` 生成骨架
- **THEN** 骨架的条目集与台账的条目集一致（仅标注内容可不同，条目集合必须相同）

#### Scenario: 新增 TLM-only case 被检出

- **WHEN** 代码新增了一个 TLM-only case 且未在台账登记
- **THEN** `tools/check_coverage_parity_ledger.sh` 返回非 0 并报出该 case 名（CI 层 warn 级，不阻塞 PR）

#### Scenario: 零生产代码改动

- **WHEN** 核对本 change 的 `git diff --stat`
- **THEN** `.h` / `.cpp` 改动数 = 0（只含 1 文档 + 2 脚本 + 1 CI + 1 README）

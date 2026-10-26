## ADDED Requirements

### Requirement: corpus-coverage-both-sides

语料 MUST 覆盖两侧的**交集**，且门禁 MUST 断言该覆盖关系。

#### Scenario: 清单项未被某侧覆盖

- **WHEN** 清单中的 case 不在某侧的 `kCases` 中
- **THEN** 门禁 MUST 判 `ERROR` / `corpus_mismatch`

#### Scenario: 覆盖超集只告警

- **WHEN** 某侧 `kCases` 含清单以外的 case
- **THEN** 门禁 MUST 告警（覆盖超集）⇒ MUST NOT 因此判 `ERROR`

#### Scenario: 语料文件缺失即 fail-closed

- **WHEN** 清单引用的 ELF 在磁盘上不存在
- **THEN** 门禁 MUST 判 `ERROR`（pre-flight）⇒ MUST NOT 跳过该 case

#### Scenario: 分支负载纳入语料

- **WHEN** 语料清单构建完成
- **THEN** MUST 同时包含 ALU/load/store 类 ELF **与** 分支密集负载（branchbench 4 档）

### Requirement: partial-run-observation-weighting

case 在 cycle cap 处被截断时，相等但无判别力的观测量 MUST NOT 被计为通过证据。

#### Scenario: 两侧都被截断

- **WHEN** 两侧的 `done` 均为 false（cap 截断）
- **THEN** `tohost` 与 `cycles` MUST 被标为 **vacuous**（不计入承重判据）
- **AND** 承重判据 MUST 为 `regs` + `memsig` + `pchash`
- **AND** 报告 MUST 标出该 case 为 partial ⇒ MUST NOT 静默呈现为完整通过

#### Scenario: 截断下承重判据不等

- **WHEN** 两侧都被截断但 `pchash`（或 `regs`/`memsig`）不等
- **THEN** 门禁 MUST 判 `FAIL` ⇒ 截断 MUST NOT 成为放行通道

#### Scenario: 一侧跑完另一侧没跑完

- **WHEN** 两侧的 `done` 不一致
- **THEN** 门禁 MUST 判 `FAIL` / `completion_mismatch`（真分歧）⇒ MUST NOT 记为 `ERROR`

#### Scenario: 完成态缺失

- **WHEN** 任一侧的 `done` 字段缺失
- **THEN** 门禁 MUST 判 `ERROR` / `completion_missing`

### Requirement: per-case-cycle-cap

cycle cap MUST 按 case 的需求设定，MUST NOT 用单一全局值。

#### Scenario: 不同负载用不同 cap

- **WHEN** 语料同时含短负载（ELF）与长负载（branchbench）
- **THEN** 门禁 MUST 按 `kind` 取默认 cap，且清单 MUST 可用第 5 列覆写

#### Scenario: cap 截断是合法结果

- **WHEN** 某 case 因 cap 截断而未跑完
- **THEN** 门禁 MUST 判该 case 为 partial 并按 partial 语义处理 ⇒ MUST NOT 静默跳过

### Requirement: parallel-execution-isolation

门禁 MUST 支持并行跑批，且并行 MUST NOT 影响被测结果。

#### Scenario: Verilator 产物 per-case 隔离

- **WHEN** 多个 case 并行跑 Verilator 后端
- **THEN** 每个 case MUST 使用独立的 work-dir 与 Verilog 路径 ⇒ MUST NOT 互相覆盖网表

#### Scenario: 报告顺序稳定

- **WHEN** 并行执行完成
- **THEN** 报告 MUST 按清单顺序排列 ⇒ MUST NOT 受完成顺序影响

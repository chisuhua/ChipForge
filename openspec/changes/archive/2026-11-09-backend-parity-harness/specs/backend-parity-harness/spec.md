## ADDED Requirements

### Requirement: parity-comparator-verdict

门禁 MUST 对每个 corpus 项给出 PASS / FAIL / ERROR 三态判定，且判定逻辑 MUST 可离线验证。

#### Scenario: 观测量全等

- **WHEN** 两侧的 5 项观测量逐项相等
- **THEN** 判定 MUST 为 PASS，且报告 MUST 含该项的全部 5 个观测量

#### Scenario: 观测量不等

- **WHEN** 任一观测量两侧不等且无合规例外
- **THEN** 判定 MUST 为 FAIL（退出码 1）⇒ MUST NOT 降级为 PASS
- **AND** 报告 MUST 含 `first_divergence`（metric + 两侧值）

#### Scenario: 观测量缺失

- **WHEN** 任一观测量在任一侧缺失
- **THEN** 判定 MUST 为 ERROR（退出码 2）⇒ MUST NOT 跳过该项或按 0 处理

#### Scenario: 确定性不符先于比对

- **WHEN** 两侧 `determinism`（node_count + names_hash）不一致
- **THEN** 判定 MUST 为 ERROR / `determinism_mismatch`，且 MUST NOT 产出任何观测量差异结论

#### Scenario: 判定逻辑可离线自检

- **WHEN** 运行 `--self-test`
- **THEN** MUST 用合成数据覆盖：全等 PASS / 不等 FAIL / 缺失 ERROR / 确定性 ERROR / 例外豁免 / 过期失效 / 作用域不匹配

### Requirement: exception-registry-enforcement

白名单 MUST 是受控清单：字段齐备、未过期、作用域匹配才能豁免差异。

#### Scenario: 未知字段拒收

- **WHEN** 白名单条目出现未知字段或容差类字段（`tolerance`/`eps`/`max_delta` …）
- **THEN** 门禁 MUST 判 ERROR 并指出该条目

#### Scenario: 过期条目不再豁免

- **WHEN** 条目的 `sunset` 已过期
- **THEN** 该条目 MUST 不再豁免任何差异 ⇒ 对应 case 判 FAIL

#### Scenario: 作用域必须匹配

- **WHEN** 例外的 `corpus_scope` 不含该 case 名（且不含 `*`）
- **THEN** 该例外 MUST NOT 豁免该 case 的差异

#### Scenario: 常驻合成例外必须存在

- **WHEN** 白名单缺少 `synthetic-self-test`
- **THEN** 门禁 MUST 判 ERROR（防白名单机制随真实例外清零而腐化）

#### Scenario: 规模护栏不阻断

- **WHEN** 真实例外数超过阈值
- **THEN** 门禁 MUST 给出「需 architect 复审」的提示
- **AND** MUST NOT 因此判 FAIL（阻断会逼出假登记）

### Requirement: corpus-single-source

语料 MUST 由单一清单驱动，且两侧 harness 对清单的覆盖 MUST 被断言。

#### Scenario: 清单项未被某侧覆盖

- **WHEN** 清单中的 case 不在某侧的 `kCases` 中
- **THEN** 门禁 MUST 判 ERROR / `corpus_mismatch`

#### Scenario: 覆盖超集只告警

- **WHEN** 某侧 `kCases` 含清单以外的 case
- **THEN** 门禁 MUST 告警（覆盖超集）⇒ MUST NOT 因此判 ERROR

#### Scenario: 分层执行

- **WHEN** 指定 `--layer pr|night|all`
- **THEN** 门禁 MUST 只执行清单中对应 `layer` 的 case

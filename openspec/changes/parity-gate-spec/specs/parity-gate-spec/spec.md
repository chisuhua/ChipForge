## ADDED Requirements

### Requirement: parity-observation-contract

双后端差分 parity 的 5 个观测量 MUST 各自有**可执行的采集契约**（谁、从哪个符号读、读不到怎么办），且语义 MUST NOT 在实现期被替换。

#### Scenario: 观测量命名冻结

- **WHEN** 检查 `docs/architecture/parity-gate-contract.md` 的 S-1
- **THEN** 观测量 id MUST 恰为 `tohost` / `regs` / `memsig` / `cycles` / `pchash` 五个 ⇒ 多一个或少一个 MUST 视为契约违规

#### Scenario: 内存签名不得依赖 host dump

- **WHEN** 检查观测量 ③ 的定义（S-6）
- **THEN** 实现 MUST 为 **DUT 内自算签名**（导出 named reg）⇒ MUST NOT 依赖 host 侧全内存读出
- **AND** 依据 MUST 可复核：Simulator 侧 `get_memory()` 为空 stub 且 `ENABLE_MEMORY_API` 未启用

#### Scenario: 逐拍语义不得用直方图替代

- **WHEN** 检查观测量 ⑤ 的定义（S-8）
- **THEN** MUST 为**逐拍有序滚动 hash** ⇒ MUST NOT 使用丢顺序的直方图（`PC_HIST`）
- **AND** 报告 MUST 能给出**首个分歧拍**（index + 两侧值）

#### Scenario: 观测量缺失即门禁故障

- **WHEN** 任一观测量在任一侧不可采集
- **THEN** 门禁 MUST 判 `ERROR`（退出码 2）⇒ MUST NOT 跳过该项或按 0 处理

### Requirement: determinism-precondition

差分比对 MUST 先验证两侧是「同一输入的确定性 elaboration」，验证失败 MUST NOT 继续比对。

#### Scenario: 确定性判据先于观测量比对

- **WHEN** 门禁启动一次差分执行
- **THEN** MUST 先比对 DAG `node_count` 与顶层信号名名单 hash
- **AND** 两者任一不等 ⇒ MUST 立即终止并报 `ERROR` / `error_kind=determinism_mismatch`（分类口径见开放项 C-1）

#### Scenario: 采样相位不得自适应对齐

- **WHEN** 两侧采样相位存在偏移
- **THEN** 门禁 MUST NOT 实现「寻找最佳 offset」之类自适应对齐
- **AND** 恒定 offset 若存在 MUST 记录进契约（S-3）并经 architect，而非在代码里补偿

#### Scenario: reset 处理两侧一致

- **WHEN** 门禁配置一次差分执行
- **THEN** 两侧 `reset_mode` MUST 相同，默认为 `none`
- **AND** Verilator 侧为支持 `none` 而新增的开关 MUST NOT 改变既有默认行为

### Requirement: exception-registry

跨后端的已知分歧 MUST 以**受控白名单**登记，每条例外携带机制说明、上游状态、变异对照、收敛期限与 corpus 作用域；白名单 MUST NOT 成为容差通道。

#### Scenario: 必填字段缺失即拒绝

- **WHEN** 白名单条目缺少 `mechanism` / `upstream_status` / `mutation_control` / `sunset` / `corpus_scope` 任一项
- **THEN** 门禁 MUST FAIL ⇒ 该条目 MUST NOT 生效

#### Scenario: 禁止容差字段

- **WHEN** 白名单出现 `tolerance` / `eps` / `max_delta` 之类字段
- **THEN** schema 校验 MUST 拒收（未知字段一律拒收）

#### Scenario: 过期例外自动失效

- **WHEN** 当前日期超过某条目的 `sunset`
- **THEN** 该条目 MUST 失效 ⇒ 门禁 MUST FAIL 并提示「升级为真 bug 类」

#### Scenario: 常驻合成例外

- **WHEN** 白名单被清空到只剩合成条目
- **THEN** 门禁 MUST 仍能验证白名单机制本身（`synthetic-self-test` 不计入真实例外数）

#### Scenario: 扩张需 architect

- **WHEN** 白名单新增或扩张条目
- **THEN** 变更 MUST 经 architect 审，且 commit message MUST 引 `ADR-089`

### Requirement: corpus-single-source

差分执行的 corpus MUST 由**单一清单文件**驱动，两侧 harness MUST NOT 各自维护手写列表。

#### Scenario: 三方集合一致

- **WHEN** 门禁执行一次全量差分
- **THEN** Simulator 侧实际执行集合、Verilator 侧实际执行集合、清单集合 MUST 三者相等 ⇒ 否则 `ERROR`

#### Scenario: 两侧交集显式化

- **WHEN** 检查首批 corpus
- **THEN** 清单 MUST 含两侧交集的 47 个 ELF + branchbench 4 档
- **AND** `rv32ui-p-simple` MUST 在清单内（补齐 Simulator 侧一侧缺失）

### Requirement: report-and-exit-codes

门禁 MUST 产出机器可读报告并区分「后端分歧」与「门禁自身故障」。

#### Scenario: 三态退出码

- **WHEN** 门禁结束
- **THEN** 退出码 MUST 为 `0=PASS` / `1=FAIL`（后端分歧）/ `2=ERROR`（TIMEOUT、runner 崩溃、观测量缺失、确定性不符、白名单非法、语料集合不一致）

#### Scenario: ERROR 不计入分歧清单

- **WHEN** 某 case 以 `ERROR` 结束
- **THEN** 报告 MUST 记录 `error_kind`，且该 case MUST NOT 出现在「既存后端分歧清单」中

#### Scenario: 报告字段齐备

- **WHEN** 读取 `build/parity/report.json`
- **THEN** 每个 case MUST 含全部 5 个观测量与 `first_divergence`（当 verdict≠PASS）

### Requirement: contract-traceability

契约 MUST 以编号单元（S-1…S-16）被下游引用，`G-A2` MUST 用覆盖矩阵证明无遗漏。

#### Scenario: 下游任务引用 S 编号

- **WHEN** 检查 A2a / A2b / A3 / A4 的 `tasks.md`
- **THEN** 每条任务 MUST 至少引用一个 `S-\d+`

#### Scenario: 覆盖矩阵无空缺

- **WHEN** `G-A2` 验收
- **THEN** 「S ⇒ 实现/测试」矩阵中每个 S MUST 有 ≥1 实现落点，或显式 `N/A + 理由`

#### Scenario: 契约变更需 architect

- **WHEN** 需要新增 S 单元或改变某条语义/判定强度
- **THEN** MUST 经 architect；纯措辞修正可由 builder 改并在 commit message 注明

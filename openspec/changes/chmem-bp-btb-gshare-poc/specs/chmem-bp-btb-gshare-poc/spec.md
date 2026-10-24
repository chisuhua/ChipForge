## ADDED Requirements

> Capability topic: `chmem-bp-btb-gshare-poc`（CH_MEM 分支预测的**机制可行性 PoC**）。
> 上游 = `docs/architecture/bp-btb-gshare-design-brief.md` §2.1 / §2.5 / §4.2。

### Requirement: bp-speculative-fetch-mechanism

CH_MEM 侧 MUST 存在**提前一拍**的分支预测机制：预测结果 MUST 作用于**下一次取指地址**的选择，使正确预测的 taken 控制转移 MUST **不产生 fall-through 取指**。

#### Scenario: 预测点在取指侧

- **WHEN** 在 fetch NORMAL 阶段以当前取指地址查询 BTB
- **THEN** 查表 MUST 为 **0 拍**（异步读）⇒ 预测结果 MUST 可在**同一拍**的 PC-mux 中被消费

#### Scenario: 预测结果与实际结果作用于同一目标

- **WHEN** 分支实际结果与投机预测都可用
- **THEN** 二者 MUST 作用于**同一个下一取指地址**的决策，且优先级 MUST 为「实际结果 > 投机预测 > 顺序取指」

#### Scenario: 正确预测须消除气泡

- **WHEN** 预测方向与目标均正确
- **THEN** 下一取指地址 MUST 直接为目标地址 ⇒ **fall-through 地址 MUST 不被取出** ⇒ 该次 taken 转移 MUST **不产生**额外取指拍

### Requirement: bp-correct-prediction-suppresses-self-redirect

当预测与实际结果**一致**时，MUST **抑制该分支自身的重定向**。否则目标指令将被取两次，表现为「预测正确但性能无收益」。

#### Scenario: 目标指令只被取一次

- **WHEN** 在 BP 启用下记录 host 侧取指地址序列
- **THEN** 序列 MUST **不含重复地址** ⇒ 目标指令 MUST 只被取一次

#### Scenario: 正确预测不产生 flush

- **WHEN** 预测与实际一致
- **THEN** MUST **不**产生 flush 事件 ⇒ 目标指令的副作用 MUST 正常发生

### Requirement: bp-mispredict-recovery

当预测与实际**不一致**时，MUST 恢复到**正确的顺序取指地址**，并抑制**错路指令**的副作用。

#### Scenario: 纠错目标是真正的 fall-through

- **WHEN** 预测为 taken 但实际为 not-taken
- **THEN** 纠错目标 MUST 为「**该分支的 fall-through 地址**」（分支所在地址 + 4），MUST NOT 为「错误目标 + 4」

#### Scenario: 错路指令副作用被抑制

- **WHEN** 发生误预测
- **THEN** 错路指令的写回 / 存储 / 分支决策 MUST 被抑制（三个消费方：分支决策、存储写回、寄存器写回）

#### Scenario: flush 的双分量语义

- **WHEN** flush 信号由多个来源合成
- **THEN** 「分支已 taken」分量 MUST 被替换为「预测错误」分量，而**停顿（freeze）分量 MUST 保留** ⇒ 两者语义 MUST NOT 相互覆盖

### Requirement: bp-pc-mux-nesting-contract

取指地址的选择 MUST 遵循**五级优先级**：异常重定向 > 分支实际结果（含误预测纠错）> 停顿保持 > 分支投机预测 > 顺序取指。

#### Scenario: 投机预测位于停顿之下

- **WHEN** 处于停顿（fetch 被保持）状态
- **THEN** 投机预测 MUST **不得**推进取指地址（停顿期间地址保持不变）

#### Scenario: 与既有协议一致

- **WHEN** 核对取址 mux 的嵌套顺序
- **THEN** MUST 与 `fetch_stall_protocol_v0.11.0.md`（rev.14）规定的「异常 > 分支 > 停顿 > 顺序」一致，且**投机预测**插入在**停顿之下、顺序之上**

### Requirement: bp-mechanism-verdict

机制验证 MUST 产出**终局式**结论：成立则放行后续实现；不成立 MUST 记录结论、落 ADR、并按既有临时口径结案，MUST NOT 悬空。

#### Scenario: 成立

- **WHEN** 测量显示 flush 事件数显著下降且 cycles 下降量与下降的 flush 事件数相符
- **THEN** MUST 放行完整实现

#### Scenario: 不成立

- **WHEN** 测量显示 flush 事件数不降
- **THEN** MUST ① 记录实测数据 ② 在性能门禁 ADR 中落**结论**（绝对值门禁恢复 / **不恢复**）③ 不恢复时沿用既有**临时口径**（cycle 比率上限 + 说明标注）④ **关闭**本验证项，MUST NOT 以「等待回复」结束

#### Scenario: 判定量

- **WHEN** 判定机制是否成立
- **THEN** 判定量 MUST 为 **flush 事件数**与 **总周期数**（MUST NOT 使用「taken 转移数」—— 该量在预测生效后是不变量，不随收益变化）

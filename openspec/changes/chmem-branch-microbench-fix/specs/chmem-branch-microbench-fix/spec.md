## ADDED Requirements

### Requirement: branchbench-dynamic-instruction-parity

分支微基准的各档 MUST 具备**相同的动态指令数**。`.text` 逐字节相同**仅**保证静态代码一致，MUST NOT 被当作动态指令数相同的证据。

#### Scenario: 档位选择不得编译成分支

- **WHEN** 对 `branch_cond` 的反汇编取指令
- **THEN** 该函数 MUST **不含任何条件分支**（只用 `sltiu`/`seqz`/`and`/`or` 等无分支算术实现档位选择）⇒ 4 档的动态路径长度相同

#### Scenario: 分支两条路径须等长

- **WHEN** 对 `branch_sel` 的反汇编分别统计 taken 与 not-taken 两条路径的指令数
- **THEN** 两条路径的指令数 MUST **相等** ⇒ 方向变化不改变每迭代的工作量

#### Scenario: 动态指令数相等须可证

- **WHEN** 在 CH_MEM/Verilator 后端测量 4 档的 **取指地址变化次数**（取指地址被保持的 cycle 不计入）
- **THEN** 该计数 MUST **逐档相同**（= 实际取到的指令数一致 ⇒ 工作量一致）⇒ ★ 由此 `cycles` 的跨档差异 MUST **全部**归因于气泡（取指地址被保持的 cycle）

#### Scenario: 工作量计数须常驻可观测

- **WHEN** 运行测量脚本（带统计开关）
- **THEN** MUST 输出**取指地址变化次数**与**总周期数**两个 host 可见计数 ⇒ 工作量一致性与气泡数 MUST 可复核（**MUST NOT** 依赖「临时改 harness」的一次性测量）。★ 该计数由**取指地址轨迹**导出（本 change 不触碰 `ip/**`）；★ **冲刷（flush）事件计数归前置2** —— 冲刷是 DUT 内部信号 ⇒ host 不可直接见 ⇒ 计数须改 `ip/cpu/plugins/ibus_chmem.h` ⇒ **违反本 change 的「不得触碰 `ip/**`」边界**

### Requirement: branchbench-mode-name-direction

分支微基准的档名 MUST 与该档的**硬件分支方向**一致。档名与硬件方向相反时，任何按档名推断收益的推理都会得到相反结论。

#### Scenario: 档名方向须核对

- **WHEN** 核对每个档位对应的 `branch_sel` 分支在硬件上的实际方向
- **THEN** 档名 MUST 反映该实际方向；★ **当两个档名与硬件方向互换时，MUST 互换重命名**（而非仅在注释中说明）

#### Scenario: 重命名须同 commit 传播

- **WHEN** 档位重命名
- **THEN** CSV 的 `mode_name` 列、测量脚本的模式名数组、计划/文档中的档名引用 MUST 在**同一 commit** 内同步更新（否则按名查找基线行的逻辑失败 ⇒ 门禁立刻红）

### Requirement: branchbench-baseline-rerecord

修正后的基线 MUST 重新录制，并 MUST 显式记录「四档 taken 分支数 / iter」以供下游判据评估信号强度。

#### Scenario: 基线重录

- **WHEN** 修正后重录基线
- **THEN** 基线 CSV 的 cycles 数值 MUST 更新（旧值作废）且 notes MUST 说明本次修正的原因

#### Scenario: taken 分支数须报告

- **WHEN** 基线重录完成
- **THEN** MUST 报告 4 档各自的 **taken 分支数 / iter** ⇒ 下游 MUST 据此判定跨档判据是否具备足够信号强度

#### Scenario: 跨档信号不足时的判据出口

- **WHEN** 跨档 taken 转移数差 **≤ 1 拍/iter**（★ **含恰好 1.0** —— 本基准每 iter 恰 1 条条件分支 ⇒ 跨档差恰好 = 1.0 ⇒ 该条件对本基准恒真 ⇒ 跨档判据结构性不可用 ⇒ 下游性能判据 MUST 退化为「单档绝对差值 + 解析模型」）
- **THEN** MUST 明确采用「**单档绝对差值 + 解析模型**」判据 ⇒ **不得**以「增加负载以制造信号」的方式回避

### Requirement: branchbench-partial-falsification-record

对已归档 0c change 的部分证伪 MUST 留痕，且 MUST NOT 改动归档产物。

#### Scenario: 事实与推论须分开记录

- **WHEN** 记录 0c 的 AC-2 结论
- **THEN** MUST 明确区分：「`.text` sha256 一致」这一**事实仍成立**；「`.text` 一致 ⇒ 动态指令数相同」这一**推论已被证伪** ⇒ 证伪记录写入本 change 的提案与基线 notes（**不得**修改已归档的 change 目录）

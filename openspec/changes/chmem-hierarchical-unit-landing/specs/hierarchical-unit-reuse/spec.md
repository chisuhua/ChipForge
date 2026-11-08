## ADDED Requirements

### Requirement: 层 2 单元可独立复用

层 2 单元 MUST 能经 `ch_device<Unit>` **脱离 CPU 上下文**独立 elaborate 并发射 Verilog；
Wrapper 形态（`ch_device<Wrapper>`，Wrapper 内含 `ch_layer`）MUST 产生正确的**父路径命名**。

#### Scenario: 单元脱离 CPU 可独立 elaborate
- **WHEN** 用 `ch_device<RegFileComponent>` 单独构建一个最小 device 并调用发射
- **THEN** MUST 成功发射 Verilog，且端口列表与该单元的 `io_type` 一一对应

#### Scenario: 父路径命名不撞名
- **WHEN** 单元被包在含 `ch_layer` 的 Wrapper 内实例化两次
- **THEN** 两次实例的端口/节点名 MUST 逐字不同，且 MUST 带父层前缀

### Requirement: 单元 manifest 键集必须与 allowlist 一致

落地单元 MUST 提供 `manifest`（逻辑契约名 → ctx 节点）；闸① MUST 校验
「扫到的节点集合」与「期望键集」一致；allowlist 中每项 MUST 对应一个真实节点。
同一 case 中两侧（`manifest` provider / `identity` provider）解析出的**逻辑契约名集合** MUST 一致。

#### Scenario: 键集一致通过
- **WHEN** 落地单元的 manifest 覆盖全部期望键，且 allowlist 无多余项
- **THEN** 闸① MUST 判 PASS

#### Scenario: allowlist 多余项报红
- **WHEN** 从 manifest 删除一个 allowlist 仍声明的键
- **THEN** 闸① MUST 失败并指出缺失键

#### Scenario: 两侧契约名集合一致
- **WHEN** 同一 case 分别用 `manifest` provider 与 `identity` provider 解析
- **THEN** 两次得到的逻辑契约名集合 MUST 相等

### Requirement: 层次化产物必须可被工具链接受

层次化产物（单文件多 module）MUST 可被 `verilator --lint-only --top-module top` 接受；
且层次化与扁平的**行为等价** MUST 由 `ch::hier_eq` 机制验证（不得只靠文档声明）。

#### Scenario: lint 通过
- **WHEN** 对层次化产物执行 `verilator --lint-only --top-module top <file>`
- **THEN** MUST exit 0，且 stderr 无 `%Error`

#### Scenario: 逐拍等价
- **WHEN** 同一 ctx 同时发射扁平与层次化两份产物并逐拍比对
- **THEN** 两者 MUST 行为等价（用上游 `ch::hier_eq` 作为机制锚）
- **AND** `GranularityScope(ForceInline)` 下 MUST 与扁平产物逐字节一致

### Requirement: 门禁仍消费扁平产物

层次化产物 MUST NOT 改变 parity 门禁的输入面：门禁仍消费**扁平** `top.v`。
层次化产物的用途限于交付 / 可单独综合 / 单元级复用。

#### Scenario: 门禁输入面不变
- **WHEN** 引入层次化单元后运行 parity 门禁
- **THEN** 门禁 MUST 仍以扁平 `top.v` 为输入，判定语义 MUST 不变
- **AND** 丢弃整组层次化产物 MUST 不影响门禁结果（回滚路径）

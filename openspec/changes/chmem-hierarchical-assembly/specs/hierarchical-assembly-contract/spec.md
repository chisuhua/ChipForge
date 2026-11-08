## ADDED Requirements

### Requirement: 单元装配必须经 root 登记

层 2 单元 MUST 经 `PipeBuilder` 持有的 root 组件的 `create_child<T>(name)` 创建，
MUST NOT 用裸 `make_unique` 直接构造。调用方 MUST 检查返回指针非空；
上游失败时 `create_child` 返回 `nullptr`。

一个 ctx 恰有一个 root；root 生命期由 `PipeBuilder` 管理。

#### Scenario: 单元登记后出现在组件树里
- **WHEN** 一个单元经 `root.create_child<RegFileComponent>("regfile")` 创建并调用 `build(pb.ctx())`
- **THEN** 该 unit 的 owner MUST 是 root，且 MUST 出现在 `toVerilogHierarchical` 的层次化产物里
- **AND** 若改为裸 `make_unique` 创建（不经 `create_child`）⇒ 层次化发射 MUST FATAL

#### Scenario: create_child 失败必须被检查
- **WHEN** `create_child` 返回 `nullptr`（上游创建失败）而调用方未检查
- **THEN** 后续 `build()` MUST 因空指针而失败，MUST NOT 静默跳过该单元

#### Scenario: 三态化节点解析
- **WHEN** 按名字解析 ctx 中的节点
- **THEN** 结果 MUST 为 `kOk` / `kMissing` / `kAmbiguous` 三态之一
- **AND** `kAmbiguous`（同名多个节点）MUST NOT 返回「首个节点」继续执行

### Requirement: 顶层端口必须在 root scope 声明

顶层端口 MUST 经 `PipeBuilder::declare_in` / `declare_out` 创建，
且创建点 MUST 处于 root-scope（`Component::set_current(root)` 生效、`scope_depth() == 0`）。
跨单元连接 MUST 落在 root scope（桥接 / `Payload`）；MUST NOT 组件之间 sibling 直连。

#### Scenario: 顶层端口归属正确
- **WHEN** 在 root-scope 调用 `declare_in` 创建顶层输入端口
- **THEN** `port_provenance` 中该端口 MUST 为 `Bubbled`（顶层 input）/ `InlineFolded`（顶层 output）
- **AND** clock/reset 端口 MUST 为 `ClockReset`，单元自身端口 MUST 为 `Self`

#### Scenario: 非 root-scope 声明被拒
- **WHEN** 在非 root-scope（`scope_depth() > 0`）调用 `declare_in`
- **THEN** 框架 MUST fail-closed，MUST NOT 产生归属落在非 root 组件的端口

### Requirement: 归属守必须在装配收尾时 fail-closed

框架构建收尾时，ctx 中**除 clock/reset 外**的每个节点，其 owner MUST 属于
`{root}` ∪ `{已登记单元}`。违反 ⇒ MUST 立即失败（不得留到层次化发射期才暴露）。

#### Scenario: 越界归属立即失败
- **WHEN** 某节点被挂到 root 之外、也不是任何已登记单元的子节点
- **THEN** 装配收尾 MUST 以 `CHREQUIRE` 失败
- **AND** 错误信息 MUST 指出该节点名与当前 owner

### Requirement: build 幂等语义必须精确定义

`PipeBuilder::build()` 的重复调用语义 MUST 显式定义：
同一状态重复调用 ⇒ **成功早退**（MUST NOT 重跑 `negotiate` / `setup` / `build`，MUST NOT 产生重复登记）；
状态已变 ⇒ MUST 显式失败。

#### Scenario: 同状态重复调用成功早退
- **WHEN** 在未改变任何配置的情况下第二次调用 `build()`
- **THEN** MUST 返回成功，且 MUST NOT 重跑 `negotiate/setup/build`
- **AND** MUST NOT 出现重复的单元登记

#### Scenario: 状态已变必须显式失败
- **WHEN** root 或单元集合已变化后再次调用 `build()`
- **THEN** MUST 显式失败，MUST NOT 静默重跑

### Requirement: 层 2 单元端口必须是标量

层 2 单元的 `io_type` 每个成员 MUST 是标量 `ch_in` / `ch_out`（`ch_uint<N>` / `ch_bool`）；
MUST NOT 使用 bundle / `ch_stream` / `ch_mem` / `ch_reg` 作端口。
该约束 MUST 在**编译期**由层 2 基类校验。

#### Scenario: 合法单元通过编译
- **WHEN** 单元的 `io_type` 全部成员为 `ch_in<ch_uint<32>>` / `ch_out<ch_bool>` 之类标量
- **THEN** 编译 MUST 通过，且该单元可被 `ch_device<...>` 独立实例化

#### Scenario: bundle 端口编译不过
- **WHEN** 把某成员改成 bundle / `ch_stream<...>` 作端口
- **THEN** 编译 MUST 失败（MUST NOT 只在运行期或发射期报错）

### Requirement: provider 槽位单例与登记闸 fail-closed

契约绑定 provider 的槽位 MUST 按 ctx 划分、每 ctx 恰一个；重复安装 MUST 立即失败
（结构上装不出双轨）。登记闸① 在 manifest 缺失时 MUST 返回 `ERROR(2)` + `manifest_missing`
（MUST NOT 退化为「有 manifest 才查」）。allowlist 变更 MUST 经 architect 审。

#### Scenario: 重复安装 provider 失败
- **WHEN** 同一 ctx 上第二次安装绑定 provider
- **THEN** MUST 立即失败，MUST NOT 覆盖或并存

#### Scenario: manifest 缺失必须报错而不是跳过
- **WHEN** 期望键集已注入但某侧 manifest 缺失
- **THEN** 门禁 MUST 返回 `ERROR(2)` 且 `error_kind = manifest_missing`
- **AND** MUST NOT 判为 PASS

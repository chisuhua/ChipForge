## ADDED Requirements

### Requirement: chmem-branch-predictor-structure

CH_MEM 后端 MUST 具备分支预测器结构：**分支目标缓冲（BTB）** + **方向预测（2-bit 饱和计数器）**，并 MUST 已被 CH_MEM 工厂注册。

#### Scenario: BP 结构存在

- **WHEN** 检查 `ip/cpu/plugins/branch_predictor_chmem.h`
- **THEN** 该文件 MUST 存在并实现 BTB 与 2-bit 方向预测表

#### Scenario: 工厂已注册

- **WHEN** 检查 `cpu_factory_chmem.h`
- **THEN** BP 插件 MUST **已被注册** ⇒ 可通过静态检查验证

#### Scenario: 范围显式化

- **WHEN** 审查 BP 的实现范围
- **THEN** 全局历史预测与返回地址栈 MUST **不在本期范围内**（须可机检：实现文件内 MUST NOT 出现相关标识）⇒ 防止范围蔓延

### Requirement: chmem-btb-entry-layout

BTB 表项 MUST 在**单个 64 位字**内编码分支地址与目标，且 MUST 能区分「有效」与「未占用」。

#### Scenario: 表项恰好 64 位

- **WHEN** 计算表项的位宽需求
- **THEN** 分支地址（32 位）+ 目标地址（32 位）+ 有效标志 MUST 恰好为 **64 位** ⇒ 有效标志 MUST 复用目标的低位（目标按 4 字节对齐 ⇒ 低两位恒为零）

#### Scenario: 未占用的表项不被命中

- **WHEN** 表项处于未占用状态
- **THEN** 查询 MUST 返回「未命中」（MUST NOT 因表项内容为零而误命中）

#### Scenario: 间接跳转不建表项

#### Scenario: 无条件跳转可预测

- **WHEN** 某条无条件跳转被建立表项
- **THEN** 表项 MUST 记录「该跳转是无条件跳转」的标志 ⇒ 预测方向 MUST 为「跳转」（MUST NOT 依赖方向表项 —— 无条件跳转不占方向表项）

- **WHEN** 遇到间接跳转
- **THEN** MUST **不**为其建立 BTB 表项（目标随运行时变化，静态缓存必然错误）⇒ 该跳转 MUST 走顺序取指

### Requirement: chmem-btb-runtime-capacity

分支目标缓冲的**有效条目数** MUST 可在运行时配置（物理容量固定）。

#### Scenario: 逻辑条目数掩码

- **WHEN** 以逻辑条目数 M 配置 BP
- **THEN** 索引 MUST 被掩码限制在 M 范围内（M ∈ 16/32/64/128/256）；M 超出合法域 MUST **快速失败**（MUST NOT 静默截断）

#### Scenario: 小容量下的表项区分

- **WHEN** M 小于物理容量
- **THEN** 表项 MUST 仍以**完整分支地址**作区分 ⇒ MUST NOT 因地址裁剪而产生结构性误命中

### Requirement: chmem-bp-direction-learning

方向预测器 MUST 从**已解析的分支**学习，无论该分支实际是否跳转。

#### Scenario: 未跳转的分支也被学习

#### Scenario: 饱和计数器语义

- **WHEN** 方向计数器的值被更新
- **THEN** 计数 MUST 按「0→1→2→3（饱和，不回退）」演化；★ 预测为「跳转」的阈值 MUST 为「计数 ≥ 2」

- **WHEN** 一个未跳转的条件分支被解析
- **THEN** 方向预测表 MUST 被更新（MUST NOT 仅在跳转时更新 ⇒ 否则预测器学不到「不跳转」⇒ 在以不跳转为主的负载上产生净负收益）

#### Scenario: 更新地址与预测地址一致

- **WHEN** 更新方向预测表
- **THEN** 更新所用的地址 MUST 与预测时所用地址指向**同一条分支**（MUST NOT 出现错位一格 ⇒ 否则学到错误表项）

#### Scenario: 第二次取同一条分支时预测命中

- **WHEN** 同一条分支第二次被取到
- **THEN** 预测 MUST 命中（MUST NOT 仍为未命中）

### Requirement: chmem-bp-speculative-fetch

分支预测 MUST 作用于**下一次取指地址**的选择，使正确预测的跳转 MUST **不产生**多余的取指拍。

#### Scenario: 预测在取指侧完成

- **WHEN** 在取指阶段查询分支目标缓冲
- **THEN** 查询 MUST 为**零延迟**（异步读）⇒ 预测结果 MUST 可在同一拍内被取址决策消费

#### Scenario: 正确预测不取中间路径

- **WHEN** 预测的方向与目标均正确
- **THEN** 下一取指地址 MUST 直接为目标地址 ⇒ 中间路径地址 MUST **不被取出**

### Requirement: chmem-bp-pc-mux-contract

取指地址的选择 MUST 遵循固定优先级：异常重定向 > 分支实际结果（含误预测纠正）> 停顿保持 > 分支投机预测 > 顺序取指。

#### Scenario: 投机预测位于停顿之下

- **WHEN** 取指被停顿保持
- **THEN** 投机预测 MUST **不得**推进取指地址

#### Scenario: 与既有取址协议一致

- **WHEN** 核对取址决策的嵌套顺序
- **THEN** MUST 与既有取址协议规定的一致（异常 > 分支 > 停顿 > 顺序），投机预测插入在停顿之下、顺序之上

### Requirement: chmem-bp-mispredict-recovery

预测与实际不一致时，MUST 恢复到正确的顺序取指地址，并抑制错路指令的副作用。

#### Scenario: 纠正目标是分支的顺序后继

- **WHEN** 预测为跳转但实际未跳转
- **THEN** 纠正目标 MUST 为该分支的**顺序后继地址**（分支地址 + 4），MUST NOT 为错误目标 + 4

#### Scenario: 正确预测时抑制自身重定向

- **WHEN** 预测与实际一致
- **THEN** 该分支自身的重定向 MUST 被抑制 ⇒ 目标指令 MUST 只被取一次（取指地址序列 MUST 无重复）

#### Scenario: 错路指令副作用被抑制

- **WHEN** 发生误预测
- **THEN** 错路指令的**分支决策 / 存储写回 / 寄存器写回** MUST 被抑制

#### Scenario: 停顿分量不被覆盖

- **WHEN** 停顿信号与预测错误信号同时存在
- **THEN** 停顿分量 MUST 保留（MUST NOT 被预测错误分量覆盖）

### Requirement: chmem-bp-observable-benefit

分支预测的收益 MUST 可被机械判定，且 MUST 有**结构性等式**（零阈值）作为锚。

#### Scenario: 校验和逐位相等

- **WHEN** 在预测启用与禁用两种状态下运行同一负载
- **THEN** 校验和 MUST **逐位相等**（预测 MUST NOT 改变程序语义）

#### Scenario: 零误预测档的等式

- **WHEN** 在**无误预测**的负载档位上比较两态周期数
- **THEN** 周期差 MUST **逐位等于**「该档在禁用态下的跳转转移数 × 每次转移的额外取指拍数」⇒ 把「每次转移 1 拍」从**分类恒等式**升级为**实测等式**

#### Scenario: 净收益模型含误预测代价

- **WHEN** 在存在误预测的档位上计算净收益
- **THEN** 净收益 MUST 为「正确预测数 × 每次正确预测节省拍数 − 误判为跳转数 × 每次误预测代价拍数」⇒ **MUST NOT** 忽略误预测项

#### Scenario: 事件计数可观测

- **WHEN** 运行测量
- **THEN** MUST 输出**已执行指令数**与**停顿/冲刷事件数**两个计数 ⇒ 判据 MUST 可复核

### Requirement: chmem-bp-toggle-surface

BP MUST 提供**可关闭**的配置面，以便比较启用与禁用的行为。

#### Scenario: 工厂参数

- **WHEN** 构建 CH_MEM CPU
- **THEN** MUST 接受「是否启用 BP」的参数，且**默认启用**（避免「未生效被误判为无效」）

#### Scenario: 命令行开关

- **WHEN** 运行仿真器
- **THEN** MUST 提供关闭 BP 与设置条目数的开关，且**关闭开关优先**

#### Scenario: 生效值可观测

- **WHEN** 仿真器启动
- **THEN** MUST 在标准错误流回显 BP 的**实际生效状态**与条目数 ⇒ 测量脚本 MUST 据此断言与请求一致

#### Scenario: 参数转发完整

- **WHEN** 通过任一构建入口构建 CPU
- **THEN** BP 参数 MUST **被完整转发**（MUST NOT 在某条路径上丢失 ⇒ 行为不一致）

### Requirement: chmem-bp-ledger-reclassification

BP 相关台账条目 MUST **逐条**重判，且族级判定行与裁决标注 MUST 同步更新。

#### Scenario: 条目态与理由齐备

- **WHEN** 审查 BP 相关台账条目
- **THEN** 每条 MUST 有明确状态与**显式**理由；MUST NOT 以继承标记充当理由；同一理由的条目数 MUST 不超过上限

#### Scenario: 族级与裁决行同步

- **WHEN** 更新条目态
- **THEN** 族级判定行、涉及用例数、总数计数、以及裁决标注段 MUST **同步更新**（MUST NOT 出现条目与族级矛盾）

#### Scenario: CH_MEM 用例名不与 TLM 独占名冲突

- **WHEN** 新增 CH_MEM 侧用例
- **THEN** 用例名 MUST 与 TLM 独占用例名**零交集**（⇒ 落库前 MUST 先快照 TLM 独占名单）

### Requirement: chmem-bp-root-cause-correction

性能根因的表述 MUST 与实测一致，MUST NOT 沿用已被证伪的说法。

#### Scenario: 根因表述修订

- **WHEN** 修订性能门禁 ADR 的根因段
- **THEN** MUST 删除「参考实现侧存在预测器 ⇒ 预测命中不冲刷」这一**已被证伪**的说法（实测：移除该预测器后周期数逐位不变）⇒ MUST 改述为「**单遍模型重定向零代价 vs 两级取指每次跳转 1 拍额外取指**」并附实测数据

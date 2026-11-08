## ADDED Requirements

### Requirement: simulator-elf-cli

Simulator 侧 MUST 提供通用 ELF CLI runner，且其可调参数与 Verilator 侧同名同义，使同一 ELF 能在两侧用同一组开关驱动。

#### Scenario: CLI 面镜像

- **WHEN** 用同一组参数分别调用 Simulator 侧与 Verilator 侧 runner
- **THEN** 两侧 MUST 接受 `--elf` / `--cycles` / `--no-bp` / `--btb-entries` / `--no-bp-uncond` / `--no-bp-gshare` / `--enable-mmu` / `--mmu-mode`

#### Scenario: reset 模式 fail-fast

- **WHEN** 传入 `--reset` 且值不是 `none`
- **THEN** runner MUST 立即失败（退出码 2）⇒ MUST NOT 静默按其他模式运行

#### Scenario: 采集不得每拍线性扫描

- **WHEN** 采集层解析观测量节点
- **THEN** MUST 一次性解析 node id 后直读 `data_map()`
- **AND** MUST NOT 每拍调用 `get_value_by_name` 或使用 `get_signal_id_by_name`

### Requirement: four-observables-simulator-side

Simulator 侧 MUST 采集 `tohost` / `regs` / `cycles` / `pchash` 四项，并在 `memsig` 未实现时**默认判 ERROR**。

#### Scenario: regs 覆盖 x1..x31

- **WHEN** 采集 `regs`
- **THEN** MUST 按 `x1..x31` 升序、每项 32-bit 小端，拼成 124 字节后取 hash ⇒ `x0` MUST NOT 参与

#### Scenario: pchash 为有序流

- **WHEN** 采集 `pchash`
- **THEN** MUST 每拍按序喂 PC 值，并记录样本数
- **AND** MUST NOT 用直方图替代

#### Scenario: memsig 未实现时 fail-closed

- **WHEN** `memsig` 尚未实现且未显式放行
- **THEN** runner MUST 以退出码 2 结束，且报告中标 `memsig: null`

### Requirement: determinism-export

Simulator 侧 MUST 导出可用于跨后端确定性比对的两项：DAG `node_count` 与顶层信号名名单 hash。

#### Scenario: 名单 hash 可复现

- **WHEN** 对同一 ELF + 同一配置重复运行
- **THEN** `node_count` 与 `names_hash` MUST 逐次相同

#### Scenario: 与 Verilator 侧可比

- **WHEN** 两侧各自导出 `node_count` / `names_hash`
- **THEN** 二者 MUST 可直接比较（同一取法）⇒ 不等时门禁归 `ERROR` / `determinism_mismatch`

### Requirement: cycle-count-parity-evidence

同一 ELF 在两侧的总拍数 MUST 相等，作为「逐拍语义」的第一条实测证据。

#### Scenario: 单 ELF 对齐

- **WHEN** 用 `rv32ui-p-add` 在两侧各跑一次
- **THEN** 两侧 `cycles` MUST 相等（当前实测 478 / 478）且 `tohost` 均为 1

### Requirement: measured-throughput-recorded

Simulator 侧实测吞吐 MUST 记录进契约，且一切按拍数估算的时长预算 MUST 以实测为准。

#### Scenario: 吞吐不得沿用旧估计

- **WHEN** 评估 branchbench 或全量 corpus 的时长
- **THEN** MUST 使用实测吞吐（约 9.1 拍/s）
- **AND** Simulator 侧 branchbench 的 cycle cap MUST ≤ 20,000 拍，否则该档登记为「仅 Verilator 侧可跑」的显式豁免

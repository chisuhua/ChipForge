# chmem-branch-microbench Specification

## Purpose
Provides a **branch-sensitive, prediction-rate-controlled micro-benchmark** for the CH_MEM/Verilator backend, so that the branch predictor (B1 ② `chmem-bp-btb-gshare`) can be accepted by a **mechanically decidable** criterion instead of an absolute CoreMark score. The benchmark runs four load patterns (always-taken, always-not-taken, 6-of-8 periodic, pseudo-random) whose **executable code is byte-for-byte identical** (the only difference is one data word holding the pattern id) so that a cycle-count difference can only come from the branch direction and never from a workload difference. The benchmark reports a 32-bit checksum through the `tohost` word (with the low bit forced so the simulator's done bit is always set, making the reported cycle count the true completion moment); the host verifies that checksum against the baseline CSV, so a wrong result fails loudly instead of producing a number. Its baseline is a **no-predictor reference**; the predictor's benefit is the cycle delta measured on the same load afterwards.

## Requirements

### Requirement: branchbench-load-isolation

分支微基准的各预测率档 MUST 具备**逐字节相同的可执行代码**：档位差异 MUST 仅体现为数据中的一位/一个字，MUST NOT 改变指令序列（否则 cycle 差会混入工作量差异，度量失效）。

#### Scenario: 各档机器码一致

- **WHEN** 对 4 个档位 ELF 的 `.text` 段取 sha256
- **THEN** 4 个 sha256 **完全相同**；4 个 ELF 的差异仅在数据段

#### Scenario: 档位不得编译期折叠

- **WHEN** 档位值以 `volatile` 数据提供（而非编译期常量）
- **THEN** 编译器**不得**把档位选择折叠为常量 ⇒ 生成的指令序列各档一致

### Requirement: branchbench-elf-layout

分支微基准 ELF MUST 以**恰好两个** `PT_LOAD` 段交付：`R E`（`.text`）+ `RW`（`tohost` 独占 1 word + 数据）。MUST NOT 存在纯 R 段。`tohost` MUST 位于 0x80001000（RW 段首 word）。

#### Scenario: 段布局可机检

- **WHEN** 执行 `readelf -l` 与 `readelf -s`
- **THEN** 恰有 2 个 `PT_LOAD`（权限 `R E` 与 `RW`）；`tohost` 符号值 = 0x80001000

#### Scenario: 纯 R 段不被接受

- **WHEN** ELF 含纯 R 段
- **THEN** CH_MEM 的段路由无对应分支 ⇒ 该段**被整段丢弃**（只读常量读成 0 ⇒ 分数静默错）⇒ 门禁 MUST 拒绝此类 ELF

### Requirement: branchbench-result-verification

微基准 MUST 在宿主侧校验**结果正确性**：程序 MUST 把 32-bit 校验和回传（经 `tohost`，LSB 置 1 以使 runner 的 done 位恒成立 ⇒ cycle 计数为真实完成时刻），宿主 MUST 将其与基线 CSV 的期望值**逐位比较**。不符时 MUST 失败且 MUST NOT 产出数值。

#### Scenario: 校验和不符即失败

- **WHEN** 基线期望值与实测 `TOHOST` 不等
- **THEN** 测量脚本返回非零并报出**实测值与期望值**，且不产出该档的数值

#### Scenario: 超时不得被当作通过

- **WHEN** 输出含 `TIMEOUT`
- **THEN** 测量脚本判为**失败**（不得仅看进程退出码 —— runner 的判据 `tohost==1` 对本基准无意义）

### Requirement: branchbench-baseline-and-ci

基线 MUST 只含 CH_MEM/Verilator 后端数据，MUST 提供 `--check-baseline` 模式做**精确相等**（无容差）的漂移检测。MUST 提供 CI 门禁步骤（warn 级）与 ctest entry。基线 MUST 标注为**微基准**（绝对值不代表真实应用）且为**无 BP 参照值**。

#### Scenario: 漂移可检出

- **WHEN** 基线中的 cycles 与重跑实测值不等
- **THEN** 脚本报出差值并返回非零（精确相等口径）⇒ 任何漂移 MUST 通过**显式重录基线 + commit 说明原因**处理

#### Scenario: CI 缺产物时显式跳过

- **WHEN** `cpu_verilator_sim` 或 4 个 ELF 之一不存在
- **THEN** CI 步骤显式输出 SKIP 并成功退出（MUST NOT 误判为通过）

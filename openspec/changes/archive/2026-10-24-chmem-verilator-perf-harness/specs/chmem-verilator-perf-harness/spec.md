## ADDED Requirements

> Capability topic: `chmem-verilator-perf-harness`（交付点 A：Dhrystone 测量链路）。上游 = ADR-088 v1.1 §5.2 判据 4。CoreMark 部分属交付点 B，不在本 spec。

### Requirement: chmem-perf-harness-elf-layout

Dhrystone 的可执行 ELF MUST 以**恰好两个** `PT_LOAD` 段交付：段 0 = **RX**（`.text`），段 1 = **RW**（tohost 独占 1 word + `.rodata`/`.data`/`.bss`）。MUST NOT 存在**纯 R** 段。`tohost` 符号 MUST 落在 0x80001000（= RW 段首 word），`.rodata` 起始 MUST **≥** `tohost + 4`。

#### Scenario: ELF 段布局可机检

- **WHEN** 对交付的 `dhrystone.elf` 执行 `readelf -l` 与 `readelf -s`
- **THEN** 恰有 2 个 `PT_LOAD` 且权限分别为 `R E` 与 `RW`；无第三个 LOAD 段；`tohost` 符号值为 0x80001000；`.rodata` 的起始地址 ≥ 0x80001004

#### Scenario: 单 RWE 段不被接受

- **WHEN** ELF 含**单个** `RWE` LOAD 段（.text 与数据同段）
- **THEN** CH_MEM 的段路由（`is_exec && !is_writable → IBus`）不成立 ⇒ 该 ELF **不可**用于测量（取指为空）⇒ 门禁/测量脚本 MUST 拒绝此类 ELF 而非产出错误数值

### Requirement: chmem-perf-harness-reproducible-build

`dhrystone.elf` MUST 由仓内构建脚本可复现生成：连跑两次 MUST 产出**字节一致**的文件，且 `nm -u` MUST 无未定义符号（`-nostdlib` 链接下尤其须排除 `memcpy`/`memset` 外部调用）。

#### Scenario: 构建可复现

- **WHEN** 连续执行两次 `build_manual_elf.sh` 的 dhrystone 目标
- **THEN** 两次产出的 ELF 通过 `cmp` 判定为字节一致，且 `nm -u` 输出为空

### Requirement: chmem-perf-harness-measurement

测量 MUST 基于 `cpu_verilator_sim` 输出的 `TOHOST` 与 `CYCLES` 字段（`CYCLES` 来自 `sim_main.cpp` 的周期计数器），MUST NOT 使用 wall-clock、elaboration 或编译耗时。测量 MUST 同时校验**程序结果正确性**（Dhrystone 迭代数 = 2000 且 checksum 与参考一致）。MUST 以**精确相等**判定「基线未漂移」（不使用容差）。

#### Scenario: 结果错误时不产出数值

- **WHEN** 被测程序未把 tohost 写为 1（或 checksum 不符）
- **THEN** 测量脚本**失败**（非零退出 + 报出）且**不**写入基线 CSV ⇒ 数值不得在结果错误时静默产出

#### Scenario: 超时不得被当作通过

- **WHEN** `cpu_verilator_sim` 输出含 `TIMEOUT`（即便其进程退出码为 0）
- **THEN** 测量脚本判定为**失败**（必须以 `TOHOST==1` 为通过条件，不得仅看退出码）

#### Scenario: 基线漂移可检出

- **WHEN** 基线 CSV 中的 `cycles` 与重跑实测值不相等
- **THEN** 校验脚本报出差值并返回非零（精确相等口径）⇒ 任何漂移 MUST 通过**显式重录基线 + commit 说明原因**处理，不得被容差吸收

### Requirement: chmem-perf-harness-ci-gate

MUST 提供 CI 门禁步骤（warn 级）调用测量脚本执行「基线未漂移」校验并接入架构门禁 workflow。**仅新增 ctest entry 不满足该要求**（CI 不运行 ctest）。

#### Scenario: CI 可见违规

- **WHEN** 基线漂移或结果校验失败
- **THEN** CI 步骤输出 `::warning` 标注与具体差异 ⇒ 违规浮到 PR 的 Annotations 面板而非仅埋在日志中

### Requirement: chmem-perf-harness-baseline-provenance

基线 CSV MUST 只含 CH_MEM/Verilator 后端的数据行，MUST NOT 修改既有的 TLM 基线文件。notes MUST 标注：① workload 为**裁剪版** Dhrystone（其 DMIPS 不代表完整工作负载）② cycle 数与 TLM **不可比**（ADR-088 §3）③ 旧版单 RWE 段 ELF 的 cycle 值**不可与新值比较**。

#### Scenario: TLM 基线不被污染

- **WHEN** 核对 `git diff` 与两个 CSV 的内容
- **THEN** 既有的 `dhrystone-baseline-matrix.csv`（TLM legacy/fsm 行）**零改动**；新 CSV 仅含 `mode=verilator` 行

# vexii-riscv-parity-runner — VexiiRiscv 客观对拍测试 runner 规格

> **承接**：[`../../proposal.md`](../../proposal.md) + [`../../design.md`](../../design.md)
> **能力定位**：VexiiRiscv 客观对拍 + CoreMark/MHz 偏差计算 + CI gate 触发

## ADDED Requirements

### Requirement: VexiiRiscv 官方数值 SSOT 加载

The system MUST load VexiiRiscv 官方公开 CoreMark/MHz 数值 from `tests/cpu/parity/vexii_riscv_baseline.json`，校验 SHA256 integrity，并提供 API 给偏差计算模块。

#### Scenario: 加载成功且校验通过
- **WHEN** vexii_riscv_runner 启动并读取 `vexii_riscv_baseline.json`
- **THEN** 系统 MUST 验证 JSON 文件 SHA256 与文件内嵌的 `coremark_sha256` 匹配，并加载 `single_issue.coremark_per_mhz_min/max` 和 `dual_issue_prefetch.coremark_per_mhz` 到内存

#### Scenario: SHA256 校验失败
- **WHEN** vexii_riscv_baseline.json 内容被修改但 `coremark_sha256` 未更新
- **THEN** 系统 MUST 抛 `std::runtime_error` with 错误信息"VexiiRiscv baseline checksum mismatch"，runner 退出码 MUST 为非零

### Requirement: CoreMark v1.01 ELF 加载与执行

The system MUST 加载 CoreMark v1.01 ELF（与 VexiiRiscv 官方使用同版本），并在 ChipForge FPGA bitstream 或 TLM 仿真上执行，记录总 cycle 数。

#### Scenario: CoreMark ELF 加载成功
- **WHEN** vexii_riscv_runner 收到 `--elf coremark_v1.01.elf --config chipforge_v1.0.0.json --target fpga|tlm`
- **THEN** 系统 MUST 解析 ELF、加载到内存、启动 CPU、等待 `tohost=1` 退出信号、记录总 cycle 数 `total_cycles`

#### Scenario: CoreMark ELF 不存在
- **WHEN** `--elf` 指定的 ELF 文件不存在
- **THEN** 系统 MUST 抛 `std::runtime_error` with 错误信息"CoreMark ELF not found"，runner 退出码 MUST 为非零

#### Scenario: CoreMark 编译选项不匹配
- **WHEN** CoreMark ELF 编译时使用的 `-march` / `-mabi` 与 `vexii_riscv_baseline.json` 中 `coremark_compilation` 字段不一致
- **THEN** 系统 MUST 输出 WARN 日志但继续执行（不阻塞），并在 result JSON 中记录 `compilation_mismatch: true`

### Requirement: CoreMark/MHz 偏差计算

The system MUST 计算 ChipForge 实测 CoreMark/MHz 与 VexiiRiscv 官方数值的偏差百分比，应用 PoC-14 与 PoC-15 不同阈值。

#### Scenario: PoC-14 single-issue 偏差 <= 10% PASS
- **WHEN** vexii_riscv_runner 用 single-issue 配置跑完，测得 CoreMark/MHz = 2.45，VexiiRiscv 官方 single-issue 区间 = 2.4–2.6（中点 = 2.5）
- **THEN** 系统 MUST 计算 reference = (2.4 + 2.6) / 2 = 2.5，deviation_pct = abs((2.45 - 2.5) / 2.5) * 100 = 2.00%，与阈值 10% 比较返回 PASS

#### Scenario: PoC-14 single-issue 偏差 > 10% FAIL
- **WHEN** vexii_riscv_runner 用 single-issue 配置跑完，测得 CoreMark/MHz = 1.8，VexiiRiscv 官方 single-issue 区间 = 2.4–2.6（中点 = 2.5）
- **THEN** 系统 MUST 计算 reference = (2.4 + 2.6) / 2 = 2.5，deviation_pct = abs((1.8 - 2.5) / 2.5) * 100 = 28.00%，与阈值 10% 比较返回 FAIL，runner 退出码 MUST 为 1

#### Scenario: PoC-15 dual 全家桶偏差 <= 15% PASS
- **WHEN** vexii_riscv_runner 用 dual 全家桶配置跑完，测得 CoreMark/MHz = 4.8，VexiiRiscv 官方 dual = 5.24
- **THEN** 系统 MUST 计算偏差 = abs((4.8 - 5.24) / 5.24) * 100 = 8.40%，与阈值 15% 比较返回 PASS

#### Scenario: PoC-15 dual 全家桶偏差 > 15% FAIL
- **WHEN** vexii_riscv_runner 用 dual 全家桶配置跑完，测得 CoreMark/MHz = 3.5，VexiiRiscv 官方 dual = 5.24
- **THEN** 系统 MUST 计算偏差 = abs((3.5 - 5.24) / 5.24) * 100 = 33.21%，与阈值 15% 比较返回 FAIL，runner 退出码 MUST 为 1

### Requirement: 偏差结果 JSON 输出

The system MUST 输出结构化 JSON 结果文件，含 CoreMark/MHz 实测值、偏差百分比、PASS/FAIL 状态、VexiiRiscv 官方数值来源 URL，供 CI 与人工 review。

#### Scenario: 输出 PASS JSON
- **WHEN** vexii_riscv_runner 跑完 PASS 的 PoC-14 测试
- **THEN** 系统 MUST 输出 `vexii_parity_result.json` 含字段：`{test_name, chipforge_coremark_per_mhz, vexii_riscv_official_min, vexii_riscv_official_max, deviation_pct, threshold_pct, status: "PASS", source_url, timestamp}`

#### Scenario: 输出 FAIL JSON
- **WHEN** vexii_riscv_runner 跑完 FAIL 的 PoC-15 测试
- **THEN** 系统 MUST 输出 `vexii_parity_result.json` 含字段：`{test_name, chipforge_coremark_per_mhz, vexii_riscv_official, deviation_pct, threshold_pct, status: "FAIL", source_url, timestamp, recommended_action}`

### Requirement: TLM↔CH_MEM 双模式同语义

The system MUST 在 TLM 模式与 CH_MEM 模式下分别跑 CoreMark，验证两个模式的 CoreMark/MHz 偏差 < 5%（ADR-040 v2.0 双模同语义 + ADR-080 byte-equal 协议前置验证）。

#### Scenario: TLM 与 CH_MEM 偏差 < 5% PASS
- **WHEN** vexii_riscv_runner 用 `--target tlm` 与 `--target chmem` 分别跑同一 CoreMark ELF，测得 TLM CoreMark/MHz = 2.4、CH_MEM CoreMark/MHz = 2.42
- **THEN** 系统 MUST 计算 TLM↔CH_MEM 偏差 = abs((2.42 - 2.4) / 2.4) * 100 = 0.83%，与阈值 5% 比较返回 PASS

#### Scenario: TLM 与 CH_MEM 偏差 > 5% FAIL
- **WHEN** vexii_riscv_runner 用 TLM 与 CH_MEM 分别跑，测得偏差 = 8%
- **THEN** 系统 MUST 抛 `std::runtime_error` with 错误信息"TLM↔CH_MEM dual-mode deviation > 5%, likely ADR-040 v2.0 violation"，runner 退出码 MUST 为非零

### Requirement: CI family tag 集成

The system MUST 提供 Catch2 family tag `[vexii-parity]` 让 `./build/bin/chipforge_tests "[vexii-parity]"` 能选择性跑 VexiiRiscv 对拍测试。

#### Scenario: PoC-14 单发射 family tag 隔离
- **WHEN** ctest 执行 `./build/bin/chipforge_tests "[vexii-parity][single]"`
- **THEN** 系统 MUST 仅跑 PoC-14 测试用例，跳过 PoC-15（标 `[v1.4-pending]`）

#### Scenario: PoC-15 dual 全家桶 family tag 隔离
- **WHEN** ctest 执行 `./build/bin/chipforge_tests "[vexii-parity][dual]"`
- **THEN** 系统 MUST 仅跑 PoC-15 测试用例

#### Scenario: 完整 vexii-parity family
- **WHEN** ctest 执行 `./build/bin/chipforge_tests "[vexii-parity]"`
- **THEN** 系统 MUST 跑所有 `[vexii-parity]` family 测试（PoC-14 + PoC-15 在 v1.4+ 后）

### Requirement: PoC-15 v1.4-pending 状态

The system MUST 在 v1.4+ 实装未完成时，PoC-15 测试用例标 `[v1.4-pending]` 并 SKIP（不 FAIL），PoC-15 真实激活条件是 v1.4+ archive 前。

#### Scenario: v1.4 启动前 PoC-15 SKIP
- **WHEN** vexii_riscv_runner 在 v1.0.0 或 v1.3.0 archive 节点跑 PoC-15
- **THEN** 系统 MUST 检测 `CHIPFORGE_VERSION` 环境变量，若 < v1.4 则输出 SKIP 状态而非 PASS/FAIL，runner 退出码 MUST 为 0

#### Scenario: v1.4+ 启动后 PoC-15 激活
- **WHEN** vexii_riscv_runner 在 v1.4+ archive 节点跑 PoC-15
- **THEN** 系统 MUST 跑 PoC-15 全流程，输出 PASS/FAIL 与退出码

### Requirement: 偏差结果归档与历史追踪

The system MUST 把每次跑的结果存入 git-tracked 文件 `tests/cpu/parity/history/vexii_parity_YYYY-MM-DD_HHMMSS.json`，供长期趋势分析。

#### Scenario: 结果文件命名
- **WHEN** vexii_riscv_runner 完成一次跑测
- **THEN** 系统 MUST 用 UTC 时间戳 `YYYY-MM-DD_HHMMSS` 命名结果文件，存入 `tests/cpu/parity/history/`

#### Scenario: 历史文件 Git LFS 跟踪
- 考虑：历史 JSON 文件可能累积（每次跑测都生成），需 Git LFS 跟踪避免仓库过大
  - **WHEN** 仓库启用 Git LFS
  - **THEN** 系统 MUST 用 `.gitattributes` 配置 `tests/cpu/parity/history/*.json` 用 Git LFS 跟踪

### Requirement: VexiiRiscv baseline 版本化

The system MUST 在 `vexii_riscv_baseline.json` 中保留 VexiiRiscv 官方数字的版本号 + 取数日期戳，每次更新 baseline 时人工审核并 git commit。

#### Scenario: baseline 版本字段必填
- **WHEN** vexii_riscv_runner 加载 baseline JSON
- **THEN** 系统 MUST 校验 `version` 字段（如 `"2025-07-01"`）非空、`fetched_at` 字段为 ISO 8601 日期格式

#### Scenario: baseline 过旧 WARN
- **WHEN** baseline JSON 的 `fetched_at` 距当前时间超过 180 天
- **THEN** 系统 MUST 输出 WARN 日志提示"VexiiRiscv baseline stale (>180 days)，需确认官方数字是否更新"，但仍继续执行（不阻塞）
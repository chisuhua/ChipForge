## ADDED Requirements

### Requirement: riscv64-unknown-elf-gcc 工具链安装

build env MUST 安装 riscv64 RISC-V 工具链, Phase 6d 6d.4 riscv-tests RV32I ELF 编译依赖此工具链。

#### Scenario: 工具链可执行文件存在

- **WHEN** `which riscv64-unknown-elf-gcc` 跑
- **THEN** 输出 `/usr/bin/riscv64-unknown-elf-gcc` 或自定义路径

#### Scenario: 工具链版本满足

- **WHEN** `riscv64-unknown-elf-gcc --version` 跑
- **THEN** 输出 GNU 14.0+ 版本信息

#### Scenario: riscv-tests ELF 编译通过

- **WHEN** 跑 `riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 -nostdlib -o /tmp/add.elf add.S` 编译 riscv-tests rv32ui-p-add
- **THEN** 退出码 0, `/tmp/add.elf` 存在且 ELF magic 正确

#### Scenario: CI 集成自动安装

- **WHEN** GitHub Actions `.github/workflows/architecture-gates.yml` 跑
- **THEN** 在 install 阶段自动 `apt install gcc-riscv64-unknown-elf` 或等效命令, CI 不依赖 build env 预装

### Requirement: Verilator + Yosys + iverilog 综合验证工具链

build env MUST 安装 Verilator ≥ 5.020 + Yosys + iverilog, Phase 6d 6d.5 Verilator 集成 + 综合验证依赖。

#### Scenario: Verilator 版本 ≥ 5.020

- **WHEN** `verilator --version` 跑
- **THEN** 输出 `Verilator 5.0XX` 或更高 (≥ 5.020)

#### Scenario: Yosys 综合 hello.v

- **WHEN** `yosys -p "read_verilog /tmp/cpphdl_poc_full.v; synth; stat"` 跑
- **THEN** 退出码 0, 输出含 gate-level netlist stat (无 error)

#### Scenario: iverilog lint hello.v

- **WHEN** `iverilog -t null /tmp/cpphdl_poc_full.v` 跑
- **THEN** 退出码 0, 无 syntax error

#### Scenario: CI 集成自动安装

- **WHEN** GitHub Actions 跑
- **THEN** 自动 `apt install verilator yosys iverilog`, CI 不依赖 build env 预装

### Requirement: ADR-037 v2.0 修订

ADR-037 "Plugin 作为设计范式" MUST 修订为 v2.0, 记录 D4 范式在 elaboration 语义下兑现。

#### Scenario: ADR-037 v2.0 内容

- **WHEN** 起草 `docs/architecture/adr/ADR-037-plugin-as-design-paradigm.md` v2.0
- **THEN** 新增章节: §v2 翻转摘要 (7 大借鉴点 SpinalHDL/VexRiscv/CppHDL → cf::plugin) + §elaboration 纪律 (8 项 CI 检查清单) + §CH_MEM 模式契约 (双文件分离 + `uint_t<N>` 双模 + `array_store` 双缓冲 + `CtrlLink` ch_bool)

#### Scenario: docs/architecture/adr.md 注册表更新

- **WHEN** `docs/architecture/adr.md` ADR-037 注册行更新
- **THEN** 版本字段改 v2.0, 关联 ADR-040 v2.0 + ADR-046

#### Scenario: cross-reference 一致性

- **WHEN** `bash tools/verify_adr.sh` 跑
- **THEN** ADR-037 v2.0 PASS, cross-reference 无 broken

### Requirement: DEVELOPMENT_SETUP.md 工具链安装指南更新

DEVELOPMENT_SETUP.md MUST 添加 riscv64 + Verilator + Yosys + iverilog 安装章节。

#### Scenario: 安装指南完整

- **WHEN** 读 `docs/DEVELOPMENT_SETUP.md`
- **THEN** 包含 riscv64-unknown-elf-gcc (apt + 源码 + 预编译 3 种方式) + Verilator (apt + 源码) + Yosys + iverilog 完整安装命令

### Requirement: PoC follow-up fixes 归档闭环

`poc-follow-up-fixes` change MUST 先 archive, 此 prerequisites change 才能 archive。

#### Scenario: 依赖顺序验证

- **WHEN** `poc-follow-up-fixes` 已 archive (CHANGELOG + openspec archive 命令)
- **THEN** `phase-6d-prerequisites` 才能 archive (显式依赖声明)

#### Scenario: CI 门禁全绿

- **WHEN** `phase-6d-prerequisites` archive 前
- **THEN** `check_plugin_portability.sh` 8/8 PASS + `verify_adr.sh` 0 FAILED + `chipforge_tests_chmem` 全量 PASS (含 PoC follow-up 修复)

### Requirement: Phase 6d main 启动依赖闭环

Phase 6d main change (`phase-6d-rtl-verification`) 启动前, 此 prerequisites change MUST archive。

#### Scenario: 6 项 prereqs 全部完成

- **WHEN** Phase 6d main 启动门槛检查
- **THEN** 6 项 prereqs 全部 archive: #1 riscv64 工具链 + #2 Verilator 工具链 + #3 ADR-037 v2.0 + #4+#5 PoC follow-up fixes + #6 测试 ELF 获取

### Requirement: 测试 ELF 获取 (Oracle 新增, Phase 6d 隐藏阻塞项)

Phase 6d 6d.4 必需的 5 指令 ELF (add/addi/auipc/jal/beq) MUST 可用。当前 `tests/cpu/manual_elf/` 仅含 add, 缺其余 4 条。

#### Scenario: 选项 A manual_elf 编写

- **WHEN** 4 条缺失指令 (addi/auipc/jal/beq) 的 `tests/cpu/manual_elf/*.S` 编写完成
- **THEN** 5 ELF 文件 (`riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 -nostdlib` 编译) 全部存在 + ELF magic 正确

#### Scenario: Phase 6d 6d.4 端到端可跑

- **WHEN** `tests/cpu/manual_elf/{add,addi,auipc,jal,beq}.elf` 5 文件全部存在
- **THEN** Phase 6d 6d.4 不被测试 ELF 缺失阻塞

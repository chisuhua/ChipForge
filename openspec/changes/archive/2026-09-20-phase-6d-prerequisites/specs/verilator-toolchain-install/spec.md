## ADDED Requirements

### Requirement: Verilator 综合验证工具链

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

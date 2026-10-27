# RISC-V CPU IP

## 测试位置

CPU 测试 **不在** `ip/cpu/test/`（该目录已于 2026-06-17 删除），实际位置：

- **单元测试**: `../tests/cpu/test_*.cpp`（14 个 GoogleTest 文件）
  - 详见 [tests/cpu/README.md](../../tests/cpu/README.md)
- **集成测试**: `../tests/cpu/integration/test_*.cpp`（3 阶段 RISC-V 验证）
- **验证规范**: [./docs/verification.md](./docs/verification.md)（前 `ip/cpu/test/README.md`，描述 Level A/B/C 测试层级）

## 概述
基于 CppTLM 的 RISC-V CPU IP 核，采用 Plugin + Stageable 架构设计。

**当前实现的 ISA 覆盖 = RV32IM + Zicsr（子集）**，即 RV32I 基础指令 40 条 + RV32M 扩展 8 条 + 8 个 M-mode CSR。

> ⚠️ **RV64 状态说明（2026-10-23 修正）**
>
> 本文档此前声明「支持 RV32I/RV64GC」，与实现不符 —— 当前实现**全部为 RV32 语义**。RV64 在路线图上已明确推迟到 v1.x 之后（`.rddf/roadmap/archive/phase-2-baremetal.md:5`），非实现遗漏。
>
> - **RV64 未实现**：骨架层（Payload key / 寄存器堆 / 传播层 / 内存 bundle）已参数化，但 ISA 语义层（移位掩码、有符号比较、M 扩展高半部、CSR 宽度等）仍写死 RV32，CH_MEM 后端另有 8 处 `kXlenBits = 32` 硬编码。
> - **RV64GC 的 C / A / F / D 四项扩展全缺**：无 RVC 解码、无 AMO/LR/SC、FPU 为空 stub。
> - **配置字段 `isa` 当前不参与 dispatch**：`CpuFactory` 按调用方的 `T` 决定 XLEN（默认 `uint32_t`），`isa` 字符串仅作记录。
>
> 完整缺口清单（含 file:line 证据 + RV64 准入判据 G1–G9）见 [docs/rv64_readiness_gap.md](docs/rv64_readiness_gap.md)；决策见 [ADR-087](../../docs/architecture/adr/ADR-087-isa-declaration-integrity-and-rv64-readiness.md)。

## 设计方案
本 IP 采用 **VexRiscv-on-CppTLM Stageable 方案**（详见 `VexRiscvOnCppTLM.md` (未实现)）作为最终实现规范。

核心设计原则：
- **水平功能切片**：每个 Plugin 横跨多个流水线阶段
- **PipeLink 跨阶段声明**：Plugin 间通过 PipeLink 共享数据
- **PipeBuilder DSL 编译式调度**：Plugin 在 setup 阶段声明/发现能力
- **权威设计**: 详见 [multi_isa_architecture.md](docs/multi_isa_architecture.md) v2.0

> 📖 `VexRiscvArch.md` (未实现) 为早期设计参考文档，仅供理解设计演进使用。

## 目录结构

| 目录 | 说明 |
|------|------|
| `tlm/` | CppTLM 事务级模型实现 |
| `rtl/` | CppHDL RTL 级实现（Phase 5） |
| `test/` | 验证测试套件（Level A/B/C） |
| `configs/` | JSON 配置文件和参数 Schema |
| `docs/` | 设计参考文档 |

## 接口概要

### 对外端口
- **IBus**（指令总线）：★ `ch_stream<cf::bundles::MemReq>` / `ch_stream<cf::bundles::MemResp>`
- **DBus**（数据总线）：★ `ch_stream<cf::bundles::MemReq>` / `ch_stream<cf::bundles::MemResp>`
  ★ **命名更正（ADR-092 §D7）**：旧文档写的 `MemReqBundle` / `MemRespBundle` **在代码中不存在**（实际是 `cf::bundles::MemReq` / `MemResp`）⇒ ★ 新核（`ip/cpu/core`）的对外接口 bundle 真名 = **`CpuBusReq` / `CpuBusResp`**（ADR-091 §D3a.3）
- **IRQ**（中断输入）：外部中断信号接口

### ImplMode 支持
- TLM_ONLY：纯事务级 ISS 高速仿真
- RTL_ONLY：纯 RTL 精确仿真（Phase 5）
- COMPARE：TLM/RTL 对比验证模式
- SHADOW：RTL 主导 + TLM 监控模式

> **注意**：ImplMode 在顶层 SoC 配置（如 `soc/riscv_virt.json`）中指定，不在此 IP 级配置中。IP 配置仅定义功能参数。

## 可配置参数
详见 [configs/cpu_params_schema.json](configs/cpu_params_schema.json)

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| isa | string | "rv32im" | 指令集架构（**当前不参与 dispatch**，仅记录；默认 `rv64gc` 已于 2026-10-23 修正，见 ADR-087） |
| pipeline_stages | int (3/5/7/10) | 5 | 流水线级数 (3=embedded, 5=default, 7=superscalar, 10=deep pipeline; 见 `cpu_params_schema.json` enum) |
| clock_freq_mhz | int (1-2000) | 100 | 时钟频率 (MHz) |
| enable_pmp | bool | true | 物理内存保护 |
| enable_mmu | bool | true | 虚拟内存开关 |
| mmu_mode | enum | "sv39" | sv32/sv39/sv48 |
| branch_predictor | enum | "gshare" | static/bimodal/gshare/tournament |
| btb_entries | enum | 64 | 16/32/64/128/256 |
| icache_latency_cycles | int (0-32) | 1 | ICache 命中延迟 |
| dcache_latency_cycles | int (0-32) | 1 | DCache 命中延迟 |

## CH_MEM 配置面（P2, 2026-10-27）

> ★ 上表是 **TLM 侧**（`CpuFactory` + `CPUConfig`）的参数面。CH_MEM 侧（`CpuFactoryChmem`）
> **不复用** `CPUConfig`（能力集不同 + 依赖隔离，理由见 `configs/cpu_config_chmem.h` 文件头）
> ⇒ 有独立的配置结构 `CpuConfigChmem` 与独立 JSON 解析层。

| 文件 | 职责 |
|------|------|
| `configs/cpu_config_chmem.h` | `CpuConfigChmem` struct + `validate_cpu_config_chmem()`；**零外部依赖**（不拉 nlohmann 进工厂头） |
| `configs/cpu_config_chmem_json.h` | JSON 解析 + **TLM-only 字段逐项带原因拒绝**（一次报告全部问题，非首错即返） |
| `configs/cpu_chmem_default.json` | 可直接使用的默认配置示例 |
| `configs/cpu_params_schema.json` | 每个 params 字段带 `x-chmem-supported` / `x-chmem-note`；顶层 `x-chmem-extra-fields` 登记 CH_MEM 独有旋钮 |

**CH_MEM 实际可配的 8 个旋钮**：`name` / `initial_pc` / `enable_bp` / `btb_entries` /
`bp_uncond` / `enable_mmu`（plumbing-only）/ `mmu_mode`（仅 log）/ `enable_cache`（`true` ⇒ fail-fast throw）。

★ **为什么是 fail-fast 拒绝而不是静默忽略**：`cpu-pipeline-tick-rca` 实测过 `load_config()`
静默丢弃 8 个字段 ⇒ 「配置看起来生效、实际没生效」的排查成本极高 ⇒ CH_MEM 侧改为
**不支持的字段一律 ERROR + 给出原因**。

**用法**（两个 runner + 门禁都支持）：

```bash
# 单跑
./build/bin/cpu_verilator_sim --elf <elf> --config ip/cpu/configs/cpu_chmem_default.json
./build/bin/parity_sim_runner --elf <elf> --config ip/cpu/configs/cpu_chmem_default.json

# 门禁：两侧 MUST 用同一份配置（否则比的是两种配置，不是两个后端）
python3 tools/parity/parity_gate.py --layer pr --cpu-config ip/cpu/configs/cpu_chmem_default.json
```

★ `--config` 与配置类 flag（`--enable-mmu` / `--no-bp` / `--btb-entries` …）**互斥**
（同时给 ⇒ rc=2）—— 静默覆盖 = 假绿通道。

## RiscV MMU Integration (mmu-cache-integration v0.2.0)

当 `enable_mmu=true` 时, `CpuFactory::build_cpu()` 条件注册 `cf::cpu::plugins::RiscvMMUPlugin` (继承 `cf::ip::mmu::MMUPlugin`)。Plugin 在 `setup()` 中声明 3 个 RiscV hook substage:

- `csr_write_satp` 挂 `execute` — CPU 写 `cpu_keys::SAT` payload key 后, 闭包读取并调 `csr_write_satp()` hook (satp CSR 写拦截)
- `sfence_vma` 挂 `execute` — 读 `SFENCE_VADDR`/`SFENCE_ASID` 后调 `sfence_vma()` (RISC-V Spec §6.2 4-way dispatch)
- `mmu_exit` 挂 `memory` — 读 `mmu_keys::EXCEPTION_CODE` 并写到 `cpu_keys::CPU_EXCEPTION_CODE`

**Payload Key 契约**: `ip/cpu/tlm/cpu_keys.h` (CPU → MMU IPC) + `ip/mmu/tlm/mmu_keys.h` 已有 Key (MMU → CPU)。Key identity 是全局 static 指针身份, IP 间通过 `pb.at_stage` 闭包共享 PayloadStore。

**baseline 行为**: `enable_mmu=false` 时不注册 MMUPlugin, byte-identical baseline (5-stage 5 nodes, stage_count 18 含 11 baseline plugins + 5 topology + 2 lane dispatch)。

**集成测试**: `tests/cpu/integration/test_{3,5,7,10}stage_riscv.cpp` 各加 `EnableMMU*` 测试 + `tests/cpu/test_cpu_riscv_mmu_hooks.cpp` 3 个 RiscV hook 集成测试。

## 快速开始

```bash
# 编译 TLM 版本
mkdir build && cd build
cmake .. -DCPU_IMPL_MODE=TLM_ONLY
make

# 运行最小测试
./test/cpu_unit_test
```

## 相关文档
- [项目架构总览](../../docs/architecture/overview.md)
- [接口设计详解](../../docs/architecture/interface-design.md)
- [测试与 DSE 框架](../../docs/architecture/testing-and-dse.md)

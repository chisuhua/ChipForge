# Phase 6d Verilator Sim — 6d.5 E8 + 6d.8 Harness 迁移

## Why

Phase 6d 6d.5 E8（Verilator sim 跑 vendored 5 ELF tohost=1 一致）与 6d.8（Harness `pb.run()` → CppHDL sim runner / Verilator）是 Oracle 2026-09-21 路径修正中**显式拆为独立 change** 的工作（audit session_id=ses_f405b4b2dffedOAwvAlKeXWW1p）。

**上游依赖**: CppHDL VerilatorBackend Phase 3.2-3.6（sibling repo ADR-035：`verilator_backend.h` 当前是 scaffolding, `dlopen_top` / `build_port_access_table` / `sync_inputs_to_vtop` 都未实装）。本 change 在 CppHDL 实装完成前**不可启动**，但需要先于 `phase-6d-fsm-chmem`（Oracle 6d.6 task 6.6: ch_state_machine 必须 Verilator sim 验证 cycle-accurate, 6d.6/6d.7 传递依赖 E8）。

**E8 降级路径**（Oracle 2026-09-20）: 原"trace byte-equal"基线基础设施不存在（CppHDL Simulator 无 trace dump 对比工具），接受 **`tohost=1` + cycle 数 ±10% 一致** 作为通过条件。

## What Changes

### 6d.5 E8 — Verilator sim 跑 vendored ELF

- **新增** `tools/verilator_runner/main.cpp`：dlopen Vtop__ALL.so，构造 Vtop 实例，连接 ELF loader，驱动 default_clock / default_reset，tick N cycle，读 DMem[0x1000]（tohost offset）验证。
- **新增** `tests/cpu/test_cpu_verilator_sim.cpp`：复用 `tests/cpu/test_cpu_chmem_vendored_elf.cpp` 的 ELF 加载逻辑，但 sim 后端走 Verilator runner。
- **验证**: 5 ELF (rv32ui-p-{add,addi,auipc,beq,jal}) 在 Verilator sim 下 tohost=1 + cycle 数与 CppHDL sim ±10%。

### 6d.8 — Harness 迁移

- **修改** `tools/cpu_sim/main.cpp`：从 `pb.run()` 迁到 `pb.elaborate()` + `ch::Simulator::tick()` 或 Verilator。
- **保留 CLI 兼容性**：`--elf`, `--mode chmem` flag。
- **修改** `[cpu-integration]` 测试 (`test_*stage_riscv.cpp`)：同步迁移到新 harness。
- **验证**: `./cpu_sim --elf add.elf` 跑 CH_MEM sim 行为与 TLM 一致（tohost=1）。
- **排除** 7stage superscalar config（预存 segfault，与本工作无关）。

## Upstream Dependency (BLOCKING)

**CppHDL VerilatorBackend Phase 3.2-3.6**（sibling repo `CppHDL/`）:

| Phase | 内容 | 估时 |
|-------|------|------|
| 3.2 | `invoke_verilator(verilog_path)` 实装：跑 `verilator --cc --build` + dlopen Vtop__ALL.so | 1w |
| 3.3 | `build_port_access_table()` 实装：VPI lookups 或 generated accessors 解析 ch_in/ch_out port → &vtop->field | 1w |
| 3.4 | `clock_node_id_` discovery + 3-eval/tick model (comb-1, clock, comb-2) 映射到 Verilator eval() | 0.5w |
| 3.5 | `cache_path_for_key()` SHA-1 cache key + `~/.cache/cpphdl/verilator/<hash>/Vtop` 增量编译 | 0.5w |
| 3.6 | VCD trace (可选) | 0.5w |

**总估时**: 3-4w CppHDL 实装（不在本 change scope）。本 change 在 CppHDL 完成 Phase 3.2-3.3 后启动。

## Acceptance

- [ ] `tools/verilator_runner/main.cpp` 实装完成
- [ ] `tests/cpu/test_cpu_verilator_sim.cpp` 5 ELF tohost=1 PASS（cycle 数 ±10% 与 CppHDL sim 一致）
- [ ] `tools/cpu_sim/main.cpp` Harness 迁移完成
- [ ] `[cpu-integration]` 测试同步迁移，0 回归
- [ ] `check_plugin_portability.sh` 仍 8/8 PASS（9/9 等 `phase-6d-fsm-chmem`）
- [ ] `verify_adr.sh` 0 FAILED

## Capabilities

无新增 capability（纯 sim backend 切换 + harness 重构，不改 public API 行为）。`pb.elaborate()` 已在 v0.3.0 (`plugin-elaboration-substrate`) 落地。

## Impact

- `tools/cpu_sim/` 用户可见 CLI：不变（`--mode chmem` flag 默认开）
- `tests/cpu/integration/test_*stage_riscv.cpp` 测试代码：同步迁移（约 4 文件）
- `CppHDL/src/core/verilator_backend.cpp` Phase 3.2-3.6 实装（**不在本 change**, sibling repo ADR-035）
- `CHANGELOG.md` v0.5.0 (Phase 6d 完整收官)

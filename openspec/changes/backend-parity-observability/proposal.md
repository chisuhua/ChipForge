---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: in-progress
created_from: docs/architecture/parity-gate-contract.md
adr:
  - ADR-089
  - ADR-088
depends_on:
  - parity-gate-spec
---

# backend-parity-observability

> **阶段 A2a（可观测性）**：让 5 个观测量在**两侧都能被采到、且采的方式一致**。
> ★ 契约锚：`docs/architecture/parity-gate-contract.md` **S-2 / S-3 / S-4 / S-5 / S-7 / S-8 / S-14**（本 change 每条 task 都引 S 编号，见 tasks.md）。
> ★ 本 change **不含比对器**（那是 A2b）与**语料落盘**（A3）。

## Why

差分门禁的前提是「两侧各有同样的观测口」。实测现状是**两侧都不齐**：

| 侧 | 现状 | 缺口 |
|---|---|---|
| Simulator | 唯一的 ELF 入口是 Catch2 里**硬编码 47 项表**（`tests/cpu/test_cpu_chmem_vendored_elf.cpp`），无 CLI；且**每拍**调 `get_value_by_name`（O(nodes)） | 无通用 runner；无 regs/pchash 采集；无 CLI 对齐 |
| Verilator | 有 CLI（`cpu_verilator_sim`），能读 `dmem[]` / `pc` / `flush_count` / `bp_*_count` | **不导出 regfile / 逐拍 PC 流**；`--stats` 是直方图（丢顺序） |
| 两侧 | — | ③ 内存签名**两侧都没有**（Simulator 侧 `get_memory()` 是空 stub） |

⇒ 不补齐这些，A2b 的比对器无从谈起。

## What Changes

| 增量 | 内容 | 状态 |
|---|---|---|
| **inc-1** | 新增 `tools/parity_runner/parity_sim_runner.cpp`（Simulator 侧通用 ELF CLI，CLI 面镜像 `cpu_verilator_sim`）+ 4 观测量采集（`tohost` / `regs` / `cycles` / `pchash`）+ 确定性导出（`node_count` / `names_hash`）+ JSON 报告片段 | ✅ **已落** |
| **inc-2** | Verilator 侧同样导出 `regs` / `pchash`（改 `sim_main.cpp` + `--json`），并加 `--reset-pulse=0`（默认不变） | ⏳ |
| **inc-3** | ③ `memsig`：DUT 侧只读探针（`mem_sig` / `mem_sig_updates`）+ 两侧读取 + 自校验 | ⏳ |

## Acceptance

- [x] Simulator 侧 CLI 可跑单个 ELF 并输出 5 项中的 4 项（`memsig` 显式 pending）
- [x] 同一 ELF 两侧 `cycles` 一致（`rv32ui-p-add`：**478 / 478**）
- [ ] 同一 ELF 两侧 `regs` / `pchash` 一致（待 inc-2）
- [ ] `memsig` 两侧可采且有自校验（待 inc-3）
- [ ] 缺任一观测量 ⇒ 退出码 2（已实现 `memsig` 的 fail-closed；其余同构）
- [ ] 抽样 ≥3 个 ELF（含 1 个 rv32um）产出**两侧 JSON 证据**

## Impact

| 面 | 影响 |
|---|---|
| 代码 | 新增 1 个 tool（≈420 行）+ CMake 目标；inc-2/inc-3 再改 `tools/verilator_runner` 与 `ip/cpu/plugins/dmem_chmem.h` |
| 测试 | 现有 47/48 ELF 基线**不变**（新 tool 独立 target，不改既有 harness 行为） |
| 成本 | ★ **实测吞吐 9.1 拍/s**（`rv32ui-p-add` 478 拍 / 52.8 s）⇒ 47 ELF 全量 ≈ 40 min 起，A3/A4 必须分层 |

---
change: backend-parity-observability
priority: P1
initiative: wave5-isa-coverage-and-bp
adr:
  - ADR-089
  - ADR-088
plan: docs/architecture/parity-gate-contract.md
status: in-progress
depends_on:
  - parity-gate-spec
---

# tasks — backend-parity-observability（阶段 A2a）

> 每条任务末尾 `[S-x]` = 它落地的契约单元（`docs/architecture/parity-gate-contract.md`）。
> ★ 本 change 分 3 个增量；**inc-1 已落**，inc-2/inc-3 待做。

## inc-1 Simulator 侧通用 runner（✅ 已落）

- [x] 1.1 新建 `tools/parity_runner/parity_sim_runner.cpp` + CMake target（`CF_PLUGIN_USE_CH_MEM`，输出到 `build/bin`）[S-3]
- [x] 1.2 CLI 面镜像 `cpu_verilator_sim`：`--elf/--cycles/--reset/--enable-mmu/--mmu-mode/--enable-cache/--no-bp/--btb-entries/--no-bp-uncond/--no-bp-gshare/--json/--verbose` [S-3, S-7]
- [x] 1.3 `--reset` 只接受 `none`，其它值 **fail-fast**（不静默忽略）[S-3]
- [x] 1.4 一次性解 node id（`ctx.get_nodes()` 扫名字）+ 每拍 `data_map()` 直读；**禁** `get_value_by_name` / `get_signal_id_by_name` [S-4, S-5, S-8]
- [x] 1.5 采集 ① `tohost`（节点 `tohost_probe_data_proxy`）[S-4]
- [x] 1.6 采集 ② `regs`（`reg_1..reg_31`，x0 不采，32-bit LE 升序 FNV-1a）[S-5]
- [x] 1.7 采集 ④ `cycles`（与 S-7 伪码同构的循环）[S-7]
- [x] 1.8 采集 ⑤ `pchash`（逐拍 `pc` + FNV-1a，记录 `pc_samples`）[S-8]
- [x] 1.9 确定性导出：`node_count` + 排序后信号名 `names_hash` [S-2]
- [x] 1.10 ③ `memsig` 未实现 ⇒ **默认 fail-closed（exit 2）**，`--allow-incomplete` 才放行并标 `null` [S-1, S-14]
- [x] 1.11 报告片段 JSON（`backend` / `determinism` / `observations` / `done` / `timeout` / `timings`）[S-14]
- [x] 1.12 stdout 卫生：build/run 期间 fd1 → fd2，输出前恢复（CppHDL CHINFO 无级别阈值）[S-14]
- [x] 1.13 **实测对齐证据**：`rv32ui-p-add` 两侧 `cycles` **478 / 478**、`tohost` 1 / 1 [S-7, O-1]
- [x] 1.14 ★ **吞吐实测入契约**（≈ 9.1 拍/s，推翻 ADR 的 575 拍/s）⇒ 契约 S2-13/S2-14 + S-15 cap 重定 + ADR-089 v1.3 [S-15]

## inc-2 Verilator 侧观测量导出（⏳）

- [ ] 2.1 实测 `reg_1..reg_31` 在生成 Verilog 里的真实信号名，回写契约 §2.2（O-2）[S-5]
- [ ] 2.2 `sim_main.cpp` 增加 `cf_read_reg()` + 逐拍 PC 滚动 hash（`--stats` 直方图保留为诊断，**不得**当 ⑤）[S-5, S-8]
- [ ] 2.3 `cpu_verilator_sim` 增加 `--json <path>`，字段与 Simulator 侧**同名同义** [S-1, S-14]
- [ ] 2.4 新增 `--reset-pulse=0`（默认 1，**既有 48/48 行为不变**）[S-3]
- [ ] 2.5 同 ELF 双侧 `regs` / `pchash` 相等（或登记差异 → 交 A3 分类）[S-5, S-8]
- [ ] 2.6 缺观测量 ⇒ exit 2（与 Simulator 侧同构）[S-10]

## inc-3 `memsig` DUT 探针（⏳）

- [ ] 3.1 `dmem_chmem.h` 内新增只读累加（复用已存在的 `word_addr` / `store_wdata` / `is_store_active`），导出 named reg `mem_sig` / `mem_sig_updates` [S-6]
- [ ] 3.2 自校验：写发生过但 `mem_sig_updates == 0` ⇒ exit 2（防「签名恒 0 全绿」）[S-6]
- [ ] 3.3 变异对照：改 1 字节 store 数据 ⇒ `memsig` MUST 变；删除写 ⇒ `mem_sig_updates` MUST 变 [S-6]
- [ ] 3.4 ★ **零 cycle 影响验证**：加探针前后同 ELF `cycles` 不变（否则说明探针进了关键路径） [S-6]
- [ ] 3.5 两侧 `memsig` 相等 [S-6]

## 验收（G-A2 的输入）

- [ ] 4.1 ≥3 个 ELF（含 1 个 rv32um）两侧 JSON 证据落 `build/parity/`，5 观测量齐备
- [ ] 4.2 `git diff --stat` 的代码改动**仅**限于 `tools/parity_runner/**` / `tools/verilator_runner/**` / `ip/cpu/plugins/dmem_chmem.h` + CMake
- [ ] 4.3 既有基线零退化：`[verilator]` 1/1、`[chmem]` ELF 47/47（抽样）、`[cpu]` 抽样

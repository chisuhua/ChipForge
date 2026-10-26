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

- [x] 2.1 实测 `reg_1..reg_31` 在生成 Verilog 里的真实信号名 = **`top__DOT___reg_N`**，已回写契约 §2.2（O-2 闭合）[S-5]
- [x] 2.2 `sim_main.cpp` 新增 `cf_run_sim_parity()`：`read_reg()`（x1..x31）+ 逐拍 PC 滚动 hash（`--stats` 直方图保留为诊断，**不得**当 ⑤）[S-5, S-8]
- [x] 2.3 `cpu_verilator_sim` 新增 `--json <path>`，字段与 Simulator 侧**同名同义**（含 `determinism` / `pc_head` / `regs_raw`）[S-1, S-14]
- [x] 2.4 新增 `--reset-pulse <0|1>`（默认 1，**既有 48/48 行为不变**）+ ★ **实测发现 `0` 会丢 init**（Verilog 只在复位分支加载 init）⇒ 已加 stderr 警告「NOT parity-compatible」[S-3]
- [x] 2.5 同 ELF 双侧 `regs` / `pchash` 相等 —— **3/3 ELF 逐位相等**（`rv32ui-p-add` / `rv32um-p-mul` / `rv32ui-p-simple`）[S-5, S-8]
- [x] 2.7 ★★ **初始状态语义定案**（实测推翻原 `reset_mode=none` 方案）：生成 Verilog **只在复位边沿**加载 ctor init（`else pc <= 32'h80000000;`）⇒ 跳过脉冲则 `pc` 从 0 起（`x30=0x1000` 而非 `0x80001000`）；而 `Simulator::reset()` 只是**清零**。⇒ 正确口径 `init_semantics = ctor-init`（Sim 不 reset / VL 打 1 拍 reset），契约 S-3 + §2.3 + C-2 已回写 [S-3]
- [x] 2.6 缺观测量 ⇒ exit 2（Verilator 侧信号缺失 = **编译期失败**（引用不存在的 `top__DOT__mem_sig` 直接编译不过）⇒ 不会静默读 0）；★ 并统一 **TIMEOUT ⇒ exit 2**（此前 Verilator 侧返回 1，与 Simulator 侧不一致）[S-10]

## inc-3 `memsig` DUT 探针（⏳）

- [x] 3.1 `dmem_chmem.h` 内新增只读累加（复用已存在的 `word_addr` / `store_wdata` / `is_store_active`），导出 named reg `mem_sig` / `mem_sig_updates`（★ 显式取值写法 `ch_uint<32> v(reg)` —— 直接 `reg * X` 会走编译期值路径导致 reg 被 codegen 裁掉）[S-6]
- [x] 3.2 自校验：写发生过但 `mem_sig_updates == 0` ⇒ exit 2（防「签名恒 0 全绿」）—— ★ **两侧都实现**（Simulator runner + Verilator parity 入口）[S-6]
- [x] 3.3 变异对照：① `mix` 里的 `store_wdata` 取反 ⇒ `memsig` MUST 变（实测 `a55f52c5`→`8fcc53ba`）② 强制 `sig_upd` 恒 0 ⇒ 两侧 MUST 判「假绿」ERROR（实测 rc=2 + 消息一致）[S-6]
- [x] 3.4 ★ **零 cycle 影响验证**：`rv32ui-p-add` 478 / `rv32um-p-mul` 472 / `rv32ui-p-simple` 38 加探针前后**不变**（探针只观测、不进 datapath）[S-6]
- [x] 3.5 两侧 `memsig` 相等 —— **5/5 corpus**（3 ELF + branchbench m0/m2）`memsig`/`memsig_updates` 逐位相等 [S-6]

## 验收（G-A2 的输入）

- [x] 4.1 ≥3 个 ELF（含 1 个 rv32um）两侧 JSON 证据 —— **已完成且扩到 5 个 corpus**（3 ELF + branchbench m0/m2），**5 项观测量 + 确定性判据全齐且逐位相等**
- [x] 4.2 `git diff --stat` 的代码改动**仅**限于 `tools/parity_runner/**` / `tools/verilator_runner/**` / `ip/cpu/plugins/dmem_chmem.h` + CMake
- [ ] 4.3 既有基线零退化：`[verilator]` 1/1、`[chmem]` ELF 47/47（抽样）、`[cpu]` 抽样

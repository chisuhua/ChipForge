---
change: parity-gate-spec
priority: P1
initiative: wave5-isa-coverage-and-bp
adr:
  - ADR-089
  - ADR-088
plan: docs/architecture/parity-gate-contract.md
status: in-progress
depends_on: []
---

# tasks — parity-gate-spec（阶段 A1 · 零代码契约）

> ★ **本 change 零代码**：所有 task 只产出文档 / 契约。MUST NOT 触碰 `ip/**` `src/**` `tests/**` `tools/**`（`proposal.md` Acceptance 最后一条可机检）。
> ★ 编号约定：每个 task 末尾的 `[S-x]` 指向它**落地的契约单元**（S-16 的粘住机制从 A1 自身开始示范）。

## 1. 事实基线（先核实，再落笔）

- [x] 1.1 核实 Simulator 侧采集 API 的真实语义：`get_value_by_name`（O(nodes)）/ `get_signal_id_by_name`（仅 trace 注册表）/ `data_map()` / `sdata_type → uint64` / `get_memory()`（空 stub）[S-5, S-6, S-8]
  - 证据：`CppHDL/src/simulator.cpp:455-470`、`CppHDL/include/simulator.h:9,477,557-568,581-582`、`CppHDL/include/core/types.h:96`
  - ★ 结论与 `ADR-089` v1.1 的 D2 ② **不符** ⇒ 触发 1.6 的 ADR 事实更正
- [x] 1.2 核实 Verilator 侧 host 读通道与时钟相位：`--public-flat-rw` / `top__DOT__dmem` / `top__DOT___pc` / `clk=1;eval;clk=0;eval` [S-3, S-4, S-8]
  - 证据：`tools/verilator_runner/sim_main.cpp:75-110,117-141,190-222`
- [x] 1.3 核实 **reset 处理两侧不同**（Simulator 不打 reset / Verilator 打 1 拍 pulse）⇒ 提出 `reset_mode = none` [S-3]
  - 证据：`tests/cpu/test_cpu_chmem_vendored_elf.cpp:157-161`、`tools/verilator_runner/sim_main.cpp:117-127`
- [x] 1.4 核实 corpus 两侧条数（47 / 48）与「`rv32ui-p-simple` 只在 Verilator 侧」 [S-13]
- [x] 1.5 核实 `PC_HIST` 是直方图（丢顺序）⇒ ⑤ 不能复用它 [S-8]
  - 证据：`tools/verilator_runner/sim_main.cpp:190-222`（`hist[]` + `dropped`）
- [x] 1.6 `ADR-089` v1.1 → **v1.2**：D2 ② 采集 API 事实更正 + Consequences 编号修正（1,2,4,4,5,6,7,4 → 1…8）[S-5]

## 2. 契约正文

- [x] 2.1 落 `docs/architecture/parity-gate-contract.md`：观测量命名与语义冻结 [S-1]
- [x] 2.2 落**确定性前置校验**（node_count + 信号名单 hash；先于任何比对）+ 分类归 `ERROR`（开放项 C-1）[S-2]
- [x] 2.3 落**采样相位与对齐**（offset 固定 0；禁自适应；`reset_mode = none`）[S-3]
- [x] 2.4 落 ① `tohost`（节点名 / 信号名 / done-pass 判定 / 缺失即 ERROR）[S-4]
- [x] 2.5 落 ② `regs`（x1..x31，x0 不采；sha256 over 124B LE）[S-5]
- [x] 2.6 落 ③ `memsig`（DUT 自算探针 + `mem_sig_updates` 自校验，防"签名恒 0 全绿"）[S-6]
- [x] 2.7 落 ④ `cycles`（含两侧同构伪码；MUST NOT 用 ④+末态替代 ⑤）[S-7]
- [x] 2.8 落 ⑤ `pchash`（逐拍有序 FNV-1a + 首分歧拍定位；禁直方图）[S-8]
- [x] 2.9 落**判定规则与禁降级**（判据强度排序；不可采 = ERROR）[S-9]
- [x] 2.10 落**三态退出码**（0/1/2）+ `error_kind` 枚举 [S-10]
- [x] 2.11 落**例外白名单格式**（8 字段 + 未知字段拒收 + 禁容差字段 + 常驻合成例外）[S-11]
- [x] 2.12 落**扩张权与规模护栏**（architect 审 / commit 引 ADR-089 / >10 提示复审 / 过期自动失效）[S-12]
- [x] 2.13 落**语料清单单一来源**（`tests/cpu/parity/corpus.txt` 格式 + 三方集合断言）[S-13]
- [x] 2.14 落**报告格式**（`build/parity/report.json` 字段表 + 缺观测量即 ERROR）[S-14]
- [x] 2.15 落**运行时预算与分层**（PR ≤8 ELF / 夜间全量 / Verilog 构建按 config 缓存 / 不设未实测阈值）[S-15]
- [x] 2.16 落**契约粘住机制**（S 编号 + 下游 tasks 必引 + G-A2 覆盖矩阵模板）[S-16]
- [x] 2.17 落**开放项表**（C-1/C-2 待 architect；O-1~O-4 待实测）—— 显式化未知，不留隐性假设 [S-2, S-3, S-5, S-6, S-15]

## 3. 立项与登记

- [x] 3.1 `openspec/changes/parity-gate-spec/` 三件套（proposal / design / tasks）+ specs delta
- [x] 3.2 roadmap `feat-backend-parity-gate` 的 A1 checkbox 勾选 + 指向契约路径
- [x] 3.3 `.rddf/improvements/backend-parity-gate-spec.md` 状态更新（A1 已落，指向契约）

## 4. 验收（本 change 的 G-A1 门禁）

- [x] 4.1 `python3 tools/check_md_tables.py`（契约 + ADR-089 + change 三件套）全 OK
- [x] 4.2 `bash tools/doc_link_check.sh` 0 broken
- [x] 4.3 `bash tools/verify_adr.sh` PASS（ADR-089 v1.2 一致）
- [x] 4.4 `openspec change validate parity-gate-spec` valid
- [x] 4.5 **零代码决议**：`git diff --stat` 不含 `ip/**` `src/**` `tests/**` `tools/**`
- [x] 4.6 architect 签：★ **C-1 = ERROR(2)**（Oracle 建议 + 用户采纳 ⇒ ADR-089 v1.3）· ★ **C-2 = `reset_mode=none`**（用户裁决）⇒ 两项闭合（2026-10-26）

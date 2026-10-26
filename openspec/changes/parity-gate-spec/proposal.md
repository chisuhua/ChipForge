---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: ready
created_from: .rddf/improvements/backend-parity-gate-spec.md
adr:
  - ADR-089
  - ADR-088
depends_on: []
---

# parity-gate-spec

> **阶段 A1（零代码契约）**：把 `ADR-089` 的 4 组决策落成**可执行的实现契约**（S-1…S-16），使后续 A2a/A2b/A3/A4 的每条任务都能被追到一条契约条款。
> ★ **本 change 零代码**：`git diff --stat` MUST NOT 触及 `ip/**` `src/**` `tests/**` `tools/**`（堵「先写工具后定口径」的后门）。

## Why

`ADR-089` v1.2 定了「parity = 同一输入的确定性双后端差分执行」这一决策，但**决策 ≠ 可实现契约**：

1. 5 个观测量只说「要比」，没说**从哪个符号读**、**在两侧分别怎么读**、**读不到怎么办**。实测已证三处与直觉不符：
   - Simulator 侧 `get_memory()` 是**空 stub**（`simulator.cpp:161-168`，且被未启用的 `ENABLE_MEMORY_API` 包住）⇒ ③ 不可能是 host 内存 dump；
   - `get_signal_id_by_name()` 只查 **trace 注册表**，而 trace **默认关**（`simulator.h:557` / `:9`）⇒ ADR 原文写的这条采集路径**不可用**；
   - Verilator 侧 `--stats` 的 `PC_HIST` 是**直方图（丢顺序）**（`sim_main.cpp:190-222`）⇒ 不能当 ⑤。
2. 「例外白名单」只说要有 5 个字段，没说**文件格式**、**作用域匹配规则**、**过期怎么失效**、**怎么防止偷偷加容差**。
3. 「corpus 两侧交集 47」是事实判断，但**两侧各维护一份手写 C++ 数组**（`test_cpu_chmem_vendored_elf.cpp` 47 / `test_cpu_verilator_sim.cpp` 48）⇒ 不加约束必然再漂。

⇒ ★ 若不定契约就开工 A2，实现者会**自选口径**（并很可能选最省事的那个），最后得到一道「两边都过就绿」的假门禁 —— 这正是 `chmem-spec-rewrite-and-doc-hygiene` 上已经吃过一次的亏。

## What Changes

- 新增 `docs/architecture/parity-gate-contract.md`（**S-1…S-16**，实现契约本体）
- 新增本 change 的 `design.md`（契约推导：代码事实基线 / 相位差结论 / 落法选择）+ `tasks.md`（A1 执行与验收）
- 新增 `specs/parity-gate-spec/spec.md`（5 条 Requirement 的可机检 Scenario）
- `ADR-089` v1.1 → **v1.2**：D2 ② 采集 API **事实更正**（`get_signal_id_by_name` → 一次性 `ctx()->get_nodes()` 解 id）+ Consequences 编号修正

★ 不改任何代码、不新增 corpus、不落白名单文件（那些是 A2a/A2b/A3 的交付物）。

## Acceptance

- [ ] `docs/architecture/parity-gate-contract.md` 存在，含 **S-1…S-16**，每条有「要求 / 验收方式 / 归属」
- [ ] 契约里的**每条代码事实**都带 `path:line`，且可复核（不含量化猜测）
- [ ] **零代码**：`git diff --stat` 不触及 `ip/**` `src/**` `tests/**` `tools/**`
- [ ] §2.3 明确写出**两侧 reset 处理不同**这一真实分歧源 + `reset_mode = none` 的统一要求
- [ ] §7 覆盖矩阵模板 + S-16 粘住机制（下游 tasks 必引 S 编号）成文
- [ ] `ADR-089` D2 ② 的事实错误已更正，`verify_adr.sh` / `check_md_tables.py` 全绿
- [ ] 开放项 §8（C-1 / C-2 待 architect；O-1~O-4 待 A2a 实测）**逐条列出**，不留隐性假设

## Impact

| 面 | 影响 |
|---|---|
| 代码 | **零**（A1） |
| 文档 | 新增 1 份契约（约 16 条 S）+ 本 change 3 份；ADR-089 小版本修订 |
| 下游 | A2a/A2b/A3/A4 的 `tasks.md` MUST 逐条引 S 编号（S-16）⇒ `G-A2` 交覆盖矩阵 |
| 风险 | 契约可能仍不完备（§8 开放项）⇒ 用「显式开放项」而非「隐性假设」兜底；O-1（相位 offset）若实测 ≠ 0 需回改契约 |

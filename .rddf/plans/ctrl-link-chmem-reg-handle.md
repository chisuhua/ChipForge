---
id: ctrl-link-chmem-reg-handle
linked_change: openspec/changes/ctrl-link-chmem-reg-handle/
linked_objective: .rddf/roadmap/objectives/objective-soc-cpu-v0110-launch.md
linked_feature: .rddf/roadmap/features/feat-mfc-multi-cycle-fsm.md
linked_improvement: .rddf/improvements/ctrl-link-chmem-reg-handle.md
tier: complex
estimated_steps: 6
estimated_total_weeks: "1 day (纯框架层, 无业务逻辑改动)"
verifier: rdd-verifier
sprint_window:
  sprint-2-early: "10-23 ~ 10-30 (Sprint 2 早期 — 同时是 Phase B 的 CppHDL API 热身 session)"
deps_status:
  blocked_by: []
  manual_deps: []
---

# Plan: ctrl-link-chmem-reg-handle

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan phase-by-phase. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 `CtrlLink` 在 CH_MEM 模式下能表达"由寄存器支撑、跨拍保持"的 halt 条件（`ch_reg<ch_bool>`），且该条件进入 OR 合并网络后**随拍更新**；同时把 `should_*()` 的适用范围显式契约化，消除"看似生效实则读到陈旧值"的静默陷阱。

**Architecture:** 单一 source-of-truth 框架层 —— `include/cf/plugin/ctrl_link.h`（A1 显式入口 + A2 唯一 OR 实现 + A3 契约）+ `include/cf/plugin/pipe_builder.h`（A5 deprecated 算子清理）；业务侧**零逻辑改动**（`ip/cpu/plugins/hazard_chmem.h` 仅注释更新）。

**Tech Stack:** C++23、CppHDL `ch_reg` / `ch_bool`（ADR-040 v2.0）、Catch2 v3.7.0、`chipforge_tests_chmem`（CH_MEM 双模 binary）。

---

## Phase 0 — 前置 baseline（0.3d）

- [x] **0.1** [BASELINE] 记录当前 HEAD baseline：`bash tools/run_chipforge_tests.sh --all`
  - **实测（2026-10-22，HEAD `f53fd5a`）**：`verify_plugin_decision` PASS (1.23s) / `chipforge_tests` **FAIL** (46.14s，唯一失败 = 已知 infra drift `test_7stage_riscv.cpp:124` `node_count()==17` 实测 9) / `chipforge_tests_chmem` PASS (578.20s) / `cpu_verilator_sim_add` PASS (8.06s) / 合计 633.68s
- [x] **0.2** [BASELINE] 记录 `[chmem]` 73 case + `[framework]` 逐 case cycle 计数
  - **TLM 实测**：`chipforge_tests` = 481 cases / 480 passed / 1 failed (上述 infra drift) / 67135 assertions
- [x] **0.3** [BASELINE] 记录构建期 deprecation warning 清单
  - ⚠️ **统计方法**：**不能用 grep 数 `||`**（同行不含 `ch_bool` 字面量的用法会漏 —— 例如 `pipe_builder.h:434` 的 `stall = stall || cl->halt_condition()`）。必须用编译器：`-fsyntax-only -Wall -Wextra` + grep `use operator| for 1-bit OR`。**实测框架内 4 处**：`ctrl_link.h:150,160` + `pipe_builder.h:434,435`

## Phase 1 — A2/A6 OR 合并唯一实现

- [x] **1.1** [RED] 写 `tests/framework/test_ctrl_link_stateful.cpp`（case A/B/C：空集合 / 单条件不退化 / 组合+有状态共存）
- [x] **1.2** [GREEN] `ctrl_link.h` 实现 `OR_combined_halt_conds()`（空 → 常量假值；单 → 直接返回；多 → `operator|` 建真 OR 门 DAG）
- [x] **1.3** [GREEN] **A6**：`halt_condition()` / `flush_condition()` 改 inline alias 转发
- [x] **1.4** [REFACTOR] 清理 `:96` "承诺但缺失"的注释
- [x] **1.5** [VERIFY] `[framework][ctrllink-stateful]` PASS

## Phase 2 — A5 deprecated 算子清理（框架内 4 → 0）

- [x] **2.1** `ctrl_link.h:150` `operator||` → `operator|`
- [x] **2.2** `ctrl_link.h:160` `operator||` → `operator|`
- [x] **2.3** `pipe_builder.h:434` `operator||` → `operator|`
- [x] **2.4** `pipe_builder.h:435` `operator||` → `operator|`
- [x] **2.5** [VERIFY] 框架内零 deprecation warning（probe TU 验证 = 0）
- [x] **2.6** [VERIFY] OR 门 DAG 逻辑等价，零回归

## Phase 3 — A1 有状态条件显式入口

- [x] **3.1** [RED] case D：注册 `ch_reg<ch_bool>`，**Simulator tick ≥ 2 拍**观测 OR 输出翻转
- [x] **3.2** [GREEN] `halt_when_stateful(const ch_reg<ch_bool>&)` + `halt_when(const ch_reg<ch_bool>&)` 重载
- [x] **3.3** [GREEN] `pipe_builder.h:307` / `:202` 两处 `should_*()` 消费点按新契约确认语义
- [x] **3.4** [VERIFY] `[framework][ctrllink-stateful]` 全 PASS

## Phase 4 — A3 契约显式化

- [x] **4.1** [RED] case E：`should_*()` 对有状态条件的契约测试（断言 `unreliable_query_count()` 递增）
- [x] **4.2** [GREEN] `ctrl_link.h:98-124` 改写 `should_*()` 文档契约 + 实现可观测告警（计数器 + 一次性 stderr 告警）
- [x] **4.3** [REFACTOR] `pipe_builder.h` elaborate 期 OR 门引用寄存器节点的防回归注释
- [x] **4.4** [VERIFY] 组合条件 `should_*()` 语义不变（`test_ctrl_link.cpp` 11+ case 零回归）

## Phase 5 — 测试接入 + 注释更新

- [x] **5.1** `tests/CMakeLists.txt` 新文件加入 `CHMEM_TEST_SOURCES`（`:118-145` 手工 list）
- [x] **5.2** [VERIFY] `./build/bin/chipforge_tests_chmem "[ctrllink-stateful]"` 可见可跑
- [x] **5.3** `ip/cpu/plugins/hazard_chmem.h:149-173` 更新 FP-4 gap 注释，指向 C.5 新归属
- [x] **5.4** [VERIFY] `git diff --stat ip/cpu/plugins/hazard_chmem.h` ⇒ 仅注释行变更

## Phase 6 — spec / 文档 / 归档

- [ ] **6.1** `openspec/specs/ctrllink-continuous-handle/spec.md` 由 delta spec 归档生成
- [x] **6.2** `AGENTS.md` 新增 "CtrlLink 有状态 halt 条件" 段 + ADR-040 速查行更新
- [x] **6.3** `CHANGELOG.md` 新增 v0.11.0 段
- [ ] **6.4** [VERIFY] 5 架构 gate 0 失败（`verify_adr` / `verify_plugin_decision` / `check_plugin_portability` / `doc_link_check` / `roadmap-r9-check`）
- [ ] **6.5** [VERIFY] `bash tools/run_chipforge_tests.sh --all` 与 Phase 0 baseline 一致
- [ ] **6.6** `openspec change validate` 0 error + `openspec archive ctrl-link-chmem-reg-handle -y`

---

## 执行期实测知识（重要，勿再踩）

### K1 — `ch_reg::operator<<=` 必须在 Simulator 创建**之前**调用

Simulator 初始化时**快照寄存器链**；之后再 `<<=` 不生效（静默无效）。

```
❌ reg <<= ch_bool(true);  ch::Simulator sim(&ctx);   // 无效
✅ reg <<= !reg;            ch::Simulator sim(&ctx);   // 有效（自翻转）
```

自翻转写法 `reg <<= !reg;` 是"逐拍更新"的最佳测试载体：`ch_bool` 的 `operator!` 建 NOT 节点 ⇒ 寄存器每拍交替 ⇒ OR 输出必然交替 ⇒ 若 OR 被冻结在初值，测试立即 FAIL。

### K2 — `ch_reg<ch_bool>` 切片到 `ch_bool` **保留** proxy 节点

上游 `lnode/reg.tpp` 的 `ch_reg` 构造函数里 `static_cast<logic_buffer<T>*>(this)->node_impl_ = proxy_node;` ⇒ 基类 `logic_buffer<ch_bool>` 持有的是 **proxy 节点**（而非 regimpl 本身）；`logic_buffer` 拷贝构造复制 `node_impl_`（`lnode/logic_buffer.tpp:17`）⇒ 切片后 OR 门仍引用 proxy ⇒ **随拍更新**。

### K3 — `operator|` 对 1-bit 操作数返回 `ch_bool`

上游 `operators_arith.h` 的 `binary_operation`：当 `result_width <= 1` 且任一操作数是 `ch_bool` 时走 `make_bool_result(op_node)` ⇒ 返回 `ch_bool`（非 `ch_uint<1>`）⇒ 可直接赋给 `ch_bool` 变量。

### K4 — deprecated 算子统计必须用编译器，不能用 grep

`grep "||"` 会漏掉同行不含 `ch_bool` 字面量的用法。正确做法：

```bash
g++-14 -std=c++23 -fsyntax-only -Wall -Wextra $FLAGS $INCS <probe.cpp> \
  | grep -E "use operator\| for 1-bit OR|use operator& for 1-bit AND"
```

### K5 — `halt_condition()` 与 `OR_combined_halt_conds()` 每次调用生成独立 OR 节点

CppHDL 的表达式即节点 ⇒ 两次调用得到两个语义等价但指针不同的节点（与改动前 `halt_condition()` 的行为一致）。**测试断言"值等价"而非"指针相等"**。

---

## 范围边界（严格遵守）

- **A4 已移出**（归属 `mmu-chmem-phase-b-c-launch` 的 C.5）—— 本 change **零惰性代码**
- **不碰业务侧 `ip/**/*_chmem.h` 的 deprecated 算子**（≥60 `||` + ≥90 `&&`）—— 另立 change
- **不改 TLM 模式的 CtrlLink 语义**（`std::function<bool()>` 路径零改动；TLM 消费方 `ibus.h:70` / `mul_div_fsm.h:300` / `hazard.h:217` 走 `#else` 分支，物理不受影响）
- **不实现 `bypass()`**（PoC stub，`ctrl_link.h` 自承"M3+ 实现"）

---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: ready
---

# Tasks — ctrl-link-chmem-reg-handle（CtrlLink 有状态 halt 条件，框架级纯改动）

> **总入口**：与 [`proposal.md`](proposal.md) + [`specs/ctrllink-continuous-handle/spec.md`](specs/ctrllink-continuous-handle/spec.md) 对齐。
> **源 draft**：`.rddf/improvements/ctrl-link-chmem-reg-handle.md`（status=ready，HARD-GATE 已过）。
> **TDD 5 步结构**：每条 task 标注 [RED → GREEN → REFACTOR] 阶段。
> **范围纪律**：**纯框架层，零惰性代码**（AC-5）。A4（HazardPlugin 信号驱动）已移出 scope，改挂 `mmu-chmem-phase-b-c-launch` 的 C.5。

---

## Phase 0 — 前置 baseline（0.3d）

- [x] **0.1** [RED] 记录当前 HEAD baseline：`bash tools/run_chipforge_tests.sh --all`，**必须含逐 case cycle 计数**（不只 PASS/FAIL）
  - **理由**：`[cpu-integration]` 存在 3 个 baseline-known FAIL + `node_count==17` infra drift（`test_7stage_riscv.cpp:124`）等既有噪声；不先锁定基线会把历史 FAIL 误归因到本 change
  - **Verify**: baseline 快照落盘（建议 `.rddf/plans/` 下随 plan 归档）
- [x] **0.2** 记录 `[chmem]` 73 case 的逐 case cycle 计数 + `[framework]` 全部 case 计数
  - **Verify**: 与 0.1 快照一致
- [x] **0.3** 记录当前构建期 deprecation warning 清单（确认 `ctrl_link.h:150,160` 两处在 warning 中）

---

## Phase 1 — A2/A6 OR 合并唯一实现（RED → GREEN）

- [x] **1.1** [RED] 写框架级单测 `tests/framework/test_ctrl_link_stateful.cpp`（新文件）：
  - case A：`OR_combined_halt_conds()` 返回的节点可被消费方 DAG 引用
  - case B：组合条件 + 有状态条件共存，OR 结果为逻辑或
  - case C：空条件集合 → 返回表示"无 stall"的常量假值
  - **Family tag**：`[framework][ctrllink-stateful]`
  - **Verify**: `./build/bin/chipforge_tests "[framework][ctrllink-stateful]"` ⇒ 此刻 FAIL（`OR_combined_halt_conds` 不存在）
- [x] **1.2** [GREEN] `include/cf/plugin/ctrl_link.h`：实现 `OR_combined_halt_conds()`（CH_MEM 分支）
  - 空集合 → 常量假值
  - 单条件 → 直接返回该条件（**不得**退化到恒假）
  - 多条件 → 依次 OR 合并，建真 OR 门 DAG
- [x] **1.3** [GREEN] **A6 定案落地**：`halt_condition()`（`:148`）与 `flush_condition()`（`:157`）改为 inline alias 转发到统一实现
  - **约束**：`halt_when` / `throw_when` / `flush_when` / `bypass` 四方法的**签名与行为不变**（`openspec/specs/ctrllink-consumption-contract/spec.md` 的 "不修改 CtrlLink 原 API" Requirement）
  - **约束**：既有调用点（`pipe_builder.h` elaborate 期合并）**零改动**
- [x] **1.4** [REFACTOR] 清理 `:96` 处"承诺但缺失"的注释，指向真实实现
- [x] **1.5** [VERIFY] `./build/bin/chipforge_tests "[framework][ctrllink-stateful]"` ⇒ PASS；既有 `tests/framework/test_ctrl_link.cpp` 11+ case 仍 PASS

---

## Phase 2 — A5 deprecated 算子清理（范围受控）

- [x] **2.1** [GREEN] `include/cf/plugin/ctrl_link.h:150`（`halt_condition` 合并循环）`operator||` → `operator|`
- [x] **2.2** [GREEN] `include/cf/plugin/ctrl_link.h:160`（`flush_condition` 合并循环）`operator||` → `operator|`
- [x] **2.3** [GREEN] `include/cf/plugin/pipe_builder.h:434`（`stall = stall || cl->halt_condition()`）`operator||` → `operator|`
- [x] **2.4** [GREEN] `include/cf/plugin/pipe_builder.h:435`（`flush = flush || cl->flush_condition()`）`operator||` → `operator|`
  - **约束**：**框架内 A5 共 4 处，全部清零**（编译器权威统计 —— 用 `g++-14 -fsyntax-only -Wall -Wextra` 逐 TU 数，**不能用 grep 数 `||`**：同一行不含 `ch_bool` 字面量的用法会漏）
  - **约束**：**不动**业务侧 `ip/**/*_chmem.h`（`decode_chmem.h` 24+36 / `hazard_chmem.h` 17+~10 / `int_alu_chmem.h` 8+~30 / `dmem_chmem.h` 7+~10 / `csr_chmem.h` 4+~10 / `cpu_factory_chmem.h` 1 …）—— 另立 change
- [x] **2.5** [VERIFY] 构建期框架范围内零 deprecation warning
  - **验证命令**（含全部 13 个 framework 头的 probe TU）：剩余 deprecated 站点数 = **0**
- [x] **2.6** [VERIFY] OR 门 DAG 逻辑等价：既有 `[framework]` + `[chmem]` 测试零回归（**逐 case cycle 计数不变**）

---

## Phase 3 — A1 有状态条件显式入口（RED → GREEN）

- [x] **3.1** [RED] 追加单测 case D：注册 `ch_reg<ch_bool>` 支撑的条件，**Simulator tick ≥2 拍**，中途改变寄存器值，观测 OR 输出**随拍翻转**
  - **约束**：单拍 elaboration 断言**不足以**证明"逐拍更新" —— 必须多拍驱动
  - **约束**：断言 SHALL 证明 OR 结果读的是**寄存器节点**而非 C++ 侧缓冲快照
- [x] **3.2** [GREEN] `include/cf/plugin/ctrl_link.h`：新增显式有状态条件入口 `halt_when_stateful(...)`
  - 内部复用既有 `halt_conds_` 存储 + OR 合并网络（**不新建平行存储**）
  - OR 门在 elaboration 期引用**寄存器节点本身**
  - **约束**：既有 `halt_when(ch_bool)` 签名与行为**不变**
- [x] **3.3** [GREEN] `include/cf/plugin/pipe_builder.h:307` / `:202`：确认两处 `should_*()` 消费点按 A3 新契约的语义
  - **已知事实（2026-10-22 实测）**：CH_MEM 运行期 `should_*()` 调用方**为零**（`mmu_exception_handler_chmem.h:89` 是注释掉的）⇒ 全部暴露面只有这两处
  - **约束**：TLM `#else` 分支**零改动**（`ibus.h:70` / `mul_div_fsm.h:300` / `hazard.h:217` 物理不受影响）
- [x] **3.4** [VERIFY] `./build/bin/chipforge_tests_chmem "[framework][ctrllink-stateful]"` ⇒ 全部 PASS（9 case / 35 assertions）
  - **注**: 本文件是 CH_MEM-only (`#ifdef CF_PLUGIN_USE_CH_MEM` 包裹) ⇒ 只能跑 `chipforge_tests_chmem` binary

---

## Phase 4 — A3 契约显式化（RED → GREEN）

- [x] **4.1** [RED] 追加单测 case E：`should_*()` 对有状态条件的契约测试
  - **要求**：明确断言其行为（"仅对组合条件可靠"），**而非"碰巧不报错"**
  - **要求**：对有状态条件 SHALL NOT 静默返回陈旧值 ⇒ 须有可观测的失败/告警语义
- [x] **4.2** [GREEN] `include/cf/plugin/ctrl_link.h:98-124`：改写 `should_halt()` / `should_throw()` / `should_flush()` 的文档契约
  - 显式声明"仅对组合条件可靠"
  - 有状态条件 ⇒ 引导到 `OR_combined_halt_conds()` 的 DAG 消费路径
  - 实现可观测的告警/失败语义（不得静默返回陈旧值）
- [x] **4.3** [REFACTOR] `include/cf/plugin/pipe_builder.h:425-445`（elaborate 期合并）：加注释/断言保证有状态条件的 OR 门**引用寄存器节点**而非 C++ 侧缓冲快照
- [x] **4.4** [VERIFY] 组合条件的 `should_*()` 语义不变（既有 `test_ctrl_link.cpp` 11+ case 零回归）

---

## Phase 5 — 测试接入 + 注释更新

- [x] **5.1** `tests/CMakeLists.txt`：把新文件 `framework/test_ctrl_link_stateful.cpp` **手动加入 `CHMEM_TEST_SOURCES`（`:118-145`）**
  - **理由**：该 list 是**逐条手写**的；GLOB（`:6`）只覆盖 TLM target ⇒ 不加就**静默不编译不运行**
- [x] **5.2** [VERIFY] `./build/bin/chipforge_tests_chmem "[framework][ctrllink-stateful]"` ⇒ case 可见可跑
- [x] **5.3** `ip/cpu/plugins/hazard_chmem.h:149-173`：更新 FP-4 gap 注释，指向 `mmu-chmem-phase-b-c-launch` 的 Phase C.5 新归属
  - **约束**：**零逻辑改动**（AC-5）
- [x] **5.4** [VERIFY] `git diff --stat ip/cpu/plugins/hazard_chmem.h` ⇒ 仅注释行变更

---

## Phase 6 — spec / 文档 / 归档

- [ ] **6.1** `openspec/specs/ctrllink-continuous-handle/spec.md`：由本 change 的 delta spec 归档生成（新 capability，ADDED Requirements）
  - **Verify**: `openspec validate --specs --strict` ⇒ PASS
- [x] **6.2** `AGENTS.md`：`chmem-mext-fsm-cycle-parity` 的硬前置状态更新为"已解除"
- [x] **6.3** `CHANGELOG.md`：新增条目（**Phase 1 先写**，`feat-phase-6d-rtl-verification` 收官时追加，避免同文件编辑顺序冲突）
- [x] **6.4** [VERIFY] 4 架构 gate 0 失败：`verify_adr.sh` / `verify_plugin_decision.sh` / `check_plugin_portability.sh` / `doc_link_check.sh` + `roadmap-r9-check.sh`
- [x] **6.5** [VERIFY] `bash tools/run_chipforge_tests.sh --all` ⇒ 全 PASS，且**逐 case cycle 计数与 Phase 0 baseline 一致**
- [ ] **6.6** `openspec change validate ctrl-link-chmem-reg-handle` ⇒ 0 error；`openspec archive ctrl-link-chmem-reg-handle -y`

---

## 遗留项（不在本 change，另立）

- [ ] **L1** 业务侧 deprecated `ch_bool::operator||` / `operator&&` 清理 ⇒ 独立 change，排在 `mmu-chmem-phase-b-c-launch` Phase B 之后
  - **编译器权威统计 (2026-10-22)**: 框架 `include/cf/plugin/` 内 **4 处已在本 change 清零**；业务侧 `ip/**/*_chmem.h` 另有 ≥60 `||` + ≥90 `&&`（`decode_chmem.h` 24+36 / `hazard_chmem.h` 17+~10 / `int_alu_chmem.h` 8+~30 / `dmem_chmem.h` 7+~10 / `csr_chmem.h` 4+~10 / `cpu_factory_chmem.h` 1 …）
  - ⚠️ **统计方法**: 不能用 grep 数 `||`（同行不含 `ch_bool` 字面量的用法会漏）⇒ 必须用 `g++ -fsyntax-only -Wall -Wextra` + grep `use operator\| for 1-bit OR`
- [x] **L2** **A4** HazardPlugin mmufault 信号驱动改造 ⇒ **归属已转移**到 `mmu-chmem-phase-b-c-launch` 的 **C.5**（驱动源是 C.1/C.5 的 `CPU_EXCEPTION_CODE` 写入）
  - **转移已完成**: `.rddf/improvements/ctrl-link-chmem-reg-handle.md` Out-of-Scope + `mmu-chmem-phase-b-c-launch.md` manual_deps + `ip/cpu/plugins/hazard_chmem.h:149-190` 注释三处同步
  - **本 change 零惰性代码**: 不引入任何"已接上但永不触发"的信号通道
- [ ] **L3** `bypass()` 实装（`bypass_active` 至今是 PoC stub，`ctrl_link.h:126-128` 自承"PoC: M3+ 实现"）⇒ 另立 change

---

**总计**: ≈ 34 tasks，≈ 1d（不含 Phase 0 baseline 等待）
**触发前置**: 无（框架级，不依赖 MMU 实现）
**下游解锁**: `chmem-mext-fsm-cycle-parity`（硬前置解除）+ `mmu-chmem-phase-b-c-launch` Phase C.5（stateful halt 通道就位）

# ADR-088: TLM 后端退役 — 冻结契约与语义裁决锚（TLM Backend Retirement: Freeze Contract & Semantic Adjudication Anchor）

> **Status**: ✅ **Accepted（v1.2，2026-10-24）** —— B0（`chmem-tlm-backend-freeze`，已归档）立冻结契约与治理锚；**B2 语义裁决已完成**（§3 五行正式表态 + §7 ADR-080 范围重定义 + §5.2 BP 专项前置）；**v1.2 追加 §5.3「性能门禁在 CH_MEM 后端的承接判据」**（实测驱动：CH_MEM DMIPS 1.24467 < 1.4 ⇒ 根因 = CH_MEM 数据通路缺 load-use 通路 ⇒ **不重校准判据**，改补能力）
> **版本历史**: v1.0（2026-10-24，B0，🚧 Proposed）→ v1.1（2026-10-24，B2 裁决完成，✅ Accepted）→ **v1.2（2026-10-24，性能门禁承接裁决，✅ Accepted）**
> **类别**: 架构治理 / 后端退役
> **关联 ADR**: ADR-040 v3.0（TLM→HDL 移植性约束，§10.1 双后端语义必须一致 / §10.2 差异必须显式契约化）· ADR-083（Plugin-style 单一 source of truth，§2.3 CppTLM=legacy / §4.3 拒绝「TLM 模式作为长期模式」）· ADR-046（多周期协议引擎豁免）· ADR-087（ISA 声明真实性，§D4 护栏形态随 TLM 退役切换）
> **关联变更**: `chmem-tlm-backend-freeze`（B0，已归档：冻结门禁 + 死代码清理 + 本 ADR）· `chmem-tlm-coverage-parity-ledger`（台账**内容**，已归档）· **B2 语义裁决（本 v1.1）** · B1 承载力 / B3 删除各自独立立项
> **路线图载体**: `.rddf/roadmap/features/feat-tlm-backend-retirement.md`（B0–B3 编排）· `.rddf/roadmap/objectives/objective-ip-cpu-tlm-backend-retirement.md`（G1/G2/G3 门禁）
> **姊妹提案**: `chmem-tlm-coverage-parity-ledger`（P1，交付台账**内容**；本 ADR 只定义**格式**）

---

## Context

### 1.1 ADR-083 给了定性，没给终态

ADR-083（Accepted 2026-10-08）已定性：

> CppHDL 是 Plugin-style 的底层设施；CppTLM 是 legacy 仿真内核，**仅 `src/cf_plugin/bridge/` 适配层依赖**；业务 IP 不使用 `ChStreamModuleBase` / `SimObject` / `REGISTER_MODULE` / `chstream_*.hh`。

但 ADR-083 §5.2「代码无需修改」+ §5.3「CI 验证」只提供了**定性**与**现状描述**，没有给：

- **冻结条款** —— TLM-only 业务代码此后还能不能长？长的话要满足什么条件？
- **退役时间线** —— 分几步走？每步的前置是什么？
- **删除判据** —— B3 凭什么判定「这个文件可以删」？

于是仓库实际状态是「双后端并行且语义漂移」，而**没有任何机制阻止漂移继续扩大**。

### 1.2 双写漂移已经在产生可测量的语义差异

同一 ISA 在两条路径上不是同一件事（实测，2026-10-24）：

| 子系统 | TLM 侧 | CH_MEM 侧 | 分歧性质 |
|--------|--------|-----------|---------|
| M 扩展 | `ip/cpu/arch/riscv/mul_div_fsm.h`（424 行，radix-2：MUL=1c / MULH=3c / DIV=33c） | `ip/cpu/arch/riscv/int_alu_chmem.h`（19 层 select 树，组合式 1 拍） | **cycle 不同构**（差 ~286c ⇒ 原「±5% / +50c」判据不适用） |
| CSR | `ip/cpu/arch/riscv/csr.h`（43 行，**纯 stub**，`setup`/`build` 全空） | `ip/cpu/plugins/csr_chmem.h`（566 行，Zicsr + trap） | TLM 侧**无实装可保留** |
| MMU PTW | `ip/mmu/lib/ptw.{h,cpp}`（149+197 行，完整 walk） | `ip/mmu/plugins/MMUPlugin_chmem.h`（**无 PTW**，miss ⇒ 回退 vaddr）；且 CH_MEM 工厂 `enable_mmu=true` 为 **plumbing-only no-op**（不注册任何 MMU plugin）⇒ MMU 在 CH_MEM 侧**仅单元级存在、流水线级未接线** | 语义不等价（回退 vaddr 与 Bare 路径同构）**+ 接线缺口（流水线级）** |
| BP（BTB/Bimodal/GShare） | `ip/cpu/plugins/branch_predictor.h`（319 行） | **完全缺失**（无 `branch_predictor_chmem.h`；CH_MEM binary 符号数 = 0） | 功能缺失 |
| MMU exception handler | `ip/cpu/plugins/mmu_exception_handler.h`（69 行，**TLM-only**） | `ip/cpu/plugins/mmu_exception_handler_chmem.h`（118 行，**存在但未注册**） | `cpu_factory_chmem.h:342-349` 被 `#ifndef CF_PLUGIN_USE_CH_MEM` 包裹 ⇒ CH_MEM 下不注册 |

### 1.3 为什么先冻结，而不是直接删

| 约束 | 事实 |
|------|------|
| 测试覆盖 | TLM binary 493 case vs CH_MEM binary 130 case；共有 30 ⇒ **TLM 独占 463**（闭合 463+30=493）。族分布实测：`[cpu]` 137 / `[cpu-integration]` 96 / `[framework]` 94 / `[mmu]` 59 / `[riscv-tests]` 48 / `[cache]` 25 / `[soc]` 20 等 —— **逐条三态标注见 [`tlm-coverage-parity-ledger.md`](../tlm-coverage-parity-ledger.md)**（v1.0 的 361 为早期估算，已按实测纠正） |
| 性能硬门禁载体 | **Dhrystone DMIPS/MHz ≥1.4**（`tests/cpu/integration/test_dhrystone.cpp` 硬绑 `CpuFactory<uint32_t>`）与 **CoreMark/MHz ≥1.9**（PoC-6，被测对象 `ip/cpu/plugins/branch_predictor.h`）**100% 在 TLM** |
| CI 盲区 | 测试**不进 CI**（Oracle 2026-10-06 决策：`.github/workflows/architecture-gates.yml` 只跑架构门禁脚本）⇒ 删 361 case 后**无自动回归发现机制** |
| 链接依赖 | `chipforge_tests_chmem`（`tests/CMakeLists.txt:183`）与 `cpu_verilator_sim`（`tools/verilator_runner/CMakeLists.txt:47`）**仍链接 `libcpptlm_core.a`** ⇒ 删 TLM 代码 ≠ 立刻脱 CppTLM |
| 删除误判风险 | 9 个文件**目录/文件名像 TLM 但被 CH_MEM 侧 include**（§5 清单）⇒ 按路径名推断 ⇒ 打断 CH_MEM 编译 |

⇒ **判据必须先于动作**。本 ADR（B0）只立**判据 + 冻结 + 治理锚**；真正的删除在 B3，由 Gate 3 放行。

---

## Decision

### 2. 冻结条款

#### 2.1 冻结范围与判定机制

**检测面** = `ip/` 下所有**非 `_chmem.h`** 的 `.h` 文件（实测 44 个候选），**减去** 9 个双模共享文件（§5.1 清单）⇒ 实测 **35 个 TLM-only 候选**（死代码清理后 31 个）。

**判定内容** = 相对 **baseline 锚点**（`.rddf/state/tlm-freeze-base.txt`，一个固定 commit sha）**新增的函数定义**。

| 要素 | 规定 |
|------|------|
| baseline 锚点 | 固定 sha（**不用 `main`**）⇒ 切分支不影响判定；换锚点必须显式改文件并在本 ADR 记录 |
| 函数定义识别 | 行首匹配 `static` / `inline` / `virtual` / `template` 或「返回类型 + 标识符 + `(`」的行；**排除** `if`/`for`/`while`/`switch`/`catch`/`return`/`sizeof` 与注释行 |
| 白名单 | `.rddf/state/tlm-freeze-allowlist.txt`，每行 3 段：`<文件:行> | <原因> | <CH_MEM 是否需同步: 是\|否\|不适用>`（+ 登记人/日期） |
| 违规行为 | 新增函数定义**未登记** ⇒ 脚本 **exit 1** 并报出 `文件:行` + 签名 + 登记提示 |
| 累计阈值 | 白名单 > 5 条 ⇒ 输出「需 ADR-088 复审」提示（不自动阻断） |
| CI 层 | `continue-on-error: true`（**warn 级**，不阻塞 PR）—— 脚本层与 CI 层是两层语义 |
| 脚本 | `tools/check_tlm_freeze.sh`（已实现，§8 有三态实测留证） |

**为什么不维护「TLM-only 文件全清单」**：文件随 PR 增删，清单必然腐化。「模式面 − 共享排除清单」是**结构性**判定：新增文件自动落入检测面，只有真被 CH_MEM include 的文件才需显式豁免（豁免动作本身要写进本 ADR）。

#### 2.2 冻结期内的合法动作

| 允许 | 不允许 |
|------|--------|
| **存量函数体**的 bugfix（不新增函数定义） | 在 TLM-only 文件**新增**函数定义而不登记白名单 |
| 双模共享文件（§5.1）的常规维护 | 把 TLM-only 逻辑**搬进**共享文件以规避检测面 |
| 为 B2 语义裁决做**对照实验**（实验代码须登记白名单并注明「CH_MEM 是否需同步」） | 在 TLM-only 路径**新增**能力（能力应加在 CH_MEM 侧） |

#### 2.3 解冻条件

仅两种：① B3 删除完成（TLM-only 业务代码面清空 ⇒ 冻结条款自然失效）；② 架构决策显式解冻（需**新 ADR**，不得由本 ADR 的实施者自行解除）。

### 3. 逐子系统语义裁决表

> **B2 正式表态已完成（2026-10-24，architect 主持）** —— 下表「B2 正式表态」列即裁决结论。
> **裁决原则**（依 ADR-040 §10.1/§10.2）：**CH_MEM 是唯一保留后端 ⇒ 语义规范由 CH_MEM 侧定义**；TLM 侧仅作历史参考，**除非 CH_MEM 侧存在正确性缺陷**（而非形态差异）。
> 五行结论速览：**M 扩展 = CH_MEM 组合式为规范**（功能 + cycle 均是）· **CSR = CH_MEM 为规范**（TLM 无实装可保留）· **MMU PTW = 现在不接线，回退 vaddr 表态为已知临时降级，接线归 `chmem-ptw-walk`（owner 已定案，`mmu-chmem-phase-b-c-launch` 仅保留 Phase C.5 异常回传前置）** · **BP = CH_MEM-only 且必须做，不适用「不做」出口** · **MMU exception handler = 必须注册，与 MMU 流水线接线同批**。

| 子系统 | TLM 侧实现 | CH_MEM 侧实现 | 已知分歧 | B2 正式表态 |
|--------|-----------|---------------|---------|-----------|
| M 扩展 | `ip/cpu/arch/riscv/mul_div_fsm.h`（424 行，radix-2，MUL=1c/MULH=3c/DIV=33c） | `ip/cpu/arch/riscv/int_alu_chmem.h`（19 层 select 树，组合式 1 拍；MULH 高位抽取等 RV32-only 语义） | **cycle 不同构**（差 ~286c）；原「±5% / +50c」判据不适用（判据本身失效，非 pass/fail） | ✅ **CH_MEM 组合式为规范（功能与 cycle 均是）**。理由：① 组合式经 `toVerilog` 即真实硬件语义（Verilator 1 拍 = 硬件真相），radix-2 是软件模拟的**时序近似**；② RISC-V spec 不规定除法延迟（只规定结果正确）⇒ cycle 不是规范语义；③ CH_MEM 工厂无 stage 寄存器/无 stall（`:329-340`），物理上无法直接接纳 33 拍 FSM。**降级项**：`chmem-mext-fsm-cycle-parity` 降为**可选演进**（非 v1.0.0 完成条件）；`soc/cpu/docs/dse/rv32um-baseline-matrix.csv` 的 cycle 基线**降为 TLM-only 历史基线**（须加表头注记，否则会被误当 parity 判据）。**回归锚**：`[chmem]` vendored ELF 47/47 + `[alu][semantics]` 288 assertions。 |
| CSR | `ip/cpu/arch/riscv/csr.h`（43 行，**纯 stub**：`setup`/`build` 全空） | `ip/cpu/plugins/csr_chmem.h`（566 行，Zicsr + trap，真实实现） | TLM 侧**无实装可保留** | ✅ **CH_MEM 为规范**。实测**无任何测试依赖 TLM stub 语义**（`tests/` 下对 `arch/riscv/csr.h` 的引用只有 `test_cpu_factory.cpp:74` 注释 + `:87` 的 `REQUIRE(plugins.size() == 14)` 注册**计数**断言）；TLM 侧真正 CSR 写路径由 `RiscvMMUPlugin` 拥有（`ip/cpu/plugins/mmu.cpp:73,94`），与 stub 无关。**B3 附带动作**：删除 stub 时须同步改 `test_cpu_factory.cpp:87` 计数（14 → 13），并**建议改为按类型名断言关键插件存在**（消除脆弱计数）。 |
| MMU PTW | `ip/mmu/lib/ptw.{h,cpp}`（149+197 行，完整 walk） | `ip/mmu/plugins/MMUPlugin_chmem.h`（**无 PTW**；`PtWalkFsmPlugin` 存在但未注册；miss ⇒ 回退 vaddr） | PTW 未落地前 CH_MEM 表现为**回退 vaddr**（与 Bare 路径同构）⇒ 语义不等价 | ⚠️ **现在不接线**；**正式表态：「回退 vaddr」= 已知临时降级（与 Bare 同构）**，**不是**规范语义。理由：① 目标是 ADR-085/086 **已锁定**的架构（`MMU_TLB`+`MMU_PTW` 拆分 D01 / `FsmBase` 封装 D10 / `PTWCache` D04 / **代码落 `ip/mmu/plugins/` 修 ADR-083 D18 违规**）；现有 `PtWalkFsmPlugin`（`ip/cpu/plugins/mmu_ptw_chmem.h:150`，sv32 2 级、裸 DSL、**位置违反 D18**、无 PTWCache）与已锁目标不符 ⇒ 现在接线**必然返工**；② 接线的三类耦合前置（页表内存访问时机 D16/D21 · 异常回传 A4/C.5 归 `mmu-chmem-phase-b-c-launch` · satp 写路径 D08/D20）**实装均未立项** ⇒ 时序倒挂。**归属（owner 已定案 2026-10-24，用户拍板；边界依 `mmu-chmem-phase-b-c-launch/tasks.md:125` 既有划分）**：
① **PTW walk 主体**（B.2' 适配器 / B.3' 抽 `build_pt_walk_fsm` / B.3'' miss⇒walk+refill+fault+fetch stall / B.3''' `MEM_ADDR` 源 / B.4' 测试）归未立项 change **`chmem-ptw-walk`**（B1 item 3）；
② **MMU 本体注册 + 异常回传**（C.1 `mmu_exception_handler_chmem` wiring / C.4 `MMUPluginChmem` 注册进 `PipeBuilder` / C.5 `hazard_chmem` halt_when wiring / C.6 集成测试）**留 `mmu-chmem-phase-b-c-launch`**；
③ **依赖方向**：C.4（注册载体）先于 B.3''（walk 接入）—— 即 `phase-b-c-launch` Phase C 提供载体，`chmem-ptw-walk` 消费 ⇒ **无双 owner**（`chmem-ptw-walk` 消费 C.1/C.5 提供的异常信号）。
`cpu_factory_chmem.h` **保持 0 PTW 注册**直至 `chmem-ptw-walk` 立项。**台账条目 = `partial`**（差异性质 = 未落地降级，禁标「不做」）。 |
| BP（BTB/Bimodal/GShare） | `ip/cpu/plugins/branch_predictor.h`（319 行）+ `.cpp`（23 行） | **完全缺失**（无 `branch_predictor_chmem.h`；CH_MEM binary 符号数 = 0） | 功能缺失；且是 **CoreMark PoC-6 的被测对象** ⇒ B3 前不可删 | ✅ **CH_MEM-only 且必须做；不适用 §5.2 判据 2 的「明确不做」出口**。理由：① CoreMark/MHz ≥1.9（PoC-6，v1.0.0 硬门禁）与 PoC-14 FPGA ≤10% 偏差是**已承诺的路线图门禁**，BP 判「不做」= 门禁**孤儿化**；② `feat-bp-btb-gshare` 已改绑 CH_MEM-only 并归 B1 第 5 项（2026-10-23）；③ **必须前置测量链路**：`branch_predictor_chmem.h` 做出来后，无 Verilator 侧 CoreMark harness 则无法验证 ≥1.9（cycle 通道已存在：`tools/verilator_runner/sim_main.cpp:98` 输出 `TOHOST=%u CYCLES=%u`；缺的是 ELF harness + score 换算 = B1 item 1）⇒ **BP 与 CoreMark harness 同批交付**。**B3 删除门禁追加双前置**（见 §5.2）。台账条目 = `not-covered`，归属 = B1 item 5（**禁标「不做」**）。 |
| MMU exception handler | `ip/cpu/plugins/mmu_exception_handler.h`（69 行，**TLM-only**：`cpu_factory_chmem.h:54-56` 的 `#ifndef` 包裹） | `ip/cpu/plugins/mmu_exception_handler_chmem.h`（118 行，**存在但未注册**） | CH_MEM 侧实现存在却未接线 ⇒ 是「接线缺口」而非「能力缺口」 | ✅ **必须注册**（属**接线缺失**，非能力缺失）。理由：CH_MEM 变体（118 行）已存在，仅被 `ip/cpu/cpu_factory_chmem.h:342-349` 的 `#ifndef CF_PLUGIN_USE_CH_MEM` 排除。**归属**：与 MMU 流水线接线（`mmu-chmem-phase-b-c-launch` Phase C / `chmem-ptw-walk`）**同批**注册 —— 单独注册会得到「异常回传源（`CPU_EXCEPTION_CODE`）尚未被写入」的惰性骨架。**B3 前置**：该文件在 CH_MEM 侧注册生效后，TLM-only 版方可删。 |

### 4. 覆盖平价台账格式（B0 交付**格式**，不交付**内容**）

台账逐条记录「某能力在 CH_MEM 侧是否有等价覆盖」，三态标注：

| 态 | 含义 | 必填字段 |
|----|------|---------|
| `covered` | CH_MEM 有等价实现**且**有回归锚 | 子系统、TLM 侧锚点（file:line）、CH_MEM 侧锚点（file:line）、回归锚点（测试文件 + case 名）、语义差异说明（应为空或仅列非语义差异） |
| `partial` | 有实现但语义不等价，或无回归锚 | 同上 + **差异性质**（如「cycle 不同构」「接线缺口」）+ 计划动作（归 B1/B2/B3 哪一阶段） |
| `not-covered` | CH_MEM 无对应实现 | 子系统、TLM 侧锚点、缺失原因、归属阶段（B1 承载力补齐 or 明确不做） |

**口径注记（必须写进台账）**：族计数求和（46 族，基线实测 **678**）≠ TLM 独占 case 数（**463**）—— 一个 case 可带多个 tag ⇒ **case 级差集以两个 binary 的 `--list-tests` 差集为唯一口径**（族内求和口径不可用）。台账实测闭合：TLM 独占 463 + 共有 30 = **493**（TLM 总数）；CH_MEM 独有 100 + 共有 30 = **130**（CH_MEM 总数）。

**内容交付**：由姊妹提案 `chmem-tlm-coverage-parity-ledger`（P1）承担。**格式不得后置**（格式后置 ⇒ B3 无判据）。

**红线可机检性（2026-10-24 闭环）**：§4 的三条红线现已**全部**由 `tools/check_coverage_parity_ledger.sh` 机检 —— ① `not-covered` 总数 ≤135 · ② **同一理由 ≤10 次** · ③ `[framework]` 族零 `not-covered`；另加相邻校验「`not-covered` 必须有显式手写理由（不得为空 / 不得为继承标记）」。② 的口径（仅作用于 `not-covered` · 理由取台账第 5 列 · 归一化仅去首尾空白 · 字面相同即同组 · 违规须列出条目名）记录在台账 §7。

### 5. B3 删除判据

#### 5.1 双模共享文件清单（9 个，include 闭包实测）

| # | 文件 | 行数 | CH_MEM 侧消费点 |
|---|------|------|----------------|
| 1 | `ip/cpu/tlm/cpu_keys.h` | 73 | `ip/cpu/plugins/mmu.cpp:27` · `mmu_exception_handler.h:18` · `tests/cpu/test_cpu_chmem_keys_modality.cpp:62` |
| 2 | `ip/mmu/tlm/mmu_keys.h` | 96 | `ip/mmu/plugins/MMUPlugin_chmem.h:70` · `ip/cpu/plugins/dmem_chmem.h:24` |
| 3 | `ip/mmu/lib/ptw.h` | 149 | `ip/mmu/plugins/MMUPlugin_chmem.h:69` |
| 4 | `ip/cpu/arch/riscv/decoder_table.h` | 430 | `ip/cpu/cpu_factory_chmem.h:41` · `int_alu_chmem.h:72` |
| 5 | `ip/cpu/arch/riscv/payload_riscv.h` | 88 | `ip/cpu/arch/riscv/int_alu_chmem.h:73` |
| 6 | `ip/cpu/core/payload_common.h` | 181 | `int_alu_chmem.h:74` · `ip/cpu/plugins/csr_chmem.h:44` |
| 7 | `ip/mmu/lib/memory_interface.h` | 63 | `ip/mmu/lib/ptw.h:21` ← `MMUPlugin_chmem.h:69` |
| 8 | `ip/cpu/arch/riscv/mul.h` | 157 | `ip/cpu/arch/riscv/mul_div_fsm.h:54` ← `ip/cpu/plugins/mul_div_fsm_chmem.h:38` |
| 9 | `ip/cpu/arch/riscv/mul_div_fsm.h` | 424 | `ip/cpu/plugins/mul_div_fsm_chmem.h:38` |

> 机器可读镜像：`.rddf/state/tlm-shared-files.txt`（冻结门禁的排除清单）。
> ⚠️ **第 10 个候选已被实测排除**：`ip/cpu/plugins/mmu_exception_handler.h`（69 行）曾被列为「未定」，但 `ip/cpu/cpu_factory_chmem.h:54-56` 把它包在 `#ifndef CF_PLUGIN_USE_CH_MEM` 内 ⇒ CH_MEM 模式**不编译**它 ⇒ 它是 **TLM-only**，不在共享清单里。

#### 5.2 删除判据

1. **按 CH_MEM binary 包含闭包反查** —— 候选文件必须**不可达**（或仅为 §5.1 清单内的共享文件，且后者**永不删除**）。**禁止**按目录名 / 文件名 / 「看起来像 TLM」推断。
2. **覆盖平价达成** —— §4 台账中该子系统相关条目的状态已可接受（`covered` / 明确 `not-covered` 且归属「不做」）。
   - ⚠️ **BP 专项前置（B2 added）**：BP 相关文件（`ip/cpu/plugins/branch_predictor.{h,cpp}`）**不适用「不做」出口**。删除前 MUST 同时满足：① `branch_predictor_chmem.h` 存在**且已被 CH_MEM 工厂注册**；② CoreMark harness 在 `cpu_verilator_sim --elf` 路径跑通并产出 `CoreMark/MHz` 实测值（依 §3 BP 行）。缺任一 ⇒ 不得删除。
3. **语义裁决完成** —— §3 表中该子系统的 B2 正式表态已给出。
4. **CI 有可回归的自动测试** —— 对应能力在 CH_MEM 侧有回归锚（当前测试不进 CI ⇒ B3 需先解决 CI 盲区或提供等价门禁）。
5. **三道前置缺一不可** —— ①②③ 任一未满足 ⇒ 不得删除。


### 5.3 性能门禁在 CH_MEM 后端的承接判据（v1.2 裁决，2026-10-24）

> **触发**：`chmem-verilator-perf-harness`（交付点 A，已归档）首次在 **CH_MEM/Verilator 后端**跑通 Dhrystone ⇒ 实测 **DMIPS/MHz = 1.24467 < 1.4 硬门禁**（TLM 侧同 workload = 1.43277）。**本节裁决该差距的性质与处置**。

#### 5.3.1 根因（实测，非推断）

| 证据 | 数据 | 结论 |
|------|------|------|
| 源码自述 | `ip/cpu/plugins/hazard_chmem.h:38`「**不实现数据前推 (forwarding) — 当前纯 stall 策略, 无旁路**」 | CH_MEM 侧 RAW 依赖**全用 stall 气泡** |
| 简单 ELF 对照 | `rv32ui-p-add` 461→478（1.037）· `rv32ui-p-lw` 279→287（1.029）· `rv32ui-p-addi` 238→246（1.034） | 比率**恒为 ~1.03**（复位/启动固定开销）⇒ **非系统性 per-instruction 成本** |
| 高依赖密度 workload | Dhrystone 794478 → 914544（**1.151**） | 差距**与依赖密度相关** ⇒ 与「纯 stall 策略」一致 |
| 组合通路特性 | CH_MEM 无 stage 寄存器（`cpu_factory_chmem.h` 8-plugin 全组合）⇒ load 数据同拍组合可得；`dmem_chmem.h:156` 读口为 **async**（同拍可得） | load-use 在组合通路下**不需** stall ⇒ 怀疑对象转向 RAW 检测 |
| ★PoC 证伪（2026-10-24 实施期） | 把 CH_MEM 侧 RAW stall 临时置为恒 false（`hazard_chmem.h` 的 `halt_when(result)` ⇒ `halt_when(ch_bool(false))`）后重建重测 | **cycles 完全不变（914544）** ⇒ **RAW stall 不是根因**，P-2 的前提被推翻（PoC 已撤销，代码复原） |
| ★分支密度对照（2026-10-24） | 比率随分支密度单调上升：无分支 `add` 1.037 / `addi` 1.034 · `jal` 1.078 · `beq` 1.098 · `bne` 1.105 · **Dhrystone 1.151**；delta 120066 cycles ÷ 2000 iter = **60 拍/迭代** ≈ 30 分支 × 2 拍 flush 代价 | **根因 = CH_MEM 无分支预测**（TLM 侧有 BTB `branch_predictor.h` ⇒ 预测命中不 flush；CH_MEM 侧无 BP ⇒ 每条 taken 分支都要 flush 流水线）⇒ **与 B1 ② BP 同源** |

⇒ **性质判定（v1.2 修订，2026-10-24 实施期 PoC 后）**：1.24467 的差距是 **CH_MEM 侧缺分支预测器的**能力缺失**（TLM 有 BTB ⇒ 预测命中不 flush；CH_MEM 无 BP ⇒ taken 分支逐条 flush）**，**不是**判据过时 ⇒ **P-1（不重校准判据）依然成立**；但 **P-2/P-4（补 load-use 通路）的前提已被 PoC 证伪 ⇒ 作废**。

#### 5.3.2 裁决

| # | 裁决 |
|---|--------|
| **P-1** | **不重校准 DMIPS ≥1.4 判据**。理由：重校准会把**架构缺陷伪装成判据变化** —— 这正是 §3 BP 行反对的「判『不做』= 门禁孤儿化」的同型错误。判据是产品承诺，CH_MEM 必须**达标**而非**改标** |
| ~~**P-2**~~ | ~~**新增 change `chmem-load-use-bypass`**~~ → **★作废（2026-10-24 实施期 PoC 证伪）**：禁用 CH_MEM 侧 RAW stall 后 **cycles 完全不变（914544）** ⇒ load-use 通路**不是**差距根因 ⇒ 该 change **不立项**（plan 已标注作废并留证） |
| **P-6** | **绝对值门禁的恢复条件 = ② `chmem-bp-btb-gshare` 落地**（差距根因 = CH_MEM 无 BP ⇒ 与 ② **同源**，非独立工作项）⇒ ① 的绝对值 1.4 判据**由 ② 的副产品恢复** ⇒ ② 的不可延后性**进一步增强**（既是 BP 门禁载体，又是性能门禁恢复的必要条件） |
| **P-3** | **临时口径**（转发落地前）：CH_MEM 侧性能基线判据 = 「同 workload **cycle 比率 ≤ 1.15**（= 现状基线）」；**绝对值 1.4 判据保留**，并在基线 CSV notes 标注「**CH_MEM 后端未达标（缺 load-use 通路），绝对值判据待 P-2 落地后自动恢复**」⇒ 既不掩盖缺口，也不让 CI 长期红 |
| ~~**P-4**~~ | ~~P-2 的修法选型（精确化 RAW vs 真旁路）~~ → **随 P-2 一并作废**（PoC 已给出答案：**两者都不是根因**）。★**方法论留存**：本节的「PoC 先行、不预设修法」流程**有效**（避免了一次基于错误根因的 change 立项） |
| **P-5** | **交付点 B（CoreMark 移植）**的中间验收改用**分支 microbenchmark**（可控预测率的 pattern 循环 ⇒ 直接测「命中/未命中 ⇒ flush 与 cycle 差异」），CoreMark 保留为最终载体且**允许顺延**（D1 的补充）⇒ ② 不被 CoreMark 移植的不确定性阻塞 |

#### 5.3.3 与既有裁决的关系

- §3 M 扩展行「CH_MEM 组合式为规范（功能与 cycle 均是）」**不受影响** —— 本节裁决的是**性能门禁的达标路径**，不是 cycle 语义的可比性
- §5.2 判据 4「CI 有可回归的自动测试」**已由交付点 A 部分满足**（新增 warn 级 CI 门禁 step；注：CI 不跑 ctest ⇒ 仅 ctest entry 不够）
- **★ ② `chmem-bp-btb-gshare` 的地位升级**（v1.2 修订）：既是 §3 BP 行的门禁载体，又是 **P-6 的绝对值门禁恢复必要条件** ⇒ ② 同时解锁 **B3 删除判据 2（BP 专项）** 与 **判据 4（CI 可回归 + 绝对值达标）**
- **★ `chmem-branch-microbench`（0c）保留**：它是 ② 的**中间验收**手段（可控预测率负载 ⇒ 增益可量化 + 单调性判据），**不依赖** ② 本身 ⇒ 可先行交付

### 6. ADR-087 v1.1 待办登记（**显式登记行**）

> **待办（owner: architect）**：ADR-087 §D4 的护栏形态随 TLM 退役切换 —— 本 ADR 冻结并最终删除 TLM-only 业务代码后，ADR-087 §D4 的 **TLM 侧** `static_assert`（`ip/cpu/arch/riscv/int_alu.h` / `branch.h`，已由 `rv64-isa-declaration-correction` 落地，3 处）**将随文件删除而消失** ⇒ **必须切换为 CH_MEM 侧 CI grep 门禁**（等价于 §2 的冻结门禁：扫描 CH_MEM 侧 `kXlenBits` / `xlen` 硬编码是否重新长出来），否则 RV64 静默算错风险会在 TLM 删除后**失去全部护栏**。
>
> 该项需在 **ADR-087 v1.1** 中正式登记并给出切换判据（建议：CH_MEM 侧 `kXlenBits` 定义点收敛为单一来源后，删除 TLM 侧护栏；在此之前 TLM 侧护栏必须保留）。

### 7. ADR-080 范围重定义（B2 附带裁决，2026-10-24）

**发现（计划冲突）**：`docs/architecture/roadmap-evolution.md`（版本节点图 + §7.3）计划在 **v1.3.0** 落地 ADR-080「TLM↔CH_MEM 双模对拍**硬门禁**」，而 TLM 退役（B3）计划在**更早窗口**（phase-4/5）完成 ⇒ **对拍对象届时已不存在**，两项计划自相矛盾。同时 **ADR-040 §10.1**（已 Accepted）已把「双后端」的所指从「TLM ↔ CH_MEM」翻转为「**CH_MEM 的两个仿真后端**（CppHDL Simulator ↔ Verilator）」⇒ ADR-080 的**范围必须随之重定义**，否则 roadmap 内部自相矛盾。

**裁决**：ADR-080 的范围从「TLM ↔ CH_MEM 双模对拍」**重定义为「CppHDL Simulator ↔ 生成 Verilog 的 Verilator 仿真 —— 后端间对拍」**。变更的是对拍的**对象**，硬门禁的**性质**不变。

| 维度 | 原计划 | 重定义后 |
|------|--------|---------|
| 对拍对象 | TLM 后端 ↔ CH_MEM 后端 | **CppHDL Simulator ↔ Verilator（生成 Verilog）** |
| 依据 | ADR-040 v2.0「双模式」语义 | **ADR-040 §10.1**「双后端语义必须一致」（v3.0 已翻转所指）+ §10.2「差异必须显式契约化」 |
| 与 TLM 退役的关系 | **冲突**（对象将被删除） | **无冲突**（对象是 CH_MEM 的两个仿真后端） |
| 落地窗口 | v1.3.0 | 不变（v1.3.0），但**内容**改为后端间 parity |

**须同步的文档**：`roadmap-evolution.md` §7.3 标题与正文 · ADR-040 头部「关联 ADR」行对 ADR-080 的描述 · `docs/architecture/adr.md`（若登记 ADR-080 规划条目）。

**注**：ADR-080 本身**尚未起草**（其编号为规划占位 —— `docs/architecture/adr.md` 无注册行、`docs/architecture/adr/` 无文件）⇒ 本次裁决只**锁定范围**，起草时依本表。

---

### 8. 影响

| 方面 | 影响 |
|------|------|
| 代码 | **无业务代码改动**（B0 只删 4 个 0-includer 死代码：201 行） |
| 测试 | 无新增/删除测试文件；双 binary 全量回归 0 退化（TLM 493 = 492 + 1 pre-existing；CH_MEM 130） |
| CI | 新增 1 个 warn 级 step（`check_tlm_freeze.sh`）；既有门禁 5 个不变 |
| 工具 | 新增 `tools/check_tlm_freeze.sh` + 3 份状态文件（`.rddf/state/tlm-freeze-{base,allowlist}.txt` / `tlm-shared-files.txt`） |
| 文档 | 新增本 ADR；ADR-083 升 v1.1（§2.5 冻结条款 + §5.4 退役时间线）；`docs/architecture/adr.md` 注册 |
| B2（本 v1.1） | **无代码改动**（裁决 + 文档）；§3 五行正式表态 + §7 ADR-080 范围重定义 + §5.2 BP 专项前置；台账 4 条目按 §3 重标（CSR `covered` / M 扩展 `covered` / PTW `partial` / BP `not-covered`，归属 B1） |
| 风险 | R1 门禁误伤 legit bugfix（只拦新增函数，不拦存量修改）· R2 ADR 范围膨胀（每项必填 ≥1 节，禁止 B2 表态）· R3 CI 维护负担（warn 级，豁免只增不减）· R4 死代码误判（0 includer + 双 binary build + 全量回归三重核）· R5 用户期望落差（B0 不删业务代码）· R6 baseline 漂移（固定 sha）· R7 函数识别误报（宁可多报）· R8 与 `rv64-isa-declaration-correction` 的顺序耦合（baseline 须晚于 A 合并） |

### 9. 验证记录（2026-10-24）

#### 9.1 B2 语义裁决实测记录（2026-10-24，architect 主持）

| 项 | 证据 / 结论 |
|---|---|
| `auto_insert_stage_regs()` 调用面 | 全仓（排除 `build/`）**0 命中** ⇒ ADR-040 §v2.0 新增 4 提及的「自动 stage 间 `ch_reg` 插入」API **存在但未启用** ⇒ CH_MEM 流水线当前**无 stage 寄存器**（§3 各裁决的形态前提） |
| CH_MEM 工厂接口形态 | `cpu_factory_chmem.h:96-110` 签名 = **散装 `std::optional` 参数**（`enable_mmu` / `mmu_mode` / `enable_cache`），**不接受 `CPUConfig`**（注释自述「不读 cfg, 不引 CpuConfig 依赖 (NG1, NG2, NG3 显式声明)」）⇒ 「CH_MEM 是否接受 `isa`」这一问题的**前提不成立**：CH_MEM 工厂**根本没有 config 入口**（ISA 契约覆盖面归 B1 决策，见 [`tlm-retirement-b1-capacity-plan.md`](../tlm-retirement-b1-capacity-plan.md)） |
| CH_MEM 工厂 MMU 现状 | `enable_mmu=true` 为 **plumbing-only no-op**（`cpu_factory_chmem.h:86` 自述「**不会**注册任何 MMU plugin」「NO translation occurs」）⇒ CH_MEM 侧 MMU 目前是**单元级存在**（`[mmu-chmem-basic]` 直接构造 `MMUPluginChmem`，**不走工厂**），**流水线级未接线** ⇒ 缺口范围大于「PTW 未注册」 |
| `enable_cache=true` | **fail-fast throw**（`cpu_factory_chmem.h:88-92`，不静默退化）⇒ CH_MEM 侧 L1Cache 完全缺失，且行为已显式化（owner = `verilator-l1cache-e2e-coverage`） |
| CSR stub 依赖面 | `tests/` 对 `RiscvCsrPlugin` / `arch/riscv/csr.h` 仅 2 处命中（`test_cpu_factory.cpp:74` 注释 / `:87` 计数断言），**无行为依赖** ⇒ §3 CSR 行裁决成立 |
| BP 符号面 | CH_MEM binary `branch_predictor` 符号数 = **0** ⇒ 功能确实缺失；Verilator 侧 cycle 通道已存在（`tools/verilator_runner/sim_main.cpp:98` 输出 `TOHOST=%u CYCLES=%u`），缺 CoreMark harness |
| ADR-080 存在性 | `docs/architecture/adr.md` 无注册行、`docs/architecture/adr/` 无文件 ⇒ 编号为**规划占位**（ADR-050~084 共 25 个同类占位）⇒ §7 裁决只锁范围 |

#### 9.2 B0 实施实测（`chmem-tlm-backend-freeze`）

| 项 | 命令 / 结果 |
|---|---|
| 门禁基线 | `bash tools/check_tlm_freeze.sh` → `violations=0 allowlist=0 candidates=31 base=a9f0d97432` + exit 0 |
| 门禁变异 | 在 `ip/cpu/arch/riscv/branch.h` 追加函数 ⇒ `VIOLATION: ... branch.h:142` + 签名 + 登记提示 + `violations=1` + **exit 1** |
| 白名单豁免 | 登记该行 ⇒ `violations=0 allowlist=1` + exit 0 |
| 还原 | `git checkout --` + 按登记文本删白名单 ⇒ `grep -vc '^#'` = 0 + `violations=0` |
| 死代码 0 includer | 4 文件的**完整路径** `#include` 命中 = 0（⚠️ basename 口径会撞框架的 `cf/plugin/plugin_exception.h` ⇒ 假阳性 8） |
| 双 binary build | `chipforge_tests` ✅ + `chipforge_tests_chmem` ✅（exit 0） |
| 全量回归 | TLM 493 cases = **492 passed + 1 failed**（`test_7stage_riscv.cpp:124`，pre-existing）；CH_MEM binary 130 cases |
| 门禁复跑（删文件后） | `violations=0`，candidates 35 → **31**（4 个死代码退出检测面） |
| 5 架构门禁 | verify_adr ✅ · verify_plugin_decision 8/8 ✅ · check_plugin_portability 13/13 ✅ · doc_link_check 0 broken ✅ · roadmap-r9-check 11 fragments ✅ |

### 10. 相关文档

- ADR-040 v3.0（TLM→HDL 移植性约束）· ADR-083（Plugin-style 单一 source of truth）· ADR-046（FSM 豁免）· ADR-087（ISA 声明真实性）
- `.rddf/roadmap/features/feat-tlm-backend-retirement.md`（B0–B3 编排 + 13 条验收）
- `.rddf/roadmap/objectives/objective-ip-cpu-tlm-backend-retirement.md`（G1/G2/G3 门禁）
- [`docs/architecture/tlm-retirement-b1-capacity-plan.md`](../tlm-retirement-b1-capacity-plan.md)（B1 承载力拆解 + 决策定案）
- [`docs/architecture/tlm-coverage-parity-ledger.md`](../tlm-coverage-parity-ledger.md)（463 条覆盖平价台账，B0 姊妹提案交付）
- `.rddf/improvements/chmem-tlm-backend-freeze.md`（B0 提案）· `.rddf/improvements/chmem-tlm-coverage-parity-ledger.md`（台账内容）
- `openspec/changes/chmem-tlm-backend-freeze/`（本 change 的 proposal / design / plan / tasks）

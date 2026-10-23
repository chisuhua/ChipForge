# ADR-088: TLM 后端退役 — 冻结契约与语义裁决锚（TLM Backend Retirement: Freeze Contract & Semantic Adjudication Anchor）

> **Status**: 🚧 Proposed（2026-10-24，`chmem-tlm-backend-freeze` B0 阶段产出）
> **类别**: 架构治理 / 后端退役
> **关联 ADR**: ADR-040 v3.0（TLM→HDL 移植性约束，§10.1 双后端语义必须一致 / §10.2 差异必须显式契约化）· ADR-083（Plugin-style 单一 source of truth，§2.3 CppTLM=legacy / §4.3 拒绝「TLM 模式作为长期模式」）· ADR-046（多周期协议引擎豁免）· ADR-087（ISA 声明真实性，§D4 护栏形态随 TLM 退役切换）
> **关联变更**: `chmem-tlm-backend-freeze`（B0：冻结门禁 + 死代码清理 + 本 ADR）；B1 承载力 / B2 语义裁决 / B3 删除各自独立立项
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
| MMU PTW | `ip/mmu/lib/ptw.{h,cpp}`（149+197 行，完整 walk） | `ip/mmu/plugins/MMUPlugin_chmem.h`（**无 PTW**，miss ⇒ 回退 vaddr） | 语义不等价（回退 vaddr 与 Bare 路径同构） |
| BP（BTB/Bimodal/GShare） | `ip/cpu/plugins/branch_predictor.h`（319 行） | **完全缺失**（无 `branch_predictor_chmem.h`；CH_MEM binary 符号数 = 0） | 功能缺失 |
| MMU exception handler | `ip/cpu/plugins/mmu_exception_handler.h`（69 行，**TLM-only**） | `ip/cpu/plugins/mmu_exception_handler_chmem.h`（118 行，**存在但未注册**） | `cpu_factory_chmem.h:342-349` 被 `#ifndef CF_PLUGIN_USE_CH_MEM` 包裹 ⇒ CH_MEM 下不注册 |

### 1.3 为什么先冻结，而不是直接删

| 约束 | 事实 |
|------|------|
| 测试覆盖 | TLM binary 493 case vs CH_MEM binary 130 case；**TLM 独占 361**（`[cpu-integration]` 96 / `[riscv-tests]` 48 / `[framework]` 94 / `[soc]` 20 / `[bundles]` 9 等**无 CH_MEM 替代**） |
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

> **B0 只填表头与已知分歧；正式表态留 B2**（`feat-tlm-backend-retirement` 的 B2 阶段，由 architect 主持）。

| 子系统 | TLM 侧实现 | CH_MEM 侧实现 | 已知分歧 | B2 正式表态 |
|--------|-----------|---------------|---------|-----------|
| M 扩展 | `ip/cpu/arch/riscv/mul_div_fsm.h`（424 行，radix-2，MUL=1c/MULH=3c/DIV=33c） | `ip/cpu/arch/riscv/int_alu_chmem.h`（19 层 select 树，组合式 1 拍；MULH 高位抽取等 RV32-only 语义） | **cycle 不同构**（差 ~286c）；原「±5% / +50c」判据不适用（判据本身失效，非 pass/fail） | *（留 B2）* |
| CSR | `ip/cpu/arch/riscv/csr.h`（43 行，**纯 stub**：`setup`/`build` 全空） | `ip/cpu/plugins/csr_chmem.h`（566 行，Zicsr + trap，真实实现） | TLM 侧**无实装可保留** ⇒ B2 方向应为「CH_MEM 为规范」 | *（留 B2）* |
| MMU PTW | `ip/mmu/lib/ptw.{h,cpp}`（149+197 行，完整 walk） | `ip/mmu/plugins/MMUPlugin_chmem.h`（**无 PTW**；`PtWalkFsmPlugin` 存在但未注册；miss ⇒ 回退 vaddr） | PTW 未落地前 CH_MEM 表现为**回退 vaddr**（与 Bare 路径同构）⇒ 语义不等价 | *（留 B2）* |
| BP（BTB/Bimodal/GShare） | `ip/cpu/plugins/branch_predictor.h`（319 行）+ `.cpp`（23 行） | **完全缺失**（无 `branch_predictor_chmem.h`；CH_MEM binary 符号数 = 0） | 功能缺失；且是 **CoreMark PoC-6 的被测对象** ⇒ B3 前不可删 | *（留 B2）* |
| MMU exception handler | `ip/cpu/plugins/mmu_exception_handler.h`（69 行，**TLM-only**：`cpu_factory_chmem.h:54-56` 的 `#ifndef` 包裹） | `ip/cpu/plugins/mmu_exception_handler_chmem.h`（118 行，**存在但未注册**） | CH_MEM 侧实现存在却未接线 ⇒ 是「接线缺口」而非「能力缺口」 | *（留 B2）* |

### 4. 覆盖平价台账格式（B0 交付**格式**，不交付**内容**）

台账逐条记录「某能力在 CH_MEM 侧是否有等价覆盖」，三态标注：

| 态 | 含义 | 必填字段 |
|----|------|---------|
| `covered` | CH_MEM 有等价实现**且**有回归锚 | 子系统、TLM 侧锚点（file:line）、CH_MEM 侧锚点（file:line）、回归锚点（测试文件 + case 名）、语义差异说明（应为空或仅列非语义差异） |
| `partial` | 有实现但语义不等价，或无回归锚 | 同上 + **差异性质**（如「cycle 不同构」「接线缺口」）+ 计划动作（归 B1/B2/B3 哪一阶段） |
| `not-covered` | CH_MEM 无对应实现 | 子系统、TLM 侧锚点、缺失原因、归属阶段（B1 承载力补齐 or 明确不做） |

**口径注记（必须写进台账）**：13 族求和 = 509 ≠ 451 ⇒ **case 级差集以两个 binary 的 `--list-tests` 差集为唯一口径**（族内求和口径不可用）。

**内容交付**：由姊妹提案 `chmem-tlm-coverage-parity-ledger`（P1）承担。**格式不得后置**（格式后置 ⇒ B3 无判据）。

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
3. **语义裁决完成** —— §3 表中该子系统的 B2 正式表态已给出。
4. **CI 有可回归的自动测试** —— 对应能力在 CH_MEM 侧有回归锚（当前测试不进 CI ⇒ B3 需先解决 CI 盲区或提供等价门禁）。
5. **三道前置缺一不可** —— ①②③ 任一未满足 ⇒ 不得删除。

### 6. ADR-087 v1.1 待办登记（**显式登记行**）

> **待办（owner: architect）**：ADR-087 §D4 的护栏形态随 TLM 退役切换 —— 本 ADR 冻结并最终删除 TLM-only 业务代码后，ADR-087 §D4 的 **TLM 侧** `static_assert`（`ip/cpu/arch/riscv/int_alu.h` / `branch.h`，已由 `rv64-isa-declaration-correction` 落地，3 处）**将随文件删除而消失** ⇒ **必须切换为 CH_MEM 侧 CI grep 门禁**（等价于 §2 的冻结门禁：扫描 CH_MEM 侧 `kXlenBits` / `xlen` 硬编码是否重新长出来），否则 RV64 静默算错风险会在 TLM 删除后**失去全部护栏**。
>
> 该项需在 **ADR-087 v1.1** 中正式登记并给出切换判据（建议：CH_MEM 侧 `kXlenBits` 定义点收敛为单一来源后，删除 TLM 侧护栏；在此之前 TLM 侧护栏必须保留）。

### 7. 影响

| 方面 | 影响 |
|------|------|
| 代码 | **无业务代码改动**（B0 只删 4 个 0-includer 死代码：201 行） |
| 测试 | 无新增/删除测试文件；双 binary 全量回归 0 退化（TLM 493 = 492 + 1 pre-existing；CH_MEM 130） |
| CI | 新增 1 个 warn 级 step（`check_tlm_freeze.sh`）；既有门禁 5 个不变 |
| 工具 | 新增 `tools/check_tlm_freeze.sh` + 3 份状态文件（`.rddf/state/tlm-freeze-{base,allowlist}.txt` / `tlm-shared-files.txt`） |
| 文档 | 新增本 ADR；ADR-083 升 v1.1（§2.5 冻结条款 + §5.4 退役时间线）；`docs/architecture/adr.md` 注册 |
| 风险 | R1 门禁误伤 legit bugfix（只拦新增函数，不拦存量修改）· R2 ADR 范围膨胀（每项必填 ≥1 节，禁止 B2 表态）· R3 CI 维护负担（warn 级，豁免只增不减）· R4 死代码误判（0 includer + 双 binary build + 全量回归三重核）· R5 用户期望落差（B0 不删业务代码）· R6 baseline 漂移（固定 sha）· R7 函数识别误报（宁可多报）· R8 与 `rv64-isa-declaration-correction` 的顺序耦合（baseline 须晚于 A 合并） |

### 8. 验证记录（2026-10-24，`chmem-tlm-backend-freeze` 实施实测）

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

### 9. 相关文档

- ADR-040 v3.0（TLM→HDL 移植性约束）· ADR-083（Plugin-style 单一 source of truth）· ADR-046（FSM 豁免）· ADR-087（ISA 声明真实性）
- `.rddf/roadmap/features/feat-tlm-backend-retirement.md`（B0–B3 编排 + 13 条验收）
- `.rddf/roadmap/objectives/objective-ip-cpu-tlm-backend-retirement.md`（G1/G2/G3 门禁）
- `.rddf/improvements/chmem-tlm-backend-freeze.md`（B0 提案）· `.rddf/improvements/chmem-tlm-coverage-parity-ledger.md`（台账内容）
- `openspec/changes/chmem-tlm-backend-freeze/`（本 change 的 proposal / design / plan / tasks）

---
id: chmem-tlm-backend-freeze
name: chmem-tlm-backend-freeze
status: draft
initiative: wave6-linux-and-productization
priority: P0
phase: phase-4
category: core-impl
type: feature
manual_deps: []
review_by: 2026-10-30
主题: TLM 后端退役 — CH_MEM 单后端收敛 (冻结 → 承载力 → 语义裁决 → 删除)
依赖: ADR-083, ADR-040, ADR-046
来源: 用户指令「路线图中我只需要 chmem 的版本，老的不能生成 verilog 后端代码的版本我希望移除」
生成时间: 2026-10-23
---

# chmem-tlm-backend-freeze

**主题**: TLM 后端退役 — CH_MEM 单后端收敛 (冻结 → 承载力 → 语义裁决 → 删除)

> 本提案是 `feat-tlm-backend-retirement` 的 **B0 阶段**（冻结 + 安全子集清理 + ADR-088 起草）。
> **不做任何 TLM-only 业务代码删除** —— 那是 B3 的事，由 Gate 3 放行。
> 路线图载体：[`objective-ip-cpu-tlm-backend-retirement`](../../.rddf/roadmap/objectives/objective-ip-cpu-tlm-backend-retirement.md)

## 架构依据

ADR-083（Plugin-style 单一 source of truth，Accepted 2026-10-08）已定性：**CppHDL 是 Plugin-style 的底层设施，CppTLM 是 legacy 仿真内核，仅 `src/cf_plugin/bridge/` 适配层依赖**。

但 ADR-083 只给了**定性**，没有给**终态** —— 没有冻结条款、没有退役时间线、没有删除判据。于是仓库实际状态是：

### 双后端已经产生同 ISA 不同语义（不是"一份代码两个后端"，是"两份语义不同的实现"）

| 子系统 | TLM 侧 | CH_MEM 侧 | 证据 |
|--------|--------|-----------|------|
| M 扩展 | 真 radix-2 FSM（MUL=455c / MULH=504c / DIV=380c） | **组合式**（M-op 1 拍，mul=472c / div=94c） | `ip/cpu/arch/riscv/mul_div_fsm.h:207-274` vs `ip/cpu/arch/riscv/int_alu_chmem.h:259-410`；`AGENTS.md`「CH_MEM 模式状态」段（`[chmem][m-ext-gap]` 条目）自认「不同构、不可比」（行号随 AGENTS.md 刷新漂移，**不引用硬行号**） |
| CSR | **纯 stub**（setup/build 全空） | 真实实现 566 行 | `ip/cpu/arch/riscv/csr.h:26-36` vs `ip/cpu/plugins/csr_chmem.h` |
| MMU PTW | 完整 walk | **回退 vaddr** | `ip/mmu/lib/ptw.cpp` vs `ip/mmu/plugins/MMUPlugin_chmem.h:149-150,224-225`「PTW 未落地前表现为回退 vaddr」 |
| BRANCH predictor | BTB/GShare/RAS 319 行 | **无** | `ip/cpu/plugins/branch_predictor.h`；CH_MEM binary 中 `BranchPredictorPlugin` 符号数 = 0 |
| MMU exception handler | 已实装 | 存在但**未注册进 pipeline** | `ip/cpu/cpu_factory_chmem.h:55` 在 `#ifndef CF_PLUGIN_USE_CH_MEM` 内 |

⇒ **ADR-083 的"单一 source of truth"只在 ALU / CSR / decoder 子集上成立。** 这不是清理问题，是架构收敛问题。

### 遗留面规模（实测）

- `ip/` 共 **72** 源文件 / 11,825 行（含 4 文件 / 201 行死代码；**有效 68 文件 / 11,624 行**）；**44 文件 / 5,331 行（46%）只有 TLM 版本**
- `src/cf_plugin/bridge/` 8 文件 / 727 行（100% 直接 include CppTLM 头：`core/stream_adapter_base.hh` / `core/event_queue.hh` / `core/chstream_module.hh` / `framework/stream_adapter.hh`）
- **10 文件 / 1,731 行是"看似 TLM-only、实际双模共享（或被 CH_MEM 侧 include）"** —— 误删即打断 CH_MEM。**7 个已点名**（合计 1,080 行）：`ip/mmu/tlm/mmu_keys.h`（96 行，被 `MMUPlugin_chmem.h:70` include）、`ip/cpu/tlm/cpu_keys.h`（73 行，mode-aware `uint_t<kBits>`）、`ip/mmu/lib/ptw.h`（149 行，`MMUPlugin_chmem.h:69` vestigial include）、`decoder_table.h`（430 行）、`payload_riscv.h`（88 行）、`payload_common.h`（181 行）、`memory_interface.h`（63 行）；**其余 3 个（651 行）构成未定**，B0 目录治理时以 include 图复核为准 —— **任何删除动作禁止按目录名/文件名推断，必须走 include 闭包**
- **4 文件 / 201 行是死代码**（0 includer，已实测）

### 为什么必须先冻结、不能直接删

| 约束 | 事实 |
|------|------|
| 测试覆盖 | TLM binary 481 case vs CH_MEM 130 case，**TLM 独占 451（93.7%）**；`[cpu-integration]` 96 / `[riscv-tests]` 48 / `[framework]` 94 / `[soc]` 20 / `[bundles]` 9 / `[picolibc]` 7 等**无 CH_MEM 替代** |
| 性能硬门禁 | **Dhrystone DMIPS/MHz ≥1.4**（v0.11.0 launch gate 判据 #3，`tests/cpu/integration/test_dhrystone.cpp:18,70` 硬绑 `CpuFactory<uint32_t>`）与 **CoreMark/MHz ≥1.9**（PoC-6）载体 **100% 在 TLM** |
| CI 盲区 | Oracle 2026-10-06 决策：**测试不进 CI**（`.github/workflows/architecture-gates.yml` 只跑架构门禁脚本）⇒ 删 451 case 后**没有任何自动机制能发现回归** |
| 链接依赖 | `chipforge_tests_chmem` 与 `cpu_verilator_sim` **仍链接 `libcpptlm_core.a`**（`tests/CMakeLists.txt:183` + `tools/verilator_runner/CMakeLists.txt`）⇒ 删 TLM 代码 ≠ 立刻能脱 CppTLM |

Oracle 2026-10-23 架构咨询定性：**方向正确（架构收敛），但"移除"是终态而非动作**；按字面立即删除退化为范围蔓延（把 v0.11.0 窗口变成双后端重建工程）。

## 范围

**In Scope:**

1. **TLM 冻结门禁脚本**（新）—— `tools/check_tlm_freeze.sh`：扫描 `ip/**/plugins/*.h` 与 `ip/**/arch/**/*.h`（非 `_chmem.h`），检测**新增 feature 函数**即 fail；bugfix 走白名单豁免（白名单须逐条写明「CH_MEM 是否需同步」裁决行）。接入 CI（**warn → 后续升级 hard**）
2. **死代码清理**（4 文件 / 201 行，实测 0 includer）—— `ip/cpu/plugins/exception.h`（56 行）、`ip/cpu/plugins/fpu.h`（41 行，独立 0 includer）、`ip/cpu/arch/riscv/fpu.h`（42 行，独立 0 includer —— 与 `ip/cpu/plugins/fpu.h` 互不引用、各自独立为死代码，非传递链）、`ip/cache/plugins/cache_keys.h`（62 行）
3. **ADR-088 起草**（新）——《TLM 仿真后端退役与 CH_MEM 单后端收敛》，含 5 项必填内容：① 冻结条款 ② **逐子系统语义裁决表**（M 扩展 / CSR / MMU PTW / BP / MMU exception handler 5 项）③ 覆盖平价台账格式定义 ④ B3 删除判据 ⑤ **ADR-087 v1.1 待办登记**（D4 护栏形态随 TLM 退役切换 —— 该条目在此显式登记为 ADR-088 必填内容，验证 Acceptance 时可核对 ADR-088 草案确有此行，非空头承诺）
4. **ADR-083 v1.1 修订**—— 增补"冻结条款 + 退役时间线"一节（当前只有定性）
5. **覆盖平价台账格式**定义（**内容**由姊妹提案 `chmem-tlm-coverage-parity-ledger` 交付，本提案只定义格式）

**Out of Scope:**

- **不删除任何 TLM-only 业务代码**（44 文件 / 5,331 行）—— B3 阶段，Gate 3 放行
- **不删 `src/cf_plugin/bridge/`** —— B3 阶段
- **不删 `tools/cpu_sim`** —— B3 阶段（它是 Dhrystone / 5 个 CLI case 的载体）
- **不删"目录名带 `tlm/` 但双模共享"的 10 文件** —— 正确动作是**改名去误导**（B3 阶段目录治理），误删即打断 CH_MEM
- **不改判据来规避门禁** —— Dhrystone DMIPS ≥1.4 是 v0.11.0 launch gate 判据 #3 且 `feat-dhrystone-dmips-gate` 已 archived done；废弃需显式 ADR 决策，非本提案权限
- **不退役 `[framework]` 94 个框架语义锚** —— 双模共享框架的语义锚，只允许移植
- **不做 ADR-040 v3.0**（移除 TLM 分支语义）—— 远期，记入 ADR-088 待办
- **不改 `ip/mmu/tlm/` + `ip/cpu/tlm/` 目录结构** —— B3 阶段的目录去误导
- **不建 OpenSpec change** —— 由 rdd-builder P0 立项时创建

## Why

**双写漂移是当前最贵的成本。** TLM 与 CH_MEM 已有 5 处同 ISA 不同语义（M 扩展 cycle / CSR stub / MMU PTW / BP / MMU exception handler 未注册）。每新增一个 feature，若两份都写，就是又一处分叉；若只写 CH_MEM，TLM 就静默腐化。**冻结 + 明确"TLM 仅维护不新增"是成本最低的止血**。

**现在是最后一次在双后端语境下做架构裁决的窗口。** v0.11.0 窗口正在做 MMU CH_MEM 集成（Phase B-E）。再往后 CH_MEM 覆盖面越大，TLM 的"参考实现"价值越低、删除成本越高、迁移工作量越大 —— 而**现在删除是灾难性的**（451 case + 2 条性能硬门禁同时失去载体）。所以正确顺序是：**先冻结（止血）→ 补齐 CH_MEM 承载力 → 裁决语义 → 才删**。

**死代码清理是零风险前置。** 4 个文件 201 行，0 includer，双 binary 编译验证即可确认。删除它们同时消除一颗定时炸弹 —— `ip/cpu/plugins/reg_file.cpp` 那类"显式实例化 `uint64_t` 却用 `keys_rv32::*`"的混用代码（`ip/cpu/docs/research/ooo_forward_compat_gap_analysis.md:263-269` 已登记为 "MUST be fixed for Phase 4 (RV64 integration)"，但该文件本身是死代码）。

**治理缺口必须先补。** 没有 ADR-088，B3 的删除动作就没有语义裁决依据（谁是正统的 M 扩展语义？）、没有覆盖平价台账（删完怎么知道丢了什么）、没有删除判据（哪些文件可删、哪些看似 TLM 实则双模共享）。**删除是终态，判据必须先于动作。**

**v0.11.0 时间盒不该被挤占。** 本提案总成本 ~2d（B0 三项），B1 主体明确归 phase-5 / v1.0.0 窗口。

## What Changes

### 变更清单

| 文件 | 动作 | 具体 |
|------|------|------|
| `tools/check_tlm_freeze.sh` | **新** | TLM 冻结门禁脚本（~80 行，仿 `tools/verify_plugin_decision.sh` 模式） |
| `.github/workflows/architecture-gates.yml` | 改 | 新增冻结门禁 step（先 warn） |
| `docs/architecture/adr/ADR-088-tlm-backend-retirement.md` | **新** | ADR-088 草案（5 项必填内容） |
| `docs/architecture/adr/ADR-083-plugin-style-single-source-of-truth.md` | 改 | v1.1 修订：增补冻结条款 + 退役时间线 |
| `docs/architecture/adr.md` | 改 | 注册 ADR-088 |
| `ip/cpu/plugins/exception.h` | **删** | 56 行，0 includer |
| `ip/cpu/plugins/fpu.h` | **删** | 41 行，0 includer |
| `ip/cpu/arch/riscv/fpu.h` | **删** | 42 行，独立死代码（与 `ip/cpu/plugins/fpu.h` 无引用关系） |
| `ip/cache/plugins/cache_keys.h` | **删** | 62 行，0 includer |

### 冻结门禁的设计

**检测对象**：`ip/**/plugins/*.h` + `ip/**/arch/**/*.h` 中**非** `_chmem.h` 结尾的文件（即 TLM-only 或双模的 TLM 侧）。

**检测内容**：这些文件的**新增函数定义**（相对 baseline 快照的 diff 行）—— 即"在 TLM 侧新增 feature"。已有的函数体修改归 bugfix，走白名单。

**白名单机制**：
- 每条豁免必须写明：`文件:行` + 变更原因 + **「CH_MEM 是否需同步」裁决行**（yes/no + 理由）
- 白名单条目累计 >5 条 ⇒ 触发 ADR-088 复审（双写漂移预警信号，Oracle R5）

**降级节奏**：本提案上线为 **warn**；B1 完成后升级为 **hard**（CI 阻塞）。

### ADR-088 的 5 项必填内容

| # | 内容 | 状态 |
|---|------|------|
| 1 | **冻结条款**：TLM 进入 maintenance-only 的定义、bugfix 白名单规则、冻结门禁的 warn→hard 升级时机 | 本提案起草 |
| 2 | **逐子系统语义裁决表**（5 项：M 扩展 / CSR / MMU PTW / BP / MMU exception handler）—— 每项必须给出"哪条语义是规范语义" | **本提案起草表头 + 已知分歧**，**B2 逐条正式表态** |
| 3 | **覆盖平价台账格式定义**：三态标注（已移植 CH_MEM / 由 `[verilator]` ELF 级覆盖 / 显式退役 + 理由）；含"批量退役"检测规则 | 本提案起草格式；内容由姊妹提案交付 |
| 4 | **B3 删除判据**：按"CH_MEM binary 包含闭包反查"（**不用路径名**）+ 反向依赖序 + 每批的可验证判据 | 本提案起草 |
| 5 | **ADR-087 v1.1 待办登记**：D4 护栏形态随 TLM 退役切换（`static_assert` 见 Change 1/2 分工 + R4 窗口期）+ CH_MEM 侧 CI grep 门禁接替 | 本提案起草（显式登记行） |

## 验收标准

> 同 `## Acceptance` 清单（下节）；中英文标题并存 —— 「验收标准」为 rdd-workflow 5 段 HARD-GATE 口径，`Acceptance` 为 OpenSpec / rdd-verifier 口径，内容相同。

## Acceptance

- [ ] `tools/check_tlm_freeze.sh` 存在且可执行；**人为在任一 TLM-only 文件新增一个函数 → 脚本返回非 0 并报出 `文件:行`**
- [ ] 冻结门禁已接入 `.github/workflows/architecture-gates.yml`（warn 级）；CI 日志可见其输出
- [ ] 4 个死代码文件已删除；`grep -rn "exception.h\|plugins/fpu.h\|arch/riscv/fpu.h\|cache_keys.h" ip/ src/ tests/ tools/` 返回 0 includer 命中
- [ ] 双 binary 编译通过（`chipforge_tests` + `chipforge_tests_chmem`）；全量测试 0 退化（`[cpu]` 129 · `[riscv-tests]` 48 · `[chmem]` 130 · `[mmu]` 59 · `[cache]` 25 全 PASS）
- [ ] `docs/architecture/adr/ADR-088-tlm-backend-retirement.md` 存在，含 **5 项必填内容各 ≥1 节**（尤其第 5 项 ADR-087 v1.1 待办的显式登记行）；已注册进 `docs/architecture/adr.md`
- [ ] ADR-088 §语义裁决表 5 个子系统**全部有表头与已知分歧记录**（正式表态留 B2，但不得留空）
- [ ] ADR-083 出 v1.1 修订，含"冻结条款 + 退役时间线"一节
- [ ] ADR-087 §D4 护栏形态随 TLM 退役切换的问题，已在 ADR-088 中登记为待办（需 architect 出 ADR-087 v1.1）
- [ ] TLM-only 业务文件数从 **44 → 40**（实测口径：`ip/` 下非 `_chmem.h` 的 TLM-only 文件清点）
- [ ] 5 个架构门禁全 PASS：`verify_adr.sh` 0 fail · `verify_plugin_decision.sh` 8/8 · `check_plugin_portability.sh` 13/13 · `doc_link_check.sh` 0 broken · `roadmap-r9-check.sh` 0 critical
- [ ] 覆盖平价台账格式已在 ADR-088 中定义，且姊妹提案 `chmem-tlm-coverage-parity-ledger` 已立项（内容交付可后置，**格式**不得后置）

## Capabilities

- **TLM 冻结契约（可执行，非口头）**：把"TLM 是 legacy"从文档定性变成 CI 门禁 —— 新 feature 一律只写 CH_MEM 版，从机制上阻止第 6 处双写分叉
- **死代码归零**：4 文件 / 201 行消除；连带消除 `reg_file.cpp` 那类"TLM-only 文件里混用 `uint64_t` 显式实例化"的定时炸弹模式
- **ADR-088 治理锚**：B3 删除动作的语义裁决依据 + 覆盖平价台账格式 + 删除判据 —— **判据先于动作**
- **覆盖平价台账格式**：451 个 TLM 独占 case 的三态标注规范 + "批量退役"检测规则 —— 使"删 TLM 后回归不可见"这一风险从**不可控**变为**可审计**
- **双后端分叉可见化**：ADR-088 §语义裁决表把 5 处已存在的同 ISA 不同语义显式登记 —— 从"散落在注释里的自认"升级为"有裁决记录的架构台账"

## Impact

### 受影响文件

| 文件 | 动作 | 影响 |
|------|------|------|
| `tools/check_tlm_freeze.sh` | **新** | ~80 行；新增 1 个 CI step |
| `.github/workflows/architecture-gates.yml` | 改 | +1 step（warn） |
| `docs/architecture/adr/ADR-088-tlm-backend-retirement.md` | **新** | ADR 草案，含 5 项必填内容（含 ADR-087 v1.1 待办登记行） |
| `docs/architecture/adr/ADR-083-plugin-style-single-source-of-truth.md` | 改 | v1.1 修订 |
| `docs/architecture/adr.md` | 改 | 注册 ADR-088 |
| `ip/cpu/plugins/exception.h` | **删** | 56 行死代码 |
| `ip/cpu/plugins/fpu.h` | **删** | 41 行死代码 |
| `ip/cpu/arch/riscv/fpu.h` | **删** | 42 行死代码（独立 0 includer） |
| `ip/cache/plugins/cache_keys.h` | **删** | 62 行死代码 |
| `.rddf/roadmap/features/feat-tlm-backend-retirement.md` | 改 | B0 段勾选状态 |

### 风险

- **R1（冻结门禁误伤 legit bugfix）**：信号 = 白名单条目快速累积。缓解 = 白名单必须附「CH_MEM 是否需同步」裁决行；累计 >5 条触发 ADR-088 复审
- **R2（ADR-088 起草范围膨胀）**：信号 = 起草超过 2 天。缓解 = 只写 5 项必填裁决表，**逐子系统的正式表态留 B2**；语义细节（可综合除法器设计等）不属 ADR-088
- **R3（CI 门禁增加维护负担）**：信号 = 开发者频繁要求白名单。缓解 = 脚本 ~80 行、复用既有门禁脚本模式；先 warn 后 hard，B1 完成前不阻塞
- **R4（死代码删除误判）**：信号 = 编译失败。缓解 = 双重验证（0 includer + 双 binary 编译）；编译失败是**即时信号**，实际损失极低
- **R5（用户期望与本提案的落差）**：用户要求"移除老的不能生成 Verilog 的版本"，但本提案**只冻结 + 删死代码**，不删业务 TLM 代码。缓解：本提案在 §范围 明确写出 B3 的删除路径与 Gate 3 判据 —— **终态是移除，只是路径必须先补承载力**（否则两条性能硬门禁 + 93.7% 覆盖同时消失）
- **回退**：3 个独立 commit（门禁 / 死代码 / ADR-088），可分别 revert；死代码删除是纯删除，`git revert` 即恢复

### 下游依赖

- `feat-tlm-backend-retirement` —— 本提案是 B0 阶段；B1（承载力补齐）/ B2（语义裁决）/ B3（删除）在路线图 feature 中定义
- `objective-ip-cpu-tlm-backend-retirement` —— Gate 1 判据 G1.1–G1.4 对应本提案的 Acceptance 前 4 项
- `feat-bp-btb-gshare` —— 其 phase-5 段已改绑 CH_MEM-only；ADR-088 §语义裁决表需对该项表态
- `feat-dhrystone-dmips-gate` —— 其 DMIPS 判据的载体在 TLM；ADR-088 §B3 删除判据须显式列入"Dhrystone harness 迁移完成"为硬前置
- `chmem-tlm-coverage-parity-ledger`（姊妹提案）—— 台账**内容**由其交付，本提案只定义格式
- `rv64-isa-declaration-correction`（同批次提案）—— 两者都属 phase-4 B0，**文件不重叠**，可并行

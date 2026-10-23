---
initiative: wave6-linux-and-productization
priority: P0
version_target: v0.11.0-prep
status: ready
# P0 立项 (2026-10-23, rdd-builder case-1 approve, auto-decision per ADR-0049):
#   advisory=complex (planner) + LLM=complex + AC=11 → 完整 P1-P3。
#   上游 = ADR-083 (Plugin-style 单一 source of truth) + improvement 5 段 (commit 2617fb2)。
#   本 change = feat-tlm-backend-retirement 的 B0 阶段（冻结 + 死代码清理 + ADR-088 起草）。
#   B1 承载力 / B2 语义裁决 / B3 删除 均不在本 change。
created_from: .rddf/improvements/chmem-tlm-backend-freeze.md
adr:
  - ADR-083
  - ADR-040
  - ADR-046
---

# chmem-tlm-backend-freeze

## Why

**ADR-083 只给了定性，没给终态。** ADR-083（Accepted 2026-10-08）已定性：CppHDL 是 Plugin-style 的底层设施，CppTLM 是 legacy 仿真内核，仅 `src/cf_plugin/bridge/` 依赖。但它**没有冻结条款、没有退役时间线、没有删除判据** —— 于是仓库实际状态是「双后端并行且语义漂移」，而没有任何机制阻止漂移继续扩大。

**双写漂移已经在产生可测量的语义差异。** 同一 ISA 在两条路径上不是同一件事：M 扩展（TLM radix-2 `MUL=455c/MULH=504c/DIV=380c` vs CH_MEM 组合式 `mul=472c/div=94c`，差 ~286 cycles 且**不同构**）；CSR（TLM 侧 `csr.h` 是纯 stub vs CH_MEM 566 行实装）；MMU PTW（TLM 完整 walk vs CH_MEM 回退 vaddr）；BP（TLM 319 行 vs CH_MEM 符号数 = 0）；MMU exception handler（CH_MEM 版存在但**未注册进 pipeline**，`cpu_factory_chmem.h:342-349` 被 `#ifndef` 包裹）。

**删除 TLM 之前必须先把治理锚立起来。** B3 删除的判据不能是「看着像 TLM 就删」——`ip/cpu/tlm/cpu_keys.h`、`ip/mmu/tlm/mmu_keys.h`、`ip/mmu/lib/ptw.h`、`decoder_table.h`、`payload_riscv.h`、`payload_common.h`、`memory_interface.h`、`mul.h`、`mul_div_fsm.h` 这 9 个文件**名字/目录像 TLM，实际被 CH_MEM 侧 include**，误删即打断 CH_MEM 编译 ⇒ 删除动作必须走 include 闭包 + ADR-088 的显式判据。

**时间盒不该被挤占。** B0 的总成本约 2d（冻结门禁脚本 + 死代码清理 + ADR-088 起草），不侵占 v0.11.0 launch 的 CPU/MMU 主线。

### 架构依据（实测，HEAD `10e717c`）

| 位置 | 实测事实 |
|------|---------|
| `ip/` 源文件 | **72** 文件 / 11,825 行（含 4 文件 / 201 行死代码；有效 68 / 11,624） |
| **TLM-only（include 闭包不可达）** | **45 文件 / 5,400 行（46%）** —— 见下方「草案数字纠正」 |
| `src/cf_plugin/bridge/` | 8 文件 / 727 行，**100%** 直 include CppTLM 头；**CH_MEM 侧 0 消费者**（`tests/CMakeLists.txt:22-25` 只编入 TLM target） |
| 测试分布 | TLM binary 481 case vs CH_MEM binary 130 case；**TLM 独占 451**（`[cpu-integration]` 96 / `[riscv-tests]` 48 / `[framework]` 94 / `[soc]` 20 / `[bundles]` 9 无 CH_MEM 替代） |
| 性能硬门禁载体 | Dhrystone DMIPS/MHz ≥1.4（`tests/cpu/integration/test_dhrystone.cpp` 硬绑 `CpuFactory<uint32_t>`）与 CoreMark ≥1.9（PoC-6）载体 **100% 在 TLM** |
| CI 盲区 | `.github/workflows/architecture-gates.yml` 只跑架构门禁脚本，**测试不进 CI**（Oracle 2026-10-06 决策）⇒ 删 451 case 后无自动回归发现机制 |
| 链接依赖 | `chipforge_tests_chmem`（`tests/CMakeLists.txt:183`）与 `cpu_verilator_sim`（`tools/verilator_runner/CMakeLists.txt:47`）**仍链接 `libcpptlm_core.a`** ⇒ 删 TLM 代码 ≠ 立刻脱 CppTLM |
| 双后端编译开关 | `CF_PLUGIN_USE_CH_MEM` 是 **per-target 编译定义**（`tests/CMakeLists.txt:166` + `tools/verilator_runner/CMakeLists.txt:35`），无全局 `option()` |

> **草案数字纠正（实测，2026-10-23）**：improvement 草案记「44 文件 / 5,331 行 TLM-only」与「10 个双模共享文件」。实测 `ip/cpu/cpu_factory_chmem.h:54-56` 把 `ip/cpu/plugins/mmu_exception_handler.h` 包在 `#ifndef CF_PLUGIN_USE_CH_MEM` 内 ⇒ CH_MEM 模式**根本不编译**它（其 includer 仅 TLM 工厂 `ip/cpu/cpu_factory.h:48` 与 TLM-only 测试 `tests/cpu/test_mmu_exception_handler.cpp:21`）⇒ 它是 **TLM-only**，不是双模共享。故：双模共享 = **9 个**，TLM-only = **45 文件 / 5,400 行**。本 proposal 与 AC 采用纠正后数字；草案原文保留作为历史。

## What Changes

### In Scope（5 项）

1. **冻结门禁**（新）`tools/check_tlm_freeze.sh`（~80 行）—— 检测对象 = `ip/**/plugins/*.h` + `ip/**/arch/**/*.h` 中**非 `_chmem.h`** 文件；检测内容 = **相对 baseline 快照的新增函数定义**；白名单条目必须写 `文件:行` + 原因 + **「CH_MEM 是否需同步」裁决行**；新增未登记函数 ⇒ **脚本返回非 0 并报出 `文件:行`**；累计白名单 >5 条 ⇒ 触发 ADR-088 复审
2. **死代码清理**（4 文件 / 201 行，0 includer 已复核）—— `ip/cpu/plugins/exception.h`(56) · `ip/cpu/plugins/fpu.h`(41) · `ip/cpu/arch/riscv/fpu.h`(42) · `ip/cache/plugins/cache_keys.h`(62)。注：`plugins/fpu.h` 与 `arch/riscv/fpu.h` **互不引用**，各自独立为死代码（非传递链）
3. **ADR-088 起草**（新）`docs/architecture/adr/ADR-088-tlm-backend-retirement.md` —— 5 项必填：① 冻结条款 ② 逐子系统语义裁决表（M 扩展 / CSR / MMU PTW / BP / MMU exception handler）③ 覆盖平价台账格式（三态标注）④ B3 删除判据（**按「CH_MEM binary 包含闭包反查」，不用路径名**）⑤ ADR-087 v1.1 待办登记（D4 护栏形态随 TLM 退役切换 —— TLM 侧 `static_assert` 会随 TLM 删除消失，须切 CH_MEM 侧 CI grep 门禁）
4. **ADR-083 v1.1 修订** —— 补「冻结条款 + 退役时间线」一节（ADR-083 §5.2/§5.3 只给了定性 + CI 验证，缺终态）
5. **覆盖平价台账的「格式」定义**（内容由姊妹提案 `chmem-tlm-coverage-parity-ledger` 交付，**格式不得后置**）

### Out of Scope（9 项）

- ❌ **不删任何 TLM-only 业务代码**（45 → 41 只删 4 个死代码；其余归 B3，由 Gate 3 放行）
- ❌ 不删 `src/cf_plugin/bridge/`（8 文件 / 727 行；`openspec/specs/mmu-cpptlm-bridge/` + `openspec/specs/bridge-tick-pattern-rationale/` 两个 spec 的退役归 B3）
- ❌ 不删 `tools/cpu_sim`
- ❌ 不删「目录名带 `tlm/` 但双模共享」的 9 个文件
- ❌ 不改判据规避门禁
- ❌ 不退役 `[framework]` 94 语义锚
- ❌ 不做 ADR-040 v3.0
- ❌ 不改 `ip/{mmu,cpu}/tlm/` 目录结构（命名治理归 ADR-083 v1.1 的后续）
- ❌ 不建 B1/B2/B3 的 change（本 change 仅 B0）

### 变更清单（7 个文件：3 改 + 3 新增 + 1 删）

| 文件 | 动作 | 具体 |
|------|------|------|
| `tools/check_tlm_freeze.sh` | **新增** | 冻结门禁（~80 行；baseline 快照 + 新增函数检测 + 白名单校验） |
| `docs/architecture/adr/ADR-088-tlm-backend-retirement.md` | **新增** | 5 项必填（冻结条款 / 语义裁决表 / 台账格式 / B3 判据 / ADR-087 v1.1 待办） |
| `ip/cpu/plugins/exception.h` | 删 | 死代码（`ExceptionPlugin`，0 includer） |
| `ip/cpu/plugins/fpu.h` | 删 | 死代码（`FPUPlugin`，0 includer） |
| `ip/cpu/arch/riscv/fpu.h` | 删 | 死代码（`RiscvFpuPlugin`，0 includer，与上者互不引用） |
| `ip/cache/plugins/cache_keys.h` | 删 | 死代码（0 includer；CH_MEM 消费点尚未落地，见 CHANGELOG.md:34） |
| `docs/architecture/adr/ADR-083-plugin-style-single-source-of-truth.md` | 改 | v1.1：补「冻结条款 + 退役时间线」 |
| `docs/architecture/adr.md` | 改 | 注册 ADR-088 |
| `.github/workflows/architecture-gates.yml` | 改 | 接入冻结门禁（`continue-on-error: true` = warn 级） |

## Capabilities

- **TLM 冻结契约（可执行，非口头）**：新增 TLM-only 代码必须显式登记并回答「CH_MEM 是否需同步」——把「双写漂移」从架构偏好变成**有门禁的契约**
- **死代码归零**：清掉 4 文件 / 201 行 0-includer 资产（`plugins/fpu.h` 与 `arch/riscv/fpu.h` 是两条独立死链，非传递链）
- **ADR-088 治理锚（判据先于动作）**：B3 删除前，判据、语义裁决表、台账格式、ADR-087 切换待办全部就位 —— **先立判据，后删代码**
- **覆盖平价台账格式**：让「删 TLM 后回归不可见」从不可控变为可审计（三态标注格式）
- **双后端分叉可见化**：5 个子系统的已知语义分歧被显式记录（正式表态留 B2，但不得留空）

## Impact

### 受影响文件

| 文件 | 动作 | 影响 |
|------|------|------|
| `tools/check_tlm_freeze.sh` | 新增 | 冻结门禁（CI warn 级接入） |
| `docs/architecture/adr/ADR-088-tlm-backend-retirement.md` | 新增 | 治理锚（5 项必填） |
| `ip/cpu/plugins/exception.h` | 删 | 56 行死代码 |
| `ip/cpu/plugins/fpu.h` | 删 | 41 行死代码 |
| `ip/cpu/arch/riscv/fpu.h` | 删 | 42 行死代码 |
| `ip/cache/plugins/cache_keys.h` | 删 | 62 行死代码 |
| `docs/architecture/adr/ADR-083-*.md` | 改 | v1.1（冻结条款 + 退役时间线） |
| `docs/architecture/adr.md` | 改 | ADR-088 注册 |
| `.github/workflows/architecture-gates.yml` | 改 | 门禁接入 |

### 风险

1. **R1（冻结门禁误伤 legit bugfix）**：在 TLM-only 文件里修 bug 会被判为「新增函数」⇒ 缓解：白名单支持 `文件:行` 粒度登记 + 豁免机制（只针对**新增函数定义**，不改现有函数体）
2. **R2（ADR-088 起草范围膨胀）**：5 项必填易滚成大文档 ⇒ 缓解：每项必填 ≥1 节即可，**不得**在本 change 内做 B2 的正式语义表态
3. **R3（CI 门禁维护负担）**：冻结门禁需长期维护 baseline 快照 ⇒ 缓解：warn 级接入（`continue-on-error: true`），不阻塞
4. **R4（死代码删除误判）**：4 个文件虽 0 includer，但可能通过「同名类 / 文档引用 / 后续计划」间接依赖 ⇒ 缓解：删除前跑 `grep -rn` 全仓（含 docs/openspec）+ 双 binary 全量 build
5. **R5（用户期望落差）**：用户期望「路线图里只留 CH_MEM」，但 B0 **不删任何业务代码** ⇒ 缓解：change 名称与 `feat-tlm-backend-retirement` 的 B0–B3 编排显式对齐，B1/B2/B3 各自独立立项

### 回退

3 个独立 commit（门禁脚本 / 死代码 / ADR-088）⇒ 任一可单独 `git revert`

## Acceptance

- [ ] `tools/check_tlm_freeze.sh` 存在且可执行；**人为在任一 TLM-only 文件新增一个函数 → 脚本返回非 0 并报出 `文件:行`**
- [ ] 冻结门禁已接入 `.github/workflows/architecture-gates.yml`（**CI 层 warn 级** = `continue-on-error: true`，不阻塞；**脚本层对违规仍返回非 0** —— 两者是不同层次，不矛盾）；CI 日志可见其输出
- [ ] 4 个死代码文件已删除；`grep -rn "exception.h\|plugins/fpu.h\|arch/riscv/fpu.h\|cache_keys.h" ip/ src/ tests/ tools/` 返回 0 includer 命中
- [ ] 双 binary 编译通过（`chipforge_tests` + `chipforge_tests_chmem`）；全量测试 0 退化（`[cpu]` 129 · `[riscv-tests]` 48 · `[mmu]` 59 · `[cache]` 25 · chmem binary 130 全 PASS）
- [ ] `docs/architecture/adr/ADR-088-tlm-backend-retirement.md` 存在，含 **5 项必填内容各 ≥1 节**（尤其第 5 项 ADR-087 v1.1 待办的显式登记行）；已注册进 `docs/architecture/adr.md`
- [ ] ADR-088 §语义裁决表 5 个子系统**全部有表头与已知分歧记录**（正式表态留 B2，但不得留空）
- [ ] ADR-083 出 v1.1 修订，含「冻结条款 + 退役时间线」一节
- [ ] ADR-087 §D4 护栏形态随 TLM 退役切换的问题，已在 ADR-088 中登记为待办（需 architect 出 ADR-087 v1.1）
- [ ] TLM-only 业务文件数 **45 → 41**（实测口径：`ip/` 下非 `_chmem.h` 的 TLM-only 文件 include 闭包清点；**修正草案的 44 → 40**，依据见 §Why 的「草案数字纠正」）
- [ ] 5 个架构门禁全 PASS：`verify_adr.sh` 0 fail · `verify_plugin_decision.sh` 8/8 · `check_plugin_portability.sh` 13/13 · `doc_link_check.sh` 0 broken · `roadmap-r9-check.sh` 0 critical
- [ ] 覆盖平价台账格式已在 ADR-088 中定义，且姊妹提案 `chmem-tlm-coverage-parity-ledger` 已立项（内容交付可后置，**格式**不得后置）

## References

- ADR-083: `docs/architecture/adr/ADR-083-plugin-style-single-source-of-truth.md`（§2.3 CppTLM 角色 = legacy · §4.3 拒绝「TLM 模式作为长期模式」· §5.2/§5.3 缺终态）
- ADR-040 v3.0: `docs/architecture/adr/ADR-040-tlm-hdl-portability-constraints.md`（§10.1 双后端语义必须一致 · §10.2 差异必须显式契约化）
- ADR-046: 多周期协议引擎豁免（CH_MEM 全组合通路的 FSM 承载路径）
- ADR-087: `docs/architecture/adr/ADR-087-isa-declaration-integrity-and-rv64-readiness.md`（§D4 护栏形态随 TLM 退役切换 → ADR-088 第 5 项待办）
- 路线图载体: `.rddf/roadmap/features/feat-tlm-backend-retirement.md`（B0–B3 编排 + 13 条验收）· `.rddf/roadmap/objectives/objective-ip-cpu-tlm-backend-retirement.md`（G1/G2/G3 门禁）
- 姊妹提案: `.rddf/improvements/chmem-tlm-coverage-parity-ledger.md`（P1，交付台账**内容**）
- Improvement 5 段: `.rddf/improvements/chmem-tlm-backend-freeze.md`

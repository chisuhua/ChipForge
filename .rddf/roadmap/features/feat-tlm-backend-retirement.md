---
id: feat-tlm-backend-retirement
kind: feature
status: active
scope: cross
phase_refs: [phase-4, phase-5]
主题: TLM 后端退役 — CH_MEM 单后端收敛 (冻结 → 承载力 → 语义裁决 → 删除)
---

## 概述

**目标**：ChipForge 只保留 CH_MEM 单一仿真/实现后端（可生成 Verilog → Verilator），退役 TLM（C++ 直仿）后端。

**当前状态（2026-10-23 实测）**：项目已把 CH_MEM 定为"新正道"（ADR-040 v2.0），但仓库仍以 TLM 为主干 —— 双后端不仅重复，而且**已经产生同 ISA 不同语义**：

| 子系统 | TLM 侧 | CH_MEM 侧 | 后果 |
|--------|--------|-----------|------|
| M 扩展 | 真 radix-2 FSM（MUL=455c / MULH=504c / DIV=380c，`ip/cpu/arch/riscv/mul_div_fsm.h:207-274`） | **组合式**（M-op 1 拍，mul=472c / div=94c，`ip/cpu/arch/riscv/int_alu_chmem.h:259-410`） | 同一 ISA 两条 cycle 语义，`AGENTS.md`「CH_MEM 模式状态」段（`[chmem][m-ext-gap]` 条目）自认「不同构、不可比」 |
| CSR | **纯 stub**（`ip/cpu/arch/riscv/csr.h:26-36` setup/build 全空） | 真实实现 566 行（`ip/cpu/plugins/csr_chmem.h`） | TLM 路径根本没跑过 CSR |
| MMU PTW | 完整 walk（`ip/mmu/lib/ptw.cpp`） | **回退 vaddr**（`ip/mmu/plugins/MMUPlugin_chmem.h:149-150,224-225`） | CH_MEM 侧分页未实装 |
| BRANCH predictor | BTB/GShare/RAS 319 行（`ip/cpu/plugins/branch_predictor.h`） | **无**（CH_MEM binary 符号数 = 0） | PoC-6 CoreMark 在 CH_MEM 无被测对象 |
| MMU exception handler | 已实装 | 存在但**未注册进 pipeline**（`ip/cpu/cpu_factory_chmem.h:55` 在 `#ifndef CF_PLUGIN_USE_CH_MEM` 内） | CH_MEM 侧 trap 路径不完整 |

⇒ **「双后端」没有兑现 ADR-083 的"单一 source of truth"，反而固化了"两条 ISA 语义"。**

### 遗留面规模（实测）

- `ip/` 共 **72** 源文件 / 11,825 行（含 4 文件 / 201 行死代码；**有效 68 文件 / 11,624 行**）；**44 文件 / 5,331 行（46%）只有 TLM 版本，无 CH_MEM 对应**
- `src/cf_plugin/bridge/` 8 文件 / 727 行（100% 直接 include CppTLM 头）
- **10 文件 / 1,731 行是"目录名带 `tlm/` 但实际双模共享"** —— 误删即打断 CH_MEM。**7 个已点名**（合计 1,080 行）：`ip/mmu/tlm/mmu_keys.h`（96 行，被 `MMUPlugin_chmem.h:70` include）、`ip/cpu/tlm/cpu_keys.h`（73 行，mode-aware `uint_t<kBits>`）、`ip/mmu/lib/ptw.h`（149 行，`MMUPlugin_chmem.h:69` vestigial include）、`decoder_table.h`（430 行）、`payload_riscv.h`（88 行）、`payload_common.h`（181 行）、`memory_interface.h`（63 行）；**其余 3 个（651 行）构成未定**，B0 目录治理时以 include 图复核为准 —— **删除动作禁止按目录名/文件名推断，必须走 include 闭包**
- **4 文件 / 201 行是死代码**（0 includer）：`ip/cpu/plugins/exception.h`、`ip/cpu/plugins/fpu.h`、`ip/cpu/arch/riscv/fpu.h`、`ip/cache/plugins/cache_keys.h`

### 测试与门禁损失面（实测 `--list-tests` 逐条比对）

- TLM binary `chipforge_tests` **481 case**；CH_MEM binary `chipforge_tests_chmem` **130 case**；共享 30
- **TLM 独占 451 case（93.7%）**：其中 `[cpu-integration]` 96、`[riscv-tests]` 48、`[framework]` 94、`[cpu]` 129、`[mmu]` 59、`[cache]` 25、`[soc]` 20、`[bundles]` 9、`[mul-div-fsm]` 9、`[picolibc]` 7、`[cpu-l1-mmu-demo]` 6、`[mmu-exception-handler]` 4、`[dhrystone]` 3
- **两条性能硬门禁载体 100% 在 TLM**：

| 门禁 | 载体 | CH_MEM 替代 |
|------|------|-------------|
| **Dhrystone DMIPS/MHz ≥1.4**（v0.11.0 launch gate 判据 #3） | `tests/cpu/integration/test_dhrystone.cpp:18,70` 硬绑 `CpuFactory<uint32_t>` | ❌ 无。需在 `cpu_verilator_sim` 重建 harness + **重测基线**（组合式 M-op cycle 语义不同，1.43 不可迁移） |
| **CoreMark/MHz ≥1.9**（PoC-6，v1.0.0 窗口） | `ip/cpu/plugins/branch_predictor.h`（CH_MEM 符号数 = 0） | ❌ 无。**被测对象本身不存在** ⇒ BP 实施目标须改绑 CH_MEM-only |

### CI 盲区（放大风险）

Oracle 2026-10-06 决策：**测试不进 CI**（`.github/workflows/architecture-gates.yml` 只跑 3 个架构门禁脚本）。⇒ 删掉 451 个 case 后**没有任何自动机制能发现语义退化**。

## 跨阶段拆分

### phase-4（v0.11.0 窗口）— B0 冻结 + 安全子集清理（零删除风险）

- **B0 冻结**：ADR 声明 TLM 进入 maintenance-only —— 只修 launch-gate 阻塞 bug，**新 feature 一律只写 CH_MEM 版**。落地为 CI 门禁脚本：新增 `ip/**/plugins/*.h`（非 `_chmem.h`）中的 feature 函数即 fail（bugfix 白名单豁免）
- **策略 C 安全子集清理**：删 4 个死代码文件（201 行），判据 = 0 includer + 非公开 API + 双 binary 编译验证
- **ADR-088 起草**：《TLM 仿真后端退役与 CH_MEM 单后端收敛》—— 承载冻结条款、语义裁决表、性能门禁重基线决策、覆盖平价台账、删除判据
- **目录去误导**（非删除）：`ip/mmu/tlm/` + `ip/cpu/tlm/` 下的双模共享文件迁到 `core/` 命名空间，消除"目录名 = 后端绑定"的误导
- **覆盖平价台账**建立：451 个 TLM 独占 case 逐条标注三态 —— 「已移植 CH_MEM」/「由 `[verilator]` ELF 级覆盖」/「显式退役（ADR-088 记录理由）」；`[framework]` 94 个框架语义锚**禁止退役，只允许移植**

### phase-5（v1.0.0+）— B1 承载力 → B2 语义裁决 → B3 删除

- **B1 承载力补齐**（CH_MEM 必须先能承载 TLM 现在承担的职责）：
  1. Dhrystone harness 移植到 `cpu_verilator_sim --elf` 路径 + 基线 CSV 重建
  2. LSU/DBus 全宽度（LB/LH/LW/LBU/LHU + SB/SH/SW）+ LD/SD（RV64 前置）
  3. MMU PTW walk FSM 实装（走 ADR-046 豁免；`ctrl-link-chmem-reg-handle` 已解除 `halt_when_stateful` 前置）
  4. MMU exception handler CH_MEM 注册进 pipeline（解 `cpu_factory_chmem.h:55` 的 `#ifndef` 包裹）
  5. BP（BTB/GShare/RAS）CH_MEM 版 —— **替代** `feat-bp-btb-gshare` 的 TLM 实施计划（避免双份实现）
  6. Hazard / RegFile / IBUS / DBus / Branch / Mul 的 CH_MEM 对应实现补齐
- **B2 语义裁决**（ADR-088 §语义裁决表，逐子系统表态）：
  | 子系统 | 待裁决 | 建议裁决 |
  |--------|--------|---------|
  | M 扩展 | TLM radix-2（多周期）vs CH_MEM 组合式（1 拍） | 功能=组合式为规范；**cycle=待 FSM 版收敛**（`chmem-mext-fsm-cycle-parity`），列 v1.0.0 完成条件 |
  | CSR | TLM stub vs CH_MEM 实装 | CH_MEM 实装为规范（TLM 无实装可保留） |
  | MMU PTW | TLM 完整 walk vs CH_MEM 回退 | CH_MEM PTW FSM 为规范（ADR-085/086 已按 Sv39/Sv48 规划） |
  | BP | TLM 有 vs CH_MEM 无 | CH_MEM-only（改绑 `feat-bp-btb-gshare` phase-5 段） |
- **B3 删除**（按反向依赖序）：
  1. 脱 CppTLM 链接：`tests/CMakeLists.txt:183` + `tools/verilator_runner/CMakeLists.txt` 的 `cf_plugin_link_cpptlm(...)` 移除
  2. 删 `tools/cpu_sim`（纯 TLM CLI）+ `src/cf_plugin/bridge/` 8 文件 + `bundles/tlb_bundles_tlm.hh`
  3. 删 44 个 TLM-only 业务文件
  4. 根 `CMakeLists.txt:126` 的 `ExternalProject_Add(cpptlm_external ...)` 下线
  5. `tools/run_chipforge_tests.sh` 默认入口改指 CH_MEM binary

## 验收标准

- [ ] **B0**：冻结门禁脚本落地（CI 可执行，TLM-only 文件新增 feature 函数即 fail）
- [ ] **B0**：4 个死代码文件删除，双 binary 编译 + 全量测试 0 退化
- [ ] **B0**：ADR-088 Proposed（含冻结条款 + 语义裁决表 + 覆盖平价台账格式 + 删除判据）
- [ ] **B0**：`ip/mmu/tlm/` + `ip/cpu/tlm/` 目录名误导消除（双模共享文件迁 `core/`，CH_MEM 编译 0 退化）
- [ ] **B0**：覆盖平价台账建立 —— 451 个 TLM 独占 case 三态标注 100% 覆盖，无批量"显式退役"
- [ ] **B1**：Dhrystone 在 `cpu_verilator_sim` 跑通 + 新基线 CSV 入库（**B3 的硬前置**）
- [ ] **B1**：CH_MEM 侧 LSU 全宽度 + MMU PTW walk FSM + MMU exception handler 注册 + BP CH_MEM 版 全部落地
- [ ] **B2**：ADR-088 §语义裁决表 5 个子系统逐条表态，M 扩展 cycle 裁决明确
- [ ] **B3**：CppTLM 链接依赖归零（`nm -C build/bin/chipforge_tests_chmem | grep -c cpptlm` = 0）
- [ ] **B3**：TLM-only 业务文件归零（`find ip/ -name "*.h" -not -name "*_chmem.h"` 逐个确认无 TLM-only 残留）
- [ ] **B3**：全量 ctest 4 entries PASS + 5 个架构门禁脚本全 PASS
- [ ] **B3**：`feat-dhrystone-dmips-gate` 的 DMIPS 判据改绑 Verilator 后端并**重测基线**（不删判据，废弃需显式 ADR）

## 关联

- **ADR-083**（Plugin-style 单一 source of truth）— 本 feature 是其"CppTLM = legacy"定性的执行计划；建议 B0 阶段出 v1.1 修订补"冻结条款 + 退役时间线"
- **ADR-040 v2.0**（TLM→HDL 移植性约束）— "CH_MEM 是新正道"支持本方向；B3 完成后需 v3.0 移除 TLM 分支语义（远期）
- **ADR-046**（多周期 FSM 豁免）— **强依赖**：B1 的 PTW walk FSM / L1Cache refill FSM / M 扩展 FSM 版全走此豁免，收敛决策反而提高其使用密度
- **ADR-087**（ISA 声明真实性与 RV64 就绪度门禁，Proposed）— **正交互补但有交叉点**：其"假 RV64 就绪"证据全在 TLM 侧；TLM 删除后 D4 的 `static_assert` 护栏对象消失，护栏须改为 **CH_MEM 侧 CI grep 门禁 + G1–G9 准入清单**（详见 `feat-rv64-isa-declaration-integrity`）
- **ADR-085 / ADR-086**（MMU PTW / L0-L1 TLB 架构）— MMU 侧已按 Sv39/Sv48 规划，CPU 侧 CH_MEM PTW 仍 sv32 ⇒ B1 承载力补齐的直接输入
- **ADR-049**（MMU paddr 消费契约）— MMU CH_MEM 化沿用
- `feat-bp-btb-gshare` — 其 phase-5 段实施目标改绑 CH_MEM-only（B1 第 5 项）
- `feat-dhrystone-dmips-gate` — 其 DMIPS 判据的后端绑定在 B2 改判 + 重基线
- `objective-ip-cpu-tlm-backend-retirement` — 本 feature 的 objective 载体（Go/No-Go gate 在此）

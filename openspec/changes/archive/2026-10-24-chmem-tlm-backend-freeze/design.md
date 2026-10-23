# Design — chmem-tlm-backend-freeze

> 上游：ADR-083（Plugin-style 单一 source of truth）· ADR-040 v3.0（§10.1/§10.2 双后端语义）· improvement 5 段 `.rddf/improvements/chmem-tlm-backend-freeze.md` · proposal.md。
> 本 change = `feat-tlm-backend-retirement` 的 **B0 阶段**（冻结 + 死代码清理 + ADR-088 起草）。B1 承载力 / B2 语义裁决 / B3 删除**均不在本 change**。

## Decision 1 — 冻结门禁的检测面（路径模式 − 共享排除清单）

ADR-083 定了性（CppTLM = legacy），但没给终态。本 change 的门禁要把「不得继续长 TLM-only 业务代码」变成**可执行契约**。

**检测面** = `ip/**/*.h` 中**非 `_chmem.h`** 的文件（实测 44 个候选），**减去** 9 个「目录/文件名像 TLM 但被 CH_MEM 侧 include」的双模共享文件：

> ⚠️ 草案原定义（「`ip/**/plugins/*.h` + `ip/**/arch/**/*.h`」）**漏掉了 `ip/mmu/{tlm,lib,policies}` 与 `ip/cache/policies` 的 TLM-only 面**（实测候选数 18 vs 44）⇒ 本版扩到 `ip/` 全域的 `.h`（`_chmem.h` 除外）。

| # | 双模共享文件（行数） | CH_MEM 侧消费点（实测） |
|---|---------------------|----------------------|
| 1 | `ip/cpu/tlm/cpu_keys.h`（73） | `ip/cpu/plugins/mmu.cpp:27` · `mmu_exception_handler.h:18` · `tests/cpu/test_cpu_chmem_keys_modality.cpp:62` |
| 2 | `ip/mmu/tlm/mmu_keys.h`（96） | `ip/mmu/plugins/MMUPlugin_chmem.h:70` · `ip/cpu/plugins/dmem_chmem.h:24` |
| 3 | `ip/mmu/lib/ptw.h`（149） | `ip/mmu/plugins/MMUPlugin_chmem.h:69` |
| 4 | `ip/cpu/arch/riscv/decoder_table.h`（430） | `ip/cpu/cpu_factory_chmem.h:41` · `int_alu_chmem.h:72` |
| 5 | `ip/cpu/arch/riscv/payload_riscv.h`（88） | `ip/cpu/arch/riscv/int_alu_chmem.h:73` |
| 6 | `ip/cpu/core/payload_common.h`（181） | `int_alu_chmem.h:74` · `ip/cpu/plugins/csr_chmem.h:44` |
| 7 | `ip/mmu/lib/memory_interface.h`（63） | `ip/mmu/lib/ptw.h:21` ← `MMUPlugin_chmem.h:69` |
| 8 | `ip/cpu/arch/riscv/mul.h`（157） | `ip/cpu/arch/riscv/mul_div_fsm.h:54` ← `ip/cpu/plugins/mul_div_fsm_chmem.h:38` |
| 9 | `ip/cpu/arch/riscv/mul_div_fsm.h`（424） | `ip/cpu/plugins/mul_div_fsm_chmem.h:38` |

> ⚠️ **共享清单是实测 include 闭包的产物，不得按路径名推断**（草案 `:48` 的原话）。清单本身登记在 ADR-088 §B3 判据里，可被任何人复核。
> ⚠️ **第 10 个候选已被实测排除**：`ip/cpu/plugins/mmu_exception_handler.h`（69 行）曾被列入「未定」候选，但 `ip/cpu/cpu_factory_chmem.h:54-56` 把它包在 `#ifndef CF_PLUGIN_USE_CH_MEM` 内 ⇒ **CH_MEM 模式不编译它** ⇒ 它是 **TLM-only**（且其 includer 只有 TLM 工厂与 TLM-only 测试）⇒ 不在共享清单里。

**为什么不维护「TLM-only 文件全清单」**：文件会随 PR 增删，清单必然腐化；「模式面 − 共享排除清单」是**结构性**判定，新增文件自动落入检测面，只有真被 CH_MEM include 的文件才需显式豁免（豁免动作本身要写进 ADR-088）。

## Decision 2 — 冻结门禁的判定机制（新增函数定义差集）

**检测内容** = TLM-only 文件中**相对 baseline 新增的函数定义**。

| 要素 | 决策 |
|------|------|
| baseline 锚点 | `.rddf/state/tlm-freeze-base.txt` 存一个 commit sha（脚本引入时的 HEAD）。文件缺失 ⇒ 脚本输出「未初始化 baseline，本次跳过检测」并 **exit 0**（首次引入不阻塞） |
| 函数定义识别 | 行首匹配 `static|inline|virtual|template` 或「返回类型 + 标识符 + `(`」的行；**排除**关键字 `if/for/while/switch/catch/return/sizeof` 与 `//`/`*` 注释行 |
| 差集比较 | 对每个候选文件：`git show <base>:<file>` 的函数签名集合 vs 当前文件的集合 ⇒ 差集 = 新增 |
| 白名单 | `.rddf/state/tlm-freeze-allowlist.txt`，每行 3 段：`文件:行 | 原因 | CH_MEM 是否需同步（是/否/不适用）`（格式与 ADR-088 §台账格式 统一） |
| 违规行为 | 新增函数定义**不在白名单** ⇒ 脚本**返回非 0** 并报出 `文件:行` + 登记提示 |
| 累计阈值 | 白名单行数 > 5 ⇒ 输出「需 ADR-088 复审」提示（不自动阻断；阻断与否由架构裁决） |
| CI 层 | `continue-on-error: true`（warn 级，不阻塞 PR）—— **脚本层与 CI 层是两层语义**：脚本对违规仍返回非 0 |

**为什么只拦「新增函数」而不拦「修改函数体」**：TLM-only 路径的**存量 bugfix 是合法需求**（B3 删除前的最后修复窗口）⇒ 拦新增函数定义（结构性扩张），不拦存量修改（行为修正）。这是冻结而非清零。

## Decision 3 — 死代码清理（4 文件 / 201 行）

`ip/cpu/plugins/exception.h`（56）· `ip/cpu/plugins/fpu.h`（41）· `ip/cpu/arch/riscv/fpu.h`（42）· `ip/cache/plugins/cache_keys.h`（62）。

- 实测 **0 includer**（`#include` 模式命中 0；仅 4 处文档提及，属预期）
- `plugins/fpu.h` 与 `arch/riscv/fpu.h` **互不引用**（`FPUPlugin` vs `RiscvFpuPlugin`）⇒ 两条**独立**死链，非传递链
- 删除后双 binary 全量 build + 测试 0 退化

## Decision 4 — ADR-088 的五项必填（小节骨架固定，内容不得留空）

| # | 必填 | 小节骨架 |
|---|------|---------|
| ① | 冻结条款 | §2 冻结范围（检测面 + 判定机制，与本 design D1/D2 一致）· §2.1 冻结期内的合法动作（存量 bugfix / 共享文件豁免登记）· §2.2 解冻条件 |
| ② | 逐子系统语义裁决表 | §3 表：5 个子系统（M 扩展 / CSR / MMU PTW / BP / MMU exception handler）× 列 `TLM 侧实现` / `CH_MEM 侧实现` / `已知分歧` / `B2 正式表态（留空待填）` —— **表头与已知分歧必须填满，正式表态留 B2** |
| ③ | 覆盖平价台账格式 | §4 三态标注格式定义（`covered` / `partial` / `not-covered` + 每态的必填字段 + 451 case 口径注记）；**内容由姊妹提案 `chmem-tlm-coverage-parity-ledger` 交付** |
| ④ | B3 删除判据 | §5「按 CH_MEM binary 包含闭包反查」+ 禁止按路径名/文件名推断 + 删除前的 3 道前置（覆盖平价 / 语义裁决 / CI 可回归） |
| ⑤ | ADR-087 v1.1 待办登记 | §6 显式登记行：TLM 侧 `static_assert` 随 TLM 删除消失 ⇒ D4 护栏须切 CH_MEM 侧 CI grep 门禁（**已由 `rv64-isa-declaration-correction` 落地 TLM 侧护栏**，B0 只需登记切换待办） |

## Decision 5 — ADR-083 v1.1 修订（补终态）

ADR-083 §5.2「代码无需修改」+ §5.3「CI 验证」只给了**定性**。v1.1 补两节：

- **§2.5 冻结条款**：TLM-only 业务代码进入冻结（引用 ADR-088 §2）；新增须登记白名单
- **§5.4 退役时间线**：B0 冻结（本 change）→ B1 承载力 → B2 语义裁决 → B3 删除（Gate 3 放行）→ 与路线图 `feat-tlm-backend-retirement` 对齐

## Decision 6 — 覆盖平价台账格式（B0 交付格式，不交付内容）

三态标注：`covered`（CH_MEM 有等价覆盖 + 回归锚）/ `partial`（有实现但语义不等价或无回归锚）/ `not-covered`（CH_MEM 无对应实现）。每态必填：子系统、TLM 侧锚点、CH_MEM 侧锚点、回归锚点、语义差异说明。

**口径注记**：13 族求和 = 509 ≠ 451（case 级差集以两个 binary 的 `--list-tests` 差集为唯一口径）—— 格式定义须写明该口径，避免沿用 509。

## Risks

| # | 风险 | 缓解 |
|---|------|------|
| R1 | 冻结门禁误伤 legit bugfix | 只拦**新增函数定义**，不拦存量函数体修改；白名单支持 `文件:行` 粒度登记 |
| R2 | ADR-088 起草范围膨胀 | 每项必填 ≥1 节即可；**禁止**在 B0 做 B2 的正式语义表态（表头 + 已知分歧即可） |
| R3 | CI 门禁维护负担 | warn 级接入（不阻塞）；baseline 锚点用固定 sha，豁免清单只增不减（新增需写进 ADR-088） |
| R4 | 死代码删除误判 | 删除前跑 `#include` 口径的 0 命中 + 双 binary 全量 build + 全量测试 |
| R5 | 用户期望落差（以为 B0 会删业务代码） | change 名称与 `feat-tlm-backend-retirement` 的 B0–B3 编排显式对齐；proposal 顶部写明「B0 不删任何业务代码」 |
| R6 | baseline 锚点与分支漂移 | 锚点用固定 sha（不是 `main`）⇒ 切分支不影响判定；换锚点须显式改文件并在 ADR-088 记录 |
| R7 | 函数定义识别误报（宏、多行签名、模板特化） | warn 级门禁 + 误报登记白名单一行即可豁免；**不追求零误报**（宁可多报，不可漏报） |
| R8 | 与 `rv64-isa-declaration-correction` 的顺序耦合 | A（护栏实装）须先落地：B 的 ADR-088 §6 登记的是 A 的实装形态；且 A 在 `ip/cpu/cpu_factory.h` 新增的 `validate_isa_xlen` 会被本门禁视为「新增函数」⇒ **A 落地后 B 的 baseline 才采集**（否则假阳性） |

## 决策摘要（给 implementer 的一句话版）

**先固化判定口径（共享清单 + baseline 锚点 + 白名单格式），再写门禁脚本，再删死代码，最后写 ADR-088 与 ADR-083 v1.1 —— 顺序不可颠倒**（判据先于动作，ADR-088 的判据必须引用门禁脚本的实际行为，而不是反过来）。

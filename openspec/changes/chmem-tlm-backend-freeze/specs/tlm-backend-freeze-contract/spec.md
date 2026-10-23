## ADDED Requirements

> Auto-derived from `.rddf/improvements/chmem-tlm-backend-freeze.md` per ADR-0025 D1/D2 + rdd-builder Phase 0 approve (2026-10-23), scenario blocks per openspec v1.4. Capability topic: `tlm-backend-freeze-contract`（上游 ADR-083 §2.3/§4.3/§5.2 + ADR-040 v3.0 §10.1/§10.2）。

### Requirement: tlm-backend-freeze-contract

TLM-only 业务代码 MUST 处于冻结状态：向 `ip/**/plugins/*.h` 或 `ip/**/arch/**/*.h` 中**非 `_chmem.h`** 的文件新增函数定义时，冻结门禁 MUST 报出 `文件:行` 并返回非 0；每个获准的新增 MUST 登记白名单条目，且条目 MUST 同时包含「`文件:行`」「原因」「CH_MEM 是否需同步」三项。门禁 MUST 以 include 闭包为口径判定「双模共享」，MUST NOT 按目录名或文件名推断。

#### Scenario: 未登记的新增函数被门禁拒绝

- **WHEN** 在任一 TLM-only 文件（如 `ip/cpu/arch/riscv/int_alu.h`）新增一个函数定义
- **THEN** `tools/check_tlm_freeze.sh` 返回非 0，输出 `文件:行` 与登记提示（不留白名单则不放行）

#### Scenario: 获准新增带完整白名单条目

- **WHEN** 新增函数已登记白名单，条目含 `文件:行` + 原因 + 「CH_MEM 是否需同步」裁决行
- **THEN** 门禁返回 0；条目缺任一项 ⇒ 门禁视为未登记（返回非 0）

#### Scenario: 双模共享文件不被误判

- **WHEN** 对 9 个双模共享文件（`ip/cpu/tlm/cpu_keys.h` · `ip/mmu/tlm/mmu_keys.h` · `ip/mmu/lib/ptw.h` · `ip/cpu/arch/riscv/decoder_table.h` · `payload_riscv.h` · `ip/cpu/core/payload_common.h` · `ip/mmu/lib/memory_interface.h` · `ip/cpu/arch/riscv/mul.h` · `mul_div_fsm.h`）运行门禁
- **THEN** 不报「TLM-only 违规」；判定依据为 include 闭包，路径名相似不影响结论

#### Scenario: 白名单累积触发复审

- **WHEN** 白名单登记条目累计 >5 条
- **THEN** 门禁输出「需 ADR-088 复审」提示（不自动阻断，由流程处置）

### Requirement: tlm-deadcode-purge

4 个 0-includer 死代码文件 MUST 被删除：`ip/cpu/plugins/exception.h` · `ip/cpu/plugins/fpu.h` · `ip/cpu/arch/riscv/fpu.h` · `ip/cache/plugins/cache_keys.h`。删除 MUST NOT 触发任何 includer 解析失败，MUST NOT 改变任一 binary 的行为。

#### Scenario: 死代码删除后零引用且零退化

- **WHEN** 删除 4 个文件并全量 build 双 binary
- **THEN** `grep -rn "exception.h\|plugins/fpu.h\|arch/riscv/fpu.h\|cache_keys.h" ip/ src/ tests/ tools/` 返回 0 includer 命中；`chipforge_tests` / `chipforge_tests_chmem` 构建成功且全量测试 0 退化

#### Scenario: 两条独立死链分别删除

- **WHEN** 分别删除 `ip/cpu/plugins/fpu.h` 与 `ip/cpu/arch/riscv/fpu.h`
- **THEN** 两者互不引用（`FPUPlugin` 与 `RiscvFpuPlugin` 无 include 关系）⇒ 各自独立删除，无传递依赖

### Requirement: tlm-retirement-adr-088

`docs/architecture/adr/ADR-088-tlm-backend-retirement.md` MUST 存在并注册进 `docs/architecture/adr.md`，且 MUST 至少含 5 节：① 冻结条款 ② 逐子系统语义裁决表 ③ 覆盖平价台账格式 ④ B3 删除判据 ⑤ ADR-087 v1.1 待办登记。ADR-083 MUST 出 v1.1 修订，含「冻结条款 + 退役时间线」一节。

#### Scenario: ADR-088 五项必填齐备

- **WHEN** 检查 ADR-088 的章节结构
- **THEN** 5 项必填各有 ≥1 节；第 5 项含 ADR-087 v1.1 待办的**显式登记行**（TLM 侧 `static_assert` 随 TLM 删除消失 ⇒ 须切 CH_MEM 侧 CI grep 门禁）

#### Scenario: 语义裁决表 5 个子系统不留空

- **WHEN** 检查 ADR-088 §语义裁决表
- **THEN** M 扩展 / CSR / MMU PTW / BP / MMU exception handler 5 行**全部有表头与已知分歧记录**（正式表态留 B2，但不得留空）

#### Scenario: B3 删除判据用闭包不用路径名

- **WHEN** 检查 ADR-088 §B3 删除判据
- **THEN** 判据表述为「按 CH_MEM binary 包含闭包反查」，MUST NOT 以目录名/文件名为判据

### Requirement: tlm-coverage-parity-ledger-format

覆盖平价台账的**格式** MUST 在 ADR-088 中定义并采用三态标注；内容交付由姊妹提案 `chmem-tlm-coverage-parity-ledger` 承担，格式 MUST NOT 随内容后置。

#### Scenario: 台账格式先行定义

- **WHEN** 检查 ADR-088 中的台账格式定义
- **THEN** 三态标注格式存在且自洽；姊妹提案 `chmem-tlm-coverage-parity-ledger` 已立项（内容可后置）

### Requirement: tlm-freeze-gate-ci-integration

冻结门禁 MUST 接入 `.github/workflows/architecture-gates.yml`，CI 层 MUST 为 warn 级（`continue-on-error: true`，不阻塞 PR），CI 日志 MUST 可见门禁输出。门禁**脚本层**对违规仍 MUST 返回非 0 —— 脚本退出码与 CI 阻塞级别是两层不同语义。

#### Scenario: CI 日志可见且不阻塞

- **WHEN** 在 PR 上运行 architecture-gates workflow
- **THEN** 日志中出现 `check_tlm_freeze.sh` 的输出；该步骤失败**不阻塞** PR（`continue-on-error: true`）

#### Scenario: 脚本退出码语义不被 CI 层稀释

- **WHEN** 门禁检测到未登记的新增函数
- **THEN** 脚本自身退出码非 0（即便 CI 步骤被标记为 tolerated，脚本语义不变）

---
change: chmem-tlm-backend-freeze
priority: P0
initiative: wave6-linux-and-productization
adr:
  - ADR-083
  - ADR-040
  - ADR-046
plan: .rddf/plans/chmem-tlm-backend-freeze.md
status: in-planning
---

# Tasks — chmem-tlm-backend-freeze

> 实施细节在 plan（TDD 5-step）；本文件是 OpenSpec 跟踪台账。execute 阶段逐 Task 勾选。
> **本 change = `feat-tlm-backend-retirement` 的 B0 阶段**（冻结 + 死代码清理 + ADR-088 起草）。**不删任何 TLM-only 业务代码**（B3 的事，Gate 3 放行）。

## Phase 0 备忘

- P0 决策：case-1 approve（auto-decision ADR-0049：advisory=complex + LLM=complex + AC=11 → 完整 P1–P3）
- **前置条件**：`rv64-isa-declaration-correction` 必须先合并（A 的 TLM 侧护栏是 ADR-088 §6 登记的实体；且 A 在 `ip/cpu/cpu_factory.h` 新增的 `validate_isa_xlen` 会被本门禁判为「新增函数」⇒ **A 落地后才采集 baseline**，否则假阳性）
- 数字纠正（实测，2026-10-23）：`ip/cpu/plugins/mmu_exception_handler.h` 在 `cpu_factory_chmem.h:54-56` 被 `#ifndef CF_PLUGIN_USE_CH_MEM` 包裹 ⇒ **TLM-only**（非双模共享）⇒ 双模共享 = **9** 个；草案的「10 个共享 / 44 TLM-only 文件」口径已更正（proposal §Why 有完整说明）
- 判定口径三条（design.md D1/D2）：检测面 = 模式面 − 9 共享文件；判定 = 相对 baseline 的**新增函数定义**差集；违规 = 未登记白名单 ⇒ 脚本 exit 1（CI 层 warn 级）

## Implementation

- [ ] Task 1 基线快照 + 前置条件核实（A 已合并 / 双 binary 计数 / 4 死代码 0-includer 复核）
- [ ] Task 2 固化判定口径三件套：`.rddf/state/tlm-freeze-base.txt`（固定 sha 锚点）+ `tlm-shared-files.txt`（9 条共享清单）+ `tlm-freeze-allowlist.txt`（`文件:行 | 原因 | CH_MEM 是否需同步`）
- [ ] Task 3 冻结门禁脚本 `tools/check_tlm_freeze.sh`（检测面 + baseline 差集 + 白名单校验 + 共享排除 + >5 条复审提示；baseline 缺失 ⇒ 跳过 exit 0）
- [ ] Task 4 门禁变异对照自证（新增函数 ⇒ exit 1 + 报 `文件:行`；登记白名单 ⇒ 0；还原后复跑 0）
- [ ] Task 5 CI 接入（`architecture-gates.yml` step，warn 级 `continue-on-error: true`）+ `tools/README.md` 登记 + yaml 校验
- [ ] Task 6 删除 4 个死代码（`ip/cpu/plugins/exception.h` / `plugins/fpu.h` / `arch/riscv/fpu.h` / `ip/cache/plugins/cache_keys.h`）+ 双 binary build + 0 退化 + 门禁复跑
- [ ] Task 7 起草 ADR-088（§2 冻结条款 3 小节 / §3 语义裁决表 5 子系统表头+已知分歧填满、正式表态留 B2 / §4 台账三态格式 + 509≠451 口径注记 / §5 B3 删除判据（闭包反查，禁路径名推断）/ §6 ADR-087 v1.1 待办**显式登记行** / §7–§9 影响与验证记录）
- [ ] Task 8 ADR-083 v1.1（§2.5 冻结条款 + §5.4 退役时间线 + 头部版本标记）+ `docs/architecture/adr.md` 注册 ADR-088 + 修订历史 1.6 行 + `verify_adr` / `doc_link_check` 自检
- [ ] Task 9 全门禁收口：7 门禁（6 架构 + 冻结门禁）+ AC 逐条机械复核 + 双 binary 全量回归 0 退化 + `openspec change validate` + **分 3 批 commit**（批 2 的已删路径**不带 `git add`**）

## Acceptance

- [ ] AC1 `tools/check_tlm_freeze.sh` 存在且可执行；**人为在任一 TLM-only 文件新增一个函数 → 脚本返回非 0 并报出 `文件:行`**
- [ ] AC2 冻结门禁已接入 `.github/workflows/architecture-gates.yml`（**CI 层 warn 级** = `continue-on-error: true`；**脚本层对违规仍返回非 0**）；CI 日志可见其输出
- [ ] AC3 4 个死代码文件已删除；`grep -rn "#include.*\(exception\|fpu\|cache_keys\)\.h" ip/ src/ tests/ tools/` 返回 0 includer 命中
- [ ] AC4 双 binary 编译通过（`chipforge_tests` + `chipforge_tests_chmem`）；全量测试 0 退化（TLM entry 仍恰 1 个 pre-existing FAIL：`test_7stage_riscv.cpp:124`）
- [ ] AC5 `docs/architecture/adr/ADR-088-tlm-backend-retirement.md` 存在，含 **5 项必填内容各 ≥1 节**（尤其第 5 项 ADR-087 v1.1 待办的显式登记行）；已注册进 `docs/architecture/adr.md`
- [ ] AC6 ADR-088 §语义裁决表 5 个子系统**全部有表头与已知分歧记录**（正式表态留 B2，但不得留空）
- [ ] AC7 ADR-083 出 v1.1 修订，含「冻结条款 + 退役时间线」一节（§2.5 + §5.4）
- [ ] AC8 ADR-087 §D4 护栏形态随 TLM 退役切换的问题，已在 ADR-088 中登记为待办（需 architect 出 ADR-087 v1.1）
- [ ] AC9 TLM-only 业务文件数从 **45 → 41**（实测口径：`ip/` 下非 `_chmem.h` 的 TLM-only 文件 include 闭包清点；**修正草案的 44 → 40**，依据见 proposal §Why 的「草案数字纠正」）
- [ ] AC10 5 个架构门禁全 PASS：`verify_adr.sh` 0 fail · `verify_plugin_decision.sh` 8/8 · `check_plugin_portability.sh` 13/13 · `doc_link_check.sh` 0 broken · `roadmap-r9-check.sh` 0 critical
- [ ] AC11 覆盖平价台账格式已在 ADR-088 中定义（三态标注 + 必填字段 + 509≠451 口径注记），且姊妹提案 `chmem-tlm-coverage-parity-ledger` 已立项（内容交付可后置，**格式**不得后置）

---
initiative: wave6-linux-and-productization
priority: P1
version_target: v0.11.0-prep
status: ready
# P0 立项 (2026-10-24, rdd-builder case-1 approve, auto-decision per ADR-0049):
#   advisory=complex (planner) + LLM=complex + AC=11 → 完整 P1-P3。
#   上游 = ADR-083 + ADR-088 §4 (台账格式, 由 chmem-tlm-backend-freeze B0 交付)
#        + improvement 5 段 (commit 2617fb2)。
#   本 change 交付 ADR-088 §4 的台账**内容** (格式已交付, 内容不得后置)。
created_from: .rddf/improvements/chmem-tlm-coverage-parity-ledger.md
adr:
  - ADR-083
  - ADR-088
  - ADR-047
---

# chmem-tlm-coverage-parity-ledger

## Why

**要删掉的测试，必须先有台账说明它被谁接住了。** ADR-047（Result 范式）的核心是「配置错误要能被静态发现，而不是运行期崩」；同一逻辑适用于测试覆盖 —— TLM 独占的 case 在 TLM 删除后会**静默消失**，没有任何机制告诉开发者「这个行为现在没人测了」。

**规模实测（2026-10-24，HEAD `913c20d`）**：

| 口径 | 实测 | 闭合校验 |
|------|------|---------|
| TLM binary 总 case | **493** | — |
| CH_MEM binary 总 case | **130** | — |
| 两者**共有** | **30** | — |
| **TLM 独占** | **463** | 463 + 30 = **493** ✅ |
| CH_MEM 独有 | **100** | 100 + 30 = **130** ✅ |

> ⚠️ **数字纠正（实测 vs 草案）**：草案记「TLM 独占 451 / TLM 总 481」。`rv64-isa-declaration-correction`（A，已归档）新增 13 个 TLM-only case（5 契约 + 3 helper 直调 + 1 cpu_sim e2e + 4 schema）并删除 1 个（`build_cpu_rv64`）⇒ **451 + 13 − 1 = 463**，TLM 总数 481 + 13 − 1 = **493**。本 change 采用实测值。

**TLM 独占 case 的族分布**（实测；同一 case 可带多 tag ⇒ tag 求和 > case 数，**不是**台账口径）：

| 族 | TLM 独占 case 数 | 备注 |
|----|----------------|------|
| `[cpu]` | 137 | 含 A 新增的 9 个 `[isa-xlen]` |
| `[cpu-integration]` | 96 | 含 1 个 pre-existing FAIL（`test_7stage_riscv.cpp:124`） |
| `[framework]` | 94 | **全部 TLM 独占**（另有 30 条共有 case 也带 framework tag） |
| `[mmu]` | 59 | |
| `[riscv-tests]` | 48 | rv32ui 40 + rv32um 8 |
| `[cache]` | 25 | |
| `[soc]` | 20 | |
| `[stall]` | 19 | |
| `[cpu-configs]` | 13 | |
| `[MMUPlugin]` / `[MultiLevelTLB]` / `[TLBUnit]` | 12 / 8 / 8 | |
| `[bundles]` | 9 | |
| `[mul-div-fsm]` | 9 | |
| `[rv32um]` | 8 | |
| `[RiscV]` / `[picolibc]` | 7 / 7 | |
| 其余 | 余量 | 合计 **463**（去重差集口径） |

**台账格式已就绪**：`chmem-tlm-backend-freeze`（B0，已归档）交付了 ADR-088 §4 的三态格式（`covered` / `partial` / `not-covered` + 必填字段 + 509≠451 口径注记）。**格式不得后置，内容现在交付**。

## What Changes

### In Scope（5 项）

1. **台账文件** `docs/architecture/tlm-coverage-parity-ledger.md`（新建）—— 覆盖**全部 463 个 TLM 独占 case**，逐条三态标注（格式依 ADR-088 §4）
2. **生成脚本** `tools/gen_coverage_parity_ledger.sh`（新建）—— 从两个 binary 的 `--list-tests` 重新生成台账骨架 + 族分布 + 闭合校验（`463 + 30 = 493` / `100 + 30 = 130`）
3. **校验脚本** `tools/check_coverage_parity_ledger.sh`（新建）—— 检测「新增 TLM-only case 未登记」等漂移；接入 CI（warn 级）
4. **B1 承载力待办清单**（台账内一节）—— 把 `covered`/`partial` 条目按 plugin 归组，映射到 `feat-tlm-backend-retirement` 的 B1 项
5. **状态措辞映射表**（台账内一节）—— 把草案的「已移植 CH_MEM」/「由 ELF 级覆盖」/「显式退役」映射到 ADR-088 §4 的三态（**ADR-088 为权威**）

### Out of Scope（7 项）

- ❌ **不改任何生产代码**（`.h` / `.cpp` 改动数 = 0）—— 本 change 只加文档 + 2 脚本 + CI step
- ❌ **不删任何测试**（删除归 B3）
- ❌ **不改 ADR-088 §4 的三态定义**（格式已定稿；本 change 只交付内容 + 措辞映射）
- ❌ **不做 B2 语义裁决**（ADR-088 §3 的正式表态留 B2）
- ❌ **不补 CH_MEM 侧能力**（承载力归 B1）
- ❌ **不迁移 `ip/mmu/tlm/` / `ip/cpu/tlm/` 目录名**（归 B1）
- ❌ **不新建 OpenSpec change 之外的任何工作流产物**（本 change 即为该工作流产物）

### 变更清单（5 个文件：1 文档 + 2 脚本 + 1 CI + 1 台账内清单）

| 文件 | 动作 | 具体 |
|------|------|------|
| `docs/architecture/tlm-coverage-parity-ledger.md` | **新增** | 463 条三态标注 + 族分布 + 闭合校验 + 措辞映射 + B1 待办清单 |
| `tools/gen_coverage_parity_ledger.sh` | **新增** | 从两个 binary 的 `--list-tests` 生成骨架 + 校验闭合（可重跑，防漂移） |
| `tools/check_coverage_parity_ledger.sh` | **新增** | 漂移检测：新增 TLM-only case 未登记 / 标注缺失 / 批量退役阈值 |
| `.github/workflows/architecture-gates.yml` | 改 | 接入 check 脚本（warn 级） |
| `tools/README.md` | 改 | 登记 2 个新脚本 |

## Capabilities

- **覆盖平价可审计**：每个 TLM 独占 case 都有明确归属（谁接住了 / 为什么不接），删除 TLM 时不再有「静默消失」的行为
- **漂移可检测**：新增 TLM-only case 未登记 ⇒ CI 报出（warn 级），保证台账随代码演进保持完整
- **B1 承载力有据可依**：`covered`/`partial` 条目按 plugin 归组 ⇒ 直接转成 B1 的补齐清单（避免 B1 靠猜）
- **批量退役红线**：`not-covered` 条目数 ≤135（30%）且同一理由不得 >10 次 ⇒ 防止「一次性显式退役大量测试」绕过评审

## Impact

### 风险

1. **R1（标注工作量）**：463 条逐条标注 = 大工作量 ⇒ 缓解：**半自动生成骨架**（脚本按族给默认态）+ 人工/AI 复核高风险族（`[cpu-integration]` 96 / `[riscv-tests]` 48 / `[mmu]` 59）
2. **R2（标注失真）**：默认态可能与实际覆盖不符 ⇒ 缓解：默认态只给「同族多数派」，并对每个族抽查 ≥3 条；`partial`/`not-covered` 必须填差异说明/理由（机检）
3. **R3（台账漂移）**：代码演进后 case 增减 ⇒ 缓解：`check` 脚本检测「新增未登记」；`gen` 脚本可重生成骨架做 diff
4. **R4（数字漂移）**：两个 binary 的 case 数会变（别的 change 也在动）⇒ 缓解：闭合校验在脚本里重算，不写死数字；台账头部标注「基线 commit」
5. **R5（误用为「已覆盖」背书）**：台账说 `covered` 不代表行为等价 ⇒ 缓解：`covered` 仍**必须**填差异说明（ADR-088 §4 要求），`partial` 显式列出差异性质

## Acceptance

- [ ] 台账文件存在，覆盖**全部 463 个 TLM 独占 case**（`--list-tests` 差集 100% 标注，无遗漏）
- [ ] 闭合校验（两个口径各自闭合）：**463 + 30 = 493**（TLM binary 总数）；**100 + 30 = 130**（CH_MEM binary 总数）；台账覆盖 = 463（**去重差集口径**，非分族求和）
- [ ] `[framework]` 94 条 **100% 标为 `covered`**，无一条 `not-covered`
- [ ] `covered` / `partial` 条目 **100% 填写差异说明**（不得只写「已覆盖」）
- [ ] `not-covered` 条目 **100% 有 ≥20 字理由 + ADR-088 裁决引用**
- [ ] 批量退役检测：同一理由 >10 次 = **0 条**；`not-covered` 总数 ≤135（30%）
- [ ] `tools/gen_coverage_parity_ledger.sh` 可从两个 binary 的 `--list-tests` 重新生成骨架，**重跑结果与台账条目集一致**（防漂移）
- [ ] `tools/check_coverage_parity_ledger.sh` 已接入 CI（warn 级），能检出「新增 TLM-only case 未登记」这一漂移
- [ ] 台账中 `covered`/`partial` 条目汇总成 **B1 承载力补齐待办清单**（按 plugin 归组，标注对应的 B1 项）
- [ ] 5 个架构门禁全 PASS（`verify_adr.sh` / `verify_plugin_decision.sh` / `check_plugin_portability.sh` / `doc_link_check.sh` / `roadmap-r9-check.sh`）
- [ ] **0 生产代码改动** —— 只新增文档 + 2 个脚本 + CI step，`git diff --stat` 中 `.h` / `.cpp` 改动数 = 0

## References

- ADR-088 §4（台账三态格式 + 必填字段 + 509≠451 口径注记）· ADR-088 §3（5 子系统语义裁决表）
- ADR-083（Plugin-style 单一 source of truth）· ADR-047（Result 范式：配置错误要静态可发现）
- 路线图：`.rddf/roadmap/features/feat-tlm-backend-retirement.md`（B0 段 + G1.4 判据）· `.rddf/roadmap/objectives/objective-ip-cpu-tlm-backend-retirement.md`
- 姊妹 change（已归档）：`openspec/changes/archive/2026-10-24-chmem-tlm-backend-freeze/`（B0：冻结门禁 + ADR-088）
- Improvement 5 段：`.rddf/improvements/chmem-tlm-coverage-parity-ledger.md`

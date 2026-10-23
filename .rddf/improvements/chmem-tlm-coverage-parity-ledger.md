---
id: chmem-tlm-coverage-parity-ledger
name: chmem-tlm-coverage-parity-ledger
status: draft
initiative: wave6-linux-and-productization
priority: P1
phase: phase-4
category: core-impl
type: feature
manual_deps: [chmem-tlm-backend-freeze]
review_by: 2026-11-06
主题: TLM 后端退役 — CH_MEM 单后端收敛 (冻结 → 承载力 → 语义裁决 → 删除)
依赖: ADR-047, ADR-083
来源: 用户指令「路线图中我只需要 chmem 的版本，老的不能生成 verilog 后端代码的版本我希望移除」+ Oracle 回归可见性风险 R2
生成时间: 2026-10-23
---

# chmem-tlm-coverage-parity-ledger

**主题**: TLM 后端退役 — CH_MEM 单后端收敛 (冻结 → 承载力 → 语义裁决 → 删除)

> 交付 `chmem-tlm-backend-freeze` 定义的**覆盖平价台账内容**（格式由前者定义）。
> 路线图载体：[`feat-tlm-backend-retirement`](../../.rddf/roadmap/features/feat-tlm-backend-retirement.md) 的 B0 段；objective = [`objective-ip-cpu-tlm-backend-retirement`](../../.rddf/roadmap/objectives/objective-ip-cpu-tlm-backend-retirement.md) Gate 1 判据 **G1.4**。

## 架构依据

ADR-047（Result 范式）的核心是"**配置错误要能被静态发现，而不是运行期崩**"。同一逻辑适用于测试覆盖：**要删掉的测试，必须先有台账说明它被谁接住了**。

当前风险（Oracle R2，高severity）：

> **CI 不跑测试**（Oracle 2026-10-06 决策，`.github/workflows/architecture-gates.yml` 只跑架构门禁脚本）⇒ 删掉 451 个 TLM 独占 case 后，**没有任何自动机制能发现语义退化**。

实测规模（`--list-tests` 逐条比对）：

| | TLM binary `chipforge_tests` | CH_MEM binary `chipforge_tests_chmem` |
|---|---|---|
| 注册 case | **481** | **130** |
| 共享 | 30 | 30 |
| 独占 | **451（93.7%）** | 100 |

451 个 TLM 独占 case 的分布（**口径注记**：下表按 tag 逐族计数，多 tag case（如 `[cpu-integration][dhrystone]`）同时计入多族 ⇒ **13 族求和 = 509 ≠ 451**；台账验证一律以两个 binary 的 `--list-tests` 差集（去重后 451）为唯一口径，本表仅用于分族导航）：

| 测试族 | TLM 独占 case | CH_MEM 是否有替代 | 替代形态 |
|--------|-------------|------------------|---------|
| `[cpu-integration]` | 96 | ❌ 无 | 3/5/7/10-stage 全 pipeline + Dhrystone + rv32ui/um runner，**唯一载体** |
| `[cpu]` | 129 | 部分 | CH_MEM 56 case 覆盖 ALU/CSR/decoder/M-op 子集；**regfile / hazard / ibus / dbus / decode / lsu / mul / predictor 7 个 plugin 无 CH_MEM 测试** |
| `[framework]` | 94 | ❌ 无 | pipe_builder / pipe_node / payload / ctrl_link / storage / negotiate —— **双模共享框架的语义锚** |
| `[mmu]` | 59 | 部分 | CH_MEM 14 = `[mmu-chmem-basic]` 7 + `[keys]` 4 + `[mmu-verilator]` 3；**CH_MEM 只做 TLB-hit，无 PTW walk** |
| `[riscv-tests]` | 48 | ⚠️ 间接 | `[verilator]` 1 case 跑同 48 ELF 判 `tohost==1`（**功能等价，不判 cycle**） |
| `[cache]` | 25 | 部分 | CH_MEM 3 = `l1cache_refill_fsm_*`；**L1CachePlugin 本体无 CH_MEM 版** |
| `[soc]` | 20 | ❌ 无 | 4 个 JSON schema + 6 个 cpu-l1-mmu-demo |
| `[bundles]` | 9 | ❌ 无 | — |
| `[mul-div-fsm]` | 9 | ❌ 无 | `[multi-cycle]` 5 case 是 invariant 非 e2e |
| `[picolibc]` | 7 | ❌ 无 | — |
| `[cpu-l1-mmu-demo]` | 6 | ⚠️ 间接 | `[mmu-verilator]` 3 case 是 plumbing-only（`AGENTS.md`「CH_MEM 模式状态」段 `[mmu-verilator]` 条目自认 "translation semantics NOT verified"） |
| `[mmu-exception-handler]` | 4 | ❌ 无 | CH_MEM 版存在但未注册进 pipeline |
| `[dhrystone]` | 3 | ❌ 无 | 硬绑 `CpuFactory<uint32_t>`（`tests/cpu/integration/test_dhrystone.cpp:18,70`） |

## 范围

**In Scope:**

1. **覆盖平价台账**（新文档）—— 451 个 TLM 独占 case 逐条标注三态
2. **台账格式落地** —— 三态定义 + "批量退役"检测规则 + 退役理由必填字段（格式由 `chmem-tlm-backend-freeze` 的 ADR-088 定义）
3. **CLI 工具**（新）—— `tools/gen_coverage_parity_ledger.sh`：从两个 binary 的 `--list-tests` 输出自动生成台账骨架（case 名 + 归属 binary），人工只填三态 + 理由
4. **CI 软门禁** —— 校验台账条目数与实际 TLM 独占 case 数一致（防漂移）；校验"批量退役"检测规则

**Out of Scope:**

- **不做测试移植** —— 本提案只做台账（**盘点 + 标注**），移植是 B1 的工作
- **不做测试删除** —— B3 才做
- **不改任何测试代码** —— 本提案 0 代码改动（除新增 CLI 工具）
- **不改 `run_chipforge_tests.sh` 的默认入口** —— B3 才改
- **不推测试进 CI** —— 那是独立的架构决策（当前 Oracle 2026-10-06 决策：CI 只跑门禁），本提案只标注"该 case 当前无自动执行载体"这一风险事实
- **不评审退役理由的合理性** —— 逐条评审属 ADR-088 §语义裁决 + Gate 1 G1.4

## Why

**没有台账的删除是不可审计的。** 451 个 case 里，有些是**框架语义锚**（`[framework]` 94 个：pipe_builder / payload / ctrl_link / storage —— 双模共享框架的唯一行为锚），有些是**功能等价可替代**（`[riscv-tests]` 48 由 `[verilator]` 1 case 覆盖），有些是**真的无替代**（`[soc]` 20 / `[bundles]` 9 / `[picolibc]` 7 / `[dhrystone]` 3）。**这三种命运完全不同，但如果没有逐条台账，B3 删除时只能靠"看起来像重复"来判断** —— 而那正是把框架语义锚误删的路径。

**批量退役是最危险的失败形态。** 如果 451 条里有 200 条被标成"显式退役"且理由雷同（如"暂时不需要"），那不是省事，是**把 93.7% 的覆盖在一个下午蒸发掉**，且因为 CI 不跑测试，几个月内无人发现。台账的"批量退役"检测规则就是为了让这种形态在 Gate 1 就被拦下。

**台账同时是 B1 的工作清单。** 「已移植 CH_MEM」的那一批，就是 B1 承载力补齐的**具体待办列表** —— `[cpu]` 129 里 7 个 plugin 无 CH_MEM 测试，对应 B1 的 LSU/DBus/Hazard/RegFile/IBUS/Branch/Mul 承载件。没有台账，B1 只能靠"我记得缺哪些"。

**成本极低。** 纯文档 + 一个生成骨架的脚本，~1.5d，不动生产代码、不动测试代码，**零回归风险**。相比之下，"删完再发现丢了 200 个 case"无法挽回。

## What Changes

### 变更清单

| 文件 | 动作 | 具体 |
|------|------|------|
| `.rddf/roadmap/features/feat-tlm-backend-retirement.md`（或 ADR-088 附件） | **新** | 覆盖平价台账正文（451 行级） |
| `tools/gen_coverage_parity_ledger.sh` | **新** | 从两 binary 的 `--list-tests` 生成台账骨架 |
| `tools/check_coverage_parity_ledger.sh` | **新** | CI 软门禁：条目数一致 + 批量退役检测 |
| `.github/workflows/architecture-gates.yml` | 改 | +1 step（warn 级） |
| `docs/architecture/adr/ADR-088-tlm-backend-retirement.md` | 改 | 台账定稿后回填引用（由姊妹提案起草） |

### 台账三态定义（格式由 `chmem-tlm-backend-freeze` 的 ADR-088 定义）

| 态 | 含义 | 必填字段 | Gate 1 约束 |
|----|------|---------|------------|
| **已移植 CH_MEM** | 该 case 的语义断言已在 CH_MEM 路径有对应实现 | 目标 case 名 / 文件 | `[framework]` 94 条 **只允许此态** |
| **由 ELF 级覆盖** | 该 case 的功能语义由更高层 case 覆盖 | 覆盖方 case 名 + **差异说明**（如「不判 cycle」） | 必须写明差异，不得只写「已覆盖」 |
| **显式退役** | 无替代，确认为不需要 | **退役理由**（≥20 字）+ ADR-088 裁决引用 | 同一理由重复 >10 条 ⇒ **Gate 1 No-Go** |

### 「批量退役」检测规则

1. 同一退役理由字符串出现 **>10 次** ⇒ 触发 Gate 1 No-Go
2. 「显式退役」总数 **>30%**（即 >135 条）⇒ 触发 Gate 1 No-Go，须逐条复核
3. `[framework]` 族出现任何「显式退役」⇒ **直接 No-Go**（框架语义锚只允许移植）
4. 「由 ELF 级覆盖」若未填差异说明 ⇒ 视为未标注，Gate 1 校验失败

### 台账文件结构

```
# TLM → CH_MEM 覆盖平价台账

## 汇总
| 态 | 条数 | 占比 | Gate 1 约束 |

## 逐族台账
### [framework] (94)
| TLM case | 归属 binary | 三态 | 目标/覆盖方 | 理由 | 裁决引用 |

### [cpu] (129)
...
```

## 验收标准

> 同 `## Acceptance` 清单（下节）；中英文标题并存 —— 「验收标准」为 rdd-workflow 5 段 HARD-GATE 口径，`Acceptance` 为 OpenSpec / rdd-verifier 口径，内容相同。

## Acceptance

- [ ] 台账文件存在，覆盖 **全部 451 个 TLM 独占 case**（`--list-tests` 差集 100% 标注，无遗漏）
- [ ] 闭合校验（两个口径各自闭合）：**451 + 30 = 481**（TLM binary 总数）；**100 + 30 = 130**（CH_MEM binary 总数）；台账覆盖 = 451（去重差集口径，非分族求和 509）
- [ ] `[framework]` 94 条 **100% 标为「已移植 CH_MEM」**，无一条「显式退役」
- [ ] 「由 ELF 级覆盖」的条目 **100% 填写差异说明**（不得只写「已覆盖」）
- [ ] 「显式退役」条目 **100% 有 ≥20 字理由 + ADR-088 裁决引用**
- [ ] 批量退役检测：同一理由 >10 次 = **0 条**；「显式退役」总数 ≤135（30%）
- [ ] `tools/gen_coverage_parity_ledger.sh` 可从两 binary 的 `--list-tests` 重新生成骨架，**重跑结果与台账条目集一致**（防漂移）
- [ ] `tools/check_coverage_parity_ledger.sh` 已接入 CI（warn 级），能检出"新增 TLM-only case 未登记"这一漂移
- [ ] 台账中「已移植 CH_MEM」条目汇总成 **B1 承载力补齐待办清单**（按 plugin 归组，标注对应的 B1 项）
- [ ] 5 个架构门禁全 PASS（`verify_adr.sh` / `verify_plugin_decision.sh` / `check_plugin_portability.sh` / `doc_link_check.sh` / `roadmap-r9-check.sh`）
- [ ] **0 生产代码改动** —— 本提案只新增文档 + 2 个脚本 + CI step，`git diff --stat` 中 `.h` / `.cpp` 改动数 = 0

## Capabilities

- **覆盖平价可审计性**：451 个 TLM 独占 case 的去向**逐条可查** —— B3 删除不再是"看起来像重复"的判断，而是有裁决引用可追溯
- **批量退役防护**：把"一把梭退役 200 个 case"这一最危险的失败形态，在 Gate 1 就拦下（3 条量化规则）
- **框架语义锚保护**：`[framework]` 94 条强制「只允许移植」—— 防止双模共享框架的行为锚在删除中被误伤
- **B1 工作清单化**：「已移植 CH_MEM」条目 = B1 承载力补齐的精确待办（7 个 plugin 无 CH_MEM 测试 + MMU PTW + BP 等）
- **回归风险显式化**：每条「由 ELF 级覆盖」都记录**差异**（如 `[verilator]` 不判 cycle）—— 未来性能判据迁 Verilator 时，知道哪部分语义没有等价物

## Impact

### 受影响文件

| 文件 | 动作 | 影响 |
|------|------|------|
| 台账文件（路径由 ADR-088 定，建议 `docs/architecture/adr/ADR-088-coverage-parity-ledger.md`） | **新** | 451 行级表格 |
| `tools/gen_coverage_parity_ledger.sh` | **新** | ~60 行；读两 binary `--list-tests` 输出 |
| `tools/check_coverage_parity_ledger.sh` | **新** | ~80 行；4 条量化规则 |
| `.github/workflows/architecture-gates.yml` | 改 | +1 step（warn） |
| `docs/architecture/adr/ADR-088-tlm-backend-retirement.md` | 改 | 回填台账引用 |

### 风险

- **R1（台账退化为形式主义）**：信号 = 大量条目三态相同且理由雷同。缓解 = 3 条批量退役量化规则 + `[framework]` 强制约束 + Gate 1 逐条评审
- **R2（台账与实际漂移）**：信号 = 新增 TLM-only case 未登记。缓解 = `gen_coverage_parity_ledger.sh` 可重生成骨架 + CI 校验条目集一致
- **R3（台账被误当"已移植"清单的完成证明）**：信号 = 有人把「已移植 CH_MEM」标为完成。缓解：台账只记录**状态**，完成判据归 B1 的各自 AC；台账中不设"已完成"列
- **R4（工作量大导致草率）**：信号 = 1.5d 工时超期。缓解：451 条可按测试族分批（`[framework]` 94 优先，因其约束最严）；台账是纯文档，**可分多次提交**，不需要一次到位
- **回退**：纯新增文件 + 1 个 CI step（warn 级），无生产代码改动 ⇒ 回退成本≈0

### 下游依赖

- `chmem-tlm-backend-freeze` —— **硬前置**：台账格式由其 ADR-088 定义；其 Acceptance 末条要求"姊妹提案已立项"
- `feat-tlm-backend-retirement` B0 段 —— Gate 1 判据 G1.4
- `objective-ip-cpu-tlm-backend-retirement` —— Gate 1 G1.4
- **B1 承载力补齐** —— 台账的「已移植 CH_MEM」条目 = B1 待办清单
- **B3 删除** —— 台账是 B3 的前置，Gate 3 G3.3「无批量退役」直接依赖本台账

---
id: 20261022-v0110-mmu-chmem-execution-sequence
status: record
record_only: true
linked_change: openspec/changes/mmu-chmem-phase-b-c-launch/
linked_objective: .rddf/roadmap/objectives/objective-soc-cpu-v0110-launch.md
linked_feature: .rddf/roadmap/features/feat-mmu-chmem-integration.md
linked_improvement: .rddf/improvements/mmu-chmem-phase-b-c-launch.md
tier: standard
estimated_steps: 0
estimated_total_weeks: "0 (纪要 — 非可执行计划)"
verifier: rdd-verifier
sprint_window: {}
deps_status:
  blocked_by: []
  manual_deps: []
---

# v0.11.0 mmu-chmem 执行序列 —— 决策纪要（非排期 SSOT）

> ⚠️ **本文件是「决策纪要」，不是可执行计划，也不是排期 SSOT。**
>
> **排期 SSOT（唯一）**：
> - 任务清单 → `openspec/changes/mmu-chmem-phase-b-c-launch/tasks.md`
> - 排期窗口 / 估时 → `.rddf/plans/mmu-chmem-pipeline-integration.md` frontmatter `sprint_window` + `objective-soc-cpu-v0110-launch.md` §8/§10
> - 变更状态 → `.rddf/state/.planner-handoff.json` / `.planner-state.json`
>
> **本文件只保留两类内容**：① **不可漂移的决策理由与分解映射**（供追溯"为什么当初这么定"）；② **CppHDL API 实测知识**（方法论，跨 change 复用）。
> **一切会随时间变化的数值（日期 / HEAD / case 数 / 排期窗口）一律不复制在本文件内** —— 原文件正因为复制了这些，才在 2026-10-22 出现"HEAD `f53fd5a` / 105 case / Sprint 6-7"等 7 处与仓内不一致的残留。

**来源**：原执行序列计划（2026-10-22 v3 修订版，生成于仓外 `~/.opencode/plan/`）。
**入仓原因**：① 该文件一度是**仓外**的第 4 份排期来源（与仓内 3 处并列声明 Phase B 起点）；② 仓内 `objective-soc-cpu-v0110-launch.md` 台账行曾直接引用其仓外路径 ⇒ **死引用**（`doc_link_check.sh` 只扫 `docs ip src/cf_plugin`，抓不到）。
**入仓方式**：内容按 §1 分解映射下沉到仓内 artifact；本文件降级为纪要，**不作废决策**。

---

## 1. 分解映射表（原文件章节 → 仓内 SSOT artifact）

| 原文件章节 | 仓内 SSOT | 状态 |
|---|---|---|
| Phase 0 — 决策收口（6 项） | `.rddf/plans/mmu-chmem-pipeline-integration.md` §Phase 3 双审裁决注释 + `openspec/changes/mmu-chmem-phase-b-c-launch/tasks.md` | ✅ 下沉 |
| Phase 1 — `ctrl-link-chmem-reg-handle` | `openspec/changes/archive/2026-10-22-ctrl-link-chmem-reg-handle/` + `.rddf/plans/ctrl-link-chmem-reg-handle.md` + capability `ctrllink-continuous-handle` | ✅ 已 archive |
| Phase 2 — 排期与归属对齐 | `.rddf/plans/mmu-chmem-pipeline-integration.md` frontmatter `sprint_window` + `objective-soc-cpu-v0110-launch.md` §8/§10 + objective §11 台账行 `sprint-2026-10-4` | ✅ 下沉 |
| Phase 2.5 — 6d 收官 | `docs/architecture/roadmap-evolution.md` §1 E 表 + ADR-040 v3.0 §10 + `CHANGELOG.md` v0.11.0 段 | ✅ 下沉 |
| Phase 3 — Phase B（15d） | `openspec/changes/mmu-chmem-phase-b-c-launch/tasks.md` Phase B 段 + `.rddf/plans/mmu-chmem-pipeline-integration.md` §Phase 3（Task 3.0-3.5） | ✅ 下沉 |
| Phase 3.5 — fetch-stall v0.2 + bp-btb 2 项 + R11.1 | `openspec/changes/mmu-chmem-phase-b-c-launch/tasks.md` Phase 3.5 段 | ✅ 下沉 |
| Phase C / D / E | 同上 change 的 Phase C / C.6 / D / D+E 段 | ✅ 下沉 |
| Session 拆分契约（session 0/1/2） | `.rddf/improvements/mmu-chmem-phase-b-c-launch.md` §What Changes | ✅ 下沉 |
| M1-M8 实测发现 | 见 §3 逐条去向 | ✅ 全部有归属 |
| K1-K6 CppHDL 实测知识 | 见 §3 —— **K2/K4/K5 保留在本文件 §4**，K1/K3 已入 ADR-040 v3.0 §10.3 + `AGENTS.md` | ⚠️ 部分保留 |
| 风险与回滚 R1-R10 | R1-R3 → `.rddf/improvements/mmu-chmem-phase-b-c-launch.md` §Impact；R6-R10 → `.rddf/plans/chmem-mmu-keys-modality.md` §8 | ✅ 下沉 |
| 待用户澄清 6 项 | Phase 0 决策（见 §2） | ✅ 已拍板 |
| 决策日志 | 见 §2 | ✅ 保留（本文件） |

---

## 2. 不可漂移的决策记录

### 2.1 Phase 0 的 6 项拍板（2026-10-22）

| # | 决策 | 一句话理由 |
|---|---|---|
| 1 | Phase B 启动窗口 = **B1（Sprint 2-3）**，工作量按源 change 的 **15d** 排 | 12-01 起只剩 8 周要塞 B(15d)+C(5d)+C.6(3d)+D(5d)+E(2d) ≈ 30d + DMIPS 硬门禁，buffer ≈ 0 |
| 2 | `fetch-stall-protocol-v0.2` **移出 pre_flight** → 新 slot **Phase 3.5**（挂 C.4 入口） | owner（wave5-bp-btb）已 defer 到 v1.0.0 ⇒ 窗口内无人可交付；且 requirements doc §1.5 BLOCKER-B5 明写 fetch-stall **反向依赖 Phase B** ⇒ 原位置是逻辑倒挂 |
| 3 | `bp-btb-coordination-3items` **收窄为 2 项**（去 `pc_reg mux` 的 BP 分支）并**并入 Phase 3.5** | 同上 owner defer；真实需求方是 MMU PTW 期间冻结 fetch，不是 BP |
| 4 | `feat-phase-6d-rtl-verification` 收官插 Phase 2 之后 | 填 Sprint 2 空档；E1-E14 逐条核验后**保持 active**（PoC-2/3 未实现，按诚实性原则不谎报） |
| 5 | **A4 整体挪到 C.5**（Phase 1 只做 A1/A2/A3/A5） | A4 的真实驱动源在 C.5；Phase 1 交付的 `ch_reg` 无 next 驱动源 ⇒ 恒 false 惰性骨架（惰性窗口 ~5 周） |
| 6 | 新文件路径 = **`ip/mmu/plugins/MMUPlugin_chmem.h`** | `ip/mmu/tlm/` 同时逃出 `check_plugin_portability.sh` 的 Check 2 与 Check 7 ⇒ 放那里等于让 4 门禁完全失明 |

### 2.2 路线选择与实施方式决策

| 决策 | 理由 |
|---|---|
| 走 **mmu-chmem track** 而非 `vexii-riscv-parity-poc` / `phase-1.5-wave-4` | 前者 v1.0.0 = 2027 Q3 且依赖不存在的 `cache-icache-fence-i` + cycle-precision 0/25；后者 scope 已腐烂（大部分被后来的 CSR/Sv32 change 做完） |
| **CtrlLink 先行**于 Phase B | 框架级小改、**不依赖** `MMUPlugin_chmem.h` 这个最大不确定性源；且 Phase 1 本身就是 `ch_reg` + OR 网 API 的**交互式热身 session**（Phase B 要用的正是同一套 API）⇒ B1 不是冷启动 |
| Phase B 按 **15d** 而非 37KB plan 原写的 10d | 10d 版**整段丢失 B.5 的 5d 集成回归**；在"历史 3 次 deep-agent 派发失败"的前提下采纳压缩版是错误方向 |
| **Phase 2 排期对齐先于 Phase 3** | 不对齐会按两个排期各干一遍；且是纯文档零代码风险 |
| 弃用 `bundles/mmu_bundles.h` 方案 | `PayloadStore` 按**指针身份**索引 ⇒ 新 key 对象 ⇒ 消费端 `has()` 恒 false ⇒ 静默回退 Bare ⇒ **测试全绿但 sv32 从未生效** |
| 全仓 deprecated `\|\|`/`&&` 清理**拆独立 change** | ~134 站点 / 7 业务文件，1d 框架小改不可能清完 |

### 2.3 2026-10-22 双审（oracle + metis）新增决策

| 决策 | 理由 |
|---|---|
| 新增前置 change **`chmem-mmu-keys-modality`**（Phase B 硬前置） | 审查发现 **B-1**：MMU payload key 非 mode-aware ⇒ CH_MEM 通道只能装 elaboration 期常量 ⇒ B.3 不可实现、AC-S3-3 无鉴别力；叠加 elaboration 顺序缺陷（消费者恒先跑）⇒ 静默 Bare。详见 §5 |

---

## 3. M1-M8 实测发现的去向

| # | 发现 | 去向 |
|---|---|---|
| M1 | `bundles/mmu_bundles.h` 方案 ⇒ sv32 静默回退 Bare（指针身份） | `.rddf/plans/chmem-mmu-keys-modality.md` §1.1 + AC-MK-1；本文件 §2.2 |
| M2 | 模板参数必须一致（`mmu_keys<uint64_t>` 是不同静态实例） | 同上 §1.1 + AC-MK-1 |
| M3 | `ip/mmu/tlm/` 是 Check 2/7 双盲区 | 同上 §2 决策 D-1 + §2.1 决策 6 |
| M4 | `CHMEM_TEST_SOURCES` 是手工 list（GLOB 只覆盖 TLM target） | 同上 Step 0.1 + AC-MK-15 |
| M5 | deprecated `ch_bool::operator\|\|` 实测（编译器统计） | ADR-040 v3.0 §10.3 + 本文件 §4 K4 |
| M6 | `OR_combined_halt_conds()` 与既有 API 关系未定 | **已关闭** —— Phase 1 定案：新 API 为唯一实现，旧名 inline alias 转发 |
| M7 | CH_MEM 运行期 `should_*()` 调用方为零 | **已关闭** —— Phase 1 实测暴露面仅 `pipe_builder.h` 2 处；能力 `ctrllink-continuous-handle` 落地 |
| M8 | `ip/mmu/plugins/mmu_ptw_chmem.h` 路径不存在 | 37KB plan §Phase 3 Task 3.3 已更正为 `ip/cpu/plugins/mmu_ptw_chmem.h` |

---

## 4. CppHDL API 实测知识（保留项）

> 这三条是**方法论**，跨 change 复用，且不随排期漂移 ⇒ 保留在本文件。
> K1 / K3 / K6 已在别处有归属或已过期，不重复。

### K2 — `ch_reg<ch_bool>` 切片到 `ch_bool` 保留 proxy 节点

上游 `lnode/reg.tpp` 的 `ch_reg` 构造函数里 `static_cast<logic_buffer<T>*>(this)->node_impl_ = proxy_node;` ⇒ 基类 `logic_buffer<ch_bool>` 持有的是 **proxy 节点**（而非 regimpl 本身）；`logic_buffer` 拷贝构造复制 `node_impl_` ⇒ **切片后 OR 门仍引用 proxy ⇒ 随拍更新**。

⇒ 这是"stateful halt 能力早已具备、只差 API 封装"的技术依据（Phase 1 `ctrl-link-chmem-reg-handle` 据此设计）。
⇒ 同一机制在 `include/cf/plugin/pipe_builder.h` 的 `elaborate()` 聚合注释中亦有记录。

### K4 — deprecated 算子统计**必须用编译器，不能用 grep**

`grep "||"` 会漏掉**同行不含 `ch_bool` 字面量**的用法（例：`pipe_builder.h:434` 的 `stall = stall || cl->halt_condition();`）。正确做法：

```bash
g++-14 -std=c++23 -fsyntax-only -Wall -Wextra <flags> <probe.cpp> \
  | grep -E "use operator\| for 1-bit OR|use operator& for 1-bit AND"
```

⇒ 已入 `docs/architecture/adr/ADR-040-tlm-hdl-portability-constraints.md` §10.3「统计方法约束」（本条为其原始出处）。

### K5 — 表达式即节点 ⇒ **同一 API 两次调用产生不同指针**

`halt_condition()` 与 `OR_combined_halt_conds()` 每次调用各自生成一个**独立 OR 节点**（CppHDL 的表达式即节点）⇒ 两次调用得到两个**语义等价但指针不同**的节点（与改动前的 `halt_condition()` 行为一致）。

⇒ **测试断言"值等价"而非"指针相等"。**

⇒ **对 `chmem-mmu-keys-modality` 的边界约束**（已补入该 plan 的 AC-MK-2 / AC-MK-12）：
**节点指针相等断言仅适用于「cell 传递」路径**（consumer 读 producer 重绑进 payload cell 的那个 proxy）；若断言涉及 **API 生成的节点**（如 OR 合并结果），必须改断言**值等价**，否则会写出必然失败或恒真的断言。

---

## 5. 2026-10-22 双审新增：B-1 缺口

**结论**：MMU payload 通道在 CH_MEM 模式下**只能承载 elaboration 期常量**（编译器实证：`PayloadStore::get(mmu_keys<uint64_t>::PADDR_VALID)` → `bool&`；`put(Payload<bool>&, ch_bool)` → 编译 error）。叠加 **elaboration 顺序缺陷**（`canonical_stage_order()` 只看 stage 名字首现序 ⇒ 消费者恒先于生产者跑 ⇒ `has()` 恒 false ⇒ 永久静默 Bare）。

⇒ 立项前置 change **`chmem-mmu-keys-modality`**（`.rddf/plans/chmem-mmu-keys-modality.md` + `.rddf/improvements/chmem-mmu-keys-modality.md`），Phase B 的 B.3/B.4 在其 archive 之前**不可开工**。

**同源问题（姊妹 change `chmem-cpu-keys-modality`，挂 Phase C.1 入口）**：`ip/cpu/tlm/cpu_keys.h` 4 个裸类型 key + `bundles/csr_bundles.h` 4 个 `Payload<bool>` key + `csr_chmem.h:191-194` 的常量化写入 + `csr_bundles.h:17` 声称 `TRAP_PENDING` 被 `ibus_chmem.h` 消费但实测 grep = 0。

---

## 6. 维护纪律

1. **本文件不更新排期**。任何窗口 / 估时 / HEAD / case 数的变化，只改 §1 表中指向的 artifact。
2. **§2 决策若被推翻**：在本文件追加一条"已推翻"记录（含日期 + 新裁决指向），**不要删历史行**（与 objective 台账的 append-only 一致）。
3. **§3/§4 的实测知识**：新增条目须带 file:line 级出处；已被 ADR-040 / `AGENTS.md` 收录的，标"已入 <文件> §<节>"并只保留原始表述。
4. **本文件被引用时**，引用方须写仓内路径 `.rddf/plans/20261022-v0110-mmu-chmem-execution-sequence.md`，**禁止**再引用仓外路径。

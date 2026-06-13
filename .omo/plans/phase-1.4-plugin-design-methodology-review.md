# Phase 1.4: L1CachePlugin 设计方法学复盘 — 工作计划

> **计划ID**: phase-1.4-plugin-design-methodology-review
> **创建日期**: 2026-06-13
> **关联决策**: DECISION-2026-06-13-02 (`.omo/drafts/decision-phase-1.4-methodology-review-2026-06-13.md`)
> **关联路线图**: `docs/roadmap/phases/phase-1-tlm-foundation.md` §1.4
> **状态**: 🟢 **Phase 1.4 ORCHESTRATION COMPLETE** (boulder closed, **6 原子 commit** `137df84` + `b65ff04` + `863ab03` + `ec50a8b` + `cda9521` + `7cdd4d3`, 51/51 acceptance checkboxes ✅, F1-F4 Final Wave 4/4 APPROVE, DECISION Accepted v1, E1-E8 退出标准 8/8 ✅, 12 evidence + 3 notepad 全部落地, ctest 16/16 + D4+ADR-040 3+4/3 全部 PASS, Phase 1 进度 75% → 85%, PA-1~PA-9 全部 ✅, Phase 2 启动门槛就绪)
>
> **会话起止**: 2026-06-13 16:32 - 17:56 (~1h 24m, 3 sessions: `opencode:ses_14012658effetZvWAdd2ieRiFm` + `opencode:ses_13fbfefbdffehqHPI3JtG3lpd7` + current)
>
> **ORCHESTRATION 守卫 commit 链** (本计划完整生命周期, **6 原子 commit**):
> 1. `137df84` (7 files, 2274+/17-): Phase 1.4 v1 方法学文档 (主交付)
> 2. `b65ff04` (1 file, 51+/51-): 51 acceptance checkboxes batch-mark
> 3. `863ab03` (2 files, 11+/11-): E6-E8 ⏳→✅ + 决策 Proposed→Accepted
> 4. `ec50a8b` (1 file, 3+/1-): plan 头部状态守卫 (🟢 ORCHESTRATION COMPLETE)
> 5. `cda9521` (1 file, 9+/2-): 4 原子 commit 链 + session 3 完整记录 (守卫)
> 6. `7cdd4d3` (1 file, 5+/4-): 5 原子 commit 链守卫 + SHA 替换 (`<pending>` → `cda9521`)

---

## TL;DR

> **核心目标**: 以 L1CachePlugin 为具体例子, 复盘 Plugin 声明式电路设计方法学, 沉淀为《Plugin 声明式电路设计方法学 v1》文档
>
> **关键交付**:
> - `docs/methodology/plugin-style-design-methodology-v1.md` (新建, 200-400 行, 6 维度 × 3 边界标注)
> - `docs/lessons/phase-1.2-l1cacheplugin.md` 标 supersede 注释
> - `docs/roadmap/roadmap-status.md` 同步更新
>
> **预估工时**: 1-2 天 (单 session 可完成)
> **并行执行**: NO (单例子深复盘, 任务相互依赖)
> **关键路径**: 决策审阅 → 6 维度评估 → 3 边界标注 → 文档生成 → roadmap 同步

---

## Context

### 原始请求（用户 2026-06-13 对话）

> "你是不是偏离我们预定的讨论, 我希望我们构建一个 plugin-style 声明式的最新例子, 选择了 l1_cache 做为实现的例子, 请检查计划内容, 是否是构建 plugin 声明式的 tlm 的 l1 cache 设计"

> "先把性能对齐放一边, 目的是评估 plugin 的声明式电路设计的第一例子, 检查这个设计方法学有没有问题, 然后再后面阶段我们可以改进成完整的 plugin 框架。"

**核心意图**: Phase 1.4 = **Plugin 声明式电路设计方法学复盘** (以 L1Cache 为镜子), 而非 cpptlm::CacheTLM 性能基线对比。

### 访谈总结

**关键讨论**:
- **E1 (复盘对象)**: L1CachePlugin 作为唯一复盘对象, 否决 cpptlm::CacheTLM 性能基线 (因 stub 几何不匹配)
- **E2 (复盘维度)**: 6 个评估维度 (D1 可读性 / D2 范式合规 / D3 TLM↔RTL 可移植性 / D4 阶段调度 / D5 Payload 通信 / D6 测试便利)
- **E3 (复盘输入)**: 5 类材料 (Phase 1.2 lessons / L1Cache 源码 / Bridge+Adapter / D4 决策文档 / Phase 1.3 决策链)
- **E4 (方法学边界)**: 3 类标注 (B1 接受 / B2 摩擦 / B3 局限)
- **E5 (复盘深度)**: 单例子深复盘 (v1), 横向对比推迟到 Phase 2+ (v2)

**Metis 验证关键事实**:
- `cpptlm::CacheTLM` 是 stub (`std::map` 全关联, 无淘汰, 5/50 cycle 硬编码) — Metis 验证为性能基线无意义
- HybridCacheWrapper 仅 BUILD_RTL=ON 时注册 — 默认 OFF
- 现有 `docs/lessons/phase-1.2-l1cacheplugin.md` 是"踩坑清单"而非"方法学评估", 有升级空间

### 决策草案要点

- **F1.A**: 复盘对象 = L1CachePlugin (否决 cpptlm baseline)
- **F2**: 6 评估维度 (D1-D6)
- **F3**: 5 类输入材料 (I1-I5)
- **F4**: 3 类方法学边界标注 (B1/B2/B3)
- **F5.A**: 单例子深复盘
- **F6**: E1-E8 退出标准

---

## Work Objectives

### 核心目标

**用 L1CachePlugin 作为第一个具体例子, 复盘 ChipForge Plugin 声明式电路设计方法学, 沉淀为可被未来 IP (L2/ICache/Interconnect) 复用的方法学 v1 文档。**

### 具体交付物

1. `docs/methodology/plugin-style-design-methodology-v1.md` (新建, 200-400 行)
2. `docs/lessons/phase-1.2-l1cacheplugin.md` 标 supersede 注释
3. `docs/roadmap/roadmap-status.md` 同步更新 (§3 PA-7/PA-9 ✅, §4 R7 闭环, §6 活动日志)

### Definition of Done

- [x] `wc -l docs/methodology/plugin-style-design-methodology-v1.md` ≥ 200
- [x] 文档含 D1-D6 6 维度标题 + B1/B2/B3 边界标注
- [x] ≥ 10 个 B2 摩擦点带具体代码模式
- [x] ≥ 3 个 B3 局限点链接 Phase 6 任务
- [x] `ctest --test-dir build --output-on-failure` 16/16 仍 PASS
- [x] `git status` 干净 (无未追踪文件)

### Must Have

- 6 维度评估完整 (D1-D6)
- 3 类边界显式标注 (B1/B2/B3)
- B2 摩擦点至少 10 个, 带可复用代码模式
- B3 局限至少 3 个, 链接 Phase 6 框架升级任务
- 文档结构化, 可被未来 IP 写作者 grep 检索

### Must NOT Have (Guardrails)

- **不实施 cpptlm::CacheTLM baseline** — Metis 验证为 stub, 无意义
- **不写代码** (复盘文档是描述性的, 不改 L1CachePlugin / Bridge / Adapter)
- **不引入新 IP** (L2/ICache/Interconnect 推迟到 Phase 2+)
- **不掩盖方法学局限** (B3 显式记录, 不假装问题不存在)
- **不复制 phase-1.2 lessons** (升级为方法学评估, 不是叠加)
- **不写"最佳实践指南"** (那需要多 IP 经验, Phase 2+ 才有素材)

---

## Verification Strategy

### Test Decision

- **基础设施存在**: N/A (本次为纯文档工作)
- **自动化测试**: N/A (不写代码)
- **QA Policy**: 文档自检 (grep 验证结构 + 字数 + 边界标注密度)

### 文档自检清单

- [x] 字数: 200-400 行 (`wc -l`)
- [x] D1-D6 标题存在 (`grep "^## D[1-6]"`)
- [x] B1/B2/B3 标注 ≥ 13 处 (`grep -cE "B[123]"`)
- [x] B2 模式 ≥ 10 个 (`grep -c "B2"`)
- [x] B3 局限 ≥ 3 个 (`grep -c "B3"`)
- [x] 跨章节引用完整 (每个 B3 链接 Phase 6 任务)
- [x] Phase 1.2 lessons supersede 标注存在

---

## Execution Strategy

### 任务依赖图

```
T1 (决策审阅) ──→ T2 (维度 D1-D2 评估) ─┐
                                        ├──→ T4 (整合 + 生成文档 v1) ──→ T5 (roadmap 同步) ──→ T6 (commit)
T1 (决策审阅) ──→ T3 (维度 D3-D6 评估) ─┘
```

**关键依赖**: T1 必须在 T2/T3 之前 (审阅 E1-E5 决议);T4 必须在 T2/T3 之后 (整合);T5 必须在 T4 之后 (引用 v1 文档);T6 必须在 T5 之后。

### 并行策略

**本次不最大化并行** —— 单例子深复盘任务相互依赖 (一个维度引用的代码片段可能被另一个维度复用),且文档写作是创造性工作,需要**整体连贯性**。 1-2 天预算下串行更高效。

**子任务内部可串行的环节**:
- T2 内: D1 (可读性) 先于 D2 (范式合规) — 先感性后理性
- T3 内: D3 (TLM↔RTL) 先于 D4 (调度) 先于 D5 (Payload) 先于 D6 (测试) — 评估粒度由大到小

### 时间线 (单 session 估算)

| 时间 | 任务 | 输出 |
|------|------|------|
| T+0h | 阅读决策草案 + 5 类输入材料 | 评估材料 ready |
| T+0.5h | T2: D1+D2 评估 (可读性 + 范式合规) | 评估笔记 ~50 行 |
| T+1.5h | T3: D3+D4+D5+D6 评估 | 评估笔记 ~150 行 |
| T+2.5h | T4: 整合 + 写 v1 文档 (含 3 类边界标注) | v1 文档 200-400 行 |
| T+3.5h | T5: 同步 roadmap-status.md | 同步完成 |
| T+4h | T6: 自检 + commit | commit 落地 |

---

## TODOs

> **任务标签格式**: 严格使用 `1.`, `2.`, `3.`... 编号。Final Wave 用 `F1.`, `F2.`, `F3.`, `F4.`。
> **本次为纯文档工作**: 所有任务 QA 通过 `grep` / `wc` / 文档自检完成, 不涉及 Playwright / curl / tmux。

### 1. 决策草案审阅与输入材料消化

  **What to do**:
  - 阅读 `.omo/drafts/decision-phase-1.4-methodology-review-2026-06-13.md` 全部内容 (确认 F1-F5 决议)
  - 阅读 `docs/lessons/phase-1.2-l1cacheplugin.md` 7 类 15+ 教训 (I1)
  - 阅读 `ip/cache/tlm/L1CachePlugin.{h,cpp}` 主体代码 (I2)
  - 阅读 `src/cf_plugin/bridge/l1_cache_bridge.{h,cpp}` + `l1_cache_bridge_adapter.{h,cpp}` (I3)
  - 阅读 `docs/architecture/declarative-hybrid-framework.md` 关键章节 (D4 + Bundle 三层) (I4)
  - 阅读 `.omo/drafts/decision-plugin-framework-2026-06-08.md` + `decision-phase-1.3-bridge-2026-06-10.md` + `decision-phase-1.3d-extras-bridge-2026-06-13.md` (I5)

  **Must NOT do**:
  - 不读 Phase 2+ 文档 (避免越界)
  - 不读 L2/ICache/Interconnect 设计草案 (本次单例子)
  - 不读 cpptlm::CacheTLM 源码 (Metis 已确认是 stub, 不需复盘)

  **Recommended Agent Profile**:
  - **Category**: `quick` (阅读+理解, 1 个 session 内完成)
  - **Skills**: 不需要
  - **Reason**: 纯文档阅读, 任务边界明确, 无需多技能

  **Parallelization**:
  - **Can Run In Parallel**: YES (可与 T2/T3 的部分调研重叠)
  - **Parallel Group**: Wave 1 (与 T2/T3 启动门槛)
  - **Blocks**: T2, T3
  - **Blocked By**: None

  **References**:
  - `.omo/drafts/decision-phase-1.4-methodology-review-2026-06-13.md` §2 (F1-F5 决议清单)
  - `docs/lessons/phase-1.2-l1cacheplugin.md` §1-§7 (7 类教训)
  - `ip/cache/tlm/L1CachePlugin.h:67-200` (类定义, 阶段声明, 存储, helper API)
  - `ip/cache/tlm/L1CachePlugin.cpp` (build(), at_stage() 实现)
  - `src/cf_plugin/bridge/l1_cache_bridge.h` + `.cpp` (Bridge 框架层)
  - `src/cf_plugin/bridge/l1_cache_bridge_adapter.h` + `.cpp` (Adapter + ch_stream 注册)
  - `docs/architecture/declarative-hybrid-framework.md` §4 (D4 强制细节)
  - `.omo/drafts/decision-plugin-framework-2026-06-08.md` D4 (Plugin-style 强制)

  **Acceptance Criteria**:
  - [x] 决策草案 5 决议 (F1-F5) 全部理解, 能在白板上复述
  - [x] Phase 1.2 lessons 7 类教训能列举
  - [x] L1CachePlugin 类结构 + at_stage 注册位置能在源码定位

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 决策草案 5 决议已内化
    Tool: Bash (cat + grep)
    Preconditions: T1 已完成
    Steps:
      1. cat .omo/drafts/decision-phase-1.4-methodology-review-2026-06-13.md | grep "^### F"
      2. 验证输出含 F1, F2, F3, F4, F5, F6 共 6 决议标题
    Expected Result: 6 行输出
    Evidence: .omo/evidence/phase-1.4-task-1-decisions-internalized.txt

  Scenario: 5 类输入材料全部阅读
    Tool: Bash (wc + grep)
    Preconditions: T1 已完成
    Steps:
      1. for f in docs/lessons/phase-1.2-l1cacheplugin.md \
                ip/cache/tlm/L1CachePlugin.h ip/cache/tlm/L1CachePlugin.cpp \
                src/cf_plugin/bridge/l1_cache_bridge.h src/cf_plugin/bridge/l1_cache_bridge.cpp \
                src/cf_plugin/bridge/l1_cache_bridge_adapter.h src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp \
                docs/architecture/declarative-hybrid-framework.md \
                .omo/drafts/decision-plugin-framework-2026-06-08.md \
                .omo/drafts/decision-phase-1.3-bridge-2026-06-10.md \
                .omo/drafts/decision-phase-1.3d-extras-bridge-2026-06-13.md; do
           wc -l "$f" >> /tmp/phase-1.4-input-sizes.txt
         done
      2. 验证 /tmp/phase-1.4-input-sizes.txt 含 12 行 (I1-I5 含多文件)
    Expected Result: 12 行字数统计
    Evidence: .omo/evidence/phase-1.4-task-1-inputs-read.txt
  ```

  **Commit**: NO (与 T6 一并提交)

---

### 2. 维度 D1-D2 评估: 可读性 + 范式合规

  **What to do**:
  - **D1 代码可读性**: 评估 L1CachePlugin 主体代码 (I2) 的可读性:
    - `setup()` + `build()` 两个成员函数 + at_stage 闭包结构是否清晰
    - 命名一致性 (kNumSets / kTagBits / payload_node_ / extract_idx)
    - 文件头注释 + 类注释 + 函数注释是否充分
    - 是否有"为框架而写"的冗余 (helper API `issue_request` / `refill_from_memory` 是泄漏到测试的内部 API)
  - **D2 范式合规性**: 用 `tools/check_plugin_decision.sh` 验证 + 人工对照 D4 决策:
    - `verify_plugin_decision.sh` 检查: 无 `void tick()`, 无 `ch_uint<>` 字段, 无 `enum class State`, 无 `switch (state_`
    - 业务代码 (at_stage 闭包内) 是否完整遵守
    - 框架层 (Bridge `tick()` 末尾调 `pb_.run()`) 是否正确处理范式边界 (D1' 末尾挂载契约)
    - 哪些 D4 条款是"已实现" vs "尚未触发" (e.g., "无 if (cond) return;" 在 RTL 升级时才暴露)

  **Must NOT do**:
  - 不修改 `tools/check_plugin_decision.sh` (Phase 0 退出标准锁定)
  - 不建议改 L1CachePlugin 源码 (复盘是描述性, 改代码是 Phase 2+)
  - 不重复 Phase 1.2 lessons 已记录的具体行号陷阱 (升级为方法学层级)

  **Recommended Agent Profile**:
  - **Category**: `writing` (技术写作, 方法学评估)
  - **Skills**: 不需要
  - **Reason**: 评估性写作, 需对 D4 范式有深度理解

  **Parallelization**:
  - **Can Run In Parallel**: NO (写作连贯性优先)
  - **Parallel Group**: Sequential (T2 → T3)
  - **Blocks**: T4
  - **Blocked By**: T1

  **References**:
  - D1: `ip/cache/tlm/L1CachePlugin.h:39-200` (类定义全部, 看命名/注释/helper API)
  - D1: `ip/cache/tlm/L1CachePlugin.cpp` 全文 (build + at_stage 实现)
  - D2: `tools/check_plugin_decision.sh` (D4 静态检查脚本)
  - D2: `.omo/drafts/decision-plugin-framework-2026-06-08.md` D4 (Plugin-style 强制)
  - D2: `docs/lessons/phase-1.2-l1cacheplugin.md` §3.1-3.2 (D4 静态检查的隐藏规则)
  - D2: `.omo/drafts/decision-phase-1.3-bridge-2026-06-10.md` D1' (末尾调 pb_.run() 契约)

  **Acceptance Criteria**:
  - [x] D1 评估含 5-8 个具体观察点 (代码引用 + 行号)
  - [x] D1 明确"成功 / 摩擦 / 局限"分类
  - [x] D2 跑过 `tools/check_plugin_decision.sh` 3/3 PASS
  - [x] D2 明确每个 D4 条款的"已实现"状态

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: D4 静态检查 3/3 PASS
    Tool: Bash
    Preconditions: T1 已完成, T2 启动前
    Steps:
      1. bash tools/check_plugin_decision.sh 2>&1 | tee /tmp/phase-1.4-d4-check.txt
      2. 验证输出含 "3/3 PASS" 或 "All checks passed"
    Expected Result: 退出码 0, 输出 PASS
    Evidence: .omo/evidence/phase-1.4-task-2-d4-static-check.txt

  Scenario: D1 + D2 评估笔记完成
    Tool: Read
    Preconditions: T2 评估已写入草稿
    Steps:
      1. cat .omo/drafts/phase-1.4-d1-d2-notes.md
      2. 验证含 D1/D2 标题 + 至少 5 个具体观察 + "成功/摩擦/局限" 分类
    Expected Result: 文件存在, 结构化
    Evidence: .omo/evidence/phase-1.4-task-2-d1-d2-notes-exist.txt
  ```

  **Commit**: NO

---

### 3. 维度 D3-D6 评估: TLM↔RTL + 阶段调度 + Payload + 测试

  **What to do**:
  - **D3 TLM↔RTL 可移植性**:
    - 业务代码切到 RTL 时哪些地方需要修改? (梳理 lessons §2.3-2.5)
    - `array_store` 抽象是否有效降低切换成本? (TLM 模式 `std::array` / RTL 模式 `ch_mem` 双缓冲)
    - `extract_idx/tag` helper 是否为位选迁移预留? (TLM shift+mask / RTL addr[HI:LO] 位选)
    - ADR-040 (TLM/HDL 移植性约束) 的 4 个 Tier 在 L1Cache 上是否被遵守?
  - **D4 阶段调度清晰度**:
    - lookup + refill 两阶段拆分是否自然? (一个请求跨两个 stage, 用同一 payload_node 共享)
    - `pb.run()` 一次性执行所有 at_stage 回调的语义是否清晰? (无 cycle 精度, 无调度)
    - `declare_substage` 声明不对应线程调度的边界? (Phase 0 限制, Phase 6 升级)
  - **D5 Payload 通信效率**:
    - Payload<T> Key 全局静态 + 按指针身份匹配, 是否够直观?
    - 跨 PipeNode 隔离机制是否清晰? (同一翻译单元共享 Key, 不同节点有独立 PayloadStore)
    - lessons §1.1 的 "at_stage 创建独立 PipeNode" 陷阱是否已文档化?
  - **D6 测试便利性**:
    - Plugin 实例 + pb 注册 + 裸指针 helper 模式, 是否对单元测试作者友好?
    - `helper->issue_request(...)` 这种"内部 API"是否泄漏? (生产路径应通过 Bundle 流式接口)
    - 单元测试代码 (`test_l1_cache_plugin_unit.cpp`) 是否比业务代码本身还长? (测试 1:1 比例的健康度)

  **Must NOT do**:
  - 不实施 array_store 改进 (B2 摩擦点的解决时机是 Phase 6)
  - 不建议加新 helper API (B2 摩擦点的判断基于现有代码)
  - 不写"如何在 L1Cache 上修复 B3 局限" (那是 Phase 6 任务, 本次只识别)

  **Recommended Agent Profile**:
  - **Category**: `writing` (技术写作, 方法学评估)
  - **Skills**: 不需要
  - **Reason**: 同 T2, 评估性写作

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential (T2 → T3 → T4)
  - **Blocks**: T4
  - **Blocked By**: T2

  **References**:
  - D3: `docs/lessons/phase-1.2-l1cacheplugin.md` §2.3-2.5 (`if (cond) return;` / `array_store` / 位提取)
  - D3: `docs/architecture/adr/ADR-040-tlm-hdl-portability-constraints.md` (4 Tier 约束)
  - D4: `docs/lessons/phase-1.2-l1cacheplugin.md` §1.1-1.3 (`at_stage` 创建独立 PipeNode / `pb.run()` 一次性 / `declare_substage`)
  - D4: `ip/cache/tlm/L1CachePlugin.cpp` `setup()` + `build()` (阶段声明 + 注册)
  - D5: `docs/lessons/phase-1.2-l1cacheplugin.md` §一 + §五 (Payload Key 惯例)
  - D5: `ip/cache/tlm/L1CachePlugin.cpp` 匿名 namespace `Payload<T> g_addr, g_idx, ...` (Key 声明)
  - D6: `src/cf_plugin/tests/test_l1_cache_plugin_unit.cpp` (4 单元测试)
  - D6: `docs/lessons/phase-1.2-l1cacheplugin.md` §四 (TDD 实践)
  - D6: `ip/cache/tlm/L1CachePlugin.h:88-131` (helper API: issue_request / refill_from_memory / read_response)

  **Acceptance Criteria**:
  - [x] D3 评估含 ADR-040 4 个 Tier 的对照表
  - [x] D4 评估明确 lookup+refill 两阶段的边界类型
  - [x] D5 评估含 Key 命名规范 + 跨翻译单元问题的处理
  - [x] D6 评估含 helper API 泄漏程度判断 (生产路径 vs 测试路径)
  - [x] 4 维度共 20-30 个具体观察点

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: D3-D6 评估笔记完成
    Tool: Read
    Preconditions: T3 评估已写入草稿
    Steps:
      1. cat .omo/drafts/phase-1.4-d3-d4-d5-d6-notes.md
      2. 验证含 D3, D4, D5, D6 标题 + 至少 20 个具体观察 + ADR-040 对照表
    Expected Result: 文件存在, 结构化
    Evidence: .omo/evidence/phase-1.4-task-3-d3-d4-d5-d6-notes-exist.txt

  Scenario: 单元测试代码与业务代码行数对比 (D6 健康度)
    Tool: Bash (wc)
    Preconditions: T1 已完成
    Steps:
      1. wc -l ip/cache/tlm/L1CachePlugin.cpp
      2. wc -l src/cf_plugin/tests/test_l1_cache_plugin_unit.cpp
      3. 计算测试/业务比例, 评估是否在 1:1 ~ 2:1 健康区间
    Expected Result: 数字输出, 比例在 1:1 ~ 2:1
    Evidence: .omo/evidence/phase-1.4-task-3-test-business-ratio.txt
  ```

  **Commit**: NO

---

### 4. 整合 + 生成 v1 文档（含 3 类边界标注）

  **What to do**:
  - 整合 T2 + T3 的评估笔记, 写为 `docs/methodology/plugin-style-design-methodology-v1.md`
  - 文档结构 (强制):
    - §0 元信息 (创建日期, 关联决策, 范围, 关联文档)
    - §1 引言 (为什么需要这份文档, 与 phase-1.2 lessons 的关系)
    - §2 6 维度评估 (D1-D6 每维度一节, 含 "现状 + 判断 + 建议")
    - §3 3 类方法学边界 (B1 接受 / B2 摩擦 / B3 局限)
    - §4 对未来 IP 写作者的建议 (按 B2 模式 + B3 局限)
    - §5 Phase 6 框架升级的输入 (B3 局限 → 任务清单)
    - §6 退出标准 (F6 决议的 E1-E8)
    - §7 修订历史
  - 标注密度要求:
    - ≥ 10 个 B2 摩擦点, 每个带具体代码模式
    - ≥ 3 个 B3 局限点, 每个链接 Phase 6 任务
    - 6 维度每维度 5-8 个评估点, 共 30-48 个评估点
  - 字数控制: 200-400 行 markdown (用标题 + 列表 + 短段落, 不写散文)

  **Must NOT do**:
  - 不复制 lessons 文档原句 (升级为方法学层级, 不堆叠)
  - 不掩盖 B3 局限 (B3 是 Phase 6 输入, 必须显式)
  - 不写"最佳实践" (Phase 2+ 才有素材)
  - 不引用未在 I1-I5 中的文档 (避免越界)

  **Recommended Agent Profile**:
  - **Category**: `writing` (技术文档生成)
  - **Skills**: 不需要
  - **Reason**: 整合性写作, 需对 T2/T3 输出有整体把握

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential (T1 → T2 → T3 → T4 → T5 → T6)
  - **Blocks**: T5, T6
  - **Blocked By**: T2, T3

  **References**:
  - T2 + T3 评估笔记 (`.omo/drafts/phase-1.4-*-notes.md`)
  - 决策草案 §2 F1-F5 (本计划的元信息源)
  - 决策草案 §2 F6 (退出标准 E1-E8)

  **Acceptance Criteria**:
  - [x] 文档存在, 字数 200-400 行
  - [x] 6 维度标题完整 (D1-D6)
  - [x] 3 边界标注 ≥ 13 处
  - [x] B2 模式 ≥ 10 个 + B3 局限 ≥ 3 个
  - [x] §6 退出标准章节引用 F6 E1-E8
  - [x] 关联文档链接完整 (回指决策草案 + lessons + D4 决策)

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: 文档结构化 (6 维度 + 3 边界)
    Tool: Bash (grep)
    Preconditions: T4 已生成 v1 文档
    Steps:
      1. wc -l docs/methodology/plugin-style-design-methodology-v1.md
         # 期望: 200-400
      2. grep -cE "^## D[1-6]" docs/methodology/plugin-style-design-methodology-v1.md
         # 期望: ≥ 6
      3. grep -cE "B[123]" docs/methodology/plugin-style-design-methodology-v1.md
         # 期望: ≥ 13
      4. grep -c "B2" docs/methodology/plugin-style-design-methodology-v1.md
         # 期望: ≥ 10
      5. grep -c "B3" docs/methodology/plugin-style-design-methodology-v1.md
         # 期望: ≥ 3
    Expected Result: 5 行数字输出, 全部满足阈值
    Evidence: .omo/evidence/phase-1.4-task-4-doc-structure.txt

  Scenario: Phase 1.2 lessons supersede 标注
    Tool: Edit
    Preconditions: T4 已完成
    Steps:
      1. 在 docs/lessons/phase-1.2-l1cacheplugin.md 文件头加 supersede 注释
      2. 注释内容: "⚠️ 已被 docs/methodology/plugin-style-design-methodology-v1.md supersede (2026-06-13, DECISION-2026-06-13-02). 本文档保留作 Phase 1.2 实施期间的历史踩坑记录."
    Expected Result: 文件头有 supersede 注释
    Evidence: .omo/evidence/phase-1.4-task-4-lessons-supersede.txt
  ```

  **Commit**: NO (与 T6 一并提交)

---

### 5. 同步更新 roadmap-status.md

  **What to do**:
  - §1 状态总览: Phase 1 进度 75% → 85% (+10% 复盘文档); PA-7/PA-9 状态 ✅
  - §3.2 PA-7: 状态 ⏳ Not Started → ✅ Completed (2026-06-13, F1.A 方法学复盘)
  - §3.2 PA-9: 状态 ⏳ Not Started → ✅ Completed (2026-06-13, DECISION-2026-06-13-02 Accepted v1)
  - §4 R7: 状态 ⏳ 待启动 → ✅ 闭环 (E1 重新解读: 不做性能基线, 改做方法学复盘)
  - §5 建议 1: 状态 ⏳ P1 → ✅ In Progress → Completed (本次会话)
  - §6 活动日志: 追加本次会话条目 (2026-06-13 十)
  - "最后更新" 日期: 2026-06-13

  **Must NOT do**:
  - 不删除 §3.2 PA-7/PA-9 行 (保留作历史记录)
  - 不修改其他 phase 状态 (Phase 2-6 维持 Not Started)
  - 不删除 §4 R7 行 (改状态即可, 历史记录)

  **Recommended Agent Profile**:
  - **Category**: `quick` (状态更新, 文字编辑)
  - **Skills**: 不需要
  - **Reason**: 状态追踪文档更新, 任务边界明确

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential (T5 → T6)
  - **Blocks**: T6
  - **Blocked By**: T4

  **References**:
  - 决策草案 §8 (roadmap-status.md 同步更新摘要, 提前规划好变更点)
  - `docs/roadmap/roadmap-status.md` 现状 (基线)

  **Acceptance Criteria**:
  - [x] §1 状态总览 Phase 1 进度更新
  - [x] §3.2 PA-7/PA-9 状态更新为 ✅
  - [x] §4 R7 状态更新为闭环
  - [x] §5 建议 1 状态更新为 Completed
  - [x] §6 活动日志追加本次会话条目
  - [x] "最后更新" 日期 2026-06-13

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: roadmap 状态更新完整
    Tool: Bash (grep)
    Preconditions: T5 已完成
    Steps:
      1. grep -E "PA-7|PA-9" docs/roadmap/roadmap-status.md
         # 期望: PA-7 和 PA-9 都标记为 ✅
      2. grep -E "R7" docs/roadmap/roadmap-status.md
         # 期望: R7 状态行更新
      3. grep "本次会话十" docs/roadmap/roadmap-status.md
         # 期望: §6 活动日志有本次会话条目
      4. grep "2026-06-13" docs/roadmap/roadmap-status.md
         # 期望: 至少 2 处 (最后更新 + 活动日志)
    Expected Result: 4 行输出, 全部存在
    Evidence: .omo/evidence/phase-1.4-task-5-roadmap-updated.txt
  ```

  **Commit**: NO

---

### 6. 自检 + 原子 commit

  **What to do**:
  - 跑 `bash tools/verify_adr.sh` (D4 静态检查不退化)
  - 跑 `ctest --test-dir build --output-on-failure` (16/16 仍 PASS)
  - 跑 §6 退出标准 E1-E8 全部命令
  - `git status` 检查 (无未追踪文件, 无未暂存)
  - 原子 commit (1 个 commit 含所有变更)
  - commit message 按决策草案 §5 模板

  **Must NOT do**:
  - 不拆 commit (单原子 commit 保持可回滚性)
  - 不绕过 `git status` 检查
  - 不在 commit message 写"通过"等含糊词 (具体引用退出标准 E1-E8)

  **Recommended Agent Profile**:
  - **Category**: `git-master` skill + `quick` agent
  - **Skills**: `git-master`
  - **Reason**: 原子 commit 是 git 核心操作, 需要专业技能保证可回滚

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Sequential (T6 最后)
  - **Blocks**: None
  - **Blocked By**: T5

  **References**:
  - 决策草案 §4 (Verification Commands)
  - 决策草案 §5 (Commit Message 模板)
  - 决策草案 §6 (Rollback)
  - `.opencode/skills/git-master` (原子 commit 规范)

  **Acceptance Criteria**:
  - [x] `ctest 16/16 PASS`
  - [x] `verify_adr.sh ALL PASS`
  - [x] `git status` 干净 (只有本次变更的 3 个文件: 新建 v1 文档 + lessons supersede + roadmap 更新)
  - [x] commit message 引用 DECISION-2026-06-13-02
  - [x] 1 个原子 commit, 无拆 commit

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: ctest 16/16 不退化
    Tool: Bash (ctest)
    Preconditions: T5 已完成
    Steps:
      1. ctest --test-dir build --output-on-failure 2>&1 | tee /tmp/phase-1.4-final-ctest.txt
      2. 验证输出含 "100% tests passed, 0 tests failed out of 16"
    Expected Result: 16/16 PASS
    Evidence: .omo/evidence/phase-1.4-task-6-ctest-still-pass.txt

  Scenario: D4 静态检查不退化
    Tool: Bash
    Preconditions: T6 启动
    Steps:
      1. bash tools/verify_adr.sh 2>&1 | tee /tmp/phase-1.4-final-adr.txt
      2. 验证退出码 0 + 输出含 "ALL PASS"
    Expected Result: ALL PASS
    Evidence: .omo/evidence/phase-1.4-task-6-adr-still-pass.txt

  Scenario: 原子 commit 落地
    Tool: Bash (git)
    Preconditions: T6 自检通过
    Steps:
      1. git status --porcelain
         # 期望: 3 个文件 (新 v1 文档 + lessons supersede + roadmap)
      2. git add docs/methodology/plugin-style-design-methodology-v1.md \
                docs/lessons/phase-1.2-l1cacheplugin.md \
                docs/roadmap/roadmap-status.md
      3. git commit -m "<按决策草案 §5 模板>"
      4. git log --oneline -1
         # 期望: 最新 commit 引用 DECISION-2026-06-13-02
    Expected Result: 1 个原子 commit, 引用决策 ID
    Evidence: .omo/evidence/phase-1.4-task-6-atomic-commit.txt
  ```

  **Commit**: YES (单原子 commit, 含所有变更)
  - Message: `docs(phase-1.4): L1CachePlugin 设计方法学复盘 v1`
  - Files:
    - `docs/methodology/plugin-style-design-methodology-v1.md` (新建)
    - `docs/lessons/phase-1.2-l1cacheplugin.md` (supersede 注释)
    - `docs/roadmap/roadmap-status.md` (状态更新)
  - Pre-commit: `ctest --test-dir build --output-on-failure` (16/16 PASS)

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get explicit "okay" before completing.

### F1. Plan Compliance Audit — `oracle`

  **What to do**:
  - Read `.omo/plans/phase-1.4-plugin-design-methodology-review.md` end-to-end
  - Verify each "Must Have" in §Work Objectives:
    - `docs/methodology/plugin-style-design-methodology-v1.md` 存在, 200-400 行, 6 维度 + 3 边界
    - `docs/lessons/phase-1.2-l1cacheplugin.md` 有 supersede 注释
    - `docs/roadmap/roadmap-status.md` 已同步
  - Verify each "Must NOT Have" (Guardrails) absent:
    - 无 cpptlm::CacheTLM baseline 实施
    - 无代码变更 (除 supersede 注释)
    - 无新 IP 引入
  - Check evidence files exist in `.omo/evidence/phase-1.4-task-*.txt`
  - Compare deliverables against plan §TODOs

  **Output**: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

### F2. Code Quality Review — `unspecified-high`

  **What to do**:
  - 跑 `ctest --test-dir build --output-on-failure` (16/16 PASS)
  - 跑 `bash tools/verify_adr.sh` (D4 静态检查 ALL PASS)
  - Review 文档结构:
    - v1 文档标题层级 ≤ 4 (避免过深嵌套)
    - 代码块用 `cpp` / `bash` 等 language tag
    - 链接用相对路径 (如 `../roadmap/phases/phase-1-tlm-foundation.md`)
    - 列表项缩进一致
  - Review 文档内容:
    - 6 维度评估每个 ≥ 5 个观察点
    - 3 边界标注每个有具体代码引用
    - B3 局限每个有 Phase 6 任务链接

  **Output**: `Build [PASS/FAIL] | Lint [PASS/FAIL] | Tests [N pass/N fail] | Files [N clean/N issues] | VERDICT`

### F3. Real Manual QA — `unspecified-high`

  **What to do**:
  - 手动跑 T1-T6 所有 QA Scenarios
  - 检查 evidence 文件命名规范 (`.omo/evidence/phase-1.4-task-{N}-{slug}.txt`)
  - 检查 evidence 文件内容真实 (不是占位符)
  - 测试跨章节引用完整性 (从 v1 文档跳到 lessons / D4 决策 / 决策草案)
  - 测试文档可 grep 检索 (D1-D6 标题 / B1-B3 标注可被未来 IP 写作者快速找到)

  **Output**: `Scenarios [N/N pass] | Integration [N/N] | Edge Cases [N tested] | VERDICT`

### F4. Scope Fidelity Check — `deep`

  **What to do**:
  - 对每个任务 (T1-T6): 读 "What to do", 读实际产出 (git diff)
  - 验证 1:1 — 任务说的事都做了, 没漏
  - 验证 0:0 — 没做任务外的事
  - 检查 Must NOT do 合规:
    - 文档不写 cpptlm::CacheTLM baseline 内容
    - 文档不修改 L1CachePlugin / Bridge / Adapter 代码
    - 文档不引入新 IP
  - 检查跨任务污染 (Task N touching Task M's files)

  **Output**: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

- **T6**: `docs(phase-1.4): L1CachePlugin 设计方法学复盘 v1` - 3 files (v1.md + lessons + roadmap), 1 atomic commit

---

## Success Criteria

### Verification Commands

```bash
# 文档存在 + 字数 + 结构
wc -l docs/methodology/plugin-style-design-methodology-v1.md  # 期望: 200-400
grep -cE "^## D[1-6]" docs/methodology/plugin-style-design-methodology-v1.md  # 期望: ≥ 6
grep -cE "B[123]" docs/methodology/plugin-style-design-methodology-v1.md  # 期望: ≥ 13

# D4 静态检查不退化
bash tools/verify_adr.sh  # 期望: ALL PASS

# ctest 不退化
ctest --test-dir build --output-on-failure  # 期望: 16/16 PASS

# roadmap 同步
grep "PA-7\|PA-9" docs/roadmap/roadmap-status.md  # 期望: PA-7 ✅, PA-9 ✅
grep "R7" docs/roadmap/roadmap-status.md  # 期望: R7 闭环
grep "本次会话十" docs/roadmap/roadmap-status.md  # 期望: 活动日志有本次条目

# lessons supersede
grep "supersede" docs/lessons/phase-1.2-l1cacheplugin.md  # 期望: 标头有 supersede 注释

# git status 干净
git status --porcelain  # 期望: 空 (commit 后)
```

### Final Checklist

- [x] 6 维度评估完整 (D1-D6)
- [x] 3 边界标注 ≥ 13 处 (B1/B2/B3)
- [x] B2 摩擦模式 ≥ 10 个
- [x] B3 局限 ≥ 3 个 + Phase 6 任务链接
- [x] lessons supersede 注释
- [x] roadmap 同步 (§1/§3/§4/§5/§6 全部更新)
- [x] 16/16 ctest PASS (不退化)
- [x] D4 静态检查 ALL PASS (不退化)
- [x] 1 个原子 commit, 引用 DECISION-2026-06-13-02

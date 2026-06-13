# Phase 1.4: L1CachePlugin 设计方法学复盘 — 决策草案 v1

> **决策ID**: DECISION-2026-06-13-02
> **决策日期**: 2026-06-13
> **决策状态**: **Proposed v1**（待本次 session 实施 + 验证后改 Accepted v1）
> **提出方**: Prometheus（基于用户对话 + Phase 1.3 全部子任务完成 + Phase 1.2 lessons 沉淀触发）
> **决策影响**: 验证 ChipForge 核心方法学 (Plugin 声明式电路设计) 是否站得住脚；为 Phase 2+ 多个 Plugin-style IP (L2/ICache/Interconnect) 提供设计模板；决定 Phase 6 完整框架是否基于 Plugin 范式
> **关联文档**:
> - `.omo/drafts/decision-plugin-framework-2026-06-08.md`（D4 Plugin-style 强制决策，父决策）
> - `.omo/drafts/decision-phase-1.3-bridge-2026-06-10.md`（v2, D1-D3）
> - `.omo/drafts/decision-phase-1.3d-extras-bridge-2026-06-13.md`（DECISION-2026-06-13-01, 姊妹决策, F1-F5 格式参考）
> - `docs/lessons/phase-1.2-l1cacheplugin.md`（7 类 15+ 模式教训, 复盘输入）
> - `docs/architecture/declarative-hybrid-framework.md`（方法学权威源）
> - `docs/roadmap/roadmap-status.md` §3 PA-7/PA-9

---

## 0. v1 修正说明（重要！）

**roadmap-status.md §3 PA-9 / PA-7 与 §5 建议 1** 中对 Phase 1.4 的描述（"cpptlm::CacheTLM baseline 对比"）是**错误解读**。Metis 实际验证后发现 `cpptlm::CacheTLM` 是 stub（`std::map` 全关联, 无淘汰, 5/50 cycle 硬编码），且无法配置为 256×64B direct-mapped，**作为性能基线没有意义**。

经用户 2026-06-13 对话澄清，Phase 1.4 的**真实目的**是：

> **用 L1CachePlugin 作为第一个具体例子，复盘 Plugin 声明式电路设计方法学**——评估这套方法学（at_stage 调度 / Payload 通信 / array_store 抽象 / uint_t<N> 编译期切换 / 无 tick() / 无状态机）是否有问题，是否能扩展到其他 IP（L2 / ICache / Interconnect）。

**本草案 E1-E5 重新解读如下**（对照原始 PA-9 E1-E5）：

| PA-9 原议题 | 错误解读（roadmap） | **正确解读（本草案 v1）** |
|------------|------------------|---------------------|
| E1: baseline 选型 | cpptlm::CacheTLM vs HybridCacheWrapper vs 手写 reference | **复盘对象**：L1CachePlugin 作为方法学复盘的"镜子"。可选对照：cpptlm::CacheTLM（tick() 风格反例）作为**方法学对比**（非性能对比） |
| E2: trace 对比工具 | 手写 vs gem5 m5out | **复盘维度**：代码可读性 / 范式合规 / TLM↔RTL 可移植性 / 阶段调度清晰度 / Payload 通信效率 |
| E3: 共享 traffic_gen | 同 Module vs 独立 seed | **复盘输入**：phase-1.2 lessons（7 类教训）+ L1Cache 源码 + D4 决策文档 + 后续 IP 设计草案 |
| E4: hit rate 容差 ±5% | 性能容差 | **方法学边界**：哪些 Plugin 写法在"方法学接受范围"内 vs "需要改进" |
| E5: 测试时长 1k vs 10k | 性能测试 | **复盘深度**：仅看 L1Cache（单 Plugin 例子）vs L1Cache + 准备中的 L2/ICache 设计草案（横向对比） |

---

## 1. Why

### 1.1 Phase 1.3 全部子任务完成的反思窗口

2026-06-13 Phase 1.3 全部 7 子任务落地（含 1.3d-extras ch_stream 注册 + full JSON e2e, 16/16 ctest PASS）。**这是 Plugin-style 第一个真实 IP 端到端跑通的里程碑**,也是**停下来审视方法学**的最佳时机：

- **L1CachePlugin（Phase 1.2, `e8deacc`）**—— 第一个 Plugin 业务代码，256 sets × 64B direct-mapped，4/4 单元测试 PASS
- **L1CacheTLMBridge（Phase 1.3a, `26fe7d2`）**—— 第一个框架层桥接，2/2 单元测试 PASS
- **L1CacheTLMBridgeAdapter（Phase 1.3d, `c8d1dd1`）**—— 第一个 cpptlm ModuleFactory 兼容层，5/5 e2e
- **L1CacheTLMBridgeAdapter ch_stream 4-字段窄桥（Phase 1.3d-extras, `387b8ca`）**—— 第一个跨 TLM↔RTL 协议转换，5/5 instantiateAll e2e

**4 个组件累计 16 个测试 PASS,但留下了未公开的方法学问题**——分散在 `docs/lessons/phase-1.2-l1cacheplugin.md` 的 7 类踩坑教训中。

### 1.2 现有 lessons 文档的局限

`docs/lessons/phase-1.2-l1cacheplugin.md` 是一份**踩坑清单**（"Phase 1.2 实施过程中踩了什么坑 / 怎么修"），粒度到"代码行级陷阱"（`if (cond) return;` 在 RTL 不可移植 / `at_stage` 闭包与 `node_of_logic_stage` 节点共享问题 / `pb.run()` 一次性执行等）。

**它没有回答的问题**:
- **方法学整体评估** —— Plugin 声明式电路设计作为一套**设计范式**,在 L1Cache 这个例子上,哪些维度是成功的、哪些是有摩擦的、哪些需要补充机制?
- **方法学边界** —— 在什么场景下 Plugin-style 写法**够用**?在什么场景下**不够用**?哪些限制是"范式本身的代价"(可接受)vs"框架实现的缺陷"(可改进)?
- **横向可复用性** —— 后续 L2Cache、ICache、Interconnect、CPU 等 Plugin 写 L1Cache 时遇到的同样问题,是否会**重复踩坑**?还是方法学能让新 IP 写作者"一次走对"?
- **向上兼容 Phase 6** —— 完整 PipeBuilder 框架(JSON 解析、CompareDriver、ScoreBoard、RTL 生成)实现后,Phase 1 写的 L1CachePlugin **业务代码不重写**的承诺,有多少信心?

### 1.3 方法学复盘 ≠ 性能基线对比

**易混淆的另一种解读**:"Phase 1.4 = 与 cpptlm::CacheTLM 做性能基线对比"。这来自 roadmap §1.4 + §2.1 的字面描述。

**为什么这是错的**:
1. `cpptlm::CacheTLM` 是 stub（`build/_deps/install/include/cpp-tlm/tlm/cache_tlm.hh:50-56`），构造签名 `(name, EventQueue*)`，无 sets/ways/block_size/policy 参数，**无法配置为 256×64B direct-mapped**
2. 内部用 `std::map<uint64_t, uint64_t>` 全关联 + 无淘汰,5/50 cycle 硬编码延迟 —— **与 L1CachePlugin 的行为模型根本不匹配**
3. 性能对比的两个实现几何不等价,任何 hit rate 差异都可归因于几何,无法作为"Plugin-style vs tick()-style"的方法学对比
4. 强对比只会浪费 1-2 天预算 + 关闭不了 R7 风险（baseline 选型不确定）

**正确的方法学对比**是: **同一个 cache 行为**(256×64B direct-mapped),用 Plugin-style 写(L1CachePlugin,~200 行) vs 用传统 tick() 写(假想参考,~80-100 行),在**方法学维度**(代码量 / 可读性 / 范式合规 / 调度清晰度 / 阶段分解合理性 / TLM↔RTL 切换成本)的对比。**不是性能数字的对比**。

### 1.4 Phase 1.4 的价值与时机

**价值**:
- **方法学是 ChipForge 的核心资产**—— Phase 0-6 所有阶段都基于 D4 Plugin-style 范式。若方法学在 L1Cache 上有隐藏问题,Phase 2+ (L2/ICache/Interconnect) 写更多 IP 时会**重复放大**
- **Phase 1.2 lessons 是"局部"信息,不是"全局"评估**—— 升级为"方法学 v1"才能横向指导后续 IP
- **Phase 6 完整框架投入的信心保障**—— 若 L1Cache 上方法学走通,Phase 6 投入(12-20 周)的 ROI 才有底
- **"评估设计方法学有没有问题"**(用户原话)的最佳时机—— 第一个真实例子跑通后,方法学的"摩擦点"已经暴露

**时机**:
- ✅ Phase 1.3 全部子任务完成(基础设施就绪)
- ✅ 16/16 ctest PASS(可验证产出)
- ✅ Phase 1.2 lessons 沉淀(输入材料)
- ✅ Phase 2 (bare-metal) 尚未启动(没有新 IP 写,正是复盘窗口)

---

## 2. What Changes（决议 F1-F5）

### F1: 复盘对象（E1 重解读）

**决议 F1.A: L1CachePlugin 作为唯一复盘对象**（推荐）

理由:
- L1Cache 是 Phase 0-1.3 累计 7 个子任务的**唯一业务 IP**,有完整的实现 + Bridge + Adapter + JSON spec + ch_stream 注册 + 单元测试
- 4 层抽象(L1CachePlugin / L1CacheTLMBridge / L1CacheTLMBridgeAdapter / JSON spec)覆盖了 Plugin 范式的所有关键决策点
- 复盘颗粒度可控(1-2 天预算)

**决议 F1.B: 不引入 cpptlm::CacheTLM 作为对比 baseline**（否决 E1 错误解读）

理由:
- cpptlm::CacheTLM 是 stub,几何与 L1CachePlugin 不匹配,无法做有意义的对比
- 即便做"方法学对比"(Plugin-style vs tick()-style),也只能作为**文字说明**,不强制实施实现
- 时间预算 1-2 天,实施 cpptlm baseline SoC JSON + 对比测试会大幅超出

**决议 F1.C: 复盘输出 = 《Plugin 声明式电路设计方法学 v1》文档**（F1 落地产物）

文档定位:
- 升级 `docs/lessons/phase-1.2-l1cacheplugin.md`（踩坑清单）为方法学评估
- **不是**"最佳实践指南"（那是 Phase 2+ 多 IP 后才有素材）
- **是**"L1Cache 这个例子上的方法学反射镜"—— 把 Phase 0-1.3 实施过程中遇到的设计问题,按方法学维度整理为**可被未来 IP 复用**的判断标准

### F2: 复盘维度（E2 重解读）

**决议 F2: 6 个评估维度**

| # | 维度 | 评估问题 | 参考输入 |
|---|------|---------|---------|
| D1 | **代码可读性** | L1CachePlugin 主体代码（~200 行）读起来是否清晰？是否有"为框架而写"的冗余？ | `ip/cache/tlm/L1CachePlugin.{h,cpp}` |
| D2 | **范式合规性** | D4 强制（无 tick() / 无状态机 / Bundle 字段用 uint_t<N> / 阶段用 at_stage() / 跨阶段用 Payload<T>）在 L1Cache 上是否完整遵守？哪些边界模糊？ | `tools/check_plugin_decision.sh` + D4 决策文档 |
| D3 | **TLM↔RTL 可移植性** | 业务代码切到 RTL 时哪些地方需要修改？`array_store` 抽象是否有效降低切换成本？`extract_idx/tag` helper 是否为位选迁移预留？ | `docs/lessons/phase-1.2-l1cacheplugin.md` §2.3-2.5 + ADR-040 |
| D4 | **阶段调度清晰度** | lookup + refill 两阶段拆分是否自然？`pb.run()` 一次性执行所有 at_stage 回调的语义是否清晰？`declare_substage` 声明不对应线程调度的边界？ | `L1CachePlugin.cpp:131-136` + lessons §1.1-1.3 |
| D5 | **Payload 通信效率** | Payload<T> Key 全局静态 + 按指针身份匹配,是否够直观？跨 PipeNode 隔离机制是否清晰？ | lessons §1.1 + §五 |
| D6 | **测试便利性** | Plugin 实例 + pb 注册 + 裸指针 helper 模式,是否对单元测试作者友好？`helper->issue_request(...)` 这种"内部 API"是否泄漏？ | `test_l1_cache_plugin_unit.cpp` + lessons §四 |

每个维度输出:
- **现状评估**（L1Cache 上的具体表现 + 代码引用）
- **方法学判断**（成功 / 有摩擦 / 需要补充机制 / 需要文档化）
- **对未来 IP 的建议**（"L2Cache 写作者应该知道什么"）

### F3: 复盘输入（E3 重解读）

**决议 F3: 5 类输入材料**

| # | 输入 | 用途 |
|---|------|------|
| I1 | `docs/lessons/phase-1.2-l1cacheplugin.md`（7 类 15+ 教训） | 已沉淀的踩坑清单 → 升级为方法学评估 |
| I2 | `ip/cache/tlm/L1CachePlugin.{h,cpp}`（~200 行 Plugin 业务代码） | 主要复盘对象 |
| I3 | `src/cf_plugin/bridge/l1_cache_bridge.{h,cpp}` + `l1_cache_bridge_adapter.{h,cpp}`（框架层 + cpptlm 适配层） | 框架层与业务层的边界 |
| I4 | `docs/architecture/declarative-hybrid-framework.md`（D4 + Bundle 三层 + Plugin 范式权威源） | 方法学参照标准 |
| I5 | `.omo/drafts/decision-plugin-framework-2026-06-08.md` + `decision-phase-1.3-bridge-2026-06-10.md` + `decision-phase-1.3d-extras-bridge-2026-06-13.md` | 决策上下文,识别"哪些是决策约束,哪些是设计选择" |

**否决 F3.B**: 用 cpptlm::CacheTLM 作为复盘输入
- 理由: stub 性质导致"传统 tick() 风格"的代表不准确;且非必需

### F4: 方法学边界（E4 重解读）

**决议 F4: 3 类边界识别**

| 边界类型 | 定义 | L1Cache 上的表现 | 处置 |
|---------|------|----------------|------|
| **B1: 范式接受范围** | "Plugin-style 写法自然、不需要妥协就能表达" | at_stage 注册、Payload 通信、uint_t<N> 字段、array_store 抽象 | ✅ 接受,作为方法学核心 |
| **B2: 范式摩擦范围** | "Plugin-style 写法能表达,但需要 helper / 包装 / 注释才能讲清楚" | 位提取 helper（`extract_idx/tag`）、裸指针 helper API（`issue_request`）、`if/else` 替代 `if (cond) return;` | ⚠️ 文档化摩擦点 + 提供模式 |
| **B3: 范式局限范围** | "Plugin-style 写法的硬限制,需要外部机制补充" | `pb.run()` 一次性执行(无 cycle 精度)、`declare_substage` 声明不对应线程调度、Payload Key 跨翻译单元不共享 | 🚧 识别为 Phase 6 框架升级的输入 |

**F4 落地产物**:
- 方法学文档每章节末尾标"边界类型" (B1/B2/B3)
- B2 类摩擦点给出**具体模式**（代码模板）供未来 IP 复用
- B3 类局限**显式**记录为"Phase 6 框架待解问题",不掩盖

### F5: 复盘深度（E5 重解读）

**决议 F5.A: 单例子深复盘（推荐）**

- 仅看 L1CachePlugin 一个 IP 例子
- 深度: 6 个维度 × 7-15 模式 = 42-90 个评估点
- 输出: 《Plugin 声明式电路设计方法学 v1》文档,~200-400 行 markdown

**否决 F5.B**: 横向看 L2/ICache/Interconnect 设计草案
- 理由: 这些 IP 尚未实现,设计草案不存在,无法做横向对比
- Phase 2+ 启动后,可在 Phase 2.5 之后做横向评估（"方法学 v2"）

### F6: 退出标准

| 编号 | 标准 | 验证方式 |
|------|------|---------|
| E1 | `docs/methodology/plugin-style-design-methodology-v1.md` 存在,字数 200-400 行 | `wc -l docs/methodology/plugin-style-design-methodology-v1.md` |
| E2 | 6 个评估维度（D1-D6）全部覆盖,每维度有"现状 + 判断 + 建议" | 文档结构化目录 |
| E3 | 3 类方法学边界（B1/B2/B3）显式标注 | grep "B[123]" 文档 |
| E4 | 至少 10 个 B2 摩擦点带具体代码模式 | grep "B2" 文档 |
| E5 | 至少 3 个 B3 局限点显式链接到 Phase 6 框架升级任务 | grep "B3" 文档 |
| E6 | `docs/lessons/phase-1.2-l1cacheplugin.md` 标"已被 v1 文档 supersede,保留作历史参考" | 文档头注 |
| E7 | ctest 16/16 仍 PASS（复盘文档不引入代码变更） | `ctest --test-dir build --output-on-failure` |
| E8 | `roadmap-status.md` 同步更新:PA-9 状态 ✅,R7 风险闭环(或降级),§5 建议 1 标记 in-progress | git diff docs/roadmap/roadmap-status.md |

---

## 3. Out of Scope

- **性能基线对比**（与 cpptlm::CacheTLM 比 hit rate / 延迟）—— Metis 验证 cpptlm::CacheTLM 是 stub,几何不等价,无意义
- **实现 cpptlm::CacheTLM Plugin-style 版本作为方法学对比** —— 实施成本高,文字描述已够
- **多 IP 横向对比**（L2/ICache/Interconnect） —— 这些 IP 尚未实现,无可比对象
- **Phase 6 完整框架的实施方案** —— B3 局限点只识别不解决
- **JSON 解析 / CompareDriver / ScoreBoard 设计** —— Phase 6 范围
- **RTL 升级 Phase 5** —— B3 局限点的解决时机,不在 Phase 1.4
- **方法学 v2 / v3** —— Phase 2+ 多 IP 实施后再做
- **修改 L1CachePlugin / Bridge / Adapter 代码** —— 复盘文档是**描述性**的,不改业务代码

---

## 4. Verification Commands（commit 前必跑）

```bash
cd /workspace/project/ChipForge

# 文档存在 + 字数
ls -la docs/methodology/plugin-style-design-methodology-v1.md  # 期望: 文件存在
wc -l docs/methodology/plugin-style-design-methodology-v1.md   # 期望: 200-400 行

# 6 维度结构化目录 + 3 类边界标注
grep -E "^## " docs/methodology/plugin-style-design-methodology-v1.md  # 期望: D1-D6 标题存在
grep -cE "B[123]" docs/methodology/plugin-style-design-methodology-v1.md  # 期望: ≥ 13 (B2×10 + B3×3)

# Phase 1.2 lessons 文档 supersede 标注
grep -i "supersede\|已被.*v1" docs/lessons/phase-1.2-l1cacheplugin.md  # 期望: 标头有 supersede 注释

# ctest 仍 PASS
ctest --test-dir build --output-on-failure 2>&1 | tail -5
# 期望: 100% tests passed, 0 tests failed out of 16

# verify_adr.sh 不退化
bash tools/verify_adr.sh 2>&1 | tail -3
# 期望: ✓ ALL PASS (Phase 1.3 状态保持)
```

---

## 5. Commit Message 模板

```
docs(phase-1.4): L1CachePlugin 设计方法学复盘 v1

- docs/methodology/plugin-style-design-methodology-v1.md (新建):
  + 6 维度评估: D1 可读性 / D2 范式合规 / D3 TLM↔RTL 可移植性
                 / D4 阶段调度 / D5 Payload 通信 / D6 测试便利
  + 3 类边界标注: B1 接受范围 / B2 摩擦范围 / B3 局限范围
  + 42-90 个评估点, ≥10 B2 模式 + ≥3 B3 链接 Phase 6
- docs/lessons/phase-1.2-l1cacheplugin.md:
  + 标头注 "已被 v1 文档 supersede, 保留作历史参考"
- docs/roadmap/roadmap-status.md:
  + §3 PA-7 状态 ✅ (Phase 1.4 完成)
  + §3 PA-9 状态 ✅ (DECISION-2026-06-13-02 Accepted)
  + §4 R7 风险闭环 (E1 重新解读: 不做性能基线, 改做方法学复盘)
  + §5 建议 1 标记 in-progress → completed
  + §6 活动日志追加本次会话

决策依据: DECISION-2026-06-13-02 (F1.A + F2 6维度 + F3 5输入 + F4 3边界 + F5.A 单例子)
无代码变更 (16/16 ctest 保持 PASS)
退出标准 E1-E8 全部通过
```

---

## 6. Rollback

```bash
cd /workspace/project/ChipForge
git revert <commit-hash>  # 单原子 commit, 一键回滚
# 或:
git rm docs/methodology/plugin-style-design-methodology-v1.md
git checkout HEAD~1 -- docs/lessons/phase-1.2-l1cacheplugin.md docs/roadmap/roadmap-status.md
```

---

## 7. 决议表（F1-F5）

| # | 决议 | 状态 |
|---|------|------|
| **F1** | 复盘对象 = L1CachePlugin（否决 cpptlm::CacheTLM 性能基线, 改为方法学复盘） | **Proposed** |
| **F2** | 6 个评估维度（D1-D6） | **Proposed** |
| **F3** | 5 类复盘输入（I1-I5） | **Proposed** |
| **F4** | 3 类方法学边界标注（B1/B2/B3） | **Proposed** |
| **F5** | 复盘深度 = 单例子深复盘（v1, 推迟 v2 到 Phase 2+） | **Proposed** |
| **F6** | E1-E8 退出标准 | **Proposed** |

---

## 8. 附:roadmap-status.md 同步更新摘要

| 章节 | 变更 |
|------|------|
| §1 状态总览 | Phase 1 进度 75% → 85%（+10% 复盘文档）; PA-7/PA-9 ✅ |
| §3.2 PA-7 | 状态: ⏳ Not Started → ✅ Completed (2026-06-13, F1.A 方法学复盘) |
| §3.2 PA-9 | 状态: ⏳ Not Started → ✅ Completed (2026-06-13, DECISION-2026-06-13-02 Accepted v1) |
| §4 R7 | 状态: ⏳ 待启动 → ✅ 闭环 (E1 重新解读: 不做性能基线, 改做方法学复盘) |
| §5 建议 1 | 状态: ⏳ P1 → ✅ In Progress → Completed (本次会话) |
| §6 活动日志 | 追加本次会话十: Phase 1.4 方法学复盘 v1 落地 |

---

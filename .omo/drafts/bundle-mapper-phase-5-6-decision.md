# 决策草案: Phase 5/6 BundleMapper 实施路径

> **决策ID**: DECISION-2026-06-12-01 (待用户最终确认 D1-D5)
> **决策日期**: 2026-06-12
> **决策状态**: **Proposed (待 Phase 5 启动时确认)**
> **提出方**: Prometheus (基于 2026-06-12 架构对齐审计 DRIFT-1)
> **决策影响**: Phase 5/6 BundleMapper 实施路径; Phase 1 业务代码迁移性
> **关联文档**: `docs/architecture/overview.md:125`, `docs/architecture/interface-design.md` §1.0, `docs/architecture/adr.md` ADR-024, `bundles/README.md`, `.omo/drafts/fix-plan-2026-06-12-design.md` §3 Change 2

---

## 1. 决策背景

2026-06-12 架构对齐审计发现 **DRIFT-1 (Bundle 字段类型双轨制)**: 架构文档 (`interface-design.md` §1-2) 描述 Bundle 用 `ch_uint<N>` + `bundle_base<Self>` + `CH_BUNDLE_FIELDS_T(...)` (CppHDL RTL 风格), 但 Phase 1 实际代码 (`bundles/mem_bundles.h`) 用 `cf::plugin::uint_t<N>` POD struct (Plugin-style D4 风格)。

审计建议: **不修改代码** (Phase 1 用 POD 是 D4 强制正确行为), 而是**文档化** Bundle 形态分阶段演进, 给 Phase 5/6 BundleMapper 实施提供 anchor。

本决策草案不实施 BundleMapper, 仅明确:
1. 实施期: Phase 5 启动时 (预计 2026-12 路线图 §6 M6 之前)
2. 实施路径: BundleMapper 模板 + 业务代码 0 改动
3. 候选决议: 5 项 (D1-D5), 留给 Phase 5 启动 session 决定

---

## 2. 候选决议 (5 项)

### D1: BundleMapper 触发条件

**候选**:
- **A** (推荐): 实施第一个 `ip/cache/rtl/L1CacheRtl.h` (Phase 5 L1CacheRtl) 时, 立即实施 BundleMapper
- **B**: 推迟到 Phase 6 完整 PipeBuilder 框架实施期, 与调度算法 + JSON 解析同步
- **C**: 推迟到 Phase 5 末, M6 (RTL 协同验证) 之前, 给 L1CacheRtl 一个 TLM 周期

**推荐**: A — L1CacheRtl 是第一个 RTL IP, 立刻需要 BundleMapper 才能与 TLM 协同验证

- [ ] **D1 决议待 Phase 5 启动 session 确认**: 选择 A / B / C, 推荐 A

### D2: BundleMapper 形态

**候选**:
- **A** (推荐): 模板特化 `BundleMapper<cf::bundles::CacheReq, bundles::CacheReqBundle>`, 一对一映射
- **B**: 通配模板 `BundleMapper<SourceT>` + SFINAE, 自动推导目标类型
- **C**: 手动逐字段赋值, 不引入模板 (简单但 Phase 5 升级痛)

**推荐**: A — 6 个 Bundle 类型有限, 一对一映射可控; B 复杂度高, C 与 Phase 6 自动化方向冲突

- [ ] **D2 决议待 Phase 5 启动前确认**: 选择 A / B / C, 推荐 A

### D3: 业务代码迁移策略

**候选**:
- **A** (推荐): 业务代码 0 改动, 仅切换 `bundles/mem_bundles.h` 包含 (原 cf::bundles 改为 bundles)
- **B**: 业务代码小改 (5-10 行/类), 用 `using BundleT = std::conditional_t<is_tlm, cf::bundles::*, bundles::*>` 切换
- **C**: 业务代码大改, 完全统一到 `bundles::*` 命名空间

**推荐**: A — 业务代码 0 改动是 Phase 0 投入变现的关键证据 (D4 决策 §3)

- [ ] **D3 决议待 Phase 5 启动 session 确认 0 改动**: 选择 A / B / C, 推荐 A

### D4: 验证策略

**候选**:
- **A** (推荐): TLM + RTL 协同仿真 (Phase 5 COMPARE 模式, `overview.md:69-71`), 执行迹完全一致 (±5% 延迟容差)
- **B**: Spike co-simulation, 对比 ISS 黄金参考
- **C**: riscv-arch-test 合规测试 + BundleMapper 编译期断言

**推荐**: A — 复用 Phase 5 已规划的 COMPARE 模式; B/C 是更深入验证, 推迟到 Phase 5 末

- [ ] **D4 决议待 Phase 5 M6 (COMPARE 模式) 确认**: 选择 A / B / C, 推荐 A

### D5: Bundle 命名空间清理

**候选**:
- **A** (推荐): Phase 5 时, `cf::bundles::*` 改名为 `bundles::*` (或别名), 与 cpptlm 命名空间对齐
- **B**: 保留 `cf::bundles::*` 不变, BundleMapper 显式跨命名空间
- **C**: 引入 `chipforge::bundles::*` 第三套命名空间

**推荐**: A — 减少概念数量; B 维护负担; C 引入冗余

- [ ] **D5 决议待 Phase 5 启动 session 同步改名确认**: 选择 A / B / C, 推荐 A

---

## 3. 工作量估算

| 任务 | 工时 |
|------|------|
| 实施 BundleMapper 6 模板特化 (6 个 Bundle) | 1-2 天 |
| 切换 `bundles/mem_bundles.h` 命名空间 | 0.5 天 |
| 验证 TLM/RTL 协同 (COMPARE 模式) | 1 天 |
| 跑单元测试 + ctest 验证无回归 | 0.5 天 |
| 更新文档 + 决策归档 | 0.5 天 |
| **合计** | **3-4 天** |

---

## 4. 等待 Phase 5 启动时确认 (5 项)

| 决议 | 推荐 | 决定时机 |
|------|------|----------|
| D1 BundleMapper 触发 | A | Phase 5 启动 session |
| D2 形态 | A | Phase 5 L1CacheRtl 实施前 |
| D3 业务代码迁移 | A | Phase 5 启动时确认 0 改动 |
| D4 验证策略 | A | Phase 5 M6 (COMPARE 模式) |
| D5 命名空间 | A | Phase 5 启动时同步改名 |

---

## 5. 决策追溯

- **本草案触发**: 2026-06-12 架构对齐审计 DRIFT-1
- **设计依据**: ADR-024 ⚠️ 状态 + `interface-design.md` §1.0 (本次新增) + `bundles/README.md` §2 (本次新增)
- **canonical 推迟**: `plugin-framework.md:129` + `bundles/README.md:102` (Phase 5/6 推迟)
- **不影响 Phase 1**: 本次仅草案, 不实施; 业务代码 0 改动是承诺

---

## 6. 影响范围 (Phase 5 启动时复核)

| 模块 | 影响 | 备注 |
|------|------|------|
| `bundles/mem_bundles.h` | 命名空间重命名 (D5) + Phase 5 Bundle 定义补全 | 6 个 Bundle 类型 |
| `ip/cache/tlm/L1CachePlugin.h` | 0 改动 (D3 推荐 A) | 仅切换 include 头 |
| `ip/cache/tlm/MemoryPlugin.h` | 0 改动 (D3 推荐 A) | 仅切换 include 头 |
| `src/cf_plugin/bridge/l1_cache_bridge.h` | 0 改动 (D3 推荐 A) | 仅切换 include 头 |
| 新增 `core/bundle/bundle_mapper.h` (CppHDL 侧) | 6 模板特化 | D2 推荐 A |
| `docs/architecture/adr.md` ADR-024 | 状态从 ⚠️ → ✅ | Mapper 实施完成后更新 |

---

## 7. 风险与回滚

| 风险 | 缓解 |
|------|------|
| BundleMapper 模板特化编译期错误定位难 | 引入 `static_assert` 检查字段数 + 名称一致 |
| 业务代码 0 改动的承诺被破坏 | D3 推荐 A 强制; 任何业务代码改动需 Phase 5 启动 session 重新评估 |
| `cf::bundles::*` → `bundles::*` 命名空间改动破坏下游 | D5 推荐 A 阶段同步改名 + 提供 1-2 release 弃用 alias |
| Phase 5 未按路线图启动 | 决策草案有效期至 Phase 6 启动; 超过 1 年未决则重审 |

---

## 8. 相关 ADR / 决策草案

- **ADR-024** (`docs/architecture/adr.md`): Bundle 三层分层 (本次增补"形态切换"小节)
- **D4 Plugin-style 强制** (`docs/architecture/plugin-framework.md` §1): 业务代码无 tick(), Bundle 字段用 `uint_t<N>`
- **decision-plugin-framework-2026-06-08.md** (`.omo/drafts/`): Phase 0 plugin 框架决策
- **decision-phase-1.3-bridge-2026-06-10.md** (`.omo/drafts/`): Phase 1.3 桥接决策
- **fix-plan-2026-06-12-design.md** §3 Change 2 (`.omo/drafts/`): 本决策草案的源设计

---

## 9. 启动 Phase 5 时 checklist

- [ ] D1 (触发条件) 决议: Phase 5 启动 session 决定
- [ ] D2 (Mapper 形态) 决议: Phase 5 L1CacheRtl 实施前决定
- [ ] D3 (业务代码迁移) 决议: Phase 5 启动时确认 0 改动
- [ ] D4 (验证策略) 决议: Phase 5 M6 (COMPARE 模式) 实施前决定
- [ ] D5 (命名空间) 决议: Phase 5 启动时同步改名
- [ ] ADR-024 状态从 ⚠️ → ✅: BundleMapper 6 模板特化编译通过后更新
- [ ] `docs/architecture/overview.md` §6 路线图 M6 标记完成: TLM/RTL 协同验证通过后更新

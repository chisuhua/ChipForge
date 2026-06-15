# ip/cpu 实施指南 v2.0 — 决策快照与入口

> **本文件位置**: `ip/cpu/docs/cpu_implementation_guide_v2.0.md`
> **版本**: v2.0 (Accepted)
> **状态**: 🟢 **Accepted** — 用户 2026-06-15 确认 F1-F6 全部决议 + 议题 1-8 全部选择
> **决策 ID**: DECISION-2026-06-15-02 (v2.0 完整版, 取代 v2.0 Proposed 草稿)
> **取代关系**: 全面取代 v1.0 通用 RISC-V TLM 指南 + v1.1 Phase 2 bare-metal 适配版 (均已废)
> **核心目标**: 用声明式电路方法学 (Plugin-style) 设计 ISA 与 CPU 架构相互独立的 RISC-V 核, 为以后 CPU 架构探索 (DSE) 打下基础

> **本文件作用**: **F1-F6 决议 + 议题 1-8 完整记录**, 集中可追溯。 **同时作为 v2.0 入口**, 链向拆分后的 4 类文档。
>
> **拆分说明** (2026-06-16): 原本 1220 行单文件混杂"决策/架构/规划/看板"四类内容。 已拆分为:
> - [`blueprint.md`](blueprint.md) — **静态架构蓝图** (微架构/目录/Plugin 套件/CpuFactory/cf_plugin 扩展点/方法学复用/VexRiscv 关系)
> - [`implementation-plan/`](implementation-plan/) — **总体实施规划** (范围/议题 1-8 实施层决策/测试金字塔/风险/M1-M5 总览) **+ 各阶段详细任务** (M1..M5)
> - [`status.md`](status.md) — **任务状态看板** (49 个 M1.x 任务粒度 PASS/FAIL/进度%)
>
> 本文件**仅保留** §3 决议与议题选择, 决策不再分散。 架构细节去 `blueprint.md`, 实施去 `implementation-plan/`, 状态去 `status.md`。

---

## 目录

- [1. 决策快照: F1-F6 决议 + 议题 1-8 选择](#1-决策快照-f1-f6-决议--议题-1-8-选择)
  - [1.1 决议草案 (F1-F6) — 全部 Accepted](#11-决议草案-f1-f6--全部-accepted)
  - [1.2 议题 1-8 选择 — 全部用户回复](#12-议题-1-8-选择--全部用户回复)
  - [1.3 议题 1 备注 — 解决 VexRiscv 思路 (用户主动提出)](#13-议题-1-备注--解决-vexriscv-思路-用户主动提出)
- [2. 文档状态: Accepted](#2-文档状态-accepted)
- [3. 拆分迁移说明 (2026-06-16)](#3-拆分迁移说明-2026-06-16)

---

## 1. 决策快照: F1-F6 决议 + 议题 1-8 选择

> **本节是 v2.0 决策的"唯一真相源"**。 任何对决议/议题选择的修订必须先改本节, 然后同步到 `blueprint.md` / `implementation-plan/`。

### 1.1 决议草案 (F1-F6) — 全部 Accepted

| # | 决议 | 状态 | 关键内容 |
|---|------|------|---------|
| **F1** | 范围 = ip/cpu 模块实施 (RISC-V RV32I/RV64I + M + Zicsr + Zifencei), 5/3 级流水线, TLM_ONLY | 🟢 **Accepted** | 排除 FPU/Vector/Multi-core/7级 (Phase 5+) |
| **F2** | 完全复用 multi_isa_architecture.md v2.0 的目录结构与 Plugin 分类 | 🟢 **Accepted** | 与权威设计 1:1 对齐 |
| **F3** | 复用 L1CachePlugin Phase 1.4 6 维度方法学 (B1 接受) | 🟢 **Accepted** | D1-D6 全维度复盘已沉淀在 methodology 文档 |
| **F4** | riscv-tests/riscv-arch-test/Spike/Python 全部推迟 (本期仅 CppTLM) | 🟢 **Accepted** | Phase 1.5 仅 CppTLM 声明式电路, 测试套件下一阶段 |
| **F5** | Phase 1 仅 TLM_ONLY, RTL/COMPARE 推迟到 Phase 5 | 🟢 **Accepted** | `ImplMode::TLM` 是本期末唯一值 |
| **F6** | RISC-V 与 ARM ISA 隔离 (`arch/riscv/` + 预留 `arch/arm/`), 为多 ISA 集成打基础 | 🟢 **Accepted** | 本期仅 riscv, arm/ 不创建 |

### 1.2 议题 1-8 选择 — 全部用户回复

| # | 议题 | 用户选择 | 选项 A | 选项 B | **选项 C (用户选)** |
|---|------|---------|--------|--------|----------------------|
| **1** | core/ 框架层实施深度 | **C** | 完整 1:1 | 最小可用 | **复用现有 cf_plugin + 扩展** |
| **2** | Plugin 拆分粒度 | **B** | 11 全部 | **5 个核心优先** | 5+6 stub |
| **3** | RegFilePlugin 物理实现 | **B+C** (同一个东西) | 纯 lambda | **array_store (cf_plugin 已有 storage.h)** | **复用 cf_plugin storage.h** |
| **4** | JSON 字段名 | **B** | 沿用旧字段 | **multi_isa v2.0 §6.1 标准** | 兼容两套 |
| **5** | Plugin 注册顺序 | **B** | 严格 multi_isa 顺序 | **CpuFactory 内置 PluginOrder 列表** | JSON `plugins[]` 数组 |
| **6** | 联调路径 | **C** | 实施 MemoryTLM | L1Cache 2KB 限制 | **picolibc 内存区域绕过 MemoryTLM stub** |
| **7** | ARM 目录 | **A** | **不创建, 仅 riscv** | 创建 arch/arm/ stub | — |
| **8** | 验证范围 | **B** | build_cpu + riscv-tests 工具链 | **build_cpu + 手工编译最小 ELF** | 单 Plugin 单元测试 |

> **议题详细论证与实施层细节** 见 [`implementation-plan/README.md` §3](implementation-plan/README.md)。

### 1.3 议题 1 备注 — 解决 VexRiscv 思路 (用户主动提出)

> 用户在议题 1 选择 C (复用 cf_plugin) 后, 主动备注:
> "**再实施过程中, 同时解决 VexRiscv 的思路**"

含义:
- 复用 cf_plugin Phase 0 (PluginBase / Payload / PipeNode / PipeBuilder / CtrlLink) 5 个头文件
- 解决 VexRiscv 思路 = 在实施 CPU Plugin 过程中, 借鉴 VexRiscv Plugin 设计模式, 解决 multi_isa v2.0 §2-4 描述的 PipeLink / DirectLink / declare_substage 等尚未实现的 API
- VexRiscv 的 40+ 复杂 Plugin (Cache, MMU, CSR, Debug, Interrupt) 共享同一流水线骨架, 我们参考其**Plugin 分类** (核心/扩展/可选) 但不直接移植其 Scala 代码
- 实施过程中如发现 cf_plugin 缺 API (例如 declare_substage, PipeLink), 应**扩展 cf_plugin** 而非在 ip/cpu 内部重新实现

> **VexRiscv 借鉴的具体方式** 见 [`blueprint.md` §8](blueprint.md)。

---

## 2. 文档状态: Accepted

- **状态**: 🟢 **Accepted** — 用户 2026-06-15 接受 F1-F6 + 议题 1-8
- **关联**: DECISION-2026-06-15-02 (v2.0 完整版)
- **取代**: v1.0 通用 RISC-V TLM 指南 + v1.1 适配版 (均已废)
- **位置**: `ip/cpu/docs/cpu_implementation_guide_v2.0.md` (本文件, **决策入口**)
- **拆分后** (2026-06-16): 4 类文档, 1 commit, 1220 行 → ~230 行 (本入口) + ~700 行 (blueprint) + ~400 行 (impl-plan) + ~250 行 (status)
- **基础文档** (权威引用, 不变):
  - `ip/cpu/docs/multi_isa_architecture.md` v2.0 (权威设计)
  - `docs/architecture/plugin-framework.md` v1.0 (Plugin 范式)
  - `docs/architecture/declarative-hybrid-framework.md` v2.1.0 (TLM/RTL 混合)
  - `docs/methodology/plugin-style-design-methodology-v1.md` (L1CachePlugin 6 维度方法学)
  - `docs/lessons/phase-1.2-l1cacheplugin.md` (行级踩坑清单)
  - `bundles/README.md` (D4 Plugin-style 强制)
  - `bundles/mem_bundles.h` (已实施 6 个 Bundle)

---

## 3. 拆分迁移说明 (2026-06-16)

### 3.1 拆分原因

原 v2.0 文件 1220 行混杂 4 类内容, 4 类读者需要不同切片:
- **架构师** 看微架构、Plugin 套件、cf_plugin 扩展点 (改动少, 长期参考)
- **实施者** 看 M1-M5 任务清单、议题 1-8 实施层细节 (随实施滚动更新)
- **项目管理者** 看 49 个 M1.x 任务 PASS/FAIL 状态 (高频更新)
- **决策审计** 看 F1-F6 + 议题 1-8 完整选择 (只读, 不变)

### 3.2 拆分映射表 (原 §X → 新位置)

| 原 §X | 原章节标题 | 性质 | **新位置** |
|-------|-----------|------|-----------|
| §1  | 核心定位 | 静态蓝图 | [`blueprint.md` §1](blueprint.md) |
| §2  | 核心原则 | 静态蓝图 | [`blueprint.md` §2](blueprint.md) |
| **§3** | **F1-F6 决议 + 议题 1-8** | **决策快照** | **本文件 §1 (保留)** |
| §4  | 范围 / 不在范围 | 总体实施规划 | [`implementation-plan/README.md` §1-2](implementation-plan/README.md) |
| §5  | 目录结构 | 静态蓝图 | [`blueprint.md` §3](blueprint.md) |
| §6  | Plugin 套件 | 静态蓝图 | [`blueprint.md` §4](blueprint.md) |
| §7  | CpuFactory | 静态蓝图 | [`blueprint.md` §5](blueprint.md) |
| §8  | 复用 cf_plugin | 静态蓝图 | [`blueprint.md` §6](blueprint.md) |
| §9  | Plugin 拆分粒度 | 实施层 | [`implementation-plan/README.md` §3.1](implementation-plan/README.md) |
| §10 | RegFile array_store | 实施层 | [`implementation-plan/README.md` §3.2](implementation-plan/README.md) |
| §11 | JSON 字段名 | 实施层 | [`implementation-plan/README.md` §3.3](implementation-plan/README.md) |
| §12 | Plugin 注册顺序 | 实施层 | [`implementation-plan/README.md` §3.4](implementation-plan/README.md) |
| §13 | 联调路径 picolibc | 实施层 | [`implementation-plan/README.md` §3.5](implementation-plan/README.md) |
| §14 | ISA 切换机制 | 实施层 | [`implementation-plan/README.md` §3.6](implementation-plan/README.md) |
| §15 | 验证范围 | 实施层 | [`implementation-plan/README.md` §3.7](implementation-plan/README.md) |
| §16 | 方法学复用 | 静态蓝图 | [`blueprint.md` §7](blueprint.md) |
| §17 | 与 VexRiscv 关系 | 静态蓝图 | [`blueprint.md` §8](blueprint.md) |
| §18 | 三级测试金字塔 | 总体实施规划 | [`implementation-plan/README.md` §4](implementation-plan/README.md) |
| §19 | 风险与边界 | 总体实施规划 | [`implementation-plan/README.md` §5](implementation-plan/README.md) |
| §20 | 里程碑 M1-M5 | 总体实施规划 | [`implementation-plan/README.md` §6](implementation-plan/README.md) |
| §21 | 联调路径 | 总体实施规划 | [`implementation-plan/README.md` §7](implementation-plan/README.md) |
| §22 | 下一步 / 启动清单 | 任务状态 | [`status.md` §0](status.md) + M1..M5 详细任务 |

### 3.3 内容保证

- **内容零损失**: 所有 22 个原章节的全部信息均在新 4 类文件中找到归宿, 表格/代码/论证完整保留
- **状态保持 🟢 Accepted**: 拆分是**重组**, 不是修订。 v2.0 决策本身 (F1-F6 + 议题 1-8) 未变
- **基础文档引用不变**: 7 个基础文档 (multi_isa / plugin-framework / declarative-hybrid / methodology / lessons / bundles) 保持原状

### 3.4 文件清单 (拆分后)

```
ip/cpu/docs/
├── README.md                                 (原) 索引 — 同步更新
├── cpu_implementation_guide_v2.0.md         (原, 现 ~230 行) 决策入口
├── blueprint.md                              (新, ~700 行) 静态架构蓝图
├── status.md                                 (新, ~250 行) 任务状态看板
├── multi_isa_architecture.md                 (原) 权威设计, 不动
├── implementation-plan/                      (新目录)
│   ├── README.md                             (新, ~400 行) 总体实施规划
│   ├── M1-cpu-skeleton.md                    (新, ~80 行) M1 详细任务
│   ├── M2-core-plugins.md                    (新, ~70 行) M2 详细任务
│   ├── M3-riscv-plugins.md                   (新, ~80 行) M3 详细任务
│   ├── M4-integration.md                     (新, ~80 行) M4 详细任务
│   └── M5-verification.md                    (新, ~70 行) M5 详细任务
└── riscv/                                    (原) 旧参考, 不动
```

### 3.5 后续维护约定

- **静态架构 (blueprint.md)**: 改动少, 仅在重大设计变更时更新
- **总体实施规划 (implementation-plan/README.md)**: 范围/风险调整时更新
- **M1..M5 详细任务 (implementation-plan/Mx-*.md)**: 任务粒度变化时更新
- **任务状态 (status.md)**: **唯一高频改文件**, 每次 Mx.y 完成即更新对应行
- **决策入口 (本文件)**: 仅在 F1-F6 决议或议题 1-8 选择变更时更新

---

*本拆分基于 v2.0 Accepted 内容重组, 决策本身未变。 后续实施严格按 `implementation-plan/M1..M5` 任务清单执行, 任务状态实时同步到 `status.md`。*

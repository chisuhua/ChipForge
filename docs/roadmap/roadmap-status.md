# 路线图执行状态跟踪

> **最后更新**: 2026-06-09
> **更新时机**: 每周一 / 阶段切换时 / 重大决策落地后
> **权威源**: `docs/roadmap/phases/*.md` + `.omo/plans/*.md` + `.omo/drafts/*.md`
> **本文件目的**: 不重复阶段文档的任务清单,只跟踪执行状态、阻塞和下一步

---

## 1. 状态总览

| 阶段 | 里程碑 | 状态 | 进度 | 阻塞项 | 下一交付物 |
|------|--------|------|------|--------|----------|
| Phase 0 | M0 - 脚手架可运行 | ✅ Completed | 100% | 无 | Phase 1 启动 |
| Phase 1 | M1 - L1CachePlugin Hello World | Not Started | 0% | 依赖 Phase 0(已解除) | Bundle 定义 |
| Phase 1* | M1 (legacy, 已取代) | Superseded | - | 无 | 已删除 (2026-06-09) |
| Phase 2 | M2 - ISA 全覆盖 | Not Started | 0% | 依赖 Phase 1 | riscv-tests 集成 |
| Phase 3 | M3/M4 - FreeRTOS/Zephyr | Not Started | 0% | 依赖 Phase 2 | ClintTlm/PlicTlm 完善 |
| Phase 4 | M5 - Linux 启动 | Not Started | 0% | 依赖 Phase 3 | Sv39 + VirtIO Block |
| Phase 5 | M6/M7/M8/M10 - RTL | Not Started | 0% | 依赖 Phase 4 | L1CacheRtl (CppHDL) |
| Phase 6 | M6 - 完整 PipeBuilder | Not Started | 0% | 依赖 2-3 Plugin 稳定 | 调度算法 |

> *Phase 1* = 已被 `phase-1-tlm-foundation.md` 取代的旧 `phase-1-foundation.md`,待决定保留/废弃/删除

---

## 2. 详细状态(按依赖顺序)

### Phase 0 - Plugin 最小脚手架

- **状态**: ✅ Completed (5/5 P0 组件已交付, 51/51 单元测试 PASS, 7/7 ctest PASS, 退出标准 v2 全部达成)
- **预估工时**: 14-16 工作日(2.5-3 周)
- **已用**: 1 个 session 集中实施(2026-06-08)
- **依赖**: 无
- **决策依据**: `.omo/drafts/decision-plugin-framework-2026-06-08.md`

**P0 任务**(详见 `phase-0-plugin-scaffolding.md`):

| # | 任务 | 工时 | 状态 | 单元测试 |
|---|------|------|------|---------|
| 1.1 | PluginBase 接口 | 2d | ✅ 完成 (2026-06-08) | 7/7 PASS |
| 1.2 | Payload&lt;T&gt; 类型安全 Key | 2d | ✅ 完成 (2026-06-08) | 8/8 PASS |
| 1.3 | PipeNode 节点 | 3d | ✅ 完成 (2026-06-08) | 14/14 PASS |
| 1.4 | PipeBuilder 编排器 | 4d | ✅ 完成 (2026-06-08) | 11/11 PASS |
| 1.5 | CtrlLink 控制 API | 3d | ✅ 完成 (2026-06-08) | 11/11 PASS |

**进度**: 5/5 P0 组件完成;51/51 单元测试通过(100% ctest pass rate)
**剩余**: 退出标准 §2.2(质量标准)与 §2.3(集成标准)待验证 — 调度确定性证明、零 TODO、覆盖率≥80%、Doxygen、与 ChStreamModuleBase/Component 共存

### Phase 1 - TLM Foundation (L1CachePlugin)

- **状态**: Not Started (0%)
- **依赖**: Phase 0
- **预估工时**: 7-9 工作日(~1.5 周)

**关键约束**(D4 强制): 业务代码无 `tick()`、Bundle 字段用 `uint_t<N>`、所有阶段用 `at_stage()`

### Phase 2 - Bare-metal 测试套件

- **状态**: Not Started (0%)
- **依赖**: Phase 1
- **关键交付**: riscv-tests RV64GC 全部 PASS + RISCOF 合规认证

### Phase 3 - RTOS

- **状态**: Not Started (0%)
- **依赖**: Phase 2
- **关键交付**: FreeRTOS + Zephyr 稳定运行

### Phase 4 - Linux 启动

- **状态**: Not Started (0%)
- **依赖**: Phase 3
- **关键交付**: OpenSBI → Linux Kernel → Shell 交互

### Phase 5 - RTL 协同验证

- **状态**: Not Started (0%)
- **依赖**: Phase 4
- **关键交付**: L1CacheRtl + COMPARE 模式 + Verilator 仿真

### Phase 6 - 完整 PipeBuilder 框架

- **状态**: Not Started (0%)
- **依赖**: 2-3 个 Plugin-style IP 稳定运行
- **关键交付**: 调度算法 + JSON 解析 + CompareDriver + RTL 生成

---

## 3. 当前未决项(Pending Actions)

| ID | 类型 | 项目 | 责任 | 状态 | 优先级 |
|----|------|------|------|------|-------|
| PA-1 | 文档 | M4: `adr.md` 新增 ADR-037 + 更新 ADR-025~036 | TBD | ✅ 已完成 (2026-06-08, v1.1) | P1 |
| PA-2 | 文档 | M5: `declarative-hybrid-framework.md` §12.0.3 责任归属表 | TBD | ✅ 已完成 (2026-06-08, v2.0.3) | P1 |
| PA-3 | 文档 | 处置 `phase-1-foundation.md`(旧 vs 新 phase-1-tlm-foundation.md) | TBD | ✅ 已完成 (2026-06-09, 文件已 git rm) | P2 |
| PA-4a | 构建 | CppTLM 集成 + 根 CMakeLists.txt 升级 | TBD | ✅ 已完成 (2026-06-08) | P1 |
| PA-4b | 构建 | CppHDL 集成(阻塞: CppHDL 上游 tests 路径 bug) | TBD | ✅ 已完成 (2026-06-08, 上游修复后集成) | P2 |
| PA-5 | 验证 | V1: `tools/verify_plugin_decision.sh`(可选) | TBD | ✅ 已完成 (2026-06-08, 3/3 PASS) | P3 |

---

## 4. 风险与阻塞

| ID | 描述 | 影响阶段 | 缓解措施 | 状态 |
|----|------|---------|---------|------|
| R1 | D4 决策(Plugin-style 强制)被 C++17 静态类型系统拒绝 | Phase 0/1 | Phase 0 退出标准强制"端到端跑通最小 Plugin" | 监控中 |
| R2 | 脚手架工时低估(可能 4 周 vs 计划 2-3 周) | Phase 0 | 每周评估;必要时拆为 Phase 0a/0b | 待启动 |
| R3 | CppHDL 集成阻塞(tests/CMakeLists.txt:38 路径 bug) | Phase 5+ | Phase 0-2 暂不需 CppHDL;Phase 5+ 再评估或用独立构建 | 已识别 + 缓解 |
| R4 | 命名冲突(halt_when vs stream_halt_when 等 4 项) | Phase 0 | 决策文档 §3.5 已识别 D6-D9 方案 | 已识别 |
| R5 | 文档决策被用户拒绝 | 全部 | 修订计划提供渐进回滚方案 | 待确认 |

---

## 5. 下一步建议(Top 3)

### 建议 2:启动 Phase 0 P0 #1 — PluginBase(2 天) — 🔄 剩余

**前置条件已就绪**:
- ✅ `src/cf_plugin/` 目录 + CMakeLists.txt + README.md(本轮完成)
- ✅ `cf_plugin` INTERFACE 库已注册(供 P0 组件消费)
- ✅ 根 `CMakeLists.txt` 集成 CppTLM(供单元测试链接)
- ✅ `compile_commands.json` 已生成(LSP 友好)
- ⚠️ 需要至少 1 个 Owner 指派(§12.0.3 责任表)

**任务**(详见 `phase-0-plugin-scaffolding.md` §1.1):
- 定义 `cf::plugin::PluginBase` 抽象基类
  - 暴露 `setup(PipeBuilder&)`(默认空) + `build(PipeBuilder&)`(纯虚)
- 编译期断言禁止 `tick()`(static_assert + final 类 + 删除的 tick())
- 单元测试 `test_plugin_lifecycle.cpp`
- 验证 cmake 链接 + 运行测试

**价值**: Phase 0 第一个 P0 组件开始落地,验证"Plugin-style"设计在 C++17 静态类型系统下可行

### 建议 3:按 1.1 → 1.5 顺序完成 Phase 0 全部 5 个 P0(共 2-3 周) — 🔄 剩余

按顺序实施剩余 4 个组件:
- 1.2 `Payload<T>` (2 天)
- 1.3 `PipeNode` (3 天)
- 1.4 `PipeBuilder` (4 天)
- 1.5 `CtrlLink` (3 天)
- 端到端验证(最小 HelloPlugin) + Phase 0 退出标准 (2-3 天)

**前置条件**: 建议 1 + 建议 2 完成 + Owner 持续投入

---

## 6. 活动日志

| 日期 | 事件 |
|------|------|
| 2026-06-09 | **本次会话五** (Quick 准备): ADR-033 (CtrlLink 4-control-API) 🚧 → ✅ Accepted, D6 共存方案绑定 (halt_when / throw_when / flush_when / bypass);git rm `docs/roadmap/phases/phase-1-foundation.md`;修正 4 处 ip/{cache,memory,interconnect,peripheral}/README.md 链接指向 `phase-1-tlm-foundation.md`;roadmap-status.md 同步 (PA-3 状态推进 + 建议 1 删除 + Phase 1* 行 + 活动日志);1 个原子 commit 提交。Phase 1 (L1CachePlugin) Large plan 启动前的前置解锁。 |
| 2026-06-09 | **本次会话四**: 路线图文档同步——`docs/roadmap/README.md` Phase 0 状态修正为 ✅ Completed(2026-06-08);`roadmap-status.md` §1 状态总览 + §2 Phase 0 详情同步;`docs/architecture/overview.md` 顶部新增"实现状态快照"banner,标明应用层(`bundles/`/`ip/*/`/`soc/riscv_virt.json`)待建设;消除文档-状态背离 |
| 2026-06-08 | **本次会话续三**: CppHDL 集成已恢复(上游 commit 7fe4a5d 修复);test_coexistence 5/5 PASS(CppTLM+CppHDL 共存);PA-5 verify_plugin_decision.sh 3/3 PASS;coverage 测量设施就位;`docs/api/cf_plugin.md` API 文档(Doxygen 替代);Phase 0 退出标准 v2 全部达成(Status: ✅ Completed);7/7 ctest 100% PASS |
| 2026-06-08 | **本次会话续二**: Phase 0 P0 #1-5 全部实施完成 (51/51 单元测试 PASS);PluginBase + Payload<T> + PipeNode + PipeBuilder + CtrlLink;ctest 聚合 5/5 PASS in 0.61s |
| 2026-06-08 | **本次会话终**: 创建 `src/cf_plugin/` 工作区 (CMakeLists.txt + README.md);`cf_plugin` INTERFACE 库已注册;cmake configure 0 错误 (1.3s + 2.9s);Phase 0 实施基础设施就绪 |
| 2026-06-08 | **本次会话续**: 根 `CMakeLists.txt` STUB → 真实集成 (v0.0.1);CppTLM 子模块已集成 (configure 0 错误, cpptlm_core 目标可达, 5+ .o 编译成功);CppHDL 因上游 tests 路径 bug 暂缓 (PA-4b 跟踪);`compile_commands.json` 已生成 |
| 2026-06-08 | **本次会话**: 创建 `roadmap-status.md` 跟踪文件;M4 (ADR-037) 验证已完成;M5 (§12.0.3 责任归属表) 实施完成 (v2.0.3) |
| 2026-06-08 | 决策记录 D1-D11 完成;Phase 0/1/6 文档创建;roadmap/README.md 调整;declarative-hybrid-framework.md 升级 v2.0.2;plugin-framework-revision-plan.md 创建 |
| 2026-06-05 | T1-T7 任务:CI 修复、.gitignore 扩展、IP stub 目录、planning banners |
| 2026-06-03 | 项目骨架初始化(roadmap, architecture, ip 目录) |

---

## 7. 更新指南

- **何时更新**:
  - 每周一上午(例行)
  - 阶段状态变更时(Not Started → In Progress → Completed)
  - 阻塞项出现/解除时
  - 重大决策落地后
- **如何更新**:
  1. 修改"最后更新"日期
  2. 更新 §1 状态总览对应行
  3. 在 §2 详细状态中标记任务进度
  4. 如有新待办,加入 §3
  5. 在 §6 活动日志追加一行
- **不要**:
  - 复制阶段文档的任务清单(本文件只跟踪状态)
  - 在本文件中详细描述任务(那是阶段文档的职责)

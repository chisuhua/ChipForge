# 项目测试入口 (Tests)

> **状态**: 🟢 M1 收官后重构 (2026-06-16)
> **架构**: 按家族 (family) 划分子目录, 而非按物理位置 (src/) 或 IP
> **总数**: 17 个单元测试 (M1 收官时 17/17 PASS, M2-M5 启动后增长)

## 1. 子目录组织

| 目录 | 家族 | 测的是什么 | 当前数量 | 增长预测 |
|------|------|------------|----------|----------|
| `framework/` | cf_plugin 框架层 | Plugin 基础设施 (PluginBase/Payload/PipeNode/PipeBuilder/CtrlLink/Storage/Coexistence/PipeArbitration) | 9 | 稳定 (框架冻结) |
| `cpu/` | CPU IP 业务 | CPU 通用抽象 (payload_common.h 8 Key) | 1 | **M2-M5 大量增长** (5+6+11+9 = 31 子任务) |
| `cache/` | L1Cache IP 业务 | L1CachePlugin 单元测试 + Bridge + Adapter e2e | 4 | 稳定 (Phase 1.3 全完结) |
| `soc/` | SoC 集成层 | SoC JSON 拓扑 + JSON Schema 验证 | 2 | 慢增 (M4-M5 联调加 1-2) |
| `bundles/` | 共享 Bundle 定义 | `bundles/mem_bundles.h` | 1 | 稳定 (新 Bundle 罕见) |
| **总计** | | | **17** | **M5 收官估 50+** |

## 2. 与之前架构的对比

### 之前 (M1 收官前)

```
src/cf_plugin/tests/    ← 17 个测试, 4 个家族混杂
├── test_plugin_lifecycle.cpp     (framework)
├── test_payload.cpp              (framework)
├── ... 7 个 framework ...
├── test_payload_common.cpp       (cpu 业务)
├── test_l1_cache_*.cpp (4 个)    (cache 业务)
├── test_soc_*.cpp (2 个)         (soc 集成)
└── test_mem_bundles.cpp          (bundles)
```

**问题**:
- 物理位置 (`src/cf_plugin/`) 暗示"全属 cf_plugin", 但实际混杂 4 家族
- 业务测试 (cpu/cache) 与 framework 测试无视觉区分
- M2 启动后会再增 5 个 CPU 测试, 进一步混乱

### 之后 (M1 收官后, 本次重构)

```
tests/                            ← 统一入口
├── README.md (本文件)
├── framework/  (9 个, cf_plugin 框架)
├── cpu/        (1 个, CPU 业务, M2-M5 大量增长)
├── cache/      (4 个, L1Cache 业务)
├── soc/        (2 个, SoC 集成)
└── bundles/    (1 个, 共享 Bundle)
```

**优势**:
- 顶层 `tests/` 入口对开发者透明
- 家族边界清晰, M2 启动后测试归位明确
- 框架与业务物理隔离, 改 cf_plugin 不会触发业务测试 rebuild

## 3. CMake 集成

测试由 `src/cf_plugin/CMakeLists.txt` 统一管理 (Phase 1.5 前只有 1 个 CMake 入口, Phase 2+ 考虑拆分)。

每个 test target 的源文件路径已更新为新位置, 但 test target 名字保留 (避免 ctest 输出变化)。

## 4. 重构决策证据

本重构由用户 2026-06-16 反馈"src/cf_plugin/tests/ 是否要移动到项目根目录"触发。
原 D-1 决策 (C1 commit) "测试不与 IP 目录绑定, 放 src/cf_plugin/tests/" 在 C1 时合理
(M1 阶段测试全属 cf_plugin), M1 收官后 17 个测试分布 4 家族, 决策不再适用。

本重构:
- **位置**: 根 tests/ (用户推荐选项 A)
- **时机**: M1 收官后, M2 启动前 (避免 M2 启动后 31 子任务混杂)
- **范围**: 只 mv, 保留 test_*.cpp 名字 (最小风险)

## 相关文档

- **M1 收官报告**: `ip/cpu/docs/status.md` §0
- **M1 实施计划**: `ip/cpu/docs/implementation-plan/M1-cpu-skeleton.md`
- **总体实施规划**: `ip/cpu/docs/implementation-plan/README.md`
- **决策入口**: `ip/cpu/docs/cpu_implementation_guide_v2.0.md`

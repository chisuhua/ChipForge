# cf_plugin — Phase 0 Plugin 脚手架

> **状态**: Phase 0 实施中(5/5 P0 组件已完成, 待退出标准验收)
> **所属阶段**: Phase 0 — Plugin 最小脚手架
> **目标版本**: ChipForge 0.0.x
> **依赖**: 无
> **决策依据**: [`.omo/drafts/decision-plugin-framework-2026-06-08.md`](../../.omo/drafts/decision-plugin-framework-2026-06-08.md)
> **实施日期**: 2026-06-08

## 1. 目标

建立 Plugin-style 设计的基础脚手架(scaffolding),为 Phase 1 业务逻辑(`L1CachePlugin` 等)提供可执行的运行时支撑。

> **关键概念区分**:
> - **脚手架(scaffolding)**:提供"挂载点"和"基本结构",每个 IP 自带自己的调度与实现
> - **框架(framework)**:完整的调度/管理/可视化能力(推迟到 Phase 6)
>
> Phase 0 **不是** "完整 Plugin 框架"——它是**让 Plugin-style 业务逻辑能跑起来**的最小集。

## 2. 当前状态

| 组件 | 状态 | 测试 |
|------|------|------|
| `cf::plugin::PluginBase` | ✅ 已完成 (2026-06-08) | 7/7 PASS |
| `cf::plugin::Payload<T>` | ✅ 已完成 (2026-06-08) | 8/8 PASS |
| `cf::plugin::PipeNode` | ✅ 已完成 (2026-06-08) | 14/14 PASS |
| `cf::plugin::PipeBuilder` | ✅ 已完成 (2026-06-08) | 11/11 PASS |
| `cf::plugin::CtrlLink` | ✅ 已完成 (2026-06-08) | 11/11 PASS |
| **总计** | **5/5 完成** | **51/51 单元测试 PASS** |

### 单元测试运行

```bash
cmake --build build --target test_plugin_lifecycle test_payload \
                                   test_pipe_node test_pipe_builder \
                                   test_ctrl_link
ctest --test-dir build --output-on-failure
# 预期: 100% tests passed, 0 tests failed out of 5
```

## 3. 目录结构(计划)

```
cf_plugin/
├── CMakeLists.txt              ← 本文件
├── README.md
├── include/
│   └── cf/
│       └── plugin/
│           ├── plugin_base.h     ← 1.1
│           ├── payload.h         ← 1.2
│           ├── pipe_node.h       ← 1.3
│           ├── pipe_builder.h    ← 1.4
│           ├── ctrl_link.h       ← 1.5
│           └── uint_t.h          ← 编译期类型切换 (uint_t<N> / bool_t)
├── src/                          ← (可选) 非模板实现
│   └── ...
└── tests/                        ← 单元测试
    ├── test_plugin_base.cpp
    ├── test_payload.cpp
    ├── test_pipe_node.cpp
    ├── test_pipe_builder.cpp
    ├── test_ctrl_link.cpp
    └── CMakeLists.txt
```

## 4. CMake 集成

当前 `cf_plugin` 是 **INTERFACE 库**(纯头文件,无需编译目标)。

- 公共头文件:`include/cf/plugin/*.h`
- 链接方式:其他 ChipForge 子目录通过 `target_link_libraries(... cf_plugin)` 消费
- C++ 标准:C++17(与父项目对齐)

### 升级到 STATIC 库的触发条件

当出现非模板实现(例如 `PipeNode` 状态机)时,转换为:

```cmake
add_library(cf_plugin STATIC
  src/pipe_node.cpp
  src/...
)
target_include_directories(cf_plugin
  PUBLIC
    $<BUILD_INTERFACE:${CF_PLUGIN_PUBLIC_INCLUDE_DIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
```

## 5. 退出标准(Phase 0 完成条件)

详见 [`docs/roadmap/phases/phase-0-plugin-scaffolding.md` §2](../../docs/roadmap/phases/phase-0-plugin-scaffolding.md):

### 5.1 功能标准

- [ ] 5 个 P0 组件全部实现且单元测试通过
- [ ] 一个**最小验证 Plugin**(~10 行代码)能在 PipeBuilder 下端到端跑通
- [ ] 与原 CppTLM/CppHDL 框架无冲突(独立编译)

### 5.2 质量标准

- [ ] **调度确定性证明**:同一组 Plugin 注册顺序相同 → 多次执行结果一致
- [ ] **零 TODO 残留**(与 CppTLM 零债务原则一致)
- [ ] 单元测试覆盖率 ≥ 80%
- [ ] API 文档(Doxygen)完整

### 5.3 集成标准

- [ ] 与 `cpptlm::ChStreamModuleBase` 共存无冲突
- [ ] 与 `ch::Component` 共存无冲突
- [ ] 编译期类型安全(错类型 Payload Key 编译失败)

## 6. 显式不做(推迟到 Phase 6)

| 推迟项 | 理由 |
|--------|------|
| `enum class ImplMode` 枚举 | Phase 0 仅做 TLM 模式验证;多模式由 Phase 6 引入 |
| `BundleMapper` 模板 | Phase 0 用编译期类型切换(`uint_t<N>`)足够 |
| `CompareDriver` / `ScoreBoard` | 验证基础设施在 Phase 5/6 引入 |
| JSON `pipeline_stages` 解析 | Phase 6 引入 |
| RTL AST 生成(VerilogCodeGen 集成) | Phase 6 引入 |

## 7. 与 CppTLM/CppHDL 的关系

| 现有框架 | cf_plugin 关系 |
|----------|---------------|
| `cpptlm::ChStreamModuleBase` | **正交**:Plugin 是声明式逻辑单元,ChStreamModuleBase 是 TLM 模块类 |
| `ch::Component` | **正交**:Component 是 CppHDL RTL 描述,Plugin 是声明式逻辑单元 |
| `chlib::PipelineStage` / `PipelineChain` | **可借鉴**:cf_plugin 的 StageLink 可调用这些已有组件作为 RTL 后端实现 |
| `chlib::stream_*_when` | **共存**:保留 chlib 自由函数,Plugin 路径用 `ctrl_link.halt_when()` 对象方法 |

> 详细决策: [`.omo/drafts/decision-plugin-framework-2026-06-08.md`](../../.omo/drafts/decision-plugin-framework-2026-06-08.md) D6-D9

## 8. 实施参考

| 借鉴源 | 用于 |
|--------|------|
| VexRiscv `Plugin.scala`(25 行) | PluginBase 设计灵感 |
| VexRiscv `Stageable[T]` | Payload<T> 模式 |
| `chlib/stream_builder.h` | PipeBuilder 链式 API |
| `chlib/pipeline.h` | CtrlLink OR 合并逻辑 |
| `chlib/state_machine.h` | PipeNode 状态机内部结构 |

## 9. 当前跟踪

- **状态跟踪**: `docs/roadmap/roadmap-status.md` §2 "Phase 0"
- **阻塞项**: 无(已就绪,等待 Owner 指派)
- **Owner**: TBD(待 §12.0.3 责任归属表填写)
- **风险**: R1(D4 决策不可逆)、R2(工时低估)、R4(命名冲突)

---

*本目录为 Phase 0 实施的工作区。Phase 0 启动后,按 [phase-0-plugin-scaffolding.md §1.1-1.5](../../docs/roadmap/phases/phase-0-plugin-scaffolding.md) 顺序添加 5 个 P0 组件。*

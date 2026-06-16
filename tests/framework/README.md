# Framework 框架层测试 (cf_plugin)

> **家族**: cf_plugin 框架基础设施
> **数量**: 9 个测试
> **Phase**: Phase 0 (5 P0 组件) + Phase 1.5 扩展

## 1. 测试列表

| 文件 | 测什么 | Phase | 备注 |
|------|--------|-------|------|
| `test_plugin_lifecycle.cpp` | `cf::plugin::PluginBase` 生命周期 (setup/build) | 0 P0 #1 | 7/7 PASS |
| `test_payload.cpp` | `cf::plugin::Payload<T>` 类型安全 Key | 0 P0 #2 | 8/8 PASS |
| `test_pipe_node.cpp` | `cf::plugin::PipeNode` 5 态状态机 | 0 P0 #3 | 14/14 PASS |
| `test_pipe_builder.cpp` | `cf::plugin::PipeBuilder` 编排器 | 0 P0 #4 | 11/11 PASS |
| `test_ctrl_link.cpp` | `cf::plugin::CtrlLink` 控制 API (halt/throw/flush/bypass) | 0 P0 #5 | 11/11 PASS |
| `test_hello_plugin.cpp` | 最小验证 Plugin (Phase 0 退出标准) | 0 退出标准 | |
| `test_coexistence.cpp` | cf_plugin 与 CppTLM/CppHDL 共存 | 0 集成标准 | |
| `test_storage.cpp` | `cf::plugin::storage::array_store<T, N>` (ADR-040) | 1.3 | |
| `test_pipe_arbitration.cpp` | `cf::plugin::PipeArbitration` 三态结构 | **1.5 M1.5+M1.6** | 🆕 M1 收官新增 |

## 2. 与业务测试的边界

framework/ 测的是**通用 Plugin 框架**, **不依赖任何 IP 业务逻辑**。 改 framework
不会触发 cpu/cache/soc/ 测试 rebuild (物理隔离)。

业务测试在 `tests/cpu/` `tests/cache/` 等。

## 3. 退出标准

Phase 0 §2.3 集成标准: 与 CppTLM ChStreamModuleBase + CppHDL ch::Component 共存
无冲突。 已通过 `test_coexistence.cpp` 验证。

## 相关文档

- **cf_plugin 文档**: `src/cf_plugin/README.md`
- **Phase 0 退出标准**: `docs/roadmap/phases/phase-0-plugin-scaffolding.md` §2
- **ADR-040**: TLM→HDL 移植性约束 (storage.h)

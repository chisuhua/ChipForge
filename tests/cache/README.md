# L1Cache IP 业务测试

> **家族**: L1Cache IP 业务 (Phase 1.2-1.3d-extras 已完结)
> **数量**: 4 个测试
> **状态**: 🟢 Phase 1.3 全完结, 测试稳定

## 1. 测试列表

| 文件 | 测什么 | Phase | 备注 |
|------|--------|-------|------|
| `test_l1_cache_plugin_unit.cpp` | L1CachePlugin 单元测试 (lookup + refill 两阶段) | 1.2 | 4/4 PASS, D4 合规 |
| `test_l1_cache_bridge.cpp` | L1CacheTLMBridge (cpptlm 适配层, D1' 末尾挂载契约) | 1.3a | |
| `test_l1_cache_plugin_e2e.cpp` | L1CachePlugin + Bridge + Adapter e2e | 1.3d | |
| `test_l1_cache_json_instantiate.cpp` | full JSON instantiateAll e2e (PA-6 闭环) | 1.3d-extras | |

## 2. 与 CPU 测试的边界

- **L1CachePlugin** 是 L1 Cache IP 业务, 在本目录
- **CPU IP 业务** 在 `tests/cpu/`
- CPU 仿真场景 (M5 联调) 会**用** L1CachePlugin, 但**不测** L1Cache 自身
- L1Cache 自身的回归测试在本目录独立维护

## 3. Phase 1.3 闭环状态

✅ Phase 1.3 全部子任务完成 (含 1.3d-extras ch_stream 注册 + full JSON e2e, PA-6 闭环)

下一里程碑: PA-7 cpptlm::CacheTLM baseline 对比 (Phase 1.3 之后, 推迟到 M5 联调后)。

## 相关文档

- **L1Cache 文档**: `ip/cache/README.md` §9 (使用指南)
- **L1Cache Lessons**: `docs/lessons/phase-1.2-l1cacheplugin.md`
- **决策草案**: `.omo/drafts/decision-phase-1.3-*.md`

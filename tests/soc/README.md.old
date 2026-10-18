# SoC 集成层测试

> **家族**: SoC 集成 (JSON 拓扑 + JSON Schema 验证)
> **数量**: 2 个测试
> **增长**: M4-M5 联调阶段可能加 1-2 个 (soc/cpu_l1_picolibc/demo.json)

## 1. 测试列表

| 文件 | 测什么 | Phase | 备注 |
|------|--------|-------|------|
| `test_soc_l1_cache_minimal_json.cpp` | `soc/l1_cache_minimal.json` 结构验证 (v2 决策 §4 拓扑) | 1.3b | |
| `test_cache_params_schema_json.cpp` | `ip/cache/configs/params_schema.json` Schema 验证 (v2 §3) | 1.3c | |

## 2. SoC 测试的边界

- **SoC 配置** 在 `soc/` 目录
- **IP 参数 Schema** 在各 IP 的 `configs/` 目录
- **集成测试** 测"JSON 是否符合 v2 决策", 不测具体行为
- **行为测试** 在 `tests/cpu/` `tests/cache/` (单元) 或 e2e (Phase 5+)

## 3. M5 联调预测

M5 阶段会新增 `soc/cpu_l1_picolibc/demo.json`, 测试可能加:
- `test_demo_soc_json.cpp` - 验证 demo.json 结构
- `test_demo_soc_e2e.cpp` - 完整 tohost=1 e2e (本期可能 1 测试 = 6 ELF 全跑通)

## 相关文档

- **SoC 配置**: `soc/l1_cache_minimal.json` `soc/l1_cache_adapter_e2e.json`
- **IP Schema**: `ip/cache/configs/params_schema.json`
- **M5 联调路径**: `ip/cpu/docs/implementation-plan/README.md` §7

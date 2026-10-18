# tests/soc/l1-cache-minimal/

L1Cache standalone SoC topology structural validation.

## SoC topology
参见 [`soc/l1_cache_minimal.json`](../../../soc/l1_cache_minimal.json) + [`soc/l1_cache_adapter_e2e.json`](../../../soc/l1_cache_adapter_e2e.json) — L1CachePlugin component structural declaration + bridge adapter e2e.

## Contents
- `test_cache_params_schema_json.cpp` — params_schema.json 验证
- `test_soc_l1_cache_minimal_json.cpp` — l1_cache_minimal.json 结构验证

## Family tag
`[soc]`

## Upstream spec
[`openspec/specs/mmu-cache-idx-vipt/spec.md`](../../../openspec/specs/mmu-cache-idx-vipt/spec.md) (桥接 L1Cache ↔ MMU VIPT 索引语义)

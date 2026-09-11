# STATUS: ACTIVE (TLM 实现中, Phase 1.3, 2026-07-01)

This IP has **full TLM implementation with Bridge + Adapter e2e**. L1CachePlugin 是第一个 Plugin-style IP。

<!-- Conforms to: docs/templates/IP_STATUS_TEMPLATE.md -->

## Existing Assets
- Plugin: `tlm/L1CachePlugin.{h,cpp}` — lookup + refill 两阶段, 256 sets × 1 way × 64B = 16KB direct-mapped
- Bridge: `src/cf_plugin/bridge/l1_cache_bridge.{h,cpp}` — L1CacheTLMBridge
- Adapter: `src/cf_plugin/bridge/l1_cache_bridge_adapter.{h,cpp}` — cpptlm ModuleFactory 兼容层
- 配置: `configs/params_schema.json`（JSON Schema draft-07）
- 替换策略: `policies/replacement_policy.{h,cpp}` — NoReplacementPolicy + LRUPolicy
- 测试: 5 测试文件, 21 test cases (`tests/cache/`) 全部 PASS
- SoC JSON: `soc/l1_cache_minimal.json` + `soc/l1_cache_adapter_e2e.json`
- 文档: `README.md`

## Implementation Roadmap
- 下一里程碑: Phase 1.5 — 256×1 → 64×4 VIPT 升级（ADR-044）+ MMU↔Cache 集成
- L2 Cache: Phase 2+
- 状态: 🟡 TLM 实现中（Phase 1.3 全部子任务落地）

## 已知限制
- **Phase 0 offset 简化**: kOffsetBits=4 (16-byte 粒度) 而非真正 64B cacheline（uint_t<512> 退化为 uint64_t）
- **Direct-mapped**: 1-way associativity 需升级到 4-way（ADR-044 VIPT 安全条件）
- **No MMU integration**: 当前 Cache 接收物理地址，无 vaddr→paddr 翻译路径
- **STATUS.md 历史补齐**: 本文件于 2026-07-01 与 `ip/cpu/STATUS.md` 同步补齐
- **`docs/` 历史补齐**: 2026-07-01 同步建立 `ip/cache/docs/{adr,architecture,integration}.md`（含 ADR-044 L1Cache VIPT 决策）

## 参考
- ADR: ADR-044（L1Cache VIPT 决策，`ip/cache/docs/adr/`）
- Phase 1 文档: `soc/cpu/docs/roadmap/phase-1-tlm-foundation.md`
- SoC 架构: `soc/cpu/docs/architecture.md`

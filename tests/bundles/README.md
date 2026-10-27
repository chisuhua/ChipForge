# 共享 Bundle 测试

> **家族**: 共享 Bundle 定义 (跨 IP 通信基础)
> **数量**: 1 个测试
> **位置特殊性**: bundles/ 目录在**项目根**, 而非 src/cf_plugin/bundles/

## 1. 测试列表

| 文件 | 测什么 | Phase | 备注 |
|------|--------|-------|------|
| `test_mem_bundles.cpp` | `bundles/mem_bundles.h` (CacheReq/CacheResp/MemReq/MemResp/Snoop) | 1.1 | |

## 2. bundles/ 的特殊位置

`bundles/` 位于**项目根**, 不是任何 IP 内部:
- `bundles/mem_bundles.h` 被 cf_plugin / L1CachePlugin / CPU Plugin 等**共享**
- 物理位置"项目根 bundles"暗示"全局共享", 与 IP 内部"局部使用"区分

## 3. Bundle 设计

| Bundle | 用途 | 使用方 |
|--------|------|--------|
| ★ `cf::bundles::MemReq` | 内存请求 (address 64 / data 64 / is_write 1 / burst_len 8 / id 8) | CPU ↔ Cache ↔ Memory |
| ★ `cf::bundles::MemResp` | 内存响应 (data 64 / id 8 / error 1 / last 1) | CPU ↔ Cache ↔ Memory |

> ★ **命名更正（ADR-092 §D7，2026-10-28）**：旧文档写的 `MemReqBundle` / `MemRespBundle` **在代码中不存在** ⇒ 实际类型名如上（定义于 `bundles/mem_bundles.h`）。
| `CacheReq` | Cache 抽象请求 (M5+ 内部用) | CPU ↔ L1Cache |
| `CacheResp` | Cache 抽象响应 (M5+ 内部用) | CPU ↔ L1Cache |
| `SnoopBundle` | 一致性侦听 (多核, Phase 6+ 推迟) | Cache ↔ Cache |

## 相关文档

- **接口设计**: `docs/architecture/interface-design.md`
- **Phase 1.1 决策**: `.omo/drafts/decision-mem-bundles-*.md` (如有)

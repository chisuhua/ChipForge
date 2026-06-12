# ChipForge 共享 Bundle 定义

> **状态**: Phase 1.1 (1/5 Phase 1 子任务完成)
> **所属阶段**: Phase 1 — 基础 TLM 平台 (L1CachePlugin)
> **目标版本**: ChipForge 0.1.x
> **依赖**: `cf::plugin::uint_t<N>` (Phase 0 脚手架)
> **详见**: [phase-1-tlm-foundation.md §1.1](../docs/roadmap/phases/phase-1-tlm-foundation.md)

## 1. 目标

为所有 IP 提供 **类型安全 + 编译期可验证** 的事务接口定义。Bundle 是 IP 之间**唯一允许**的通信载体（除 `Payload<T>` 跨阶段通信外）。

## 2. 设计原则（D4 决策强制）

| 原则 | 实现 |
|------|------|
| **所有字段用 `cf::plugin::uint_t<N>`** | 编译期 TLM/RTL 切换，Phase 6 升级 RTL 不重写业务 |
| **POD struct**（无虚函数） | TLM 事务直接 memcpy；零开销 |
| **字段默认构造 = 0** | 避免未初始化字段污染事务流 |
| **AXI 风格字段布局** | 行业事实标准 (address/data/is_write/burst_len/id) |
| **无 IO 方向语义** | TLM 模式下方向由调用方决定；RTL 模式 (Phase 5) 引入 `as_master/slave_direction()` |
| **Bundle 形态分阶段** | Phase 1: POD (`cf::plugin::uint_t<N>`) / Phase 5: `ch_uint<N>` + `bundle_base` / Phase 6: 自动 codegen | 详见 [interface-design.md §1.0](../docs/architecture/interface-design.md#10-bundle-形态演进-phase-1--phase-5--phase-6) + ADR-024 形态切换小节 |

## 3. 当前 Bundle 清单

| Bundle | 字段 (位宽) | 用途 | 流向 |
|--------|-------------|------|------|
| `cf::bundles::MemReq` | `address(64)` / `data(64)` / `is_write(1)` / `burst_len(8)` / `id(8)` | CPU → Memory 请求 | Master → Slave |
| `cf::bundles::MemResp` | `data(64)` / `id(8)` / `error(1)` / `last(1)` | Memory → CPU 响应 | Slave → Master |
| `cf::bundles::CacheReq` | `address(64)` / `data(64)` / `is_write(1)` / `op(2)` / `id(8)` | CPU → L1Cache 请求 | Master → Slave |
| `cf::bundles::CacheResp` | `data(64)` / `hit(1)` / `error(1)` / `id(8)` | L1Cache → CPU 响应 | Slave → Master |
| `cf::bundles::L1CachePluginBundle` | `tag(20)` / `idx(8)` / `line_data(512)` / `valid(1)` / `dirty(1)` | L1Cache 内部状态 (跨阶段通信) | lookup ↔ refill |
| `cf::bundles::IntBundle` | `irq(1)` / `ack(1)` | 中断接口 (PLIC/CLINT → CPU) | Slave → Master |

### 字段位宽选择依据

- `address(64)`: RV64 物理地址空间 (sv39/sv48)
- `data(64)`: RV64 XLEN = 64 位寄存器宽度
- `burst_len(8)`: AXI 标准 (0=1 beat, 255=256 beats)
- `id(8)`: 256 个 in-flight 事务 (Phase 2+ RTOS 足够)
- `op(2)`: 4 种操作编码 (Read/Write/Invalidate/Flush)
- `tag(20)`: 假设 40-bit 物理地址, 8-bit idx, 12-bit offset (4KB 页, 256 sets)
- `idx(8)`: 256 sets (典型 32KB 8-way L1)
- `line_data(512)`: 64 字节 cache line
- `valid(1)` / `dirty(1)`: MESI/Write-back 最小集

## 4. 使用示例

```cpp
#include "bundles/mem_bundles.h"

using cf::bundles::MemReq;
using cf::bundles::MemResp;

// CPU 发起读请求
MemReq req{};
req.address = 0x80000000ULL;  // OpenSBI 入口
req.is_write = false;
req.burst_len = 0;  // 单次读
req.id = 1;

// Memory 响应
MemResp resp{};
resp.data = *(uint64_t*)0x80000000;
resp.id = 1;  // 匹配请求 ID
resp.error = false;
resp.last = true;
```

## 5. D4 合规静态检查

```bash
# 禁止 Bundle 字段使用 raw uintN_t
! grep -rnE "uint(32|64)_t +(address|data|is_write|burst_len|id|tag|idx|line_data|valid|dirty|irq|ack|error|last|hit|op)" bundles/

# 禁止 Bundle 字段直接使用 ch_uint<>
! grep -rnE "ch_uint<\d+> +(address|data|...)" bundles/
```

## 6. Phase 0 已知限制

`cf::plugin::uint_t<N>` 当前 N > 64 时退化为 `uint64_t`（见 `include/cf/plugin/uint_t.h:37` 兜底）。影响：
- `L1CachePluginBundle::line_data(512)` 实际只存 64-bit (MSB 截断)
- Phase 1.1 不涉及完整 cache line 数据流（只用 64-bit 写穿场景），未触发限制
- **Phase 6 升级** 将引入 `__int128` (GCC/Clang) 或 `boost::multiprecision::uint512_t`

## 7. 退出标准（Phase 1.1 子任务）

- [x] 6 个 Bundle 类型全部定义
- [x] 所有字段使用 `cf::plugin::uint_t<N>` (D4 合规)
- [x] 单元测试 `test_mem_bundles.cpp` 9/9 PASS
- [x] 编译干净 (`-Wall -Wextra -Wpedantic`)
- [x] D4 静态检查脚本就绪（§5）

## 8. 与其他 Phase 关系

| 阶段 | 与 bundles/ 关系 |
|------|------------------|
| Phase 0 | `cf::plugin::uint_t<N>` 是字段类型基础 |
| Phase 1.2 (L1CachePlugin) | **直接消费** 所有 6 个 Bundle |
| Phase 2 (Bare-metal) | 增加 `TraceBundle` (HTIF tohost/fromhost) |
| Phase 3 (RTOS) | 增加 `TimerBundle`, `PLIC 多源中断 Bundle` |
| Phase 5 (RTL) | BundleMapper 转换为 `ch_uint<N>` + `bundle_base<T>` 派生 |
| Phase 6 (完整框架) | 引入 JSON 描述 + 自动 codegen |

# ChipForge IP 目录 (ip-catalog)

> **📌 作用**: 项目级"IP 总目录"。**新人第一站** — 读完本文件即可知道项目中有哪些 IP、各自用途、成熟度、如何查阅。
>
> **本文件不可替代**单 IP 文档; 单 IP 的设计细节请去 [`ip/<name>/docs/`](../../../ip/<name>/docs/README.md)。

## 维护约定

- 每次新增/删除/重大重构 IP 时, **必须同步更新本文件** (建议在 PR 中标记 reviewer 检查)
- 状态字段 (`规划中 / 设计中 / 实现中 / 稳定 / 弃用`) 是仓库可信度指标, 不要随意标"稳定"
- "可独立使用" 标记: `是` = 该 IP 可在 SoC 之外被其它 IP 直接调用; `否` = 必须通过特定 SoC 配置才能实例化

## IP 索引

> **速查表**: 按"角色"分组, 每组内按字母序排列。点开进入单 IP 主页 `ip/<name>/README.md`。

### 计算核心 (Compute)

| IP | 状态 | 类型 | 关键参数 | 可独立使用 | 实现范围 | 实施预计 | 主页 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `cpu` | 🟡 设计中 | RISC-V CPU (Plugin + Stageable) | `isa`, `pipeline_stages`, `branch_predictor` | 是 | Plugin + Stageable 架构设计 + 5-stage RISC-V 指令子集 (35 子测试 PASS in 5.28s) | Phase 1.4+ (CPU Plugin 实施推迟到 Phase 1.4, 等 L1CachePlugin 稳定) | [ip/cpu/](../../../ip/cpu/README.md) |
| `tilecore` | 🟡 初始设计 | 双层脉动矩阵乘 (类 NVIDIA Tensor Core) | Tile 大小, dtype 集合, CNU 配置 | 否 (依赖 `tilecopy`) | 0 LOC, 仅 `STATUS.md` (INITIAL DESIGN) + `docs/architecture.md` (27KB 设计文档) | Phase 5+ (见 [`ip/tilecore/STATUS.md`](../../../ip/tilecore/STATUS.md) + `docs/roadmap/phases/phase-5-rtl.md`, GPU 形态) | [ip/tilecore/](../../../ip/tilecore/README.md) |

### 数据搬运 (Data Movement)

| IP | 状态 | 类型 | 关键参数 | 可独立使用 | 实现范围 | 实施预计 | 主页 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `tilecopy` | 🟡 初始设计 | Tile 级异步数据搬运 (类 TMA) | Tile 形状, scope (cta/cluster) | 是 | 0 LOC, 仅 `STATUS.md` (INITIAL DESIGN) + `docs/architecture.md` (5KB 设计文档) | Phase 5+ (见 [`ip/tilecopy/STATUS.md`](../../../ip/tilecopy/STATUS.md) + `docs/roadmap/phases/phase-5-rtl.md`) | [ip/tilecopy/](../../../ip/tilecopy/README.md) |

### 存储 (Memory)

| IP | 状态 | 类型 | 关键参数 | 可独立使用 | 实现范围 | 实施预计 | 主页 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `cache` | 🟡 TLM 实现中 (Phase 1.3, L1 unified direct-mapped 16KB, L1I/L1D/L2 未拆分) | 多级缓存 (L1I/L1D/L2) | `size_kb`, `associativity`, `replacement_policy` | 是 | `L1CachePlugin` (256 sets × 1 way × 64B = 16KB direct-mapped) + `L1CacheTLMBridge` + `L1CacheTLMBridgeAdapter` (cpptlm ModuleFactory 兼容层 + ch_stream 注册) + `ReplacementPolicy` 抽象接口 (Phase 1.4 落地: NoReplacementPolicy + LRUPolicy reference impl). 5 个 unit tests + bridge/e2e/instantiate 测试 PASS. Phase 1.5 待实施: L2CachePlugin (8-way) + JSON 配置驱动策略选择 | Phase 1.4+ (ReplacementPolicy 接口已落地; L2CachePlugin 推迟到 Phase 1.5) | [ip/cache/](../../../ip/cache/README.md) |
| `memory` | 🔴 规划中 | 主存 (SRAM/DRAM 模型) | `size_mb`, `latency_cycles` | 是 | 0 LOC, 仅 `STATUS.md` (PLANNED) | Phase 2+ (见 [`ip/memory/STATUS.md`](../../../ip/memory/STATUS.md) + `docs/roadmap/phases/phase-2-baremetal.md`) | [ip/memory/](../../../ip/memory/README.md) |

### 互连 (Interconnect)

| IP | 状态 | 类型 | 关键参数 | 可独立使用 | 实现范围 | 实施预计 | 主页 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `interconnect` | 🔴 规划中 | 总线 / NoC | `topology`, `data_width` | 是 | 0 LOC, 仅 `STATUS.md` (PLANNED) | Phase 2+ (见 [`ip/interconnect/STATUS.md`](../../../ip/interconnect/STATUS.md) + `docs/roadmap/phases/phase-2-baremetal.md`) | [ip/interconnect/](../../../ip/interconnect/README.md) |

### 外设 (Peripheral)

| IP | 状态 | 类型 | 关键参数 | 可独立使用 | 实现范围 | 实施预计 | 主页 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `peripheral` | 🔴 规划中 | PLIC / CLINT / UART / Timer | 子模块清单, 中断路由 | 是 | 0 LOC, 仅 `STATUS.md` (PLANNED) | Phase 3+ (见 [`ip/peripheral/STATUS.md`](../../../ip/peripheral/STATUS.md) + `docs/roadmap/phases/phase-3-rtos.md`) | [ip/peripheral/](../../../ip/peripheral/README.md) |

## IP 间依赖图

```
tilecore ──depends──▶ tilecopy
    │
    └──needs──▶ interconnect ──routes──▶ memory
                                       └─cache
cpu ──via──▶ interconnect
```

> 解读: `tilecore` 强依赖 `tilecopy` 搬运数据; 所有计算核心经 `interconnect` 访问 `memory`/`cache`。
> 详细依赖矩阵在 SoC 配置中显式声明 (见 [`soc/docs/integration-guide.md`](../../../soc/docs/integration-guide.md))。

## 文档层次导航

```
ip-catalog.md  (本文)           ← 你在这里
  └─ ip/<name>/README.md        ← 单 IP 入口
       ├─ docs/README.md        ← 该 IP 文档索引
       ├─ docs/architecture.md  ← 微架构
       ├─ docs/configuration.md ← 可调参数 (推荐补)
       └─ docs/integration.md   ← 集成方式 (可选)
```

详细规范见 [`ip/README.md`](../../../ip/README.md) 的 "`ip/<name>/docs/` 标准子结构" 段落。

## 状态图例

| 标记 | 含义 |
| --- | --- |
| 🔴 规划中 | 仅 ADR/草图, 无代码 |
| 🟡 设计中 / 初始设计 | 有 README + 架构文档, 代码未落地或仅骨架 |
| 🟡 TLM 实现中 | TLM 模型落地, RTL 待做 |
| 🟢 稳定 | 已通过 L1/L2 集成测试, 可在多个 SoC 中复用 |
| ⚫ 弃用 | 已被新 IP 替代, 仅保留以支持历史 SoC |

## 相关文档

- [架构总览 (overview.md)](overview.md) — 项目级架构图
- [接口设计 (interface-design.md)](interface-design.md) — 跨 IP 接口约定
- [SoC 集成指南](../../../soc/docs/integration-guide.md) — 如何在 SoC 中组合 IP
- [路线图](../roadmap/roadmap-status.md) — IP 成熟度演进时间表

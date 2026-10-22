---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: ready
depends_on: []
supersedes: []
related_to:
  - ADR-040 v2.0/v3.0 (TLM↔CH_MEM 双模同语义 + payload key 模态约束 §10.6)
  - ADR-083 (Plugin-style 单一 source of truth — 双文件分离)
  - objective-ip-mmu-mmu-chmem-phase-b-e.md (review_by=2027-01-06)
  - objective-soc-cpu-v0110-launch.md (v0.11.0 launch gate)
  - openspec/changes/mmu-chmem-phase-b-c-launch (Phase B 的 B.3/B.4 依赖本 change)
  - .rddf/plans/chmem-mmu-keys-modality.md (TDD 5 步施工计划)
  - .rddf/plans/20261022-v0110-mmu-chmem-execution-sequence.md (执行序列决策纪要 §4 K2/K4/K5)
---

# chmem-mmu-keys-modality — MMU payload key 模式化 + CH_MEM 生产者/消费者顺序契约

> **状态**: 🟢 ready → 实施中（2026-10-22 build agent）。
> **定位**: **`mmu-chmem-phase-b-c-launch` Phase B 的 B.3/B.4 硬前置** —— 本 change archive 之前 Phase B 不可开工。

## Why

Phase B 的 B.3 要实装 `MMUPlugin_chmem`（单级 `ch_mem` TLB + 复用 `PtWalkFsmPlugin`），发射 `MMU_VADDR` / `PADDR` / `PADDR_VALID` / `PTW_ACTIVE` 四个 payload key。**当前这条通道装不下它们**：

1. **key 非 mode-aware** —— `ip/mmu/tlm/mmu_keys.h` 的 key 用**裸类型**（`Payload<std::uint64_t>` / `Payload<bool>`）。CH_MEM 模式下 `PayloadStore::put` 是**精确类型匹配**（`include/cf/plugin/payload.h:122-128`），裸类型 cell 只能装 **POD 常量**，装不下 `ch_bool` / `ch_uint` ⇒ 消费端 `ch_bool(cell)` 把 DAG 节点**折成 elaboration 期常量** ⇒ **sv32 翻译退化为常量路径**。
   - **编译器实证**：`put(Payload<bool>&, ch_uint<1>)` → `error: no matching function for call to 'PayloadStore::put(Payload<bool>&, ch_uint<1>)'`；`get(Payload<bool>&)` → `bool&`（POD）。
   - **仓库内活体实例**：`ip/cpu/plugins/csr_chmem.h:191-194` 用 `static_cast<bool>(select(...))` 把运行期表达式折成常量写入 `Payload<bool>` ⇒ CSR 写使能经 payload 通道实际是常量化（测试靠 `last_csr_write_en_` 观察口绕开 payload 通道验证，故全绿）⇒ **"全绿但零语义"在本仓是既有模式，不是理论风险**。
2. **elaboration 顺序缺陷** —— `canonical_stage_order()` 是 stage 名字在 `stages_` 的**首现序**（`include/cf/plugin/pipe_builder.h:215-224`），与 plugin 注册顺序无关。factory 在 `pb->build()` 之前注册了 `at_stage("fetch", EARLY, populate)`（`ip/cpu/cpu_factory_chmem.h:204`），⇒ `"fetch"` 被钉在最前；而 `MMUPlugin_chmem` 的 `at_stage("tlb_lookup_ifetch", ...)` 只能在 `pb->build()` 内注册 ⇒ **消费者恒先于生产者跑** ⇒ `has()` 恒 false ⇒ **永久静默 Bare**（CH_MEM 的 `elaborate()` 只跑一遍，没有 TLM "下一拍 cell 还在"的补偿）。
   - loadstore 侧能否排在前面**取决于 MMU 与 DMem 的注册顺序**（碰巧成立才成立）⇒ 一腿死、一腿活，且**完全静默**。

⇒ 二者叠加：Phase B 按原计划实施，会**编译通过、测试全绿、翻译永不发生**。

## What Changes

| # | 改动 | 文件 |
|---|---|---|
| 1 | 5 个目标 key 改 mode-aware（`cf::plugin::uint_t<N>`）：`MMU_VADDR` / `PADDR` / `EXCEPTION_CODE` / `PTW_ACTIVE` / `PADDR_VALID` | `ip/mmu/tlm/mmu_keys.h` |
| 2 | 2 个 CH_MEM 消费点改为「读 DAG 节点」而非「读常量」 | `ip/cpu/plugins/ibus_chmem.h`、`ip/cpu/plugins/dmem_chmem.h` |
| 3 | 新增 CH_MEM 单测（类型断言 + 鉴别力 + 顺序契约 + 变异对照） | `tests/mmu/test_mmu_keys_modality.cpp`（**手动加入 `CHMEM_TEST_SOURCES`**） |
| 4 | 新增 CI 检查（**WARN-only** + 已知违规 allowlist） | `tools/check_plugin_portability.sh` Check 13 |
| 5 | ADR-040 v3.0 新增「payload key 模态约束」节 + 新 capability spec | `docs/architecture/adr/ADR-040-*.md`、`openspec/specs/payload-key-modality/spec.md` |
| 6 | 文档同步（CHANGELOG / AGENTS.md 纪律段 / 37KB plan key 裁决 / handoff） | 多处 |

**不做**（姊妹 change `chmem-cpu-keys-modality`，挂 Phase C.1 入口）：`ip/cpu/tlm/cpu_keys.h` 4 key + `bundles/csr_bundles.h` 4 bool key + `ip/cache/plugins/cache_keys.h` 1 key + `mmu_keys` 其余 8 key + `cpu_keys.h:34-36` static_assert 放宽 + `csr_chmem.h:191-194` 常量化修复。

## 顺序契约（本 change 的核心交付之一）

> CH_MEM 模式下 payload cell 里的 ch 代理是「C++ 侧重绑的指针」（`build/_deps/install/include/cpp-hdl/lnode/logic_buffer.tpp:21-26` 的 `logic_buffer<T>::operator=` 只重绑 `node_impl_`），消费者**读取瞬间快照**该指针 ⇒ **生产者必须先写**。
>
> **唯一不依赖 plugin 注册顺序的做法**：producer 与 consumer 挂**同一个 stage 名**，producer 用 **EARLY**、consumer 用 **NORMAL**（`Phase` enum: EARLY=0 < NORMAL=1 < LATE=2；`pipe_builder.h:414-424` 同 stage 内按 phase 顺序、同 phase 桶内按插入序稳定排序）。
>
> producer **不得**注册新的 stage 名（会进 `stages_` 改变 canonical order）；需要 node 时用 `declare_substage(parent, sub)`（只建 node，不进 `stages_`，`pipe_builder.h:122-132`）。

## 验收（AC-MK-1..17，详见 tasks.md）

**判据 = failing-set 逐条相等**（baseline：TLM 481 case 含 1 known fail `test_7stage_riscv.cpp:124 node_count==17`；CH_MEM 114 case / 19622 assertions；`openspec validate --specs --strict` 71 passed + 4 pre-existing failed），**不是"全绿"**。

---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: draft
depends_on: []
supersedes: []
related_to:
  - ADR-040 v2.0 (TLM↔CH_MEM 双模同语义)
  - ADR-083 (Plugin-style 单一 source of truth)
  - feat-rv32um-m-extension.md (phase-4)
  - openspec/specs/chmem-mext-datapath/spec.md (本 change 为其补单元级语义锚)
---

# chmem-alu-unit-semantics — CH_MEM ALU 单元级语义锚 (跑真实 select 树)

> **范围 (scope)**：1 个新测试文件 + 1 处 legacy 注释 + 文档 + spec。**不改任何业务实现。**
> **触发 (trigger)**：`chmem-mext-comb-datapath` 验收时发现 AGENTS.md 自述的 known issue
> "CH_MEM ALU **无单元级语义测试**（`m3_poc_alu_elaborate` 只验 `alu.v` 生成，非语义锚）"。
> **目标版本**：v0.11.0。
> **治理路径**：rdd-builder（测试专用 + spec delta；无公开 API 变更，无业务代码改动）。

## Why

### 1.1 现状：既有 "ALU 测试" 零覆盖真实 datapath

`tests/cpu/test_cpu_rtl_regfile_alu.cpp` 的 4 个 ALU 相关 case 实况：

| case | 实际测什么 | 跑真实 select 树？ |
|---|---|---|
| `m3_poc_alu_elaborate` | 只验 `alu.v` 非空 + 含 "module" | ❌ 无语义 |
| `m3_poc_alu_byte_equal` | 比对 `tlm_alu_compute` vs `chmem_ref_alu`（**两份手写 C++ 镜像**） | ❌ 与真实实现无关 |
| `m3_poc_5stage_byte_equal` | 比对 `tlm_alu_compute_op` vs `chmem_alu_compute_op`（**又是两份手写 C++ 镜像**） | ❌ 与真实实现无关 |
| `m3_poc_regfile_alu_combined` | 只验 elaboration 不崩 + verilog 非空 | ❌ 无语义 |

→ "byte-equal" 实为 **C++ 镜像自比**，对 `int_alu_chmem.h` 的 select 树零覆盖，却给出"已对标"的虚假信心。

### 1.2 缺口：接线层无任何测试锚

`tests/framework/test_chmem_arith_poc.cpp` Part D 覆盖 8 条 M-op 表达式，但那是**裸 `ch_uint` 表达式**，不经 `RiscvIntAluPlugin` ⇒ **不锚**：

- `is_mext` gate（`funct7 == 0x01`）是否正确
- 8 层 M-op select 在 11 层 base 树**之后**的顺序
- 6 个 base R-type op 的 `f7_00` 加固是否误伤 `is_opimm` 侧
- `RD_DATA` 写者链

即"表达式对、接线错"这一整类 bug 当前**无任何测试能发现**。

### 1.3 风险

- `chmem-mext-comb-datapath` 把 19 层 select 树的判别逻辑全部押在 gate 上，gate 写错只会表现为"某类 ELF 数值错"，端到端 ELF 测试定位成本高
- 未来的 FSM 路径（`chmem-mext-fsm-cycle-parity`）会重写同一棵树，届时若无语义锚，回归无判据

## What Changes

| # | 文件 | 改动 |
|---|---|---|
| 1 | `tests/cpu/test_cpu_chmem_alu_semantics.cpp` | **新增**：真跑 19 层 select 树的单元语义锚，6 个 TEST_CASE / 39 组求值 / 288 assertions |
| 2 | `tests/cMakeLists.txt` | `CHMEM_TEST_SOURCES` 加入新文件（**显式列表，非 GLOB** —— 漏加则测试静默不存在） |
| 3 | `tests/cpu/test_cpu_rtl_regfile_alu.cpp` | 仅注释：警告既有 "byte-equal" 是 C++ 镜像自比，不可当回归锚；指向新文件 |
| 4 | `AGENTS.md` | 关闭 known issue（ALU 单元语义测试已补）+ `[chmem]` family 计数 66 → 72 |
| 5 | `openspec/specs/chmem-alu-semantics/spec.md` | 新 capability（archive 时 apply） |

**Non-Goals**：

- **不改任何业务实现**（`int_alu_chmem.h` / `cpu_factory_chmem.h` / TLM 路径全部不动）—— 本 change 只加测试
- **不删 legacy "byte-equal" 测试**（避免动既有基线；只加警告注释）
- **不做 CH_MEM ALU 的可综合性改造**（组合除法仍 emit `/`、`%`；可综合版归 `chmem-mext-fsm-cycle-parity`）

## Impact

- **受益**：CH_MEM ALU 的 gate / select 顺序 / `f7_00` 加固 / `RD_DATA` 写者链首次有单元级锚；M-op 接线类 bug 从"端到端数值错"降级为"单测即报"
- **回归面**：新文件只加测试，不改实现 → 业务行为零变化；`[chmem]` family case 数 66 → 72（AGENTS.md 同步）
- **开销**：39 组求值 ≈ 0.42s（实测），每组独立 context；`[chmem]` 全套 833s → 增量可忽略
- **风险**：期望值若手算错会误报 —— 实测已发生 1 次（`ANDI(0xF0,-4096)` 我算成 0xF0，实为 0x00，**ALU 是对的**），已修正；这正是"独立手算期望值"的价值

## Acceptance

见 `specs/chmem-alu-semantics/spec.md`。摘要：

1. 6 个 TEST_CASE / 39 组求值 / 288 assertions 全 PASS，0.42s 量级
2. 变异对照：V2a（去 `f7_01` gate）→ 被抓住；V2c（删 M-op 层）→ 被抓住；V2b'（移层 + 去加固）→ 被抓住
3. `[chmem]` 全套零新增 FAIL（10 个 pre-existing rv32ui load/store FAIL 不变）；`[verilator]` 48 ELF 不回归；TLM 全量无新增 FAIL
4. 5 个架构门禁全 exit 0
5. `openspec validate --specs --strict` 对新 spec 全 PASS
6. 文档一致：AGENTS.md known issue 关闭 + family 计数更新

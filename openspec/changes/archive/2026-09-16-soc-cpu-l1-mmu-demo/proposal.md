# Change: `soc-cpu-l1-mmu-demo` — Phase 1.5 Wave 2 TLB refill + 结构验证 demo + Wave 1 lessons-learned (修订版)

> **Schema**: spec-driven
> **Date**: 2026-09-15
> **Status**: PROPOSED
> **Priority**: Wave 2 of Phase 1.5（详见 `soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md`）
> **Depends on**: `riscv-tests-rv32ui` (v0.2.2, archived 2026-09-15)
> **Unblocks**: `cache-dse-sweep` · `cpu-pipeline-multi-cycle` · `plugin-framework-cycle-precision`

## Why

> Wave 1 (`riscv-tests-rv32ui`) 完成客观 30/40 PASS 基线。Wave 2 兑现 PTW TLB refill（关闭 `tlb_lookup_ifetch` stall 链空转），把 CPU+MMU+Memory 结构验证 demo 跑通 ≥5 个 riscv-tests 用例到 `tohost=1`，并把 Wave 1 暴露的偏差归档为 lessons-learned。
>
> **修订说明 (2026-09-16, Oracle+Metis 评审后)**：原"端到端 CPU+MMU+L1+Memory demo"被重新界定。评审发现 (1) MMU 地址翻译在 CPU 流水线中是装饰性的（IBus/DBus 不消费 PADDR，无 issue_request 调用者），(2) PTW 从 stub 内存读 PTE 而非真实内存，(3) 代码库中不存在 `cf::soc::Topology::from_json` API。因此 demo 缩窄为 **TLB refill 验证 + CPU+MMU+Memory 结构 smoke**，L1 仅在 JSON 声明不实例化，PTW real-memory wiring + PADDR consumption + 典范顺序修复统一推迟到 Wave 3。

## What Changes

> **Capabilities**: 2 NEW + 1 MODIFIED + 1 LESSONS-LEARNED SPEC
> - `soc-cpu-mmu-demo-topology` NEW: demo 结构契约 (CPU+MMU+Memory，C++ 手动构建 + JSON 结构验证；L1 声明不实例化)
> - `mmu-ptw-tlb-refill` NEW: PTW 完成回调除写 PADDR 外还须通过 `MultiLevelTLB::refill_from_ptw` refill 统一 TLB
> - `riscv-tests-fixture` MODIFIED: 矩阵 row 5/10 个新增 category `feature stub` + LOAD width extraction OOS Wave 2 路由
> - `phase-1.5-wave1-retro` NEW (lessons-learned spec, 不带 production code): 归档 rv32ui 矩阵偏差 + 5 个真 bug 超出 fix-rv32ui-N scope 的事实 + 7-stage segfault 残留

## 1. Why（背景与动机）

### 1.1 Wave 1 完成状态 (2026-09-15, post-`riscv-tests-rv32ui` v0.2.2)

**`[riscv-tests]` 30 PASS / 10 FAIL**（`soc/cpu/docs/dse/rv32ui-baseline-matrix.csv`）：
- 30 PASS: add/addi/and/auipc/beq/bge/bgeu/blt/bltu/bne/jal/jalr/lui/or/ori/simple/sll/slli/slt/slti/sltiu/sltu/sra/srai/srl/srli/sub/xor/xori
- 10 FAIL (全部 `category=feature stub`, LOAD width extraction OOS):
  - `lb/lbu/lh/lhu/lw` — 直接缺宽度提取
  - `sb/sh/sw/st_ld/ld_st` — 内嵌 load 回读验证失败同源

**4 架构 gates PASS**, 全量 `chipforge_tests` 378/389 PASS。

### 1.2 Wave 1 实际现象 vs 原 roadmap 触发条件的偏差

| 项 | roadmap 预期 | Wave 1 实际 | 偏差含义 |
|----|--------------|--------------|----------|
| `riscv-tests-rv32ui` 通过率 | "≥60% PASS (不含 CSR/trap)" | 30/40 = **75%** | ✅ 超预期 |
| `cpu-pipeline-fix-rv32ui-N` 触发 | "≥3 真 bug" | 10 fail **全 feature stub**，但本次 d279635 顺带修了 **5 个真 bug**（hazard deadlock、branch B-type 误判、AUIPC 未实现、decode writes_rd 误判、env-p harness），**超出该 change 原计划 scope** | ⚠️ 原 change scope 与实际 fix mismatch |
| `soc-cpu-l1-mmu-demo` 触发 | "Wave 1 通过率 ≥40%" | 30/40 = 75% | ✅ 满足；但 demo 实际意义（PTW 接线）与通过率松耦合 |
| 5 个 pre-existing RISC-V 仿真失败 | "toolchain 配置问题，与代码无关" | 4 个被本次 CPU 修复治愈（3/5/10-stage + real_tohost），仅 7-stage superscalar segfault 残留 | ⚠️ 原归因错误（应为 pipeline bug） |
| 7-stage superscalar cpu_sim | 未明确记录 | `git stash` 验证 segfault 与本次修复无关 | 🆕 独立 pre-existing 问题，建议独立归档 |

### 1.3 Wave 2 价值与范围（修订）

roadmap §4 Wave 2 双轨：
1. `soc-cpu-l1-mmu-demo` — demo（修订后：CPU+MMU+Memory 结构 smoke + TLB refill 验证）
2. `cpu-pipeline-fix-rv32ui-N` — 修 X 个真 bug（**已实质完成大部分**，scope 仅剩 byte-wise tohost edge + 7-stage segfault，可压缩归档）

本次 change **合并 B+C**（用户指示），修订后范围：
- 主任务: PTW TLB refill 接线 + CPU+MMU+Memory 结构验证 demo（C++ 手动构建，JSON 结构验证）
- lessons-learned: 把 Wave 1 的偏差（含 d279635 已超出原 fix-rv32ui-N scope 的事实、7-stage segfault 残留、矩阵分类演进）作为 spec 永久化供未来会话参考
- 缩减的 `cpu-pipeline-fix-rv32ui-N`（仅剩 byte-wise tohost edge）作为 demo 任务的 follow-up 候选（不在本次 scope）

## 2. What Changes（变更范围）

### 2.1 框架层

| 改动 | 位置 | 行数估计 |
|------|------|----------|
| 新 `soc/cpu_l1_mmu_demo.json`（结构声明，含 L1 声明） | `soc/` | +40 |
| PTW 完成回调补 TLB refill（`multi_tlb_->refill_from_ptw`） | `ip/mmu/tlm/MMUPlugin.cpp` | +15 |
| `test_cpu_l1_mmu_demo` runner: C++ 手动构建 CPU+MMU+Memory + 植入 stub PTE + 选 5 riscv-tests 通过子集端到端 tohost=1 | `tests/soc/` | +30 |
| `riscv-tests-fixture` spec delta: matrix schema 加 `feature stub` category + LOAD width OOS 路由注释 | `openspec/changes/soc-cpu-l1-mmu-demo/specs/riscv-tests-fixture/spec.md` (delta) | +20 |
| `phase-1.5-wave1-retro` lessons-learned spec（永久归档偏差） | `openspec/changes/soc-cpu-l1-mmu-demo/specs/phase-1.5-wave1-retro/spec.md` | +30 |
| docs: `soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md` §7 Lessons learned 节补录 Wave 1 偏差 | `soc/cpu/docs/roadmap/` | +30 |
| ADR-045 注解：stall 机制 inert until PADDR consumption (Wave 3) | `docs/architecture/adr/ADR-045-*.md` | +10 |

### 2.2 PTW TLB refill 接线（修订）

roadmap line 195 已识别："`tlb_lookup_ifetch` 在 PTW 期间不 stall / 无 TLB refill"。当前 `MMUPlugin.cpp:60-64` PTW 成功回调 `(uint64_t paddr, uint8_t perms)` 仅写 PADDR → tlb_lookup_ifetch 节点。修订后：在 PTW 成功回调内通过 **`multi_tlb_->refill_from_ptw(vaddr, current_asid_, paddr, perms)`** 把 walk 结果写入统一 TLB（`multi_level_tlb.cpp:44` 已有该 API），避免下一 fetch 又重复 start_walk。回调闭包需捕获 `this` 以访问 `current_asid_` 与 `multi_tlb_`。

**不做**（推迟到 Wave 3，与 PADDR consumption 一起）：PTW 从真实内存读 PTE（当前 `ptw.cpp:95-101` 仍从 `pte_stub_memory_` 读）。demo 用 `stub_write_pte` 植入 identity 映射。

### 2.3 demo JSON 拓扑（结构声明）

`soc/cpu_l1_mmu_demo.json` 为**结构声明**（`test_mmu_minimal_json.cpp` 式的结构验证），不驱动运行时实例化（无 JSON→Plugin instantiator，不存在 `cf::soc::Topology`）：

```json
{
  "name": "cpu_l1_mmu_demo",
  "memory_map": [
    {"name": "pci_region", "base": "0x80000000", "size": "64KB", "type": "ram"}
  ],
  "components": [
    {"type": "cf::cpu::CpuFactory", "config": "ip/cpu/configs/cpu_default.json"},
    {"type": "cf::ip::mmu::MMUPlugin", "sv": "sv32"},
    {"type": "cf::ip::cache::L1CachePlugin", "size_kb": 4, "assoc": 1},
    {"type": "cf::cpu::PicolibcHostMemory", "base": "0x80000000", "size": "64KB"}
  ]
}
```

L1CachePlugin 仅在 JSON 中**声明**，runner 不实例化（instantiation deferred to Wave 3 `cache-dse-sweep`）。

### 2.4 Wave 2 通过率门槛

demo 成功判定：跑 `riscv-tests-rv32ui` 矩阵中 5 个 PASS 用例（add/addi/auipc/jal/beq，纯整数 ALU/branch 避开 LOAD width OOS）从 C++ 手动构建的 CPU+MMU+Memory runner 端到端跑通，`tohost=1`。

## 3. Impact

### 3.1 受影响文件

- `soc/cpu_l1_mmu_demo.json` (NEW, structural topology)
- `tests/soc/test_cpu_l1_mmu_demo.cpp` (NEW, C++ manual runner + stub PTE plant)
- `tests/mmu/test_ptw_tlb_refill_integration.cpp` (NEW, PTW TLB refill unit test)
- `ip/mmu/tlm/MMUPlugin.cpp` (PTW success callback: add `multi_tlb_->refill_from_ptw`)
- `tests/cpu/integration/test_rv32ui_runner.cpp` (CSV category 注释，可选)
- `soc/cpu/docs/dse/rv32ui-baseline-matrix.csv` (regenerated)
- `soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md` (lessons learned 节)
- `docs/architecture/adr/ADR-045-plugin-ctrl-link-consumption.md` (annotation)
- `AGENTS.md` known-test-status 段（sync with v0.2.3）
- `soc/README.md` 速查表（add new JSON entry）

### 3.2 受影响 spec

- `riscv-tests-fixture` (MODIFIED delta): 矩阵 schema
- `soc-cpu-mmu-demo-topology` (NEW): demo C++ + JSON 结构契约
- `mmu-ptw-tlb-refill` (NEW): PTW refill 契约（原名 `mmu-ptw-real-memory-wiring`，改名反映实质 scope）
- `phase-1.5-wave1-retro` (NEW lessons-learned): Wave 1 偏差永久化

### 3.3 解锁项

- `cache-dse-sweep` (Wave 3): demo 端到端稳定后，cache 在真实 CPU+MMU 负载下可做 sweep。注意：L1 实例化仍需在 cache-dse-sweep 时完成。
- `cpu-pipeline-multi-cycle` (Wave 3): demo TLB-refill + stall 稳定后做 LATENCY>1 多周期。典范顺序修复（R2）在此 change 中作为前置依赖。
- `plugin-framework-cycle-precision` (Wave 3): demo 跑通后接 `pb.run(cycle_count=N)` 真 cycle 精度
# Design: `soc-cpu-l1-mmu-demo` — Wave 2 TLB refill + 结构验证 demo + lessons-learned (修订版)

> **Schema**: spec-driven
> **Date**: 2026-09-15 (revised 2026-09-16)
> **Scope**: 实现 PTW TLB refill 接线 + CPU+MMU+Memory 结构验证 demo + Wave 1 lessons-learned 归档。修订见 Oracle+Metis 评审结果：PTW refill 用 `multi_tlb_->refill_from_ptw`；C++ 手动构建 runner；L1 仅声明；R2 推迟 Wave 3。

## 1. Context

### 1.1 Wave 1 完成状态（v0.2.2）

- `[riscv-tests]` 30 PASS / 10 FAIL（10 全 feature stub：LOAD width extraction OOS）
- 4 架构 gates 全 PASS
- `chipforge_tests` 378/389 PASS
- 5 个 pipeline 真 bug 已修（d279635）：HazardPlugin 死锁、branch B-type 误判、AUIPC 未实现、decode writes_rd 误判、env-p harness

### 1.2 Wave 2 双轨

roadmap §4：Wave 2 同时做 `soc-cpu-l1-mmu-demo`（端到端 SoC）+ `cpu-pipeline-fix-rv32ui-N`（真 bug 修复）。本次合并 B+C：
- B: `soc-cpu-l1-mmu-demo` 端到端 demo + PTW TLB refill
- C: Wave 1 lessons-learned 归档（Phase 1.5 偏差永久化）
- 原 `cpu-pipeline-fix-rv32ui-N` scope 已在 d279635 完成，仅剩 byte-wise tohost edge + 7-stage segfault 单独归档

## 2. Architecture

### 2.1 demo JSON 拓扑（结构声明）

`soc/cpu_l1_mmu_demo.json` 为结构声明（仅用于 `test_mmu_minimal_json.cpp` 式 JSON 结构验证），不驱动运行时实例化。L1CachePlugin 仅在 JSON 中声明、不实例化，推迟到 Wave 3 `cache-dse-sweep`。

```
soc/cpu_l1_mmu_demo.json
├── memory_map:
│   └── pci_region: base=0x80000000 size=64KB type=ram (PicolibcHostMemory 后端)
└── components:
    ├── CpuFactory (CPU + StageLink plugins)
    ├── MMUPlugin (Sv32, unified TLB, PTW)
    ├── L1CachePlugin (4KB 1-way unified, write-through) [结构声明 only]
    └── PicolibcHostMemory (64KB window @ 0x80000000)
```

### 2.2 PTW TLB refill 数据流（修复 roadmap line 195）

```
Cycle N (TLB miss):
  IBusPlugin fetch → tlb_lookup_ifetch miss → start_walk
    → MMUPlugin walks PTW (from stub PTE) → PTE found
    → PTW success callback (MMUPlugin.cpp:60-64):
      WRITE PADDR to tlb_lookup_ifetch node        [existing]
      REFILL multi_tlb_->refill_from_ptw(vaddr, current_asid_, paddr, perms)  [NEW]
      CLEAR PTW_ACTIVE=0                            [existing]
Cycle N+1:
  IBusPlugin fetch → tlb_lookup_ifetch hit (unified TLB) → no walk
```

注意：当前 PTW 从 `pte_stub_memory_`（`ptw.cpp:95-101`）读 PTE，demo 通过 `stub_write_pte` 植入 identity 映射。PTW real-memory wiring 推迟 Wave 3（与 PADDR consumption 一起）。

### 2.3 demo runner 数据流（`tests/soc/test_cpu_l1_mmu_demo.cpp`）

C++ 手动构建（参考 `test_rv32ui_runner.cpp:100-116`），无 JSON→Plugin instantiator：

```
For each of 5 riscv-tests ELF (add/addi/auipc/jal/beq):
  1. Load ELF via load_elf_full → entry_addr, tohost_addr, sections
  2. Construct PicolibcHostMemory (window=0x80000000, tohost_addr=elf.tohost_addr)
  3. Load sections via load_section
  4. Build CPU via CpuFactory (config=cpu_default, enable_mmu=true)
  5. Plant identity stub PTE via ptw()->stub_write_pte(...) over 0x80000000 window
  6. Inject PicolibcHostMemory into IBusPlugin + DBusPlugin
  7. Set fetch PC = entry_addr
  8. For cycle in [0, 10000):
       pb->run()
       if mem.exited(): break
  9. REQUIRE(mem.exited() && mem.exit_code() == 0)
```

## 3. Implementation Details

### 3.1 PTW TLB refill（`ip/mmu/tlm/MMUPlugin.cpp`）

**修改点**：PTW success 回调（`MMUPlugin.cpp:60-64`，lambda 签名 `(uint64_t paddr, uint8_t perms)`）。

```cpp
// 当前（MMUPlugin.cpp:60-64）: 仅写 PADDR + 清 PTW_ACTIVE
// 修订后: 追加 refill_from_ptw（回调闭包需捕获 this 以访问 current_asid_ 与 multi_tlb_）
ptw_->on_success([this, node, vaddr](uint64_t paddr, uint8_t perms) {
  // 写 PADDR（existing）
  node->operator()(KeyType::PADDR) = paddr;
  node->operator()(KeyType::MMU_VADDR) = vaddr;
  // NEW: refill 统一 TLB（正确 API，见 multi_level_tlb.cpp:44）
  multi_tlb_->refill_from_ptw(vaddr, current_asid_, paddr, perms);
  // 清 PTW_ACTIVE（existing）
  node->operator()(MmuKeys::PTW_ACTIVE) = 0;
});
```

**前置条件**：`MultiLevelTLB::refill_from_ptw(vaddr, asid, paddr, perms)` 已存在（`multi_level_tlb.cpp:44`）。无需新增 `PTE::valid()` 谓词（V=0 走 `on_fault` 回调，不经过 success 路径）。

**不做**：PTW real-memory wiring（`advance_from_stub` → 真实内存读 PTE）推迟 Wave 3。A/D bit 更新（accessed/dirty）推迟（demo identity 映射无需）。

### 3.2 demo JSON（`soc/cpu_l1_mmu_demo.json`）

结构声明 JSON（`test_mmu_minimal_json.cpp` 式验证），不含运行时实例化：

```json
{
  "name": "cpu_l1_mmu_demo",
  "memory_map": [
    {"name": "ram", "base": "0x80000000", "size": "64KB", "type": "ram"}
  ],
  "components": [
    {"type": "cf::cpu::CpuFactory",  "config": "ip/cpu/configs/cpu_default.json"},
    {"type": "cf::ip::mmu::MMUPlugin", "sv": "sv32"},
    {"type": "cf::ip::cache::L1CachePlugin", "size_kb": 4, "assoc": 1},
    {"type": "cf::cpu::PicolibcHostMemory", "base": "0x80000000", "size": "64KB"}
  ]
}
```

### 3.3 demo runner（`tests/soc/test_cpu_l1_mmu_demo.cpp`）

参照 `tests/cpu/integration/test_rv32ui_runner.cpp` 模式，C++ 手动构建（无 JSON instantiator）：
- 5 个 TEST_CASE（每个 riscv-tests ELF 一个）
- 复用 `cf::tools::load_elf_full` + `PicolibcHostMemory::Config` + `CpuFactory`
- 唯一差异：`enable_mmu=true` → MMUPlugin 被 `build_cpu` 注册 → PTW + TLB refill 路径生效
- **新增**：植入 identity stub PTE（`ptw()->stub_write_pte(...)`）覆盖 0x80000000 窗口
- L1CachePlugin **不**实例化（仅 JSON 声明）

### 3.4 spec delta `riscv-tests-fixture`

参照已 archive 的 `riscv-tests-fixture` spec（v0.2.2），本 change 仅做最小 delta：
- **ADDED Requirement**: matrix schema `category` 列 `feature stub` 语义（LOAD width extraction OOS）
- **ADDED Requirement**: 40 ELF 名清单（与 v0.2.2 磁盘状态一致）
- 不动原 requirements（保留 backward compat）

### 3.5 lessons-learned spec `phase-1.5-wave1-retro`

新 spec，不带 production code（仅文档），4 个 lessons：
- **L1**: rv32ui 矩阵偏差（30 PASS / 10 feature stub vs 原计划 ≥3 真 bug）
- **L2**: d279635 5 个真 bug 超出原 `cpu-pipeline-fix-rv32ui-N` scope
- **L3**: 5 个 pre-existing RISC-V 仿真失败归因错误（4 个是 pipeline bug 而非 toolchain）
- **L4**: 7-stage superscalar segfault 残留（pre-existing）

## 4. Test Strategy

### 4.1 Test Plan

| Commit | 新增测试 | 期望结果（fix 前 FAIL, fix 后 PASS） |
|-------|----------|-----------------------------------|
| A (PTW refill) | `tests/mmu/test_ptw_tlb_refill_integration.cpp` 2 case | refill 成功 + IBus fetch hit |
| B (demo runner) | `tests/soc/test_cpu_l1_mmu_demo.cpp` 6 case (1 JSON 结构 + 5 ELF runner) | 6/6 PASS (1 JSON + add/addi/auipc/jal/beq) |

### 4.2 4 架构 gates

每 commit 后跑：
- `bash tools/verify_adr.sh`
- `bash tools/verify_plugin_decision.sh`
- `bash tools/check_plugin_portability.sh`
- `bash tools/doc_link_check.sh`

### 4.3 全量回归

每 commit 后跑 `chipforge_tests` 与 ctest，确保失败数 ≤11（baseline 378/389，+8 新测试后分母 397）。

## 5. Risk Assessment

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| MMU enable 引入新 CPU 路径 bug（hazard/branch 真 bug 可能复现） | 中 | demo fail | Commit A 先单独验证 PTW refill；Commit B 跑 [riscv-tests] 30 PASS baseline 验证不退化 |
| L1CachePlugin 声明未实例化造成读者误解 | 中 | 低 | spec 明确注明 "declared, instantiation deferred to Wave 3 cache-dse-sweep" |
| 7-stage superscalar segfault 仍遗留（独立 pre-existing） | 高 | demo 不受影响（demo 用 5-stage config） | 单独立 `cpu-pipeline-7stage-superscalar-fix` change |
| LOAD width OOS 仍 10 个 feature stub | 高 | demo 受影响（用非 LOAD 用例绕开） | 选 PASS 用例中非 LOAD 类（add/addi/auipc/jal/beq 纯整数 ALU + branch） |
| PTW stub PTE 植入后 V=0 路径 | 低 | demo 异常 | stub PTE 全部 V=1 identity；V=0 走 fault 回调（非 success 路径），不触发异常 |
| 无 JSON instantiator（不存在 `cf::soc::Topology`） | 确定 | 阻塞原 JSON 方案 | C++ 手动构建 runner；JSON 仅结构验证 |

## 6. Open Questions

- Q1: demo 是否需要支持 ecall（riscv-tests v2 退出口）？**答**: 不需要。demo 用 rv32ui-p-* ELF 通过 v0.2.2 env-p harness（直接 `sw tohost`），不触发 ecall。
- Q2: 5-stage superscalar 是否也做 demo？**答**: 7-stage segfault 阻塞 superscalar；demo 仅跑 default 5-stage config（`cpu_default.json`）。

## 7. Migration / Rollout

无 migration（demo 是新拓扑）。Rollout：main branch 4 commits → CHANGELOG v0.2.3 → archive change → Wave 3 DSE 启动。

## 8. References

- `soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md` (Wave 2 定义)
- `openspec/changes/archive/2026-09-15-riscv-tests-rv32ui/` (v0.2.2 predecessor)
- `openspec/changes/archive/2026-09-15-plugin-framework-stall/` (stall 原语来源)
- `docs/architecture/adr/ADR-040.md` (D4 范式)
- `docs/architecture/adr/ADR-045.md` (CtrlLink 消费契约)
- `docs/architecture/plugin-style-design-methodology-v1.md` (D4 方法学)
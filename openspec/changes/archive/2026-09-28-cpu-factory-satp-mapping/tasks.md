# Tasks

> **TDD 5 步**：每条任务标 [RED → GREEN → REFACTOR] 阶段
> **父 change**: debug-cpu-l1-mmu-demo-deep-rca (v0.10.1 archive, 2026-09-28)
> **Oracle+Metis 双审修订**: 编码公式 / scope 扩张 / 负向测试 全部纳入

## Phase A — Red: 写 failing unit test (CpuFactory_MMUCfg_PassesSatpValue)

- [ ] A.1 在 `tests/cpu/test_cpu_factory.cpp` 加 TEST_CASE "CpuFactory_MMUCfg_PassesSatpValue", 5 个 SECTION:
  - SECTION "Sv32": `REQUIRE(make_satp_value(SvMode::Sv32, 0x80000) == (1ULL << 31) | 0x80000)`
  - SECTION "Sv39": `REQUIRE(make_satp_value(SvMode::Sv39, 0x100000) == (8ULL << 60) | 0x100000)`
  - SECTION "Sv48": `REQUIRE(make_satp_value(SvMode::Sv48, 0x100000) == (9ULL << 60) | 0x100000)`
  - SECTION "Bare": `REQUIRE(make_satp_value(SvMode::Bare, 0x12345) == 0)`
  - SECTION "Negative PPN masked": `REQUIRE((make_satp_value(SvMode::Sv39, 0xFFFFFFFFFFFFFULL) & 0xFFFFFFFFFULL) == 0xFFFFFFFFFULL)` (PPN 超大被 mask 限到 44 bits)
- [ ] A.2 `cmake --build build --target chipforge_tests -j$(nproc)` → 期望 5/5 SECTION FAIL (RED, 因为 helper 函数还不存在)

## Phase B — Green: 改 cpu_factory.h + mmu.h ctor + helper 实现

- [ ] B.1 在 `ip/cpu/cpu_factory.h` 加 `namespace cf::cpu::detail`:
  - `inline std::uint64_t make_satp_value(cf::ip::mmu::SvMode sv_mode, std::uint64_t satp_ppn)` — per RISC-V Spec §4.3.1
    - Bare → 0
    - Sv32 → `(1ULL << 31) | (satp_ppn & 0x3FFFFFULL)` (PPN 22 bits, NO shift)
    - Sv39 → `(8ULL << 60) | (satp_ppn & 0xFFFFFFFFFULL)` (PPN 44 bits, MODE=8, NO shift)
    - Sv48 → `(9ULL << 60) | (satp_ppn & 0xFFFFFFFFFULL)` (PPN 44 bits, MODE=9, NO shift)
    - default → 0 (defensive)
  - `inline std::uint64_t extract_satp_ppn(cf::ip::mmu::SvMode sv_mode, std::uint64_t satp_value)` — mode-aware PPN 提取
    - Sv32 → `satp_value & 0x3FFFFFULL`
    - Sv39/Sv48 → `satp_value & 0xFFFFFFFFFULL`
    - Bare → 0
  - 加注释引用 RISC-V Privileged Spec §4.3.1 satp CSR layout
- [ ] B.2 `ip/cpu/cpu_factory.h:390` 把 `/*satp_value=*/0` 改为 `cf::cpu::detail::make_satp_value(sv_mode, config.satp_ppn)`
- [ ] B.3 `ip/cpu/cpu_factory.h` `CPUConfig` 结构加 `std::uint64_t satp_ppn = 0;` 字段 (additive, 默认 0 → Bare mode via shortcut, 合法 boot 前状态)
- [ ] B.4 `ip/cpu/plugins/mmu.h` `RiscvMMUPlugin` ctor 改写:
  ```cpp
  RiscvMMUPlugin(SvMode sv_mode, ..., std::uint64_t satp_value = 0, mem)
    : MMUPlugin(sv_mode, ..., mem), satp_value_(satp_value) {
    set_satp_value(satp_value);
    set_satp_ppn(cf::cpu::detail::extract_satp_ppn(sv_mode, satp_value));
  }
  ```
  (关键: 让基类 satp_ppn_ 真正更新, 不再 Bare shortcut 永真)
- [ ] B.5 在 `tests/soc/page_table_helpers.h` 新增 `plant_identity_page_table(PicolibcHostMemory& mem, std::uint64_t pte_base, std::uint64_t vaddr)`:
  - 单条 4MB superpage leaf PTE 写入 (pte_base + VPN1*4)
  - VPN1 = `(vaddr >> 22) & 0x3FF`
  - PPN = `(vaddr >> 12) & 0x3FFFFF`
  - PTE = `(PPN << 10) | 0xDF` (V|R|W|X|U|A|D, 22-bit PPN in bits [31:10], flags in bits [9:0])
  - pte_base 约束: 4KB-aligned, 不与 ELF .text VMA 重叠 (建议 window_base + 60*1024)
- [ ] B.6 `cmake --build build --target chipforge_tests -j$(nproc)` → 期望 5/5 SECTION PASS (GREEN)

## Phase C — Refactor: 整理 helper + 注释

- [ ] C.1 验证 helper 命名一致 (`make_satp_value` / `extract_satp_ppn`)
- [ ] C.2 `cpu_factory.h` 加注释解释 RISC-V Spec §4.3.1 satp CSR layout (Sv32 MODE=bit31, Sv39/48 MODE=[63:60])
- [ ] C.3 重跑 unit test 仍 PASS (refactor 不破坏)

## Phase D — E2E: 翻转 test_cpu_l1_mmu_demo.cpp config + 负向测试

- [x] D.1 `tests/soc/test_cpu_l1_mmu_demo.cpp` config 翻转尝试 **(部分: helper 在位, enable_mmu 保持 false)**
- [x] D.1a 试运行 e2e: 暴露 CPU pipeline 在 vaddr=0 PTW fault (异常码 12) 后陷入 hazard 重试循环
- [x] D.1b **回滚** `cfg.enable_mmu = false` workaround (保持 v0.10.1 行为, 6/6 PASS 不退化)
- [x] D.1c 更新 header comment 说明: 本 change 落地 infrastructure (helper + ctor + plant), 但 e2e flip 需 CPU pipeline 配合
- [ ] D.2 **推迟** 真 sv32 e2e 翻转 (出本 change scope) — 跟踪独立 follow-up `cpu-pipeline-mmufault-handler`
- [ ] D.3 负向测试 (防 Bare shortcut 假阳性) — **推迟** (依赖 D.2)

## Phase E — Regression: 不退化其他 4 family

- [ ] E.1 [cpu] ≥117/117 PASS (含新增 CpuFactory_MMUCfg_PassesSatpValue TEST_CASE)
- [ ] E.2 [cpu-integration] ≥81/81 PASS
- [ ] E.3 [mmu] ≥53/53 PASS (注意: Bare shortcut 行为不变, 不退化)
- [ ] E.4 [riscv-tests] ≥40/40 PASS (走 enable_mmu=false 路径, 不动)
- [ ] E.5 `verify_plugin_decision.sh` + `check_plugin_portability.sh` 不退化 (D4 + ADR-040 v2.0)

## Phase F — Archive

- [ ] F.1 全部 AC 完成 → `openspec validate cpu-factory-satp-mapping`
- [ ] F.2 写 commit message 含 "RCA: cpu-factory:390 satp_value hardcode"
- [ ] F.3 `openspec archive cpu-factory-satp-mapping` (归档时序: 独立 archive, 不依赖 mfc-cpu-pipeline-multi-cycle-fsm)
- [ ] F.4 更新 CHANGELOG.md v0.10.2 entry: cpu_factory satp mapping fix, 真 sv32 translation e2e, 解除 deep-rca workaround
- [ ] F.5 更新 AGENTS.md §已知测试状态: [cpu-l1-mmu-demo] 真 MMU demo (enable_mmu=true), 6/6 PASS
- [ ] F.6 更新 debug-cpu-l1-mmu-demo-deep-rca archived spec.md §Purpose "superseded by cpu-factory-satp-mapping"

## 依赖关系

```
debug-cpu-l1-mmu-demo-deep-rca (v0.10.1 archive ✅)
        ↓
cpu-factory-satp-mapping (本 change) → archive v0.10.2
        ↓
mfc-cpu-pipeline-multi-cycle-fsm (P3#8) — 无新依赖, 但 PoC-2 CSR+MMU 集成测试有 ground-truth baseline
```

## 估时

- Phase A: 0.5 天 (写 5 SECTION unit test)
- Phase B: 1-1.5 天 (5 个文件改动: cpu_factory.h detail helper + 改动 + CPUConfig + mmu.h ctor + helper)
- Phase C: 0.5 天 (refactor + 注释)
- Phase D: 1-1.5 天 (config flip + PTE plant 调试 + 负向测试)
- Phase E: 0.5 天 (5 family 回归)
- Phase F: 0.5 天 (archive + 文档)

**总估时**: 2-3 天 (主要风险在 Phase D 的 PTE helper + 负向测试设计)

## Known Limitations (出本 change scope)

- `ip/cpu/plugins/mmu.cpp:37` 的 `csr_write_satp` mask (48-bit) 对 Sv32 错: 运行时 CSR write satp 路径仍 broken。**本 change 只走 ctor 路径** (demo ELF 不写 satp), 所以不触发。但应跟踪独立 follow-up `mmu-csr-write-satp-sv32-mask-fix`
- RiscvMMUPlugin ctor 改动在 `ip/cpu/plugins/mmu.h` 内 (派生 adapter), 算"CPU adapter"非"MMU 算法"。`ip/mmu/lib/` + `ip/mmu/tlm/` lib/ 域仍不动
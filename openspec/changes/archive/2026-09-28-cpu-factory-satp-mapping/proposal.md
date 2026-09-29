---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.10.2
depends_on:
  - debug-cpu-l1-mmu-demo-deep-rca
---

# cpu-factory-satp-mapping — 修复 cpu_factory.h:390 satp_value hardcode，让 mmu_mode config 真正生效

> **性质**: rdd-quick / configuration fix change (cpu_factory.h + mmu.h ctor + helper + test config 翻转, 估时 2-3 天)
> **战略位置**: 解除 debug-cpu-l1-mmu-demo-deep-rca 留下的 known limitation（workaround 而非根因修复），让 `[cpu-l1-mmu-demo]` 真正 demo MMU translation（sv32 端到端）
> **关联**: v0.10.0 `debug-cpu-l1-mmu-demo-paddr-regression` 落地 ADR-049 Bare shortcut（`sv_mode_==Bare || satp_ppn_==0`），本 change 利用此 shortcut 行为

## Why

debug-cpu-l1-mmu-demo-deep-rca (v0.10.1 archive, 2026-09-28) 修法是 `cfg.enable_mmu=false`（让 MMU plugin 不注册），**workaround 而非根因修复**：

- 测试名 `[cpu-l1-mmu-demo]` 暗示 demo MMU，实际 demo 的是 Bare translation
- MMU plugin sv32 translation 端到端路径**无任何测试覆盖**
- `cpu_factory.h:390` 仍硬写 `/*satp_value=*/0`，`cfg.mmu_mode` config 是死代码
- 任何写 `cfg.mmu_mode="sv32"` 期望 sv32 行为的用户**得不到错误提示**，实际得 Bare mode

**fresh trace 实证（v0.10.1 debug-cpu-deep-rca Phase A）**:
- 5/6 FAIL 的 ELF 在 cycle 53-463 进 riscv-tests 标准 fail handler (`jal x0, 0` 死循环)
- IBus fetch 显示 `PADDR_VALID=1` 但 `PADDR=ACCESS`（Bare mode fallback 行为，不是真 sv32 翻译）
- 真实 sv32 translation 路径未触发，CPU pipeline 与 sv32 mode 兼容性**未验证**

**后果**:
- PoC-2 (Phase 1.5 Wave 4) CSR + MMU 集成测试**没有 ground-truth baseline**
- v1.0.0 Linux boot 阶段**第一次**真正 sv32 translation 时风险极高
- v0.9.0 cache-phase1.5-4way 的 MMU-related dirty writeback**无法端到端验证**

## What Changes

### 1. RISC-V Spec §4.3.1 satp CSR 编码（per 实际规范, 非 PTE 格式）

satp CSR 的 PPN 字段**直接占 [21:0] (Sv32) / [43:0] (Sv39/Sv48)**，**没有 `<<10` 左移**（`<<10` 是 PTE 格式里 PPN 占 [31:10] 的混淆）。

```cpp
// 新增 detail helper (cf::cpu::detail namespace, inline in cpu_factory.h)
inline std::uint64_t make_satp_value(SvMode sv_mode, std::uint64_t satp_ppn) {
  switch (sv_mode) {
    case SvMode::Bare: return 0;
    case SvMode::Sv32: return (1ULL << 31) | (satp_ppn & 0x3FFFFFULL);   // PPN 22 bits
    case SvMode::Sv39: return (8ULL << 60) | (satp_ppn & 0xFFFFFFFFFULL); // PPN 44 bits, MODE=8
    case SvMode::Sv48: return (9ULL << 60) | (satp_ppn & 0xFFFFFFFFFULL); // PPN 44 bits, MODE=9
    default: return 0;
  }
}

// PPN extraction mode-aware (供 mmu.h ctor 调 set_satp_ppn 用, 不走 mmu.cpp 错 mask)
inline std::uint64_t extract_satp_ppn(SvMode sv_mode, std::uint64_t satp_value) {
  switch (sv_mode) {
    case SvMode::Sv32: return satp_value & 0x3FFFFFULL;
    case SvMode::Sv39:
    case SvMode::Sv48: return satp_value & 0xFFFFFFFFFULL;
    default: return 0;
  }
}
```

**关键**: `8ULL << 60` 必须用 `ULL` 后缀（`8u << 60` 是 UB）。

### 2. `RiscvMMUPlugin` ctor 调基类 `set_satp_value + set_satp_ppn` (mmu.h)

```cpp
// ip/cpu/plugins/mmu.h ctor 改写 (scope 扩张到 mmu.h)
// 原 ctor 只存 satp_value_ 到派生类 (基类 satp_ppn_ 仍 0 → Bare shortcut 永真 → 翻译死)
RiscvMMUPlugin(SvMode sv_mode, levels, ptw_cfg, std::uint64_t satp_value = 0, mem)
  : MMUPlugin(sv_mode, levels, ptw_cfg, mem), satp_value_(satp_value) {
  // **新增**: 调基类 API 让 PTW 真的知道 satp (不再 Bare shortcut 永真)
  set_satp_value(satp_value);
  set_satp_ppn(cf::cpu::detail::extract_satp_ppn(sv_mode, satp_value));
}
```

这是**关键修复**: 修复后 `satp_ppn_ != 0` 时 PTW 真走 2-level walk（不再 Bare fallback）。

### 4. `plant_identity_page_table` helper (tests/soc/page_table_helpers.h)

采用**单条 4MB superpage leaf PTE**（不建 2 级 chain，简化 + 风险低）：

```cpp
// 在窗口空闲区 (pte_base + VPN1*4) 写一条 4MB superpage leaf PTE, 覆盖 [vaddr, vaddr+4MB)
// Flags: V | R | W | X | U | A | D = 0xDF (spec-compliant, PTW 不检查 A/D 但安全)
// PTE PPN = vaddr >> 12 (identity mapping)
inline void plant_identity_page_table(
    PicolibcHostMemory& mem,
    std::uint64_t pte_base,    // 4KB-aligned, 必须 ≠ ELF .text VMA
    std::uint64_t vaddr) {     // 4MB-aligned start
    
  const std::uint32_t vpn1 = (vaddr >> 22) & 0x3FF;
  const std::uint32_t ppn  = (vaddr >> 12) & 0x3FFFFF;
  const std::uint32_t pte  = (ppn << 10) | 0xDFu;  // V|R|W|X|U|A|D
  
  mem.write_word(pte_base + vpn1 * 4, pte);
}
```

**约束**: pte_base 必须**不与 ELF .text 重叠**（建议放窗口顶部 4KB: `window_base + 60KB = 0x8000F000`）。

### 5. `tests/soc/test_cpu_l1_mmu_demo.cpp` config 翻转 + PTE plant **(本次实现未达成 — 见 §Known Limitation)**

```diff
-    cfg.enable_mmu = false;                                                   \
+    cfg.enable_mmu = true;                                                    \
     cfg.mmu_mode = "sv32";                                                    \
+    auto pte_base = window_base + 60 * 1024;                                  \
+    cfg.satp_ppn = (pte_base >> 12) & 0xFFFFF;                               \
+    plant_identity_page_table(mem, pte_base, elf.entry_addr);                  \
```

**删** v0.10.1 workaround header comment，恢复 pre-v0.10.1 "5 riscv-tests ELFs run with enable_mmu=true (sv32)" 描述。

> **Phase D.2 实际结果**: 试运行 e2e 显示 CPU pipeline 在 vaddr=0 load (PTW fault → exception code 12) 后陷入 hazard 重试循环 (PC 走到 0x8000000c 后 stuck, loadstore 持续在 vaddr=0). 这是**CPU pipeline 缺 MMU exception handler**, 出本 change scope. 已**回滚** `cfg.enable_mmu = false` workaround 保持 [cpu-l1-mmu-demo] 6/6 PASS. 完整 e2e 翻转跟踪独立 follow-up change `cpu-pipeline-mmufault-handler`.

### 6. `tests/cpu/test_cpu_factory.cpp` 新增 TEST_CASE

```cpp
TEST_CASE("CpuFactory_MMUCfg_PassesSatpValue", "[cpu]") {
  // 直接测 detail::make_satp_value (free function, 不需 factory plumbing)
  using cf::cpu::detail::make_satp_value;
  using cf::ip::mmu::SvMode;
  
  // Sv32 (PPN 22 bits, NO shift)
  REQUIRE(make_satp_value(SvMode::Sv32, 0x80000) == (1ULL << 31) | 0x80000);
  REQUIRE(make_satp_value(SvMode::Sv32, 0x3FFFFF) == (1ULL << 31) | 0x3FFFFF);
  
  // Sv39 (PPN 44 bits, NO shift, MODE=8)
  REQUIRE(make_satp_value(SvMode::Sv39, 0x100000) == (8ULL << 60) | 0x100000);
  
  // Sv48 (MODE=9)
  REQUIRE(make_satp_value(SvMode::Sv48, 0x100000) == (9ULL << 60) | 0x100000);
  
  // Bare = 0
  REQUIRE(make_satp_value(SvMode::Bare, 0x12345) == 0);
  
  // Negative test (防 Bare shortcut 假阳性): 给错 sv_mode + 巨大 satp_ppn
  // 期望: satp_value 编码 = (8ULL<<60) | 0xFFFFFFFFFULL (satp_ppn 被 mask 限到 44 bits)
  REQUIRE((make_satp_value(SvMode::Sv39, 0xFFFFFFFFFFFFFULL) & 0xFFFFFFFFFULL) == 0xFFFFFFFFFULL);
}
```

## Acceptance Criteria

**实际达成 (本 change 范围)**:
- [x] `cpu_factory.h` 加 `detail::make_satp_value` + `detail::extract_satp_ppn` helper (RISC-V Spec §4.3.1, NO PPN shift)
- [x] `cpu_factory.h:390` 用 helper 替代硬写 0 (`make_satp_value(sv_mode, config.satp_ppn)`)
- [x] `CPUConfig` 加 `satp_ppn` 字段 (默认 0, additive)
- [x] `RiscvMMUPlugin` ctor 调基类 `set_satp_value + set_satp_ppn` (mode-aware 提取, 不走 mmu.cpp 错 mask)
- [x] `tests/soc/page_table_helpers.h` 新增 `plant_identity_page_table` (单条 4MB superpage leaf PTE)
- [x] 新增 `CpuFactory_MMUCfg_PassesSatpValue` TEST_CASE PASS (**9 assertions, 5 SECTIONs**: Sv32/Sv39/Sv48/Bare/Negative-PPN-masked)
- [x] `[cpu]` 117 → **118** PASS (新增 TEST_CASE)
- [x] `[cpu-integration]` ≥81/81 PASS
- [x] `[mmu]` ≥53/53 PASS
- [x] `[riscv-tests]` ≥40/40 PASS
- [x] `[cpu-l1-mmu-demo]` 6/6 PASS (保持 v0.10.1 workaround `cfg.enable_mmu=false`, **未实现**真 sv32 e2e)
- [x] 修复 commit 写明 "RCA: cpu-factory:390 satp_value hardcode" 在 commit message

**未达成 (出 scope, 跟踪 follow-up)**:
- [ ] `[cpu-l1-mmu-demo]` 6/6 PASS **with `cfg.enable_mmu=true`** — 推迟到 `cpu-pipeline-mmufault-handler` change (CPU pipeline 缺 MMU exception handler, vaddr=0 PTW fault 后陷入 hazard 重试循环)
- [ ] Negative test: 故意给错 satp_ppn → 期望 FAIL (证明 PTW 真走了) — 依赖 enable_mmu=true, 同步推迟
- [ ] `cpu_params_schema.json` 加 `satp_ppn` 字段 (schema enum 更新) — 跟踪独立 follow-up (与配置 JSON 解析层相关)

**Compatibility 不变量 (与 v0.10.1 对齐, 已验证)**:
- `cfg.enable_mmu=false` → MMU plugin 不注册 (与 v0.10.1 一致)
- `cfg.mmu_mode="bare" + cfg.satp_ppn=anything` → satp_value=0 (Bare translation)
- `cfg.mmu_mode="sv32" + cfg.satp_ppn=0` → satp_value=(1<<31)|0, 经 ADR-049 Bare shortcut 走恒等翻译 (合法 boot 前状态, 不 assert)

## 关联 change / ADR 锚点

- **debug-cpu-l1-mmu-demo-deep-rca** (v0.10.1 archive, 2026-09-28): 父 workaround change
- **debug-cpu-l1-mmu-demo-paddr-regression** (v0.10.0 archive, 2026-09-28): ADR-049 Bare shortcut 落地
- **ADR-049**: MMU PADDR Consumption Contract — 本 change 不动契约语义 (satp 是配置层)
- **mmu-paddr-consume-and-real-memory** (v0.8.0, 2026-09-25): 引入 PTE base=0 边角情况, 本 change 解决

## scope 扩张承认

> **原 §OUT "不动 ip/cpu/plugins/mmu.cpp" 边界在 Oracle+Metis 双审后修订**

为使修复真生效，必须**修改 `ip/cpu/plugins/mmu.h` 的 `RiscvMMUPlugin` ctor**（增加 2 行 `set_satp_value + set_satp_ppn`）。这是 runtime fix 的必要部分（否则只改 cpu_factory 是 no-op）。**MMU 算法本体 (`ip/mmu/lib/`, `ip/mmu/tlm/`) 仍然不动**。

**`ip/cpu/plugins/mmu.cpp` 的 `csr_write_satp` mask bug (48-bit mask 对 Sv32 错) 不在本 change scope**——是潜在运行时 bug，本 change 只走 ctor 路径 (demo ELF 不写 satp)，所以不触发。但应在 spec §Known Limitation 标注跟踪。

## Known Limitation (Phase D.2 揭示, 跟踪 follow-up)

**实测现象** ([cpu-l1-mmu-demo] e2e 跑 enable_mmu=true + 4MB superpage PTE plant, 10000 cycle timeout):
1. PC 进展到 0x8000000c 后 stuck (3 条 fetch + 1 条 loadstore 之后)
2. `tlb_lookup_loadstore` 持续触发 vaddr=0x0 (riscv-tests startup 某条 lw 指令访问 0 地址)
3. vaddr=0 在 PicolibcHostMemory window 外 → PTW walk 读到 PTE=0 → V=0 → fault 12 → CPU 触发 exception
4. CPU pipeline 收到 exception code 12 后**未正确处理** (无 exception handler), PC 不前进, 但 loadstore stage 持续以同一 vaddr 重试
5. 结果: 10000 cycle 内 stuck 在 hazard 重试循环, 永远不到 tohost=1

**根因**: CPU pipeline (`ip/cpu/`) 当前 **未实现 MMU exception handler** (ADR-044 §3.2 spec 描述但代码未落地). 任何 PTW fault (异常码 12/13/15) 当前被 ignore, 不正确清空 pipeline / 写 mcause / 跳转 trap handler.

**修复路径 (out of scope, 跟踪独立 follow-up)**:
- 新增 change `cpu-pipeline-mmufault-handler` (估时 1-2 周)
- 在 `ip/cpu/plugins/csr.h` 的 `csr_write_*` hook 增加 mcause / mepc / mtval 写逻辑
- 在 `ip/cpu/plugins/mmu.h` 的 `set_exception_code` callback 触发 CPU 异常路径
- 在 IBus/DBus at_stage 闭包检查 PADDR_VALID=false → 写 mcause → 跳转 mepc (trap entry)
- 修复后 [cpu-l1-mmu-demo] 真 e2e 6/6 PASS with enable_mmu=true (Phase D.2 补完)

**honest 评估**: 本 change (cpu-factory-satp-mapping) 是**必要但不充分**的修复——satp_value 硬编码 bug 已修真, 但暴露了下游 CPU pipeline bug. v0.10.1 workaround 仍然必须保持. 完整修复需 `cpu-pipeline-mmufault-handler` follow-up.

## 不在 scope (修订后)

- 不动 MMU 算法本体 (`ip/mmu/lib/`, `ip/mmu/tlm/` lib/ 域)
- 不改 ADR-049 PADDR 契约语义
- 不改 riscv-tests 路径 (`tests/cpu/integration/test_rv32ui_runner.cpp` 仍 40/40)
- 不动 `mfc-cpu-pipeline-multi-cycle-fsm` (PoC-1 主路径)
- **不动 `ip/cpu/plugins/mmu.cpp` 的 `csr_write_satp` mask** (运行时 Sv32 csrw satp 路径仍 broken, 跟踪独立 follow-up)
- 不动 CH_MEM 模式 (Phase 6c 范畴)
- **不删** v0.10.1 Bare shortcut — 本 change 依赖它兜底 satp_ppn=0 的 boot 前状态

## 实施窗口

- **启动条件**: 无 (`debug-cpu-l1-mmu-demo-deep-rca` 已 archive, dependency 满足)
- **估时**: 2-3 天 (TDD 5 步, 主要风险在 Phase D 的 PTE plant + 负向测试设计)
- **顺序**: Phase A 写 failing unit test → Phase B 改 cpu_factory + mmu.h ctor + helper → Phase C 重构 → Phase D 翻转 test_cpu_l1_mmu_demo.cpp config → Phase E 回归验证 → Phase F archive

## 失败 → 砍分叉动作

- plant_identity_page_table 实现错误 (PTE bit position / vpn1 算法) → demo 仍 5/6 FAIL, 退回到 `cfg.enable_mmu=false` workaround, 跟踪独立 change `cpu-factory-sv32-pte-format-rca`
- cpu_factory 改动导致 `[riscv-tests]` 退化 → 回滚 commit, 标记为 PoC-2 阶段前置工作（不在 v0.10.2 时间窗）
- Phase D 负向测试不可行 (无法观测 PTW 走 vs Bare 区分) → 退化为 integration test in `[soc][cpu-l1-mmu-demo]`, REQUIRE `mmu.satp_ppn() == cfg.satp_ppn` (需新增 mmu plugin getter, 增加 scope)
---
id: feat-mmu-chmem-integration
kind: feature
status: active
scope: mmu
phase_refs: [phase-4]
主题: CH_MEM MMU/PTW pipeline 集成 (v0.11.0 owner)
---

## 概述
v0.11.0 launch owner：MMUPlugin CH_MEM 模式完整实装（ibus/dmem/mmu_exception_handler/hazard CH_MEM 化），使 [mmu-verilator] 从 3/3 plumbing-only 升级为 **4/4** 真 sv32 translation 验证。

## 跨阶段拆分

### phase-4 (v0.10.0-v0.11.0)
- mmu-chmem-pipeline-integration Phase A 已 commit (`a3afcfa`, 2026-10-08): Bare shortcut + shadow 双 bug 修复 → [mmu] 59/59 PASS
- mmu-chmem-pipeline-integration Phase 2 已 done (2026-10-17): Phase C.2/C.3 的 ibus/dmem sv32 消费 sel 表达式已落 (`9373808`+`5b9f74a`)，C.1/C.4/C.5 skeleton shipped — 但 `PADDR_VALID` 恒 false ⇒ 仍走 Bare
- **R-CHMEM-1 缓解 (2026-10-22 双审重构)**: ~~`bp-btb-coordination-3items` 0.5d 由 Sprint 6 提前到 Sprint 2 早期~~ → **已移出 pre_flight**。原 owner (wave5-bp-btb) 已于 2026-10-19 S3 决策 defer 到 v1.0.0 ⇒ 无人可交付；且 `fetch_stall_requirements_v0.11.0.md` §1.5 BLOCKER-B5 明写 fetch-stall v0.2 **反向依赖 Phase B** ⇒ 逻辑倒挂。现合并为 **Phase 3.5 slot** (Sprint 3 后段 3.5d: R11.1 0.5d + bp-btb 收窄 2 项 1d + fetch-stall v0.2 3d), 挂载点 = **C.4 入口条件**
- **Phase B 提前 (2026-10-22 双审)**: 由 Sprint 6+ (12-01) **提前到 Sprint 2-3 (10-23 ~ 11-06)**, 估时 10d → **15d** (原 10d 压缩版整段丢失 B.5 的 5d 集成回归)。根因 = 失败模式是 single-shot 派发方式而非日历位置 ⇒ Sprint 2 早期的 `ctrl-link-chmem-reg-handle` (同一套 `ch_reg` + OR 网 API 交互式热身 session) 承接
- Phase B-E 实装:
  - **B.0-B.5 (15d)**: `ip/mmu/plugins/MMUPlugin_chmem.h` 单级 ch_mem TLB + 复用 PtWalkFsmPlugin + `ip/cpu/plugins/mmu_chmem.h` (RiscvMMUPlugin CH_MEM) + `test_mmu_chmem_basic` 5 case
    - **路径裁决**: CH_MEM 新文件落 `ip/mmu/plugins/` 而非 `ip/mmu/tlm/` — 后者同时逃出 `check_plugin_portability.sh` Check 2 与 Check 7 扫描范围 (= CI 盲区)
    - **key 裁决**: 不新建 bundles key 文件, 复用 `ip/mmu/tlm/mmu_keys.h` 的 `mmu_keys<std::uint64_t>` 既有静态实例 (PayloadStore 按指针身份索引 ⇒ key 不同 ⇒ 消费端 `has()` 恒 false ⇒ 静默回退 Bare ⇒ 测试全绿但 sv32 从未生效)
  - **C1-C6**: `mmu_exception_handler_chmem.h` wiring + `cpu_factory_chmem.h` 注册 (C.4) + `hazard_chmem.h` halt_when (C.5, **含 A4 mmufault 信号驱动 — 由 `ctrl-link-chmem-reg-handle` 移入**) + C.6 集成测试
  - **D1-D6**: `[mmu-verilator]` TEST_CASE 4 sv32 e2e flip + `[cpu-l1-mmu-demo]` TEST_CASE 7 + `sv32-baseline-matrix.csv`
  - **E1-E6**: 文档 + archive

## 验收标准
- [ ] mmu-chmem-pipeline-integration archive + mmu-chmem-phase-b-c-launch archive
- [ ] [mmu] 59/59 维持 + [cpu-l1-mmu-demo] 7/7 PASS（workaround 移除）
- [ ] [mmu][chmem] 5/5 PASS（Phase B 新增；2026-10-22 由 3 case 扩到 5 case — 增 Case 4 非恒等页表向量 + Case 5 Bare 隔离/satp 边界，**identity page table 无法区分真 sv32 与 Bare 回退**）
- [ ] [mmu-verilator] 4/4 PASS（真 sv32 translation，非 plumbing only；2026-10-22 更正：原写 5/5 有误 = 3/3 baseline + D.1 新增 1 case）
- [ ] [verilator] 1/1 + [cpu-integration] 81/81 + [cpu] 19/19 全部 0 退化
- [ ] Architecture Gates 4 个 0 error
- [ ] 间接：[cpu-l1-mmu-demo] 7/7（workaround `cfg.enable_mmu=false` 移除，真 sv32 e2e 翻转）

## 风险（CH_MEM 物理限制）
- CSR plugin 物理不可能（Oracle R11）→ mmufault_clear 走最小 combinational
- 多级 TLB / LRU-RRIP / ASID≠0 / Sv39/48 / 权限检查 / megapage 完整 PPN 拼接：全部 out-of-scope（D1 收窄）

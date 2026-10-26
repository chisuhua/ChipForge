---
id: phase-5
kind: phase
status: deferred
primary_scope: soc-cpu
co_scopes: [ip-mmu, ip-cache]
phase_refs: []
主题: v1.0.0+ Wave 6 产品化
---

# Phase 5: v1.0.0+ Wave 6 产品化

**目标**: S/U mode + BTB + AMO + FreeRTOS + CoreMark ≥2.3 + Linux-on-FPGA

**状态**: ⏸ deferred (wave6 启动期, 依赖 phase-4 archive)

## 完成条件
- [ ] S/U mode 支持 (P3#11)
- [ ] BTB 分支预测 (P3#13) — ★ **v2.3 范围已重排（planner 2026-10-26，A5 决策）**：① **本期 MUST 含增量 1**（`is_unconditional` + `jal` 建项）② GShare（★ **移植**，TLM 参考已有）③ RAS（★ **新增设计**，条件性）⇒ ★ 判据 = **净收益率 ≥ 84.5%**（当前 CH_MEM Bimodal-only 实测 **56.67%** ⇒ 未达标 ⇒ GShare 大概率必须进本期）⇒ ★ 详见 `feat-bp-btb-gshare` 的 phase-5 段 + `openspec/changes/chmem-bp-btb-gshare` 的 **Task 7 / AC-27 / AC-28 / AC-29**
- [ ] AMO/LR/SC (P3#12)
- [ ] FreeRTOS demo (P3#14)
- [ ] CoreMark ≥ 2.3
- [ ] v1.2.0: 4-way + PMP + Debug + Linux-sim
- [ ] v1.3.0: Linux-on-FPGA + RV32F + chip-selector
- [ ] **B1 承载力补齐**: Dhrystone 在 `cpu_verilator_sim` 跑通 + 新基线 CSV · CH_MEM 侧 LSU 全宽度 + LD/SD · MMU PTW walk FSM · MMU exception handler CH_MEM 注册 · BP CH_MEM 版 — 见 `objective-ip-cpu-tlm-backend-retirement` Gate 2
- [x] **B2 语义裁决**: ADR-088 §语义裁决表 5 子系统逐条表态（M 扩展 cycle 裁决明确）+ DMIPS 判据改绑 Verilator 后端并重测基线 ✅ **已完成**（2026-10-24，ADR-088 **v1.2 Accepted**：§3 五行正式表态 + §7 ADR-080 范围重定义 + §5.2 BP 专项前置 + §5.3 性能门禁承接；台账已按 §3 重标。★**DMIPS 基线已改绑并重测**（交付 ①：CH_MEM 侧 **1.24467**，**未达 1.4** ⇒ 依 **P-6**，恢复条件 = ② BP 落地，不重校准判据））
- [ ] **B3 TLM 删除**: CppTLM 链接归零 · TLM-only 业务文件归零 · `tools/cpu_sim` + `src/cf_plugin/bridge/` 移除 · 全量 ctest 4 entries PASS — Gate 3
- [ ] **XLEN 护栏形态切换**: `grep -rn "kXlenBits = 32" ip/cpu/` = 0 + CH_MEM 侧 CI grep 门禁上线（ADR-087 D3/D4，v1.1 修订）

## 关联
- OpenSpec: `wave6-linux-and-productization` initiative
- 版本节点: v1.0.0 → v1.3.0
- 前置: phase-4 archive
- 相关 objective: `objective-soc-cpu-v100-launch` · `objective-ip-cpu-tlm-backend-retirement` (B1/B2/B3 段)
- 相关 feature: `feat-tlm-backend-retirement` (B1/B2/B3 段) · `feat-rv64-isa-declaration-integrity` (D3/D4 段)
- ADRs: ADR-088 (TLM 退役, 待起草) + ADR-087 v1.1 (XLEN 护栏形态)

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
- [ ] ★★ **阶段 B — 覆盖平价台账重新定性**（2026-10-26 ADR-089 D5 / ADR-088 v1.4 §10.2）：台账从「迁移追踪器」⇒「**CH_MEM 覆盖边界规格**」⇒ 头部 partial **292 → 282** 修正 + not-covered 项理由补全 + **冻结标记**（只增不改）⇒ `check_coverage_parity_ledger.sh` 仍 PASS ⇒ ★ **parity 语义完全交给差分门禁**
- [ ] ★★ **阶段 C 的 5 条硬前置**（ADR-088 §10.3 + §5.2 判据 6/7）—— ★ **缺一不得删任何 TLM-only case**：
  - [ ] ① **阶段 A 差分门禁已上线且全绿**（ADR-089 V-1~V-6）⇒ 见 phase-4 阶段 A
  - [ ] ② **CI 具备 build 能力**（判据 4 的可判定化 = 差分门禁**在 CI 中被执行**且结果被回读）⇒ CI 侧分层 corpus 快子集 + 全量夜间
  - [ ] ③ **TLM 参考实现的语义已抽成规格**（样板：`ip/cpu/docs/bp-btb-gshare-design-brief.md` §3.1 / §7 D10 的 GShare 语义）
  - [ ] ④ **台账已冻结为覆盖边界规格**（= 上面的阶段 B）
  - [ ] ⑤ ★★ **门禁体系自身改版**（★ **治理层自噬**）：`check_plugin_portability.sh` 的 Check 2/5/6/7/13 建立在双模前提上 ⇒ 删 TLM 模态后 **Check 2/7 会真空 PASS** ⇒ MUST 改写为单模语义 + **空集合 MUST 显式报告「该检查已退役」** + `check_tlm_freeze.sh` 退役 + ADR-040 升 v4.0 + ADR-083/AGENTS.md 双模表述清除 ⇒ ★ **验收 = 变异自检**（注入旧 Check 5 能抓的 `if(ch_bool)` ⇒ 新检查 MUST 仍 RED）
- [ ] **XLEN 护栏形态切换**: `grep -rn "kXlenBits = 32" ip/cpu/` = 0 + CH_MEM 侧 CI grep 门禁上线（ADR-087 D3/D4，v1.1 修订）

## 关联
- OpenSpec: `wave6-linux-and-productization` initiative
- 版本节点: v1.0.0 → v1.3.0
- 前置: phase-4 archive
- 相关 objective: `objective-soc-cpu-v100-launch` · `objective-ip-cpu-tlm-backend-retirement` (B1/B2/B3 段) · ★ `objective-backend-parity-and-single-model`（阶段 B/C）
- 相关 feature: `feat-tlm-backend-retirement` (B1/B2/B3 段) · `feat-rv64-isa-declaration-integrity` (D3/D4 段) · ★ `feat-backend-parity-gate`（阶段 A，阶段 C 的前置）
- ADRs: ADR-088 v1.4 (TLM 退役, **§10 目标态与三段路径**) + ADR-087 v1.1 (XLEN 护栏形态) + ★ ADR-089 (双后端差分 parity 门禁)

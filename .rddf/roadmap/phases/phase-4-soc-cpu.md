---
id: phase-4
kind: phase
status: active
primary_scope: soc-cpu
co_scopes: [ip-mmu]
phase_refs: []
主题: v0.10.x-v0.11.0 Wave 5 ISA 覆盖 + BP
---

# Phase 4: v0.10.x-v0.11.0 Wave 5 ISA 覆盖 + BP

**目标**: mfc FSM 收官 + mmu-chmem CH_MEM 集成 + rv32um 覆盖 + 分支预测 (BTB/GShare)

**状态**: 🔄 active (2026-10-08)

## 完成条件
- [ ] `mfc-cpu-pipeline-multi-cycle-fsm` Phase H archive (50/60 → 60/60)
- [ ] `mmu-chmem-pipeline-integration` archive (Phase A 已 commit a3afcfa, B-E 待)
- [ ] [riscv-tests] rv32um 8/8 PASS (当前 0/8, 真 radix-2 实装)
- [ ] [cpu-l1-mmu-demo] 7/7 PASS (workaround 移除)
- [ ] [mmu-verilator] 5/5 PASS (真 sv32 translation)
- [ ] DMIPS/MHz ≥ 1.4 (硬门禁)
- [ ] **PoC-1**: MUL/DIV 1c/33c rv32um 100% · **PoC-2**: RV32C ≥95% · **PoC-3**: ICache ≥90%
- [ ] ADR-082 negotiate capability 集成（mfc Phase G）
- [ ] **PoC-6**: CoreMark/MHz ≥ 1.9 (BP 完成) — **defer → v1.0.0** (决策 2026-10-19, 见 `objective-soc-cpu-v0110-launch.md §11`)
- [ ] **前置2 PoC**（`chmem-bp-btb-gshare-poc`）★ **机制判定已完成（2026-10-26）**：机制成立（Δcycles 4 档全改善 + 校验和逐位不变 + ΔFLUSH ≡ Δcycles）⇒ Dhrystone cycles **−7.4%** / DMIPS **1.34472**（★ 仍 < 1.4 硬门禁 ⇒ **绝对值门禁未恢复** ⇒ 净收益率 **56.67%** vs 需 **84.51%**）⇒ ★ **待归档**（★ 归档 = 本 change 的开工前置之一，见 `chmem-bp-btb-gshare` 的定位段）
- [x] **TLM 冻结门禁上线**（B0）: CI 可执行，TLM-only 文件新增 feature 函数即 fail — 见 `objective-ip-cpu-tlm-backend-retirement` Gate 1 ✅ **已完成**（2026-10-24，change `chmem-tlm-backend-freeze` 归档；`tools/check_tlm_freeze.sh` 已在 `architecture-gates.yml` warn 级 step，violations=0 / candidates=31 / base=a9f0d97432）
- [x] **TLM 死代码清理**（B0）: 4 文件 / 201 行删除，双 binary 0 退化 ✅ **已完成**（2026-10-24，同上 change 归档；`ip/cpu/plugins/{exception,fpu}.h` + `ip/cpu/arch/riscv/fpu.h` + `ip/cache/plugins/cache_keys.h` 已删）
- [x] **ADR-088 Proposed**（B0）: TLM 仿真后端退役与 CH_MEM 单后端收敛（冻结条款 + 语义裁决表 + 覆盖平价台账 + 删除判据） ✅ **已完成并升 Accepted v1.2**（2026-10-24；B0 立冻结契约 + B2 语义裁决完成 + §5.3 性能门禁承接裁决）
- [x] **覆盖平价台账建立**（B0）: TLM 独占 case 三态标注 100% 覆盖，`[framework]` 94 条标「移植」 ✅ **已完成**（2026-10-24，change `chmem-tlm-coverage-parity-ledger` 归档；台账 `docs/architecture/tlm-coverage-parity-ledger.md` = **463** 条 / 46 族 · covered 154 · partial 292 · not-covered 27；机检 `tools/check_coverage_parity_ledger.sh` PASS。★**数字更正**：草案 451 → 实测 463（ADR-088 §1.3 已同步））
- [x] **ISA 声明纠偏落地**（ADR-087 D1/D2）: `isa` 默认 `rv32im` + `xlen` 字段 + schema 三态 — 见 `feat-rv64-isa-declaration-integrity` ✅ **已完成**（2026-10-24，change `rv64-isa-declaration-correction` 归档，AC 10/10；`[cpu]` 129 → **137**；新增 `tools/check_xlen_hygiene.sh` 软门禁）
- [ ] ★★ **阶段 A — 双后端差分 parity 门禁**（2026-10-26 architect 裁决 ADR-089 + ADR-088 v1.4 §10；路线图见 `objective-backend-parity-and-single-model`）:
  - [ ] ★ **G-A0** parity 架构 + 路线图经 Oracle + Metis 双审通过（★ 2026-10-26 用户裁决：**不立即开工**）
  - [ ] **A1** `parity-gate-spec` —— ★ **零代码**（★ **含 `tools/**`**）：落 5 观测量采集接口（★ ③ = DUT 自算签名 / ⑤ = 逐拍 PC 流定义）+ 确定性判据 + 例外白名单 **5 字段** + fail-closed（每观测量一例）+ 语料分层 + 报告格式 + ★ **S-编号化** ⇒ ★ **G-A1 门禁：未落盘 ⇒ 不开 A2a**
  - [ ] **A2a** `backend-parity-observability` —— ★ **Simulator 侧通用 ELF runner**（CLI 对齐 `cpu_verilator_sim`）+ 两侧 5 观测量采集 + **样例 JSON 证据**（★ ③ 内存签名 = DUT 自算 mailbox）
  - [ ] **A2b** `backend-parity-harness` —— ★ 确定性判据先行 + 比对器（②③⑤ bit-exact / ⑤ 逐拍 / ④ 总拍数）+ fail-closed（≥5 例）+ 三态退出码 + 变异自检（≥2 例）+ 白名单校验脚本 + ★ **S-编号覆盖矩阵**
  - [ ] **A3** `backend-parity-corpus` —— ★ **两侧交集 47 ELF**（★ `rv32ui-p-simple` 补进 Simulator 侧）+ branchbench 4 档（★ Simulator 侧 runner 依赖 A2a）+ **单一清单文件** ⇒ ★ **首跑产出「既存后端分歧清单」** ⇒ 分类（上游对齐 / 有意例外 / 真 bug）⇒ ★ **G-A3 门禁：清单归零或全部显式登记**
  - [ ] **A4** `backend-parity-ci` —— 门禁接线 + ★ **CI build 能力**（= ADR-088 §5.2 判据 4/7 落点）+ 分层 corpus + ★ **单元级快车道** + ★ **Verilog 构建产物按 config 缓存**
  - [ ] ★ **本阶段独立于 TLM 退役** ⇒ 可与 BP 增量 3 并行（★ A3 跑批与 BP 量收益**合并执行**，★ 前置 = A2a 完成；★ 若 BP 跑批先于 A2a ⇒ 放弃合并、分开跑）

## 关联
- OpenSpec: `wave5-isa-coverage-and-bp` initiative
- 版本节点: v0.10.x → v0.11.0
- ADRs: ADR-046 (FSM 豁免) + ADR-082 (negotiate) + ADR-070 (RVC) + ADR-087 (ISA 声明真实性, Proposed) + ★ **ADR-089**（双后端差分 parity 门禁, ✅ Accepted 2026-10-26）+ ★ **ADR-088 v1.4 §10**（目标态与三段路径）
- Active changes: `mmu-chmem-pipeline-integration` (6/166), `mfc-cpu-pipeline-multi-cycle-fsm` (50/60), `mfc-extract-fsm-h` (0/20)
- 相关 objective: `objective-soc-cpu-v0110-launch` · `objective-ip-cpu-tlm-backend-retirement` (B0 冻结段) · ★ `objective-backend-parity-and-single-model`（阶段 A）
- 相关 feature: `feat-rv64-isa-declaration-integrity` · `feat-tlm-backend-retirement` (B0 段) · ★ `feat-backend-parity-gate`（阶段 A）

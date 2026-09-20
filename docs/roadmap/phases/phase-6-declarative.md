# Phase 6：完整 PipeBuilder 框架 + RTL 生成

> **Status**: v2.0.3 (2026-09-20 更新 — Phase 6c 完整收官, Phase 6d 拆分为独立 phase doc)
> **里程碑**: M6 - 完整 Plugin 框架 + RTL 协同验证
> **Depends on**: Phase 0/1/2/3/4/5 完成, 且至少 2-3 个 Plugin-style IP 稳定运行
> **决策依据**: `.omo/drafts/decision-plugin-framework-2026-06-08.md`
> **目标版本**: ChipForge 0.3.x

**目标**：将 Phase 0 提供的"Plugin 最小脚手架"升级为完整 PipeBuilder 框架, 并集成 RTL 生成能力。

> **本文件为 Phase 6 框架级总览**。**Phase 6c 单独归档于 [`openspec/changes/archive/2026-09-17-plugin-elaboration-substrate/`](../../../openspec/changes/archive/2026-09-17-plugin-elaboration-substrate/) + [`openspec/changes/archive/2026-09-20-fix-5stage-mux-segv-elaboration/`](../../../openspec/changes/archive/2026-09-20-fix-5stage-mux-segv-elaboration/)**, 详见本文件 §2。**Phase 6d 已拆分为独立 phase doc**: [`phase-6d-rtl-verification.md`](phase-6d-rtl-verification.md)。

---

## 1. Phase 6 范围拆分 (v2.0.3, 2026-09-20 修订)

> 原 v2.0.2 拆分 6a/6b/6c, v2.0.3 进一步将 6c 拆分为**已完成 (6c)** + **新增独立 Phase 6d**。

| 子阶段 | 内容 | 工时 | 状态 | 文档 |
|--------|------|------|------|------|
| **Phase 6a** | PipeBuilder::auto_schedule 调度算法 + JSON `pipeline_stages` 解析 | 4-6 周 | ⏸ Not Started | (待 Phase 6d 端到端验证后) |
| **Phase 6b** | CompareDriver + ScoreBoard + TLM↔RTL 对比基线 | 4-6 周 | ⏸ Not Started | (依赖 Phase 6a) |
| **Phase 6c** | **完整 RTL 生成 (VerilogCodeGen 集成) + 通用 TLM↔RTL 桥接** | **4-8 周** | **✅ Completed (2026-09-20)** | **OpenSpec archive** |
| **Phase 6d** | **5-stage Pipeline CH_MEM 端到端 + Verilator + MMU/PTW FSM** | **6-8 周** | **⏸ Not Started** | **[phase-6d-rtl-verification.md](phase-6d-rtl-verification.md)** |

**Phase 6c v0.3.0 + v0.3.1 收官**: 详见 `CHANGELOG.md` §v0.3.0 + §v0.3.1 + `docs/roadmap/roadmap-status.md` §2 "Phase 6 - 完整 PipeBuilder 框架 (拆分 6a/6b/6c/6d)" 节。

---

## 2. Phase 6c 收官总结

### 2.1 范围 vs 实际交付

| 范围条目 (来自 §1 Phase 6c) | 实际交付 | 状态 | 推迟到 |
|------|------|------|------|
| `enum class ImplMode {TLM_ONLY, RTL_ONLY, COMPARE, SHADOW}` 完整实现 | `CF_PLUGIN_USE_CH_MEM` 编译开关 + CH_MEM 双模 | ⚠️ 部分 (TLM/RTL only; COMPARE 推迟 Phase 6b) | Phase 6b |
| `BundleMapper` 完整模板 | `array_store<T, N>` 抽象替代 + `_chmem.h` 双文件分离 | ⚠️ 部分 (TLM 模式不需要, CH_MEM 直接用 ch 类型) | Phase 6d+ |
| `CompareDriver` + `ScoreBoard` 基类 | 未实现 | ❌ 推迟 | Phase 6b |
| JSON `pipeline_stages` 完整解析 | 未实现 | ❌ 推迟 | Phase 6a |
| **完整 RTL AST 生成 (VerilogCodeGen 集成)** | **✅ 完整实现**: `PipeBuilder::to_verilog()` + `ch::toVerilog()` + 6 个 Verilog 真实生成 | ✅ **完成** | — |
| 通用 TLM↔RTL 桥接 (扩展 `HybridCacheWrapper` 模式) | ⚠️ L1CachePlugin 保持 TLM 模式兼容 (Phase 1+), 桥接层 `L1CacheTLMBridge` 仍 TLM-only | ⚠️ 部分 | Phase 6d (依赖 Phase 6b CompareDriver) |
| 性能基准测试套件 | 未实现 | ❌ 推迟 | Phase 6d+ |
| DSE 集成 (参数扫描 + Pareto 分析) | 未实现 | ❌ 推迟 | Phase 6d+ |

### 2.2 关键产出 (11 commits, ~2815 行代码)

> Oracle 2026-09-20 修正: 实际 M1-M5 = 9 commits (CHANGELOG v0.3.0 清单) + M6 + M6.1 = 2 commits, **合计 11**。原文档 "10 commits" 与 roadmap-status 头部 "8 核心 + 2 修复 = 10" 均为错误, 本节以 CHANGELOG 为权威源统一为 11。

| 路径 | 行数 | 内容 |
|------|------|------|
| `include/cf/plugin/uint_t.h` | 112 | 双模 `uint_t<N>` + `bool_t` + `CF_PLUGIN_USE_CH_MEM` 开关 |
| `include/cf/plugin/payload.h` | 251 | 双模 `PayloadStore` (TLM POD / CH_MEM ch 代理) + fail-fast get-miss (v0.3.1 M6) |
| `include/cf/plugin/pipe_builder.h` | 481 | 双模 `PipeBuilder` + `elaborate(ctx)` + Prereq-1 4 API (`to_verilog`/`create_simulator`) + stage plumbing + per-stage stall wiring |
| `include/cf/plugin/storage.h` | 158 | 双模 `array_store<T, N>` (TLM 单缓冲 / CH_MEM 双缓冲 commit swap) |
| `include/cf/plugin/ctrl_link.h` | ~175 | 双模 `CtrlLink` (TLM `std::function<bool()>` / CH_MEM `ch_bool` + OR-merge) |
| `tools/check_plugin_portability.sh` v2.0 | ~280 | 8 项检查 (Tier-1 ch 渗透翻转 + if(ch_bool) + Check 8 PayloadStore fail-fast) |
| `tests/framework/test_cppHDL_hello_poc.cpp` | 230 | W0 PoC (6 PoC: ch_device/toVerilog/Simulator/ch_bool/ch_uint/full_chain) |
| `tests/framework/test_elaborate_connector.cpp` | 376 | M1 elaboration PoC |
| `tests/framework/test_elaborate_pipeline2.cpp` | 375 | M2 2-stage pipeline stall matrix + TLM↔CH_MEM byte-equal + Verilog `always_ff @(posedge)` 真实生成 |
| `tests/framework/test_payload_store_miss_throws.cpp` | 65 | v0.3.1 M6 PayloadStore fail-fast fail-safe 测试 |
| `ip/cpu/plugins/reg_file_chmem.h` | 218 | M3/W5 RegFile CH_MEM (32 独立 `ch_reg` + x0 select 屏蔽, singleton ctx_aware fix) |
| `ip/cpu/arch/riscv/int_alu_chmem.h` | ~140 | M3/W5 IntAlu CH_MEM (11-op select 树) |
| `ip/cpu/plugins/branch_chmem.h` | ~280 | M4/W7 BranchPlugin CH_MEM (6-op B-type select 树) |
| `ip/cpu/plugins/hazard_chmem.h` | ~330 | M4/W7 HazardPlugin CH_MEM (RAW 检测 6 条件 OR 合并) |
| `ip/cpu/cpu_factory_chmem.h` | ~250 | M4/W8 CpuFactoryChmem 5-stage 集成 (4 plugin 注册 + stage linking connectors + v0.3.1 EARLY 预填充) |
| `ip/cpu/cpu_factory.h` | 改造 | `build_cpu()` CH_MEM 分支 + EARLY payload pre-population (v0.3.1 M6) |
| `tests/cpu/test_cpu_rtl_regfile_alu.cpp` | ~340 | M3/W6 PoC RegFile+ALU 单元级 sim + toVerilog |
| `docs/audit/cppHDL-maturity-audit.md` | 227 | W0 审计报告 |
| `docs/architecture/adr/ADR-046-multi-cycle-fsm-exemption.md` | 249 | 新增 ADR-046 多周期 FSM 豁免 |
| `docs/architecture/adr/ADR-040-tlm-hdl-portability-constraints.md` v2.0 | 509 | ADR-040 v2.0 重订: `CH_MEM 是新正道` (翻转 v1.0 `ch 渗透禁令`) |
| `openspec/changes/archive/2026-09-17-plugin-elaboration-substrate/` | — | OpenSpec change archive (proposal + design + tasks + specs) |
| `openspec/changes/archive/2026-09-20-fix-5stage-mux-segv-elaboration/` | — | M6 SEGV 修复 OpenSpec change archive |
| `CHANGELOG.md` (v0.3.0 + v0.3.1) | ~90 | Phase 6c 收官 + M6 修复 |

### 2.3 验证门 (实测 2026-09-20)

- ✅ `verify_adr.sh`: 31 PASS / 10 EXPECTED_MISSING / 0 FAILED / 0 STALE
- ✅ `verify_plugin_decision.sh`: D4 + ADR-040 检查 3+4/3 全部 PASS
- ✅ `check_plugin_portability.sh` v2.0: **8/8 PASS**
- ✅ TLM baseline `[framework]`: 89/89 PASS, 275 assertions 零回归
- ✅ CH_MEM `[cpphdl]`: 6/6 PASS, 15 assertions
- ✅ CH_MEM `[chmem]`: 9/9 PASS, 69 assertions
- ✅ `pipeline2_stall_matrix`: 16/16 PASS (Verilog `always_ff @(posedge)` 真实生成)
- ✅ M3 PoC: `m3_poc_regfile_elaborate` 7/7 PASS + `m3_poc_alu_elaborate` 6/6 PASS
- ✅ `m4_poc_5stage_simulator_tick`: 12/12 PASS (v0.3.1 M6 SEGV 修复后)
- ✅ 6 个 Verilog 文件真实生成 (`hello.v`/`pipeline2_device.v`/`regfile.v`/`alu.v`/`elaborate_connector.v`)

### 2.4 推迟到 Phase 6d+ 的剩余项 (透明记录)

| 推迟项 | 原因 | 影响 | 计划 |
|--------|------|------|------|
| riscv-tests RV32I 5 指令 (add/addi/auipc/jal/beq) `tohost=1` 端到端 CppHDL sim | build env 缺 riscv64-unknown-elf-gcc 工具链 | Phase 6c 仅 PoC + 单元级 | **Phase 6d** 启动前 prerequisites |
| Verilator / Yosys / iverilog 综合验证集成 | 未安装 | Phase 6c 仅 CppHDL Simulator 跑通 | **Phase 6d** 集成 |
| M3/W6 byte-equal 完整版 (#95) | cell put/get CH_MEM 不稳定 | 单元级已通过 (M3 PoC) | **Phase 6d** 修复 |
| 5-stage DecoderPlugin + BranchPlugin + HazardPlugin 完整 CH_MEM | M4 仅骨架 + PoC | W7-8 仅骨架 + PoC | **Phase 6d** 完整 |
| MMU/PTW 多周期 FSM | 依赖 `ch_state_machine` 简化实现 + Verilator | 已锁定 (ADR-046) | **Phase 6d** Verilator 后端 |
| L1Cache refill FSM | 同 MMU/PTW | 同上 | **Phase 6d** |
| Harness 迁移 (pb.run() → CppHDL sim runner / Verilator) | W9 仅文档 | `tools/cpu_sim/main.cpp` TLM 入口等待 M5 | **Phase 6d** 实施 |

---

## 3. Phase 6d 拆分理由 (2026-09-20 v2.0.3)

**原 Phase 6c 时间盒 4-8 周 (日历) 实际消耗约 1 周 (2026-09-16 → 2026-09-20, 5 个日历日)**, 主要 scope 风险来自:
1. **M3 启动前的 6 个 Prereq** (Oracle/Metis 审查发现的关键缺陷): `pipe_builder.h` 4 API 缺失 + `int_alu_chmem.h` 语法错误 + `reg_file_chmem.h` dual static 架构缺陷 + 范围降级
2. **M4 HazardPlugin 不可推迟** (Metis A4 关键风险): 无 hazard → riscv-tests 静默失败
3. **M5 范围扩展** (Oracle/Metis 反馈): riscv-tests 端到端从 M4 推到 M5

> Oracle 2026-09-20 修正: 原 §3 称 "实际消耗 ~9 周 (W0-W9)" 是 plan-week 标签与日历时间混淆, 应明确 W0-W9 为计划里程碑编号而非日历周。

**Phase 6d 单独拆分理由**:
1. **业务代码完整化**: Phase 6c PoC (单元级 + toVerilog) → Phase 6d 端到端 (riscv-tests 5 指令 tohost=1)
2. **工具链首次集成**: riscv64-unknown-elf-gcc + Verilator 是 Phase 6d 5-stage 仿真与综合验证的硬前置
3. **多周期 FSM 实装**: MMU PTW + L1Cache refill 依赖 `ch_state_machine` + Verilator (ADR-046)
4. **Harness 迁移**: `pb.run()` → `pb.elaborate()` + `ch::Simulator::tick()` 是业务层最后切换

**Phase 6d 估时依据**: Phase 6c 框架能力已 PoC 实证 (零 framework-level 重做), 6d 工作主要是业务代码完整化 + 工具链首次集成 + 多周期 FSM, 估时 10-12 周合理。

---

## 4. Phase 6 后续阶段 (Phase 6d → 6a → 6b 顺序)

| 阶段 | 目标 | 依赖 | 估时 |
|------|------|------|------|
| **Phase 6d** | 5-stage Pipeline CH_MEM 端到端 + Verilator + MMU/PTW FSM | Phase 6c ✅; 工具链 + ADR-037 v2.0 | 6-8 周 |
| **Phase 6a** | PipeBuilder::auto_schedule 调度算法 + JSON `pipeline_stages` | Phase 6d 端到端验证后 | 4-6 周 |
| **Phase 6b** | CompareDriver + ScoreBoard + TLM↔RTL 对比基线 | Phase 6a + Phase 6d | 4-6 周 |

**Phase 6 全阶段完成 =** `cf::plugin` 从"每周期仿真器"→ "elaboration DSL"→ "调度自动化"→ "TLM↔RTL 对比验证" 的完整 SpinalHDL 风格 RTL 生成方法学闭环。

---

## 5. 与 Phase 0 脚手架的关系

Phase 0 提供的 5 个接口 (`PluginBase` / `Payload<T>` / `PipeNode` / `PipeBuilder` / `CtrlLink`) 在 Phase 6c 期间**保持稳定** (仅扩展, 不破坏现有业务代码):
- ✅ Phase 0 业务代码 (L1CachePlugin 等) 在 TLM 模式下零回归 (89/89 PASS)
- ✅ CH_MEM 模式作为新增编译开关 (`-DCF_PLUGIN_USE_CH_MEM`), 业务代码 `_chmem.h` 双文件分离, 复用 `at_stage` API
- ✅ D4 + ADR-040 静态检查在两种模式下均 PASS

---

## 6. 决策可追溯

Phase 6 的所有设计决策来源于:
- **决策记录**: `.omo/drafts/decision-plugin-framework-2026-06-08.md` (D1, D5, D11)
- **v2.0.1 §12.2 Phase 1a/1b/1c 拆分方案**: 本阶段重新映射为 6a/6b/6c
- **v2.0.3 (2026-09-20) Phase 6d 拆分**: 6c 拆分为"框架能力" + 6d "端到端验证"
- **ADR-040 v2.0** (2026-09-17): CH_MEM 是新正道
- **ADR-046** (2026-09-16): 多周期协议引擎豁免 D4
- **DECISION-2026-06-13-02 F1.A** (Phase 1.4): L1CachePlugin 设计方法学基线

任何对 Phase 6 范围/接口的修改, **必须**同步更新决策记录。

---

## 7. 详细阶段文档链接

| 阶段 | 文档 | OpenSpec change |
|------|------|------|
| Phase 0 | [phase-0-plugin-scaffolding.md](phase-0-plugin-scaffolding.md) | (无, Phase 0 前 OpenSpec 工作流) |
| **Phase 6c** | **本文件 §2** | [`archive/2026-09-17-plugin-elaboration-substrate/`](../../../openspec/changes/archive/2026-09-17-plugin-elaboration-substrate/) + [`archive/2026-09-20-fix-5stage-mux-segv-elaboration/`](../../../openspec/changes/archive/2026-09-20-fix-5stage-mux-segv-elaboration/) |
| **Phase 6d** | **[phase-6d-rtl-verification.md](phase-6d-rtl-verification.md)** | `openspec/changes/phase-6d-rtl-verification/` (待创建) |
| Phase 6a/6b | (待 Phase 6d 完成后细化) | (待创建) |

---

## 8. 修订历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-06-08 | 初版: Phase 6 范围拆分 6a/6b/6c (来自 `declarative-hybrid-framework.md §12.2.4`) |
| 2.0.1 | 2026-06-08 | v2.0 阶段文档创建 |
| 2.0.2 | 2026-06-13 | 范围细化, Phase 1.4 方法学复盘链接 |
| **2.0.3** | **2026-09-20** | **Phase 6c 完整收官条目 (§2) + Phase 6d 独立拆分 (§3) + Phase 6d 文档链接 (§7)** |

---

*本文件为 Phase 6 框架级总览。Phase 6c 已完成 (详见 §2 + OpenSpec archive), Phase 6d 待启动 (详见独立 phase doc [phase-6d-rtl-verification.md](phase-6d-rtl-verification.md))。*

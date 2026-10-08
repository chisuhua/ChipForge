# ADR-037：Plugin 作为设计范式（不是工具）

| 字段 | 值 |
|------|-----|
| 状态 | ✅ v2.0 Accepted (Phase 6c M5 落地, 2026-09-17) — v1.0 由 Phase 0 草案升级 |
| 来源 | v1.0: `.omo/drafts/decision-plugin-framework-2026-06-08.md` (gitignored 草稿, D1-D9 决策)<br>v2.0: Phase 6c W0 审计 + Oracle 重构报告 (`openspec/changes/plugin-elaboration-substrate/proposal.md`) |
| 关联 ADR | ADR-025 (Plugin 基类无 tick)、ADR-040 v2.0 (TLM→HDL 移植性, CH_MEM 是新正道)、ADR-046 (多周期 FSM 豁免) |
| 详细全文 | [`docs/architecture/adr.md` §ADR-037](../adr.md) |

---

## 摘要

### v1.0 决策 (2026-06-08)
- **D1**: 路线图前插入 Phase 0 = Plugin 最小**脚手架** (2-3 周)
- **D2**: Phase 1 Hello World = L1CachePlugin (真实 Plugin, 不是占位)
- **D4**: 业务逻辑强制采用 **Plugin-style** 设计 (无 `tick()`、无状态机、Bundle 字段用 `uint_t<N>`)
- **D5**: Phase 6 = 完整 PipeBuilder 框架 + RTL 生成 (12-20 周)
- **D6-D9**: 4 项命名冲突解决方案

### v2.0 重大变更 (Phase 6c, 2026-09-16)
- **D4 兑现**: Plugin-style 不再只是 TLM 仿真范式, 而是 **elaboration 期间发射 lnode DAG 的硬件描述范式**。`pb.elaborate()` 执行一次 at_stage 闭包, 发射硬件 DAG; `ch::toVerilog(ctx)` 输出 Verilog; `ch::Simulator::tick()` / Verilator 做 cycle 仿真。
- **D5 拆解**: Phase 6 拆为 Phase 6a (PipeBuilder::auto_schedule) + 6b (CompareDriver + ScoreBoard) + 6c (✅ M1-M5 RTL 兑现, 2026-09-20 archive) + 6d (5-stage Pipeline CH_MEM + Verilator + MMU/PTW FSM, 11-13 周)。
- **D10**: `cf::plugin` 在 `-DCF_PLUGIN_USE_CH_MEM` 下走 elaboration 正道; TLM 模式 deprecated (CH_MEM 双模共存, 零回归)。
- **D11**: 多周期协议引擎豁免 D4 "无状态机" 禁令 (ADR-046), 但必须使用 `chlib::ch_state_machine` DSL。
- **D12** (Phase 6c M5, 2026-09-17): D4 elaboration 语义经 M1-M5 **8 commit** 验证 (`25e2672 / 9a03bb2 / 918e577 / baa504b / 3e8ada2 / a38a1e4 / 19d4f5d / edad878 / b68996a`)。CH_MEM 双模零回归, PoC #1-#4 全部 PASS。

### 影响 (v2.0 增量)
- **README 承诺兑现**: "CppTLM + CppHDL-based" (v1.0 仅 TLM; v2.0 TLM + HDL 双层)
- **核心洞察**: VexRiscv 能编译 Verilog 不因为 Scala 翻译, 是因为"执行即布线"; cf::plugin 不发明翻译器, 让 lambda 体直接操作 ch 类型, 执行即发射 lnode DAG
- **TLM 路径 deprecation**: `pb.run()` 标 `[[deprecated]]`; CH_MEM 是新正道 (见 ADR-040 v2.0)
- **ADR-040**: TLM→HDL 移植性约束 → v2.0 Accepted (CH_MEM 是新正道; Tier-1 Check 5 新增)
- **ADR-046 新增**: 多周期 FSM 豁免 D4

### 不可逆性
D4 (Plugin-style 强制) 不可逆 —— v2.0 进一步强化: Plugin-style 是 elaboration 期间发射硬件 DAG 的唯一方式 (不能再退回 tick())。

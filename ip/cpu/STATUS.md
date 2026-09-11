# STATUS: ACTIVE (TLM 实现中, Phase 1.5, 2026-07-01)

This IP has **significant TLM implementation**. 11 Plugin 套件在 CpuFactory 中真实注册（EARLY×2 + NORMAL×7 + LATE×2）。

<!-- Conforms to: docs/templates/IP_STATUS_TEMPLATE.md -->

## Existing Assets
- Plugin 套件: IBusPlugin, BranchPredictorPlugin, RiscvDecodePlugin, HazardPlugin, RiscvIntAluPlugin, RiscvMulPlugin, RiscvBranchPlugin, RiscvLsuPlugin, RiscvCsrPlugin, DBusPlugin, RegFilePlugin
- CpuFactory: 集中 PluginOrder + build_cpu() 入口
- 配置: `configs/cpu_params_schema.json`（支持 3/5/7/10-stage pipeline）
- 测试: 19 文件, 114 test cases (`tests/cpu/` + `tests/cpu/integration/`)
- 文档: `README.md` + `docs/multi_isa_architecture.md` + `docs/implementation-plan/`

## Implementation Roadmap
- 下一里程碑: `mmu-tlb-ptw-impl` — RiscvMMUPlugin 集成 + satp/sfence.vma hook
- DSE 工具链: `tools/cpu_sim/` — 576-config sweep 完成（M5-DSE）
- RISC-V 仿真: 5 个集成测试因工具链配置预存失败（与代码无关）
- 状态: 🟡 TLM 实现中（Phase 1.5 范围: MMU 集成 + ISA 覆盖扩展）

## 已知限制
- **MMU Plugin 未注册**: RiscvMMUPlugin 仅类型别名，未在 CpuFactory 注册（ADR-042 推迟，mmu-tlb-ptw-impl 恢复）
- **FPU Plugin**: Phase 5+ 推迟
- **Exception Plugin**: Phase 5+ 推迟
- **RetirePlugin**: Phase 5+ 推迟（当前 ipc=0.0 是已知 Phase 1.5 限制）
- **RISC-V 仿真测试失败**: 5 个预存失败（riscv64 assembler path + tohost 字符串配置）
- **docs/ 目录未对齐标准**: `ip/cpu/` 是最老的 IP，`docs/` 子结构未遵守 `ip/README.md` 标准

## 参考
- 架构: `docs/multi_isa_architecture.md` v2.0
- ADR: ADR-042（Plugin 推迟决策）
- 实施: `ip/cpu/docs/implementation-plan/README.md`

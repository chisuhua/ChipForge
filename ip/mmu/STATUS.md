# STATUS: PARTIAL (骨架阶段, mmu-ip-skeleton, 2026-06-29)

This IP has **skeleton-level partial implementation**. 目录骨架、Plugin 入口、Bundle、Config schema 落地，TLB/PTW 算法推迟到下一个 change。

<!-- Conforms to: docs/templates/IP_STATUS_TEMPLATE.md (PARTIAL variant) -->

## Existing Assets
- 目录骨架: `lib/` (纯 C++ 算法) + `tlm/` (Plugin 框架集成) + `rtl/` (Phase 5+ 沿用) + `configs/` (JSON Schema) + `docs/` (架构/配置/集成) + `policies/` (替换策略) + `test/` (预留, 测试在 `tests/mmu/`)
- 文档: `README.md` + `STATUS.md` + `docs/{README,architecture,configuration,integration}.md`
- LOC: ~1500 (含 stub, 不含 .cpp/.h 业务实现细节)

## Implementation Roadmap
- 下一里程碑: **mmu-tlb-ptw-impl** change —— TLB lookup/insert 算法 + PageTableWalker Sv32/Sv39/Sv48 解码 + MMUPlugin at_stage 闭包实装 + CtrlLink halt_when PTW stall
- 依赖: `cf::plugin` Phase 0 (5/5 P0 组件稳定) + `ip/cache/` Plugin-style 先例 + `ip/cpu/plugins/` ISA 无关 Plugin 套件
- 状态: 🟡 骨架阶段 (mmu-ip-skeleton, 2026-06-29 落地, 同 mmu-tlb-ptw-impl+ 实施)
- 子模块: TLB 模板化 + MultiLevelTLB 编排器 + PageTableWalker 接口 + 4 种替换策略 + Plugin 集成

## 已知限制
- **TLB/PTW 算法 stub**：骨架阶段提供接口，算法实现推迟到 `mmu-tlb-ptw-impl`
- **PTW Sv32/Sv39/Sv48 解码 stub**：仅接口稳定，具体解码推迟
- **CtrlLink halt_when PTW stall 未实装**：仅声明接口
- **CPU 集成未实装**：`RiscvMMUPlugin` 仅声明类型别名，satp/sfence.vma hook 推迟
- **split_id 拓扑未实装**：`topology` 字段保留枚举值 `split_id`，骨架阶段仅 `unified` 工作
- **cpptlm MMUTLMBridge 未实装**：与 `L1CacheTLMBridge` 同构但推迟到 TLB/PTW 算法稳定后
- **rtl/ 目录空**：Phase 5+ CppHDL 转换时填充

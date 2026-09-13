# STATUS: INTEGRATED (mmu-cache-integration + L1Cache VIPT + SoC 全链, 2026-09-13)

mmu-tlb-ptw-impl (archived 2026-09-12) + mmu-cache-integration (current) 完成:
- TLB lookup/insert/invalidate (4 替换策略: None/FIFO/LRU/RRIP)
- PTW Sv39 三级 walk (含 reserved encoding + V=0 fault)
- MultiLevelTLB coherence (并行查 + 返回最深 hit + shadow fill + cross-level invalidate)
- TLBFactory 11 组特化 + VIPT safety check
- MMUPlugin at_stage 闭包实装 (tlb_lookup_ifetch + tlb_lookup_loadstore + ptw_l0/l1/l2)
- RISC-V RiscvMMUPlugin 实装 (satp CSR + SFENCE.VMA hook + exception 12/13/15 + csr_write_satp)
- mmu_keys.h 14 Key (10 原有 + 4 新增 MMU_VADDR/EXCEPTION_CODE/SATP_PPN/SATP_MODE)
- invalidate_vaddr_any_asid 新 API (RISC-V SFENCE.VMA rs1!=0, rs2=0 跨 ASID 失效)
- MMUTLMBridge + MMUTLMBridgeAdapter (cpptlm ModuleFactory 兼容 + ch_stream 4 字段窄桥)
- bundles/tlb_bundles_tlm.hh (TlbReqBundle/TlbRespBundle)
- L1CachePlugin VIPT 索引 (consume pl::MMU_VADDR + pl::PADDR, PIPT fallback 兼容 baseline)
- cache_keys.h (VIPT Key 集合 + vipt_fallback Knob)
- soc/mmu_minimal.json (tg → mmu → l1 → mem 全链集成)
- 39 个 mmu tests + 24 个 cache tests + 14 个 soc tests (306/306 PASS)

下一里程碑: `cache-dse-sweep` (DSE 48-case 配置扫描) + `cache-phase1.5-4way` (4-way VIPT 升 Phase 1.5)

<!-- Conforms to: docs/templates/IP_STATUS_TEMPLATE.md (PARTIAL variant) -->

## Existing Assets
- 目录骨架: `lib/` (纯 C++ 算法) + `tlm/` (Plugin 框架集成) + `rtl/` (Phase 5+ 沿用) + `configs/` (JSON Schema) + `docs/` (架构/配置/集成) + `policies/` (替换策略) + `test/` (预留, 测试在 `tests/mmu/`)
- 文档: `README.md` + `STATUS.md` + `docs/{README,architecture,configuration,integration}.md`
- LOC: ~1500 (含 stub, 不含 .cpp/.h 业务实现细节)

## Implementation Roadmap
- 下一里程碑: **mmu-tlb-ptw-impl** change —— TLB lookup/insert 算法 + PageTableWalker Sv32/Sv39/Sv48 解码 + MMUPlugin at_stage 闭包实装 + CtrlLink halt_when PTW stall
- 前提依赖: ADR-044 (L1 Cache VIPT 锁定, [`ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md`](../cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md)) — MMUPlugin 输出 `pl::MMU_VADDR` 供 VIPT L1 索引
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

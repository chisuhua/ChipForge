# tests/soc/soc-cpu-l1-mmu-demo/

CPU + MMU + Memory SoC topology end-to-end test family.

## SoC topology
参见 [`soc/cpu_l1_mmu_demo.json`](../../../soc/cpu_l1_mmu_demo.json) — CPU(cf::cpu::CpuFactory) + MMU(cf::ip::mmu::MMUPlugin sv32 + RiscvMMUPlugin + PicolibcHostMemory memory_interface) + L1Cache (declared only) + Memory.

## Contents
- `test_cpu_l1_mmu_demo.cpp` — TEST_CASE 7 (最新 plan) + 6 base cases: `add`, `addi`, `auipc`, `jal`, `beq`, `cpu_l1_mmu_demo_json_structure` + `cpu_l1_mmu_demo_sv32_translation_flipped` (follow-up)
- `page_table_helpers.h` — helper: `plant_identity_page_table()` (cpu-factory-satp-mapping v0.10.2)

## Family tag
`[soc][cpu-l1-mmu-demo]`

## Upstream spec
[`openspec/specs/cpu-l1-mmu-demo-regression/spec.md`](../../../openspec/specs/cpu-l1-mmu-demo-regression/spec.md)

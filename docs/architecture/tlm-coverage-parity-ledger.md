# TLM 覆盖平价台账（TLM Coverage-Parity Ledger）

> **格式权威**: [ADR-088 §4](../architecture/adr/ADR-088-tlm-backend-retirement.md)（三态 `covered` / `partial` / `not-covered` + 必填字段 + 口径注记）—— 本文件是它的**内容**。
> **基线 commit**: `d93f9a72ff349b4418db4a8a48c0bcda565c05e8`（计数随代码演进漂移；漂移由 `tools/check_coverage_parity_ledger.sh` 检出）
> **生成 / 校验**: `tools/gen_coverage_parity_ledger.sh`（生成骨架 + 闭合校验）· `tools/check_coverage_parity_ledger.sh`（漂移 + 红线检测，CI warn 级）
> **配套 change**: `chmem-tlm-coverage-parity-ledger`（交付内容）· 格式由 `chmem-tlm-backend-freeze`（已归档）交付。
> **B2 语义裁决（2026-10-24，ADR-088 v1.1 §3）已落地到本台账**：BP 10 条 → `not-covered`（归属 B1 item 5，**非「不做」**）· M 扩展 cycle 类 8 条补注「cycle 判据已退役」· PTW 17 条补注「回退 vaddr = 已知临时降级，归 `chmem-ptw-walk`」· CSR/satp 路由 6 条补注「CH_MEM 工厂不接受 `CPUConfig`」。详见 §7。

## 1. 口径与闭合校验

| 口径 | 值 |
|---|---|
| TLM binary 总 case | **493** |
| CH_MEM binary 总 case | **130** |
| 两者共有 | **30** |
| **TLM 独占（本台账覆盖对象）** | **463** |
| CH_MEM 独有 | **100** |
| 闭合式 1 | TLM 独占 463 + 共有 30 = **493** ✅ |
| 闭合式 2 | CH_MEM 独有 100 + 共有 30 = **130** ✅ |

**口径纪律**（ADR-088 §4 注记）：case 级覆盖一律用**两个 binary 的 `--list-tests` 差集**；
**禁止**用「按族 case 数求和」—— 一个 case 可带多个 tag，族求和天然 > 差集数（基线实测族计数之和 = 678 ≠ 463）。

**三态分布**：covered **154** · partial **292** · not-covered **27**（上限 135 = 30%）✅ —— 27 = B0 基线 17 + **B2 裁决降级的 BP 10 条**（见 §7；原写 17 系 B2 重标时漏改，已纠正）

## 2. 族级判定（46 族）

> 条目级三态由**其所属族的判定**继承（`framework` 族强制 covered —— AC 红线；多族冲突时取最保守）。
> 族内 case 数之和 > 463 属正常（同 case 可属多族）。

| # | 族 | 涉及 case 数 | 族判定 | CH_MEM 侧锚点 | 差异说明 |
|---|---|---|---|---|---|
| 2.1 | `cpu` | 137 | `partial` | `tests/cpu/test_cpu_chmem_alu_semantics.cpp`（19 层 select 树真跑，6 case / 288 assertions）+ `test_cpu_5stage.cpp` + `test_cpu_rtl_regfile_alu.cpp` + `test_cpu_decoder_chmem.cpp` + `test_cpu_decoded_inst_migration.cpp` + `test_cpu_chmem_keys_modality.cpp` + `test_cpu_chmem_mem_read_drift.cpp` | CH_MEM 侧覆盖 ALU 语义 / decode / regfile / key 模态 / 内存读时序，但**不含** BP（10 条条目已按 B2 裁决降为 `not-covered`，见 §7）· 不含 DBus/IBus/LSU 的 TLM 实现细节 · ISA 契约**不适用**（CH_MEM 工厂不接受 `CPUConfig`，见 ADR-088 §9.1 / §7）。 |
| 2.2 | `cpu-integration` | 96 | `partial` | `tests/cpu/test_cpu_chmem_vendored_elf.cpp`（47 vendored ELF）+ `tests/cpu/test_cpu_verilator_sim.cpp`（48 ELF 严格 `tohost==1`）+ `tests/cpu/test_cpu_5stage.cpp` | CH_MEM 侧有 5-stage 与 ELF 级端到端，但 **3/7/10-stage 拓扑、dhrystone 性能载体、7-stage canonical-ordering 断言**仅在 TLM ⇒ 差异为「流水线形态覆盖不全 + 性能基线缺失」。 |
| 2.3 | `framework` | 94 | `covered` | `tests/framework/test_chmem_arith_poc.cpp`（22 条 arith-poc）+ `test_chmem_multi_cycle_fsm.cpp` + `test_ctrl_link_stateful.cpp` + `test_elaborate_connector.cpp` / `test_elaborate_pipeline2.cpp` / `test_payload_store_miss_throws.cpp` / `test_cppHDL_hello_poc.cpp`；CH_MEM binary 另有 30 条 `[framework]` 共有 case | 框架语义锚（PipeBuilder / Payload / storage / elaborate）在 CH_MEM 侧完整承接；差异仅为底层类型（`uint_t<N>` POD → `ch_uint<N>`），不影响被测语义。**本族不得批量退役**（AC 红线）。 |
| 2.4 | `mmu` | 59 | `partial` | `tests/mmu/test_mmu_chmem_basic.cpp`（7 case，TLB 命中路径）+ `test_mmu_keys_modality.cpp`（4 case / 15 assertions）+ `test_mmu_bare_plumbing_verilator.cpp`（3 case，**plumbing only**）+ `test_mmu_ptw_fsm_chmem.cpp` | CH_MEM 侧覆盖 TLB 命中 / key 模态 / Bare plumbing，但**无 PTW walk**（`MMUPluginChmem` miss ⇒ 回退 vaddr，ADR-088 §3 已知分歧）⇒ 差异为「多级页表转换语义不等价」。 |
| 2.5 | `riscv-tests` | 48 | `covered` | `tests/cpu/test_cpu_chmem_vendored_elf.cpp`（47 个 vendored ELF）+ `tests/cpu/test_cpu_verilator_sim.cpp`（48 ELF 严格 `TOHOST=1`，含 8 个 rv32um） | 指令级行为已由 CH_MEM/Verilator 侧的 ELF 级回归等价承接（含 rv32ui 40 + rv32um 8）；差异仅为 cycle 数不同构（组合式 1 拍 vs TLM radix-2 多拍）—— cycle parity 归 ADR-088 §3 的 M 扩展裁决。 |
| 2.6 | `cache` | 25 | `partial` | `tests/cache/test_l1cache_refill_fsm_chmem.cpp`（仅 refill FSM） | CH_MEM 侧只有 refill FSM，**L1CachePlugin 本体无覆盖** ⇒ 差异为「主体缺失」；本族是 B1 承载力的主要缺口。 |
| 2.7 | `soc` | 20 | `partial` | `tests/cpu/test_cpu_chmem_*` 系列（CH_MEM 侧 130 case 含 SoC 级配置校验的等价物） | SoC JSON 拓扑校验与后端无关（可共用同一测试文件），但当前**未纳入 CH_MEM target** ⇒ 差异为「可复用未复用」。 |
| 2.8 | `stall` | 19 | `not-covered` | （无）CH_MEM 为**全组合通路**（`cpu_factory_chmem.h:437-441` 无 stage 寄存器） | stall 矩阵（hazard/flush/hold）在 CH_MEM 侧**不存在对应概念**（无流水级停顿）⇒ 不可映射；并非「未测」而是「语义不适用」。 |
| 2.9 | `cpu-configs` | 13 | `partial` | `tests/cpu/test_cpu_chmem_keys_modality.cpp` + `tests/mmu/test_mmu_config_schema.cpp`（共用同一 schema） | schema 校验与后端无关（可共用），但 isa enum 收窄 + `xlen` 属性的**契约校验**在 CH_MEM 侧未接 ⇒ 差异为「配置可被接受但不被校验」。 |
| 2.10 | `MMUPlugin` | 12 | `partial` | `tests/mmu/test_mmu_chmem_basic.cpp` | 同 `[mmu]`：CH_MEM 侧 MMUPlugin 为 TLB-only（无 PTW walk）。 |
| 2.11 | `bundles` | 9 | `covered` | `tests/cpu/test_cpu_chmem_keys_modality.cpp` + `tests/framework/test_chmem_arith_poc.cpp`（共用同一 bundle 定义） | Bundle 字段语义在两条路径上共用同一份定义（`bundles/mem_bundles.h`），差异仅在底层类型（`uint_t<N>` → `ch_uint<N>`）⇒ 被测语义一致。 |
| 2.12 | `isa-xlen` | 9 | `partial` | （无对应实现）`ip/cpu/cpu_factory_chmem.h` 未接入 `validate_isa_xlen` | isa/xlen 契约（ADR-087 §D1/D2）在 CH_MEM 工厂侧**未实现** ⇒ 同一份配置在两条路径上行为不一致；须在 B1 补同款校验。 |
| 2.13 | `mul-div-fsm` | 9 | `partial` | `tests/cpu/test_cpu_chmem_alu_semantics.cpp`（8 条 M-op 的组合式实现真跑）+ `tests/cpu/test_cpu_verilator_sim.cpp`（8 个 rv32um ELF） | M 扩展**指令级行为**已覆盖，但多周期 FSM 的时序语义不可比（TLM radix-2 MUL=1c/MULH=3c/DIV=33c vs CH_MEM 组合 1 拍，差 ~286c，ADR-088 §3）⇒ 差异为「cycle 不同构」。 |
| 2.14 | `rv32um` | 8 | `covered` | `tests/cpu/test_cpu_verilator_sim.cpp` 的 8 个 rv32um ELF（严格 `tohost==1` 判据） | M 扩展指令级行为已覆盖；差异为 cycle 不同构（TLM radix-2 DIV=33c vs CH_MEM 组合 1 拍）。 |
| 2.15 | `MultiLevelTLB` | 8 | `partial` | （无对应实现）`ip/mmu/plugins/MMUPlugin_chmem.h` 用 8 项全相联 TLB，无多级结构 | 多级 TLB 结构在 CH_MEM 侧不存在 ⇒ 差异为「结构缺失」。 |
| 2.16 | `TLBUnit` | 8 | `partial` | （无对应实现） | TLB 单元级行为在 CH_MEM 侧无独立单元（并入 MMUPluginChmem 内部 TLB）⇒ 差异为「单元边界不同」。 |
| 2.17 | `PTWUnit` | 6 | `partial` | `tests/mmu/test_mmu_ptw_fsm_chmem.cpp`（PTW FSM 存在但**未注册进 CH_MEM 流水线**） | PTW 语义在 CH_MEM 侧有 FSM 实现但未接线 ⇒ 差异为「实现存在、链路缺失」。 |
| 2.18 | `TLBFactoryTest` | 6 | `partial` | （无对应实现） | TLB 工厂在 CH_MEM 侧不存在（TLB 内联于 MMUPluginChmem）⇒ 差异为「工厂抽象缺失」。 |
| 2.19 | `cpu-l1-mmu-demo` | 6 | `partial` | `tests/cpu/test_cpu_chmem_vendored_elf.cpp`（端到端等价物） | demo 级流水线在 CH_MEM 侧有 ELF 级等价物，但 3/5/7-stage 组合与 MMU 断言不全。 |
| 2.20 | `negotiate` | 5 | `partial` | `tests/framework/test_negotiate.cpp`（CH_MEM 共有 1 条） | negotiate 生命周期在 CH_MEM 侧只有 1 条共有 case，TLM 侧 5 条更细（capability 协商矩阵）⇒ 差异为「覆盖深度不足」。 |
| 2.21 | `MMUCacheIntegration` | 4 | `not-covered` | （无）CH_MEM 侧**无 L1CachePlugin 本体**（只有 refill FSM） | MMU×Cache 集成依赖 L1CachePlugin 本体，而 CH_MEM 侧该本体不存在 ⇒ 集成路径整体不可映射；须归 B1 承载力。 |
| 2.22 | `mul-div-fsm-integration` | 4 | `partial` | `tests/cpu/test_cpu_verilator_sim.cpp` | 同 `[mul-div-fsm]`：行为覆盖、时序不可比。 |
| 2.23 | `mmu-exception-handler` | 4 | `partial` | `ip/cpu/plugins/mmu_exception_handler_chmem.h`（118 行，**存在但未注册**：`cpu_factory_chmem.h:342-349` 被 `#ifndef` 包裹） | CH_MEM 侧有实现但**未接线** ⇒ 差异为「实现存在、注册缺失」（ADR-088 §3 语义裁决表第 5 项）。 |
| 2.24 | `result-paradigm` | 4 | `covered` | `tests/framework/test_pipe_builder_stall.cpp` 的 CH_MEM 对应物 + `tests/framework/test_plugin_lifecycle.cpp`（共有 30 条 `[framework]`） | Result 范式（ADR-047）语义在 CH_MEM 侧由共有 framework case 承接 ⇒ 差异仅为 API 形态。 |
| 2.25 | `MMUMinimalJson` | 4 | `partial` | （无对应 CH_MEM 测试文件） | JSON 拓扑校验与后端无关，但测试文件未纳入 CH_MEM target ⇒ 同 `[soc]`。 |
| 2.26 | `dhrystone` | 3 | `not-covered` | （无）CH_MEM 侧无 Dhrystone 载体（`tests/cpu/integration/test_dhrystone.cpp` 硬绑 `CpuFactory<uint32_t>`） | 性能硬门禁载体（DMIPS/MHz ≥1.4）100% 在 TLM；CH_MEM 组合通路无法承载该基线 ⇒ 须在 Verilator 侧重建（ADR-088 §5.2 前置 4「CI 有可回归的自动测试」的必要输入）。 |
| 2.27 | `elf-loader` | 3 | `partial` | （无 CH_MEM 对应测试文件；loader 本身为宿主侧工具） | ELF 加载逻辑与后端无关（可共用），但测试文件未纳入 CH_MEM target ⇒ 差异为「可复用未复用」。 |
| 2.28 | `tohost` | 3 | `covered` | `tests/cpu/test_cpu_verilator_sim.cpp`（严格 `TOHOST=1`）+ `tests/cpu/test_cpu_chmem_vendored_elf.cpp` | tohost 退出约定在 CH_MEM 侧以更严格判据（严格 `==1`）覆盖 ⇒ 语义等价且更严。 |
| 2.29 | `paddr-propagation` | 3 | `partial` | （无对应实现） | paddr 传播在 CH_MEM 侧未实现（CH_MEM 无 PTW ⇒ 无 paddr 产生路径）。 |
| 2.30 | `base-window` | 3 | `partial` | `tests/cpu/test_cpu_memory_model_chmem.cpp` | 同 `[base-addr]`。 |
| 2.31 | `cycle-parity` | 3 | `not-covered` | （无）CH_MEM 侧无 radix-2 FSM ⇒ 无可比的 cycle 序列 | cycle parity 断言的前提是两侧同构多周期实现；CH_MEM 组合化后**判据本身失效**（非 pass/fail）。 |
| 2.32 | `bare-shortcut` | 3 | `partial` | `tests/mmu/test_mmu_bare_plumbing_verilator.cpp` | Bare 短路判定在 CH_MEM 侧有 plumbing 级覆盖，语义级未验证。 |
| 2.33 | `shadow-bug` | 3 | `partial` | （无对应实现） | satp shadow 类缺陷的回归锚在 CH_MEM 侧缺失（无 PTW ⇒ 无 satp 写路径）。 |
| 2.34 | `ptw` | 3 | `partial` | `tests/mmu/test_mmu_ptw_fsm_chmem.cpp` | 同 `[PTWUnit]`。 |
| 2.35 | `real-memory` | 3 | `partial` | `tests/mmu/test_mmu_bare_plumbing_verilator.cpp`（plumbing only，**translation semantics NOT verified**） | 真实内存页表 walk 在 CH_MEM 侧未验证（仅 plumbing）⇒ 差异为「语义未验证」。 |
| 2.36 | `base-addr` | 2 | `partial` | `tests/cpu/test_cpu_memory_model_chmem.cpp` | 宿主内存基址语义在 CH_MEM 侧有等价内存模型，但断言细节不同。 |
| 2.37 | `cpu-sim` | 2 | `not-covered` | （无）`tools/cpu_sim` 是 TLM 专用 CLI（CH_MEM 走 `tools/verilator_runner`） | CLI 工具面本身是 TLM 专属（`pb->run()` 直仿）⇒ 随 TLM 退役整体迁移到 verilator_runner（B3），不属「覆盖缺口」。 |
| 2.38 | `write_half` | 2 | `partial` | `tests/cpu/test_dmem_chmem_sv32.cpp` | 半字写语义在 CH_MEM 侧有 dmem 等价物，断言细节不同。 |
| 2.39 | `PTW` | 2 | `partial` | `tests/mmu/test_mmu_ptw_fsm_chmem.cpp` | 同 `[PTWUnit]`：未注册 ⇒ 行为未生效。 |
| 2.40 | `performance` | 1 | `not-covered` | （无） | 性能类断言在 CH_MEM 组合通路上无意义（无流水线统计）⇒ 不可映射。 |
| 2.41 | `probe` | 1 | `partial` | （无 CH_MEM 对应物） | 探针类小用例，CH_MEM 侧无等价物。 |
| 2.42 | `multi-section` | 1 | `partial` | （同上） | 同 `[elf-loader]`。 |
| 2.43 | `write_word` | 1 | `partial` | `tests/cpu/test_dmem_chmem_sv32.cpp` | 同 `[write_half]`。 |
| 2.44 | `load_section` | 1 | `partial` | （同上） | 同 `[elf-loader]`。 |
| 2.45 | `stub-guard` | 1 | `partial` | （无 CH_MEM 对应物） | stub 守卫类断言针对 TLM stub 降级路径，CH_MEM 侧无 stub 概念。 |
| 2.46 | `api` | 1 | `partial` | `tests/framework/test_pipe_builder.cpp`（共有 30 条 framework） | API 表面在 CH_MEM 侧部分覆盖，细节断言不同。 |

## 3. 条目级台账（463 条）

> 格式：`| case | tags | 态 | 锚点 | 差异说明 |`。`covered`/`partial` 条目的锚点与差异说明**继承族级**（§2 对应行）；
> `not-covered` 条目必须给**逐条差异化理由**（≥20 字 + ADR-088 引用）—— 红线：同一理由 ≤10 次。


### 3.11 `bundles`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `mem_req_fields` | `bundles` | `covered` | 继承 §2.11 | 继承族级差异说明 |
| `mem_resp_fields` | `bundles` | `covered` | 继承 §2.11 | 继承族级差异说明 |
| `cache_req_fields` | `bundles` | `covered` | 继承 §2.11 | 继承族级差异说明 |
| `cache_resp_fields` | `bundles` | `covered` | 继承 §2.11 | 继承族级差异说明 |
| `l1cache_plugin_bundle_fields` | `bundles` | `covered` | 继承 §2.11 | 继承族级差异说明 |
| `int_bundle_fields` | `bundles` | `covered` | 继承 §2.11 | 继承族级差异说明 |
| `d4_compliance` | `bundles` | `covered` | 继承 §2.11 | 继承族级差异说明 |
| `bundle_readwrite` | `bundles` | `covered` | 继承 §2.11 | 继承族级差异说明 |
| `bundle_sizeof` | `bundles` | `covered` | 继承 §2.11 | 继承族级差异说明 |

### 3.6 `cache`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `bridge_tick_triggers_plugin_run` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `bridge_tick_forwards_4_fields` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `instantiate_all_succeeds` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `l1_instance_is_bridge_adapter` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `three_module_topology_resolved` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `start_ticks_and_cycle_advance` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `bridge_pb_run_count_after_cycles` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `module_factory_recognizes_adapter_type` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `adapter_constructs_with_bridge` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `adapter_tick_triggers_bridge_pb_run` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `miss_workflow_through_adapter` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `multiple_transactions_via_adapter` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `lookup_miss_path` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `refill_path` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `hit_after_refill` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `d4_compliance_runtime` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `MMUPluginOutputDrivesVIPTIndex` | `cache` `MMUCacheIntegration` | `not-covered` | — | CH_MEM 侧无 L1CachePlugin 本体 ⇒ MMU 输出无 VIPT 索引可驱动；须先在 B1 补 L1Cache 本体 |
| `NoMMUOutputFallsBackToPIPT` | `cache` `MMUCacheIntegration` | `not-covered` | — | PIPT 回退路径依赖 L1CachePlugin 本体（CH_MEM 缺失）⇒ 该回退语义在 CH_MEM 侧无载体 |
| `VIPTMissWhenVAddrMatchesButPAddrAbsent` | `cache` `MMUCacheIntegration` | `not-covered` | — | VIPT 命中/未命中判定需 L1CachePlugin 与 MMU 联动；CH_MEM 侧两者不同时存在（MMU 无 PTW、Cache 无本体） |
| `EndToEndTranslationThroughBridge` | `cache` `MMUCacheIntegration` | `not-covered` | — | 端到端翻译链 = MMU(PTW) × L1Cache(bridge)；CH_MEM 侧两环皆缺 ⇒ 整链不可映射 |
| `factory_create_lru` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `factory_create_none` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `factory_unknown_throws` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `lru_on_access_increments` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |
| `no_replacement_victim_returns_zero` | `cache` | `partial` | 继承 §2.6 | 继承族级差异说明 |

### 3.2 `cpu-integration`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `build_10stage_deep` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `10stage_topology_from_config` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `10stage_mul_latency_5` | `cpu-integration` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 M 扩展行：CH_MEM 组合式为规范 ⇒ cycle 判据已退役**（差异为「cycle 不同构」，非 pass/fail） |
| `EnableMMU10StageDeepPipeline` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `10stage_add_elf_end_to_end` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `build_3stage` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `EnableMMU3StageBuilds` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `3_vs_5_stages` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `tohost_3stage` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `multiple_instances` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `3stage_add_elf_end_to_end` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `build_5stage` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `EnableMMU5StageBuilds` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `EnableMMUDisabledBitIdenticalBaseline` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `host_memory_init` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `load_binary` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `tohost_mechanism` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `tohost_fail` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `5stage_add_elf_end_to_end` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `build_7stage_superscalar` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `EnableMMU7StageCommitRetire` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `7stage_topology_from_config` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `7stage_dispatch_width_2` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `7stage_add_elf_end_to_end` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `canonical_ordering_violation_detected` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `canonical_ordering_mmu_disabled_skips` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `canonical_ordering_cpu_factory_correct` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `canonical_ordering_counters_record_order` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `add_elf` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `sub_elf` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `and_elf` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `or_elf` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `sll_elf` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `srli_elf` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `dhrystone_legacy_tohost_pass` | `cpu-integration` `dhrystone` | `not-covered` | — | Dhrystone 以 TLM `pb->run()` 直仿测吞吐，CH_MEM 组合通路无同构性能计数 ⇒ 需在 Verilator 侧重建基线 |
| `dhrystone_fsm_dmips_mhz_minimum_1_4` | `cpu-integration` `dhrystone` | `not-covered` | — | DMIPS/MHz ≥1.4 硬门禁（v0.11.0 launch 判据）依赖 TLM 逐周期统计；CH_MEM/Verilator 侧尚无等价测量载体 |
| `dhrystone_fsm_dmips_mhz_metric` | `cpu-integration` `dhrystone` `performance` | `not-covered` | — | DMIPS 指标计算需逐周期 retire 计数，CH_MEM 组合通路不产生该统计 ⇒ 指标口径不可迁移 |
| `hazard_no_raw_no_stall_when_scoreboard_empty` | `cpu-integration` `stall` | `not-covered` | — | 记分板空 ⇒ 无停顿的判定依赖 TLM scoreboard 插件；CH_MEM 侧无该插件 ⇒ 断言前提不存在 |
| `hazard_raw_chain_stall_execute` | `cpu-integration` `stall` | `not-covered` | — | RAW 链停顿语义依赖流水线级间 hold；CH_MEM 为全组合通路（无 stage 寄存器）⇒ 停顿概念不存在 |
| `hazard_long_raw_chain_repeated_stall` | `cpu-integration` `stall` | `not-covered` | — | 长 RAW 链重复停顿同上：无流水级 ⇒ 无多级连续停顿可建模 |
| `hazard_war_not_stall_framework_consumes_ctrl` | `cpu-integration` `stall` | `not-covered` | — | WAR 不停顿 + 框架消费 ctrl 的语义依赖 TLM 调度模型；CH_MEM 组合通路无对应调度点 |
| `mul_div_fsm_integration_mul_legacy` | `cpu-integration` `mul-div-fsm-integration` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 M 扩展行：CH_MEM 组合式为规范 ⇒ cycle 判据已退役**（差异为「cycle 不同构」，非 pass/fail） |
| `mul_div_fsm_integration_mul_fsm` | `cpu-integration` `mul-div-fsm-integration` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 M 扩展行：CH_MEM 组合式为规范 ⇒ cycle 判据已退役**（差异为「cycle 不同构」，非 pass/fail） |
| `mul_div_fsm_integration_div_legacy` | `cpu-integration` `mul-div-fsm-integration` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 M 扩展行：CH_MEM 组合式为规范 ⇒ cycle 判据已退役**（差异为「cycle 不同构」，非 pass/fail） |
| `mul_div_fsm_integration_div_fsm_b1_fix` | `cpu-integration` `mul-div-fsm-integration` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 M 扩展行：CH_MEM 组合式为规范 ⇒ cycle 判据已退役**（差异为「cycle 不同构」，非 pass/fail） |

### 3.41 `probe`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `probe_7stage` | `probe` | `partial` | 继承 §2.41 | 继承族级差异说明 |

### 3.2 `cpu-integration`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `rv32ui-p-add` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-addi` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-and` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-andi` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-auipc` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-beq` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-bge` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-bgeu` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-blt` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-bltu` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-bne` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-jal` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-jalr` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-lb` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-lbu` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-ld_st` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-lh` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-lhu` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-lui` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-lw` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-or` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-ori` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-sb` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-sh` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-simple` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-sll` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-slli` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-slt` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-slti` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-sltiu` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-sltu` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-sra` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-srai` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-srl` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-srli` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-st_ld` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-sub` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-sw` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-xor` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32ui-p-xori` | `cpu-integration` `riscv-tests` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32um-p-mul` | `cpu-integration` `riscv-tests` `rv32um` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32um-p-mulh` | `cpu-integration` `riscv-tests` `rv32um` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32um-p-mulhsu` | `cpu-integration` `riscv-tests` `rv32um` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32um-p-mulhu` | `cpu-integration` `riscv-tests` `rv32um` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32um-p-div` | `cpu-integration` `riscv-tests` `rv32um` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32um-p-divu` | `cpu-integration` `riscv-tests` `rv32um` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32um-p-rem` | `cpu-integration` `riscv-tests` `rv32um` | `covered` | 继承 §2.2 | 继承族级差异说明 |
| `rv32um-p-remu` | `cpu-integration` `riscv-tests` `rv32um` | `covered` | 继承 §2.2 | 继承族级差异说明 |

### 3.9 `cpu-configs`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `schema isa enum narrowed to implemented set` | `cpu-configs` | `partial` | 继承 §2.9 | 继承族级差异说明 |
| `schema has xlen property` | `cpu-configs` | `partial` | 继承 §2.9 | 继承族级差异说明 |
| `schema keeps isa-conditioned mmu_mode constraint` | `cpu-configs` | `partial` | 继承 §2.9 | 继承族级差异说明 |
| `config instances declare rv32im and sv32` | `cpu-configs` | `partial` | 继承 §2.9 | 继承族级差异说明 |
| `schema_has_9_new_optional_fields` | `cpu-configs` | `partial` | 继承 §2.9 | 继承族级差异说明 |
| `schema_new_field_defaults_correct` | `cpu-configs` | `partial` | 继承 §2.9 | 继承族级差异说明 |
| `schema_width_fields_have_enum_constraint` | `cpu-configs` | `partial` | 继承 §2.9 | 继承族级差异说明 |
| `schema_renamed_latency_fields` | `cpu-configs` | `partial` | 继承 §2.9 | 继承族级差异说明 |
| `cpu_default_json_uses_renamed_fields` | `cpu-configs` | `partial` | 继承 §2.9 | 继承族级差异说明 |
| `cpu_embedded_json_uses_renamed_fields` | `cpu-configs` | `partial` | 继承 §2.9 | 继承族级差异说明 |
| `cpu_superscalar_json_uses_renamed_fields` | `cpu-configs` | `partial` | 继承 §2.9 | 继承族级差异说明 |
| `cpu_deep_pipeline_json_uses_renamed_fields` | `cpu-configs` | `partial` | 继承 §2.9 | 继承族级差异说明 |
| `schema_new_fields_do_not_break_5stage_baseline` | `cpu-configs` | `partial` | 继承 §2.9 | 继承族级差异说明 |

### 3.1 `cpu`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `beq_bne` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `blt_bge` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `bltu_bgeu` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `no_predict_initially` | `cpu` | `not-covered` | — | CH_MEM 侧无预测器状态机（冷启动不预测语义无载体）⇒ 归属 B1 item 5（ADR-088 §3 BP 行） |
| `predict_after_update` | `cpu` | `not-covered` | — | CH_MEM 侧无预测器更新路径 ⇒ 归属 B1 item 5（ADR-088 §3 BP 行） |
| `btb_entry` | `cpu` | `not-covered` | — | CH_MEM 侧无 BTB 结构（`branch_predictor_chmem.h` 不存在，符号数 0）⇒ 无对应物；归属 B1 item 5，**非「不做」**（ADR-088 §3 BP 行） |
| `not_taken_returns_zero` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `multiple_addresses` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `reset` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `per_thread_ghr_isolation` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `custom_btb_size` | `cpu` | `not-covered` | — | 同上：BTB 尺寸可配置能力在 CH_MEM 侧无载体；归属 B1 item 5（ADR-088 §3 BP 行） |
| `btb_runtime_16` | `cpu` | `not-covered` | — | 同上：BTB 表项数运行时配置在 CH_MEM 侧无载体；归属 B1 item 5（ADR-088 §3 BP 行） |
| `btb_runtime_64` | `cpu` | `not-covered` | — | 同上：BTB 表项数运行时配置在 CH_MEM 侧无载体；归属 B1 item 5（ADR-088 §3 BP 行） |
| `btb_runtime_256` | `cpu` | `not-covered` | — | 同上：BTB 表项数运行时配置在 CH_MEM 侧无载体；归属 B1 item 5（ADR-088 §3 BP 行） |
| `default_config` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `build_cpu_rv32` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `embedded_config` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `default_json_config` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `build_cpu_registers_11_real_plugins` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `CpuFactory_MMUCfg_PassesSatpValue` | `cpu` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §9.1：CH_MEM 工厂不接受 `CPUConfig` 且 `enable_mmu=true` 为 no-op ⇒ satp 路由在 CH_MEM 侧无载体** |
| `CPUConfig default isa/xlen contract` | `cpu` `isa-xlen` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `unsupported isa fail-fast` | `cpu` `isa-xlen` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `implemented isa xlen mismatch fail-fast` | `cpu` `isa-xlen` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `kernel T vs xlen cross-check fail-fast (reserved path)` | `cpu` `isa-xlen` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `declared-reserved accepted with single warning` | `cpu` `isa-xlen` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `validate_isa_xlen rejects kernel T mismatch (direct call)` | `cpu` `isa-xlen` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `validate_isa_xlen reserved path direct call` | `cpu` `isa-xlen` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `validate_isa_xlen unsupported isa direct call` | `cpu` `isa-xlen` | `partial` | 继承 §2.1 | 继承族级差异说明 |

### 3.2 `cpu-integration`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `CSRWriteSatpRoutesToRiscVMMUPlugin` | `cpu-integration` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §9.1：CH_MEM 工厂不接受 `CPUConfig` 且 `enable_mmu=true` 为 no-op ⇒ satp 路由在 CH_MEM 侧无载体** |
| `SFENCEVMARoutesToRiscVMMUPlugin` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |
| `MMUExceptionPropagatesToCPU` | `cpu-integration` | `partial` | 继承 §2.2 | 继承族级差异说明 |

### 3.36 `base-addr`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `cpu_sim --elf add.elf still works (back-compat)` | `base-addr` `cpu-sim` | `not-covered` | — | cpu_sim 是 TLM 专用 CLI（`pb->run()` 直仿）；CH_MEM 走 verilator_runner ⇒ 工具面整体迁移（B3），非覆盖缺口 |
| `cpu_sim --base-addr 0x80000000 accepts riscv-tests ELF (without hanging)` | `base-addr` `cpu-sim` | `not-covered` | — | 同上：cpu_sim 工具专属语义（base-addr 兼容），随工具迁移到 verilator_runner |

### 3.1 `cpu`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `cpu_sim_real_tohost` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `cpu_sim parses xlen from config (fail-fast on mismatch)` | `cpu` `isa-xlen` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `dbus_rv32_instantiate` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `dbus_rv64_instantiate` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `dbus_build_compiles` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `dbus_multiple_build_calls` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `dbus_destruction_safe` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `lui_decode` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `jal_decode` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `addi_decode` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `sub_decode` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `beq_decode` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `u_type` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `j_type` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `i_type_branch` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `i_type_alu` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `misc` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `b_type` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `l_type` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `s_type` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `r_type` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |

### 3.27 `elf-loader`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `elf loader full: multi PROGBITS section all loaded` | `elf-loader` `multi-section` | `partial` | 继承 §2.27 | 继承族级差异说明 |
| `elf loader full: tohost_addr extracted from .tohost section` | `elf-loader` `tohost` | `covered` | 继承 §2.27 | 继承族级差异说明 |
| `elf loader full: no .tohost section returns UINT64_MAX` | `elf-loader` `tohost` | `covered` | 继承 §2.27 | 继承族级差异说明 |

### 3.1 `cpu`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `d1_uid_payload_exists` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `d1_thread_id_payload_exists` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `d1_iid_pc_payload_exists` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `d2_reg_file_plugin_templated` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `d2_reg_file_plugin_multi_thread` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `d2_hazard_plugin_multi_thread` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `d2_branch_predictor_multi_thread` | `cpu` | `not-covered` | — | CH_MEM 侧无预测器 TID 参数面（多线程预测器隔离无载体）⇒ 归属 B1 item 5（ADR-088 §3 BP 行） |
| `d3_hazard_kind_enum` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `d4_branch_predictor_tid_param` | `cpu` | `not-covered` | — | 同上：预测器 TID 参数无载体 ⇒ 归属 B1 item 5（ADR-088 §3 BP 行） |
| `existing_reg_file_default` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `plugin_base_set_tid_default_noop` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `plugin_base_set_tid_overridable` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `reg_file_set_tid_stores_tid` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `hazard_set_tid_stores_tid` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `branch_predictor_set_tid_stores_tid` | `cpu` | `not-covered` | — | 同上：预测器 TID 存储语义无载体 ⇒ 归属 B1 item 5（ADR-088 §3 BP 行） |
| `pipe_builder_run_calls_set_tid_default` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `pipe_builder_run_calls_set_tid_per_thread` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `pipe_builder_run_dispatches_stages_per_tid` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `no_hazard_initially` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `raw_hazard_rs1` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `raw_hazard_rs2` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `waw_hazard` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `reset_clears_scoreboard` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `multi_thread_scoreboard` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `hazard_custom_n_regs` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `ibus_rv32_instantiate` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `ibus_rv64_instantiate` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `ibus_set_instruction` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `ibus_build_compiles` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `ibus_state_persistence` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `add_sub` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `logic` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `shift` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `compare` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `add` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `sub` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `sll` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `slt` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `sltu` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `xor` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `srl` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `sra` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `or` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `and` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `lsu_rv32_instantiate` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `lsu_rv64_instantiate` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `lsu_build_compiles` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `lsu_destruction_safe` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `mmu_exception_handler_page_fault_sets_pending` | `cpu` `mmu-exception-handler` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `mmu_exception_handler_no_fault_no_pending` | `cpu` `mmu-exception-handler` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `mmu_exception_handler_reset_clears_pending` | `cpu` `mmu-exception-handler` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `mmu_exception_handler_abi_smoke_reusable` | `cpu` `mmu-exception-handler` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `IBusPlugin_Consumes_PADDR_Over_VADDR_EndToEnd` | `cpu` `mmu` `paddr-propagation` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |
| `IBusPlugin_FallsBack_To_VADDR_When_MMU_Disabled` | `cpu` `mmu` `paddr-propagation` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `MMU_Translation_Writes_PADDR_And_PADDR_Valid_To_TlbLookupIfetch` | `cpu` `mmu` `paddr-propagation` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |
| `mul` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `div` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `rem` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `div_by_zero` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `boundary` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `instantiate_default_state` | `cpu` `mul-div-fsm` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `setup_build_no_throw` | `cpu` `mul-div-fsm` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `plugin_registers_to_pipe_builder` | `cpu` `mul-div-fsm` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `fsm_mul_state_transition` | `cpu` `mul-div-fsm` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 M 扩展行：CH_MEM 组合式为规范 ⇒ cycle 判据已退役**（差异为「cycle 不同构」，非 pass/fail） |
| `fsm_div_33_cycle` | `cpu` `mul-div-fsm` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 M 扩展行：CH_MEM 组合式为规范 ⇒ cycle 判据已退役**（差异为「cycle 不同构」，非 pass/fail） |
| `mul_one_cycle_busy` | `cpu` `mul-div-fsm` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 M 扩展行：CH_MEM 组合式为规范 ⇒ cycle 判据已退役**（差异为「cycle 不同构」，非 pass/fail） |
| `latency_one_is_baseline` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `latency_three_adds_substages` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `latency_five_adds_four_substages` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `rv32_keys_instantiate` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `rv64_keys_instantiate` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `rv32_cross_stage_read_write` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `rd_idx_boundary` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `decode_payload_struct` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `result_field` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `template_sanity` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |

### 3.30 `base-window`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `picolibc base=0 default preserves add.elf behavior` | `base-window` | `partial` | 继承 §2.30 | 继承族级差异说明 |
| `picolibc base=0x80000000 accepts riscv-tests memory layout` | `base-window` | `partial` | 继承 §2.30 | 继承族级差异说明 |

### 3.38 `write_half`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `picolibc write_half writes 2 bytes little-endian` | `write_half` | `partial` | 继承 §2.38 | 继承族级差异说明 |

### 3.43 `write_word`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `picolibc write_word per-byte tohost check (non-zero low byte)` | `write_word` | `partial` | 继承 §2.43 | 继承族级差异说明 |

### 3.28 `tohost`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `picolibc write_half at tohost triggers exit (PASS)` | `tohost` `write_half` | `covered` | 继承 §2.28 | 继承族级差异说明 |

### 3.30 `base-window`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `picolibc out-of-window write silently dropped` | `base-window` | `partial` | 继承 §2.30 | 继承族级差异说明 |

### 3.44 `load_section`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `picolibc load_section loads at sh_addr` | `load_section` | `partial` | 继承 §2.44 | 继承族级差异说明 |

### 3.1 `cpu`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `rv32_initial` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `rv64_initial` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `x0_write_mask` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `rv32_rw` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `rv64_rw` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `reg_file_custom_n_regs` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `multi_thread_isolation` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `five_stage_byte_identical` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `seven_stage_has_retire` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `three_stage_merged` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |
| `ten_stage_at_least_ten` | `cpu` | `partial` | 继承 §2.1 | 继承族级差异说明 |

### 3.3 `framework`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `namespace_isolation` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `can_include_all_three_headers` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `payload_coexists_with_chuint` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `plugin_can_hold_cpp_namespace_member` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `pipe_builder_orchestrates_plugin_lifecycle` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `empty_no_halt` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `single_halt` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `halt_or_merge` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `throw_independent` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `flush_independent` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `priority_halt_beats_throw` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `bypass` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `bypass_unregistered` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `clear` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `null_condition_ignored` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `fluent_api_returns_self` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `hello_plugin_runs` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `determinism_multiple_runs` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `determinism_independent_instances` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |

### 3.1 `cpu`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `cycle_parity_test_tlm_busy_cycles` | `cpu` `cycle-parity` `mul-div-fsm` | `not-covered` | — | cycle parity 断言的前提是两侧同构多周期实现；CH_MEM 组合化后判据本身失效（ADR-088 §3 M 扩展分歧） |

### 3.31 `cycle-parity`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `cycle_parity_test_chmem_file_exists` | `cycle-parity` `framework` `mul-div-fsm` | `covered` | 继承 §2.31 | 继承族级差异说明 |
| `cycle_parity_test_chmem_namespace_symbol` | `cycle-parity` `framework` `mul-div-fsm` | `covered` | 继承 §2.31 | 继承族级差异说明 |

### 3.3 `framework`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `mul_div_result_type_alias` | `framework` `result-paradigm` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `mul_div_result_ok_static_factory` | `framework` `result-paradigm` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `mul_div_result_err_static_factory` | `framework` `result-paradigm` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `mul_div_fsm_build_failfast_passes` | `framework` `result-paradigm` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `negotiate_hook_called_before_setup_build` | `framework` `negotiate` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `negotiate_default_empty_impl` | `framework` `negotiate` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `negotiate_missing_provider_failfast` | `framework` `negotiate` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `negotiate_resolved_providers_succeed` | `framework` `negotiate` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `capability_table_unresolved_lists_missing_keys` | `framework` `negotiate` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `payload_name` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `payload_different_types` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `store_put_get` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `compile_time_type_safety` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `store_isolation` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `payload_singleton` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `store_multi_type` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `store_has_clear` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `default_idle` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `fired_path` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `blocked_path` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `cancel_path` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `complete_cancel` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `arb_in_pipe_node` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `arb_in_pipe_builder` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `register_plugin` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `at_stage_order` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `run_executes_callbacks` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `node_of_logic_stage` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `node_auto_creation` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `setup_called_before_build` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `declare_substage` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `reset_all` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `has_stage` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `multi_plugin_stage_sharing` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `phase_values` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `empty_ctrl_no_effect` | `framework` `stall` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `single_halt_skip_callback` | `framework` `stall` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `single_halt_false_normal` | `framework` `stall` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `or_merge_two_ctrls` | `framework` `stall` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `or_merge_three_ctrls` | `framework` `stall` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `other_stage_not_affected` | `framework` `stall` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `throw_when_throws` | `framework` `stall` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `throw_then_no_commit` | `framework` `stall` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `flush_not_framework_consumed` | `framework` `stall` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `register_ctrl_link_count` | `framework` `stall` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `halt_stateful` | `framework` `stall` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `shared_ptr_ctrl_keeps_alive` | `framework` `stall` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `initial_state_idle` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `idle_to_firing` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `firing_to_moving` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `moving_to_blocked` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `blocked_to_firing` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `cancel_from_firing` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `cancel_from_moving` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `cancel_from_idle_no_op` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `pipe_node_reset` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `payload_access_via_operator` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `payload_via_put` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `payload_isolation_between_nodes` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `factory_create_node` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `state_name` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `plugin_base_is_abstract` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `derived_must_implement_build` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `good_plugin_works` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `setup_default_empty` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `setup_before_build` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `uint_t_width` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `no_copy` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `basic_array_interface` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `storage_reset` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `commit_noop_in_tlm` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `pipe_builder_commit_hook` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |
| `multiple_hooks_order` | `framework` | `covered` | 继承 §2.3 | 继承族级差异说明 |

### 3.4 `mmu`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `TypicalCPUSv39` | `mmu` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `TypicalGPUHighPorts` | `mmu` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `Construct` | `mmu` `MMUPlugin` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `ModeSet` | `mmu` `MMUPlugin` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `SetupDeclaresSubstages` | `mmu` `MMUPlugin` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `BareMode` | `mmu` `MMUPlugin` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `SingleLevelConfig` | `mmu` `MMUPlugin` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `SFENCEVMAInvalidatesTLBEntry` | `mmu` `MMUPlugin` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `SFENCEVMARs1ZeroTriggersAllInvalidate` | `mmu` `MMUPlugin` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `CsrWriteSatpUpdatesModeAndInvalidatesTLB` | `mmu` `MMUPlugin` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §9.1：CH_MEM 工厂不接受 `CPUConfig` 且 `enable_mmu=true` 为 no-op ⇒ satp 路由在 CH_MEM 侧无载体** |
| `MultiLevelTLBInvalidateASIDSwitch` | `mmu` `MMUPlugin` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `SFENCEVMARs1NonZeroInvalidatesAllASIDs` | `mmu` `MMUPlugin` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `PTWSv39ThreeLevelWalkCompletesViaAdvanceFromStub` | `mmu` `MMUPlugin` `PTW` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |
| `PTWAdvanceFromStubIsNoOpWhenNotBusy` | `mmu` `MMUPlugin` `PTW` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |

### 3.32 `bare-shortcut`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `BareShortcut_Sv32ZeroPpnInitiatesPTW` | `bare-shortcut` `mmu` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |
| `BareShortcut_Sv32NonZeroPpnTLBHitBypassesBare` | `bare-shortcut` `mmu` | `partial` | 继承 §2.32 | 继承族级差异说明 |
| `BareShortcut_Sv39NonZeroPpnTLBHitBypassesBare` | `bare-shortcut` `mmu` | `partial` | 继承 §2.32 | 继承族级差异说明 |

### 3.4 `mmu`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `ShadowBug_CtorSetsBaseSatpValue` | `mmu` `shadow-bug` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §9.1：CH_MEM 工厂不接受 `CPUConfig` 且 `enable_mmu=true` 为 no-op ⇒ satp 路由在 CH_MEM 侧无载体** |
| `ShadowBug_CsrWriteSatpUpdatesBaseNotShadow` | `mmu` `shadow-bug` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §9.1：CH_MEM 工厂不接受 `CPUConfig` 且 `enable_mmu=true` 为 no-op ⇒ satp 路由在 CH_MEM 侧无载体** |
| `ShadowBug_CsrWriteSatpPpnExtractionModeAware` | `mmu` `shadow-bug` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §9.1：CH_MEM 工厂不接受 `CPUConfig` 且 `enable_mmu=true` 为 no-op ⇒ satp 路由在 CH_MEM 侧无载体** |
| `L0Hit` | `mmu` `MultiLevelTLB` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `L0MissL1HitShadowFill` | `mmu` `MultiLevelTLB` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `BothMissTriggersRefill` | `mmu` `MultiLevelTLB` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `RefillFromPtwFillAll` | `mmu` `MultiLevelTLB` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |
| `NoShadowFillFlag` | `mmu` `MultiLevelTLB` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `InvalidateAsidAllLevels` | `mmu` `MultiLevelTLB` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `InvalidateAllBothLevels` | `mmu` `MultiLevelTLB` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `NumLevels` | `mmu` `MultiLevelTLB` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `PTW_Sv32_Walk_Via_Stub_Memory_BackwardCompat` | `mmu` `ptw` `real-memory` `stub-guard` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |

### 3.46 `api`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `PTW_Has_AdvanceFromRealMemory_Method` | `api` `mmu` `ptw` `real-memory` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |

### 3.4 `mmu`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `PTW_Sv32_VPN_HighBit_RealMemory_Walk_Succeeds` | `mmu` `ptw` `real-memory` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |
| `ptw_tlb_hit_fetch_not_stalled` | `mmu` `stall` | `not-covered` | — | PTW 命中后 fetch 仍停顿的时序依赖 TLM 多级停顿模型（scoreboard + hold）；CH_MEM 无 PTW 亦无流水级停顿 ⇒ 不可映射 |
| `ptw_tlb_miss_fetch_stalled` | `mmu` `stall` | `not-covered` | — | PTW 未命中触发 fetch 停顿：CH_MEM 侧 PTW 未注册（回退 vaddr）⇒ 该停顿路径不存在 |
| `ptw_complete_unstall_fetch_resumes` | `mmu` `stall` | `not-covered` | — | PTW 完成解除 fetch 停顿：同上，CH_MEM 无 PTW 状态机接线 ⇒ 无可恢复点 |
| `ptw_tlb_refill_after_completion` | `mmu` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |
| `ptw_walk_completes_and_idles` | `mmu` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |
| `PTWWalkCallbackClosureCapturesVaddr` | `mmu` `PTWUnit` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |
| `PTWInvalidPTETriggersFaultCallbackWithCode12` | `mmu` `PTWUnit` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |
| `PTWReservedEncodingTriggersFaultCallbackWithCode15` | `mmu` `PTWUnit` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |
| `PTWWalkCallbackWritesMMUVaddrOnSuccess` | `mmu` `PTWUnit` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |
| `PTWInvalidPTEWritesMMUVaddrOnFault` | `mmu` `PTWUnit` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |
| `PTWReservedEncodingWritesMMUVaddrOnFault` | `mmu` `PTWUnit` | `partial` | 继承族级 | 继承族级差异说明；**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`** |
| `CreateValidConfig` | `mmu` `TLBFactoryTest` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `CreateSmallFullyAssoc` | `mmu` `TLBFactoryTest` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `RejectInvalidEntries` | `mmu` `TLBFactoryTest` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `RejectInvalidWays` | `mmu` `TLBFactoryTest` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `RejectInvalidAsid` | `mmu` `TLBFactoryTest` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `NameIsCorrect` | `mmu` `TLBFactoryTest` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `DefaultConstructMiss` | `mmu` `TLBUnit` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `InsertThenHit` | `mmu` `TLBUnit` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `MissOnASIDMismatch` | `mmu` `TLBUnit` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `GlobalBypassesASID` | `mmu` `TLBUnit` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `InvalidateVaddr` | `mmu` `TLBUnit` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `InvalidateAsid` | `mmu` `TLBUnit` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `InvalidateAll` | `mmu` `TLBUnit` | `partial` | 继承 §2.4 | 继承族级差异说明 |
| `InsertFromNoStatsPollution` | `mmu` `TLBUnit` | `partial` | 继承 §2.4 | 继承族级差异说明 |

### 3.7 `soc`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `schema_has_required_top_level_fields` | `soc` | `partial` | 继承 §2.7 | 继承族级差异说明 |
| `schema_type_const_is_l1_cache` | `soc` | `partial` | 继承 §2.7 | 继承族级差异说明 |
| `schema_impl_mode_enum_includes_tlm_only` | `soc` | `partial` | 继承 §2.7 | 继承族级差异说明 |
| `schema_params_required_4_core_fields` | `soc` | `partial` | 继承 §2.7 | 继承族级差异说明 |
| `schema_params_strict_no_additional` | `soc` | `partial` | 继承 §2.7 | 继承族级差异说明 |
| `schema_param_defaults_match_l1_cache_geometry` | `soc` | `partial` | 继承 §2.7 | 继承族级差异说明 |
| `soc_json_has_required_top_level_fields` | `soc` | `partial` | 继承 §2.7 | 继承族级差异说明 |
| `soc_json_modules_match_v2_topology` | `soc` | `partial` | 继承 §2.7 | 继承族级差异说明 |
| `soc_json_connections_match_v2_pipeline` | `soc` | `partial` | 继承 §2.7 | 继承族级差异说明 |
| `soc_json_l1_bridge_params_match_plugin_geometry` | `soc` | `partial` | 继承 §2.7 | 继承族级差异说明 |

### 3.25 `MMUMinimalJson`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `soc_mmu_json_has_required_top_level_fields` | `MMUMinimalJson` `soc` | `partial` | 继承 §2.25 | 继承族级差异说明 |
| `soc_mmu_json_modules_match_full_chain_topology` | `MMUMinimalJson` `soc` | `partial` | 继承 §2.25 | 继承族级差异说明 |
| `soc_mmu_json_connections_match_full_chain_pipeline` | `MMUMinimalJson` `soc` | `partial` | 继承 §2.25 | 继承族级差异说明 |
| `soc_mmu_json_params_validate_vipt_safe_geometry` | `MMUMinimalJson` `soc` | `partial` | 继承 §2.25 | 继承族级差异说明 |

### 3.19 `cpu-l1-mmu-demo`

| case | tags | 态 | 锚点 | 差异说明 |
|---|---|---|---|---|
| `cpu_l1_mmu_demo_json_structure` | `cpu-l1-mmu-demo` `soc` | `partial` | 继承 §2.19 | 继承族级差异说明 |
| `cpu_l1_mmu_demo_add` | `cpu-l1-mmu-demo` `soc` | `partial` | 继承 §2.19 | 继承族级差异说明 |
| `cpu_l1_mmu_demo_addi` | `cpu-l1-mmu-demo` `soc` | `partial` | 继承 §2.19 | 继承族级差异说明 |
| `cpu_l1_mmu_demo_auipc` | `cpu-l1-mmu-demo` `soc` | `partial` | 继承 §2.19 | 继承族级差异说明 |
| `cpu_l1_mmu_demo_jal` | `cpu-l1-mmu-demo` `soc` | `partial` | 继承 §2.19 | 继承族级差异说明 |
| `cpu_l1_mmu_demo_beq` | `cpu-l1-mmu-demo` `soc` | `partial` | 继承 §2.19 | 继承族级差异说明 |

## 4. 措辞映射（早期草案 ↔ ADR-088 §4 三态）

| 草案措辞 | ADR-088 §4 三态 | 说明 |
|---|---|---|
| 已移植 CH_MEM | `covered` | CH_MEM 侧有等价覆盖**且**有回归锚；仍须填差异说明（`covered` ≠ 行为完全等价） |
| 由 ELF 级覆盖 | `covered`（语义等价时）/ `partial`（不等价时） | ELF 级端到端通过**不自动**等于语义等价（如 M 扩展 cycle 不同构 ⇒ 记 `partial`） |
| 显式退役 | `not-covered` | CH_MEM 侧确实不承接且**不计划**承接；须给逐条理由 + ADR-088 引用 |

## 5. B1 承载力补齐待办清单（`covered`/`partial` 条目汇总）

| # | 缺口（按 plugin / 能力归组） | 台账来源 | B1 待办 |
|---|---|---|---|
| 5.1 | **BP（BTB/Bimodal/GShare）**—— **B2 裁决：CH_MEM-only 且必须做** | 台账 §7 的 10 条 `not-covered`（`[cpu]` 族内，已按 B2 裁决降级） | CH_MEM 无 `branch_predictor_chmem.h`（319 行 TLM 实现 vs CH_MEM 符号数 0）。**不适用 §5.2 的「不做」出口**；**必须与 CoreMark harness（B1 item 1）同批**交付（否则 ≥1.9 门禁无法验证）；B3 删除门禁追加双前置（ADR-088 §5.2 BP 专项） |
| 5.2 | LSU / DBus 宽窄访问 | partial 族 `[cpu]` 内 LSU/DBus 用例 | CH_MEM 无 `lsu_chmem.h` / `dbus_chmem.h`（DMem 只做通路，宽度语义未对齐）⇒ B1 须补 |
| 5.3 | L1CachePlugin 本体 | not-covered 4 条 `[cache]`+`[MMUCacheIntegration]` + partial 21 条 `[cache]` | CH_MEM 只有 refill FSM（`l1_cache_refill_fsm_chmem.h`）⇒ 本体缺失；连带 MMU×Cache 集成不可映射 |
| 5.4 | MMU PTW walk | partial 族 `[mmu]`（52）+ `[PTW*]`/`[ptw]`/`[real-memory]`/`[paddr-propagation]`/`[bare-shortcut]`/`[shadow-bug]` | CH_MEM 的 `MMUPluginChmem` 为 TLB-only（miss ⇒ 回退 vaddr）；`PtWalkFsmPlugin` 存在但**未注册** ⇒ B1 须接线 |
| 5.5 | MMU exception handler 接线 | partial 族 `[mmu-exception-handler]`（4） | CH_MEM 侧实现存在（118 行）但被 `cpu_factory_chmem.h:342-349` 的 `#ifndef` 排除 ⇒ B1 须注册并补异常路径 |
| 5.6 | 多级 TLB / TLB 抽象 | partial 族 `[MultiLevelTLB]`（8）+ `[TLBUnit]`（8）+ `[TLBFactoryTest]`（6） | CH_MEM 用 8 项全相联 TLB，无多级/单元/工厂抽象 ⇒ B1 须决策：迁移结构 or 判定不需要 |
| 5.7 | 3/7/10-stage 流水线拓扑 | partial 族 `[cpu-integration]`（96，含 3/5/7/10-stage + canonical ordering） | CH_MEM 侧仅 5-stage（`cpu_factory_chmem.h` 无 stage 寄存器，全组合）⇒ B1 须决策是否需要多级形态 |
| 5.8 | CPUConfig isa/xlen 契约 | partial 族 `[isa-xlen]`（9）+ `[cpu-configs]`（13） | `validate_isa_xlen` 只在 TLM 工厂接线（`cpu_factory_chmem.h` 无同款校验）⇒ B1 须补同款契约校验 |
| 5.9 | 性能基线载体 | not-covered 3 条 `[dhrystone]` + `[performance]` + `[cycle-parity]` | DMIPS/MHz ≥1.4 与 CoreMark ≥1.9 的载体 100% 在 TLM ⇒ B1 须在 Verilator 侧重建测量与基线 CSV |
| 5.10 | 工具面迁移 | not-covered 2 条 `[cpu-sim]` | `tools/cpu_sim`（TLM 专属 CLI）整体迁往 `tools/verilator_runner`（B3 的一部分，但基线须在 B1 期间建立） |
| 5.11 | 多周期 M 扩展语义 | partial 族 `[mul-div-fsm]`（9）+ `[mul-div-fsm-integration]`（4）+ `[cycle-parity]` | CH_MEM 组合式 19 层 select 树 vs TLM radix-2 FSM：指令行为已覆盖、**cycle 语义不可比** ⇒ 需 B2 裁决「CH_MEM 为规范」或补 FSM 路径 |

## 6. 维护规则

1. 新增 TLM-only case ⇒ **必须**登记到 §3（否则 `check_coverage_parity_ledger.sh` 报出并 exit 1）
2. 计数漂移 ⇒ 跑 `tools/gen_coverage_parity_ledger.sh` 重算闭合，并更新头部基线 commit
3. `not-covered` 新增 ⇒ 需给**逐条差异化理由**（同理由 ≤10 次红线）；批量退役必须先有显式 ADR
4. 三态变更（尤其 `partial` → `covered`）⇒ 必须同步更新 §2 族级判定与 §5 B1 清单
5. 本台账**不替代** ADR-088 §3 的语义裁决表 —— 覆盖状态（有无回归锚）与语义等价性（是否同构）是两件事

## 7. B2 语义裁决对台账的落地标注（2026-10-24）

> **权威**：[ADR-088 v1.1 §3](../architecture/adr/ADR-088-tlm-backend-retirement.md)（逐子系统语义裁决表，5 行正式表态）。本节记录裁决**如何改变本台账的条目态**（既有的族级继承被**条目级覆盖**，解决 §6 维护规则 5 提到的「族内异质性」问题）。

| 裁决项（ADR-088 §3） | 影响的台账条目 | 态变化 | 覆盖后的差异说明 |
|---|---|---|---|
| **BP = CH_MEM-only 且必须做（不适用「不做」出口）** | `[cpu]` 族内 **10 条** BP 条目（`btb_entry` / `custom_btb_size` / `btb_runtime_{16,64,256}` / `no_predict_initially` / `predict_after_update` / `d2_branch_predictor_multi_thread` / `d4_branch_predictor_tid_param` / `branch_predictor_set_tid_stores_tid`） | `partial` → **`not-covered`** | 「CH_MEM 侧无 BTB/预测器结构（`branch_predictor_chmem.h` 不存在，符号数 0）⇒ 无对应物；**归属 B1 item 5，非「不做」**」（ADR-088 §3 BP 行） |
| **M 扩展 = CH_MEM 组合式为规范（功能与 cycle 均是）** | 8 条 cycle 相关（`fsm_mul_state_transition` / `fsm_div_33_cycle` / `mul_one_cycle_busy` / `mul_div_fsm_integration_{mul,div}_legacy` / `mul_div_fsm_integration_{mul_fsm,div_fsm_b1_fix}` / `10stage_mul_latency_5`） | 维持 `partial` | 补注「**ADR-088 §3：CH_MEM 组合式为规范 ⇒ cycle 判据已退役**（差异为 cycle 不同构，非 pass/fail）」 |
| **MMU PTW = 现在不接线；回退 vaddr = 已知临时降级，归 `chmem-ptw-walk`** | 17 条 PTW/paddr 条目 | 维持 `partial` | 补注「**ADR-088 §3 PTW 行：回退 vaddr = 已知临时降级（与 Bare 同构），实装归 `chmem-ptw-walk`**」 |
| **CH_MEM 工厂不接受 `CPUConfig`（NG1–NG3）** | 6 条 CSR/satp 路由条目 | 维持 `partial` | 补注「**ADR-088 §9.1：CH_MEM 工厂不接受 `CPUConfig` 且 `enable_mmu=true` 为 no-op ⇒ satp 路由在 CH_MEM 侧无载体**」 |
| CSR = CH_MEM 为规范 | （无 TLM 独占 CSR **寄存器语义**条目 —— TLM 侧 `csr.h` 是纯 stub，无行为测试） | — | 裁决记录在 ADR-088 §3；本台账无需改态 |

**红线复核（裁决后）**：`not-covered` 总数 **27**（原 17 + BP 10）≤ 135 ✅ · `[framework]` 族 0 条 `not-covered` ✅ · 同一理由 ≤10 次（BP 的 10 条按 4 组差异化理由写，最大组 5 条）✅。

**实现缺口已闭环（2026-10-24，rdd-quick `quick-ledger-same-reason-redline`）**：`tools/check_coverage_parity_ledger.sh` 现已机检全部三条红线 ——
① `not-covered` 总数 ≤135 · ② **同一理由 ≤10 次**（原脚本仅声明 `MAX_SAME_REASON=10` 常量而未使用 ⇒ 属既有 spec `tlm-coverage-parity-ledger-red-lines` 的实现缺口）· ③ `[framework]` 族零 `not-covered`。

**② 的机检口径**（记录以防口径漂移）：
- **仅作用于 `not-covered` 条目** —— `covered`/`partial` 的理由列大量相同（`继承族级差异说明…`）属正常继承形态，不适用本红线；
- **理由 = 台账第 5 列**（`awk -F'|'` 的 `$6`；`$1` 是首个 `|` 之前的空串 ⇒ 条目行 NF 恒为 7，已实测 27/27 行符合）；
- **归一化 = 仅去首尾空白**；**分组 = 完全相同的理由字符串**（字面口径）—— 不做语义归类，避免「语义相同但措辞不同」的批量理由逃逸；
- **违规须逐条列出该组条目名**（spec Scenario 明文要求「脚本报错并列出违规条目」）；
- 附带输出**理由分布**（分组数 / 最大组 / 重复组数 / 显式理由缺失数）供人工复核批量退役形态。

**相邻校验**：`not-covered` 条目的理由列**不得为空、也不得为继承标记**（`继承…`）—— 二者等价于「未写理由」，一并报 `VIOLATION: not-covered 条目缺显式理由`。

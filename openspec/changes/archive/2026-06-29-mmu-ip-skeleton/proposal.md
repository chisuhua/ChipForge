## Why

`ip/cpu/plugins/mmu.h` 当前是占位类（注释明确写"未来扩展 TLB + 页表遍历"），与 `ip/cache/` 等已具备完整目录骨架的 IP 不对称。MMU 是微架构密集型单元（CAM 查找 + 多级 + 替换策略 + PTW 状态机），与 CPU 执行流水线分支（branch_predictor/reg_file/hazard）性质不同，**适合独立为 `ip/mmu/`**，通过 `ip/cpu/plugins/mmu.h` 降级为 RISC-V ISA 适配器继承之。

本 change 落地 **`ip/mmu/` 骨架**（目录、Plugin、Bundle、TLB/MultiLevelTLB 抽象、policies、配置 schema、HDL 友好约束），**不实现 TLB/PTW 算法**——后者推迟到下一个 change `mmu-tlb-ptw-impl`，避免单 change 过大。

## What Changes

- **新增** `ip/mmu/` IP 目录骨架（与 `ip/cache/` 同构，**`lib/` + `tlm/` 双层切分**）
- **新增** `ip/mmu/lib/` 纯 C++ 算法层（**不依赖** `cf::plugin::*`，仅 include `cf::plugin/uint_t.h` 位宽类型）
  - `ip/mmu/lib/tlb_base.h`（抽象基类，MultiLevelTLB 持多态）
  - `ip/mmu/lib/tlb.h`（**模板化**单级 TLB：`template<ENTRIES, WAYS, TAG_BITS, ASID_BITS, PORTS>`，1:1 映射 CppHDL `ch::Component`）
  - `ip/mmu/lib/tlb_entry.h`（模板化，参数化 TAG_BITS/ASID_BITS）
  - `ip/mmu/lib/tlb_lookup.h`（用 valid bit 替代 `std::optional`）
  - `ip/mmu/lib/multi_level_tlb.h`（N 级 TLB 编排器，coherence 协议 + shadow fill）
  - `ip/mmu/lib/ptw.h`（PTW 接口 + Sv32/Sv39/Sv48 解码器，**用 Payload 跨周期传递状态，禁止业务 `tick()`**）
  - `ip/mmu/lib/tlb_factory.h`（按 JSON 配置选模板特化，DSE 零运行时分支）
- **新增** `ip/mmu/tlm/` 声明式 Plugin 层（依赖 `cf::plugin::PluginBase`，**所有业务逻辑用 `at_stage()` 声明**）
  - `ip/mmu/tlm/MMUPlugin.{h,cpp}`（PluginBase 派生，`setup()` 用 `declare_substage()` 声明 TLB lookup + PTW 子阶段；`build()` 用 `at_stage()` 注册翻译回调；持 `MultiLevelTLB` + `PTW` 为成员）
  - `ip/mmu/tlm/mmu_keys.h`（`Payload<T>` Key 集合：VADDR/PADDR/PTW_ACTIVE/PTW_L0_PTE 等，与 `ip/cpu/arch/riscv/payload_riscv.h` 同构）
- **新增** `ip/mmu/policies/tlb_replacement_policy.h` + 4 个策略（None/FIFO/LRU/RRIP）
- **新增** `ip/mmu/configs/params_schema.json`（含 `topology` / `asid_bits` / `supported_page_sizes` / `num_lookup_ports` 等 GPU 友好配置）
- **新增** `ip/mmu/{README.md, STATUS.md, docs/{README,architecture,configuration,integration}.md}`
- **新增** `bundles/mem_bundles.h` 扩展：加 `TlbReq` / `TlbResp` Bundle
- **重构** `ip/cpu/plugins/mmu.h`：占位降级为 `RiscvMMUPlugin : public cf::ip::mmu::MMUPlugin`，处理 RISC-V 特定 satp/exception 映射
- **新增** `tests/mmu/` 下 4 个测试文件（骨架 smoke + 多级 coherence + config schema 验证 + factory 特化）
- **修改** `ip/README.md` STATUS 表格，加 `mmu` 行

## Capabilities

### New Capabilities

- `mmu-ip-skeleton`: 建立 `ip/mmu/` IP 骨架（目录/STATUS/Plugin/Bundle/文档/配置），含 TLB 模板 + MultiLevelTLB 编排器 + 4 种替换策略 + PageTableWalker 接口；HDL 友好约束（编译期定长、禁 optional/virtual/dynamic alloc）；GPU/CPU 双兼容（asid_bits 0-16、num_lookup_ports 1-8、topology unified/split_id、supported_page_sizes 可配置）；shadow fill 默认 true 但可配置。**注**："配置驱动"是**编译期枚举式**——`TLBFactory` 在 `tlb_factory.cpp` 的 `if-else` 链显式实例化 5-7 组典型组合（entries × ways × asid_bits × ports），新增组合需修改工厂实现 + 重编译，**不**是运行时无限制配置。
- `mmu-tlb-coherence-protocol`: MultiLevelTLB 多级 TLB coherence 协议 —— 浅层 shadow fill（hit 下一级时回填浅层）、深层 evict 时反向失效浅层相同 (asid, vaddr) 条目、ASID/process switch 时的 invalidate_asid/invalidate_all 协议。

### Modified Capabilities

（无现有 spec 的 REQUIREMENTS 被本 change 修改；`ip-status-markdown-contract` 仅约束 zero-code IP，`test-location-discipline` 仅约束 IP test 位置约定 —— 本 change 自然遵循这两条约束但不改其 REQUIREMENTS。）

## Impact

- **新增文件**:
  - `ip/mmu/` 下 14 个文件 (5 个 tlm/.h, 1 个 tlm/.cpp, 4 个 policies/.h, 1 个 configs/params_schema.json, 3 个 docs, README, STATUS)
  - `bundles/mem_bundles.h` 内加 `TlbReq`/`TlbResp` (~30 LOC)
  - `tests/mmu/` 下 4 个 .cpp 测试文件
- **修改文件**:
  - `ip/cpu/plugins/mmu.h`: 占位 → `RiscvMMUPlugin` 继承 `cf::ip::mmu::MMUPlugin`（~30 LOC, 增量）
  - `ip/README.md`: STATUS 表格加 1 行
  - `CHANGELOG.md`: 加 v0.0.x 记录（"MMU IP 骨架 + lib/tlm 双层切分 + 多级 TLB 抽象 + 4 种策略 + GPU/CPU 双兼容配置 + Plugin 范式合规"）
  - `docs/architecture/overview.md` §"可插拔策略模式": 加 TLB 行
- **不修改**: `L1CachePlugin` / `L1CacheTLMBridge` / `cf_plugin` 框架层 / `cpp_plugin` Bridge 框架 / CppTLM / CppHDL
- **依赖与时序**:
  - 本 change 必须在 v0.0.5 (empty-directory-cleanup) **之后**实施——v0.0.5 已删除所有空目录，本 change 不创建空目录
  - 本 change **不**触及 `ip/cpu/plugins/` 中其他 Plugin 文件（hazard/branch_predictor/reg_file 等）——保持 M3 阶段 CPU Plugin 零干扰
  - 后续 change `mmu-tlb-ptw-impl` 必须在本 change 完成后开始
- **基线影响**:
  - 当前 ctest 基线: 21/21 PASS (cache-policy-foundation 完成后)
  - 增量: 骨架 + 多级 + config + factory 共约 12-15 个新测试
- **breaking 变更**: `ip/cpu/plugins/mmu.h` 占位类语义变化（不再"未来扩展 TLB+PTW"——这些现在归 `ip/mmu/`）；命名空间从 `cf::cpu::plugins::MMUPlugin` 改为继承 `cf::ip::mmu::MMUPlugin`，原 `MMUPlugin` 占位保留为类型别名 `using MMUPlugin = cf::ip::mmu::MMUPlugin`（向后兼容）
- **HDL 友好性**:
  - 全 `std::array` / `uint_t<N>` / 模板化
  - 禁 `std::optional`/`std::variant`/`virtual`/动态分配
  - Phase 5 CppHDL 转换时，`TLB<ENTRIES, WAYS, TAG_BITS, ASID_BITS, PORTS>` 1:1 映射 `ch::Component`，算法 0 改动
  - **Phase 5 HDL 1:1 映射验证推迟到 `mmu-tlb-ptw-impl` 之后**（forward-looking claim，骨架阶段通过 `tools/verify_hdl_friendly.sh` 静态检查 + `static_assert` 锁死硬约束）

## Alternatives Considered

### Alternative A: 在 `ip/cpu/plugins/` 下扩展 mmu.h，保持现状不分 IP

**放弃理由**: TLB 是微架构密集单元（CAM + 多级 + 替换策略 + PTW 状态机），与 branch_predictor/reg_file/hazard 性质不同；与 `ip/cache/` 现状不对称；Phase 1.5 L2CachePlugin 也是独立 Plugin 路径（不内嵌到 `ip/cpu/plugins/`）。**不放弃**：建立 `ip/mmu/` 独立 IP。

### Alternative B: TLB 单 Plugin 类继承 (`TLBL0`, `TLBL1` 子类)

**放弃理由**: 与 L1CachePlugin/L2CachePlugin 同病——每加一级要新建类；coherence 协议难统一；DSE 配置扫描需 N 个类。**不放弃**：模板化 `TLB<ENTRIES, WAYS, ...>` + `MultiLevelTLB` 编排器 + `TLBFactory` 工厂。N=1..4 任意级，零代码重复。

### Alternative C: GPU 兼容推迟到独立 change

**放弃理由**: 用户明确要求"通过配置而不是硬编码"。如果 skeleton 阶段把 RISC-V 9-bit ASID 硬编码进 C++，后续 GPU 兼容 change 需要重构所有 TLB 接口（破坏 ABI）。**不放弃**：skeleton 阶段就通过 `asid_bits`/`num_lookup_ports`/`topology`/`supported_page_sizes` 配置化，GPU/CPU 零代码切换。

### Alternative D: 把 HDL 友好性推迟到 Phase 5

**放弃理由**: Phase 5 CppHDL 转换时再回头修改 TLB 类的 std::array/uint_t 选择成本巨大（涉及所有 policy、所有 Bundle、所有 at_stage 闭包）。**不放弃**：skeleton 阶段就锁定 HDL 友好约束（`#error` 编译期检查禁 optional/virtual），Phase 5 转换零阻力。

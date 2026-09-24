## Why

2026-06-17 架构对齐审查发现 3 项 CRITICAL 漂移（`soc/riscv_virt.json` 引用 7 个不存在的类 + `impl_mode` 字段无人消费 + `soc/RiscvVirtSoC.h/cpp` 描述为已存在但实际为 0 个 .h/.cpp），以及 4 项 HIGH 漂移（命名不一致：`L1CacheTlm` 文档名 vs `L1CachePlugin` 实际类名；`RiscvIssTlm` 在 grep 中 0 匹配；`MemReqBundle : public bundle_base` 描述与 `POD + uint_t<N>` 实现矛盾）。这些漂移已对新人造成实质性误导（如按 `riscv_virt.json` 跑会全失败），必须在本轮修复以恢复文档/代码一致性基线。

## What Changes

- **删除** `soc/riscv_virt.json`（幽灵 SoC，引用 7 个不存在类 + impl_mode 字段无消费者）
- **删除** `ip/cpu/cpu_factory.cpp`（12 行 stub，无对应 .cpp 实现）
- **重写** `docs/architecture/overview.md` §"SoC 层是 IP 组合器" 段落（指向 `l1_cache_minimal.json` 真实工作示例替代虚构 `RiscvVirtSoC.h/cpp`）
- **重写** `docs/architecture/overview.md` §"ch_stream 接口即 ISA 无关层" 段落（推迟到 Phase 1.4+ 实际有第 2 个 CPU IP 时再验证）
- **更新** `docs/architecture/interface-design.md` §1.0 增加"已实现 POD vs 设计目标 bundle_base"对照表
- **跨 7 文档 grep 清理** `RiscvIssTlm|L1CacheTlm|BusMatrixTlm|DramTlm|UartTlm|ClintTlm|PlicTlm|RiscvCoreTlm` 8 个幽灵类名（替换为 Plugin 风格命名或删除段落）
- **重写** `soc/README.md` 顶部说明（从"已实现 RISC-V virt"改为"目前 2 个 L1Cache 验证配置"）
- **更新** `docs/architecture/code-framework-mapping.md §7.4` 漂移表（从"待修复"列表移到"已修复"段）
- **新增** `docs/architecture/adr/ADR-041-bridge-tick-pattern.md`（明确 Bridge 层允许 `tick()` 适配的混合模式合法性）
- **更新** `docs/architecture/adr.md` 插入 ADR-041 摘要
- **更新** `CHANGELOG.md` 记录 v0.0.2 "doc-code-realignment"

## Capabilities

### New Capabilities

- `arch-doc-consistency-baseline`: 建立文档/代码一致性基线，定义"幽灵引用 0 容忍"规则并以 CI grep 脚本固化。
- `bridge-tick-pattern-rationale`: 明确 Bridge 适配层允许 `tick()` 模式的边界条件，区分业务 Plugin（无 tick）与框架 Bridge（允许 tick 适配）的责任划分。

### Modified Capabilities

（无现有 spec，无 modified capabilities）

## Impact

- **影响文件**:
  - 删除: `soc/riscv_virt.json`, `ip/cpu/cpu_factory.cpp`
  - 修改: `docs/architecture/{overview,interface-design,code-framework-mapping}.md`, `soc/README.md`, `docs/architecture/adr.md`, `CHANGELOG.md`
  - 新增: `docs/architecture/adr/ADR-041-bridge-tick-pattern.md`
- **CI 影响**: 新增 grep 检查脚本（`tools/verify_no_ghost_refs.sh`），阻断任何带幽灵类名的 .md/.json/.h/.cpp
- **无代码运行时影响**: 仅文档/JSON 同步，不改任何运行时行为
- **breaking 变更**: 仅删除文件，无 API 变更（删除的 riscv_virt.json 不可运行；删除的 cpu_factory.cpp 是 stub）

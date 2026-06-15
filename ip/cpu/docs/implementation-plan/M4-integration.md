# M4 — CpuFactory + JSON 配置 + 集成测试

> **本文件位置**: `ip/cpu/docs/implementation-plan/M4-integration.md`
> **状态**: 🟡 待启动 (依赖 M2 + M3 完成)
> **估算**: 2-3 d
> **总体任务清单**: 见 [`README.md` §6 M4 行](README.md)

## 1. 目标

把 M2+M3 实施的 11 个 Plugin 组装成可调用的 `CpuFactory.build_cpu(config)`, 接受 JSON 配置返回完整 PipeBuilder, 并跑通 5 级 + 3 级流水线集成测试。这是"骨架+血肉"集成的一步。

## 2. 任务清单

| # | 任务 | 文件 | 验收 | 估时 |
|---|------|------|------|------|
| **M4.1** | 实施 `CpuFactory::build_cpu()` (议题 5 选 B: 集中 PluginOrder) | `ip/cpu/cpu_factory.h` | 编译通过, build_cpu() 返回可用 PipeBuilder | 0.5d |
| **M4.2** | 修订 `configs/cpu_default.json` (5 级 RV32IM_Zicsr, multi_isa v2.0 §6.1 字段) | `ip/cpu/configs/cpu_default.json` | JSON Schema 校验通过 | 0.3d |
| **M4.3** | 修订 `configs/cpu_embedded.json` (3 级 RV32I) | `ip/cpu/configs/cpu_embedded.json` | JSON Schema 校验通过 | 0.2d |
| **M4.4** | 新增 `configs/cpu_params_schema.json` (JSON Schema 校验) | `ip/cpu/configs/cpu_params_schema.json` | ajv 校验通过 | 0.2d |
| **M4.5** | 实施 `tests/integration/test_5stage_riscv.cpp` | `ip/cpu/tests/integration/` | build_cpu() 跑通 + 跑 add.elf + tohost=1 | 0.5d |
| **M4.6** | 实施 `tests/integration/test_3stage_riscv.cpp` | `ip/cpu/tests/integration/` | build_cpu() 跑通 + 跑 add.elf + tohost=1 | 0.3d |
| **M4.7** | 实施 `tests/manual_elf/add.S` (RV32I ADD 最小程序, 写 1 到 tohost) | `ip/cpu/tests/manual_elf/add.S` | 编译生成 add.elf, tohost=1 | 0.2d |
| **M4.8** | 实施 `tests/manual_elf/link.ld` (picolibc 链接脚本) | `ip/cpu/tests/manual_elf/link.ld` | 编译通过 | 0.1d |
| **M4.9** | 实施 `tests/manual_elf/README.md` (编译脚本) | `ip/cpu/tests/manual_elf/README.md` | 文档存在 | 0.05d |
| **M4.10** | 议题 6 选 C: 实施 `PicolibcHostMemory` 静态 RAM 模块 (64KB) | `ip/cpu/picolibc_host_memory.h` + `.cpp` | 单元测试 PASS | 0.4d |
| **M4.11** | build_cpu() 端到端跑通 (5 级 + 3 级) | — | 2/2 集成测试 PASS | (累计) |

## 3. 依赖

- ✅ M2 完成 (5 个 P0 Plugin)
- ✅ M3 完成 (6 个 RISC-V Plugin)

## 4. 完成判据

- [ ] M4.1-M4.10 全部 10 个子任务代码 + 文档 commit
- [ ] M4.11: ctest 2/2 集成测试 PASS (5 级 + 3 级 各跑通 add.elf, tohost=1)
- [ ] 16/16 ctest 全局不退化
- [ ] JSON Schema 校验工具 (ajv) 集成到 CI

## 5. 风险与缓解

| 风险 | 缓解 |
|------|------|
| CpuFactory::build_cpu() 调度顺序与 multi_isa v2.0 §3.2 不一致 | 严格按 §3.2 EARLY → NORMAL → LATE 顺序; 单元测试覆盖调度表生成 |
| JSON Schema 字段不匹配 (议题 4 选 B) | M4.4 实施时先冻结字段, 再写测试 |
| picolibc 工具链不熟悉 | M4.7 启动前花 0.5d 调研; 必要时回退到 riscv-gcc 裸 ELF |
| PicolibcHostMemory 64KB 限制 | 手工编译小 ELF (add.S 远 < 1KB), 留余量 |

## 6. 任务编号约定

`M4.x` 其中 x = 1..11 (与本文件 §2 表格 # 列对应)

## 相关文档

- 总体任务: [`README.md`](README.md) §6 M4 行
- CpuFactory 蓝图: [`../blueprint.md`](../blueprint.md) §5
- JSON 字段: [`../multi_isa_architecture.md`](../multi_isa_architecture.md) §6.1
- 议题 1-8 实施层决策: [`README.md`](README.md) §3
- 任务状态: [`../status.md`](../status.md)

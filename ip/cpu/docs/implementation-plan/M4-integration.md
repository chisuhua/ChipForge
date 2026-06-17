# M4 — CpuFactory + JSON 配置 + 集成测试

> **本文件位置**: `ip/cpu/docs/implementation-plan/M4-integration.md`
> **状态**: 🟢 v2.0 子任务 11/11 完成; 🟡 M4-DSE 子阶段 (M4.12-M4.19) 待启动
> **估算**: 2-3 d (v2.0) + 1 周 (M4-DSE)
> **总体任务清单**: 见 [`README.md` §6 M4 行](README.md)
> **DSE 详细设计**: 见 [`../dse_architecture.md`](../dse_architecture.md)

## 1. 目标

把 M2+M3 实施的 11 个 Plugin 组装成可调用的 `CpuFactory.build_cpu(config)`, 接受 JSON 配置返回完整 PipeBuilder, 并跑通 5 级 + 3 级流水线集成测试。这是"骨架+血肉"集成的一步。

**M4-DSE 子阶段**: 在 M4 v2.0 基础上, 把 `CpuFactory::build_cpu()` 从空 stub 变成真实注册 11 个 plugin, 让 `CPUConfig` 的运行时字段 (`pipeline_stages` / `btb_entries` / `branch_predictor` / `mul_latency` / `ext_*`) 真正影响行为。

## 2. 任务清单 (v2.0)

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

## 2.1 任务清单 (M4-DSE 子阶段) — 见 dse_architecture.md §9 Phase A + B

| # | 任务 | 文件 | 验收 | 估时 |
|---|------|------|------|------|
| **M4.12** | `PluginBase::setup_with_config(pb, const void*)` 加 3 行 | `include/cf/plugin/plugin_base.h` | 编译通过 + 现有 ctest 不退化 | 0.1d |
| **M4.13** | `PipeBuilder::merge_stage(name, parent)` 新增方法 | `include/cf/plugin/pipe_builder.h` | 单元测试 PASS | 0.2d |
| **M4.14** | `CpuFactory::build_cpu` 替换 stub, 真实 register 11 个 plugin | `ip/cpu/cpu_factory.h` | test_cpu_factory 升级 PASS | 1d |
| **M4.15** | 修复 `reg_file.cpp:40` 双重定义 bug (删除 .cpp build() 重定义) | `ip/cpu/plugins/reg_file.cpp` | reg_file_test PASS | 0.05d |
| **M4.16** | `parse_config(json_text)` + `validate_config(cfg)` + CPUConfig +12 字段 | `ip/cpu/cpu_factory.cpp` | 4 个示例 JSON 解析通过 | 0.5d |
| **M4.17** | `BranchPredictorPlugin` 模板参数化 + 10 种显式实例化 | `ip/cpu/plugins/branch_predictor.{h,cpp}` | 5 种 BTB 大小编译通过 | 0.5d |
| **M4.18** | `IBusPlugin` / `DBusPlugin` / `HazardPlugin` 接受 cfg 字段 | `ip/cpu/plugins/{ibus,dbus,hazard}.h` | 字段接受, 默认行为不变 | 0.3d |
| **M4.19** | test_cpu_factory 升级: 断言 `plugin_count()` / `stage_names()` / 拓扑 | `tests/cpu/test_cpu_factory.cpp` | 8 个用例 PASS | 0.3d |
| **M4-DSE 累计** | | | **0/8** | **~3 d** |

> **设计依据**: 见 [`../dse_architecture.md` §6 + §7](../dse_architecture.md)。

## 3. 依赖

- ✅ M2 完成 (5 个 P0 Plugin)
- ✅ M3 完成 (6 个 RISC-V Plugin)
- 🟡 M4-DSE 额外依赖 cf_plugin 加 2 个 API (`setup_with_config` / `merge_stage`)

## 4. 完成判据

### 4.1 v2.0 判据

- [ ] M4.1-M4.10 全部 10 个子任务代码 + 文档 commit
- [ ] M4.11: ctest 2/2 集成测试 PASS (5 级 + 3 级 各跑通 add.elf, tohost=1)
- [ ] 16/16 ctest 全局不退化
- [ ] JSON Schema 校验工具 (ajv) 集成到 CI

### 4.2 M4-DSE 判据 (新增)

- [ ] M4.12-M4.13: cf_plugin 2 个 API 扩展, 现有 18 个 ctest 不退化
- [ ] M4.14: `CpuFactory::build_cpu` 真实 register 11 个 plugin, 测试断言 `plugin_count() == 11`
- [ ] M4.15: `reg_file.cpp` 删除重复 build() 定义, 修复 `keys_rv32::` 硬编码 bug
- [ ] M4.16: 4 个 JSON 示例文件可被 `parse_config` 解析
- [ ] M4.17: `BranchPredictorPlugin<uint32_t, 16/32/64/128/256, 8/16>` 全部编译通过
- [ ] M4.18: `cfg.icache_latency` / `dcache_latency` 字段被 plugin 接受
- [ ] M4.19: test_cpu_factory 8 个用例全 PASS, 验证 `plugin_count` / `stage_names` / 拓扑

## 5. 风险与缓解

| 风险 | 缓解 |
|------|------|
| CpuFactory::build_cpu() 调度顺序与 multi_isa v2.0 §3.2 不一致 | 严格按 §3.2 EARLY → NORMAL → LATE 顺序; 单元测试覆盖调度表生成 |
| JSON Schema 字段不匹配 (议题 4 选 B) | M4.4 实施时先冻结字段, 再写测试 |
| picolibc 工具链不熟悉 | M4.7 启动前花 0.5d 调研; 必要时回退到 riscv-gcc 裸 ELF |
| PicolibcHostMemory 64KB 限制 | 手工编译小 ELF (add.S 远 < 1KB), 留余量 |
| **M4-DSE**: `BranchPredictorPlugin` 模板实例化膨胀二进制 | 5 种 BTB × 2 xlen = 10 实例, 每实例 < 5KB, 总膨胀 < 50KB, 可接受 |
| **M4-DSE**: `reg_file.cpp:40` bug 影响现有 test_reg_file | 修复后 reg_file_test 应当仍然 PASS (头文件版 build() 行为正确) |
| **M4-DSE**: `merge_stage` API 可能与其他 declare_substage 调用者冲突 | 仅影响 ip/cpu/, L1CachePlugin 不调用 merge_stage |

## 6. 任务编号约定

- v2.0: `M4.x` 其中 x = 1..11
- M4-DSE: `M4.x` 其中 x = 12..19 (新增, 不与 v2.0 冲突)

## 相关文档

- 总体任务: [`README.md`](README.md) §6 M4 行
- CpuFactory 蓝图: [`../blueprint.md`](../blueprint.md) §5
- JSON 字段: [`../multi_isa_architecture.md`](../multi_isa_architecture.md) §6.1
- **DSE 详细设计**: [`../dse_architecture.md`](../dse_architecture.md)
- 议题 1-8 实施层决策: [`README.md`](README.md) §3
- 任务状态: [`../status.md`](../status.md) §4.1

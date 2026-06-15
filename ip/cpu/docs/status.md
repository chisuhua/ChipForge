# ip/cpu 任务状态看板 (Status Board)

> **本文件位置**: `ip/cpu/docs/status.md`
> **作用**: M1.x 任务粒度的实时状态 (PASS/FAIL/进度%)。**唯一高频改文件**。
> **最后更新**: 2026-06-16 (拆分日, 全部任务 🔵 待启动)

> **本文件不包含**任务详细描述。 见 [`implementation-plan/M1..M5`](implementation-plan/)。
> **本文件不包含**静态架构、决策、范围。 见 [`blueprint.md`](blueprint.md) / [`cpu_implementation_guide_v2.0.md`](cpu_implementation_guide_v2.0.md) / [`implementation-plan/README.md`](implementation-plan/README.md)。

---

## 0. 总体状态

| 维度 | 状态 |
|------|------|
| **v2.0 文档** | 🟢 Accepted (用户 2026-06-15 接受 F1-F6 + 议题 1-8) |
| **M1 启动** | 🔵 待启动 (用户授权) |
| **M1-M5 累计完成** | 0 / 50 子任务 (0%) |
| **ctest 全局** | 16/16 PASS (基线, M1 启动后需保持) |
| **git tag** | 🔵 待 M5 创建 `phase-1.5-cpu-v2.0-2026-MM-DD` |

### 状态图例

| 标记 | 含义 |
|------|------|
| 🔵 待启动 | 任务已规划, 未开始 |
| 🟡 进行中 | 任务部分完成 |
| 🟢 PASS | 任务完成 + 验收通过 |
| 🔴 FAIL | 任务未通过验收, 见 "问题与回退" 段 |
| ⏸️ 阻塞 | 任务依赖外部输入 (用户授权 / 上游任务) |
| ⏭️ 跳过 | 任务推迟到后续阶段 |

---

## 1. M1 状态 — CPU 核心框架层

> 详细任务: [`implementation-plan/M1-cpu-skeleton.md`](implementation-plan/M1-cpu-skeleton.md)

| 任务 | 描述 | 状态 | 进度 | 验收 | 备注 |
|------|------|------|------|------|------|
| M1.1 | 扩展 `cf::plugin::PipeBuilder` 增加 `at_stage` | 🔵 待启动 | 0% | test_pipe_builder PASS | |
| M1.2 | 扩展 `cf::plugin::PipeBuilder` 增加 `declare_substage` | 🔵 待启动 | 0% | test_pipe_builder PASS | |
| M1.3 | 新增 `cf::plugin::PipeLink` StageLink | 🔵 待启动 | 0% | test_ctrl_link PASS | |
| M1.4 | 新增 `cf::plugin::PipeLink` DirectLink | 🔵 待启动 | 0% | test_ctrl_link PASS | |
| M1.5 | 新增 `cf::plugin::PipeArbitration` | 🔵 待启动 | 0% | test_pipe_node PASS | |
| M1.6 | 集成 PipeArbitration 到 PipeNode | 🔵 待启动 | 0% | test_pipe_node PASS | |
| M1.7 | 新增 `ip/cpu/core/payload_common.h` | 🔵 待启动 | 0% | test_payload PASS | |
| M1.8 | 4/4 框架级单元测试 PASS | 🔵 待启动 | 0% | ctest 4/4 | |
| **M1 累计** | | 🔵 待启动 | **0/8 (0%)** | ctest 4/4 + 16/16 不退化 | |

---

## 2. M2 状态 — ISA 无关 Plugin 套件

> 详细任务: [`implementation-plan/M2-core-plugins.md`](implementation-plan/M2-core-plugins.md)

| 任务 | 描述 | 状态 | 进度 | 验收 | 备注 |
|------|------|------|------|------|------|
| M2.1 | 实施 `RegFilePlugin` (array_store 模板) | 🔵 待启动 | 0% | test_reg_file 4-6 PASS | |
| M2.2 | 实施 `HazardPlugin` (RAW/WAW/WAR 检测) | 🔵 待启动 | 0% | test_hazard 4-6 PASS | |
| M2.3 | 实施 `BranchPredictorPlugin` (P1) | 🔵 待启动 | 0% | test_branch_predictor 4-6 PASS | |
| M2.4 | 实施 `IBusPlugin` | 🔵 待启动 | 0% | test_ibus 4-6 PASS | |
| M2.5 | 实施 `DBusPlugin` | 🔵 待启动 | 0% | test_dbus 4-6 PASS | |
| M2.6 | `fpu.h` P3+ 占位 | 🔵 待启动 | 0% | 占位存在 | |
| M2.7 | `mmu.h` P3+ 占位 | 🔵 待启动 | 0% | 占位存在 | |
| M2.8 | `exception.h` P3+ 占位 | 🔵 待启动 | 0% | 占位存在 | |
| M2.9 | 5/5 P0 单元测试 PASS | 🔵 待启动 | 0% | ctest 5/5 | |
| **M2 累计** | | 🔵 待启动 | **0/9 (0%)** | ctest 5/5 + 16/16 不退化 | |

---

## 3. M3 状态 — RISC-V ISA 特有 Plugin 套件

> 详细任务: [`implementation-plan/M3-riscv-plugins.md`](implementation-plan/M3-riscv-plugins.md)

| 任务 | 描述 | 状态 | 进度 | 验收 | 备注 |
|------|------|------|------|------|------|
| M3.1 | 实施 `decoder_table.h` | 🔵 待启动 | 0% | test_decode PASS | |
| M3.2 | 实施 `payload_riscv.h` | 🔵 待启动 | 0% | test_decode PASS | |
| M3.3 | 实施 `RiscvDecodePlugin` | 🔵 待启动 | 0% | test_decode 4-6 PASS | |
| M3.4 | 实施 `RiscvIntAluPlugin` | 🔵 待启动 | 0% | test_int_alu 4-6 PASS | |
| M3.5 | 实施 `RiscvBranchPlugin` (P1) | 🔵 待启动 | 0% | test_branch 4-6 PASS | |
| M3.6 | 实施 `RiscvMulPlugin` (P1, 3 级子流水) | 🔵 待启动 | 0% | test_mul 4-6 PASS | |
| M3.7 | 实施 `RiscvLsuPlugin` (P1) | 🔵 待启动 | 0% | test_lsu 4-6 PASS | |
| M3.8 | `RiscvCsrPlugin` P2 stub | 🔵 待启动 | 0% | 占位存在 | |
| M3.9 | `arch/riscv/fpu.h` P3+ 占位 | 🔵 待启动 | 0% | 占位存在 | |
| M3.10 | 6 × 单元测试 PASS | 🔵 待启动 | 0% | ctest 6/6 | |
| M3.11 | RV32I 译码正确性 (35+ 指令) | 🔵 待启动 | 0% | 35+ PASS | |
| M3.12 | RV32I 整数运算 (10 指令) | 🔵 待启动 | 0% | 10 PASS | |
| **M3 累计** | | 🔵 待启动 | **0/12 (0%)** | ctest 6/6 + 16/16 不退化 | |

---

## 4. M4 状态 — CpuFactory + JSON + 集成测试

> 详细任务: [`implementation-plan/M4-integration.md`](implementation-plan/M4-integration.md)

| 任务 | 描述 | 状态 | 进度 | 验收 | 备注 |
|------|------|------|------|------|------|
| M4.1 | 实施 `CpuFactory::build_cpu()` | 🔵 待启动 | 0% | 编译通过 + 返回可用 PipeBuilder | |
| M4.2 | 修订 `configs/cpu_default.json` | 🔵 待启动 | 0% | JSON Schema 校验 | |
| M4.3 | 修订 `configs/cpu_embedded.json` | 🔵 待启动 | 0% | JSON Schema 校验 | |
| M4.4 | 新增 `configs/cpu_params_schema.json` | 🔵 待启动 | 0% | ajv 校验 | |
| M4.5 | `tests/integration/test_5stage_riscv.cpp` | 🔵 待启动 | 0% | build_cpu() 跑通 + tohost=1 | |
| M4.6 | `tests/integration/test_3stage_riscv.cpp` | 🔵 待启动 | 0% | build_cpu() 跑通 + tohost=1 | |
| M4.7 | `tests/manual_elf/add.S` | 🔵 待启动 | 0% | 编译 + tohost=1 | |
| M4.8 | `tests/manual_elf/link.ld` | 🔵 待启动 | 0% | 编译通过 | |
| M4.9 | `tests/manual_elf/README.md` | 🔵 待启动 | 0% | 文档存在 | |
| M4.10 | `PicolibcHostMemory` 64KB 静态 RAM | 🔵 待启动 | 0% | 单元测试 PASS | |
| M4.11 | build_cpu() 端到端 (5 级 + 3 级) | 🔵 待启动 | 0% | 2/2 集成测试 PASS | |
| **M4 累计** | | 🔵 待启动 | **0/11 (0%)** | ctest 2/2 + 16/16 不退化 | |

---

## 5. M5 状态 — 联调 + 文档收尾

> 详细任务: [`implementation-plan/M5-verification.md`](implementation-plan/M5-verification.md)

| 任务 | 描述 | 状态 | 进度 | 验收 | 备注 |
|------|------|------|------|------|------|
| M5.1 | `soc/cpu_l1_picolibc/demo.json` | 🔵 待启动 | 0% | SoC 实例化通过 | |
| M5.2 | 编译 6 个 RISC-V ELF (add/sub/and/or/sll/srli) | 🔵 待启动 | 0% | 6 .elf 编译通过 | |
| M5.3 | `tests/integration/test_demo_soc.cpp` | 🔵 待启动 | 0% | 6/6 tohost=1 PASS | |
| M5.4 | 修订 `ip/cpu/README.md` | 🔵 待启动 | 0% | 文档完整 | |
| M5.5 | 修订 `ip/cpu/docs/README.md` | 🔵 待启动 | 0% | 索引完整 | |
| M5.6 | 起草 ADR-XXX (Plugin 推迟决策) | 🔵 待启动 | 0% | ADR Accepted | |
| M5.7 | 起草 `docs/lessons/m1-m5-cpu-implementation.md` | 🔵 待启动 | 0% | lessons 文档存在 | |
| M5.8 | `git commit` + `git tag` | 🔵 待启动 | 0% | tag 创建 | |
| M5.9 | 4-6 ELF + 16/16 ctest + D4 + ADR-040 | 🔵 待启动 | 0% | 全部通过 | |
| **M5 累计** | | 🔵 待启动 | **0/9 (0%)** | 全部 9 项验收通过 | |

---

## 6. 累计进度

| 阶段 | 子任务数 | 完成 | 进度 | 累计 ctest |
|------|----------|------|------|------------|
| M1 | 8 | 0 | 0% | 4/4 |
| M2 | 9 | 0 | 0% | 5/5 |
| M3 | 12 | 0 | 0% | 6/6 |
| M4 | 11 | 0 | 0% | 2/2 |
| M5 | 9 | 0 | 0% | 4-6 ELF |
| **总计** | **49** | **0** | **0%** | — |

---

## 7. 问题与回退 (Lessons Learned Log)

> **这里记录 M1-M5 实施过程中遇到的 B2 摩擦 + 解决方案 + 决策**。 M5.7 起草 `docs/lessons/m1-m5-cpu-implementation.md` 时从这里汇总。

| 日期 | 阶段 | 问题 | 解决 | 决策 ID |
|------|------|------|------|---------|
| 2026-06-16 | 拆分 | v2.0 文档 1220 行混杂决策/架构/规划/看板 | 拆为 4 类文件 (blueprint/plan/status/entry) | — |
| _待填_ | _M1._ | _..._ | _..._ | _..._ |

---

## 8. 更新约定

- **每次子任务完成**: 更新对应行的 状态/进度/备注 列
- **每周 review**: 累计进度 6 段必须刷新
- **遇到 B2 摩擦**: 立即填 §7 "问题与回退", 同步给 L1Cache lessons 文档
- **状态徽章变化**: 任务从 🔵 → 🟡 → 🟢/🔴, 不允许跳级

---

## 相关文档

- **静态架构**: [`blueprint.md`](blueprint.md)
- **总体实施规划**: [`implementation-plan/README.md`](implementation-plan/README.md)
- **决策入口**: [`cpu_implementation_guide_v2.0.md`](cpu_implementation_guide_v2.0.md)
- **M1 详细**: [`implementation-plan/M1-cpu-skeleton.md`](implementation-plan/M1-cpu-skeleton.md)
- **M2 详细**: [`implementation-plan/M2-core-plugins.md`](implementation-plan/M2-core-plugins.md)
- **M3 详细**: [`implementation-plan/M3-riscv-plugins.md`](implementation-plan/M3-riscv-plugins.md)
- **M4 详细**: [`implementation-plan/M4-integration.md`](implementation-plan/M4-integration.md)
- **M5 详细**: [`implementation-plan/M5-verification.md`](implementation-plan/M5-verification.md)

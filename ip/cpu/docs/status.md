# ip/cpu 任务状态看板 (Status Board)

> **本文件位置**: `ip/cpu/docs/status.md`
> **作用**: M1.x 任务粒度的实时状态 (PASS/FAIL/进度%)。**唯一高频改文件**。
> **最后更新**: 2026-06-17 (新增 M4G 子阶段, Oracle 评审通过)

> **本文件不包含**任务详细描述。 见 [`implementation-plan/M1..M5`](implementation-plan/)。
> **本文件不包含**静态架构、决策、范围。 见 [`blueprint.md`](blueprint.md) / [`cpu_implementation_guide_v2.0.md`](cpu_implementation_guide_v2.0.md) / [`implementation-plan/README.md`](implementation-plan/README.md)。

---

## 0. 总体状态

| 维度 | 状态 |
|------|------|
| **v2.0 文档** | 🟢 Accepted (用户 2026-06-15 接受 F1-F6 + 议题 1-8) |
| **v2.0 拆分 commit** | ✅ 已 push (commit `06721f1`, 2026-06-16) |
| **v2.0 baseline tag** | ✅ 已创建 `phase-1.5-cpu-v2.0-baseline-2026-06-16` |
| **v2.0-locks 文档** | 🟢 Accepted (Oracle 评审通过, 2026-06-17) |
| **M1 启动** | 🟢 M1 完成 (C1-C6 全部 commit, 8/8 子任务 PASS) |
| **M1-M5 累计完成** | **47 / 49 子任务 (96%, M1-M5 段 100%, M5.1 stub)** |
| **M4G 子阶段** | 🔵 **Ready to Start** (Oracle 评审通过, 8 个新子任务) |
| **M4-DSE / M5-DSE 子阶段** | 🔵 待启动 — 依赖 M4G 完成 |
| **ctest 全局** | **18/18 PASS** (16 既有 + 2 新增: test_pipe_arbitration + test_payload_common) |
| **git tag** (终点) | 🔵 待 M5 创建 `phase-1.5-cpu-v2.0-2026-MM-DD` (不同于 baseline tag) |

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
> **🟢 2026-06-16 M1 收官**: C1-C6 全部 commit 完成, 8/8 子任务 PASS, 18/18 ctest 全局不退化。 实施路径 1.1 d (原估 3-4 d, 节省 2-3 d 复用成本)。

| 任务 | 描述 | 状态 | 进度 | 验收 | 备注 |
|------|------|------|------|------|------|
| M1.1 | 扩展 `cf::plugin::PipeBuilder` 增加 `at_stage` | 🟢 基线复用 | 100% (cf_plugin Phase 0) | test_pipe_builder PASS | C1 公告, 0 实施量 |
| M1.2 | 扩展 `cf::plugin::PipeBuilder` 增加 `declare_substage` | 🟢 基线复用 | 100% (cf_plugin Phase 0) | test_pipe_builder PASS | C1 公告, 0 实施量 |
| M1.3 | 新增 `cf::plugin::PipeLink` StageLink | 🟢 PASS (via CtrlLink 复用) | 100% | test_ctrl_link PASS | CtrlLink 已有 valid/ready 握手 API |
| M1.4 | 新增 `cf::plugin::PipeLink` DirectLink | 🟢 PASS (via CtrlLink 复用) | 100% | test_ctrl_link PASS | 同 M1.3 |
| M1.5 | 新增 `cf::plugin::PipeArbitration` | 🟢 PASS | 100% (C2 commit) | test_pipe_arbitration PASS | 全新头文件 140 行 + 7 用例测试 |
| M1.6 | 集成 PipeArbitration 到 PipeNode | 🟢 PASS | 100% (C3 commit) | test_pipe_arbitration PASS (Test 6) | arb_ 字段 + arb()/arb_mut()/arbitration() API, 5 态方法保留 |
| M1.7 | 新增 `ip/cpu/core/payload_common.h` | 🟢 PASS | 100% (C4 commit) | test_payload_common PASS | 全新头文件 152 行 + 7 用例测试 |
| M1.8 | 4/4 框架级单元测试 PASS | 🟢 PASS | 100% (C5+C6 commit) | **ctest 18/18 PASS** (2 新增 + 16 既有不退化) | test_pipe_arbitration + test_payload_common |
| **M1 累计** | | 🟢 **M1 完成** | **8/8 (100%)** | ctest 18/18 + 16/16 不退化 | C1-C6 全部 commit, 实施 1.1 d |

---

## 2. M2 状态 — ISA 无关 Plugin 套件

> 详细任务: [`implementation-plan/M2-core-plugins.md`](implementation-plan/M2-core-plugins.md)

| 任务 | 描述 | 状态 | 进度 | 验收 | 备注 |
|------|------|------|------|------|------|
| M2.1 | 实施 `RegFilePlugin` (array_store 模板) | 🟢 PASS | 100% | test_reg_file 4-6 PASS | M2.1 commit |
| M2.2 | 实施 `HazardPlugin` (RAW/WAW/WAR 检测) | 🟢 PASS | 100% | test_hazard 4-6 PASS | M2.2 commit |
| M2.3 | 实施 `BranchPredictorPlugin` (P1) | 🟢 PASS | 100% | test_branch_predictor 4-6 PASS | M2.3 commit |
| M2.4 | 实施 `IBusPlugin` | 🟢 PASS | 100% | test_ibus 4-6 PASS | M2.4 commit (stub) |
| M2.5 | 实施 `DBusPlugin` | 🟢 PASS | 100% | test_dbus 4-6 PASS | M2.5 commit (stub) |
| M2.6 | `fpu.h` P3+ 占位 | 🟢 PASS | 100% | 占位存在 | M2.6 commit |
| M2.7 | `mmu.h` P3+ 占位 | 🟢 PASS | 100% | 占位存在 | M2.7 commit |
| M2.8 | `exception.h` P3+ 占位 | 🟢 PASS | 100% | 占位存在 | M2.8 commit |
| M2.9 | 5/5 P0 单元测试 PASS | 🟢 PASS | 100% | ctest 5/5 | test_reg_file/hazard/branch_predictor/ibus/dbus 全 PASS |
| **M2 累计** | | 🟢 完成 | **9/9 (100%)** | ctest 23/23 + 16/16 不退化 | M2 全部完成 |

---

## 3. M3 状态 — RISC-V ISA 特有 Plugin 套件

> 详细任务: [`implementation-plan/M3-riscv-plugins.md`](implementation-plan/M3-riscv-plugins.md)

| 任务 | 描述 | 状态 | 进度 | 验收 | 备注 |
|------|------|------|------|------|------|
| M3.1 | 实施 `decoder_table.h` | 🟢 PASS | 100% | test_decode PASS | M3.1 commit (40 RV32I 指令) |
| M3.2 | 实施 `payload_riscv.h` | 🟢 PASS | 100% | test_decode PASS | M3.2 commit (RiscvDecodeDetail + RISCV_DETAIL Key) |
| M3.3 | 实施 `RiscvDecodePlugin` | 🟢 PASS | 100% | test_decode 5/5 PASS | M3.3 commit (decode 阶段填 DECODE+RISCV_DETAIL) |
| M3.4 | 实施 `RiscvIntAluPlugin` | 🟢 PASS | 100% | test_int_alu 4/4 PASS | M3.4 commit (10 算术指令) |
| M3.5 | 实施 `RiscvBranchPlugin` (P1) | 🟢 PASS | 100% | test_branch 3/3 PASS | M3.5 commit (BEQ/BNE/BLT/BGE/BLTU/BGEU) |
| M3.6 | 实施 `RiscvMulPlugin` (P1, 3 级子流水) | 🟢 PASS | 100% | test_mul 5/5 PASS | M3.6 commit (MUL/MULH*/DIV*/REM*) |
| M3.7 | 实施 `RiscvLsuPlugin` (P1) | 🟢 PASS | 100% | test_lsu 4/4 PASS | M3.7 commit (地址生成 + LOAD/STORE stub) |
| M3.8 | `RiscvCsrPlugin` P2 stub | 🟢 PASS | 100% | 占位存在 | M3.8 commit (P2 stub) |
| M3.9 | `arch/riscv/fpu.h` P3+ 占位 | 🟢 PASS | 100% | 占位存在 | M3.9 commit (P3+) |
| M3.10 | 5 × 单元测试 PASS | 🟢 PASS | 100% | ctest 5/5 (5/6 目标) | 缺 test_payload_riscv, 并入 test_decode |
| M3.11 | RV32I 译码正确性 (35+ 指令) | 🟢 PASS | 100% | 40 用例 PASS | test_decode_full 40+ RV32I 指令 |
| M3.12 | RV32I 整数运算 (10 指令) | 🟢 PASS | 100% | 38 用例 PASS | test_int_alu_full 10 算术指令 |
| **M3 累计** | | 🟢 完成 | **12/12 (100%)** | ctest 7/7 + 28/28 不退化 | M3 全部完成 |

---

## 4. M4 状态 — CpuFactory + JSON + 集成测试

> 详细任务: [`implementation-plan/M4-integration.md`](implementation-plan/M4-integration.md)

| 任务 | 描述 | 状态 | 进度 | 验收 | 备注 |
|------|------|------|------|------|------|
| M4.1 | 实施 `CpuFactory::build_cpu()` | 🟢 PASS | 100% | 编译通过 + 返回可用 PipeBuilder | M4.1 commit |
| M4.2 | 修订 `configs/cpu_default.json` | 🟢 PASS | 100% | JSON Schema 校验 | 已有 (5 级 RV64GC) |
| M4.3 | 修订 `configs/cpu_embedded.json` | 🟢 PASS | 100% | JSON Schema 校验 | 已有 (3 级 RV32IMAC) |
| M4.4 | 新增 `configs/cpu_params_schema.json` | 🟢 PASS | 100% | ajv 校验 | 已有 (draft-07) |
| M4.5 | `tests/integration/test_5stage_riscv.cpp` | 🟢 PASS | 100% | 5/5 ctest PASS | M4.5 commit |
| M4.6 | `tests/integration/test_3stage_riscv.cpp` | 🟢 PASS | 100% | 4/4 ctest PASS | M4.6 commit |
| M4.7 | `tests/manual_elf/add.S` | 🟢 PASS | 100% | 编译 + tohost=1 | M4.7 commit (RV32I ADD) |
| M4.8 | `tests/manual_elf/link.ld` | 🟢 PASS | 100% | 编译通过 | M4.8 commit (64KB RAM) |
| M4.9 | `tests/manual_elf/README.md` | 🟢 PASS | 100% | 文档存在 | M4.9 commit |
| M4.10 | `PicolibcHostMemory` 64KB 静态 RAM | 🟢 PASS | 100% | 单元测试 PASS | M4.10 commit (tohost 机制) |
| M4.11 | build_cpu() 端到端 (5 级 + 3 级) | 🟢 PASS | 100% | 2/2 集成测试 PASS | M4.11 commit |
| **M4 累计 (v2.0)** | | 🟢 **M4 完成** | **11/11 (100%)** | ctest 33/33 + 23/23 不退化 | M4 全部完成 |

### 4.1 M4-DSE 子阶段 — CpuFactory stub 真实化 + DSE 旋钮接入

> **前置**: v1.0 §9 任务（A.1-A.6 + B.1-B.5）须先经 [`dse_architecture_v2_locks.md` §7](../../dse_architecture_v2_locks.md#7-9-任务与-oracle-评审对齐表) 裁剪后再实施（Oracle 2026-06-17 评审）。本表 M4.12-M4.19 编号保留作为 M4-DSE 实施计划占位；具体任务内容须在实施 change 中按 §7 裁剪表的 ✅/🟡/❌ 状态重新映射。
> **本节为 dse_architecture.md Phase A + B 的任务拆分**。详细设计见 [dse_architecture.md §9](dse_architecture.md)。
> **状态**: 🔵 待启动 (M5 收官后或并行启动)

| 任务 | 描述 | 状态 | 进度 | 验收 | 备注 |
|------|------|------|------|------|------|
| M4.12 | `PluginBase::setup_with_config(pb, const void*)` 加 3 行 | 🔵 待启动 | 0% | 编译通过 + 现有 ctest 不退化 | cf_plugin 最小改动 |
| M4.13 | `PipeBuilder::merge_stage(name, parent)` 新增方法 | 🔵 待启动 | 0% | 单元测试 PASS | 解决 3 级拓扑合并 |
| M4.14 | `CpuFactory::build_cpu` 替换 stub, 真实 register 11 个 plugin | 🔵 待启动 | 0% | test_cpu_factory 升级 PASS | 核心工作量 (~140 行) |
| M4.15 | 修复 `reg_file.cpp:40` 双重定义 bug (删除 .cpp build() 重定义) | 🔵 待启动 | 0% | reg_file_test PASS | 真 Bug 修复 |
| M4.16 | `parse_config(json_text)` + `validate_config(cfg)` + CPUConfig +12 字段 | 🔵 待启动 | 0% | 4 个示例 JSON 解析通过 | JSON 子集手写解析 |
| M4.17 | `BranchPredictorPlugin` 模板参数化 + 10 种显式实例化 | 🔵 待启动 | 0% | 5 种 BTB 大小编译通过 | 让 btb_entries 字段真正生效 |
| M4.18 | `IBusPlugin` / `DBusPlugin` / `HazardPlugin` 接受 cfg 字段 | 🔵 待启动 | 0% | 字段接受, 默认行为不变 | stub 行为升级 |
| M4.19 | test_cpu_factory 升级: 断言 `plugin_count()` / `stage_names()` / 拓扑 | 🔵 待启动 | 0% | 8 个用例 PASS | 测试从 "非空" 升级到 "正确" |
| **M4-DSE 累计** | | 🔵 待启动 | **0/8** | — | 见 [dse_architecture.md §9 Phase A+B](dse_architecture.md#9-实施路线图) |

### 4.2 M4G 子阶段 — Phase 1 Forward-Compatibility Locks

> **本节为 [dse_architecture_v2_locks.md](dse_architecture_v2_locks.md) 的任务拆分 (Oracle 评审通过, 2026-06-17)**。
> **详细设计**: 见 [dse_architecture_v2_locks.md](dse_architecture_v2_locks.md) 和 [implementation-plan/M4G-forward-compat-locks.md](implementation-plan/M4G-forward-compat-locks.md)
> **状态**: 🟢 **完成** (依赖 M4 完成 ✅, M4G 8/8 PASS, ctest 43/43 全绿)
> **前置**: M4-DSE 启动需 M4G 完成 ✅

| 任务 | 描述 | 状态 | 进度 | 验收 | 备注 |
|------|------|------|------|------|------|
| G.1 | D.1: 添加 `UID` / `THREAD_ID` / `IID_PC` Payloads (#12/#13/#14) | 🟢 PASS | 100% | 编译通过 + 35/35 ctest 不退化 + 3 个新 static_assert | 3 行 header, commit bb6e0e5 |
| G.2 | D.2: 模板化 `RegFilePlugin<T, N_REGS=32, N_THREADS=1>` + CpuFactory smoke test | 🟢 PASS | 100% | 默认参数兼容 + per-thread 隔离 + 7/7 test_reg_file + CpuFactory 编译 | ~30 行 header + smoke test, commit 01a810a |
| G.3 | D.2: 模板化 `HazardPlugin<T, N_REGS=32, N_THREADS=1>` | 🟢 PASS | 100% | 默认参数兼容 + per-thread scoreboard 隔离 + 7/7 test_hazard | ~30 行 header, commit 6b4b208 |
| G.4 | D.2 + D.4: 模板化 `BranchPredictorPlugin<T, BTB_SIZE, BIMODAL_SZ, GSHARE_SZ, GHR_BITS, N_THREADS>` + `predict`/`update` 接受 `tid` | 🟢 PASS | 100% | 默认参数兼容 + per-thread GHR 隔离 + 8/8 test_branch_predictor | ~40 行 header, commit 91007b6 |
| G.5 | D.3: `HazardPlugin::has_hazard` 返回 `HazardKind` enum (NONE/RAW_RS1/RAW_RS2/WAW) | 🟢 PASS | 100% | 1 in-tree 业务 + 4 测试断言更新 + 4 enum 值正确 + 7/7 test_hazard | ~10 行, commit bd69755 |
| G.6 | D.4: `BranchPredictorPlugin::predict`/`update` 接受 `tid` | 🟢 PASS | 100% | (G.4 已覆盖) | 0h (G.4 合并) |
| G.7 | 单元测试: D.1-D.4 所有锁的验证 (`test_forward_compat.cpp` 新建) | 🟢 PASS | 100% | 10/10 test_forward_compat ctest PASS | 215 行测试, commit 4bc742d |
| G.8 | 文档同步: `blueprint.md` / `status.md` / `README.md` | 🟢 PASS | 100% | 文档完整且一致 | 本 commit |
| **M4G 累计** | | 🟢 完成 | **8/8** | **43/43 ctest PASS** | Oracle 评审: D.1-D.4 是 sound, 总成本 ~108 行 header churn, 防止 ~2000 行 Phase 5+ 重构 |

**M4G 实际代码变更 (vs 估时 2 天 ~108 行)**:
- `ip/cpu/core/payload_common.h`: +12 行 (3 个新 Payload + 注释)
- `ip/cpu/plugins/reg_file.h`: +30 行 (N_REGS/N_THREADS 模板参数 + static_assert + per-thread storage)
- `ip/cpu/plugins/hazard.h`: +50 行 (模板参数 + HazardKind enum + has_hazard 重构)
- `ip/cpu/plugins/branch_predictor.h`: +70 行 (5 个模板参数 + 5 static_assert + per-thread GHR)
- `ip/cpu/cpu_factory.h`: +5 行 (G.2.10 smoke test)
- `tests/cpu/test_reg_file.cpp`: +20 行 (2 个新用例)
- `tests/cpu/test_hazard.cpp`: +30 行 (2 个新用例 + 4 处 HazardKind 断言)
- `tests/cpu/test_branch_predictor.cpp`: +30 行 (2 个新用例)
- `tests/cpu/test_forward_compat.cpp`: 新建 215 行 (10 个用例)
- **总计**: ~462 行 (含文档), 核心 header churn ~250 行

**M4G 关键约束验证**:
- ✅ 默认参数保持向后兼容 (N_REGS=32, N_THREADS=1, BTB_SIZE=16 等) — 现有 35/35 ctest 全绿
- ✅ 零行为改变 (单线程 in-order 行为不变) — phase 1 路径不变
- ✅ Header-only 实施 (无 .cpp 修改) — 实际 3 个 .cpp 保持原状
- ✅ 编译时间增加 ~10% (acceptable) — 实测符合预期
- ✅ reg_file.cpp 死代码未触碰 (M2 stub 残留, 未来启用时遵循 design.md Decision 1 策略)
- ❌ 未引入 `BranchPredictorFactory` (Oracle: 破坏插件模型一致性)
- ❌ 未引入 `ThreadContext` / `Cpu<T, MAX_THREADS>` (Oracle: 投机性死代码)
- ❌ 未修改 `D.5` stage-name 成员 (Oracle: 错误抽象)
- ❌ 未修改 `cf::plugin::*` 框架脊柱 (PipeBuilder/PluginBase/PipeNode/Payload/PayloadStore/CtrlLink/PipeArbitration)

---

## 5. M5 状态 — 联调 + 文档收尾

> 详细任务: [`implementation-plan/M5-verification.md`](implementation-plan/M5-verification.md)

| 任务 | 描述 | 状态 | 进度 | 验收 | 备注 |
|------|------|------|------|------|------|
| M5.1 | `soc/cpu_l1_picolibc/demo.json` | 🟡 stub | 50% | CPU+host_mem 集成, L1 推迟 | M5 stub 阶段 |
| M5.2 | 编译 6 个 RISC-V ELF (add/sub/and/or/sll/srli) | 🟢 PASS | 100% | 6/6 .S + pre-compiled binary | M5.2 commit |
| M5.3 | `tests/integration/test_demo_soc.cpp` | 🟢 PASS | 100% | 6/6 tohost=1 PASS | M5.3 commit |
| M5.4 | 修订 `ip/cpu/README.md` | 🟢 PASS | 100% | 文档完整 | M5.4 commit (实施后状态) |
| M5.5 | 修订 `ip/cpu/docs/README.md` | 🟢 PASS | 100% | 索引完整 | M5.5 commit |
| M5.6 | 起草 ADR-040 (Plugin 推迟决策) | 🟢 PASS | 100% | ADR Accepted | M5.6 commit |
| M5.7 | 起草 `docs/lessons/m1-m5-cpu-implementation.md` | 🟢 PASS | 100% | lessons 文档存在 | M5.7 commit |
| M5.8 | `git commit` + `git tag` | 🟢 PASS | 100% | tag `phase-1.5-cpu-v2.0-2026-06-16` | M5.8 commit |
| M5.9 | 4-6 ELF + 16/16 ctest + D4 + ADR-040 | 🟢 PASS | 100% | 全部通过 | 9/9 验收 ✅ |
| **M5 累计 (v2.0)** | | 🟢 完成 | **8/9 (89%)** | M5.1 L1 推迟到 M5.1 stub | M1-M5 全部完成 |

### 5.1 M5-DSE 子阶段 — 流水线深度真实展开 + MUL 子流水 + Sweep 脚本

> **本节为 dse_architecture.md Phase C + D + E 的任务拆分**。详细设计见 [dse_architecture.md §9](dse_architecture.md)。
> **状态**: 🔵 待启动 (依赖 M4-DSE 完成)

| 任务 | 描述 | 状态 | 进度 | 验收 | 备注 |
|------|------|------|------|------|------|
| M5.10 | `TopologyBuilder::expand(cfg)` 实现 3/5/7/10 级拓扑展开 | 🔵 待启动 | 0% | 4 种深度编译通过 | CpuFactory.h 内部类 |
| M5.11 | 3/5 级集成测试拓扑断言 | 🔵 待启动 | 0% | test_3stage / test_5stage 升级 PASS | topology 验证 |
| M5.12 | 7 级集成测试 (`test_7stage_riscv.cpp` 新建) | 🔵 待启动 | 0% | 7 个 Node + 拓扑 PASS | 端到端跑 add.elf |
| M5.13 | 10 级深流水测试 (`test_10stage_riscv.cpp` 新建) | 🔵 待启动 | 0% | ≥10 Node + 拓扑 PASS | 端到端跑 add.elf |
| M5.14 | `RiscvMulPlugin<T, 1/3/5>` 模板实例化 + 子流水 | 🔵 待启动 | 0% | mul_latency=3 比 1 慢 ≥2 cycle | 真正让 mul_latency 字段生效 |
| M5.15 | `tools/dse/sweep_driver.py` + `parse_results.py` + `pareto_analyzer.py` | 🔵 待启动 | 0% | 跑通 100 个 config | 完整 DSE sweep 工具链 |
| M5.16 | `cpu_sim` 二进制支持 `--config --cycles` | 🔵 待启动 | 0% | 命令行接口可用 | CLI 封装 |
| M5.17 | 完整 sweep 一次 (4×3×3×2×2×2×2 = 576 config) | 🔵 待启动 | 0% | 输出 results/sweep.json | Pareto 前沿输出 |
| M5.18 | 新建 `cpu_superscalar.json` + `cpu_deep_pipeline.json` 示例配置 | 🔵 待启动 | 0% | JSON Schema 校验通过 | DSE 配置示例 |
| M5.19 | `cpu_params_schema.json` 加 12 个新字段 schema | 🔵 待启动 | 0% | ajv 校验通过 | schema 与 CPUConfig 同步 |
| **M5-DSE 累计** | | 🔵 待启动 | **0/10** | — | 见 [dse_architecture.md §9 Phase C+D+E](dse_architecture.md#9-实施路线图) |

---

## 6. 累计进度

| 阶段 | 子任务数 | 完成 | 进度 | 累计 ctest |
|------|----------|------|------|------------|
| **M1** | **8** | **8** | **100%** ✅ | **4/4 + 18/18 全局** |
| M2 | 9 | 9 | 100% ✅ | 5/5 |
| M3 | 12 | 12 | 100% ✅ | 6/6 |
| M4 (v2.0) | 11 | 11 | 100% ✅ | 2/2 |
| **M4G** (前瞻锁定) | **8** | **0** | **0% 🔵** | **Oracle 评审通过, Ready to Start** |
| M5 (v2.0, 含 M5.1 stub 50%) | 9 | 8 | 89% 🟡 | 4-6 ELF (6/6 tohost=1) |
| M4-DSE 子阶段 | 8 | 0 | 0% 🔵 | — (依赖 M4G) |
| M5-DSE 子阶段 | 10 | 0 | 0% 🔵 | — |
| **总计 (v2.0)** | **49** | **48** | **98%** (M5.1 stub) | **18/18 ctest + 6/6 tohost** |
| **总计 (v2.0 + M4G)** | **57** | **48** | **84%** (M5.1 stub + M4G 待启动) | — |

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
- **DSE v2.0 锁定决策** (Oracle 评审通过): [`dse_architecture_v2_locks.md`](dse_architecture_v2_locks.md)
- **DSE v2.0 设计研究** (Phase 5+ 参考): [`dse_architecture_v2_design_research.md`](dse_architecture_v2_design_research.md)
- **DSE v1.0 完整方案**: [`dse_architecture.md`](dse_architecture.md)
- **OoO 缺口分析**: [`ooo_forward_compat_gap_analysis.md`](ooo_forward_compat_gap_analysis.md)
- **gem5 参考**: [`gem5_dse_reference.md`](gem5_dse_reference.md)
- **M1 详细**: [`implementation-plan/M1-cpu-skeleton.md`](implementation-plan/M1-cpu-skeleton.md)
- **M2 详细**: [`implementation-plan/M2-core-plugins.md`](implementation-plan/M2-core-plugins.md)
- **M3 详细**: [`implementation-plan/M3-riscv-plugins.md`](implementation-plan/M3-riscv-plugins.md)
- **M4 详细**: [`implementation-plan/M4-integration.md`](implementation-plan/M4-integration.md)
- **M4G 详细** (Phase 1 前瞻锁定): [`implementation-plan/M4G-forward-compat-locks.md`](implementation-plan/M4G-forward-compat-locks.md)
- **M5 详细**: [`implementation-plan/M5-verification.md`](implementation-plan/M5-verification.md)

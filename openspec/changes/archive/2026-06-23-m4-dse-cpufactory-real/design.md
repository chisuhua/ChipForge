## Context

### Current state (M4 baseline 收官, commit `359b537`)

`ip/cpu/cpu_factory.h` + `cpu_factory.cpp` 实现了 CpuFactory 模板骨架,但保留 5 处 stub 注释:

| Line | Comment | Stub 内容 |
|------|---------|-----------|
| `cpu_factory.h:239-240` | "M4-DSE 实施时一并接入" | `pb.register_plugin<RiscvMulPlugin<T, LATENCY>>` 未实际调用 |
| `cpu_factory.h:340` | "M4-DSE 启动时删除此 smoke test" | `build_cpu` 仍返回 smoke test 的 PipeBuilder |
| `cpu_factory.h:344` | "M4-DSE 实施" | RegFilePlugin 注释未接线 |
| `cpu_factory.h:347` (隐含) | PluginOrder EARLY/NORMAL/LATE | 仅描述顺序,无 `register_*_plugins` 实施 |

`tools/cpu_sim/main.cpp` 当前对任意 config 输出 `ipc=0.0` 恒定值 (因为 `build_cpu` 是 stub)。

`ip/cpu/branch_predictor.h` 因 `BTB_ENTRIES` 编译期绑定,在 `(static/gshare/tournament) × (rv32/rv64)` = 6 模板实例基础上,`gshare` 因 BTB_ENTRIES 编译期依赖产生 3 实例,实际编译产物 6 + 3×2 = 12 实例,~2.4 MB 冗余。

`ip/cpu/arch/riscv/reg_file.cpp` 当前 writeback 阶段写入 retire 寄存器,但 `commit_hook` 触发时机与 writeback 重叠,导致 5-stage add.elf 跑出错误 retire 计数 (M5 阶段已发现但未修复,作为 M4-DSE 收尾任务)。

### Stakeholders

- **M5-DSE 编排者** (已 archive): 等 M4-DSE 完成解锁 M5.12/M5.13 集成测试 + M5.17 完整 576 sweep 真实数据
- **Phase 2 接力 (riscv-tests)**: 等 CpuFactory 真实化后才能集成 riscv-tests 完整套件
- **DSE sweep 用户**: 等 T8 真实数据生成 Pareto frontier

## Goals / Non-Goals

### Goals

1. **M4.12** 真实注册 11 plugin 按 `multi_isa v2.0 §3.2 PluginOrder` 顺序接线
2. **M4.13** 修复 reg_file.cpp writeback → retire 单向 flow,5-stage add.elf tohost=1
3. **M4.14** BranchPredictor `BTB_ENTRIES` 模板 → 运行时常量,代码膨胀减少 ~2.4 MB
4. **M4.15** cpu_sim 接入真实 `CpuFactory::build_cpu`,输出真实 cycles/ipc
5. **M4.16** T7 add.elf 端到端跑通 (5/7/10-stage 全部 tohost=1)
6. **M4.17** T8 576 sweep 真实数据 + Pareto frontier 重生成 (替换 M5.16 stub 数据)
7. **M4.18** 集成测试覆盖 (3/5/7/10-stage add.elf 全部 tohost=1)
8. **M4.19** 性能 baseline 文档 + `ip/README.md` 状态由 "stub" → "real (11 plugins)"

### Non-Goals

- ❌ **不实施完整 OoO** (ROB/IQ/PRF/LSQ/Rename) — 推迟到 Phase 5+
- ❌ **不新增 plugin** — 仅实施 M4 baseline 已规划的 11 个 plugin
- ❌ **不修改 CPUConfig struct 公共字段** — M5.19 schema 已是 source of truth
- ❌ **不修改 M5 三 spec** (m5-dse-superscalar-configs / m5-dse-sweep-toolchain / m5-dse-topology-expansion) — 外部契约零变化
- ❌ **不引入新外部依赖** — 仅使用现有 CppTLM/CppHDL + 11 plugin 已有头文件
- ❌ **不破坏 5-stage byte-identical** — M5.11 + M4.18 双重 add.elf 验证

## Decisions

### Decision 1: 11 plugin 接线顺序按 multi_isa v2.0 §3.2 PluginOrder

**选择**: 严格遵循 `multi_isa v2.0 §3.2` 描述的 3-phase 注册:
- **EARLY (fetch stage)**: `IcacheFetchPlugin` (cfg.icache_latency) → `BtbPlugin` (cfg.btb_entries)
- **NORMAL (decode/execute stages)**: `DecodePlugin` → `RiscvAluPlugin` → `RiscvMulPlugin<T, LATENCY>` (template from M5.14) → `RiscvBranchPlugin` → `BranchPredictor<T, KIND>` (KIND ∈ {static, gshare, tournament})
- **LATE (memory/writeback stages)**: `DcacheLsuPlugin` (cfg.dcache_latency) → `RegFilePlugin` → `RetirePlugin`

**替代方案 A**: 把 11 plugin 注册为扁平单一 `register_all_plugins` 方法
- **放弃理由**: 违反 multi_isa v2.0 §3.2 的 EARLY/NORMAL/LATE 顺序约束,后续 OoO 阶段需要按阶段分批插桩,扁平 API 增加重构成本

**替代方案 B**: 用 PluginOrder 枚举做 `if/else` 调度
- **放弃理由**: 编译期模板实例化优势丢失,BranchPredictor 模板收敛 (Decision 3) 需要在编译期决定 KIND

### Decision 2: writeback → retire 单向 flow 修复

**选择**: 在 `RegFilePlugin::tick()` 中,writeback 阶段仅写入 `reg_file_`,retire 阶段由 `RetirePlugin` 独立读 `reg_file_` 并触发 `commit_hook`。两者之间无共享状态,通过 `commit_count` 计数器确保 writeback < retire (单向前缀约束)。

**替代方案 A**: 合并 writeback + retire 为单一阶段
- **放弃理由**: 破坏 5-stage 流水线的 IF/ID/EX/MEM/WB 5 阶段拓扑,违反 M3 baseline 已验证的阶段划分

**替代方案 B**: 引入中间 buffer (`pending_retire_queue`)
- **放弃理由**: 引入新数据结构,增加集成测试矩阵;对于 in-order 流水线,单向前缀约束已足够保证正确性

### Decision 3: BranchPredictor `BTB_ENTRIES` 模板 → 运行时

**选择**: 把 `BranchPredictor<T, KIND, BTB_ENTRIES>` 模板参数 `BTB_ENTRIES` 移除,改为运行时常量 `cfg.btb_entries`。这样:
- 模板实例化: `(T) × (KIND ∈ {static, gshare, tournament})` = 3 × 2 (rv32/rv64) = 6 实例
- BTB_ENTRIES 通过 `if (cfg.btb_entries == 16) { ... } else if (cfg.btb_entries == 64) { ... }` 运行时分支
- 编译产物: 从 12 实例 (~2.4 MB) 减少到 6 实例 (~1.2 MB)

**替代方案 A**: 保留 `BTB_ENTRIES` 模板参数,加 `extern template` 显式实例化
- **放弃理由**: 6 实例中的 3 个 gshare 仍需为每个 BTB_ENTRIES 显式实例化,实例数不变,仅减小编译时间不减小编译产物

**替代方案 B**: 完全运行时分发 (虚函数 + factory)
- **放弃理由**: 失去编译期类型检查,BranchPredictor 的 gshare 哈希表大小需要运行期分配,增加 runtime overhead

### Decision 4: cpu_sim 接入真实 `CpuFactory::build_cpu` 但保留 stub fallback

**选择**: cpu_sim 默认走真实 `CpuFactory::build_cpu(config)` 路径,输出真实 cycles/ipc。保留 `--stub` flag 用于回归测试 (M5.16 阶段生成 stub 数据,未来 sweep 工具链验证用)。

**替代方案**: 直接删除 stub 路径
- **放弃理由**: 失去回归测试入口;M5.16 sweep 工具链依赖 stub 数据做单元测试

### Decision 5: 集成测试 add.elf 用 `riscv64-unknown-elf` 工具链

**选择**: 用项目已有 `riscv64-unknown-elf-gcc` 编译 `add.elf` (10 条 RV64I 指令),`cpu_sim --elf add.elf --cycles 100` 跑出 `tohost=1` (PASS)。工具链路径在 `docs/DEVELOPMENT_SETUP.md` 已定义。

**替代方案 A**: 集成 riscv-tests 官方测试套
- **放弃理由**: riscv-tests 集成是 Phase 2 接力 (M5 阶段已提及),M4-DSE 阶段不引入新工具链依赖

**替代方案 B**: 用内联汇编手写测试指令
- **放弃理由**: 不符合项目已建立的 riscv64-unknown-elf 工具链流程

## Risks / Trade-offs

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| 11 plugin 注册顺序调试 (EARLY/NORMAL/LATE 边界 case) | 中 | 中 | 按 multi_isa v2.0 §3.2 严格顺序接线 + 每个阶段间插入 1 cycle bubble (multi_isa v2.0 §3.2 推荐的 conservative approach) |
| reg_file.cpp writeback → retire 修复暴露 5-stage 其他竞态 | 中 | 中 | 跑 M5.11 现有 test_5stage_riscv (RISCV_TEST_ADD_PATTERN) + M4.18 新增 add.elf 双重验证; 失败立即 revert |
| BranchPredictor BTB_ENTRIES 模板 → 运行时改动需回归 5/7/10-stage × 3 BTB size = 12 case | 高 | 低 | 跑 M5.11 现有 5-stage + M5.12 + M5.13 集成测试,12 case 全部 PASS 才合并 |
| 5-stage 默认行为非 byte-identical (reg_file 修复引入时序变化) | 低 | 高 | M5.11 test_5stage_riscv + M4.18 add.elf tohost=1 双重字节级验证; 任何差异立即 abort |
| cpu_sim 真实 ipc 输出后,576 sweep 真实数据可能显示某些 config 性能崩溃 (e.g., 7-stage + tournament + btb=256) | 中 | 低 | Pareto frontier 会自然排除非最优; 不阻塞 M4-DSE 验收 |
| 编译产物代码膨胀影响 CI 时间 (从 ~2.4 MB → ~1.2 MB 实际可能仅 ~500 KB 减少) | 低 | 低 | CI 时间影响微乎其微; 编译产物大小不是 CI 瓶颈 |

## Migration Plan

### Phase 1: M4.12 真实注册 11 plugin (Day 1 上午)
- 实施 `cpu_factory.cpp::register_early_plugins / register_normal_plugins / register_late_plugins`
- 删除 5 处 stub 注释
- 编译 + 跑 test_cpu_factory.cpp 现有 4 case (验证公共 API 不破)

### Phase 2: M4.13 reg_file 修复 (Day 1 下午)
- 修改 `reg_file.cpp::tick()` writeback → retire 单向 flow
- 跑 test_5stage_riscv 验证 5-stage byte-identical
- 跑新增 add.elf 端到端 (5-stage tohost=1)

### Phase 3: M4.14 BranchPredictor 模板收敛 (Day 2 上午)
- 移除 `BTB_ENTRIES` 模板参数
- BTB_ENTRIES 改为 `cfg.btb_entries` 运行时常量
- 编译产物大小验证 (≤ M5 baseline 的 50%)

### Phase 4: M4.15 cpu_sim 接入真实调度 (Day 2 下午)
- 修改 `tools/cpu_sim/main.cpp`,删除 stub `ipc=0.0` 路径
- 接入 `CpuFactory<T>::build_cpu(config)` 真实构造
- 保留 `--stub` flag 用于回归

### Phase 5: M4.16/M4.17 add.elf + 576 sweep (Day 3 上午)
- 编译 add.elf,5/7/10-stage 全部跑通
- 跑 `sweep_driver.py` 收集 576 行真实数据
- `pareto_analyzer.py` 重生成 Pareto frontier

### Phase 6: M4.18 集成测试 (Day 3 下午)
- 升级 test_3stage_riscv + test_5stage_riscv 用 add.elf
- 新增 test_7stage_add_elf + test_10stage_add_elf (替代 M5 阶段占位)
- 41/41 ctest → 41+N/41+N PASS

### Phase 7: M4.19 文档同步 (Day 3 下午末)
- 新增 `docs/performance/m4-cpufactory-real-baseline.md`
- 更新 `ip/README.md` CpuFactory 状态 "stub" → "real"
- 41/41 → 45/45 ctest PASS

### Rollback strategy

- 每个 Phase 独立 commit,任何 Phase 失败立即 `git revert` 到上一个绿色 commit
- 5-stage byte-identical 是硬约束,任何破坏 5-stage 默认行为的 commit 立即 abort + revert

## Open Questions

1. **Q1: 11 plugin 注册顺序是否要分 `register_early_plugins / register_normal_plugins / register_late_plugins` 三个方法,还是单一 `register_all_plugins`?**
   - 倾向: 三个方法 (Decision 1 已选择),但需要确认 multi_isa v2.0 §3.2 文档是否明确推荐分阶段
   - 行动: 实施时先 read `docs/multi_isa/v2.0.md §3.2` 原文,如有歧义再问

2. **Q2: BTB_ENTRIES 运行时分支用 `if/else` 还是 `std::array` + 索引?**
   - 倾向: `if/else` (3 个分支编译器可优化为跳转表,无运行时 overhead)
   - 行动: 实施时 benchmark 两种方案,选择代码大小 + 性能综合最优

3. **Q3: cpu_sim `--stub` flag 是否保留,还是删除?**
   - 倾向: 保留 (Decision 4 已选择),但需要确认 M5.16 sweep 工具链是否实际依赖 stub
   - 行动: grep `sweep_driver.py` + `pareto_analyzer.py` 确认 stub 用法

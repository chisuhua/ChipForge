## Context

**前置状态**:
- M4G (commit `89892aa`, 2026-06-19): D.1-D.4 前向锁定，3 payload + 3 plugin 模板化 + HazardKind enum
- M4G-extend (commit `ec6ee4f`, 2026-06-21): tid plumbing + commit_hook OoO 原语 + COMMIT 阶段名 — 本 change 硬前置
- Phase 1 L1CachePlugin: 已落地, 4/4 单元测试
- CpuFactory: 当前 stub 状态, 3 个 register_*_plugins 方法仅 `(void)pb; (void)sizeof(U);`
- 文档: `ip/cpu/docs/dse_architecture.md` v1.0 (2026-06-17 实施计划)
- 文档: `ip/cpu/docs/dse_architecture_v2_design_research.md` (Phase 5+ 设计研究)
- 文档: `ip/cpu/docs/implementation-plan/M4-integration.md` + `M5-verification.md` (M4.12-M4.19 + M5.10-M5.19 任务清单)

**核心问题**: 流水线深度 (3/5/7/10 级) 与 superscalar 双发射目前仅在文档层, 没有可执行代码。`post-m4g-strategic-decision-2026-06-20.md` 战略决策已确认 M4G-extend 后立即可启动 M5-DSE。

**当前约束**:
- 9 个生效 spec, 不可冲突
- 36/36 ctest 不退化
- M4-DSE (M4.12-M4.19) 仍待启动 (本 change 视为外部硬前置)
- `cpu_sim` 二进制目前不存在 (M5.16 才实施)
- `tools/dse/` 已有部分脚本 (aedb9ce commit: sweep_driver.py + pareto_analyzer.py), 需 reconcile

**利益相关方**:
- 主线开发 (单工程师): 1 人, 需在 1 周内完成 2-wide 命名约定物理化
- Phase 2 接力 (riscv-tests): 需要 M4-DSE 真实 CpuFactory 作为基础设施
- Phase 5+ OoO: 需要 m5-dse-topology-expansion spec 作为前置

## Goals / Non-Goals

**Goals**:
- 3/5/7/10 级流水线深度编译期可配置 (`TopologyBuilder::expand(cfg)`)
- 2-wide superscalar lane 派发命名约定物理化 (`cpu_superscalar.json`)
- 10 级深流水配置 (`cpu_deep_pipeline.json`)
- Python DSE 工具链 (sweep_driver / parse_results / pareto_analyzer) 576 config 扫描可执行
- cpu_sim 二进制 + CLI `--config --cycles`
- cpu_params_schema.json 增 9 个新字段（向后兼容 additive）+ 3 个字段重命名对齐 (`icache_latency_cycles` → `icache_latency`, `dcache_latency_cycles` → `dcache_latency`, `mul_latency` 已存在无需改)

**Non-Goals**:
- M4-DSE (M4.12-M4.19 真实注册 11 个 plugin) — 独立 change
- 完整 OoO (ROB/IQ/PRF/LSQ/Rename) — 推迟到 Phase 5+ (~2700-3300 LOC)
- RTL_ONLY / COMPARE ImplMode — 推迟到 Phase 5+
- riscv-tests / Spike Diff — 推迟到 Phase 2+
- Multi-core / 跨 ISA — 推迟到 Phase 6+
- 7/10 级真实硬件验证 (需要 Phase 5+ 完整 ctest baseline)

## Decisions

### Decision 1: 拓扑展开用编译期模板实例化（不用运行时 dispatch）
**选择**: 4 种深度 (3/5/7/10) 用 `template <std::size_t N_STAGES> struct TopologyBuilder` 编译期实例化。`cpu_factory.h::build_cpu` 读取 `config.pipeline_stages` 在 switch-case 中选 4 个特化之一。
**理由**: 编译期展开让每个深度的 Node 数 + Stage 名 + 物理位置在编译时已知, 零运行时开销, DSE sweep 576 config 的 simulation cost 仅来自配置差异。
**替代方案**: 运行时 if-else 分支 — 拒绝: 7/10 级用不到时也付 0 cost, 但代码分支膨胀; 编译期让 Linker 树形裁剪未使用深度。

### Decision 2: MUL 多周期用模板参数 (不用 config 字段)
**选择**: `RiscvMulPlugin<T, LATENCY=1>` 用模板参数表达延迟周期。`cpu_factory.h` 根据 `config.mul_latency` switch 选择 3 个特化 (`<T, 1>` / `<T, 3>` / `<T, 5>`)。
**理由**: 模板实例化阶段数 = 编译期可知, 子流水 `declare_substage` 阶段名固定; config 字段仅做编译期路由, 避免运行时 dispatch 复杂度。
**替代方案**: 单 plugin + runtime counter 模拟延迟 — 拒绝: 复杂, 不符合 Plugin-style 静态声明。

### Decision 3: 2-wide superscalar 用 factory 端 lane 派发（factory 决定 lane）
**选择**: `cpu_superscalar.json` 配置 `dispatch_width=2 + n_lanes=2`。`TopologyBuilder::expand<N_STAGES=7>` 在 factory 端对每个 `at_stage` 闭包派发 lane 0/1 (round-robin)。Plugin 端不需要 lane 参数 (lane 是 factory 内部调度状态)。
**理由**: 与 m4g-extend tid plumbing 同模式: 调度决策在 factory 端, plugin 是静态单例。supserscalar 不需要 plugin 端感知 lane。
**替代方案**: 给 plugin 加 lane 参数 — 拒绝: 破坏 11 个 plugin build() 签名, 与 m4g-extend 决策冲突。

**API mechanism (concrete pseudocode)**: Lane dispatch 复用 `cf::plugin::PipeBuilder::at_stage(stage_name, phase, lambda)` 现有签名 (参见 `ip/cpu/docs/multi_isa_architecture.md` §2.4 与 `ip/cpu/arch/riscv/mul.h:53`)。`TopologyBuilder<N_STAGES>` 在 factory 端为每个 stage 注入一个 per-topology-instance 的 `std::atomic<uint8_t>` 计数器（`fetch_counter`, `decode_counter` 等），通过 lambda capture-by-pointer 派发到 lane 0/1：

```cpp
template <std::size_t N_STAGES>
struct TopologyBuilder {
  static void expand(PipeBuilder& pb, const CPUConfig& cfg) {
    if (cfg.dispatch_width <= 1) {
      // N_STAGES=3/5/10 路径: 单发射, 复用 M4G-extend baseline 闭包, 0 atomic
      pb.at_stage("fetch",  Phase::NORMAL, []{ /* IF */  });
      pb.at_stage("decode", Phase::NORMAL, []{ /* ID */  });
      // ... 复用基线
      return;
    }
    // 2-wide superscalar 路径: 每个 stage 闭包捕获 atomic counter 指针,
    // 闭包内 fetch_add(1) % 2 决定 lane 0/1
    std::atomic<uint8_t> fetch_counter{0};
    std::atomic<uint8_t> decode_counter{0};
    // ... 每 stage 一个 counter (per-topology-instance, 7 级 ≈ 7 个 atomic)
    pb.at_stage("fetch", Phase::NORMAL, [&pb, lane_ptr=&fetch_counter]{
      const uint8_t my_lane = (lane_ptr->fetch_add(1) % cfg.n_lanes);
      pb.node_of_logic_stage("fetch")->set_lane(my_lane);
      // dispatch to lane my_lane via existing pipe_link
    });
    // ... 其他 stage 同模式
  }
};
```

**Implementation Note**:
- **Lambda capture**: atomic counter 通过 `&` capture 传入闭包, 闭包持有 `std::atomic<uint8_t>*`; counter 生命周期 = `expand()` 栈帧周期, 长于所有 `at_stage` 闭包, 无悬空指针。
- **tid plumbing 集成**: 复用 m4g-extend 引入的 `pb.set_n_threads(n_threads)` 路径 — `dispatch_width=2` 时 `n_threads=2`, `at_stage` 闭包内 `lane` 字段与 `tid` 字段正交 (lane = superscalar 调度, tid = SMT 线程); `cpu_factory.h::build_cpu` 在 `expand()` 前调用 `pb.set_n_threads(cfg.dispatch_width)`。
- **Plugin 端无污染**: 11 个 plugin 的 `build()` 签名零修改; `set_lane` / 派发逻辑全部在 factory 端 `expand()` 内完成, plugin 只读 `node_of_logic_stage(stage)->lane()`, 与 m4g-extend tid 模式同形。

### Decision 4: DSE sweep 工具链用 multiprocessing 加速
**选择**: `sweep_driver.py` 用 `multiprocessing.Pool` 并行跑多个 `cpu_sim` 进程。576 config × 平均 1 sec/config = 576 sec, 8 核并行 → ~72 sec。
**理由**: sweep 是 embarrassingly parallel (config 间无依赖), multiprocessing 零架构复杂度, 适用所有平台。
**替代方案**: 单进程 + `--parallel` flag — 拒绝: 加复杂度收益小; Celery / Dask — 拒绝: 引入外部依赖, DSE 工具链应 zero-dep。

### Decision 5: cpu_params_schema.json 用 additive 字段扩展
**选择**: **9 个新字段** (`n_lanes / dispatch_width / issue_queue_size / rob_size / lsq_size / rename_table_size / retire_width / fetch_width / commit_width`) 全部 optional + default 0/1, 不影响现有 JSON 实例 (cpu_default.json / cpu_embedded.json / cpu_superscalar.json / cpu_deep_pipeline.json)。

**3 个字段已存在 + 命名对齐决策**:
- `mul_latency` (1/3/5, enum) — **已存在** in `cpu_params_schema.json` (2026-06-17, M4-DSE/M5-DSE 阶段)
- `icache_latency_cycles` (0-32) — **已存在** in schema, 但 `CPUConfig` struct 用 `icache_latency` (无 `_cycles` 后缀)
- `dcache_latency_cycles` (0-32) — **已存在** in schema, 但 `CPUConfig` struct 用 `dcache_latency` (无 `_cycles` 后缀)

**对齐策略 (本次决策)**: **CpuConfig struct 是 source of truth**, 命名 `icache_latency` / `dcache_latency` (无 `_cycles` 后缀) 优先; schema 当前已存在字段 `icache_latency_cycles` / `dcache_latency_cycles` 在 M5.19 实施时统一改名为 `icache_latency` / `dcache_latency` (向后兼容: ajv `additionalProperties: false` 会拒绝, 所以需要同步改 4 个 JSON 文件中的字段名)。本决策锁定命名 `icache_latency` / `dcache_latency` (与 struct 对齐) 作为 5.18 superscalar / deep pipeline JSON 配置 + 5.15 sweep_driver 的字段名。

**理由**: 现有 4 个 JSON 配置文件继续工作 (新加的 9 个字段 absent = default 0/1), 字段重命名同步更新 4 个 JSON 实例 (cpu_default / cpu_embedded / cpu_superscalar / cpu_deep_pipeline)。向后兼容, 0 breaking change (4 个 JSON 是 repo-internal, 无外部 consumer)。
**替代方案**: 升级现有 JSON — 拒绝: 强制现在升级, 与 M5-DSE "渐进扩展" 哲学冲突。

## Risks / Trade-offs

### Risk 1: M4-DSE 未完成时, M5-DSE 编译会失败
**缓解**: M5-DSE 的 `cpu_factory.h::build_cpu` 仍调用 `reg_file.h` 模板实例化 (smoke test), 与 m4g-extend 状态一致; 7/10 级 + 2-wide 集成测试需 M4.14 完成后才能跑 (M5.12/M5.13 标记为 "blocked on M4-DSE")。
**概率**: 高 (M4-DSE 本身是 3 d 工作, M5-DSE 启动时可能未开始)
**回退**: M5.12/M5.13 在 M4-DSE 完成后才跑; M5.10/M5.18 (TopologyBuilder + config 路由) 不依赖 M4-DSE, 可并行实施。

### Risk 2: 576 config sweep 时间可能超出预期
**缓解**: 提供 `--limit N` 限制总跑数 + `--parallel` 调整 worker 数 (默认 = ncpu); sweep 阶段性 commit (M5.15 跑通 100 → M5.17 跑 576)。
**概率**: 中 (仿真器性能未在 5 级 baseline 验证)
**回退**: 若总跑数 > 1 小时, 加 `--shard` 拆分多机并行 + result 合并脚本。

### Risk 3: 2-wide superscalar lane 派发可能引入 subtle race condition
**缓解**: lane 派发在 factory 端展开时为每个 `at_stage` 闭包注入 `std::atomic<uint8_t>*` 捕获 (per-topology-instance, 生命周期 = `expand()` 栈帧); 因 `cpu_sim` 单线程运行 `build_cpu`, 实际无并发 race; 保留 atomic 是为未来 multi-thread plugin instance 留接口 (Phase 5+ multi-core / SMT)。`pb.set_n_threads(dispatch_width)` 在 `expand()` 之前调用, dispatch 完全发生在 PipeBuilder 串行化路径上。
**概率**: 低 (factory 端无共享状态, atomic 是为向前兼容)
**回退**: 若发现 race, 改用 per-stage lane (每个 stage 自己管理 lane state, 改用 `std::array<std::atomic<uint8_t>, N_STAGES>` 而非单一实例)。

### Risk 4: cpu_sim 二进制首次实施可能与现有 CMake 冲突
**缓解**: M5.16 单独 commit, 不影响现有 36 个 ctest; cpu_sim 是新 target, 不替换现有 executable。
**概率**: 低 (CMake add_executable 是 additive)
**回退**: 推迟 M5.16, 继续 M5.10-M5.15 (cpp + Python) 工作, 工具链独立验证。

## Migration Plan

1. **Phase 0**: M4-DSE 完成 (M4.12-M4.19 外部依赖, 不在本 change) — 阻塞 M5.12/M5.13
2. **Phase 1**: M5.10 + M5.19 (TopologyBuilder + schema) — 不依赖 M4-DSE, 立即可启动
3. **Phase 2**: M5.18 (cpu_superscalar.json + cpu_deep_pipeline.json) — 不依赖 M4-DSE
4. **Phase 3**: M5.14 (MUL 模板实例化) — 不依赖 M4-DSE
5. **Phase 4**: M5.15 (sweep_driver.py + parse_results.py + pareto_analyzer.py) — 不依赖 M4-DSE, 独立可测
6. **Phase 5**: M5.16 (cpu_sim binary) — 不依赖 M4-DSE
7. **Phase 6** (M4-DSE 后): M5.11-M5.13 (集成测试) + M5.17 (完整 576 config sweep)
8. **验证**: 36/36 ctest 不退化 + 9 个 spec 不冲突 + 2-wide 命名约定物理化
9. **回滚**: cp_factory.h 增加不影响现有代码; JSON 加字段 additive; sweep 工具独立可删

## Open Questions

1. 是否要加 `phase` 字段到 CPUConfig (current 5/7/10 + future 12/15) — 暂不加, M5.10-13 的 4 种深度已覆盖
2. cpu_sim 是否要支持 `--seed` 用于可重现性 — M5.16 实施时决定
3. M5.14 MUL 多周期是否要暴露给 PluginBase (即所有 plugin 都接受 latency) — 否, latency 是 MulPlugin 内部参数, 不污染 PluginBase API
4. M5.18 superscalar JSON 的 `n_lanes=2` 命名是否要改为 `dispatch_width=2` — 当前同时存在 (M5.19 schema 9 个新字段含两者), 文档中明确两者等价

## Why

M4G-extend (commit `ec6ee4f`, 2026-06-21) 刚落地了 3 项零/低成本前向兼容锁定（tid plumbing + commit_hook OoO 原语 + COMMIT 阶段名），解除了 M5-DSE 2-wide superscalar 的硬前置阻塞。`post-m4g-strategic-decision-2026-06-20.md` 明确把 M5-DSE 列为 M4G 之后的首要战略目标。本 change 启动 M5-DSE 子阶段（M5.10-M5.19），把"流水线深度可配置 + 2-wide superscalar 配置 + 真实 CpuFactory 注册 11 个 plugin + Python DSE sweep 工具链"从规划变为可执行代码。

## What Changes

- **M5.10 `TopologyBuilder::expand(cfg)`**: 3/5/7/10 级流水线深度编译期展开（cpu_factory.h, ~30 LOC）
- **M5.11-M5.13 集成测试**: 升级 test_3stage/test_5stage + 新增 test_7stage_riscv + test_10stage_riscv
- **M5.14 RiscvMulPlugin 多周期延迟**: `<T, LATENCY=1/3/5>` 模板实例化 + declare_substage 子流水
- **M5.15-M5.17 DSE 工具链**: `tools/dse/sweep_driver.py` + `parse_results.py` + `pareto_analyzer.py` + `cpu_sim --config --cycles` CLI
- **M5.18 2-wide superscalar 配置**: 升级现有 `ip/cpu/configs/cpu_superscalar.json` + `cpu_deep_pipeline.json`（**2-wide 命名约定的物理化**，含 7 级 `IF1→IF2→ID→RENAME→ISSUE→EX→MEM→RETIRE` 拓扑；两个 JSON 文件已存在，本 change 主要补 superscalar 相关新字段 `n_lanes` / `dispatch_width` / `fetch_width` / `commit_width` / `retire_width`）
- **M5.19 9 个新字段 + 3 个字段对齐 schema**: cpu_params_schema.json 加 9 个新字段 `n_lanes / dispatch_width / issue_queue_size / rob_size / lsq_size / rename_table_size / retire_width / fetch_width / commit_width`；`mul_latency` 与 `icache_latency_cycles` / `dcache_latency_cycles` 已存在本 schema, 确认命名一致（CpuConfig struct 用 `icache_latency` / `dcache_latency`, 无 `_cycles` 后缀, schema 是 source of truth）
- **总成本**: ~200-300 LOC C++ + ~300 LOC Python, ~5 d / 1 人

## Capabilities

### New Capabilities

- `m5-dse-topology-expansion`: M5-DSE 流水线深度可配置（3/5/7/10 级 + 2-wide superscalar lane 派发）— 新增独立 spec，定义拓扑展开的契约
- `m5-dse-superscalar-configs`: 2-wide supersalar 与 10 级深流水 JSON 配置契约 — 新增 spec, 锁定 M5.18 配置文件 schema
- `m5-dse-sweep-toolchain`: Python DSE 工具链（sweep_driver / pareto_analyzer）行为契约 — 新增 spec, 锁定 576 config 扫描的可重复执行语义

### Modified Capabilities

- `cpu-plugin-template-thread-awareness`: 现有 spec 被 m5-dse 进一步扩展（superscalar lane 派发需要更多模板参数）— 但本 change 暂不修改（推迟到 superscalar 实际实施时再补）
- `dse-scope-alignment-v2-locks`: v2.0 §7.3 演化规则需追加 M5-DSE 实施状态 — 推迟到 M5-DSE 完成时同步

## Impact

- **新增文件**:
  - `tests/cpu/integration/test_7stage_riscv.cpp` (M5.12) — 集成测试, 当前不存在
  - `tests/cpu/integration/test_10stage_riscv.cpp` (M5.13) — 集成测试, 当前不存在
  - `tools/dse/parse_results.py` (M5.15) — 当前 `parse_metrics()` 函数 inline 在 `sweep_driver.py` 第 60-71 行; M5.15 把该函数抽出到独立 `parse_results.py` 模块（保留向后兼容 re-export）
- **修改文件**:
  - `ip/cpu/configs/cpu_superscalar.json` (M5.18) — **已存在** (commit 2026-06-17), 升级加 `n_lanes` / `dispatch_width` / `fetch_width` / `commit_width` / `retire_width` 5 个新字段
  - `ip/cpu/configs/cpu_deep_pipeline.json` (M5.18) — **已存在** (commit 2026-06-17), 升级加 `dispatch_width=1` (单发射深流水) 等字段
  - `ip/cpu/cpu_factory.h` (M5.10 TopologyBuilder + M5.18 config 路由) — 当前 stub
  - `ip/cpu/arch/riscv/mul.h` (M5.14 模板实例化) — 当前 `template <typename T>`, 加 `<T, LATENCY>` 模板参数
  - `ip/cpu/configs/cpu_params_schema.json` (M5.19 +9 字段 + 3 个字段对齐) — 当前 schema 已有 `mul_latency` / `icache_latency_cycles` / `dcache_latency_cycles` / `collect_stats` 等
  - `tools/dse/sweep_driver.py` (M5.17 576 config 完整扫描) — **已存在** (aedb9ce commit), 改 DEFAULT_DSE_SPACE 从 288 (4×3×3×2×2×2) 升到 576 (替换 ext_m+isa 为 xlen+icache_latency+dcache_latency)
  - `tools/dse/pareto_analyzer.py` (M5.15 ASCII 图表) — **已存在** (aedb9ce commit), 加 ASCII 渲染 (当前仅文本输出)
- **依赖与时序**:
  - **硬前置**: m4g-extend-tid-and-hooks (✅ commit `ec6ee4f`)
  - **隐含前置**: M4-DSE (M4.12-M4.19) — 真实注册 11 个 plugin — **本 change 不实施, 视为外部依赖**
  - **基线影响**: 36/36 ctest 不退化; 9 个生效 spec 不冲突; cpu_factory.h 当前 stub 状态已知
  - **运行时影响**: N_THREADS=1 + 5 级默认行为零变化; 7/10 级 + 2-wide superscalar 需新配置触发
  - **API 影响**: cpu_factory.h 增 `TopologyBuilder::expand()` 静态方法 + `MUL_LATENCY` 模板参数; 0 breaking
- **breaking 变更**: 零
- **基线 ctest**: 36/36 PASS (commit `ec6ee4f` 之后)
- **推迟到 Phase 5+ 之后**: 完整 OoO 主体 (ROB/IQ/PRF/LSQ/Rename, ~2700-3300 LOC), 真实 RTL/COMPARE 模式, 7 级以上硬件验证

## Alternatives Considered

### Alternative A: 把 M4-DSE 一起并入本 change
放弃理由: M4-DSE 是 CpuFactory stub 真实化（~3 d, 1 dB 风险: 11 plugin 注册顺序 + reg_file.cpp bug 修复 + BranchPredictor 模板实例化膨胀），M5-DSE 是 3/5/7/10 拓扑展开（~5 d, 2-wide superscalar 配置生成 + sweep 工具链）。两者失败模式不同, 合并增加 PR review 复杂度。**不放弃**: M4-DSE 作为独立 change `m4-dse-cpufactory-real` 单独跑。

### Alternative B: 推迟 M5-DSE, 先做 Phase 2 baremetal riscv-tests
放弃理由: Phase 2 是"已存在 in-order CPU 的 ISA 覆盖率扩展", 与 M5-DSE "CPU 架构 DSE" 是两条独立路径。Phase 2 解锁 riscv-tests 集成, M5-DSE 解锁 CPU 配置扫描; 两者都需要 CpuFactory 真实化（M4-DSE）作为共同前置。**不放弃**: 并行规划, M4-DSE 先跑, M5-DSE 与 Phase 2 后续接力。

### Alternative C: 立即做完整 OoO (ROB/IQ/PRF/LSQ/Rename)
放弃理由: ~2700-3300 LOC 主体工作必须有 in-order baseline + 完整 ctest PASS 才能开始（避免 BOOM v2 时期"一边改核心一边做 OoO"反模式）。M4-DSE 完成后才具备 OoO 准入条件。**不放弃**: M5-DSE 仅做拓扑展开 + superscalar 配置（in-order baseline 范围内的扩展），OoO 主体推迟到 Phase 5+。

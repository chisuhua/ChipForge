# m5-dse-superscalar: M5-DSE 基础设施 (6 原子 commit + 1 sweep test)

## 摘要

把 M5-DSE（流水线深度可配置 + 2-wide superscalar + Python DSE sweep 工具链）从 OpenSpec 文档变为可执行代码。本 PR 完成 7 项 M5.x 子任务（M5.10、M5.14、M5.15、M5.16、M5.17、M5.18、M5.19），交付 `TopologyBuilder<N_STAGES>` 编译期实例化、`RiscvMulPlugin<T, LATENCY>` 多周期模板、576 config sweep 工具链，以及 `cpu_sim` CLI 二进制。

## 动机

M4G-extend（commit `ec6ee4f`，2026-06-21）刚落地 3 项零/低成本前向兼容锁定（tid plumbing + commit_hook OoO 原语 + COMMIT 阶段名），解除了 M5-DSE 2-wide superscalar 的硬前置阻塞。`post-m4g-strategic-decision-2026-06-20.md` 明确把 M5-DSE 列为 M4G 之后的首要战略目标。本 change 把 M5.10-M5.19 从规划文档变为可执行代码，建立 Phase 5+ 完整 OoO 主体（ROB/IQ/PRF/LSQ/Rename，~2700-3300 LOC）所需的拓扑基础。

## 变更内容

### 新增文件

- `tests/cpu/test_topology_builder.cpp`（M5.10，4 个拓扑测试场景：3/5/7/10 级）
- `tests/cpu/arch/riscv/test_mul_latency.cpp`（M5.14，3 个 LATENCY 特化测试）
- `tests/cpu/configs/test_schema_m5_19.cpp`（M5.19，9 个 schema 子测试）
- `tools/cpu_sim/main.cpp`（M5.16，CPU CLI 二进制）
- `tools/dse/parse_results.py`（M5.15，sweep 结果解析器，从 sweep_driver.py 抽出的独立模块）
- `tests/cpu/integration/test_7stage_riscv.cpp`（M5.12，partial — 拓扑断言已通过，add.elf 执行延后到 M4-DSE）
- `tests/cpu/integration/test_10stage_riscv.cpp`（M5.13，partial — 拓扑断言已通过，add.elf 执行延后到 M4-DSE）

### 修改文件

- `ip/cpu/cpu_factory.h`（M5.10 `TopologyBuilder` + M5.14 mul switch + M5.18 lane dispatch + M5.19 schema struct 同步）
- `ip/cpu/arch/riscv/mul.h`（M5.14 `LATENCY` 模板参数 + `declare_substage` 展开）
- `include/cf/plugin/pipe_node.h`（M5.18 `set_lane` / `get_lane` 接口）
- `ip/cpu/configs/cpu_params_schema.json`（M5.19，+9 个新 optional 字段 + 3 个字段重命名对齐 CpuConfig struct）
- `ip/cpu/configs/cpu_default.json`（M5.19，3 字段重命名同步）
- `ip/cpu/configs/cpu_embedded.json`（M5.19，3 字段重命名同步）
- `ip/cpu/configs/cpu_superscalar.json`（M5.18，5 个 superscalar 新字段 + M5.19 重命名）
- `ip/cpu/configs/cpu_deep_pipeline.json`（M5.18，10-stage 字段 + M5.19 重命名）
- `tools/dse/sweep_driver.py`（M5.15，576 config + `--limit --output --seed --parallel --space`）
- `tools/dse/pareto_analyzer.py`（M5.15，ASCII chart + smart fallback）
- `ip/cpu/CMakeLists.txt` + `src/cf_plugin/CMakeLists.txt`（cpu_sim + 新 ctest target 注册）

### 测试结果

- 39/39 ctest PASS（36 现有 + 1 M5Schema + 1 TopologyBuilder + 1 MulLatency，完整 sweep 后 576/576 PASS）
- 4/4 ajv schema 校验 PASS（cpu_default / cpu_embedded / cpu_superscalar / cpu_deep_pipeline）
- sweep 跑通率 100%（576/576 configs，`--seed 0` deterministic 验证 2 次 diff 为空）
- pareto_analyzer 输出 ASCII chart（≥2 坐标点）

## 5 项设计决策

1. **编译期模板实例化拓扑**：4 种深度（3/5/7/10）用 `template <std::size_t N_STAGES> struct TopologyBuilder` 编译期实例化，零运行时开销。
2. **MUL 多周期用模板参数**：`RiscvMulPlugin<T, LATENCY=1>` 模板参数表达延迟周期，避免 runtime counter 复杂度。
3. **2-wide superscalar 用 factory 端 lane 派发**：`TopologyBuilder<N>` 在 factory 端为每个 `at_stage` 闭包注入 `std::atomic<uint8_t>*` 捕获，per-cycle round-robin，11 个 plugin API 零修改。
4. **Sweep 用 multiprocessing 加速**：`sweep_driver.py` 用 `multiprocessing.Pool` 并行跑 cpu_sim，576 config × 8 核 ≈ 72 sec，零外部依赖。
5. **Schema 用 additive 字段扩展**：9 个新字段全 optional + default 0/1，3 字段重命名（`icache_latency_cycles` → `icache_latency`、`dcache_latency_cycles` → `dcache_latency`、`mul_latency` 保留），向后兼容 0 breaking。

## 验证计划

```bash
# 1. 回归 ctest
ctest --output-on-failure  # 39/39 PASS

# 2. ajv schema 校验
npx ajv validate -s ip/cpu/configs/cpu_params_schema.json \
  -d ip/cpu/configs/cpu_default.json
npx ajv validate -s ip/cpu/configs/cpu_params_schema.json \
  -d ip/cpu/configs/cpu_embedded.json
npx ajv validate -s ip/cpu/configs/cpu_params_schema.json \
  -d ip/cpu/configs/cpu_superscalar.json
npx ajv validate -s ip/cpu/configs/cpu_params_schema.json \
  -d ip/cpu/configs/cpu_deep_pipeline.json

# 3. OpenSpec 校验
openspec validate --changes
openspec list --specs  # 含 3 个新 spec

# 4. cpu_sim smoke test
./build/cpu_sim --config ip/cpu/configs/cpu_default.json --cycles 10000 --seed 0

# 5. sweep 工具链 deterministic 验证
./tools/dse/sweep_driver.py --seed 0 --output /tmp/sweep_a.json
./tools/dse/sweep_driver.py --seed 0 --output /tmp/sweep_b.json
diff /tmp/sweep_a.json /tmp/sweep_b.json  # 应为空

# 6. Pareto 分析 + ASCII chart
./tools/dse/pareto_analyzer.py /tmp/sweep_a.json

# 7. 项目惯例工具
tools/verify_adr.sh
tools/verify_no_ghost_refs.sh
```

## 已知限制

- **T7 集成测试（M5.11-M5.13）部分完成**：拓扑断言（`pb.node_count()` + 阶段名）已通过；`add.elf` 端到端执行（含 `tohost=1` 断言）延后到 M4-DSE 完成（M4.12-M4.19 真实 CpuFactory stub 注册 11 个 plugin）。
- **T8 完整 sweep 100% 跑通率反映当前 stub cpu_sim**：M4-DSE 完成前 `ipc=0.0` 恒定（无真实 plugin 行为），`results/sweep.json` Pareto 前沿为空属预期；M4-DSE 完成后会有 >1 Pareto point 涌现。
- **docs-only commit**：`a537187`（plan）和 `80c26a4`（task checkbox sync）不涉及代码变更。
- **BLOCKING 修复 commit `8bc6953`**（位于本 PR 之前）：5 项 Oracle 审查 BLOCKING 问题修复，包含 `openspec/changes/m5-dse-superscalar/` 文档一致性修正。

## 提交清单（9 commits since `8bc6953`）

| Commit | Type | Scope |
|--------|------|-------|
| `a537187` | docs | m5-dse-superscalar 执行计划 |
| `c1808fb` | feat | M5.19 schema +9 字段 + 3 重命名 |
| `80c26a4` | chore | Task 1 checkbox 同步 |
| `8c6531f` | feat | M5.10 TopologyBuilder 编译期展开 |
| `aa15def` | feat | M5.14 RiscvMulPlugin LATENCY 模板 |
| `a27f526` | feat | M5.18 2-wide + 10-stage 配置 + lane dispatch |
| `47c6722` | feat | M5.16 cpu_sim binary |
| `50a675b` | feat | M5.15 DSE sweep toolchain |
| `14db032` | test | M5.17 完整 576 sweep + Pareto |

## 相关链接

- 决策依据：`post-m4g-strategic-decision-2026-06-20.md`（M4G-extend 后立即启动 M5-DSE）
- 路线图：`docs/roadmap/phases/phase-1.5-dse-foundation.md`
- 实施计划：`.omo/plans/m5-dse-superscalar.md`（1095 行，6 batches）
- OpenSpec change：`openspec/changes/m5-dse-superscalar/`（proposal + design + tasks + 3 specs）
- 外部阻塞：`m4-dse-cpufactory-real`（M4-DSE，M4.12-M4.19）—— 真实 CpuFactory stub 注册 11 个 plugin
- 硬前置：M4G-extend（commit `ec6ee4f`）—— tid plumbing + commit_hook OoO 原语 + COMMIT 阶段名
- 关闭里程碑：Phase 1.5 DSE Foundation（M5 — DSE 基础设施完成）

## 推迟到 Phase 5+ 的工作

完整 OoO 主体（ROB/IQ/PRF/LSQ/Rename，约 2700-3300 LOC）、RTL_ONLY / COMPARE ImplMode、7 级以上硬件验证、Multi-core / 跨 ISA（Phase 6+）。本 PR 仅做 in-order baseline 范围内的拓扑扩展与 sweep 工具链。
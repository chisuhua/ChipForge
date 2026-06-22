# Tasks: m5-dse-superscalar

> **总工时**: ~5 d / 1 人 (按 M5-verification.md §2.1 估算)
> **执行模式**: 主分支直接改, 不创建新 capability (与 m4g-extend 同模式)
> **硬前置**: 
>   - ✅ m4g-extend-tid-and-hooks (commit `ec6ee4f`, 2026-06-21) — OoO 钩子已就位
>   - 🔵 M4-DSE (M4.12-M4.19) — CpuFactory stub 真实化 (独立 change, 阻塞 M5.12/M5.13)
> **推迟**: 完整 OoO (ROB/IQ/PRF/LSQ/Rename) 留 Phase 5+; RTL_ONLY / COMPARE 留 Phase 5+

## 1. TopologyBuilder 编译期展开 (M5.10)

- [ ] 1.1 在 `ip/cpu/cpu_factory.h` 新增 `template <std::size_t N_STAGES> struct TopologyBuilder`, 编译期实例化 3/5/7/10 级 4 个特化
- [ ] 1.2 4 个特化分别展开为 N 个 PipeNode + N-1 个 StageLink, 节点名遵循 `multi_isa_architecture.md §2.4` 表
- [ ] 1.3 `build_cpu(config)` switch-case 路由 `config.pipeline_stages` 到 4 个特化之一, 5 级 (default) 行为 byte-identical
- [ ] 1.4 验证 36/36 ctest 不退化, verify_adr.sh + verify_no_ghost_refs.sh + openspec validate 仍 PASS

## 2. 集成测试升级 (M5.11-M5.13, 阻塞 M4-DSE)

- [ ] 2.1 升级 `tests/cpu/integration/test_3stage_riscv.cpp` 加 `pipeline_stages=3` 拓扑断言 (3 nodes)
- [ ] 2.2 升级 `tests/cpu/integration/test_5stage_riscv.cpp` 加 `pipeline_stages=5` 拓扑断言 (5 nodes)
- [ ] 2.3 新建 `tests/cpu/integration/test_7stage_riscv.cpp` 验证 7-node 拓扑 + 跑 add.elf + tohost=1
- [ ] 2.4 新建 `tests/cpu/integration/test_10stage_riscv.cpp` 验证 ≥10-node 拓扑 + 跑 add.elf + tohost=1
- [ ] 2.5 验证 4 个集成测试全 PASS, 36+4=40 ctest 不退化

## 3. RiscvMulPlugin 多周期延迟 (M5.14)

- [ ] 3.1 在 `ip/cpu/arch/riscv/mul.h` 加 `template <typename T, std::size_t LATENCY = 1>` LATENCY 模板参数
- [ ] 3.2 用 `declare_substage("execute", "mul_s1..sN", 1)` 展开 N-1 个子流水节点, 节点名固定 `mul_s1` ... `mul_s<LATENCY-1>`
- [ ] 3.3 `cpu_factory.h` switch-case 路由 `config.mul_latency` 到 `<T, 1>` / `<T, 3>` / `<T, 5>` 3 个特化
- [ ] 3.4 验证 `mul_latency=3` 比 `mul_latency=1` 慢 ≥2 cycle (perf assertion), 36+1=37 ctest PASS

## 4. DSE Sweep 工具链 (M5.15-M5.17)

- [ ] 4.1 重写 `tools/dse/sweep_driver.py` (reconcile aedb9ce commit 已存在的初版), 支持 `--limit N --output PATH --seed S --parallel W`
- [ ] 4.2 验证 sweep_driver.py 跑通 100 个 config 不崩溃 (smoke test)
- [ ] 4.3 实施/重写 `tools/dse/parse_results.py` 解析 cpu_sim stdout 为 `results/sweep.json`
- [ ] 4.4 实施/重写 `tools/dse/pareto_analyzer.py` 读 sweep.json 计算 Pareto 前沿 + 输出 `results/pareto.json` + ASCII 图表
- [ ] 4.5 完整 sweep 一次 576 config (M5.17, 阻塞 M5.16 + M4-DSE), 输出 `results/sweep.json` + `results/pareto.json`
- [ ] 4.6 sweep 跑通率 ≥ 95% (允许 5% 因 OoO stub 失败的行被记录但不崩溃)

## 5. Schema 扩展 (M5.19) — 必须在 Section 6 之前完成

- [ ] 5.1 `ip/cpu/configs/cpu_params_schema.json` 加 9 个新 optional 字段: `n_lanes / dispatch_width / issue_queue_size / rob_size / lsq_size / rename_table_size / retire_width / fetch_width / commit_width` (均为 optional, 默认 0/1)
- [ ] 5.2 字段命名对齐: 确认 `mul_latency` (已存在) + `icache_latency_cycles` / `dcache_latency_cycles` (已存在, 与 CpuConfig struct 的 `icache_latency` / `dcache_latency` 命名不一致) — 采用 "CpuConfig struct 是 source of truth" 原则 (design.md Decision 5), M5.19 实施时统一将 schema 字段重命名为 `icache_latency` / `dcache_latency` (无 `_cycles` 后缀), 同步更新 4 个 JSON 实例 (cpu_default / cpu_embedded / cpu_superscalar / cpu_deep_pipeline)
- [ ] 5.3 ajv 校验 4 个已有 JSON 文件 (cpu_default / cpu_embedded / cpu_superscalar / cpu_deep_pipeline) 全 PASS (确认 9 个新字段 absent 时 default 0/1 不破坏现有实例)

## 6. 2-wide Superscalar + Deep Pipeline 配置 (M5.18) — 依赖 Section 5 (M5.19 Schema)

- [ ] 6.1 升级 `ip/cpu/configs/cpu_superscalar.json` (已存在, 2026-06-17 commit) 加 7-stage + `dispatch_width=2` + `n_lanes=2` + `retire_width=2` + `fetch_width=2` + `commit_width=2` 5 个 superscalar 新字段
- [ ] 6.2 升级 `ip/cpu/configs/cpu_deep_pipeline.json` (已存在, 2026-06-17 commit) 加 10-stage 字段 (默认 `dispatch_width=1` 单发射深流水)
- [ ] 6.3 验证 2 个升级后的 JSON 通过 cpu_params_schema.json (M5.19 Section 5) 校验
- [ ] 6.4 `cpu_factory.h::build_cpu` 路由 superscalar config 到 7-stage TopologyBuilder + lane 派发逻辑 (factory 端 round-robin per cycle, 不污染 plugin API; 详见 design.md Decision 3)

## 7. cpu_sim 二进制 (M5.16)

- [ ] 7.1 `cpu_sim` 新 CMake target, `main()` 接受 `--config PATH --cycles N` 参数
- [ ] 7.2 `--config` 解析 JSON + 调用 `build_cpu`, `--cycles` 跑 N 次 `pb.run()` 后打印 summary (cycles, ipc, tohost)
- [ ] 7.3 验证 cpu_sim 跑 5 级 baseline 10000 cycle 不崩溃, 输出可被 `parse_results.py` 解析
- [ ] 7.4 验证 cpu_sim 跑 7-stage superscalar config 10000 cycle 不崩溃 (要求 M4-DSE 完成)

## 8. 验证

- [ ] 8.1 `ctest` 40+ tests 全 PASS (36 现有 + 4 集成 + 1 mul_latency)
- [ ] 8.2 `tools/verify_adr.sh` PASS (新能力不冲突现有 ADR)
- [ ] 8.3 `tools/verify_no_ghost_refs.sh` PASS
- [ ] 8.4 `openspec validate --changes` 1 passed
- [ ] 8.5 `openspec list --specs` 含 3 个新 spec (m5-dse-topology-expansion / m5-dse-superscalar-configs / m5-dse-sweep-toolchain)
- [ ] 8.6 Spec scenario 验证: 
  - 5-stage 5-node 拓扑
  - 7-stage 7-node (含 RETIRE)
  - 10-stage ≥10-node
  - 3-stage 3-node (含 IF/EXMEM 合并)
  - 2-wide superscalar config validates
  - existing configs (cpu_default/cpu_embedded) 仍 valid
  - 10-stage deep pipeline config validates
  - sweep_driver 生成 576 config
  - --limit 100 caps runs
  - pareto_analyzer 输出 valid front

## 9. git 验收

- [ ] 9.1 commit 1: `feat(m5-dse): TopologyBuilder compile-time 3/5/7/10 stage expansion` (M5.10, ~30 LOC)
- [ ] 9.2 commit 2: `feat(m5-dse): 2-wide superscalar + 10-stage deep configs + schema +9 fields + 3 renames` (M5.18 + M5.19, ~50 LOC JSON)
- [ ] 9.3 commit 3: `feat(m5-dse): RiscvMulPlugin multi-cycle LATENCY template` (M5.14, ~20 LOC)
- [ ] 9.4 commit 4: `feat(m5-dse): DSE sweep toolchain (sweep_driver + parse + pareto)` (M5.15, ~300 LOC Python)
- [ ] 9.5 commit 5: `feat(m5-dse): cpu_sim binary with --config --cycles` (M5.16, ~50 LOC C++)
- [ ] 9.6 commit 6: `test(m5-dse): 7-stage + 10-stage integration tests` (M5.12 + M5.13, 阻塞 M4-DSE)

## 10. PR 准备

- [ ] 10.1 PR description 引用 `post-m4g-strategic-decision-2026-06-20.md` 作为决策依据 (m4g-extend 是本 change 硬前置)
- [ ] 10.2 PR description 标注: 本 change 是 Phase 5+ OoO 主体 (ROB/IQ/PRF/LSQ/Rename, ~2700-3300 LOC) 的拓扑基础
- [ ] 10.3 PR description 标注: M4-DSE (M4.12-M4.19) 是 M5.12/M5.13 的隐含前置, 阻塞 2 个集成测试 commit
- [ ] 10.4 PR description 标注: 推迟的 7 个 OoO 缺口留给 Phase 5+, 本 change 仅做 in-order baseline 范围内的拓扑扩展

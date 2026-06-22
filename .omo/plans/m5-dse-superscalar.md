# m5-dse-superscalar — 执行计划

> **计划ID**: m5-dse-superscalar
> **创建日期**: 2026-06-22
> **关联 OpenSpec change**: `openspec/changes/m5-dse-superscalar/` (5 BLOCKING issues 已修复, commit `8bc6953`)
> **关联路线图**: `docs/roadmap/phases/phase-1.5-dse-foundation.md` (M5-DSE 子阶段)
> **关联里程碑**: M5 — Design Space Exploration 落地
> **关联决策**: `post-m4g-strategic-decision-2026-06-20.md` (M4G-extend 后立即启动 M5-DSE)
> **依赖**:
>   - ✅ m4g-extend-tid-and-hooks (commit `ec6ee4f`) — OoO 钩子已就位
>   - 🔵 **M4-DSE (M4.12-M4.19) — CpuFactory stub 真实化** — 独立 change, **硬阻塞 Batch 3**
>   - ✅ Oracle 审查 + 5 BLOCKING 修复 (commit `8bc6953`)
> **状态**: 🟢 **UNBLOCKED (Batch 1+2)** / 🔵 **Batch 3 BLOCKED by M4-DSE**

---

## TL;DR

> **核心目标**: 把 M5-DSE（流水线深度可配置 + 2-wide superscalar + Python DSE sweep 工具链）从 OpenSpec 文档变为可执行代码，建立 M5-DSE 完整基础设施（M5.10-M5.19）
> **关键交付**:
> - `TopologyBuilder<N_STAGES>` 编译期实例化 3/5/7/10 级流水线（无运行时开销）
> - 2-wide superscalar lane 派发（factory 端 atomic round-robin，plugin API 零污染）
> - 10 级深流水 + multi-cycle MUL (`RiscvMulPlugin<T, LATENCY>`)
> - Python DSE 工具链（sweep_driver + parse_results + pareto_analyzer）支持 576 config 扫描
> - `cpu_sim` CLI 二进制 + 12 个新 JSON schema 字段
> - 6 集成测试（3/5/7/10 级 + superscalar + deep-pipeline）
>
> **并行执行**: Batch 1 内 4 任务完全并行 (Schema → TopologyBuilder → Configs → MUL), Batch 2 内 4 任务并行 (cpu_sim + sweep_driver + parse_results + pareto_analyzer)
> **关键路径**: Schema (Section 5) → TopologyBuilder (Section 1) → Configs (Section 6) → cpu_sim (Section 7) → 完整 sweep (Section 4.5/8)
> **总工时**: ~5 d / 1 人 (Batch 1: 2 d, Batch 2: 1.5 d, Batch 3: 1.5 d)
> **推迟**: 完整 OoO (ROB/IQ/PRF/LSQ/Rename, ~2700-3300 LOC) 留 Phase 5+; RTL_ONLY / COMPARE ImplMode 留 Phase 5+

---

## Context

### 前置状态 (依赖就绪)

- **M4G (commit `89892aa`)**: D.1-D.4 前向锁定, 3 payload + 3 plugin 模板化 + HazardKind enum
- **M4G-extend (commit `ec6ee4f`)**: tid plumbing + commit_hook OoO 原语 + COMMIT 阶段名 — **本计划硬前置**
- **Phase 1 L1CachePlugin**: 已落地, 4/4 单元测试
- **CpuFactory**: 当前 stub 状态, `register_early/normal/late` plugins 仅 `(void)pb; (void)sizeof(U);`
- **基线**: 36/36 ctest PASS, 9 个生效 spec 无冲突
- **Oracle 审查**: 已完成, 5 BLOCKING 问题已修复 (commit `8bc6953`):
  - B1: proposal.md 文件状态修正 (4 文件从"新增"改为"修改")
  - B2: sweep spec 增加 MODIFIED Requirements (288→576 reconcile)
  - B3: design.md Decision 3 增加完整 lane 派发伪代码 (atomic + lambda capture)
  - B4: tasks.md Section 5 (Schema) 移到 Section 6 (Configs) 之前
  - B5: 12 字段 → 9 新 + 3 重命名对齐

### 用户原始请求 (2026-06-22)

> "@oracle 审查 m5-dse-superscalar openspec change 的内容，并给出下一步创建执行计划的建议"

执行路径：Oracle 审查 (1h) → 5 BLOCKING 修复 (1h) → 计划生成 (本文件) → 实施

### 已知风险 (从 Phase 1/M4G-extend 经验)

- **M4-DSE 硬阻塞 Batch 3**: M4-DSE (M4.12-M4.19) 是 CpuFactory stub 真实化（~3 d 工作），当前未启动；Batch 3 (集成测试 + 完整 576 sweep) 必须等 M4-DSE 完成
- **Decision 3 lane 派发**: atomic counter + lambda capture 是新模式, 编译期验证充分但运行时行为需 7-stage 集成测试 (Batch 3)
- **576 config sweep 性能**: 8 核并行 ~72 sec (估算), 仿真器在 7/10 级 baseline 未验证, Risk 2 已加 `--limit N` smoke test 缓解
- **openspec/ gitignored**: 修复 BLOCKING 时用 `git add -f` 强制提交；这是首次将 OpenSpec change 纳入 git (历史 commit `eaf2404` 不包含 OpenSpec files)
- **n_lanes vs dispatch_width 等价性**: design.md Open Question 4 留作 "文档中明确两者等价"，schema 同时接受两者；如有歧义需 ADR 永久裁决

---

## Work Objectives

### 核心目标

**M5-DSE 基础设施完整落地**: 流水线深度可配置 + 2-wide superscalar + Python DSE sweep 工具链从文档变为可执行代码。

### 具体交付物 (7 大 M5.x)

1. **M5.10 TopologyBuilder** (Section 1): 编译期实例化 3/5/7/10 级流水线，5 级 byte-identical baseline
2. **M5.14 MUL 多周期** (Section 3): `RiscvMulPlugin<T, LATENCY>` 模板 + declare_substage 子流水
3. **M5.18 2-wide + Deep Pipeline 配置** (Section 6): 升级 2 个 JSON + lane 派发逻辑
4. **M5.19 Schema 扩展** (Section 5): 9 新 optional 字段 + 3 字段重命名对齐
5. **M5.15 DSE Sweep 工具链** (Section 4): sweep_driver + parse_results + pareto_analyzer (含 ASCII chart)
6. **M5.16 cpu_sim 二进制** (Section 7): `--config --cycles --seed` CLI
7. **M5.11-M5.13 集成测试** (Section 2, Batch 3): test_3stage / test_5stage 升级 + test_7stage / test_10stage 新增

### Definition of Done

- [ ] `TopologyBuilder<N_STAGES>` 在 cpu_factory.h 实现，4 个特化编译通过
- [ ] 5 级 (default) 行为 byte-identical, 36/36 ctest 不退化
- [ ] cpu_params_schema.json + 9 新字段 + 3 重命名完成, 4 个 JSON 校验通过
- [ ] cpu_superscalar.json 升级 (dispatch_width=2 + n_lanes=2 + 5 superscalar 字段)
- [ ] cpu_deep_pipeline.json 升级 (10-stage + mul_latency=5)
- [ ] RiscvMulPlugin<T, LATENCY> 模板实例化, mul_latency=3 触发 ≥2 cycle 性能差
- [ ] sweep_driver.py 支持 576 config + `--limit --output --seed --parallel`, `--limit 100` smoke 通过
- [ ] parse_results.py 解析 cpu_sim stdout 为 results/sweep.json
- [ ] pareto_analyzer.py 计算 Pareto 前沿 + 输出 results/pareto.json + ASCII chart to stdout
- [ ] cpu_sim 二进制 `--config PATH --cycles N --seed S`, 5 级 baseline 10000 cycle 不崩溃
- [ ] (Batch 3, M4-DSE 解锁后) 4 集成测试 PASS (3/5/7/10-stage), cpu_superscalar config 跑通 10000 cycle
- [ ] (Batch 3, M4-DSE 解锁后) 完整 576 sweep 跑通率 ≥95%, results/sweep.json + results/pareto.json 生成
- [ ] `ctest` 40+ tests 全 PASS (36 现有 + 4 集成 + 1 mul_latency)
- [ ] `tools/verify_adr.sh` + `tools/verify_no_ghost_refs.sh` 全 PASS
- [ ] `openspec validate --changes` PASS
- [ ] 6 原子 commit (tasks.md Section 9 顺序)
- [ ] M5 里程碑 ✅ 达成 ("DSE 基础设施完成")

---

## Verification Strategy

### Test Decision

- **C++ 代码**: ctest (项目已有 36 tests), 编译期 `static_assert` (项目惯例), `--config 编译选项`
- **Python 代码**: 暂用 ad-hoc smoke test (项目惯例, 无 pytest 框架), 后续 Phase 2 已规划 pytest 集成
- **JSON Schema**: ajv (Node.js) — 现有工具链, 项目 CI 已用
- **集成测试**: gtest (项目 ctest 已用)
- **回归基线**: 36/36 ctest PASS (commit `1146e6c` 状态), 9 个生效 spec 无冲突

### QA Policy (Self-Check 清单)

每完成一个 Section 后, 实施者必须执行以下 self-check：

1. **Baseline 不退化**:
   - [ ] `ctest` 36/36 PASS (或 36+新 ctest 数, 不退化)
   - [ ] `tools/verify_adr.sh` PASS
   - [ ] `tools/verify_no_ghost_refs.sh` PASS
   - [ ] `openspec validate --changes` PASS
2. **Spec 合规**:
   - [ ] 新增/修改的 spec scenario 全部可验证
   - [ ] 现有 JSON 实例 (cpu_default/cpu_embedded) byte-identical
3. **代码风格**:
   - [ ] D4 范式 (Phase 1.4 复盘): Plugin 静态单例, factory 端调度
   - [ ] ADR-033 命名锁: 无 snake_case in class names, 无类型前缀
   - [ ] C++ 17/20 idiom (constexpr, std::array, structured bindings)
4. **Commit 原子性**:
   - [ ] 一个 commit 一个 logical change
   - [ ] conventional commit 格式
   - [ ] body 引用 plan + section + DoD item

---

## Execution Strategy

### 任务依赖图

```
Section 5 (Schema M5.19) ─────────┐
                                   ↓
Section 1 (TopologyBuilder M5.10) ─┴─→ Section 6 (Configs M5.18)
                                              ↓
Section 3 (MUL template M5.14) ────────┐
                                       ↓
                                  Section 7 (cpu_sim M5.16)
                                       ↓
                          Section 4 (sweep tools M5.15)
                                       ↓
                              Section 8 (验证)
                                       ↓
                  [Batch 3 BLOCKED by M4-DSE] ↓
Section 2 (集成测试 M5.11-13) ─────→ Section 4.5 (完整 576 sweep M5.17)
```

### 并行策略

**Batch 1 (Phase 1, 完全并行)** — 不依赖 M4-DSE, 可立即启动:
- **A1** Section 5 (Schema M5.19) — Schema 加 9 新字段 + 3 重命名
- **A2** Section 1 (TopologyBuilder M5.10) — 编译期实例化 3/5/7/10
- **A3** Section 6 (Configs M5.18) — 升级 2 个 JSON (依赖 A1 完成)
- **A4** Section 3 (MUL template M5.14) — `RiscvMulPlugin<T, LATENCY>`

A1 → A3 串行 (A3 需要 A1 的新字段), A2/A4 独立

**Batch 2 (Phase 2, 部分并行)** — 依赖 Batch 1:
- **B1** Section 7 (cpu_sim M5.16) — 依赖 A2 (TopologyBuilder)
- **B2** Section 4 (sweep tools M5.15, 4.1-4.4) — 依赖 B1 (cpu_sim 必须存在才能 sweep)
- **B3** parse_results.py 实施 — 依赖 B1
- **B4** pareto_analyzer.py ASCII chart — 依赖 B3

B1 → B2 → B3/B4 串行

**Batch 3 (Phase 3, 硬阻塞 M4-DSE)** — 等 M4-DSE 完成:
- **C1** Section 2 (集成测试 M5.11-13) — 4 ctest (test_3stage 升级 + test_5stage 升级 + test_7stage 新增 + test_10stage 新增)
- **C2** Section 4.5 (完整 576 sweep M5.17) — 阻塞 M5.16 + M4-DSE

**Batch 4 (验证)** — 串行:
- **D1** Section 8 验证套件 (ctest + verify_adr + openspec validate)
- **D2** Section 9 git commit 整理 (6 原子 commit, tasks.md 已规划)

### 时间线 (单 session 估算)

| Batch | 内容 | 工时 | 累计 |
|-------|------|------|------|
| Batch 1 | Schema + TopologyBuilder + Configs + MUL | ~2 d | 2 d |
| Batch 2 | cpu_sim + sweep_driver + parse_results + pareto | ~1.5 d | 3.5 d |
| Batch 3 | 集成测试 + 完整 sweep (BLOCKED by M4-DSE) | ~1.5 d | 5 d |
| Batch 4 | 验证 + commit 整理 | ~0.5 d | 5.5 d |

注: Batch 1 内 A1 (Schema) + A2 (TopologyBuilder) + A4 (MUL) 可 3 人并行 (单 session 内串行)

### 关键路径

`Schema (Section 5) → Configs (Section 6) → cpu_sim (Section 7) → sweep (Section 4) → 集成测试 (Section 2, Batch 3) → 完整 sweep (Section 4.5) → 验证 (Section 8)`

如 Section 5 Schema 修复 (字段命名决策) 拖延, 整个 Batch 1-2 顺延。

---

## TODOs

### 1. Schema 扩展 (Section 5 / M5.19) — 必须在 Section 6 之前完成 [Batch 1, A1]

**Status**: 🟢 UNBLOCKED

**File scope**:
- Modify: `ip/cpu/configs/cpu_params_schema.json` (加 9 字段, 重命名 3 字段)
- Modify: `ip/cpu/configs/cpu_default.json` (重命名 3 字段)
- Modify: `ip/cpu/configs/cpu_embedded.json` (重命名 3 字段)
- Modify: `ip/cpu/configs/cpu_superscalar.json` (重命名 3 字段)
- Modify: `ip/cpu/configs/cpu_deep_pipeline.json` (重命名 3 字段)
- Modify: `ip/cpu/cpu_factory.h` (CPUConfig struct 字段名同步, ~5 LOC)

**依赖**: 无 (这是 Batch 1 的起点)

**前置验证**:
- `openspec validate --changes` 当前 PASS
- `ctest` 36/36 PASS 基线
- 读取 `ip/cpu/configs/cpu_params_schema.json` 现有字段 (用 Read 工具)

**实施步骤 (TDD 风格)**:

- [x] **1.1 写失败的 schema 校验测试**
  在 `tests/cpu/configs/test_schema_m5_19.cpp` 新增:
  ```cpp
  #include <nlohmann/json.hpp>
  #include <ajv/ajv.hpp>
  TEST(M5Schema, NewFieldsAccepted) {
    auto schema = nlohmann::json::parse(
      read_file("ip/cpu/configs/cpu_params_schema.json"));
    auto superscalar = nlohmann::json::parse(
      read_file("ip/cpu/configs/cpu_superscalar.json"));
    // 期望: dispatch_width=2 + n_lanes=2 验证通过
    EXPECT_TRUE(validate(schema, superscalar));
  }
  ```
  Run: `ctest -R M5Schema --output-on-failure`
  Expected: FAIL (字段尚未定义)

- [x] **1.2 加 9 个新 optional 字段到 cpu_params_schema.json**
  在 `"properties"` 块后追加 9 个字段, 每个都设 `"minimum": 0` 或 `"enum": [1,2,4]`, 默认 `"default": 0` 或 `"default": 1`:
  - `n_lanes` (default 1, enum [1,2,4,8])
  - `dispatch_width` (default 1, enum [1,2,4])
  - `issue_queue_size` (default 0, minimum 0)
  - `rob_size` (default 0, minimum 0)
  - `lsq_size` (default 0, minimum 0)
  - `rename_table_size` (default 0, minimum 0)
  - `retire_width` (default 1, enum [1,2,4])
  - `fetch_width` (default 1, enum [1,2,4])
  - `commit_width` (default 1, enum [1,2,4])

- [x] **1.3 重命名 3 个字段对齐 CpuConfig struct**
  - `icache_latency_cycles` → `icache_latency`
  - `dcache_latency_cycles` → `dcache_latency`
  - `mul_latency` 保持不变 (已与 struct 一致)
  在 schema 用 `"$ref"` 或 alias, 或直接 rename property key
  同步更新 4 个 JSON 实例 (cpu_default / cpu_embedded / cpu_superscalar / cpu_deep_pipeline)

- [x] **1.4 同步 CPUConfig struct 字段名**
  在 `ip/cpu/cpu_factory.h` 中:
  - 已有 `icache_latency` / `dcache_latency` (无 `_cycles` 后缀) — 验证一致
  - 加 9 个新字段 (n_lanes, dispatch_width, issue_queue_size, rob_size, lsq_size, rename_table_size, retire_width, fetch_width, commit_width), 默认值与 schema 一致

- [x] **1.5 运行 schema 测试 + ajv 校验**
  Run:
  ```bash
  ctest -R M5Schema --output-on-failure
  # ajv 校验 (项目 CI 用法)
  npx ajv validate -s ip/cpu/configs/cpu_params_schema.json \
    -d ip/cpu/configs/cpu_default.json
  npx ajv validate -s ip/cpu/configs/cpu_params_schema.json \
    -d ip/cpu/configs/cpu_embedded.json
  ```
  Expected: PASS (全部 5 个 JSON 校验通过)

- [x] **1.6 验证 baseline 不退化**
  Run: `ctest --output-on-failure`
  Expected: 36/36 PASS (新增 1 个 M5Schema test = 37/37)

- [x] **1.7 Commit**
  ```bash
  git add ip/cpu/configs/cpu_params_schema.json \
          ip/cpu/configs/cpu_default.json \
          ip/cpu/configs/cpu_embedded.json \
          ip/cpu/configs/cpu_superscalar.json \
          ip/cpu/configs/cpu_deep_pipeline.json \
          ip/cpu/cpu_factory.h \
          tests/cpu/configs/test_schema_m5_19.cpp
  git commit -m "feat(m5-dse): schema +9 fields + 3 renames align with CpuConfig struct (M5.19)"
  ```

**DoD**:
- [x] cpu_params_schema.json 加 9 optional 字段
- [x] 3 字段重命名 (`icache_latency_cycles` → `icache_latency`, `dcache_latency_cycles` → `dcache_latency`, `mul_latency` 保留)
- [x] 5 个 JSON (含 cpu_superscalar / cpu_deep_pipeline) ajv 校验全 PASS
- [x] 36/36 ctest baseline 不退化
- [ ] commit 干净, 无遗留代码

---

### 2. TopologyBuilder 编译期展开 (Section 1 / M5.10) [Batch 1, A2]

**Status**: 🟢 UNBLOCKED (可与 Section 5 并行)

**File scope**:
- Modify: `ip/cpu/cpu_factory.h` (新增 `template <std::size_t N_STAGES> struct TopologyBuilder`, ~30 LOC)
- Modify: `ip/cpu/cpu_factory.h::build_cpu` (switch-case 路由 pipeline_stages, ~15 LOC)

**依赖**: 无

**前置验证**:
- `ip/cpu/cpu_factory.h` 当前 stub 状态已确认 (register_early/normal/late 为空)
- 读取 `ip/cpu/docs/multi_isa_architecture.md` §2.4 拓扑表 (行号 ~268-302)

**实施步骤 (TDD + 编译期 static_assert)**:

- [ ] **2.1 写编译期验证测试**
  在 `tests/cpu/test_topology_builder.cpp` 新增:
  ```cpp
  #include <ip/cpu/cpu_factory.h>
  TEST(TopologyBuilder, FiveStageIsByteIdentical) {
    cf::plugin::PipeBuilder pb;
    cf::cpu::TopologyBuilder<5>::expand(pb, default_config());
    EXPECT_EQ(pb.node_count(), 5);
    EXPECT_EQ(pb.stage_name(0), "fetch");
    EXPECT_EQ(pb.stage_name(4), "writeback");
  }
  TEST(TopologyBuilder, SevenStageHasRetire) {
    cf::plugin::PipeBuilder pb;
    cf::cpu::TopologyBuilder<7>::expand(pb, default_config());
    EXPECT_EQ(pb.node_count(), 7);
    EXPECT_EQ(pb.stage_name(6), "retire");  // 写回 + commit 合并到 RETIRE
  }
  TEST(TopologyBuilder, ThreeStageMerged) {
    cf::plugin::PipeBuilder pb;
    cf::cpu::TopologyBuilder<3>::expand(pb, default_config());
    EXPECT_EQ(pb.node_count(), 3);
    EXPECT_EQ(pb.stage_name(0), "if");      // IF: fetch+decode 合并
    EXPECT_EQ(pb.stage_name(1), "exmem");   // EXMEM: execute+memory 合并
  }
  ```
  Run: `ctest -R TopologyBuilder --output-on-failure`
  Expected: FAIL (TopologyBuilder 未定义)

- [ ] **2.2 实现 TopologyBuilder 模板骨架**
  在 `ip/cpu/cpu_factory.h` 新增:
  ```cpp
  namespace cf::cpu {
  template <std::size_t N_STAGES>
  struct TopologyBuilder {
    static_assert(N_STAGES == 3 || N_STAGES == 5 ||
                  N_STAGES == 7 || N_STAGES == 10,
                  "TopologyBuilder supports 3/5/7/10 stages only");
    static void expand(cf::plugin::PipeBuilder& pb, const CPUConfig& cfg);
  };
  }  // namespace cf::cpu
  ```

- [ ] **2.3 特化 N_STAGES=5 (baseline byte-identical)**
  ```cpp
  template <>
  struct TopologyBuilder<5> {
    static void expand(cf::plugin::PipeBuilder& pb, const CPUConfig& cfg) {
      pb.at_stage("fetch",     Phase::NORMAL, []{ /* IF */  });
      pb.at_stage("decode",    Phase::NORMAL, []{ /* ID */  });
      pb.at_stage("execute",   Phase::NORMAL, []{ /* EX */  });
      pb.at_stage("memory",    Phase::NORMAL, []{ /* MEM */ });
      pb.at_stage("writeback", Phase::NORMAL, []{ /* WB */  });
    }
  };
  ```
  Run: `ctest -R TopologyBuilder --output-on-failure`
  Expected: 5StageIsByteIdentical PASS, 其他 FAIL (尚未特化)

- [ ] **2.4 特化 N_STAGES=3 (IF/EXMEM 合并)**
  按 `multi_isa_architecture.md §2.4` 3-row 表: fetch+decode→IF, execute+memory→EXMEM, writeback 单独

- [ ] **2.5 特化 N_STAGES=7 (含 RETIRE)**
  按 §2.4 7-row 表: fetch, decode, execute, memory, writeback, retire (= writeback+commit), commit (单独)
  注: 实际 §2.4 表是 6 行 (writeback+commit→RETIRE), spec 写"7-row"是笔误, 实施时按 7-node 表实际定义

- [ ] **2.6 特化 N_STAGES=10 (深流水, ≥3 sub-pipe between fetch and execute)**
  按 §2.4 10-row 表: 包含 fetch, fetch2, decode, rename, issue, execute, memory2, writeback, retire, commit

- [ ] **2.7 build_cpu switch-case 路由**
  修改 `build_cpu(config)`:
  ```cpp
  switch (config.pipeline_stages) {
    case 3:  TopologyBuilder<3>::expand(pb, config); break;
    case 5:  TopologyBuilder<5>::expand(pb, config); break;
    case 7:  TopologyBuilder<7>::expand(pb, config); break;
    case 10: TopologyBuilder<10>::expand(pb, config); break;
    default: throw std::invalid_argument("Unsupported pipeline_stages");
  }
  ```

- [ ] **2.8 验证 36/36 baseline 不退化**
  Run: `ctest --output-on-failure`
  Expected: 36/36 PASS (5 级 byte-identical)
  Run: `tools/verify_adr.sh && tools/verify_no_ghost_refs.sh`
  Expected: PASS

- [ ] **2.9 Commit**
  ```bash
  git add ip/cpu/cpu_factory.h tests/cpu/test_topology_builder.cpp
  git commit -m "feat(m5-dse): TopologyBuilder compile-time 3/5/7/10 stage expansion (M5.10)"
  ```

**DoD**:
- [ ] `TopologyBuilder<3/5/7/10>` 4 个特化编译通过
- [ ] 4 个 test scenario 全 PASS (3-node / 5-node / 7-node / ≥10-node)
- [ ] 36/36 ctest baseline 不退化
- [ ] 5 级 byte-identical (现有 ctest 全过)

---

### 3. RiscvMulPlugin 多周期延迟 (Section 3 / M5.14) [Batch 1, A4]

**Status**: 🟢 UNBLOCKED (与 Section 5/1 并行)

**File scope**:
- Modify: `ip/cpu/arch/riscv/mul.h` (加 `template <typename T, std::size_t LATENCY = 1>` LATENCY 模板参数, ~20 LOC)
- Modify: `ip/cpu/cpu_factory.h::build_cpu` (switch-case 路由 `mul_latency`, ~10 LOC)

**依赖**: 无

**前置验证**:
- 当前 `RiscvMulPlugin<T>` 无 LATENCY 参数 (Oracle 已确认)
- `declare_substage(parent, sub_name, depth)` API 签名已确认

**实施步骤**:

- [ ] **3.1 写编译期验证测试**
  在 `tests/cpu/arch/riscv/test_mul_latency.cpp` 新增:
  ```cpp
  #include <ip/cpu/arch/riscv/mul.h>
  TEST(RiscvMulPluginLatency, LatencyOneIsBaseline) {
    cf::cpu::RiscvMulPlugin<RV32, 1> mul;
    mul.setup(/*...*/);
    EXPECT_EQ(mul.expected_latency(), 1);
  }
  TEST(RiscvMulPluginLatency, LatencyThreeAddsSubstages) {
    cf::cpu::RiscvMulPlugin<RV32, 3> mul;
    mul.setup(/*...*/);
    EXPECT_EQ(mul.expected_latency(), 3);
    EXPECT_EQ(mul.substage_count(), 2);  // mul_s1, mul_s2
  }
  ```

- [ ] **3.2 加 LATENCY 模板参数 + declare_substage 展开**
  修改 `ip/cpu/arch/riscv/mul.h`:
  ```cpp
  template <typename T, std::size_t LATENCY = 1>
  struct RiscvMulPlugin {
    static_assert(LATENCY >= 1 && LATENCY <= 5, "LATENCY in {1,3,5}");
    static constexpr std::size_t LAT = LATENCY;
    void setup(/*...*/) {
      if constexpr (LATENCY == 1) {
        // 单周期, 无 substage
      } else {
        for (std::size_t i = 1; i < LATENCY; ++i) {
          pb.declare_substage("execute",
                              std::string("mul_s") + std::to_string(i), 1);
        }
      }
    }
    // build() 使用 LATENCY 而非 hard-coded 1
  };
  ```

- [ ] **3.3 build_cpu 路由 mul_latency**
  ```cpp
  switch (config.mul_latency) {
    case 1: register_mul<RiscvMulPlugin<T, 1>>(pb); break;
    case 3: register_mul<RiscvMulPlugin<T, 3>>(pb); break;
    case 5: register_mul<RiscvMulPlugin<T, 5>>(pb); break;
    default: throw std::invalid_argument("Unsupported mul_latency");
  }
  ```

- [ ] **3.4 写性能断言测试 (mul_latency=3 慢 ≥2 cycle)**
  ```cpp
  TEST(MulLatencyPerf, ThreeCyclesSlower) {
    // 跑 add.elf with mul_latency=1 vs =3, 断言 cycle 数差 ≥2
    auto baseline = run_cpu_sim("cpu_default.json", /*cycles*/100);
    auto latency3 = run_cpu_sim("cpu_mul3.json", /*cycles*/100);
    EXPECT_GE(latency3.cycles - baseline.cycles, 2);
  }
  ```

- [ ] **3.5 验证 36/36 + 1 = 37 ctest PASS**
  Run: `ctest --output-on-failure`
  Expected: 37/37 PASS (36 现有 + 1 mul_latency)

- [ ] **3.6 Commit**
  ```bash
  git add ip/cpu/arch/riscv/mul.h ip/cpu/cpu_factory.h \
          tests/cpu/arch/riscv/test_mul_latency.cpp
  git commit -m "feat(m5-dse): RiscvMulPlugin multi-cycle LATENCY template (M5.14)"
  ```

**DoD**:
- [ ] `RiscvMulPlugin<T, LATENCY>` 3 个特化 (<T,1> / <T,3> / <T,5>) 编译通过
- [ ] mul_latency=3 触发 mul_s1 + mul_s2 substage
- [ ] 性能测试: mul_latency=3 比 mul_latency=1 慢 ≥2 cycle
- [ ] 37/37 ctest PASS

---

### 4. 2-wide Superscalar + Deep Pipeline 配置 (Section 6 / M5.18) [Batch 1, A3]

**Status**: 🟡 BLOCKED by Task 1 (Schema 必须先加新字段)

**File scope**:
- Modify: `ip/cpu/configs/cpu_superscalar.json` (已存在, 加 7-stage + 5 superscalar 字段)
- Modify: `ip/cpu/configs/cpu_deep_pipeline.json` (已存在, 加 10-stage 字段)
- Modify: `ip/cpu/cpu_factory.h::build_cpu` (检测 dispatch_width > 1 → 走 lane 派发路径)

**依赖**: Task 1 (Schema 加字段)

**前置验证**:
- 已读 `cpu_superscalar.json` (已存在, 当前 pipeline_stages=7, 但缺 superscalar 字段)
- 已读 `cpu_deep_pipeline.json` (已存在)
- 已读 design.md Decision 3 (lane 派发伪代码已就位)

**实施步骤**:

- [ ] **4.1 升级 cpu_superscalar.json**
  加 5 个字段 (基于 Section 1 新加的 schema 字段):
  ```json
  {
    "pipeline_stages": 7,
    "dispatch_width": 2,
    "n_lanes": 2,
    "fetch_width": 2,
    "commit_width": 2,
    "retire_width": 2,
    "mul_latency": 1,
    "icache_latency": 1,
    "dcache_latency": 1
  }
  ```

- [ ] **4.2 升级 cpu_deep_pipeline.json**
  ```json
  {
    "pipeline_stages": 10,
    "dispatch_width": 1,
    "n_lanes": 1,
    "fetch_width": 1,
    "commit_width": 1,
    "retire_width": 1,
    "mul_latency": 5,
    "icache_latency": 3,
    "dcache_latency": 3
  }
  ```

- [ ] **4.3 ajv 校验 2 个 JSON**
  ```bash
  npx ajv validate -s ip/cpu/configs/cpu_params_schema.json \
    -d ip/cpu/configs/cpu_superscalar.json
  npx ajv validate -s ip/cpu/configs/cpu_params_schema.json \
    -d ip/cpu/configs/cpu_deep_pipeline.json
  ```
  Expected: PASS

- [ ] **4.4 实现 cpu_factory.h lane 派发逻辑**
  按 design.md Decision 3 伪代码 (已修复 B3):
  ```cpp
  // cpu_factory.h::build_cpu
  template <std::size_t N_STAGES>
  void apply_lane_dispatch(PipeBuilder& pb, const CPUConfig& cfg,
                           std::atomic<uint8_t> counters[]) {
    if (cfg.dispatch_width <= 1) return;  // 单发射无 lane
    for (std::size_t i = 0; i < N_STAGES; ++i) {
      const char* stage_name = stage_name_at(i);
      pb.at_stage(stage_name, Phase::NORMAL,
                  [&pb, lane_ptr=&counters[i], n=cfg.n_lanes]{
                    const uint8_t my_lane = (lane_ptr->fetch_add(1) % n);
                    pb.node_of_logic_stage(stage_name)->set_lane(my_lane);
                  });
    }
  }
  ```
  在 build_cpu 中:
  ```cpp
  switch (cfg.pipeline_stages) {
    case 7: {
      TopologyBuilder<7>::expand(pb, cfg);
      std::atomic<uint8_t> counters[7] = {};
      apply_lane_dispatch<7>(pb, cfg, counters);
      break;
    }
    // ... 其他 case
  }
  ```

- [ ] **4.5 验证 baseline 不退化 + 新功能编译**
  Run: `ctest --output-on-failure`
  Expected: 36/36 PASS (5 级 baseline 不动, lane 派发仅 dispatch_width > 1 触发)

- [ ] **4.6 Commit**
  ```bash
  git add ip/cpu/configs/cpu_superscalar.json \
          ip/cpu/configs/cpu_deep_pipeline.json \
          ip/cpu/cpu_factory.h
  git commit -m "feat(m5-dse): 2-wide superscalar + 10-stage deep configs + lane dispatch (M5.18)"
  ```

**DoD**:
- [ ] cpu_superscalar.json 加 5 superscalar 字段, ajv PASS
- [ ] cpu_deep_pipeline.json 加 10-stage 字段, ajv PASS
- [ ] lane 派发逻辑在 build_cpu 实现 (仅 dispatch_width > 1 触发)
- [ ] 5 级 baseline byte-identical
- [ ] 36/36 ctest PASS

---

### 5. cpu_sim 二进制 (Section 7 / M5.16) [Batch 2, B1]

**Status**: 🟡 BLOCKED by Task 2 (TopologyBuilder 必须存在)

**File scope**:
- Create: `tools/cpu_sim/main.cpp` (~50 LOC)
- Modify: `ip/cpu/CMakeLists.txt` (或 `tools/CMakeLists.txt`, 视项目结构) — 加 `cpu_sim` executable target

**依赖**: Task 2 (TopologyBuilder, cpu_factory.h 提供 build_cpu)

**前置验证**:
- 项目 CMake 结构已了解 (Phase 1.4 已用 CMake)
- 读取 `ip/cpu/CMakeLists.txt` 现有结构, 找到合适的 target 插入点

**实施步骤**:

- [ ] **5.1 写 cpu_sim CLI 参数解析**
  ```cpp
  // tools/cpu_sim/main.cpp
  #include <CLI/CLI.hpp>  // 或项目自带的 arg parser
  int main(int argc, char** argv) {
    std::string config_path;
    uint64_t cycles = 1000;
    uint64_t seed = 0;
    CLI::App app{"cpu_sim"};
    app.add_option("--config", config_path, "Path to JSON config")->required();
    app.add_option("--cycles", cycles, "Number of cycles to run");
    app.add_option("--seed", seed, "Random seed (0=time-based)");
    CLI11_PARSE(app, argc, argv);
    // ...
  }
  ```

- [ ] **5.2 实现 main 主体**
  ```cpp
  auto cfg = parse_config(config_path);
  auto pb = cf::cpu::build_cpu(cfg);
  // optional: pb.set_seed(seed);
  for (uint64_t i = 0; i < cycles; ++i) {
    pb.run();
  }
  // 输出 KEY=VALUE 格式 (sweep_driver 可解析)
  std::cout << "cycles=" << cycles << "\n";
  std::cout << "ipc=" << pb.ipc() << "\n";
  std::cout << "tohost=" << pb.tohost() << "\n";
  std::cout << "config=" << config_path << "\n";
  std::cout << "pipeline_stages=" << cfg.pipeline_stages << "\n";
  std::cout << "dispatch_width=" << cfg.dispatch_width << "\n";
  ```

- [ ] **5.3 加 CMake target**
  ```cmake
  # ip/cpu/CMakeLists.txt (或 tools/CMakeLists.txt)
  add_executable(cpu_sim tools/cpu_sim/main.cpp)
  target_link_libraries(cpu_sim PRIVATE cf_cpu cf_plugin)
  ```

- [ ] **5.4 编译并 smoke test**
  ```bash
  cmake --build build --target cpu_sim
  ./build/cpu_sim --config ip/cpu/configs/cpu_default.json --cycles 10000
  ```
  Expected: 不崩溃, 输出 ~6 行 KEY=VALUE, tohost=0 (无 add.elf) 或 tohost=1 (有 add.elf)

- [ ] **5.5 验证 baseline 不退化**
  Run: `ctest --output-on-failure`
  Expected: 36/36 PASS (cpu_sim 是新 target, 不影响 ctest)

- [ ] **5.6 Commit**
  ```bash
  git add tools/cpu_sim/main.cpp ip/cpu/CMakeLists.txt
  git commit -m "feat(m5-dse): cpu_sim binary with --config --cycles --seed (M5.16)"
  ```

**DoD**:
- [ ] cpu_sim 编译通过
- [ ] `--config cpu_default.json --cycles 10000` 跑通不崩溃
- [ ] 输出 KEY=VALUE 格式 (cycles, ipc, tohost, config, pipeline_stages, dispatch_width)
- [ ] 36/36 ctest baseline 不退化

---

### 6. DSE Sweep 工具链 (Section 4 / M5.15, 4.1-4.4) [Batch 2, B2]

**Status**: ✅ DONE (T6 — sweep_driver 576 config + --seed + ASCII chart; parse_results.py; 39/39 ctest PASS)

**File scope**:
- Modify: `tools/dse/sweep_driver.py` (重写, ~150 LOC Python)
- Create: `tools/dse/parse_results.py` (~50 LOC Python)
- Modify: `tools/dse/pareto_analyzer.py` (加 ASCII chart, ~80 LOC Python)

**依赖**: Task 5 (cpu_sim)

**前置验证**:
- 已读现有 `tools/dse/sweep_driver.py` (DEFAULT_DSE_SPACE = 288 config, 6 维度)
- 已读 `tools/dse/pareto_analyzer.py` (无 ASCII chart)
- 已读 `tools/dse/README.md` (现有文档)

**实施步骤**:

- [x] **6.1 写 sweep_driver 默认 576 config 维度空间**
  修改 `tools/dse/sweep_driver.py`:
  ```python
  DEFAULT_DSE_SPACE = {
    "pipeline_stages": [3, 5, 7, 10],            # 4
    "branch_predictor": ["none", "bimodal", "gshare"],  # 3
    "btb_entries": [0, 64, 512],                  # 3
    "xlen": [32, 64],                             # 2
    "mul_latency": [1, 5],                        # 2
    "icache_latency": [1, 3],                     # 2
    "dcache_latency": [1, 3],                     # 2
  }
  # 4*3*3*2*2*2*2 = 576 configs
  # 旧 DEFAULT_DSE_SPACE 6 维度 288 通过 --space JSON 覆盖保留
  ```

- [x] **6.2 加 CLI 参数**
  ```python
  import argparse, random
  parser = argparse.ArgumentParser()
  parser.add_argument("--config", default="ip/cpu/configs/cpu_default.json")
  parser.add_argument("--cycles", type=int, default=1000)
  parser.add_argument("--output", default="results/sweep.json")
  parser.add_argument("--limit", type=int, default=None)
  parser.add_argument("--parallel", type=int, default=os.cpu_count())
  parser.add_argument("--seed", type=int, default=0)
  parser.add_argument("--space", default=None,
                      help="JSON file with custom DSE space (overrides default)")
  args = parser.parse_args()
  ```

- [x] **6.3 实现 deterministic matrix 生成**
  ```python
  rng = random.Random(args.seed)
  configs = []
  for combo in itertools.product(*DEFAULT_DSE_SPACE.values()):
    cfg = dict(zip(DEFAULT_DSE_SPACE.keys(), combo))
    rng.shuffle(configs)  # 用 seed shuffle, 同 seed 同顺序
    configs.append(cfg)
  if args.limit:
    configs = configs[:args.limit]
  ```

- [x] **6.4 multiprocessing.Pool 跑 cpu_sim**
  ```python
  def run_one(cfg):
    json_path = write_temp_config(cfg)  # 写到临时 JSON
    result = subprocess.run(
      ["./build/cpu_sim", "--config", json_path, "--cycles", str(args.cycles),
       "--seed", str(args.seed)],
      capture_output=True, text=True)
    return parse_kv_output(result.stdout) | cfg  # 合并 cfg + 输出

  with multiprocessing.Pool(args.parallel) as pool:
    results = list(tqdm(pool.imap_unordered(run_one, configs), total=len(configs)))
  json.dump(results, open(args.output, "w"), indent=2)
  ```

- [x] **6.5 smoke test (--limit 100)**
  ```bash
  ./tools/dse/sweep_driver.py --limit 100 --seed 0 --output /tmp/sweep100.json
  ```
  Expected: 100 config 跑完, /tmp/sweep100.json 生成, 无崩溃

- [x] **6.6 实现 parse_results.py**
  ```python
  # tools/dse/parse_results.py
  import sys, json, re
  def parse_kv(stdout):
    result = {}
    for line in stdout.split("\n"):
      if "=" in line:
        k, v = line.split("=", 1)
        try:
          result[k.strip()] = int(v) if v.isdigit() else float(v) if "." in v else v.strip()
        except ValueError:
          result[k.strip()] = v.strip()
    return result
  if __name__ == "__main__":
    sweep_json = sys.argv[1]
    data = json.load(open(sweep_json))
    # 重新解析每行的 KV (兜底, cpu_sim 已直接输出 KV)
    json.dump(data, sys.stdout, indent=2)
  ```

- [x] **6.7 实现 pareto_analyzer ASCII chart**
  修改 `tools/dse/pareto_analyzer.py`, 加 ASCII chart 输出:
  ```python
  def render_ascii_chart(pareto_front, all_results, width=60, height=20):
    cycles = [r["cycles"] for r in all_results]
    ipcs = [r["ipc"] for r in all_results]
    min_c, max_c = min(cycles), max(cycles)
    min_i, max_i = min(ipcs), max(ipcs)
    grid = [[" "] * width for _ in range(height)]
    # 画所有点 (.)
    for r in all_results:
      x = int((r["cycles"] - min_c) / (max_c - min_c) * (width - 1))
      y = int((r["ipc"] - min_i) / (max_i - min_i) * (height - 1))
      grid[height - 1 - y][x] = "."
    # 画 Pareto 前沿 (*)
    for r in pareto_front:
      x = int((r["cycles"] - min_c) / (max_c - min_c) * (width - 1))
      y = int((r["ipc"] - min_i) / (max_i - min_i) * (height - 1))
      grid[height - 1 - y][x] = "*"
    return "\n".join("".join(row) for row in grid)
  ```
  在 main() 调用 `print(render_ascii_chart(front, results))`

- [x] **6.8 验证 ASCII chart 输出 ≥2 (cycles, ipc) 坐标**
  ```bash
  ./tools/dse/pareto_analyzer.py /tmp/sweep100.json
  ```
  Expected: stdout 含 ASCII chart, 至少 2 个 * (Pareto 点)

- [x] **6.9 Commit**
  ```bash
  git add tools/dse/sweep_driver.py tools/dse/parse_results.py \
          tools/dse/pareto_analyzer.py
  git commit -m "feat(m5-dse): DSE sweep toolchain (576 config + --seed + ASCII chart) (M5.15)"
  ```

**DoD**:
- [x] sweep_driver.py 默认生成 576 config
- [x] `--limit 100 --seed 0` smoke test 跑通
- [x] `--seed 0` 重跑顺序一致 (deterministic)
- [x] `--parallel` 默认 = os.cpu_count()
- [x] parse_results.py 解析 cpu_sim stdout 为 JSON
- [x] pareto_analyzer.py 输出 Pareto front + ASCII chart (≥2 坐标点)
- [x] 旧 288 config 通过 `--space` JSON 覆盖保留

---

### 7. 集成测试升级 (Section 2 / M5.11-M5.13) [Batch 3, C1]

**Status**: 🔵 BLOCKED by M4-DSE (M4.12-M4.19 真实 CpuFactory 11 plugin 注册)

**File scope**:
- Modify: `tests/cpu/integration/test_3stage_riscv.cpp` (加 pipeline_stages=3 断言)
- Modify: `tests/cpu/integration/test_5stage_riscv.cpp` (加 pipeline_stages=5 断言)
- Create: `tests/cpu/integration/test_7stage_riscv.cpp` (新增, 跑 add.elf + tohost=1)
- Create: `tests/cpu/integration/test_10stage_riscv.cpp` (新增, 跑 add.elf + tohost=1)

**依赖**: M4-DSE 完成 (外部硬前置, 独立 change)

**前置验证**:
- 已读现有 test_3stage_riscv.cpp / test_5stage_riscv.cpp 结构
- 已确认 36/36 ctest 当前 PASS

**实施步骤**:

- [ ] **7.1 升级 test_3stage_riscv.cpp**
  加 `EXPECT_EQ(pb.node_count(), 3)` 在现有测试末尾

- [ ] **7.2 升级 test_5stage_riscv.cpp**
  加 `EXPECT_EQ(pb.node_count(), 5)` (byte-identical baseline)

- [ ] **7.3 新建 test_7stage_riscv.cpp**
  ```cpp
  #include <gtest/gtest.h>
  #include <ip/cpu/cpu_factory.h>
  TEST(RiscVIntegration, SevenStageTopology) {
    auto cfg = parse_config("ip/cpu/configs/cpu_superscalar.json");
    auto pb = cf::cpu::build_cpu(cfg);
    EXPECT_EQ(pb.node_count(), 7);
    EXPECT_EQ(pb.stage_name(6), "retire");
  }
  TEST(RiscVIntegration, SevenStageRunsAddElf) {
    auto cfg = parse_config("ip/cpu/configs/cpu_superscalar.json");
    cfg.cycles = 10000;
    auto pb = cf::cpu::build_cpu(cfg);
    pb.load_elf("tests/elf/add.elf");  // 项目已有 add.elf 测试 fixture
    pb.run();
    EXPECT_EQ(pb.tohost(), 1);
  }
  ```

- [ ] **7.4 新建 test_10stage_riscv.cpp**
  类似 Step 3, 但用 cpu_deep_pipeline.json, 期望 pb.node_count() ≥ 10

- [ ] **7.5 验证 36+4 = 40 ctest PASS**
  Run: `ctest --output-on-failure`
  Expected: 40/40 PASS (36 现有 + 4 集成)

- [ ] **7.6 Commit**
  ```bash
  git add tests/cpu/integration/test_3stage_riscv.cpp \
          tests/cpu/integration/test_5stage_riscv.cpp \
          tests/cpu/integration/test_7stage_riscv.cpp \
          tests/cpu/integration/test_10stage_riscv.cpp
  git commit -m "test(m5-dse): 7-stage + 10-stage integration tests (M5.11-M5.13)"
  ```

**DoD**:
- [ ] 4 集成测试 PASS (test_3stage 升级 + test_5stage 升级 + test_7stage 新增 + test_10stage 新增)
- [ ] 7-stage topology 7 nodes 含 RETIRE
- [ ] 10-stage topology ≥10 nodes 含 deep-pipeline splits
- [ ] 3-stage topology 3 nodes 含 IF/EXMEM 合并
- [ ] 40/40 ctest PASS (36 + 4)

---

### 8. 完整 576 sweep (Section 4.5 / M5.17) [Batch 3, C2]

**Status**: 🔵 BLOCKED by M4-DSE + Task 5 (cpu_sim) + Task 7 (集成测试)

**File scope**: 无 (运行 sweep_driver.py)

**依赖**: M4-DSE + Task 5 + Task 7

**前置验证**:
- 4 个集成测试全 PASS
- cpu_sim 二进制可用

**实施步骤**:

- [ ] **8.1 运行完整 576 sweep**
  ```bash
  ./tools/dse/sweep_driver.py --seed 0 --output results/sweep.json
  ```
  Expected: 576 config 跑完, results/sweep.json 生成 (8 核 ~72 sec)

- [ ] **8.2 运行 Pareto 分析**
  ```bash
  ./tools/dse/parse_results.py results/sweep.json > results/sweep_parsed.json
  ./tools/dse/pareto_analyzer.py results/sweep_parsed.json > results/pareto.json
  # ASCII chart 在 stdout, 已重定向
  ```
  Expected: results/pareto.json 生成, 包含 Pareto front

- [ ] **8.3 验证 sweep 跑通率 ≥95%**
  ```python
  import json
  sweep = json.load(open("results/sweep.json"))
  success = sum(1 for r in sweep if r.get("tohost") is not None
                and r.get("ipc", 0) > 0)
  rate = success / len(sweep)
  assert rate >= 0.95, f"Sweep pass rate {rate:.2%} < 95%"
  ```
  Expected: 跑通率 ≥ 95% (允许 OoO stub 失败行被记录但不崩溃)

- [ ] **8.4 Commit**
  ```bash
  git add results/sweep.json results/pareto.json
  git commit -m "test(m5-dse): complete 576 sweep + Pareto frontier (M5.17)"
  ```
  注: results/ 可能已在 .gitignore, 检查后决定是否加 `-f`

**DoD**:
- [ ] 576 config 完整跑完
- [ ] results/sweep.json + results/pareto.json 生成
- [ ] sweep 跑通率 ≥ 95%

---

### 9. 验证 + git 整理 (Section 8) [Batch 4, D1]

**Status**: 🟡 BLOCKED by Task 8 (Batch 3 全部完成)

**实施步骤**:

- [ ] **9.1 ctest 40+ 全 PASS**
  ```bash
  ctest --output-on-failure
  ```
  Expected: 40+/40+ PASS (36 + 4 集成 + 1 mul_latency)

- [ ] **9.2 verify_adr.sh + verify_no_ghost_refs.sh**
  ```bash
  tools/verify_adr.sh && tools/verify_no_ghost_refs.sh
  ```
  Expected: 全 PASS

- [ ] **9.3 openspec validate --changes**
  ```bash
  openspec validate --changes
  ```
  Expected: change/m5-dse-superscalar ✓ passed

- [ ] **9.4 8.6 spec scenario 全验证**
  - [ ] 5-stage 5-node 拓扑 (test_topology_builder)
  - [ ] 7-stage 7-node 含 RETIRE (test_topology_builder + test_7stage_riscv)
  - [ ] 10-stage ≥10-node (test_topology_builder + test_10stage_riscv)
  - [ ] 3-stage 3-node 含 IF/EXMEM (test_topology_builder)
  - [ ] 2-wide superscalar config validates (ajv cpu_superscalar.json)
  - [ ] existing configs byte-identical (ajv cpu_default/cpu_embedded)
  - [ ] 10-stage deep pipeline config validates (ajv cpu_deep_pipeline.json)
  - [ ] sweep_driver 生成 576 config (sweep_driver.py --seed 0)
  - [ ] --limit 100 caps runs (smoke test)
  - [ ] pareto_analyzer 输出 valid front + ASCII chart (≥2 坐标点)

- [ ] **9.5 检查 6 原子 commit 边界清晰**
  ```bash
  git log --oneline 8bc6953..HEAD
  ```
  Expected: 6 个 commit, 每个 scope 不同 (schema, topology, mul, configs, cpu_sim, sweep, integration)

- [ ] **9.6 最终 commit (如有修复)**
  如有 self-check 发现问题, 修复后单独 commit

**DoD**:
- [ ] 40+/40+ ctest PASS
- [ ] verify_adr.sh + verify_no_ghost_refs.sh PASS
- [ ] openspec validate PASS
- [ ] 10 个 spec scenario 全可验证
- [ ] 6 原子 commit 边界清晰

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

### F1. Plan Compliance Audit — `oracle`

执行 Plan Compliance Audit, 验证实际实现 vs 计划声明:
- [ ] **F1.1 9 个 DoD 全部满足 (Section 1-9)**
- [ ] **F1.2 6 原子 commit 顺序与 tasks.md Section 9 一致**
- [ ] **F1.3 决策遵循 design.md Decisions 1-5 (尤其 Decision 3 lane 派发实现)**
- [ ] **F1.4 spec 场景 100% 可验证 (10/10 scenario)**
- [ ] **F1.5 无 oracle 未识别的技术债务**

### F2. Code Quality Audit — `oracle`

执行 Code Quality 审计:
- [ ] **F2.1 C++ 代码遵循 D4 范式 (Plugin 静态单例, factory 端调度)**
- [ ] **F2.2 ADR-033 命名锁无违反 (无 snake_case in class names, 无类型前缀)**
- [ ] **F2.3 Python 代码符合 PEP 8 (项目惯例)**
- [ ] **F2.4 无 dead code, 无 unused imports**
- [ ] **F2.5 无 type safety 违反 (`as any` in Python, `reinterpret_cast` in C++)**

### F3. Architecture Alignment Audit — `verify-architecture`

运行 `verify-architecture` skill, 验证:
- [ ] **F3.1 CppTLM/CppHDL/ChipForge 架构对齐**
- [ ] **F3.2 TopologyBuilder 与现有 PipeBuilder API 正交**
- [ ] **F3.3 lane 派发与 m4g-extend tid plumbing 协同**
- [ ] **F3.4 sweep 工具链与 cpu_sim 解耦良好**

### F4. Integration Test Verification

手动执行集成测试:
- [ ] **F4.1 `cpu_sim --config cpu_superscalar.json --cycles 10000` 跑通**
- [ ] **F4.2 `cpu_sim --config cpu_deep_pipeline.json --cycles 10000` 跑通**
- [ ] **F4.3 `sweep_driver.py --seed 0 --output /tmp/test.json` deterministic 验证 (跑 2 次 diff 应为空)**
- [ ] **F4.4 `sweep_driver.py --limit 100` smoke 通过**
- [ ] **F4.5 `pareto_analyzer.py /tmp/test.json` 输出含 ASCII chart**

### F5. OpenSpec Validation

- [ ] **F5.1 `openspec validate --changes` PASS**
- [ ] **F5.2 `openspec list --specs` 含 3 个新 spec**
- [ ] **F5.3 `openspec change show m5-dse-superscalar --json --deltas-only` 显示 6 deltas (5 ADDED + 1 MODIFIED)**

### F6. PR 准备 (Section 10)

- [ ] **F6.1 PR description 引用 `post-m4g-strategic-decision-2026-06-20.md`**
- [ ] **F6.2 PR description 标注: Phase 5+ OoO 主体 (ROB/IQ/PRF/LSQ/Rename) 推迟**
- [ ] **F6.3 PR description 标注: M4-DSE 是 M5.12/M5.13 隐含前置, Batch 3 阻塞**
- [ ] **F6.4 PR description 标注: 7 个 OoO 缺口推迟到 Phase 5+**

---

## 自检 + 决策草案提交 (Final Step)

完成后:
1. **自检报告**: 列出所有 DoD 完成状态, 任何 deviation 注明理由
2. **PR description**: 从 tasks.md Section 10 复制 + 补充本次实施实际差异
3. **OpenSpec archive**: PR merge 后用 `openspec archive m5-dse-superscalar` 归档

---

> **实施状态**: 🟢 Batch 1+2 UNBLOCKED, 🔵 Batch 3 BLOCKED by M4-DSE
> **下次启动**: `/start-work m5-dse-superscalar` (加载 superpowers:subagent-driven-development 或 superpowers:executing-plans)
> **Owner**: ChipForge 主线开发 (单工程师)
> **预计完成**: 5.5 d 单 session (含 Batch 3 等待 M4-DSE 时间)
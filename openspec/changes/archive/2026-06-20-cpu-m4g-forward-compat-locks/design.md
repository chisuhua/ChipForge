## Context

M4 已完成（19/19 子任务 PASS，ctest 35/35），`CpuFactory::build_cpu(CPUConfig)` 已可接受 12 字段配置并返回可用 PipeBuilder。M4-DSE 子阶段（8 子任务）计划将 `build_cpu` 真实实例化 11 个插件并接入 DSE sweep；M5-DSE（10 子任务）实施拓扑展开。

M4G 子阶段是 M4 与 M4-DSE 之间的**前瞻锁定窗口**。Oracle 评审（`bg_df09c224`, 2026-06-17）识别出当前 CPU 插件层的 **2 个硬墙**（`PayloadStore` 按类型键控、`RegFile` 单一全局数组）和 **6 个中等障碍**会在 Phase 5+（OoO/SMT/superscalar）引发约 **2000 行重构**。M4G 通过约 **108 行 header churn** 锁定 4 个关键决策（D.1-D.4），零行为改变。

**关键约束**：
- 模板参数必须有默认值，保证现有 `<typename T>` 调用点零修改
- 默认值必须保持当前 ABI（`N_REGS=32, N_THREADS=1`）
- `cf::plugin::*` 框架脊柱（`PipeBuilder`/`PipeNode`/`Payload<T>`/`PayloadStore`/`CtrlLink`/`PipeArbitration`）完全不修改，作为 OoO-friendly 不变脊柱

## Goals / Non-Goals

**Goals:**
- 落地 D.1（3 个新 Payload）+ D.2（3 个插件模板化）+ D.3（HazardKind enum）+ D.4（BranchPredictor tid 参数）
- 8+ 个 ctest 验证 D.1-D.4 + 多线程隔离 + 默认参数兼容性
- 现有 35/35 ctest 不退化
- 文档同步（blueprint/status/README）
- M4-DSE 可以在 M4G 完成后直接基于模板化插件实施

**Non-Goals:**
- 不实施 D.5（stage-name 成员，错误抽象）
- 不实施 D.8（ThreadContext 结构，投机性死代码）
- 不实施 D.9（Cpu 类，投机性死代码）
- 不创建 `BranchPredictorFactory`（破坏插件模型一致性）
- 不修改 `cf::plugin::*` 任何文件
- 不实施 Phase 5+ OoO/SMT/superscalar 功能（仅锁定接口）

## Decisions

### Decision 1: 模板参数默认值策略

**选择**: 所有新增模板参数使用默认值，且默认值 = 当前 ABI（`N_REGS=32, N_THREADS=1`）。

**理由**:
- C++17 模板默认参数编译期消去：调用点 `RegFilePlugin<uint32_t>` 自动实例化为 `RegFilePlugin<uint32_t, 32, 1>`，与旧版本 ABI 完全相同
- 不需要修改任何现有调用点（M2/M3 实施的 11 个插件、4-6 个 ctest 用例）
- 编译时间增加 ~10%（acceptable，Oracle 评审确认）

**.cpp 显式实例化策略**:
- 现有 `ip/cpu/plugins/{reg_file,hazard,branch_predictor}.cpp` 各有 2 行 `template class XxxPlugin<std::uint32_t>;` / `template class XxxPlugin<std::uint64_t>;`
- M4G 实施后**保持单参数形式**：依赖 C++17 默认模板实参补全 `N_REGS=32, N_THREADS=1`（如 `RegFilePlugin<std::uint32_t>` 等价于 `RegFilePlugin<std::uint32_t, 32, 1>`）
- **禁止写全形式**（如 `template class RegFilePlugin<std::uint32_t, 32, 1>;`）：全形式实例化结果与单参数形式 ABI 完全相同，但显式列出所有参数会与未来 M4-DSE 的非默认实例化（如 `RegFilePlugin<std::uint32_t, 16, 2>`）混淆
- `branch_predictor.cpp` 同理：保持 `<std::uint32_t>` / `<std::uint64_t>` 单参数形式

**替代方案**:
- 强制显式实例化：所有调用点修改 → 35+ 个文件改动，风险高，Oracle 不建议
- `static_if` (C++17 if constexpr) 选择字段：增加复杂度，无收益

### Decision 2: tid 参数传递方式

**选择**: 所有多线程感知方法接受 `tid` 默认参数（默认 0），不引入 `ThreadContext<T>` 结构（D.8）。

**理由**:
- 默认 `tid=0` 保持 Phase 1 单线程行为
- 调用方在 `at_stage` 回调中从 `n->payload(KeyType::THREAD_ID)` 读取 tid，传递给插件方法
- 推迟到 Phase 5+ 再决定是否引入 `ThreadContext<T>` 聚合结构

**替代方案**:
- `ThreadContext<T>` 结构体（D.8）：Oracle 判定"投机性死代码"，无消费者
- 全局 TLS tid：违反 cf_plugin 无全局状态原则

### Decision 3: HazardKind enum 设计

**选择**: `enum class HazardKind : std::uint8_t { NONE, RAW_RS1, RAW_RS2, WAW }`，4 个值。

**理由**:
- 区分 RAW 读 RS1/RS2（不同发射端口）与 WAW（不同提交端口），为 Phase 5+ 发射逻辑预留区分
- `uint8_t` 底层类型，序列化友好
- 4 个值覆盖 RISC-V 常见冒险（M4 范围）

**检测顺序与优先级语义**:
- `has_hazard(dec, tid)` 实现为 if-else 链，按 **RAW_RS1 → RAW_RS2 → WAW** 顺序检测，**第一个匹配即返回**
- 含义：若指令同时 `reads_rs1 && reads_rs2` 且 RS1 和 RS2 都在飞，**优先返回 `RAW_RS1`**，绝不返回 `RAW_RS2`
- 理由：Phase 1 单发射场景下，`reads_rs1 && reads_rs2` 但 `has_raw(rs1_idx) && has_raw(rs2_idx)` 同时成立极罕见；优先级选取"先 RS1 后 RS2"匹配现有 RISC-V 实现惯例
- **Phase 5+ 多发射场景**：如果需要同时报告多个冒险源，应改用 bitmask（如 `uint8_t { NONE=0, RAW_RS1=1, RAW_RS2=2, WAW=4 }`），但这不在 M4G 范围
- 当前 `enum class` 设计是单值返回，与 Phase 5+ 升级路径**不冲突**（可作为兼容层保留，旧 `NONE != h` 检查仍工作）

**替代方案**:
- 返回 `bool`（旧 API）：无法区分 RAW 位置，Phase 5+ 必须重命名
- 返回 bitmask（`uint8_t`）：语义模糊，调试困难

### Decision 4: 测试策略

**选择**: 新建 `tests/cpu/test_forward_compat.cpp`，8+ 个 GoogleTest 用例，覆盖 D.1-D.4 + 多线程隔离 + 默认参数兼容性。

**理由**:
- 8 个用例对应 proposal 的 4 个决策 + 4 个回归/隔离验证
- 集成到现有 `tests/cpu/CMakeLists.txt`，与 `test_reg_file`/`test_hazard`/`test_branch_predictor` 同级
- 不修改现有 4-6 个 `test_reg_file` 用例（默认参数下行为零变化）

**替代方案**:
- 合并到现有 test 文件：增加每个 test 文件 ~20 行，3 个文件改动，PR review 复杂
- 独立 test 文件但放 `ip/cpu/test/`：违反 v0.0.5 约定（测试在 `tests/<ip>/`）

## Risks / Trade-offs

### Risk 1: 模板实例化爆炸

**描述**: `RegFilePlugin<T, N_REGS, N_THREADS>` 理论上有 2 (T) × 7 (N_REGS) × 4 (N_THREADS) = 56 种实例化。

**缓解**:
- 默认值编译期消去：实际只实例化 2（T = uint32/uint64）+ 测试用的 2-3 个变体
- `static_assert(N_REGS >= 1 && N_REGS <= 128)` + `static_assert(N_THREADS >= 1 && N_THREADS <= 4)` 限制合法组合
- 编译期 `nm` 符号表检查

### Risk 2: API break 隐藏调用者

**描述**: `has_hazard` 返回类型从 `bool` 改为 `HazardKind` 可能影响未来代码。

**缓解**:
- `grep -rn "has_hazard" ip/cpu/ tests/cpu/` 全项目搜索（已确认 1 个 in-tree 调用者）
- 编译期类型检查（编译器会暴露所有 break）
- 默认 `tid=0` 参数保持所有现有调用点兼容

### Risk 3: per-thread 隔离的边界条件

**描述**: `tid >= N_THREADS` 不会崩溃但行为未定义。

**缓解**:
- `static_assert` 编译期限制 `N_THREADS in [1, 4]`
- `std::array` operator[] 越界时未定义（期望崩溃而非静默错误）
- 单元测试覆盖 `N_THREADS=2` 的 per-thread 隔离

### Risk 4: Phase 6 array_store 双缓冲假设

**描述**: 如果 Phase 6 实施 `array_store` 双缓冲，`std::array<array_store<T, N_REGS>, N_THREADS>` 可能不兼容。

**缓解**:
- 当前阶段不涉及 `array_store` 内部改变
- Phase 6 实施时，per-thread 双缓冲需要显式处理（不在 M4G 范围）
- 记录在 `dse_architecture_v2_design_research.md` 作为已知未知

### Risk 5: 编译时间影响

**描述**: 3 个插件模板化可能导致编译时间增加。

**缓解**:
- Oracle 评审实测 ~10% 增加（acceptable）
- 默认值编译期消去，实际新增实例化极少
- `BranchPredictorPlugin` 已有 10 个显式实例化在 .cpp，模板参数扩展不增加实例化数量

## Migration Plan

无外部 migration（M4G 是内部重构 + 前瞻锁定）：

1. **Phase 1（已落地）**: 现有 35/35 ctest PASS
2. **M4G 实施（本 change）**: 添加 Payload、模板化插件、添加测试，35/35 + 8+ 不退化
3. **M4-DSE（依赖本 change）**: 基于模板化插件实施 `build_cpu` 真实实例化
4. **Phase 5+（未来）**: OoO/SMT/superscalar 基于 M4G 锁定的接口实施，无需重构 Phase 1 代码

**回滚策略**: 如果 M4G 实施失败（编译错误或 ctest 大规模退化），`git revert` 整个 M4G commit 链即可，所有改动是 header-only + 新增测试，main 分支其余代码零影响。

## Open Questions

1. **`array_store` 是否需要 per-thread 感知？** — 当前决策：不需要（M4G 仅模板化外层 `std::array`）。Phase 6 实施 OoO 时再评估。
2. **`BranchPredictorPlugin` 的 BTB/BIMODAL/GSHARE 参数是否需要运行时配置？** — 当前决策：编译期常量（M4G 模板参数）。M4-DSE 实施 sweep 时如果需要运行时切换，可能需要改为 `std::vector` 或策略模式。
3. **是否需要在 M4G 添加 `cf::cpu::core::HazardKind` 命名空间？** — 当前决策：放在 `cf::cpu::plugins::HazardKind`（与 `HazardPlugin` 同命名空间）。如果 Phase 5+ 多个插件共用，重新评估。
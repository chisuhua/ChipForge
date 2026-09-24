## Why

M4G 子阶段（M4 集成与 M4-DSE 之间的前瞻锁定窗口）在 `ip/cpu/docs/implementation-plan/M4G-forward-compat-locks.md` 中规划，Oracle 评审通过（`bg_df09c224`, 2026-06-17）。该子阶段识别出当前 CPU 插件层的 **2 个硬墙**（PayloadStore 按类型键控、RegFile 单一全局数组）和 **6 个中等障碍**会在 Phase 5+（OoO、SMT、superscalar）引发约 **2000 行重构**。M4G 通过约 **108 行 header churn** 锁定 4 个关键决策（D.1-D.4），零行为改变，防止后续 DSE 阶段在锁已固定的代码上再叠加重写。

为什么是现在：
- M4 已完成（19/19 子任务 PASS，ctest 35/35），CpuFactory 已可接受 CPUConfig 实例化插件
- M4-DSE（`build_cpu` 真实实现 + DSE sweep）会触及 `RegFilePlugin`/`HazardPlugin`/`BranchPredictorPlugin` 的注册逻辑。如果 M4-DSE 先实施，M4G 的模板化改动会引发代码冲突
- Oracle 明确建议：D.1-D.4 是 sound and should ship（~50 行代码，预防 ~2000 行 Phase 5+ 重构）；D.5/D.8/D.9 是投机性死代码，已从本计划删除

## What Changes

- **D.1** 在 `ip/cpu/core/payload_common.h` 新增 3 个 `Payload`：`UID`（uint8，OoO ROB index 占位）、`THREAD_ID`（uint2，SMT 线程 ID）、`IID_PC`（T，superscalar 多指令 PC 区分）。Phase 1 默认值 0，零行为变化。
- **D.2** 模板化 `RegFilePlugin<T>` 为 `<typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>`。存储从 `array_store<T, 32>` 改为 `std::array<array_store<T, N_REGS>, N_THREADS>`，所有 `read_reg`/`write_reg` 接受 `tid` 默认参数。
- **D.2** 模板化 `HazardPlugin<T>` 为 `<typename T, std::size_t N_REGS = 32, std::size_t N_THREADS = 1>`。Scoreboard 从 `std::array<bool, 32>` 改为 `std::array<std::array<bool, N_REGS>, N_THREADS>`。
- **D.2 + D.4** 模板化 `BranchPredictorPlugin<T>` 为 `<typename T, BTB_SIZE, BIMODAL_SZ, GSHARE_SZ, GHR_BITS, N_THREADS = 1>`，同步在 `predict`/`update` 添加 `tid` 参数。
- **D.3** 修改 `HazardPlugin::has_hazard` 返回类型：`bool` → `enum class HazardKind { NONE, RAW_RS1, RAW_RS2, WAW }`，添加 `tid` 默认参数。
- **新增** `tests/cpu/test_forward_compat.cpp`：8+ 个 GoogleTest 用例，验证 D.1-D.4 + 多线程隔离 + 现有 API 兼容性。
- **文档同步** `ip/cpu/docs/blueprint.md` §5 标注 M4G 模板参数；`status.md` 添加 §4.2 M4G 子阶段；`README.md` 索引添加 M4G 实施计划链接。
- **不修改** `cf::plugin::PipeBuilder`、`PipeNode`、`Payload<T>`、`PayloadStore`、`CtrlLink`、`PipeArbitration` —— Oracle 评审确认这些是 OoO-friendly 的不变脊柱。

所有模板参数均有默认值（`N_REGS=32, N_THREADS=1`），现有调用点（`<typename T>`）零修改自动使用新签名。**没有 breaking change**（默认参数保证 ABI 兼容）。

## Capabilities

### New Capabilities

- `cpu-m4g-payload-extensibility`: 新增 `UID`/`THREAD_ID`/`IID_PC` 三个 Payload，为 Phase 5+ OoO（ROB index）和 SMT（线程 ID）预留识别字段。Phase 1 默认值 0，行为零变化。
- `cpu-plugin-template-thread-awareness`: `RegFilePlugin`、`HazardPlugin`、`BranchPredictorPlugin` 三个 ISA 无关插件模板化以支持 `N_REGS` 和 `N_THREADS` 参数，默认值保持当前 ABI。`predict`/`update`/`has_hazard` 等方法接受 `tid` 默认参数。
- `cpu-hazard-kind-enum`: `HazardPlugin::has_hazard` 返回类型从 `bool` 改为 `enum class HazardKind`，区分 `NONE`/`RAW_RS1`/`RAW_RS2`/`WAW` 四种状态，保留 `tid` 默认参数。

### Modified Capabilities

（无现有 spec，无 modified capabilities。M4G 是新增锁定，不修改现有 spec 行为。）

## Impact

- **影响文件**:
  - 修改: `ip/cpu/core/payload_common.h`（+3 行 Payload 声明）
  - 修改: `ip/cpu/plugins/reg_file.h`（~30 行模板化）
  - 修改: `ip/cpu/plugins/hazard.h`（~35 行模板化 + enum）
  - 修改: `ip/cpu/plugins/branch_predictor.h`（~40 行模板化 + tid 参数）
  - 新增: `tests/cpu/test_forward_compat.cpp`（~150 行，8+ 测试用例）
  - 修改: `ip/cpu/docs/blueprint.md`（§5 添加 M4G 标注）
  - 修改: `ip/cpu/docs/status.md`（添加 §4.2 M4G 状态）
  - 修改: `ip/cpu/docs/README.md`（索引添加 M4G 链接）
- **依赖与时序**:
  - 本 change 依赖 M4（✅ 已完成：19/19 ctest PASS）
  - 本 change 是 M4-DSE / M5-DSE 的前置依赖（必须在本 change 完成后启动 DSE 实施）
- **基线影响**:
  - 现有 ctest 不退化（M2 收官时 ctest 数量约 35，来自 `src/cf_plugin/CMakeLists.txt` 注册的 14 个 test_payload/pipe_node/pipe_builder/... + 14 个 cpu 单元测试 + 4 个 cache + 2 个 soc + 1 个 bundles。M4G 实施后通过 `ctest --test-dir build` 实测确认）
  - 新增 8+ ctest 验证 D.1-D.4
  - 编译时间预期增加 ~10%（acceptable，Oracle 评审确认）
- **运行时影响**: 零变化（默认 `N_REGS=32, N_THREADS=1`，Phase 1 行为完全保留）
- **API 影响**: 1 个 in-tree 业务调用者 break（`has_hazard` 返回类型变化，`hazard.h:126`）；**4 个测试断言需更新**（`tests/cpu/test_hazard.cpp` line 34/44/56/65 的 `assert(hz.has_hazard(dec))` 改为 `assert(... != HazardKind::NONE)`，见 tasks.md G.5.6）；**1 个内部硬编码需修正**（`ip/cpu/plugins/reg_file.cpp` 5 处 `pl::keys_rv32::*` 改为 T 推导，见 tasks.md G.2.8）；0 个外部 API break
- **breaking 变更**: 无外部 break。所有内部变更通过编译期类型检查暴露：1 个业务调用者 + 4 个测试断言 + 1 个内部硬编码，均在 tasks.md G.2.8 / G.5.5 / G.5.6 任务覆盖
- **不变量**: Phase 1 框架脊柱（`PipeBuilder`/`PipeNode`/`Payload<T>`/`PayloadStore`/`CtrlLink`/`PipeArbitration`）完全不修改，作为 OoO 扩展的硬约束

## Alternatives Considered

### Alternative A: 推迟 M4G 到 Phase 5（OoO 实施时再处理）

**放弃理由**: Phase 5 实施时再处理 = 在 OoO 代码基础上叠加 Phase 1 重构，冲突风险极高。Oracle 评审明确建议 M4G "should ship"：~108 行 header churn 防止 ~2000 行 Phase 5+ 重构。**不放弃**：立即落地。

### Alternative B: 实施 D.5/D.8/D.9（stage-name 成员、ThreadContext 结构、Cpu 类）

**放弃理由**: Oracle 评审明确判定这些是"投机性死代码"（无消费者）。实施会引入第二组合机制（破坏插件模型同构性）。**不放弃**：保持 D.1-D.4 范围，已从本 change 显式排除。

### Alternative C: 实施 §6 分支预测器工厂（独立工厂类）

**放弃理由**: Oracle 评审确认"破坏插件模型一致性"。BranchPredictorPlugin 模板化 + 默认参数已覆盖 DSE 配置需求，无需独立工厂。**不放弃**：用 D.2 模板化替代。
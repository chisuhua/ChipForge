# Phase 6c M6 — 5-Stage Simulator SEGV 修复 (PayloadStore Null-Impl 防御)

## Why

Phase 6c cf::plugin elaboration 模式 (CH_MEM) 在 `m4_poc_5stage_simulator_tick` 测试上 SEGV。
根因（已确诊）：`PayloadStore::get<T>()` 在 cell 缺失时调用 `T{}` 默认构造，对 `ch_uint<XLEN>` 和 `ch_bool` 产生 `impl_ = nullptr` 的句柄。下游任何算子（`==`、`!=`、`select` 等）都基于此 null 句柄构造，最终产生 `srcs_.size() == 0` 的 muximpl，`ch::Simulator::initialize()` 在 `muximpl::create_instruction` 调用 `false_value()->id()` 时解引用 nullptr，SEGV。

工具链已就绪（riscv64-unknown-elf-gcc 16.1.0 + verilator），环境不再是阻塞。

参考：Oracle 评审（ses_f4d2c8a6）推荐 **A + C 组合**，拒绝 B（placeholder literal 在 elaboration 模式下会污染硬件图，产生比 SEGV 更糟的静默错误硬件）。

## What Changes

### 框架级修复（方案 A）

- **payload.h::PayloadStore**：拆分读/写路径
  - `const T& get(key)` 读 miss 时**抛** `std::runtime_error("PayloadStore cell missing: <key_name> (CH_MEM elaboration: populate before at_stage accesses)")`
  - `T& get(key)` 非 const 写访问器**保留** `emplace-on-miss`（支持 `n(key) = v` 的写路径）
  - TLM 兼容模式保持原行为（`#else` 分支不变）
  - 文档更新：`// Phase 6c M6: CH_MEM 模式下 get() 读 miss 抛异常（fail-fast），写路径仍 emplace-on-miss 保持便利性`

### 工厂级修复（方案 C）

- **cpu_factory_chmem.h::build_cpu()**：在 `pb->build()` 之前，按 at_stage 闭包访问清单为每个阶段（fetch/decode/execute/memory/writeback/branch）显式预填充占位符
  - 使用真实 `ch_literal<0, W>{}` 字面值（非 null 句柄），这是**工厂层显式决策**而非框架隐式行为
  - 占位符与流水线取指逻辑无关——`writeback` 在 PoC 阶段本就不驱动真实 fetch

### Plugin 同步加固

- **branch_chmem.h / hazard_chmem.h / reg_file_chmem.h / int_alu_chmem.h**：检查所有 `n->operator()(KeyType::*)` 调用，确保依赖的 cell 在对应 stage 已被预填充（框架 A 已保证编译期/运行期异常；工厂 C 提供具体值）

### CI 检查（Check 8）

- **tests/framework/test_payload_store_miss_throws.cpp**（新增）：CH_MEM 模式下读未填充 cell 抛异常，消息含 key name；写后读正常；TLM 模式行为不变
- **tools/check_plugin_portability.sh**：Check 计数 7/7 → **8/8**（Check 8 = PayloadStore CH_MEM get miss 抛异常静态验证：编译时模板实例化断言）

## Acceptance Criteria

- [ ] `tests/framework/test_payload_store_miss_throws.cpp` PASS（3 个 assertion：抛异常/含 key name/TLM 兼容）
- [ ] `tests/cpu/test_cpu_5stage.cpp::m4_poc_5stage_simulator_tick` PASS（不 SEGV）
- [ ] `tests/cpu/test_cpu_5stage.cpp::m4_poc_5stage_build` PASS（保持）
- [ ] `tests/cpu/test_cpu_5stage.cpp::m4_poc_5stage_elaborate_verilog` PASS（保持）
- [ ] `tests/cpu/test_cpu_rtl_regfile_alu.cpp` 全部 PASS（保持）
- [ ] `bash tools/check_plugin_portability.sh` 显示 `8/8 PASS`
- [ ] `bash tools/verify_adr.sh` 显示 0 FAILED（保持）
- [ ] `bash tools/verify_plugin_decision.sh` PASS（保持）
- [ ] TLM 兼容测试（chipforge_tests，无 CH_MEM）回归：391 PASS（baseline 已知 12 失败）
- [ ] `commit + push origin main`，标题："fix(plugin): Phase 6c M6 5-stage Simulator SEGV (PayloadStore fail-fast + factory pre-populate)"

## Capabilities

- **cf::plugin/PayloadStore**：从 `T{} default` 升级为 `const-get 抛异常 + non-const-get emplace` 双路径
- **cf::plugin/cpu_factory**：从隐式依赖阶段 cell 升级为显式预填充声明（EARLY phase at_stage 闭包）
- **ChipForge Plugin 框架**：暴露所有隐式假设为显式行为，符合 D4 范式

## Impact

- **影响面**：cf::plugin PayloadStore（CH_MEM 路径）+ cpu_factory_chmem + 4 个 CH_MEM Plugin（branch/hazard/reg_file/int_alu）
- **破坏性**：CH_MEM 模式下 `n->operator()(KeyType::X) = v` 后**未**读取 X 的写法从静默通过变为抛异常（这是修复目的）。TLM 模式零影响。
- **测试**：[framework] 新增 1 测试，[cpu] 5-stage 测试从 FAIL → PASS，[cpu] rtl_regfile_alu 保持 PASS
- **不破坏的边界**：不修改 CppHDL（externally managed）；不动 TLM 兼容路径；不动 RegFile 已有单例 fix

## Tasks

1. **payload.h 拆分读写路径**（2 min）
   - 改 `const T& get(key)`：cell miss 时抛 `std::runtime_error`
   - 保留 `T& get(key)` 非 const emplace-on-miss
   - 更新头文件注释

2. **test_payload_store_miss_throws.cpp 框架单测**（3 min）
   - TEST_CASE 1: CH_MEM 模式 `get(key)` 未填 → 抛异常含 key.name()
   - TEST_CASE 2: CH_MEM 模式写后读 → 正常
   - TEST_CASE 3: TLM 兼容模式（不在 CH_MEM 编译）行为不变（通过 #ifdef 分支独立验证）

3. **tools/check_plugin_portability.sh Check 8**（2 min）
   - 新增检查：扫描 `src/cf_plugin/` 和 `include/cf/plugin/payload.h` 确认 CH_MEM 路径 get miss 抛异常（grep 静态验证）

4. **cpu_factory_chmem.h EARLY-stage 预填充**（5 min）
   - 在 `pb->build()` 之前注册 EARLY at_stage 闭包
   - 为 fetch/decode/execute/memory/writeback/branch 6 个 stage 填充 RS1/RS2/PC/RESULT/RD_DATA/DECODE 等 cell
   - 使用 `ch_uint<32>(ch_literal<0, 32>{})` 等真实字面值

5. **5-stage Simulator 验证 + 全回归**（5 min）
   - `m4_poc_5stage_simulator_tick` 必须 PASS
   - `[framework]` 全部 PASS（含新增 Check 8 单测）
   - `[cache]` `[cpu]` `[cpu-integration]` `[soc]` 全部 PASS（TLM 兼容回归）
   - `ctest` 双 target（chipforge_tests + chipforge_tests_chmem）

6. **commit + push**（2 min）
   - `git add -A` → `git commit -m "fix(plugin): Phase 6c M6 5-stage Simulator SEGV (PayloadStore fail-fast + factory pre-populate)"`
   - `git push origin main`

## 风险登记

| 风险 | 缓解 |
|------|------|
| TLM 测试隐式依赖读 miss 默认值 | 任务 5 全回归；若有命中，Check 8b 单测保护 |
| 框架异常在某些 at_stage 闭包中途抛出 → 留下半建图 | 可接受（elaboration 全有或全无，现状 SEGV 更糟） |
| 工厂预填充样板在每 stage 重复，新 stage 遗忘 | A 的异常即防御；后续 M7+ 可加 `stage_required_cell_keys` 声明 API |
| 5 文件未 commit（M5 文档收口）与 M6 修复冲突 | M6 commit 包含全部（M5 + M6 一并提交） |
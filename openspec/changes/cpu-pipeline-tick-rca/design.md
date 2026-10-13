## Context

**当前状态**:
- 5 个 CPU 仿真测试 FAIL (调用 `./build/src/cf_plugin/cpu_sim` CLI)
- C++ API 路径 (`test_rv32ui_runner.cpp` 直接调 `CpuFactory::build_cpu + pb->run()`) 跑同一 `rv32ui-p-add` ELF **48/48 PASS**
- 实测症状: PC 通过 writeback +4 推进 (证明 `pb->run()` 调了), memory 节点 payload (DECODE/INSTRUCTION) 全 0 (证明 decode/memory 阶段 at_stage 闭包未触发或未传播)
- `cpu-sim-add-elf-tohost-fix` change (前置, 2026-10-12) 已落地工具层 workaround, 但根因未修

**约束**:
- **纯诊断 change** — 不动 product code 修复
- 临时 debug log 允许, 但必须 revert (tasks.md 显式 checkbox)
- 已有 working path (C++ API runner) 作为正确性参考
- 不动共享头文件 (避免与 `mmu-chmem-pipeline-integration` 冲突)
- 不重写 `StageLinkPlugin` (HIGH 风险, 仅 L1/L2/L4 排除后才考虑)

**Stakeholders**:
- CPU IP owner (v0.11.0 启动 owner, 消费诊断结论)
- `mmu-chmem-pipeline-integration` change owner (协调避免共享头文件改动)
- 后续 `cpu-pipeline-tick-fix` change owner (基于本 change 实施修复)

## Goals / Non-Goals

**Goals**:
- 调查 4 候选根因, 给出每条的 confirmed / excluded / partial 判定
- 综合最可能根因, 输出可执行的修复建议 (含具体 patch sketch)
- 0 regression: `[riscv-tests] 48/48`, `[cpu-l1-mmu-demo] 6/6`, `[cpu-integration][mul-div-fsm-integration] 4/4` 全部保持 PASS
- 0 product code 改动 (除临时 debug log, 必 revert)
- 输出可复现诊断方法 (后续 change 可引用)

**Non-Goals**:
- 不修根因 (留给 `cpu-pipeline-tick-fix` change)
- 不改 `cfg.enable_mmu` 默认值 (待 L4 结论定)
- 不重写 `StageLinkPlugin` (HIGH 风险, 仅排除后才考虑)
- 不修复 `test_dhrystone` SIGTERM (独立 issue)
- 不动 `mfc-extract-fsm-h` (v0.11.0 推迟 scope)

## 4 调查线 + 1 Control Experiment 设计

### 调查线排序与 rationale

| 序 | 线 | 候选根因 | 信心度 (Metis + Oracle 综合) | 排序理由 |
|---|---|---|---|---|
| **0** | **Control Experiment** | `cfg.enable_mmu` 分歧 | **关键判别** | 1 个 build+run 实验即可判别 L1/L2/L3 是否为根因; 必前置 |
| **1** | **L4** (新加) | `cfg.enable_mmu` CLI 默认 true vs runner 显式 false | **HIGH (~55-70%)** | Metis + Oracle 双复审一致识别为最高优先遗漏候选; control experiment 直接验证 |
| **2** | **L2** | `ibus.h:54-67` fetch CtrlLink `halt_when` 在 `tlb_lookup_ifetch` 节点存在时是否误判 | **MEDIUM (~25-30%)** | 症状签名高度吻合 (PC 推进但 payload 全 0 = fetch stalled); 需静态 + runtime 验证 |
| **3** | **L3** | `stage_link.h` EARLY 闭包在 5 段链上 payload 跨阶段传播 | **LOW (~10-15%)** | 两路径注册顺序相同, EARLY 顺序确定; 间接嫌疑, 仅 L1/L2/L4 排除后深查 |
| **4** | **L1** (降级) | `build_cpu_impl` Result 包装 + `auto_throw` 解包 + `std::unique_ptr<PipeBuilder>` | **VERY LOW (~5-10%)** | 二者极薄, 无 tick 差异机制; 仅 5 分钟静态排除 |

### L4 (Config Divergence) — 新加最高优先

**假设**:
- `tools/cpu_sim/main.cpp:load_config()` **不解析 `enable_mmu` 字段** (只解析 7 字段: `isa`/`pipeline_stages`/`mul_latency`/`mul_impl`/`dispatch_width`/`n_lanes`/`icache_latency`/`dcache_latency`/`n_threads`)
- `cfg.enable_mmu` 保留 `CPUConfig` struct 默认值 = `true` (cpu_factory.h:136)
- 启用 MMU → `RiscvMMUPlugin` 注册 → `tlb_lookup_ifetch` 节点存在 → fetch CtrlLink `halt_when` 评估 `PTW_ACTIVE`
- 而 `test_rv32ui_runner.cpp:109` 显式 `cfg.enable_mmu = false`, 不注册 MMU plugin
- **真差异** 在 config 而非 build 路径

**control experiment** (必前置):
1. 临时加 `tools/cpu_sim/main.cpp` 接受 `--no-mmu` flag (不允许 commit)
2. 跑 `cpu_sim --no-mmu --elf build/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 1000`
3. **若 tohost=1**: L4 confirmed, 根因即 config 分歧, L2/L3/L1 全部排除
4. **若 tohost=0**: L4 excluded, 进入 L2/L3/L1 调查

**静态证据** (无需 runtime):
- `tools/cpu_sim/main.cpp:88-111` `load_config()` 不读 `enable_mmu`
- `ip/cpu/cpu_factory.h:136` `enable_mmu` 默认 `true`
- `test_rv32ui_runner.cpp:109` runner 显式 `false`

**fallback** (如 control experiment 不可执行):
- 跑 `cpu_sim --elf <riscv-tests-elf>` 看是否复现 FAIL (riscv-tests-rv32ui-p-add 在 cpu_sim CLI 下应 FAIL, 因为 CLI enable_mmu=true)
- 与 C++ API 路径 (PASS) 对比, 确认差异在 CPUConfig 而非 ELF 或其他

### L2 (fetch CtrlLink halt_when)

**假设**:
- `ibus.h:54-67` 注册 `fetch_ctrl->halt_when([&pb]() { ... PTW_ACTIVE ... })`
- 当 `enable_mmu=true` 时 `tlb_lookup_ifetch` 节点存在, `PTW_ACTIVE` 读
- 初始 cycle `PTW_ACTIVE` 可能未初始化 (PayloadStore fail-fast 抛 v0.3.1 M6) 或误判为 truthy
- fetch stage 永远 stall → decode/memory 阶段不调 → payload 全 0
- PC 仍推进因为 writeback 不受 fetch stall 影响 (writeback 是另一阶段)

**静态证据** (无需 runtime):
- `ip/cpu/plugins/ibus.h:54-67` halt_when lambda 完整代码
- `ip/cpu/plugins/mmu.h` `RiscvMMUPlugin` 注册逻辑
- `ip/cpu/plugins/mmu_exception_handler.h` (TLM 版, **非** `_chmem.h`) — 必须区分, 不读 CHMEM 文件

**runtime 验证** (临时 debug log, 必 revert):
- **Log 点 1**: `cf::plugin::pipe_builder.h:185-187` `should_stall_stage` 调用前打印 "STALL CHECK: stage=X result=Y"
- **Log 点 2**: `tools/cpu_sim/main.cpp` 在 `pb->build()` 后 dump `pb->ctrl_link_count("fetch")` + `pb->ctrl_link_count("decode")` + 有效 config 全字段
- **Log 点 3**: `ip/cpu/plugins/ibus.h:62-66` halt_when lambda 内打印 "halt_when called, PTW_ACTIVE=X, tlb_node_exists=Y"

**fallback** (如修改 framework 不可逆):
- 跑 cpu_sim 在 `enable_mmu=true` 和 `false` (临时 --no-mmu) 下对比 stall 计数
- 对比 stall 触发条件

### L3 (StageLinkPlugin EARLY)

**假设**:
- `ip/cpu/plugins/stage_link.h` EARLY 闭包在 fetch→decode→execute→memory→writeback 链上传递 payload
- 如果 EARLY 闭包未注册或注册顺序错, payload 跨阶段不传播
- 两路径 (CLI vs C++ API) 注册顺序应相同 (CpuFactory::build_cpu 内部)

**静态证据** (无需 runtime):
- `ip/cpu/plugins/stage_link.h` EARLY 闭包注册代码
- `ip/cpu/cpu_factory.h` `register_early_plugins` 调用顺序

**runtime 验证** (临时 debug log, 必 revert):
- **Log 点 4**: StageLink EARLY 闭包入口 dump 源/目标节点的 PC + INSTRUCTION
- **Log 点 5**: `pb->run()` 结束时 dump fetch/decode/memory/writeback 节点 PC + INSTRUCTION 终态

**fallback**: 仅在 L1/L2/L4 排除后启动

### L1 (build_cpu_impl + auto_throw) — 降级

**假设**:
- `build_cpu_impl` 是 `try { return build_cpu(); } catch { return unexpected; }` 薄包装 (cpu_factory.h:406-418)
- `auto_throw` 解包 Result, 失败 throw (见 result_macros.h:76-81)
- **无 tick 行为差异机制** — 几乎不可能是根因

**静态证据** (5 分钟):
- `ip/cpu/cpu_factory.h:406-418` build_cpu_impl 完整代码
- `include/cf/plugin/result_macros.h:76-81` auto_throw 实现
- 结论: 二者对 `pb->run()` 主循环无副作用, 几乎可立即 excluded

**L1 降级为 1 个静态结论**: 5 分钟内读完 2 个文件, 写 "excluded" 块, 立即继续 L2/L3

## Risks / Trade-offs

**[R1] 临时 debug log 残留污染** → Mitigation: tasks 显式 "revert" checkbox + archive 前 `git status --porcelain` 校验 + 架构 gate 兜底 (revert 不净会触发 verify 失败)

**[R2] `mmu-chmem-pipeline-integration` 共享头文件冲突** → Mitigation: tasks 顶部"文件白名单 + 排除清单"固定不读 `_chmem.h` 文件, 不改 `payload_common.h` 等共享头; 仅读 TLM `*.h` 验证

**[R3] L2 调查触及 framework 边界 (`pipe_builder.h`)** → Mitigation: 仅加 `cerr` log, 不改 control flow; log 用 `if (i < 50) break` 限制输出量; 不提交到 HEAD

**[R4] Control experiment 临时加 `--no-mmu` flag 误 commit** → Mitigation: 独立 patch 文件 + `git stash` + tasks "revert & verify" checkbox

**[R5] L3 触发 StageLink 重写 scope creep** → Mitigation: L3 仅静态 + log, **不修改** StageLink 代码; 如需重写, 留给 `cpu-pipeline-tick-fix` change

**[R6] 调查方向选择错误 (L4 实际不是根因)** → Mitigation: 4 调查线每线必须有 stop-and-document 块; L4 control experiment 失败立即 pivot 到 L2; L1/L2 失败 pivot 到 L3; 任何线 "inconclusive" 状态保留, 不强制结论

## Migration Plan

**部署**:
1. 创建 `openspec/changes/cpu-pipeline-tick-rca/` 目录 + 6 个 artifacts
2. 执行 control experiment (临时加 `--no-mmu`, 跑测试, 必 revert)
3. 按 L4 → L2 → L3 → L1 顺序调查, 每线写 stop-and-document 块
4. 综合 design.md §3 写最可能根因 + 修复建议
5. 跑架构 gate 0 FAIL + `[riscv-tests] 48/48` + 5 FAIL 状态保持
6. 跑 `git status --porcelain` 校验仅 openspec artifacts, 无 product code 改动
7. commit change artifacts (无 product code commit)

**回滚**:
- 单 commit revert 即可 (无 product code 改动, 仅 change artifacts)
- revert 后状态: 5 FAIL 保持, 无副作用

**顺序**:
- 阶段 0: 准备工作 (tasks 顶部白名单 + change artifacts 初始化)
- 阶段 1: control experiment (L4 验证)
- 阶段 2: L4 调查 + stop-and-document
- 阶段 3: L2 调查 (仅在 L4 排除后)
- 阶段 4: L3 调查 (仅在 L1/L2 排除后)
- 阶段 5: L1 静态排除 (5 分钟, 任何时候)
- 阶段 6: 综合诊断 + 验证 + 文档同步

## Open Questions (继承并扩展)

**OQ1** (从 cpu-sim-add-elf-tohost-fix 转移): pipeline 不 tick 真根因 → 本 change L4/L2/L3/L1 调查输出

**OQ2** (新): `cfg.enable_mmu` 默认值是否应改?
- 如果 L4 confirmed: `cpu-pipeline-tick-fix` 改 `CPUConfig::enable_mmu` 默认 `false` (Bare-friendly default) + `load_config()` 解析 enable_mmu 字段
- 如果 L4 excluded: 改 cpu_sim CLI 显式 pin `cfg.enable_mmu = false` 作为 work-around
- 需与 `mmu-chmem-pipeline-integration` 协调 (mmu tests 期望 enable_mmu=true)

**OQ3** (新): L2 fetch CtrlLink `halt_when` lambda 是否需 Bare-mode guard?
- 候选: `if (!tlb_n || tlb_n is bare) return false;` (避免在 Bare mode 误读 PTW_ACTIVE)
- 需 L2 调查结论定

**OQ4** (新): L3 StageLink 是否需重新审视?
- 仅在 L1/L2/L4 全部 excluded 后启动
- 需 deep-dive 设计, 不在本 change scope

## 附录: Metis + Oracle 复审意见整合

### Metis 关键意见 (Plan Consultant, 2026-10-12)

1. **Hidden intentions 风险**: 前提可能失真 (load_config 不解析 enable_mmu, runner 显式 false), 3 调查线未覆盖 config 分歧
2. **AI failure points**: framework rabbit hole, debug log 清理, 静态/dynamic 边界
3. **Scope check**:
   - L1 降级 (极薄包装, 无 tick 差异机制)
   - L2 触及 `mmu_exception_handler_chmem.h:62` (CH_MEM) — 5 FAIL 走 TLM, 非必要勿扩入
   - L3 间接嫌疑, 需 L1/L2/L4 排除后
4. **Plan-level concerns**: tasks 12-20 粗粒度, 缺 L1→L2→L3 stop-and-document 节点, 需"已读清单 + 排除清单"
5. **总体**: NEEDS REVISION — 新增 Step 0 control experiment, L1 降级, dynamic 实验白名单 + revert 校验, per-line 收敛块

### Oracle 关键意见 (High-IQ Consultant, 2026-10-12)

1. **真根因最可能位置**: L2 (fetch CtrlLink halt_when) 信心 ~55%, L3 ~25%, L1 ~10%, 遗漏 L4 ~10%
2. **三条调查线是否穷举**: 未穷举, 遗漏 L4 (config 分歧), 排序建议 L4 > L2 > L3 > L1
3. **静态 vs debug log 边界**:
   - 可纯静态: L4 (config diff), L1 (wrapper 语义)
   - 必须 runtime: stall stage 名, config 全字段 dump, StageLink EARLY fetch/decode dump
4. **Framework 诊断陷阱**:
   - `pb->run()` 无 CLI vs API 行为差异, 差异来自输入
   - `unique_ptr` 在 main 作用域 vs auto var RAII 等价
   - `auto_throw` 吞掉错误明细 (`to_exception(err, "")`)
5. **修复方案风险**: L1 LOW (但可能不修根因), L2 MEDIUM (Bare-mode byte-identical), L3 HIGH (StageLink 重写)
6. **依赖风险**: mfc-extract-fsm-h LOW, mmu-chmem (需协调若 L3 触及), mmufault-handler v1 (throw 路径 terminate 风险)
7. **总体**: NEEDS REVISION — 补 L4, 调排序, 加 log 点, revert gate

### 整合决策

- **采纳** L4 新增 + 升最高优先 (Metis + Oracle 一致)
- **采纳** L1 降级为 5 分钟静态排除 (Metis + Oracle 一致)
- **采纳** control experiment 必前置 (Metis 建议 + Oracle 同意)
- **采纳** 排序 L4 > L2 > L3 > L1 (Oracle 排序)
- **采纳** 文件白名单 + 排除清单 (Metis 建议)
- **采纳** per-line × 步 + stop-and-document (Metis 建议)
- **采纳** 3 个 runtime log 点 (Oracle 详细) + revert gate
- **拒绝** 修复建议含 patch (本 change 是纯诊断, 修复留给 `cpu-pipeline-tick-fix`)
- **拒绝** L3 StageLink 重写 (HIGH 风险, 留给后续 change)

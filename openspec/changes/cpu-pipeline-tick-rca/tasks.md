# Tasks: cpu-pipeline-tick-rca

> **纯诊断 change, 不动 product code 修复**. 4 调查线 (L4 config divergence > L2 fetch CtrlLink > L3 StageLink EARLY > L1 build_cpu_impl) + 1 control experiment. 每线 stop-and-document, 临时 debug log 必 revert. 0 regression 硬约束.

## 0. 准备工作 (前置)

- [ ] 0.1 初始化 change workspace (本 change artifacts 已就位: `openspec/chases/cpu-pipeline-tick-rca/{proposal,design,tasks}.md` + `specs/`)
- [ ] 0.2 跑 `bash tools/verify_adr.sh` + `verify_plugin_decision.sh` + `check_plugin_portability.sh` 确认 baseline 0 FAIL (33+8+12)
- [ ] 0.3 跑 `./build/bin/chipforge_tests [riscv-tests]` 确认 baseline 48/48 PASS
- [ ] 0.4 跑 `./build/bin/chipforge_tests [cpu]` 确认 baseline 128/129 (1 FAIL cpu_sim_real_tohost)
- [ ] 0.5 跑 `./build/bin/chipforge_tests [cpu-l1-mmu-demo]` 确认 baseline 6/6 PASS
- [ ] 0.6 跑 `./build/bin/chipforge_tests [cpu-integration][mul-div-fsm-integration]` 确认 baseline 4/4 PASS
- [ ] 0.7 **固化文件白名单 + 排除清单** (在 design.md §附录 写入):
  - **白名单 (可读)**: `tools/cpu_sim/main.cpp`, `ip/cpu/cpu_factory.h`, `ip/cpu/plugins/ibus.h`, `ip/cpu/plugins/dbus.h`, `ip/cpu/plugins/stage_link.h`, `ip/cpu/plugins/mmu.h`, `ip/cpu/plugins/mmu_exception_handler.h` (TLM), `include/cf/plugin/pipe_builder.h`, `include/cf/plugin/result_macros.h`, `ip/cpu/picolibc_host_memory.h`
  - **排除清单 (不读不改)**: `src/cf_plugin/bridge/*`, `_chmem.h` (CH_MEM), `build/_deps/**` (CppTLM/CppHDL 源码), `mmu_chmem.h` (Phase 6d 在途), `ip/mmu/**` (除 `mmu.h` / `mmu_exception_handler.h`)

## 1. Control Experiment (L4 判别 — 必前置)

> **目的**: 1 个 build+run 实验快速判别 L4 (config divergence) 是否为根因. 若 tohost=1 → L4 confirmed, L2/L3/L1 全部排除; 若 tohost=0 → L4 excluded, 进入 L2/L3/L1.

- [ ] 1.1 **临时加 `--no-mmu` CLI flag** (patch 文件管理, 禁止 commit):
  - 改 `tools/cpu_sim/main.cpp` argv 解析接受 `--no-mmu`
  - 当指定时设 `cfg.enable_mmu = false` (覆盖 JSON 默认 true)
  - 验证编译: `cmake --build build --target cpu_sim -j4` 成功
- [ ] 1.2 **跑 control experiment**:
  - 命令: `./build/src/cf_plugin/cpu_sim --no-mmu --elf build/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 1000`
  - **记录输出**: cycles / tohost / pipeline_stages
- [ ] 1.3 **判定**:
  - **若 tohost=1**: L4 confirmed → 跳到 §5 综合诊断
  - **若 tohost=0**: L4 excluded → 继续 §2 (L2 调查)
- [ ] 1.4 **revert `--no-mmu` 改动** (必须):
  - `git checkout tools/cpu_sim/main.cpp` 还原
  - `cmake --build build --target cpu_sim -j4` 重编译确认无残留
  - 跑 baseline `./build/bin/chipforge_tests [cpu]` 确认 128/129 状态保持
- [ ] 1.5 **stop-and-document 块** (在 design.md §1.4 写入):
  - 结论: L4 [confirmed / excluded / partial]
  - 证据: control experiment 命令 + 输出 (5 行内)
  - 信心度: HIGH / MEDIUM / LOW
  - 下一步: pivot 到 L2 / 跳到 §5

## 2. L2 调查 (fetch CtrlLink halt_when)

> **前提**: §1.3 L4 excluded 后启动. 若 L4 confirmed, 跳过本节.

- [ ] 2.1 **静态读 `ip/cpu/plugins/ibus.h:54-67` halt_when lambda**:
  - 完整记录 lambda 条件: `if (!n) return false; return static_cast<bool>((*n)(MmuKeys::PTW_ACTIVE));`
  - 记录: `*n = node_of_logic_stage("tlb_lookup_ifetch")` 在 enable_mmu=false 时返回 nullptr, halt 返回 false (no stall)
  - 记录: enable_mmu=true 时 tlb_lookup_ifetch 节点存在, `PTW_ACTIVE` 读取决于 init 时序
- [ ] 2.2 **静态读 `ip/cpu/cpu_factory.h` `register_early_plugins`** (L420-461):
  - 记录: enable_mmu=true 时 RiscvMMUPlugin + MmuExceptionHandlerPlugin 注册顺序
  - 记录: IBusPlugin 注册后于 MMUPlugin (canonical ordering ADR-048)
  - 记录: `tlb_lookup_ifetch` 节点何时被 register, 是否在 `pb->build()` 前已存在
- [ ] 2.3 **临时 log 点 1** (pipe_builder.h stall 分支):
  - **patch 文件**: `tools/cpu_sim/main.cpp` 在 `pb->build()` 后加:
    ```cpp
    std::cerr << "DEBUG: ctrl_link_count(fetch)=" << pb->ctrl_link_count("fetch") << "\n";
    std::cerr << "DEBUG: ctrl_link_count(decode)=" << pb->ctrl_link_count("decode") << "\n";
    std::cerr << "DEBUG: cfg.enable_mmu=" << cfg.enable_mmu << "\n";
    ```
  - 编译 + 跑 cpu_sim (不带 --no-mmu, 默认 enable_mmu=true) 1000 cycles
  - **记录**: ctrl_link_count(fetch) 是否 > 0
- [ ] 2.4 **临时 log 点 2** (halt_when lambda 内):
  - **patch 文件**: `ip/cpu/plugins/ibus.h:62-66` lambda 内加:
    ```cpp
    std::cerr << "DEBUG: halt_when called, tlb_node=" << (n?"Y":"N") << " PTW_ACTIVE=" << (n?static_cast<bool>((*n)(MmuKeys::PTW_ACTIVE)):-1) << "\n";
    ```
  - 编译 + 跑 cpu_sim 1000 cycles, 输出限流 `if (cycle < 20) print`
  - **记录**: tlb_node 存在性, PTW_ACTIVE 值, halt_when 返回值
- [ ] 2.5 **判定**:
  - **若 halt_when 返回 true 持续**: L2 confirmed (stall 永远触发)
  - **若 halt_when 返回 true 仅初始 cycle**: L2 partial (init 时序问题)
  - **若 halt_when 永远 false**: L2 excluded (fetch 不 stall)
- [ ] 2.6 **revert 临时 log** (必须):
  - `git checkout ip/cpu/plugins/ibus.h` 还原
  - `git checkout tools/cpu_sim/main.cpp` 还原
  - 重编译, 跑 baseline 验证 0 regression
- [ ] 2.7 **stop-and-document 块** (在 design.md §2.5 写入):
  - 结论: L2 [confirmed / excluded / partial]
  - 证据: log 输出 (10 行内)
  - 信心度: HIGH / MEDIUM / LOW
  - 下一步: pivot 到 L3 / 跳到 §5

## 3. L3 调查 (StageLink EARLY)

> **前提**: §1.3 L4 excluded AND §2.5 L2 excluded 后启动. 仅在两线都排除后才深查, 避免 scope creep.

- [ ] 3.1 **静态读 `ip/cpu/plugins/stage_link.h` EARLY 闭包**:
  - 记录: 5 段 (fetch/decode/execute/memory/writeback) EARLY 注册顺序
  - 记录: payload key 传播路径
- [ ] 3.2 **静态读 `ip/cpu/cpu_factory.h` `build_cpu` 主流程** (L370-398):
  - 记录: StageLinkPlugin 注册位置 (相对 TopologyBuilder / MMUPlugin / IBusPlugin)
  - 记录: `pb->build()` 调用顺序
- [ ] 3.3 **临时 log 点 3** (StageLink EARLY 入口):
  - **patch 文件**: StageLink 闭包内加 `cerr` dump 源/目标节点 PC + INSTRUCTION
  - 输出限流 `if (cycle < 20 && i % 5 == 0) print`
  - 编译 + 跑 cpu_sim 1000 cycles
  - **记录**: PC / INSTRUCTION 是否跨阶段传播
- [ ] 3.4 **判定**:
  - **若 payload 跨阶段不传播**: L3 confirmed
  - **若 payload 正常传播**: L3 excluded
- [ ] 3.5 **revert 临时 log** (必须):
  - `git checkout ip/cpu/plugins/stage_link.h` (或对应位置)
  - 重编译, 跑 baseline 验证
- [ ] 3.6 **stop-and-document 块** (在 design.md §3.4 写入):
  - 结论: L3 [confirmed / excluded]
  - 证据: log 输出 (10 行内)
  - 信心度: HIGH / MEDIUM / LOW
  - 下一步: 跳到 §5

## 4. L1 调查 (build_cpu_impl + auto_throw) — 降级

> **任何时候可执行** (5 分钟, 无 control experiment 依赖). 结论几乎必然是 "excluded".

- [ ] 4.1 **静态读 `ip/cpu/cpu_factory.h:406-418` build_cpu_impl**:
  - 记录: `try { return build_cpu(config, mem); } catch { return unexpected; }`
  - 结论: 极薄 try/catch 委托, 无运行时行为差异
- [ ] 4.2 **静态读 `include/cf/plugin/result_macros.h:76-81` auto_throw**:
  - 记录: 解包 Result, 失败 throw
  - 结论: 对 `pb->run()` 主循环无副作用
- [ ] 4.3 **静态读 unique_ptr 在 main 作用域 vs auto var**:
  - 记录: RAII 行为等价, 析构时机相同
  - 结论: 无 tick 差异机制
- [ ] 4.4 **stop-and-document 块** (在 design.md §4.3 写入):
  - 结论: L1 **excluded** (静态分析已足够, 无需 runtime)
  - 证据: 2 个函数 + 1 个类型 (10 行内)
  - 信心度: HIGH (5 分钟静态排除)

## 5. 综合诊断

- [ ] 5.1 **汇总 4 调查线结论** (在 design.md §5.1 写入):
  - L4: [confirmed / excluded] + evidence
  - L2: [confirmed / excluded / partial] + evidence
  - L3: [confirmed / excluded] + evidence
  - L1: **excluded** + evidence
- [ ] 5.2 **最可能根因** (在 design.md §5.2 写入):
  - 综合排序: 列出 ranked 候选 (按 confidence)
  - 每候选含: 触发条件 / 触发路径 / 影响范围
- [ ] 5.3 **修复建议** (在 design.md §5.3 写入):
  - **抽象** (不写 patch, 留给 `cpu-pipeline-tick-fix`):
    - 候选 1 (L4 confirmed): `load_config()` 解析 `enable_mmu` + `CPUConfig::enable_mmu` 默认改 `false` + cpu_sim 显式 pin
    - 候选 2 (L2 confirmed): `ibus.h` halt_when lambda 加 Bare-mode guard `if (!tlb_n) return false;` (但当前已是此逻辑, 可能需更深)
    - 候选 3 (L3 confirmed): StageLink 重新审视 (本 change 不动, 留给后续)
  - 每候选含: risk assessment (LOW/MEDIUM/HIGH) + 实施复杂度 + 与在途 change 协调需求
- [ ] 5.4 **`cpu-pipeline-tick-fix` change 启动建议** (在 design.md §5.4 写入):
  - 优先级 + 与 `mmu-chmem-pipeline-integration` 协调点
  - OQ1/OQ2/OQ3 答案 (从调查结论抽取)

## 6. 验证 & 收尾

- [ ] 6.1 **跑架构 gate**: `bash tools/verify_adr.sh` + `verify_plugin_decision.sh` + `check_plugin_portability.sh` — 必 0 FAIL
- [ ] 6.2 **跑测试 baseline** (0 regression 硬约束):
  - `./build/bin/chipforge_tests [riscv-tests]` 必 48/48 PASS
  - `./build/bin/chipforge_tests [cpu]` 必 128/129 (1 FAIL 状态保持)
  - `./build/bin/chipforge_tests [cpu-integration]` 必 30/35 (4 FAIL + 1 SIGTERM 状态保持)
  - `./build/bin/chipforge_tests [cpu-l1-mmu-demo]` 必 6/6 PASS
  - `./build/bin/chipforge_tests [cpu-integration][mul-div-fsm-integration]` 必 4/4 PASS
- [ ] 6.3 **`git status --porcelain` 校验无 product code 改动**:
  - 仅 `openspec/changes/cpu-pipeline-tick-rca/` 内文件 + (可选) `AGENTS.md` + `ip/cpu/plugins/mmu_exception_handler_chmem.h` 注释
  - **不允许**: 任何 `tools/`, `ip/cpu/`, `src/cf_plugin/`, `tests/`, `soc/` 下的 *.cpp/*.h 改动
- [ ] 6.4 **更新 `AGENTS.md` §known test status**:
  - 5 FAIL 描述从 "根因 defer Phase 6d.6+ cpu-pipeline-tick-rca change" 改为 "诊断中, see openspec/changes/cpu-pipeline-tick-rca/, 真根因 [L4/L2/L3 之一]"
- [ ] 6.5 **更新 `ip/cpu/plugins/mmu_exception_handler_chmem.h:26` 注释**:
  - 从 "defer Phase 6d.6+ cpu-pipeline-tick-rca change" 改为 "defer Phase 6d.6+ cpu-pipeline-tick-fix, 诊断 see cpu-pipeline-tick-rca"
- [ ] 6.6 **commit change artifacts**:
  - `git add openspec/changes/cpu-pipeline-tick-rca/`
  - `git add AGENTS.md` (if updated)
  - `git add ip/cpu/plugins/mmu_exception_handler_chmem.h` (if updated)
  - **禁止**: `git add tools/ ip/cpu/ src/cf_plugin/ tests/ soc/`
  - commit message: `diag(cpu): cpu-pipeline-tick-rca — 诊断 cpu_sim CLI 路径 pipeline 不 tick 根因`
- [ ] 6.7 (可选) **更新 tasks.md checkboxes** 标记所有 §0-5 已完成, §6 已 done
- [ ] 6.8 (可选) **写 changelog entry** in `CHANGELOG.md` 记录诊断结论 (与 `cpu-sim-add-elf-tohost-fix` 同位)

## 依赖关系

```
0.1 → 0.2 → 0.3 → 0.4 → 0.5 → 0.6 → 0.7 (白名单固化)
                  ↓
        1.1 → 1.2 → 1.3 (L4 判定) → 1.4 (revert) → 1.5 (doc)
                                          ↓
                            L4 confirmed → 跳到 §5
                            L4 excluded → ↓
        2.1 → 2.2 → 2.3 (log 1) → 2.4 (log 2) → 2.5 (L2 判定)
                                          ↓
                L2 confirmed → 跳到 §5
                L2 excluded → ↓
        3.1 → 3.2 → 3.3 (log 3) → 3.4 (L3 判定)
                                          ↓
                L3 confirmed → 跳到 §5
                L3 excluded → ↓
        4.1 → 4.2 → 4.3 (L1 静态排除, any-time) → 4.4 (doc)
                                          ↓
                5.1 → 5.2 → 5.3 → 5.4 (综合)
                                          ↓
        6.1 → 6.2 → 6.3 → 6.4 → 6.5 → 6.6 → 6.7 → 6.8
```

## 总任务数

38+ checkbox (7 阶段). 估计 1-2 session 完成 (control experiment 30 分钟, L2 1 小时, L3 30 分钟, L1 5 分钟, 综合 + 验证 30 分钟).

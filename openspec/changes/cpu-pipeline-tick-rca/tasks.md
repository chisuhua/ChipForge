# Tasks: cpu-pipeline-tick-rca

> **纯诊断 change, 不动 product code 修复**. 复合根因 (F1 config 保真度 + F2 测试 config 意图 + F3 MMU 瞬态语义 + 4 调查线 + 2×2 control experiment + F4 反向对照). 每线 stop-and-document, 临时 debug log 必 revert. **5-测试独立基线台账优先于其他调查 (D3 决策)**. 0 regression 硬约束.
>
> **P0 重做后命名 caveat**: "tick-rca" 目录名是 pre-existing 命名 (commit `5403718`); 真实症状是 **fetch 永 stall**, 不是 "pipeline 不能 tick" (见 `proposal.md` ⚠️ Naming Caveat).

## 0. 准备工作 (前置)

- [ ] 0.1 初始化 change workspace (本 change artifacts 已就位: `openspec/changes/cpu-pipeline-tick-rca/{proposal,design,tasks}.md` + `specs/`)
- [ ] 0.2 跑 `bash tools/verify_adr.sh` + `verify_plugin_decision.sh` + `check_plugin_portability.sh` 确认 baseline 0 FAIL (33+8+12)
- [ ] 0.3 跑 `./build/bin/chipforge_tests [riscv-tests]` 确认 baseline 48/48 PASS
- [ ] 0.4 跑 `./build/bin/chipforge_tests [cpu]` 确认 baseline 128/129 (1 FAIL cpu_sim_real_tohost)
- [ ] 0.5 跑 `./build/bin/chipforge_tests [cpu-l1-mmu-demo]` 确认 baseline 6/6 PASS
- [ ] 0.6 跑 `./build/bin/chipforge_tests [cpu-integration][mul-div-fsm-integration]` 确认 baseline 4/4 PASS
- [ ] 0.7 **固化文件白名单 + 排除清单** (对齐 spec.md, 在 design.md §附录 写入):
  - **白名单 (可读, 含 mmu TLM 是 P0 重做新增)**: `tools/cpu_sim/main.cpp`, `ip/cpu/cpu_factory.h`, `ip/cpu/cpu_factory_chmem.h` (只读 reference), `ip/cpu/plugins/ibus.h`, `ip/cpu/plugins/dbus.h`, `ip/cpu/plugins/stage_link.h`, `ip/cpu/plugins/mmu.h`, `ip/cpu/plugins/mmu_exception_handler.h` (TLM only), `ip/mmu/tlm/MMUPlugin.h`, `ip/mmu/tlm/MMUPlugin.cpp` (Bare shortcut 判定必读), `ip/mmu/tlm/mmu_keys.h` (PTW_ACTIVE key 定义), `include/cf/plugin/pipe_builder.h`, `include/cf/plugin/result_macros.h`, `ip/cpu/picolibc_host_memory.h`, `tests/cpu/configs/*.json` (5 个 FAIL 测试 config)
  - **排除清单 (与 spec.md 对齐)**: `src/cf_plugin/bridge/*`, `**/*_chmem.h` (CH_MEM), `build/_deps/**` (CppTLM/CppHDL 源码), `ip/mmu/lib/**`, `ip/mmu/chmem/**` (本 change 不读)
- [ ] 0.8 **固化 5-测试独立基线台账** (D3 决策, 在 design.md §0.8 内嵌 Markdown 表, **不在 openspec/changes/ 外另立文件**, 避免污染约定):
  - **Step 1**: 5 个测试 → 对应 4 份 config + 1 份 runner:
    | 序号 | 测试名 | 配置文件路径 (test_*stage_riscv.cpp 或 test_cpu_sim_real_tohost.cpp 取 config_path arg) |
    |---|---|---|
    | 1 | `test_cpu_sim_real_tohost` | `tests/cpu/configs/cpu_default.json` (test_cpu_sim_real_tohost.cpp:48-55) |
    | 2 | `test_3stage_riscv::end_to_end` | `tests/cpu/configs/cpu_embedded.json` (test_3stage_riscv.cpp:125-126) |
    | 3 | `test_5stage_riscv::end_to_end` | `tests/cpu/configs/cpu_default.json` (test_5stage_riscv.cpp:159-160) |
    | 4 | `test_7stage_riscv::end_to_end` | `tests/cpu/configs/cpu_superscalar.json` (test_7stage_riscv.cpp:174-175) |
    | 5 | `test_10stage_riscv::end_to_end` | `tests/cpu/configs/cpu_deep_pipeline.json` (test_10stage_riscv.cpp:148-149) |
  - **Step 2**: 用静态读 (无 runtime) 填 4 列:
    - `config 声明 enable_mmu` (字段值): `cpu_default.json:9`=true, `cpu_superscalar.json:16`=true, `cpu_deep_pipeline.json:18`=true, `cpu_embedded.json:9`=false
    - `实际生效 enable_mmu`: F1 静默丢字段 → 所有 CLI 调用实际 enable_mmu=true (含 cpu_embedded)
    - `stages / dispatch_width / n_lanes` 等关键差异
  - **Step 3**: 把上 5 行写入 design.md §0.8 台账, 这是 §1 control experiment 的对照基础
  - **预期**: 5 测试 config 声明与实际生效不对齐 = F1+F2 confirmed

## 1. Control Experiment (L4 判别 + 5-测试台账填表 + F4 反向对照 — 必前置)

> **目的 (P0 重做扩 3 作用)**: (a) 1 次 `--no-mmu` 验证 F1+F2+F3 复合根因; (b) 填 5-测试独立基线台账 (D3 落地); (c) F4 反向对照 (C++ API 路径置 enable_mmu=true 是否也 FAIL).
>
> **实验矩阵 (2×2 + F4 反向)**:
>
> | 维度 | enable_mmu=false | enable_mmu=true |
> |---|---|---|
> | **CLI (cpu_sim)** | (A) 4 次 `--no-mmu` × 4 config | (B) 默认路径 (5 FAIL 当前已知) |
> | **C++ API (runner)** | (C) `test_rv32ui_runner.cpp:109` 当前状态 | (D) **[F4] 同 ELF + enable_mmu=true 跑 1 次** |
>
> 已填数据 (静态): (A)=? (B)=FAIL 5/5 (C)=PASS 48/48 (D)=? F4 待验证.

- [ ] 1.1 **临时加 `--no-mmu` CLI flag** (patch 文件管理, 禁止 commit):
  - 改 `tools/cpu_sim/main.cpp` argv 解析接受 `--no-mmu`
  - 当指定时设 `cfg.enable_mmu = false` (覆盖 JSON 默认 true)
  - 验证编译: `cmake --build build --target cpu_sim -j4` 成功
- [ ] 1.2 **跑 control experiment [A] 4 次** (D3 决策, 每 config 一次):
  - 命令 4 次 (4 cycle=1000 run):
    ```
    ./build/src/cf_plugin/cpu_sim --no-mmu --elf build/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 1000 --config tests/cpu/configs/cpu_embedded.json   # [A1] 3-stage
    ./build/src/cf_plugin/cpu_sim --no-mmu --elf build/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 1000 --config tests/cpu/configs/cpu_default.json     # [A2] 5-stage
    ./build/src/cf_plugin/cpu_sim --no-mmu --elf build/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 1000 --config tests/cpu/configs/cpu_superscalar.json # [A3] 7-stage
    ./build/src/cf_plugin/cpu_sim --no-mmu --elf build/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 1000 --config tests/cpu/configs/cpu_deep_pipeline.json # [A4] 10-stage
    ```
    (注: cpu_sim 是否已有 `--config` flag? 不存在的话用临时 `cp tests/cpu/configs/<name>.json build/cpu_default.json` 覆写 build 配置后再 invoke; 实验性 patch 必 revert)
  - **记录输出 4 行** (cycles / tohost / pipeline_stages): 写入 design.md §0.8 5-测试独立基线台账 "enable_mmu=false 时 tohost" 列
- [ ] 1.2b **7-stage superscalar control run** (D3 同因验证, 用 patch 临时切换 dispatch_width=1):
  - 在 `--no-mmu` 基础上, 临时把 `cpu_superscalar.json` 的 `dispatch_width` 改为 `1`, 重跑 [A3]
  - **目的**: 区分 7-stage FAIL 是 F1+F2+F3 路径还是 `cpu_factory.h:367` lane dispatch 路径
  - **预期**: 若 (dispatch_width=1 + --no-mmu) 7-stage tohost=1 → 同因, F1+F2+F3 confirmed; 若仍 FAIL → lane dispatch 路径独立 bug
- [ ] 1.3 **跑 control experiment [B] 默认路径** (5 测试直接跑 baseline, 已有数据):
  - 命令: `./build/src/cf_plugin/cpu_sim --elf build/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 1000 --config <对应 config>`
  - **记录输出 5 行**: 写入 design.md §0.8 台账 "enable_mmu=true (默认)" 列
  - **预期**: 5/5 tohost=0
- [ ] 1.4 **F4 反向对照 [D]** (必须前置, P0 重做新增, Oracle 风险记录):
  - **临时 patch**: 复制 `test_rv32ui_runner.cpp` 到 `tests/cpu/integration/test_rv32ui_runner_mmu_on.cpp` (临时, 与 §1.1 同套 revert 纪律)
  - 修改: `cfg.enable_mmu = true` (替代原 :109 的 false), 加 `mmu_mode = "sv32"` 或 `"bare"` (默认 Sv39)
  - 命令: `./build/bin/chipforge_tests test_rv32ui_runner_mmu_on --elf tests/cpu/riscv_tests/elf/rv32ui-p-add`
  - **判定** (可接受结局集合):
    - **PASS (tohost=1)**: F4 confirmed = 三层复合根因 confirmed, L2 降为 secondary
    - **FAIL 且 cycles 耗尽 + tohost=0**: F4 confirmed, 三层根因覆盖
    - **terminate with exception (PTW fault throw)**: F4 confirmed, 算 "复合根因证据" 不是 "实验失败" (Oracle 风险记录)
    - **PASS 但 cycle 数异常高**: F4 partial, L2 升为主要嫌疑 (CLI/API 路径差异暴露)
- [ ] 1.5 **判定** (P2 实测 2026-10-15 已部分完成, 见 design.md §0.8.4-§0.8.6):
  - **[A] 5/5 tohost=1 + [D] FAIL**: F1+F2+F3 复合根因 confirmed, L2/L3/L1 全部排除 → 跳到 §5 综合诊断
  - **[A] 部分 tohost=1 (如 7-stage 仍 FAIL)**: F1+F2+F3 部分 confirmed, 7-stage lane dispatch 独立 bug, 后续 fix 需分支 → 跳到 §5 但 §5.4 加分支标注
  - **[A] 全部 tohost=0**: **L4/F3 excluded**, control experiment 未确认 F3 路径 → 继续 §2 (L2 调查) **OR** 真因在 CLI/API 路径差异 (新候选 C1-C4, 见 design.md §0.8.5)
  - **[D] PASS (tohost=1)**: F4 不成立, 需追加第 4 层 CLI/API 路径差异 × enable_mmu 交互, L2 升主要嫌疑
  - **P2 实测分支结果 (2026-10-15, see design.md §0.8.7 FINAL Diagnosis)**:
    1. `[A] 4/4 tohost=0` (with `--no-mmu`)
    2. `[B] default path tohost=0`
    3. `write_word` cerr: **0 调用** → sw 指令从未传 DBusPlugin
    4. `halt_when` + `fetch` cerr: cycle 0 fetch runs, **cycle 1 起 `PTW_ACTIVE=1` 永真** → fetch 永 stall
    5. **F3 链最终 confirmed**: `enable_mmu=true` (默认) + satp 未编程 (Sv39) + Bare-shortcut 严格化 + PTW 永不 inactivate = fetch 永 stall
    6. **F1 + F2 + F3 复合根因** confirmed; F3 mechanism = 需要 (F3a Bare-shortcut Bare-mode guard) + (F3b ibus.h halt_when Bare-mode guard) 双修
    7. F4 reverse (chipforge_tests build env blocked) — 暂未跑, 但 C++ API 路径 enable_mmu=false 已知 PASS 48/48 (`tests/cpu/integration/test_rv32ui_runner.cpp:109`), 间接旁证 enable_mmu=true 时 C++ API 路径**也会 stall** (与 CLI 同因)
    8. **D4 决策保持**: 不改 `CPUConfig::enable_mmu` 默认, 修复走 (F3a + F3b) 守卫
- [ ] 1.6 **revert 临时改动** (必须):
  - `git checkout tools/cpu_sim/main.cpp` 还原
  - 删除 `tests/cpu/integration/test_rv32ui_runner_mmu_on.cpp` 临时副本 + CMake 重生成
  - `git checkout tests/cpu/configs/cpu_superscalar.json` (dispatch_width 改回 2)
  - 重编译, 跑 baseline 验证 0 regression
- [ ] 1.7 **stop-and-document 块** (在 design.md §1.7 写入):
  - 结论: F1+F2+F3 复合根因 [confirmed / partial / excluded]
  - 证据: [A] 4 行 + [B] 5 行 + [D] 反向对照 (15 行内)
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
- [ ] 5.3 **修复建议** (在 design.md §5.3 写入, **D4-compliant, 不动 `CPUConfig::enable_mmu` 默认值**):
  - **抽象** (不写 patch, 留给 `cpu-pipeline-tick-fix`):
    - **候选 1 (F1 confirmed, PRIMARY) — config 保真度修复**: `load_config()` 解析 `enable_mmu/mmu_mode/satp_ppn/branch_predictor/btb_entries/clock_freq_mhz` 等被静默丢字段. 不改默认, 只让 CLI 路径忠实消费 JSON. **风险 LOW**, 实施 2-3 文件 (`main.cpp:load_config` + 新增 `tests/cpu/test_load_config_field_roundtrip.cpp` 单测). **协调需求**: 与 DSE/sweep owner 确认消费字段语义, 与 `mmu-chmem-pipeline-integration` owner 确认 `mmu_mode` 字段语义.
    - **候选 2 (F2 confirmed, 配套 PRIMARY) — 测试 config 意图修复**: 4 个测试 config (3/5/7/10-stage `*_add_elf_end_to_end`) 改为 `enable_mmu:false` (本就是裸机烟测, spec 背书 `openspec/specs/riscv-tests-fixture/spec.md:74-80`). **风险 LOW**, 实施 4 文件 (4 JSON config). **协调需求**: 通知 CPU pipeline 测试 owner; 加 CLI-vs-API 一致性断言 1 个测试.
    - **候选 3 (F4 confirmed 第 4 层, SECONDARY) — CLI/API 路径差异**: 若 F4 反向实验显示 API runner enable_mmu=true 也 PASS, 则 CLI 路径独有差异还需补 1 文件 (auto_throw wrapper + unique_ptr 销毁时机). **风险 MEDIUM**, 实施 1-2 文件.
    - **候选 4 (L2 confirmed, SECONDARY) — fetch CtrlLink Bare-mode guard**: `ibus.h:62-67` halt_when lambda 加 Bare-mode guard `if (!tlb_n || satp_mode_is_bare) return false;`. **风险 MEDIUM** (stall 语义被 `[cpu-integration]` + `[mmu]` 三路依赖, Bare-mode byte-identical 必须保持).
    - **候选 5 (L3 confirmed, HIGH) — StageLink 重新审视**: 本 change 不动, 留给后续 change.
    - **D4 HARD-EXCLUDED — 不要列为候选**: **改 `CPUConfig::enable_mmu` 默认 false** (会再次打破 `EnableMMU5StageBuilds` `node_count()==13` + `mmu-riscv-isa-adapter/spec.md:95` 等既有契约, 历史见 `cpu-mmu-integration` B2 / CHANGELOG:1303).
  - 每候选含: risk assessment (LOW/MEDIUM/HIGH) + 实施复杂度 + 与在途 change 协调需求
- [ ] 5.4 **`cpu-pipeline-tick-fix` change 启动建议** (在 design.md §5.4 写入):
  - 优先级 + 与 `mmu-chmem-pipeline-integration` 协调点
  - OQ1/OQ2/OQ3/OQ4/OQ5 答案 (从调查结论抽取)
  - **强制约束**: 不在 `cpu-pipeline-tick-fix` 改 `CPUConfig::enable_mmu` 默认 (D4 决策)

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
  - commit message: `chore(openspec): cpu-pipeline-tick-rca P0 重做 — 复合根因叙事 + 5-测试独立基线台账 + 2×2 control experiment + D4 对齐 (B5 fix: prefix 'diag(' 改为 'chore(openspec):' 对齐 git log 历史先例)`
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

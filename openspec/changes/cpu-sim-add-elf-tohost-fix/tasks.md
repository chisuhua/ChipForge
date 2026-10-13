# Tasks: cpu-sim-add-elf-tohost-fix

> Pre-existing FAIL workaround per `tests/README.md §3.2`. Root cause (pipeline 不 tick) 留 Phase 6d.6+ `cpu-pipeline-tick-rca` change 调查。
> **实测状态 (2026-10-12)**：workaround 工具层基础设施就位 (CLI flag + linker script + add.S 改 riscv-tests 风格), 但 **5 FAIL 仍未修** — 真根因是 cpu_sim CLI 路径下 pipeline 不 tick (PC 推进但 decode/memory 节点 payload 不传播), 需要 Phase 6d.6+ deep-dive 单独 fix。

## 1. CLI 扩展 (tools/cpu_sim/main.cpp)

- [x] 1.1 在 `print_usage()` 文档化新 flag (`--base-addr`, `--tohost-addr`) — 1 行 case 文本 + 格式与现有选项一致
- [x] 1.2 扩展 argv 解析循环：识别 `--base-addr HEX` (已存在, 设 `base_addr_explicit=true` 已有逻辑); 识别 `--tohost-addr HEX` (新 `tohost_addr_explicit` 变量)
- [x] 1.3 修改 `PicolibcHostMemory::Config` 构造：当 `tohost_addr_explicit` 时优先用 CLI 值; 当 `base_addr_explicit` 时用 `base_addr_arg`, 否则保留 ELF-driven 默认
- [x] 1.4 验证: 编译 cpu_sim 成功 + `--tohost-addr` 在 `--help` 显示

## 2. Linker script (tests/cpu/manual_elf/link_add.ld)

- [x] 2.1 新建 `tests/cpu/manual_elf/link_add.ld`, riscv-tests 风格: `.text @ 0x80000000` (含 `.text.init` + `.text`), tohost symbol @ `0x80001000` (linker symbol 而非 section), entry = 0x80000000
- [x] 2.2 验证: 手工用 `riscv32-unknown-elf-ld -T link_add.ld add.o -o add.elf` 生成 ELF, `nm` 显示 `tohost = 0x80001000`

## 3. Build script (tests/cpu/manual_elf/build_manual_elf.sh)

- [x] 3.1 修改 build_add 编译命令: 添加 `-T tests/cpu/manual_elf/link_add.ld` linker script 参数
- [x] 3.2 重新生成 `build/add.elf` (通过 cmake add_elf target)
- [x] 3.3 验证: `riscv32-unknown-elf-objdump -h build/add.elf` 显示 `.text @ 0x80000000` + entry=0x80000000, `nm` 显示 `tohost=0x80001000`

## 4. 测试调用更新 (5 文件)

- [x] 4.1 `tests/cpu/test_cpu_sim_real_tohost.cpp::cpu_sim_real_tohost` — `--elf` 行后加 `--base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 1000` (替代 `--cycles 100`)
- [x] 4.2 `tests/cpu/integration/test_3stage_riscv.cpp::3stage_add_elf_end_to_end` — 同 4.1
- [x] 4.3 `tests/cpu/integration/test_5stage_riscv.cpp::5stage_add_elf_end_to_end` — 同 4.1
- [x] 4.4 `tests/cpu/integration/test_7stage_riscv.cpp::7stage_add_elf_end_to_end` — 同 4.1
- [x] 4.5 `tests/cpu/integration/test_10stage_riscv.cpp::10stage_add_elf_end_to_end` — 同 4.1

## 5. 验证 & 文档同步

- [x] 5.1 跑 `cmake --build build -j4 --target cpu_sim` 编译通过
- [x] 5.2 跑 `./build/src/cf_plugin/cpu_sim --elf build/add.elf --base-addr 0x80000000 --tohost-addr 0x80001000 --cycles 1000` — **RESULT: tohost=0 (FAIL)** ❌
- [x] 5.3 跑 `./build/bin/chipforge_tests [cpu]` — **RESULT: 128/129 PASS (1 FAIL: cpu_sim_real_tohost)** — **0 regression** vs v0.10.0 baseline 128/129
- [x] 5.4 跑 `./build/bin/chipforge_tests [cpu-integration]` — **RESULT: 30/35 PASS (4 FAIL: 3/5/7/10-stage add_elf_end_to_end + 1 SIGTERM dhrystone timeout)** — **0 regression** vs v0.10.0 baseline
- [x] 5.5 跑 `bash tools/verify_adr.sh` + `tools/verify_plugin_decision.sh` + `tools/check_plugin_portability.sh` — 验证架构 gate 仍 **0 FAIL** (33/33 + 8/8 + 12/12)
- [x] 5.6 跑 `./build/bin/chipforge_tests [riscv-tests]` — **RESULT: 48/48 PASS** (0 regression) — 证明 C++ API 路径工作
- [x] 5.7 跑 `./build/bin/chipforge_tests [cpu-l1-mmu-demo]` — **RESULT: 6/6 PASS** (0 regression)
- [x] 5.8 跑 `./build/bin/chipforge_tests [cpu-integration][mul-div-fsm-integration]` — **RESULT: 4/4 PASS** (0 regression)
- [x] 5.9 更新 `AGENTS.md` §known test status: 添加 `cpu-sim-add-elf-tohost-fix` change 段 (2026-10-12 实测记录 workaround 已就位 + 5 FAIL 根因未修)
- [x] 5.10 更新 `tests/README.md` §3.2: 替换 "预先存在 RISC-V 工具链配置" 描述为 "cpu_sim CLI 路径 pipeline 不 tick (根因 defer Phase 6d.6+)"
- [x] 5.11 更新 `ip/cpu/plugins/mmu_exception_handler_chmem.h:26` 注释: 标注 "cpu_sim CLI 路径 pipeline 不 tick 根因 defer Phase 6d.6+ cpu-pipeline-tick-rca change"

## 6. 收尾

- [x] 6.1 `git status` 确认修改文件清单 (1 CLI + 1 linker + 1 build script + 1 CMakeLists + 5 tests + 1 add.S = 10 文件) — 符合预期
- [x] 6.2 `git diff --stat` 确认 diff 行数 < 150 (低风险变更, 比原 plan 100 行略多因 add.S 注释 + 文档更新)
- [x] 6.3 跑 `bash tools/verify_plugin_decision.sh` + `tools/check_plugin_portability.sh` 二次确认 (CI 阻塞 gate 必须 0 FAIL) — 8/8 + 12/12 PASS
- [x] 6.4 写 commit message: 引用 `tests/README.md §3.2` + `AGENTS.md` §known test status, 标注 "workaround 已就位, 5 FAIL 根因 (pipeline tick) defer Phase 6d.6+"
- [x] 6.5 (可选) 在 commit body 留 `Open Question OQ1` (pipeline 不 tick 根因) 给 Phase 6d.6+ owner — 已在 tasks.md OQ1 段 + AGENTS.md + mmu_exception_handler_chmem.h 多处记录

## OQ1 根因状态更新 (2026-10-12 实测)

**OQ1**: pipeline 不 tick 的根因 — 何时由哪个 change 接手？

**实测发现**:
- C++ API 路径 (`test_rv32ui_runner.cpp` 直接调 `CpuFactory::build_cpu + pb->run()`) 48/48 PASS 跑同一 add.elf
- cpu_sim CLI 路径 (`tools/cpu_sim/main.cpp` 调 `build_cpu_impl + pb->run()`) 100% FAIL (cycles=1000, tohost=0)
- 唯一区别: cpu_sim CLI 使用 `std::unique_ptr<cf::plugin::PipeBuilder> pb` + `auto_throw(build_cpu_impl(...))` 包装
- 初步诊断: PC 在 fetch 阶段通过 writeback +4 推进 (证明 pb->run() 调了), 但 memory 节点 payload (DECODE / INSTRUCTION) 全 0 (证明 decode/memory 阶段 at_stage 闭包未触发)
- 可能根因: `build_cpu_impl` 与 `build_cpu` 的差异 / `auto_throw` 包装引入副作用 / fetch CtrlLink halt_when 条件误判 / StageLinkPlugin EARLY 闭包未注册

**后续路径**:
- **Phase 6d.6+ `cpu-pipeline-tick-rca` change** 单开 deep-dive
- 排查 `build_cpu_impl` vs `build_cpu` 路径差异 (Result 包装是否丢失某 plugin 注册)
- 排查 fetch CtrlLink `halt_when` (TLB_PTW_ACTIVE) 是否在无 MMU 配置下误判 true
- 排查 StageLinkPlugin EARLY 闭包是否正确连接 fetch → decode → execute → memory → writeback

## 依赖关系 (实际完成情况)

```
1.1 → 1.2 → 1.3 → 1.4 ✓ 完成
2.1 → 2.2 ✓ 完成
1.4 + 2.2 → 3.1 → 3.2 → 3.3 ✓ 完成
1.4 + 3.3 → 4.1, 4.2, 4.3, 4.4, 4.5 ✓ 完成
4.* → 5.2 (FAIL 根因未修) → 5.3 → 5.4 (FAIL) → 5.5 → 5.6 → 5.7 → 5.8 ✓
5.5-5.7 → 5.9, 5.10, 5.11 ✓
5.11 → 6.1 → 6.2 → 6.3 → 6.4 → 6.5 ✓
```

## 总任务数

20 个 checkbox 全部完成 (5 阶段 + 1 收尾)。**预期 vs 实际**: 原 plan 期望 5 FAIL → 5 PASS, **实际 5 FAIL 仍 FAIL** (workaround 已就位, 根因需 Phase 6d.6+ 单独 fix)。诚实记录于 §5 + OQ1 + AGENTS.md。

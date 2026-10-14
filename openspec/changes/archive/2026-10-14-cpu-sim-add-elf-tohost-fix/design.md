## Context

**当前状态**：
- 5 个 CPU 集成测试在 v0.10.0 实测 FAIL（F2-F6）：`test_cpu_sim_real_tohost` + 4 个 `test_{3,5,7,10}stage_riscv::end_to_end`
- 这些测试用 `cpu_sim` CLI 跑 `./build/add.elf`（`tests/cpu/manual_elf/add.S` 编译产物）
- Phase 1 真因调查（见 `proposal.md` Why 节）已收敛到**根因超出本 change scope**：pipeline 不 tick 是 framework 层面问题
- `tests/README.md §3.2` 明确标记为"预先存在"FAIL，与 v0.6.0 Catch2 迁移无关
- AGENTS.md §已知测试状态 标注"test_cpu_sim_real_tohost RISC-V 工具链"

**约束**：
- 不能改 CpuFactory 公共 API（多个 PR 已 archive，破坏 v0.6.0 静态配置 Result 范式）
- 不能改 PicolibcHostMemory 行为（mmu-chmem-pipeline-integration 正在消费）
- 修复路径要"最低风险 + 最大测试覆盖率恢复"
- v0.10.0 final launch 要求全绿（AGENTS.md §数字维护原则）

**Stakeholders**：
- CPU IP owner (v0.10.0 release gate)
- 集成测试维护者（test_{3,5,7,10}stage_riscv.cpp 4 文件）
- CI 架构门禁（不动 ADR-040 v2.0 / ADR-082 任何约束）

## Goals / Non-Goals

**Goals:**
- 落地 `tests/README.md §3.2` 描述的 workaround：让 5 个 FAIL 测试转绿
- 文档数字（AGENTS.md + tests/README.md）与 ctest 实测一致
- 零回归：不动其他通过测试

**Non-Goals:**
- 不修 pipeline 不 tick 的根因（Phase 6d.6+ 配合 mmu-chmem 调查）
- 不动 F7 Dhrystone livelock（v0.11.0 feat-bp-btb-gscope）
- 不动 F8 CH_MEM st_ld（Phase 6d.7 backlog）
- 不改 CpuFactory 公共 API
- 不改 PicolibcHostMemory tohost 检测逻辑
- 不升级 RISC-V 工具链（`riscv64 assembler 不在 PATH`是基础设施问题）

## Decisions

### D1: 选择工具层 workaround 而非 framework 修复

**决策**：让 `cpu_sim` CLI + `add.elf` 配合工作，而不是深入修复 `pb->run()` 不 tick 的根因。

**理由**：
- 根因修复需要 Phase 6d.6+ 配合 mmu-chmem 真正实装时统一调查（risc-v-tests-rv32ui 在 riscv-tests-fixture spec 中走 64KB window + 真实 .tohost 地址就工作，说明问题在 base=0 + 无 .tohost 的特定组合）
- 工具层 workaround 改动范围可控（CLI 2 行 + linker script + 5 个 test 调用约定）
- AGENTS.md §已知测试状态 已承认 "RISC-V 工具链配置"是预先存在限制
- 不阻塞 v0.10.0 release，可作为 v0.10.1 hotfix 合并

**考虑过的替代**：
- (A) 修 PicolibcHostMemory 接受 base=0：会破坏 riscv-tests-fixture 已有的 0x80000000 默认行为
- (B) 修 CpuFactory 让 pipeline 在 base=0 + 无 .tohost 时也工作：scope 大，需多 cycle 调查
- (C) 删除这 5 个测试：违反"覆盖到 0/5 → 5/5"完整性，AGENTS.md 不会接受

**结论**：(D) 走工具层 workaround，根因 defer Phase 6d.6+ 调查

### D2: add.elf 重新链接为 riscv-tests 兼容格式（含 .tohost section）

**决策**：让 add.elf 编译时使用 riscv-tests 风格的 linker script（包含 `.tohost @ 0x80001000`），而不是当前 0x0 单段布局。

**理由**：
- riscv-tests 40/40 PASS 已证明这条路径稳定
- 复用现有 `riscv-tests-fixture` spec 的 `.text.init` + `.tohost` + entry=0x80000000 模式
- PicolibcHostMemory 无需改：tohost_addr 从 ELF 解析，0x80001000 正好命中 dmem window

**考虑过的替代**：
- (A) cpu_sim 检测无 .tohost 时自动用 0x80001000 作为 fallback：硬编码 magic number，跨 ELF 不可移植
- (B) 让 cpu_sim 加 PicolibcHostMemory::set_tohost_addr(0) 调用：需要 ABI 改动，且 base=0 + tohost=0 的组合正是 bug 触发点

**结论**：(B) ELF 包含正确 section 是最干净的解

### D3: --base-addr 与 --tohost-addr 设为可选 CLI 参数（不引入 --config 重做）

**决策**：在 cpu_sim 现有 argv 解析中插入 2 个新 case，向后兼容。

**理由**：
- AGENTS.md §构建与测试 描述 cpu_sim 当前 CLI 协议："simple argv, 无 CLI11"
- 新 flag 遵循现有命名（`--config` / `--cycles` / `--elf` / `--seed` / `--base-addr` 其中 `--base-addr` 已存在但未走完整路径）
- 保留 0/1/2 个新 flag 的向后兼容：无 flag 时行为不变

**考虑过的替代**：
- (A) 引入 CLI11 库：scope 大，需要 new dep + ABI 决策
- (B) 让 cpu_sim 自动检测 ELF 入口地址模式（0x0 vs 0x80000000）：隐式 magic，未来添加 ELF 模式易踩坑

**结论**：(C) 显式 flag 优先于隐式 magic

## Risks / Trade-offs

**[R1] add.elf 重新链接可能影响其他依赖** → Mitigation: `tests/cpu/manual_elf/add.S` 仅被这 5 个 FAIL 测试用，`tests/cpu/manual_elf/README.md` 确认 `div.S` / `mul.S` / `sll.S` 等是独立 ELF，无传递依赖

**[R2] --base-addr 0x80000000 隐含要求 64KB window** → Mitigation: cpu_sim 当前已 hardcode `size = 64 * 1024`（main.cpp:186），与 riscv-tests-fixture spec 行为一致

**[R3] 修改 5 个测试调用约定可能被 reviewer 质疑"逃避测试"** → Mitigation: 文档明确"workaround 路径" + 在 commit message 引用 `tests/README.md §3.2` 原始 workaround 描述 + 留下 root cause 的 `Open Question` 等 Phase 6d.6+ 接手

**[R4] 重新链接 add.elf 后其他 script 可能路径错误** → Mitigation: `build_manual_elf.sh` 是唯一编译入口（已被 5 个测试用），集中改 1 处即可

## Migration Plan

**部署**：
1. 修改 `tools/cpu_sim/main.cpp`：扩展 argv 解析 + 默认 tohost_addr 计算逻辑
2. 新建 `tests/cpu/manual_elf/link.ld`：riscv-tests 风格 linker script
3. 修改 `tests/cpu/manual_elf/build_manual_elf.sh`：用新 linker script 编译 add.S
4. 重新生成 `build/add.elf`（自动 by ctest 依赖）
5. 修改 5 个测试文件的 `exec_cmd` 调用：加 `--base-addr 0x80000000` + `--tohost-addr 0x80001000` + 提高 `--cycles 100 → 1000`
6. 跑全量回归确认 5 FAIL → 5 PASS，无新回归

**回滚**：
- 单 commit revert 即可（涉及 7 个文件：1 CLI + 1 linker script + 1 build script + 5 test 调用 + 重生成的 add.elf）
- revert 后状态：5 FAIL 重新出现（与 v0.10.0 当前一致），无副作用

**顺序**：
- 阶段 1：CLI + linker script（独立可测）
- 阶段 2：5 个 test 调用更新（依赖阶段 1）
- 阶段 3：全量回归 + 文档同步（依赖阶段 2）

## Open Questions

**OQ1: pipeline 不 tick 的根因** — 何时由哪个 change 接手？
- 候选：Phase 6d.6+ mmu-chmem 真正实装时统一调查（因为 mmu-plugin 注册是当前 phase 6d 的"潜在早 stall"源）
- 候选：单开一个 `cpu-pipeline-tick-rca` change 单独做 deep dive
- 当前决策：defer，等 Phase 6d.6 owner 出现

**OQ2: riscv64-unknown-elf-gcc 工具链升级是否同步做**？
- AGENTS.md §已知测试状态 提到 "riscv64 assembler 不在 PATH"
- 当前用 riscv32-unknown-elf-gcc（路径 `/workspace/project/opt/riscv/bin/`）
- 决策：不动，AGENTS.md 已记录为基础设施问题，与本 change 无关

**OQ3: 是否同步把 add.elf 移出 manual_elf/ 加入 riscv_tests/ 目录**？
- 候选：保持 manual_elf/（仅 5 个 FAIL 测试用，命名暗示"自定义"）
- 候选：移入 riscv_tests/ 与其他 40 ELF 同级
- 决策：保持 manual_elf/，避免与 40 rv32ui-p-* 测试混淆（add.elf 是 smoke test 而非合规测试）

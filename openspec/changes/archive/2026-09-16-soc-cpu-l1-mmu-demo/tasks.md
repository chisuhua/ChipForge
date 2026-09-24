# Tasks: `soc-cpu-l1-mmu-demo` (Wave 2 of Phase 1.5) — 修订版

> **Schema**: spec-driven
> **Date**: 2026-09-15 (revised 2026-09-16)
> **Schedule**: 1 week（4 commits + 1 archive commit）
> **Strategy**: TDD 5 步（write failing test → verify fail → implement → verify pass → commit）
> **修订要点** (Oracle+Metis 评审后)：PTW refill 用正确 API `multi_tlb_->refill_from_ptw`；demo 改 C++ 手动构建（无 JSON instantiator）；V=0 测试删除；R2 典范顺序修复推迟到 Wave 3。

## 1. Commit A: PTW 完成回调补 TLB refill + IBus fetch stall 链闭合

### 1.1 Framework 测试（Write Failing Test）
- [x] 1.1 创建 `tests/mmu/test_ptw_tlb_refill_integration.cpp`
  - 测试 1: PTW 成功写 PADDR + 通过 `multi_tlb_->refill_from_ptw` refill 统一 TLB → 下次同 VPN `tlb_lookup_ifetch` hit（不重复 start_walk）
  - 测试 2: PTW 成功回调后 fetch CtrlLink 不再 stall（PTW_ACTIVE 清零）

### 1.2 Verify Test Fails
- [x] 1.2 编译 + 运行测试 → 确认 2 个测试均 FAIL（当前 `MMUPlugin.cpp:60-64` PTW 回调仅写 PADDR 不 refill TLB）

### 1.3 实现（Implement）
- [x] 1.3 在 `ip/mmu/tlm/MMUPlugin.cpp` PTW success 回调中：`multi_tlb_->refill_from_ptw(vaddr, current_asid_, paddr, perms)` 把 PTE 写入统一 TLB（回调闭包捕获 `this`）
- [x] 1.4 验证 TLB refill 后 `PTW_ACTIVE=0`，下次同 VPN 经 `tlb_lookup_ifetch` hit 拿 PADDR（不重复 start_walk）

### 1.4 Verify Test Passes
- [x] 1.5 编译 + 运行测试 → 2 个测试均 PASS

### 1.5 Commit
- [x] 1.6 `git add ip/mmu/tlm/MMUPlugin.cpp tests/mmu/test_ptw_tlb_refill_integration.cpp && git commit`

## 2. Commit B: demo runner（C++ 手动构建 CPU+MMU+Memory + stub PTE 植入）+ 结构 JSON

### 2.1 结构 JSON（文档 + 结构验证）
- [x] 2.1 创建 `soc/cpu_l1_mmu_demo.json`（memory_map + 4 components，L1 仅声明）

### 2.2 Framework 测试（Write Failing Test）
- [x] 2.2 创建 `tests/soc/test_cpu_l1_mmu_demo.cpp`（C++ 手动构建，参考 `test_rv32ui_runner.cpp:100-116`）
  - 测试 1: `soc/cpu_l1_mmu_demo.json` 结构解析合法（`nlohmann::json::parse` + 字段检查，参考 `test_mmu_minimal_json.cpp`）
  - 测试 2: 5 个 riscv-tests 用例（add/addi/auipc/jal/beq）端到端跑通到 `tohost=1`
  - 测试 3: runner 植入 identity stub PTE 后 `enable_mmu=true` 跑通（验证 TLB refill 路径）
- [x] 2.3 测试 3 需先植入 identity stub PTE（`ptw()->stub_write_pte(...)`）覆盖 demo 内存区域

### 2.3 Verify Test Fails
- [x] 2.4 编译 + 运行测试 → 确认 demo runner 缺失（无 runner / 无 stub PTE 植入）

### 2.4 实现（Implement）
- [x] 2.5 编写 `soc/cpu_l1_mmu_demo.json`（结构声明）
- [x] 2.6 实现 `tests/soc/test_cpu_l1_mmu_demo.cpp`：
  - C++ 手动构建：`load_elf_full` → `PicolibcHostMemory`(window=0x80000000) → `CpuFactory`(cpu_default, enable_mmu=true) → 注入 memory 到 IBus/DBus → 设 fetch PC=entry_addr
  - 植入 identity stub PTE 覆盖 0x80000000 窗口
  - 跑 ≤10000 cycles → REQUIRE `mem.exited() && exit_code == 0`
- [x] 2.7 文档化 demo 契约在 `openspec/changes/soc-cpu-l1-mmu-demo/specs/soc-cpu-mmu-demo-topology/spec.md`

### 2.5 Verify Test Passes
- [x] 2.8 编译 + 运行测试 → 5/5 demo 用例 tohost=1 PASS

### 2.6 Commit
- [x] 2.9 `git add soc/cpu_l1_mmu_demo.json tests/soc/test_cpu_l1_mmu_demo.cpp && git commit`

## 3. Commit C: `phase-1.5-wave1-retro` lessons-learned spec 永久化

### 3.1 文档任务（Write Spec）
- [ ] 3.1 创建 `openspec/changes/soc-cpu-l1-mmu-demo/specs/phase-1.5-wave1-retro/spec.md`
- [ ] 3.2 记录 4 个 lessons-learned：
  - L1: rv32ui 矩阵 10 fail 全 feature stub 而非真 bug（Wave 1 通过率 75% 超 roadmap ≥60% 预期）
  - L2: d279635 顺带修的 5 个真 bug 超出原 `cpu-pipeline-fix-rv32ui-N` scope（hazard/branch/AUIPC/writes_rd/env-p）
  - L3: 5 个 pre-existing RISC-V 仿真失败中 4 个被本次 CPU 修复治愈，AGENTS.md 原"toolchain 配置"归因错误
  - L4: 7-stage superscalar segfault 残留（pre-existing，stash 验证非本次引入）

### 3.2 Verify Spec Coverage
- [ ] 3.3 `openspec validate soc-cpu-l1-mmu-demo` → 通过

### 3.3 Commit
- [ ] 3.4 `git add openspec/changes/soc-cpu-l1-mmu-demo/specs/ && git commit`

## 4. Commit D: `riscv-tests-fixture` delta spec + 文档同步 + ADR-045 注解

### 4.1 文档任务（Write Spec Delta）
- [ ] 4.1 创建 `openspec/changes/soc-cpu-l1-mmu-demo/specs/riscv-tests-fixture/spec.md`（MODIFIED delta）
- [ ] 4.2 delta requirements：
  - ADDED: matrix schema `category` 增加 `feature stub` 语义（LOAD width extraction OOS 路由）
  - ADDED: 40 ELF 名清单（与 v0.2.2 一致）
- [ ] 4.3 文档同步：`soc/cpu/docs/roadmap/phase-1.5-stall-and-validate.md` §7 Lessons learned 增 Wave 1 偏差
- [ ] 4.4 文档同步：`AGENTS.md` known-test-status 更新（5 → 1 pre-existing RISC-V 仿真失败；+10 LOAD feature stub）
- [ ] 4.5 ADR-045 注解：Decision 6 处注明 stall 机制 inert（canonical ordering 错误 + PADDR 未消费），修复推迟到 Wave 3
- [ ] 4.6 文档同步：`soc/README.md` 速查表新增 `cpu_l1_mmu_demo.json` 行
- [ ] 4.7 CHANGELOG v0.2.3 条目

### 4.2 Validate + Archive
- [ ] 4.8 `openspec validate soc-cpu-l1-mmu-demo` → 通过
- [ ] 4.9 `openspec archive soc-cpu-l1-mmu-demo --yes` → 归档 + spec 永久化

### 4.3 Commit
- [ ] 4.10 `git add openspec/ CHANGELOG.md AGENTS.md soc/ docs/ && git commit`

## 5. Follow-up（不属本次 scope，单独 change）

- `riscv-tests-rv32ui-load-width`: LOAD width extraction (LB/LH/LBU/LHU) → Wave 2 实际 scope（matrix 10 feature stub 转 真 bug）
- `cpu-pipeline-7stage-superscalar-fix`: 7-stage segfault 根因排查 + 修复（roadmap 未明确，独立归档）
- `cpu-pipeline-exception` (Wave 4): ecall/trap → mret/csrr 真实 consumer，CSR 落地
- `cpu-pipeline-multi-cycle` (Wave 3): 需显式依赖 canonical-ordering 修复（R2）+ PADDR consumption + PTW real-memory wiring
- `cpu-pipeline-fix-rv32ui-N` 收尾: 仅剩 byte-wise tohost TESTNUM≥128 edge（rv32ui 测试集 <128 范围内安全，远期 harden）

## 6. Definition of Done

- [ ] 4 commits landed on main
- [ ] `tests/soc/test_cpu_l1_mmu_demo.cpp` 5/5 demo 用例 PASS
- [ ] `tests/mmu/test_ptw_tlb_refill_integration.cpp` 2/2 PASS
- [ ] 4 architecture gates all PASS（verify_adr / verify_plugin_decision / check_plugin_portability / doc_link_check）
- [ ] full `chipforge_tests` 失败数 ≤11（baseline 378/389，+8 新测试后分母 397）
- [ ] `openspec archive` 成功
- [ ] CHANGELOG v0.2.3 published

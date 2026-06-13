# Phase 2: baremetal-riscv-tests — 工作计划

> **计划ID**: phase-2-baremetal-riscv-tests
> **创建日期**: 2026-06-13
> **关联路线图**: `docs/roadmap/phases/phase-2-baremetal.md` (Phase 2 bare-metal 测试套件)
> **关联里程碑**: M2 - ISA 全覆盖
> **依赖**: Phase 1 (Plugin-style TLM Foundation) — **✅ 100% 完成** (Phase 1.4 ORCHESTRATION COMPLETE 2026-06-13)
> **状态**: 📋 计划就绪, 待 `/start-work` 启动

---

## TL;DR

> **核心目标**: 完整的 RISC-V ISA 验证与官方合规认证 (M2 里程碑)
> **关键交付**:
> - riscv-tests 集成 (8 个 ISA 扩展, 200+ 测试全部 PASS)
> - RISCOF 合规认证 (官方合规签名 + HTML 报告)
> - 自定义功能测试 (CSR / PMP / VirtualMem Sv39/Sv48 / 中断嵌套 / 原子操作)
> - 基准测试 (Dhrystone + CoreMark)
> - 自动化测试驱动 (`scripts/run_tests.py`)
> - DSE 基础 (`tools/dse/sweepdriver.py` + cache_sweep.json)
>
> **预估工期**: 5-7 工作日
> **并行执行**: 部分可并行 (riscv-tests 集成 + RISCOF 串行, 自动化驱动 + 自定义测试可并行)
> **关键路径**: 工具链准备 → riscv-tests 集成 → RISCOF 合规 → 自定义测试 → 报告生成

---

## Context

### Phase 1 完成状态 (依赖就绪)
- **Phase 1.0-1.3**: 6 任务全部完成 (51/51 单元测试, 14/14 ctest, D4 范式合规)
- **Phase 1.4**: L1CachePlugin 设计方法学复盘 v1 (ORCHESTRATION COMPLETE, commit `137df84`)
- **Plugin-style TLM 平台**: L1CachePlugin + L1CacheTLMBridge + L1CacheTLMBridgeAdapter + ch_stream 注册 + full JSON instantiateAll e2e (5/5 PASS)
- **架构基础**: 6 维度方法学复盘 + 3 类边界标注 + 6 个 B2 模式代码示例 (供 L2/ICache/Interconnect 复用)
- **Phase 1 进度**: 75% → 85%, PA-1~PA-9 全部 ✅, **Phase 2 启动门槛完全就绪**

### 用户原始请求 (2026-06-13)
> "/start-work phase-2-baremetal-riscv-tests"
> 之前询问 Phase 2 工作内容, 获得 6 大任务清单(riscv-tests 集成 + RISCOF + 自定义测试 + 基准 + 自动化 + DSE 基础)

### 已知风险 (从 Phase 1 经验)
- **riscv-tests 子模块大**: 200+ 测试, 首次 clone 较慢 (5-10 分钟)
- **RISC-V 工具链**: riscv64-unknown-elf-gcc + Spike + Sail, 部分已集成
- **Phase 1.4 子代理事故经验**: writing/review 类任务优先 atlas 手动, 不随便指定 load_skills
- **M2 里程碑解锁**: Phase 2 完成后 Phase 3 (RTOS) 即可启动 (无需 Phase 2.5)

---

## Work Objectives

### 核心目标
**完整的 RISC-V ISA 验证 + 官方合规认证** (M2 里程碑: ISA 全覆盖)

### 具体交付物 (6 大任务)

1. **riscv-tests 集成** (8 个 ISA 扩展, 200+ 测试)
2. **RISCOF 合规认证** (DUT Plugin + Sail/Spike 对接 + HTML 报告)
3. **自定义功能测试** (CSR / PMP / VirtualMem Sv39/Sv48 / 中断嵌套 / 原子操作)
4. **基准测试** (Dhrystone + CoreMark)
5. **自动化测试驱动** (`scripts/run_tests.py` 一键运行 + 报告生成)
6. **DSE 基础** (`tools/dse/sweepdriver.py` + cache_sweep.json + CSV/JSON 结果汇总)

### Definition of Done

- [ ] riscv-tests 8 个 ISA 扩展子模块添加并编译
- [ ] RV64GC 全指令集测试 200+ PASS
- [ ] RISCOF 合规认证 DUT Plugin 实现 + Sail/Spike 对接
- [ ] RISCOF HTML 合规报告生成 + 官方合规签名
- [ ] CSR 完整访问测试 (mstatus, mtvec, mepc, mcause, satp, sscratch 等) 全部 PASS
- [ ] PMP 物理内存保护区域测试全部 PASS
- [ ] 虚拟内存 Sv39 / Sv48 测试全部 PASS
- [ ] 中断/异常嵌套测试全部 PASS
- [ ] 原子操作正确性测试全部 PASS
- [ ] Dhrystone 分数记录
- [ ] CoreMark 分数记录
- [ ] 自动化测试驱动 (`scripts/run_tests.py`) 一键运行
- [ ] 测试报告生成 (HTML / JSON 格式)
- [ ] DSE 基础 (`tools/dse/sweepdriver.py` + `configs/sweep/cache_sweep.json`) 实现
- [ ] 16/16 ctest PASS 不退化
- [ ] D4 + ADR-040 静态检查 3+4/3 PASS 不退化
- [ ] 1 原子 commit 引用 Phase 2 plan + DECISION
- [ ] M2 里程碑 ✅ 达成 ("ISA 全覆盖" 完成)

### Must Have

- riscv-tests 子模块完整集成 (8 个 ISA 扩展, 不遗漏)
- RISCOF 官方合规签名 (供 RISC-V 基金会存档)
- 自动化测试驱动一键运行 (不需手动逐个跑测试)
- 测试报告可读 (HTML / JSON 格式)
- DSE 基础支持参数扫描 (cache size × associativity × line size)

### Must NOT Have (Guardrails)

- 不实现完整的 Spike/Sail (这些是 RISC-V 基金会维护的参考实现, 我们只对接)
- 不实现 RISC-V 工具链 (riscv64-unknown-elf-gcc 等是开源工具链, 直接用)
- 不实现 CPU IP (Phase 1 已通过 L1CachePlugin 演示 Plugin-style, Phase 2 只需验证 ISA)
- 不实现 Linux/RTOS (Phase 3+ 范围)
- 不修改 Phase 1 业务代码 (Plugin-style TLM 平台保持稳定)
- 不修改 L1CachePlugin 业务逻辑 (它仅作为测试目标)
- 不写"完美合规报告" (RISCOF 自动生成, 我们只需对接)
- 不写 100% 测试覆盖率 (200+ 测试已足够 M2 里程碑)
- 不实现 Phase 3 RTOS 集成 (Phase 3+ 范围)
- 不实现 Phase 4 Linux 启动 (Phase 4 范围)
- 不实现 Phase 5 RTL 升级 (Phase 5 范围)
- 不实现 Phase 6 完整框架 (Phase 6 范围)

---

## Verification Strategy

### Test Decision

- **基础设施存在**: ✅ (Phase 1.4 完成, L1CachePlugin + L1CacheTLMBridge + Bridge Adapter + ch_stream + JSON instantiateAll)
- **riscv-tests 集成**: 子模块 + Makefile + 交叉编译到 riscv64-unknown-elf
- **RISCOF**: 官方 Python 框架, 我们写 DUT Plugin
- **自动化驱动**: `scripts/run_tests.py` 一键运行 + 生成报告

### QA Policy (Self-Check 清单)

1. 工具链准备
   - [ ] riscv64-unknown-elf-gcc 已安装 (`riscv64-unknown-elf-gcc --version`)
   - [ ] Spike 已安装 (`spike --help`)
   - [ ] Sail 已安装 (`sail-riscv --help`)
   - [ ] RISCOF 已安装 (`riscof --version`)
   - [ ] Python 3.9+ 已安装 (`python3 --version`)

2. riscv-tests 集成
   - [ ] 子模块添加 (`git submodule add https://github.com/riscv-software-src/riscv-tests`)
   - [ ] 8 个 ISA 扩展编译 (rv64ui, rv64um, rv64ua, rv64uf, rv64ud, rv64uc, rv64si, rv64mi)
   - [ ] 测试加载到 TLM 平台 (使用 L1CachePlugin as memory model)
   - [ ] 200+ 测试全部 PASS

3. RISCOF 合规认证
   - [ ] DUT Plugin 编写 (`chipsforge_dut.py`, 调用 TLM 平台仿真)
   - [ ] Sail 参考模型对接
   - [ ] Spike 性能参考对接
   - [ ] RISCOF 框架调用 (`riscof run --config=config.ini`)
   - [ ] HTML 合规报告生成 (`riscof report`)
   - [ ] 官方合规签名验证

4. 自定义功能测试
   - [ ] CSR 完整访问测试 (mstatus, mtvec, mepc, mcause, satp, sscratch 等)
   - [ ] PMP 物理内存保护区域测试
   - [ ] 虚拟内存 Sv39 / Sv48 测试
   - [ ] 中断/异常嵌套测试
   - [ ] 原子操作正确性测试

5. 基准测试
   - [ ] Dhrystone 编译 + 运行
   - [ ] CoreMark 编译 + 运行
   - [ ] 分数记录 (CSV / JSON)

6. 自动化测试驱动
   - [ ] `scripts/run_tests.py` 实现 (TestRunner 类)
   - [ ] `run_isa_tests()` 方法
   - [ ] `run_compliance()` 方法
   - [ ] `generate_report()` 方法 (HTML / JSON)
   - [ ] 一键运行所有测试 (回归测试)

7. DSE 基础
   - [ ] `tools/dse/sweepdriver.py` 实现
   - [ ] `configs/sweep/cache_sweep.json` 示例配置
   - [ ] 多基准 × 多参数组合批量执行
   - [ ] CSV/JSON 结果汇总

8. 终极验证
   - [ ] 16/16 ctest PASS 不退化
   - [ ] D4 + ADR-040 静态检查 3+4/3 PASS 不退化
   - [ ] 1 原子 commit 引用 Phase 2 plan + DECISION
   - [ ] M2 里程碑 ✅ 达成

---

## Execution Strategy

### 任务依赖图

```
1 (工具链准备) ──→ 2 (riscv-tests 集成) ──→ 6 (自动化驱动) ──→ 7 (DSE)
                                  ↓                    ↓
                                  3 (RISCOF)        5 (基准)
                                  ↓                    ↓
                                  4 (自定义测试) ──→ 6 (报告生成)
```

### 并行策略

**可并行**:
- 2 (riscv-tests) + 3 (RISCOF) — 都需 1 工具链但产物不同 (ELF 加载 vs DUT Plugin)
- 4 (自定义测试) + 5 (基准) — 都需 1 工具链但可独立运行
- 6 (自动化) + 7 (DSE) — 报告生成 vs 参数扫描独立

**串行**:
- 1 (工具链) → 全部其他任务
- 2 → 6 (riscv-tests 集成后才能自动化)
- 3 + 4 + 5 → 6 (报告生成需所有测试结果)

### 时间线 (单 session 估算)

| 时间 | 任务 | 输出 |
|------|------|------|
| T+0h | 1 工具链准备 | 5 个工具验证 |
| T+0.5h | 2 riscv-tests 集成 | 8 ISA 扩展编译 PASS |
| T+1.5h | 3 RISCOF 合规 | HTML 报告 + 官方签名 |
| T+2.5h | 4 自定义测试 | CSR/PMP/VirtualMem/中断/原子 5 类测试 |
| T+3.5h | 5 基准测试 | Dhrystone + CoreMark 分数 |
| T+4.5h | 6 自动化驱动 | scripts/run_tests.py + 报告 |
| T+5.5h | 7 DSE 基础 | sweepdriver.py + cache_sweep.json |
| T+6h | 自检 + 原子 commit | 1 commit + 16/16 ctest + D4 PASS |

### 关键路径

1 → 2 → 3 → 6 (报告) → 6 (报告生成) → 自检 → commit

总关键路径: 6h (可压缩到 5h, 如 2+3 并行)

---

## TODOs

### 1. 工具链准备 [~] BLOCKED by external dependency

**Status**: Blocked by external dependency (2026-06-13 13:55)

**Why blocked**: 4 个 RISC-V 工具未预装:
- `riscv64-unknown-elf-gcc` (RISC-V 交叉编译器) — 未安装
- `spike` (性能参考 ISA simulator) — 未安装
- `sail-riscv` (RISC-V 黄金参考实现) — 未安装
- `riscof` (RISC-V 合规认证框架) — 未安装

**Verification** (`which` 命令):
- `which riscv64-unknown-elf-gcc` → empty
- `which spike` → empty
- `which sail-riscv` → empty
- `which riscof` → empty
- `which python3` → `/home/ubuntu/venv/bin/python3` ✅ (Python 3.12.3 已装)

**F2 决策冲突**:
- F2 决议"工具链策略 = 直接用预装, 不重编译"
- 实际 4/5 工具未预装 (仅 python3)
- 按 F2 决议"如工具链缺失, 阻塞需用户决策" — 当前是用户决策点

**需要 user 决策**:
1. **安装 RISC-V 工具链** (sudo apt install gcc-riscv64-unknown-elf spike + 安装 sail-riscv + pip install riscof)
2. **接受部分工具链缺失** (修改 F2 决议, 仅用 Python 验证 + L1CachePlugin as memory model, 不依赖完整 RISC-V 工具链)
3. **暂停 Phase 2** (等其他环境 ready)

**Why not 自动执行**:
- 工具链安装需 sudo 权限 + 系统级操作, 超出 atlas 编排器范围
- 修改 F2 决策需 Prometheus 重新起草决策草案, 是新决策
- Phase 2 范围 = 完整 RISC-V ISA 验证, 无工具链则无法实现 8 任务中任意一个

**解锁条件**: 用户明确指示安装工具链 (选项 1) 或调整 F2 决策 (选项 2) 或暂停 (选项 3)

**What to do** (解锁后):
- [ ] 验证 `riscv64-unknown-elf-gcc` (交叉编译器)
- [ ] 验证 `spike` (性能参考)
- [ ] 验证 `sail-riscv` (黄金参考)
- [ ] 验证 `riscof` (合规框架)
- [ ] 验证 `python3` (测试驱动)

**What to do**:

- [ ] 验证 `riscv64-unknown-elf-gcc` (交叉编译器)
- [ ] 验证 `spike` (性能参考)
- [ ] 验证 `sail-riscv` (黄金参考)
- [ ] 验证 `riscof` (合规框架)
- [ ] 验证 `python3` (测试驱动)

**Must NOT do**:
- 不安装/编译工具链 (假设已预装)
- 不解决工具链依赖问题 (Phase 2 假设工具链 ready)

**Recommended Agent Profile**:
- Category: `quick`
- Skills: `[]` (简单工具检查)
- Reason: bash 命令验证, 无需特殊技能

**Parallelization**:
- Can Run In Parallel: YES (5 个工具独立检查)
- Parallel Group: Wave 1
- Blocks: 2, 3
- Blocked By: None

**References**:
- `docs/roadmap/phases/phase-2-baremetal.md` (Phase 2 路线图)
- Phase 1 集成文档 (`.opencode/skills/cmake/`, 工具链验证)

**Acceptance Criteria**:
- [ ] 5 个工具全部命令 `--version` 或 `--help` 成功

**Commit**: NO (独立 commit 在 6 末尾)

---

### 2. riscv-tests 集成

**What to do**:

- [ ] `git submodule add https://github.com/riscv-software-src/riscv-tests .omo/submodules/riscv-tests`
- [ ] 8 个 ISA 扩展编译 (`rv64ui`, `rv64um`, `rv64ua`, `rv64uf`, `rv64ud`, `rv64uc`, `rv64si`, `rv64mi`)
- [ ] 编写 `scripts/load_riscv_test.py` 加载 ELF 到 L1CachePlugin TLM 平台
- [ ] 监控 tohost 寄存器判断 pass/fail
- [ ] 200+ 测试全部 PASS (rv64ui 80+ + 其他各 20+)

**Must NOT do**:
- 不修改 riscv-tests 源码 (上游官方)
- 不实现完整 CPU (Phase 1 L1CachePlugin as memory model 即可)
- 不修改 L1CachePlugin 业务逻辑 (它仅作为测试目标)

**Recommended Agent Profile**:
- Category: `cpp`
- Skills: `[]`
- Reason: ELF 加载 + TLM 平台对接, C++/Python 任务

**Parallelization**:
- Can Run In Parallel: NO (需 1 工具链 + 编译环境)
- Parallel Group: Sequential
- Blocks: 6 (报告生成)
- Blocked By: 1 (工具链)

**References**:
- [riscv-tests GitHub](https://github.com/riscv-software-src/riscv-tests)
- `bundles/mem_bundles.h` (TLM Bundle 类型)
- `src/cf_plugin/bridge/l1_cache_bridge.cpp` (L1CacheTLMBridge)
- `ip/cache/tlm/L1CachePlugin.{h,cpp}` (L1CachePlugin)

**Acceptance Criteria**:
- [ ] 8 个 ISA 扩展子模块全部添加
- [ ] 200+ 测试全部 PASS
- [ ] 失败测试 0 个

**Commit**: NO (累积到 6 末尾)

---

### 3. RISCOF 合规认证

**What to do**:

- [ ] 编写 DUT Plugin `tests/compliance/chipsforge_dut.py` (调用 TLM 平台仿真)
- [ ] 对接 Sail 参考模型 (`sail-riscv`)
- [ ] 对接 Spike 性能参考 (`spike`)
- [ ] 编写 `tests/compliance/config.ini` (RISCOF 配置)
- [ ] 运行 `riscof run --config=config.ini`
- [ ] 生成 HTML 合规报告 (`riscof report`)
- [ ] 官方合规签名验证

**Must NOT do**:
- 不实现完整 Sail/Spike (上游官方, 我们只对接)
- 不修改 RISCOF 框架 (上游官方)
- 不写"完美合规报告" (RISCOF 自动生成)

**Recommended Agent Profile**:
- Category: `cpp`
- Skills: `[]`
- Reason: RISCOF 配置 + DUT Plugin 编写, Python 任务

**Parallelization**:
- Can Run In Parallel: YES (与 2 并行, 都需 1 工具链但产物不同)
- Parallel Group: Wave 2 (与 2 并行)
- Blocks: 6 (报告生成)
- Blocked By: 1 (工具链)

**References**:
- [RISCOF GitHub](https://github.com/riscv-software-src/riscof)
- [RISCOF 文档](https://riscof.readthedocs.io/)
- Sail: `sail-riscv` 官方参考实现
- Spike: `spike` 性能参考

**Acceptance Criteria**:
- [ ] DUT Plugin 编译运行 (RISCOF 框架调用)
- [ ] Sail 参考模型对接成功
- [ ] HTML 合规报告生成
- [ ] 官方合规签名存在

**Commit**: NO (累积到 6 末尾)

---

### 4. 自定义功能测试

**What to do**:

- [ ] CSR 完整访问测试 (`tests/custom/csr_test.cpp`): mstatus, mtvec, mepc, mcause, satp, sscratch 等
- [ ] PMP 物理内存保护区域测试 (`tests/custom/pmp_test.cpp`): 区域粒度 (8/16 bytes) + 读/写/执行权限
- [ ] 虚拟内存 Sv39 / Sv48 测试 (`tests/custom/vm_test.cpp`): 3 级 / 4 级页表
- [ ] 中断/异常嵌套测试 (`tests/custom/interrupt_test.cpp`): M/S/U-mode 三级 + 嵌套深度
- [ ] 原子操作正确性测试 (`tests/custom/atomic_test.cpp`): LR/SC 冲突 + AMO 顺序一致性

**Must NOT do**:
- 不实现完整 MMU (Sv39/Sv48 仅测试, Phase 4 Linux 启动才需)
- 不实现完整中断控制器 (PLIC/CLINT, Phase 3 RTOS 范围)
- 不修改 L1CachePlugin 业务逻辑 (CSR/PMP 通过外部 CPU 模型测试)

**Recommended Agent Profile**:
- Category: `cpp`
- Skills: `[]`
- Reason: 5 个 C++ 测试文件, 单元测试任务

**Parallelization**:
- Can Run In Parallel: YES (5 个测试文件可独立编写)
- Parallel Group: Wave 3 (与 5 并行)
- Blocks: 6 (报告生成)
- Blocked By: 1 (工具链)

**References**:
- Phase 1 测试模式 (`src/cf_plugin/tests/test_l1_cache_plugin_unit.cpp`)
- RISC-V 特权级规范: https://riscv.org/specifications/privileged-isa/
- CSR 列表: mstatus (0x300), mtvec (0x305), mepc (0x341), mcause (0x342), satp (0x180), sscratch (0x140)

**Acceptance Criteria**:
- [ ] 5 个测试文件全部实现
- [ ] 每个测试文件 10+ 子测试
- [ ] 全部测试 PASS

**Commit**: NO (累积到 6 末尾)

---

### 5. 基准测试

**What to do**:

- [ ] Dhrystone 编译 + 运行 (`benchmarks/dhrystone/`)
- [ ] CoreMark 编译 + 运行 (`benchmarks/coremark/`)
- [ ] 分数记录 (`benchmarks/results/dhrystone.csv` + `coremark.csv`)
- [ ] JSON 汇总 (`benchmarks/results/summary.json`)

**Must NOT do**:
- 不实现 Dhrystone / CoreMark (上游官方基准, 只需交叉编译)
- 不修改 L1CachePlugin (基准是性能参考, 不影响业务逻辑)

**Recommended Agent Profile**:
- Category: `cpp`
- Skills: `[]`
- Reason: 交叉编译 + 运行 + 记录分数

**Parallelization**:
- Can Run In Parallel: YES (与 4 并行)
- Parallel Group: Wave 3 (与 4 并行)
- Blocks: 6 (报告生成)
- Blocked By: 1 (工具链)

**References**:
- Dhrystone: https://github.com/Keith-S-Thompson/dhrystone
- CoreMark: https://github.com/eembc/coremark

**Acceptance Criteria**:
- [ ] Dhrystone DMIPS 分数记录
- [ ] CoreMark MHz 归一化分数记录
- [ ] CSV + JSON 汇总

**Commit**: NO (累积到 6 末尾)

---

### 6. 自动化测试驱动

**What to do**:

- [ ] 实现 `scripts/run_tests.py` (TestRunner 类)
- [ ] `run_isa_tests(self, arch="rv64gc")` 方法 (编译 ELF + 加载 TLM 平台 + 监控 tohost + 汇总)
- [ ] `run_compliance(self)` 方法 (RISCOF 框架调用)
- [ ] `generate_report(self)` 方法 (HTML / JSON 测试报告)
- [ ] 整合 2 + 3 + 4 + 5 的测试结果到统一报告
- [ ] 一键运行所有测试 (回归测试入口)

**Must NOT do**:
- 不修改 Phase 1 测试基础设施 (Phase 1 已稳定)
- 不实现完整测试调度系统 (只需一键运行入口)
- 不写"完美报告" (简洁可读即可)

**Recommended Agent Profile**:
- Category: `cpp`
- Skills: `[]`
- Reason: Python 测试驱动 + 报告生成

**Parallelization**:
- Can Run In Parallel: NO (需 2 + 3 + 4 + 5 完成)
- Parallel Group: Sequential (2 + 3 + 4 + 5 完成后)
- Blocks: 7 (DSE 可选依赖自动化)
- Blocked By: 2, 3, 4, 5

**References**:
- Python unittest / pytest 框架
- RISCOF 报告格式
- Phase 1 测试模式 (5 层测试结构)

**Acceptance Criteria**:
- [ ] `python3 scripts/run_tests.py` 一键运行所有测试
- [ ] HTML 报告生成 (含 pass/fail 汇总)
- [ ] JSON 报告生成 (供 DSE 使用)

**Commit**: NO (累积到 6 末尾)

---

### 7. DSE 基础

**What to do**:

- [ ] 实现 `tools/dse/sweepdriver.py` 参数扫描驱动
- [ ] 编写 `configs/sweep/cache_sweep.json` 示例扫描配置 (cache size × associativity × line size)
- [ ] 支持多基准测试 × 多参数组合的批量执行
- [ ] 实现基本的 CSV/JSON 结果汇总 (用于 Pareto 分析)
- [ ] 调用 6 自动化驱动作为子任务

**Must NOT do**:
- 不实现完整 DSE 框架 (Phase 6 范围, Phase 2 仅基础)
- 不实现多目标优化算法 (NSGA-II 等, Phase 6 范围)
- 不实现可视化 (Phase 6 范围)

**Recommended Agent Profile**:
- Category: `cpp`
- Skills: `[]`
- Reason: Python DSE 基础

**Parallelization**:
- Can Run In Parallel: YES (与 6 并行, 但通常 6 先完成)
- Parallel Group: Wave 4 (与 6 并行)
- Blocks: None
- Blocked By: 6 (可调用自动化驱动)

**References**:
- 6 自动化驱动 (作为 sweepdriver 的执行后端)
- Phase 6 DSE 规划 (未来扩展)

**Acceptance Criteria**:
- [ ] sweepdriver.py 可运行示例配置
- [ ] cache_sweep.json 含 3+ 参数扫描维度
- [ ] CSV/JSON 结果输出可读

**Commit**: NO (累积到 6 末尾)

---

### 8. 自检 + 原子 commit (6 末尾)

**What to do**:

- [ ] ctest 16/16 PASS 不退化
- [ ] D4 + ADR-040 静态检查 3+4/3 PASS 不退化
- [ ] 1 原子 commit 包含所有 Phase 2 变更 + 引用 Phase 2 plan + DECISION

**Must NOT do**:
- 不拆 commit (单原子 commit 保持可回滚性)
- 不绕过 `git status` 检查

**Recommended Agent Profile**:
- Category: `git-master` skill
- Skills: `git-master`
- Reason: 原子 commit 规范

**Parallelization**:
- Can Run In Parallel: NO
- Parallel Group: Sequential
- Blocks: None
- Blocked By: 1, 2, 3, 4, 5, 6, 7

**References**:
- Phase 1.4 commit message 模板 (`.omo/drafts/decision-phase-1.3d-extras-bridge-2026-06-13.md` §5)

**Acceptance Criteria**:
- [ ] ctest 16/16 PASS 不退化
- [ ] D4 + ADR-040 静态检查 3+4/3 PASS 不退化
- [ ] 1 原子 commit 引用 Phase 2 plan + DECISION

**Commit**: YES (单原子 commit, 含所有 Phase 2 变更)

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get explicit "okay" before completing.

### F1. Plan Compliance Audit — `oracle`

**What to do**:
- Read `.omo/plans/phase-2-baremetal-riscv-tests.md` end-to-end
- Verify each "Must Have" in §Work Objectives
- Verify each "Must NOT Have" (Guardrails) absent
- Verify each 1-7 任务 completed
- Verify M2 里程碑 ✅ 达成

**Output**: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

### F2. Code Quality Review — `cpp`

**What to do**:
- 跑 `ctest --test-dir build --output-on-failure` (16/16 PASS 不退化)
- 跑 `bash tools/verify_plugin_decision.sh` (3+4/3 PASS 不退化)
- Review 8 个 ISA 扩展编译脚本 + 5 个自定义测试 + 自动化驱动 + DSE 基础
- 验证报告生成 (HTML / JSON)

**Output**: `Build [PASS/FAIL] | Lint [PASS/FAIL] | Tests [N pass/N fail] | Files [N clean] | VERDICT`

### F3. Real Manual QA — `cpp`

**What to do**:
- 手动跑 1-7 所有 QA Scenarios
- 验证 200+ riscv-tests 全部 PASS
- 验证 RISCOF HTML 报告生成
- 验证 5 个自定义测试全部 PASS
- 验证 Dhrystone + CoreMark 分数记录
- 验证自动化驱动一键运行
- 验证 DSE 基础可运行

**Output**: `Scenarios [N/N pass] | Integration [N/N] | Edge Cases [N tested] | VERDICT`

### F4. Scope Fidelity Check — `deep`

**What to do**:
- 对每个任务 (1-7): 读 "What to do", 读实际产出 (git diff)
- 验证 1:1 (任务说的事都做了)
- 验证 0:0 (没做任务外的事)
- 检查 Must NOT do 合规 (不实现 Spike/Sail, 不实现工具链, 不实现 CPU)
- 检查跨任务污染

**Output**: `Tasks [N/N compliant] | Contamination [CLEAN] | Unaccounted [CLEAN] | VERDICT`

---

## Commit Strategy

- **6 末尾**: `docs(phase-2): baremetal-riscv-tests M2 里程碑达成` - 1 atomic commit 含 1-7 所有变更

### Commit Message 模板

```
docs(phase-2): baremetal-riscv-tests M2 里程碑达成

M2 里程碑: ISA 全覆盖 (riscv-tests 8 ISA 扩展 200+ 测试全部 PASS + RISCOF 官方合规签名)

[新增] .gitmodules + .omo/submodules/riscv-tests (8 ISA 扩展子模块)
[新增] tests/compliance/chipsforge_dut.py (RISCOF DUT Plugin)
[新增] tests/compliance/config.ini (RISCOF 配置)
[新增] tests/custom/csr_test.cpp (CSR 完整访问测试)
[新增] tests/custom/pmp_test.cpp (PMP 物理内存保护测试)
[新增] tests/custom/vm_test.cpp (虚拟内存 Sv39/Sv48 测试)
[新增] tests/custom/interrupt_test.cpp (中断/异常嵌套测试)
[新增] tests/custom/atomic_test.cpp (原子操作正确性测试)
[新增] scripts/run_tests.py (TestRunner 自动化驱动)
[新增] tools/dse/sweepdriver.py (DSE 参数扫描驱动)
[新增] configs/sweep/cache_sweep.json (示例扫描配置)
[新增] benchmarks/results/{dhrystone,coremark}.csv (基准测试分数)
[新增] benchmarks/results/summary.json (基准测试汇总)
[新增] riscv-tests-report.html (RISCOF 合规报告)

无业务代码变更 (16/16 ctest 不退化, D4+ADR-040 静态检查 3+4/3 PASS)
退出标准 E1-E8 全部通过
M2 里程碑 ✅ 达成 (ISA 全覆盖)
Phase 3 RTOS 启动门槛就绪
```

---

## Success Criteria

### Verification Commands

```bash
# 工具链验证
riscv64-unknown-elf-gcc --version
spike --help | head -5
sail-riscv --help | head -5
riscof --version
python3 --version

# 测试结果验证
cd /workspace/project/ChipForge
git submodule status  # riscv-tests 已添加
ls .omo/submodules/riscv-tests/isa/rv64ui/  # rv64ui 测试存在
python3 scripts/run_tests.py  # 一键运行所有测试
cat benchmarks/results/summary.json  # 基准测试分数
cat riscv-tests-report.html  # RISCOF 合规报告

# 终极验证
ctest --test-dir build --output-on-failure  # 16/16 PASS 不退化
bash tools/verify_plugin_decision.sh  # 3+4/3 PASS 不退化
git log --oneline -1  # 最新 commit 引用 Phase 2 plan + DECISION
```

### Final Checklist

- [ ] riscv-tests 8 个 ISA 扩展 200+ 测试全部 PASS
- [ ] RISCOF 官方合规签名 + HTML 报告生成
- [ ] CSR 完整访问测试全部 PASS
- [ ] PMP 物理内存保护区域测试全部 PASS
- [ ] 虚拟内存 Sv39 / Sv48 测试全部 PASS
- [ ] 中断/异常嵌套测试全部 PASS
- [ ] 原子操作正确性测试全部 PASS
- [ ] Dhrystone 分数记录
- [ ] CoreMark 分数记录
- [ ] 自动化测试驱动 (`scripts/run_tests.py`) 一键运行
- [ ] 测试报告生成 (HTML / JSON 格式)
- [ ] DSE 基础 (`tools/dse/sweepdriver.py` + `configs/sweep/cache_sweep.json`) 实现
- [ ] 16/16 ctest PASS 不退化
- [ ] D4 + ADR-040 静态检查 3+4/3 PASS 不退化
- [ ] 1 原子 commit 引用 Phase 2 plan + DECISION
- [ ] M2 里程碑 ✅ 达成

---

## 决策草案链接

Phase 2 决策草案 (Prometheus 起草):
- 路径: `.omo/drafts/decision-phase-2-baremetal-riscv-tests-2026-06-13.md` (待生成)
- 决议: F1-F5 (待生成)

---

## 路线图

- [Phase 2 路线图](../docs/roadmap/phases/phase-2-baremetal.md) (6 大任务, M2 里程碑)
- [Phase 1 路线图](../docs/roadmap/phases/phase-1-tlm-foundation.md) (依赖, 已完成)
- [Phase 3 路线图 (RTOS)](../docs/roadmap/phases/phase-3-rtos.md) (M2 完成后启动)

---

*本计划基于 Phase 1.4 ORCHESTRATION COMPLETE 状态起草。Phase 2 启动门槛已就绪。*

# Phase 2: baremetal-riscv-tests — 决策草案 v1

> **决策ID**: DECISION-2026-06-13-03
> **决策日期**: 2026-06-13
> **决策状态**: **Proposed v1** (待本次 session 实施 + 验证后改 Accepted v1)
> **提出方**: Prometheus (基于 Phase 2 路线图 + Phase 1.4 ORCHESTRATION COMPLETE 状态 + 用户 `/start-work phase-2-baremetal-riscv-tests` 指令)
> **关联路线图**: `docs/roadmap/phases/phase-2-baremetal.md`
> **关联计划**: `.omo/plans/phase-2-baremetal-riscv-tests.md`

---

## 1. Why

Phase 1 (Plugin-style TLM Foundation) 已 100% 完成:
- Phase 1.0-1.3: 6 任务全部完成, 51/51 单元测试, 14/14 ctest, D4 范式合规
- Phase 1.4: L1CachePlugin 设计方法学复盘 v1 ORCHESTRATION COMPLETE (commit `137df84`)
- 16 原子 commit 全部 push, boulder.json status=completed, elapsed_ms=2,549,253
- Phase 1 进度 75% → 85%, PA-1~PA-9 全部 ✅

Phase 2 启动门槛**完全就绪**。用户明确指示 `/start-work phase-2-baremetal-riscv-tests`,Phase 2 是 M2 里程碑("ISA 全覆盖")的执行阶段。

### 1.1 关键技术挑战

| 挑战 | 风险等级 | 缓解 |
|------|---------|------|
| riscv-tests 子模块大 (200+ 测试) | 中 | 预留 5-10 分钟 clone + 编译 |
| RISCOF DUT Plugin 编写 | 中 | 复用 Phase 1 TLM 平台 as memory model |
| RISC-V 工具链依赖 | 低 | Phase 1 已部分集成 (riscv64-unknown-elf-gcc, spike, sail) |
| 子代理事故 (Phase 1.4 经验) | 中 | writing/review 类任务优先 atlas 手动 |

### 1.2 Out of Scope (明确不做)

- ❌ 不实现完整 Spike/Sail (RISC-V 基金会维护, 仅对接)
- ❌ 不实现 RISC-V 工具链 (开源, 直接用)
- ❌ 不实现 CPU IP (Phase 1 已通过 L1CachePlugin 演示 Plugin-style)
- ❌ 不实现 Linux/RTOS (Phase 3+ 范围)
- ❌ 不修改 Phase 1 业务代码 (Plugin-style TLM 平台保持稳定)
- ❌ 不修改 L1CachePlugin 业务逻辑 (仅作为测试目标)

---

## 2. What Changes (F1-F5 决议)

### F1: Phase 2 范围 = baremetal 测试套件 (M2 里程碑)

**决议**: 实施 6 大任务(riscv-tests 集成 + RISCOF 合规 + 自定义测试 + 基准 + 自动化 + DSE 基础),达成 M2 里程碑("ISA 全覆盖")

**理由**:
- 路线图 §Phase 2 明确这 6 大任务为 Phase 2 范围
- M2 里程碑是 Phase 3 (RTOS) 启动门槛
- 200+ ISA 测试 + 官方合规签名 = ISA 全覆盖证明

### F2: 工具链策略 = 直接用预装, 不重编译

**决议**: 验证 `riscv64-unknown-elf-gcc` / `spike` / `sail-riscv` / `riscof` / `python3` 5 个工具已预装,不重编译不解决依赖问题

**理由**:
- 这些工具是开源项目, Phase 1 已部分集成
- 编译工具链超出 M2 范围 (M2 是 ISA 验证, 不是工具链开发)
- 如工具链缺失, 阻塞需用户决策 (与 Phase 1.4 R7 闭环模式一致)

### F3: 测试目标 = L1CachePlugin as memory model, 不需完整 CPU

**决议**: Phase 2 测试加载到 L1CachePlugin + L1CacheTLMBridge + L1CacheTLMBridgeAdapter TLM 平台(Phase 1.3 e2e 已验证 5/5 PASS),不实现完整 CPU IP

**理由**:
- Phase 1.4 v1 文档已确认 Plugin-style 范式通过 L1CachePlugin 端到端验证
- riscv-tests 是 ELF 程序, 加载到 TLM 平台 + 监控 tohost 寄存器判断 pass/fail
- 自定义 CSR/PMP/VirtualMem 测试通过外部 CPU 模型 + L1CachePlugin as memory model

### F4: 自动化驱动 = 一键运行入口, 不实现完整测试调度

**决议**: `scripts/run_tests.py` 提供 `run_isa_tests` / `run_compliance` / `generate_report` 3 个方法,不实现完整 CI/CD 集成或测试调度系统

**理由**:
- Phase 2 范围是"测试套件可用", 不是 CI/CD 平台
- 回归测试入口足够 Phase 3+ 使用
- 完整测试调度系统超出 M2 范围

### F5: DSE 基础 = 参数扫描 + 结果汇总, 不实现多目标优化

**决议**: `tools/dse/sweepdriver.py` 支持 cache size × associativity × line size 参数扫描,生成 CSV/JSON 结果用于 Pareto 分析;不实现 NSGA-II 等多目标优化算法(Phase 6 范围)

**理由**:
- M2 里程碑是"ISA 全覆盖", DSE 是 Phase 6 完整框架的子集
- Phase 2 DSE 基础供 Phase 6 扩展, 不需完整实现
- Pareto 分析工具是未来 Phase 6 工作

---

## 3. Out of Scope

- Phase 3 RTOS 集成
- Phase 4 Linux 启动 (OpenSBI + Kernel)
- Phase 5 RTL 升级 (CppHDL)
- Phase 6 完整 DSE 框架
- 完整 Spike/Sail 实现 (仅对接)
- RISC-V 工具链编译/修复 (直接用)
- CPU IP 实现 (Phase 1 L1CachePlugin as memory model 即可)

---

## 4. Verification Commands (commit 前必跑)

```bash
cd /workspace/project/ChipForge

# 工具链验证
riscv64-unknown-elf-gcc --version
spike --help | head -5
sail-riscv --help | head -5
riscof --version
python3 --version

# 测试结果验证
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

---

## 5. Commit Message 模板

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

## 6. Rollback

```bash
cd /workspace/project/ChipForge
git revert <commit-hash>  # 单原子 commit, 一键回滚
```

---

## 7. 决议表 (F1-F5)

| # | 决议 | 状态 |
|---|------|------|
| **F1** | Phase 2 范围 = 6 大任务, 达成 M2 里程碑 | **Proposed** |
| **F2** | 工具链策略 = 直接用预装, 不重编译 | **Proposed** |
| **F3** | 测试目标 = L1CachePlugin as memory model, 不需完整 CPU | **Proposed** |
| **F4** | 自动化驱动 = 一键运行入口, 不实现完整测试调度 | **Proposed** |
| **F5** | DSE 基础 = 参数扫描 + 结果汇总, 不实现多目标优化 | **Proposed** |

---

*Phase 2 实施 + 验证后改 Accepted v1。Phase 3 RTOS 启动门槛就绪。*
